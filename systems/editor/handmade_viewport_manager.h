#ifndef HANDMADE_VIEWPORT_MANAGER_H
#define HANDMADE_VIEWPORT_MANAGER_H

/*
    Viewport Manager System
    
    Manages multiple 3D viewports for scene editing with different camera modes,
    rendering options, and manipulation tools. Supports split-screen layouts,
    picture-in-picture, and synchronized views.
*/

#include "handmade_main_editor.h"
#include "handmade_gizmos.h"
#include "../renderer/handmade_renderer.h"

#define MAX_VIEWPORTS 8
#define VIEWPORT_MIN_SIZE 200
#define VIEWPORT_OVERLAY_LAYERS 4

// Viewport display modes
typedef enum viewport_mode {
    VIEWPORT_PERSPECTIVE = 0,
    VIEWPORT_ORTHOGRAPHIC,
    VIEWPORT_TOP,
    VIEWPORT_FRONT,
    VIEWPORT_RIGHT,
    VIEWPORT_LEFT,
    VIEWPORT_BACK,
    VIEWPORT_BOTTOM,
    VIEWPORT_CUSTOM
} viewport_mode;

// Shading modes
typedef enum viewport_shading {
    SHADING_WIREFRAME = 0,
    SHADING_SOLID,
    SHADING_SHADED,
    SHADING_TEXTURED,
    SHADING_MATERIAL_PREVIEW,
    SHADING_LIGHTING_ONLY,
    SHADING_OVERDRAW,
    SHADING_DEPTH,
    SHADING_NORMALS,
    SHADING_UVS
} viewport_shading;

// Viewport flags
typedef enum viewport_flags {
    VIEWPORT_SHOW_GRID = (1 << 0),
    VIEWPORT_SHOW_GIZMOS = (1 << 1),
    VIEWPORT_SHOW_STATS = (1 << 2),
    VIEWPORT_SHOW_WIREFRAME_OVERLAY = (1 << 3),
    VIEWPORT_SHOW_BOUNDS = (1 << 4),
    VIEWPORT_SHOW_COLLIDERS = (1 << 5),
    VIEWPORT_SHOW_LIGHTS = (1 << 6),
    VIEWPORT_SHOW_CAMERAS = (1 << 7),
    VIEWPORT_SHOW_PARTICLES = (1 << 8),
    VIEWPORT_SHOW_AUDIO_SOURCES = (1 << 9),
    VIEWPORT_SHOW_NAV_MESH = (1 << 10),
    VIEWPORT_SHOW_SELECTION_OUTLINE = (1 << 11),
    VIEWPORT_ENABLE_POST_PROCESS = (1 << 12),
    VIEWPORT_ENABLE_BLOOM = (1 << 13),
    VIEWPORT_ENABLE_SSAO = (1 << 14),
    VIEWPORT_ENABLE_FOG = (1 << 15)
} viewport_flags;

// Camera controller
typedef struct viewport_camera {
    // Transform
    v3 position;
    quat rotation;
    f32 fov;
    f32 near_plane;
    f32 far_plane;
    
    // Orbit camera
    v3 orbit_target;
    f32 orbit_distance;
    f32 orbit_pitch;
    f32 orbit_yaw;
    
    // Movement
    v3 velocity;
    f32 move_speed;
    f32 rotate_speed;
    f32 zoom_speed;
    f32 smoothing;
    
    // Matrices
    m4x4 view_matrix;
    m4x4 projection_matrix;
    m4x4 view_projection;
    
    // Cached state
    bool matrices_dirty;
} viewport_camera;

// Viewport overlay
typedef struct viewport_overlay {
    char text[256];
    v2 position;
    v4 color;
    f32 scale;
    bool enabled;
} viewport_overlay;

// Viewport statistics
typedef struct viewport_stats {
    uint32_t draw_calls;
    uint32_t triangles;
    uint32_t vertices;
    f32 frame_time_ms;
    f32 gpu_time_ms;
    uint32_t texture_memory_mb;
    uint32_t mesh_memory_mb;
} viewport_stats;

// Individual viewport
typedef struct viewport {
    uint32_t id;
    char name[64];
    bool is_active;
    bool is_focused;
    bool is_maximized;
    
    // Display
    viewport_mode mode;
    viewport_shading shading;
    uint32_t flags;
    
    // Camera
    viewport_camera camera;
    bool camera_locked;
    
    // Rendering
    uint32_t framebuffer;
    uint32_t color_texture;
    uint32_t depth_texture;
    uint32_t picking_texture;
    v2 size;
    v2 position;
    
    // Gizmos
    gizmo_system* gizmo_system;
    uint32_t active_gizmo_id;
    
    // Grid
    struct {
        bool visible;
        f32 spacing;
        uint32_t subdivisions;
        v4 color_major;
        v4 color_minor;
        f32 fade_distance;
    } grid;
    
    // Overlays
    viewport_overlay overlays[VIEWPORT_OVERLAY_LAYERS];
    uint32_t overlay_count;
    
    // Statistics
    viewport_stats stats;
    bool show_stats;
    
    // Selection
    uint32_t* selected_objects;
    uint32_t selection_count;
    
    // Custom render callback
    void (*custom_render)(struct viewport* vp, renderer_state* renderer);
} viewport;

