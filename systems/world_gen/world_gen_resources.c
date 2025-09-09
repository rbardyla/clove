/*
    World Generation - Resource Distribution System
    Handles placement and distribution of resources across the world
*/

#include "handmade_world_gen.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define internal static

// Fast random number generator (declared in handmade_world_gen.c)
extern f32 fast_randf(u32 seed);

// Resource distribution patterns
typedef struct resource_pattern {
    resource_type type;
    f32 base_probability;    // Base spawn chance
    f32 cluster_factor;      // How much resources cluster together
    f32 depth_preference;    // Preferred depth (negative for underground)
    f32 biome_modifiers[WORLD_BIOME_COUNT]; // Multiplier per biome
    f32 elevation_min;       // Minimum elevation
    f32 elevation_max;       // Maximum elevation
    f32 temperature_min;     // Minimum temperature
    f32 temperature_max;     // Maximum temperature
    f32 rarity_multiplier;   // Overall rarity (lower = more rare)
} resource_pattern;

// Initialize resource distribution patterns
internal void
init_resource_patterns(resource_pattern *patterns) {
    u32 pattern_count = 0;
    
    // Stone - common everywhere
    resource_pattern *stone = &patterns[pattern_count++];
    stone->type = RESOURCE_STONE;
    stone->base_probability = 0.6f;
    stone->cluster_factor = 0.3f;
    stone->depth_preference = 0.0f;
    stone->elevation_min = -1000.0f;
    stone->elevation_max = 3000.0f;
    stone->temperature_min = -50.0f;
    stone->temperature_max = 60.0f;
    stone->rarity_multiplier = 1.0f;
    // Higher in mountains, lower in water
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) stone->biome_modifiers[i] = 1.0f;
    stone->biome_modifiers[BIOME_MOUNTAINS] = 2.0f;
    stone->biome_modifiers[BIOME_SNOW_MOUNTAINS] = 2.2f;
    stone->biome_modifiers[BIOME_OCEAN] = 0.1f;
    stone->biome_modifiers[BIOME_SWAMP] = 0.7f;
    
    // Iron - common in mountains and hills
    resource_pattern *iron = &patterns[pattern_count++];
    iron->type = RESOURCE_IRON;
    iron->base_probability = 0.3f;
    iron->cluster_factor = 0.8f;
    iron->depth_preference = -10.0f; // Prefers underground
    iron->elevation_min = -100.0f;
    iron->elevation_max = 2000.0f;
    iron->temperature_min = -30.0f;
    iron->temperature_max = 40.0f;
    iron->rarity_multiplier = 0.7f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) iron->biome_modifiers[i] = 0.5f;
    iron->biome_modifiers[BIOME_MOUNTAINS] = 3.0f;
    iron->biome_modifiers[BIOME_SNOW_MOUNTAINS] = 2.5f;
    iron->biome_modifiers[BIOME_BADLANDS] = 2.0f;
    iron->biome_modifiers[BIOME_OCEAN] = 0.0f;
    
    // Gold - rare, deep, in specific biomes
    resource_pattern *gold = &patterns[pattern_count++];
    gold->type = RESOURCE_GOLD;
    gold->base_probability = 0.05f;
    gold->cluster_factor = 0.9f;
    gold->depth_preference = -50.0f;
    gold->elevation_min = -200.0f;
    gold->elevation_max = 2500.0f;
    gold->temperature_min = -20.0f;
    gold->temperature_max = 50.0f;
    gold->rarity_multiplier = 0.2f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) gold->biome_modifiers[i] = 0.1f;
    gold->biome_modifiers[BIOME_MOUNTAINS] = 5.0f;
    gold->biome_modifiers[BIOME_DESERT] = 3.0f;
    gold->biome_modifiers[BIOME_BADLANDS] = 4.0f;
    gold->biome_modifiers[BIOME_VOLCANIC] = 2.0f;
    
    // Diamond - very rare, deep mountains
    resource_pattern *diamond = &patterns[pattern_count++];
    diamond->type = RESOURCE_DIAMOND;
    diamond->base_probability = 0.01f;
    diamond->cluster_factor = 0.95f;
    diamond->depth_preference = -100.0f;
    diamond->elevation_min = 800.0f;
    diamond->elevation_max = 3000.0f;
    diamond->temperature_min = -40.0f;
    diamond->temperature_max = 20.0f;
    diamond->rarity_multiplier = 0.05f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) diamond->biome_modifiers[i] = 0.0f;
    diamond->biome_modifiers[BIOME_MOUNTAINS] = 8.0f;
    diamond->biome_modifiers[BIOME_SNOW_MOUNTAINS] = 10.0f;
    diamond->biome_modifiers[BIOME_VOLCANIC] = 1.0f;
    
    // Coal - common sedimentary deposits
    resource_pattern *coal = &patterns[pattern_count++];
    coal->type = RESOURCE_COAL;
    coal->base_probability = 0.4f;
    coal->cluster_factor = 0.7f;
    coal->depth_preference = -20.0f;
    coal->elevation_min = -50.0f;
    coal->elevation_max = 1000.0f;
    coal->temperature_min = -20.0f;
    coal->temperature_max = 35.0f;
    coal->rarity_multiplier = 0.8f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) coal->biome_modifiers[i] = 1.0f;
    coal->biome_modifiers[BIOME_SWAMP] = 3.0f; // Ancient swamps = coal
    coal->biome_modifiers[BIOME_FOREST] = 2.0f;
    coal->biome_modifiers[BIOME_JUNGLE] = 1.5f;
    coal->biome_modifiers[BIOME_DESERT] = 0.3f;
    coal->biome_modifiers[BIOME_OCEAN] = 0.0f;
    
    // Water - everywhere but concentrated in specific areas
    resource_pattern *water = &patterns[pattern_count++];
    water->type = RESOURCE_WATER;
    water->base_probability = 0.8f;
    water->cluster_factor = 0.4f;
    water->depth_preference = 5.0f; // Surface preference
    water->elevation_min = -1000.0f;
    water->elevation_max = 2000.0f;
    water->temperature_min = -30.0f;
    water->temperature_max = 60.0f;
    water->rarity_multiplier = 1.2f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) water->biome_modifiers[i] = 1.0f;
    water->biome_modifiers[BIOME_OCEAN] = 5.0f;
    water->biome_modifiers[BIOME_SWAMP] = 4.0f;
    water->biome_modifiers[BIOME_JUNGLE] = 2.0f;
    water->biome_modifiers[BIOME_DESERT] = 0.1f;
    water->biome_modifiers[BIOME_TUNDRA] = 3.0f; // Ice = water
    
    // Wood - in forests and jungles
    resource_pattern *wood = &patterns[pattern_count++];
    wood->type = RESOURCE_WOOD;
    wood->base_probability = 0.7f;
    wood->cluster_factor = 0.6f;
    wood->depth_preference = 10.0f; // Surface trees
    wood->elevation_min = -10.0f;
    wood->elevation_max = 1500.0f;
    wood->temperature_min = -10.0f;
    wood->temperature_max = 40.0f;
    wood->rarity_multiplier = 1.0f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) wood->biome_modifiers[i] = 0.1f;
    wood->biome_modifiers[BIOME_FOREST] = 5.0f;
    wood->biome_modifiers[BIOME_JUNGLE] = 6.0f;
    wood->biome_modifiers[BIOME_TAIGA] = 4.0f;
    wood->biome_modifiers[BIOME_GRASSLAND] = 1.0f;
    wood->biome_modifiers[BIOME_SAVANNA] = 0.8f;
    
    // Food - vegetation and animals
    resource_pattern *food = &patterns[pattern_count++];
    food->type = RESOURCE_FOOD;
    food->base_probability = 0.5f;
    food->cluster_factor = 0.4f;
    food->depth_preference = 5.0f;
    food->elevation_min = -50.0f;
    food->elevation_max = 1200.0f;
    food->temperature_min = -5.0f;
    food->temperature_max = 45.0f;
    food->rarity_multiplier = 1.0f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) food->biome_modifiers[i] = 1.0f;
    food->biome_modifiers[BIOME_GRASSLAND] = 3.0f;
    food->biome_modifiers[BIOME_FOREST] = 2.5f;
    food->biome_modifiers[BIOME_JUNGLE] = 4.0f;
    food->biome_modifiers[BIOME_SAVANNA] = 2.0f;
    food->biome_modifiers[BIOME_OCEAN] = 1.5f; // Fish
    food->biome_modifiers[BIOME_DESERT] = 0.2f;
    food->biome_modifiers[BIOME_TUNDRA] = 0.3f;
    
    // Oil - sedimentary basins and ocean
    resource_pattern *oil = &patterns[pattern_count++];
    oil->type = RESOURCE_OIL;
    oil->base_probability = 0.1f;
    oil->cluster_factor = 0.9f;
    oil->depth_preference = -80.0f; // Deep underground
    oil->elevation_min = -500.0f;
    oil->elevation_max = 200.0f;
    oil->temperature_min = 10.0f;
    oil->temperature_max = 60.0f;
    oil->rarity_multiplier = 0.3f;
    for (u32 i = 0; i < WORLD_BIOME_COUNT; i++) oil->biome_modifiers[i] = 0.5f;
    oil->biome_modifiers[BIOME_DESERT] = 4.0f; // Middle East style
    oil->biome_modifiers[BIOME_OCEAN] = 3.0f; // Offshore drilling
    oil->biome_modifiers[BIOME_SWAMP] = 2.0f; // Ancient organic matter
    oil->biome_modifiers[BIOME_GRASSLAND] = 1.5f;
    oil->biome_modifiers[BIOME_MOUNTAINS] = 0.2f;
}

