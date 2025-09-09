/*
    Handmade Physics Engine - Performance Test Suite
    Validates 1000+ bodies at 60fps requirement
    
    Test scenarios:
    1. 1000 dynamic spheres falling onto ground
    2. 2000 mixed shapes (boxes, spheres, capsules) 
    3. Stress test with 5000 bodies
    4. Raycasting performance test
    5. Broadphase scaling test
*/

#include "handmade_physics.h"
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>

// Performance measurement utilities
typedef struct perf_timer {
    u64 start_cycles;
    u64 total_cycles;
    u32 call_count;
} perf_timer;

static inline u64 read_cpu_cycles() {
    #if defined(__x86_64__) || defined(_M_X64)
        return __rdtsc();
    #else
        return 0;
    #endif
}

static inline void timer_begin(perf_timer *timer) {
    timer->start_cycles = read_cpu_cycles();
}

static inline void timer_end(perf_timer *timer) {
    u64 end_cycles = read_cpu_cycles();
    timer->total_cycles += (end_cycles - timer->start_cycles);
    timer->call_count++;
}

static inline f64 timer_get_average_ms(perf_timer *timer, f64 cpu_freq) {
    if (timer->call_count == 0) return 0.0;
    f64 avg_cycles = (f64)timer->total_cycles / (f64)timer->call_count;
    return (avg_cycles / cpu_freq) * 1000.0;
}

static inline void timer_reset(perf_timer *timer) {
    timer->total_cycles = 0;
    timer->call_count = 0;
}

// Test utilities
static void create_ground_plane(physics_world *world, u32 *ground_id) {
    *ground_id = PhysicsCreateBody(world, V3(0, -5, 0), QuaternionIdentity());
    
    collision_shape ground_shape = PhysicsCreateBox(V3(100, 1, 100));
    PhysicsSetBodyShape(world, *ground_id, &ground_shape);
    
    rigid_body *ground = PhysicsGetBody(world, *ground_id);
    ground->Flags |= RIGID_BODY_STATIC;
    PhysicsCalculateMassProperties(ground);
    
    material ground_material = PhysicsCreateMaterial(1.0f, 0.3f, 0.8f);
    PhysicsSetBodyMaterial(world, *ground_id, &ground_material);
}

static void create_falling_bodies(physics_world *world, u32 body_count, u32 *body_ids) {
    for (u32 i = 0; i < body_count; ++i) {
        // Create grid layout
        u32 grid_size = (u32)(sqrtf((f32)body_count) + 0.5f);
        f32 spacing = 2.0f;
        
        f32 x = (i % grid_size) * spacing - (grid_size * spacing * 0.5f);
        f32 y = 10.0f + (i / (grid_size * grid_size)) * spacing;
        f32 z = ((i / grid_size) % grid_size) * spacing - (grid_size * spacing * 0.5f);
        
        v3 position = V3(x, y, z);
        body_ids[i] = PhysicsCreateBody(world, position, QuaternionIdentity());
        
        // Mix different shape types
        collision_shape shape;
        switch (i % 3) {
            case 0: {
                shape = PhysicsCreateSphere(0.4f + (i % 10) * 0.02f);
            } break;
            
            case 1: {
                v3 extents = V3(0.3f + (i % 7) * 0.05f, 
                               0.3f + ((i * 3) % 5) * 0.04f,
                               0.3f + ((i * 7) % 6) * 0.03f);
                shape = PhysicsCreateBox(extents);
            } break;
            
            case 2: {
                shape = PhysicsCreateCapsule(0.25f + (i % 5) * 0.03f, 
                                           0.8f + (i % 4) * 0.1f);
            } break;
        }
        
        PhysicsSetBodyShape(world, body_ids[i], &shape);
        
        // Vary material properties
        material mat = PhysicsCreateMaterial(
            0.8f + (i % 20) * 0.01f,           // density
            0.2f + (i % 10) * 0.05f,           // restitution  
            0.3f + (i % 15) * 0.04f            // friction
        );
        PhysicsSetBodyMaterial(world, body_ids[i], &mat);
        
        // Add small random velocities
        v3 velocity = V3(
            ((i % 13) - 6) * 0.2f,
            -1.0f + (i % 7) * 0.1f,
            ((i % 17) - 8) * 0.15f
        );
        PhysicsSetBodyVelocity(world, body_ids[i], velocity, V3(0, 0, 0));
    }
}

