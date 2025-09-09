// handmade_editor_dock.h - Professional docking system for game engine editor
// PERFORMANCE: Zero allocations per frame, cache-coherent tree traversal
// TARGET: 100+ docked windows at 60fps with smooth animations

#ifndef HANDMADE_EDITOR_DOCK_H
#define HANDMADE_EDITOR_DOCK_H

#include "../gui/handmade_gui.h"
#include "../renderer/handmade_math.h"

#define MAX_DOCK_NODES 256
#define MAX_WINDOWS_PER_DOCK 32
#define DOCK_MIN_SIZE 100.0f
#define DOCK_SPLITTER_SIZE 4.0f
#define DOCK_TAB_HEIGHT 25.0f
#define DOCK_ANIMATION_SPEED 10.0f

// Forward declarations
typedef struct dock_node dock_node;
typedef struct dock_window dock_window;
typedef struct dock_manager dock_manager;

// Dock split types
typedef enum {
    DOCK_SPLIT_NONE = 0,
    DOCK_SPLIT_HORIZONTAL,  // Left/Right
    DOCK_SPLIT_VERTICAL,    // Top/Bottom
} dock_split_type;

// Dock drop zones for preview
typedef enum {
    DOCK_DROP_NONE = 0,
    DOCK_DROP_CENTER,
    DOCK_DROP_LEFT,
    DOCK_DROP_RIGHT,
    DOCK_DROP_TOP,
    DOCK_DROP_BOTTOM,
    DOCK_DROP_TAB,
} dock_drop_zone;

// Window state for docking
typedef struct dock_window {
    gui_id id;
    char title[64];
    v2 undocked_pos;
    v2 undocked_size;
    bool is_docked;
    dock_node* dock_node;
    bool is_visible;
    bool is_focused;
    void* user_data;
    
    // Animation state
    f32 tab_hover_t;
    f32 tab_active_t;
} dock_window;

// Cache-aligned dock node for optimal traversal
typedef struct dock_node {
    // Core identification - first cache line (64 bytes)
    gui_id id;
    v2 pos;
    v2 size;
    v2 content_pos;
    v2 content_size;
    dock_split_type split_type;
    f32 split_ratio;
    f32 split_ratio_target;
    f32 _pad0[3];  // Padding to 64 bytes
    
    // Tree structure - second cache line
    dock_node* parent;
    dock_node* children[2];
    
    // Window management
    dock_window* windows[MAX_WINDOWS_PER_DOCK];
    i32 window_count;
    i32 active_tab;
    
    // Visual state
    f32 animation_t;
    bool is_visible;
    bool is_active;
    bool _pad1[2];
    
    // Performance tracking
    u64 last_update_cycles;
    u32 draw_calls_this_frame;
    u32 _pad2;
} CACHE_ALIGN dock_node;

// Drag state for docking operations
typedef struct dock_drag_state {
    bool is_dragging;
    dock_window* dragged_window;
    v2 drag_offset;
    dock_node* source_node;
    dock_node* target_node;
    dock_drop_zone drop_zone;
    f32 preview_alpha;
} dock_drag_state;

// Resize state for splitters
typedef struct dock_resize_state {
    bool is_resizing;
    dock_node* resize_node;
    bool resize_child_index;
    v2 resize_start_pos;
    f32 resize_start_ratio;
} dock_resize_state;

// Preview overlay for dock operations
typedef struct dock_preview {
    bool active;
    rect preview_rect;
    dock_drop_zone zone;
    f32 alpha;
    f32 alpha_target;
} dock_preview;

// Performance statistics
typedef struct dock_performance_stats {
    u64 tree_traversal_cycles;
    u64 layout_update_cycles;
    u64 render_cycles;
    u32 nodes_traversed;
    u32 windows_rendered;
    u32 draw_calls;
    f32 frame_time_ms;
} dock_performance_stats;

// Main dock manager
typedef struct dock_manager {
    // Node pool - cache-friendly allocation
    dock_node nodes[MAX_DOCK_NODES];
    u32 node_count;
    u32 free_node_indices[MAX_DOCK_NODES];
    u32 free_node_count;
    
    // Window registry
    dock_window windows[MAX_DOCK_NODES * 4];  // Assuming avg 4 windows per node
    u32 window_count;
    
    // Root dockspace
    dock_node* root;
    v2 viewport_pos;
    v2 viewport_size;
    
    // Interaction state
    dock_drag_state drag;
    dock_resize_state resize;
    dock_preview preview;
    
    // Hot/active tracking
    gui_id hot_node;
    gui_id active_node;
    gui_id hot_tab;
    gui_id active_tab;
    
    // Configuration
    f32 min_node_size;
    f32 splitter_size;
    f32 tab_height;
    f32 animation_speed;
    
    // Theme
    color32 color_splitter;
    color32 color_splitter_hover;
    color32 color_splitter_active;
    color32 color_tab_bg;
    color32 color_tab_active;
    color32 color_tab_hover;
    color32 color_drop_overlay;
    color32 color_window_bg;
    
    // Performance tracking
    dock_performance_stats stats;
    bool show_debug_overlay;
    
    // GUI context reference
    gui_context* gui;
    
} dock_manager;

