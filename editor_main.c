/*
    Handmade Editor - Main Entry Point
    Complete editor shell with all core systems integrated
*/

#include "handmade_platform.h"
#include <stdio.h>
#include <math.h>
#include <GL/gl.h>

// Basic math types (will be replaced with your math library)
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;
typedef struct { f32 m[16]; } mat4;

// Editor state
typedef struct {
    // Camera
    v3 camera_position;
    v3 camera_rotation;
    f32 camera_zoom;
    
    // Viewport control
    bool camera_rotating;
    bool camera_panning;
    f32 last_mouse_x;
    f32 last_mouse_y;
    
    // Editor panels
    bool show_hierarchy;
    bool show_inspector;
    bool show_console;
    bool show_assets;
    f32 hierarchy_width;
    f32 inspector_width;
    f32 console_height;
    
    // Performance
    f64 last_frame_time;
    f64 frame_time_accumulator;
    u32 frame_count;
    f32 fps;
    
    // Grid
    bool show_grid;
    bool show_wireframe;
    bool show_stats;
    
    // Scene
    u32 selected_object;
    u32 object_count;
    
    // Tools
    enum { TOOL_SELECT, TOOL_MOVE, TOOL_ROTATE, TOOL_SCALE } current_tool;
    
    // Initialized flag
    bool initialized;
} EditorState;

static EditorState* g_editor = NULL;

// Simple immediate mode UI helpers
static void DrawRect(f32 x, f32 y, f32 w, f32 h, v4 color) {
    glBegin(GL_QUADS);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void DrawText(f32 x, f32 y, const char* text, v4 color) {
    // Placeholder - will integrate your font rendering
    glColor4f(color.x, color.y, color.z, color.w);
    // Text rendering would go here
}

static void DrawLine(v3 start, v3 end, v4 color) {
    glBegin(GL_LINES);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex3f(start.x, start.y, start.z);
    glVertex3f(end.x, end.y, end.z);
    glEnd();
}

// Draw 3D grid
static void DrawGrid(f32 size, f32 spacing) {
    v4 grid_color = {0.3f, 0.3f, 0.3f, 1.0f};
    v4 axis_color = {0.5f, 0.5f, 0.5f, 1.0f};
    
    int lines = (int)(size / spacing);
    f32 half_size = size * 0.5f;
    
    for (int i = -lines; i <= lines; i++) {
        f32 pos = i * spacing;
        v4 color = (i == 0) ? axis_color : grid_color;
        
        // X-axis lines
        DrawLine((v3){-half_size, 0, pos}, (v3){half_size, 0, pos}, color);
        // Z-axis lines
        DrawLine((v3){pos, 0, -half_size}, (v3){pos, 0, half_size}, color);
    }
    
    // Draw main axes
    DrawLine((v3){0, 0, 0}, (v3){1, 0, 0}, (v4){1, 0, 0, 1});  // X - Red
    DrawLine((v3){0, 0, 0}, (v3){0, 1, 0}, (v4){0, 1, 0, 1});  // Y - Green
    DrawLine((v3){0, 0, 0}, (v3){0, 0, 1}, (v4){0, 0, 1, 1});  // Z - Blue
}

// Setup perspective projection
static void SetupProjection(f32 width, f32 height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    f32 aspect = width / height;
    f32 fov = 60.0f;
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    
    f32 top = near_plane * tanf(fov * 0.5f * 3.14159f / 180.0f);
    f32 bottom = -top;
    f32 left = bottom * aspect;
    f32 right = top * aspect;
    
    glFrustum(left, right, bottom, top, near_plane, far_plane);
}

// Setup camera view
static void SetupCamera(EditorState* editor) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Simple orbit camera
    glTranslatef(0, 0, -editor->camera_zoom);
    glRotatef(editor->camera_rotation.x, 1, 0, 0);
    glRotatef(editor->camera_rotation.y, 0, 1, 0);
    glTranslatef(-editor->camera_position.x, 
                 -editor->camera_position.y, 
                 -editor->camera_position.z);
}

