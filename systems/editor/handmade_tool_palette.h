#ifndef HANDMADE_TOOL_PALETTE_H
#define HANDMADE_TOOL_PALETTE_H

/*
    Tool Palette System
    
    Provides all creation and manipulation tools for the editor including
    primitive creation, terrain sculpting, painting, and custom tools.
*/

#include "handmade_main_editor.h"
#include "handmade_gizmos.h"

#define MAX_TOOLS 64
#define MAX_TOOL_PRESETS 16
#define TOOL_HISTORY_SIZE 32

// Tool categories
typedef enum tool_category {
    TOOL_CATEGORY_SELECTION = 0,
    TOOL_CATEGORY_TRANSFORM,
    TOOL_CATEGORY_CREATION,
    TOOL_CATEGORY_TERRAIN,
    TOOL_CATEGORY_PAINTING,
    TOOL_CATEGORY_SCULPTING,
    TOOL_CATEGORY_MEASUREMENT,
    TOOL_CATEGORY_ANNOTATION,
    TOOL_CATEGORY_CUSTOM
} tool_category;

// Tool types
typedef enum tool_type {
    // Selection tools
    TOOL_SELECT = 0,
    TOOL_SELECT_BOX,
    TOOL_SELECT_LASSO,
    TOOL_SELECT_PAINT,
    TOOL_SELECT_MAGIC_WAND,
    
    // Transform tools
    TOOL_MOVE,
    TOOL_ROTATE,
    TOOL_SCALE,
    TOOL_UNIVERSAL_TRANSFORM,
    
    // Creation tools
    TOOL_CREATE_CUBE,
    TOOL_CREATE_SPHERE,
    TOOL_CREATE_CYLINDER,
    TOOL_CREATE_PLANE,
    TOOL_CREATE_CAPSULE,
    TOOL_CREATE_TORUS,
    TOOL_CREATE_CUSTOM_MESH,
    TOOL_CREATE_LIGHT,
    TOOL_CREATE_CAMERA,
    TOOL_CREATE_EMPTY,
    TOOL_CREATE_PARTICLE_SYSTEM,
    TOOL_CREATE_AUDIO_SOURCE,
    
    // Terrain tools
    TOOL_TERRAIN_RAISE,
    TOOL_TERRAIN_LOWER,
    TOOL_TERRAIN_SMOOTH,
    TOOL_TERRAIN_FLATTEN,
    TOOL_TERRAIN_RAMP,
    TOOL_TERRAIN_EROSION,
    TOOL_TERRAIN_PAINT_TEXTURE,
    TOOL_TERRAIN_PAINT_FOLIAGE,
    
    // Painting tools
    TOOL_PAINT_VERTEX_COLOR,
    TOOL_PAINT_WEIGHT,
    TOOL_PAINT_TEXTURE,
    
    // Sculpting tools
    TOOL_SCULPT_DRAW,
    TOOL_SCULPT_SMOOTH,
    TOOL_SCULPT_PINCH,
    TOOL_SCULPT_INFLATE,
    TOOL_SCULPT_GRAB,
    TOOL_SCULPT_CREASE,
    
    // Measurement tools
    TOOL_MEASURE_DISTANCE,
    TOOL_MEASURE_ANGLE,
    TOOL_MEASURE_AREA,
    TOOL_MEASURE_VOLUME,
    
    // Annotation tools
    TOOL_ANNOTATE_TEXT,
    TOOL_ANNOTATE_ARROW,
    TOOL_ANNOTATE_SHAPE,
    
    TOOL_COUNT
} tool_type;

