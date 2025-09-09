/*
 * Handmade Particle System
 * High-performance CPU and GPU particle simulation
 * 
 * FEATURES:
 * - Pool-based allocation (zero heap allocations)
 * - SIMD-optimized physics
 * - GPU compute shader support
 * - Spatial hashing for collisions
 * - Texture atlas animation
 * 
 * PERFORMANCE TARGETS:
 * - CPU: 100K particles @ 60 FPS
 * - GPU: 1M+ particles @ 60 FPS
 */

#ifndef HANDMADE_PARTICLES_H
#define HANDMADE_PARTICLES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define PARTICLE_MAX_EMITTERS      256
#define PARTICLE_MAX_PER_EMITTER   4096
#define PARTICLE_MAX_TOTAL         (1024 * 1024)  // 1M particles
#define PARTICLE_TEXTURE_SLOTS     64
#define PARTICLE_FORCE_FIELDS      32

// ============================================================================
// TYPES
// ============================================================================

typedef uint32_t particle_id;
typedef uint32_t emitter_id;

// Basic types (if not defined elsewhere)
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
typedef struct color32 { u8 r, g, b, a; } color32;
#endif

// Particle state - Structure of Arrays for SIMD
typedef struct particle_state {
    // Position and velocity - aligned for SIMD
    f32* position_x;
    f32* position_y;
    f32* position_z;
    f32* velocity_x;
    f32* velocity_y;
    f32* velocity_z;
    
    // Visual properties
    f32* size;
    f32* rotation;
    f32* opacity;
    u32* color;
    
    // Lifetime
    f32* age;
    f32* max_age;
    
    // Physics
    f32* mass;
    f32* drag;
    
    // Metadata
    u32* texture_id;
    u32* flags;
    
    u32 count;
    u32 capacity;
} particle_state;

// Emission shape types
typedef enum emission_shape {
    EMISSION_POINT,
    EMISSION_BOX,
    EMISSION_SPHERE,
    EMISSION_CONE,
    EMISSION_MESH,
    EMISSION_RING,
    EMISSION_LINE
} emission_shape;

// Particle blend modes
typedef enum particle_blend {
    BLEND_ALPHA,
    BLEND_ADDITIVE,
    BLEND_MULTIPLY,
    BLEND_SCREEN
} particle_blend;

// Emitter configuration
typedef struct emitter_config {
    // Emission
    emission_shape shape;
    v3 position;
    v3 direction;
    f32 spread_angle;
    v3 box_min, box_max;  // For box emission
    f32 radius;           // For sphere/ring emission
    
    // Spawn rate
    f32 emission_rate;    // Particles per second
    u32 burst_count;      // For burst emission
    b32 continuous;
    
    // Initial particle properties
    f32 start_speed;
    f32 start_speed_variance;
    f32 start_size;
    f32 start_size_variance;
    f32 start_rotation;
    f32 start_rotation_variance;
    color32 start_color;
    color32 end_color;
    
    // Lifetime
    f32 particle_lifetime;
    f32 lifetime_variance;
    f32 emitter_lifetime;  // -1 for infinite
    
    // Physics
    v3 gravity;
    f32 drag_coefficient;
    b32 world_space;       // vs local space
    b32 enable_collision;
    
    // Visual
    u32 texture_id;
    particle_blend blend_mode;
    b32 animated_texture;
    u32 animation_frames;
    f32 animation_speed;
} emitter_config;

// Particle emitter
typedef struct particle_emitter {
    emitter_id id;
    emitter_config config;
    
    // Runtime state
    v3 world_position;
    f32 time_alive;
    f32 emission_accumulator;
    b32 is_active;
    b32 is_paused;
    
    // Particle pool indices
    u32 particle_start;
    u32 particle_count;
    u32 particle_capacity;
} particle_emitter;

// Force field for particle interaction
typedef struct force_field {
    v3 position;
    f32 radius;
    f32 strength;
    
    enum {
        FORCE_ATTRACT,
        FORCE_REPEL,
        FORCE_VORTEX,
        FORCE_TURBULENCE
    } type;
    
    b32 is_active;
} force_field;

