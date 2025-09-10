#ifndef MINIMAL_RENDERER_H
#define MINIMAL_RENDERER_H

#include "handmade_platform.h"

// Color type compatible with GUI system
typedef union {
    u32 packed;
    struct {
        u8 b, g, r, a;
    };
} color32;

// Vector types needed by GUI
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;

// Minimal renderer structure
typedef struct {
    u32 width;
    u32 height;
    // Counters for GUI performance tracking
    u64 pixels_drawn;
    u64 primitives_drawn;
} renderer;

// Color helper functions
static inline color32 rgba(u8 r, u8 g, u8 b, u8 a) {
    color32 c;
    c.r = r; c.g = g; c.b = b; c.a = a;
    return c;
}

static inline color32 rgb(u8 r, u8 g, u8 b) {
    return rgba(r, g, b, 255);
}

// Math and size macros needed by GUI
#define Kilobytes(value) ((value) * 1024LL)
typedef size_t umm;

// Vector helper functions
static inline v2 v2_make(f32 x, f32 y) { 
    v2 result = {x, y}; 
    return result; 
}

// Renderer functions expected by GUI system
void renderer_init(renderer *r, u32 width, u32 height);
void renderer_shutdown(renderer *r);

// Basic drawing functions that GUI needs
void renderer_fill_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_draw_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_text(renderer *r, int x, int y, const char *text, color32 color);
void renderer_text_size(renderer *r, const char *text, int *w, int *h);

// Additional functions for performance overlay
void renderer_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_rect_outline(renderer *r, int x, int y, int w, int h, color32 color, int thickness);
void renderer_line(renderer *r, int x1, int y1, int x2, int y2, color32 color);

// Color helper for GUI macros
static inline color32 color32_make(u8 r, u8 g, u8 b, u8 a) {
    return rgba(r, g, b, a);
}

#endif // MINIMAL_RENDERER_H