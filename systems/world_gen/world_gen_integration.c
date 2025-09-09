/*
    World Generation Integration
    Bridges World Generation with Achievement and Settings systems
*/

#include "handmade_world_gen.h"
#include "../achievements/handmade_achievements.h"
#include "../settings/handmade_settings.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define internal static

// Integrate World Generation with Achievement system
b32
world_gen_integrate_with_achievements(world_gen_system *world_gen, achievement_system *achievements) {
    if (!world_gen || !achievements || !world_gen->initialized) return 0;
    
    printf("[WORLD_GEN] Integrating with Achievement system...\n");
    
    // Register world generation specific statistics
    achievements_register_stat(achievements, "biomes_discovered", "Biomes discovered", STAT_INT);
    achievements_register_stat(achievements, "resources_found", "Resources found", STAT_INT);
    achievements_register_stat(achievements, "features_discovered", "Terrain features discovered", STAT_INT);
    achievements_register_stat(achievements, "world_chunks_explored", "World chunks explored", STAT_INT);
    achievements_register_stat(achievements, "distance_traveled", "Distance traveled (meters)", STAT_FLOAT);
    achievements_register_stat(achievements, "highest_elevation", "Highest elevation reached", STAT_FLOAT);
    achievements_register_stat(achievements, "lowest_depth", "Lowest depth reached", STAT_FLOAT);
    achievements_register_stat(achievements, "caves_explored", "Caves explored", STAT_INT);
    achievements_register_stat(achievements, "rare_resources_found", "Rare resources found", STAT_INT);
    
    // Register exploration achievements using available functions
    achievements_register_counter(achievements, "world_explorer", "World Explorer", 
                                 "Discover 5 different biomes", CATEGORY_EXPLORATION, 
                                 "biomes_discovered", 5);
    
    achievements_register_progress(achievements, "world_walker", "World Walker", 
                                  "Travel 10km from spawn", CATEGORY_EXPLORATION, 
                                  "distance_traveled", 10000.0f);
    
    achievements_register_progress(achievements, "mountaineer", "Mountaineer", 
                                  "Reach an elevation above 2000m", CATEGORY_EXPLORATION, 
                                  "highest_elevation", 2000.0f);
    
    achievements_register_progress(achievements, "deep_diver", "Deep Diver", 
                                  "Explore depths below -100m", CATEGORY_EXPLORATION, 
                                  "lowest_depth", -100.0f);
    
    achievements_register_counter(achievements, "resource_hunter", "Resource Hunter", 
                                 "Find 50 resource deposits", CATEGORY_EXPLORATION, 
                                 "resources_found", 50);
    
    achievements_register_counter(achievements, "cave_explorer", "Cave Explorer", 
                                 "Explore 10 different caves", CATEGORY_EXPLORATION, 
                                 "caves_explored", 10);
    
    achievements_register_counter(achievements, "treasure_finder", "Treasure Finder", 
                                 "Find 5 rare resources", CATEGORY_EXPLORATION, 
                                 "rare_resources_found", 5);
    
    achievements_register_counter(achievements, "cartographer", "Cartographer", 
                                 "Explore 100 different chunks", CATEGORY_EXPLORATION, 
                                 "world_chunks_explored", 100);
    
    printf("[WORLD_GEN] Achievement integration complete\n");
    return 1;
}

