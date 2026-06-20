/*
----------------------------------------------------------------
Contents:
This file provides linear algebra types and operations

----------------------------------------------------------------
Code info:
- dla prefix
- math.h and string.h dependant
- matrices are column-major, so accessing element is data[col][row]

----------------------------------------------------------------
Usage:
- does not require building implementation unlike most demigurg libraries
- read through what available, include and use

----------------------------------------------------------------
Notes:
- SIMD can be implemented
*/

#ifndef DEMIGURG_LINEAR_ALGEBRA_H
#define DEMIGURG_LINEAR_ALGEBRA_H

#include <math.h>
#include <string.h>
 
// ===========================
// Scalar helpers
 
#define LLA_PI 3.14159265358979323846f
#define LLA_DEG2RAD(d) ((d) * (LLA_PI / 180.0f))
#define LLA_RAD2DEG(r) ((r) * (180.0f / LLA_PI))
 
static inline float dla_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float dla_lerpf (float a, float b, float t) { return a + t * (b - a); }
static inline float dla_minf  (float a, float b)          { return a < b ? a : b;   }
static inline float dla_maxf  (float a, float b)          { return a > b ? a : b;   }
 
// ===========================
// Types
 
typedef struct { float x, y;       } dla_vec2;
typedef struct { float x, y, z;    } dla_vec3;
typedef struct { float x, y, z, w; } dla_vec4;
 
// Column-major: m[col][row]
typedef struct { float m[2][2]; } dla_mat2x2;
typedef struct { float m[3][2]; } dla_mat2x3;
typedef struct { float m[3][3]; } dla_mat3x3;
typedef struct { float m[4][4]; } dla_mat4x4;
 
// ===========================
// dla_vec2
 
// Arithmetic
static inline dla_vec2 dla_vec2_add  (dla_vec2 a, dla_vec2 b) { return (dla_vec2){a.x+b.x, a.y+b.y}; }
static inline dla_vec2 dla_vec2_sub  (dla_vec2 a, dla_vec2 b) { return (dla_vec2){a.x-b.x, a.y-b.y}; }
static inline dla_vec2 dla_vec2_mul  (dla_vec2 a, dla_vec2 b) { return (dla_vec2){a.x*b.x, a.y*b.y}; }
static inline dla_vec2 dla_vec2_div  (dla_vec2 a, dla_vec2 b) { return (dla_vec2){a.x/b.x, a.y/b.y}; }
static inline dla_vec2 dla_vec2_neg  (dla_vec2 a)             { return (dla_vec2){-a.x, -a.y};        }
static inline dla_vec2 dla_vec2_scale(dla_vec2 a, float s)    { return (dla_vec2){a.x*s, a.y*s};      }
static inline dla_vec2 dla_vec2_adds (dla_vec2 a, float s)    { return (dla_vec2){a.x+s, a.y+s};      }
 
// Products
// dot product
static inline float dla_vec2_dot  (dla_vec2 a, dla_vec2 b) { return a.x*b.x + a.y*b.y; }
// Z component of the 3-D cross product
static inline float dla_vec2_cross(dla_vec2 a, dla_vec2 b) { return a.x*b.y - a.y*b.x; }
 
// Length / distance

// square of length (no sqrt applied)
static inline float dla_vec2_len2 (dla_vec2 a)             { return dla_vec2_dot(a, a); }
static inline float dla_vec2_len  (dla_vec2 a)             { return sqrtf(dla_vec2_len2(a)); }

// square of distance (no sqrt applied)
static inline float dla_vec2_dist2(dla_vec2 a, dla_vec2 b) { return dla_vec2_len2(dla_vec2_sub(a, b)); }
static inline float dla_vec2_dist (dla_vec2 a, dla_vec2 b) { return sqrtf(dla_vec2_dist2(a, b)); }
 
// Normalization
static inline dla_vec2 dla_vec2_normalize(dla_vec2 a) {
    float len = dla_vec2_len(a);
    return (len > 1e-8f) ? dla_vec2_scale(a, 1.f / len) : (dla_vec2){0};
}
 
// Interpolation / clamping
static inline dla_vec2 dla_vec2_lerp (dla_vec2 a, dla_vec2 b, float t) {
    return (dla_vec2){dla_lerpf(a.x,b.x,t), dla_lerpf(a.y,b.y,t)};
}
static inline dla_vec2 dla_vec2_min  (dla_vec2 a, dla_vec2 b) { return (dla_vec2){dla_minf(a.x,b.x), dla_minf(a.y,b.y)}; }
static inline dla_vec2 dla_vec2_max  (dla_vec2 a, dla_vec2 b) { return (dla_vec2){dla_maxf(a.x,b.x), dla_maxf(a.y,b.y)}; }
static inline dla_vec2 dla_vec2_clamp(dla_vec2 v, dla_vec2 lo, dla_vec2 hi) {
    return (dla_vec2){dla_clampf(v.x,lo.x,hi.x), dla_clampf(v.y,lo.y,hi.y)};
}
static inline dla_vec2 dla_vec2_abs  (dla_vec2 a) { return (dla_vec2){fabsf(a.x), fabsf(a.y)}; }
 
// Geometry
// perpendicular vector (CCW 90 degrees)
static inline dla_vec2 dla_vec2_perp   (dla_vec2 a)             { return (dla_vec2){-a.y, a.x}; }
static inline dla_vec2 dla_vec2_reflect(dla_vec2 v, dla_vec2 n) {
    return dla_vec2_sub(v, dla_vec2_scale(n, 2.f * dla_vec2_dot(v, n)));
}
 
// Equality (component-wise within eps)
static inline int dla_vec2_eq(dla_vec2 a, dla_vec2 b, float eps) {
    return (fabsf(a.x-b.x) <= eps) && (fabsf(a.y-b.y) <= eps);
}
 
