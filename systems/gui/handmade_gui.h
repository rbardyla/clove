// handmade_gui.h - Production-ready immediate mode GUI system
// PERFORMANCE: Zero allocations per frame, all state on stack
// TARGET: 60fps with hundreds of windows, efficient vertex batching

#ifndef HANDMADE_GUI_H
#define HANDMADE_GUI_H

#ifndef GUI_DEMO_STANDALONE
#include "handmade_platform.h"
#include "handmade_renderer.h"
#include "../renderer/handmade_math.h"
#else
// Standalone mode - minimal includes only
#include <stdint.h>
#include <stdbool.h>
#endif
#include <stddef.h>

// GUI ID system - for tracking widget state
typedef uint64_t gui_id;

// Widget states
typedef enum {
    WIDGET_IDLE = 0,
    WIDGET_HOT,      // Mouse hovering
    WIDGET_ACTIVE,    // Being interacted with
    WIDGET_DISABLED
} widget_state;

// Layout types
typedef enum {
    LAYOUT_NONE = 0,
    LAYOUT_VERTICAL,
    LAYOUT_HORIZONTAL,
    LAYOUT_GRID
} layout_type;

// Window flags for window management
typedef enum {
    GUI_WINDOW_NONE        = 0,
    GUI_WINDOW_MOVEABLE    = (1 << 0),
    GUI_WINDOW_RESIZABLE   = (1 << 1),
    GUI_WINDOW_CLOSABLE    = (1 << 2),
    GUI_WINDOW_COLLAPSIBLE = (1 << 3),
    GUI_WINDOW_NO_TITLEBAR = (1 << 4),
    GUI_WINDOW_NO_BORDER   = (1 << 5),
    GUI_WINDOW_DOCKABLE    = (1 << 6),
    GUI_WINDOW_ALWAYS_ON_TOP = (1 << 7)
} gui_window_flags;

// Dock space flags
typedef enum {
    GUI_DOCK_NONE = 0,
    GUI_DOCK_LEFT,
    GUI_DOCK_RIGHT,
    GUI_DOCK_TOP,
    GUI_DOCK_BOTTOM,
    GUI_DOCK_CENTER,
    GUI_DOCK_SPLIT_HORIZONTAL,
    GUI_DOCK_SPLIT_VERTICAL
} gui_dock_flags;

// Tree node flags
typedef enum {
    GUI_TREE_NODE_NONE = 0,
    GUI_TREE_NODE_SELECTED = (1 << 0),
    GUI_TREE_NODE_OPENED = (1 << 1),
    GUI_TREE_NODE_LEAF = (1 << 2),
    GUI_TREE_NODE_BULLET = (1 << 3)
} gui_tree_node_flags;

// Comprehensive theme colors
typedef struct {
    color32 background;
    color32 panel;
    color32 window_bg;
    color32 titlebar;
    color32 titlebar_active;
    color32 button;
    color32 button_hover;
    color32 button_active;
    color32 text;
    color32 text_disabled;
    color32 text_selected;
    color32 border;
    color32 border_shadow;
    color32 slider_bg;
    color32 slider_fill;
    color32 slider_handle;
    color32 checkbox_bg;
    color32 checkbox_check;
    color32 input_bg;
    color32 input_border;
    color32 input_cursor;
    color32 menu_bg;
    color32 menu_hover;
    color32 tab_bg;
    color32 tab_active;
    color32 scrollbar_bg;
    color32 scrollbar_handle;
    color32 graph_bg;
    color32 graph_line;
    color32 graph_grid;
    color32 dock_preview;
    color32 selection_bg;
    color32 warning;
    color32 error;
    color32 success;
} gui_theme;

// Performance metrics
#define FRAME_TIME_HISTORY_SIZE 120

typedef struct {
    u64 frames_rendered;
    f32 avg_frame_time;
    f32 min_frame_time;
    f32 max_frame_time;
    u32 widgets_this_frame;
    u32 draw_calls_this_frame;
    u32 vertices_this_frame;
    f32 cpu_usage;
    u32 memory_usage_kb;
    bool show_metrics;
    
    // Frame time history for graph
    f32 frame_time_history[FRAME_TIME_HISTORY_SIZE];
    u32 frame_time_history_index;
} gui_performance_stats;

