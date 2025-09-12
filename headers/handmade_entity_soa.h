#ifndef HANDMADE_ENTITY_SOA_H
#define HANDMADE_ENTITY_SOA_H

#include "handmade_memory.h"
#include <immintrin.h>  // For SIMD
#include <math.h>
#include <stdio.h>

// SIMD-aligned vector types
typedef struct v3 {
    union {
        struct { f32 x, y, z; };
        f32 e[3];
    };
} v3;

typedef struct v4 {
    union {
        struct { f32 x, y, z, w; };
        f32 e[4];
        __m128 simd;
    };
} v4;

typedef struct quat {
    union {
        struct { f32 x, y, z, w; };
        f32 e[4];
        __m128 simd;
    };
} quat;

typedef struct mat4 {
    union {
        f32 e[16];
        f32 m[4][4];
        __m128 rows[4];
    };
} mat4;

// Entity handle for safe referencing
typedef struct entity_handle {
    u32 index;
    u32 generation;
} entity_handle;

#define INVALID_ENTITY_HANDLE ((entity_handle){0xFFFFFFFF, 0xFFFFFFFF})
#define MAX_ENTITIES 65536
#define CACHE_LINE_SIZE 64

// Component flags for fast queries
typedef enum component_flags {
    COMPONENT_TRANSFORM  = (1 << 0),
    COMPONENT_MESH       = (1 << 1),
    COMPONENT_PHYSICS    = (1 << 2),
    COMPONENT_COLLIDER   = (1 << 3),
    COMPONENT_RENDER     = (1 << 4),
    COMPONENT_LIGHT      = (1 << 5),
    COMPONENT_CAMERA     = (1 << 6),
    COMPONENT_SCRIPT     = (1 << 7),
    COMPONENT_AUDIO      = (1 << 8),
    COMPONENT_PARTICLE   = (1 << 9),
    COMPONENT_AI         = (1 << 10),
} component_flags;

// Structure of Arrays for cache-friendly iteration
typedef struct transform_soa {
    // Positions packed for SIMD
    f32* positions_x;     // All X coordinates contiguous
    f32* positions_y;     // All Y coordinates contiguous
    f32* positions_z;     // All Z coordinates contiguous
    f32* positions_w;     // Padding for SIMD alignment
    
    // Rotations as quaternions
    f32* rotations_x;
    f32* rotations_y;
    f32* rotations_z;
    f32* rotations_w;
    
    // Scales
    f32* scales_x;
    f32* scales_y;
    f32* scales_z;
    f32* scales_w;       // Uniform scale or padding
    
    // World matrices (computed)
    mat4* world_matrices;
    
    // Dirty flags for incremental updates
    u32* dirty_flags;
} transform_soa;

typedef struct physics_soa {
    // Velocities
    f32* velocities_x;
    f32* velocities_y;
    f32* velocities_z;
    
    // Accelerations
    f32* accelerations_x;
    f32* accelerations_y;
    f32* accelerations_z;
    
    // Physical properties
    f32* masses;
    f32* drag_coefficients;
    f32* restitutions;
    
    // Forces accumulator
    f32* forces_x;
    f32* forces_y;
    f32* forces_z;
} physics_soa;

typedef struct render_soa {
    u32* mesh_ids;
    u32* material_ids;
    u32* shader_ids;
    f32* lod_distances;
    u8* visibility_flags;
    u32* render_layers;
} render_soa;

// Entity storage with generation for safe handles
typedef struct entity_storage {
    // Entity metadata
    u32* generations;      // Generation counters for each slot
    u32* component_masks;  // Bitmask of components
    u32* entity_versions;  // Version for change detection
    
    // Free list management
    u32* free_indices;
    u32 free_count;
    u32 entity_count;
    u32 max_entities;
    
    // Component arrays (SoA)
    transform_soa transforms;
    physics_soa physics;
    render_soa render;
    
    // Spatial acceleration
    u32* spatial_indices;  // Indices into spatial structure
    
    // Name lookup (optional)
    u64* name_hashes;
    char** names;
} entity_storage;

// Archetype for efficient queries
typedef struct archetype {
    u32 component_mask;
    u32* entity_indices;
    u32 entity_count;
    u32 capacity;
} archetype;

// Entity query result
typedef struct entity_query {
    u32* indices;
    u32 count;
    u32 component_mask;
} entity_query;

