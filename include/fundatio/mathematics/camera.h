/*
----------------------------------------------------------------
Contents:
This file provides basic camera mathematics and prefab movement systems.

----------------------------------------------------------------
Code info:
- fnd_cam prefix
- linear_algebra.h dependent

----------------------------------------------------------------
Usage:
- does not require building implementation unlike most fundatio libraries
- read through what available, include and use
*/

#ifndef FUNDATIO_CAMERA_H
#define FUNDATIO_CAMERA_H

#include "fundatio/mathematics/linear_algebra.h"

// ===========================
// Projection Math

static inline fnd_lia_mat4 fnd_cam_perspective(float fov_y_rad, float aspect, float z_near, float z_far) {
    fnd_lia_mat4 m = (fnd_lia_mat4){0};
    float tan_half_fov = tanf(fov_y_rad * 0.5f);

    m.m[0][0] = 1.0f / (aspect * tan_half_fov);
    m.m[1][1] = 1.0f / tan_half_fov;
    m.m[2][2] = -(z_far + z_near) / (z_far - z_near);
    m.m[2][3] = -1.0f;
    m.m[3][2] = -(2.0f * z_far * z_near) / (z_far - z_near);

    return m;
}

static inline fnd_lia_mat4 fnd_cam_orthographic(float left, float right, float bottom, float top, float z_near, float z_far) {
    fnd_lia_mat4 m = (fnd_lia_mat4){0};

    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = -2.0f / (z_far - z_near);
    m.m[3][0] = -(right + left) / (right - left);
    m.m[3][1] = -(top + bottom) / (top - bottom);
    m.m[3][2] = -(z_far + z_near) / (z_far - z_near);
    m.m[3][3] = 1.0f;

    return m;
}

// ===========================
// Camera

typedef struct fnd_cam_camera {
    fnd_lia_vec3 position;
    float    yaw;   // Radians
    float    pitch; // Radians
} fnd_cam_camera;

static inline fnd_lia_vec3 fnd_cam_camera_get_forward(fnd_cam_camera cam) {
    float cos_p = cosf(cam.pitch);
    fnd_lia_vec3 dir = {
        sinf(cam.yaw) * cos_p,
        sinf(cam.pitch),
        -cosf(cam.yaw) * cos_p
    };
    return fnd_lia_vec3_normalize(dir);
}

static inline fnd_lia_vec3 fnd_cam_camera_get_right(fnd_cam_camera cam) {
    fnd_lia_vec3 fwd = fnd_cam_camera_get_forward(cam);
    fnd_lia_vec3 world_up = { 0.0f, 1.0f, 0.0f };
    return fnd_lia_vec3_normalize(fnd_lia_vec3_cross(fwd, world_up));
}

static inline fnd_lia_vec3 fnd_cam_camera_get_up(fnd_cam_camera cam) {
    fnd_lia_vec3 fwd = fnd_cam_camera_get_forward(cam);
    fnd_lia_vec3 rgt = fnd_cam_camera_get_right(cam);
    return fnd_lia_vec3_cross(rgt, fwd);
}

static inline void fnd_cam_camera_rotate(fnd_cam_camera* cam, float delta_yaw, float delta_pitch, float pitch_limit) {
    cam->yaw += delta_yaw;
    cam->pitch += delta_pitch;
    cam->pitch = fnd_lia_float_clamp(cam->pitch, -pitch_limit, pitch_limit);
}

static inline fnd_lia_mat4 fnd_cam_camera_look_at(fnd_lia_vec3 eye, fnd_lia_vec3 target, fnd_lia_vec3 up) {
    fnd_lia_vec3 f = fnd_lia_vec3_normalize(fnd_lia_vec3_sub(target, eye));
    fnd_lia_vec3 r = fnd_lia_vec3_normalize(fnd_lia_vec3_cross(f, up));
    fnd_lia_vec3 u = fnd_lia_vec3_cross(r, f);
    fnd_lia_vec3 d = fnd_lia_vec3_scale(f, -1.0f); // View space looking along -Z

    fnd_lia_mat4 m;
    m.m[0][0] = r.x;  m.m[0][1] = u.x;  m.m[0][2] = d.x;  m.m[0][3] = 0.0f;
    m.m[1][0] = r.y;  m.m[1][1] = u.y;  m.m[1][2] = d.y;  m.m[1][3] = 0.0f;
    m.m[2][0] = r.z;  m.m[2][1] = u.z;  m.m[2][2] = d.z;  m.m[2][3] = 0.0f;
    m.m[3][0] = -fnd_lia_vec3_dot(r, eye);
    m.m[3][1] = -fnd_lia_vec3_dot(u, eye);
    m.m[3][2] = -fnd_lia_vec3_dot(d, eye);
    m.m[3][3] = 1.0f;

    return m;
}

static inline fnd_lia_mat4 fnd_cam_camera_get_view_matrix(fnd_cam_camera cam) {
    fnd_lia_vec3 fwd = fnd_cam_camera_get_forward(cam);
    fnd_lia_vec3 target = fnd_lia_vec3_add(cam.position, fwd);
    fnd_lia_vec3 world_up = { 0.0f, 1.0f, 0.0f };
    return fnd_cam_camera_look_at(cam.position, target, world_up);
}

// ===========================
// Free Camera (Freely flying)

typedef struct fnd_cam_free_camera {
    fnd_cam_camera camera;
    float       speed;
} fnd_cam_free_camera;