// Console log entry
typedef struct {
    char message[256];
    color32 color;
    f64 timestamp;
    int level; // 0=info, 1=warning, 2=error
} gui_log_entry;

// Asset browser entry
typedef struct {
    char name[64];
    char path[256];
    int type; // 0=folder, 1=texture, 2=sound, 3=model, etc.
    u64 size;
    f64 modified_time;
} gui_asset_entry;

// Property inspector property
typedef struct {
    char name[32];
    int type; // 0=int, 1=float, 2=bool, 3=string, 4=color, 5=vector
    void* data;
    f32 min_val;
    f32 max_val;
} gui_property;

// Layout stack entry (legacy structure - will be replaced)
typedef struct {
    layout_type type;
    int x, y;
    int width, height;
    int cursor_x, cursor_y;
    int item_spacing;
    int max_extent;  // Maximum width/height used
} layout_info_legacy;

// Window state
typedef struct {
    gui_id id;
    char title[64];
    v2 pos;
    v2 size;
    v2 min_size;
    bool open;
    bool collapsed;
    bool docked;
    gui_dock_flags dock_side;
    gui_window_flags flags;
    f32 alpha;
} gui_window;

// Docking node
typedef struct gui_dock_node {
    gui_id id;
    v2 pos;
    v2 size;
    bool is_split;
    bool is_leaf;
    gui_dock_flags split_axis;
    f32 split_ratio;
    struct gui_dock_node* parent;
    struct gui_dock_node* child[2];
    gui_window* windows[8];
    int window_count;
    int selected_tab;
} gui_dock_node;

// Layout stack entry (enhanced)
typedef struct {
    layout_type type;
    v2 pos;
    v2 size;
    v2 cursor;
    v2 content_size;
    f32 item_spacing;
    v2 max_extent;
    int columns; // for grid layout
    int current_column;
    bool auto_wrap;
} layout_info;

// GUI context - comprehensive state management
typedef struct {
    renderer *renderer;
    platform_state *platform;
    
    // Widget state
    gui_id hot_id;       // Widget under mouse
    gui_id active_id;    // Widget being interacted with
    gui_id keyboard_id;  // Widget with keyboard focus
    gui_id last_id;      // For generating sequential IDs
    
    // Input state
    v2 mouse_pos;
    v2 mouse_delta;
    bool mouse_down[3];  // Left, Right, Middle
    bool mouse_clicked[3];
    bool mouse_released[3];
    bool mouse_double_clicked;
    f32 mouse_wheel;
    
    // Keyboard state
    bool key_down[512];
    bool key_pressed[512];  // Just pressed this frame
    char text_input[32];
    int text_input_len;
    
    // Layout stack
    layout_info layout_stack[64];
    int layout_depth;
    
    // Window management
    gui_window windows[128];
    int window_count;
    gui_window* current_window;
    
    // Docking system
    gui_dock_node* dock_space_root;
    gui_dock_node dock_nodes[256];
    int dock_node_count;
    bool dock_preview_active;
    v2 dock_preview_pos;
    v2 dock_preview_size;
    
    // Theme and styling
    gui_theme theme;
    bool theme_hot_reload;
    char theme_file_path[256];
    
    // Performance tracking
    gui_performance_stats perf;
    u64 frame_start_time;
    
    // Console system
    gui_log_entry console_log[1024];
    int console_log_count;
    int console_log_head;
    bool console_auto_scroll;
    
    // Asset browser
    gui_asset_entry assets[512];
    int asset_count;
    char asset_current_path[256];
    char asset_search_filter[64];
    
    // Property inspector
    gui_property properties[128];
    int property_count;
    void* selected_object;
    char property_search[64];
    
    // Scene hierarchy
    void* scene_root;
    void* selected_node;
    gui_tree_node_flags tree_flags[256];
    
    // Built-in windows state
    bool show_demo;
    bool show_performance;
    bool show_console;
    bool show_assets;
    bool show_properties;
    bool show_hierarchy;
    bool show_style_editor;
    
    // Hot reload
    f64 last_hot_reload_check;
    
    // Memory for temporary allocations within frame
    u8 temp_memory[Kilobytes(64)];
    umm temp_memory_used;
} gui_context;

