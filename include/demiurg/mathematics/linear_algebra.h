/*
----------------------------------------------------------------
Contents:
This file provides linear algebra types and operations

----------------------------------------------------------------
Code info:
- dmg_lia prefix
- math.h and string.h dependant
- matrices are column-major, so accessing element is data[col][row]

----------------------------------------------------------------
Usage:
- does not require building implementation unlike most demiurg libraries
- read through what available, include and use

----------------------------------------------------------------
Notes:
- SIMD can be implemented
*/

#ifndef DEMIURG_LINEAR_ALGEBRA_H
#define DEMIURG_LINEAR_ALGEBRA_H

#include <math.h>
#include <string.h>

// ===========================
// Scalar helpers

#define DMG_LIA_PI 3.14159265358979323846f
#define DMG_LIA_DEG2RAD(d) ((d) * (DMG_LIA_PI / 180.0f))
#define DMG_LIA_RAD2DEG(r) ((r) * (180.0f / DMG_LIA_PI))

static inline float dmg_lia_float_clamp(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

// ===========================
// Types

typedef struct { float x, y;       } dmg_lia_vec2;
typedef struct { float x, y, z;    } dmg_lia_vec3;
typedef struct { float x, y, z, w; } dmg_lia_vec4;

// Column-major: m[col][row]
typedef struct { float m[2][2]; }   dmg_lia_mat2;
typedef struct { float m[3][3]; }   dmg_lia_mat3;
typedef struct { float m[4][4]; }   dmg_lia_mat4;

// ===========================
// Vec2

static inline dmg_lia_vec2 dmg_lia_vec2_add(dmg_lia_vec2 a, dmg_lia_vec2 b) { 
    return (dmg_lia_vec2){ a.x + b.x, a.y + b.y }; 
}

static inline dmg_lia_vec2 dmg_lia_vec2_sub(dmg_lia_vec2 a, dmg_lia_vec2 b) {
    return (dmg_lia_vec2){ a.x - b.x, a.y - b.y };
}

static inline dmg_lia_vec2 dmg_lia_vec2_scale(dmg_lia_vec2 a, float s) { 
    return (dmg_lia_vec2){ a.x * s, a.y * s };
}

static inline float dmg_lia_vec2_dot(dmg_lia_vec2 a, dmg_lia_vec2 b) { 
    return a.x * b.x + a.y * b.y; 
}
static inline float dmg_lia_vec2_len(dmg_lia_vec2 a) { 
    return sqrtf(dmg_lia_vec2_dot(a, a)); 
}

static inline dmg_lia_vec2 dmg_lia_vec2_normalize(dmg_lia_vec2 a) {
    float len = dmg_lia_vec2_len(a);
    return len > 0.0f ? dmg_lia_vec2_scale(a, 1.0f / len) : a;
}

// ===========================
// Vec3

static inline dmg_lia_vec3 dmg_lia_vec3_add(dmg_lia_vec3 a, dmg_lia_vec3 b) { 
    return (dmg_lia_vec3){ a.x + b.x, a.y + b.y, a.z + b.z }; 
}

static inline dmg_lia_vec3 dmg_lia_vec3_sub(dmg_lia_vec3 a, dmg_lia_vec3 b) { 
    return (dmg_lia_vec3){ a.x - b.x, a.y - b.y, a.z - b.z }; 
}

static inline dmg_lia_vec3 dmg_lia_vec3_scale(dmg_lia_vec3 a, float s) { 
    return (dmg_lia_vec3){ a.x * s, a.y * s, a.z * s }; 
}

static inline float dmg_lia_vec3_dot(dmg_lia_vec3 a, dmg_lia_vec3 b) { 
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline dmg_lia_vec3 dmg_lia_vec3_cross(dmg_lia_vec3 a, dmg_lia_vec3 b) {
    return (dmg_lia_vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float dmg_lia_vec3_len(dmg_lia_vec3 a) { 
    return sqrtf(dmg_lia_vec3_dot(a, a)); 
}

static inline dmg_lia_vec3 dmg_lia_vec3_normalize(dmg_lia_vec3 a) {
    float len = dmg_lia_vec3_len(a);
    return len > 0.0f ? dmg_lia_vec3_scale(a, 1.0f / len) : a;
}

// ===========================
// Vec4

static inline dmg_lia_vec4 dmg_lia_vec4_add(dmg_lia_vec4 a, dmg_lia_vec4 b) { 
    return (dmg_lia_vec4){ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; 
}

static inline dmg_lia_vec4 dmg_lia_vec4_sub(dmg_lia_vec4 a, dmg_lia_vec4 b) { 
    return (dmg_lia_vec4){ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; 
}

static inline dmg_lia_vec4 dmg_lia_vec4_scale(dmg_lia_vec4 a, float s) {
    return (dmg_lia_vec4){ a.x * s, a.y * s, a.z * s, a.w * s }; 
}

static inline float dmg_lia_vec4_dot(dmg_lia_vec4 a, dmg_lia_vec4 b) { 
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; 
}

static inline float dmg_lia_vec4_len(dmg_lia_vec4 a) { 
    return sqrtf(dmg_lia_vec4_dot(a, a)); 
}

static inline dmg_lia_vec4 dmg_lia_vec4_normalize(dmg_lia_vec4 a) {
    float len = dmg_lia_vec4_len(a);
    return len > 0.0f ? dmg_lia_vec4_scale(a, 1.0f / len) : a;
}

// ===========================
// Mat2

static inline dmg_lia_mat2 dmg_lia_mat2_identity(void) {
    dmg_lia_mat2 r; memset(&r, 0, sizeof(r));
    r.m[0][0] = 1.0f; r.m[1][1] = 1.0f;
    return r;
}

static inline dmg_lia_mat2 dmg_lia_mat2_add(dmg_lia_mat2 a, dmg_lia_mat2 b) {
    dmg_lia_mat2 r;
    for (int c = 0; c < 2; ++c)
        for (int rr = 0; rr < 2; ++rr)
            r.m[c][rr] = a.m[c][rr] + b.m[c][rr];
    return r;
}

static inline dmg_lia_mat2 dmg_lia_mat2_mul(dmg_lia_mat2 a, dmg_lia_mat2 b) {
    dmg_lia_mat2 r;
    for (int c = 0; c < 2; ++c)
        for (int rr = 0; rr < 2; ++rr)
            r.m[c][rr] = a.m[0][rr] * b.m[c][0] + a.m[1][rr] * b.m[c][1];
    return r;
}

static inline dmg_lia_vec2 dmg_lia_mat2_mul_vec2(dmg_lia_mat2 a, dmg_lia_vec2 v) {
    return (dmg_lia_vec2){
        a.m[0][0] * v.x + a.m[1][0] * v.y,
        a.m[0][1] * v.x + a.m[1][1] * v.y
    };
}

static inline float dmg_lia_mat2_det(dmg_lia_mat2 a) {
    return a.m[0][0] * a.m[1][1] - a.m[1][0] * a.m[0][1];
}

static inline dmg_lia_mat2 dmg_lia_mat2_transpose(dmg_lia_mat2 a) {
    dmg_lia_mat2 r;
    for (int c = 0; c < 2; ++c)
        for (int rr = 0; rr < 2; ++rr)
            r.m[c][rr] = a.m[rr][c];
    return r;
}

// ===========================
// Mat3

static inline dmg_lia_mat3 dmg_lia_mat3_identity(void) {
    dmg_lia_mat3 r; memset(&r, 0, sizeof(r));
    r.m[0][0] = 1.0f; r.m[1][1] = 1.0f;
    r.m[2][2] = 1.0f; return r;
}

static inline dmg_lia_mat3 dmg_lia_mat3_add(dmg_lia_mat3 a, dmg_lia_mat3 b) {
    dmg_lia_mat3 r;
    for (int c = 0; c < 3; ++c)
        for (int rr = 0; rr < 3; ++rr)
            r.m[c][rr] = a.m[c][rr] + b.m[c][rr];
    return r;
}

static inline dmg_lia_mat3 dmg_lia_mat3_mul(dmg_lia_mat3 a, dmg_lia_mat3 b) {
    dmg_lia_mat3 r;
    for (int c = 0; c < 3; ++c)
        for (int rr = 0; rr < 3; ++rr)
            r.m[c][rr] = a.m[0][rr] * b.m[c][0] + a.m[1][rr] * b.m[c][1] + a.m[2][rr] * b.m[c][2];
    return r;
}

static inline dmg_lia_vec3 dmg_lia_mat3_mul_vec3(dmg_lia_mat3 a, dmg_lia_vec3 v) {
    return (dmg_lia_vec3){
        a.m[0][0] * v.x + a.m[1][0] * v.y + a.m[2][0] * v.z,
        a.m[0][1] * v.x + a.m[1][1] * v.y + a.m[2][1] * v.z,
        a.m[0][2] * v.x + a.m[1][2] * v.y + a.m[2][2] * v.z
    };
}

static inline float dmg_lia_mat3_det(dmg_lia_mat3 a) {
    return a.m[0][0] * (a.m[1][1] * a.m[2][2] - a.m[2][1] * a.m[1][2])
         - a.m[1][0] * (a.m[0][1] * a.m[2][2] - a.m[2][1] * a.m[0][2])
         + a.m[2][0] * (a.m[0][1] * a.m[1][2] - a.m[1][1] * a.m[0][2]);
}

static inline dmg_lia_mat3 dmg_lia_mat3_transpose(dmg_lia_mat3 a) {
    dmg_lia_mat3 r;
    for (int c = 0; c < 3; ++c)
        for (int rr = 0; rr < 3; ++rr)
            r.m[c][rr] = a.m[rr][c];
    return r;
}

// ===========================
// Mat4

static inline dmg_lia_mat4 dmg_lia_mat4_identity(void) {
    dmg_lia_mat4 r; memset(&r, 0, sizeof(r));
    r.m[0][0] = 1.0f; r.m[1][1] = 1.0f;
    r.m[2][2] = 1.0f; r.m[3][3] = 1.0f;
    return r;
}

static inline dmg_lia_mat4 dmg_lia_mat4_add(dmg_lia_mat4 a, dmg_lia_mat4 b) {
    dmg_lia_mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            r.m[c][rr] = a.m[c][rr] + b.m[c][rr];
    return r;
}

static inline dmg_lia_mat4 dmg_lia_mat4_mul(dmg_lia_mat4 a, dmg_lia_mat4 b) {
    dmg_lia_mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            r.m[c][rr] = a.m[0][rr] * b.m[c][0] + a.m[1][rr] * b.m[c][1]
                       + a.m[2][rr] * b.m[c][2] + a.m[3][rr] * b.m[c][3];
    return r;
}

static inline dmg_lia_vec4 dmg_lia_mat4_mul_vec4(dmg_lia_mat4 a, dmg_lia_vec4 v) {
    return (dmg_lia_vec4){
        a.m[0][0] * v.x + a.m[1][0] * v.y + a.m[2][0] * v.z + a.m[3][0] * v.w,
        a.m[0][1] * v.x + a.m[1][1] * v.y + a.m[2][1] * v.z + a.m[3][1] * v.w,
        a.m[0][2] * v.x + a.m[1][2] * v.y + a.m[2][2] * v.z + a.m[3][2] * v.w,
        a.m[0][3] * v.x + a.m[1][3] * v.y + a.m[2][3] * v.z + a.m[3][3] * v.w
    };
}

static inline dmg_lia_mat4 dmg_lia_mat4_transpose(dmg_lia_mat4 a) {
    dmg_lia_mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            r.m[c][rr] = a.m[rr][c];
    return r;
}

// ===========================
// Homogeneous transforms - mat3 (2D transforms, vec2 in homogeneous coords)

static inline dmg_lia_mat3 dmg_lia_mat3_translate(float tx, float ty) {
    dmg_lia_mat3 r = dmg_lia_mat3_identity();
    r.m[2][0] = tx;
    r.m[2][1] = ty;
    return r;
}

static inline dmg_lia_mat3 dmg_lia_mat3_scale(float sx, float sy) {
    dmg_lia_mat3 r = dmg_lia_mat3_identity();
    r.m[0][0] = sx;
    r.m[1][1] = sy;
    return r;
}

static inline dmg_lia_mat3 dmg_lia_mat3_rotate(float radians) {
    dmg_lia_mat3 r = dmg_lia_mat3_identity();
    float c = cosf(radians);
    float s = sinf(radians);
    r.m[0][0] = c;  r.m[1][0] = -s;
    r.m[0][1] = s;  r.m[1][1] = c;
    return r;
}

static inline dmg_lia_vec2 dmg_lia_mat3_transform_point(dmg_lia_mat3 a, dmg_lia_vec2 p) {
    dmg_lia_vec3 h = dmg_lia_mat3_mul_vec3(a, (dmg_lia_vec3){ p.x, p.y, 1.0f });
    return (dmg_lia_vec2){ h.x, h.y };
}

static inline dmg_lia_vec2 dmg_lia_mat3_transform_dir(dmg_lia_mat3 a, dmg_lia_vec2 d) {
    dmg_lia_vec3 h = dmg_lia_mat3_mul_vec3(a, (dmg_lia_vec3){ d.x, d.y, 0.0f });
    return (dmg_lia_vec2){ h.x, h.y };
}

// ===========================
// Homogeneous transforms - mat4 (3D transforms, vec3 in homogeneous coords)

static inline dmg_lia_mat4 dmg_lia_mat4_translate(float tx, float ty, float tz) {
    dmg_lia_mat4 r = dmg_lia_mat4_identity();
    r.m[3][0] = tx;
    r.m[3][1] = ty;
    r.m[3][2] = tz;
    return r;
}

static inline dmg_lia_mat4 dmg_lia_mat4_scale(float sx, float sy, float sz) {
    dmg_lia_mat4 r = dmg_lia_mat4_identity();
    r.m[0][0] = sx;
    r.m[1][1] = sy;
    r.m[2][2] = sz;
    return r;
}

static inline dmg_lia_mat4 dmg_lia_mat4_rotate_x(float radians) {
    dmg_lia_mat4 r = dmg_lia_mat4_identity();
    float c = cosf(radians);
    float s = sinf(radians);
    r.m[1][1] = c;  r.m[2][1] = -s;
    r.m[1][2] = s;  r.m[2][2] = c;
    return r;
}

static inline dmg_lia_mat4 dmg_lia_mat4_rotate_y(float radians) {
    dmg_lia_mat4 r = dmg_lia_mat4_identity();
    float c = cosf(radians);
    float s = sinf(radians);
    r.m[0][0] = c;   r.m[2][0] = s;
    r.m[0][2] = -s;  r.m[2][2] = c;
    return r;
}

static inline dmg_lia_mat4 dmg_lia_mat4_rotate_z(float radians) {
    dmg_lia_mat4 r = dmg_lia_mat4_identity();
    float c = cosf(radians);
    float s = sinf(radians);
    r.m[0][0] = c;  r.m[1][0] = -s;
    r.m[0][1] = s;  r.m[1][1] = c;
    return r;
}

static inline dmg_lia_vec3 dmg_lia_mat4_transform_point(dmg_lia_mat4 a, dmg_lia_vec3 p) {
    dmg_lia_vec4 h = dmg_lia_mat4_mul_vec4(a, (dmg_lia_vec4){ p.x, p.y, p.z, 1.0f });
    return (dmg_lia_vec3){ h.x, h.y, h.z };
}

static inline dmg_lia_vec3 dmg_lia_mat4_transform_dir(dmg_lia_mat4 a, dmg_lia_vec3 d) {
    dmg_lia_vec4 h = dmg_lia_mat4_mul_vec4(a, (dmg_lia_vec4){ d.x, d.y, d.z, 0.0f });
    return (dmg_lia_vec3){ h.x, h.y, h.z };
}

// ===========================
// Affine storage types
// Memory-optimal packing of an affine mat3/mat4 for storage/transfer
// (the omitted bottom row is always [0 ... 0 1] for an affine transform).
// These carry no math ops of their own - pack a real matrix into them
// to store or upload, and unpack back into a real matrix to compute with.

typedef struct { float m[3][2]; } dmg_lia_affine2; // packed dmg_lia_mat3
typedef struct { float m[4][3]; } dmg_lia_affine3; // packed dmg_lia_mat4

static inline dmg_lia_affine2 dmg_lia_affine2_pack(dmg_lia_mat3 a) {
    dmg_lia_affine2 r;
    for (int c = 0; c < 3; ++c) {
        r.m[c][0] = a.m[c][0];
        r.m[c][1] = a.m[c][1];
    }
    return r;
}

static inline dmg_lia_mat3 dmg_lia_affine2_unpack(dmg_lia_affine2 a) {
    dmg_lia_mat3 r;
    for (int c = 0; c < 3; ++c) {
        r.m[c][0] = a.m[c][0];
        r.m[c][1] = a.m[c][1];
        r.m[c][2] = (c == 2) ? 1.0f : 0.0f;
    }
    return r;
}

static inline dmg_lia_affine3 dmg_lia_affine3_pack(dmg_lia_mat4 a) {
    dmg_lia_affine3 r;
    for (int c = 0; c < 4; ++c) {
        r.m[c][0] = a.m[c][0];
        r.m[c][1] = a.m[c][1];
        r.m[c][2] = a.m[c][2];
    }
    return r;
}

static inline dmg_lia_mat4 dmg_lia_affine3_unpack(dmg_lia_affine3 a) {
    dmg_lia_mat4 r;
    for (int c = 0; c < 4; ++c) {
        r.m[c][0] = a.m[c][0];
        r.m[c][1] = a.m[c][1];
        r.m[c][2] = a.m[c][2];
        r.m[c][3] = (c == 3) ? 1.0f : 0.0f;
    }
    return r;
}

#endif // DEMIURG_LINEAR_ALGEBRA_H
