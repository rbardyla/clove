// handmade_gizmos.h - Professional viewport manipulation gizmos
// PERFORMANCE: GPU-accelerated picking, <0.2ms render for 10 active gizmos
// TARGET: Sub-pixel precision, smooth interaction, UE5-quality visuals

#ifndef HANDMADE_GIZMOS_H
#define HANDMADE_GIZMOS_H

#include <stdint.h>
#include <stdbool.h>
#include "../renderer/handmade_math.h"

#define MAX_ACTIVE_GIZMOS 32
#define GIZMO_PICK_BUFFER_SIZE 256

// ============================================================================
// GIZMO TYPES
// ============================================================================

typedef enum {
    GIZMO_TYPE_NONE = 0,
    GIZMO_TYPE_TRANSLATION,
    GIZMO_TYPE_ROTATION,
    GIZMO_TYPE_SCALE,
    GIZMO_TYPE_UNIVERSAL,  // All three combined
    GIZMO_TYPE_BOUNDS,      // Bounding box
    GIZMO_TYPE_LIGHT,       // Light-specific
    GIZMO_TYPE_CAMERA,      // Camera frustum
    GIZMO_TYPE_CUSTOM,
} gizmo_type;

typedef enum {
    GIZMO_MODE_LOCAL,
    GIZMO_MODE_WORLD,
    GIZMO_MODE_VIEW,
} gizmo_mode;

typedef enum {
    GIZMO_AXIS_NONE = 0,
    GIZMO_AXIS_X = (1 << 0),
    GIZMO_AXIS_Y = (1 << 1),
    GIZMO_AXIS_Z = (1 << 2),
    GIZMO_AXIS_XY = GIZMO_AXIS_X | GIZMO_AXIS_Y,
    GIZMO_AXIS_XZ = GIZMO_AXIS_X | GIZMO_AXIS_Z,
    GIZMO_AXIS_YZ = GIZMO_AXIS_Y | GIZMO_AXIS_Z,
    GIZMO_AXIS_XYZ = GIZMO_AXIS_X | GIZMO_AXIS_Y | GIZMO_AXIS_Z,
    GIZMO_AXIS_SCREEN = (1 << 3),
} gizmo_axis;

typedef enum {
    GIZMO_OP_NONE,
    GIZMO_OP_TRANSLATE,
    GIZMO_OP_ROTATE,
    GIZMO_OP_SCALE,
} gizmo_operation;

// ============================================================================
// GIZMO STATE
// ============================================================================

typedef struct gizmo_config {
    // Visual settings
    f32 scale;                  // Overall gizmo scale
    f32 line_thickness;         // Line width
    f32 handle_size;            // Size of grab handles
    f32 rotation_ring_radius;   // Rotation gizmo radius
    f32 screen_space_size;      // Size in screen space
    
    // Colors
    color32 axis_colors[3];     // X, Y, Z colors
    color32 selected_color;     // When axis is selected
    color32 hover_color;        // When hovering
    color32 disabled_color;     // When disabled
    color32 plane_color;        // Plane handles
    f32 plane_alpha;            // Transparency for planes
    
    // Behavior
    bool enable_snapping;
    f32 translation_snap;       // Grid snap for translation
    f32 rotation_snap;          // Angle snap for rotation (degrees)
    f32 scale_snap;             // Scale snap increment
    
    // Interaction
    f32 pick_tolerance;         // Pixel tolerance for picking
    bool show_measurements;     // Show distance/angle while dragging
    bool auto_scale_with_zoom;  // Scale gizmo with camera zoom
} gizmo_config;

typedef struct gizmo_transform {
    v3 position;
    quat rotation;
    v3 scale;
    mat4 matrix;
    mat4 inverse_matrix;
} gizmo_transform;

typedef struct gizmo_interaction {
    bool is_active;
    bool is_hovering;
    
    gizmo_axis active_axis;
    gizmo_operation operation;
    
    // Drag state
    v3 drag_start_world;
    v3 drag_current_world;
    v3 drag_delta;
    
    // Original transform when drag started
    gizmo_transform start_transform;
    
    // Screen space positions
    v2 mouse_start;
    v2 mouse_current;
    
    // Constraint
    v3 constraint_axis;
    plane constraint_plane;
} gizmo_interaction;

