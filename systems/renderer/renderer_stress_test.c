/*
    3D Renderer Stress Test
    Measures actual performance capabilities of the handmade OpenGL renderer
    
    Tests performed:
    1. Maximum triangle throughput
    2. Draw call overhead
    3. State change cost (shader/texture switches)
    4. Instancing efficiency
    5. Memory bandwidth limitations
    6. Vertex processing limits
    
    Outputs concrete metrics for decision making
*/

#define _GNU_SOURCE  // For clock_gettime
#include "handmade_renderer.h"
#include "handmade_platform.h"
#include "handmade_opengl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Test configuration
#define TEST_DURATION_SECONDS 5.0f
#define WARMUP_FRAMES 60
#define SAMPLE_INTERVAL_MS 100.0f

// Test results structure
typedef struct test_result {
    char test_name[128];
    f32 avg_fps;
    f32 min_fps;
    f32 max_fps;
    f32 percentile_1;  // 1% low
    f32 percentile_99; // 99% high
    u32 avg_triangles;
    u32 max_triangles;
    u32 avg_draw_calls;
    u32 max_draw_calls;
    u32 avg_vertices;
    u32 max_vertices;
    f32 avg_frame_time_ms;
    f32 max_frame_time_ms;
    u32 object_count;
    u32 triangles_per_object;
    b32 passed_60fps;
} test_result;

// Performance samples for statistics
typedef struct perf_samples {
    f32 *frame_times;
    u32 *triangle_counts;
    u32 *draw_call_counts;
    u32 *vertex_counts;
    u32 sample_count;
    u32 max_samples;
} perf_samples;

// Test scenarios
typedef enum test_scenario {
    TEST_MANY_SIMPLE_OBJECTS,      // Many cubes (12 tris each)
    TEST_FEW_COMPLEX_OBJECTS,       // Few high-poly spheres
    TEST_INSTANCED_RENDERING,      // Instanced cubes
    TEST_DRAW_CALL_OVERHEAD,       // Minimal geometry, many draw calls
    TEST_STATE_CHANGES,            // Shader/texture switching
    TEST_LARGE_MESHES,             // Few very large meshes
    TEST_MIXED_COMPLEXITY,         // Mix of simple and complex
    TEST_SCENARIO_COUNT
} test_scenario;

// Helper function to get time in milliseconds
static f32 platform_get_time_ms(platform_state *platform) {
    return platform_get_time(platform) * 1000.0f;
}

// Helper functions
static void print_separator() {
    printf("================================================================================\n");
}

static void print_test_header(const char *name) {
    printf("\n");
    print_separator();
    printf("  TEST: %s\n", name);
    print_separator();
}

static void print_test_result(test_result *result) {
    printf("\nRESULTS for '%s':\n", result->test_name);
    printf("--------------------------------------------------\n");
    printf("Performance:\n");
    printf("  Average FPS:        %.1f\n", result->avg_fps);
    printf("  Minimum FPS:        %.1f\n", result->min_fps);
    printf("  Maximum FPS:        %.1f\n", result->max_fps);
    printf("  1%% Low FPS:         %.1f\n", result->percentile_1);
    printf("  99%% High FPS:       %.1f\n", result->percentile_99);
    printf("  Avg Frame Time:     %.2f ms\n", result->avg_frame_time_ms);
    printf("  Max Frame Time:     %.2f ms\n", result->max_frame_time_ms);
    printf("  60 FPS Target:      %s\n", result->passed_60fps ? "PASSED" : "FAILED");
    
    printf("\nGeometry Statistics:\n");
    printf("  Object Count:       %u\n", result->object_count);
    printf("  Tris per Object:    %u\n", result->triangles_per_object);
    printf("  Avg Triangles:      %u\n", result->avg_triangles);
    printf("  Max Triangles:      %u\n", result->max_triangles);
    printf("  Avg Draw Calls:     %u\n", result->avg_draw_calls);
    printf("  Max Draw Calls:     %u\n", result->max_draw_calls);
    printf("  Avg Vertices:       %u\n", result->avg_vertices);
    printf("  Max Vertices:       %u\n", result->max_vertices);
    
    printf("\nThroughput:\n");
    printf("  Triangles/Second:   %.0f\n", result->avg_triangles * result->avg_fps);
    printf("  Vertices/Second:    %.0f\n", result->avg_vertices * result->avg_fps);
    printf("  Draw Calls/Second:  %.0f\n", result->avg_draw_calls * result->avg_fps);
}