static inline void fnd_cam_free_camera_rotate(fnd_cam_free_camera* free_cam, float delta_yaw, float delta_pitch, float pitch_limit) {
    fnd_cam_camera_rotate(&free_cam->camera, delta_yaw, delta_pitch, pitch_limit);
}

// Full 6-DOF fly movement (movement direction relative to view orientation)
static inline void fnd_cam_free_camera_move_fly(fnd_cam_free_camera* free_cam, float forward_axis, float right_axis, float up_axis, float dt) {
    fnd_lia_vec3 fwd = fnd_cam_camera_get_forward(free_cam->camera);
    fnd_lia_vec3 rgt = fnd_cam_camera_get_right(free_cam->camera);
    fnd_lia_vec3 up  = fnd_cam_camera_get_up(free_cam->camera);

    fnd_lia_vec3 move = { 0.0f, 0.0f, 0.0f };
    move = fnd_lia_vec3_add(move, fnd_lia_vec3_scale(fwd, forward_axis));
    move = fnd_lia_vec3_add(move, fnd_lia_vec3_scale(rgt, right_axis));
    move = fnd_lia_vec3_add(move, fnd_lia_vec3_scale(up, up_axis));

    float step = free_cam->speed * dt;
    free_cam->camera.position = fnd_lia_vec3_add(free_cam->camera.position, fnd_lia_vec3_scale(move, step));
}

// Ground/FPS-style movement (forward and right are restricted to the horizontal ground plane)
static inline void fnd_cam_free_camera_move_fps(fnd_cam_free_camera* free_cam, float forward_axis, float right_axis, float up_axis, float dt) {
    fnd_lia_vec3 fwd = fnd_cam_camera_get_forward(free_cam->camera);
    fwd.y = 0.0f;
    fwd = fnd_lia_vec3_normalize(fwd);

    fnd_lia_vec3 rgt = fnd_cam_camera_get_right(free_cam->camera);
    rgt.y = 0.0f;
    rgt = fnd_lia_vec3_normalize(rgt);

    fnd_lia_vec3 world_up = { 0.0f, 1.0f, 0.0f };

    fnd_lia_vec3 move = { 0.0f, 0.0f, 0.0f };
    move = fnd_lia_vec3_add(move, fnd_lia_vec3_scale(fwd, forward_axis));
    move = fnd_lia_vec3_add(move, fnd_lia_vec3_scale(rgt, right_axis));
    move = fnd_lia_vec3_add(move, fnd_lia_vec3_scale(world_up, up_axis));

    float step = free_cam->speed * dt;
    free_cam->camera.position = fnd_lia_vec3_add(free_cam->camera.position, fnd_lia_vec3_scale(move, step));
}

// ===========================
// Orbit Camera (orbiting around point)

typedef struct fnd_cam_orbit_camera {
    fnd_cam_camera camera;
    float       speed;
    fnd_lia_vec3    target;
    float       distance;
} fnd_cam_orbit_camera;

static inline void fnd_cam_orbit_camera_update_position(fnd_cam_orbit_camera* orbit_cam) {
    fnd_lia_vec3 fwd = fnd_cam_camera_get_forward(orbit_cam->camera);
    // Position camera along -forward vector at defined distance from target
    orbit_cam->camera.position = fnd_lia_vec3_sub(orbit_cam->target, fnd_lia_vec3_scale(fwd, orbit_cam->distance));
}

static inline void fnd_cam_orbit_camera_rotate(fnd_cam_orbit_camera* orbit_cam, float delta_yaw, float delta_pitch, float pitch_limit) {
    fnd_cam_camera_rotate(&orbit_cam->camera, delta_yaw, delta_pitch, pitch_limit);
    fnd_cam_orbit_camera_update_position(orbit_cam);
}

static inline void fnd_cam_orbit_camera_pan(fnd_cam_orbit_camera* orbit_cam, float right_axis, float up_axis, float dt) {
    fnd_lia_vec3 rgt = fnd_cam_camera_get_right(orbit_cam->camera);
    fnd_lia_vec3 up  = fnd_cam_camera_get_up(orbit_cam->camera);

    float step = orbit_cam->speed * dt;
    fnd_lia_vec3 pan = { 0.0f, 0.0f, 0.0f };
    pan = fnd_lia_vec3_add(pan, fnd_lia_vec3_scale(rgt, right_axis * step));
    pan = fnd_lia_vec3_add(pan, fnd_lia_vec3_scale(up, up_axis * step));

    orbit_cam->target = fnd_lia_vec3_add(orbit_cam->target, pan);
    fnd_cam_orbit_camera_update_position(orbit_cam);
}

static inline void fnd_cam_orbit_camera_zoom(fnd_cam_orbit_camera* orbit_cam, float zoom_axis, float dt) {
    orbit_cam->distance -= zoom_axis * orbit_cam->speed * dt;
    if (orbit_cam->distance < 0.001f) {
        orbit_cam->distance = 0.001f;
    }
    fnd_cam_orbit_camera_update_position(orbit_cam);
}

static inline fnd_lia_mat4 fnd_cam_orbit_camera_get_view_matrix(fnd_cam_orbit_camera* orbit_cam) {
    fnd_cam_orbit_camera_update_position(orbit_cam);
    fnd_lia_vec3 world_up = { 0.0f, 1.0f, 0.0f };
    return fnd_cam_camera_look_at(orbit_cam->camera.position, orbit_cam->target, world_up);
}

#endif // FUNDATIO_CAMERA_H
