// gui_demo.c - Comprehensive demonstration of the handmade GUI system
// Showcases all widgets, features, and production-ready tools

#ifdef GUI_DEMO_STANDALONE
// For standalone mode, we need minimal math types without including handmade.h
#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>

// Basic types
typedef int8_t   i8;
typedef int16_t  i16;  
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t   umm;
typedef float    r32;
typedef double   r64;
typedef float    f32;
typedef double   f64;

// Math constants
#define PI 3.14159265358979323846f

// Vector types
typedef struct { r32 x, y; } v2;
typedef struct { r32 x, y, z; } v3;
typedef struct { r32 x, y, z, w; } v4;
typedef v4 quat;

// Color type from renderer
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

// Inline functions
static inline v2 v2_make(r32 x, r32 y) { v2 result = {x, y}; return result; }
static inline v3 v3_make(r32 x, r32 y, r32 z) { v3 result = {x, y, z}; return result; }
static inline v4 v4_make(r32 x, r32 y, r32 z, r32 w) { v4 result = {x, y, z, w}; return result; }
static inline quat quat_make(r32 x, r32 y, r32 z, r32 w) { quat result = {x, y, z, w}; return result; }

// Color helper functions
static inline color32 rgba(u8 r, u8 g, u8 b, u8 a) {
    color32 c;
    c.r = r; c.g = g; c.b = b; c.a = a;
    return c;
}

static inline color32 rgb(u8 r, u8 g, u8 b) {
    return rgba(r, g, b, 255);
}

// Platform types for GUI - using forward declarations to avoid conflicts

// ReadCPUTimer implementation
static inline u64 ReadCPUTimer(void) { 
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000ULL + (u64)ts.tv_nsec / 1000ULL;
}

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))
#define Kilobytes(value) ((value) * 1024LL)
#define Megabytes(value) (Kilobytes(value) * 1024LL)
#define global_variable static
#define internal static
#define local_persist static

// Platform forward declarations to avoid conflicts  
// Define actual structures for standalone mode
typedef struct {
    struct {
        bool keys[256];
    } keyboard;
    struct {
        v2 pos;
        bool left;
        bool right;
        bool middle;
    } mouse;
} platform_input;

typedef struct {
    u32 *pixels;
    int width;
    int height;
    int pitch;
} platform_framebuffer;

typedef struct {
    u32 *pixels;
    int width;
    int height;
    int pitch;
    rect clip_rect;
    // Minimal renderer for standalone
    u64 pixels_drawn;
    u64 primitives_drawn;
} renderer;

// Platform state (minimal for standalone)
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