// ============================================================================
// CORE API
// ============================================================================

// Initialize dock manager
void dock_init(dock_manager* dm, gui_context* gui);
void dock_shutdown(dock_manager* dm);

// Create root dockspace
void dock_begin_dockspace(dock_manager* dm, const char* id, v2 pos, v2 size);
void dock_end_dockspace(dock_manager* dm);

// Window docking
dock_window* dock_register_window(dock_manager* dm, const char* title, gui_id id);
bool dock_begin_window(dock_manager* dm, const char* title, bool* p_open);
void dock_end_window(dock_manager* dm);

// Manual docking control
void dock_dock_window(dock_manager* dm, dock_window* window, dock_node* target, dock_drop_zone zone);
void dock_undock_window(dock_manager* dm, dock_window* window);
void dock_float_window(dock_manager* dm, dock_window* window, v2 pos, v2 size);

// Node operations
dock_node* dock_split_node(dock_manager* dm, dock_node* node, dock_split_type split, f32 ratio);
void dock_merge_node(dock_manager* dm, dock_node* node);
void dock_set_split_ratio(dock_manager* dm, dock_node* node, f32 ratio);

// ============================================================================
// LAYOUT & RENDERING
// ============================================================================

// Update dock tree layout (call once per frame)
void dock_update_layout(dock_manager* dm, f32 dt);

// Render dock system
void dock_render(dock_manager* dm);
void dock_render_debug_overlay(dock_manager* dm);

// Handle input
void dock_handle_input(dock_manager* dm, v2 mouse_pos, bool mouse_down, bool mouse_clicked);

// ============================================================================
// QUERIES
// ============================================================================

// Find operations
dock_node* dock_find_node_at_pos(dock_manager* dm, v2 pos);
dock_window* dock_find_window(dock_manager* dm, const char* title);
dock_node* dock_get_window_node(dock_manager* dm, dock_window* window);

// State queries
bool dock_is_window_docked(dock_manager* dm, const char* title);
bool dock_is_window_visible(dock_manager* dm, const char* title);
bool dock_is_window_focused(dock_manager* dm, const char* title);

// Layout queries
rect dock_get_content_rect(dock_manager* dm, dock_node* node);
v2 dock_get_window_size(dock_manager* dm, dock_window* window);

// ============================================================================
// SERIALIZATION
// ============================================================================

// Save/load dock layout
void dock_save_layout(dock_manager* dm, const char* filename);
void dock_load_layout(dock_manager* dm, const char* filename);

// Layout presets
void dock_apply_preset_default(dock_manager* dm);
void dock_apply_preset_code(dock_manager* dm);     // Code editing layout
void dock_apply_preset_art(dock_manager* dm);      // Art/level design layout
void dock_apply_preset_debug(dock_manager* dm);    // Debugging layout

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Node management
dock_node* dock_alloc_node(dock_manager* dm);
void dock_free_node(dock_manager* dm, dock_node* node);
void dock_clear_node(dock_node* node);

// Tree operations
void dock_update_node_recursive(dock_manager* dm, dock_node* node, f32 dt);
void dock_render_node_recursive(dock_manager* dm, dock_node* node);
void dock_calculate_layout_recursive(dock_node* node);

// Interaction helpers
dock_drop_zone dock_get_drop_zone(dock_node* node, v2 mouse_pos);
rect dock_get_drop_zone_rect(dock_node* node, dock_drop_zone zone);
bool dock_is_over_splitter(dock_node* node, v2 mouse_pos, f32 threshold);

// Animation helpers
void dock_animate_split_ratio(dock_node* node, f32 dt, f32 speed);
void dock_animate_preview(dock_preview* preview, f32 dt, f32 speed);

// Tab rendering
void dock_render_tab_bar(dock_manager* dm, dock_node* node);
void dock_render_tab(dock_manager* dm, dock_window* window, rect tab_rect, bool is_active, bool is_hover);

// Breadth-first traversal for cache efficiency
typedef void (*dock_node_visitor)(dock_node* node, void* user_data);
void dock_traverse_breadth_first(dock_manager* dm, dock_node_visitor visitor, void* user_data);

// SIMD-optimized layout calculations
void dock_calculate_layouts_simd(dock_manager* dm);

#endif // HANDMADE_EDITOR_DOCK_H