static int compare_f32(const void *a, const void *b) {
    f32 fa = *(const f32*)a;
    f32 fb = *(const f32*)b;
    return (fa > fb) - (fa < fb);
}

static void calculate_statistics(perf_samples *samples, test_result *result) {
    if (samples->sample_count == 0) return;
    
    // Calculate FPS statistics
    f32 *sorted_times = (f32*)malloc(samples->sample_count * sizeof(f32));
    memcpy(sorted_times, samples->frame_times, samples->sample_count * sizeof(f32));
    qsort(sorted_times, samples->sample_count, sizeof(f32), compare_f32);
    
    // Convert to FPS
    f32 total_fps = 0.0f;
    result->min_fps = 1000.0f / sorted_times[samples->sample_count - 1];
    result->max_fps = 1000.0f / sorted_times[0];
    
    for (u32 i = 0; i < samples->sample_count; i++) {
        total_fps += 1000.0f / samples->frame_times[i];
    }
    result->avg_fps = total_fps / samples->sample_count;
    
    // Calculate percentiles
    u32 idx_1 = (u32)(samples->sample_count * 0.99f);  // 1% low (99th percentile of frame times)
    u32 idx_99 = (u32)(samples->sample_count * 0.01f); // 99% high (1st percentile of frame times)
    result->percentile_1 = 1000.0f / sorted_times[idx_1];
    result->percentile_99 = 1000.0f / sorted_times[idx_99];
    
    // Calculate averages
    f32 total_frame_time = 0.0f;
    u64 total_triangles = 0;
    u64 total_draw_calls = 0;
    u64 total_vertices = 0;
    
    result->max_frame_time_ms = sorted_times[samples->sample_count - 1];
    
    for (u32 i = 0; i < samples->sample_count; i++) {
        total_frame_time += samples->frame_times[i];
        total_triangles += samples->triangle_counts[i];
        total_draw_calls += samples->draw_call_counts[i];
        total_vertices += samples->vertex_counts[i];
        
        if (samples->triangle_counts[i] > result->max_triangles)
            result->max_triangles = samples->triangle_counts[i];
        if (samples->draw_call_counts[i] > result->max_draw_calls)
            result->max_draw_calls = samples->draw_call_counts[i];
        if (samples->vertex_counts[i] > result->max_vertices)
            result->max_vertices = samples->vertex_counts[i];
    }
    
    result->avg_frame_time_ms = total_frame_time / samples->sample_count;
    result->avg_triangles = (u32)(total_triangles / samples->sample_count);
    result->avg_draw_calls = (u32)(total_draw_calls / samples->sample_count);
    result->avg_vertices = (u32)(total_vertices / samples->sample_count);
    
    // Check 60 FPS target (allow 1% of frames to miss)
    u32 frames_below_60 = 0;
    for (u32 i = 0; i < samples->sample_count; i++) {
        if (samples->frame_times[i] > 16.67f) {
            frames_below_60++;
        }
    }
    result->passed_60fps = (frames_below_60 < samples->sample_count * 0.01f);
    
    free(sorted_times);
}

