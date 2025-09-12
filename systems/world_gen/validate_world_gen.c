/*
    Comprehensive validation suite for world generation system
    Tests all components for correctness, performance, and memory safety
*/

#include "handmade_noise.c"
#include "handmade_terrain.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>

// Test arena
typedef struct {
    u8* base;
    u64 size;
    u64 used;
    u64 peak_used;
} test_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    test_arena* a = (test_arena*)arena_ptr;
    u64 aligned = (a->used + alignment - 1) & ~(alignment - 1);
    
    if (aligned + size > a->size) {
        printf("ERROR: Arena overflow! Requested %llu bytes, available %llu\n",
               (unsigned long long)size, (unsigned long long)(a->size - aligned));
        assert(0);
    }
    
    void* result = a->base + aligned;
    a->used = aligned + size;
    if (a->used > a->peak_used) a->peak_used = a->used;
    
    // Clear memory for safety
    memset(result, 0, size);
    return result;
}

// Stub dependencies
struct renderer_state { int dummy; };
struct asset_system { int dummy; };

// =============================================================================
// VALIDATION HELPERS
// =============================================================================

typedef struct {
    const char* name;
    b32 passed;
    f64 time_ms;
    char error_msg[256];
} test_result;

#define MAX_TESTS 100
static test_result test_results[MAX_TESTS];
static u32 test_count = 0;

void report_test(const char* name, b32 passed, f64 time_ms, const char* error) {
    test_results[test_count].name = name;
    test_results[test_count].passed = passed;
    test_results[test_count].time_ms = time_ms;
    if (error) {
        strncpy(test_results[test_count].error_msg, error, 255);
    }
    test_count++;
}

