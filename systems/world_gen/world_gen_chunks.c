/*
    World Generation - Chunk Management and Generation
    Handles chunk loading, generation, and caching
*/

#include "handmade_world_gen.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define internal static

// Fast random number generator (declared in handmade_world_gen.c)
extern f32 fast_randf(u32 seed);

// Chunk generation context
internal void
init_generation_context(generation_context *ctx, world_gen_system *world_gen, 
                       world_chunk *chunk, i32 chunk_x, i32 chunk_y) {
    ctx->world_gen = world_gen;
    ctx->chunk = chunk;
    ctx->global_x = chunk_x * WORLD_CHUNK_SIZE;
    ctx->global_y = chunk_y * WORLD_CHUNK_SIZE;
    ctx->random_seed = (u32)world_gen_get_chunk_seed(world_gen, chunk_x, chunk_y);
}

// Generate basic terrain for a chunk
internal void
generate_chunk_terrain(generation_context *ctx) {
    world_chunk *chunk = ctx->chunk;
    world_gen_system *system = ctx->world_gen;
    
    f32 min_elevation = 10000.0f;
    f32 max_elevation = -10000.0f;
    f32 total_elevation = 0.0f;
    f32 total_temperature = 0.0f;
    
    // Generate each tile
    for (i32 y = 0; y < WORLD_CHUNK_SIZE; y++) {
        for (i32 x = 0; x < WORLD_CHUNK_SIZE; x++) {
            world_tile *tile = &chunk->tiles[x][y];
            
            i32 world_x = ctx->global_x + x;
            i32 world_y = ctx->global_y + y;
            
            // Generate elevation
            tile->elevation = world_gen_sample_elevation(system, (f32)world_x, (f32)world_y);
            
            // Calculate climate
            tile->climate = world_gen_calculate_climate(system, (f32)world_x, (f32)world_y, tile->elevation);
            
            // Determine biome
            tile->biome = world_gen_determine_biome(system, 
                                                   tile->climate.temperature,
                                                   tile->climate.humidity, 
                                                   tile->elevation);
            
            // Initialize other properties
            tile->secondary_biome = tile->biome;
            tile->biome_blend = 0.0f;
            tile->feature = FEATURE_NONE;
            tile->resource = RESOURCE_NONE;
            tile->resource_density = 0.0f;
            tile->resource_quality = 0.0f;
            tile->structure_id = 0;
            tile->structure_health = 0.0f;
            tile->explored = 0;
            tile->visible = 0;
            tile->danger_level = 0.0f;
            tile->last_update_time = 0;
            
            // Track statistics
            if (tile->elevation < min_elevation) min_elevation = tile->elevation;
            if (tile->elevation > max_elevation) max_elevation = tile->elevation;
            total_elevation += tile->elevation;
            total_temperature += tile->climate.temperature;
        }
    }
    
    // Set chunk metadata
    chunk->average_elevation = total_elevation / (WORLD_CHUNK_SIZE * WORLD_CHUNK_SIZE);
    chunk->average_temperature = total_temperature / (WORLD_CHUNK_SIZE * WORLD_CHUNK_SIZE);
    
    // Determine dominant biome
    i32 biome_counts[WORLD_BIOME_COUNT] = {0};
    for (i32 y = 0; y < WORLD_CHUNK_SIZE; y++) {
        for (i32 x = 0; x < WORLD_CHUNK_SIZE; x++) {
            biome_type biome = chunk->tiles[x][y].biome;
            if (biome < WORLD_BIOME_COUNT) {
                biome_counts[biome]++;
            }
        }
    }
    
    i32 max_count = 0;
    for (i32 i = 0; i < WORLD_BIOME_COUNT; i++) {
        if (biome_counts[i] > max_count) {
            max_count = biome_counts[i];
            chunk->dominant_biome = (biome_type)i;
        }
    }
    
    printf("[WORLD_GEN] Generated terrain for chunk (%d,%d), dominant biome: %d, avg elevation: %.1fm\n",
           chunk->chunk_x, chunk->chunk_y, chunk->dominant_biome, chunk->average_elevation);
}