// Initialize entity storage
static entity_storage* entity_storage_init(arena* a, u32 max_entities) {
    entity_storage* storage = arena_alloc(a, sizeof(entity_storage));
    storage->max_entities = max_entities;
    
    // Allocate metadata arrays
    storage->generations = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    storage->component_masks = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    storage->entity_versions = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    storage->free_indices = arena_alloc_array(a, u32, max_entities);
    
    // Initialize free list
    storage->free_count = max_entities;
    for (u32 i = 0; i < max_entities; i++) {
        storage->free_indices[i] = max_entities - 1 - i;
    }
    
    // Allocate transform SoA
    storage->transforms.positions_x = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.positions_y = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.positions_z = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.positions_w = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    storage->transforms.rotations_x = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.rotations_y = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.rotations_z = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.rotations_w = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    storage->transforms.scales_x = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.scales_y = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.scales_z = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->transforms.scales_w = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    storage->transforms.world_matrices = arena_alloc_array_aligned(a, mat4, max_entities, CACHE_LINE_SIZE);
    storage->transforms.dirty_flags = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    
    // Allocate physics SoA
    storage->physics.velocities_x = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.velocities_y = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.velocities_z = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    storage->physics.accelerations_x = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.accelerations_y = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.accelerations_z = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    storage->physics.masses = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.drag_coefficients = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.restitutions = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    storage->physics.forces_x = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.forces_y = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->physics.forces_z = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    
    // Allocate render SoA
    storage->render.mesh_ids = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    storage->render.material_ids = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    storage->render.shader_ids = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    storage->render.lod_distances = arena_alloc_array_aligned(a, f32, max_entities, CACHE_LINE_SIZE);
    storage->render.visibility_flags = arena_alloc_array_aligned(a, u8, max_entities, CACHE_LINE_SIZE);
    storage->render.render_layers = arena_alloc_array_aligned(a, u32, max_entities, CACHE_LINE_SIZE);
    
    // Initialize default values
    for (u32 i = 0; i < max_entities; i++) {
        storage->transforms.scales_x[i] = 1.0f;
        storage->transforms.scales_y[i] = 1.0f;
        storage->transforms.scales_z[i] = 1.0f;
        storage->transforms.scales_w[i] = 1.0f;
        
        storage->transforms.rotations_w[i] = 1.0f; // Identity quaternion
        
        storage->physics.masses[i] = 1.0f;
        storage->physics.drag_coefficients[i] = 0.1f;
        storage->physics.restitutions[i] = 0.5f;
    }
    
    return storage;
}

// Create entity
static entity_handle entity_create(entity_storage* storage) {
    if (storage->free_count == 0) {
        return INVALID_ENTITY_HANDLE;
    }
    
    u32 index = storage->free_indices[--storage->free_count];
    storage->generations[index]++;
    storage->entity_count++;
    storage->entity_versions[index]++;
    
    return (entity_handle){index, storage->generations[index]};
}

// Destroy entity
static void entity_destroy(entity_storage* storage, entity_handle handle) {
    if (handle.index >= storage->max_entities ||
        storage->generations[handle.index] != handle.generation) {
        return; // Invalid handle
    }
    
    storage->component_masks[handle.index] = 0;
    storage->free_indices[storage->free_count++] = handle.index;
    storage->entity_count--;
    storage->entity_versions[handle.index]++;
}

// Validate handle
static inline int entity_valid(entity_storage* storage, entity_handle handle) {
    return handle.index < storage->max_entities &&
           storage->generations[handle.index] == handle.generation;
}

// Add component
static inline void entity_add_component(entity_storage* storage, entity_handle handle, u32 component_flag) {
    if (!entity_valid(storage, handle)) return;
    storage->component_masks[handle.index] |= component_flag;
    storage->entity_versions[handle.index]++;
}

// Remove component
static inline void entity_remove_component(entity_storage* storage, entity_handle handle, u32 component_flag) {
    if (!entity_valid(storage, handle)) return;
    storage->component_masks[handle.index] &= ~component_flag;
    storage->entity_versions[handle.index]++;
}

// Has component
static inline int entity_has_component(entity_storage* storage, entity_handle handle, u32 component_flag) {
    if (!entity_valid(storage, handle)) return 0;
    return (storage->component_masks[handle.index] & component_flag) == component_flag;
}

