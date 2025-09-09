// handmade_timeline.h - Professional animation timeline and sequencer
// PERFORMANCE: 10,000 keyframes at 60fps, SIMD interpolation, GPU timeline rendering
// TARGET: Frame-accurate editing, non-destructive workflows, UE5 Sequencer quality

#ifndef HANDMADE_TIMELINE_H
#define HANDMADE_TIMELINE_H

#include <stdint.h>
#include <stdbool.h>
#include "../renderer/handmade_math.h"

#define MAX_TRACKS 256
#define MAX_KEYFRAMES_PER_TRACK 4096
#define MAX_TIMELINE_LAYERS 32
#define MAX_MARKERS 128
#define MAX_UNDO_STATES 64

// ============================================================================
// TIME REPRESENTATION
// ============================================================================

typedef struct timeline_time {
    int64_t ticks;          // High precision time in ticks
    f32 seconds;            // Floating point seconds
    uint32_t frame;         // Frame number
    uint32_t subframe;      // Subframe for interpolation
} timeline_time;

typedef struct timeline_range {
    timeline_time start;
    timeline_time end;
    timeline_time duration;
} timeline_range;

// ============================================================================
// KEYFRAME SYSTEM
// ============================================================================

typedef enum {
    KEYFRAME_LINEAR,
    KEYFRAME_STEP,
    KEYFRAME_BEZIER,
    KEYFRAME_CUBIC,
    KEYFRAME_ELASTIC,
    KEYFRAME_BOUNCE,
    KEYFRAME_CUSTOM,
} keyframe_interpolation;

typedef enum {
    TRACK_TYPE_TRANSFORM,
    TRACK_TYPE_FLOAT,
    TRACK_TYPE_VECTOR,
    TRACK_TYPE_COLOR,
    TRACK_TYPE_BOOL,
    TRACK_TYPE_EVENT,
    TRACK_TYPE_AUDIO,
    TRACK_TYPE_ANIMATION,
    TRACK_TYPE_CAMERA,
    TRACK_TYPE_CUSTOM,
} track_type;

typedef struct keyframe {
    timeline_time time;
    
    // Value storage
    union {
        f32 float_value;
        v3 vector_value;
        v4 vector4_value;
        color32 color_value;
        bool bool_value;
        transform transform_value;
        void* custom_value;
    } value;
    
    // Interpolation
    keyframe_interpolation interp_type;
    
    // Bezier handles for smooth curves
    struct {
        v2 in_tangent;
        v2 out_tangent;
        f32 in_weight;
        f32 out_weight;
    } bezier;
    
    // Metadata
    uint32_t flags;
    void* user_data;
} keyframe;

// ============================================================================
// TRACKS
// ============================================================================

typedef struct timeline_track {
    uint32_t id;
    char name[64];
    track_type type;
    
    // Hierarchy
    uint32_t parent_id;
    uint32_t* child_ids;
    uint32_t child_count;
    uint32_t depth_level;
    
    // Keyframes - Structure of Arrays for SIMD
    struct {
        timeline_time* times;
        void* values;           // Type-specific array
        keyframe_interpolation* interp_types;
        v2* in_tangents;
        v2* out_tangents;
        uint32_t count;
        uint32_t capacity;
    } keyframes;
    
    // Track properties
    bool is_enabled;
    bool is_locked;
    bool is_solo;
    bool is_muted;
    bool is_expanded;
    bool is_selected;
    
    // Visual properties
    color32 color;
    f32 height;
    f32 vertical_offset;
    
    // Binding to scene object
    void* target_object;
    const char* target_property;
    
    // Performance cache
    uint32_t last_evaluated_frame;
    void* cached_value;
} timeline_track;

// ============================================================================
// TIMELINE LAYERS
// ============================================================================

typedef struct timeline_layer {
    uint32_t id;
    char name[64];
    
    // Tracks in this layer
    uint32_t* track_ids;
    uint32_t track_count;
    
    // Layer properties
    bool is_visible;
    bool is_locked;
    f32 opacity;
    blend_mode blend;
    
    // Transform
    v2 offset;
    f32 scale;
} timeline_layer;

// ============================================================================
// MARKERS & REGIONS
// ============================================================================

typedef struct timeline_marker {
    timeline_time time;
    char label[64];
    color32 color;
    uint32_t icon_id;
    void* user_data;
} timeline_marker;

typedef struct timeline_region {
    timeline_range range;
    char name[64];
    color32 color;
    f32 alpha;
    bool is_loop_region;
} timeline_region;

// ============================================================================
// PLAYBACK STATE
// ============================================================================

typedef enum {
    PLAYBACK_STOPPED,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED,
    PLAYBACK_SCRUBBING,
} playback_state;

typedef struct playback_config {
    f32 playback_rate;
    bool loop_enabled;
    timeline_range loop_range;
    bool snap_to_frame;
    f32 frame_rate;
    bool realtime_preview;
} playback_config;