// Math helper functions
static inline f32 hm_max(f32 a, f32 b) { return (a > b) ? a : b; }
static inline f32 hm_min(f32 a, f32 b) { return (a < b) ? a : b; }
static inline f32 hm_clamp(f32 value, f32 min, f32 max) { return hm_min(hm_max(value, min), max); }
static inline v2 v2_add(v2 a, v2 b) { return v2_make(a.x + b.x, a.y + b.y); }
static inline v2 v2_sub(v2 a, v2 b) { return v2_make(a.x - b.x, a.y - b.y); }
static inline umm AlignPow2(umm value, umm alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

#endif // HANDMADE_MATH_H
#endif // GUI_DEMO_STANDALONE

#include "handmade_gui.h"
#ifndef GUI_DEMO_STANDALONE
#include "handmade_platform.h"
#include "handmade_renderer.h"
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// DEMO STATE MANAGEMENT
// ============================================================================

typedef struct {
    // Window visibility flags
    bool show_main_demo;
    bool show_widgets_demo;
    bool show_layout_demo;
    bool show_styling_demo;
    bool show_performance_demo;
    bool show_neural_demo;
    bool show_tools_demo;
    bool show_console_demo;
    bool show_property_demo;
    bool show_asset_browser_demo;
    bool show_scene_hierarchy_demo;
    
    // Widget state for demos
    bool demo_checkbox;
    bool demo_checkbox2;
    bool demo_checkbox3;
    f32 demo_float_slider;
    f32 demo_float_slider2[3];
    i32 demo_int_slider;
    f32 demo_color[4];
    char demo_text_buffer[256];
    i32 demo_combo_selection;
    i32 demo_listbox_selection;
    bool demo_tree_node_open;
    
    // Neural network demo data
    f32 neural_weights[16];
    f32 neural_activations[8];
    f32 neural_graph_data[100];
    i32 neural_graph_head;
    
    // Performance test data
    i32 perf_widget_count;
    f32 perf_frame_times[120];
    i32 perf_frame_time_head;
    
    // Asset browser demo
    char asset_filter[64];
    
    // Property inspector demo object
    struct {
        v3 position;
        v3 rotation;
        v3 scale;
        f32 health;
        i32 level;
        bool active;
        char name[64];
        f32 color[4];
    } demo_object;
    
} gui_demo_state;

global_variable gui_demo_state g_demo_state = {0};

// ============================================================================
// DEMO INITIALIZATION
// ============================================================================

internal void
demo_init_state(void) {
    // Initialize demo state with reasonable defaults
    g_demo_state.show_main_demo = true;
    g_demo_state.demo_checkbox = true;
    g_demo_state.demo_float_slider = 0.5f;
    g_demo_state.demo_float_slider2[0] = 0.2f;
    g_demo_state.demo_float_slider2[1] = 0.6f;
    g_demo_state.demo_float_slider2[2] = 0.8f;
    g_demo_state.demo_int_slider = 42;
    g_demo_state.demo_color[0] = 1.0f;
    g_demo_state.demo_color[1] = 0.5f;
    g_demo_state.demo_color[2] = 0.2f;
    g_demo_state.demo_color[3] = 1.0f;
    strcpy(g_demo_state.demo_text_buffer, "Hello, GUI!");
    g_demo_state.perf_widget_count = 100;
    
    // Initialize demo object properties
    g_demo_state.demo_object.position = v3_make(0, 0, 0);
    g_demo_state.demo_object.rotation = v3_make(0, 0, 0);
    g_demo_state.demo_object.scale = v3_make(1, 1, 1);
    g_demo_state.demo_object.health = 100.0f;
    g_demo_state.demo_object.level = 1;
    g_demo_state.demo_object.active = true;
    strcpy(g_demo_state.demo_object.name, "Demo Object");
    g_demo_state.demo_object.color[0] = 0.8f;
    g_demo_state.demo_object.color[1] = 0.2f;
    g_demo_state.demo_object.color[2] = 0.4f;
    g_demo_state.demo_object.color[3] = 1.0f;
    
    // Initialize neural network demo data
    for (i32 i = 0; i < ArrayCount(g_demo_state.neural_weights); i++) {
        g_demo_state.neural_weights[i] = (f32)(rand() % 1000) / 1000.0f - 0.5f;
    }
    
    for (i32 i = 0; i < ArrayCount(g_demo_state.neural_activations); i++) {
        g_demo_state.neural_activations[i] = (f32)(rand() % 1000) / 1000.0f;
    }
    
    // Initialize graph data
    for (i32 i = 0; i < ArrayCount(g_demo_state.neural_graph_data); i++) {
        g_demo_state.neural_graph_data[i] = sinf((f32)i * 0.1f) * 0.5f + 0.5f;
    }
}

// ============================================================================
// MAIN DEMO WINDOW
// ============================================================================

internal void
demo_show_main_window(gui_context *ctx) {
    if (!g_demo_state.show_main_demo) return;
    
    if (gui_begin_window(ctx, "Handmade GUI Demo", &g_demo_state.show_main_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Welcome to the Handmade GUI System!");
        gui_text_colored(ctx, ctx->theme.success, "Production-ready immediate mode GUI with zero allocations per frame");
        gui_separator(ctx);
        
        gui_text(ctx, "Demo Categories:");
        gui_indent(ctx, 20);
        
        if (gui_button(ctx, "Basic Widgets Demo")) {
            g_demo_state.show_widgets_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Layout System Demo")) {
            g_demo_state.show_layout_demo = true;
        }
        
        if (gui_button(ctx, "Styling & Themes Demo")) {
            g_demo_state.show_styling_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Performance Demo")) {
            g_demo_state.show_performance_demo = true;
        }
        
        if (gui_button(ctx, "Neural Network Visualization")) {
            g_demo_state.show_neural_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Production Tools")) {
            g_demo_state.show_tools_demo = true;
        }
        
        gui_unindent(ctx, 20);
        gui_separator(ctx);
        
        gui_text(ctx, "Built-in Production Tools:");
        gui_indent(ctx, 20);
        
        gui_checkbox(ctx, "Performance Metrics", &ctx->perf.show_metrics);
        
        if (gui_button(ctx, "Console/Log Viewer")) {
            g_demo_state.show_console_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Property Inspector")) {
            g_demo_state.show_property_demo = true;
        }
        
        if (gui_button(ctx, "Asset Browser")) {
            g_demo_state.show_asset_browser_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Scene Hierarchy")) {
            g_demo_state.show_scene_hierarchy_demo = true;
        }
        
        gui_unindent(ctx, 20);
        gui_separator(ctx);
        
        // Quick stats
        gui_text(ctx, "Current Frame Stats:");
        gui_indent(ctx, 20);
        gui_text(ctx, "Widgets Drawn: %u", ctx->perf.widgets_this_frame);
        gui_text(ctx, "Frame Time: %.2fms (%.0f FPS)", 
                 ctx->perf.avg_frame_time, 
                 1000.0f / hm_max(ctx->perf.avg_frame_time, 0.001f));
        gui_text(ctx, "Memory Used: %llu bytes temp", ctx->temp_memory_used);
        gui_unindent(ctx, 20);
        
        gui_separator(ctx);
        if (gui_button(ctx, "Test Log Messages")) {
            gui_log(ctx, "Info: This is a test log message");
            gui_log_warning(ctx, "Warning: This is a test warning");
            gui_log_error(ctx, "Error: This is a test error");
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// WIDGETS DEMO
// ============================================================================

internal void
demo_show_widgets_window(gui_context *ctx) {
    if (!g_demo_state.show_widgets_demo) return;
    
    if (gui_begin_window(ctx, "Basic Widgets Demo", &g_demo_state.show_widgets_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Button Variants:");
        gui_separator(ctx);
        
        if (gui_button(ctx, "Regular Button")) {
            gui_log(ctx, "Regular button clicked!");
        }
        gui_same_line(ctx, 10);
        if (gui_button_small(ctx, "Small")) {
            gui_log(ctx, "Small button clicked!");
        }
        gui_same_line(ctx, 10);
        if (gui_button_colored(ctx, "Colored", ctx->theme.success)) {
            gui_log(ctx, "Colored button clicked!");
        }
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Input Widgets:");
        gui_separator(ctx);
        
        gui_checkbox(ctx, "Primary Checkbox", &g_demo_state.demo_checkbox);
        gui_checkbox(ctx, "Secondary Checkbox", &g_demo_state.demo_checkbox2);
        gui_checkbox(ctx, "Tertiary Checkbox", &g_demo_state.demo_checkbox3);
        
        gui_spacing(ctx, 5);
        gui_slider_float(ctx, "Float Slider", &g_demo_state.demo_float_slider, 0.0f, 1.0f);
        gui_slider_int(ctx, "Integer Slider", &g_demo_state.demo_int_slider, 0, 100);
        
        gui_spacing(ctx, 5);
        // Simulate multi-component sliders
        gui_text(ctx, "Multi-Component Slider (RGB):");
        gui_slider_float(ctx, "Red", &g_demo_state.demo_float_slider2[0], 0.0f, 1.0f);
        gui_slider_float(ctx, "Green", &g_demo_state.demo_float_slider2[1], 0.0f, 1.0f);
        gui_slider_float(ctx, "Blue", &g_demo_state.demo_float_slider2[2], 0.0f, 1.0f);
        
        // Show color preview
        color32 preview_color = rgb(
            (u8)(g_demo_state.demo_float_slider2[0] * 255),
            (u8)(g_demo_state.demo_float_slider2[1] * 255),
            (u8)(g_demo_state.demo_float_slider2[2] * 255)
        );
        gui_text_colored(ctx, preview_color, "Color Preview Text");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Text Input:");
        gui_separator(ctx);
        gui_text(ctx, "Current text: '%s'", g_demo_state.demo_text_buffer);
        gui_text_colored(ctx, ctx->theme.text_disabled, "(Text input widget not fully implemented yet)");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Selection Widgets:");
        gui_separator(ctx);
        gui_text_colored(ctx, ctx->theme.text_disabled, "(Combo boxes and list boxes not fully implemented yet)");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Tree Nodes:");
        gui_separator(ctx);
        if (gui_tree_node(ctx, "Expandable Node")) {
            gui_text(ctx, "Child item 1");
            gui_text(ctx, "Child item 2");
            if (gui_tree_node(ctx, "Nested Node")) {
                gui_text(ctx, "Nested child");
            }
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// LAYOUT DEMO
// ============================================================================

internal void
demo_show_layout_window(gui_context *ctx) {
    if (!g_demo_state.show_layout_demo) return;
    
    if (gui_begin_window(ctx, "Layout System Demo", &g_demo_state.show_layout_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Vertical Layout (Default):");
        gui_separator(ctx);
        
        gui_begin_layout(ctx, LAYOUT_VERTICAL, 5);
        gui_button(ctx, "Button 1");
        gui_button(ctx, "Button 2");
        gui_button(ctx, "Button 3");
        gui_end_layout(ctx);
        
        gui_spacing(ctx, 15);
        gui_text(ctx, "Horizontal Layout:");
        gui_separator(ctx);
        
        gui_begin_layout(ctx, LAYOUT_HORIZONTAL, 10);
        gui_button(ctx, "Left");
        gui_button(ctx, "Center");
        gui_button(ctx, "Right");
        gui_end_layout(ctx);
        
        gui_spacing(ctx, 15);
        gui_text(ctx, "Grid Layout (3 columns):");
        gui_separator(ctx);
        
        gui_begin_grid(ctx, 3, 5);
        for (i32 i = 1; i <= 9; i++) {
            char button_label[32];
            snprintf(button_label, sizeof(button_label), "Grid %d", i);
            gui_button(ctx, button_label);
        }
        gui_end_grid(ctx);
        
        gui_spacing(ctx, 15);
        gui_text(ctx, "Manual Positioning with Same Line:");
        gui_separator(ctx);
        
        gui_button(ctx, "First");
        gui_same_line(ctx, 20);
        gui_text(ctx, "Text after button");
        gui_same_line(ctx, 20);
        gui_checkbox(ctx, "Checkbox", &g_demo_state.demo_checkbox);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Indentation:");
        gui_indent(ctx, 20);
        gui_text(ctx, "Indented text level 1");
        gui_indent(ctx, 20);
        gui_text(ctx, "Indented text level 2");
        gui_unindent(ctx, 40); // Unindent both levels
        gui_text(ctx, "Back to normal");
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        gui_text(ctx, "Layout separators help organize content");
    }
    gui_end_window(ctx);
}

// ============================================================================
// STYLING DEMO
// ============================================================================

internal void
demo_show_styling_window(gui_context *ctx) {
    if (!g_demo_state.show_styling_demo) return;
    
    if (gui_begin_window(ctx, "Styling & Themes Demo", &g_demo_state.show_styling_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Current Theme Colors:");
        gui_separator(ctx);
        
        // Show theme colors as colored text samples
        gui_text_colored(ctx, ctx->theme.text, "Normal Text");
        gui_text_colored(ctx, ctx->theme.text_disabled, "Disabled Text");
        gui_text_colored(ctx, ctx->theme.success, "Success/Good");
        gui_text_colored(ctx, ctx->theme.warning, "Warning/Caution");
        gui_text_colored(ctx, ctx->theme.error, "Error/Danger");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Button Color Variants:");
        gui_separator(ctx);
        
        gui_button_colored(ctx, "Default Button", ctx->theme.button);
        gui_button_colored(ctx, "Success Button", ctx->theme.success);
        gui_button_colored(ctx, "Warning Button", ctx->theme.warning);
        gui_button_colored(ctx, "Error Button", ctx->theme.error);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Theme Switching:");
        gui_separator(ctx);
        
        static bool use_dark_theme = true;
        if (gui_checkbox(ctx, "Use Dark Theme", &use_dark_theme)) {
            ctx->theme = use_dark_theme ? gui_dark_theme() : gui_light_theme();
        }
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Custom Styled Elements:");
        gui_separator(ctx);
        
        // Create some custom colored elements
        layout_info *layout = gui_current_layout(ctx);
        renderer *r = ctx->renderer;
        
        // Custom colored rectangles
        v2 pos = layout->cursor;
        f32 rect_size = 30.0f;
        f32 spacing = rect_size + 10.0f;
        
        renderer_fill_rect(r, (int)pos.x, (int)pos.y, (int)rect_size, (int)rect_size, ctx->theme.success);
        renderer_fill_rect(r, (int)(pos.x + spacing), (int)pos.y, (int)rect_size, (int)rect_size, ctx->theme.warning);
        renderer_fill_rect(r, (int)(pos.x + spacing * 2), (int)pos.y, (int)rect_size, (int)rect_size, ctx->theme.error);
        
        gui_advance_cursor(ctx, v2_make(spacing * 3, rect_size));
        
        gui_text(ctx, "Custom color swatches using direct renderer calls");
    }
    gui_end_window(ctx);
}

// ============================================================================
// PERFORMANCE DEMO
// ============================================================================

internal void
demo_show_performance_window(gui_context *ctx) {
    if (!g_demo_state.show_performance_demo) return;
    
    if (gui_begin_window(ctx, "Performance Demo", &g_demo_state.show_performance_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Performance Stress Test:");
        gui_separator(ctx);
        
        gui_slider_int(ctx, "Widget Count", &g_demo_state.perf_widget_count, 10, 500);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Rendering %d widgets:", g_demo_state.perf_widget_count);
        
        // Stress test: render many widgets
        u64 start_time = ReadCPUTimer();
        
        gui_begin_layout(ctx, LAYOUT_GRID, 2);
        gui_current_layout(ctx)->columns = 10; // 10 columns for compact layout
        
        for (i32 i = 0; i < g_demo_state.perf_widget_count; i++) {
            char widget_label[16];
            snprintf(widget_label, sizeof(widget_label), "%d", i);
            
            if (i % 3 == 0) {
                gui_button_small(ctx, widget_label);
            } else if (i % 3 == 1) {
                static bool dummy_bool = false;
                gui_checkbox(ctx, widget_label, &dummy_bool);
            } else {
                gui_text(ctx, widget_label);
            }
        }
        gui_end_grid(ctx);
        
        u64 end_time = ReadCPUTimer();
        f32 render_time_ms = (f32)(end_time - start_time) / 1000.0f;
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        gui_text(ctx, "Render Statistics:");
        gui_text(ctx, "Time to render %d widgets: %.2fms", g_demo_state.perf_widget_count, render_time_ms);
        gui_text(ctx, "Widgets per millisecond: %.1f", g_demo_state.perf_widget_count / hm_max(render_time_ms, 0.001f));
        gui_text(ctx, "Total widgets this frame: %u", ctx->perf.widgets_this_frame);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Memory Usage:");
        gui_text(ctx, "Temp memory used: %llu bytes", ctx->temp_memory_used);
        gui_text(ctx, "Temp memory available: %llu bytes", sizeof(ctx->temp_memory) - ctx->temp_memory_used);
        
        // Store frame time for history
        g_demo_state.perf_frame_times[g_demo_state.perf_frame_time_head] = ctx->perf.avg_frame_time;
        g_demo_state.perf_frame_time_head = (g_demo_state.perf_frame_time_head + 1) % ArrayCount(g_demo_state.perf_frame_times);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Frame Time History (Placeholder for graph):");
        gui_text_colored(ctx, ctx->theme.text_disabled, "Graph widget not fully implemented yet");
    }
    gui_end_window(ctx);
}

// ============================================================================
// NEURAL NETWORK DEMO
// ============================================================================

internal void
demo_show_neural_window(gui_context *ctx) {
    if (!g_demo_state.show_neural_demo) return;
    
    if (gui_begin_window(ctx, "Neural Network Visualization", &g_demo_state.show_neural_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Neural Network Visualization:");
        gui_separator(ctx);
        
        // Animate neural network data
        static f32 animation_time = 0;
        animation_time += ctx->perf.avg_frame_time / 1000.0f;
        
        for (i32 i = 0; i < ArrayCount(g_demo_state.neural_activations); i++) {
            g_demo_state.neural_activations[i] = (sinf(animation_time + i * 0.5f) + 1.0f) * 0.5f;
        }
        
        // Update graph data
        g_demo_state.neural_graph_data[g_demo_state.neural_graph_head] = g_demo_state.neural_activations[0];
        g_demo_state.neural_graph_head = (g_demo_state.neural_graph_head + 1) % ArrayCount(g_demo_state.neural_graph_data);
        
        gui_text(ctx, "Network Architecture: 4-6-4-2");
        gui_text(ctx, "Current activations:");
        
        gui_begin_layout(ctx, LAYOUT_HORIZONTAL, 10);
        for (i32 i = 0; i < 4; i++) {
            char activation_str[32];
            snprintf(activation_str, sizeof(activation_str), "%.2f", g_demo_state.neural_activations[i]);
            
            // Color based on activation level
            f32 activation = g_demo_state.neural_activations[i];
            u8 intensity = (u8)(activation * 255);
            color32 activation_color = rgb(intensity, intensity/2, 255 - intensity);
            
            gui_text_colored(ctx, activation_color, activation_str);
        }
        gui_end_layout(ctx);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Activation Graph (Placeholder):");
        gui_text_colored(ctx, ctx->theme.text_disabled, "Neural network visualization widgets not fully implemented yet");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Training Controls:");
        gui_separator(ctx);
        
        static f32 learning_rate = 0.01f;
        gui_slider_float(ctx, "Learning Rate", &learning_rate, 0.001f, 0.1f);
        
        static i32 epochs = 100;
        gui_slider_int(ctx, "Epochs", &epochs, 1, 1000);
        
        if (gui_button(ctx, "Start Training (Simulated)")) {
            gui_log(ctx, "Started neural network training with learning rate %.3f for %d epochs", learning_rate, epochs);
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Reset Network")) {
            for (i32 i = 0; i < ArrayCount(g_demo_state.neural_weights); i++) {
                g_demo_state.neural_weights[i] = (f32)(rand() % 1000) / 1000.0f - 0.5f;
            }
            gui_log(ctx, "Neural network weights randomized");
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// PRODUCTION TOOLS DEMO
// ============================================================================

internal void
demo_show_tools_window(gui_context *ctx) {
    if (!g_demo_state.show_tools_demo) return;
    
    if (gui_begin_window(ctx, "Production Tools Demo", &g_demo_state.show_tools_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Built-in Production Tools:");
        gui_separator(ctx);
        
        gui_text(ctx, "Performance Monitoring:");
        gui_checkbox(ctx, "Show Performance Overlay", &ctx->perf.show_metrics);
        gui_text_colored(ctx, ctx->theme.text_disabled, "Real-time performance metrics in top-right corner");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Hot Reload System:");
        static bool hot_reload_enabled = false;
        gui_checkbox(ctx, "Enable Theme Hot Reload", &hot_reload_enabled);
        if (hot_reload_enabled && !ctx->theme_hot_reload) {
            gui_enable_hot_reload(ctx, "theme.conf");
            gui_log(ctx, "Hot reload enabled for theme.conf");
        }
        gui_text_colored(ctx, ctx->theme.text_disabled, "Automatically reloads theme file when modified");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Debug Tools:");
        if (gui_button(ctx, "Open Console")) {
            g_demo_state.show_console_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Open Property Inspector")) {
            g_demo_state.show_property_demo = true;
        }
        
        if (gui_button(ctx, "Open Asset Browser")) {
            g_demo_state.show_asset_browser_demo = true;
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Open Scene Hierarchy")) {
            g_demo_state.show_scene_hierarchy_demo = true;
        }
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        gui_text(ctx, "System Information:");
        gui_text(ctx, "GUI Context Size: %zu bytes", sizeof(gui_context));
        gui_text(ctx, "Temp Memory Pool: %zu KB", sizeof(ctx->temp_memory) / 1024);
        gui_text(ctx, "Max Windows: %d", ArrayCount(ctx->windows));
        gui_text(ctx, "Max Layout Depth: %d", ArrayCount(ctx->layout_stack));
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Test Actions:");
        if (gui_button(ctx, "Generate Test Log Entries")) {
            gui_log(ctx, "Test info message %d", rand() % 1000);
            gui_log_warning(ctx, "Test warning message %d", rand() % 1000);
            gui_log_error(ctx, "Test error message %d", rand() % 1000);
        }
        
        if (gui_button(ctx, "Clear All Logs")) {
            gui_clear_log(ctx);
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// CONSOLE DEMO
// ============================================================================

internal void
demo_show_console_window(gui_context *ctx) {
    if (!g_demo_state.show_console_demo) return;
    
    if (gui_begin_window(ctx, "Console/Log Viewer", &g_demo_state.show_console_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Console Log (%d entries):", ctx->console_log_count);
        gui_same_line(ctx, 200);
        if (gui_button(ctx, "Clear")) {
            gui_clear_log(ctx);
        }
        
        gui_separator(ctx);
        
        // Display log entries (simplified version)
        i32 display_count = hm_min(ctx->console_log_count, 20); // Show last 20 entries
        
        for (i32 i = 0; i < display_count; i++) {
            i32 entry_index = (ctx->console_log_head + ctx->console_log_count - display_count + i) % ArrayCount(ctx->console_log);
            gui_log_entry *entry = &ctx->console_log[entry_index];
            
            // Format timestamp
            char time_str[32];
            snprintf(time_str, sizeof(time_str), "[%.1fs]", entry->timestamp / 1000.0);
            
            // Display log entry with appropriate color
            gui_text_colored(ctx, ctx->theme.text_disabled, time_str);
            gui_same_line(ctx, 80);
            gui_text_colored(ctx, entry->color, entry->message);
        }
        
        if (ctx->console_log_count == 0) {
            gui_text_colored(ctx, ctx->theme.text_disabled, "No log entries. Use the buttons in other demos to generate logs.");
        }
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        gui_checkbox(ctx, "Auto-scroll to bottom", &ctx->console_auto_scroll);
        
        gui_text(ctx, "Test Log Generation:");
        if (gui_button(ctx, "Info")) {
            gui_log(ctx, "This is an info message");
        }
        gui_same_line(ctx, 10);
        if (gui_button(ctx, "Warning")) {
            gui_log_warning(ctx, "This is a warning message");
        }
        gui_same_line(ctx, 10);
        if (gui_button(ctx, "Error")) {
            gui_log_error(ctx, "This is an error message");
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// PROPERTY INSPECTOR DEMO
// ============================================================================

internal void
demo_show_property_window(gui_context *ctx) {
    if (!g_demo_state.show_property_demo) return;
    
    if (gui_begin_window(ctx, "Property Inspector", &g_demo_state.show_property_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Object Properties:");
        gui_separator(ctx);
        
        gui_text(ctx, "Transform:");
        gui_indent(ctx, 15);
        gui_slider_float(ctx, "Position X", &g_demo_state.demo_object.position.x, -10.0f, 10.0f);
        gui_slider_float(ctx, "Position Y", &g_demo_state.demo_object.position.y, -10.0f, 10.0f);
        gui_slider_float(ctx, "Position Z", &g_demo_state.demo_object.position.z, -10.0f, 10.0f);
        
        gui_slider_float(ctx, "Rotation X", &g_demo_state.demo_object.rotation.x, -3.14f, 3.14f);
        gui_slider_float(ctx, "Rotation Y", &g_demo_state.demo_object.rotation.y, -3.14f, 3.14f);
        gui_slider_float(ctx, "Rotation Z", &g_demo_state.demo_object.rotation.z, -3.14f, 3.14f);
        
        gui_slider_float(ctx, "Scale X", &g_demo_state.demo_object.scale.x, 0.1f, 5.0f);
        gui_slider_float(ctx, "Scale Y", &g_demo_state.demo_object.scale.y, 0.1f, 5.0f);
        gui_slider_float(ctx, "Scale Z", &g_demo_state.demo_object.scale.z, 0.1f, 5.0f);
        gui_unindent(ctx, 15);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Object Properties:");
        gui_indent(ctx, 15);
        gui_text(ctx, "Name: %s", g_demo_state.demo_object.name);
        gui_slider_float(ctx, "Health", &g_demo_state.demo_object.health, 0.0f, 100.0f);
        gui_slider_int(ctx, "Level", &g_demo_state.demo_object.level, 1, 100);
        gui_checkbox(ctx, "Active", &g_demo_state.demo_object.active);
        gui_unindent(ctx, 15);
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Material Color:");
        gui_indent(ctx, 15);
        gui_slider_float(ctx, "Red", &g_demo_state.demo_object.color[0], 0.0f, 1.0f);
        gui_slider_float(ctx, "Green", &g_demo_state.demo_object.color[1], 0.0f, 1.0f);
        gui_slider_float(ctx, "Blue", &g_demo_state.demo_object.color[2], 0.0f, 1.0f);
        gui_slider_float(ctx, "Alpha", &g_demo_state.demo_object.color[3], 0.0f, 1.0f);
        gui_unindent(ctx, 15);
        
        // Color preview
        color32 preview = rgb(
            (u8)(g_demo_state.demo_object.color[0] * 255),
            (u8)(g_demo_state.demo_object.color[1] * 255),
            (u8)(g_demo_state.demo_object.color[2] * 255)
        );
        gui_text_colored(ctx, preview, "Color Preview");
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        if (gui_button(ctx, "Reset to Defaults")) {
            g_demo_state.demo_object.position = v3_make(0, 0, 0);
            g_demo_state.demo_object.rotation = v3_make(0, 0, 0);
            g_demo_state.demo_object.scale = v3_make(1, 1, 1);
            g_demo_state.demo_object.health = 100.0f;
            g_demo_state.demo_object.level = 1;
            g_demo_state.demo_object.active = true;
            g_demo_state.demo_object.color[0] = 0.8f;
            g_demo_state.demo_object.color[1] = 0.2f;
            g_demo_state.demo_object.color[2] = 0.4f;
            g_demo_state.demo_object.color[3] = 1.0f;
            gui_log(ctx, "Object properties reset to defaults");
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// ASSET BROWSER DEMO
// ============================================================================

internal void
demo_show_asset_browser_window(gui_context *ctx) {
    if (!g_demo_state.show_asset_browser_demo) return;
    
    if (gui_begin_window(ctx, "Asset Browser", &g_demo_state.show_asset_browser_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Asset Browser (Simulated):");
        gui_separator(ctx);
        
        gui_text(ctx, "Current Path: %s", ctx->asset_current_path);
        
        // Simulated file listing
        const char* demo_assets[] = {
            "../ (Parent Directory)",
            "textures/ (Folder)",
            "models/ (Folder)",
            "sounds/ (Folder)",
            "scripts/ (Folder)",
            "player.png (Texture, 512x512)",
            "enemy.png (Texture, 256x256)",
            "background.jpg (Texture, 1920x1080)",
            "sword.obj (Model, 2.1KB)",
            "shield.obj (Model, 1.8KB)",
            "jump.wav (Sound, 44kHz)",
            "music.ogg (Sound, 3:42)",
        };
        
        gui_spacing(ctx, 5);
        gui_text(ctx, "Filter:");
        gui_text(ctx, "%s", g_demo_state.asset_filter);
        gui_text_colored(ctx, ctx->theme.text_disabled, "(Text input not fully implemented)");
        
        gui_spacing(ctx, 10);
        gui_text(ctx, "Assets:");
        gui_separator(ctx);
        
        for (i32 i = 0; i < ArrayCount(demo_assets); i++) {
            const char* asset_name = demo_assets[i];
            
            // Different button colors for different file types
            color32 button_color = ctx->theme.button;
            if (strstr(asset_name, "(Folder)")) {
                button_color = ctx->theme.warning;
            } else if (strstr(asset_name, "(Texture")) {
                button_color = ctx->theme.success;
            } else if (strstr(asset_name, "(Sound")) {
                button_color = rgb(100, 150, 255);
            }
            
            if (gui_button_colored(ctx, asset_name, button_color)) {
                gui_log(ctx, "Selected asset: %s", asset_name);
            }
        }
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        if (gui_button(ctx, "Refresh Assets")) {
            gui_log(ctx, "Asset browser refreshed (simulated)");
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Import Asset")) {
            gui_log(ctx, "Asset import dialog opened (simulated)");
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// SCENE HIERARCHY DEMO
// ============================================================================

internal void
demo_show_scene_hierarchy_window(gui_context *ctx) {
    if (!g_demo_state.show_scene_hierarchy_demo) return;
    
    if (gui_begin_window(ctx, "Scene Hierarchy", &g_demo_state.show_scene_hierarchy_demo, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Scene Graph:");
        gui_separator(ctx);
        
        // Simulated scene hierarchy
        if (gui_tree_node(ctx, "Scene Root")) {
            if (gui_tree_node(ctx, "Player")) {
                gui_text(ctx, "PlayerController");
                gui_text(ctx, "MeshRenderer");
                gui_text(ctx, "Collider");
            }
            
            if (gui_tree_node(ctx, "Environment")) {
                if (gui_tree_node(ctx, "Terrain")) {
                    gui_text(ctx, "TerrainRenderer");
                    gui_text(ctx, "PhysicsBody");
                }
                gui_text(ctx, "Skybox");
                if (gui_tree_node(ctx, "Props")) {
                    gui_text(ctx, "Tree_001");
                    gui_text(ctx, "Rock_001");
                    gui_text(ctx, "Grass_Patch_001");
                }
            }
            
            if (gui_tree_node(ctx, "UI")) {
                gui_text(ctx, "Main Canvas");
                gui_text(ctx, "HUD");
                gui_text(ctx, "Menu");
            }
            
            if (gui_tree_node(ctx, "Lighting")) {
                gui_text(ctx, "Directional Light (Sun)");
                gui_text(ctx, "Point Light (Torch)");
                gui_text(ctx, "Ambient Light");
            }
        }
        
        gui_spacing(ctx, 10);
        gui_separator(ctx);
        if (gui_button(ctx, "Add Object")) {
            gui_log(ctx, "Add object dialog opened (simulated)");
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Delete Selected")) {
            gui_log(ctx, "Delete selected object (simulated)");
        }
        
        if (gui_button(ctx, "Expand All")) {
            gui_log(ctx, "Expanded all hierarchy nodes (simulated)");
        }
        gui_same_line(ctx, 20);
        if (gui_button(ctx, "Collapse All")) {
            gui_log(ctx, "Collapsed all hierarchy nodes (simulated)");
        }
    }
    gui_end_window(ctx);
}

// ============================================================================
// MAIN DEMO FUNCTION
// ============================================================================

void gui_run_demo(gui_context *ctx) {
    // Initialize demo state on first run
    local_persist bool first_run = true;
    if (first_run) {
        demo_init_state();
        first_run = false;
        
        // Add some initial log messages
        gui_log(ctx, "GUI Demo started successfully");
        gui_log(ctx, "System initialized with %zu bytes temp memory", sizeof(ctx->temp_memory));
        gui_log_warning(ctx, "This is a comprehensive demo of the GUI system");
    }
    
    // Show all demo windows based on state
    demo_show_main_window(ctx);
    demo_show_widgets_window(ctx);
    demo_show_layout_window(ctx);
    demo_show_styling_window(ctx);
    demo_show_performance_window(ctx);
    demo_show_neural_window(ctx);
    demo_show_tools_window(ctx);
    demo_show_console_window(ctx);
    demo_show_property_window(ctx);
    demo_show_asset_browser_window(ctx);
    demo_show_scene_hierarchy_window(ctx);
}

// ============================================================================
// STANDALONE DEMO MAIN (if compiled as standalone)
// ============================================================================

#ifdef GUI_DEMO_STANDALONE

// Minimal platform and renderer stubs for standalone demo
typedef struct {
    struct { int x, y; bool left_down, right_down, middle_down; int wheel_delta; } mouse;
    struct { bool keys[512]; char text_input[32]; int text_input_length; } keyboard;
    f64 delta_time;
} minimal_platform;

typedef struct {
    uint32_t *pixels;
    int width, height, pitch;
} minimal_renderer;

void minimal_renderer_init(minimal_renderer *r, int width, int height) {
    r->width = width;
    r->height = height;
    r->pitch = width;
    r->pixels = malloc(width * height * sizeof(uint32_t));
}

void minimal_renderer_fill_rect(minimal_renderer *r, int x, int y, int w, int h, color32 color) {
    // Stub implementation
}

void minimal_renderer_text(minimal_renderer *r, int x, int y, const char *text, color32 color) {
    // Stub implementation
}

// Stub renderer functions for GUI system
void renderer_fill_rect(renderer *r, int x, int y, int w, int h, color32 color) {}
void renderer_draw_rect(renderer *r, int x, int y, int w, int h, color32 color) {}
void renderer_text(renderer *r, int x, int y, const char *text, color32 color) {}
void renderer_text_size(renderer *r, const char *text, int *w, int *h) { *w = strlen(text) * 8; *h = 12; }
void renderer_line(renderer *r, int x0, int y0, int x1, int y1, color32 color) {}
void renderer_fill_circle(renderer *r, int cx, int cy, int radius, color32 color) {}
void renderer_circle(renderer *r, int cx, int cy, int radius, color32 color) {}

// ReadCPUTimer is defined in the standalone math header above

int main(void) {
    printf("Handmade GUI Demo (Standalone Mode)\n");
    printf("This would run the full GUI demo with a minimal platform layer.\n");
    printf("Integrate with your platform layer to see the actual demo.\n");
    
    minimal_platform platform = {0};
    minimal_renderer r = {0};
    minimal_renderer_init(&r, 1280, 720);
    
    gui_context gui = {0};
    gui_init(&gui, (renderer*)&r, (platform_state*)&platform);
    
    // Simulate a few frames
    for (int frame = 0; frame < 10; frame++) {
        gui_begin_frame(&gui);
        gui_run_demo(&gui);
        gui_end_frame(&gui);
        printf("Frame %d: %u widgets drawn\n", frame, gui.perf.widgets_this_frame);
    }
    
    gui_shutdown(&gui);
    free(r.pixels);
    
    return 0;
}

#endif // GUI_DEMO_STANDALONE