// ===========================
// dla_vec3
 
// Arithmetic
static inline dla_vec3 dla_vec3_add  (dla_vec3 a, dla_vec3 b) { return (dla_vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline dla_vec3 dla_vec3_sub  (dla_vec3 a, dla_vec3 b) { return (dla_vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline dla_vec3 dla_vec3_mul  (dla_vec3 a, dla_vec3 b) { return (dla_vec3){a.x*b.x, a.y*b.y, a.z*b.z}; }
static inline dla_vec3 dla_vec3_div  (dla_vec3 a, dla_vec3 b) { return (dla_vec3){a.x/b.x, a.y/b.y, a.z/b.z}; }
static inline dla_vec3 dla_vec3_neg  (dla_vec3 a)             { return (dla_vec3){-a.x, -a.y, -a.z};           }
static inline dla_vec3 dla_vec3_scale(dla_vec3 a, float s)    { return (dla_vec3){a.x*s, a.y*s, a.z*s};        }
static inline dla_vec3 dla_vec3_adds (dla_vec3 a, float s)    { return (dla_vec3){a.x+s, a.y+s, a.z+s};        }
 
// Products
static inline float    dla_vec3_dot  (dla_vec3 a, dla_vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline dla_vec3 dla_vec3_cross(dla_vec3 a, dla_vec3 b) {
    return (dla_vec3){ a.y*b.z - a.z*b.y,
                       a.z*b.x - a.x*b.z,
                       a.x*b.y - a.y*b.x };
}
 
// Length / distance
static inline float dla_vec3_len2 (dla_vec3 a)             { return dla_vec3_dot(a, a); }
static inline float dla_vec3_len  (dla_vec3 a)             { return sqrtf(dla_vec3_len2(a)); }
static inline float dla_vec3_dist2(dla_vec3 a, dla_vec3 b) { return dla_vec3_len2(dla_vec3_sub(a, b)); }
static inline float dla_vec3_dist (dla_vec3 a, dla_vec3 b) { return sqrtf(dla_vec3_dist2(a, b)); }
 
// Normalization
static inline dla_vec3 dla_vec3_normalize(dla_vec3 a) {
    float len = dla_vec3_len(a);
    return (len > 1e-8f) ? dla_vec3_scale(a, 1.f / len) : (dla_vec3){0};
}
 
// Interpolation / clamping
static inline dla_vec3 dla_vec3_lerp (dla_vec3 a, dla_vec3 b, float t) {
    return (dla_vec3){dla_lerpf(a.x,b.x,t), dla_lerpf(a.y,b.y,t), dla_lerpf(a.z,b.z,t)};
}
static inline dla_vec3 dla_vec3_min  (dla_vec3 a, dla_vec3 b) {
    return (dla_vec3){dla_minf(a.x,b.x), dla_minf(a.y,b.y), dla_minf(a.z,b.z)};
}
static inline dla_vec3 dla_vec3_max  (dla_vec3 a, dla_vec3 b) {
    return (dla_vec3){dla_maxf(a.x,b.x), dla_maxf(a.y,b.y), dla_maxf(a.z,b.z)};
}
static inline dla_vec3 dla_vec3_clamp(dla_vec3 v, dla_vec3 lo, dla_vec3 hi) {
    return (dla_vec3){dla_clampf(v.x,lo.x,hi.x), dla_clampf(v.y,lo.y,hi.y), dla_clampf(v.z,lo.z,hi.z)};
}
static inline dla_vec3 dla_vec3_abs  (dla_vec3 a) { return (dla_vec3){fabsf(a.x), fabsf(a.y), fabsf(a.z)}; }
 
// Geometry
static inline dla_vec3 dla_vec3_reflect(dla_vec3 v, dla_vec3 n) {
    return dla_vec3_sub(v, dla_vec3_scale(n, 2.f * dla_vec3_dot(v, n)));
}
// Snell's law refraction; returns {0} on total internal reflection
static inline dla_vec3 dla_vec3_refract(dla_vec3 v, dla_vec3 n, float eta) {
    float d = dla_vec3_dot(n, v);
    float k = 1.f - eta*eta*(1.f - d*d);
    if (k < 0.f) return (dla_vec3){0};
    return dla_vec3_sub(dla_vec3_scale(v, eta),
                        dla_vec3_scale(n, eta * d + sqrtf(k)));
}
// Angle between two vectors in radians
static inline float dla_vec3_angle(dla_vec3 a, dla_vec3 b) {
    float d = dla_vec3_dot(dla_vec3_normalize(a), dla_vec3_normalize(b));
    return acosf(dla_clampf(d, -1.f, 1.f));
}
 
// Equality (component-wise within eps)
static inline int dla_vec3_eq(dla_vec3 a, dla_vec3 b, float eps) {
    return (fabsf(a.x-b.x) <= eps) && (fabsf(a.y-b.y) <= eps) && (fabsf(a.z-b.z) <= eps);
}
 
// ===========================
// dla_vec4
 
// Arithmetic
static inline dla_vec4 dla_vec4_add  (dla_vec4 a, dla_vec4 b) { return (dla_vec4){a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w}; }
static inline dla_vec4 dla_vec4_sub  (dla_vec4 a, dla_vec4 b) { return (dla_vec4){a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w}; }
static inline dla_vec4 dla_vec4_mul  (dla_vec4 a, dla_vec4 b) { return (dla_vec4){a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w}; }
static inline dla_vec4 dla_vec4_div  (dla_vec4 a, dla_vec4 b) { return (dla_vec4){a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w}; }
static inline dla_vec4 dla_vec4_neg  (dla_vec4 a)             { return (dla_vec4){-a.x, -a.y, -a.z, -a.w};              }
static inline dla_vec4 dla_vec4_scale(dla_vec4 a, float s)    { return (dla_vec4){a.x*s, a.y*s, a.z*s, a.w*s};          }
static inline dla_vec4 dla_vec4_adds (dla_vec4 a, float s)    { return (dla_vec4){a.x+s, a.y+s, a.z+s, a.w+s};          }
 
// Products
static inline float dla_vec4_dot(dla_vec4 a, dla_vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
 
// Length / distance
static inline float dla_vec4_len2 (dla_vec4 a)             { return dla_vec4_dot(a, a); }
static inline float dla_vec4_len  (dla_vec4 a)             { return sqrtf(dla_vec4_len2(a)); }
static inline float dla_vec4_dist2(dla_vec4 a, dla_vec4 b) { return dla_vec4_len2(dla_vec4_sub(a, b)); }
static inline float dla_vec4_dist (dla_vec4 a, dla_vec4 b) { return sqrtf(dla_vec4_dist2(a, b)); }
 
// Normalization
static inline dla_vec4 dla_vec4_normalize(dla_vec4 a) {
    float len = dla_vec4_len(a);
    return (len > 1e-8f) ? dla_vec4_scale(a, 1.f / len) : (dla_vec4){0};
}
 
// Interpolation / clamping
static inline dla_vec4 dla_vec4_lerp (dla_vec4 a, dla_vec4 b, float t) {
    return (dla_vec4){dla_lerpf(a.x,b.x,t), dla_lerpf(a.y,b.y,t),
                      dla_lerpf(a.z,b.z,t), dla_lerpf(a.w,b.w,t)};
}
static inline dla_vec4 dla_vec4_min  (dla_vec4 a, dla_vec4 b) {
    return (dla_vec4){dla_minf(a.x,b.x), dla_minf(a.y,b.y), dla_minf(a.z,b.z), dla_minf(a.w,b.w)};
}
static inline dla_vec4 dla_vec4_max  (dla_vec4 a, dla_vec4 b) {
    return (dla_vec4){dla_maxf(a.x,b.x), dla_maxf(a.y,b.y), dla_maxf(a.z,b.z), dla_maxf(a.w,b.w)};
}
static inline dla_vec4 dla_vec4_clamp(dla_vec4 v, dla_vec4 lo, dla_vec4 hi) {
    return (dla_vec4){dla_clampf(v.x,lo.x,hi.x), dla_clampf(v.y,lo.y,hi.y),
                      dla_clampf(v.z,lo.z,hi.z), dla_clampf(v.w,lo.w,hi.w)};
}
static inline dla_vec4 dla_vec4_abs  (dla_vec4 a) {
    return (dla_vec4){fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w)};
}
 
// Geometry
// Perspective divide: (x,y,z,w) -> (x/w, y/w, z/w)
static inline dla_vec3 dla_vec4_pdiv(dla_vec4 a) {
    float inv = (fabsf(a.w) > 1e-8f) ? 1.f / a.w : 1.f;
    return (dla_vec3){a.x*inv, a.y*inv, a.z*inv};
}
 
// Equality (component-wise within eps)
static inline int dla_vec4_eq(dla_vec4 a, dla_vec4 b, float eps) {
    return (fabsf(a.x-b.x) <= eps) && (fabsf(a.y-b.y) <= eps) &&
           (fabsf(a.z-b.z) <= eps) && (fabsf(a.w-b.w) <= eps);
}
 
// ===========================
// dla_mat2x2
 
// Element access
static inline float    dla_mat2x2_get(dla_mat2x2 m, int col, int row)           { return m.m[col][row]; }
static inline void     dla_mat2x2_set(dla_mat2x2 *m, int col, int row, float v) { m->m[col][row] = v;   }
static inline dla_vec2 dla_mat2x2_col(dla_mat2x2 m, int c) { return (dla_vec2){m.m[c][0], m.m[c][1]}; }
static inline dla_vec2 dla_mat2x2_row(dla_mat2x2 m, int r) { return (dla_vec2){m.m[0][r], m.m[1][r]}; }
 
// Arithmetic
static inline dla_mat2x2 dla_mat2x2_add(dla_mat2x2 a, dla_mat2x2 b) {
    dla_mat2x2 r; int i, j;
    for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) r.m[i][j] = a.m[i][j]+b.m[i][j];
    return r;
}
static inline dla_mat2x2 dla_mat2x2_sub(dla_mat2x2 a, dla_mat2x2 b) {
    dla_mat2x2 r; int i, j;
    for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) r.m[i][j] = a.m[i][j]-b.m[i][j];
    return r;
}
static inline dla_mat2x2 dla_mat2x2_scale(dla_mat2x2 a, float s) {
    dla_mat2x2 r; int i, j;
    for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) r.m[i][j] = a.m[i][j]*s;
    return r;
}
// Matrix x matrix
static inline dla_mat2x2 dla_mat2x2_mul(dla_mat2x2 a, dla_mat2x2 b) {
    dla_mat2x2 r = {0}; int i, j, k;
    for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) for (k = 0; k < 2; k++)
        r.m[i][j] += a.m[k][j] * b.m[i][k];
    return r;
}
// Matrix x vector
static inline dla_vec2 dla_mat2x2_mulv(dla_mat2x2 m, dla_vec2 v) {
    return (dla_vec2){ m.m[0][0]*v.x + m.m[1][0]*v.y,
                       m.m[0][1]*v.x + m.m[1][1]*v.y };
}
 
// Transpose
static inline dla_mat2x2 dla_mat2x2_transpose(dla_mat2x2 m) {
    dla_mat2x2 r;
    r.m[0][0] = m.m[0][0]; r.m[0][1] = m.m[1][0];
    r.m[1][0] = m.m[0][1]; r.m[1][1] = m.m[1][1];
    return r;
}
// Determinant
static inline float dla_mat2x2_det(dla_mat2x2 m) {
    return m.m[0][0]*m.m[1][1] - m.m[1][0]*m.m[0][1];
}
// Inverse (returns {0} when singular)
static inline dla_mat2x2 dla_mat2x2_inverse(dla_mat2x2 m) {
    float d = dla_mat2x2_det(m);
    if (fabsf(d) < 1e-8f) return (dla_mat2x2){0};
    float inv = 1.f / d;
    dla_mat2x2 r;
    r.m[0][0] =  m.m[1][1]*inv; r.m[0][1] = -m.m[0][1]*inv;
    r.m[1][0] = -m.m[1][0]*inv; r.m[1][1] =  m.m[0][0]*inv;
    return r;
}
// Trace
static inline float dla_mat2x2_trace(dla_mat2x2 m) { return m.m[0][0] + m.m[1][1]; }
 
// Transform factories
// 2-D counter-clockwise rotation, angle in radians
static inline dla_mat2x2 dla_mat2x2_rotation(float angle) {
    float c = cosf(angle), s = sinf(angle);
    dla_mat2x2 r;
    r.m[0][0] =  c; r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
}
static inline dla_mat2x2 dla_mat2x2_scaling(float sx, float sy) {
    dla_mat2x2 r = {0};
    r.m[0][0] = sx; r.m[1][1] = sy;
    return r;
}

// ===========================
// dla_mat2x3 

// Element access
static inline float    dla_mat2x3_get(dla_mat2x3 m, int col, int row)           { return m.m[col][row]; }
static inline void     dla_mat2x3_set(dla_mat2x3 *m, int col, int row, float v) { m->m[col][row] = v;   }
static inline dla_vec2 dla_mat2x3_col(dla_mat2x3 m, int c) { return (dla_vec2){m.m[c][0], m.m[c][1]}; }

// Arithmetic
static inline dla_mat2x3 dla_mat2x3_add(dla_mat2x3 a, dla_mat2x3 b) {
    dla_mat2x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 2; j++) r.m[i][j] = a.m[i][j]+b.m[i][j];
    return r;
}
static inline dla_mat2x3 dla_mat2x3_sub(dla_mat2x3 a, dla_mat2x3 b) {
    dla_mat2x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 2; j++) r.m[i][j] = a.m[i][j]-b.m[i][j];
    return r;
}
static inline dla_mat2x3 dla_mat2x3_scale(dla_mat2x3 a, float s) {
    dla_mat2x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 2; j++) r.m[i][j] = a.m[i][j]*s;
    return r;
}

