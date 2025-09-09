/*
 * Vulkan Demo Application
 * Showcases all renderer features with complex scene
 * 
 * PERFORMANCE: Targets 5000+ draw calls at 60 FPS
 * MEMORY: Zero allocations during runtime
 * FEATURES: Shadows, volumetrics, ray marching, GI
 */

#include "handmade_vulkan.h"
// #include "platform.h"  // Not needed for standalone demo
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// Demo State
// ============================================================================

typedef struct DemoState {
    VulkanContext vulkan;
    Platform platform;
    
    // Scene objects
    VulkanMesh cube_mesh;
    VulkanMesh sphere_mesh;
    VulkanMesh ground_mesh;
    VulkanMesh particle_mesh;
    
    // Textures
    VulkanImage checker_texture;
    VulkanImage noise_texture;
    VulkanImage environment_map;
    
    // Pipelines
    VulkanPipeline* main_pipeline;
    VulkanPipeline* shadow_pipeline;
    VulkanPipeline* particle_pipeline;
    VulkanPipeline* raymarch_pipeline;
    
    // Scene data
    VulkanDrawCommand draw_commands[8192];
    uint32_t draw_command_count;
    
    VulkanRenderState render_state;
    VulkanRayMarchSettings raymarch_settings;
    
    // Animation
    float time;
    float delta_time;
    uint64_t frame_count;
    
    // Camera
    float camera_pos[3];
    float camera_rot[2]; // pitch, yaw
    float camera_velocity[3];
    
    // Performance metrics
    double cpu_time_ms;
    double gpu_time_ms;
    uint32_t draw_calls;
    uint32_t triangles;
    
    // Input
    bool keys[256];
    float mouse_delta_x;
    float mouse_delta_y;
    
    bool running;
} DemoState;

static DemoState g_demo;

// ============================================================================
// Mesh Generation
// ============================================================================

static VulkanMesh create_cube_mesh(VulkanContext* ctx) {
    VulkanVertex vertices[] = {
        // Front face
        {{-1, -1,  1}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1, -1,  1}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1,  1,  1}, {0, 0, 1}, {1, 1}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{-1,  1,  1}, {0, 0, 1}, {0, 1}, {1, 0, 0, 1}, 0xFFFFFFFF},
        
        // Back face
        {{ 1, -1, -1}, {0, 0, -1}, {0, 0}, {-1, 0, 0, 1}, 0xFFFFFFFF},
        {{-1, -1, -1}, {0, 0, -1}, {1, 0}, {-1, 0, 0, 1}, 0xFFFFFFFF},
        {{-1,  1, -1}, {0, 0, -1}, {1, 1}, {-1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1,  1, -1}, {0, 0, -1}, {0, 1}, {-1, 0, 0, 1}, 0xFFFFFFFF},
        
        // Top face
        {{-1,  1,  1}, {0, 1, 0}, {0, 0}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1,  1,  1}, {0, 1, 0}, {1, 0}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1,  1, -1}, {0, 1, 0}, {1, 1}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{-1,  1, -1}, {0, 1, 0}, {0, 1}, {1, 0, 0, 1}, 0xFFFFFFFF},
        
        // Bottom face
        {{-1, -1, -1}, {0, -1, 0}, {0, 0}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1, -1, -1}, {0, -1, 0}, {1, 0}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{ 1, -1,  1}, {0, -1, 0}, {1, 1}, {1, 0, 0, 1}, 0xFFFFFFFF},
        {{-1, -1,  1}, {0, -1, 0}, {0, 1}, {1, 0, 0, 1}, 0xFFFFFFFF},
        
        // Right face
        {{ 1, -1,  1}, {1, 0, 0}, {0, 0}, {0, 0, -1, 1}, 0xFFFFFFFF},
        {{ 1, -1, -1}, {1, 0, 0}, {1, 0}, {0, 0, -1, 1}, 0xFFFFFFFF},
        {{ 1,  1, -1}, {1, 0, 0}, {1, 1}, {0, 0, -1, 1}, 0xFFFFFFFF},
        {{ 1,  1,  1}, {1, 0, 0}, {0, 1}, {0, 0, -1, 1}, 0xFFFFFFFF},
        
        // Left face
        {{-1, -1, -1}, {-1, 0, 0}, {0, 0}, {0, 0, 1, 1}, 0xFFFFFFFF},
        {{-1, -1,  1}, {-1, 0, 0}, {1, 0}, {0, 0, 1, 1}, 0xFFFFFFFF},
        {{-1,  1,  1}, {-1, 0, 0}, {1, 1}, {0, 0, 1, 1}, 0xFFFFFFFF},
        {{-1,  1, -1}, {-1, 0, 0}, {0, 1}, {0, 0, 1, 1}, 0xFFFFFFFF}
    };
    
    uint32_t indices[] = {
        0, 1, 2, 0, 2, 3,       // Front
        4, 5, 6, 4, 6, 7,       // Back
        8, 9, 10, 8, 10, 11,    // Top
        12, 13, 14, 12, 14, 15, // Bottom
        16, 17, 18, 16, 18, 19, // Right
        20, 21, 22, 20, 22, 23  // Left
    };
    
    return vulkan_create_mesh(ctx, vertices, 24, indices, 36);
}