// Add biome transitions and blending
internal void
generate_chunk_biome_blending(generation_context *ctx) {
    world_chunk *chunk = ctx->chunk;
    world_gen_system *system = ctx->world_gen;
    
    // Add biome transitions for more natural looking boundaries
    for (i32 y = 1; y < WORLD_CHUNK_SIZE - 1; y++) {
        for (i32 x = 1; x < WORLD_CHUNK_SIZE - 1; x++) {
            world_tile *tile = &chunk->tiles[x][y];
            
            // Check neighboring tiles for different biomes
            biome_type neighbors[8] = {
                chunk->tiles[x-1][y-1].biome, chunk->tiles[x][y-1].biome, chunk->tiles[x+1][y-1].biome,
                chunk->tiles[x-1][y].biome,                               chunk->tiles[x+1][y].biome,
                chunk->tiles[x-1][y+1].biome, chunk->tiles[x][y+1].biome, chunk->tiles[x+1][y+1].biome
            };
            
            // Count different biomes in neighborhood
            i32 biome_variety = 0;
            biome_type different_biome = tile->biome;
            
            for (i32 i = 0; i < 8; i++) {
                if (neighbors[i] != tile->biome) {
                    biome_variety++;
                    different_biome = neighbors[i];
                }
            }
            
            // If we have biome variety, create a transition
            if (biome_variety > 2) {
                tile->secondary_biome = different_biome;
                
                // Blend factor based on noise for natural transitions
                i32 world_x = ctx->global_x + x;
                i32 world_y = ctx->global_y + y;
                f32 blend_noise = world_gen_noise_2d(&system->biome_noise, 
                                                    (f32)world_x * 0.1f, (f32)world_y * 0.1f);
                tile->biome_blend = (blend_noise + 1.0f) * 0.5f; // 0-1 range
                tile->biome_blend = fmaxf(0.0f, fminf(1.0f, tile->biome_blend));
            }
        }
    }
}

// Generate terrain features
internal void
generate_chunk_features(generation_context *ctx) {
    world_chunk *chunk = ctx->chunk;
    world_gen_system *system = ctx->world_gen;
    
    for (i32 y = 0; y < WORLD_CHUNK_SIZE; y++) {
        for (i32 x = 0; x < WORLD_CHUNK_SIZE; x++) {
            world_tile *tile = &chunk->tiles[x][y];
            i32 world_x = ctx->global_x + x;
            i32 world_y = ctx->global_y + y;
            
            // Feature placement based on biome and terrain
            f32 feature_noise = world_gen_noise_2d(&system->detail_noise[0], 
                                                  (f32)world_x * 0.02f, (f32)world_y * 0.02f);
            
            // Hills and valleys
            if (feature_noise > 0.6f && tile->elevation > system->sea_level + 50.0f) {
                tile->feature = FEATURE_HILL;
            } else if (feature_noise < -0.6f && tile->elevation > system->sea_level) {
                tile->feature = FEATURE_VALLEY;
            }
            
            // Cliffs near elevation changes
            if (x > 0 && y > 0) {
                f32 elevation_diff = fabsf(tile->elevation - chunk->tiles[x-1][y-1].elevation);
                if (elevation_diff > 100.0f) {
                    tile->feature = FEATURE_CLIFF;
                }
            }
            
            // Rivers in valleys near water
            f32 river_noise = world_gen_noise_2d(&system->detail_noise[1], 
                                                (f32)world_x * 0.01f, (f32)world_y * 0.01f);
            if (river_noise > system->river_threshold && tile->feature == FEATURE_VALLEY) {
                tile->feature = FEATURE_RIVER;
            }
            
            // Lakes in low areas
            if (tile->elevation < chunk->average_elevation - 20.0f && 
                tile->elevation > system->sea_level && 
                fast_randf(ctx->random_seed + x + y * WORLD_CHUNK_SIZE) > 0.99f) {
                tile->feature = FEATURE_LAKE;
            }
            
            // Cave entrances in mountains
            if (tile->biome == BIOME_MOUNTAINS || tile->biome == BIOME_SNOW_MOUNTAINS) {
                f32 cave_noise = world_gen_noise_2d(&system->cave_noise, (f32)world_x, (f32)world_y);
                if (cave_noise > system->cave_threshold) {
                    tile->feature = FEATURE_CAVE_ENTRANCE;
                }
            }
            
            // Biome-specific features
            switch (tile->biome) {
                case BIOME_DESERT:
                    if (fast_randf(ctx->random_seed + world_x + world_y) > 0.999f) {
                        tile->feature = FEATURE_OASIS;
                    }
                    break;
                    
                case BIOME_VOLCANIC:
                    if (fast_randf(ctx->random_seed + world_x + world_y) > 0.995f) {
                        tile->feature = FEATURE_GEYSER;
                    }
                    break;
                    
                case BIOME_TUNDRA:
                    if (fast_randf(ctx->random_seed + world_x + world_y) > 0.998f) {
                        tile->feature = FEATURE_GLACIER;
                    }
                    break;
                    
                default:
                    break;
            }
        }
    }
}