// Compose two affine transforms (a * b), respecting the implicit [0 0 1] bottom row
static inline dla_mat2x3 dla_mat2x3_mul(dla_mat2x3 a, dla_mat2x3 b) {
    dla_mat2x3 r;
    r.m[0][0] = a.m[0][0]*b.m[0][0] + a.m[1][0]*b.m[0][1];
    r.m[0][1] = a.m[0][1]*b.m[0][0] + a.m[1][1]*b.m[0][1];
    r.m[1][0] = a.m[0][0]*b.m[1][0] + a.m[1][0]*b.m[1][1];
    r.m[1][1] = a.m[0][1]*b.m[1][0] + a.m[1][1]*b.m[1][1];
    r.m[2][0] = a.m[0][0]*b.m[2][0] + a.m[1][0]*b.m[2][1] + a.m[2][0];
    r.m[2][1] = a.m[0][1]*b.m[2][0] + a.m[1][1]*b.m[2][1] + a.m[2][1];
    return r;
}

// Transform a point (w=1, includes translation)
static inline dla_vec2 dla_mat2x3_mulv(dla_mat2x3 m, dla_vec2 v) {
    return (dla_vec2){ m.m[0][0]*v.x + m.m[1][0]*v.y + m.m[2][0],
                       m.m[0][1]*v.x + m.m[1][1]*v.y + m.m[2][1] };
}

