#ifndef HANDMADE_WORLD_GEN_H
#define HANDMADE_WORLD_GEN_H

/*
    Handmade Procedural World Generation
    Complete world generation system with zero external dependencies
    
    Features:
    - Infinite world generation using chunk-based system
    - Multiple biomes with realistic transitions
    - Layered noise generation for terrain variety
    - Resource distribution and cave systems
    - Climate simulation and weather patterns
    - Deterministic generation from seeds
    
    PERFORMANCE: Targets
    - Chunk generation: <16ms (60 FPS with 1 chunk/frame)
    - Memory usage: <512MB for active chunks
    - Cache efficiency: 95%+ chunk reuse
    - Terrain query: <1μs per sample
*/

#include "../../src/handmade.h"

// World generation constants
#define WORLD_GEN_MAGIC_NUMBER 0x57474548  // "HEWG" in hex
#define WORLD_GEN_VERSION 1
#define WORLD_CHUNK_SIZE 64
#define WORLD_CHUNK_HEIGHT 256
#define WORLD_MAX_ACTIVE_CHUNKS 64
#define WORLD_BIOME_COUNT 16
#define WORLD_NOISE_LAYERS 8
#define WORLD_STRUCTURE_COUNT 32
#define WORLD_RESOURCE_TYPES 16
#define WORLD_SEED_DEFAULT 12345

// Forward declarations
typedef struct achievement_system achievement_system;
typedef struct settings_system settings_system;

// World generation noise parameters
typedef struct noise_params {
    f32 frequency;
    f32 amplitude;
    i32 octaves;
    f32 lacunarity;
    f32 persistence;
    f32 offset_x;
    f32 offset_y;
    u32 seed;
} noise_params;

// Biome type enumeration
typedef enum biome_type {
    BIOME_OCEAN = 0,
    BIOME_BEACH,
    BIOME_GRASSLAND,
    BIOME_FOREST,
    BIOME_JUNGLE,
    BIOME_DESERT,
    BIOME_SAVANNA,
    BIOME_TAIGA,
    BIOME_TUNDRA,
    BIOME_SWAMP,
    BIOME_MOUNTAINS,
    BIOME_SNOW_MOUNTAINS,
    BIOME_VOLCANIC,
    BIOME_ICE_CAPS,
    BIOME_BADLANDS,
    BIOME_MUSHROOM_ISLAND
} biome_type;

// Terrain feature types
typedef enum terrain_feature {
    FEATURE_NONE = 0,
    FEATURE_HILL,
    FEATURE_VALLEY,
    FEATURE_CLIFF,
    FEATURE_CAVE_ENTRANCE,
    FEATURE_RIVER,
    FEATURE_LAKE,
    FEATURE_CRATER,
    FEATURE_RIDGE,
    FEATURE_PLATEAU,
    FEATURE_CANYON,
    FEATURE_SINKHOLE,
    FEATURE_GEYSER,
    FEATURE_HOT_SPRING,
    FEATURE_OASIS,
    FEATURE_GLACIER
} terrain_feature;

// Resource types for world generation
typedef enum resource_type {
    RESOURCE_NONE = 0,
    RESOURCE_STONE,
    RESOURCE_IRON,
    RESOURCE_COPPER,
    RESOURCE_GOLD,
    RESOURCE_DIAMOND,
    RESOURCE_COAL,
    RESOURCE_OIL,
    RESOURCE_WATER,
    RESOURCE_WOOD,
    RESOURCE_FOOD,
    RESOURCE_CRYSTAL,
    RESOURCE_RARE_EARTH,
    RESOURCE_URANIUM,
    RESOURCE_GEOTHERMAL,
    RESOURCE_MAGICAL
} resource_type;

// Climate data
typedef struct climate_data {
    f32 temperature;      // -40.0f to +50.0f Celsius
    f32 humidity;         // 0.0f to 1.0f
    f32 precipitation;    // 0.0f to 500.0f mm/month
    f32 wind_speed;       // 0.0f to 50.0f m/s
    f32 wind_direction;   // 0.0f to 360.0f degrees
    f32 elevation_factor; // Altitude modifier
    f32 ocean_distance;   // Distance to nearest ocean
} climate_data;

// Biome definition
typedef struct biome_definition {
    biome_type type;
    char name[64];
    char description[256];
    
    // Climate requirements
    f32 min_temperature;
    f32 max_temperature;
    f32 min_humidity;
    f32 max_humidity;
    f32 min_elevation;
    f32 max_elevation;
    
    // Visual properties
    u32 primary_color;
    u32 secondary_color;
    u32 grass_color;
    u32 foliage_color;
    
    // Generation parameters
    f32 terrain_roughness;
    f32 vegetation_density;
    f32 structure_frequency;
    f32 resource_abundance;
    
    // Common resources
    resource_type common_resources[8];
    f32 resource_weights[8];
    u32 resource_count;
    
    // Terrain features
    terrain_feature common_features[8];
    f32 feature_weights[8];
    u32 feature_count;
} biome_definition;