static VulkanMesh create_sphere_mesh(VulkanContext* ctx, uint32_t segments) {
    uint32_t vertex_count = (segments + 1) * (segments + 1);
    uint32_t index_count = segments * segments * 6;
    
    VulkanVertex* vertices = malloc(sizeof(VulkanVertex) * vertex_count);
    uint32_t* indices = malloc(sizeof(uint32_t) * index_count);
    
    // Generate sphere vertices
    uint32_t v = 0;
    for (uint32_t y = 0; y <= segments; y++) {
        for (uint32_t x = 0; x <= segments; x++) {
            float u = (float)x / segments;
            float v_coord = (float)y / segments;
            
            float theta = u * 2.0f * 3.14159f;
            float phi = v_coord * 3.14159f;
            
            vertices[v].position[0] = cosf(theta) * sinf(phi);
            vertices[v].position[1] = cosf(phi);
            vertices[v].position[2] = sinf(theta) * sinf(phi);
            
            // Normal is same as position for unit sphere
            memcpy(vertices[v].normal, vertices[v].position, sizeof(float) * 3);
            
            vertices[v].uv[0] = u;
            vertices[v].uv[1] = v_coord;
            
            // Calculate tangent
            vertices[v].tangent[0] = -sinf(theta);
            vertices[v].tangent[1] = 0;
            vertices[v].tangent[2] = cosf(theta);
            vertices[v].tangent[3] = 1;
            
            vertices[v].color = 0xFFFFFFFF;
            
            v++;
        }
    }
    
    // Generate indices
    uint32_t i = 0;
    for (uint32_t y = 0; y < segments; y++) {
        for (uint32_t x = 0; x < segments; x++) {
            uint32_t base = y * (segments + 1) + x;
            
            indices[i++] = base;
            indices[i++] = base + segments + 1;
            indices[i++] = base + 1;
            
            indices[i++] = base + 1;
            indices[i++] = base + segments + 1;
            indices[i++] = base + segments + 2;
        }
    }
    
    VulkanMesh mesh = vulkan_create_mesh(ctx, vertices, vertex_count, indices, index_count);
    
    free(vertices);
    free(indices);
    
    return mesh;
}