// Transform a direction (w=0, translation ignored)
static inline dla_vec2 dla_mat2x3_mul_dir(dla_mat2x3 m, dla_vec2 d) {
    return (dla_vec2){ m.m[0][0]*d.x + m.m[1][0]*d.y,
                       m.m[0][1]*d.x + m.m[1][1]*d.y };
}

// Inverse (returns {0} when singular)
// Exploits affine structure: inv(linear) for the 2×2 part, -inv(linear)*t for translation
static inline dla_mat2x3 dla_mat2x3_inverse(dla_mat2x3 m) {
    float det = m.m[0][0]*m.m[1][1] - m.m[1][0]*m.m[0][1];
    if (fabsf(det) < 1e-8f) return (dla_mat2x3){0};
    float inv = 1.f / det;
    dla_mat2x3 r;
    r.m[0][0] =  m.m[1][1]*inv;
    r.m[0][1] = -m.m[0][1]*inv;
    r.m[1][0] = -m.m[1][0]*inv;
    r.m[1][1] =  m.m[0][0]*inv;
    r.m[2][0] = -(r.m[0][0]*m.m[2][0] + r.m[1][0]*m.m[2][1]);
    r.m[2][1] = -(r.m[0][1]*m.m[2][0] + r.m[1][1]*m.m[2][1]);
    return r;
}

// Transform factories
static inline dla_mat2x3 dla_mat2x3_identity(void) {
    dla_mat2x3 r = {0};
    r.m[0][0] = 1.f; r.m[1][1] = 1.f;
    return r;
}

static inline dla_mat2x3 dla_mat2x3_translation(float tx, float ty) {
    dla_mat2x3 r = {0};
    r.m[0][0] = 1.f; r.m[1][1] = 1.f;
    r.m[2][0] = tx;  r.m[2][1] = ty;
    return r;
}