typedef struct playback_context {
    playback_state state;
    timeline_time current_time;
    timeline_time last_update_time;
    
    playback_config config;
    
    // Performance
    f64 real_time_start;
    f64 timeline_time_start;
    uint32_t frames_played;
    f32 actual_fps;
} playback_context;

// ============================================================================
// SELECTION & EDITING
// ============================================================================

typedef struct timeline_selection {
    // Selected keyframes
    struct {
        uint32_t track_id;
        uint32_t keyframe_index;
    } selected_keyframes[256];
    uint32_t keyframe_count;
    
    // Selected tracks
    uint32_t selected_tracks[64];
    uint32_t track_count;
    
    // Time selection
    timeline_range time_selection;
    bool has_time_selection;
    
    // Box selection
    rect selection_box;
    bool is_box_selecting;
} timeline_selection;

typedef struct edit_operation {
    enum {
        EDIT_MOVE_KEYFRAMES,
        EDIT_SCALE_KEYFRAMES,
        EDIT_DELETE_KEYFRAMES,
        EDIT_ADD_KEYFRAME,
        EDIT_MODIFY_VALUE,
        EDIT_MODIFY_TANGENT,
    } type;
    
    void* affected_data;
    void* old_values;
    void* new_values;
    uint32_t data_count;
} edit_operation;

// ============================================================================
// TIMELINE UI
// ============================================================================

typedef struct timeline_viewport {
    // Time range visible
    timeline_range visible_range;
    
    // Zoom and pan
    f32 time_scale;         // Pixels per second
    f32 vertical_scale;     // Track height multiplier
    v2 scroll_position;
    
    // Layout
    f32 header_height;
    f32 ruler_height;
    f32 track_list_width;
    rect timeline_rect;
    rect tracks_rect;
    
    // Grid settings
    f32 major_grid_interval;
    f32 minor_grid_interval;
    bool snap_to_grid;
} timeline_viewport;

typedef struct timeline_theme {
    // Background
    color32 background;
    color32 ruler_bg;
    color32 track_bg;
    color32 track_bg_alt;
    
    // Grid
    color32 grid_major;
    color32 grid_minor;
    color32 grid_frame;
    
    // Playhead
    color32 playhead;
    color32 playhead_handle;
    
    // Keyframes
    color32 keyframe_normal;
    color32 keyframe_selected;
    color32 keyframe_hover;
    
    // Curves
    color32 curve_line;
    color32 tangent_line;
    color32 tangent_handle;
    
    // Selection
    color32 selection_box;
    color32 time_selection;
} timeline_theme;

// ============================================================================
// TIMELINE SYSTEM
// ============================================================================

typedef struct timeline {
    char name[64];
    uint32_t id;
    
    // Tracks - Structure of Arrays
    timeline_track* tracks;
    uint32_t track_count;
    uint32_t track_capacity;
    
    // Layers
    timeline_layer layers[MAX_TIMELINE_LAYERS];
    uint32_t layer_count;
    
    // Markers and regions
    timeline_marker markers[MAX_MARKERS];
    uint32_t marker_count;
    timeline_region* regions;
    uint32_t region_count;
    
    // Time properties
    timeline_range total_range;
    f32 frame_rate;
    uint32_t ticks_per_frame;
    
    // Playback
    playback_context playback;
    
    // Selection and editing
    timeline_selection selection;
    
    // Viewport
    timeline_viewport viewport;
    
    // Visual settings
    timeline_theme theme;
    
    // Undo/Redo
    edit_operation undo_stack[MAX_UNDO_STATES];
    uint32_t undo_index;
    uint32_t undo_count;
    
    // Performance cache
    struct {
        uint32_t* sorted_track_indices;
        keyframe** visible_keyframes;
        uint32_t visible_keyframe_count;
        bool cache_dirty;
    } cache;
    
    // Stats
    struct {
        uint64_t evaluation_time_us;
        uint64_t render_time_us;
        uint32_t keyframes_evaluated;
        uint32_t tracks_processed;
    } stats;
} timeline;

// ============================================================================
// CORE API
// ============================================================================

// Timeline management
timeline* timeline_create(const char* name, f32 frame_rate);
void timeline_destroy(timeline* tl);
void timeline_clear(timeline* tl);

// Track management
timeline_track* timeline_add_track(timeline* tl, const char* name, track_type type);
void timeline_remove_track(timeline* tl, uint32_t track_id);
timeline_track* timeline_get_track(timeline* tl, uint32_t track_id);

// Keyframe operations
keyframe* timeline_add_keyframe(timeline* tl, uint32_t track_id, 
                               timeline_time time, void* value);
