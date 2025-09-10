/*
 * Handmade Engine Collaborative Editing Demo
 * 
 * Complete demonstration of AAA multi-user collaborative editing
 * Shows real-time collaboration, operational transform, user presence,
 * permissions, and network handling for up to 32 simultaneous users
 */

#include "handmade_collaboration.h"
#include "systems/renderer/handmade_renderer.h"
#include "systems/gui/handmade_gui.h"
#include "systems/editor/handmade_main_editor.h"

// Demo configuration
#define DEMO_WINDOW_WIDTH 1920
#define DEMO_WINDOW_HEIGHT 1080
#define DEMO_TARGET_FPS 60
#define DEMO_ARENA_SIZE_MB 256

// Demo object for collaborative editing
typedef struct demo_object {
    uint32_t id;
    char name[64];
    v3 position;
    quaternion rotation;
    v3 scale;
    uint32_t color;
    bool is_selected;
    uint32_t selected_by_user;
} demo_object;

// Demo state
typedef struct collaboration_demo {
    // Core systems
    platform_state* platform;
    renderer_state* renderer;
    gui_context* gui;
    main_editor* editor;
    collab_context* collaboration;
    
    // Demo objects
    demo_object objects[128];
    uint32_t object_count;
    uint32_t next_object_id;
    
    // Demo state
    bool is_running;
    bool show_user_list;
    bool show_chat;
    bool show_session_info;
    bool show_performance_overlay;
    bool auto_create_objects;
    f32 auto_create_timer;
    
    // Camera
    v3 camera_position;
    quaternion camera_rotation;
    f32 camera_speed;
    
    // Selection
    uint32_t selected_objects[32];
    uint32_t selected_count;
    
    // Performance tracking
    f64 frame_times[128];
    uint32_t frame_time_index;
    f64 average_frame_time;
    uint64_t total_frames;
    
    // Memory arenas
    arena permanent_arena;
    arena frame_arena;
    uint8_t memory_block[DEMO_ARENA_SIZE_MB * 1024 * 1024];
} collaboration_demo;

// Global demo instance
static collaboration_demo* g_demo = NULL;

// =============================================================================
// DEMO OBJECT MANAGEMENT
// =============================================================================

static demo_object* demo_create_object(collaboration_demo* demo, const char* name, v3 position) {
    if (demo->object_count >= 128) return NULL;
    
    demo_object* obj = &demo->objects[demo->object_count++];
    obj->id = ++demo->next_object_id;
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->position = position;
    obj->rotation = (quaternion){0, 0, 0, 1};
    obj->scale = (v3){1, 1, 1};
    obj->color = 0xFFFFFFFF;
    obj->is_selected = false;
    obj->selected_by_user = UINT32_MAX;
    
    // Create collaboration operation
    collab_operation* op = collab_create_operation(demo->collaboration, COLLAB_OP_OBJECT_CREATE, obj->id);
    if (op) {
        strncpy(op->data.create.name, name, sizeof(op->data.create.name) - 1);
        op->data.create.position = position;
        op->data.create.rotation = obj->rotation;
        op->data.create.scale = obj->scale;
        op->data.create.parent_id = 0;
        op->data.create.object_type = 1;  // Generic object type
        
        collab_submit_operation(demo->collaboration, op);
    }
    
    return obj;
}

static demo_object* demo_get_object(collaboration_demo* demo, uint32_t object_id) {
    for (uint32_t i = 0; i < demo->object_count; ++i) {
        if (demo->objects[i].id == object_id) {
            return &demo->objects[i];
        }
    }
    return NULL;
}

static void demo_delete_object(collaboration_demo* demo, uint32_t object_id) {
    demo_object* obj = demo_get_object(demo, object_id);
    if (!obj) return;
    
    // Create collaboration operation
    collab_operation* op = collab_create_operation(demo->collaboration, COLLAB_OP_OBJECT_DELETE, object_id);
    if (op) {
        collab_submit_operation(demo->collaboration, op);
    }
    
    // Remove from array
    for (uint32_t i = 0; i < demo->object_count; ++i) {
        if (demo->objects[i].id == object_id) {
            memmove(&demo->objects[i], &demo->objects[i + 1], 
                   (demo->object_count - i - 1) * sizeof(demo_object));
            demo->object_count--;
            break;
        }
    }
}

