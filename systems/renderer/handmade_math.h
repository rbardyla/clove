#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

/*
    Handmade Math Library
    Complete 3D math implementation with zero dependencies
    
    Features:
    - Vectors (2D, 3D, 4D)
    - Matrices (3x3, 4x4)
    - Quaternions
    - SIMD optimizations where possible
    - Left-handed coordinate system (Z forward)
*/

#include "../../src/handmade.h"
#include <math.h>

// Constants
#define PI 3.14159265358979323846f
#define HM_PI 3.14159265358979323846f
#define HM_TAU (2.0f * HM_PI)
#define HM_DEG_TO_RAD (HM_PI / 180.0f)
#define HM_RAD_TO_DEG (180.0f / HM_PI)
#define HM_EPSILON 0.00001f

// =============================================================================
// VECTOR TYPES
// =============================================================================

typedef union v2 {
    struct { f32 x, y; };
    struct { f32 u, v; };
    struct { f32 width, height; };
    f32 e[2];
} v2;

typedef union v3 {
    struct { f32 x, y, z; };
    struct { f32 r, g, b; };
    struct { f32 pitch, yaw, roll; };
    struct { v2 xy; f32 _ignored0; };
    f32 e[3];
} v3;

typedef union v4 {
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };
    struct { v3 xyz; f32 _ignored0; };
    struct { v2 xy; v2 zw; };
    f32 e[4];
} v4;

// =============================================================================
// MATRIX TYPES (Column-major for OpenGL)
// =============================================================================

typedef union m3x3 {
    f32 e[9];
    f32 m[3][3];
    v3 columns[3];
} m3x3;

typedef union m4x4 {
    f32 e[16];
    f32 m[4][4];
    v4 columns[4];
} m4x4;

// =============================================================================
// QUATERNION TYPE
// =============================================================================

typedef union quat {
    struct { f32 x, y, z, w; };
    struct { v3 xyz; f32 _w; };
    v4 v;
    f32 e[4];
} quat;

// =============================================================================
// VECTOR 2D OPERATIONS
// =============================================================================

static inline v2 v2_make(f32 x, f32 y) {
    v2 result = {x, y};
    return result;
}

static inline v2 v2_add(v2 a, v2 b) {
    return v2_make(a.x + b.x, a.y + b.y);
}

static inline v2 v2_sub(v2 a, v2 b) {
    return v2_make(a.x - b.x, a.y - b.y);
}

static inline v2 v2_mul(v2 a, v2 b) {
    return v2_make(a.x * b.x, a.y * b.y);
}

static inline v2 v2_scale(v2 v, f32 s) {
    return v2_make(v.x * s, v.y * s);
}