// Structure definition (trees, rocks, buildings, etc.)
typedef struct world_structure {
    u32 id;
    char name[64];
    biome_type preferred_biome;
    
    f32 spawn_probability;
    f32 cluster_size;
    f32 min_spacing;
    
    // Bounding box
    i32 width;
    i32 height; 
    i32 depth;
    
    // Resource yield
    resource_type yields[4];
    f32 yield_amounts[4];
    u32 yield_count;
    
    b32 blocks_movement;
    b32 provides_shelter;
    f32 durability;
} world_structure;

// Chunk tile data
typedef struct world_tile {
    f32 elevation;           // Height above sea level
    biome_type biome;        // Primary biome
    biome_type secondary_biome; // For biome transitions
    f32 biome_blend;         // 0.0f = primary, 1.0f = secondary
    
    climate_data climate;    // Local climate
    terrain_feature feature; // Special terrain feature
    
    // Resources
    resource_type resource;
    f32 resource_density;    // 0.0f to 1.0f
    f32 resource_quality;    // 0.0f to 1.0f
    
    // Structures
    u32 structure_id;        // 0 = no structure
    f32 structure_health;    // Current durability
    
    // Gameplay data
    b32 explored;
    b32 visible;
    f32 danger_level;        // For AI and spawning
    u64 last_update_time;
} world_tile;

// World chunk - 64x64 tiles
typedef struct world_chunk {
    i32 chunk_x;
    i32 chunk_y;
    u64 chunk_id;            // Unique identifier
    
    world_tile tiles[WORLD_CHUNK_SIZE][WORLD_CHUNK_SIZE];
    
    // Chunk metadata
    biome_type dominant_biome;
    f32 average_elevation;
    f32 average_temperature;
    f32 resource_richness;
    
    // Generation state
    b32 generated;
    b32 structures_placed;
    b32 resources_calculated;
    b32 climate_calculated;
    
    // Performance data
    u64 generation_time_us;  // Microseconds to generate
    u64 last_access_time;
    u32 access_count;
    
    // Neighbors (for seamless generation)
    struct world_chunk *neighbors[8]; // N, NE, E, SE, S, SW, W, NW
} world_chunk;

// Main world generation system
typedef struct world_gen_system {
    // Initialization
    b32 initialized;
    u64 world_seed;
    f32 world_scale;         // Affects feature sizes
    
    // Chunk management
    world_chunk active_chunks[WORLD_MAX_ACTIVE_CHUNKS];
    u32 active_chunk_count;
    world_chunk *chunk_hash[256]; // Hash table for quick lookup
    
    // Biome definitions
    biome_definition biomes[WORLD_BIOME_COUNT];
    u32 biome_count;
    
    // Structure definitions
    world_structure structures[WORLD_STRUCTURE_COUNT];
    u32 structure_count;
    
    // Noise generators (layered for complexity)
    noise_params elevation_noise;
    noise_params temperature_noise;
    noise_params humidity_noise;
    noise_params biome_noise;
    noise_params cave_noise;
    noise_params resource_noise;
    noise_params detail_noise[WORLD_NOISE_LAYERS];
    u32 detail_noise_count;
    
    // Generation settings
    f32 sea_level;
    f32 mountain_threshold;
    f32 cave_threshold;
    f32 river_threshold;
    f32 biome_blend_distance;
    
    // Climate simulation
    f32 global_temperature_offset;
    f32 seasonal_variation;
    f32 latitude_effect;
    f32 altitude_effect;
    
    // Performance monitoring
    u64 total_chunks_generated;
    u64 total_generation_time_us;
    u32 chunks_per_second;
    u32 cache_hits;
    u32 cache_misses;
    
    // Memory management
    u8 *memory;
    u32 memory_size;
    u32 memory_used;
} world_gen_system;

// Generation context (passed to generation functions)
typedef struct generation_context {
    world_gen_system *world_gen;
    world_chunk *chunk;
    i32 global_x;
    i32 global_y;
    u32 random_seed;
} generation_context;

// =============================================================================
// CORE API
// =============================================================================

// System management
world_gen_system *world_gen_init(void *memory, u32 memory_size, u64 seed);
void world_gen_shutdown(world_gen_system *system);
void world_gen_update(world_gen_system *system, f32 dt);