// Draw editor panels
static void DrawPanels(PlatformState* platform, EditorState* editor) {
    f32 width = platform->window.width;
    f32 height = platform->window.height;
    
    // Setup 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    
    // Panel colors
    v4 panel_bg = {0.15f, 0.15f, 0.15f, 1.0f};
    v4 header_bg = {0.1f, 0.1f, 0.1f, 1.0f};
    v4 text_color = {0.9f, 0.9f, 0.9f, 1.0f};
    
    // Draw hierarchy panel
    if (editor->show_hierarchy) {
        DrawRect(0, 0, editor->hierarchy_width, height, panel_bg);
        DrawRect(0, 0, editor->hierarchy_width, 30, header_bg);
        DrawText(10, 20, "Hierarchy", text_color);
    }
    
    // Draw inspector panel
    if (editor->show_inspector) {
        f32 inspector_x = width - editor->inspector_width;
        DrawRect(inspector_x, 0, editor->inspector_width, height, panel_bg);
        DrawRect(inspector_x, 0, editor->inspector_width, 30, header_bg);
        DrawText(inspector_x + 10, 20, "Inspector", text_color);
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
        DrawText(console_x + 10, console_y + 20, "Console", text_color);
    }
    
    // Draw toolbar
    f32 toolbar_height = 40;
    f32 toolbar_x = editor->show_hierarchy ? editor->hierarchy_width : 0;
    f32 toolbar_width = width;
    if (editor->show_hierarchy) toolbar_width -= editor->hierarchy_width;
    if (editor->show_inspector) toolbar_width -= editor->inspector_width;
    
    DrawRect(toolbar_x, 30, toolbar_width, toolbar_height, header_bg);
    
    // Draw FPS and memory stats
    if (editor->show_stats) {
        char stats_text[256];
        snprintf(stats_text, sizeof(stats_text), 
                 "FPS: %.0f | Frame: %.2fms | Mem: P:%.1fMB/%.0fGB F:%.1fMB/%.0fMB", 
                 editor->fps, editor->last_frame_time * 1000.0,
                 platform->permanent_arena.used / (1024.0 * 1024.0),
                 platform->permanent_arena.size / (1024.0 * 1024.0 * 1024.0),
                 platform->frame_arena.used / (1024.0 * 1024.0),
                 platform->frame_arena.size / (1024.0 * 1024.0));
        DrawText(toolbar_x + 10, 50, stats_text, text_color);
    }
    
    // Restore 3D rendering state
    glEnable(GL_DEPTH_TEST);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

// Initialize editor
void GameInit(PlatformState* platform) {
    // Allocate editor state from permanent memory
    g_editor = PushStruct(&platform->permanent_arena, EditorState);
    
    // Initialize editor state
    g_editor->camera_position = (v3){0, 0, 0};
    g_editor->camera_rotation = (v3){-30, 45, 0};
    g_editor->camera_zoom = 10.0f;
    
    g_editor->show_hierarchy = true;
    g_editor->show_inspector = true;
    g_editor->show_console = true;
    g_editor->show_grid = true;
    g_editor->show_stats = true;
    
    g_editor->hierarchy_width = 250;
    g_editor->inspector_width = 300;
    g_editor->console_height = 200;
    
    g_editor->current_tool = TOOL_SELECT;
    
    // Setup OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    g_editor->initialized = true;
    
    Platform->DebugPrint("Editor initialized successfully\n");
}

// Update editor
void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_editor || !g_editor->initialized) return;
    
    PlatformInput* input = &platform->input;
    
    // Camera controls
    if (input->mouse[MOUSE_RIGHT].down) {
        if (!g_editor->camera_rotating) {
            g_editor->camera_rotating = true;
            g_editor->last_mouse_x = input->mouse_x;
            g_editor->last_mouse_y = input->mouse_y;
        }
        
        f32 dx = input->mouse_x - g_editor->last_mouse_x;
        f32 dy = input->mouse_y - g_editor->last_mouse_y;
        
        g_editor->camera_rotation.y += dx * 0.5f;
        g_editor->camera_rotation.x += dy * 0.5f;
        
        // Clamp vertical rotation
        if (g_editor->camera_rotation.x > 89.0f) g_editor->camera_rotation.x = 89.0f;
        if (g_editor->camera_rotation.x < -89.0f) g_editor->camera_rotation.x = -89.0f;
        
        g_editor->last_mouse_x = input->mouse_x;
        g_editor->last_mouse_y = input->mouse_y;
    } else {
        g_editor->camera_rotating = false;
    }
    
    // Middle mouse for panning
    if (input->mouse[MOUSE_MIDDLE].down) {
        if (!g_editor->camera_panning) {
            g_editor->camera_panning = true;
            g_editor->last_mouse_x = input->mouse_x;
            g_editor->last_mouse_y = input->mouse_y;
        }
        
        f32 dx = input->mouse_x - g_editor->last_mouse_x;
        f32 dy = input->mouse_y - g_editor->last_mouse_y;
        
        g_editor->camera_position.x -= dx * 0.01f * g_editor->camera_zoom;
        g_editor->camera_position.y += dy * 0.01f * g_editor->camera_zoom;
        
        g_editor->last_mouse_x = input->mouse_x;
        g_editor->last_mouse_y = input->mouse_y;
    } else {
        g_editor->camera_panning = false;
    }
    
    // Scroll for zoom
    if (input->mouse_wheel != 0) {
        g_editor->camera_zoom -= input->mouse_wheel * 2.0f;
        if (g_editor->camera_zoom < 1.0f) g_editor->camera_zoom = 1.0f;
        if (g_editor->camera_zoom > 100.0f) g_editor->camera_zoom = 100.0f;
    }
    
    // Keyboard shortcuts
    if (input->keys[KEY_F1].pressed) g_editor->show_hierarchy = !g_editor->show_hierarchy;
    if (input->keys[KEY_F2].pressed) g_editor->show_inspector = !g_editor->show_inspector;
    if (input->keys[KEY_F3].pressed) g_editor->show_console = !g_editor->show_console;
    if (input->keys[KEY_G].pressed) g_editor->show_grid = !g_editor->show_grid;
    
    // Tool selection
    if (input->keys[KEY_Q].pressed) g_editor->current_tool = TOOL_SELECT;
    if (input->keys[KEY_W].pressed) g_editor->current_tool = TOOL_MOVE;
    if (input->keys[KEY_E].pressed) g_editor->current_tool = TOOL_ROTATE;
    if (input->keys[KEY_R].pressed) g_editor->current_tool = TOOL_SCALE;
    
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
    if (!g_editor || !g_editor->initialized) return;
    
    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Calculate viewport (accounting for panels)
    f32 viewport_x = g_editor->show_hierarchy ? g_editor->hierarchy_width : 0;
    f32 viewport_y = g_editor->show_console ? g_editor->console_height : 0;
    f32 viewport_width = platform->window.width;
    f32 viewport_height = platform->window.height - 70;  // Account for toolbar
    
    if (g_editor->show_hierarchy) viewport_width -= g_editor->hierarchy_width;
    if (g_editor->show_inspector) viewport_width -= g_editor->inspector_width;
    if (g_editor->show_console) viewport_height -= g_editor->console_height;
    
    // Setup 3D viewport
    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    
    // Setup projection and camera
    SetupProjection(viewport_width, viewport_height);
    SetupCamera(g_editor);
    
    // Draw grid
    if (g_editor->show_grid) {
        DrawGrid(100.0f, 1.0f);
    }
    
    // Draw test cube
    glPushMatrix();
    glTranslatef(0, 0.5f, 0);
    
    glBegin(GL_QUADS);
    glColor3f(0.5f, 0.5f, 1.0f);
    // Front face
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    // Back face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    // Top face
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    // Bottom face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    // Right face
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    // Left face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();
    
    glPopMatrix();
    
    // Reset viewport for UI rendering
    glViewport(0, 0, platform->window.width, platform->window.height);
    
    // Draw UI panels
    DrawPanels(platform, g_editor);
}

// Shutdown editor
void GameShutdown(PlatformState* platform) {
    Platform->DebugPrint("Editor shutting down\n");
    g_editor = NULL;
}

// Hot reload callback
void GameOnReload(PlatformState* platform) {
    Platform->DebugPrint("Editor module reloaded\n");
    // Restore editor state pointer after reload
    // The actual data is preserved in platform memory
}