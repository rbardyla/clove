/*
    Test program for terrain generation system
*/

#include "handmade_noise.c"
#include "handmade_terrain.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Simple arena implementation
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} test_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    test_arena* a = (test_arena*)arena_ptr;
    u64 aligned = (a->used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > a->size) {
        printf("Arena out of memory!\n");
        return 0;
    }
    void* result = a->base + aligned;
    a->used = aligned + size;
    return result;
}

// Stub implementations for dependencies
struct renderer_state { int dummy; };
struct asset_system { int dummy; };

// ASCII visualization of terrain chunk
void visualize_chunk(terrain_chunk* chunk) {
    if (!chunk || !chunk->is_generated) {
        printf("Chunk not generated\n");
        return;
    }
    
    // Find height range
    f32 min_height = 1e9f, max_height = -1e9f;
    for (u32 i = 0; i < chunk->vertex_count; i++) {
        f32 h = chunk->vertices[i].position.y;
        if (h < min_height) min_height = h;
        if (h > max_height) max_height = h;
    }
    
    printf("\nTerrain Chunk (%d, %d) LOD %u:\n", chunk->chunk_x, chunk->chunk_z, chunk->lod_level);
    printf("Height range: [%.1f, %.1f]\n", min_height, max_height);
    printf("Vertices: %u, Indices: %u\n\n", chunk->vertex_count, chunk->index_count);
    
    // Create ASCII height map (simplified - just sample grid)
    const u32 display_size = 32;
    const char* chars = " .-:=+*#%@";
    
    u32 step = (TERRAIN_CHUNK_SIZE / (1 << chunk->lod_level)) / display_size;
    if (step == 0) step = 1;
    
    for (u32 z = 0; z < display_size; z++) {
        for (u32 x = 0; x < display_size; x++) {
            u32 idx = z * step * (TERRAIN_CHUNK_SIZE / (1 << chunk->lod_level) + 1) + x * step;
            if (idx >= chunk->vertex_count) {
                printf("?");
                continue;
            }
            
            f32 h = chunk->vertices[idx].position.y;
            f32 normalized = (h - min_height) / (max_height - min_height + 0.001f);
            s32 level = (s32)(normalized * 9);
            if (level < 0) level = 0;
            if (level > 9) level = 9;
            printf("%c", chars[level]);
        }
        printf("\n");
    }
}

// Test biome distribution
void test_biome_distribution(terrain_system* terrain) {
    printf("\n=== Biome Distribution Test ===\n");
    
    u32 biome_counts[BIOME_COUNT] = {0};
    const u32 SAMPLES = 10000;
    
    for (u32 i = 0; i < SAMPLES; i++) {
        f32 x = ((f32)rand() / RAND_MAX - 0.5f) * 10000.0f;
        f32 z = ((f32)rand() / RAND_MAX - 0.5f) * 10000.0f;
        terrain_biome biome = terrain_get_biome(terrain, x, z);
        biome_counts[biome]++;
    }
    
    const char* biome_names[] = {
        "Ocean", "Beach", "Grassland", "Forest", "Mountain", "Snow"
    };
    
    for (u32 i = 0; i < BIOME_COUNT; i++) {
        f32 percentage = (f32)biome_counts[i] / SAMPLES * 100.0f;
        printf("%s: %.1f%%\n", biome_names[i], percentage);
    }
}

// Performance benchmark
void benchmark_generation(terrain_system* terrain) {
    printf("\n=== Generation Performance ===\n");
    
    const u32 CHUNKS_TO_TEST = 10;
    terrain_chunk test_chunk;
    
    // Allocate buffers (need +1 for edge vertices)
    u32 max_vertices = (TERRAIN_CHUNK_SIZE + 1) * (TERRAIN_CHUNK_SIZE + 1);
    u32 max_indices = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 6;
    test_chunk.vertices = malloc(sizeof(terrain_vertex) * max_vertices);
    test_chunk.indices = malloc(sizeof(u32) * max_indices);
    
    // Test different LOD levels
    for (u32 lod = 0; lod <= 3; lod++) {
        clock_t start = clock();
        
        for (u32 i = 0; i < CHUNKS_TO_TEST; i++) {
            terrain_generate_chunk(terrain, &test_chunk, i, 0, lod);
        }
        
        double time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
        double per_chunk = time_ms / CHUNKS_TO_TEST;
        
        printf("LOD %u: %.2f ms per chunk (%u vertices)\n", 
               lod, per_chunk, test_chunk.vertex_count);
    }
    
    free(test_chunk.vertices);
    free(test_chunk.indices);
}

int main() {
    printf("=== Handmade Terrain System Test ===\n\n");
    
    // Create arenas with aligned allocation
    test_arena main_arena = {0};
    main_arena.size = 128 * 1024 * 1024;  // 128MB
    main_arena.base = aligned_alloc(32, main_arena.size);
    
    // Create stub renderer and asset system
    struct renderer_state renderer = {0};
    struct asset_system assets = {0};
    
    // Initialize terrain system
    terrain_system* terrain = terrain_init((arena*)&main_arena, &renderer, &assets, 12345);
    
    // Test 1: Generate and visualize a chunk
    printf("\n=== Single Chunk Generation ===\n");
    terrain_chunk* chunk = &terrain->chunks[0];
    terrain_generate_chunk(terrain, chunk, 0, 0, 0);
    visualize_chunk(chunk);
    
    // Test 2: Height sampling
    printf("\n=== Height Sampling Test ===\n");
    f32 heights[5];
    f32 positions[] = {0, 100, 500, 1000, 5000};
    for (u32 i = 0; i < 5; i++) {
        heights[i] = terrain_get_height(terrain, positions[i], positions[i]);
        printf("Height at (%.0f, %.0f): %.2f\n", positions[i], positions[i], heights[i]);
    }
    
    // Test 3: Biome distribution
    test_biome_distribution(terrain);
    
    // Test 4: LOD generation
    printf("\n=== LOD Generation Test ===\n");
    for (u32 lod = 0; lod <= 3; lod++) {
        terrain_generate_chunk(terrain, chunk, 0, 0, lod);
        printf("LOD %u: %u vertices, %u triangles\n", 
               lod, chunk->vertex_count, chunk->index_count / 3);
    }
    
    // Test 5: Performance benchmark
    benchmark_generation(terrain);
    
    // Test 6: Terrain update simulation
    printf("\n=== Terrain Update Simulation ===\n");
    v3 camera_pos = {0, 100, 0};
    clock_t start = clock();
    terrain_update(terrain, camera_pos, 0.016f);
    double update_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    printf("Update time: %.2f ms\n", update_time);
    
    // Print final statistics
    terrain_print_stats(terrain);
    
    free(main_arena.base);
    printf("\n=== Test Complete ===\n");
    return 0;
}