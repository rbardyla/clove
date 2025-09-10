/*
    2D Physics Demo for Handmade Engine
    
    Demonstrates:
    - Bouncing balls
    - Falling boxes
    - Static obstacles
    - Mouse interaction
    - Debug visualization
*/

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include "handmade_gui.h"
#include "handmade_physics_2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Demo state
typedef struct {
    bool initialized;
    
    // Core systems
    Renderer renderer;
    HandmadeGUI gui;
    Physics2DWorld physics;
    
    // Memory arena for physics
    MemoryArena physics_arena;
    u8* physics_memory;
    
    // Demo state
    f32 time_accumulator;
    bool paused;
    bool spawn_circles;
    f32 spawn_timer;
    
    // UI state
    bool show_physics_panel;
    bool show_debug_panel;
    f32 gravity_strength;
    f32 air_friction;
    
    // Mouse interaction
    RigidBody2D* dragged_body;
    v2 mouse_world_pos;
} DemoState;

static DemoState g_state = {0};

// Convert screen coordinates to world coordinates
static v2 ScreenToWorld(v2 screen_pos, Camera2D* camera, u32 viewport_width, u32 viewport_height) {
    // Normalize to [-1, 1]
    v2 ndc;
    ndc.x = (screen_pos.x / (f32)viewport_width) * 2.0f - 1.0f;
    ndc.y = 1.0f - (screen_pos.y / (f32)viewport_height) * 2.0f; // Flip Y
    
    // Apply camera transform
    v2 world;
    world.x = (ndc.x / camera->zoom) + camera->position.x;
    world.y = (ndc.y / camera->zoom) + camera->position.y;
    
    return world;
}

// Find body at world position
static RigidBody2D* FindBodyAtPosition(Physics2DWorld* physics, v2 world_pos) {
    for (u32 i = 0; i < physics->max_bodies; i++) {
        RigidBody2D* body = &physics->bodies[i];
        if (!body->active) continue;
        
        if (body->shape.type == SHAPE_2D_CIRCLE) {
            v2 delta = v2_sub(world_pos, body->position);
            if (v2_length_sq(delta) <= body->shape.circle.radius * body->shape.circle.radius) {
                return body;
            }
        } else if (body->shape.type == SHAPE_2D_BOX) {
            // Simple point-in-box test (axis-aligned for simplicity)
            v2 half = body->shape.box.half_extents;
            if (fabsf(world_pos.x - body->position.x) <= half.x &&
                fabsf(world_pos.y - body->position.y) <= half.y) {
                return body;
            }
        }
    }
    return NULL;
}

