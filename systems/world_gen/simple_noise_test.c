/*
    Simple test for Perlin noise generation
*/

#include "handmade_noise.c"
#include <stdio.h>
#include <stdlib.h>

// Simple arena
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} simple_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    simple_arena* a = (simple_arena*)arena_ptr;
    u64 aligned = (a->used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > a->size) return 0;
    void* result = a->base + aligned;
    a->used = aligned + size;
    return result;
}

int main() {
    printf("=== Simple Noise Test ===\n\n");
    
    // Create arena with aligned allocation
    simple_arena arena = {0};
    arena.size = 1024 * 1024;  // 1MB
    // Use aligned_alloc for proper SIMD alignment
    arena.base = aligned_alloc(32, arena.size);
    
    // Initialize noise
    noise_state* state = noise_init((struct arena*)&arena, 12345);
    printf("Initialized noise state\n");
    
    // Test single point
    f32 val = noise_perlin_2d(state, 0.5f, 0.5f);
    printf("Perlin 2D at (0.5, 0.5): %f\n", val);
    
    val = noise_perlin_3d(state, 0.5f, 0.5f, 0.5f);
    printf("Perlin 3D at (0.5, 0.5, 0.5): %f\n", val);
    
    // Test fractal noise
    noise_config config = {
        .frequency = 0.1f,
        .amplitude = 1.0f,
        .octaves = 4,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .seed = 12345
    };
    
    val = noise_fractal_2d(state, &config, 5.0f, 5.0f);
    printf("Fractal noise: %f\n", val);
    
    // Test range
    f32 min = 999.0f, max = -999.0f;
    for (int i = 0; i < 1000; i++) {
        f32 x = (f32)(i % 100) / 10.0f;
        f32 y = (f32)(i / 100) / 10.0f;
        f32 n = noise_perlin_2d(state, x, y);
        if (n < min) min = n;
        if (n > max) max = n;
    }
    printf("Noise range: [%.3f, %.3f]\n", min, max);
    
    free(arena.base);
    printf("\nTest complete!\n");
    return 0;
}