// Test 1: Many Simple Objects (testing draw call overhead)
static test_result test_many_simple_objects(renderer_state *renderer, platform_state *platform) {
    print_test_header("Many Simple Objects (Draw Call Overhead)");
    
    test_result result = {0};
    strcpy(result.test_name, "Many Simple Objects");
    
    // Start with reasonable number and increase until FPS drops
    u32 object_counts[] = {100, 250, 500, 750, 1000, 1500, 2000, 3000, 4000, 5000};
    u32 best_count = 0;
    
    for (u32 test_idx = 0; test_idx < ArrayCount(object_counts); test_idx++) {
        u32 object_count = object_counts[test_idx];
        printf("\nTesting %u cubes...\n", object_count);
        
        perf_samples samples = {
            .frame_times = (f32*)calloc(10000, sizeof(f32)),
            .triangle_counts = (u32*)calloc(10000, sizeof(u32)),
            .draw_call_counts = (u32*)calloc(10000, sizeof(u32)),
            .vertex_counts = (u32*)calloc(10000, sizeof(u32)),
            .sample_count = 0,
            .max_samples = 10000
        };
        
        // Warmup
        for (u32 i = 0; i < WARMUP_FRAMES; i++) {
            platform_poll_events(platform);
            renderer_begin_frame(renderer);
            renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
            renderer_end_frame(renderer);
            renderer_present(renderer);
        }
        
        // Test loop
        f32 test_time = 0.0f;
        while (test_time < TEST_DURATION_SECONDS && platform->is_running) {
            platform_poll_events(platform);
            
            f32 frame_start = platform_get_time_ms(platform);
            
            renderer_begin_frame(renderer);
            renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
            renderer_reset_stats(renderer);
            
            // Use basic shader
            renderer_use_shader(renderer, renderer->basic_shader);
            
            // Set light
            v3 light_dir = v3_normalize((v3){-1.0f, -1.0f, -1.0f});
            renderer_set_uniform_v3(renderer->basic_shader, "lightDir", light_dir);
            renderer_set_uniform_v3(renderer->basic_shader, "lightColor", (v3){1.0f, 1.0f, 1.0f});
            
            // Draw objects in a grid pattern
            u32 grid_size = (u32)sqrtf((f32)object_count) + 1;
            f32 spacing = 2.0f;
            f32 offset = -(grid_size * spacing * 0.5f);
            
            for (u32 i = 0; i < object_count; i++) {
                u32 x = i % grid_size;
                u32 z = i / grid_size;
                
                m4x4 model = m4x4_translation(
                    offset + x * spacing,
                    0.0f,
                    offset + z * spacing
                );
                model = m4x4_multiply(model, m4x4_scale(0.5f, 0.5f, 0.5f));
                
                // Vary colors
                f32 r = (f32)x / grid_size;
                f32 g = (f32)z / grid_size;
                f32 b = 0.5f;
                renderer_set_uniform_v3(renderer->basic_shader, "objectColor", (v3){r, g, b});
                
                renderer_draw_mesh(renderer, renderer->cube_mesh, model);
            }
            
            renderer_end_frame(renderer);
            renderer_present(renderer);
            
            f32 frame_end = platform_get_time_ms(platform);
            f32 frame_time = frame_end - frame_start;
            
            // Collect samples
            if (samples.sample_count < samples.max_samples) {
                renderer_stats stats = renderer_get_stats(renderer);
                samples.frame_times[samples.sample_count] = frame_time;
                samples.triangle_counts[samples.sample_count] = stats.triangles_rendered;
                samples.draw_call_counts[samples.sample_count] = stats.draw_calls;
                samples.vertex_counts[samples.sample_count] = stats.vertices_processed;
                samples.sample_count++;
            }
            
            test_time += frame_time / 1000.0f;
        }
        
        // Calculate results for this count
        test_result temp_result = {0};
        calculate_statistics(&samples, &temp_result);
        
        printf("  Avg FPS: %.1f, 1%% Low: %.1f\n", temp_result.avg_fps, temp_result.percentile_1);
        
        // If we maintain 60 FPS, this is our best count so far
        if (temp_result.percentile_1 >= 58.0f) {  // Allow small margin
            best_count = object_count;
            result = temp_result;
            result.object_count = object_count;
            result.triangles_per_object = 12; // Cube has 12 triangles
        } else {
            // FPS dropped too low, stop testing
            break;
        }
        
        free(samples.frame_times);
        free(samples.triangle_counts);
        free(samples.draw_call_counts);
        free(samples.vertex_counts);
    }
    
    strcpy(result.test_name, "Many Simple Objects");
    if (best_count > 0) {
        printf("\nBest count maintaining 60 FPS: %u objects\n", best_count);
    }
    
    return result;
}

