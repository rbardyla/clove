/*
    3D Renderer Test
    Demonstrates the handmade OpenGL rendering pipeline
*/

#include "handmade_renderer.h"
#include "handmade_platform.h"
#include "handmade_opengl.h"
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

int main() {
    printf("=== Handmade 3D Renderer Test ===\n\n");
    
    // Initialize platform
    window_config config = {
        .title = "Handmade 3D Renderer",
        .width = 1280,
        .height = 720,
        .fullscreen = false,
        .vsync = true,
        .resizable = true,
        .samples = 4
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
    v3 camera_pos = {5.0f, 5.0f, 5.0f};
    v3 camera_target = {0.0f, 0.0f, 0.0f};
    v3 camera_up = {0.0f, 1.0f, 0.0f};
    v3 camera_forward = v3_normalize(v3_sub(camera_target, camera_pos));
    renderer_set_camera(renderer, camera_pos, camera_forward, camera_up);
    
    // Create some test objects
    printf("\nCreating test scene...\n");
    
    // Main loop
    f32 time = 0.0f;
    while (platform->is_running) {
        // Poll events
        platform_poll_events(platform);
        
        // Handle input
        if (platform_key_pressed(platform, KEY_ESCAPE)) {
            platform->is_running = false;
        }
        
        // Toggle wireframe
        if (platform_key_pressed(platform, KEY_W)) {
            renderer->wireframe_mode = !renderer->wireframe_mode;
            // glPolygonMode(GL_FRONT_AND_BACK, renderer->wireframe_mode ? GL_LINE : GL_FILL);
            // Wireframe mode toggle (commented out - needs GL extension)
        }
        
        // Camera controls
        f32 dt = platform_get_dt(platform);
        f32 camera_speed = 5.0f * dt;
        f32 rotation_speed = 2.0f * dt;
        
        if (platform_key_down(platform, KEY_LEFT)) {
            time -= rotation_speed;
        }
        if (platform_key_down(platform, KEY_RIGHT)) {
            time += rotation_speed;
        }
        
        // Orbit camera
        f32 radius = 8.0f;
        camera_pos.x = cosf(time) * radius;
        camera_pos.z = sinf(time) * radius;
        camera_pos.y = 5.0f;
        
        camera_forward = v3_normalize(v3_sub(camera_target, camera_pos));
        renderer_set_camera(renderer, camera_pos, camera_forward, camera_up);
        
        // Begin frame
        renderer_begin_frame(renderer);
        
        // Clear
        renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
        
        // Use basic shader
        renderer_use_shader(renderer, renderer->basic_shader);
        
        // Set light
        v3 light_dir = v3_normalize((v3){-1.0f, -1.0f, -1.0f});
        v3 light_color = {1.0f, 1.0f, 1.0f};
        renderer_set_uniform_v3(renderer->basic_shader, "lightDir", light_dir);
        renderer_set_uniform_v3(renderer->basic_shader, "lightColor", light_color);
        
        // Draw cube at center
        m4x4 model = m4x4_identity();
        model = m4x4_multiply(model, m4x4_rotation_y(time * 0.5f));
        renderer_set_uniform_v3(renderer->basic_shader, "objectColor", (v3){0.8f, 0.3f, 0.3f});
        renderer_draw_mesh(renderer, renderer->cube_mesh, model);
        
        // Draw orbiting spheres
        for (i32 i = 0; i < 4; i++) {
            f32 angle = (f32)i * (PI * 0.5f) + time;
            f32 orbit_radius = 3.0f;
            
            model = m4x4_translation(
                cosf(angle) * orbit_radius,
                sinf(time * 2.0f + (f32)i) * 0.5f,
                sinf(angle) * orbit_radius
            );
            model = m4x4_multiply(model, m4x4_scale(0.5f, 0.5f, 0.5f));
            
            v3 colors[] = {
                {0.3f, 0.8f, 0.3f},
                {0.3f, 0.3f, 0.8f},
                {0.8f, 0.8f, 0.3f},
                {0.8f, 0.3f, 0.8f}
            };
            
            renderer_set_uniform_v3(renderer->basic_shader, "objectColor", colors[i]);
            renderer_draw_mesh(renderer, renderer->sphere_mesh, model);
        }
        
        // Draw ground grid
        renderer_draw_grid(renderer, 10.0f, 20, (v3){0.3f, 0.3f, 0.3f});
        
        // Draw coordinate axes at origin
        renderer_draw_line(renderer, (v3){0,0,0}, (v3){2,0,0}, (v3){1,0,0}); // X - Red
        renderer_draw_line(renderer, (v3){0,0,0}, (v3){0,2,0}, (v3){0,1,0}); // Y - Green
        renderer_draw_line(renderer, (v3){0,0,0}, (v3){0,0,2}, (v3){0,0,1}); // Z - Blue
        
        // End frame
        renderer_end_frame(renderer);
        renderer_present(renderer);
        
        // Print stats every second
        static f32 stat_timer = 0.0f;
        stat_timer += dt;
        if (stat_timer >= 1.0f) {
            renderer_stats stats = renderer_get_stats(renderer);
            printf("FPS: %.1f | Draw Calls: %u | Triangles: %u | Vertices: %u\n",
                   1.0f / dt, stats.draw_calls, stats.triangles_rendered, stats.vertices_processed);
            stat_timer = 0.0f;
        }
        
        time += dt;
    }
    
    // Cleanup
    printf("\nShutting down...\n");
    renderer_shutdown(renderer);
    platform_shutdown(platform);
    
    printf("✓ Renderer test completed successfully!\n");
    return 0;
}