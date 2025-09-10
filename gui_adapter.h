#ifndef GUI_ADAPTER_H
#define GUI_ADAPTER_H

// Complete adapter for GUI system - provides all types it needs
#include "handmade_platform.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stddef.h>

// Basic types that GUI needs
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
typedef float f32;
typedef double f64;
typedef size_t umm;

// Math constants
#define PI 3.14159265358979323846f

// Memory macros
#define Kilobytes(value) ((value) * 1024LL)
#define Megabytes(value) (Kilobytes(value) * 1024LL)
#define global_variable static
#define internal static
#define local_persist static
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

// Vector types
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;
typedef v4 quat;

// Color type
typedef union {
    u32 packed;
    struct {
        u8 b, g, r, a;
    };
} color32;

// Rectangle type
typedef struct {
    int x0, y0, x1, y1;
} rect;

// Vector helper functions
static inline v2 v2_make(f32 x, f32 y) { v2 result = {x, y}; return result; }
static inline v3 v3_make(f32 x, f32 y, f32 z) { v3 result = {x, y, z}; return result; }
static inline v4 v4_make(f32 x, f32 y, f32 z, f32 w) { v4 result = {x, y, z, w}; return result; }
static inline quat quat_make(f32 x, f32 y, f32 z, f32 w) { quat result = {x, y, z, w}; return result; }
static inline v2 v2_add(v2 a, v2 b) { return v2_make(a.x + b.x, a.y + b.y); }
static inline v2 v2_sub(v2 a, v2 b) { return v2_make(a.x - b.x, a.y - b.y); }

// Color helper functions
static inline color32 rgba(u8 r, u8 g, u8 b, u8 a) {
    color32 c;
    c.r = r; c.g = g; c.b = b; c.a = a;
    return c;
}

static inline color32 rgb(u8 r, u8 g, u8 b) {
    return rgba(r, g, b, 255);
}

static inline color32 color32_make(u8 r, u8 g, u8 b, u8 a) {
    return rgba(r, g, b, a);
}

// Math helper functions
static inline f32 hm_max(f32 a, f32 b) { return (a > b) ? a : b; }
static inline f32 hm_min(f32 a, f32 b) { return (a < b) ? a : b; }
static inline f32 hm_clamp(f32 value, f32 min, f32 max) { return hm_min(hm_max(value, min), max); }
static inline umm AlignPow2(umm value, umm alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// Platform state compatible with what GUI expects
typedef struct {
    struct {
        i32 x, y;
        bool left_down;
        bool right_down; 
        bool middle_down;
        f32 wheel_delta;
    } mouse;
    struct {
        bool keys[256];
        char text_input[256];
        i32 text_input_length;
    } keyboard;
} platform_state;

// Minimal renderer for GUI
typedef struct {
    u32 width;
    u32 height;
    u32 *pixels;
    int pitch;
    rect clip_rect;
    u64 pixels_drawn;
    u64 primitives_drawn;
} renderer;

// CPU timer for GUI
static inline u64 ReadCPUTimer(void) { 
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000ULL + (u64)ts.tv_nsec / 1000ULL;
}

// Renderer functions that GUI needs
void renderer_init(renderer *r, u32 width, u32 height);
void renderer_shutdown(renderer *r);
void renderer_fill_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_draw_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_text(renderer *r, int x, int y, const char *text, color32 color);
void renderer_text_size(renderer *r, const char *text, int *w, int *h);
void renderer_rect(renderer *r, int x, int y, int w, int h, color32 color);
void renderer_rect_outline(renderer *r, int x, int y, int w, int h, color32 color, int thickness);
void renderer_line(renderer *r, int x1, int y1, int x2, int y2, color32 color);

// Minimal gui_context structure
typedef struct gui_context {
    renderer *renderer;
    platform_state *platform;
    
    // Widget state
    u64 hot_id;
    u64 active_id;
    
    // Input state
    v2 mouse_pos;
    v2 mouse_delta;
    bool mouse_down[3];
    bool mouse_clicked[3];
    bool mouse_released[3];
    f32 mouse_wheel;
    
    // Layout (simplified)
    v2 cursor;
    
    // Theme colors (simplified)
    struct {
        color32 background;
        color32 button;
        color32 button_hover;
        color32 button_active;
        color32 text;
        color32 border;
        color32 checkbox_bg;
        color32 checkbox_check;
        color32 slider_bg;
        color32 slider_fill;
        color32 slider_handle;
        color32 panel;
    } theme;
    
    // Performance tracking
    struct {
        u64 frames_rendered;
        f32 avg_frame_time;
        f32 min_frame_time;
        f32 max_frame_time;
        u32 widgets_this_frame;
        u32 draw_calls_this_frame;
        u32 vertices_this_frame;
    } perf;
    
    u64 frame_start_time;
    
    // Layout stack (simplified)
    struct {
        v2 pos;
        v2 size;
        v2 cursor;
    } layout_stack[8];
    int layout_depth;
    
    // Temp memory
    u8 temp_memory[Kilobytes(64)];
    umm temp_memory_used;
} gui_context;

// Minimal GUI theme and system functions we need
void gui_init(gui_context *ctx, renderer *r, platform_state *p);
void gui_begin_frame(gui_context *ctx);
void gui_end_frame(gui_context *ctx);
void gui_shutdown(gui_context *ctx);
bool gui_button(gui_context *ctx, const char *label);
bool gui_checkbox(gui_context *ctx, const char *label, bool *value);
bool gui_slider_float(gui_context *ctx, const char *label, f32 *value, f32 min_val, f32 max_val);
void gui_text(gui_context *ctx, const char *fmt, ...);
void gui_separator(gui_context *ctx);
void gui_performance_overlay(gui_context *ctx, bool show_graph);
bool gui_begin_window(gui_context *ctx, const char *title, bool *p_open, u32 flags);
void gui_end_window(gui_context *ctx);
void gui_show_demo_window(gui_context *ctx, bool *p_open);

// Window flags
#define GUI_WINDOW_NONE 0

#endif // GUI_ADAPTER_H