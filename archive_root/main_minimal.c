/*
    HANDMADE ENGINE WITH BASIC RENDERER
    
    Following Casey Muratori's philosophy:
    1. Always have something working
    2. Build up, don't build out
    3. Understand every line of code
    4. No black boxes
    
    Now with basic rendering capabilities:
    - Draw colored shapes (triangles, quads, circles)
    - 2D sprite rendering with textures
    - Simple BMP texture loading
    - Basic 2D camera system
*/

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include "handmade_gui.h"
#include <stdio.h>
#include <math.h>
#include <GL/gl.h>

// Application state with renderer and GUI
typedef struct {
    bool initialized;
    f32 time_accumulator;
    f32 background_color[3];
    
    // Renderer
    Renderer renderer;
    
    // GUI system
    HandmadeGUI gui;
    
    // Demo objects
    Texture test_texture;
    f32 demo_rotation;
    
    // GUI demo state
    bool show_debug_panel;
    bool show_demo_panel;
    bool demo_checkbox;
    f32 demo_slider_value;
} GameState;

static GameState g_state = {0};

// Required Game* functions that platform expects
void GameInit(PlatformState* platform) {
    printf("=== HANDMADE ENGINE WITH RENDERER ===\n");
    printf("Window size: %dx%d\n", platform->window.width, platform->window.height);
    
    g_state.initialized = true;
    g_state.time_accumulator = 0.0f;
    g_state.demo_rotation = 0.0f;
    
    // Set up initial background color (dark blue)
    g_state.background_color[0] = 0.1f;
    g_state.background_color[1] = 0.15f; 
    g_state.background_color[2] = 0.2f;
    
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
    
    // Initialize GUI demo state
    g_state.show_debug_panel = true;
    g_state.show_demo_panel = true;
    g_state.demo_checkbox = false;
    g_state.demo_slider_value = 0.5f;
    
    // Try to load a test texture (optional - will use white texture if not found)
    // g_state.test_texture = RendererLoadTextureBMP(&g_state.renderer, "test.bmp");
    
    printf("Renderer and GUI initialized successfully\n");
    printf("Controls:\n");
    printf("  ESC - Quit\n");
    printf("  SPACE - Print debug info\n");
    printf("  WASD - Move camera\n");
    printf("  QE - Zoom camera\n");
    printf("  G - Toggle GUI debug panel\n");
    printf("  H - Toggle GUI demo panel\n");
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_state.initialized) return;
    
    g_state.time_accumulator += dt;
    g_state.demo_rotation += dt * 0.5f; // Slow rotation for demo
    
    // Animate background color slightly
    f32 time = g_state.time_accumulator;
    g_state.background_color[0] = 0.1f + 0.05f * sinf(time * 0.5f);
    g_state.background_color[1] = 0.15f + 0.05f * sinf(time * 0.7f);
    g_state.background_color[2] = 0.2f + 0.05f * sinf(time * 0.3f);
    
    // Handle input
    if (platform->input.keys[KEY_ESCAPE].pressed) {
        platform->window.should_close = true;
    }
    
    if (platform->input.keys[KEY_SPACE].pressed) {
        printf("=== Renderer Debug Info at %.2fs ===\n", g_state.time_accumulator);
        RendererShowDebugInfo(&g_state.renderer);
    }
    
    if (platform->input.keys[KEY_G].pressed) {
        g_state.show_debug_panel = !g_state.show_debug_panel;
    }
    
    if (platform->input.keys[KEY_H].pressed) {
        g_state.show_demo_panel = !g_state.show_demo_panel;
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
        if (camera->zoom > 10.0f) camera->zoom = 10.0f;
    }
    
    // Update viewport if window was resized
    if (platform->window.resized) {
        RendererSetViewport(&g_state.renderer, platform->window.width, platform->window.height);
    }
}