// SIMD batch transform update
static void transform_update_batch_simd(transform_soa* transforms, u32* indices, u32 count) {
    // Process 4 entities at a time using SIMD
    u32 simd_count = count & ~3; // Round down to multiple of 4
    
    for (u32 i = 0; i < simd_count; i += 4) {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];
        u32 i3 = indices[i + 3];
        
        // Load positions
        __m128 px = _mm_set_ps(transforms->positions_x[i3], transforms->positions_x[i2],
                               transforms->positions_x[i1], transforms->positions_x[i0]);
        __m128 py = _mm_set_ps(transforms->positions_y[i3], transforms->positions_y[i2],
                               transforms->positions_y[i1], transforms->positions_y[i0]);
        __m128 pz = _mm_set_ps(transforms->positions_z[i3], transforms->positions_z[i2],
                               transforms->positions_z[i1], transforms->positions_z[i0]);
        
        // Load scales
        __m128 sx = _mm_set_ps(transforms->scales_x[i3], transforms->scales_x[i2],
                               transforms->scales_x[i1], transforms->scales_x[i0]);
        __m128 sy = _mm_set_ps(transforms->scales_y[i3], transforms->scales_y[i2],
                               transforms->scales_y[i1], transforms->scales_y[i0]);
        __m128 sz = _mm_set_ps(transforms->scales_z[i3], transforms->scales_z[i2],
                               transforms->scales_z[i1], transforms->scales_z[i0]);
        
        // Build world matrices (simplified for demonstration)
        // In production, include full quaternion rotation
        for (u32 j = 0; j < 4; j++) {
            u32 idx = indices[i + j];
            if (transforms->dirty_flags[idx]) {
                mat4* m = &transforms->world_matrices[idx];
                
                // Scale
                m->m[0][0] = transforms->scales_x[idx];
                m->m[1][1] = transforms->scales_y[idx];
                m->m[2][2] = transforms->scales_z[idx];
                m->m[3][3] = 1.0f;
                
                // Translation
                m->m[0][3] = transforms->positions_x[idx];
                m->m[1][3] = transforms->positions_y[idx];
                m->m[2][3] = transforms->positions_z[idx];
                
                // Clear dirty flag
                transforms->dirty_flags[idx] = 0;
            }
        }
    }
    
    // Handle remaining entities
    for (u32 i = simd_count; i < count; i++) {
        u32 idx = indices[i];
        if (transforms->dirty_flags[idx]) {
            mat4* m = &transforms->world_matrices[idx];
            
            m->m[0][0] = transforms->scales_x[idx];
            m->m[1][1] = transforms->scales_y[idx];
            m->m[2][2] = transforms->scales_z[idx];
            m->m[3][3] = 1.0f;
            
            m->m[0][3] = transforms->positions_x[idx];
            m->m[1][3] = transforms->positions_y[idx];
            m->m[2][3] = transforms->positions_z[idx];
            
            transforms->dirty_flags[idx] = 0;
        }
    }
}