// Viewport layout presets
typedef enum viewport_layout {
    LAYOUT_SINGLE = 0,
    LAYOUT_SPLIT_HORIZONTAL,
    LAYOUT_SPLIT_VERTICAL,
    LAYOUT_QUAD,
    LAYOUT_THREE_TOP,
    LAYOUT_THREE_BOTTOM,
    LAYOUT_THREE_LEFT,
    LAYOUT_THREE_RIGHT,
    LAYOUT_CUSTOM
} viewport_layout;

// Viewport manager
typedef struct viewport_manager {
    // Viewports
    viewport viewports[MAX_VIEWPORTS];
    uint32_t viewport_count;
    viewport* active_viewport;
    viewport* focused_viewport;
    
    // Layout
    viewport_layout current_layout;
    f32 split_positions[3];  // For custom splits
    bool is_transitioning;
    f32 transition_progress;
    
    // Shared resources
    renderer_state* renderer;
    gizmo_system shared_gizmo_system;
    
    // Camera bookmarks
    struct {
        char name[64];
        viewport_camera camera;
    } bookmarks[8];
    uint32_t bookmark_count;
    
    // Performance
    bool enable_frustum_culling;
    bool enable_occlusion_culling;
    uint32_t max_draw_distance;
    
    // Callbacks
    void (*on_viewport_resize)(viewport_manager* manager, viewport* vp);
    void (*on_viewport_focus)(viewport_manager* manager, viewport* vp);
    void (*on_selection_changed)(viewport_manager* manager, viewport* vp);
    
    // Memory
    arena* arena;
} viewport_manager;

// =============================================================================
// VIEWPORT MANAGER API
// =============================================================================

// Initialization
viewport_manager* viewport_manager_create(arena* arena, renderer_state* renderer);
void viewport_manager_destroy(viewport_manager* manager);

// Viewport management
viewport* viewport_manager_add(viewport_manager* manager, viewport_mode mode);
void viewport_manager_remove(viewport_manager* manager, viewport* vp);
viewport* viewport_manager_get_active(viewport_manager* manager);
void viewport_manager_set_active(viewport_manager* manager, viewport* vp);

// Layout management
void viewport_manager_set_layout(viewport_manager* manager, viewport_layout layout);
void viewport_manager_split(viewport_manager* manager, viewport* vp, bool horizontal);
void viewport_manager_maximize(viewport_manager* manager, viewport* vp);
void viewport_manager_restore(viewport_manager* manager);

// Camera control
void viewport_camera_orbit(viewport_camera* camera, f32 delta_yaw, f32 delta_pitch);
void viewport_camera_pan(viewport_camera* camera, v2 delta);
void viewport_camera_zoom(viewport_camera* camera, f32 delta);
void viewport_camera_focus(viewport_camera* camera, v3 target, f32 distance);
void viewport_camera_frame_selection(viewport* vp, v3* positions, uint32_t count);
void viewport_camera_look_at(viewport_camera* camera, v3 target, v3 up);

// Camera bookmarks
void viewport_manager_save_bookmark(viewport_manager* manager, const char* name);
void viewport_manager_load_bookmark(viewport_manager* manager, uint32_t index);

// Rendering
void viewport_manager_update(viewport_manager* manager, f32 dt);
void viewport_manager_render_all(viewport_manager* manager);
void viewport_render(viewport* vp, renderer_state* renderer);
void viewport_render_overlay(viewport* vp, gui_context* gui);

// Display settings
void viewport_set_mode(viewport* vp, viewport_mode mode);
void viewport_set_shading(viewport* vp, viewport_shading shading);
void viewport_set_flags(viewport* vp, uint32_t flags);
void viewport_toggle_flag(viewport* vp, viewport_flags flag);

// Grid
void viewport_set_grid(viewport* vp, f32 spacing, uint32_t subdivisions);
void viewport_set_grid_colors(viewport* vp, v4 major, v4 minor);

// Overlays
void viewport_add_overlay(viewport* vp, const char* text, v2 position, v4 color);
void viewport_clear_overlays(viewport* vp);
void viewport_show_stats(viewport* vp, bool show);

// Selection
void viewport_set_selection(viewport* vp, uint32_t* object_ids, uint32_t count);
void viewport_highlight_objects(viewport* vp, uint32_t* object_ids, uint32_t count, v4 color);

// Input handling
bool viewport_handle_mouse(viewport* vp, v2 mouse_pos, int button, bool down);
bool viewport_handle_mouse_move(viewport* vp, v2 mouse_pos, v2 delta);
bool viewport_handle_mouse_wheel(viewport* vp, f32 delta);
bool viewport_handle_key(viewport* vp, int key, bool down);

// Picking
uint32_t viewport_pick_object(viewport* vp, v2 screen_pos);
void viewport_pick_rectangle(viewport* vp, v2 start, v2 end, uint32_t* objects, uint32_t* count);

// Screenshot
void viewport_capture_screenshot(viewport* vp, const char* filename);
void viewport_capture_thumbnail(viewport* vp, uint32_t width, uint32_t height, void* buffer);

// Performance
void viewport_set_quality(viewport* vp, uint32_t level);
void viewport_enable_culling(viewport* vp, bool frustum, bool occlusion);

#endif // HANDMADE_VIEWPORT_MANAGER_H