typedef struct gizmo_instance {
    uint32_t id;
    gizmo_type type;
    gizmo_mode mode;
    
    // Transform
    gizmo_transform transform;
    gizmo_transform* target_transform;  // Transform being manipulated
    
    // State
    bool is_visible;
    bool is_enabled;
    gizmo_axis enabled_axes;
    
    // Interaction
    gizmo_interaction interaction;
    
    // Visual overrides
    f32 scale_override;
    color32* color_overrides;
} gizmo_instance;

// ============================================================================
// GIZMO GEOMETRY CACHE
// ============================================================================

typedef struct gizmo_mesh {
    // Vertex data
    v3* positions;
    v3* normals;
    color32* colors;
    uint32_t vertex_count;
    
    // Index data
    uint16_t* indices;
    uint32_t index_count;
    
    // GPU buffers
    uint32_t vbo;
    uint32_t ibo;
    uint32_t vao;
} gizmo_mesh;

typedef struct gizmo_geometry_cache {
    // Pre-built meshes for each gizmo type
    gizmo_mesh translation_arrows[3];   // X, Y, Z arrows
    gizmo_mesh translation_planes[3];   // XY, XZ, YZ planes
    gizmo_mesh translation_center;      // Center sphere
    
    gizmo_mesh rotation_rings[3];       // X, Y, Z rotation rings
    gizmo_mesh rotation_sphere;         // Free rotation sphere
    
    gizmo_mesh scale_handles[3];        // X, Y, Z scale handles
    gizmo_mesh scale_planes[3];         // XY, XZ, YZ scale planes
    gizmo_mesh scale_center;            // Uniform scale center
    
    gizmo_mesh bounds_box;              // Bounding box lines
    gizmo_mesh light_cone;              // Light visualization
    gizmo_mesh camera_frustum;          // Camera frustum
    
    bool is_initialized;
} gizmo_geometry_cache;

// ============================================================================
// GIZMO RENDERER
// ============================================================================

typedef struct gizmo_render_state {
    // Current camera
    mat4 view_matrix;
    mat4 projection_matrix;
    mat4 view_projection;
    v3 camera_position;
    v3 camera_forward;
    
    // Viewport
    int viewport_x, viewport_y;
    int viewport_width, viewport_height;
    
    // GPU picking
    uint32_t pick_fbo;
    uint32_t pick_texture;
    uint32_t pick_depth;
    
    // Shaders
    uint32_t gizmo_shader;
    uint32_t pick_shader;
    
    // Current render stats
    uint32_t draw_calls;
    uint32_t vertices_rendered;
} gizmo_render_state;

// ============================================================================
// GIZMO SYSTEM
// ============================================================================

typedef struct gizmo_system {
    // Configuration
    gizmo_config config;
    
    // Active gizmos
    gizmo_instance gizmos[MAX_ACTIVE_GIZMOS];
    uint32_t gizmo_count;
    uint32_t active_gizmo_id;
    
    // Geometry cache
    gizmo_geometry_cache geometry;
    
    // Rendering
    gizmo_render_state render;
    
    // Interaction
    struct {
        uint32_t hot_gizmo_id;
        gizmo_axis hot_axis;
        bool is_dragging;
        v2 mouse_position;
    } interaction;
    
    // Performance
    struct {
        uint64_t pick_time_us;
        uint64_t render_time_us;
        uint32_t gizmos_rendered;
    } stats;
} gizmo_system;

// ============================================================================
// CORE API
// ============================================================================

// System management
void gizmo_system_init(gizmo_system* system);
void gizmo_system_shutdown(gizmo_system* system);
void gizmo_system_reset(gizmo_system* system);

// Configuration
void gizmo_set_config(gizmo_system* system, const gizmo_config* config);
gizmo_config gizmo_get_default_config(void);

// Gizmo management
uint32_t gizmo_create(gizmo_system* system, gizmo_type type);
void gizmo_destroy(gizmo_system* system, uint32_t gizmo_id);
gizmo_instance* gizmo_get(gizmo_system* system, uint32_t gizmo_id);

