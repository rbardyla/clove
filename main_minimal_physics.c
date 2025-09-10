/*
    HANDMADE ENGINE WITH RENDERER, GUI, AND 2D PHYSICS
    
    Extends the minimal engine with simple 2D physics:
    - Integrates seamlessly with existing renderer and GUI
    - Demonstrates physics with interactive demo
    - Maintains 60fps performance target
    - Zero external dependencies
*/

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include "handmade_gui.h"
#include "handmade_physics_2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>

// Application state with renderer, GUI, and physics
typedef struct {
    bool initialized;
    f32 time_accumulator;
    
    // Core systems
    Renderer renderer;
    HandmadeGUI gui;
    Physics2DWorld physics;
    
    // Memory for physics
    MemoryArena physics_arena;
    u8* physics_memory;
    
    // Demo state
    bool physics_enabled;
    bool physics_paused;
    f32 spawn_timer;
    
    // UI panels
    bool show_renderer_panel;
    bool show_physics_panel;
    bool show_stats_panel;
    
    // Mouse interaction
    RigidBody2D* dragged_body;
    v2 mouse_world_pos;
    
    // Demo objects
    f32 demo_rotation;
} GameState;

static GameState g_state = {0};

// Convert screen to world coordinates
static v2 ScreenToWorld(v2 screen_pos, Camera2D* camera, u32 viewport_width, u32 viewport_height) {
    v2 ndc;
    ndc.x = (screen_pos.x / (f32)viewport_width) * 2.0f - 1.0f;
    ndc.y = 1.0f - (screen_pos.y / (f32)viewport_height) * 2.0f;
    
    v2 world;
    world.x = (ndc.x / camera->zoom) + camera->position.x;
    world.y = (ndc.y / camera->zoom) + camera->position.y;
    
    return world;
}

// Find body at position
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
            v2 half = body->shape.box.half_extents;
            v2 local_pos = v2_sub(world_pos, body->position);
            local_pos = v2_rotate(local_pos, -body->rotation);
            
            if (fabsf(local_pos.x) <= half.x && fabsf(local_pos.y) <= half.y) {
                return body;
            }
        }
    }
    return NULL;
}