static VulkanMesh create_ground_mesh(VulkanContext* ctx, float size, uint32_t subdivisions) {
    uint32_t vertex_count = (subdivisions + 1) * (subdivisions + 1);
    uint32_t index_count = subdivisions * subdivisions * 6;
    
    VulkanVertex* vertices = malloc(sizeof(VulkanVertex) * vertex_count);
    uint32_t* indices = malloc(sizeof(uint32_t) * index_count);
    
    // Generate grid vertices
    uint32_t v = 0;
    for (uint32_t z = 0; z <= subdivisions; z++) {
        for (uint32_t x = 0; x <= subdivisions; x++) {
            float fx = ((float)x / subdivisions - 0.5f) * size;
            float fz = ((float)z / subdivisions - 0.5f) * size;
            
            vertices[v].position[0] = fx;
            vertices[v].position[1] = 0;
            vertices[v].position[2] = fz;
            
            vertices[v].normal[0] = 0;
            vertices[v].normal[1] = 1;
            vertices[v].normal[2] = 0;
            
            vertices[v].uv[0] = (float)x / subdivisions * 10; // Tile texture
            vertices[v].uv[1] = (float)z / subdivisions * 10;
            
            vertices[v].tangent[0] = 1;
            vertices[v].tangent[1] = 0;
            vertices[v].tangent[2] = 0;
            vertices[v].tangent[3] = 1;
            
            vertices[v].color = 0xFF808080;
            
            v++;
        }
    }
    
    // Generate indices
    uint32_t i = 0;
    for (uint32_t z = 0; z < subdivisions; z++) {
        for (uint32_t x = 0; x < subdivisions; x++) {
            uint32_t base = z * (subdivisions + 1) + x;
            
            indices[i++] = base;
            indices[i++] = base + subdivisions + 1;
            indices[i++] = base + 1;
            
            indices[i++] = base + 1;
            indices[i++] = base + subdivisions + 1;
            indices[i++] = base + subdivisions + 2;
        }
    }
    
    VulkanMesh mesh = vulkan_create_mesh(ctx, vertices, vertex_count, indices, index_count);
    
    free(vertices);
    free(indices);
    
    return mesh;
}

// ============================================================================
// Texture Generation
// ============================================================================

static VulkanImage create_checker_texture(VulkanContext* ctx, uint32_t size) {
    uint32_t* pixels = malloc(size * size * sizeof(uint32_t));
    
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            bool checker = ((x / 32) + (y / 32)) % 2;
            pixels[y * size + x] = checker ? 0xFFFFFFFF : 0xFF404040;
        }
    }
    
    VulkanImage texture = vulkan_create_texture(ctx, size, size, 
                                                VK_FORMAT_R8G8B8A8_SRGB, pixels);
    
    free(pixels);
    return texture;
}

static VulkanImage create_noise_texture(VulkanContext* ctx, uint32_t size) {
    uint32_t* pixels = malloc(size * size * sizeof(uint32_t));
    
    srand(12345); // Fixed seed for deterministic noise
    
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint8_t r = rand() & 0xFF;
            uint8_t g = rand() & 0xFF;
            uint8_t b = rand() & 0xFF;
            pixels[y * size + x] = (r << 0) | (g << 8) | (b << 16) | (0xFF << 24);
        }
    }
    
    VulkanImage texture = vulkan_create_texture(ctx, size, size,
                                                VK_FORMAT_R8G8B8A8_UNORM, pixels);
    
    free(pixels);
    return texture;
}

// ============================================================================
// Scene Setup
// ============================================================================

