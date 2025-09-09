// game_types.h - Common type definitions for Crystal Dungeons
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Basic types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

// Math constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef HM_PI
#define HM_PI M_PI
#endif

// Vector types
typedef struct v2 {
    f32 x, y;
} v2;

typedef struct v3 {
    f32 x, y, z;
} v3;

typedef struct v4 {
    f32 x, y, z, w;
} v4;

typedef struct quat {
    f32 x, y, z, w;
} quat;

typedef struct mat4 {
    f32 m[16];
} mat4;

// Color type
typedef struct color32 {
    u8 r, g, b, a;
} color32;

// Rectangle type (using min/max for compatibility)
typedef struct rect {
    v2 min;
    v2 max;
} rect;

// Transform type
typedef struct transform {
    v3 position;
    quat rotation;
    v3 scale;
} transform;

// Plane type
typedef struct plane {
    v3 normal;
    f32 distance;
} plane;

// Ray type
typedef struct ray {
    v3 origin;
    v3 direction;
} ray;

// Input state
typedef struct input_state {
    bool keys[256];
    bool mouse_buttons[3];
    v2 mouse_position;
    v2 mouse_delta;
    f32 mouse_wheel;
} input_state;

// Neural network forward declarations
typedef struct neural_network neural_network;
typedef struct layer layer;

// Activation types
typedef enum {
    ACTIVATION_RELU,
    ACTIVATION_TANH,
    ACTIVATION_SIGMOID,
    ACTIVATION_LINEAR
} activation_type;

// Blend modes
typedef enum {
    BLEND_NORMAL,
    BLEND_ADD,
    BLEND_MULTIPLY,
    BLEND_SCREEN
} blend_mode;

// Helper functions
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

static inline v2 v2_scale(v2 v, f32 s) {
    return v2_make(v.x * s, v.y * s);
}

static inline f32 v2_length(v2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static inline v2 v2_normalize(v2 v) {
    f32 len = v2_length(v);
    if (len > 0) {
        return v2_scale(v, 1.0f / len);
    }
    return v;
}

static inline v3 v3_make(f32 x, f32 y, f32 z) {
    v3 result = {x, y, z};
    return result;
}

static inline v3 v3_add(v3 a, v3 b) {
    return v3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline v3 v3_sub(v3 a, v3 b) {
    return v3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline color32 color32_make(u8 r, u8 g, u8 b, u8 a) {
    color32 result = {r, g, b, a};
    return result;
}

static inline rect rect_make(f32 x, f32 y, f32 w, f32 h) {
    rect result;
    result.min = v2_make(x, y);
    result.max = v2_make(x + w, y + h);
    return result;
}

#endif // GAME_TYPES_H