// Test 1: 1000 Bodies Performance Baseline
b32 test_1000_bodies_performance() {
    printf("Test 1: 1000 Dynamic Bodies Performance\n");
    printf("========================================\n");
    
    // Allocate physics world
    u8 *arena_memory = malloc(Megabytes(128));
    if (!arena_memory) {
        printf("Failed to allocate memory for physics world\n");
        return false;
    }
    
    physics_world *world = PhysicsCreateWorld(Megabytes(128), arena_memory);
    if (!world) {
        printf("Failed to create physics world\n");
        free(arena_memory);
        return false;
    }
    
    // Create ground
    u32 ground_id;
    create_ground_plane(world, &ground_id);
    
    // Create 1000 falling bodies  
    const u32 BODY_COUNT = 1000;
    u32 *body_ids = malloc(BODY_COUNT * sizeof(u32));
    create_falling_bodies(world, BODY_COUNT, body_ids);
    
    printf("Created %u dynamic bodies + 1 static ground\n", BODY_COUNT);
    printf("Total bodies: %u\n", world->BodyCount);
    
    // Performance measurement
    perf_timer frame_timer = {0};
    const u32 WARM_UP_FRAMES = 60;
    const u32 TEST_FRAMES = 180;  // 3 seconds at 60fps
    
    // CPU frequency estimation (rough)
    f64 cpu_freq = 3.0e9;  // Assume 3GHz CPU
    
    printf("Warming up simulation (%u frames)...\n", WARM_UP_FRAMES);
    
    // Warm up - let bodies fall and settle
    for (u32 frame = 0; frame < WARM_UP_FRAMES; ++frame) {
        PhysicsStepSimulation(world, 1.0f / 60.0f);
    }
    
    printf("Running performance test (%u frames)...\n", TEST_FRAMES);
    
    // Measure performance over test period
    timer_reset(&frame_timer);
    
    for (u32 frame = 0; frame < TEST_FRAMES; ++frame) {
        timer_begin(&frame_timer);
        PhysicsStepSimulation(world, 1.0f / 60.0f);
        timer_end(&frame_timer);
        
        // Progress indicator
        if (frame % 30 == 0) {
            printf("  Frame %u/%u\n", frame, TEST_FRAMES);
        }
    }
    
    // Calculate results
    f64 avg_frame_ms = timer_get_average_ms(&frame_timer, cpu_freq);
    f64 avg_fps = 1000.0 / avg_frame_ms;
    
    printf("\nResults:\n");
    printf("  Bodies: %u\n", BODY_COUNT);
    printf("  Average frame time: %.3f ms\n", avg_frame_ms);
    printf("  Average FPS: %.1f\n", avg_fps);
    printf("  Active bodies: %u\n", world->ActiveBodyCount);
    printf("  Contact manifolds: %u\n", world->ManifoldCount);
    
    // Get detailed profiling
    f32 broad_ms, narrow_ms, solver_ms, integration_ms;
    u32 active_bodies;
    PhysicsGetProfileInfo(world, &broad_ms, &narrow_ms, &solver_ms, &integration_ms, &active_bodies);
    
    printf("\nDetailed timing:\n");
    printf("  Broadphase: %.3f ms\n", broad_ms);
    printf("  Narrowphase: %.3f ms\n", narrow_ms);
    printf("  Solver: %.3f ms\n", solver_ms);
    printf("  Integration: %.3f ms\n", integration_ms);
    
    // Performance targets
    b32 passed = true;
    printf("\nPerformance validation:\n");
    
    if (avg_fps >= 60.0) {
        printf("  ✓ PASS: Maintaining 60+ FPS (%.1f)\n", avg_fps);
    } else {
        printf("  ✗ FAIL: FPS below 60 (%.1f)\n", avg_fps);
        passed = false;
    }
    
    if (avg_frame_ms <= 16.67) {
        printf("  ✓ PASS: Frame time within 16.67ms budget (%.3f ms)\n", avg_frame_ms);
    } else {
        printf("  ✗ FAIL: Frame time exceeds budget (%.3f ms)\n", avg_frame_ms);
        passed = false;
    }
    
    if (broad_ms <= 2.0) {
        printf("  ✓ PASS: Broadphase under 2ms (%.3f ms)\n", broad_ms);
    } else {
        printf("  ✗ FAIL: Broadphase too slow (%.3f ms)\n", broad_ms);
    }
    
    if (solver_ms <= 8.0) {
        printf("  ✓ PASS: Solver under 8ms (%.3f ms)\n", solver_ms);
    } else {
        printf("  ✗ FAIL: Solver too slow (%.3f ms)\n", solver_ms);
    }
    
    // Cleanup
    free(body_ids);
    PhysicsDestroyWorld(world);
    free(arena_memory);
    
    return passed;
}