void timeline_remove_keyframe(timeline* tl, uint32_t track_id, uint32_t index);
void timeline_move_keyframe(timeline* tl, uint32_t track_id, uint32_t index,
                           timeline_time new_time);
keyframe* timeline_find_keyframe(timeline* tl, uint32_t track_id, 
                                timeline_time time);

// ============================================================================
// EVALUATION
// ============================================================================

// Evaluate track at time
void timeline_evaluate(timeline* tl, timeline_time time);
void timeline_evaluate_track(timeline* tl, uint32_t track_id, timeline_time time,
                            void* out_value);

// Interpolation
f32 timeline_interpolate_linear(f32 a, f32 b, f32 t);
f32 timeline_interpolate_bezier(f32 a, f32 b, v2 p1, v2 p2, f32 t);
void timeline_interpolate_value(track_type type, void* a, void* b, 
                               f32 t, void* out);

// SIMD batch evaluation
void timeline_evaluate_batch_simd(timeline* tl, timeline_time* times, 
                                 uint32_t count, void* out_values);

// ============================================================================
// PLAYBACK
// ============================================================================

// Playback control
void timeline_play(timeline* tl);
void timeline_pause(timeline* tl);
void timeline_stop(timeline* tl);
void timeline_set_time(timeline* tl, timeline_time time);
void timeline_step_forward(timeline* tl);
void timeline_step_backward(timeline* tl);

// Playback configuration
void timeline_set_playback_config(timeline* tl, const playback_config* config);
void timeline_set_loop_range(timeline* tl, timeline_range range);

// Update
void timeline_update_playback(timeline* tl, f32 dt);

// ============================================================================
// UI INTERFACE
// ============================================================================

// Rendering
void timeline_render(timeline* tl);
void timeline_render_ruler(timeline* tl);
void timeline_render_tracks(timeline* tl);
void timeline_render_keyframes(timeline* tl);
void timeline_render_curves(timeline* tl);
void timeline_render_playhead(timeline* tl);

// Input handling
bool timeline_handle_mouse(timeline* tl, v2 mouse_pos, int button, bool down);
bool timeline_handle_mouse_move(timeline* tl, v2 mouse_pos);
bool timeline_handle_mouse_wheel(timeline* tl, f32 delta);
bool timeline_handle_key(timeline* tl, int key, bool down);

// Viewport control
void timeline_set_visible_range(timeline* tl, timeline_range range);
void timeline_zoom(timeline* tl, f32 factor, timeline_time pivot);
void timeline_pan(timeline* tl, v2 delta);
void timeline_frame_selection(timeline* tl);
void timeline_frame_all(timeline* tl);

// ============================================================================
// SELECTION & EDITING
// ============================================================================

// Selection
void timeline_select_keyframe(timeline* tl, uint32_t track_id, uint32_t index,
                             bool add_to_selection);
void timeline_select_track(timeline* tl, uint32_t track_id, bool add);
void timeline_select_time_range(timeline* tl, timeline_range range);
void timeline_clear_selection(timeline* tl);

// Box selection
void timeline_begin_box_select(timeline* tl, v2 start);
void timeline_update_box_select(timeline* tl, v2 current);
void timeline_end_box_select(timeline* tl);

// Editing operations
void timeline_move_selection(timeline* tl, timeline_time delta);
void timeline_scale_selection(timeline* tl, f32 scale, timeline_time pivot);
void timeline_delete_selection(timeline* tl);
void timeline_copy_selection(timeline* tl);
void timeline_paste(timeline* tl, timeline_time time);

// Undo/Redo
void timeline_undo(timeline* tl);
void timeline_redo(timeline* tl);
void timeline_begin_edit(timeline* tl);
void timeline_end_edit(timeline* tl);

// ============================================================================
// IMPORT/EXPORT
// ============================================================================

// Serialization
bool timeline_save(timeline* tl, const char* filename);
bool timeline_load(timeline* tl, const char* filename);

// Animation import
bool timeline_import_animation(timeline* tl, const char* filename);
bool timeline_export_animation(timeline* tl, const char* filename);

// ============================================================================
// UTILITIES
// ============================================================================

// Time conversion
timeline_time timeline_seconds_to_time(timeline* tl, f32 seconds);
f32 timeline_time_to_seconds(timeline* tl, timeline_time time);
timeline_time timeline_frame_to_time(timeline* tl, uint32_t frame);
uint32_t timeline_time_to_frame(timeline* tl, timeline_time time);

// Snapping
timeline_time timeline_snap_time(timeline* tl, timeline_time time);
v2 timeline_snap_position(timeline* tl, v2 pos);

// Track hierarchy
void timeline_set_track_parent(timeline* tl, uint32_t track_id, uint32_t parent_id);
void timeline_get_track_children(timeline* tl, uint32_t track_id,
                                uint32_t* out_children, uint32_t* out_count);

#endif // HANDMADE_TIMELINE_H