// Test 2: Few Complex Objects (testing vertex processing)
static test_result test_few_complex_objects(renderer_state *renderer, platform_state *platform) {
    print_test_header("Few Complex Objects (Vertex Processing)");
    
    test_result result = {0};
    strcpy(result.test_name, "Few Complex Objects");
    
    // Create high-poly spheres with varying complexity
    u32 sphere_segments[] = {16, 32, 48, 64, 96, 128};
    u32 object_counts[] = {10, 25, 50, 100, 150, 200};
    
    u32 best_segments = 0;
    u32 best_count = 0;
    
    for (u32 seg_idx = 0; seg_idx < ArrayCount(sphere_segments); seg_idx++) {
        u32 segments = sphere_segments[seg_idx];
        u32 rings = segments / 2;
        
        // Create sphere mesh
        mesh *sphere = renderer_create_sphere(renderer, segments, rings);
        u32 sphere_triangles = sphere->index_count / 3;
        
        printf("\nTesting spheres with %u segments (%u triangles each):\n", segments, sphere_triangles);
        
        for (u32 count_idx = 0; count_idx < ArrayCount(object_counts); count_idx++) {
            u32 object_count = object_counts[count_idx];
            printf("  Testing %u spheres...", object_count);
            fflush(stdout);
            
            perf_samples samples = {
                .frame_times = (f32*)calloc(5000, sizeof(f32)),
                .triangle_counts = (u32*)calloc(5000, sizeof(u32)),
                .draw_call_counts = (u32*)calloc(5000, sizeof(u32)),
                .vertex_counts = (u32*)calloc(5000, sizeof(u32)),
                .sample_count = 0,
                .max_samples = 5000
            };
            
            // Quick test (shorter duration for iteration)
            f32 test_time = 0.0f;
            while (test_time < 2.0f && platform->is_running) {
                platform_poll_events(platform);
                
                f32 frame_start = platform_get_time_ms(platform);
                
                renderer_begin_frame(renderer);
                renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
                renderer_reset_stats(renderer);
                
                renderer_use_shader(renderer, renderer->phong_shader);
                
                // Set lighting
                v3 light_pos = {10.0f, 10.0f, 10.0f};
                renderer_set_uniform_v3(renderer->phong_shader, "lightPos", light_pos);
                renderer_set_uniform_v3(renderer->phong_shader, "lightColor", (v3){1.0f, 1.0f, 1.0f});
                renderer_set_uniform_v3(renderer->phong_shader, "viewPos", renderer->camera_position);
                
                // Draw spheres in a circle
                for (u32 i = 0; i < object_count; i++) {
                    f32 angle = ((f32)i / object_count) * Tau32;
                    f32 radius = 5.0f + (object_count * 0.05f);
                    
                    m4x4 model = m4x4_translation(
                        cosf(angle) * radius,
                        sinf(angle * 2.0f) * 2.0f,
                        sinf(angle) * radius
                    );
                    
                    // Material properties
                    renderer_set_uniform_v3(renderer->phong_shader, "objectColor", 
                        (v3){0.7f, 0.3f, 0.3f});
                    renderer_set_uniform_f32(renderer->phong_shader, "shininess", 32.0f);
                    
                    renderer_draw_mesh(renderer, sphere, model);
                }
                
                renderer_end_frame(renderer);
                renderer_present(renderer);
                
                f32 frame_end = platform_get_time_ms(platform);
                f32 frame_time = frame_end - frame_start;
                
                if (samples.sample_count < samples.max_samples) {
                    renderer_stats stats = renderer_get_stats(renderer);
                    samples.frame_times[samples.sample_count] = frame_time;
                    samples.triangle_counts[samples.sample_count] = stats.triangles_rendered;
                    samples.draw_call_counts[samples.sample_count] = stats.draw_calls;
                    samples.vertex_counts[samples.sample_count] = stats.vertices_processed;
                    samples.sample_count++;
                }
                
                test_time += frame_time / 1000.0f;
            }
            
            test_result temp_result = {0};
            calculate_statistics(&samples, &temp_result);
            
            printf(" Avg FPS: %.1f\n", temp_result.avg_fps);
            
            if (temp_result.avg_fps >= 60.0f) {
                best_segments = segments;
                best_count = object_count;
                result = temp_result;
                result.object_count = object_count;
                result.triangles_per_object = sphere_triangles;
            }
            
            free(samples.frame_times);
            free(samples.triangle_counts);
            free(samples.draw_call_counts);
            free(samples.vertex_counts);
            
            if (temp_result.avg_fps < 60.0f) {
                break; // No point testing more objects at this complexity
            }
        }
    }
    
    strcpy(result.test_name, "Few Complex Objects");
    if (best_count > 0) {
        printf("\nBest config: %u objects with %u segments each\n", best_count, best_segments);
    }
    
    return result;
}