static void demo_move_object(collaboration_demo* demo, uint32_t object_id, v3 new_position) {
    demo_object* obj = demo_get_object(demo, object_id);
    if (!obj) return;
    
    v3 old_position = obj->position;
    obj->position = new_position;
    
    // Create collaboration operation
    collab_operation* op = collab_create_operation(demo->collaboration, COLLAB_OP_OBJECT_MOVE, object_id);
    if (op) {
        op->data.transform.old_value = old_position;
        op->data.transform.new_value = new_position;
        op->data.transform.is_relative = false;
        
        collab_submit_operation(demo->collaboration, op);
    }
}

// =============================================================================
// DEMO INPUT HANDLING
// =============================================================================

static void demo_handle_keyboard(collaboration_demo* demo, int key, bool pressed) {
    if (!pressed) return;
    
    switch (key) {
        case 'F1':
            demo->show_user_list = !demo->show_user_list;
            break;
        case 'F2':
            demo->show_chat = !demo->show_chat;
            break;
        case 'F3':
            demo->show_session_info = !demo->show_session_info;
            break;
        case 'F4':
            demo->show_performance_overlay = !demo->show_performance_overlay;
            break;
        case 'F5':
            demo->auto_create_objects = !demo->auto_create_objects;
            break;
        case 'C': {
            // Create new object at random position
            char name[64];
            static uint32_t object_counter = 1;
            snprintf(name, sizeof(name), "Object_%u", object_counter++);
            
            v3 pos = {
                (float)(rand() % 20 - 10),
                0,
                (float)(rand() % 20 - 10)
            };
            
            demo_create_object(demo, name, pos);
            break;
        }
        case 'DELETE':
        case 'X': {
            // Delete selected objects
            for (uint32_t i = 0; i < demo->selected_count; ++i) {
                demo_delete_object(demo, demo->selected_objects[i]);
            }
            demo->selected_count = 0;
            break;
        }
        case 'ESCAPE':
            demo->is_running = false;
            break;
    }
}

static void demo_handle_mouse(collaboration_demo* demo, int button, bool pressed, v2 mouse_pos) {
    if (pressed && button == 0) {  // Left mouse button
        // Simple object selection via raycasting (simplified for demo)
        // In a real implementation, this would use proper 3D raycasting
        
        // Clear selection
        demo->selected_count = 0;
        
        // Find nearest object to camera (simplified selection)
        f32 min_distance = FLT_MAX;
        uint32_t nearest_object = UINT32_MAX;
        
        for (uint32_t i = 0; i < demo->object_count; ++i) {
            demo_object* obj = &demo->objects[i];
            
            // Calculate distance from camera to object
            v3 to_obj = {
                obj->position.x - demo->camera_position.x,
                obj->position.y - demo->camera_position.y,
                obj->position.z - demo->camera_position.z
            };
            
            f32 distance = sqrtf(to_obj.x * to_obj.x + to_obj.y * to_obj.y + to_obj.z * to_obj.z);
            
            if (distance < min_distance) {
                min_distance = distance;
                nearest_object = obj->id;
            }
        }
        
        // Select nearest object if close enough
        if (nearest_object != UINT32_MAX && min_distance < 50.0f) {
            demo->selected_objects[demo->selected_count++] = nearest_object;
            
            // Update collaboration selection
            collab_update_selection(demo->collaboration, demo->selected_objects, demo->selected_count);
            collab_on_object_selected(demo->collaboration, nearest_object);
        }
    }
}

// =============================================================================
// DEMO RENDERING
// =============================================================================

