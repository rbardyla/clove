/*
    Test program for SIMD-optimized noise generation
    Verifies performance and quality of Perlin noise
*/

#include "handmade_noise.c"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

// Simple arena for testing
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} test_arena;

arena* temp_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    test_arena* a = (test_arena*)arena_ptr;
    u64 aligned_used = (a->used + alignment - 1) & ~(alignment - 1);
    
    if (aligned_used + size > a->size) {
        printf("Arena out of memory!\n");
        return 0;
    }
    
    void* result = a->base + aligned_used;
    a->used = aligned_used + size;
    return result;
}

// ASCII visualization of heightmap
void visualize_heightmap(f32* heightmap, u32 width, u32 height) {
    const char* chars = " .-:=+*#%@";
    
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            f32 h = heightmap[y * width + x];
            // Remap from [-1, 1] to [0, 9]
            s32 level = (s32)((h + 1.0f) * 4.5f);
            if (level < 0) level = 0;
            if (level > 9) level = 9;
            printf("%c", chars[level]);
        }
        printf("\n");
    }
}

// Performance test
void benchmark_noise(noise_state* state) {
    const u32 SAMPLES = 1000000;
    f32* x_coords = malloc(SAMPLES * sizeof(f32));
    f32* y_coords = malloc(SAMPLES * sizeof(f32));
    f32* output = malloc(SAMPLES * sizeof(f32));
    
    // Generate random coordinates
    for (u32 i = 0; i < SAMPLES; i++) {
        x_coords[i] = (f32)(rand() % 1000) / 10.0f;
        y_coords[i] = (f32)(rand() % 1000) / 10.0f;
    }
    
    // Benchmark scalar version
    clock_t start = clock();
    for (u32 i = 0; i < SAMPLES; i++) {
        output[i] = noise_perlin_2d(state, x_coords[i], y_coords[i]);
    }
    double scalar_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark SIMD version
    start = clock();
    noise_perlin_2d_simd(state, x_coords, y_coords, output, SAMPLES);
    double simd_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("\n=== Noise Performance Benchmark ===\n");
    printf("Samples: %u\n", SAMPLES);
    printf("Scalar time: %.2f ms (%.2f Msamples/s)\n", 
           scalar_time, (SAMPLES / 1000000.0) / (scalar_time / 1000.0));
    printf("SIMD time: %.2f ms (%.2f Msamples/s)\n",
           simd_time, (SAMPLES / 1000000.0) / (simd_time / 1000.0));
    printf("Speedup: %.2fx\n", scalar_time / simd_time);
    
    free(x_coords);
    free(y_coords);
    free(output);
}

int main() {
    printf("=== Handmade SIMD Noise Test ===\n\n");
    
    // Create test arena with aligned allocation
    test_arena main_arena = {0};
    main_arena.size = MEGABYTES(16);
    main_arena.base = aligned_alloc(32, main_arena.size);
    main_arena.used = 0;
    
    test_arena temp = {0};
    temp.size = MEGABYTES(8);
    temp.base = aligned_alloc(32, temp.size);
    temp.used = 0;
    temp_arena = (arena*)&temp;
    
    // Initialize noise with seed
    noise_state* state = noise_init((arena*)&main_arena, 12345);
    printf("Noise state initialized\n");
    
    // Test 1: Single point noise
    printf("\n=== Single Point Test ===\n");
    f32 val = noise_perlin_2d(state, 0.5f, 0.5f);
    printf("Perlin 2D at (0.5, 0.5): %f\n", val);
    
    val = noise_perlin_3d(state, 0.5f, 0.5f, 0.5f);
    printf("Perlin 3D at (0.5, 0.5, 0.5): %f\n", val);
    
    // Test 2: Fractal noise
    printf("\n=== Fractal Noise Test ===\n");
    noise_config config = {
        .frequency = 0.1f,
        .amplitude = 1.0f,
        .octaves = 4,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .seed = 12345
    };
    
    val = noise_fractal_2d(state, &config, 5.0f, 5.0f);
    printf("Fractal noise at (5, 5): %f\n", val);
    
    // Test 3: Generate small heightmap
    printf("\n=== Heightmap Generation ===\n");
    const u32 MAP_SIZE = 64;
    f32* heightmap = (f32*)arena_push_size((arena*)&main_arena, sizeof(f32) * MAP_SIZE * MAP_SIZE, 16);
    
    terrain_params params = {
        .base_frequency = 0.05f,
        .amplitude = 1.0f,
        .octaves = 4,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .elevation_scale = 1.0f,
        .elevation_offset = 0.0f,
        .erosion_strength = 0.0f,
        .ridge_frequency = 0.0f,
        .valley_depth = 0.0f
    };
    
    terrain_generate_heightmap(state, &params, heightmap, MAP_SIZE, MAP_SIZE);
    
    printf("Generated %ux%u heightmap:\n\n", MAP_SIZE, MAP_SIZE);
    visualize_heightmap(heightmap, MAP_SIZE, MAP_SIZE);
    
    // Test 4: Performance benchmark
    benchmark_noise(state);
    
    // Test 5: Verify range
    printf("\n=== Range Verification ===\n");
    f32 min_val = 999.0f, max_val = -999.0f;
    for (u32 i = 0; i < 10000; i++) {
        f32 x = (f32)(rand() % 10000) / 100.0f;
        f32 y = (f32)(rand() % 10000) / 100.0f;
        f32 n = noise_perlin_2d(state, x, y);
        if (n < min_val) min_val = n;
        if (n > max_val) max_val = n;
    }
    printf("Perlin noise range over 10000 samples: [%.3f, %.3f]\n", min_val, max_val);
    
    // Cleanup
    free(main_arena.base);
    free(temp.base);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}