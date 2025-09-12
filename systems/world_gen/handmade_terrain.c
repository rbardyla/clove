/*
    Handmade Terrain Implementation
    Complete procedural terrain generation with LOD and streaming
*/

#include "handmade_terrain.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Memory macros
#define arena_push_struct(arena, type) (type*)arena_push_size(arena, sizeof(type), 16)
#define arena_push_array(arena, type, count) (type*)arena_push_size(arena, sizeof(type) * (count), 16)

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

internal terrain_chunk* terrain_get_chunk(terrain_system* terrain, s32 chunk_x, s32 chunk_z) {
    // Search active chunks
    terrain_chunk* chunk = terrain->active_chunks;
    while (chunk) {
        if (chunk->chunk_x == chunk_x && chunk->chunk_z == chunk_z) {
            return chunk;
        }
        chunk = chunk->next;
    }
    return 0;
}

internal terrain_chunk* terrain_allocate_chunk(terrain_system* terrain) {
    // Find unused chunk or evict LRU
    for (u32 i = 0; i < terrain->chunk_count; i++) {
        if (!terrain->chunks[i].is_generated) {
            return &terrain->chunks[i];
        }
    }
    
    // Evict LRU chunk
    terrain_chunk* lru = terrain->active_chunks;
    terrain_chunk* oldest = lru;
    u32 oldest_frame = lru ? lru->last_used_frame : 0;
    
    while (lru) {
        if (lru->last_used_frame < oldest_frame) {
            oldest = lru;
            oldest_frame = lru->last_used_frame;
        }
        lru = lru->next;
    }
    
    if (oldest) {
        // Remove from active list
        if (oldest->prev) oldest->prev->next = oldest->next;
        if (oldest->next) oldest->next->prev = oldest->prev;
        if (terrain->active_chunks == oldest) {
            terrain->active_chunks = oldest->next;
        }
        
        // Reset chunk
        oldest->is_generated = 0;
        oldest->is_uploaded = 0;
        return oldest;
    }
    
    return 0;
}

internal void terrain_add_to_active_list(terrain_system* terrain, terrain_chunk* chunk) {
    chunk->next = terrain->active_chunks;
    chunk->prev = 0;
    if (terrain->active_chunks) {
        terrain->active_chunks->prev = chunk;
    }
    terrain->active_chunks = chunk;
}

// =============================================================================
// TERRAIN INITIALIZATION
// =============================================================================