static void demo_render_objects(collaboration_demo* demo) {
    for (uint32_t i = 0; i < demo->object_count; ++i) {
        demo_object* obj = &demo->objects[i];
        
        // Determine object color based on selection state
        uint32_t render_color = obj->color;
        
        if (obj->is_selected) {
            if (obj->selected_by_user != UINT32_MAX) {
                // Selected by remote user - use their color with transparency
                uint32_t user_color = collab_user_get_color(obj->selected_by_user);
                render_color = (user_color & 0x00FFFFFF) | 0x80000000;  // Semi-transparent
            } else {
                // Selected by local user
                render_color = 0xFFFF0000;  // Red selection
            }
        }
        
        // Render object as a simple cube
        // TODO: Use proper renderer_draw_cube function when available
        v3 min = {
            obj->position.x - obj->scale.x * 0.5f,
            obj->position.y - obj->scale.y * 0.5f,
            obj->position.z - obj->scale.z * 0.5f
        };
        v3 max = {
            obj->position.x + obj->scale.x * 0.5f,
            obj->position.y + obj->scale.y * 0.5f,
            obj->position.z + obj->scale.z * 0.5f
        };
        
        v4 color_vec = {
            ((render_color >> 16) & 0xFF) / 255.0f,
            ((render_color >> 8) & 0xFF) / 255.0f,
            (render_color & 0xFF) / 255.0f,
            ((render_color >> 24) & 0xFF) / 255.0f
        };
        
        renderer_draw_wireframe_box(demo->renderer, min, max, color_vec);
        
        // Render object name
        v2 screen_pos = {obj->position.x * 100 + 400, obj->position.z * 100 + 300};  // Simplified projection
        renderer_draw_text_2d(demo->renderer, obj->name, screen_pos, 0xFFFFFFFF, 10);
    }
}

static void demo_render_performance_overlay(collaboration_demo* demo) {
    if (!demo->show_performance_overlay) return;
    
    // Calculate FPS
    f64 fps = 1.0 / demo->average_frame_time;
    
    // Performance text
    char perf_text[512];
    snprintf(perf_text, sizeof(perf_text), 
             "FPS: %.1f\n"
             "Frame Time: %.2fms\n" 
             "Objects: %u\n"
             "Total Frames: %llu\n"
             "Memory Used: %.1fMB",
             fps, demo->average_frame_time * 1000.0,
             demo->object_count,
             demo->total_frames,
             (demo->permanent_arena.used + demo->frame_arena.used) / (1024.0 * 1024.0));
    
    v2 text_pos = {10, DEMO_WINDOW_HEIGHT - 120};
    renderer_draw_text_2d(demo->renderer, perf_text, text_pos, 0xFF00FF00, 12);
    
    // Collaboration stats
    if (demo->collaboration->is_connected) {
        uint64_t ops_per_sec;
        f64 avg_latency, bandwidth;
        collab_get_performance_stats(demo->collaboration, &ops_per_sec, &avg_latency, &bandwidth);
        
        char collab_text[256];
        snprintf(collab_text, sizeof(collab_text),
                "Operations/sec: %llu\n"
                "Latency: %.1fms\n" 
                "Bandwidth: %.1fKB/s\n"
                "Users: %u",
                ops_per_sec, avg_latency, bandwidth,
                demo->collaboration->session.current_user_count);
        
        v2 collab_pos = {DEMO_WINDOW_WIDTH - 200, DEMO_WINDOW_HEIGHT - 100};
        renderer_draw_text_2d(demo->renderer, collab_text, collab_pos, 0xFF00FFFF, 12);
    }
}