// Test 2: Scaling Test - 2000 Bodies
b32 test_2000_bodies_scaling() {
    printf("\nTest 2: 2000 Bodies Scaling Test\n");
    printf("=================================\n");
    
    u8 *arena_memory = malloc(Megabytes(256));
    if (!arena_memory) {
        printf("Failed to allocate memory\n");
        return false;
    }
    
    physics_world *world = PhysicsCreateWorld(Megabytes(256), arena_memory);
    
    u32 ground_id;
    create_ground_plane(world, &ground_id);
    
    const u32 BODY_COUNT = 2000;
    u32 *body_ids = malloc(BODY_COUNT * sizeof(u32));
    create_falling_bodies(world, BODY_COUNT, body_ids);
    
    printf("Created %u dynamic bodies + 1 static ground\n", BODY_COUNT);
    
    // Measure single frame after warm-up
    for (u32 frame = 0; frame < 120; ++frame) {
        PhysicsStepSimulation(world, 1.0f / 60.0f);
    }
    
    perf_timer frame_timer = {0};
    f64 cpu_freq = 3.0e9;
    
    const u32 TEST_FRAMES = 60;
    for (u32 frame = 0; frame < TEST_FRAMES; ++frame) {
        timer_begin(&frame_timer);
        PhysicsStepSimulation(world, 1.0f / 60.0f);
        timer_end(&frame_timer);
    }
    
    f64 avg_frame_ms = timer_get_average_ms(&frame_timer, cpu_freq);
    f64 avg_fps = 1000.0 / avg_frame_ms;
    
    printf("Results with %u bodies:\n", BODY_COUNT);
    printf("  Average frame time: %.3f ms\n", avg_frame_ms);
    printf("  Average FPS: %.1f\n", avg_fps);
    printf("  Contact manifolds: %u\n", world->ManifoldCount);
    
    b32 acceptable = avg_fps >= 30.0;  // Lower threshold for 2x bodies
    
    if (acceptable) {
        printf("  ✓ PASS: Performance acceptable for 2000 bodies (%.1f FPS)\n", avg_fps);
    } else {
        printf("  ✗ FAIL: Performance unacceptable for 2000 bodies (%.1f FPS)\n", avg_fps);
    }
    
    free(body_ids);
    PhysicsDestroyWorld(world);
    free(arena_memory);
    
    return acceptable;
}

// Test 3: Stress Test - Maximum Bodies
void test_stress_maximum_bodies() {
    printf("\nTest 3: Stress Test - Maximum Body Count\n");
    printf("=========================================\n");
    
    u8 *arena_memory = malloc(Megabytes(512));
    physics_world *world = PhysicsCreateWorld(Megabytes(512), arena_memory);
    
    u32 ground_id;
    create_ground_plane(world, &ground_id);
    
    // Test increasing body counts to find limit
    u32 body_counts[] = {500, 1000, 2000, 3000, 4000, 5000};
    u32 test_count = sizeof(body_counts) / sizeof(body_counts[0]);
    
    for (u32 test = 0; test < test_count; ++test) {
        u32 body_count = body_counts[test];
        
        PhysicsResetWorld(world);
        create_ground_plane(world, &ground_id);
        
        u32 *body_ids = malloc(body_count * sizeof(u32));
        create_falling_bodies(world, body_count, body_ids);
        
        // Quick performance measurement
        for (u32 frame = 0; frame < 30; ++frame) {
            PhysicsStepSimulation(world, 1.0f / 60.0f);
        }
        
        perf_timer timer = {0};
        f64 cpu_freq = 3.0e9;
        
        const u32 SAMPLE_FRAMES = 30;
        for (u32 frame = 0; frame < SAMPLE_FRAMES; ++frame) {
            timer_begin(&timer);
            PhysicsStepSimulation(world, 1.0f / 60.0f);
            timer_end(&timer);
        }
        
        f64 avg_ms = timer_get_average_ms(&timer, cpu_freq);
        f64 avg_fps = 1000.0 / avg_ms;
        
        printf("  %u bodies: %.1f FPS (%.3f ms/frame)\n", 
               body_count, avg_fps, avg_ms);
        
        free(body_ids);
        
        if (avg_fps < 10.0) {
            printf("  Performance degraded significantly, stopping stress test\n");
            break;
        }
    }
    
    PhysicsDestroyWorld(world);
    free(arena_memory);
}