// ============================================================================
// CORE SYSTEM
// ============================================================================

// Initialize GUI system
void gui_init(gui_context *ctx, renderer *r, platform_state *p);
void gui_begin_frame(gui_context *ctx);
void gui_end_frame(gui_context *ctx);
void gui_shutdown(gui_context *ctx);

// Hot reload support
void gui_enable_hot_reload(gui_context *ctx, const char* theme_path);
void gui_check_hot_reload(gui_context *ctx);

// ============================================================================
// WINDOW MANAGEMENT
// ============================================================================

// Window system
bool gui_begin_window(gui_context *ctx, const char *title, bool *p_open, gui_window_flags flags);
void gui_end_window(gui_context *ctx);
void gui_set_next_window_pos(gui_context *ctx, v2 pos);
void gui_set_next_window_size(gui_context *ctx, v2 size);
void gui_set_next_window_size_constraints(gui_context *ctx, v2 size_min, v2 size_max);

// Docking system
void gui_dock_space(gui_context *ctx, v2 size);
void gui_dock_window(gui_context *ctx, const char* window_name, gui_dock_flags dock_side);
bool gui_is_window_docked(gui_context *ctx, const char* window_name);

// ============================================================================
// LAYOUT MANAGEMENT
// ============================================================================

void gui_begin_panel(gui_context *ctx, const char *title);
void gui_end_panel(gui_context *ctx);
void gui_begin_layout(gui_context *ctx, layout_type type, f32 spacing);
void gui_end_layout(gui_context *ctx);
void gui_begin_grid(gui_context *ctx, int columns, f32 spacing);
void gui_end_grid(gui_context *ctx);
void gui_same_line(gui_context *ctx, f32 offset);
void gui_new_line(gui_context *ctx);
void gui_spacing(gui_context *ctx, f32 pixels);
void gui_separator(gui_context *ctx);
void gui_indent(gui_context *ctx, f32 width);
void gui_unindent(gui_context *ctx, f32 width);

// ============================================================================
// BASIC WIDGETS
// ============================================================================

// Buttons
bool gui_button(gui_context *ctx, const char *label);
bool gui_button_colored(gui_context *ctx, const char *label, color32 color);
bool gui_button_small(gui_context *ctx, const char *label);
bool gui_invisible_button(gui_context *ctx, const char *str_id, v2 size);
bool gui_arrow_button(gui_context *ctx, const char *str_id, int dir);

// Input widgets
bool gui_checkbox(gui_context *ctx, const char *label, bool *value);
bool gui_radio_button(gui_context *ctx, const char *label, bool active);
bool gui_slider_float(gui_context *ctx, const char *label, f32 *value, f32 min, f32 max);
bool gui_slider_float2(gui_context *ctx, const char *label, f32 value[2], f32 min, f32 max);
bool gui_slider_float3(gui_context *ctx, const char *label, f32 value[3], f32 min, f32 max);
bool gui_slider_int(gui_context *ctx, const char *label, i32 *value, i32 min, i32 max);
bool gui_drag_float(gui_context *ctx, const char *label, f32 *value, f32 speed, f32 min, f32 max);
bool gui_drag_int(gui_context *ctx, const char *label, i32 *value, f32 speed, i32 min, i32 max);
bool gui_input_text(gui_context *ctx, const char *label, char *buffer, size_t buffer_size);
bool gui_input_float(gui_context *ctx, const char *label, f32 *value);
bool gui_input_int(gui_context *ctx, const char *label, i32 *value);

// Color widgets
bool gui_color_edit3(gui_context *ctx, const char *label, f32 color[3]);
bool gui_color_edit4(gui_context *ctx, const char *label, f32 color[4]);
bool gui_color_picker3(gui_context *ctx, const char *label, f32 color[3]);
bool gui_color_button(gui_context *ctx, const char *desc_id, color32 color);