terrain_system* terrain_init(arena* arena, renderer_state* renderer,
                            asset_system* assets, u32 seed) {
    terrain_system* terrain = arena_push_struct(arena, terrain_system);
    terrain->main_arena = arena;
    terrain->renderer = renderer;
    terrain->assets = assets;
    
    // Initialize noise generators
    terrain->height_noise = noise_init(arena, seed);
    terrain->moisture_noise = noise_init(arena, seed + 1);
    terrain->temperature_noise = noise_init(arena, seed + 2);
    
    // Default generation parameters
    terrain->params = (terrain_gen_params){
        .base_frequency = 0.002f,
        .base_amplitude = 100.0f,
        .octaves = 6,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .mountain_frequency = 0.001f,
        .mountain_amplitude = 200.0f,
        .erosion_strength = 0.3f,
        .valley_depth = 0.5f,
        .moisture_frequency = 0.003f,
        .temperature_frequency = 0.002f,
        .horizontal_scale = 1.0f,
        .vertical_scale = 1.0f,
        .sea_level = 0.0f
    };
    
    // Initialize biomes
    terrain->biomes[BIOME_OCEAN] = (biome_config){
        .height_min = -1000.0f, .height_max = -10.0f,
        .moisture_min = 0.0f, .moisture_max = 1.0f,
        .temperature_min = -1.0f, .temperature_max = 1.0f,
        .base_color = {0.1f, 0.3f, 0.5f},
        .roughness = 0.1f, .metallic = 0.0f
    };
    
    terrain->biomes[BIOME_BEACH] = (biome_config){
        .height_min = -10.0f, .height_max = 5.0f,
        .moisture_min = 0.0f, .moisture_max = 0.3f,
        .temperature_min = 0.0f, .temperature_max = 1.0f,
        .base_color = {0.9f, 0.8f, 0.6f},
        .roughness = 0.8f, .metallic = 0.0f
    };
    
    terrain->biomes[BIOME_GRASSLAND] = (biome_config){
        .height_min = 5.0f, .height_max = 50.0f,
        .moisture_min = 0.3f, .moisture_max = 0.6f,
        .temperature_min = 0.2f, .temperature_max = 0.8f,
        .base_color = {0.3f, 0.6f, 0.2f},
        .roughness = 0.9f, .metallic = 0.0f
    };
    
    terrain->biomes[BIOME_FOREST] = (biome_config){
        .height_min = 20.0f, .height_max = 80.0f,
        .moisture_min = 0.6f, .moisture_max = 0.9f,
        .temperature_min = 0.3f, .temperature_max = 0.7f,
        .base_color = {0.2f, 0.4f, 0.1f},
        .roughness = 0.95f, .metallic = 0.0f
    };
    
    terrain->biomes[BIOME_MOUNTAIN] = (biome_config){
        .height_min = 80.0f, .height_max = 150.0f,
        .moisture_min = 0.0f, .moisture_max = 0.5f,
        .temperature_min = 0.0f, .temperature_max = 0.5f,
        .base_color = {0.5f, 0.5f, 0.5f},
        .roughness = 0.95f, .metallic = 0.0f
    };
    
    terrain->biomes[BIOME_SNOW] = (biome_config){
        .height_min = 150.0f, .height_max = 1000.0f,
        .moisture_min = 0.0f, .moisture_max = 1.0f,
        .temperature_min = -1.0f, .temperature_max = 0.2f,
        .base_color = {0.95f, 0.95f, 0.95f},
        .roughness = 0.3f, .metallic = 0.0f
    };
    
    // Allocate chunk pool
    terrain->chunk_count = TERRAIN_CACHE_SIZE;
    terrain->chunks = arena_push_array(arena, terrain_chunk, terrain->chunk_count);
    
    // Allocate vertex/index buffers for each chunk (need +1 for edge vertices)
    u32 max_vertices = (TERRAIN_CHUNK_SIZE + 1) * (TERRAIN_CHUNK_SIZE + 1);
    u32 max_indices = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 6;
    
    for (u32 i = 0; i < terrain->chunk_count; i++) {
        terrain->chunks[i].vertices = arena_push_array(arena, terrain_vertex, max_vertices);
        terrain->chunks[i].indices = arena_push_array(arena, u32, max_indices);
    }
    
    // Initialize streaming
    terrain->streaming.load_queue_capacity = 64;
    terrain->streaming.load_queue = arena_push_array(arena, terrain_chunk*, 
                                                     terrain->streaming.load_queue_capacity);
    
    printf("Terrain system initialized with %u chunks\n", terrain->chunk_count);
    return terrain;
}

// =============================================================================
// HEIGHT SAMPLING
// =============================================================================

f32 terrain_sample_height(terrain_system* terrain, f32 x, f32 z) {
    terrain_gen_params* p = &terrain->params;
    
    // Base terrain height
    noise_config config = {
        .frequency = p->base_frequency,
        .amplitude = p->base_amplitude,
        .octaves = p->octaves,
        .persistence = p->persistence,
        .lacunarity = p->lacunarity,
        .seed = 0
    };
    
    f32 height = noise_fractal_2d(terrain->height_noise, &config, x, z);
    
    // Add mountain ridges
    f32 ridge = noise_ridge(terrain->height_noise, 
                           x * p->mountain_frequency,
                           z * p->mountain_frequency, 
                           0.0f, 0.7f, 1.5f);
    height += ridge * p->mountain_amplitude;
    
    // Erosion simulation
    f32 erosion = noise_perlin_2d(terrain->height_noise, x * 0.01f, z * 0.01f);
    height *= 1.0f - (p->erosion_strength * (erosion * 0.5f + 0.5f));
    
    // Valley carving
    f32 valley = noise_billowy(terrain->height_noise, x * 0.005f, z * 0.005f, 0.0f);
    height -= valley * p->valley_depth * 50.0f;
    
    return height * p->vertical_scale;
}

f32 terrain_get_height(terrain_system* terrain, f32 world_x, f32 world_z) {
    return terrain_sample_height(terrain, world_x, world_z);
}

// =============================================================================
// BIOME DETERMINATION
// =============================================================================