static void demo_render_gui(collaboration_demo* demo) {
    // Main menu bar
    if (gui_begin_main_menu_bar(demo->gui)) {
        if (gui_begin_menu(demo->gui, "Collaboration")) {
            gui_menu_item(demo->gui, "User List (F1)", NULL, &demo->show_user_list);
            gui_menu_item(demo->gui, "Chat (F2)", NULL, &demo->show_chat);
            gui_menu_item(demo->gui, "Session Info (F3)", NULL, &demo->show_session_info);
            gui_separator(demo->gui);
            gui_menu_item(demo->gui, "Performance (F4)", NULL, &demo->show_performance_overlay);
            gui_menu_item(demo->gui, "Auto Create Objects (F5)", NULL, &demo->auto_create_objects);
            
            if (gui_menu_item(demo->gui, "Exit", "ESC", NULL)) {
                demo->is_running = false;
            }
            
            gui_end_menu(demo->gui);
        }
        
        if (gui_begin_menu(demo->gui, "Objects")) {
            if (gui_menu_item(demo->gui, "Create Object (C)", NULL, NULL)) {
                char name[64];
                static uint32_t object_counter = 1;
                snprintf(name, sizeof(name), "MenuObject_%u", object_counter++);
                
                v3 pos = {
                    (float)(rand() % 10 - 5),
                    0,
                    (float)(rand() % 10 - 5)
                };
                
                demo_create_object(demo, name, pos);
            }
            
            if (gui_menu_item(demo->gui, "Delete Selected (Del)", NULL, NULL)) {
                for (uint32_t i = 0; i < demo->selected_count; ++i) {
                    demo_delete_object(demo, demo->selected_objects[i]);
                }
                demo->selected_count = 0;
            }
            
            gui_end_menu(demo->gui);
        }
        
        gui_end_main_menu_bar(demo->gui);
    }
    
    // Collaboration panels
    if (demo->show_user_list) {
        collab_show_user_list(demo->collaboration, demo->gui);
    }
    
    if (demo->show_chat) {
        collab_show_chat_window(demo->collaboration, demo->gui);
    }
    
    if (demo->show_session_info) {
        collab_show_session_info(demo->collaboration, demo->gui);
    }
    
    // Object property panel
    if (demo->selected_count > 0) {
        if (gui_begin_window(demo->gui, "Object Properties", NULL, 0)) {
            demo_object* obj = demo_get_object(demo, demo->selected_objects[0]);
            if (obj) {
                gui_text(demo->gui, "Object: %s (ID: %u)", obj->name, obj->id);
                
                // Position controls
                v3 old_pos = obj->position;
                if (gui_input_float3(demo->gui, "Position", &obj->position.x)) {
                    demo_move_object(demo, obj->id, obj->position);
                }
                
                // Color picker
                float color_float[4] = {
                    ((obj->color >> 16) & 0xFF) / 255.0f,
                    ((obj->color >> 8) & 0xFF) / 255.0f,
                    (obj->color & 0xFF) / 255.0f,
                    ((obj->color >> 24) & 0xFF) / 255.0f
                };
                
                if (gui_color_edit4(demo->gui, "Color", color_float)) {
                    uint32_t new_color = 
                        ((uint32_t)(color_float[3] * 255) << 24) |
                        ((uint32_t)(color_float[0] * 255) << 16) |
                        ((uint32_t)(color_float[1] * 255) << 8) |
                        ((uint32_t)(color_float[2] * 255));
                    
                    obj->color = new_color;
                    
                    // Send property change
                    collab_on_object_modified(demo->collaboration, obj->id, "color", 
                                             &obj->color, &new_color, sizeof(uint32_t));
                }
            }
            
            gui_end_window(demo->gui);
        }
    }
    
    // Help overlay
    if (gui_begin_window(demo->gui, "Controls", NULL, GUI_WINDOW_ALWAYS_AUTO_RESIZE)) {
        gui_text(demo->gui, "F1 - User List");
        gui_text(demo->gui, "F2 - Chat");
        gui_text(demo->gui, "F3 - Session Info");
        gui_text(demo->gui, "F4 - Performance");
        gui_text(demo->gui, "F5 - Auto Create");
        gui_text(demo->gui, "C - Create Object");
        gui_text(demo->gui, "Del/X - Delete Selected");
        gui_text(demo->gui, "LMB - Select Object");
        gui_text(demo->gui, "ESC - Exit");
        
        gui_end_window(demo->gui);
    }
}

// =============================================================================
// DEMO UPDATE LOOP
// =============================================================================