// Create physics demo scene
static void CreatePhysicsScene(GameState* state) {
    Physics2DWorld* physics = &state->physics;
    Physics2DReset(physics);
    
    // Create boundaries
    RigidBody2D* ground = Physics2DCreateBody(physics, V2(0, -2.8f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(ground, V2(6.0f, 0.2f));
    ground->color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    
    RigidBody2D* left = Physics2DCreateBody(physics, V2(-4.0f, 0), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(left, V2(0.2f, 3.5f));
    left->color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    
    RigidBody2D* right = Physics2DCreateBody(physics, V2(4.0f, 0), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(right, V2(0.2f, 3.5f));
    right->color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    
    // Create angled platforms
    RigidBody2D* plat1 = Physics2DCreateBody(physics, V2(-2.0f, -0.5f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(plat1, V2(1.5f, 0.1f));
    plat1->rotation = 0.4f;
    plat1->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    RigidBody2D* plat2 = Physics2DCreateBody(physics, V2(2.0f, 0.0f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(plat2, V2(1.5f, 0.1f));
    plat2->rotation = -0.4f;
    plat2->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Create center obstacle
    RigidBody2D* center = Physics2DCreateBody(physics, V2(0, 0.5f), BODY_TYPE_STATIC);
    Physics2DSetCircleShape(center, 0.3f);
    center->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Create initial dynamic bodies
    for (int i = 0; i < 20; i++) {
        f32 x = -2.5f + (rand() % 50) / 10.0f;
        f32 y = 1.0f + (rand() % 15) / 10.0f;
        
        RigidBody2D* body = Physics2DCreateBody(physics, V2(x, y), BODY_TYPE_DYNAMIC);
        
        if (rand() % 2 == 0) {
            // Circle
            f32 radius = 0.1f + (rand() % 20) / 100.0f;
            Physics2DSetCircleShape(body, radius);
            body->color = COLOR(
                0.4f + (rand() % 60) / 100.0f,
                0.4f + (rand() % 60) / 100.0f,
                0.7f + (rand() % 30) / 100.0f,
                1.0f
            );
        } else {
            // Box
            f32 size = 0.1f + (rand() % 20) / 100.0f;
            Physics2DSetBoxShape(body, V2(size, size));
            body->rotation = (rand() % 628) / 100.0f;
            body->color = COLOR(
                0.7f + (rand() % 30) / 100.0f,
                0.4f + (rand() % 60) / 100.0f,
                0.4f + (rand() % 60) / 100.0f,
                1.0f
            );
        }
        
        body->material.restitution = 0.3f + (rand() % 40) / 100.0f;
        body->material.friction = 0.5f + (rand() % 50) / 100.0f;
    }
}

// Spawn new physics body
static void SpawnPhysicsBody(GameState* state, v2 position, bool is_circle) {
    RigidBody2D* body = Physics2DCreateBody(&state->physics, position, BODY_TYPE_DYNAMIC);
    if (!body) return;
    
    if (is_circle) {
        f32 radius = 0.08f + (rand() % 20) / 100.0f;
        Physics2DSetCircleShape(body, radius);
        body->color = COLOR(
            0.3f + (rand() % 70) / 100.0f,
            0.5f + (rand() % 50) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            1.0f
        );
    } else {
        f32 size = 0.08f + (rand() % 20) / 100.0f;
        Physics2DSetBoxShape(body, V2(size * 1.2f, size));
        body->rotation = (rand() % 628) / 100.0f;
        body->color = COLOR(
            0.5f + (rand() % 50) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            1.0f
        );
    }
    
    body->material.restitution = 0.2f + (rand() % 60) / 100.0f;
    body->material.friction = 0.4f + (rand() % 60) / 100.0f;
    
    // Give initial velocity
    body->velocity = V2(
        ((rand() % 200) - 100) / 200.0f,
        -((rand() % 100) / 200.0f)
    );
}

// Game initialization
void GameInit(PlatformState* platform) {
    printf("=== HANDMADE ENGINE WITH PHYSICS ===\n");
    printf("Window size: %dx%d\n", platform->window.width, platform->window.height);
    
    // Initialize random
    srand(time(NULL));
    
    g_state.initialized = true;
    g_state.time_accumulator = 0.0f;
    g_state.demo_rotation = 0.0f;
    g_state.physics_enabled = true;
    g_state.physics_paused = false;
    
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
    
    // Allocate and initialize physics
    size_t physics_memory_size = MEGABYTES(2);
    g_state.physics_memory = (u8*)malloc(physics_memory_size);
    g_state.physics_arena.base = g_state.physics_memory;
    g_state.physics_arena.size = physics_memory_size;
    g_state.physics_arena.used = 0;
    
    if (!Physics2DInit(&g_state.physics, &g_state.physics_arena, 300)) {
        printf("Failed to initialize physics!\n");
        g_state.physics_enabled = false;
    } else {
        CreatePhysicsScene(&g_state);
        g_state.physics.debug_draw_enabled = true;
    }
    
    // Initialize UI state
    g_state.show_renderer_panel = true;
    g_state.show_physics_panel = true;
    g_state.show_stats_panel = true;
    
    // Set initial camera
    g_state.renderer.camera.zoom = 0.35f;
    g_state.renderer.camera.position = V2(0, 0);
    
    printf("All systems initialized\n");
    printf("\nControls:\n");
    printf("  ESC          - Quit\n");
    printf("  SPACE        - Pause/Resume physics\n");
    printf("  R            - Reset physics scene\n");
    printf("  C/B          - Spawn circles/boxes (hold)\n");
    printf("  Mouse Drag   - Move physics bodies\n");
    printf("  WASD         - Move camera\n");
    printf("  QE           - Zoom camera\n");
    printf("  1/2/3        - Toggle UI panels\n");
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_state.initialized) return;
    
    g_state.time_accumulator += dt;
    g_state.demo_rotation += dt * 0.5f;
    
    // Handle input
    if (platform->input.keys[KEY_ESCAPE].pressed) {
        platform->window.should_close = true;
    }
    
    if (platform->input.keys[KEY_SPACE].pressed && g_state.physics_enabled) {
        g_state.physics_paused = !g_state.physics_paused;
    }
    
    if (platform->input.keys[KEY_R].pressed && g_state.physics_enabled) {
        CreatePhysicsScene(&g_state);
    }
    
    // Toggle panels
    if (platform->input.keys[KEY_1].pressed) {
        g_state.show_renderer_panel = !g_state.show_renderer_panel;
    }
    if (platform->input.keys[KEY_2].pressed) {
        g_state.show_physics_panel = !g_state.show_physics_panel;
    }
    if (platform->input.keys[KEY_3].pressed) {
        g_state.show_stats_panel = !g_state.show_stats_panel;
    }
    
    // Camera controls
    Camera2D* camera = &g_state.renderer.camera;
    f32 camera_speed = 3.0f * dt;
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
    
    // Physics interaction
    if (g_state.physics_enabled) {
        // Convert mouse to world coordinates
        g_state.mouse_world_pos = ScreenToWorld(
            V2(platform->input.mouse_x, platform->input.mouse_y),
            camera,
            g_state.renderer.viewport_width,
            g_state.renderer.viewport_height
        );
        
        // Mouse dragging
        if (platform->input.mouse[MOUSE_LEFT].pressed) {
            g_state.dragged_body = FindBodyAtPosition(&g_state.physics, g_state.mouse_world_pos);
        }
        
        if (platform->input.mouse[MOUSE_LEFT].down && g_state.dragged_body) {
            if (g_state.dragged_body->type == BODY_TYPE_DYNAMIC) {
                v2 delta = v2_sub(g_state.mouse_world_pos, g_state.dragged_body->position);
                g_state.dragged_body->velocity = v2_scale(delta, 8.0f);
            }
        } else {
            g_state.dragged_body = NULL;
        }
        
        // Spawn bodies
        g_state.spawn_timer -= dt;
        if (g_state.spawn_timer <= 0) {
            if (platform->input.keys[KEY_C].down) {
                SpawnPhysicsBody(&g_state, g_state.mouse_world_pos, true);
                g_state.spawn_timer = 0.1f;
            }
            if (platform->input.keys[KEY_B].down) {
                SpawnPhysicsBody(&g_state, g_state.mouse_world_pos, false);
                g_state.spawn_timer = 0.1f;
            }
        }
        
        // Update physics
        if (!g_state.physics_paused) {
            Physics2DStep(&g_state.physics, dt);
        }
    }
    
    // Update viewport if resized
    if (platform->window.resized) {
        RendererSetViewport(&g_state.renderer, platform->window.width, platform->window.height);
    }
}

void GameRender(PlatformState* platform) {
    if (!g_state.initialized) return;
    
    // Animated background
    f32 time = g_state.time_accumulator;
    f32 r = 0.05f + 0.03f * sinf(time * 0.5f);
    f32 g = 0.08f + 0.03f * sinf(time * 0.7f);
    f32 b = 0.12f + 0.03f * sinf(time * 0.3f);
    
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Begin rendering
    RendererBeginFrame(&g_state.renderer);
    HandmadeGUI_BeginFrame(&g_state.gui, platform);
    
    // Draw non-physics demo objects (background decoration)
    if (!g_state.physics_enabled) {
        // Draw some animated shapes if physics is disabled
        for (int i = 0; i < 5; i++) {
            f32 offset_x = -2.0f + i * 1.0f;
            f32 offset_y = sinf(time + i * 0.5f) * 0.5f;
            Color color = COLOR(
                0.5f + 0.3f * sinf(time + i * 1.2f),
                0.5f + 0.3f * sinf(time + i * 1.7f + 1.0f),
                0.5f + 0.3f * sinf(time + i * 2.1f + 2.0f),
                0.8f
            );
            RendererDrawCircle(&g_state.renderer, V2(offset_x, offset_y), 0.2f, color, 32);
        }
    }
    
    // Draw physics world
    if (g_state.physics_enabled) {
        Physics2DDebugDraw(&g_state.physics, &g_state.renderer);
        
        // Draw mouse cursor
        RendererDrawCircle(&g_state.renderer, g_state.mouse_world_pos, 0.02f, COLOR_WHITE, 16);
        
        // Draw drag line
        if (g_state.dragged_body) {
            RendererDrawLine(&g_state.renderer, g_state.mouse_world_pos, 
                           g_state.dragged_body->position, 0.02f, COLOR_YELLOW);
        }
    }
    
    // === GUI PANELS ===
    
    // Renderer Panel
    if (g_state.show_renderer_panel) {
        GUIPanel renderer_panel = {
            .position = V2(10.0f, 10.0f),
            .size = V2(200.0f, 120.0f),
            .title = "Renderer",
            .open = &g_state.show_renderer_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &renderer_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            char text[128];
            snprintf(text, sizeof(text), "Camera: %.2f, %.2f", 
                    g_state.renderer.camera.position.x, g_state.renderer.camera.position.y);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            cursor.y -= 20.0f;
            snprintf(text, sizeof(text), "Zoom: %.2f", g_state.renderer.camera.zoom);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            cursor.y -= 20.0f;
            snprintf(text, sizeof(text), "Viewport: %dx%d", 
                    g_state.renderer.viewport_width, g_state.renderer.viewport_height);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Physics Panel
    if (g_state.show_physics_panel && g_state.physics_enabled) {
        GUIPanel physics_panel = {
            .position = V2(220.0f, 10.0f),
            .size = V2(220.0f, 200.0f),
            .title = "Physics",
            .open = &g_state.show_physics_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &physics_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            // Pause button
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), 
                                  g_state.physics_paused ? "Resume" : "Pause")) {
                g_state.physics_paused = !g_state.physics_paused;
            }
            
            cursor.y -= 35.0f;
            
            // Reset button
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), "Reset")) {
                CreatePhysicsScene(&g_state);
            }
            
            cursor.y -= 35.0f;
            
            // Debug options
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show AABBs", 
                                &g_state.physics.debug_draw_aabb);
            
            cursor.y -= 25.0f;
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show Velocities", 
                                &g_state.physics.debug_draw_velocities);
            
            cursor.y -= 25.0f;
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show Contacts", 
                                &g_state.physics.debug_draw_contacts);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Stats Panel
    if (g_state.show_stats_panel) {
        GUIPanel stats_panel = {
            .position = V2(450.0f, 10.0f),
            .size = V2(180.0f, 160.0f),
            .title = "Statistics",
            .open = &g_state.show_stats_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &stats_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            char text[128];
            snprintf(text, sizeof(text), "Time: %.2f", g_state.time_accumulator);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            if (g_state.physics_enabled) {
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Bodies: %u/%u", 
                        g_state.physics.body_count, g_state.physics.max_bodies);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Contacts: %u", g_state.physics.contact_count);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Checks: %u", g_state.physics.collision_checks);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Status: %s", 
                        g_state.physics_paused ? "PAUSED" : "RUNNING");
                HandmadeGUI_Label(&g_state.gui, cursor, text);
            }
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Title and instructions
    v2 overlay_pos = V2(10.0f, (f32)g_state.gui.renderer->viewport_height - 100.0f);
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "Handmade Engine + Physics", 1.2f, COLOR_WHITE);
    
    overlay_pos.y -= 25.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "C/B spawn | Mouse drag | 1/2/3 panels", 
                    1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    overlay_pos.y -= 20.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "WASD move | QE zoom | Space pause", 
                    1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    // End rendering
    HandmadeGUI_EndFrame(&g_state.gui);
    RendererEndFrame(&g_state.renderer);
}

void GameShutdown(PlatformState* platform) {
    printf("Shutting down engine with physics\n");
    
    // Cleanup
    HandmadeGUI_Shutdown(&g_state.gui);
    RendererShutdown(&g_state.renderer);
    
    if (g_state.physics_enabled) {
        Physics2DShutdown(&g_state.physics);
    }
    
    if (g_state.physics_memory) {
        free(g_state.physics_memory);
        g_state.physics_memory = NULL;
    }
    
    g_state.initialized = false;
}

void GameOnReload(PlatformState* platform) {
    printf("Game hot-reloaded\n");
}