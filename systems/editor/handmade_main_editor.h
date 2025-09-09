#ifndef HANDMADE_MAIN_EDITOR_H
#define HANDMADE_MAIN_EDITOR_H

/*
    Professional Game Editor - Main System
    
    This is the main editor window that hosts all panels and manages
    the entire editor state. Built on the handmade engine foundation.
    
    Features:
    - Docking system for flexible panel layout
    - Multi-viewport support
    - Project management
    - Tool integration
    - Performance monitoring
*/

#include "../../src/handmade.h"
#include "../renderer/handmade_renderer.h"
#include "../gui/handmade_gui.h"
#include "../physics/handmade_physics.h"
#include "../ai/handmade_neural.h"

// Forward declarations
typedef struct scene_hierarchy scene_hierarchy;
typedef struct property_inspector property_inspector;
typedef struct viewport_manager viewport_manager;
typedef struct tool_palette tool_palette;
typedef struct timeline_editor timeline_editor;
typedef struct code_editor code_editor;
typedef struct console_window console_window;
typedef struct profiler_window profiler_window;
typedef struct settings_window settings_window;
typedef struct asset_browser asset_browser;

// Editor configuration
#define EDITOR_VERSION_MAJOR 1
#define EDITOR_VERSION_MINOR 0
#define EDITOR_VERSION_PATCH 0
#define EDITOR_MAX_RECENT_PROJECTS 10
#define EDITOR_MAX_VIEWPORTS 4
#define EDITOR_MAX_PANELS 32
#define EDITOR_AUTOSAVE_INTERVAL_SECONDS 300

// Panel types
typedef enum editor_panel_type {
    PANEL_NONE = 0,
    PANEL_SCENE_HIERARCHY,
    PANEL_PROPERTY_INSPECTOR,
    PANEL_VIEWPORT,
    PANEL_TOOL_PALETTE,
    PANEL_TIMELINE,
    PANEL_CODE_EDITOR,
    PANEL_CONSOLE,
    PANEL_PROFILER,
    PANEL_SETTINGS,
    PANEL_ASSET_BROWSER,
    PANEL_MATERIAL_EDITOR,
    PANEL_PARTICLE_EDITOR,
    PANEL_NODE_EDITOR,
    PANEL_BUILD_SETTINGS,
    PANEL_COUNT
} editor_panel_type;

// Editor modes
typedef enum editor_mode {
    EDITOR_MODE_EDIT = 0,
    EDITOR_MODE_PLAY,
    EDITOR_MODE_PAUSE,
    EDITOR_MODE_STEP
} editor_mode;

// Panel state
typedef struct editor_panel {
    editor_panel_type type;
    char title[64];
    bool is_open;
    bool is_focused;
    bool is_docked;
    
    // Layout
    v2 position;
    v2 size;
    v2 min_size;
    v2 max_size;
    
    // Docking
    uint32_t dock_id;
    uint32_t dock_tab_id;
    
    // Panel-specific data pointer
    void* data;
    
    // Callbacks
    void (*on_gui)(struct editor_panel* panel, gui_context* gui);
    void (*on_update)(struct editor_panel* panel, f32 dt);
    void (*on_close)(struct editor_panel* panel);
} editor_panel;

// Docking node for panel layout
typedef struct dock_node {
    uint32_t id;
    bool is_leaf;
    bool is_horizontal_split;
    f32 split_ratio;
    
    // For leaf nodes
    uint32_t panel_indices[8];
    uint32_t panel_count;
    uint32_t active_tab;
    
    // For split nodes
    struct dock_node* left;
    struct dock_node* right;
    
    // Layout
    v2 position;
    v2 size;
} dock_node;

// Project settings
typedef struct project_settings {
    char project_path[512];
    char project_name[128];
    char company_name[128];
    char version[32];
    
    // Build settings
    uint32_t target_platforms;
    char output_directory[512];
    bool enable_optimization;
    bool enable_debug_info;
    
    // Runtime settings
    uint32_t default_resolution_width;
    uint32_t default_resolution_height;
    bool fullscreen_by_default;
    uint32_t target_fps;
} project_settings;