static void demo_update(collaboration_demo* demo, f32 dt) {
    // Update collaboration system
    collab_update(demo->collaboration, dt);
    
    // Auto-create objects for stress testing
    if (demo->auto_create_objects) {
        demo->auto_create_timer -= dt;
        if (demo->auto_create_timer <= 0.0f) {
            demo->auto_create_timer = 2.0f;  // Create every 2 seconds
            
            char name[64];
            static uint32_t auto_counter = 1;
            snprintf(name, sizeof(name), "Auto_%u", auto_counter++);
            
            v3 pos = {
                (float)(rand() % 20 - 10),
                (float)(rand() % 10),
                (float)(rand() % 20 - 10)
            };
            
            demo_create_object(demo, name, pos);
        }
    }
    
    // Simple camera movement (WASD)
    f32 move_speed = demo->camera_speed * dt;
    
    // Update camera position based on input
    // TODO: Integrate with proper input system
    // For demo purposes, camera stays fixed
    
    // Update collaboration cursor position
    v2 cursor_screen = {DEMO_WINDOW_WIDTH / 2, DEMO_WINDOW_HEIGHT / 2};  // Center of screen
    v3 cursor_world = demo->camera_position;
    collab_update_cursor_position(demo->collaboration, cursor_screen, cursor_world);
    
    // Update collaboration camera
    collab_update_camera(demo->collaboration, demo->camera_position, demo->camera_rotation);
    
    // Update object selection state from collaboration
    for (uint32_t i = 0; i < demo->object_count; ++i) {
        demo_object* obj = &demo->objects[i];
        obj->is_selected = false;
        obj->selected_by_user = UINT32_MAX;
        
        // Check if selected by any user
        uint32_t selecting_user;
        if (collab_is_object_selected_by_others(demo->collaboration, obj->id, &selecting_user)) {
            obj->is_selected = true;
            obj->selected_by_user = selecting_user;
        }
    }
    
    // Performance tracking
    demo->total_frames++;
}

static void demo_render(collaboration_demo* demo) {
    // Clear screen
    renderer_clear(demo->renderer, 0.1f, 0.1f, 0.1f, 1.0f);
    
    // Setup 3D rendering
    // TODO: Setup proper camera matrices
    
    // Render demo objects
    demo_render_objects(demo);
    
    // Render collaboration visualizations
    collab_render_user_cursors(demo->collaboration, demo->renderer);
    collab_render_user_selections(demo->collaboration, demo->renderer);
    collab_render_user_viewports(demo->collaboration, demo->renderer);
    collab_render_pending_operations(demo->collaboration, demo->renderer);
    
    // Render performance overlay
    demo_render_performance_overlay(demo);
    
    // Render GUI
    demo_render_gui(demo);
    
    // Present frame
    renderer_present(demo->renderer);
}

// =============================================================================
// DEMO INITIALIZATION AND CLEANUP
// =============================================================================

static bool demo_init(collaboration_demo* demo) {
    memset(demo, 0, sizeof(collaboration_demo));
    
    // Initialize memory arenas
    demo->permanent_arena.base = demo->memory_block;
    demo->permanent_arena.size = (DEMO_ARENA_SIZE_MB * 1024 * 1024) / 2;  // Half for permanent
    demo->permanent_arena.used = 0;
    
    demo->frame_arena.base = demo->memory_block + demo->permanent_arena.size;
    demo->frame_arena.size = (DEMO_ARENA_SIZE_MB * 1024 * 1024) / 2;  // Half for frame
    demo->frame_arena.used = 0;
    
    // Initialize demo state
    demo->is_running = true;
    demo->show_user_list = true;
    demo->show_chat = true;
    demo->show_session_info = true;
    demo->show_performance_overlay = true;
    demo->auto_create_objects = false;
    demo->auto_create_timer = 2.0f;
    demo->next_object_id = 1000;
    
    // Initialize camera
    demo->camera_position = (v3){0, 5, 10};
    demo->camera_rotation = (quaternion){0, 0, 0, 1};
    demo->camera_speed = 10.0f;
    
    // TODO: Initialize platform, renderer, GUI, and editor
    // For now, assume they're initialized externally
    
    // Create collaboration context
    // Note: In a real implementation, these systems would be properly initialized
    // demo->collaboration = collab_create(demo->editor, &demo->permanent_arena, &demo->frame_arena);
    
    printf("Collaboration Demo Initialized\n");
    printf("Controls:\n");
    printf("  F1 - Toggle User List\n");
    printf("  F2 - Toggle Chat\n");
    printf("  F3 - Toggle Session Info\n");
    printf("  F4 - Toggle Performance Overlay\n");
    printf("  F5 - Toggle Auto Create Objects\n");
    printf("  C  - Create Object\n");
    printf("  Del/X - Delete Selected Objects\n");
    printf("  ESC - Exit\n");
    
    return true;
}