// SIMD physics integration
static void physics_integrate_simd(physics_soa* physics, transform_soa* transforms, 
                                   u32* indices, u32 count, f32 dt) {
    __m128 dt_vec = _mm_set1_ps(dt);
    __m128 half_dt_sq = _mm_set1_ps(0.5f * dt * dt);
    
    // Process 4 entities at a time
    u32 simd_count = count & ~3;
    
    for (u32 i = 0; i < simd_count; i += 4) {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];
        u32 i3 = indices[i + 3];
        
        // Load velocities
        __m128 vx = _mm_set_ps(physics->velocities_x[i3], physics->velocities_x[i2],
                               physics->velocities_x[i1], physics->velocities_x[i0]);
        __m128 vy = _mm_set_ps(physics->velocities_y[i3], physics->velocities_y[i2],
                               physics->velocities_y[i1], physics->velocities_y[i0]);
        __m128 vz = _mm_set_ps(physics->velocities_z[i3], physics->velocities_z[i2],
                               physics->velocities_z[i1], physics->velocities_z[i0]);
        
        // Load accelerations
        __m128 ax = _mm_set_ps(physics->accelerations_x[i3], physics->accelerations_x[i2],
                               physics->accelerations_x[i1], physics->accelerations_x[i0]);
        __m128 ay = _mm_set_ps(physics->accelerations_y[i3], physics->accelerations_y[i2],
                               physics->accelerations_y[i1], physics->accelerations_y[i0]);
        __m128 az = _mm_set_ps(physics->accelerations_z[i3], physics->accelerations_z[i2],
                               physics->accelerations_z[i1], physics->accelerations_z[i0]);
        
        // Load positions
        __m128 px = _mm_set_ps(transforms->positions_x[i3], transforms->positions_x[i2],
                               transforms->positions_x[i1], transforms->positions_x[i0]);
        __m128 py = _mm_set_ps(transforms->positions_y[i3], transforms->positions_y[i2],
                               transforms->positions_y[i1], transforms->positions_y[i0]);
        __m128 pz = _mm_set_ps(transforms->positions_z[i3], transforms->positions_z[i2],
                               transforms->positions_z[i1], transforms->positions_z[i0]);
        
        // Update positions: p = p + v*dt + 0.5*a*dt^2
        px = _mm_add_ps(px, _mm_add_ps(_mm_mul_ps(vx, dt_vec), 
                                       _mm_mul_ps(ax, half_dt_sq)));
        py = _mm_add_ps(py, _mm_add_ps(_mm_mul_ps(vy, dt_vec), 
                                       _mm_mul_ps(ay, half_dt_sq)));
        pz = _mm_add_ps(pz, _mm_add_ps(_mm_mul_ps(vz, dt_vec), 
                                       _mm_mul_ps(az, half_dt_sq)));
        
        // Update velocities: v = v + a*dt
        vx = _mm_add_ps(vx, _mm_mul_ps(ax, dt_vec));
        vy = _mm_add_ps(vy, _mm_mul_ps(ay, dt_vec));
        vz = _mm_add_ps(vz, _mm_mul_ps(az, dt_vec));
        
        // Apply drag
        __m128 drag = _mm_set_ps(physics->drag_coefficients[i3], physics->drag_coefficients[i2],
                                 physics->drag_coefficients[i1], physics->drag_coefficients[i0]);
        __m128 drag_factor = _mm_sub_ps(_mm_set1_ps(1.0f), drag);
        vx = _mm_mul_ps(vx, drag_factor);
        vy = _mm_mul_ps(vy, drag_factor);
        vz = _mm_mul_ps(vz, drag_factor);
        
        // Store back results
        f32 px_arr[4], py_arr[4], pz_arr[4];
        f32 vx_arr[4], vy_arr[4], vz_arr[4];
        
        _mm_store_ps(px_arr, px);
        _mm_store_ps(py_arr, py);
        _mm_store_ps(pz_arr, pz);
        _mm_store_ps(vx_arr, vx);
        _mm_store_ps(vy_arr, vy);
        _mm_store_ps(vz_arr, vz);
        
        for (u32 j = 0; j < 4; j++) {
            u32 idx = indices[i + j];
            transforms->positions_x[idx] = px_arr[j];
            transforms->positions_y[idx] = py_arr[j];
            transforms->positions_z[idx] = pz_arr[j];
            physics->velocities_x[idx] = vx_arr[j];
            physics->velocities_y[idx] = vy_arr[j];
            physics->velocities_z[idx] = vz_arr[j];
            transforms->dirty_flags[idx] = 1;
        }
    }
    
    // Handle remaining entities
    for (u32 i = simd_count; i < count; i++) {
        u32 idx = indices[i];
        
        // Update position
        transforms->positions_x[idx] += physics->velocities_x[idx] * dt + 
                                        0.5f * physics->accelerations_x[idx] * dt * dt;
        transforms->positions_y[idx] += physics->velocities_y[idx] * dt + 
                                        0.5f * physics->accelerations_y[idx] * dt * dt;
        transforms->positions_z[idx] += physics->velocities_z[idx] * dt + 
                                        0.5f * physics->accelerations_z[idx] * dt * dt;
        
        // Update velocity
        physics->velocities_x[idx] += physics->accelerations_x[idx] * dt;
        physics->velocities_y[idx] += physics->accelerations_y[idx] * dt;
        physics->velocities_z[idx] += physics->accelerations_z[idx] * dt;
        
        // Apply drag
        f32 drag_factor = 1.0f - physics->drag_coefficients[idx];
        physics->velocities_x[idx] *= drag_factor;
        physics->velocities_y[idx] *= drag_factor;
        physics->velocities_z[idx] *= drag_factor;
        
        transforms->dirty_flags[idx] = 1;
    }
}

// Query entities with components
static entity_query entity_query_create(entity_storage* storage, arena* temp_arena, u32 component_mask) {
    entity_query query = {0};
    query.component_mask = component_mask;
    query.indices = arena_alloc_array(temp_arena, u32, storage->max_entities);
    
    for (u32 i = 0; i < storage->max_entities; i++) {
        if ((storage->component_masks[i] & component_mask) == component_mask) {
            query.indices[query.count++] = i;
        }
    }
    
    return query;
}

// Performance statistics
typedef struct entity_perf_stats {
    f64 transform_update_ms;
    f64 physics_update_ms;
    f64 query_time_ms;
    u32 cache_misses;
    u32 simd_operations;
} entity_perf_stats;

static entity_perf_stats g_entity_stats = {0};

static void entity_print_perf_stats(void) {
    printf("=== Entity System Performance ===\n");
    printf("Transform Update: %.3f ms\n", g_entity_stats.transform_update_ms);
    printf("Physics Update: %.3f ms\n", g_entity_stats.physics_update_ms);
    printf("Query Time: %.3f ms\n", g_entity_stats.query_time_ms);
    printf("SIMD Operations: %u\n", g_entity_stats.simd_operations);
    printf("Estimated Cache Misses: %u\n", g_entity_stats.cache_misses);
}

#endif // HANDMADE_ENTITY_SOA_H