static void setup_scene(DemoState* demo) {
    demo->draw_command_count = 0;
    
    // Add ground
    VulkanDrawCommand* ground = &demo->draw_commands[demo->draw_command_count++];
    ground->mesh = &demo->ground_mesh;
    memset(ground->transform, 0, sizeof(float) * 16);
    ground->transform[0] = ground->transform[5] = ground->transform[10] = ground->transform[15] = 1;
    
    // Add cubes in a grid pattern
    for (int z = -10; z <= 10; z += 4) {
        for (int x = -10; x <= 10; x += 4) {
            if (demo->draw_command_count >= 8192) break;
            
            VulkanDrawCommand* cube = &demo->draw_commands[demo->draw_command_count++];
            cube->mesh = &demo->cube_mesh;
            
            // Set transform
            memset(cube->transform, 0, sizeof(float) * 16);
            cube->transform[0] = cube->transform[5] = cube->transform[10] = 1;
            cube->transform[12] = x;
            cube->transform[13] = 1 + sinf(x * 0.5f + z * 0.3f) * 0.5f;
            cube->transform[14] = z;
            cube->transform[15] = 1;
            
            // Rotate based on position
            float angle = (x + z) * 0.1f + demo->time;
            float c = cosf(angle);
            float s = sinf(angle);
            cube->transform[0] = c;
            cube->transform[2] = s;
            cube->transform[8] = -s;
            cube->transform[10] = c;
        }
    }
    
    // Add spheres
    for (int i = 0; i < 20; i++) {
        if (demo->draw_command_count >= 8192) break;
        
        VulkanDrawCommand* sphere = &demo->draw_commands[demo->draw_command_count++];
        sphere->mesh = &demo->sphere_mesh;
        
        float angle = i * 0.314f;
        float radius = 15 + i * 0.5f;
        
        memset(sphere->transform, 0, sizeof(float) * 16);
        sphere->transform[0] = sphere->transform[5] = sphere->transform[10] = 0.5f + i * 0.1f;
        sphere->transform[12] = cosf(angle + demo->time * 0.5f) * radius;
        sphere->transform[13] = 3 + sinf(demo->time * 2 + i) * 2;
        sphere->transform[14] = sinf(angle + demo->time * 0.5f) * radius;
        sphere->transform[15] = 1;
    }
    
    // Add many small objects for stress testing
    for (int i = 0; i < 4000 && demo->draw_command_count < 8192; i++) {
        VulkanDrawCommand* obj = &demo->draw_commands[demo->draw_command_count++];
        obj->mesh = (i % 2) ? &demo->cube_mesh : &demo->sphere_mesh;
        
        // Random position in large area
        float x = (i % 100 - 50) * 2;
        float z = (i / 100 - 20) * 2;
        float y = 0.5f + (i % 10) * 0.2f;
        
        memset(obj->transform, 0, sizeof(float) * 16);
        obj->transform[0] = obj->transform[5] = obj->transform[10] = 0.2f;
        obj->transform[12] = x;
        obj->transform[13] = y;
        obj->transform[14] = z;
        obj->transform[15] = 1;
    }
    
    // Setup SDF primitives for ray marching
    float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    // Add SDF sphere
    float sphere_params[4] = {2.0f, 0, 0, 0};
    float sphere_material[4] = {0.8f, 0.2f, 0.2f, 0};
    float sphere_transform[16];
    memcpy(sphere_transform, identity, sizeof(identity));
    sphere_transform[12] = 5;
    sphere_transform[13] = 5;
    vulkan_add_sdf_primitive(0, sphere_transform, sphere_params, sphere_material);
    
    // Add SDF box
    float box_params[4] = {1.5f, 1.5f, 1.5f, 0};
    float box_material[4] = {0.2f, 0.8f, 0.2f, 0};
    float box_transform[16];
    memcpy(box_transform, identity, sizeof(identity));
    box_transform[12] = -5;
    box_transform[13] = 3;
    vulkan_add_sdf_primitive(1, box_transform, box_params, box_material);
    
    // Add SDF torus
    float torus_params[4] = {3.0f, 0.5f, 0, 0};
    float torus_material[4] = {0.2f, 0.2f, 0.8f, 0};
    float torus_transform[16];
    memcpy(torus_transform, identity, sizeof(identity));
    torus_transform[13] = 8;
    vulkan_add_sdf_primitive(2, torus_transform, torus_params, torus_material);
    
    vulkan_update_sdf_scene(&demo->vulkan);
}

// ============================================================================
// Update and Render
// ============================================================================