// Tool settings
typedef struct tool_settings {
    // Common settings
    f32 strength;
    f32 size;
    f32 falloff;
    bool symmetric;
    bool proportional;
    
    // Snapping
    bool snap_enabled;
    f32 snap_grid_size;
    f32 snap_angle;
    bool snap_to_surface;
    bool snap_to_vertex;
    
    // Constraints
    bool constrain_to_axis;
    v3 constraint_axis;
    bool constrain_to_plane;
    plane constraint_plane;
    
    // Tool-specific settings
    union {
        // Selection
        struct {
            bool additive;
            bool subtractive;
            f32 tolerance;
        } selection;
        
        // Creation
        struct {
            v3 dimensions;
            uint32_t segments_x, segments_y, segments_z;
            f32 radius;
            f32 height;
        } creation;
        
        // Terrain
        struct {
            f32 brush_hardness;
            uint32_t texture_index;
            f32 texture_opacity;
            bool use_heightmap;
            void* heightmap_data;
        } terrain;
        
        // Painting
        struct {
            v4 color;
            f32 opacity;
            uint32_t blend_mode;
            uint32_t channel_mask;
        } painting;
        
        // Sculpting
        struct {
            bool use_symmetry;
            uint32_t symmetry_axis;
            bool dynamic_topology;
            f32 detail_size;
        } sculpting;
    };
} tool_settings;

// Tool preset
typedef struct tool_preset {
    char name[64];
    tool_type type;
    tool_settings settings;
    uint32_t hotkey;
} tool_preset;

// Tool state
typedef struct tool_state {
    tool_type type;
    tool_category category;
    char name[64];
    char icon[64];
    
    // Settings
    tool_settings settings;
    tool_settings default_settings;
    
    // State
    bool is_active;
    bool is_enabled;
    bool is_visible;
    
    // Operation state
    bool is_operating;
    v3 operation_start_pos;
    v3 operation_current_pos;
    f32 operation_distance;
    f64 operation_start_time;
    
    // Preview
    bool show_preview;
    void* preview_data;
    
    // Callbacks
    void (*on_activate)(struct tool_state* tool);
    void (*on_deactivate)(struct tool_state* tool);
    void (*on_update)(struct tool_state* tool, f32 dt);
    void (*on_render)(struct tool_state* tool, renderer_state* renderer);
    bool (*on_mouse_down)(struct tool_state* tool, v2 pos, int button);
    bool (*on_mouse_move)(struct tool_state* tool, v2 pos, v2 delta);
    bool (*on_mouse_up)(struct tool_state* tool, v2 pos, int button);
    bool (*on_key)(struct tool_state* tool, int key, bool down);
    
    // Custom data
    void* user_data;
} tool_state;

// Brush settings for painting/sculpting
typedef struct brush_settings {
    // Shape
    enum {
        BRUSH_SHAPE_CIRCLE,
        BRUSH_SHAPE_SQUARE,
        BRUSH_SHAPE_CUSTOM
    } shape;
    
    // Profile
    f32* falloff_curve;
    uint32_t curve_points;
    
    // Dynamics
    bool pressure_affects_size;
    bool pressure_affects_strength;
    bool pressure_affects_opacity;
    
    // Stroke
    f32 spacing;
    f32 smoothing;
    bool use_lazy_mouse;
    f32 lazy_radius;
    
    // Texture
    uint32_t texture_id;
    f32 texture_scale;
    f32 texture_rotation;
} brush_settings;

// Tool history entry
typedef struct tool_history_entry {
    tool_type tool;
    f64 timestamp;
    void* undo_data;
    uint32_t undo_size;
} tool_history_entry;