// Text
void gui_text(gui_context *ctx, const char *fmt, ...);
void gui_text_colored(gui_context *ctx, color32 color, const char *fmt, ...);
void gui_text_disabled(gui_context *ctx, const char *fmt, ...);
void gui_text_wrapped(gui_context *ctx, const char *fmt, ...);
void gui_label_text(gui_context *ctx, const char *label, const char *fmt, ...);
void gui_bullet_text(gui_context *ctx, const char *fmt, ...);

// ============================================================================
// ADVANCED WIDGETS
// ============================================================================

// Trees and selectables
bool gui_tree_node(gui_context *ctx, const char *label);
bool gui_tree_node_ex(gui_context *ctx, const char *label, gui_tree_node_flags flags);
void gui_tree_pop(gui_context *ctx);
bool gui_selectable(gui_context *ctx, const char *label, bool selected);
bool gui_selectable_ex(gui_context *ctx, const char *label, bool *p_selected, v2 size);

// List boxes and combos
bool gui_begin_listbox(gui_context *ctx, const char *label, v2 size);
void gui_end_listbox(gui_context *ctx);
bool gui_listbox(gui_context *ctx, const char *label, i32 *current_item, const char* const items[], i32 items_count);
bool gui_combo(gui_context *ctx, const char *label, i32 *current_item, const char* const items[], i32 items_count);
bool gui_begin_combo(gui_context *ctx, const char *label, const char *preview_value);
void gui_end_combo(gui_context *ctx);

// Menus
bool gui_begin_menu_bar(gui_context *ctx);
void gui_end_menu_bar(gui_context *ctx);
bool gui_begin_menu(gui_context *ctx, const char *label);
void gui_end_menu(gui_context *ctx);
bool gui_menu_item(gui_context *ctx, const char *label, const char *shortcut, bool selected, bool enabled);

// Tabs
bool gui_begin_tab_bar(gui_context *ctx, const char *str_id);
void gui_end_tab_bar(gui_context *ctx);
bool gui_begin_tab_item(gui_context *ctx, const char *label, bool *p_open);
void gui_end_tab_item(gui_context *ctx);

// Progress and status
void gui_progress_bar(gui_context *ctx, f32 fraction, v2 size, const char *overlay);
void gui_spinner(gui_context *ctx, f32 radius, i32 thickness, color32 color);

// ============================================================================
// VISUALIZATION WIDGETS
// ============================================================================

void gui_plot_lines(gui_context *ctx, const char *label, f32 *values, i32 values_count, f32 scale_min, f32 scale_max, v2 graph_size);
void gui_plot_histogram(gui_context *ctx, const char *label, f32 *values, i32 values_count, f32 scale_min, f32 scale_max, v2 graph_size);
void gui_heatmap(gui_context *ctx, const char *label, f32 *values, i32 width, i32 height, f32 scale_min, f32 scale_max, v2 size);

// Neural network visualization
typedef struct {
    i32 layer_count;
    i32 *layer_sizes;
    f32 **weights;
    f32 *activations;
    f32 *biases;
    const char **layer_names;
} gui_neural_network;

void gui_neural_network_viewer(gui_context *ctx, const char *label, gui_neural_network *network, v2 size);
void gui_tensor_viewer(gui_context *ctx, const char *label, f32 *data, i32 dims[], i32 dim_count, v2 size);
void gui_activation_graph(gui_context *ctx, const char *label, f32 *activations, i32 count, v2 size);

// ============================================================================
// BUILT-IN PRODUCTION TOOLS
// ============================================================================

// Performance overlay
void gui_show_performance_metrics(gui_context *ctx, bool *p_open);
void gui_performance_overlay(gui_context *ctx, bool show_graph);

// Property inspector
void gui_show_property_inspector(gui_context *ctx, bool *p_open, void *object);
void gui_property_float(gui_context *ctx, const char *name, f32 *value, f32 min, f32 max);
void gui_property_int(gui_context *ctx, const char *name, i32 *value, i32 min, i32 max);
void gui_property_bool(gui_context *ctx, const char *name, bool *value);
void gui_property_color(gui_context *ctx, const char *name, f32 color[4]);
void gui_property_string(gui_context *ctx, const char *name, char *buffer, size_t buffer_size);
void gui_property_vector3(gui_context *ctx, const char *name, f32 vector[3]);