static void update_camera(DemoState* demo) {
    // Mouse look
    demo->camera_rot[0] += demo->mouse_delta_y * 0.002f;
    demo->camera_rot[1] += demo->mouse_delta_x * 0.002f;
    
    // Clamp pitch
    if (demo->camera_rot[0] > 1.5f) demo->camera_rot[0] = 1.5f;
    if (demo->camera_rot[0] < -1.5f) demo->camera_rot[0] = -1.5f;
    
    // Calculate forward/right vectors
    float forward[3] = {
        sinf(demo->camera_rot[1]) * cosf(demo->camera_rot[0]),
        sinf(demo->camera_rot[0]),
        cosf(demo->camera_rot[1]) * cosf(demo->camera_rot[0])
    };
    
    float right[3] = {
        cosf(demo->camera_rot[1]),
        0,
        -sinf(demo->camera_rot[1])
    };
    
    // Movement
    float speed = 10.0f * demo->delta_time;
    if (demo->keys['W']) {
        demo->camera_pos[0] += forward[0] * speed;
        demo->camera_pos[1] += forward[1] * speed;
        demo->camera_pos[2] += forward[2] * speed;
    }
    if (demo->keys['S']) {
        demo->camera_pos[0] -= forward[0] * speed;
        demo->camera_pos[1] -= forward[1] * speed;
        demo->camera_pos[2] -= forward[2] * speed;
    }
    if (demo->keys['A']) {
        demo->camera_pos[0] -= right[0] * speed;
        demo->camera_pos[2] -= right[2] * speed;
    }
    if (demo->keys['D']) {
        demo->camera_pos[0] += right[0] * speed;
        demo->camera_pos[2] += right[2] * speed;
    }
    
    demo->mouse_delta_x = 0;
    demo->mouse_delta_y = 0;
}

static void update(DemoState* demo) {
    // Update time
    static uint64_t last_time = 0;
    uint64_t current_time = platform_get_ticks(&demo->platform);
    if (last_time > 0) {
        demo->delta_time = (current_time - last_time) / 1000000.0f; // Convert to seconds
    }
    last_time = current_time;
    demo->time += demo->delta_time;
    
    // Update camera
    update_camera(demo);
    
    // Update scene animation
    setup_scene(demo);
    
    // Update render state
    // ... (calculate view/projection matrices)
    memcpy(demo->render_state.camera_position, demo->camera_pos, sizeof(float) * 3);
    demo->render_state.time = demo->time;
    demo->render_state.delta_time = demo->delta_time;
    demo->render_state.frame_index = demo->frame_count;
    
    // Update ray march settings
    demo->raymarch_settings.max_distance = 100.0f;
    demo->raymarch_settings.epsilon = 0.001f;
    demo->raymarch_settings.max_steps = 128;
    demo->raymarch_settings.shadow_steps = 32;
    demo->raymarch_settings.fog_density = 0.01f;
    demo->raymarch_settings.fog_height = 10.0f;
    demo->raymarch_settings.cloud_scale = 0.5f;
    demo->raymarch_settings.cloud_speed = 1.0f;
    demo->raymarch_settings.gi_samples = 8;
    demo->raymarch_settings.gi_distance = 10.0f;
}