static inline dla_mat2x3 dla_mat2x3_translation_v(dla_vec2 t) {
    return dla_mat2x3_translation(t.x, t.y);
}

// 2-D counter-clockwise rotation, angle in radians
static inline dla_mat2x3 dla_mat2x3_rotation(float angle) {
    float c = cosf(angle), s = sinf(angle);
    dla_mat2x3 r = {0};
    r.m[0][0] =  c; r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
}

static inline dla_mat2x3 dla_mat2x3_scaling(float sx, float sy) {
    dla_mat2x3 r = {0};
    r.m[0][0] = sx; r.m[1][1] = sy;
    return r;
}
// TRS composite: T * R * S
static inline dla_mat2x3 dla_mat2x3_trs(dla_vec2 t, float angle, dla_vec2 s) {
    return dla_mat2x3_mul(
        dla_mat2x3_mul(
            dla_mat2x3_translation_v(t),
            dla_mat2x3_rotation(angle)
        ),
        dla_mat2x3_scaling(s.x, s.y)
    );
}

// Conversions
// Extract the linear part as a mat2x2 (drops translation column)
static inline dla_mat2x2 dla_mat2x3_to_mat2x2(dla_mat2x3 m) {
    dla_mat2x2 r;
    r.m[0][0]=m.m[0][0]; r.m[0][1]=m.m[0][1];
    r.m[1][0]=m.m[1][0]; r.m[1][1]=m.m[1][1];
    return r;
}
// Embed into a mat3x3 with explicit [0 0 1] bottom row
static inline dla_mat3x3 dla_mat2x3_to_mat3x3(dla_mat2x3 m) {
    dla_mat3x3 r = {0};
    r.m[0][0]=m.m[0][0]; r.m[0][1]=m.m[0][1];
    r.m[1][0]=m.m[1][0]; r.m[1][1]=m.m[1][1];
    r.m[2][0]=m.m[2][0]; r.m[2][1]=m.m[2][1];
    r.m[2][2]=1.f;
    return r;
}
// Extract from a mat3x3 (drops the bottom row)
static inline dla_mat2x3 dla_mat2x3_from_mat3x3(dla_mat3x3 m) {
    dla_mat2x3 r;
    r.m[0][0]=m.m[0][0]; r.m[0][1]=m.m[0][1];
    r.m[1][0]=m.m[1][0]; r.m[1][1]=m.m[1][1];
    r.m[2][0]=m.m[2][0]; r.m[2][1]=m.m[2][1];
    return r;
}
 
// ===========================
// dla_mat3x3
 
// Element access
static inline float    dla_mat3x3_get(dla_mat3x3 m, int col, int row)           { return m.m[col][row]; }
static inline void     dla_mat3x3_set(dla_mat3x3 *m, int col, int row, float v) { m->m[col][row] = v;   }
static inline dla_vec3 dla_mat3x3_col(dla_mat3x3 m, int c) {
    return (dla_vec3){m.m[c][0], m.m[c][1], m.m[c][2]};
}
static inline dla_vec3 dla_mat3x3_row(dla_mat3x3 m, int r) {
    return (dla_vec3){m.m[0][r], m.m[1][r], m.m[2][r]};
}
 
// Arithmetic
static inline dla_mat3x3 dla_mat3x3_add(dla_mat3x3 a, dla_mat3x3 b) {
    dla_mat3x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i][j] = a.m[i][j]+b.m[i][j];
    return r;
}
static inline dla_mat3x3 dla_mat3x3_sub(dla_mat3x3 a, dla_mat3x3 b) {
    dla_mat3x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i][j] = a.m[i][j]-b.m[i][j];
    return r;
}
static inline dla_mat3x3 dla_mat3x3_scale(dla_mat3x3 a, float s) {
    dla_mat3x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i][j] = a.m[i][j]*s;
    return r;
}
// Matrix x matrix
static inline dla_mat3x3 dla_mat3x3_mul(dla_mat3x3 a, dla_mat3x3 b) {
    dla_mat3x3 r = {0}; int i, j, k;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) for (k = 0; k < 3; k++)
        r.m[i][j] += a.m[k][j] * b.m[i][k];
    return r;
}
// Matrix x vector
static inline dla_vec3 dla_mat3x3_mulv(dla_mat3x3 m, dla_vec3 v) {
    return (dla_vec3){ m.m[0][0]*v.x + m.m[1][0]*v.y + m.m[2][0]*v.z,
                       m.m[0][1]*v.x + m.m[1][1]*v.y + m.m[2][1]*v.z,
                       m.m[0][2]*v.x + m.m[1][2]*v.y + m.m[2][2]*v.z };
}
 