// Integrate World Generation with Settings system
b32
world_gen_integrate_with_settings(world_gen_system *world_gen, settings_system *settings) {
    if (!world_gen || !settings || !world_gen->initialized) return 0;
    
    printf("[WORLD_GEN] Integrating with Settings system...\n");
    
    // Register world generation settings
    settings_register_int(settings, "world_render_distance", "Chunk render distance",
                         CATEGORY_GRAPHICS, 8, 4, 16, 0);
    
    settings_register_float(settings, "world_detail_scale", "World detail scale",
                           CATEGORY_GRAPHICS, world_gen->world_scale, 0.5f, 2.0f, 0);
    
    settings_register_bool(settings, "world_show_biome_borders", "Show biome borders",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_bool(settings, "world_show_chunk_borders", "Show chunk borders",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_bool(settings, "world_show_elevation_colors", "Color by elevation",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_bool(settings, "world_show_resource_overlay", "Show resource overlay",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_float(settings, "world_sea_level", "Sea level height",
                           CATEGORY_GAMEPLAY, world_gen->sea_level, -100.0f, 100.0f, SETTING_ADVANCED);
    
    settings_register_float(settings, "world_mountain_threshold", "Mountain height threshold",
                           CATEGORY_GAMEPLAY, world_gen->mountain_threshold, 200.0f, 1000.0f, SETTING_ADVANCED);
    
    settings_register_float(settings, "world_temperature_offset", "Global temperature offset",
                           CATEGORY_GAMEPLAY, world_gen->global_temperature_offset, -20.0f, 40.0f, SETTING_ADVANCED);
    
    settings_register_int(settings, "world_max_chunks", "Maximum cached chunks",
                         CATEGORY_GRAPHICS, WORLD_MAX_ACTIVE_CHUNKS, 16, 128, SETTING_ADVANCED);
    
    printf("[WORLD_GEN] Settings integration complete\n");
    return 1;
}

// Trigger exploration achievements when discovering new areas
void
world_gen_trigger_exploration_achievements(world_gen_system *world_gen, achievement_system *achievements, world_tile *tile) {
    if (!world_gen || !achievements || !tile) return;
    
    // Track biome discovery
    static b32 biomes_seen[WORLD_BIOME_COUNT] = {0};
    if (!biomes_seen[tile->biome]) {
        biomes_seen[tile->biome] = 1;
        achievements_add_stat_int(achievements, "biomes_discovered", 1);
        printf("[WORLD_GEN] New biome discovered: %d\n", tile->biome);
    }
    
    // Track feature discovery
    if (tile->feature != FEATURE_NONE) {
        achievements_add_stat_int(achievements, "features_discovered", 1);
        
        if (tile->feature == FEATURE_CAVE_ENTRANCE) {
            achievements_add_stat_int(achievements, "caves_explored", 1);
        }
    }
    
    // Track resource discovery
    if (tile->resource != RESOURCE_NONE) {
        achievements_add_stat_int(achievements, "resources_found", 1);
        
        // Rare resources
        if (tile->resource == RESOURCE_DIAMOND || 
            tile->resource == RESOURCE_GOLD ||
            tile->resource == RESOURCE_URANIUM ||
            tile->resource == RESOURCE_MAGICAL) {
            achievements_add_stat_int(achievements, "rare_resources_found", 1);
            printf("[WORLD_GEN] Rare resource discovered: %d\n", tile->resource);
        }
    }
    
    // Track elevation records
    f32 current_highest = achievements_get_stat_float(achievements, "highest_elevation");
    if (tile->elevation > current_highest) {
        achievements_set_stat_float(achievements, "highest_elevation", tile->elevation);
        printf("[WORLD_GEN] New elevation record: %.1fm\n", tile->elevation);
    }
    
    f32 current_lowest = achievements_get_stat_float(achievements, "lowest_depth");
    if (tile->elevation < current_lowest) {
        achievements_set_stat_float(achievements, "lowest_depth", tile->elevation);
        printf("[WORLD_GEN] New depth record: %.1fm\n", tile->elevation);
    }
}

// Update world generation settings from settings system
internal void
world_gen_apply_settings(world_gen_system *world_gen, settings_system *settings) {
    if (!world_gen || !settings) return;
    
    // Apply world generation settings
    world_gen->world_scale = settings_get_float(settings, "world_detail_scale");
    world_gen->sea_level = settings_get_float(settings, "world_sea_level");
    world_gen->mountain_threshold = settings_get_float(settings, "world_mountain_threshold");
    world_gen->global_temperature_offset = settings_get_float(settings, "world_temperature_offset");
}

// Export heightmap for external visualization
void
world_gen_export_heightmap(world_gen_system *system, char *filename, i32 width, i32 height) {
    if (!system || !filename) return;
    
    printf("[WORLD_GEN] Exporting heightmap %dx%d to %s\n", width, height, filename);
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("[WORLD_GEN] Failed to create heightmap file\n");
        return;
    }
    
    // Simple PGM format header
    fprintf(file, "P2\n%d %d\n255\n", width, height);
    
    f32 center_x = (f32)width * 0.5f * 10.0f;  // Scale factor
    f32 center_y = (f32)height * 0.5f * 10.0f;
    
    for (i32 y = 0; y < height; y++) {
        for (i32 x = 0; x < width; x++) {
            f32 world_x = ((f32)x - center_x);
            f32 world_y = ((f32)y - center_y);
            
            f32 elevation = world_gen_sample_elevation(system, world_x, world_y);
            
            // Normalize elevation to 0-255 range
            i32 gray_value = (i32)((elevation + 200.0f) * 0.5f); // Assume -200 to 300 range
            gray_value = fmaxf(0, fminf(255, gray_value));
            
            fprintf(file, "%d ", gray_value);
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("[WORLD_GEN] Heightmap exported successfully\n");
}

// Export biome map for external visualization
void
world_gen_export_biome_map(world_gen_system *system, char *filename, i32 width, i32 height) {
    if (!system || !filename) return;
    
    printf("[WORLD_GEN] Exporting biome map %dx%d to %s\n", width, height, filename);
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("[WORLD_GEN] Failed to create biome map file\n");
        return;
    }
    
    // Simple text format
    fprintf(file, "Biome Map %dx%d\n", width, height);
    
    f32 center_x = (f32)width * 0.5f * 10.0f;
    f32 center_y = (f32)height * 0.5f * 10.0f;
    
    for (i32 y = 0; y < height; y++) {
        for (i32 x = 0; x < width; x++) {
            f32 world_x = ((f32)x - center_x);
            f32 world_y = ((f32)y - center_y);
            
            biome_type biome = world_gen_sample_biome(system, world_x, world_y);
            fprintf(file, "%d ", biome);
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    printf("[WORLD_GEN] Biome map exported successfully\n");
}

// Auto-save world generation settings to file
internal b32
world_gen_save_settings(world_gen_system *world_gen, char *filename) {
    if (!world_gen || !filename) return 0;
    
    FILE *file = fopen(filename, "w");
    if (!file) return 0;
    
    fprintf(file, "# Handmade World Generation Settings\n");
    fprintf(file, "world_seed=%llu\n", (unsigned long long)world_gen->world_seed);
    fprintf(file, "world_scale=%.3f\n", world_gen->world_scale);
    fprintf(file, "sea_level=%.1f\n", world_gen->sea_level);
    fprintf(file, "mountain_threshold=%.1f\n", world_gen->mountain_threshold);
    fprintf(file, "cave_threshold=%.3f\n", world_gen->cave_threshold);
    fprintf(file, "river_threshold=%.3f\n", world_gen->river_threshold);
    fprintf(file, "global_temperature_offset=%.1f\n", world_gen->global_temperature_offset);
    fprintf(file, "seasonal_variation=%.1f\n", world_gen->seasonal_variation);
    fprintf(file, "latitude_effect=%.3f\n", world_gen->latitude_effect);
    fprintf(file, "altitude_effect=%.6f\n", world_gen->altitude_effect);
    
    fclose(file);
    printf("[WORLD_GEN] Settings saved to %s\n", filename);
    return 1;
}

// Load world generation settings from file
internal b32
world_gen_load_settings(world_gen_system *world_gen, char *filename) {
    if (!world_gen || !filename) return 0;
    
    FILE *file = fopen(filename, "r");
    if (!file) return 0;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue; // Skip comments
        
        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            if (strcmp(key, "world_seed") == 0) {
                world_gen->world_seed = (u64)strtoull(value, NULL, 10);
            } else if (strcmp(key, "world_scale") == 0) {
                world_gen->world_scale = (f32)atof(value);
            } else if (strcmp(key, "sea_level") == 0) {
                world_gen->sea_level = (f32)atof(value);
            } else if (strcmp(key, "mountain_threshold") == 0) {
                world_gen->mountain_threshold = (f32)atof(value);
            } else if (strcmp(key, "cave_threshold") == 0) {
                world_gen->cave_threshold = (f32)atof(value);
            } else if (strcmp(key, "river_threshold") == 0) {
                world_gen->river_threshold = (f32)atof(value);
            } else if (strcmp(key, "global_temperature_offset") == 0) {
                world_gen->global_temperature_offset = (f32)atof(value);
            }
        }
    }
    
    fclose(file);
    printf("[WORLD_GEN] Settings loaded from %s\n", filename);
    return 1;
}