// handmade_renderer.c - Software renderer implementation with SIMD
// PERFORMANCE: SSE2/AVX2 optimized pixel operations

#include "handmade_renderer.h"
#include <string.h>
#include <immintrin.h>  // SIMD intrinsics
#include <math.h>

// Embedded 8x8 bitmap font - basic ASCII characters
// Each byte represents one row of 8 pixels
static const uint8_t FONT_DATA[128][8] = {
    // Space (32)
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00},
    ['"'] = {0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['#'] = {0x66, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0x66, 0x00},
    ['$'] = {0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00},
    ['%'] = {0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x46, 0x00},
    ['&'] = {0x3C, 0x66, 0x3C, 0x38, 0x67, 0x66, 0x3F, 0x00},
    ['\''] = {0x06, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['('] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    [')'] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    ['*'] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    ['+'] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['/'] = {0x00, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00},
    
    // Numbers
    ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x18, 0x38, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x06, 0x0E, 0x1E, 0x66, 0x7F, 0x06, 0x06, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    
    [':'] = {0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00},
    [';'] = {0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['<'] = {0x0E, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0E, 0x00},
    ['='] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['>'] = {0x70, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x70, 0x00},
    ['?'] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
    ['@'] = {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00},
    
    // Uppercase letters
    ['A'] = {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J'] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S'] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    
    // Lowercase letters (simplified, same as uppercase for now)
    ['a'] = {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
    ['b'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x60, 0x60, 0x60, 0x3C, 0x00},
    ['d'] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    ['f'] = {0x0E, 0x18, 0x3E, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['g'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x7C},
    ['h'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j'] = {0x06, 0x00, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C},
    ['k'] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
    ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
    ['q'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06},
    ['r'] = {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
    ['t'] = {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00},
    ['u'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['v'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},
    ['x'] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
    ['y'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x7C},
    ['z'] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
};

static bitmap_font g_default_font = {
    .width = 8,
    .height = 8
};

void renderer_init(renderer *r, platform_framebuffer *fb) {
    r->pixels = fb->pixels;
    r->width = fb->width;
    r->height = fb->height;
    r->pitch = fb->pitch / 4;  // Convert bytes to pixels
    
    r->clip_rect.x0 = 0;
    r->clip_rect.y0 = 0;
    r->clip_rect.x1 = fb->width;
    r->clip_rect.y1 = fb->height;
    
    // Initialize font
    r->font = &g_default_font;
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 8; j++) {
            r->font->glyphs[i][j] = FONT_DATA[i][j];
        }
    }
    
    r->pixels_drawn = 0;
    r->primitives_drawn = 0;
}

void renderer_begin_frame(renderer *r) {
    r->pixels_drawn = 0;
    r->primitives_drawn = 0;
}

void renderer_end_frame(renderer *r) {
    // Frame complete - could add stats here
}

// PERFORMANCE: AVX2 optimized clear - processes 8 pixels per iteration
void renderer_clear(renderer *r, color32 color) {
    uint32_t *pixels = r->pixels;
    int count = r->width * r->height;
    
#ifdef __AVX2__
    // PERFORMANCE: AVX2 path - 8 pixels/iteration
    __m256i color_vec = _mm256_set1_epi32(color.packed);
    int simd_count = count & ~7;  // Round down to multiple of 8
    
    for (int i = 0; i < simd_count; i += 8) {
        _mm256_storeu_si256((__m256i *)(pixels + i), color_vec);
    }
    
    // Handle remaining pixels
    for (int i = simd_count; i < count; i++) {
        pixels[i] = color.packed;
    }
#elif defined(__SSE2__)
    // PERFORMANCE: SSE2 path - 4 pixels/iteration
    __m128i color_vec = _mm_set1_epi32(color.packed);
    int simd_count = count & ~3;  // Round down to multiple of 4
    
    for (int i = 0; i < simd_count; i += 4) {
        _mm_storeu_si128((__m128i *)(pixels + i), color_vec);
    }
    
    // Handle remaining pixels
    for (int i = simd_count; i < count; i++) {
        pixels[i] = color.packed;
    }
#else
    // Scalar fallback
    for (int i = 0; i < count; i++) {
        pixels[i] = color.packed;
    }
#endif
    
    r->pixels_drawn += count;
}

void renderer_set_clip(renderer *r, int x0, int y0, int x1, int y1) {
    // Clamp to screen bounds
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > r->width) x1 = r->width;
    if (y1 > r->height) y1 = r->height;
    
    r->clip_rect.x0 = x0;
    r->clip_rect.y0 = y0;
    r->clip_rect.x1 = x1;
    r->clip_rect.y1 = y1;
}

void renderer_reset_clip(renderer *r) {
    r->clip_rect.x0 = 0;
    r->clip_rect.y0 = 0;
    r->clip_rect.x1 = r->width;
    r->clip_rect.y1 = r->height;
}

void renderer_pixel(renderer *r, int x, int y, color32 color) {
    if (x >= r->clip_rect.x0 && x < r->clip_rect.x1 &&
        y >= r->clip_rect.y0 && y < r->clip_rect.y1) {
        r->pixels[y * r->pitch + x] = color.packed;
        r->pixels_drawn++;
    }
}

// PERFORMANCE: SIMD optimized rectangle fill
void renderer_fill_rect(renderer *r, int x, int y, int w, int h, color32 color) {
    // Clip rectangle
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    
    if (x0 < r->clip_rect.x0) x0 = r->clip_rect.x0;
    if (y0 < r->clip_rect.y0) y0 = r->clip_rect.y0;
    if (x1 > r->clip_rect.x1) x1 = r->clip_rect.x1;
    if (y1 > r->clip_rect.y1) y1 = r->clip_rect.y1;
    
    if (x0 >= x1 || y0 >= y1) return;
    
    int rect_width = x1 - x0;
    
#ifdef __AVX2__
    // PERFORMANCE: AVX2 path
    __m256i color_vec = _mm256_set1_epi32(color.packed);
    int simd_width = rect_width & ~7;
    
    for (int row = y0; row < y1; row++) {
        uint32_t *row_ptr = r->pixels + row * r->pitch + x0;
        
        // SIMD portion
        for (int col = 0; col < simd_width; col += 8) {
            _mm256_storeu_si256((__m256i *)(row_ptr + col), color_vec);
        }
        
        // Remainder
        for (int col = simd_width; col < rect_width; col++) {
            row_ptr[col] = color.packed;
        }
    }
#elif defined(__SSE2__)
    // PERFORMANCE: SSE2 path
    __m128i color_vec = _mm_set1_epi32(color.packed);
    int simd_width = rect_width & ~3;
    
    for (int row = y0; row < y1; row++) {
        uint32_t *row_ptr = r->pixels + row * r->pitch + x0;
        
        // SIMD portion
        for (int col = 0; col < simd_width; col += 4) {
            _mm_storeu_si128((__m128i *)(row_ptr + col), color_vec);
        }
        
        // Remainder
        for (int col = simd_width; col < rect_width; col++) {
            row_ptr[col] = color.packed;
        }
    }
#else
    // Scalar fallback
    for (int row = y0; row < y1; row++) {
        uint32_t *row_ptr = r->pixels + row * r->pitch + x0;
        for (int col = 0; col < rect_width; col++) {
            row_ptr[col] = color.packed;
        }
    }
#endif
    
    r->pixels_drawn += rect_width * (y1 - y0);
    r->primitives_drawn++;
}

void renderer_draw_rect(renderer *r, int x, int y, int w, int h, color32 color) {
    // Top and bottom
    renderer_fill_rect(r, x, y, w, 1, color);
    renderer_fill_rect(r, x, y + h - 1, w, 1, color);
    
    // Left and right
    renderer_fill_rect(r, x, y + 1, 1, h - 2, color);
    renderer_fill_rect(r, x + w - 1, y + 1, 1, h - 2, color);
    
    r->primitives_drawn++;
}

// Bresenham's line algorithm
void renderer_line(renderer *r, int x0, int y0, int x1, int y1, color32 color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int dx_abs = dx < 0 ? -dx : dx;
    int dy_abs = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    int err = dx_abs - dy_abs;
    
    while (1) {
        renderer_pixel(r, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy_abs) {
            err -= dy_abs;
            x0 += sx;
        }
        if (e2 < dx_abs) {
            err += dx_abs;
            y0 += sy;
        }
    }
    
    r->primitives_drawn++;
}

// Midpoint circle algorithm
void renderer_circle(renderer *r, int cx, int cy, int radius, color32 color) {
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        renderer_pixel(r, cx + x, cy + y, color);
        renderer_pixel(r, cx + y, cy + x, color);
        renderer_pixel(r, cx - y, cy + x, color);
        renderer_pixel(r, cx - x, cy + y, color);
        renderer_pixel(r, cx - x, cy - y, color);
        renderer_pixel(r, cx - y, cy - x, color);
        renderer_pixel(r, cx + y, cy - x, color);
        renderer_pixel(r, cx + x, cy - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
    
    r->primitives_drawn++;
}

void renderer_fill_circle(renderer *r, int cx, int cy, int radius, color32 color) {
    // Filled circle using horizontal lines
    for (int y = -radius; y <= radius; y++) {
        int x = (int)(sqrt(radius * radius - y * y) + 0.5);
        renderer_fill_rect(r, cx - x, cy + y, 2 * x + 1, 1, color);
    }
    
    r->primitives_drawn++;
}

void renderer_text(renderer *r, int x, int y, const char *text, color32 color) {
    if (!text || !r->font) return;
    
    int start_x = x;
    
    while (*text) {
        char c = *text++;
        
        if (c == '\n') {
            x = start_x;
            y += r->font->height;
            continue;
        }
        
        if ((unsigned char)c >= 128) {
            c = '?';  // Replace unknown chars
        }
        
        // Draw character bitmap
        for (int row = 0; row < 8; row++) {
            uint8_t row_data = r->font->glyphs[(int)c][row];
            for (int col = 0; col < 8; col++) {
                if (row_data & (0x80 >> col)) {
                    renderer_pixel(r, x + col, y + row, color);
                }
            }
        }
        
        x += r->font->width;
    }
}

void renderer_text_size(renderer *r, const char *text, int *w, int *h) {
    if (!text || !r->font) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    
    int max_width = 0;
    int current_width = 0;
    int lines = 1;
    
    while (*text) {
        if (*text == '\n') {
            if (current_width > max_width) {
                max_width = current_width;
            }
            current_width = 0;
            lines++;
        } else {
            current_width += r->font->width;
        }
        text++;
    }
    
    if (current_width > max_width) {
        max_width = current_width;
    }
    
    if (w) *w = max_width;
    if (h) *h = lines * r->font->height;
}

// Alpha blending with premultiplied alpha
void renderer_blend_rect(renderer *r, int x, int y, int w, int h, color32 color) {
    // Clip rectangle
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    
    if (x0 < r->clip_rect.x0) x0 = r->clip_rect.x0;
    if (y0 < r->clip_rect.y0) y0 = r->clip_rect.y0;
    if (x1 > r->clip_rect.x1) x1 = r->clip_rect.x1;
    if (y1 > r->clip_rect.y1) y1 = r->clip_rect.y1;
    
    if (x0 >= x1 || y0 >= y1) return;
    
    // Premultiply alpha
    color = premultiply_alpha(color);
    uint32_t inv_alpha = 255 - color.a;
    
    for (int row = y0; row < y1; row++) {
        uint32_t *row_ptr = r->pixels + row * r->pitch + x0;
        
        for (int col = 0; col < (x1 - x0); col++) {
            color32 dst;
            dst.packed = row_ptr[col];
            
            // Blend formula: result = src + dst * (1 - src_alpha)
            dst.r = color.r + (dst.r * inv_alpha) / 255;
            dst.g = color.g + (dst.g * inv_alpha) / 255;
            dst.b = color.b + (dst.b * inv_alpha) / 255;
            dst.a = 255;  // Assume opaque destination
            
            row_ptr[col] = dst.packed;
        }
    }
    
    r->pixels_drawn += (x1 - x0) * (y1 - y0);
    r->primitives_drawn++;
}

// sqrt is now provided by math.h