// Chunk management
world_chunk *world_gen_get_chunk(world_gen_system *system, i32 chunk_x, i32 chunk_y);
world_chunk *world_gen_generate_chunk(world_gen_system *system, i32 chunk_x, i32 chunk_y);
void world_gen_unload_chunk(world_gen_system *system, world_chunk *chunk);
void world_gen_preload_area(world_gen_system *system, i32 center_x, i32 center_y, i32 radius);

// Terrain queries
f32 world_gen_sample_elevation(world_gen_system *system, f32 world_x, f32 world_y);
biome_type world_gen_sample_biome(world_gen_system *system, f32 world_x, f32 world_y);
climate_data world_gen_sample_climate(world_gen_system *system, f32 world_x, f32 world_y);
world_tile *world_gen_get_tile(world_gen_system *system, i32 world_x, i32 world_y);

// Noise generation (Perlin/Simplex noise implementation)
f32 world_gen_noise_2d(noise_params *params, f32 x, f32 y);
f32 world_gen_noise_3d(noise_params *params, f32 x, f32 y, f32 z);
f32 world_gen_fbm_noise(noise_params *params, f32 x, f32 y, i32 octaves);
f32 world_gen_ridge_noise(noise_params *params, f32 x, f32 y);
f32 world_gen_turbulence_noise(noise_params *params, f32 x, f32 y);

// Biome system
void world_gen_register_biome(world_gen_system *system, biome_definition *biome);
biome_type world_gen_determine_biome(world_gen_system *system, f32 temperature, f32 humidity, f32 elevation);
void world_gen_blend_biomes(world_tile *tile, biome_definition *primary, biome_definition *secondary, f32 blend_factor);

// Structure placement
void world_gen_register_structure(world_gen_system *system, world_structure *structure);
void world_gen_place_structures(generation_context *ctx);
b32 world_gen_can_place_structure(generation_context *ctx, world_structure *structure, i32 x, i32 y);

// Resource distribution
void world_gen_distribute_resources(generation_context *ctx);
resource_type world_gen_determine_resource(generation_context *ctx, i32 x, i32 y);
f32 world_gen_calculate_resource_density(generation_context *ctx, resource_type resource, i32 x, i32 y);

// Cave and underground systems
void world_gen_generate_caves(generation_context *ctx);
b32 world_gen_is_cave(generation_context *ctx, i32 x, i32 y, i32 depth);
void world_gen_create_cave_network(generation_context *ctx, i32 start_x, i32 start_y);

// Climate simulation
climate_data world_gen_calculate_climate(world_gen_system *system, f32 world_x, f32 world_y, f32 elevation);
f32 world_gen_temperature_at_point(world_gen_system *system, f32 world_x, f32 world_y, f32 elevation);
f32 world_gen_humidity_at_point(world_gen_system *system, f32 world_x, f32 world_y);
f32 world_gen_precipitation_at_point(world_gen_system *system, f32 world_x, f32 world_y);

// Rivers and water systems
void world_gen_generate_rivers(generation_context *ctx);
void world_gen_create_river_path(generation_context *ctx, i32 start_x, i32 start_y, i32 target_x, i32 target_y);
void world_gen_place_lakes(generation_context *ctx);

// Utility functions
u32 world_gen_hash_chunk_id(i32 chunk_x, i32 chunk_y);
u64 world_gen_get_chunk_seed(world_gen_system *system, i32 chunk_x, i32 chunk_y);
f32 world_gen_interpolate_smooth(f32 a, f32 b, f32 t);
f32 world_gen_interpolate_cubic(f32 a, f32 b, f32 c, f32 d, f32 t);

// Debug and visualization
void world_gen_print_stats(world_gen_system *system);
void world_gen_print_chunk_info(world_chunk *chunk);
void world_gen_export_heightmap(world_gen_system *system, char *filename, i32 width, i32 height);
void world_gen_export_biome_map(world_gen_system *system, char *filename, i32 width, i32 height);

// Integration helpers
b32 world_gen_integrate_with_achievements(world_gen_system *world_gen, achievement_system *achievements);
b32 world_gen_integrate_with_settings(world_gen_system *world_gen, settings_system *settings);
void world_gen_trigger_exploration_achievements(world_gen_system *world_gen, achievement_system *achievements, world_tile *tile);

// Advanced features
void world_gen_simulate_erosion(world_gen_system *system, world_chunk *chunk, f32 time_factor);
void world_gen_simulate_tectonics(world_gen_system *system, f32 time_factor);
void world_gen_update_seasonal_changes(world_gen_system *system, f32 season_progress);

// Optimization functions
void world_gen_optimize_chunk_cache(world_gen_system *system);
void world_gen_compress_inactive_chunks(world_gen_system *system);
b32 world_gen_should_unload_chunk(world_gen_system *system, world_chunk *chunk, i32 player_x, i32 player_y);

#endif // HANDMADE_WORLD_GEN_H