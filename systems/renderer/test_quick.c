/*
    Quick renderer capability test
    Runs a brief test to verify the renderer performance measurement system works
*/

#define _GNU_SOURCE
#include "handmade_renderer.h"
#include "handmade_platform.h"
#include "handmade_opengl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

int main() {
    printf("=== Quick Renderer Capability Test ===\n\n");
    
    // Initialize platform
    window_config config = {
        .title = "Renderer Quick Test",
        .width = 1280,
        .height = 720,
        .fullscreen = false,
        .vsync = false,  // Disable vsync for measurements
        .resizable = false,
        .samples = 1
    };
    
    platform_state *platform = platform_init(&config, Megabytes(64), Megabytes(32));
    if (!platform) {
        printf("Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize renderer
    renderer_state *renderer = renderer_init(platform, Megabytes(128));
    if (!renderer) {
        printf("Failed to initialize renderer\n");
        platform_shutdown(platform);
        return 1;
    }
    
    // Set up camera
    v3 camera_pos = {10.0f, 10.0f, 10.0f};
    v3 camera_target = {0.0f, 0.0f, 0.0f};
    v3 camera_up = {0.0f, 1.0f, 0.0f};
    v3 camera_forward = v3_normalize(v3_sub(camera_target, camera_pos));
    renderer_set_camera(renderer, camera_pos, camera_forward, camera_up);
    
    // Set projection
    f32 aspect = (f32)config.width / (f32)config.height;
    m4x4 projection = renderer_create_perspective(60.0f, aspect, 0.1f, 100.0f);
    renderer_set_projection_matrix(renderer, projection);
    
    printf("Testing various object counts...\n\n");
    
    // Test different object counts
    u32 test_counts[] = {10, 50, 100, 250, 500, 1000, 2000};
    
    for (u32 test_idx = 0; test_idx < sizeof(test_counts)/sizeof(test_counts[0]); test_idx++) {
        u32 object_count = test_counts[test_idx];
        
        // Run for 1 second
        f32 total_time = 0.0f;
        u32 frame_count = 0;
        f32 min_frame_time = 1000.0f;
        f32 max_frame_time = 0.0f;
        u32 total_triangles = 0;
        u32 total_draw_calls = 0;
        
        while (total_time < 1.0f && platform->is_running) {
            platform_poll_events(platform);
            
            f32 frame_start = platform_get_time(platform);
            
            renderer_begin_frame(renderer);
            renderer_clear(renderer, (v4){{0.1f, 0.1f, 0.15f, 1.0f}}, true, true);
            renderer_reset_stats(renderer);
            
            // Use basic shader
            renderer_use_shader(renderer, renderer->basic_shader);
            
            // Set light
            v3 light_dir = v3_normalize((v3){{-1.0f, -1.0f, -1.0f}});
            renderer_set_uniform_v3(renderer->basic_shader, "lightDir", light_dir);
            renderer_set_uniform_v3(renderer->basic_shader, "lightColor", (v3){{1.0f, 1.0f, 1.0f}});
            
            // Draw objects
            u32 grid_size = (u32)sqrtf((f32)object_count) + 1;
            f32 spacing = 1.5f;
            f32 offset = -(grid_size * spacing * 0.5f);
            
            for (u32 i = 0; i < object_count; i++) {
                u32 x = i % grid_size;
                u32 z = i / grid_size;
                
                m4x4 model = m4x4_translation(
                    offset + x * spacing,
                    0.0f,
                    offset + z * spacing
                );
                model = m4x4_multiply(model, m4x4_scale(0.4f, 0.4f, 0.4f));
                
                f32 r = (f32)x / grid_size;
                f32 g = (f32)z / grid_size;
                renderer_set_uniform_v3(renderer->basic_shader, "objectColor", (v3){{r, g, 0.5f}});
                
                renderer_draw_mesh(renderer, renderer->cube_mesh, model);
            }
            
            renderer_end_frame(renderer);
            renderer_present(renderer);
            
            f32 frame_end = platform_get_time(platform);
            f32 frame_time = (frame_end - frame_start) * 1000.0f; // Convert to ms
            
            renderer_stats stats = renderer_get_stats(renderer);
            total_triangles += stats.triangles_rendered;
            total_draw_calls += stats.draw_calls;
            
            if (frame_time < min_frame_time) min_frame_time = frame_time;
            if (frame_time > max_frame_time) max_frame_time = frame_time;
            
            frame_count++;
            total_time += frame_time / 1000.0f;
        }
        
        // Calculate results
        f32 avg_frame_time = (total_time * 1000.0f) / frame_count;
        f32 avg_fps = 1000.0f / avg_frame_time;
        u32 avg_triangles = total_triangles / frame_count;
        u32 avg_draw_calls = total_draw_calls / frame_count;
        
        printf("%4u objects: %6.1f FPS (%.2f-%.2f ms) | %6u triangles | %4u draw calls",
               object_count, avg_fps, min_frame_time, max_frame_time, 
               avg_triangles, avg_draw_calls);
        
        if (avg_fps < 60.0f) {
            printf(" <- Below 60 FPS!\n");
            break;
        } else {
            printf("\n");
        }
    }
    
    printf("\n=== Quick Test Summary ===\n");
    printf("Renderer appears to be working correctly.\n");
    printf("For detailed performance analysis, run: ./renderer_stress_test\n\n");
    
    // Cleanup
    renderer_shutdown(renderer);
    platform_shutdown(platform);
    
    return 0;
}