// Calculate resource spawn probability at a specific location
internal f32
calculate_resource_probability(resource_pattern *pattern, world_tile *tile, 
                             f32 world_x, f32 world_y, world_gen_system *system) {
    
    // Base probability
    f32 probability = pattern->base_probability;
    
    // Biome modifier
    if (tile->biome < WORLD_BIOME_COUNT) {
        probability *= pattern->biome_modifiers[tile->biome];
    }
    
    // Elevation check
    if (tile->elevation < pattern->elevation_min || tile->elevation > pattern->elevation_max) {
        probability *= 0.1f; // Heavily penalize out-of-range elevations
    }
    
    // Temperature check
    if (tile->climate.temperature < pattern->temperature_min || 
        tile->climate.temperature > pattern->temperature_max) {
        probability *= 0.3f; // Penalize temperature mismatches
    }
    
    // Depth preference (positive = surface, negative = underground)
    f32 depth_factor = 1.0f;
    if (pattern->depth_preference > 0) {
        // Surface resource - penalize if too deep
        depth_factor = fmaxf(0.1f, 1.0f - (fabsf(pattern->depth_preference) * 0.01f));
    } else {
        // Underground resource - bonus for depth
        depth_factor = 1.0f + (fabsf(pattern->depth_preference) * 0.005f);
    }
    probability *= depth_factor;
    
    // Clustering - resources near similar resources get bonus
    f32 cluster_noise = world_gen_noise_2d(&system->resource_noise, 
                                          world_x * pattern->cluster_factor, 
                                          world_y * pattern->cluster_factor);
    cluster_noise = (cluster_noise + 1.0f) * 0.5f; // 0-1 range
    probability *= (0.5f + cluster_noise * pattern->cluster_factor);
    
    // Rarity multiplier
    probability *= pattern->rarity_multiplier;
    
    // Resource richness from chunk
    // (This would be calculated elsewhere and stored in chunk)
    
    return fmaxf(0.0f, fminf(1.0f, probability));
}