// Tool palette
typedef struct tool_palette {
    // Tools
    tool_state tools[MAX_TOOLS];
    uint32_t tool_count;
    tool_state* active_tool;
    tool_state* previous_tool;
    
    // Categories
    bool category_expanded[9];
    
    // Presets
    tool_preset presets[MAX_TOOL_PRESETS];
    uint32_t preset_count;
    
    // Brush settings
    brush_settings brush;
    
    // History
    tool_history_entry history[TOOL_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;
    
    // Quick access toolbar
    tool_type quick_tools[10];
    uint32_t quick_tool_count;
    
    // Display
    bool show_labels;
    bool show_tooltips;
    bool compact_mode;
    f32 icon_size;
    
    // Callbacks
    void (*on_tool_changed)(tool_palette* palette, tool_state* old_tool, tool_state* new_tool);
    void (*on_operation_complete)(tool_palette* palette, tool_state* tool);
    
    // Memory
    arena* arena;
} tool_palette;

// =============================================================================
// TOOL PALETTE API
// =============================================================================

// Initialization
tool_palette* tool_palette_create(arena* arena);
void tool_palette_destroy(tool_palette* palette);
void tool_palette_init_default_tools(tool_palette* palette);

// Tool management
tool_state* tool_palette_add_tool(tool_palette* palette, tool_type type, const char* name);
void tool_palette_remove_tool(tool_palette* palette, tool_type type);
tool_state* tool_palette_get_tool(tool_palette* palette, tool_type type);
void tool_palette_activate_tool(tool_palette* palette, tool_type type);
void tool_palette_deactivate_current(tool_palette* palette);
void tool_palette_toggle_tool(tool_palette* palette, tool_type type);

// Tool operations
bool tool_palette_begin_operation(tool_palette* palette, v3 start_pos);
void tool_palette_update_operation(tool_palette* palette, v3 current_pos);
void tool_palette_end_operation(tool_palette* palette);
void tool_palette_cancel_operation(tool_palette* palette);

// Settings
void tool_palette_set_settings(tool_palette* palette, tool_type type, const tool_settings* settings);
void tool_palette_reset_settings(tool_palette* palette, tool_type type);
void tool_palette_apply_preset(tool_palette* palette, uint32_t preset_index);

// Presets
uint32_t tool_palette_save_preset(tool_palette* palette, const char* name);
void tool_palette_load_preset(tool_palette* palette, uint32_t index);
void tool_palette_delete_preset(tool_palette* palette, uint32_t index);

// Brush
void tool_palette_set_brush_size(tool_palette* palette, f32 size);
void tool_palette_set_brush_strength(tool_palette* palette, f32 strength);
void tool_palette_set_brush_falloff(tool_palette* palette, f32* curve, uint32_t points);

// Quick access
void tool_palette_add_quick_tool(tool_palette* palette, tool_type type);
void tool_palette_remove_quick_tool(tool_palette* palette, tool_type type);
void tool_palette_activate_quick_tool(tool_palette* palette, uint32_t index);

// History
void tool_palette_undo(tool_palette* palette);
void tool_palette_redo(tool_palette* palette);
void tool_palette_clear_history(tool_palette* palette);

// GUI
void tool_palette_draw_panel(tool_palette* palette, gui_context* gui);
void tool_palette_draw_toolbar(tool_palette* palette, gui_context* gui);
void tool_palette_draw_settings(tool_palette* palette, gui_context* gui);
void tool_palette_draw_brush_preview(tool_palette* palette, gui_context* gui, v2 pos);

// Input
bool tool_palette_handle_input(tool_palette* palette, input_event* event);
bool tool_palette_handle_hotkey(tool_palette* palette, uint32_t key, bool ctrl, bool shift, bool alt);

// Tool-specific operations
void tool_create_primitive(tool_palette* palette, tool_type primitive_type, v3 position);
void tool_terrain_modify(tool_palette* palette, v3 position, f32 delta);
void tool_paint_vertex_colors(tool_palette* palette, uint32_t* vertices, uint32_t count, v4 color);
void tool_sculpt_mesh(tool_palette* palette, void* mesh, v3 position, v3 normal);

// Helpers
const char* tool_get_name(tool_type type);
const char* tool_get_icon(tool_type type);
tool_category tool_get_category(tool_type type);
bool tool_requires_selection(tool_type type);
bool tool_is_continuous(tool_type type);

#endif // HANDMADE_TOOL_PALETTE_H