// Particle system context
typedef struct particle_system {
    // Memory arena
    void* memory;
    u64 memory_size;
    u64 memory_used;
    
    // Particle data (SoA)
    particle_state particles;
    
    // Emitters
    particle_emitter* emitters;
    u32 emitter_count;
    u32 emitter_capacity;
    
    // Force fields
    force_field force_fields[PARTICLE_FORCE_FIELDS];
    u32 force_field_count;
    
    // Spatial hash for collisions
    struct {
        u32* cell_starts;
        u32* cell_ends;
        u32* particle_indices;
        f32 cell_size;
        u32 grid_size;
    } spatial_hash;
    
    // GPU resources (optional)
    struct {
        void* compute_shader;
        void* particle_buffer;
        void* constant_buffer;
        b32 enabled;
    } gpu;
    
    // Statistics
    struct {
        u32 particles_spawned;
        u32 particles_killed;
        f32 update_time_ms;
        f32 render_time_ms;
    } stats;
    
    // Configuration
    b32 use_simd;
    b32 use_gpu;
    b32 enable_collisions;
    f32 fixed_timestep;
} particle_system;

// ============================================================================
// PARTICLE SYSTEM API
// ============================================================================

// System lifecycle
particle_system* particles_init(void* memory, u64 memory_size);
void particles_shutdown(particle_system* system);
void particles_reset(particle_system* system);

// Emitter management
emitter_id particles_create_emitter(particle_system* system, const emitter_config* config);
void particles_destroy_emitter(particle_system* system, emitter_id id);
void particles_update_emitter(particle_system* system, emitter_id id, const emitter_config* config);
particle_emitter* particles_get_emitter(particle_system* system, emitter_id id);

// Emitter control
void particles_play_emitter(particle_system* system, emitter_id id);
void particles_stop_emitter(particle_system* system, emitter_id id);
void particles_pause_emitter(particle_system* system, emitter_id id);
void particles_burst_emitter(particle_system* system, emitter_id id, u32 count);

// Force fields
u32 particles_add_force_field(particle_system* system, const force_field* field);
void particles_remove_force_field(particle_system* system, u32 index);
void particles_update_force_field(particle_system* system, u32 index, const force_field* field);

// System update
void particles_update(particle_system* system, f32 delta_time);
void particles_update_simd(particle_system* system, f32 delta_time);
void particles_update_gpu(particle_system* system, f32 delta_time);

// Rendering
typedef struct particle_render_data {
    f32* positions;      // x,y,z interleaved
    f32* sizes;
    u32* colors;
    f32* rotations;
    u32* texture_ids;
    u32 count;
} particle_render_data;

particle_render_data particles_get_render_data(particle_system* system);
void particles_sort_for_rendering(particle_system* system, v3 camera_position);

// Collision
void particles_enable_collisions(particle_system* system, b32 enable);
void particles_set_collision_callback(particle_system* system, 
    void (*callback)(particle_id, v3 position, v3 normal));

// ============================================================================
// PRESET EFFECTS
// ============================================================================

// Common particle effects
emitter_config particles_preset_fire(v3 position);
emitter_config particles_preset_smoke(v3 position);
emitter_config particles_preset_explosion(v3 position, f32 radius);
emitter_config particles_preset_sparks(v3 position, v3 direction);
emitter_config particles_preset_rain(v3 area_min, v3 area_max);
emitter_config particles_preset_snow(v3 area_min, v3 area_max);
emitter_config particles_preset_magic(v3 position, color32 color);
emitter_config particles_preset_blood(v3 position, v3 direction);
emitter_config particles_preset_dust(v3 position, f32 radius);
emitter_config particles_preset_water_splash(v3 position);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Performance tuning
void particles_set_max_particles(particle_system* system, u32 max);
void particles_set_update_frequency(particle_system* system, f32 hz);
void particles_enable_lod(particle_system* system, b32 enable, f32 distance);

// Debug visualization
void particles_debug_draw_emitters(particle_system* system);
void particles_debug_draw_forces(particle_system* system);
void particles_debug_print_stats(particle_system* system);

#endif // HANDMADE_PARTICLES_H