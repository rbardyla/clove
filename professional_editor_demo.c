/*
    Professional Game Editor Demo
    
    Demonstrates the complete professional editor with all panels,
    tools, and features for game development.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "systems/editor/handmade_main_editor.h"
#include "src/handmade.h"

// Demo configuration
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define PERMANENT_MEMORY_SIZE (256 * 1024 * 1024)  // 256 MB
#define FRAME_MEMORY_SIZE (64 * 1024 * 1024)       // 64 MB

int main(int argc, char** argv) {
    printf("==================================================\n");
    printf("   HANDMADE PROFESSIONAL GAME EDITOR v1.0.0\n");
    printf("==================================================\n");
    printf("Phase 1: Full Feature Functional GUI\n");
    printf("\n");
    printf("Features:\n");
    printf("  - Multi-viewport 3D scene editing\n");
    printf("  - GameObject/Component architecture\n");
    printf("  - Dynamic property inspection\n");
    printf("  - Professional tool palette\n");
    printf("  - Flexible docking system\n");
    printf("  - Project management\n");
    printf("  - Play/Pause/Stop simulation\n");
    printf("\n");
    printf("Controls:\n");
    printf("  - Left Mouse: Select/Interact\n");
    printf("  - Right Mouse: Context menu\n");
    printf("  - Middle Mouse: Pan viewport\n");
    printf("  - Mouse Wheel: Zoom\n");
    printf("  - W/A/S/D: Move camera\n");
    printf("  - Q/E: Move up/down\n");
    printf("  - Shift: Move faster\n");
    printf("  - F: Focus on selection\n");
    printf("  - G: Move tool\n");
    printf("  - R: Rotate tool\n");
    printf("  - T: Scale tool\n");
    printf("  - Ctrl+N: New project\n");
    printf("  - Ctrl+O: Open project\n");
    printf("  - Ctrl+S: Save project\n");
    printf("  - Ctrl+Z: Undo\n");
    printf("  - Ctrl+Y: Redo\n");
    printf("  - Space: Play/Pause\n");
    printf("  - ESC: Exit\n");
    printf("==================================================\n\n");
    
    // Initialize platform
    platform_state platform = {0};
    if (!platform_init(&platform, "Handmade Professional Editor", 
                       WINDOW_WIDTH, WINDOW_HEIGHT)) {
        fprintf(stderr, "ERROR: Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize renderer
    renderer_state* renderer = renderer_create(&platform);
    if (!renderer) {
        fprintf(stderr, "ERROR: Failed to initialize renderer\n");
        platform_shutdown(&platform);
        return 1;
    }
    
    // Create main editor
    main_editor* editor = main_editor_create(&platform, renderer,
                                            PERMANENT_MEMORY_SIZE,
                                            FRAME_MEMORY_SIZE);
    if (!editor) {
        fprintf(stderr, "ERROR: Failed to create editor\n");
        renderer_destroy(renderer);
        platform_shutdown(&platform);
        return 1;
    }
    
    // Create or open a default project
    const char* project_path = "./EditorProject";
    if (!platform.file_exists(project_path)) {
        printf("Creating new project at: %s\n", project_path);
        main_editor_new_project(editor, project_path, "Demo Project");
    } else {
        printf("Opening existing project at: %s\n", project_path);
        main_editor_open_project(editor, project_path);
    }
    
    // Create some demo objects in the scene
    scene* current_scene = editor->scene_hierarchy->current_scene;
    if (current_scene && current_scene->object_count == 0) {
        printf("Creating demo scene objects...\n");
        
        // Create main camera
        uint32_t camera_id = scene_create_object(current_scene, "Main Camera");
        gameobject* camera = scene_get_object(current_scene, camera_id);
        camera->transform.position = (v3){0, 5, 10};
        camera->transform.rotation = quat_look_rotation((v3){0, -0.3f, -1}, (v3){0, 1, 0});
        scene_add_component(current_scene, camera_id, COMPONENT_CAMERA);
        
        // Create directional light
        uint32_t light_id = scene_create_object(current_scene, "Directional Light");
        gameobject* light = scene_get_object(current_scene, light_id);
        light->transform.position = (v3){5, 10, 5};
        light->transform.rotation = quat_euler((v3){-45, -30, 0});
        scene_add_component(current_scene, light_id, COMPONENT_LIGHT);
        
        // Create ground plane
        uint32_t ground_id = scene_create_object(current_scene, "Ground");
        gameobject* ground = scene_get_object(current_scene, ground_id);
        ground->transform.scale = (v3){20, 1, 20};
        scene_add_component(current_scene, ground_id, COMPONENT_MESH_RENDERER);
        scene_add_component(current_scene, ground_id, COMPONENT_COLLIDER);
        
        // Create some cubes
        for (int i = 0; i < 5; i++) {
            char name[64];
            sprintf(name, "Cube %d", i + 1);
            uint32_t cube_id = scene_create_object(current_scene, name);
            gameobject* cube = scene_get_object(current_scene, cube_id);
            cube->transform.position = (v3){
                (i - 2) * 3.0f,
                1.0f,
                0
            };
            scene_add_component(current_scene, cube_id, COMPONENT_MESH_RENDERER);
            scene_add_component(current_scene, cube_id, COMPONENT_RIGIDBODY);
            scene_add_component(current_scene, cube_id, COMPONENT_COLLIDER);
        }
        
        // Create a sphere
        uint32_t sphere_id = scene_create_object(current_scene, "Sphere");
        gameobject* sphere = scene_get_object(current_scene, sphere_id);
        sphere->transform.position = (v3){0, 5, -5};
        sphere->transform.scale = (v3){2, 2, 2};
        scene_add_component(current_scene, sphere_id, COMPONENT_MESH_RENDERER);
        scene_add_component(current_scene, sphere_id, COMPONENT_LIGHT);
        
        // Create particle system
        uint32_t particles_id = scene_create_object(current_scene, "Particle System");
        gameobject* particles = scene_get_object(current_scene, particles_id);
        particles->transform.position = (v3){0, 3, 5};
        scene_add_component(current_scene, particles_id, COMPONENT_PARTICLE_SYSTEM);
        
        // Create UI Canvas
        uint32_t canvas_id = scene_create_object(current_scene, "UI Canvas");
        scene_add_component(current_scene, canvas_id, COMPONENT_UI_CANVAS);
        
        // Create UI Text as child of canvas
        uint32_t text_id = scene_create_child(current_scene, canvas_id, "Score Text");
        gameobject* text = scene_get_object(current_scene, text_id);
        text->transform.position = (v3){-0.9f, 0.9f, 0};
        scene_add_component(current_scene, text_id, COMPONENT_UI_TEXT);
        
        printf("Demo scene created with %d objects\n", current_scene->object_count);
    }
    
    // Main loop
    printf("\nEditor running. Press ESC to exit.\n");
    
    f64 last_time = platform_get_time();
    bool running = true;
    
    while (running && editor->is_running) {
        // Calculate delta time
        f64 current_time = platform_get_time();
        f32 dt = (f32)(current_time - last_time);
        last_time = current_time;
        
        // Clamp delta time
        if (dt > 0.1f) dt = 0.1f;
        
        // Poll events
        platform_event event;
        while (platform_poll_event(&platform, &event)) {
            // Convert to input event for editor
            input_event editor_event = {0};
            
            switch (event.type) {
                case PLATFORM_EVENT_KEY_DOWN:
                case PLATFORM_EVENT_KEY_UP:
                    editor_event.type = event.type == PLATFORM_EVENT_KEY_DOWN ? 
                                       INPUT_KEY_DOWN : INPUT_KEY_UP;
                    editor_event.key.code = event.key.keycode;
                    editor_event.key.shift = event.key.shift;
                    editor_event.key.ctrl = event.key.ctrl;
                    editor_event.key.alt = event.key.alt;
                    
                    // Check for exit
                    if (event.type == PLATFORM_EVENT_KEY_DOWN && 
                        event.key.keycode == KEY_ESCAPE) {
                        running = false;
                    }
                    break;
                    
                case PLATFORM_EVENT_MOUSE_MOVE:
                    editor_event.type = INPUT_MOUSE_MOVE;
                    editor_event.mouse.x = event.mouse.x;
                    editor_event.mouse.y = event.mouse.y;
                    editor_event.mouse.dx = event.mouse.dx;
                    editor_event.mouse.dy = event.mouse.dy;
                    break;
                    
                case PLATFORM_EVENT_MOUSE_DOWN:
                case PLATFORM_EVENT_MOUSE_UP:
                    editor_event.type = event.type == PLATFORM_EVENT_MOUSE_DOWN ?
                                       INPUT_MOUSE_DOWN : INPUT_MOUSE_UP;
                    editor_event.mouse.x = event.mouse.x;
                    editor_event.mouse.y = event.mouse.y;
                    editor_event.mouse.button = event.mouse.button;
                    break;
                    
                case PLATFORM_EVENT_MOUSE_WHEEL:
                    editor_event.type = INPUT_MOUSE_WHEEL;
                    editor_event.mouse.wheel_delta = event.mouse.wheel_delta;
                    break;
                    
                case PLATFORM_EVENT_WINDOW_RESIZE:
                    platform.window_width = event.window.width;
                    platform.window_height = event.window.height;
                    renderer_resize(renderer, event.window.width, event.window.height);
                    break;
                    
                case PLATFORM_EVENT_WINDOW_CLOSE:
                    running = false;
                    break;
            }
            
            // Send event to editor
            if (editor_event.type != 0) {
                main_editor_handle_input(editor, &editor_event);
            }
        }
        
        // Update editor
        main_editor_update(editor, dt);
        
        // Render editor
        main_editor_render(editor);
        
        // Cap at 60 FPS
        usleep(16666);
    }
    
    printf("\nShutting down editor...\n");
    
    // Cleanup
    main_editor_destroy(editor);
    renderer_destroy(renderer);
    platform_shutdown(&platform);
    
    printf("Editor closed successfully.\n");
    
    return 0;
}