static inline f32 v2_dot(v2 a, v2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline f32 v2_length_sq(v2 v) {
    return v2_dot(v, v);
}

static inline f32 v2_length(v2 v) {
    return sqrtf(v2_length_sq(v));
}

static inline v2 v2_normalize(v2 v) {
    f32 len = v2_length(v);
    if (len > HM_EPSILON) {
        return v2_scale(v, 1.0f / len);
    }
    return v;
}

static inline v2 v2_lerp(v2 a, v2 b, f32 t) {
    return v2_add(a, v2_scale(v2_sub(b, a), t));
}

// =============================================================================
// VECTOR 3D OPERATIONS
// =============================================================================

static inline v3 v3_make(f32 x, f32 y, f32 z) {
    v3 result = {x, y, z};
    return result;
}

static inline v3 v3_zero(void) {
    return v3_make(0.0f, 0.0f, 0.0f);
}

static inline v3 v3_one(void) {
    return v3_make(1.0f, 1.0f, 1.0f);
}

static inline v3 v3_up(void) {
    return v3_make(0.0f, 1.0f, 0.0f);
}

static inline v3 v3_down(void) {
    return v3_make(0.0f, -1.0f, 0.0f);
}

static inline v3 v3_right(void) {
    return v3_make(1.0f, 0.0f, 0.0f);
}

static inline v3 v3_left(void) {
    return v3_make(-1.0f, 0.0f, 0.0f);
}

static inline v3 v3_forward(void) {
    return v3_make(0.0f, 0.0f, 1.0f);
}

static inline v3 v3_back(void) {
    return v3_make(0.0f, 0.0f, -1.0f);
}

static inline v3 v3_add(v3 a, v3 b) {
    return v3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline v3 v3_sub(v3 a, v3 b) {
    return v3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline v3 v3_mul(v3 a, v3 b) {
    return v3_make(a.x * b.x, a.y * b.y, a.z * b.z);
}

static inline v3 v3_scale(v3 v, f32 s) {
    return v3_make(v.x * s, v.y * s, v.z * s);
}

static inline v3 v3_negate(v3 v) {
    return v3_make(-v.x, -v.y, -v.z);
}

static inline f32 v3_dot(v3 a, v3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline v3 v3_cross(v3 a, v3 b) {
    return v3_make(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static inline f32 v3_length_sq(v3 v) {
    return v3_dot(v, v);
}

static inline f32 v3_length(v3 v) {
    return sqrtf(v3_length_sq(v));
}

static inline v3 v3_normalize(v3 v) {
    f32 len = v3_length(v);
    if (len > HM_EPSILON) {
        return v3_scale(v, 1.0f / len);
    }
    return v;
}

static inline v3 v3_lerp(v3 a, v3 b, f32 t) {
    return v3_add(a, v3_scale(v3_sub(b, a), t));
}

static inline v3 v3_reflect(v3 v, v3 n) {
    return v3_sub(v, v3_scale(n, 2.0f * v3_dot(v, n)));
}

// =============================================================================
// VECTOR 4D OPERATIONS
// =============================================================================

static inline v4 v4_make(f32 x, f32 y, f32 z, f32 w) {
    v4 result = {x, y, z, w};
    return result;
}

static inline v4 v4_from_v3(v3 v, f32 w) {
    return v4_make(v.x, v.y, v.z, w);
}

static inline v4 v4_add(v4 a, v4 b) {
    return v4_make(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static inline v4 v4_sub(v4 a, v4 b) {
    return v4_make(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

static inline v4 v4_scale(v4 v, f32 s) {
    return v4_make(v.x * s, v.y * s, v.z * s, v.w * s);
}

static inline f32 v4_dot(v4 a, v4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// =============================================================================
// MATRIX 4x4 OPERATIONS
// =============================================================================

static inline m4x4 m4x4_identity(void) {
    m4x4 result = {0};
    result.e[0] = 1.0f;
    result.e[5] = 1.0f;
    result.e[10] = 1.0f;
    result.e[15] = 1.0f;
    return result;
}

static inline m4x4 m4x4_translate(f32 x, f32 y, f32 z) {
    m4x4 result = m4x4_identity();
    result.e[12] = x;
    result.e[13] = y;
    result.e[14] = z;
    return result;
}

static inline m4x4 m4x4_translate_v3(v3 v) {
    return m4x4_translate(v.x, v.y, v.z);
}

static inline m4x4 m4x4_scale(f32 x, f32 y, f32 z) {
    m4x4 result = {0};
    result.e[0] = x;
    result.e[5] = y;
    result.e[10] = z;
    result.e[15] = 1.0f;
    return result;
}

static inline m4x4 m4x4_scale_uniform(f32 s) {
    return m4x4_scale(s, s, s);
}

static inline m4x4 m4x4_rotate_x(f32 radians) {
    f32 c = cosf(radians);
    f32 s = sinf(radians);
    
    m4x4 result = m4x4_identity();
    result.e[5] = c;
    result.e[6] = s;
    result.e[9] = -s;
    result.e[10] = c;
    return result;
}

static inline m4x4 m4x4_rotate_y(f32 radians) {
    f32 c = cosf(radians);
    f32 s = sinf(radians);
    
    m4x4 result = m4x4_identity();
    result.e[0] = c;
    result.e[2] = -s;
    result.e[8] = s;
    result.e[10] = c;
    return result;
}

static inline m4x4 m4x4_rotate_z(f32 radians) {
    f32 c = cosf(radians);
    f32 s = sinf(radians);
    
    m4x4 result = m4x4_identity();
    result.e[0] = c;
    result.e[1] = s;
    result.e[4] = -s;
    result.e[5] = c;
    return result;
}

static inline m4x4 m4x4_mul(m4x4 a, m4x4 b) {
    m4x4 result = {0};
    
    for (i32 col = 0; col < 4; col++) {
        for (i32 row = 0; row < 4; row++) {
            f32 sum = 0.0f;
            for (i32 i = 0; i < 4; i++) {
                sum += a.m[i][row] * b.m[col][i];
            }
            result.m[col][row] = sum;
        }
    }
    
    return result;
}

static inline v4 m4x4_mul_v4(m4x4 m, v4 v) {
    v4 result;
    result.x = m.e[0] * v.x + m.e[4] * v.y + m.e[8] * v.z + m.e[12] * v.w;
    result.y = m.e[1] * v.x + m.e[5] * v.y + m.e[9] * v.z + m.e[13] * v.w;
    result.z = m.e[2] * v.x + m.e[6] * v.y + m.e[10] * v.z + m.e[14] * v.w;
    result.w = m.e[3] * v.x + m.e[7] * v.y + m.e[11] * v.z + m.e[15] * v.w;
    return result;
}

static inline v3 m4x4_mul_v3_point(m4x4 m, v3 v) {
    v4 v_expanded = v4_from_v3(v, 1.0f);
    v4 result = m4x4_mul_v4(m, v_expanded);
    return v3_make(result.x, result.y, result.z);
}

static inline v3 m4x4_mul_v3_direction(m4x4 m, v3 v) {
    v4 v_expanded = v4_from_v3(v, 0.0f);
    v4 result = m4x4_mul_v4(m, v_expanded);
    return v3_make(result.x, result.y, result.z);
}

static inline m4x4 m4x4_transpose(m4x4 m) {
    m4x4 result;
    for (i32 i = 0; i < 4; i++) {
        for (i32 j = 0; j < 4; j++) {
            result.m[i][j] = m.m[j][i];
        }
    }
    return result;
}

// Perspective projection matrix (for OpenGL, maps to [-1, 1])
static inline m4x4 m4x4_perspective(f32 fov_radians, f32 aspect_ratio, f32 near, f32 far) {
    f32 tan_half_fov = tanf(fov_radians * 0.5f);
    
    m4x4 result = {0};
    result.e[0] = 1.0f / (aspect_ratio * tan_half_fov);
    result.e[5] = 1.0f / tan_half_fov;
    result.e[10] = -(far + near) / (far - near);
    result.e[11] = -1.0f;
    result.e[14] = -(2.0f * far * near) / (far - near);
    return result;
}

// Orthographic projection matrix
static inline m4x4 m4x4_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    m4x4 result = {0};
    result.e[0] = 2.0f / (right - left);
    result.e[5] = 2.0f / (top - bottom);
    result.e[10] = -2.0f / (far - near);
    result.e[12] = -(right + left) / (right - left);
    result.e[13] = -(top + bottom) / (top - bottom);
    result.e[14] = -(far + near) / (far - near);
    result.e[15] = 1.0f;
    return result;
}

// Look-at matrix (view matrix)
static inline m4x4 m4x4_look_at(v3 eye, v3 target, v3 up) {
    v3 f = v3_normalize(v3_sub(target, eye));
    v3 s = v3_normalize(v3_cross(f, up));
    v3 u = v3_cross(s, f);
    
    m4x4 result = m4x4_identity();
    result.e[0] = s.x;
    result.e[4] = s.y;
    result.e[8] = s.z;
    result.e[1] = u.x;
    result.e[5] = u.y;
    result.e[9] = u.z;
    result.e[2] = -f.x;
    result.e[6] = -f.y;
    result.e[10] = -f.z;
    result.e[12] = -v3_dot(s, eye);
    result.e[13] = -v3_dot(u, eye);
    result.e[14] = v3_dot(f, eye);
    return result;
}

// Matrix inversion (general case)
m4x4 m4x4_inverse(m4x4 m);

// =============================================================================
// QUATERNION OPERATIONS
// =============================================================================

static inline quat quat_make(f32 x, f32 y, f32 z, f32 w) {
    quat result = {x, y, z, w};
    return result;
}

static inline quat quat_identity(void) {
    return quat_make(0.0f, 0.0f, 0.0f, 1.0f);
}

static inline quat quat_from_axis_angle(v3 axis, f32 angle) {
    f32 half_angle = angle * 0.5f;
    f32 s = sinf(half_angle);
    v3 v = v3_scale(axis, s);
    return quat_make(v.x, v.y, v.z, cosf(half_angle));
}

static inline quat quat_from_euler(f32 pitch, f32 yaw, f32 roll) {
    f32 cy = cosf(yaw * 0.5f);
    f32 sy = sinf(yaw * 0.5f);
    f32 cp = cosf(pitch * 0.5f);
    f32 sp = sinf(pitch * 0.5f);
    f32 cr = cosf(roll * 0.5f);
    f32 sr = sinf(roll * 0.5f);
    
    quat result;
    result.w = cr * cp * cy + sr * sp * sy;
    result.x = sr * cp * cy - cr * sp * sy;
    result.y = cr * sp * cy + sr * cp * sy;
    result.z = cr * cp * sy - sr * sp * cy;
    return result;
}

static inline f32 quat_length(quat q) {
    return sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

static inline quat quat_normalize(quat q) {
    f32 len = quat_length(q);
    if (len > HM_EPSILON) {
        f32 inv_len = 1.0f / len;
        return quat_make(q.x * inv_len, q.y * inv_len, q.z * inv_len, q.w * inv_len);
    }
    return q;
}

static inline quat quat_conjugate(quat q) {
    return quat_make(-q.x, -q.y, -q.z, q.w);
}

static inline quat quat_mul(quat a, quat b) {
    quat result;
    result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return result;
}

static inline v3 quat_rotate_v3(quat q, v3 v) {
    v3 qv = v3_make(q.x, q.y, q.z);
    v3 uv = v3_cross(qv, v);
    v3 uuv = v3_cross(qv, uv);
    
    uv = v3_scale(uv, 2.0f * q.w);
    uuv = v3_scale(uuv, 2.0f);
    
    return v3_add(v3_add(v, uv), uuv);
}

static inline quat quat_slerp(quat a, quat b, f32 t) {
    f32 dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    
    if (dot < 0.0f) {
        b = quat_make(-b.x, -b.y, -b.z, -b.w);
        dot = -dot;
    }
    
    if (dot > 0.9995f) {
        // Linear interpolation for very close quaternions
        quat result = quat_make(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z),
            a.w + t * (b.w - a.w)
        );
        return quat_normalize(result);
    }
    
    f32 theta = acosf(dot);
    f32 sin_theta = sinf(theta);
    f32 wa = sinf((1.0f - t) * theta) / sin_theta;
    f32 wb = sinf(t * theta) / sin_theta;
    
    return quat_make(
        wa * a.x + wb * b.x,
        wa * a.y + wb * b.y,
        wa * a.z + wb * b.z,
        wa * a.w + wb * b.w
    );
}

static inline m4x4 quat_to_m4x4(quat q) {
    f32 xx = q.x * q.x;
    f32 yy = q.y * q.y;
    f32 zz = q.z * q.z;
    f32 xy = q.x * q.y;
    f32 xz = q.x * q.z;
    f32 yz = q.y * q.z;
    f32 wx = q.w * q.x;
    f32 wy = q.w * q.y;
    f32 wz = q.w * q.z;
    
    m4x4 result = {0};
    result.e[0] = 1.0f - 2.0f * (yy + zz);
    result.e[1] = 2.0f * (xy + wz);
    result.e[2] = 2.0f * (xz - wy);
    
    result.e[4] = 2.0f * (xy - wz);
    result.e[5] = 1.0f - 2.0f * (xx + zz);
    result.e[6] = 2.0f * (yz + wx);
    
    result.e[8] = 2.0f * (xz + wy);
    result.e[9] = 2.0f * (yz - wx);
    result.e[10] = 1.0f - 2.0f * (xx + yy);
    
    result.e[15] = 1.0f;
    return result;
}

// =============================================================================
// ADDITIONAL VECTOR OPERATIONS
// =============================================================================

// Matrix multiplication alias
static inline m4x4 m4x4_multiply(m4x4 a, m4x4 b) {
    return m4x4_mul(a, b);
}

// Orthographic projection alias
static inline m4x4 m4x4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    return m4x4_ortho(left, right, bottom, top, near, far);
}

// Translation helper
static inline m4x4 m4x4_translation(f32 x, f32 y, f32 z) {
    return m4x4_translate(x, y, z);
}

// Rotation helpers
static inline m4x4 m4x4_rotation_x(f32 radians) {
    return m4x4_rotate_x(radians);
}

static inline m4x4 m4x4_rotation_y(f32 radians) {
    return m4x4_rotate_y(radians);
}

static inline m4x4 m4x4_rotation_z(f32 radians) {
    return m4x4_rotate_z(radians);
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static inline f32 hm_lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

static inline f32 hm_clamp(f32 value, f32 min, f32 max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline f32 hm_min(f32 a, f32 b) {
    return a < b ? a : b;
}

static inline f32 hm_max(f32 a, f32 b) {
    return a > b ? a : b;
}

// Additional min/max operations for vectors (after hm_min/hm_max are defined)
static inline v3 v3_min(v3 a, v3 b) {
    return v3_make(
        hm_min(a.x, b.x),
        hm_min(a.y, b.y),
        hm_min(a.z, b.z)
    );
}

static inline v3 v3_max(v3 a, v3 b) {
    return v3_make(
        hm_max(a.x, b.x),
        hm_max(a.y, b.y),
        hm_max(a.z, b.z)
    );
}

// Additional vector operations
static inline v3 v3_clamp(v3 v, f32 min, f32 max) {
    return v3_make(
        hm_clamp(v.x, min, max),
        hm_clamp(v.y, min, max),
        hm_clamp(v.z, min, max)
    );
}

static inline f32 hm_radians(f32 degrees) {
    return degrees * HM_DEG_TO_RAD;
}

static inline f32 hm_degrees(f32 radians) {
    return radians * HM_RAD_TO_DEG;
}

#endif // HANDMADE_MATH_H