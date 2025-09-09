// handmade_renderer.h - Software renderer with SIMD optimization
// PERFORMANCE: All operations optimized for cache efficiency and vectorization

#ifndef HANDMADE_RENDERER_H
#define HANDMADE_RENDERER_H

#include "handmade_platform.h"
#include <stdint.h>

// Color operations - premultiplied alpha for performance
typedef union {
    uint32_t packed;
    struct {
        uint8_t b, g, r, a;
    };
} color32;

// Rectangle for clipping and drawing
typedef struct {
    int x0, y0, x1, y1;
} rect;

// Bitmap font - embedded 8x8 monospace
typedef struct {
    uint8_t glyphs[256][8];  // 256 ASCII chars, 8 bytes each (8x8 bitmap)
    int width;
    int height;
} bitmap_font;

// Renderer state
typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int pitch;  // in pixels, not bytes
    
    rect clip_rect;
    bitmap_font *font;
    
    // Performance counters
    uint64_t pixels_drawn;
    uint64_t primitives_drawn;
} renderer;

// Initialize renderer
void renderer_init(renderer *r, platform_framebuffer *fb);
void renderer_begin_frame(renderer *r);
void renderer_end_frame(renderer *r);

// Basic operations
void renderer_clear(renderer *r, color32 color);
void renderer_set_clip(renderer *r, int x0, int y0, int x1, int y1);
void renderer_reset_clip(renderer *r);

// Primitives - all SIMD optimized where possible
void renderer_pixel(renderer *r, int x, int y, color32 color);
void renderer_fill_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_draw_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_line(renderer *r, int x0, int y0, int x1, int y1, color32 color);
void renderer_circle(renderer *r, int cx, int cy, int radius, color32 color);
void renderer_fill_circle(renderer *r, int cx, int cy, int radius, color32 color);

// Text rendering
void renderer_text(renderer *r, int x, int y, const char *text, color32 color);
void renderer_text_size(renderer *r, const char *text, int *w, int *h);

// Blending operations
void renderer_blend_rect(renderer *r, int x, int y, int w, int h, color32 color);

// Color utilities
static inline color32 rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    color32 c;
    c.r = r; c.g = g; c.b = b; c.a = a;
    return c;
}

static inline color32 rgb(uint8_t r, uint8_t g, uint8_t b) {
    return rgba(r, g, b, 255);
}

// Premultiply alpha for correct blending
static inline color32 premultiply_alpha(color32 c) {
    if (c.a == 255) return c;
    if (c.a == 0) {
        c.packed = 0;
        return c;
    }
    c.r = (c.r * c.a) / 255;
    c.g = (c.g * c.a) / 255;
    c.b = (c.b * c.a) / 255;
    return c;
}

#endif // HANDMADE_RENDERER_H