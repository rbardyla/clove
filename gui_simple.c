// Simple GUI implementation - fixes all compilation issues
// Based on handmade_gui.c but with conflicts resolved

#include "gui_adapter.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Define GUI_DEMO_STANDALONE to get our types
#define GUI_DEMO_STANDALONE
#include "systems/gui/handmade_gui.h"

// ============================================================================
// INTERNAL CONSTANTS AND MACROS
// ============================================================================

#define GUI_MAX_DRAW_COMMANDS 8192
#define GUI_TEXT_BUFFER_SIZE 4096
#define GUI_VERTEX_BUFFER_SIZE 65536
#define GUI_FLT_MAX 3.402823466e+38F

// Widget sizing constants
#define GUI_DEFAULT_BUTTON_HEIGHT 20.0f
#define GUI_DEFAULT_ITEM_SPACING_Y 4.0f
#define GUI_DEFAULT_ITEM_SPACING_X 8.0f
#define GUI_DEFAULT_INDENT_SPACING 21.0f
#define GUI_DEFAULT_WINDOW_PADDING_X 8.0f
#define GUI_DEFAULT_WINDOW_PADDING_Y 8.0f
#define GUI_DEFAULT_FRAME_PADDING_X 4.0f
#define GUI_DEFAULT_FRAME_PADDING_Y 3.0f

// Color manipulation macros
#define GUI_COLOR_ALPHA(color, alpha) rgba((color).r, (color).g, (color).b, (alpha))
#define GUI_COLOR_DARKEN(color, factor) rgba((color).r * (factor), (color).g * (factor), (color).b * (factor), (color).a)
#define GUI_COLOR_LIGHTEN(color, factor) rgba(hm_min(255, (color).r + (255 - (color).r) * (factor)), hm_min(255, (color).g + (255 - (color).g) * (factor)), hm_min(255, (color).b + (255 - (color).b) * (factor)), (color).a)

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