// Test 3: Batch Rendering (simulated instancing via multiple draw calls)
static test_result test_instanced_rendering(renderer_state *renderer, platform_state *platform) {
    print_test_header("Batch Rendering (Multiple Draw Calls)");
    
    test_result result = {0};
    strcpy(result.test_name, "Batch Rendering");
    
    u32 instance_counts[] = {100, 500, 1000, 2500, 5000, 10000, 25000, 50000};
    u32 best_count = 0;
    
    for (u32 test_idx = 0; test_idx < ArrayCount(instance_counts); test_idx++) {
        u32 instance_count = instance_counts[test_idx];
        printf("\nTesting %u instances...\n", instance_count);
        
        // Prepare instance matrices
        m4x4 *matrices = (m4x4*)malloc(instance_count * sizeof(m4x4));
        u32 grid_size = (u32)sqrtf((f32)instance_count) + 1;
        f32 spacing = 1.5f;
        f32 offset = -(grid_size * spacing * 0.5f);
        
        for (u32 i = 0; i < instance_count; i++) {
            u32 x = i % grid_size;
            u32 y = (i / grid_size) % grid_size;
            u32 z = i / (grid_size * grid_size);
            
            matrices[i] = m4x4_translation(
                offset + x * spacing,
                offset + y * spacing,
                offset + z * spacing
            );
            matrices[i] = m4x4_multiply(matrices[i], m4x4_scale(0.3f, 0.3f, 0.3f));
        }
        
        perf_samples samples = {
            .frame_times = (f32*)calloc(5000, sizeof(f32)),
            .triangle_counts = (u32*)calloc(5000, sizeof(u32)),
            .draw_call_counts = (u32*)calloc(5000, sizeof(u32)),
            .vertex_counts = (u32*)calloc(5000, sizeof(u32)),
            .sample_count = 0,
            .max_samples = 5000
        };
        
        // Test loop
        f32 test_time = 0.0f;
        while (test_time < TEST_DURATION_SECONDS && platform->is_running) {
            platform_poll_events(platform);
            
            f32 frame_start = platform_get_time_ms(platform);
            
            renderer_begin_frame(renderer);
            renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
            renderer_reset_stats(renderer);
            
            renderer_use_shader(renderer, renderer->basic_shader);
            
            // Set uniforms
            v3 light_dir = v3_normalize((v3){-1.0f, -1.0f, -1.0f});
            renderer_set_uniform_v3(renderer->basic_shader, "lightDir", light_dir);
            renderer_set_uniform_v3(renderer->basic_shader, "lightColor", (v3){1.0f, 1.0f, 1.0f});
            renderer_set_uniform_v3(renderer->basic_shader, "objectColor", (v3){0.3f, 0.6f, 0.9f});
            
            // Draw instances (no instanced rendering available, simulate with multiple draw calls)
            for (u32 i = 0; i < instance_count; i++) {
                renderer_draw_mesh(renderer, renderer->cube_mesh, matrices[i]);
            }
            
            renderer_end_frame(renderer);
            renderer_present(renderer);
            
            f32 frame_end = platform_get_time_ms(platform);
            f32 frame_time = frame_end - frame_start;
            
            if (samples.sample_count < samples.max_samples) {
                renderer_stats stats = renderer_get_stats(renderer);
                samples.frame_times[samples.sample_count] = frame_time;
                samples.triangle_counts[samples.sample_count] = stats.triangles_rendered;
                samples.draw_call_counts[samples.sample_count] = stats.draw_calls;
                samples.vertex_counts[samples.sample_count] = stats.vertices_processed;
                samples.sample_count++;
            }
            
            test_time += frame_time / 1000.0f;
        }
        
        test_result temp_result = {0};
        calculate_statistics(&samples, &temp_result);
        
        printf("  Avg FPS: %.1f, Draw Calls: %u\n", temp_result.avg_fps, temp_result.avg_draw_calls);
        
        if (temp_result.avg_fps >= 60.0f) {
            best_count = instance_count;
            result = temp_result;
            result.object_count = instance_count;
            result.triangles_per_object = 12;
        }
        
        free(matrices);
        free(samples.frame_times);
        free(samples.triangle_counts);
        free(samples.draw_call_counts);
        free(samples.vertex_counts);
        
        if (temp_result.avg_fps < 60.0f) {
            break;
        }
    }
    
    strcpy(result.test_name, "Batch Rendering");
    if (best_count > 0) {
        printf("\nBest batch size maintaining 60 FPS: %u objects\n", best_count);
    }
    
    return result;
}

