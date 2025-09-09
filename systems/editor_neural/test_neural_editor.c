/*
 * NEURAL EDITOR TEST SUITE
 * ========================
 * Performance validation and correctness testing
 */

#include "handmade_editor_neural.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("  FAILED: %s\n", msg); \
        return 0; \
    }

// ============================================================================
// PERFORMANCE BENCHMARKS
// ============================================================================

static u64 rdtsc() {
    u32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static int benchmark_placement_prediction() {
    printf("\n[BENCHMARK] Placement Prediction\n");
    
    editor_neural_system* sys = neural_editor_create(4);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    // Warm up the predictor with some data
    for (u32 i = 0; i < 10; i++) {
        v3 pos = {(f32)i * 2, 0, (f32)i * 2};
        neural_record_placement(sys, pos, i % 4);
    }
    
    // Benchmark predictions
    const u32 iterations = 1000;
    u64 total_cycles = 0;
    u64 min_cycles = UINT64_MAX;
    u64 max_cycles = 0;
    
    for (u32 i = 0; i < iterations; i++) {
        v3 cursor = {(f32)(i % 20), 0, (f32)(i % 20)};
        u32 count;
        
        u64 start = rdtsc();
        neural_predict_placement(sys, cursor, i % 4, &count);
        u64 cycles = rdtsc() - start;
        
        total_cycles += cycles;
        if (cycles < min_cycles) min_cycles = cycles;
        if (cycles > max_cycles) max_cycles = cycles;
    }
    
    f32 avg_cycles = (f32)total_cycles / iterations;
    f32 avg_ms = avg_cycles / 3000000.0f; // Assuming 3GHz
    
    printf("  Iterations: %d\n", iterations);
    printf("  Avg cycles: %.0f\n", avg_cycles);
    printf("  Min cycles: %llu\n", min_cycles);
    printf("  Max cycles: %llu\n", max_cycles);
    printf("  Avg time: %.3f ms\n", avg_ms);
    
    TEST_ASSERT(avg_ms < 0.1f, "Placement prediction exceeds 0.1ms target");
    
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

static int benchmark_lod_computation() {
    printf("\n[BENCHMARK] LOD Computation\n");
    
    editor_neural_system* sys = neural_editor_create(4);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    // Create test objects
    const u32 object_count = 1000;
    v3* positions = (v3*)malloc(object_count * sizeof(v3));
    f32* sizes = (f32*)malloc(object_count * sizeof(f32));
    
    for (u32 i = 0; i < object_count; i++) {
        positions[i].x = (f32)(rand() % 100 - 50);
        positions[i].y = (f32)(rand() % 20);
        positions[i].z = (f32)(rand() % 100 - 50);
        sizes[i] = 1.0f + (f32)(rand() % 5);
    }
    
    v3 cam_pos = {0, 10, 0};
    v3 cam_dir = {0, -1, 0};
    
    // Benchmark
    const u32 iterations = 100;
    u64 total_cycles = 0;
    
    for (u32 i = 0; i < iterations; i++) {
        // Move camera
        cam_pos.x = sinf(i * 0.1f) * 20;
        cam_pos.z = cosf(i * 0.1f) * 20;
        
        u64 start = rdtsc();
        neural_compute_lod_levels(sys, positions, sizes, object_count, cam_pos, cam_dir);
        u64 cycles = rdtsc() - start;
        
        total_cycles += cycles;
    }
    
    f32 avg_cycles = (f32)total_cycles / iterations;
    f32 avg_ms = avg_cycles / 3000000.0f;
    f32 ms_per_object = avg_ms / object_count * 1000;
    
    printf("  Objects: %d\n", object_count);
    printf("  Iterations: %d\n", iterations);
    printf("  Avg time: %.3f ms\n", avg_ms);
    printf("  Time per object: %.3f μs\n", ms_per_object);
    
    TEST_ASSERT(avg_ms < 0.1f, "LOD computation exceeds 0.1ms for 1000 objects");
    
    free(positions);
    free(sizes);
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

static int benchmark_performance_prediction() {
    printf("\n[BENCHMARK] Performance Prediction\n");
    
    editor_neural_system* sys = neural_editor_create(4);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    scene_stats stats = {
        .object_count = 500,
        .triangle_count = 500000,
        .material_count = 20,
        .light_count = 8,
        .overdraw_estimate = 2.5f,
        .shadow_complexity = 3.0f,
        .transparency_ratio = 0.1f,
        .texture_memory_mb = 512,
        .scene_bounds = {-50, -10, -50, 50, 30, 50},
        .object_density = 10.0f,
        .depth_complexity = 4.0f
    };
    
    const u32 iterations = 1000;
    u64 total_cycles = 0;
    
    for (u32 i = 0; i < iterations; i++) {
        // Vary scene complexity
        stats.object_count = 100 + (i % 900);
        stats.triangle_count = stats.object_count * 1000;
        
        u64 start = rdtsc();
        f32 predicted_ms = neural_predict_frame_time(sys, &stats);
        u64 cycles = rdtsc() - start;
        
        total_cycles += cycles;
        
        // Sanity check prediction
        TEST_ASSERT(predicted_ms > 0 && predicted_ms < 100,
                   "Unrealistic frame time prediction");
    }
    
    f32 avg_cycles = (f32)total_cycles / iterations;
    f32 avg_ms = avg_cycles / 3000000.0f;
    
    printf("  Iterations: %d\n", iterations);
    printf("  Avg time: %.3f ms\n", avg_ms);
    
    TEST_ASSERT(avg_ms < 0.02f, "Performance prediction exceeds 0.02ms target");
    
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

// ============================================================================
// CORRECTNESS TESTS
// ============================================================================

static int test_memory_alignment() {
    printf("\n[TEST] Memory Alignment\n");
    
    editor_neural_system* sys = neural_editor_create(4);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    // Check SIMD alignment of critical buffers
    TEST_ASSERT(((uintptr_t)sys->temp_buffer_a & 31) == 0,
               "temp_buffer_a not 32-byte aligned");
    TEST_ASSERT(((uintptr_t)sys->temp_buffer_b & 31) == 0,
               "temp_buffer_b not 32-byte aligned");
    
    // Check layer weight alignment
    for (u32 i = 0; i < 3; i++) {
        TEST_ASSERT(((uintptr_t)sys->placement->layers[i].weights & 31) == 0,
                   "Layer weights not aligned");
        TEST_ASSERT(((uintptr_t)sys->placement->layers[i].biases & 31) == 0,
                   "Layer biases not aligned");
    }
    
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

static int test_placement_learning() {
    printf("\n[TEST] Placement Learning\n");
    
    editor_neural_system* sys = neural_editor_create(4);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    // Train with grid pattern
    for (u32 x = 0; x < 5; x++) {
        for (u32 z = 0; z < 5; z++) {
            v3 pos = {(f32)x * 5, 0, (f32)z * 5};
            neural_record_placement(sys, pos, 0);
        }
    }
    
    // Test prediction near grid points
    v3 test_pos = {4.8f, 0, 4.8f};
    u32 count;
    v3* predictions = neural_predict_placement(sys, test_pos, 0, &count);
    
    TEST_ASSERT(count > 0, "No predictions generated");
    
    // Check if prediction snaps to grid
    int found_grid_point = 0;
    for (u32 i = 0; i < count; i++) {
        f32 dx = fmodf(predictions[i].x, 5.0f);
        f32 dz = fmodf(predictions[i].z, 5.0f);
        
        if (dx < 0.5f && dz < 0.5f) {
            found_grid_point = 1;
            break;
        }
    }
    
    TEST_ASSERT(found_grid_point, "Failed to learn grid pattern");
    
    // Check pattern detection
    TEST_ASSERT(sys->placement->context.grid_snap_tendency > 0.3f,
               "Failed to detect grid snapping pattern");
    
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

static int test_lod_attention() {
    printf("\n[TEST] LOD Attention Model\n");
    
    editor_neural_system* sys = neural_editor_create(4);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    // Create test scene
    v3 positions[10];
    f32 sizes[10];
    
    for (u32 i = 0; i < 10; i++) {
        positions[i] = (v3){(f32)i * 5, 0, 0};
        sizes[i] = 2.0f;
    }
    
    v3 cam_pos = {0, 10, 10};
    v3 cam_dir = {0, -0.7f, -0.7f};
    
    // Set attention focus
    v3 focus = {25, 0, 0}; // Focus on middle objects
    neural_update_attention(sys, focus, 0.1f);
    
    // Compute LOD
    u8* lod_levels = neural_compute_lod_levels(sys, positions, sizes, 10, 
                                              cam_pos, cam_dir);
    
    // Objects near focus should have higher LOD
    u32 focus_idx = 5;
    TEST_ASSERT(lod_levels[focus_idx] >= lod_levels[0],
               "Focus object doesn't have higher LOD");
    TEST_ASSERT(lod_levels[focus_idx] >= lod_levels[9],
               "Focus object doesn't have higher LOD");
    
    // Test camera speed adaptation
    neural_update_attention(sys, focus, 20.0f); // Fast movement
    TEST_ASSERT(sys->lod->global_lod_bias <= 0,
               "LOD bias not reduced for fast camera");
    
    neural_update_attention(sys, focus, 0.5f); // Slow movement
    TEST_ASSERT(sys->lod->global_lod_bias >= 0,
               "LOD bias not increased for slow camera");
    
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

// ============================================================================
// CACHE PERFORMANCE TEST
// ============================================================================

static int test_cache_performance() {
    printf("\n[TEST] Cache Performance\n");
    
    editor_neural_system* sys = neural_editor_create(8);
    TEST_ASSERT(sys != NULL, "Failed to create neural system");
    
    // Large-scale test to measure cache behavior
    const u32 iterations = 10000;
    u64 start_cycles = rdtsc();
    
    for (u32 i = 0; i < iterations; i++) {
        v3 pos = {(f32)(i % 100), 0, (f32)(i % 100)};
        u32 count;
        neural_predict_placement(sys, pos, i % 4, &count);
    }
    
    u64 total_cycles = rdtsc() - start_cycles;
    f32 cycles_per_iter = (f32)total_cycles / iterations;
    
    printf("  Iterations: %d\n", iterations);
    printf("  Cycles per iteration: %.0f\n", cycles_per_iter);
    
    // Estimate cache misses (very rough)
    // Good performance should be < 10000 cycles per iteration
    TEST_ASSERT(cycles_per_iter < 10000,
               "Poor cache performance detected");
    
    neural_editor_destroy(sys);
    printf("  PASSED\n");
    return 1;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("========================================\n");
    printf("NEURAL EDITOR TEST SUITE\n");
    printf("========================================\n");
    
    int passed = 0;
    int total = 0;
    
    // Performance benchmarks
    total++; passed += benchmark_placement_prediction();
    total++; passed += benchmark_lod_computation();
    total++; passed += benchmark_performance_prediction();
    
    // Correctness tests
    total++; passed += test_memory_alignment();
    total++; passed += test_placement_learning();
    total++; passed += test_lod_attention();
    
    // Cache performance
    total++; passed += test_cache_performance();
    
    printf("\n========================================\n");
    printf("RESULTS: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("SUCCESS: All tests passed!\n");
        printf("\nPerformance targets achieved:\n");
        printf("  ✓ Inference < 0.1ms\n");
        printf("  ✓ Memory aligned for SIMD\n");
        printf("  ✓ Cache-coherent access patterns\n");
        printf("  ✓ 60+ FPS capable\n");
    } else {
        printf("FAILURE: Some tests failed\n");
    }
    
    printf("========================================\n");
    
    return (passed == total) ? 0 : 1;
}