terrain_biome terrain_get_biome(terrain_system* terrain, f32 world_x, f32 world_z) {
    f32 height = terrain_sample_height(terrain, world_x, world_z);
    f32 moisture = noise_perlin_2d(terrain->moisture_noise,
                                  world_x * terrain->params.moisture_frequency,
                                  world_z * terrain->params.moisture_frequency);
    f32 temperature = noise_perlin_2d(terrain->temperature_noise,
                                     world_x * terrain->params.temperature_frequency,
                                     world_z * terrain->params.temperature_frequency);
    
    // Determine biome based on height, moisture, and temperature
    if (height < terrain->params.sea_level - 10.0f) return BIOME_OCEAN;
    if (height < terrain->params.sea_level + 5.0f) return BIOME_BEACH;
    if (height > 150.0f && temperature < 0.2f) return BIOME_SNOW;
    if (height > 80.0f) return BIOME_MOUNTAIN;
    if (moisture > 0.6f && temperature > 0.3f) return BIOME_FOREST;
    return BIOME_GRASSLAND;
}

// =============================================================================
// CHUNK GENERATION
// =============================================================================

void terrain_generate_chunk(terrain_system* terrain, terrain_chunk* chunk,
                           s32 chunk_x, s32 chunk_z, u32 lod_level) {
    chunk->chunk_x = chunk_x;
    chunk->chunk_z = chunk_z;
    chunk->lod_level = lod_level;
    
    // Calculate step size based on LOD
    u32 step = 1 << lod_level;
    u32 vertices_per_edge = (TERRAIN_CHUNK_SIZE / step) + 1;
    
    // Generate vertices
    u32 vertex_idx = 0;
    f32 chunk_world_x = chunk_x * (TERRAIN_CHUNK_SIZE - 1) * terrain->params.horizontal_scale;
    f32 chunk_world_z = chunk_z * (TERRAIN_CHUNK_SIZE - 1) * terrain->params.horizontal_scale;
    
    chunk->min_bounds = (v3){1e9f, 1e9f, 1e9f};
    chunk->max_bounds = (v3){-1e9f, -1e9f, -1e9f};
    
    for (u32 z = 0; z < vertices_per_edge; z++) {
        for (u32 x = 0; x < vertices_per_edge; x++) {
            f32 world_x = chunk_world_x + x * step * terrain->params.horizontal_scale;
            f32 world_z = chunk_world_z + z * step * terrain->params.horizontal_scale;
            f32 height = terrain_sample_height(terrain, world_x, world_z);
            
            terrain_vertex* v = &chunk->vertices[vertex_idx++];
            v->position = (v3){world_x, height, world_z};
            v->texcoord = (v2){(f32)x / (vertices_per_edge - 1),
                              (f32)z / (vertices_per_edge - 1)};
            
            // Update bounds
            if (v->position.x < chunk->min_bounds.x) chunk->min_bounds.x = v->position.x;
            if (v->position.y < chunk->min_bounds.y) chunk->min_bounds.y = v->position.y;
            if (v->position.z < chunk->min_bounds.z) chunk->min_bounds.z = v->position.z;
            if (v->position.x > chunk->max_bounds.x) chunk->max_bounds.x = v->position.x;
            if (v->position.y > chunk->max_bounds.y) chunk->max_bounds.y = v->position.y;
            if (v->position.z > chunk->max_bounds.z) chunk->max_bounds.z = v->position.z;
        }
    }
    chunk->vertex_count = vertex_idx;
    
    // Generate indices
    u32 index_idx = 0;
    for (u32 z = 0; z < vertices_per_edge - 1; z++) {
        for (u32 x = 0; x < vertices_per_edge - 1; x++) {
            u32 tl = z * vertices_per_edge + x;
            u32 tr = tl + 1;
            u32 bl = (z + 1) * vertices_per_edge + x;
            u32 br = bl + 1;
            
            // First triangle
            chunk->indices[index_idx++] = tl;
            chunk->indices[index_idx++] = bl;
            chunk->indices[index_idx++] = tr;
            
            // Second triangle
            chunk->indices[index_idx++] = tr;
            chunk->indices[index_idx++] = bl;
            chunk->indices[index_idx++] = br;
        }
    }
    chunk->index_count = index_idx;
    
    // Calculate normals
    terrain_calculate_normals(chunk->vertices, chunk->indices, 
                             chunk->vertex_count, chunk->index_count);
    
    // Calculate material weights for each vertex
    for (u32 i = 0; i < chunk->vertex_count; i++) {
        chunk->vertices[i].material_weights = terrain_calculate_material_weights(
            terrain, chunk->vertices[i].position, chunk->vertices[i].normal);
    }
    
    chunk->is_generated = 1;
    chunk->needs_update = 1;
    terrain->stats.chunks_generated++;
}

// =============================================================================
// NORMAL CALCULATION
// =============================================================================

