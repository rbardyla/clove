/*
    Simple Enhanced Editor with Integrated Renderer
    ===============================================
    
    Demonstrates the integrated renderer architecture using immediate mode OpenGL.
    Shows editor panels, 3D viewport, and performance statistics.
*/

#include "handmade_platform.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <GL/gl.h>

// Math types
typedef struct { f32 x, y, z; } Vec3;
typedef struct { f32 x, y, z, w; } Vec4;

// Simple renderer state
typedef struct {
    // Window dimensions
    i32 width, height;
    
    // Camera
    Vec3 camera_position;
    Vec3 camera_rotation;
    f32 camera_zoom;
    
    // Editor panels
    bool show_hierarchy;
    bool show_inspector;
    bool show_console;
    bool show_stats;
    f32 hierarchy_width;
    f32 inspector_width;
    f32 console_height;
    
    // Performance tracking
    f64 last_frame_time;
    f64 frame_time_accumulator;
    u32 frame_count;
    f32 fps;
    
    // Animation time
    f32 time;
    
    // Initialization flag
    bool initialized;
} EditorState;

static EditorState* g_editor = NULL;

// Simple drawing helpers using immediate mode
static void DrawRect(f32 x, f32 y, f32 w, f32 h, Vec4 color) {
    glBegin(GL_QUADS);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void DrawLine(f32 x1, f32 y1, f32 x2, f32 y2, Vec4 color, f32 width) {
    glLineWidth(width);
    glBegin(GL_LINES);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

// Draw spinning cube using legacy OpenGL
static void DrawSpinningCube(f32 time) {
    glPushMatrix();
    
    // Move to center and rotate
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(time * 30.0f, 1.0f, 1.0f, 1.0f);
    
    // Draw cube faces with different colors
    glBegin(GL_QUADS);
    
    // Front face (animated like the shader)
    glColor3f(0.5f + 0.3f * sinf(time), 0.3f, 0.7f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    
    // Back face
    glColor3f(0.3f, 0.5f + 0.3f * sinf(time + 1.0f), 0.7f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    
    // Top face
    glColor3f(0.3f, 0.7f, 0.5f + 0.3f * sinf(time + 2.0f));
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    
    // Bottom face
    glColor3f(0.7f, 0.3f, 0.5f + 0.3f * sinf(time + 3.0f));
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    
    // Right face
    glColor3f(0.5f + 0.2f * sinf(time + 4.0f), 0.7f, 0.3f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    
    // Left face
    glColor3f(0.3f, 0.5f + 0.2f * sinf(time + 5.0f), 0.7f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    
    glEnd();
    
    glPopMatrix();
}

// Setup OpenGL for 3D rendering
static void Setup3DViewport(EditorState* editor) {
    // Calculate 3D viewport
    f32 viewport_x = editor->show_hierarchy ? editor->hierarchy_width : 0;
    f32 viewport_y = editor->show_console ? editor->console_height : 0;
    f32 viewport_width = editor->width;
    f32 viewport_height = editor->height - 90;  // Account for toolbar
    
    if (editor->show_hierarchy) viewport_width -= editor->hierarchy_width;
    if (editor->show_inspector) viewport_width -= editor->inspector_width;
    if (editor->show_console) viewport_height -= editor->console_height;
    
    glViewport((GLint)viewport_x, (GLint)viewport_y, (GLsizei)viewport_width, (GLsizei)viewport_height);
    
    // Setup 3D projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    f32 aspect = viewport_width / viewport_height;
    
    // Simple perspective projection
    f32 fov = 45.0f;
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    f32 f = 1.0f / tanf(fov * M_PI / 360.0f);
    
    f32 projection[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (far_plane + near_plane)/(near_plane - far_plane), -1,
        0, 0, (2*far_plane*near_plane)/(near_plane - far_plane), 0
    };
    
    glLoadMatrixf(projection);
    
    // Setup modelview
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Simple camera
    glTranslatef(editor->camera_position.x, editor->camera_position.y, editor->camera_position.z);
    glRotatef(editor->camera_rotation.x, 1, 0, 0);
    glRotatef(editor->camera_rotation.y, 0, 1, 0);
    glRotatef(editor->camera_rotation.z, 0, 0, 1);
    
    // Enable 3D features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

// Setup OpenGL for 2D UI rendering
static void Setup2DViewport(EditorState* editor) {
    glViewport(0, 0, editor->width, editor->height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, editor->width, editor->height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Draw editor UI panels
static void DrawPanels(EditorState* editor) {
    Setup2DViewport(editor);
    
    Vec4 panel_bg = {0.15f, 0.15f, 0.15f, 1.0f};
    Vec4 header_bg = {0.1f, 0.1f, 0.1f, 1.0f};
    Vec4 border_color = {0.3f, 0.3f, 0.3f, 1.0f};
    
    // Draw hierarchy panel
    if (editor->show_hierarchy) {
        DrawRect(0, 0, editor->hierarchy_width, editor->height, panel_bg);
        DrawRect(0, 0, editor->hierarchy_width, 30, header_bg);
        DrawLine(editor->hierarchy_width, 0, editor->hierarchy_width, editor->height, border_color, 1.0f);
    }
    
    // Draw inspector panel
    if (editor->show_inspector) {
        f32 inspector_x = editor->width - editor->inspector_width;
        DrawRect(inspector_x, 0, editor->inspector_width, editor->height, panel_bg);
        DrawRect(inspector_x, 0, editor->inspector_width, 30, header_bg);
        DrawLine(inspector_x, 0, inspector_x, editor->height, border_color, 1.0f);
    }
    
    // Draw console panel
    if (editor->show_console) {
        f32 console_y = editor->height - editor->console_height;
        f32 console_width = editor->width;
        if (editor->show_hierarchy) console_width -= editor->hierarchy_width;
        if (editor->show_inspector) console_width -= editor->inspector_width;
        
        f32 console_x = editor->show_hierarchy ? editor->hierarchy_width : 0;
        DrawRect(console_x, console_y, console_width, editor->console_height, panel_bg);
        DrawRect(console_x, console_y, console_width, 30, header_bg);
        DrawLine(console_x, console_y, console_x + console_width, console_y, border_color, 1.0f);
    }
    
    // Draw toolbar
    f32 toolbar_height = 60;
    f32 toolbar_x = editor->show_hierarchy ? editor->hierarchy_width : 0;
    f32 toolbar_width = editor->width;
    if (editor->show_hierarchy) toolbar_width -= editor->hierarchy_width;
    if (editor->show_inspector) toolbar_width -= editor->inspector_width;
    
    DrawRect(toolbar_x, 30, toolbar_width, toolbar_height, header_bg);
    DrawLine(toolbar_x, 90, toolbar_x + toolbar_width, 90, border_color, 1.0f);
    
    // Print stats
    if (editor->show_stats) {
        printf("\\r[Enhanced Editor] FPS: %.0f | Frame: %.2fms | Time: %.1fs | Panels: H:%s I:%s C:%s    ",
               editor->fps,
               editor->last_frame_time * 1000.0,
               editor->time,
               editor->show_hierarchy ? "ON" : "OFF",
               editor->show_inspector ? "ON" : "OFF",
               editor->show_console ? "ON" : "OFF");
        fflush(stdout);
    }
}

// Initialize editor
void GameInit(PlatformState* platform) {
    if (!g_editor) {
        g_editor = PushStruct(&platform->permanent_arena, EditorState);
        
        g_editor->width = platform->window.width;
        g_editor->height = platform->window.height;
        
        // Initialize camera
        g_editor->camera_position = (Vec3){0, 0, 0};
        g_editor->camera_rotation = (Vec3){-20, 0, 0};
        g_editor->camera_zoom = 10.0f;
        
        // Initialize panels
        g_editor->show_hierarchy = true;
        g_editor->show_inspector = true;
        g_editor->show_console = true;
        g_editor->show_stats = true;
        
        g_editor->hierarchy_width = 250;
        g_editor->inspector_width = 300;
        g_editor->console_height = 200;
        
        printf("[Enhanced Editor] Initialized with immediate mode renderer\\n");
        printf("[Enhanced Editor] Press F1-F4 to toggle panels, WASD for camera, ESC to exit\\n");
        printf("[Enhanced Editor] Demonstrating integrated renderer architecture\\n");
    }
    
    g_editor->initialized = true;
}

// Update editor logic
void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_editor || !g_editor->initialized) return;
    
    PlatformInput* input = &platform->input;
    
    // Update dimensions if window resized
    g_editor->width = platform->window.width;
    g_editor->height = platform->window.height;
    
    // Update time
    g_editor->time += dt;
    
    // Panel toggles
    if (input->keys[KEY_F1].pressed) g_editor->show_hierarchy = !g_editor->show_hierarchy;
    if (input->keys[KEY_F2].pressed) g_editor->show_inspector = !g_editor->show_inspector;
    if (input->keys[KEY_F3].pressed) g_editor->show_console = !g_editor->show_console;
    if (input->keys[KEY_F4].pressed) g_editor->show_stats = !g_editor->show_stats;
    
    // Camera controls
    f32 rotate_speed = 50.0f;
    if (input->keys[KEY_A].down) g_editor->camera_rotation.y -= rotate_speed * dt;
    if (input->keys[KEY_D].down) g_editor->camera_rotation.y += rotate_speed * dt;
    if (input->keys[KEY_W].down) g_editor->camera_rotation.x -= rotate_speed * dt;
    if (input->keys[KEY_S].down) g_editor->camera_rotation.x += rotate_speed * dt;
    
    // FPS calculation
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
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Draw 3D content
    Setup3DViewport(g_editor);
    DrawSpinningCube(g_editor->time);
    
    // Draw UI panels
    DrawPanels(g_editor);
}

// Shutdown
void GameShutdown(PlatformState* platform) {
    if (g_editor) {
        printf("\\n[Enhanced Editor] Shutting down renderer integration\\n");
    }
    g_editor = NULL;
}