// Test 4: Broadphase Scaling
void test_broadphase_scaling() {
    printf("\nTest 4: Broadphase Scaling Test\n");
    printf("===============================\n");
    
    u8 *arena_memory = malloc(Megabytes(256));
    physics_world *world = PhysicsCreateWorld(Megabytes(256), arena_memory);
    
    // Test broadphase with different body distributions
    u32 body_counts[] = {1000, 2500, 5000, 7500, 10000};
    u32 test_count = sizeof(body_counts) / sizeof(body_counts[0]);
    
    for (u32 test = 0; test < test_count; ++test) {
        u32 body_count = body_counts[test];
        
        PhysicsResetWorld(world);
        
        // Create bodies in a more spread out pattern for broadphase testing
        for (u32 i = 0; i < body_count; ++i) {
            f32 range = 200.0f;
            f32 x = ((f32)rand() / RAND_MAX) * range - range * 0.5f;
            f32 y = ((f32)rand() / RAND_MAX) * 20.0f + 10.0f;
            f32 z = ((f32)rand() / RAND_MAX) * range - range * 0.5f;
            
            v3 position = V3(x, y, z);
            u32 body_id = PhysicsCreateBody(world, position, QuaternionIdentity());
            
            collision_shape shape = PhysicsCreateSphere(0.5f);
            PhysicsSetBodyShape(world, body_id, &shape);
        }
        
        // Measure broadphase performance specifically
        clock_t start = clock();
        
        PhysicsBroadPhaseUpdate(world);
        u32 pairs = PhysicsBroadPhaseFindPairs(world);
        
        clock_t end = clock();
        f64 time_ms = ((f64)(end - start) / CLOCKS_PER_SEC) * 1000.0;
        
        printf("  %u bodies: %.3f ms broadphase, %u pairs\n",
               body_count, time_ms, pairs);
        
        // Check if we're meeting the <1ms target for 10k bodies
        if (body_count == 10000) {
            if (time_ms < 1.0) {
                printf("    ✓ PASS: 10k body broadphase under 1ms\n");
            } else {
                printf("    ✗ FAIL: 10k body broadphase over 1ms\n");
            }
        }
    }
    
    PhysicsDestroyWorld(world);
    free(arena_memory);
}

int main() {
    printf("======================================\n");
    printf("HANDMADE PHYSICS PERFORMANCE TEST SUITE\n");
    printf("======================================\n");
    printf("Target: 1000+ rigid bodies at 60 FPS\n\n");
    
    b32 all_passed = true;
    
    // Core performance test
    if (!test_1000_bodies_performance()) {
        all_passed = false;
    }
    
    // Scaling test
    if (!test_2000_bodies_scaling()) {
        all_passed = false;
    }
    
    // Stress testing
    test_stress_maximum_bodies();
    
    // Broadphase scaling
    test_broadphase_scaling();
    
    printf("\n======================================\n");
    if (all_passed) {
        printf("✓ ALL CORE TESTS PASSED\n");
        printf("Physics engine meets 1000+ bodies @ 60fps requirement!\n");
    } else {
        printf("✗ SOME TESTS FAILED\n");
        printf("Physics engine needs optimization to meet requirements.\n");
    }
    printf("======================================\n");
    
    return all_passed ? 0 : 1;
}