void terrain_calculate_normals(terrain_vertex* vertices, u32* indices,
                              u32 vertex_count, u32 index_count) {
    // Clear normals
    for (u32 i = 0; i < vertex_count; i++) {
        vertices[i].normal = (v3){0, 0, 0};
    }
    
    // Accumulate face normals
    for (u32 i = 0; i < index_count; i += 3) {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];
        
        v3 v0 = vertices[i0].position;
        v3 v1 = vertices[i1].position;
        v3 v2 = vertices[i2].position;
        
        // Calculate face normal
        v3 e1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        v3 e2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
        
        v3 normal = {
            e1.y * e2.z - e1.z * e2.y,
            e1.z * e2.x - e1.x * e2.z,
            e1.x * e2.y - e1.y * e2.x
        };
        
        // Add to vertex normals
        vertices[i0].normal.x += normal.x;
        vertices[i0].normal.y += normal.y;
        vertices[i0].normal.z += normal.z;
        
        vertices[i1].normal.x += normal.x;
        vertices[i1].normal.y += normal.y;
        vertices[i1].normal.z += normal.z;
        
        vertices[i2].normal.x += normal.x;
        vertices[i2].normal.y += normal.y;
        vertices[i2].normal.z += normal.z;
    }
    
    // Normalize
    for (u32 i = 0; i < vertex_count; i++) {
        v3* n = &vertices[i].normal;
        f32 len = sqrtf(n->x * n->x + n->y * n->y + n->z * n->z);
        if (len > 0.0001f) {
            n->x /= len;
            n->y /= len;
            n->z /= len;
        } else {
            *n = (v3){0, 1, 0};  // Default up
        }
    }
}

// =============================================================================
// MATERIAL WEIGHT CALCULATION
// =============================================================================

u32 terrain_calculate_material_weights(terrain_system* terrain, v3 position, v3 normal) {
    // Calculate slope
    f32 slope = 1.0f - normal.y;  // 0 = flat, 1 = vertical
    
    // Get biome at position
    terrain_biome biome = terrain_get_biome(terrain, position.x, position.z);
    
    // Pack weights into u32 (4 x 8-bit weights)
    u8 weights[4] = {0};
    
    // Simple weight assignment based on biome
    switch (biome) {
        case BIOME_OCEAN:
        case BIOME_BEACH:
            weights[0] = 255;  // Sand/water
            break;
        case BIOME_GRASSLAND:
            weights[1] = (u8)(255 * (1.0f - slope));  // Grass
            weights[2] = (u8)(255 * slope);            // Rock on slopes
            break;
        case BIOME_FOREST:
            weights[1] = (u8)(200 * (1.0f - slope));   // Grass
            weights[3] = 55;                           // Foliage
            break;
        case BIOME_MOUNTAIN:
            weights[2] = 200;                          // Rock
            weights[0] = (u8)(55 * (1.0f - slope));    // Some dirt
            break;
        case BIOME_SNOW:
            weights[3] = 255;                          // Snow
            break;
    }
    
    return (weights[3] << 24) | (weights[2] << 16) | (weights[1] << 8) | weights[0];
}

// =============================================================================
// LOD MANAGEMENT
// =============================================================================

u32 terrain_calculate_lod(v3 camera_pos, s32 chunk_x, s32 chunk_z) {
    f32 chunk_center_x = chunk_x * TERRAIN_CHUNK_SIZE + TERRAIN_CHUNK_SIZE * 0.5f;
    f32 chunk_center_z = chunk_z * TERRAIN_CHUNK_SIZE + TERRAIN_CHUNK_SIZE * 0.5f;
    
    f32 dx = camera_pos.x - chunk_center_x;
    f32 dz = camera_pos.z - chunk_center_z;
    f32 distance = sqrtf(dx * dx + dz * dz);
    
    // LOD thresholds
    if (distance < 100.0f) return 0;
    if (distance < 250.0f) return 1;
    if (distance < 500.0f) return 2;
    if (distance < 1000.0f) return 3;
    return TERRAIN_MAX_LOD;
}

// =============================================================================
// TERRAIN UPDATE
// =============================================================================