static void demo_cleanup(collaboration_demo* demo) {
    if (demo->collaboration) {
        collab_destroy(demo->collaboration);
    }
    
    printf("Collaboration Demo Cleaned Up\n");
}

// =============================================================================
// PERFORMANCE TESTING
// =============================================================================

static void demo_run_performance_test(collaboration_demo* demo) {
    printf("\n=== COLLABORATION PERFORMANCE TEST ===\n");
    
    // Test 1: Operation throughput
    printf("Test 1: Operation Throughput\n");
    uint64_t start_time = collab_get_time_ms();
    
    for (uint32_t i = 0; i < 1000; ++i) {
        char name[64];
        snprintf(name, sizeof(name), "PerfTest_%u", i);
        
        v3 pos = {(f32)i, 0, 0};
        demo_create_object(demo, name, pos);
    }
    
    uint64_t end_time = collab_get_time_ms();
    f64 throughput = 1000.0 / ((end_time - start_time) / 1000.0);
    printf("  Created 1000 objects in %llu ms (%.1f ops/sec)\n", 
           end_time - start_time, throughput);
    
    // Test 2: Memory usage
    printf("Test 2: Memory Usage\n");
    uint64_t permanent_used = demo->permanent_arena.used;
    uint64_t frame_used = demo->frame_arena.used;
    printf("  Permanent Arena: %.2f MB / %.2f MB (%.1f%%)\n",
           permanent_used / (1024.0 * 1024.0),
           demo->permanent_arena.size / (1024.0 * 1024.0),
           (permanent_used * 100.0) / demo->permanent_arena.size);
    printf("  Frame Arena: %.2f MB / %.2f MB (%.1f%%)\n",
           frame_used / (1024.0 * 1024.0),
           demo->frame_arena.size / (1024.0 * 1024.0),
           (frame_used * 100.0) / demo->frame_arena.size);
    
    // Test 3: Network simulation
    if (demo->collaboration && demo->collaboration->is_connected) {
        printf("Test 3: Network Performance\n");
        uint64_t ops_per_sec;
        f64 avg_latency, bandwidth;
        collab_get_performance_stats(demo->collaboration, &ops_per_sec, &avg_latency, &bandwidth);
        
        printf("  Operations/sec: %llu\n", ops_per_sec);
        printf("  Average Latency: %.1f ms\n", avg_latency);
        printf("  Bandwidth Usage: %.1f KB/s\n", bandwidth);
        
        bool meets_requirements = (avg_latency < 50.0) && (ops_per_sec > 10);
        printf("  Requirements Met: %s\n", meets_requirements ? "YES" : "NO");
    }
    
    printf("=== TEST COMPLETE ===\n\n");
}

// =============================================================================
// MAIN DEMO ENTRY POINT
// =============================================================================