// Transpose
static inline dla_mat3x3 dla_mat3x3_transpose(dla_mat3x3 m) {
    dla_mat3x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i][j] = m.m[j][i];
    return r;
}
// Determinant
static inline float dla_mat3x3_det(dla_mat3x3 m) {
    return m.m[0][0]*(m.m[1][1]*m.m[2][2] - m.m[2][1]*m.m[1][2])
          -m.m[1][0]*(m.m[0][1]*m.m[2][2] - m.m[2][1]*m.m[0][2])
          +m.m[2][0]*(m.m[0][1]*m.m[1][2] - m.m[1][1]*m.m[0][2]);
}
// Inverse (returns {0} when singular)
static inline dla_mat3x3 dla_mat3x3_inverse(dla_mat3x3 m) {
    float d = dla_mat3x3_det(m);
    if (fabsf(d) < 1e-8f) return (dla_mat3x3){0};
    float inv = 1.f / d;
    dla_mat3x3 r;
    r.m[0][0] = (m.m[1][1]*m.m[2][2] - m.m[2][1]*m.m[1][2])*inv;
    r.m[0][1] =-(m.m[0][1]*m.m[2][2] - m.m[2][1]*m.m[0][2])*inv;
    r.m[0][2] = (m.m[0][1]*m.m[1][2] - m.m[1][1]*m.m[0][2])*inv;
    r.m[1][0] =-(m.m[1][0]*m.m[2][2] - m.m[2][0]*m.m[1][2])*inv;
    r.m[1][1] = (m.m[0][0]*m.m[2][2] - m.m[2][0]*m.m[0][2])*inv;
    r.m[1][2] =-(m.m[0][0]*m.m[1][2] - m.m[1][0]*m.m[0][2])*inv;
    r.m[2][0] = (m.m[1][0]*m.m[2][1] - m.m[2][0]*m.m[1][1])*inv;
    r.m[2][1] =-(m.m[0][0]*m.m[2][1] - m.m[2][0]*m.m[0][1])*inv;
    r.m[2][2] = (m.m[0][0]*m.m[1][1] - m.m[1][0]*m.m[0][1])*inv;
    return r;
}
// Trace
static inline float dla_mat3x3_trace(dla_mat3x3 m) { return m.m[0][0] + m.m[1][1] + m.m[2][2]; }
// Normal matrix: transpose of inverse (for transforming surface normals)
static inline dla_mat3x3 dla_mat3x3_normal_matrix(dla_mat3x3 m) {
    return dla_mat3x3_transpose(dla_mat3x3_inverse(m));
}
 
// Transform factories
// Rotation around an arbitrary axis (Rodrigues), angle in radians
static inline dla_mat3x3 dla_mat3x3_rotation(dla_vec3 axis, float angle) {
    dla_vec3 u = dla_vec3_normalize(axis);
    float c = cosf(angle), s = sinf(angle), t = 1.f - c;
    float x = u.x, y = u.y, z = u.z;
    dla_mat3x3 r;
    r.m[0][0] = t*x*x+c;   r.m[0][1] = t*x*y+s*z; r.m[0][2] = t*x*z-s*y;
    r.m[1][0] = t*x*y-s*z; r.m[1][1] = t*y*y+c;   r.m[1][2] = t*y*z+s*x;
    r.m[2][0] = t*x*z+s*y; r.m[2][1] = t*y*z-s*x; r.m[2][2] = t*z*z+c;
    return r;
}
// Euler angles ZYX convention, angles in radians
static inline dla_mat3x3 dla_mat3x3_from_euler(float rx, float ry, float rz) {
    dla_mat3x3 Rx = dla_mat3x3_rotation((dla_vec3){1,0,0}, rx);
    dla_mat3x3 Ry = dla_mat3x3_rotation((dla_vec3){0,1,0}, ry);
    dla_mat3x3 Rz = dla_mat3x3_rotation((dla_vec3){0,0,1}, rz);
    return dla_mat3x3_mul(dla_mat3x3_mul(Rz, Ry), Rx);
}
static inline dla_mat3x3 dla_mat3x3_scaling(float sx, float sy, float sz) {
    dla_mat3x3 r = {0};
    r.m[0][0] = sx; r.m[1][1] = sy; r.m[2][2] = sz;
    return r;
}
 
// Promotion from mat2x2 (upper-left embed, rest = identity)
static inline dla_mat3x3 dla_mat3x3_from_mat2x2(dla_mat2x2 a) {
    dla_mat3x3 r = {0};
    r.m[0][0]=a.m[0][0]; r.m[0][1]=a.m[0][1];
    r.m[1][0]=a.m[1][0]; r.m[1][1]=a.m[1][1];
    r.m[2][2]=1.f;
    return r;
}
 
// ===========================
// dla_mat4x4
 
// Element access
static inline float    dla_mat4x4_get(dla_mat4x4 m, int col, int row)           { return m.m[col][row]; }
static inline void     dla_mat4x4_set(dla_mat4x4 *m, int col, int row, float v) { m->m[col][row] = v;   }
static inline dla_vec4 dla_mat4x4_col(dla_mat4x4 m, int c) {
    return (dla_vec4){m.m[c][0], m.m[c][1], m.m[c][2], m.m[c][3]};
}
static inline dla_vec4 dla_mat4x4_row(dla_mat4x4 m, int r) {
    return (dla_vec4){m.m[0][r], m.m[1][r], m.m[2][r], m.m[3][r]};
}
 
// Arithmetic
static inline dla_mat4x4 dla_mat4x4_add(dla_mat4x4 a, dla_mat4x4 b) {
    dla_mat4x4 r; int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) r.m[i][j] = a.m[i][j]+b.m[i][j];
    return r;
}
static inline dla_mat4x4 dla_mat4x4_sub(dla_mat4x4 a, dla_mat4x4 b) {
    dla_mat4x4 r; int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) r.m[i][j] = a.m[i][j]-b.m[i][j];
    return r;
}
static inline dla_mat4x4 dla_mat4x4_scale(dla_mat4x4 a, float s) {
    dla_mat4x4 r; int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) r.m[i][j] = a.m[i][j]*s;
    return r;
}
// Matrix x matrix
static inline dla_mat4x4 dla_mat4x4_mul(dla_mat4x4 a, dla_mat4x4 b) {
    dla_mat4x4 r = {0}; int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) for (k = 0; k < 4; k++)
        r.m[i][j] += a.m[k][j] * b.m[i][k];
    return r;
}
// Matrix x vector
static inline dla_vec4 dla_mat4x4_mulv(dla_mat4x4 m, dla_vec4 v) {
    return (dla_vec4){ m.m[0][0]*v.x + m.m[1][0]*v.y + m.m[2][0]*v.z + m.m[3][0]*v.w,
                       m.m[0][1]*v.x + m.m[1][1]*v.y + m.m[2][1]*v.z + m.m[3][1]*v.w,
                       m.m[0][2]*v.x + m.m[1][2]*v.y + m.m[2][2]*v.z + m.m[3][2]*v.w,
                       m.m[0][3]*v.x + m.m[1][3]*v.y + m.m[2][3]*v.z + m.m[3][3]*v.w };
}
// Transform a point (w=1, includes translation)
static inline dla_vec3 dla_mat4x4_mul_point(dla_mat4x4 m, dla_vec3 p) {
    return dla_vec4_pdiv(dla_mat4x4_mulv(m, (dla_vec4){p.x, p.y, p.z, 1.f}));
}
// Transform a direction (w=0, translation ignored)
static inline dla_vec3 dla_mat4x4_mul_dir(dla_mat4x4 m, dla_vec3 d) {
    dla_vec4 r = dla_mat4x4_mulv(m, (dla_vec4){d.x, d.y, d.z, 0.f});
    return (dla_vec3){r.x, r.y, r.z};
}
 