void GameRender(PlatformState* platform) {
    if (!g_state.initialized) return;
    
    // Clear screen with animated background color
    glClearColor(g_state.background_color[0], 
                 g_state.background_color[1], 
                 g_state.background_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Begin renderer frame
    RendererBeginFrame(&g_state.renderer);
    
    // Begin GUI frame
    HandmadeGUI_BeginFrame(&g_state.gui, platform);
    
    // Draw various demo shapes to show renderer capabilities
    f32 time = g_state.time_accumulator;
    
    // Animated colored rectangles
    for (int i = 0; i < 5; i++) {
        f32 offset_x = -1.5f + i * 0.75f;
        f32 offset_y = sinf(time + i * 0.5f) * 0.3f;
        Color color = COLOR(
            0.5f + 0.5f * sinf(time + i * 1.2f),
            0.5f + 0.5f * sinf(time + i * 1.7f + 1.0f),
            0.5f + 0.5f * sinf(time + i * 2.1f + 2.0f),
            1.0f
        );
        RendererDrawRect(&g_state.renderer, V2(offset_x, offset_y), V2(0.4f, 0.4f), color);
    }
    
    // Rotating triangle in center
    Triangle triangle = {
        .p1 = V2(0.0f + 0.3f * cosf(g_state.demo_rotation), 
                  0.8f + 0.3f * sinf(g_state.demo_rotation)),
        .p2 = V2(-0.26f + 0.3f * cosf(g_state.demo_rotation + 2.09f), 
                  0.65f + 0.3f * sinf(g_state.demo_rotation + 2.09f)),
        .p3 = V2(0.26f + 0.3f * cosf(g_state.demo_rotation - 2.09f), 
                  0.65f + 0.3f * sinf(g_state.demo_rotation - 2.09f)),
        .color = COLOR_YELLOW
    };
    RendererDrawTriangle(&g_state.renderer, &triangle);
    
    // Circle that pulsates
    f32 pulse = 0.15f + 0.05f * sinf(time * 3.0f);
    RendererDrawCircle(&g_state.renderer, V2(0.0f, -0.8f), pulse, COLOR_GREEN, 32);
    
    // Rectangle outline that changes thickness
    f32 thickness = 0.02f + 0.01f * sinf(time * 2.0f);
    RendererDrawRectOutline(&g_state.renderer, V2(1.2f, 0.5f), V2(0.8f, 0.6f), thickness, COLOR_RED);
    
    // Animated line
    v2 line_start = V2(-1.2f, -0.5f);
    v2 line_end = V2(-1.2f + 0.8f * cosf(time), -0.5f + 0.3f * sinf(time * 2.0f));
    RendererDrawLine(&g_state.renderer, line_start, line_end, 0.05f, COLOR_BLUE);
    
    // Sprite demo (using white texture with color tint)
    Sprite sprite = {
        .position = V2(-0.8f, 0.8f),
        .size = V2(0.3f, 0.3f),
        .rotation = g_state.demo_rotation * 0.5f,
        .color = COLOR(1.0f, 0.8f, 0.4f, 0.8f),
        .texture = g_state.renderer.white_texture,
        .texture_offset = V2(0.0f, 0.0f),
        .texture_scale = V2(1.0f, 1.0f)
    };
    RendererDrawSprite(&g_state.renderer, &sprite);
    
    // If we loaded a texture, draw it
    if (g_state.test_texture.valid) {
        Sprite textured_sprite = {
            .position = V2(0.8f, 0.8f),
            .size = V2(0.5f, 0.5f),
            .rotation = -g_state.demo_rotation * 0.3f,
            .color = COLOR_WHITE,
            .texture = g_state.test_texture,
            .texture_offset = V2(0.0f, 0.0f),
            .texture_scale = V2(1.0f, 1.0f)
        };
        RendererDrawSprite(&g_state.renderer, &textured_sprite);
    }
    
    // === GUI RENDERING ===
    
    // GUI Debug Panel
    if (g_state.show_debug_panel) {
        GUIPanel debug_panel = {
            .position = V2(10.0f, 10.0f),
            .size = V2(250.0f, 150.0f),
            .title = "Debug Info",
            .open = &g_state.show_debug_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &debug_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            // Display debug information
            char debug_text[256];
            snprintf(debug_text, sizeof(debug_text), "FPS: %.1f", 1.0f / 0.016f); // Approximate
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Time: %.2f", g_state.time_accumulator);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Camera: %.2f, %.2f", 
                    g_state.renderer.camera.position.x, g_state.renderer.camera.position.y);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            cursor.y -= 20.0f;
            snprintf(debug_text, sizeof(debug_text), "Zoom: %.2f", g_state.renderer.camera.zoom);
            HandmadeGUI_Label(&g_state.gui, cursor, debug_text);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // GUI Demo Panel
    if (g_state.show_demo_panel) {
        GUIPanel demo_panel = {
            .position = V2(300.0f, 10.0f),
            .size = V2(200.0f, 200.0f),
            .title = "GUI Demo",
            .open = &g_state.show_demo_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &demo_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            // Demo button
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), "Click Me!")) {
                printf("GUI Button clicked!\n");
            }
            
            cursor.y -= 35.0f;
            
            // Demo checkbox
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Enable Demo", &g_state.demo_checkbox);
            
            cursor.y -= 25.0f;
            
            // Simple text labels
            HandmadeGUI_Label(&g_state.gui, cursor, "GUI System Working!");
            
            cursor.y -= 20.0f;
            char status_text[128];
            snprintf(status_text, sizeof(status_text), "Checkbox: %s", 
                    g_state.demo_checkbox ? "ON" : "OFF");
            HandmadeGUI_Label(&g_state.gui, cursor, status_text);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Simple on-screen text overlay
    v2 overlay_pos = V2(10.0f, (f32)g_state.gui.renderer->viewport_height - 50.0f);
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "Handmade Engine with GUI", 1.2f, COLOR_WHITE);
    
    overlay_pos.y -= 25.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "Press G/H to toggle panels", 1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    // End GUI frame
    HandmadeGUI_EndFrame(&g_state.gui);
    
    // End renderer frame
    RendererEndFrame(&g_state.renderer);
}

void GameShutdown(PlatformState* platform) {
    printf("Shutting down engine with renderer and GUI\n");
    
    // Cleanup GUI
    HandmadeGUI_Shutdown(&g_state.gui);
    
    // Cleanup renderer
    if (g_state.test_texture.valid) {
        RendererFreeTexture(&g_state.renderer, &g_state.test_texture);
    }
    RendererShutdown(&g_state.renderer);
    
    g_state.initialized = false;
}

void GameOnReload(PlatformState* platform) {
    printf("Game hot-reloaded\n");
    // Nothing to do for hot reload in minimal version
}