// Create demo scene
static void CreateDemoScene(DemoState* state) {
    Physics2DWorld* physics = &state->physics;
    
    // Create ground
    RigidBody2D* ground = Physics2DCreateBody(physics, V2(0, -2.5f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(ground, V2(5.0f, 0.2f));
    ground->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Create side walls
    RigidBody2D* left_wall = Physics2DCreateBody(physics, V2(-3.5f, 0), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(left_wall, V2(0.2f, 3.0f));
    left_wall->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    RigidBody2D* right_wall = Physics2DCreateBody(physics, V2(3.5f, 0), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(right_wall, V2(0.2f, 3.0f));
    right_wall->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Create some static obstacles
    RigidBody2D* platform1 = Physics2DCreateBody(physics, V2(-1.5f, -0.5f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(platform1, V2(1.0f, 0.1f));
    platform1->color = COLOR(0.4f, 0.4f, 0.4f, 1.0f);
    platform1->rotation = 0.3f;
    
    RigidBody2D* platform2 = Physics2DCreateBody(physics, V2(1.5f, 0.0f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(platform2, V2(1.0f, 0.1f));
    platform2->color = COLOR(0.4f, 0.4f, 0.4f, 1.0f);
    platform2->rotation = -0.3f;
    
    // Create some initial dynamic bodies
    for (int i = 0; i < 10; i++) {
        f32 x = -2.0f + (i % 5) * 0.8f;
        f32 y = 1.0f + (i / 5) * 0.8f;
        
        RigidBody2D* body = Physics2DCreateBody(physics, V2(x, y), BODY_TYPE_DYNAMIC);
        
        if (i % 2 == 0) {
            // Create circle
            Physics2DSetCircleShape(body, 0.15f + (rand() % 3) * 0.05f);
            body->color = COLOR(
                0.5f + (rand() % 50) / 100.0f,
                0.5f + (rand() % 50) / 100.0f,
                0.8f + (rand() % 20) / 100.0f,
                1.0f
            );
        } else {
            // Create box
            f32 size = 0.15f + (rand() % 3) * 0.05f;
            Physics2DSetBoxShape(body, V2(size, size));
            body->color = COLOR(
                0.8f + (rand() % 20) / 100.0f,
                0.5f + (rand() % 50) / 100.0f,
                0.5f + (rand() % 50) / 100.0f,
                1.0f
            );
        }
        
        // Set material properties
        body->material.restitution = 0.3f + (rand() % 5) / 10.0f;
        body->material.friction = 0.5f + (rand() % 5) / 10.0f;
        
        // Give random initial velocity
        body->velocity = V2(
            ((rand() % 100) - 50) / 100.0f,
            ((rand() % 100) - 50) / 100.0f
        );
    }
}

// Spawn new body at position
static void SpawnBody(DemoState* state, v2 position, bool is_circle) {
    Physics2DWorld* physics = &state->physics;
    
    RigidBody2D* body = Physics2DCreateBody(physics, position, BODY_TYPE_DYNAMIC);
    if (!body) return; // Max bodies reached
    
    if (is_circle) {
        f32 radius = 0.1f + (rand() % 30) / 100.0f;
        Physics2DSetCircleShape(body, radius);
        body->color = COLOR(
            0.3f + (rand() % 70) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            1.0f
        );
    } else {
        f32 size = 0.1f + (rand() % 30) / 100.0f;
        Physics2DSetBoxShape(body, V2(size, size));
        body->color = COLOR(
            0.3f + (rand() % 70) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            1.0f
        );
        body->rotation = (rand() % 628) / 100.0f; // Random rotation
    }
    
    // Random material
    body->material.restitution = 0.2f + (rand() % 60) / 100.0f;
    body->material.friction = 0.3f + (rand() % 70) / 100.0f;
}

// Game functions
void GameInit(PlatformState* platform) {
    printf("=== 2D PHYSICS DEMO ===\n");
    printf("Controls:\n");
    printf("  ESC        - Quit\n");
    printf("  SPACE      - Pause/Resume\n");
    printf("  R          - Reset scene\n");
    printf("  C          - Spawn circles (hold)\n");
    printf("  B          - Spawn boxes (hold)\n");
    printf("  Mouse Drag - Move bodies\n");
    printf("  WASD       - Move camera\n");
    printf("  QE         - Zoom camera\n");
    printf("  P          - Toggle physics panel\n");
    printf("  D          - Toggle debug panel\n");
    
    // Initialize random
    srand(time(NULL));
    
    // Initialize renderer
    if (!RendererInit(&g_state.renderer, platform->window.width, platform->window.height)) {
        printf("Failed to initialize renderer!\n");
        return;
    }
    
    // Initialize GUI
    if (!HandmadeGUI_Init(&g_state.gui, &g_state.renderer)) {
        printf("Failed to initialize GUI!\n");
        return;
    }
    
    // Allocate physics memory
    size_t physics_memory_size = MEGABYTES(4);
    g_state.physics_memory = (u8*)malloc(physics_memory_size);
    g_state.physics_arena.base = g_state.physics_memory;
    g_state.physics_arena.size = physics_memory_size;
    g_state.physics_arena.used = 0;
    
    // Initialize physics
    if (!Physics2DInit(&g_state.physics, &g_state.physics_arena, 500)) {
        printf("Failed to initialize physics!\n");
        return;
    }
    
    // Create demo scene
    CreateDemoScene(&g_state);
    
    // Initialize demo state
    g_state.initialized = true;
    g_state.paused = false;
    g_state.spawn_circles = false;
    g_state.spawn_timer = 0.0f;
    g_state.show_physics_panel = true;
    g_state.show_debug_panel = true;
    g_state.gravity_strength = -9.81f;
    g_state.air_friction = 0.01f;
    
    // Set camera to view the scene
    g_state.renderer.camera.zoom = 0.3f;
    g_state.renderer.camera.position = V2(0, 0);
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_state.initialized) return;
    
    g_state.time_accumulator += dt;
    
    // Handle input
    if (platform->input.keys[KEY_ESCAPE].pressed) {
        platform->window.should_close = true;
    }
    
    if (platform->input.keys[KEY_SPACE].pressed) {
        g_state.paused = !g_state.paused;
        printf("Physics %s\n", g_state.paused ? "PAUSED" : "RESUMED");
    }
    
    if (platform->input.keys[KEY_R].pressed) {
        Physics2DReset(&g_state.physics);
        CreateDemoScene(&g_state);
        printf("Scene reset\n");
    }
    
    if (platform->input.keys[KEY_P].pressed) {
        g_state.show_physics_panel = !g_state.show_physics_panel;
    }
    
    if (platform->input.keys[KEY_D].pressed) {
        g_state.show_debug_panel = !g_state.show_debug_panel;
    }
    
    // Camera controls
    Camera2D* camera = &g_state.renderer.camera;
    f32 camera_speed = 2.0f * dt;
    f32 zoom_speed = 2.0f * dt;
    
    if (platform->input.keys[KEY_W].down) {
        camera->position.y += camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_S].down) {
        camera->position.y -= camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_A].down) {
        camera->position.x -= camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_D].down) {
        camera->position.x += camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_Q].down) {
        camera->zoom *= (1.0f - zoom_speed);
        if (camera->zoom < 0.1f) camera->zoom = 0.1f;
    }
    if (platform->input.keys[KEY_E].down) {
        camera->zoom *= (1.0f + zoom_speed);
        if (camera->zoom > 2.0f) camera->zoom = 2.0f;
    }
    
    // Convert mouse to world coordinates
    g_state.mouse_world_pos = ScreenToWorld(
        V2(platform->input.mouse_x, platform->input.mouse_y),
        camera,
        g_state.renderer.viewport_width,
        g_state.renderer.viewport_height
    );
    
    // Mouse interaction with bodies
    if (platform->input.mouse[MOUSE_LEFT].pressed) {
        g_state.dragged_body = FindBodyAtPosition(&g_state.physics, g_state.mouse_world_pos);
    }
    
    if (platform->input.mouse[MOUSE_LEFT].down && g_state.dragged_body) {
        // Drag body towards mouse
        if (g_state.dragged_body->type == BODY_TYPE_DYNAMIC) {
            v2 delta = v2_sub(g_state.mouse_world_pos, g_state.dragged_body->position);
            g_state.dragged_body->velocity = v2_scale(delta, 10.0f);
        }
    } else {
        g_state.dragged_body = NULL;
    }
    
    // Spawn bodies
    g_state.spawn_timer -= dt;
    if (g_state.spawn_timer <= 0) {
        if (platform->input.keys[KEY_C].down) {
            SpawnBody(&g_state, g_state.mouse_world_pos, true);
            g_state.spawn_timer = 0.1f;
        }
        if (platform->input.keys[KEY_B].down) {
            SpawnBody(&g_state, g_state.mouse_world_pos, false);
            g_state.spawn_timer = 0.1f;
        }
    }
    
    // Update physics
    if (!g_state.paused) {
        Physics2DStep(&g_state.physics, dt);
    }
    
    // Update viewport if window was resized
    if (platform->window.resized) {
        RendererSetViewport(&g_state.renderer, platform->window.width, platform->window.height);
    }
}

void GameRender(PlatformState* platform) {
    if (!g_state.initialized) return;
    
    // Clear screen
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Begin rendering
    RendererBeginFrame(&g_state.renderer);
    HandmadeGUI_BeginFrame(&g_state.gui, platform);
    
    // Draw physics world
    Physics2DDebugDraw(&g_state.physics, &g_state.renderer);
    
    // Draw mouse cursor in world space
    RendererDrawCircle(&g_state.renderer, g_state.mouse_world_pos, 0.02f, COLOR_WHITE, 16);
    
    // Highlight dragged body
    if (g_state.dragged_body) {
        v2 line_end = g_state.dragged_body->position;
        RendererDrawLine(&g_state.renderer, g_state.mouse_world_pos, line_end, 0.02f, COLOR_YELLOW);
    }
    
    // === GUI ===
    
    // Physics control panel
    if (g_state.show_physics_panel) {
        GUIPanel physics_panel = {
            .position = V2(10.0f, 10.0f),
            .size = V2(250.0f, 220.0f),
            .title = "Physics Controls",
            .open = &g_state.show_physics_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &physics_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            // Pause button
            char pause_text[32];
            snprintf(pause_text, sizeof(pause_text), "%s", g_state.paused ? "Resume" : "Pause");
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), pause_text)) {
                g_state.paused = !g_state.paused;
            }
            
            cursor.y -= 35.0f;
            
            // Reset button
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), "Reset Scene")) {
                Physics2DReset(&g_state.physics);
                CreateDemoScene(&g_state);
            }
            
            cursor.y -= 35.0f;
            
            // Gravity control
            HandmadeGUI_Label(&g_state.gui, cursor, "Gravity:");
            cursor.y -= 25.0f;
            
            char gravity_text[64];
            snprintf(gravity_text, sizeof(gravity_text), "Strength: %.1f", g_state.gravity_strength);
            HandmadeGUI_Label(&g_state.gui, cursor, gravity_text);
            
            cursor.y -= 25.0f;
            
            // Debug toggles
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show AABBs", &g_state.physics.debug_draw_aabb);
            cursor.y -= 25.0f;
            
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show Velocities", &g_state.physics.debug_draw_velocities);
            cursor.y -= 25.0f;
            
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show Contacts", &g_state.physics.debug_draw_contacts);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Debug info panel
    if (g_state.show_debug_panel) {
        GUIPanel debug_panel = {
            .position = V2(270.0f, 10.0f),
            .size = V2(200.0f, 180.0f),
            .title = "Debug Info",
            .open = &g_state.show_debug_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &debug_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            char debug_text[256];
            
            snprintf(debug_text, sizeof(debug_text), "Bodies: %u/%u", 
                    g_state.physics.body_count, g_state.physics.max_bodies);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Contacts: %u", g_state.physics.contact_count);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Checks: %u", g_state.physics.collision_checks);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Camera: %.2f, %.2f", 
                    g_state.renderer.camera.position.x, g_state.renderer.camera.position.y);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Zoom: %.2f", g_state.renderer.camera.zoom);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Status: %s", g_state.paused ? "PAUSED" : "RUNNING");
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Instructions overlay
    v2 overlay_pos = V2(10.0f, (f32)g_state.gui.renderer->viewport_height - 120.0f);
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "2D Physics Demo", 1.2f, COLOR_WHITE);
    
    overlay_pos.y -= 25.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "Hold C/B to spawn circles/boxes", 1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    overlay_pos.y -= 20.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "Drag bodies with mouse", 1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    overlay_pos.y -= 20.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "WASD/QE to move camera", 1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    // End rendering
    HandmadeGUI_EndFrame(&g_state.gui);
    RendererEndFrame(&g_state.renderer);
}

void GameShutdown(PlatformState* platform) {
    printf("Shutting down physics demo\n");
    
    // Cleanup
    HandmadeGUI_Shutdown(&g_state.gui);
    RendererShutdown(&g_state.renderer);
    Physics2DShutdown(&g_state.physics);
    
    if (g_state.physics_memory) {
        free(g_state.physics_memory);
        g_state.physics_memory = NULL;
    }
    
    g_state.initialized = false;
}

void GameOnReload(PlatformState* platform) {
    printf("Physics demo hot-reloaded\n");
}