// Transform binding
void gizmo_bind_transform(gizmo_system* system, uint32_t gizmo_id, 
                         gizmo_transform* transform);
void gizmo_set_transform(gizmo_system* system, uint32_t gizmo_id,
                        const gizmo_transform* transform);

// Properties
void gizmo_set_mode(gizmo_system* system, uint32_t gizmo_id, gizmo_mode mode);
void gizmo_set_enabled_axes(gizmo_system* system, uint32_t gizmo_id, 
                           gizmo_axis axes);
void gizmo_set_visible(gizmo_system* system, uint32_t gizmo_id, bool visible);

// ============================================================================
// RENDERING
// ============================================================================

// Camera setup
void gizmo_set_camera(gizmo_system* system, const mat4* view, const mat4* proj,
                     const v3* camera_pos);
void gizmo_set_viewport(gizmo_system* system, int x, int y, int width, int height);

// Rendering
void gizmo_render_all(gizmo_system* system);
void gizmo_render(gizmo_system* system, uint32_t gizmo_id);

// Debug rendering
void gizmo_render_debug_overlay(gizmo_system* system);

// ============================================================================
// INTERACTION
// ============================================================================

// Input handling
bool gizmo_mouse_button(gizmo_system* system, int button, bool down, v2 mouse_pos);
bool gizmo_mouse_move(gizmo_system* system, v2 mouse_pos);
bool gizmo_mouse_wheel(gizmo_system* system, f32 delta);

// Picking
uint32_t gizmo_pick(gizmo_system* system, v2 screen_pos, gizmo_axis* out_axis);
bool gizmo_is_over_any(gizmo_system* system, v2 screen_pos);

// Manipulation
bool gizmo_begin_drag(gizmo_system* system, uint32_t gizmo_id, 
                     gizmo_axis axis, v2 mouse_pos);
void gizmo_update_drag(gizmo_system* system, v2 mouse_pos);
void gizmo_end_drag(gizmo_system* system);
bool gizmo_is_dragging(gizmo_system* system);

// ============================================================================
// UTILITIES
// ============================================================================

// Coordinate conversion
v3 gizmo_screen_to_world(gizmo_system* system, v2 screen_pos, f32 depth);
v2 gizmo_world_to_screen(gizmo_system* system, v3 world_pos);
ray gizmo_screen_to_ray(gizmo_system* system, v2 screen_pos);

// Transform helpers
gizmo_transform gizmo_transform_identity(void);
gizmo_transform gizmo_transform_from_matrix(const mat4* matrix);
void gizmo_transform_to_matrix(const gizmo_transform* transform, mat4* out_matrix);
gizmo_transform gizmo_transform_multiply(const gizmo_transform* a, 
                                        const gizmo_transform* b);

// Snapping
v3 gizmo_snap_translation(v3 position, f32 snap_size);
quat gizmo_snap_rotation(quat rotation, f32 snap_angle);
v3 gizmo_snap_scale(v3 scale, f32 snap_size);

// ============================================================================
// ADVANCED FEATURES
// ============================================================================

// Multi-selection
void gizmo_enable_multi_select(gizmo_system* system, uint32_t* gizmo_ids, 
                              uint32_t count);
v3 gizmo_get_selection_center(gizmo_system* system);

// Constraints
void gizmo_set_constraint_plane(gizmo_system* system, const plane* p);
void gizmo_set_constraint_axis(gizmo_system* system, const v3* axis);
void gizmo_clear_constraints(gizmo_system* system);

// Custom gizmos
uint32_t gizmo_create_custom(gizmo_system* system, const gizmo_mesh* mesh);
void gizmo_set_custom_colors(gizmo_system* system, uint32_t gizmo_id,
                            const color32* colors, uint32_t count);

// Measurements
f32 gizmo_get_drag_distance(gizmo_system* system);
f32 gizmo_get_drag_angle(gizmo_system* system);
v3 gizmo_get_drag_delta(gizmo_system* system);

#endif // HANDMADE_GIZMOS_H