// Get or load a chunk
world_chunk *
world_gen_get_chunk(world_gen_system *system, i32 chunk_x, i32 chunk_y) {
    if (!system || !system->initialized) return 0;
    
    // Check hash table first
    u32 hash = world_gen_hash_chunk_id(chunk_x, chunk_y);
    (void)hash; // Silence unused variable warning for now
    
    // Linear probe for collisions
    for (u32 i = 0; i < system->active_chunk_count; i++) {
        world_chunk *test_chunk = &system->active_chunks[i];
        if (test_chunk->chunk_x == chunk_x && test_chunk->chunk_y == chunk_y) {
            system->cache_hits++;
            test_chunk->last_access_time = system->total_chunks_generated;
            test_chunk->access_count++;
            return test_chunk;
        }
    }
    
    system->cache_misses++;
    
    // Generate new chunk if not found
    return world_gen_generate_chunk(system, chunk_x, chunk_y);
}

// Generate a new chunk
world_chunk *
world_gen_generate_chunk(world_gen_system *system, i32 chunk_x, i32 chunk_y) {
    if (!system || !system->initialized) return 0;
    
    // Check if we have space for more chunks
    if (system->active_chunk_count >= WORLD_MAX_ACTIVE_CHUNKS) {
        // TODO: Implement chunk eviction policy
        printf("[WORLD_GEN] Warning: Maximum active chunks reached\n");
        return 0;
    }
    
    clock_t start_time = clock();
    
    // Allocate new chunk
    world_chunk *chunk = &system->active_chunks[system->active_chunk_count++];
    memset(chunk, 0, sizeof(world_chunk));
    
    chunk->chunk_x = chunk_x;
    chunk->chunk_y = chunk_y;
    chunk->chunk_id = ((u64)chunk_x << 32) | (u64)chunk_y;
    chunk->last_access_time = system->total_chunks_generated;
    chunk->access_count = 1;
    
    // Set up generation context
    generation_context ctx;
    init_generation_context(&ctx, system, chunk, chunk_x, chunk_y);
    
    // Generate terrain
    generate_chunk_terrain(&ctx);
    chunk->generated = 1;
    
    // Generate biome blending
    generate_chunk_biome_blending(&ctx);
    
    // Generate terrain features
    generate_chunk_features(&ctx);
    
    // Resources will be generated separately
    chunk->resources_calculated = 0;
    chunk->structures_placed = 0;
    chunk->climate_calculated = 1; // Already done in terrain generation
    
    // Calculate generation time
    clock_t end_time = clock();
    chunk->generation_time_us = (u64)(((f64)(end_time - start_time) / CLOCKS_PER_SEC) * 1000000.0);
    
    // Update system statistics
    system->total_chunks_generated++;
    system->total_generation_time_us += chunk->generation_time_us;
    
    // Add to hash table
    u32 hash = world_gen_hash_chunk_id(chunk_x, chunk_y);
    system->chunk_hash[hash] = chunk;
    
    // Calculate resource richness
    chunk->resource_richness = 0.0f;
    f32 resource_noise = world_gen_fbm_noise(&system->resource_noise, 
                                           (f32)(chunk_x * WORLD_CHUNK_SIZE), 
                                           (f32)(chunk_y * WORLD_CHUNK_SIZE), 4);
    chunk->resource_richness = (resource_noise + 1.0f) * 0.5f; // 0-1 range
    
    printf("[WORLD_GEN] Generated chunk (%d,%d) in %.3f ms, resource richness: %.2f\n",
           chunk_x, chunk_y, (f32)chunk->generation_time_us / 1000.0f, chunk->resource_richness);
    
    return chunk;
}

// Unload a chunk from memory
void
world_gen_unload_chunk(world_gen_system *system, world_chunk *chunk) {
    if (!system || !chunk) return;
    
    // Remove from hash table
    u32 hash = world_gen_hash_chunk_id(chunk->chunk_x, chunk->chunk_y);
    if (system->chunk_hash[hash] == chunk) {
        system->chunk_hash[hash] = 0;
    }
    
    // Find chunk in active array and remove it
    for (u32 i = 0; i < system->active_chunk_count; i++) {
        if (&system->active_chunks[i] == chunk) {
            // Move last chunk to this position
            if (i < system->active_chunk_count - 1) {
                system->active_chunks[i] = system->active_chunks[system->active_chunk_count - 1];
                
                // Update hash table for moved chunk
                world_chunk *moved_chunk = &system->active_chunks[i];
                u32 moved_hash = world_gen_hash_chunk_id(moved_chunk->chunk_x, moved_chunk->chunk_y);
                system->chunk_hash[moved_hash] = moved_chunk;
            }
            
            system->active_chunk_count--;
            break;
        }
    }
    
    printf("[WORLD_GEN] Unloaded chunk (%d,%d)\n", chunk->chunk_x, chunk->chunk_y);
}