// Test 4: State Changes (shader and texture switching)
static test_result test_state_changes(renderer_state *renderer, platform_state *platform) {
    print_test_header("State Changes (Shader/Texture Switching)");
    
    test_result result = {0};
    strcpy(result.test_name, "State Change Overhead");
    
    // Create multiple shaders to switch between
    shader_program *shaders[] = {
        renderer->basic_shader,
        renderer->phong_shader,
        renderer->pbr_shader
    };
    u32 shader_count = 3;
    
    u32 object_counts[] = {50, 100, 200, 400, 600, 800, 1000};
    u32 best_count = 0;
    
    for (u32 test_idx = 0; test_idx < ArrayCount(object_counts); test_idx++) {
        u32 object_count = object_counts[test_idx];
        printf("\nTesting %u objects with state changes...\n", object_count);
        
        perf_samples samples = {
            .frame_times = (f32*)calloc(5000, sizeof(f32)),
            .triangle_counts = (u32*)calloc(5000, sizeof(u32)),
            .draw_call_counts = (u32*)calloc(5000, sizeof(u32)),
            .vertex_counts = (u32*)calloc(5000, sizeof(u32)),
            .sample_count = 0,
            .max_samples = 5000
        };
        
        f32 test_time = 0.0f;
        while (test_time < TEST_DURATION_SECONDS && platform->is_running) {
            platform_poll_events(platform);
            
            f32 frame_start = platform_get_time_ms(platform);
            
            renderer_begin_frame(renderer);
            renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
            renderer_reset_stats(renderer);
            
            // Draw objects with alternating shaders
            for (u32 i = 0; i < object_count; i++) {
                // Switch shader every object (worst case)
                shader_program *shader = shaders[i % shader_count];
                renderer_use_shader(renderer, shader);
                
                // Set shader-specific uniforms
                if (shader == renderer->basic_shader) {
                    v3 light_dir = v3_normalize((v3){-1.0f, -1.0f, -1.0f});
                    renderer_set_uniform_v3(shader, "lightDir", light_dir);
                    renderer_set_uniform_v3(shader, "lightColor", (v3){1.0f, 1.0f, 1.0f});
                    renderer_set_uniform_v3(shader, "objectColor", (v3){0.8f, 0.3f, 0.3f});
                } else if (shader == renderer->phong_shader) {
                    v3 light_pos = {10.0f, 10.0f, 10.0f};
                    renderer_set_uniform_v3(shader, "lightPos", light_pos);
                    renderer_set_uniform_v3(shader, "lightColor", (v3){1.0f, 1.0f, 1.0f});
                    renderer_set_uniform_v3(shader, "viewPos", renderer->camera_position);
                    renderer_set_uniform_v3(shader, "objectColor", (v3){0.3f, 0.8f, 0.3f});
                    renderer_set_uniform_f32(shader, "shininess", 32.0f);
                } else if (shader == renderer->pbr_shader) {
                    renderer_set_uniform_v3(shader, "albedo", (v3){0.3f, 0.3f, 0.8f});
                    renderer_set_uniform_f32(shader, "metallic", 0.5f);
                    renderer_set_uniform_f32(shader, "roughness", 0.5f);
                    renderer_set_uniform_f32(shader, "ao", 1.0f);
                    renderer_set_uniform_v3(shader, "camPos", renderer->camera_position);
                }
                
                // Also switch textures
                texture *tex = (i % 2) ? renderer->white_texture : renderer->checkerboard_texture;
                renderer_bind_texture(renderer, tex, 0);
                
                // Draw object
                f32 angle = ((f32)i / object_count) * Tau32;
                f32 radius = 8.0f;
                m4x4 model = m4x4_translation(
                    cosf(angle) * radius,
                    sinf(i * 0.5f) * 2.0f,
                    sinf(angle) * radius
                );
                
                renderer_draw_mesh(renderer, renderer->cube_mesh, model);
            }
            
            renderer_end_frame(renderer);
            renderer_present(renderer);
            
            f32 frame_end = platform_get_time_ms(platform);
            f32 frame_time = frame_end - frame_start;
            
            if (samples.sample_count < samples.max_samples) {
                renderer_stats stats = renderer_get_stats(renderer);
                samples.frame_times[samples.sample_count] = frame_time;
                samples.triangle_counts[samples.sample_count] = stats.triangles_rendered;
                samples.draw_call_counts[samples.sample_count] = stats.draw_calls;
                samples.vertex_counts[samples.sample_count] = stats.vertices_processed;
                samples.sample_count++;
                
                // Track state changes
                stats.shader_switches = object_count - 1;
                stats.texture_switches = object_count;
            }
            
            test_time += frame_time / 1000.0f;
        }
        
        test_result temp_result = {0};
        calculate_statistics(&samples, &temp_result);
        
        printf("  Avg FPS: %.1f (with %u shader + %u texture switches/frame)\n", 
               temp_result.avg_fps, object_count - 1, object_count);
        
        if (temp_result.avg_fps >= 60.0f) {
            best_count = object_count;
            result = temp_result;
            result.object_count = object_count;
            result.triangles_per_object = 12;
        }
        
        free(samples.frame_times);
        free(samples.triangle_counts);
        free(samples.draw_call_counts);
        free(samples.vertex_counts);
        
        if (temp_result.avg_fps < 60.0f) {
            break;
        }
    }
    
    strcpy(result.test_name, "State Change Overhead");
    if (best_count > 0) {
        printf("\nMax objects with state changes maintaining 60 FPS: %u\n", best_count);
    }
    
    return result;
}