// Main editor state
typedef struct main_editor {
    // Core systems
    platform_state* platform;
    renderer_state* renderer;
    gui_context* gui;
    physics_world* physics;
    
    // Editor mode
    editor_mode mode;
    f64 play_mode_start_time;
    
    // Panels
    editor_panel panels[EDITOR_MAX_PANELS];
    uint32_t panel_count;
    editor_panel* focused_panel;
    
    // Docking
    dock_node* dock_root;
    uint32_t next_dock_id;
    
    // Panel systems
    scene_hierarchy* scene_hierarchy;
    property_inspector* property_inspector;
    viewport_manager* viewport_manager;
    tool_palette* tool_palette;
    timeline_editor* timeline_editor;
    code_editor* code_editor;
    console_window* console;
    profiler_window* profiler;
    settings_window* settings;
    asset_browser* asset_browser;
    
    // Project
    project_settings project;
    char recent_projects[EDITOR_MAX_RECENT_PROJECTS][512];
    uint32_t recent_project_count;
    
    // Editor preferences
    struct {
        char theme_name[64];
        f32 ui_scale;
        bool auto_save_enabled;
        uint32_t auto_save_interval;
        bool vsync_enabled;
        bool show_fps;
        bool show_stats;
        char external_code_editor[512];
        char external_image_editor[512];
    } preferences;
    
    // Layout presets
    struct {
        char name[64];
        uint8_t layout_data[4096];
        uint32_t layout_size;
    } layout_presets[8];
    uint32_t layout_preset_count;
    uint32_t active_layout_preset;
    
    // Undo/Redo system
    struct {
        void* command_buffer;
        uint32_t buffer_size;
        uint32_t write_pos;
        uint32_t read_pos;
        uint32_t undo_pos;
    } undo_system;
    
    // Performance
    struct {
        f64 frame_times[256];
        uint32_t frame_time_index;
        f64 average_frame_time;
        f64 worst_frame_time;
        uint64_t total_frames;
        uint64_t total_draws;
        uint64_t total_vertices;
    } stats;
    
    // Memory
    arena* permanent_arena;
    arena* frame_arena;
    
    // State
    bool is_running;
    bool needs_save;
    f64 last_save_time;
} main_editor;

// =============================================================================
// MAIN EDITOR API
// =============================================================================

// Initialization
main_editor* main_editor_create(platform_state* platform, renderer_state* renderer, 
                                uint64_t permanent_memory_size, uint64_t frame_memory_size);
void main_editor_destroy(main_editor* editor);

// Project management
bool main_editor_new_project(main_editor* editor, const char* project_path, const char* project_name);
bool main_editor_open_project(main_editor* editor, const char* project_path);
bool main_editor_save_project(main_editor* editor);
bool main_editor_close_project(main_editor* editor);
void main_editor_add_recent_project(main_editor* editor, const char* project_path);

// Panel management
editor_panel* main_editor_add_panel(main_editor* editor, editor_panel_type type);
void main_editor_remove_panel(main_editor* editor, editor_panel* panel);
void main_editor_focus_panel(main_editor* editor, editor_panel* panel);
editor_panel* main_editor_find_panel(main_editor* editor, editor_panel_type type);
void main_editor_toggle_panel(main_editor* editor, editor_panel_type type);

// Docking system
dock_node* main_editor_dock_split(main_editor* editor, dock_node* node, bool horizontal, f32 ratio);
void main_editor_dock_panel(main_editor* editor, editor_panel* panel, dock_node* node);
void main_editor_undock_panel(main_editor* editor, editor_panel* panel);
void main_editor_save_layout(main_editor* editor, const char* preset_name);
void main_editor_load_layout(main_editor* editor, uint32_t preset_index);

// Mode control
void main_editor_set_mode(main_editor* editor, editor_mode mode);
void main_editor_play(main_editor* editor);
void main_editor_pause(main_editor* editor);
void main_editor_stop(main_editor* editor);
void main_editor_step(main_editor* editor);

// Main loop
void main_editor_update(main_editor* editor, f32 dt);
void main_editor_render(main_editor* editor);
void main_editor_handle_input(main_editor* editor, input_event* event);

// Preferences
void main_editor_save_preferences(main_editor* editor);
void main_editor_load_preferences(main_editor* editor);
void main_editor_apply_theme(main_editor* editor, const char* theme_name);

// Undo/Redo
void main_editor_push_undo(main_editor* editor, void* command, uint32_t size);
void main_editor_undo(main_editor* editor);
void main_editor_redo(main_editor* editor);
void main_editor_clear_undo_history(main_editor* editor);

// Utilities
void main_editor_show_message(main_editor* editor, const char* title, const char* message);
bool main_editor_show_confirm(main_editor* editor, const char* title, const char* message);
const char* main_editor_show_file_dialog(main_editor* editor, const char* title, 
                                         const char* filter, bool save_mode);

#endif // HANDMADE_MAIN_EDITOR_H