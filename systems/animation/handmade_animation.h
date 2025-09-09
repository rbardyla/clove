/*
 * Handmade Animation System
 * Skeletal animation with blending and inverse kinematics
 * 
 * FEATURES:
 * - Bone hierarchies with local/world transforms
 * - Animation clip blending (linear, additive, layered)
 * - Inverse Kinematics (CCD, FABRIK, analytical)
 * - Animation state machines
 * - Procedural animation helpers
 * - Root motion extraction
 * 
 * PERFORMANCE:
 * - Fixed memory allocation
 * - SIMD-optimized matrix operations
 * - Cache-friendly bone layout
 * - Zero heap allocations
 */

#ifndef HANDMADE_ANIMATION_H
#define HANDMADE_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define ANIM_MAX_BONES          256
#define ANIM_MAX_CLIPS          128
#define ANIM_MAX_BLEND_LAYERS   8
#define ANIM_MAX_IK_CHAINS      16
#define ANIM_MAX_CONSTRAINTS    32
#define ANIM_MAX_EVENTS         64

// ============================================================================
// TYPES
// ============================================================================

// Basic types
#ifndef HANDMADE_TYPES_DEFINED
typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int32_t b32;

typedef struct v2 { f32 x, y; } v2;
typedef struct v3 { f32 x, y, z; } v3;
typedef struct v4 { f32 x, y, z, w; } v4;
typedef struct quat { f32 x, y, z, w; } quat;

typedef struct mat4 {
    f32 m[4][4];
} mat4;

typedef struct transform {
    v3 position;
    quat rotation;
    v3 scale;
} transform;
#endif

// Bone structure
typedef struct anim_bone {
    char name[32];
    i32 parent_index;  // -1 for root
    i32 child_count;
    i32 children[8];   // Indices of children
    
    // Transforms
    transform local;      // Local space transform
    transform world;      // World space transform
    mat4 bind_pose;      // Inverse bind pose matrix
    mat4 skinning_matrix; // Final skinning matrix
    
    // Constraints
    b32 has_constraints;
    f32 rotation_limits[6]; // Min/max for XYZ euler
    f32 scale_limits[2];    // Min/max scale
} anim_bone;

// Animation keyframe
typedef struct anim_keyframe {
    f32 time;
    transform trans;
    u32 flags;  // Interpolation type, etc
} anim_keyframe;

// Animation channel (per bone)
typedef struct anim_channel {
    u32 bone_index;
    u32 keyframe_count;
    anim_keyframe* keyframes;
} anim_channel;

// Animation clip
typedef struct anim_clip {
    char name[64];
    f32 duration;
    f32 sample_rate;
    u32 channel_count;
    anim_channel* channels;
    b32 looping;
    b32 root_motion;
} anim_clip;

// Animation state for playback
typedef struct anim_state {
    u32 clip_index;
    f32 time;
    f32 speed;
    f32 weight;
    b32 playing;
    b32 finished;
} anim_state;

// Blend modes
typedef enum blend_mode {
    BLEND_LINEAR,      // Lerp between poses
    BLEND_ADDITIVE,    // Add delta to base
    BLEND_OVERRIDE,    // Full replacement
    BLEND_LAYERED      // Mask-based layering
} blend_mode;

// IK solver types
typedef enum ik_solver {
    IK_CCD,           // Cyclic Coordinate Descent
    IK_FABRIK,        // Forward And Backward Reaching
    IK_ANALYTICAL_2D, // 2-bone analytical
    IK_ANALYTICAL_3D  // 3-bone analytical
} ik_solver;

// IK chain configuration
typedef struct ik_chain {
    char name[32];
    u32 tip_bone;        // End effector
    u32 root_bone;       // Start of chain
    u32 chain_length;    // Number of bones
    
    v3 target_position;
    quat target_rotation;
    
    ik_solver solver;
    u32 max_iterations;
    f32 tolerance;
    f32 weight;
    
    b32 enabled;
} ik_chain;

// Animation layer for blending
typedef struct anim_layer {
    anim_state state;
    blend_mode mode;
    f32 weight;
    u32 bone_mask[8];  // Bit mask for affected bones
    b32 enabled;
} anim_layer;

// State machine node
typedef struct anim_node {
    char name[32];
    u32 clip_index;
    f32 speed;
    b32 looping;
} anim_node;

// State machine transition
typedef struct anim_transition {
    u32 from_node;
    u32 to_node;
    f32 duration;
    f32 exit_time;  // Normalized time to start transition
    
    enum {
        TRANS_IMMEDIATE,
        TRANS_CROSSFADE,
        TRANS_FROZEN  // Freeze and blend
    } type;
    
    // Conditions
    b32 (*condition)(void* user_data);
    void* user_data;
} anim_transition;

// Animation state machine
typedef struct anim_state_machine {
    anim_node* nodes;
    u32 node_count;
    
    anim_transition* transitions;
    u32 transition_count;
    
    u32 current_node;
    u32 next_node;
    f32 transition_time;
    b32 transitioning;
} anim_state_machine;

// Complete skeleton
typedef struct anim_skeleton {
    anim_bone* bones;
    u32 bone_count;
    u32 root_bone;
    
    // Cached transforms
    mat4* world_matrices;
    mat4* skinning_matrices;
    
    // IK chains
    ik_chain* ik_chains;
    u32 ik_chain_count;
} anim_skeleton;