// Transpose
static inline dla_mat4x4 dla_mat4x4_transpose(dla_mat4x4 m) {
    dla_mat4x4 r; int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) r.m[i][j] = m.m[j][i];
    return r;
}
// Trace
static inline float dla_mat4x4_trace(dla_mat4x4 m) {
    return m.m[0][0] + m.m[1][1] + m.m[2][2] + m.m[3][3];
}
 
// Determinant
static inline float dla_mat4x4_det(dla_mat4x4 m) {
#define E(c,r) m.m[c][r]
    float s0 = E(0,0)*E(1,1) - E(1,0)*E(0,1);
    float s1 = E(0,0)*E(2,1) - E(2,0)*E(0,1);
    float s2 = E(0,0)*E(3,1) - E(3,0)*E(0,1);
    float s3 = E(1,0)*E(2,1) - E(2,0)*E(1,1);
    float s4 = E(1,0)*E(3,1) - E(3,0)*E(1,1);
    float s5 = E(2,0)*E(3,1) - E(3,0)*E(2,1);
    float c5 = E(2,2)*E(3,3) - E(3,2)*E(2,3);
    float c4 = E(1,2)*E(3,3) - E(3,2)*E(1,3);
    float c3 = E(1,2)*E(2,3) - E(2,2)*E(1,3);
    float c2 = E(0,2)*E(3,3) - E(3,2)*E(0,3);
    float c1 = E(0,2)*E(2,3) - E(2,2)*E(0,3);
    float c0 = E(0,2)*E(1,3) - E(1,2)*E(0,3);
#undef E
    return s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0;
}
 
// Inverse (returns {0} when singular)
static inline dla_mat4x4 dla_mat4x4_inverse(dla_mat4x4 m) {
#define E(c,r) m.m[c][r]
    float s0 = E(0,0)*E(1,1) - E(1,0)*E(0,1);
    float s1 = E(0,0)*E(2,1) - E(2,0)*E(0,1);
    float s2 = E(0,0)*E(3,1) - E(3,0)*E(0,1);
    float s3 = E(1,0)*E(2,1) - E(2,0)*E(1,1);
    float s4 = E(1,0)*E(3,1) - E(3,0)*E(1,1);
    float s5 = E(2,0)*E(3,1) - E(3,0)*E(2,1);
    float c5 = E(2,2)*E(3,3) - E(3,2)*E(2,3);
    float c4 = E(1,2)*E(3,3) - E(3,2)*E(1,3);
    float c3 = E(1,2)*E(2,3) - E(2,2)*E(1,3);
    float c2 = E(0,2)*E(3,3) - E(3,2)*E(0,3);
    float c1 = E(0,2)*E(2,3) - E(2,2)*E(0,3);
    float c0 = E(0,2)*E(1,3) - E(1,2)*E(0,3);
    float det = s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0;
    if (fabsf(det) < 1e-8f) return (dla_mat4x4){0};
    float inv = 1.f / det;
    dla_mat4x4 r;
    r.m[0][0] = ( E(1,1)*c5 - E(2,1)*c4 + E(3,1)*c3)*inv;
    r.m[0][1] = (-E(0,1)*c5 + E(2,1)*c2 - E(3,1)*c1)*inv;
    r.m[0][2] = ( E(0,1)*c4 - E(1,1)*c2 + E(3,1)*c0)*inv;
    r.m[0][3] = (-E(0,1)*c3 + E(1,1)*c1 - E(2,1)*c0)*inv;
    r.m[1][0] = (-E(1,0)*c5 + E(2,0)*c4 - E(3,0)*c3)*inv;
    r.m[1][1] = ( E(0,0)*c5 - E(2,0)*c2 + E(3,0)*c1)*inv;
    r.m[1][2] = (-E(0,0)*c4 + E(1,0)*c2 - E(3,0)*c0)*inv;
    r.m[1][3] = ( E(0,0)*c3 - E(1,0)*c1 + E(2,0)*c0)*inv;
    r.m[2][0] = ( E(1,3)*s5 - E(2,3)*s4 + E(3,3)*s3)*inv;
    r.m[2][1] = (-E(0,3)*s5 + E(2,3)*s2 - E(3,3)*s1)*inv;
    r.m[2][2] = ( E(0,3)*s4 - E(1,3)*s2 + E(3,3)*s0)*inv;
    r.m[2][3] = (-E(0,3)*s3 + E(1,3)*s1 - E(2,3)*s0)*inv;
    r.m[3][0] = (-E(1,2)*s5 + E(2,2)*s4 - E(3,2)*s3)*inv;
    r.m[3][1] = ( E(0,2)*s5 - E(2,2)*s2 + E(3,2)*s1)*inv;
    r.m[3][2] = (-E(0,2)*s4 + E(1,2)*s2 - E(3,2)*s0)*inv;
    r.m[3][3] = ( E(0,2)*s3 - E(1,2)*s1 + E(2,2)*s0)*inv;
#undef E
    return r;
}
 