internal gui_id 
gui_hash_data(const void *data, size_t size) {
    u64 hash = 0xcbf29ce484222325ULL;
    const u8 *bytes = (const u8 *)data;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

internal gui_id 
gui_hash_string(const char *str) {
    return gui_hash_data(str, strlen(str));
}

internal bool
gui_rect_contains_point(v2 rect_pos, v2 rect_size, v2 point) {
    return point.x >= rect_pos.x && point.x < rect_pos.x + rect_size.x &&
           point.y >= rect_pos.y && point.y < rect_pos.y + rect_size.y;
}

layout_info*
gui_current_layout(gui_context *ctx) {
    return &ctx->layout_stack[ctx->layout_depth];
}

void
gui_advance_cursor(gui_context *ctx, v2 size) {
    layout_info *layout = gui_current_layout(ctx);
    
    switch (layout->type) {
        case LAYOUT_VERTICAL:
            layout->cursor.y += size.y + layout->item_spacing;
            layout->max_extent.x = hm_max(layout->max_extent.x, size.x);
            layout->max_extent.y += size.y + layout->item_spacing;
            break;
            
        case LAYOUT_HORIZONTAL:
            layout->cursor.x += size.x + layout->item_spacing;
            layout->max_extent.x += size.x + layout->item_spacing;
            layout->max_extent.y = hm_max(layout->max_extent.y, size.y);
            break;
            
        default:
            // LAYOUT_NONE - cursor doesn't advance automatically
            break;
    }
    
    layout->content_size.x = hm_max(layout->content_size.x, layout->cursor.x - layout->pos.x);
    layout->content_size.y = hm_max(layout->content_size.y, layout->cursor.y - layout->pos.y);
}

internal void*
gui_temp_alloc_impl(gui_context *ctx, umm size) {
    size = AlignPow2(size, 8);
    if (ctx->temp_memory_used + size > sizeof(ctx->temp_memory)) {
        return NULL;
    }
    
    void* result = ctx->temp_memory + ctx->temp_memory_used;
    ctx->temp_memory_used += size;
    return result;
}

// ============================================================================
// THEME SYSTEM
// ============================================================================

gui_theme gui_dark_theme(void) {
    gui_theme theme = {0};
    theme.background = rgb(30, 30, 30);
    theme.panel = rgb(45, 45, 45);
    theme.window_bg = rgb(37, 37, 38);
    theme.titlebar = rgb(60, 60, 60);
    theme.titlebar_active = rgb(80, 80, 80);
    theme.button = rgb(60, 60, 60);
    theme.button_hover = rgb(75, 75, 75);
    theme.button_active = rgb(90, 90, 90);
    theme.text = rgb(220, 220, 220);
    theme.text_disabled = rgb(120, 120, 120);
    theme.text_selected = rgb(255, 255, 255);
    theme.border = rgb(80, 80, 80);
    theme.border_shadow = rgb(20, 20, 20);
    theme.slider_bg = rgb(50, 50, 50);
    theme.slider_fill = rgb(80, 140, 220);
    theme.slider_handle = rgb(100, 160, 240);
    theme.checkbox_bg = rgb(50, 50, 50);
    theme.checkbox_check = rgb(100, 160, 240);
    theme.input_bg = rgb(25, 25, 25);
    theme.input_border = rgb(80, 80, 80);
    theme.input_cursor = rgb(255, 255, 255);
    theme.menu_bg = rgb(50, 50, 50);
    theme.menu_hover = rgb(65, 65, 65);
    theme.tab_bg = rgb(50, 50, 50);
    theme.tab_active = rgb(37, 37, 38);
    theme.scrollbar_bg = rgb(40, 40, 40);
    theme.scrollbar_handle = rgb(70, 70, 70);
    theme.graph_bg = rgb(20, 20, 20);
    theme.graph_line = rgb(100, 220, 100);
    theme.graph_grid = rgb(50, 50, 50);
    theme.dock_preview = rgba(100, 160, 240, 128);
    theme.selection_bg = rgba(100, 160, 240, 80);
    theme.warning = rgb(255, 165, 0);
    theme.error = rgb(240, 80, 80);
    theme.success = rgb(100, 220, 100);
    return theme;
}

gui_theme gui_default_theme(void) {
    return gui_dark_theme();
}

gui_theme gui_light_theme(void) {
    return gui_dark_theme();
}

// ============================================================================
// CORE SYSTEM IMPLEMENTATION
// ============================================================================

void gui_init(gui_context *ctx, renderer *r, platform_state *p) {
    memset(ctx, 0, sizeof(gui_context));
    ctx->renderer = r;
    ctx->platform = p;
    ctx->theme = gui_dark_theme();
    
    // Initialize root layout
    layout_info *root = &ctx->layout_stack[0];
    root->type = LAYOUT_NONE;
    root->pos = v2_make(0, 0);
    root->size = v2_make((f32)r->width, (f32)r->height);
    root->cursor = v2_make(GUI_DEFAULT_WINDOW_PADDING_X, GUI_DEFAULT_WINDOW_PADDING_Y);
    root->item_spacing = GUI_DEFAULT_ITEM_SPACING_Y;
    ctx->layout_depth = 0;
    
    // Initialize performance tracking
    ctx->frame_start_time = ReadCPUTimer();
    
    // Initialize console
    ctx->console_auto_scroll = true;
}

void gui_begin_frame(gui_context *ctx) {
    ctx->frame_start_time = ReadCPUTimer();
    ctx->temp_memory_used = 0;
    
    // Update input state from platform
    platform_state *p = ctx->platform;
    
    // Mouse state
    v2 new_mouse_pos = v2_make((f32)p->mouse.x, (f32)p->mouse.y);
    ctx->mouse_delta = v2_sub(new_mouse_pos, ctx->mouse_pos);
    ctx->mouse_pos = new_mouse_pos;
    
    // Update mouse buttons
    for (int i = 0; i < 3; i++) {
        bool was_down = ctx->mouse_down[i];
        bool is_down = (i == 0 ? p->mouse.left_down : 
                       i == 1 ? p->mouse.right_down : 
                       p->mouse.middle_down);
        
        ctx->mouse_down[i] = is_down;
        ctx->mouse_clicked[i] = !was_down && is_down;
        ctx->mouse_released[i] = was_down && !is_down;
    }
    
    ctx->mouse_wheel = (f32)p->mouse.wheel_delta;
    
    // Reset per-frame state
    if (ctx->mouse_released[0]) {
        ctx->active_id = 0;
    }
    
    ctx->hot_id = 0;
    ctx->current_window = NULL;
    
    // Update performance stats
    ctx->perf.frames_rendered++;
    ctx->perf.widgets_this_frame = 0;
    ctx->perf.draw_calls_this_frame = 0;
    ctx->perf.vertices_this_frame = 0;
}

void gui_end_frame(gui_context *ctx) {
    // Update performance metrics
    u64 frame_end_time = ReadCPUTimer();
    f32 frame_time_ms = (f32)(frame_end_time - ctx->frame_start_time) / 1000.0f;
    
    ctx->perf.avg_frame_time = (ctx->perf.avg_frame_time * 0.95f) + (frame_time_ms * 0.05f);
    ctx->perf.min_frame_time = (frame_time_ms < ctx->perf.min_frame_time || ctx->perf.frames_rendered < 60) ? 
                               frame_time_ms : ctx->perf.min_frame_time;
    ctx->perf.max_frame_time = hm_max(ctx->perf.max_frame_time, frame_time_ms);
    
    // Store frame time in history
    ctx->perf.frame_time_history[ctx->perf.frame_time_history_index] = frame_time_ms;
    ctx->perf.frame_time_history_index = (ctx->perf.frame_time_history_index + 1) % FRAME_TIME_HISTORY_SIZE;
}

void gui_shutdown(gui_context *ctx) {
    memset(ctx, 0, sizeof(gui_context));
}

// ============================================================================
// ID SYSTEM
// ============================================================================

gui_id gui_get_id(gui_context *ctx, const void *ptr) {
    return gui_hash_data(&ptr, sizeof(ptr));
}

gui_id gui_get_id_str(gui_context *ctx, const char *str) {
    return gui_hash_string(str);
}

gui_id gui_get_id_int(gui_context *ctx, i32 int_id) {
    return gui_hash_data(&int_id, sizeof(int_id));
}

// ============================================================================
// BASIC WIDGET IMPLEMENTATIONS
// ============================================================================

bool gui_button_colored(gui_context *ctx, const char *label, color32 base_color) {
    layout_info *layout = gui_current_layout(ctx);
    renderer *r = ctx->renderer;
    
    // Calculate button size
    int text_w, text_h;
    renderer_text_size(r, label, &text_w, &text_h);
    v2 button_size = v2_make((f32)text_w + 2 * GUI_DEFAULT_FRAME_PADDING_X, 
                             GUI_DEFAULT_BUTTON_HEIGHT);
    
    v2 pos = layout->cursor;
    
    // Generate ID and check interaction
    gui_id id = gui_get_id_str(ctx, label);
    bool hovered = gui_rect_contains_point(pos, button_size, ctx->mouse_pos);
    
    if (hovered) {
        ctx->hot_id = id;
        if (ctx->mouse_clicked[0]) {
            ctx->active_id = id;
        }
    }
    
    bool clicked = (ctx->active_id == id && ctx->mouse_released[0] && hovered);
    
    // Determine color based on state
    color32 color = base_color;
    if (ctx->active_id == id && hovered) {
        color = GUI_COLOR_DARKEN(base_color, 0.8f);
    } else if (hovered) {
        color = GUI_COLOR_LIGHTEN(base_color, 0.1f);
    }
    
    // Draw button
    renderer_fill_rect(r, (int)pos.x, (int)pos.y, (int)button_size.x, (int)button_size.y, color);
    renderer_draw_rect(r, (int)pos.x, (int)pos.y, (int)button_size.x, (int)button_size.y, ctx->theme.border);
    
    // Center text
    v2 text_pos = v2_make(pos.x + (button_size.x - text_w) * 0.5f,
                          pos.y + (button_size.y - text_h) * 0.5f);
    renderer_text(r, (int)text_pos.x, (int)text_pos.y, label, ctx->theme.text);
    
    gui_advance_cursor(ctx, button_size);
    ctx->perf.widgets_this_frame++;
    
    return clicked;
}

bool gui_button(gui_context *ctx, const char *label) {
    return gui_button_colored(ctx, label, ctx->theme.button);
}

bool gui_checkbox(gui_context *ctx, const char *label, bool *value) {
    layout_info *layout = gui_current_layout(ctx);
    renderer *r = ctx->renderer;
    
    f32 box_size = 16.0f;
    v2 pos = layout->cursor;
    
    bool box_hovered = gui_rect_contains_point(pos, v2_make(box_size, box_size), ctx->mouse_pos);
    
    if (box_hovered && ctx->mouse_clicked[0]) {
        *value = !*value;
    }
    
    // Draw checkbox box
    color32 bg_color = box_hovered ? ctx->theme.button_hover : ctx->theme.checkbox_bg;
    renderer_fill_rect(r, (int)pos.x, (int)pos.y, (int)box_size, (int)box_size, bg_color);
    renderer_draw_rect(r, (int)pos.x, (int)pos.y, (int)box_size, (int)box_size, ctx->theme.border);
    
    // Draw checkmark if checked
    if (*value) {
        f32 check_padding = 3.0f;
        renderer_fill_rect(r, (int)(pos.x + check_padding), (int)(pos.y + check_padding), 
                          (int)(box_size - 2 * check_padding), (int)(box_size - 2 * check_padding), 
                          ctx->theme.checkbox_check);
    }
    
    // Draw label
    v2 label_pos = v2_make(pos.x + box_size + GUI_DEFAULT_ITEM_SPACING_X, pos.y + 2);
    renderer_text(r, (int)label_pos.x, (int)label_pos.y, label, ctx->theme.text);
    
    int text_w, text_h;
    renderer_text_size(r, label, &text_w, &text_h);
    v2 total_size = v2_make(box_size + GUI_DEFAULT_ITEM_SPACING_X + text_w, box_size);
    
    gui_advance_cursor(ctx, total_size);
    ctx->perf.widgets_this_frame++;
    
    return *value;
}

bool gui_slider_float(gui_context *ctx, const char *label, f32 *value, f32 min_val, f32 max_val) {
    layout_info *layout = gui_current_layout(ctx);
    renderer *r = ctx->renderer;
    
    v2 pos = layout->cursor;
    f32 slider_width = 200.0f;
    f32 slider_height = GUI_DEFAULT_BUTTON_HEIGHT;
    
    // Draw label
    renderer_text(r, (int)pos.x, (int)pos.y, label, ctx->theme.text);
    pos.y += 16;
    
    gui_id id = gui_get_id(ctx, value);
    
    // Calculate handle position
    f32 t = (*value - min_val) / (max_val - min_val);
    t = hm_clamp(t, 0.0f, 1.0f);
    f32 handle_size = 12.0f;
    f32 handle_x = pos.x + t * (slider_width - handle_size);
    
    // Check mouse interaction
    v2 slider_rect_size = v2_make(slider_width, slider_height);
    bool hovered = gui_rect_contains_point(pos, slider_rect_size, ctx->mouse_pos);
    
    if (hovered && ctx->mouse_clicked[0]) {
        ctx->active_id = id;
    }
    
    bool changed = false;
    if (ctx->active_id == id && ctx->mouse_down[0]) {
        f32 new_t = (ctx->mouse_pos.x - pos.x) / slider_width;
        new_t = hm_clamp(new_t, 0.0f, 1.0f);
        f32 new_value = min_val + new_t * (max_val - min_val);
        if (new_value != *value) {
            *value = new_value;
            changed = true;
        }
        handle_x = pos.x + new_t * (slider_width - handle_size);
    }
    
    // Draw slider track
    f32 track_y = pos.y + slider_height * 0.4f;
    f32 track_height = slider_height * 0.2f;
    renderer_fill_rect(r, (int)pos.x, (int)track_y, (int)slider_width, (int)track_height, ctx->theme.slider_bg);
    
    // Draw filled portion
    f32 fill_width = handle_x - pos.x + handle_size * 0.5f;
    renderer_fill_rect(r, (int)pos.x, (int)track_y, (int)fill_width, (int)track_height, ctx->theme.slider_fill);
    
    // Draw handle
    v2 handle_pos = v2_make(handle_x, pos.y);
    color32 handle_color = (ctx->active_id == id) ? ctx->theme.button_active : 
                          (hovered ? ctx->theme.button_hover : ctx->theme.slider_handle);
    renderer_fill_rect(r, (int)handle_pos.x, (int)handle_pos.y, (int)handle_size, (int)slider_height, handle_color);
    renderer_draw_rect(r, (int)handle_pos.x, (int)handle_pos.y, (int)handle_size, (int)slider_height, ctx->theme.border);
    
    // Draw value
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.2f", *value);
    v2 value_pos = v2_make(pos.x + slider_width + 10, pos.y + 4);
    renderer_text(r, (int)value_pos.x, (int)value_pos.y, value_str, ctx->theme.text);
    
    v2 total_size = v2_make(slider_width + 60, slider_height + 16);
    gui_advance_cursor(ctx, total_size);
    ctx->perf.widgets_this_frame++;
    
    return changed;
}

void gui_text(gui_context *ctx, const char *fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    layout_info *layout = gui_current_layout(ctx);
    renderer *r = ctx->renderer;
    
    v2 pos = layout->cursor;
    renderer_text(r, (int)pos.x, (int)pos.y, buffer, ctx->theme.text);
    
    int text_w, text_h;
    renderer_text_size(r, buffer, &text_w, &text_h);
    gui_advance_cursor(ctx, v2_make((f32)text_w, (f32)text_h));
    ctx->perf.widgets_this_frame++;
}

void gui_separator(gui_context *ctx) {
    layout_info *layout = gui_current_layout(ctx);
    renderer *r = ctx->renderer;
    
    v2 pos = layout->cursor;
    v2 size = v2_make(layout->size.x - 2 * GUI_DEFAULT_WINDOW_PADDING_X, 1);
    renderer_fill_rect(r, (int)pos.x, (int)pos.y, (int)size.x, (int)size.y, ctx->theme.border);
    gui_advance_cursor(ctx, v2_make(size.x, 5));
    ctx->perf.widgets_this_frame++;
}

// ============================================================================
// PERFORMANCE OVERLAY
// ============================================================================

void gui_performance_overlay(gui_context *ctx, bool show_graph) {
    renderer *r = ctx->renderer;
    
    // Position overlay in top-right corner
    f32 overlay_width = 300.0f;
    f32 overlay_height = 80.0f;
    v2 overlay_pos = v2_make(r->width - overlay_width - 10, 10);
    
    // Semi-transparent background
    color32 bg_color = GUI_COLOR_ALPHA(ctx->theme.panel, 200);
    renderer_fill_rect(r, (int)overlay_pos.x, (int)overlay_pos.y, 
                      (int)overlay_width, (int)overlay_height, bg_color);
    renderer_draw_rect(r, (int)overlay_pos.x, (int)overlay_pos.y, 
                      (int)overlay_width, (int)overlay_height, ctx->theme.border);
    
    // Draw performance text
    v2 text_pos = v2_add(overlay_pos, v2_make(8, 8));
    
    char perf_text[512];
    snprintf(perf_text, sizeof(perf_text), 
             "Frame Time: %.1fms (%.0f FPS)\n"
             "Widgets: %u  Draw Calls: %u",
             ctx->perf.avg_frame_time,
             1000.0f / hm_max(ctx->perf.avg_frame_time, 0.001f),
             ctx->perf.widgets_this_frame,
             ctx->perf.draw_calls_this_frame);
    
    // Split text into lines and draw each
    char *line = perf_text;
    f32 line_height = 14.0f;
    i32 line_num = 0;
    
    while (line && *line) {
        char *next_line = strchr(line, '\n');
        if (next_line) *next_line = 0;
        
        v2 line_pos = v2_make(text_pos.x, text_pos.y + line_num * line_height);
        renderer_text(r, (int)line_pos.x, (int)line_pos.y, line, ctx->theme.text);
        
        if (next_line) {
            *next_line = '\n';
            line = next_line + 1;
        } else {
            break;
        }
        line_num++;
    }
}

// ============================================================================
// SIMPLE WINDOW SYSTEM  
// ============================================================================

bool gui_begin_window(gui_context *ctx, const char *title, bool *p_open, gui_window_flags flags) {
    gui_text(ctx, "%s", title);
    gui_separator(ctx);
    return p_open ? *p_open : true;
}

void gui_end_window(gui_context *ctx) {
    // Nothing needed for simple implementation
}

// ============================================================================
// DEMO WINDOW
// ============================================================================

void gui_show_demo_window(gui_context *ctx, bool *p_open) {
    if (!p_open || !*p_open) return;
    
    if (gui_begin_window(ctx, "GUI Demo", p_open, GUI_WINDOW_NONE)) {
        gui_text(ctx, "Welcome to the GUI Demo!");
        gui_separator(ctx);
        
        static bool test_bool = true;
        gui_checkbox(ctx, "Test Checkbox", &test_bool);
        
        static f32 test_float = 0.5f;
        gui_slider_float(ctx, "Test Slider", &test_float, 0.0f, 1.0f);
        
        if (gui_button(ctx, "Test Button")) {
            printf("Button was clicked!\n");
        }
        
        gui_text(ctx, "This is colored text!");
    }
    gui_end_window(ctx);
}

// Stub functions to satisfy linker
void gui_check_hot_reload(gui_context *ctx) {}
void gui_log(gui_context *ctx, const char *fmt, ...) {}
void* gui_temp_alloc(gui_context *ctx, umm size) { return gui_temp_alloc_impl(ctx, size); }
void gui_temp_reset(gui_context *ctx) { ctx->temp_memory_used = 0; }