// Preload chunks around a center point
void
world_gen_preload_area(world_gen_system *system, i32 center_x, i32 center_y, i32 radius) {
    if (!system) return;
    
    i32 center_chunk_x = center_x / WORLD_CHUNK_SIZE;
    i32 center_chunk_y = center_y / WORLD_CHUNK_SIZE;
    
    printf("[WORLD_GEN] Preloading %dx%d chunk area around (%d,%d)\n", 
           radius * 2 + 1, radius * 2 + 1, center_chunk_x, center_chunk_y);
    
    u32 chunks_loaded = 0;
    clock_t start_time = clock();
    
    for (i32 dy = -radius; dy <= radius; dy++) {
        for (i32 dx = -radius; dx <= radius; dx++) {
            i32 chunk_x = center_chunk_x + dx;
            i32 chunk_y = center_chunk_y + dy;
            
            world_chunk *chunk = world_gen_get_chunk(system, chunk_x, chunk_y);
            if (chunk) {
                chunks_loaded++;
            }
        }
    }
    
    clock_t end_time = clock();
    f32 total_time = ((f32)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0f;
    
    printf("[WORLD_GEN] Preloaded %u chunks in %.2f ms (%.2f ms per chunk)\n",
           chunks_loaded, total_time, chunks_loaded > 0 ? total_time / chunks_loaded : 0.0f);
}

// Print detailed chunk information
void
world_gen_print_chunk_info(world_chunk *chunk) {
    if (!chunk) return;
    
    printf("\n=== Chunk Info (%d,%d) ===\n", chunk->chunk_x, chunk->chunk_y);
    printf("ID: %llu\n", (unsigned long long)chunk->chunk_id);
    printf("Generated: %s\n", chunk->generated ? "Yes" : "No");
    printf("Dominant biome: %d\n", chunk->dominant_biome);
    printf("Average elevation: %.1fm\n", chunk->average_elevation);
    printf("Average temperature: %.1f°C\n", chunk->average_temperature);
    printf("Resource richness: %.2f\n", chunk->resource_richness);
    printf("Generation time: %.3f ms\n", (f32)chunk->generation_time_us / 1000.0f);
    printf("Access count: %u\n", chunk->access_count);
    printf("Structures placed: %s\n", chunk->structures_placed ? "Yes" : "No");
    printf("Resources calculated: %s\n", chunk->resources_calculated ? "Yes" : "No");
    
    // Count features and biomes
    i32 feature_counts[16] = {0};
    i32 biome_counts[WORLD_BIOME_COUNT] = {0};
    
    for (i32 y = 0; y < WORLD_CHUNK_SIZE; y++) {
        for (i32 x = 0; x < WORLD_CHUNK_SIZE; x++) {
            world_tile *tile = &chunk->tiles[x][y];
            if (tile->feature < 16) feature_counts[tile->feature]++;
            if (tile->biome < WORLD_BIOME_COUNT) biome_counts[tile->biome]++;
        }
    }
    
    printf("Terrain features:\n");
    char *feature_names[] = {"None", "Hill", "Valley", "Cliff", "Cave", "River", 
                           "Lake", "Crater", "Ridge", "Plateau", "Canyon", 
                           "Sinkhole", "Geyser", "Hot Spring", "Oasis", "Glacier"};
    for (i32 i = 0; i < 16; i++) {
        if (feature_counts[i] > 0) {
            printf("  %s: %d tiles\n", feature_names[i], feature_counts[i]);
        }
    }
    
    printf("Biome distribution:\n");
    for (i32 i = 0; i < WORLD_BIOME_COUNT; i++) {
        if (biome_counts[i] > 0) {
            printf("  Biome %d: %d tiles (%.1f%%)\n", i, biome_counts[i], 
                   (f32)biome_counts[i] / (WORLD_CHUNK_SIZE * WORLD_CHUNK_SIZE) * 100.0f);
        }
    }
    
    printf("============================\n\n");
}