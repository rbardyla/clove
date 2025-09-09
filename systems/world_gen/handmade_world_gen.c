/*
    Handmade Procedural World Generation - Core Implementation
    Complete world generation system with Perlin noise, biomes, and chunk management
*/

#include "handmade_world_gen.h"
#include "../../src/handmade.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define internal static
#define PI 3.14159265359f

// Permutation table for Perlin noise
internal u8 permutation[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    // Repeat the pattern
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

// Fast random number generator state
internal u32 rng_state = 1;

internal u32
fast_rand(u32 seed) {
    rng_state = seed;
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

f32
fast_randf(u32 seed) {
    return (f32)fast_rand(seed) / (f32)0xFFFFFFFF;
}

// Interpolation functions
internal f32
smooth_step(f32 t) {
    return t * t * (3.0f - 2.0f * t);
}

internal f32
smoother_step(f32 t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

f32
world_gen_interpolate_smooth(f32 a, f32 b, f32 t) {
    return a + smooth_step(t) * (b - a);
}

f32
world_gen_interpolate_cubic(f32 a, f32 b, f32 c, f32 d, f32 t) {
    f32 p = (d - c) - (a - b);
    f32 q = (a - b) - p;
    f32 r = c - a;
    f32 s = b;
    return p*t*t*t + q*t*t + r*t + s;
}

// Gradient function for Perlin noise
internal f32
gradient_2d(i32 hash, f32 x, f32 y) {
    i32 h = hash & 15;
    f32 u = h < 8 ? x : y;
    f32 v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

internal f32
gradient_3d(i32 hash, f32 x, f32 y, f32 z) {
    i32 h = hash & 15;
    f32 u = h < 8 ? x : y;
    f32 v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

// Core Perlin noise implementation
f32
world_gen_noise_2d(noise_params *params, f32 x, f32 y) {
    // Apply frequency and offset
    x = (x + params->offset_x) * params->frequency;
    y = (y + params->offset_y) * params->frequency;
    
    // Get integer coordinates
    i32 x0 = (i32)floorf(x);
    i32 y0 = (i32)floorf(y);
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;
    
    // Get fractional part
    f32 dx = x - x0;
    f32 dy = y - y0;
    
    // Hash coordinates
    i32 h00 = permutation[permutation[x0 & 255] + (y0 & 255)];
    i32 h10 = permutation[permutation[x1 & 255] + (y0 & 255)];
    i32 h01 = permutation[permutation[x0 & 255] + (y1 & 255)];
    i32 h11 = permutation[permutation[x1 & 255] + (y1 & 255)];
    
    // Calculate gradients
    f32 g00 = gradient_2d(h00, dx, dy);
    f32 g10 = gradient_2d(h10, dx - 1, dy);
    f32 g01 = gradient_2d(h01, dx, dy - 1);
    f32 g11 = gradient_2d(h11, dx - 1, dy - 1);
    
    // Interpolate
    f32 u = smoother_step(dx);
    f32 v = smoother_step(dy);
    
    f32 nx0 = g00 + u * (g10 - g00);
    f32 nx1 = g01 + u * (g11 - g01);
    f32 result = nx0 + v * (nx1 - nx0);
    
    return result * params->amplitude;
}

f32
world_gen_noise_3d(noise_params *params, f32 x, f32 y, f32 z) {
    // Apply frequency and offset
    x = (x + params->offset_x) * params->frequency;
    y = (y + params->offset_y) * params->frequency;
    z = z * params->frequency;
    
    // Get integer coordinates
    i32 x0 = (i32)floorf(x); i32 x1 = x0 + 1;
    i32 y0 = (i32)floorf(y); i32 y1 = y0 + 1;
    i32 z0 = (i32)floorf(z); i32 z1 = z0 + 1;
    
    // Get fractional part
    f32 dx = x - x0; f32 dy = y - y0; f32 dz = z - z0;
    
    // Hash coordinates
    i32 h000 = permutation[permutation[permutation[x0 & 255] + (y0 & 255)] + (z0 & 255)];
    i32 h100 = permutation[permutation[permutation[x1 & 255] + (y0 & 255)] + (z0 & 255)];
    i32 h010 = permutation[permutation[permutation[x0 & 255] + (y1 & 255)] + (z0 & 255)];
    i32 h110 = permutation[permutation[permutation[x1 & 255] + (y1 & 255)] + (z0 & 255)];
    i32 h001 = permutation[permutation[permutation[x0 & 255] + (y0 & 255)] + (z1 & 255)];
    i32 h101 = permutation[permutation[permutation[x1 & 255] + (y0 & 255)] + (z1 & 255)];
    i32 h011 = permutation[permutation[permutation[x0 & 255] + (y1 & 255)] + (z1 & 255)];
    i32 h111 = permutation[permutation[permutation[x1 & 255] + (y1 & 255)] + (z1 & 255)];
    
    // Calculate gradients
    f32 g000 = gradient_3d(h000, dx,     dy,     dz);
    f32 g100 = gradient_3d(h100, dx - 1, dy,     dz);
    f32 g010 = gradient_3d(h010, dx,     dy - 1, dz);
    f32 g110 = gradient_3d(h110, dx - 1, dy - 1, dz);
    f32 g001 = gradient_3d(h001, dx,     dy,     dz - 1);
    f32 g101 = gradient_3d(h101, dx - 1, dy,     dz - 1);
    f32 g011 = gradient_3d(h011, dx,     dy - 1, dz - 1);
    f32 g111 = gradient_3d(h111, dx - 1, dy - 1, dz - 1);
    
    // Interpolate
    f32 u = smoother_step(dx);
    f32 v = smoother_step(dy);
    f32 w = smoother_step(dz);
    
    f32 nx00 = g000 + u * (g100 - g000);
    f32 nx10 = g010 + u * (g110 - g010);
    f32 nx01 = g001 + u * (g101 - g001);
    f32 nx11 = g011 + u * (g111 - g011);
    
    f32 nxy0 = nx00 + v * (nx10 - nx00);
    f32 nxy1 = nx01 + v * (nx11 - nx01);
    
    f32 result = nxy0 + w * (nxy1 - nxy0);
    
    return result * params->amplitude;
}

// Fractal Brownian Motion (FBM) noise
f32
world_gen_fbm_noise(noise_params *params, f32 x, f32 y, i32 octaves) {
    f32 result = 0.0f;
    f32 amplitude = params->amplitude;
    f32 frequency = params->frequency;
    f32 max_value = 0.0f;
    
    noise_params octave_params = *params;
    
    for (i32 i = 0; i < octaves; i++) {
        octave_params.frequency = frequency;
        octave_params.amplitude = amplitude;
        
        result += world_gen_noise_2d(&octave_params, x, y);
        max_value += amplitude;
        
        amplitude *= params->persistence;
        frequency *= params->lacunarity;
    }
    
    return result / max_value; // Normalize
}

// Ridge noise for mountain ridges
f32
world_gen_ridge_noise(noise_params *params, f32 x, f32 y) {
    f32 noise = world_gen_noise_2d(params, x, y);
    return 1.0f - fabsf(noise);
}

// Turbulence noise for chaotic patterns
f32
world_gen_turbulence_noise(noise_params *params, f32 x, f32 y) {
    f32 result = 0.0f;
    f32 amplitude = params->amplitude;
    f32 frequency = params->frequency;
    
    noise_params turbulence_params = *params;
    
    for (i32 i = 0; i < params->octaves; i++) {
        turbulence_params.frequency = frequency;
        turbulence_params.amplitude = amplitude;
        
        result += fabsf(world_gen_noise_2d(&turbulence_params, x, y));
        
        amplitude *= params->persistence;
        frequency *= params->lacunarity;
    }
    
    return result;
}

// Initialize biome definitions
internal void
init_default_biomes(world_gen_system *system) {
    system->biome_count = 0;
    
    // Ocean
    biome_definition *ocean = &system->biomes[system->biome_count++];
    ocean->type = BIOME_OCEAN;
    strcpy(ocean->name, "Ocean");
    strcpy(ocean->description, "Deep blue waters teeming with marine life");
    ocean->min_temperature = -2.0f; ocean->max_temperature = 30.0f;
    ocean->min_humidity = 0.8f; ocean->max_humidity = 1.0f;
    ocean->min_elevation = -1000.0f; ocean->max_elevation = -1.0f;
    ocean->primary_color = 0xFF4169E1; // Royal Blue
    ocean->secondary_color = 0xFF0000CD; // Medium Blue
    ocean->terrain_roughness = 0.1f;
    ocean->vegetation_density = 0.0f;
    ocean->structure_frequency = 0.0f;
    ocean->resource_abundance = 0.3f;
    ocean->common_resources[0] = RESOURCE_WATER;
    ocean->resource_weights[0] = 1.0f;
    ocean->resource_count = 1;
    
    // Grassland
    biome_definition *grassland = &system->biomes[system->biome_count++];
    grassland->type = BIOME_GRASSLAND;
    strcpy(grassland->name, "Grassland");
    strcpy(grassland->description, "Rolling hills covered in tall grass");
    grassland->min_temperature = 5.0f; grassland->max_temperature = 25.0f;
    grassland->min_humidity = 0.3f; grassland->max_humidity = 0.7f;
    grassland->min_elevation = 0.0f; grassland->max_elevation = 500.0f;
    grassland->primary_color = 0xFF228B22; // Forest Green
    grassland->secondary_color = 0xFF32CD32; // Lime Green
    grassland->terrain_roughness = 0.3f;
    grassland->vegetation_density = 0.6f;
    grassland->structure_frequency = 0.2f;
    grassland->resource_abundance = 0.5f;
    grassland->common_resources[0] = RESOURCE_FOOD;
    grassland->common_resources[1] = RESOURCE_STONE;
    grassland->resource_weights[0] = 0.7f;
    grassland->resource_weights[1] = 0.3f;
    grassland->resource_count = 2;
    
    // Forest
    biome_definition *forest = &system->biomes[system->biome_count++];
    forest->type = BIOME_FOREST;
    strcpy(forest->name, "Forest");
    strcpy(forest->description, "Dense woodland with towering trees");
    forest->min_temperature = 0.0f; forest->max_temperature = 20.0f;
    forest->min_humidity = 0.6f; forest->max_humidity = 1.0f;
    forest->min_elevation = 0.0f; forest->max_elevation = 800.0f;
    forest->primary_color = 0xFF006400; // Dark Green
    forest->secondary_color = 0xFF228B22; // Forest Green
    forest->terrain_roughness = 0.4f;
    forest->vegetation_density = 0.9f;
    forest->structure_frequency = 0.8f;
    forest->resource_abundance = 0.7f;
    forest->common_resources[0] = RESOURCE_WOOD;
    forest->common_resources[1] = RESOURCE_FOOD;
    forest->common_resources[2] = RESOURCE_STONE;
    forest->resource_weights[0] = 0.6f;
    forest->resource_weights[1] = 0.3f;
    forest->resource_weights[2] = 0.1f;
    forest->resource_count = 3;
    
    // Desert
    biome_definition *desert = &system->biomes[system->biome_count++];
    desert->type = BIOME_DESERT;
    strcpy(desert->name, "Desert");
    strcpy(desert->description, "Vast sandy wasteland under scorching sun");
    desert->min_temperature = 20.0f; desert->max_temperature = 50.0f;
    desert->min_humidity = 0.0f; desert->max_humidity = 0.2f;
    desert->min_elevation = -50.0f; desert->max_elevation = 800.0f;
    desert->primary_color = 0xFFF4A460; // Sandy Brown
    desert->secondary_color = 0xFFDEB887; // Burlywood
    desert->terrain_roughness = 0.2f;
    desert->vegetation_density = 0.1f;
    desert->structure_frequency = 0.05f;
    desert->resource_abundance = 0.8f;
    desert->common_resources[0] = RESOURCE_STONE;
    desert->common_resources[1] = RESOURCE_GOLD;
    desert->common_resources[2] = RESOURCE_OIL;
    desert->resource_weights[0] = 0.5f;
    desert->resource_weights[1] = 0.3f;
    desert->resource_weights[2] = 0.2f;
    desert->resource_count = 3;
    
    // Mountains
    biome_definition *mountains = &system->biomes[system->biome_count++];
    mountains->type = BIOME_MOUNTAINS;
    strcpy(mountains->name, "Mountains");
    strcpy(mountains->description, "Towering peaks reaching toward the sky");
    mountains->min_temperature = -10.0f; mountains->max_temperature = 15.0f;
    mountains->min_humidity = 0.3f; mountains->max_humidity = 0.8f;
    mountains->min_elevation = 800.0f; mountains->max_elevation = 3000.0f;
    mountains->primary_color = 0xFF696969; // Dim Gray
    mountains->secondary_color = 0xFF708090; // Slate Gray
    mountains->terrain_roughness = 0.9f;
    mountains->vegetation_density = 0.3f;
    mountains->structure_frequency = 0.1f;
    mountains->resource_abundance = 1.0f;
    mountains->common_resources[0] = RESOURCE_STONE;
    mountains->common_resources[1] = RESOURCE_IRON;
    mountains->common_resources[2] = RESOURCE_GOLD;
    mountains->common_resources[3] = RESOURCE_DIAMOND;
    mountains->resource_weights[0] = 0.4f;
    mountains->resource_weights[1] = 0.3f;
    mountains->resource_weights[2] = 0.2f;
    mountains->resource_weights[3] = 0.1f;
    mountains->resource_count = 4;
    
    // Tundra
    biome_definition *tundra = &system->biomes[system->biome_count++];
    tundra->type = BIOME_TUNDRA;
    strcpy(tundra->name, "Tundra");
    strcpy(tundra->description, "Frozen plains with sparse vegetation");
    tundra->min_temperature = -30.0f; tundra->max_temperature = 5.0f;
    tundra->min_humidity = 0.2f; tundra->max_humidity = 0.6f;
    tundra->min_elevation = 0.0f; tundra->max_elevation = 500.0f;
    tundra->primary_color = 0xFFE0FFFF; // Light Cyan
    tundra->secondary_color = 0xFFB0E0E6; // Powder Blue
    tundra->terrain_roughness = 0.2f;
    tundra->vegetation_density = 0.2f;
    tundra->structure_frequency = 0.05f;
    tundra->resource_abundance = 0.4f;
    tundra->common_resources[0] = RESOURCE_WATER;
    tundra->common_resources[1] = RESOURCE_STONE;
    tundra->resource_weights[0] = 0.7f;
    tundra->resource_weights[1] = 0.3f;
    tundra->resource_count = 2;
}

// Hash function for chunk lookup
u32
world_gen_hash_chunk_id(i32 chunk_x, i32 chunk_y) {
    u32 h1 = (u32)(chunk_x * 73856093);
    u32 h2 = (u32)(chunk_y * 19349663);
    return (h1 ^ h2) & 255; // Mod 256 for hash table
}

u64
world_gen_get_chunk_seed(world_gen_system *system, i32 chunk_x, i32 chunk_y) {
    return system->world_seed + (u64)chunk_x * 1000000 + (u64)chunk_y;
}

// System initialization
world_gen_system *
world_gen_init(void *memory, u32 memory_size, u64 seed) {
    if (memory_size < sizeof(world_gen_system)) {
        return 0;
    }
    
    world_gen_system *system = (world_gen_system *)memory;
    memset(system, 0, sizeof(world_gen_system));
    
    system->memory = (u8 *)memory;
    system->memory_size = memory_size;
    system->memory_used = sizeof(world_gen_system);
    
    system->world_seed = seed;
    system->world_scale = 1.0f;
    system->initialized = 1;
    
    // Initialize noise parameters
    system->elevation_noise = (noise_params){0.01f, 100.0f, 6, 2.0f, 0.5f, 0.0f, 0.0f, (u32)seed};
    system->temperature_noise = (noise_params){0.005f, 30.0f, 4, 2.0f, 0.6f, 1000.0f, 0.0f, (u32)seed + 1};
    system->humidity_noise = (noise_params){0.008f, 1.0f, 4, 2.0f, 0.5f, 2000.0f, 1000.0f, (u32)seed + 2};
    system->biome_noise = (noise_params){0.003f, 1.0f, 3, 2.0f, 0.4f, 3000.0f, 2000.0f, (u32)seed + 3};
    system->cave_noise = (noise_params){0.05f, 1.0f, 3, 2.0f, 0.6f, 4000.0f, 3000.0f, (u32)seed + 4};
    system->resource_noise = (noise_params){0.02f, 1.0f, 4, 2.0f, 0.5f, 5000.0f, 4000.0f, (u32)seed + 5};
    
    // Initialize detail noise layers
    system->detail_noise_count = 3;
    for (u32 i = 0; i < system->detail_noise_count; i++) {
        system->detail_noise[i] = (noise_params){
            0.1f * (i + 1), 10.0f / (i + 1), 2, 2.0f, 0.5f, 
            (f32)(i * 1000), (f32)(i * 500), (u32)seed + 10 + i
        };
    }
    
    // World generation settings
    system->sea_level = 0.0f;
    system->mountain_threshold = 500.0f;
    system->cave_threshold = 0.6f;
    system->river_threshold = 0.8f;
    system->biome_blend_distance = 50.0f;
    
    // Climate settings
    system->global_temperature_offset = 15.0f; // Base temperature
    system->seasonal_variation = 10.0f;
    system->latitude_effect = 0.5f;
    system->altitude_effect = -0.0065f; // Temperature drop per meter
    
    // Initialize biome definitions
    init_default_biomes(system);
    
    // Initialize chunk hash table
    for (u32 i = 0; i < 256; i++) {
        system->chunk_hash[i] = 0;
    }
    
    printf("[WORLD_GEN] World generation system initialized\n");
    printf("[WORLD_GEN] Seed: %llu\n", (unsigned long long)seed);
    printf("[WORLD_GEN] Memory allocated: %u KB\n", memory_size / 1024);
    printf("[WORLD_GEN] Biomes registered: %u\n", system->biome_count);
    
    return system;
}

void
world_gen_shutdown(world_gen_system *system) {
    if (!system) return;
    
    printf("[WORLD_GEN] Shutting down world generation system\n");
    printf("[WORLD_GEN] Total chunks generated: %llu\n", (unsigned long long)system->total_chunks_generated);
    printf("[WORLD_GEN] Average generation time: %.3f ms\n", 
           system->total_chunks_generated > 0 ? 
           (f32)system->total_generation_time_us / (f32)system->total_chunks_generated / 1000.0f : 0.0f);
    
    system->initialized = 0;
}

void
world_gen_update(world_gen_system *system, f32 dt) {
    if (!system || !system->initialized) return;
    
    // Update performance counters
    static f32 stats_timer = 0.0f;
    stats_timer += dt;
    
    if (stats_timer >= 1.0f) {
        system->chunks_per_second = (u32)((f32)system->total_chunks_generated / stats_timer);
        stats_timer = 0.0f;
    }
    
    // Optimize chunk cache periodically
    static f32 cache_timer = 0.0f;
    cache_timer += dt;
    
    if (cache_timer >= 5.0f) {
        world_gen_optimize_chunk_cache(system);
        cache_timer = 0.0f;
    }
}

// Climate calculation
climate_data
world_gen_calculate_climate(world_gen_system *system, f32 world_x, f32 world_y, f32 elevation) {
    climate_data climate = {0};
    
    // Base temperature from noise + global settings
    f32 temp_noise = world_gen_noise_2d(&system->temperature_noise, world_x, world_y);
    climate.temperature = system->global_temperature_offset + temp_noise;
    
    // Altitude effect on temperature
    climate.temperature += elevation * system->altitude_effect;
    
    // Latitude effect (distance from equator)
    f32 latitude = fabsf(world_y * 0.001f); // Simulate latitude
    climate.temperature -= latitude * system->latitude_effect * 20.0f;
    
    // Humidity from noise
    climate.humidity = (world_gen_noise_2d(&system->humidity_noise, world_x, world_y) + 1.0f) * 0.5f;
    climate.humidity = fmaxf(0.0f, fminf(1.0f, climate.humidity));
    
    // Ocean distance effect on humidity
    climate.ocean_distance = 0.0f; // Simplified for now
    
    // Precipitation based on humidity and temperature
    climate.precipitation = climate.humidity * fmaxf(0.0f, (climate.temperature + 10.0f)) * 10.0f;
    
    // Wind simulation
    climate.wind_speed = (world_gen_noise_2d(&system->biome_noise, world_x * 0.1f, world_y * 0.1f) + 1.0f) * 25.0f;
    climate.wind_direction = world_gen_noise_2d(&system->biome_noise, world_x * 0.05f, world_y * 0.05f) * 360.0f;
    
    climate.elevation_factor = elevation / 1000.0f;
    
    return climate;
}

f32
world_gen_sample_elevation(world_gen_system *system, f32 world_x, f32 world_y) {
    // Base terrain elevation
    f32 base_elevation = world_gen_fbm_noise(&system->elevation_noise, world_x, world_y, 6) * 200.0f;
    
    // Mountain ridges
    f32 mountain_noise = world_gen_ridge_noise(&system->elevation_noise, world_x * 0.002f, world_y * 0.002f);
    f32 mountain_mask = fmaxf(0.0f, mountain_noise - 0.3f) * 3.33f; // 0-1 range
    base_elevation += mountain_mask * 800.0f;
    
    // Fine detail
    for (u32 i = 0; i < system->detail_noise_count; i++) {
        base_elevation += world_gen_noise_2d(&system->detail_noise[i], world_x, world_y);
    }
    
    return base_elevation;
}

biome_type
world_gen_determine_biome(world_gen_system *system, f32 temperature, f32 humidity, f32 elevation) {
    // Simple biome determination based on temperature, humidity, and elevation
    if (elevation < system->sea_level) {
        return BIOME_OCEAN;
    }
    
    if (elevation > system->mountain_threshold) {
        if (temperature < -5.0f) return BIOME_SNOW_MOUNTAINS;
        return BIOME_MOUNTAINS;
    }
    
    if (temperature < -10.0f) return BIOME_TUNDRA;
    if (temperature > 35.0f && humidity < 0.2f) return BIOME_DESERT;
    
    if (humidity > 0.8f) {
        if (temperature > 25.0f) return BIOME_JUNGLE;
        if (temperature > 10.0f) return BIOME_SWAMP;
        return BIOME_FOREST;
    }
    
    if (humidity > 0.5f) return BIOME_FOREST;
    if (humidity > 0.3f) return BIOME_GRASSLAND;
    
    return BIOME_SAVANNA;
}

biome_type
world_gen_sample_biome(world_gen_system *system, f32 world_x, f32 world_y) {
    f32 elevation = world_gen_sample_elevation(system, world_x, world_y);
    climate_data climate = world_gen_calculate_climate(system, world_x, world_y, elevation);
    return world_gen_determine_biome(system, climate.temperature, climate.humidity, elevation);
}

world_tile *
world_gen_get_tile(world_gen_system *system, i32 world_x, i32 world_y) {
    i32 chunk_x = world_x / WORLD_CHUNK_SIZE;
    i32 chunk_y = world_y / WORLD_CHUNK_SIZE;
    
    world_chunk *chunk = world_gen_get_chunk(system, chunk_x, chunk_y);
    if (!chunk) return 0;
    
    i32 tile_x = world_x - (chunk_x * WORLD_CHUNK_SIZE);
    i32 tile_y = world_y - (chunk_y * WORLD_CHUNK_SIZE);
    
    if (tile_x < 0 || tile_x >= WORLD_CHUNK_SIZE || tile_y < 0 || tile_y >= WORLD_CHUNK_SIZE) {
        return 0;
    }
    
    return &chunk->tiles[tile_x][tile_y];
}

void
world_gen_print_stats(world_gen_system *system) {
    if (!system) return;
    
    printf("\n=== World Generation System Stats ===\n");
    printf("Initialized: %s\n", system->initialized ? "Yes" : "No");
    printf("World seed: %llu\n", (unsigned long long)system->world_seed);
    printf("Active chunks: %u/%u\n", system->active_chunk_count, WORLD_MAX_ACTIVE_CHUNKS);
    printf("Total chunks generated: %llu\n", (unsigned long long)system->total_chunks_generated);
    printf("Generation rate: %u chunks/second\n", system->chunks_per_second);
    printf("Cache hits: %u\n", system->cache_hits);
    printf("Cache misses: %u\n", system->cache_misses);
    printf("Cache hit rate: %.1f%%\n", 
           system->cache_hits + system->cache_misses > 0 ? 
           (f32)system->cache_hits / (f32)(system->cache_hits + system->cache_misses) * 100.0f : 0.0f);
    printf("Average generation time: %.3f ms\n",
           system->total_chunks_generated > 0 ?
           (f32)system->total_generation_time_us / (f32)system->total_chunks_generated / 1000.0f : 0.0f);
    printf("Memory used: %u KB / %u KB\n", system->memory_used / 1024, system->memory_size / 1024);
    printf("Registered biomes: %u\n", system->biome_count);
    printf("========================================\n\n");
}

void
world_gen_optimize_chunk_cache(world_gen_system *system) {
    // Implementation would optimize chunk cache
    // For now, just a placeholder
    (void)system;
}

b32
world_gen_should_unload_chunk(world_gen_system *system, world_chunk *chunk, i32 player_x, i32 player_y) {
    if (!chunk) return 0;
    
    i32 dx = chunk->chunk_x - (player_x / WORLD_CHUNK_SIZE);
    i32 dy = chunk->chunk_y - (player_y / WORLD_CHUNK_SIZE);
    i32 distance_sq = dx * dx + dy * dy;
    
    // Unload if chunk is more than 8 chunks away and hasn't been accessed recently
    return (distance_sq > 64) && (system->total_chunks_generated - chunk->last_access_time > 1000);
}