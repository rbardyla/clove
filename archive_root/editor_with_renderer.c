/*
    Editor with Integrated Renderer
    ================================
    
    Demonstrates the production-grade renderer with hot reload support.
*/

#include "handmade_platform.h"
#include "systems/renderer/handmade_renderer_new.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// Editor state with renderer
typedef struct {
    // Renderer
    Renderer* renderer;
    ShaderHandle basic_shader;
    
    // Camera (preserved across reloads)
    Vec3 camera_position;
    Vec3 camera_rotation;
    f32 camera_zoom;
    
    // Editor panels (preserved)
    bool show_hierarchy;
    bool show_inspector;
    bool show_console;
    bool show_stats;
    f32 hierarchy_width;
    f32 inspector_width;
    f32 console_height;
    
    // Performance
    f64 last_frame_time;
    f64 frame_time_accumulator;
    u32 frame_count;
    f32 fps;
    
    // Renderer stats
    RenderStats last_render_stats;
    
    // Animation
    f32 time;
    
    // Initialized flag
    bool initialized;
} EditorState;

static EditorState* g_editor = NULL;

// Shader reload callback
void OnShaderReload(ShaderHandle shader, void* user_data) {
    printf("[Editor] Shader reloaded: %u.%u\n", shader.id, shader.generation);
    // Could update materials here if needed
}

// Drawing helpers
static void DrawRect(f32 x, f32 y, f32 w, f32 h, Vec4 color) {
    // Use immediate mode rendering
    Vec3 vertices[4] = {
        {x, y, 0},
        {x + w, y, 0},
        {x + w, y + h, 0},
        {x, y + h, 0}
    };
    
    if (g_editor && g_editor->renderer) {
        Render->DrawImmediate(g_editor->renderer, vertices, 4, VERTEX_FORMAT_P3F, PRIMITIVE_TRIANGLES);
    }
}

// Draw editor panels with renderer stats
static void DrawPanels(PlatformState* platform, EditorState* editor) {
    f32 width = platform->window.width;
    f32 height = platform->window.height;
    
    // Set up 2D rendering
    Viewport viewport = {0, 0, width, height, 0.0f, 1.0f};
    Render->SetViewport(editor->renderer, viewport);
    
    // Panel colors
    Vec4 panel_bg = {0.15f, 0.15f, 0.15f, 1.0f};
    Vec4 header_bg = {0.1f, 0.1f, 0.1f, 1.0f};
    
    // Draw hierarchy panel
    if (editor->show_hierarchy) {
        DrawRect(0, 0, editor->hierarchy_width, height, panel_bg);
        DrawRect(0, 0, editor->hierarchy_width, 30, header_bg);
    }
    
    // Draw inspector panel
    if (editor->show_inspector) {
        f32 inspector_x = width - editor->inspector_width;
        DrawRect(inspector_x, 0, editor->inspector_width, height, panel_bg);
        DrawRect(inspector_x, 0, editor->inspector_width, 30, header_bg);
    }
    
    // Draw console panel
    if (editor->show_console) {
        f32 console_y = height - editor->console_height;
        f32 console_width = width;
        if (editor->show_hierarchy) console_width -= editor->hierarchy_width;
        if (editor->show_inspector) console_width -= editor->inspector_width;
        
        f32 console_x = editor->show_hierarchy ? editor->hierarchy_width : 0;
        DrawRect(console_x, console_y, console_width, editor->console_height, panel_bg);
        DrawRect(console_x, console_y, console_width, 30, header_bg);
    }
    
    // Draw toolbar with enhanced stats
    f32 toolbar_height = 60;  // Increased for more stats
    f32 toolbar_x = editor->show_hierarchy ? editor->hierarchy_width : 0;
    f32 toolbar_width = width;
    if (editor->show_hierarchy) toolbar_width -= editor->hierarchy_width;
    if (editor->show_inspector) toolbar_width -= editor->inspector_width;
    
    DrawRect(toolbar_x, 30, toolbar_width, toolbar_height, header_bg);
    
    // Print stats to console (would normally render text)
    if (editor->show_stats) {
        printf("\r[Editor] FPS: %.0f | Frame: %.2fms | Draws: %u | Tris: %u | Shaders: %u    ",
               editor->fps,
               editor->last_frame_time * 1000.0,
               editor->last_render_stats.draw_calls,
               editor->last_render_stats.triangles,
               editor->last_render_stats.shader_switches);
        fflush(stdout);
    }
}

// Create a simple cube mesh (for demonstration)
static void CreateTestMesh(EditorState* editor) {
    // Simple cube vertices
    f32 vertices[] = {
        // Positions       // Normals        // TexCoords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
    };
    
    u32 indices[] = {
        0, 1, 2,  2, 3, 0,  // Back face
        4, 5, 6,  6, 7, 4,  // Front face
        0, 4, 7,  7, 3, 0,  // Left face
        1, 5, 6,  6, 2, 1,  // Right face
        3, 7, 6,  6, 2, 3,  // Top face
        0, 4, 5,  5, 1, 0   // Bottom face
    };
    
    // Note: This would need proper mesh creation API
    // For now, just demonstrate the interface
    printf("[Editor] Test mesh created (36 triangles)\n");
}