// Determine what resource (if any) spawns at a location
resource_type
world_gen_determine_resource(generation_context *ctx, i32 x, i32 y) {
    world_tile *tile = &ctx->chunk->tiles[x][y];
    i32 world_x = ctx->global_x + x;
    i32 world_y = ctx->global_y + y;
    
    // Initialize resource patterns
    resource_pattern patterns[WORLD_RESOURCE_TYPES];
    init_resource_patterns(patterns);
    
    // Use deterministic random based on world position
    u32 seed = ctx->random_seed + (u32)world_x * 73856093 + (u32)world_y * 19349663;
    f32 random_value = fast_randf(seed);
    
    // Try each resource type
    f32 cumulative_probability = 0.0f;
    
    for (u32 i = 1; i < WORLD_RESOURCE_TYPES; i++) { // Skip RESOURCE_NONE
        resource_pattern *pattern = &patterns[i-1];
        
        f32 probability = calculate_resource_probability(pattern, tile, 
                                                       (f32)world_x, (f32)world_y, 
                                                       ctx->world_gen);
        
        cumulative_probability += probability * 0.1f; // Scale down probabilities
        
        if (random_value < cumulative_probability) {
            return pattern->type;
        }
    }
    
    return RESOURCE_NONE;
}

// Calculate resource density at a location
f32
world_gen_calculate_resource_density(generation_context *ctx, resource_type resource, i32 x, i32 y) {
    if (resource == RESOURCE_NONE) return 0.0f;
    
    world_tile *tile = &ctx->chunk->tiles[x][y];
    i32 world_x = ctx->global_x + x;
    i32 world_y = ctx->global_y + y;
    
    // Base density from noise
    f32 density_noise = world_gen_fbm_noise(&ctx->world_gen->resource_noise, 
                                           (f32)world_x * 0.1f, (f32)world_y * 0.1f, 3);
    f32 base_density = (density_noise + 1.0f) * 0.5f; // 0-1 range
    
    // Modify based on biome and elevation
    f32 biome_modifier = 1.0f;
    switch (resource) {
        case RESOURCE_STONE:
            biome_modifier = tile->biome == BIOME_MOUNTAINS ? 1.5f : 1.0f;
            break;
        case RESOURCE_IRON:
        case RESOURCE_GOLD:
        case RESOURCE_DIAMOND:
            biome_modifier = tile->elevation > 500.0f ? 1.3f : 0.8f;
            break;
        case RESOURCE_WOOD:
            biome_modifier = (tile->biome == BIOME_FOREST || tile->biome == BIOME_JUNGLE) ? 1.5f : 0.5f;
            break;
        case RESOURCE_WATER:
            biome_modifier = tile->climate.humidity;
            break;
        case RESOURCE_OIL:
            biome_modifier = tile->elevation < 100.0f ? 1.2f : 0.6f;
            break;
        default:
            biome_modifier = 1.0f;
            break;
    }
    
    f32 final_density = base_density * biome_modifier * ctx->chunk->resource_richness;
    return fmaxf(0.0f, fminf(1.0f, final_density));
}

