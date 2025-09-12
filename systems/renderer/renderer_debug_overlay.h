#ifndef RENDERER_DEBUG_OVERLAY_H
#define RENDERER_DEBUG_OVERLAY_H

#include "handmade_renderer.h"

// Debug overlay API
void renderer_debug_overlay_init(float target_fps);
void renderer_debug_overlay_shutdown();
void renderer_debug_overlay_frame_start();
void renderer_debug_overlay_frame_end(renderer_stats* stats);
void renderer_debug_overlay_render(renderer_state* renderer);

// Simple immediate-mode drawing functions for debug overlay
// These are stubs - implement with actual renderer
static inline void renderer_draw_text(renderer_state* r, int x, int y, const char* text, u32 color) {
    // TODO: Implement with bitmap font or debug font
    (void)r; (void)x; (void)y; (void)text; (void)color;
}

static inline void renderer_draw_rect(renderer_state* r, int x, int y, int w, int h, u32 color) {
    // TODO: Implement with immediate mode quad
    (void)r; (void)x; (void)y; (void)w; (void)h; (void)color;
}

static inline void renderer_draw_line(renderer_state* r, int x1, int y1, int x2, int y2, u32 color) {
    // TODO: Implement with line primitive
    (void)r; (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
}

#endif // RENDERER_DEBUG_OVERLAY_H