// Animation system context
typedef struct animation_system {
    // Memory arena
    void* memory;
    u64 memory_size;
    u64 memory_used;
    
    // Skeletons
    anim_skeleton* skeletons;
    u32 skeleton_count;
    u32 skeleton_capacity;
    
    // Animation clips
    anim_clip* clips;
    u32 clip_count;
    u32 clip_capacity;
    
    // Blend layers
    anim_layer layers[ANIM_MAX_BLEND_LAYERS];
    
    // State machine
    anim_state_machine* state_machines;
    u32 state_machine_count;
    
    // Temporary buffers for blending
    transform* blend_buffer;
    f32* weight_buffer;
    
    // Configuration
    b32 use_simd;
    b32 enable_root_motion;
    f32 fixed_timestep;
} animation_system;

// ============================================================================
// ANIMATION SYSTEM API
// ============================================================================

// System lifecycle
animation_system* animation_init(void* memory, u64 memory_size);
void animation_shutdown(animation_system* system);
void animation_reset(animation_system* system);

// Skeleton management
u32 animation_create_skeleton(animation_system* system, u32 bone_count);
anim_bone* animation_add_bone(animation_system* system, u32 skeleton_id, 
                              const char* name, i32 parent);
void animation_build_skeleton(animation_system* system, u32 skeleton_id);
void animation_set_bind_pose(animation_system* system, u32 skeleton_id);

// Animation clip management
u32 animation_load_clip(animation_system* system, const char* file_path);
u32 animation_create_clip(animation_system* system, const char* name, 
                          f32 duration, u32 channel_count);
void animation_add_keyframe(animation_system* system, u32 clip_id, 
                            u32 bone_id, f32 time, transform trans);

// Playback control
void animation_play(animation_system* system, u32 layer, u32 clip_id);
void animation_stop(animation_system* system, u32 layer);
void animation_pause(animation_system* system, u32 layer);
void animation_set_time(animation_system* system, u32 layer, f32 time);
void animation_set_speed(animation_system* system, u32 layer, f32 speed);

// Blending
void animation_set_blend_mode(animation_system* system, u32 layer, blend_mode mode);
void animation_set_layer_weight(animation_system* system, u32 layer, f32 weight);
void animation_set_bone_mask(animation_system* system, u32 layer, u32* bone_indices, u32 count);
void animation_crossfade(animation_system* system, u32 from_layer, u32 to_layer, f32 duration);

// IK setup
u32 animation_add_ik_chain(animation_system* system, u32 skeleton_id, 
                           u32 tip_bone, u32 root_bone);
void animation_set_ik_target(animation_system* system, u32 skeleton_id, 
                             u32 chain_id, v3 position, quat* rotation);
void animation_set_ik_solver(animation_system* system, u32 skeleton_id, 
                             u32 chain_id, ik_solver solver);
void animation_enable_ik(animation_system* system, u32 skeleton_id, 
                         u32 chain_id, b32 enable);

// State machine
u32 animation_create_state_machine(animation_system* system);
u32 animation_add_state(animation_system* system, u32 machine_id, 
                        const char* name, u32 clip_id);
void animation_add_transition(animation_system* system, u32 machine_id,
                              u32 from_state, u32 to_state, f32 duration);
void animation_trigger_transition(animation_system* system, u32 machine_id,
                                  const char* target_state);

// Update
void animation_update(animation_system* system, u32 skeleton_id, f32 delta_time);
void animation_update_ik(animation_system* system, u32 skeleton_id);
void animation_update_state_machine(animation_system* system, u32 machine_id, f32 delta_time);

// Pose evaluation
void animation_evaluate_clip(animation_system* system, u32 clip_id, 
                             f32 time, transform* out_pose);
void animation_blend_poses(transform* pose_a, transform* pose_b, 
                          f32 weight, u32 bone_count, transform* out_pose);
void animation_apply_additive(transform* base_pose, transform* additive_pose,
                              f32 weight, u32 bone_count, transform* out_pose);

// Transform utilities
void animation_calculate_world_transforms(animation_system* system, u32 skeleton_id);
void animation_calculate_skinning_matrices(animation_system* system, u32 skeleton_id);
mat4* animation_get_skinning_matrices(animation_system* system, u32 skeleton_id);

// Root motion
v3 animation_extract_root_motion(animation_system* system, u32 skeleton_id);
void animation_apply_root_motion(animation_system* system, u32 skeleton_id, v3 motion);

// Procedural animation helpers
void animation_look_at(animation_system* system, u32 skeleton_id, 
                       u32 bone_id, v3 target, f32 weight);
void animation_apply_physics(animation_system* system, u32 skeleton_id,
                             u32 bone_id, v3 force, f32 delta_time);
void animation_apply_spring(animation_system* system, u32 skeleton_id,
                            u32 bone_id, f32 stiffness, f32 damping);

// Debug
void animation_debug_draw_skeleton(animation_system* system, u32 skeleton_id);
void animation_debug_draw_ik_targets(animation_system* system, u32 skeleton_id);
void animation_debug_print_hierarchy(animation_system* system, u32 skeleton_id);

#endif // HANDMADE_ANIMATION_H