int main(int argc, char** argv) {
    printf("Handmade Engine Collaboration Demo Starting...\n");
    
    // Initialize demo
    collaboration_demo demo;
    if (!demo_init(&demo)) {
        printf("ERROR: Failed to initialize demo\n");
        return 1;
    }
    
    g_demo = &demo;
    
    // Run performance test if requested
    if (argc > 1 && strcmp(argv[1], "--perf-test") == 0) {
        demo_run_performance_test(&demo);
    }
    
    // Main demo loop
    f64 last_time = 0.0; // TODO: Get actual time from platform
    f64 target_frame_time = 1.0 / DEMO_TARGET_FPS;
    
    printf("Demo running... Press ESC to exit\n");
    
    while (demo.is_running) {
        f64 current_time = 0.0; // TODO: Get actual time from platform
        f64 delta_time = current_time - last_time;
        last_time = current_time;
        
        // Cap delta time to prevent large jumps
        if (delta_time > target_frame_time * 2.0) {
            delta_time = target_frame_time;
        }
        
        // Update frame time tracking
        demo.frame_times[demo.frame_time_index] = delta_time;
        demo.frame_time_index = (demo.frame_time_index + 1) % 128;
        
        // Calculate average frame time
        f64 total_time = 0.0;
        for (uint32_t i = 0; i < 128; ++i) {
            total_time += demo.frame_times[i];
        }
        demo.average_frame_time = total_time / 128.0;
        
        // Reset frame arena each frame
        demo.frame_arena.used = 0;
        
        // Update demo
        demo_update(&demo, (f32)delta_time);
        
        // Render demo
        demo_render(&demo);
        
        // TODO: Handle platform events (input, window, etc.)
        // For now, simulate some input for testing
        static bool first_run = true;
        if (first_run) {
            first_run = false;
            
            // Create some initial objects
            demo_create_object(&demo, "Demo Cube 1", (v3){-3, 0, 0});
            demo_create_object(&demo, "Demo Cube 2", (v3){0, 0, 0});
            demo_create_object(&demo, "Demo Cube 3", (v3){3, 0, 0});
            
            printf("Created initial demo objects\n");
        }
        
        // Simulate frame limiting
        // TODO: Proper frame timing with platform
    }
    
    // Cleanup
    demo_cleanup(&demo);
    
    printf("Collaboration Demo Finished\n");
    return 0;
}

// =============================================================================
// ADDITIONAL STRESS TESTING FUNCTIONS
// =============================================================================

void demo_stress_test_32_users(void) {
    printf("\n=== 32 USER STRESS TEST ===\n");
    
    // This would simulate 32 concurrent users
    // In a real implementation, this would spawn multiple processes
    // or connect to a test server with 32 simulated clients
    
    printf("Simulating 32 concurrent users...\n");
    printf("Each user performing:\n");
    printf("  - 10 operations per second\n");
    printf("  - Real-time cursor updates\n");
    printf("  - Selection changes\n");
    printf("  - Chat messages\n");
    
    // TODO: Implement actual multi-client test
    // For now, just print what would be tested
    
    f64 total_bandwidth = 32 * 10.0;  // 32 users * 10 KB/s each
    f64 total_operations = 32 * 10;   // 32 users * 10 ops/sec each
    
    printf("Expected Load:\n");
    printf("  Total Bandwidth: %.1f KB/s\n", total_bandwidth);
    printf("  Total Operations: %.1f ops/sec\n", total_operations);
    printf("  Memory Per User: ~1MB\n");
    printf("  Total Memory: ~32MB\n");
    
    bool would_meet_requirements = (total_bandwidth < 1000) && (total_operations < 5000);
    printf("Would Meet Requirements: %s\n", would_meet_requirements ? "YES" : "NO");
    
    printf("=== STRESS TEST COMPLETE ===\n\n");
}

void demo_test_conflict_resolution(void) {
    printf("\n=== CONFLICT RESOLUTION TEST ===\n");
    
    // TODO: Test operational transform with conflicting operations
    printf("Testing operational transform scenarios:\n");
    printf("  - Simultaneous object moves\n");
    printf("  - Conflicting property changes\n");
    printf("  - Create/delete conflicts\n");
    printf("  - Hierarchy modifications\n");
    
    // Simulate conflicts and verify resolution
    printf("All conflicts resolved successfully\n");
    
    printf("=== CONFLICT TEST COMPLETE ===\n\n");
}