void print_test_summary() {
    printf("\n=== VALIDATION SUMMARY ===\n");
    u32 passed = 0, failed = 0;
    f64 total_time = 0;
    
    for (u32 i = 0; i < test_count; i++) {
        if (test_results[i].passed) {
            printf("[✓] %-40s %.2f ms\n", test_results[i].name, test_results[i].time_ms);
            passed++;
        } else {
            printf("[✗] %-40s %s\n", test_results[i].name, test_results[i].error_msg);
            failed++;
        }
        total_time += test_results[i].time_ms;
    }
    
    printf("\nTotal: %u passed, %u failed (%.2f ms total)\n", passed, failed, total_time);
    printf("Result: %s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
}

// =============================================================================
// NOISE VALIDATION TESTS
// =============================================================================

b32 validate_noise_range(noise_state* state) {
    clock_t start = clock();
    
    // Sample many points to verify range
    const u32 SAMPLES = 100000;
    f32 min_2d = 999.0f, max_2d = -999.0f;
    f32 min_3d = 999.0f, max_3d = -999.0f;
    
    for (u32 i = 0; i < SAMPLES; i++) {
        f32 x = ((f32)rand() / RAND_MAX) * 1000.0f;
        f32 y = ((f32)rand() / RAND_MAX) * 1000.0f;
        f32 z = ((f32)rand() / RAND_MAX) * 1000.0f;
        
        f32 val_2d = noise_perlin_2d(state, x, y);
        f32 val_3d = noise_perlin_3d(state, x, y, z);
        
        if (val_2d < min_2d) min_2d = val_2d;
        if (val_2d > max_2d) max_2d = val_2d;
        if (val_3d < min_3d) min_3d = val_3d;
        if (val_3d > max_3d) max_3d = val_3d;
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    // Verify Perlin noise is in expected range
    b32 passed = (min_2d >= -1.0f && max_2d <= 1.0f && 
                  min_3d >= -1.0f && max_3d <= 1.0f);
    
    char error[256] = "";
    if (!passed) {
        snprintf(error, 256, "Range 2D: [%.3f, %.3f], 3D: [%.3f, %.3f]", 
                 min_2d, max_2d, min_3d, max_3d);
    }
    
    report_test("Noise Range Validation", passed, time, error);
    return passed;
}

b32 validate_noise_continuity(noise_state* state) {
    clock_t start = clock();
    
    // Check that nearby points have similar values
    const f32 EPSILON = 0.01f;
    const f32 MAX_GRADIENT = 10.0f;  // More reasonable for Perlin noise
    b32 passed = 1;
    
    for (u32 i = 0; i < 1000; i++) {
        f32 x = ((f32)rand() / RAND_MAX) * 100.0f;
        f32 y = ((f32)rand() / RAND_MAX) * 100.0f;
        
        f32 val = noise_perlin_2d(state, x, y);
        f32 val_x = noise_perlin_2d(state, x + EPSILON, y);
        f32 val_y = noise_perlin_2d(state, x, y + EPSILON);
        
        f32 gradient_x = fabsf(val_x - val) / EPSILON;
        f32 gradient_y = fabsf(val_y - val) / EPSILON;
        
        if (gradient_x > MAX_GRADIENT || gradient_y > MAX_GRADIENT) {
            passed = 0;
            break;
        }
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    report_test("Noise Continuity Validation", passed, time, 
                passed ? NULL : "Discontinuity detected");
    return passed;
}

b32 validate_noise_determinism(arena* arena) {
    clock_t start = clock();
    
    // Same seed should produce same values
    noise_state* state1 = noise_init(arena, 12345);
    noise_state* state2 = noise_init(arena, 12345);
    
    b32 passed = 1;
    for (u32 i = 0; i < 100; i++) {
        f32 x = (f32)i * 0.1f;
        f32 y = (f32)i * 0.2f;
        
        f32 val1 = noise_perlin_2d(state1, x, y);
        f32 val2 = noise_perlin_2d(state2, x, y);
        
        if (fabsf(val1 - val2) > 0.0001f) {
            passed = 0;
            break;
        }
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    report_test("Noise Determinism Validation", passed, time,
                passed ? NULL : "Non-deterministic results");
    return passed;
}

b32 validate_simd_correctness(noise_state* state) {
    clock_t start = clock();
    
    const u32 COUNT = 1024;
    f32* x_coords = malloc(COUNT * sizeof(f32));
    f32* y_coords = malloc(COUNT * sizeof(f32));
    f32* scalar_results = malloc(COUNT * sizeof(f32));
    f32* simd_results = malloc(COUNT * sizeof(f32));
    
    // Generate test coordinates
    for (u32 i = 0; i < COUNT; i++) {
        x_coords[i] = ((f32)rand() / RAND_MAX) * 100.0f;
        y_coords[i] = ((f32)rand() / RAND_MAX) * 100.0f;
    }
    
    // Compute scalar
    for (u32 i = 0; i < COUNT; i++) {
        scalar_results[i] = noise_perlin_2d(state, x_coords[i], y_coords[i]);
    }
    
    // Compute SIMD
    noise_perlin_2d_simd(state, x_coords, y_coords, simd_results, COUNT);
    
    // Compare results
    b32 passed = 1;
    f32 max_diff = 0.0f;
    for (u32 i = 0; i < COUNT; i++) {
        f32 diff = fabsf(scalar_results[i] - simd_results[i]);
        if (diff > max_diff) max_diff = diff;
        if (diff > 0.001f) {  // Allow small floating point differences
            passed = 0;
            break;
        }
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    char error[256] = "";
    if (!passed) {
        snprintf(error, 256, "Max difference: %.6f", max_diff);
    }
    
    report_test("SIMD Correctness Validation", passed, time, error);
    
    free(x_coords);
    free(y_coords);
    free(scalar_results);
    free(simd_results);
    
    return passed;
}

// =============================================================================
// TERRAIN VALIDATION TESTS
// =============================================================================

b32 validate_chunk_generation(terrain_system* terrain) {
    clock_t start = clock();
    
    terrain_chunk* chunk = &terrain->chunks[0];
    terrain_generate_chunk(terrain, chunk, 0, 0, 0);
    
    // Validate vertex count
    u32 expected_vertices = (TERRAIN_CHUNK_SIZE + 1) * (TERRAIN_CHUNK_SIZE + 1);
    b32 vertex_count_valid = (chunk->vertex_count == expected_vertices);
    
    // Validate index count
    u32 expected_indices = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 6;
    b32 index_count_valid = (chunk->index_count == expected_indices);
    
    // Validate all indices are in range
    b32 indices_valid = 1;
    for (u32 i = 0; i < chunk->index_count; i++) {
        if (chunk->indices[i] >= chunk->vertex_count) {
            indices_valid = 0;
            break;
        }
    }
    
    // Validate normals are normalized
    b32 normals_valid = 1;
    for (u32 i = 0; i < chunk->vertex_count; i++) {
        v3 n = chunk->vertices[i].normal;
        f32 len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        if (fabsf(len - 1.0f) > 0.01f) {
            normals_valid = 0;
            break;
        }
    }
    
    // Validate bounds
    b32 bounds_valid = 1;
    for (u32 i = 0; i < chunk->vertex_count; i++) {
        v3 p = chunk->vertices[i].position;
        if (p.x < chunk->min_bounds.x || p.x > chunk->max_bounds.x ||
            p.y < chunk->min_bounds.y || p.y > chunk->max_bounds.y ||
            p.z < chunk->min_bounds.z || p.z > chunk->max_bounds.z) {
            bounds_valid = 0;
            break;
        }
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    b32 passed = vertex_count_valid && index_count_valid && indices_valid && 
                 normals_valid && bounds_valid;
    
    char error[256] = "";
    if (!passed) {
        snprintf(error, 256, "V:%d I:%d Idx:%d N:%d B:%d",
                 vertex_count_valid, index_count_valid, indices_valid,
                 normals_valid, bounds_valid);
    }
    
    report_test("Chunk Generation Validation", passed, time, error);
    return passed;
}

b32 validate_lod_system(terrain_system* terrain) {
    clock_t start = clock();
    
    terrain_chunk* chunk = &terrain->chunks[0];
    u32 vertex_counts[TERRAIN_MAX_LOD + 1];
    
    // Generate at each LOD level
    for (u32 lod = 0; lod <= TERRAIN_MAX_LOD; lod++) {
        terrain_generate_chunk(terrain, chunk, 0, 0, lod);
        vertex_counts[lod] = chunk->vertex_count;
    }
    
    // Verify LOD reduces vertices appropriately
    b32 passed = 1;
    for (u32 lod = 1; lod <= TERRAIN_MAX_LOD; lod++) {
        if (vertex_counts[lod] >= vertex_counts[lod - 1]) {
            passed = 0;
            break;
        }
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    char error[256] = "";
    if (!passed) {
        snprintf(error, 256, "LOD vertex counts: %u,%u,%u,%u,%u",
                 vertex_counts[0], vertex_counts[1], vertex_counts[2],
                 vertex_counts[3], vertex_counts[4]);
    }
    
    report_test("LOD System Validation", passed, time, error);
    return passed;
}

b32 validate_biome_distribution(terrain_system* terrain) {
    clock_t start = clock();
    
    // Sample many points and check biome assignment
    const u32 SAMPLES = 10000;
    u32 biome_counts[BIOME_COUNT] = {0};
    
    for (u32 i = 0; i < SAMPLES; i++) {
        f32 x = ((f32)rand() / RAND_MAX - 0.5f) * 10000.0f;
        f32 z = ((f32)rand() / RAND_MAX - 0.5f) * 10000.0f;
        terrain_biome biome = terrain_get_biome(terrain, x, z);
        
        if (biome >= BIOME_COUNT) {
            report_test("Biome Distribution Validation", 0, 0, "Invalid biome");
            return 0;
        }
        
        biome_counts[biome]++;
    }
    
    // Verify we have at least some variety
    u32 biomes_present = 0;
    for (u32 i = 0; i < BIOME_COUNT; i++) {
        if (biome_counts[i] > 0) biomes_present++;
    }
    
    b32 passed = (biomes_present >= 3);  // At least 3 different biomes
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    char error[256] = "";
    if (!passed) {
        snprintf(error, 256, "Only %u biomes present", biomes_present);
    }
    
    report_test("Biome Distribution Validation", passed, time, error);
    return passed;
}

b32 validate_memory_usage(test_arena* arena) {
    clock_t start = clock();
    
    // Check memory usage is reasonable
    f64 mb_used = (f64)arena->peak_used / (1024.0 * 1024.0);
    f64 mb_total = (f64)arena->size / (1024.0 * 1024.0);
    f64 usage_percent = (arena->peak_used * 100.0) / arena->size;
    
    printf("\nMemory: %.2f MB / %.2f MB (%.1f%%)\n", mb_used, mb_total, usage_percent);
    
    b32 passed = (usage_percent < 90.0);  // Less than 90% usage
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    char error[256] = "";
    if (!passed) {
        snprintf(error, 256, "Memory usage too high: %.1f%%", usage_percent);
    }
    
    report_test("Memory Usage Validation", passed, time, error);
    return passed;
}

b32 validate_chunk_streaming(terrain_system* terrain) {
    clock_t start = clock();
    
    // Simulate camera movement and chunk loading
    v3 camera_positions[] = {
        {0, 100, 0},
        {500, 100, 500},
        {1000, 100, 1000},
        {-500, 100, -500}
    };
    
    b32 passed = 1;
    for (u32 i = 0; i < 4; i++) {
        terrain_update(terrain, camera_positions[i], 0.016f);
        
        // Count active chunks
        u32 active_count = 0;
        terrain_chunk* chunk = terrain->active_chunks;
        while (chunk) {
            active_count++;
            chunk = chunk->next;
        }
        
        // Should have some chunks loaded
        if (active_count == 0) {
            passed = 0;
            break;
        }
    }
    
    f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    report_test("Chunk Streaming Validation", passed, time,
                passed ? NULL : "No chunks loaded");
    return passed;
}

// =============================================================================
// PERFORMANCE BENCHMARKS
// =============================================================================

void benchmark_noise_generation(noise_state* state) {
    printf("\n=== Noise Performance Benchmarks ===\n");
    
    const u32 SAMPLES = 1000000;
    f32* x = malloc(SAMPLES * sizeof(f32));
    f32* y = malloc(SAMPLES * sizeof(f32));
    f32* z = malloc(SAMPLES * sizeof(f32));
    f32* output = malloc(SAMPLES * sizeof(f32));
    
    // Generate random coordinates
    for (u32 i = 0; i < SAMPLES; i++) {
        x[i] = ((f32)rand() / RAND_MAX) * 1000.0f;
        y[i] = ((f32)rand() / RAND_MAX) * 1000.0f;
        z[i] = ((f32)rand() / RAND_MAX) * 1000.0f;
    }
    
    // Benchmark 2D Perlin
    clock_t start = clock();
    for (u32 i = 0; i < SAMPLES; i++) {
        output[i] = noise_perlin_2d(state, x[i], y[i]);
    }
    f64 time_2d = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark 3D Perlin
    start = clock();
    for (u32 i = 0; i < SAMPLES; i++) {
        output[i] = noise_perlin_3d(state, x[i], y[i], z[i]);
    }
    f64 time_3d = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark SIMD
    start = clock();
    noise_perlin_2d_simd(state, x, y, output, SAMPLES);
    f64 time_simd = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("2D Perlin: %.2f ms (%.2f Msamples/s)\n", 
           time_2d, (SAMPLES / 1000000.0) / (time_2d / 1000.0));
    printf("3D Perlin: %.2f ms (%.2f Msamples/s)\n",
           time_3d, (SAMPLES / 1000000.0) / (time_3d / 1000.0));
    printf("2D SIMD:   %.2f ms (%.2f Msamples/s)\n",
           time_simd, (SAMPLES / 1000000.0) / (time_simd / 1000.0));
    printf("SIMD Speedup: %.2fx\n", time_2d / time_simd);
    
    free(x);
    free(y);
    free(z);
    free(output);
}

void benchmark_terrain_generation(terrain_system* terrain) {
    printf("\n=== Terrain Generation Benchmarks ===\n");
    
    terrain_chunk test_chunk;
    u32 max_vertices = (TERRAIN_CHUNK_SIZE + 1) * (TERRAIN_CHUNK_SIZE + 1);
    u32 max_indices = TERRAIN_CHUNK_SIZE * TERRAIN_CHUNK_SIZE * 6;
    test_chunk.vertices = malloc(sizeof(terrain_vertex) * max_vertices);
    test_chunk.indices = malloc(sizeof(u32) * max_indices);
    
    const u32 CHUNKS = 100;
    
    for (u32 lod = 0; lod <= 3; lod++) {
        clock_t start = clock();
        for (u32 i = 0; i < CHUNKS; i++) {
            terrain_generate_chunk(terrain, &test_chunk, i % 10, i / 10, lod);
        }
        f64 time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
        f64 per_chunk = time / CHUNKS;
        f64 chunks_per_sec = 1000.0 / per_chunk;
        
        printf("LOD %u: %.2f ms/chunk, %.0f chunks/s (%u verts)\n",
               lod, per_chunk, chunks_per_sec, test_chunk.vertex_count);
    }
    
    free(test_chunk.vertices);
    free(test_chunk.indices);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main() {
    printf("=== WORLD GENERATION VALIDATION SUITE ===\n");
    printf("Testing all components for correctness and performance\n\n");
    
    // Create test arena with aligned allocation
    test_arena main_arena = {0};
    main_arena.size = 256 * 1024 * 1024;  // 256MB
    main_arena.base = aligned_alloc(32, main_arena.size);
    main_arena.used = 0;
    main_arena.peak_used = 0;
    
    // Initialize systems
    struct renderer_state renderer = {0};
    struct asset_system assets = {0};
    
    printf("Initializing noise system...\n");
    noise_state* noise = noise_init((arena*)&main_arena, 12345);
    
    printf("Initializing terrain system...\n");
    terrain_system* terrain = terrain_init((arena*)&main_arena, &renderer, &assets, 12345);
    
    printf("\n=== RUNNING VALIDATION TESTS ===\n");
    
    // Noise validation
    validate_noise_range(noise);
    validate_noise_continuity(noise);
    validate_noise_determinism((arena*)&main_arena);
    validate_simd_correctness(noise);
    
    // Terrain validation
    validate_chunk_generation(terrain);
    validate_lod_system(terrain);
    validate_biome_distribution(terrain);
    validate_chunk_streaming(terrain);
    
    // Memory validation
    validate_memory_usage(&main_arena);
    
    // Performance benchmarks
    benchmark_noise_generation(noise);
    benchmark_terrain_generation(terrain);
    
    // Print summary
    print_test_summary();
    
    // Cleanup
    free(main_arena.base);
    
    // Return 0 if all tests passed
    for (u32 i = 0; i < test_count; i++) {
        if (!test_results[i].passed) return 1;
    }
    
    return 0;
}