// Distribute resources across a chunk
void
world_gen_distribute_resources(generation_context *ctx) {
    world_chunk *chunk = ctx->chunk;
    
    u32 resources_placed = 0;
    f32 total_density = 0.0f;
    
    // Process each tile
    for (i32 y = 0; y < WORLD_CHUNK_SIZE; y++) {
        for (i32 x = 0; x < WORLD_CHUNK_SIZE; x++) {
            world_tile *tile = &chunk->tiles[x][y];
            
            // Determine resource type
            resource_type resource = world_gen_determine_resource(ctx, x, y);
            tile->resource = resource;
            
            if (resource != RESOURCE_NONE) {
                // Calculate density and quality
                tile->resource_density = world_gen_calculate_resource_density(ctx, resource, x, y);
                
                // Quality is partially random, partially based on depth/biome
                u32 quality_seed = ctx->random_seed + x + y * WORLD_CHUNK_SIZE + (u32)resource * 1000;
                f32 base_quality = fast_randf(quality_seed);
                
                // Deeper/rarer resources tend to be higher quality
                f32 depth_bonus = 0.0f;
                if (tile->elevation > 1000.0f) depth_bonus += 0.2f; // High altitude bonus
                if (resource == RESOURCE_DIAMOND || resource == RESOURCE_GOLD) depth_bonus += 0.3f;
                
                tile->resource_quality = fmaxf(0.0f, fminf(1.0f, base_quality + depth_bonus));
                
                resources_placed++;
                total_density += tile->resource_density;
            } else {
                tile->resource_density = 0.0f;
                tile->resource_quality = 0.0f;
            }
        }
    }
    
    chunk->resources_calculated = 1;
    
    printf("[WORLD_GEN] Distributed %u resources in chunk (%d,%d), avg density: %.3f\n",
           resources_placed, chunk->chunk_x, chunk->chunk_y, 
           resources_placed > 0 ? total_density / resources_placed : 0.0f);
}