// Transform factories
 
// Translation
static inline dla_mat4x4 dla_mat4x4_translation(float tx, float ty, float tz) {
    dla_mat4x4 r = {0};
    r.m[0][0]=1.f; r.m[1][1]=1.f; r.m[2][2]=1.f; r.m[3][3]=1.f;
    r.m[3][0]=tx;  r.m[3][1]=ty;  r.m[3][2]=tz;
    return r;
}
static inline dla_mat4x4 dla_mat4x4_translation_v(dla_vec3 t) {
    return dla_mat4x4_translation(t.x, t.y, t.z);
}
 
// Scaling
static inline dla_mat4x4 dla_mat4x4_scaling(float sx, float sy, float sz) {
    dla_mat4x4 r = {0};
    r.m[0][0]=sx; r.m[1][1]=sy; r.m[2][2]=sz; r.m[3][3]=1.f;
    return r;
}
static inline dla_mat4x4 dla_mat4x4_scaling_v(dla_vec3 s) {
    return dla_mat4x4_scaling(s.x, s.y, s.z);
}
 
// Embed mat3x3 in upper-left of mat4x4 (rest = identity)
static inline dla_mat4x4 dla_mat4x4_from_mat3x3(dla_mat3x3 a) {
    dla_mat4x4 r = {0}; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i][j] = a.m[i][j];
    r.m[3][3] = 1.f;
    return r;
}
 
// Rotation around an arbitrary axis, angle in radians
static inline dla_mat4x4 dla_mat4x4_rotation(dla_vec3 axis, float angle) {
    return dla_mat4x4_from_mat3x3(dla_mat3x3_rotation(axis, angle));
}
 
// Convenience axis rotations
static inline dla_mat4x4 dla_mat4x4_rotation_x(float a) {
    return dla_mat4x4_rotation((dla_vec3){1,0,0}, a);
}
static inline dla_mat4x4 dla_mat4x4_rotation_y(float a) {
    return dla_mat4x4_rotation((dla_vec3){0,1,0}, a);
}
static inline dla_mat4x4 dla_mat4x4_rotation_z(float a) {
    return dla_mat4x4_rotation((dla_vec3){0,0,1}, a);
}
 
// TRS composite: T * R * S
static inline dla_mat4x4 dla_mat4x4_trs(dla_vec3 t, dla_vec3 axis, float angle, dla_vec3 s) {
    return dla_mat4x4_mul(
        dla_mat4x4_mul(
            dla_mat4x4_translation_v(t),
            dla_mat4x4_rotation(axis, angle)
        ),
        dla_mat4x4_scaling_v(s)
    );
}
 
// Look-at (right-handed, camera looks toward -Z)
static inline dla_mat4x4 dla_mat4x4_lookat(dla_vec3 eye, dla_vec3 center, dla_vec3 up) {
    dla_vec3 f = dla_vec3_normalize(dla_vec3_sub(center, eye));
    dla_vec3 r = dla_vec3_normalize(dla_vec3_cross(f, up));
    dla_vec3 u = dla_vec3_cross(r, f);
    dla_mat4x4 m = {0};
    m.m[0][0]= r.x; m.m[1][0]= r.y; m.m[2][0]= r.z;
    m.m[0][1]= u.x; m.m[1][1]= u.y; m.m[2][1]= u.z;
    m.m[0][2]=-f.x; m.m[1][2]=-f.y; m.m[2][2]=-f.z;
    m.m[3][0]=-dla_vec3_dot(r, eye);
    m.m[3][1]=-dla_vec3_dot(u, eye);
    m.m[3][2]= dla_vec3_dot(f, eye);
    m.m[3][3]= 1.f;
    return m;
}
 
// Perspective projection (right-handed, NDC depth in [-1, 1])
static inline dla_mat4x4 dla_mat4x4_perspective
(float fovy_rad, float aspect, float near, float far) {
    float f   = 1.f / tanf(fovy_rad * 0.5f);
    float rng = 1.f / (near - far);
    dla_mat4x4 m = {0};
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (near + far) * rng;
    m.m[2][3] = -1.f;
    m.m[3][2] = 2.f * near * far * rng;
    return m;
}
 
// Orthographic projection (right-handed, NDC depth in [-1, 1])
static inline dla_mat4x4 dla_mat4x4_ortho
(float left, float right, float bottom, float top, float near, float far) {
    dla_mat4x4 m = {0};
    m.m[0][0] =  2.f / (right - left);
    m.m[1][1] =  2.f / (top - bottom);
    m.m[2][2] = -2.f / (far - near);
    m.m[3][0] = -(right + left)   / (right - left);
    m.m[3][1] = -(top   + bottom) / (top   - bottom);
    m.m[3][2] = -(far   + near)   / (far   - near);
    m.m[3][3] =  1.f;
    return m;
}
 
// ===========================
// Cross-size promotions
 
// Extract upper-left 3x3 from a mat4x4
static inline dla_mat3x3 dla_mat4x4_to_mat3x3(dla_mat4x4 a) {
    dla_mat3x3 r; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i][j] = a.m[i][j];
    return r;
}
// Embed mat2x2 in the upper-left of a mat4x4 (rest = identity)
static inline dla_mat4x4 dla_mat4x4_from_mat2x2(dla_mat2x2 a) {
    return dla_mat4x4_from_mat3x3(dla_mat3x3_from_mat2x2(a));
}
 
#endif // DEMIGURG_LINEAR_ALGEBRA_H