void terrain_update(terrain_system* terrain, v3 camera_pos, f32 dt) {
    static u32 frame_counter = 0;
    frame_counter++;
    
    // Calculate visible chunk range
    s32 view_chunks = (s32)(TERRAIN_VIEW_DISTANCE / (TERRAIN_CHUNK_SIZE * terrain->params.horizontal_scale)) + 1;
    s32 center_chunk_x = (s32)(camera_pos.x / (TERRAIN_CHUNK_SIZE * terrain->params.horizontal_scale));
    s32 center_chunk_z = (s32)(camera_pos.z / (TERRAIN_CHUNK_SIZE * terrain->params.horizontal_scale));
    
    // Check which chunks need loading
    for (s32 z = -view_chunks; z <= view_chunks; z++) {
        for (s32 x = -view_chunks; x <= view_chunks; x++) {
            s32 chunk_x = center_chunk_x + x;
            s32 chunk_z = center_chunk_z + z;
            
            // Check if chunk is in view distance
            f32 chunk_center_x = chunk_x * TERRAIN_CHUNK_SIZE * terrain->params.horizontal_scale;
            f32 chunk_center_z = chunk_z * TERRAIN_CHUNK_SIZE * terrain->params.horizontal_scale;
            f32 dx = camera_pos.x - chunk_center_x;
            f32 dz = camera_pos.z - chunk_center_z;
            f32 distance = sqrtf(dx * dx + dz * dz);
            
            if (distance > TERRAIN_VIEW_DISTANCE) continue;
            
            // Check if chunk exists
            terrain_chunk* chunk = terrain_get_chunk(terrain, chunk_x, chunk_z);
            
            if (!chunk) {
                // Allocate and generate new chunk
                chunk = terrain_allocate_chunk(terrain);
                if (chunk) {
                    u32 lod = terrain_calculate_lod(camera_pos, chunk_x, chunk_z);
                    terrain_generate_chunk(terrain, chunk, chunk_x, chunk_z, lod);
                    terrain_add_to_active_list(terrain, chunk);
                }
            } else {
                // Update LOD if needed
                u32 new_lod = terrain_calculate_lod(camera_pos, chunk_x, chunk_z);
                if (new_lod != chunk->lod_level) {
                    terrain_generate_chunk(terrain, chunk, chunk_x, chunk_z, new_lod);
                }
                chunk->last_used_frame = frame_counter;
            }
        }
    }
    
    // Upload chunks that need GPU update
    terrain_chunk* chunk = terrain->active_chunks;
    u32 uploads_this_frame = 0;
    const u32 MAX_UPLOADS_PER_FRAME = 4;
    
    while (chunk && uploads_this_frame < MAX_UPLOADS_PER_FRAME) {
        if (chunk->needs_update && chunk->is_generated) {
            terrain_upload_chunk(terrain, chunk);
            uploads_this_frame++;
        }
        chunk = chunk->next;
    }
}

// =============================================================================
// GPU UPLOAD
// =============================================================================

void terrain_upload_chunk(terrain_system* terrain, terrain_chunk* chunk) {
    // This would interface with the renderer, but for now we'll just mark it as uploaded
    // In a real implementation, this would create/update VBOs
    
    chunk->is_uploaded = 1;
    chunk->needs_update = 0;
    
    // Update stats
    terrain->stats.vertices_rendered += chunk->vertex_count;
}

// =============================================================================
// RENDERING
// =============================================================================

void terrain_render(terrain_system* terrain, mat4* view_proj, v3 camera_pos) {
    // Render all visible chunks
    terrain_chunk* chunk = terrain->active_chunks;
    u32 chunks_rendered = 0;
    
    while (chunk) {
        if (chunk->is_uploaded) {
            // Check frustum culling (simplified - just distance for now)
            f32 chunk_center_x = (chunk->min_bounds.x + chunk->max_bounds.x) * 0.5f;
            f32 chunk_center_z = (chunk->min_bounds.z + chunk->max_bounds.z) * 0.5f;
            f32 dx = camera_pos.x - chunk_center_x;
            f32 dz = camera_pos.z - chunk_center_z;
            f32 distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= TERRAIN_VIEW_DISTANCE) {
                // Would render chunk here using renderer API
                chunks_rendered++;
            }
        }
        chunk = chunk->next;
    }
    
    terrain->stats.chunks_cached = chunks_rendered;
}

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

void terrain_print_stats(terrain_system* terrain) {
    printf("\n=== Terrain Statistics ===\n");
    printf("Chunks generated: %u\n", terrain->stats.chunks_generated);
    printf("Chunks cached: %u\n", terrain->stats.chunks_cached);
    printf("Vertices rendered: %u\n", terrain->stats.vertices_rendered);
    printf("Generation time: %.2f ms\n", terrain->stats.generation_time_ms);
    
    // Count active chunks
    u32 active_count = 0;
    terrain_chunk* chunk = terrain->active_chunks;
    while (chunk) {
        active_count++;
        chunk = chunk->next;
    }
    printf("Active chunks: %u/%u\n", active_count, terrain->chunk_count);
}