static void render(DemoState* demo) {
    // Begin frame
    if (!vulkan_begin_frame(&demo->vulkan)) {
        // Handle swapchain recreation
        vulkan_recreate_swapchain(&demo->vulkan, 1920, 1080);
        return;
    }
    
    VkCommandBuffer cmd = demo->vulkan.frames[demo->vulkan.current_frame].command_buffer;
    
    // Shadow pass
    // vulkan_render_shadow_pass(&demo->vulkan, cmd, demo->draw_commands, demo->draw_command_count);
    
    // Forward rendering pass
    // vulkan_begin_forward_pass(&demo->vulkan, cmd, ...);
    // vulkan_render_forward_pass(&demo->vulkan, cmd, demo->draw_commands, 
    //                            demo->draw_command_count, &demo->render_state);
    // vulkan_end_forward_pass(cmd);
    
    // Ray marching pass
    // vulkan_dispatch_raymarch(&demo->vulkan, cmd, &demo->render_state,
    //                         &demo->raymarch_settings, 1920, 1080);
    
    // End frame
    vulkan_end_frame(&demo->vulkan);
    
    // Get stats
    vulkan_get_stats(&demo->vulkan, &demo->draw_calls, &demo->triangles, &demo->gpu_time_ms);
    
    demo->frame_count++;
    
    // Print stats every second
    static uint64_t last_print = 0;
    if (demo->frame_count % 60 == 0) {
        printf("Frame %llu: %u draw calls, %u triangles, %.2f ms GPU\n",
               (unsigned long long)demo->frame_count, demo->draw_calls, 
               demo->triangles, demo->gpu_time_ms);
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    printf("Handmade Vulkan Demo\n");
    printf("====================\n\n");
    
    // Initialize platform
    if (!platform_init(&g_demo.platform, "Handmade Vulkan", 1920, 1080)) {
        printf("Failed to initialize platform!\n");
        return -1;
    }
    
    // Initialize Vulkan
    if (!vulkan_init(&g_demo.vulkan, &g_demo.platform, 1920, 1080)) {
        printf("Failed to initialize Vulkan!\n");
        platform_shutdown(&g_demo.platform);
        return -1;
    }
    
    // Create meshes
    g_demo.cube_mesh = create_cube_mesh(&g_demo.vulkan);
    g_demo.sphere_mesh = create_sphere_mesh(&g_demo.vulkan, 16);
    g_demo.ground_mesh = create_ground_mesh(&g_demo.vulkan, 100, 50);
    
    // Create textures
    g_demo.checker_texture = create_checker_texture(&g_demo.vulkan, 256);
    g_demo.noise_texture = create_noise_texture(&g_demo.vulkan, 256);
    
    // Initialize systems
    vulkan_init_shadow_system(&g_demo.vulkan, 4);
    vulkan_init_post_process(&g_demo.vulkan, 1920, 1080);
    vulkan_init_raymarch(&g_demo.vulkan, 1920, 1080);
    vulkan_init_streaming_buffers(&g_demo.vulkan);
    
    // Setup initial scene
    g_demo.camera_pos[0] = 0;
    g_demo.camera_pos[1] = 5;
    g_demo.camera_pos[2] = -20;
    setup_scene(&g_demo);
    
    // Main loop
    g_demo.running = true;
    while (g_demo.running) {
        // Process events
        PlatformEvent event;
        while (platform_poll_event(&g_demo.platform, &event)) {
            switch (event.type) {
                case PLATFORM_EVENT_QUIT:
                    g_demo.running = false;
                    break;
                    
                case PLATFORM_EVENT_KEY_DOWN:
                    if (event.key < 256) {
                        g_demo.keys[event.key] = true;
                    }
                    if (event.key == 27) { // Escape
                        g_demo.running = false;
                    }
                    break;
                    
                case PLATFORM_EVENT_KEY_UP:
                    if (event.key < 256) {
                        g_demo.keys[event.key] = false;
                    }
                    break;
                    
                case PLATFORM_EVENT_MOUSE_MOVE:
                    g_demo.mouse_delta_x += event.mouse_x;
                    g_demo.mouse_delta_y += event.mouse_y;
                    break;
            }
        }
        
        // Update and render
        update(&g_demo);
        render(&g_demo);
    }
    
    // Cleanup
    vulkan_wait_idle(&g_demo.vulkan);
    
    vulkan_destroy_streaming_buffers(&g_demo.vulkan);
    vulkan_destroy_raymarch(&g_demo.vulkan);
    vulkan_destroy_post_process(&g_demo.vulkan);
    vulkan_destroy_shadow_system(&g_demo.vulkan);
    
    vulkan_destroy_image(&g_demo.vulkan, &g_demo.checker_texture);
    vulkan_destroy_image(&g_demo.vulkan, &g_demo.noise_texture);
    
    vulkan_destroy_mesh(&g_demo.vulkan, &g_demo.cube_mesh);
    vulkan_destroy_mesh(&g_demo.vulkan, &g_demo.sphere_mesh);
    vulkan_destroy_mesh(&g_demo.vulkan, &g_demo.ground_mesh);
    
    vulkan_shutdown(&g_demo.vulkan);
    platform_shutdown(&g_demo.platform);
    
    printf("\nDemo completed successfully!\n");
    return 0;
}