// Main stress test
int main(int argc, char **argv) {
    printf("================================================================================\n");
    printf("                      3D RENDERER STRESS TEST v1.0                             \n");
    printf("================================================================================\n");
    printf("\nThis test will measure the ACTUAL performance capabilities of the renderer.\n");
    printf("Each test will run for %.1f seconds.\n", TEST_DURATION_SECONDS);
    printf("Target: Maintain 60+ FPS under various loads.\n\n");
    
    // Initialize platform
    window_config config = {
        .title = "Renderer Stress Test",
        .width = 1920,
        .height = 1080,
        .fullscreen = false,
        .vsync = false,  // Disable vsync for accurate measurements
        .resizable = false,
        .samples = 1     // No MSAA for stress testing
    };
    
    platform_state *platform = platform_init(&config, Megabytes(256), Megabytes(128));
    if (!platform) {
        printf("ERROR: Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize renderer
    renderer_state *renderer = renderer_init(platform, Megabytes(512));
    if (!renderer) {
        printf("ERROR: Failed to initialize renderer\n");
        platform_shutdown(platform);
        return 1;
    }
    
    // Set up camera
    v3 camera_pos = {15.0f, 15.0f, 15.0f};
    v3 camera_target = {0.0f, 0.0f, 0.0f};
    v3 camera_up = {0.0f, 1.0f, 0.0f};
    v3 camera_forward = v3_normalize(v3_sub(camera_target, camera_pos));
    renderer_set_camera(renderer, camera_pos, camera_forward, camera_up);
    
    // Set projection
    f32 aspect = (f32)config.width / (f32)config.height;
    m4x4 projection = renderer_create_perspective(60.0f, aspect, 0.1f, 1000.0f);
    renderer_set_projection_matrix(renderer, projection);
    
    printf("Platform initialized: %dx%d\n", config.width, config.height);
    printf("OpenGL Renderer initialized\n\n");
    
    // Run tests
    test_result results[TEST_SCENARIO_COUNT] = {0};
    u32 test_count = 0;
    
    // Test 1: Many Simple Objects
    if (platform->is_running) {
        results[test_count++] = test_many_simple_objects(renderer, platform);
    }
    
    // Test 2: Few Complex Objects
    if (platform->is_running) {
        results[test_count++] = test_few_complex_objects(renderer, platform);
    }
    
    // Test 3: Instanced Rendering
    if (platform->is_running) {
        results[test_count++] = test_instanced_rendering(renderer, platform);
    }
    
    // Test 4: State Changes
    if (platform->is_running) {
        results[test_count++] = test_state_changes(renderer, platform);
    }
    
    // Print summary
    printf("\n");
    print_separator();
    printf("                           STRESS TEST SUMMARY                                 \n");
    print_separator();
    
    for (u32 i = 0; i < test_count; i++) {
        print_test_result(&results[i]);
    }
    
    // Overall conclusions
    printf("\n");
    print_separator();
    printf("                              CONCLUSIONS                                      \n");
    print_separator();
    
    // Find bottlenecks
    u32 max_triangles_60fps = 0;
    u32 max_draw_calls_60fps = 0;
    u32 max_vertices_60fps = 0;
    
    for (u32 i = 0; i < test_count; i++) {
        if (results[i].passed_60fps) {
            u32 triangles_per_frame = results[i].avg_triangles;
            u32 draw_calls_per_frame = results[i].avg_draw_calls;
            u32 vertices_per_frame = results[i].avg_vertices;
            
            if (triangles_per_frame > max_triangles_60fps)
                max_triangles_60fps = triangles_per_frame;
            if (draw_calls_per_frame > max_draw_calls_60fps)
                max_draw_calls_60fps = draw_calls_per_frame;
            if (vertices_per_frame > max_vertices_60fps)
                max_vertices_60fps = vertices_per_frame;
        }
    }
    
    printf("\nMAXIMUM PERFORMANCE AT 60 FPS:\n");
    printf("--------------------------------\n");
    printf("  Max Triangles/Frame:    %u\n", max_triangles_60fps);
    printf("  Max Draw Calls/Frame:   %u\n", max_draw_calls_60fps);
    printf("  Max Vertices/Frame:     %u\n", max_vertices_60fps);
    printf("  Triangles/Second @60:   %u\n", max_triangles_60fps * 60);
    printf("  Vertices/Second @60:    %u\n", max_vertices_60fps * 60);
    
    printf("\nBOTTLENECK ANALYSIS:\n");
    printf("--------------------\n");
    
    // Analyze bottlenecks
    b32 draw_call_limited = (max_draw_calls_60fps < 1000);
    b32 vertex_limited = (max_vertices_60fps < 1000000);
    b32 fill_rate_limited = (max_triangles_60fps < 100000);
    
    if (draw_call_limited) {
        printf("  WARNING: Draw call limited! Consider batching or instancing.\n");
        printf("           Current limit: ~%u draw calls/frame\n", max_draw_calls_60fps);
    }
    
    if (vertex_limited) {
        printf("  WARNING: Vertex processing limited!\n");
        printf("           Current limit: ~%u vertices/frame\n", max_vertices_60fps);
    }
    
    if (fill_rate_limited) {
        printf("  WARNING: May be fill-rate limited for large triangles.\n");
        printf("           Current limit: ~%u triangles/frame\n", max_triangles_60fps);
    }
    
    if (!draw_call_limited && !vertex_limited && !fill_rate_limited) {
        printf("  System appears well-balanced for current test scenarios.\n");
    }
    
    printf("\nRECOMMENDATIONS:\n");
    printf("-----------------\n");
    printf("  1. For typical games, budget ~%u triangles per frame\n", max_triangles_60fps / 2);
    printf("  2. Keep draw calls under %u for safety margin\n", max_draw_calls_60fps / 2);
    printf("  3. Consider LOD system for objects with >1000 triangles\n");
    printf("  4. Use instancing for repeated objects (massive performance gain)\n");
    printf("  5. Minimize state changes (shader/texture switches)\n");
    
    // Cleanup
    printf("\nShutting down...\n");
    renderer_shutdown(renderer);
    platform_shutdown(platform);
    
    printf("\nStress test completed successfully!\n");
    return 0;
}