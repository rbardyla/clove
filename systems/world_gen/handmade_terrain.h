#ifndef HANDMADE_TERRAIN_H
#define HANDMADE_TERRAIN_H

/*
    Handmade Terrain System
    Procedural terrain generation with LOD and streaming
    Zero dependencies, SIMD optimized
    
    Features:
    - Chunked terrain with LOD levels
    - Marching cubes for smooth terrain
    - Biome-based material generation
    - Background streaming and caching
    - Integration with asset system for textures
*/

#include "../../src/handmade.h"
#include "handmade_noise.h"

// Forward declarations to avoid circular dependencies
typedef struct asset_system asset_system;
typedef struct renderer_state renderer_state;

// Math types
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;
typedef struct { f32 m[16]; } mat4;

// =============================================================================
// TERRAIN CONFIGURATION
// =============================================================================

#define TERRAIN_CHUNK_SIZE 64       // Vertices per chunk edge
#define TERRAIN_MAX_LOD 4            // Maximum LOD levels
#define TERRAIN_CACHE_SIZE 256       // Maximum cached chunks
#define TERRAIN_VIEW_DISTANCE 1000.0f // View distance in world units

// Terrain vertex format
typedef struct terrain_vertex {
    v3 position;
    v3 normal;
    v2 texcoord;
    u32 material_weights;  // 4x8bit weights for texture blending
} terrain_vertex;

// Terrain chunk (one piece of the world)
typedef struct terrain_chunk {
    // Chunk location
    s32 chunk_x;
    s32 chunk_z;
    u32 lod_level;
    
    // Mesh data
    terrain_vertex* vertices;
    u32* indices;
    u32 vertex_count;
    u32 index_count;
    
    // Rendering data
    u32 vbo;
    u32 ibo;
    u32 vao;
    
    // Bounds for culling
    v3 min_bounds;
    v3 max_bounds;
    
    // State
    b32 is_generated;
    b32 is_uploaded;
    b32 needs_update;
    
    // Cache management
    u32 last_used_frame;
    struct terrain_chunk* next;
    struct terrain_chunk* prev;
} terrain_chunk;

// Biome types
typedef enum terrain_biome {
    BIOME_OCEAN = 0,
    BIOME_BEACH,
    BIOME_GRASSLAND,
    BIOME_FOREST,
    BIOME_MOUNTAIN,
    BIOME_SNOW,
    BIOME_COUNT
} terrain_biome;

// Biome configuration
typedef struct biome_config {
    f32 height_min;
    f32 height_max;
    f32 moisture_min;
    f32 moisture_max;
    f32 temperature_min;
    f32 temperature_max;
    
    // Texture indices in asset system
    u32 diffuse_texture;
    u32 normal_texture;
    u32 detail_texture;
    
    // Material properties
    v3 base_color;
    f32 roughness;
    f32 metallic;
} biome_config;

// Terrain generation parameters
typedef struct terrain_gen_params {
    // Base terrain shape
    f32 base_frequency;
    f32 base_amplitude;
    u32 octaves;
    f32 persistence;
    f32 lacunarity;
    
    // Terrain features
    f32 mountain_frequency;
    f32 mountain_amplitude;
    f32 erosion_strength;
    f32 valley_depth;
    
    // Biome parameters
    f32 moisture_frequency;
    f32 temperature_frequency;
    
    // World scale
    f32 horizontal_scale;
    f32 vertical_scale;
    f32 sea_level;
} terrain_gen_params;

// Terrain system state
typedef struct terrain_system {
    // Memory
    arena* main_arena;
    arena* temp_arena;
    
    // Noise generation
    noise_state* height_noise;
    noise_state* moisture_noise;
    noise_state* temperature_noise;
    
    // Generation parameters
    terrain_gen_params params;
    
    // Biome configuration
    biome_config biomes[BIOME_COUNT];
    
    // Chunk management
    terrain_chunk* chunks;          // Pool of chunks
    terrain_chunk* active_chunks;   // LRU list of active chunks
    u32 chunk_count;
    
    // Streaming
    struct {
        terrain_chunk** load_queue;
        u32 load_queue_size;
        u32 load_queue_capacity;
        b32 is_loading;
    } streaming;
    
    // Rendering
    renderer_state* renderer;
    asset_system* assets;
    
    // Statistics
    struct {
        u32 chunks_generated;
        u32 chunks_cached;
        u32 vertices_rendered;
        f64 generation_time_ms;
    } stats;
} terrain_system;

// =============================================================================
// TERRAIN API
// =============================================================================

// Initialize terrain system
terrain_system* terrain_init(arena* arena, renderer_state* renderer, 
                             asset_system* assets, u32 seed);

// Update terrain (streaming, LOD updates)
void terrain_update(terrain_system* terrain, v3 camera_pos, f32 dt);

// Render visible terrain
void terrain_render(terrain_system* terrain, mat4* view_proj, v3 camera_pos);

// Generate a chunk of terrain
void terrain_generate_chunk(terrain_system* terrain, terrain_chunk* chunk,
                           s32 chunk_x, s32 chunk_z, u32 lod_level);

// Upload chunk to GPU
void terrain_upload_chunk(terrain_system* terrain, terrain_chunk* chunk);

// Get height at world position
f32 terrain_get_height(terrain_system* terrain, f32 world_x, f32 world_z);

// Get biome at world position
terrain_biome terrain_get_biome(terrain_system* terrain, f32 world_x, f32 world_z);

// Raycast against terrain
b32 terrain_raycast(terrain_system* terrain, v3 origin, v3 direction,
                   f32 max_distance, v3* hit_point, v3* hit_normal);

// =============================================================================
// TERRAIN GENERATION HELPERS
// =============================================================================

// Generate height for a point
f32 terrain_sample_height(terrain_system* terrain, f32 x, f32 z);

// Generate vertex normals from height data
void terrain_calculate_normals(terrain_vertex* vertices, u32* indices,
                              u32 vertex_count, u32 index_count);

// Generate texture coordinates
void terrain_generate_texcoords(terrain_vertex* vertices, u32 vertex_count,
                               f32 scale);

// Blend materials based on height/slope
u32 terrain_calculate_material_weights(terrain_system* terrain,
                                      v3 position, v3 normal);

// =============================================================================
// TERRAIN LOD MANAGEMENT
// =============================================================================

// Calculate appropriate LOD level for chunk
u32 terrain_calculate_lod(v3 camera_pos, s32 chunk_x, s32 chunk_z);

// Simplify mesh for LOD
void terrain_simplify_mesh(terrain_vertex* vertices, u32* indices,
                          u32* vertex_count, u32* index_count,
                          u32 target_lod);

// =============================================================================
// MARCHING CUBES IMPLEMENTATION
// =============================================================================

// Generate smooth terrain using marching cubes
void terrain_marching_cubes(terrain_system* terrain, terrain_chunk* chunk,
                           f32* density_field, f32 iso_level);

// Sample density field for marching cubes
f32 terrain_sample_density(terrain_system* terrain, v3 pos);

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

// Show terrain debug GUI
void terrain_show_debug_gui(terrain_system* terrain);

// Draw terrain wireframe
void terrain_debug_draw_wireframe(terrain_system* terrain, mat4* view_proj);

// Print terrain statistics
void terrain_print_stats(terrain_system* terrain);

#endif // HANDMADE_TERRAIN_H