// Console/log viewer
void gui_show_console(gui_context *ctx, bool *p_open);
void gui_log(gui_context *ctx, const char *fmt, ...);
void gui_log_warning(gui_context *ctx, const char *fmt, ...);
void gui_log_error(gui_context *ctx, const char *fmt, ...);
void gui_clear_log(gui_context *ctx);

// Asset browser
void gui_show_asset_browser(gui_context *ctx, bool *p_open);
bool gui_asset_thumbnail(gui_context *ctx, const char *path, v2 size);
void gui_refresh_assets(gui_context *ctx, const char *root_path);

// Scene hierarchy
void gui_show_scene_hierarchy(gui_context *ctx, bool *p_open, void *scene_root);
void gui_hierarchy_node(gui_context *ctx, void *node, const char *name, bool has_children);

// Style/theme editor
void gui_show_style_editor(gui_context *ctx, bool *p_open);
void gui_style_color_picker(gui_context *ctx, const char *name, color32 *color);

// Demo window
void gui_show_demo_window(gui_context *ctx, bool *p_open);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// ID generation
gui_id gui_get_id(gui_context *ctx, const void *ptr);
gui_id gui_get_id_str(gui_context *ctx, const char *str);
gui_id gui_get_id_int(gui_context *ctx, i32 int_id);

// Internal helper functions (exposed for advanced usage)
layout_info* gui_current_layout(gui_context *ctx);
void gui_advance_cursor(gui_context *ctx, v2 size);

// Widget state
bool gui_is_item_hovered(gui_context *ctx);
bool gui_is_item_active(gui_context *ctx);
bool gui_is_item_clicked(gui_context *ctx, i32 button);
bool gui_is_item_visible(gui_context *ctx);
bool gui_is_window_focused(gui_context *ctx);
bool gui_is_window_hovered(gui_context *ctx);

// Layout helpers
v2 gui_get_content_region_avail(gui_context *ctx);
v2 gui_get_content_region_max(gui_context *ctx);
v2 gui_get_window_pos(gui_context *ctx);
v2 gui_get_window_size(gui_context *ctx);
v2 gui_get_cursor_pos(gui_context *ctx);
void gui_set_cursor_pos(gui_context *ctx, v2 pos);
f32 gui_get_text_line_height(gui_context *ctx);
f32 gui_get_frame_height(gui_context *ctx);

// Drawing helpers
void gui_draw_line(gui_context *ctx, v2 p1, v2 p2, color32 color, f32 thickness);
void gui_draw_rect(gui_context *ctx, v2 p_min, v2 p_max, color32 color, f32 rounding, f32 thickness);
void gui_draw_rect_filled(gui_context *ctx, v2 p_min, v2 p_max, color32 color, f32 rounding);
void gui_draw_circle(gui_context *ctx, v2 center, f32 radius, color32 color, i32 segments, f32 thickness);
void gui_draw_circle_filled(gui_context *ctx, v2 center, f32 radius, color32 color, i32 segments);
void gui_draw_text(gui_context *ctx, v2 pos, color32 color, const char *text_begin, const char *text_end);

// Theme management
gui_theme gui_default_theme(void);
gui_theme gui_dark_theme(void);
gui_theme gui_light_theme(void);
void gui_load_theme(gui_context *ctx, const char *filename);
void gui_save_theme(gui_context *ctx, const char *filename);
void gui_push_style_color(gui_context *ctx, i32 idx, color32 color);
void gui_pop_style_color(gui_context *ctx, i32 count);

// Memory management
void* gui_temp_alloc(gui_context *ctx, umm size);
void gui_temp_reset(gui_context *ctx);

// Input helpers
bool gui_is_key_pressed(gui_context *ctx, i32 key);
bool gui_is_key_down(gui_context *ctx, i32 key);
bool gui_is_mouse_clicked(gui_context *ctx, i32 button);
bool gui_is_mouse_down(gui_context *ctx, i32 button);
v2 gui_get_mouse_pos(gui_context *ctx);
v2 gui_get_mouse_delta(gui_context *ctx);
f32 gui_get_mouse_wheel(gui_context *ctx);

#endif // HANDMADE_GUI_H