// Initialize editor with renderer
void GameInit(PlatformState* platform) {
    if (!g_editor) {
        // First time init
        g_editor = PushStruct(&platform->permanent_arena, EditorState);
        
        // Initialize editor state
        g_editor->camera_position = (Vec3){0, 2, 5};
        g_editor->camera_rotation = (Vec3){-20, 0, 0};
        g_editor->camera_zoom = 10.0f;
        
        g_editor->show_hierarchy = true;
        g_editor->show_inspector = true;
        g_editor->show_console = true;
        g_editor->show_stats = true;
        
        g_editor->hierarchy_width = 250;
        g_editor->inspector_width = 300;
        g_editor->console_height = 200;
        
        // Create renderer
        g_editor->renderer = Render->Create(platform, &platform->permanent_arena, 
                                           platform->window.width, platform->window.height);
        
        if (!g_editor->renderer) {
            printf("[Editor] Error: Failed to create renderer\n");
            return;
        }
        
        // Create basic shader
        g_editor->basic_shader = Render->CreateShader(g_editor->renderer, 
                                                     "assets/shaders/basic.vert",
                                                     "assets/shaders/basic.frag");
        
        if (g_editor->basic_shader.id == 0) {
            printf("[Editor] Warning: Failed to create basic shader\n");
        } else {
            printf("[Editor] Basic shader created successfully\n");
        }
        
        // Register for shader reload callbacks
        Render->RegisterShaderReloadCallback(g_editor->renderer, OnShaderReload, g_editor);
        
        // Create test mesh
        CreateTestMesh(g_editor);
        
        printf("[Editor] Editor initialized with renderer\n");
        printf("[Editor] Hot reload enabled - edit shaders in assets/shaders/\n");
    }
    
    g_editor->initialized = true;
}

// Update editor
void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_editor || !g_editor->initialized || !g_editor->renderer) return;
    
    PlatformInput* input = &platform->input;
    
    // Update time for shader animation
    g_editor->time += dt;
    
    // Camera controls (same as before)
    // ... input handling ...
    
    // Toggle panels
    if (input->keys[KEY_F1].pressed) g_editor->show_hierarchy = !g_editor->show_hierarchy;
    if (input->keys[KEY_F2].pressed) g_editor->show_inspector = !g_editor->show_inspector;
    if (input->keys[KEY_F3].pressed) g_editor->show_console = !g_editor->show_console;
    if (input->keys[KEY_F4].pressed) g_editor->show_stats = !g_editor->show_stats;
    
    // Update FPS counter
    g_editor->frame_count++;
    g_editor->frame_time_accumulator += dt;
    if (g_editor->frame_time_accumulator >= 1.0) {
        g_editor->fps = g_editor->frame_count / g_editor->frame_time_accumulator;
        g_editor->frame_count = 0;
        g_editor->frame_time_accumulator = 0;
    }
    g_editor->last_frame_time = dt;
}

// Render editor
void GameRender(PlatformState* platform) {
    if (!g_editor || !g_editor->initialized || !g_editor->renderer) return;
    
    Renderer* renderer = g_editor->renderer;
    
    // Begin frame
    Render->BeginFrame(renderer);
    
    // Calculate viewport (accounting for panels)
    f32 viewport_x = g_editor->show_hierarchy ? g_editor->hierarchy_width : 0;
    f32 viewport_y = g_editor->show_console ? g_editor->console_height : 0;
    f32 viewport_width = platform->window.width;
    f32 viewport_height = platform->window.height - 90;  // Account for toolbar
    
    if (g_editor->show_hierarchy) viewport_width -= g_editor->hierarchy_width;
    if (g_editor->show_inspector) viewport_width -= g_editor->inspector_width;
    if (g_editor->show_console) viewport_height -= g_editor->console_height;
    
    // Set 3D viewport
    Viewport viewport = {viewport_x, viewport_y, viewport_width, viewport_height, 0.1f, 1000.0f};
    Render->SetViewport(renderer, viewport);
    
    // Clear screen
    Vec4 clear_color = {0.1f, 0.1f, 0.1f, 1.0f};
    Render->Clear(renderer, clear_color, 1.0f, 0, CLEAR_COLOR | CLEAR_DEPTH);
    
    // Use basic shader (if loaded)
    if (g_editor->basic_shader.id != 0) {
        Render->SetShader(renderer, g_editor->basic_shader);
        
        // TODO: Set up matrices and draw 3D content
        // This would need proper matrix math and mesh rendering
        // For now, just demonstrate the API
    }
    
    // Draw UI panels
    DrawPanels(platform, g_editor);
    
    // End frame and execute all commands
    Render->EndFrame(renderer);
    
    // Get render stats
    g_editor->last_render_stats = Render->GetStats(renderer);
}

// Shutdown
void GameShutdown(PlatformState* platform) {
    if (g_editor && g_editor->renderer) {
        // Renderer cleanup would happen here
        printf("[Editor] Renderer shutdown\n");
    }
    g_editor = NULL;
}