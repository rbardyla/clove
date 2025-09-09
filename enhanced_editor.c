/*
    Enhanced Handmade Game Editor
    A functional editor with real panels and features
*/

#include "systems/renderer/handmade_renderer.h"
#include "systems/renderer/handmade_platform.h" 
#include "systems/renderer/handmade_opengl.h"
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// DATA STRUCTURES
// ============================================================================

#define MAX_GAME_OBJECTS 256
#define MAX_NAME_LENGTH 64

// Simple GameObject system
typedef struct {
    char name[MAX_NAME_LENGTH];
    v3 position;
    v3 rotation;
    v3 scale;
    v4 color;
    bool active;
    int parent_id;  // -1 if no parent
    int mesh_type;  // 0=cube, 1=sphere, 2=plane
} game_object;

// Scene hierarchy state
typedef struct {
    game_object objects[MAX_GAME_OBJECTS];
    int object_count;
    int selected_object;
    bool expanded[MAX_GAME_OBJECTS];
} scene_state;

// Property inspector state
typedef struct {
    bool show_transform;
    bool show_rendering;
    bool show_physics;
    char edit_buffer[256];
    int editing_field;  // Which field is being edited
} inspector_state;

// Viewport state
typedef struct {
    v3 camera_position;
    v3 camera_rotation;
    f32 camera_zoom;
    bool is_orbiting;
    bool is_panning;
    int last_mouse_x;
    int last_mouse_y;
    int viewport_mode;  // 0=perspective, 1=top, 2=front, 3=side
} viewport_state;

// Main editor state
typedef struct {
    // Panels
    bool show_scene_hierarchy;
    bool show_inspector;
    bool show_console;
    bool show_toolbar;
    
    // Panel positions and sizes
    int hierarchy_width;
    int inspector_width;
    int console_height;
    
    // Core states
    scene_state scene;
    inspector_state inspector;
    viewport_state viewport;
    
    // Input
    bool keys[256];
    int mouse_x, mouse_y;
    bool mouse_left, mouse_right, mouse_middle;
    bool mouse_left_pressed;
    
    // Editor mode
    int tool_mode;  // 0=select, 1=move, 2=rotate, 3=scale
    bool playing;
    f32 play_time;
    
    // Console
    char console_lines[10][256];
    int console_line_count;
} editor_state;

// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

void scene_init(scene_state* scene) {
    scene->object_count = 0;
    scene->selected_object = -1;
    
    // Create some default objects
    game_object* obj;
    
    // Main Camera
    obj = &scene->objects[scene->object_count++];
    strcpy(obj->name, "Main Camera");
    obj->position = (v3){0, 5, 10};
    obj->rotation = (v3){-20, 0, 0};
    obj->scale = (v3){1, 1, 1};
    obj->color = (v4){0.8f, 0.8f, 0.8f, 1.0f};
    obj->active = true;
    obj->parent_id = -1;
    obj->mesh_type = -1;  // No mesh for camera
    
    // Directional Light
    obj = &scene->objects[scene->object_count++];
    strcpy(obj->name, "Directional Light");
    obj->position = (v3){5, 10, 5};
    obj->rotation = (v3){-45, -30, 0};
    obj->scale = (v3){1, 1, 1};
    obj->color = (v4){1.0f, 0.9f, 0.7f, 1.0f};
    obj->active = true;
    obj->parent_id = -1;
    obj->mesh_type = -1;  // No mesh for light
    
    // Ground Plane
    obj = &scene->objects[scene->object_count++];
    strcpy(obj->name, "Ground");
    obj->position = (v3){0, 0, 0};
    obj->rotation = (v3){0, 0, 0};
    obj->scale = (v3){20, 0.1f, 20};
    obj->color = (v4){0.3f, 0.5f, 0.3f, 1.0f};
    obj->active = true;
    obj->parent_id = -1;
    obj->mesh_type = 0;  // Cube
    
    // Player Cube
    obj = &scene->objects[scene->object_count++];
    strcpy(obj->name, "Player");
    obj->position = (v3){0, 1, 0};
    obj->rotation = (v3){0, 0, 0};
    obj->scale = (v3){1, 1, 1};
    obj->color = (v4){0.2f, 0.4f, 0.8f, 1.0f};
    obj->active = true;
    obj->parent_id = -1;
    obj->mesh_type = 0;  // Cube
    
    // Enemy Cubes
    for (int i = 0; i < 3; i++) {
        obj = &scene->objects[scene->object_count++];
        sprintf(obj->name, "Enemy %d", i + 1);
        obj->position = (v3){(i - 1) * 4.0f, 1, -5};
        obj->rotation = (v3){0, 0, 0};
        obj->scale = (v3){1, 1, 1};
        obj->color = (v4){0.8f, 0.2f, 0.2f, 1.0f};
        obj->active = true;
        obj->parent_id = -1;
        obj->mesh_type = 0;  // Cube
    }
}

game_object* scene_create_object(scene_state* scene, const char* name) {
    if (scene->object_count >= MAX_GAME_OBJECTS) return NULL;
    
    game_object* obj = &scene->objects[scene->object_count++];
    strcpy(obj->name, name);
    obj->position = (v3){0, 0, 0};
    obj->rotation = (v3){0, 0, 0};
    obj->scale = (v3){1, 1, 1};
    obj->color = (v4){1, 1, 1, 1};
    obj->active = true;
    obj->parent_id = -1;
    obj->mesh_type = 0;
    
    return obj;
}

void scene_delete_object(scene_state* scene, int index) {
    if (index < 0 || index >= scene->object_count) return;
    
    // Shift remaining objects
    for (int i = index; i < scene->object_count - 1; i++) {
        scene->objects[i] = scene->objects[i + 1];
    }
    scene->object_count--;
    
    // Update selection
    if (scene->selected_object == index) {
        scene->selected_object = -1;
    } else if (scene->selected_object > index) {
        scene->selected_object--;
    }
}

// ============================================================================
// GUI RENDERING
// ============================================================================

void draw_rect(renderer_state* renderer, f32 x, f32 y, f32 w, f32 h, v4 color) {
    // Convert screen coords to 3D
    f32 scale = 0.01f;
    f32 offset_x = -6.4f;
    f32 offset_y = 3.6f;
    
    v3 min = {offset_x + x * scale, offset_y - y * scale - h * scale, 0};
    v3 max = {offset_x + x * scale + w * scale, offset_y - y * scale, 0};
    
    // Draw filled rect using cube (flat)
    v3 center = {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, 0};
    v3 size = {max.x - min.x, max.y - min.y, 0.01f};
    
    m4x4 transform = m4x4_identity();
    transform = m4x4_translate(transform, center);
    transform = m4x4_scale(transform, size);
    
    renderer_set_model_matrix(renderer, &transform);
    renderer_set_color(renderer, color);
    renderer_draw_cube(renderer);
}

void draw_text(renderer_state* renderer, const char* text, f32 x, f32 y, v4 color) {
    // Simple text rendering using rectangles for now
    draw_rect(renderer, x - 2, y - 2, strlen(text) * 8 + 4, 14, (v4){0, 0, 0, 0.8f});
    
    // For real text, we'd render character by character
    // For now, just show a colored bar to indicate text
    draw_rect(renderer, x, y, strlen(text) * 6, 10, color);
}

bool draw_button(renderer_state* renderer, const char* label, f32 x, f32 y, f32 w, f32 h, editor_state* editor) {
    bool hovered = editor->mouse_x >= x && editor->mouse_x <= x + w &&
                   editor->mouse_y >= y && editor->mouse_y <= y + h;
    bool clicked = hovered && editor->mouse_left_pressed;
    
    v4 color = clicked ? (v4){0.4f, 0.5f, 0.9f, 1.0f} :
               hovered ? (v4){0.3f, 0.4f, 0.8f, 1.0f} :
                        (v4){0.2f, 0.3f, 0.6f, 1.0f};
    
    draw_rect(renderer, x, y, w, h, color);
    draw_text(renderer, label, x + 10, y + h/2 - 5, (v4){1, 1, 1, 1});
    
    return clicked;
}

// ============================================================================
// PANEL RENDERING
// ============================================================================

void render_scene_hierarchy(renderer_state* renderer, editor_state* editor) {
    f32 panel_x = 0;
    f32 panel_y = 30;
    f32 panel_w = editor->hierarchy_width;
    f32 panel_h = 720 - 30 - editor->console_height;
    
    // Panel background
    draw_rect(renderer, panel_x, panel_y, panel_w, panel_h, (v4){0.15f, 0.15f, 0.15f, 1.0f});
    
    // Title
    draw_text(renderer, "SCENE HIERARCHY", panel_x + 10, panel_y + 20, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    
    // Draw objects
    f32 y_offset = panel_y + 50;
    for (int i = 0; i < editor->scene.object_count; i++) {
        game_object* obj = &editor->scene.objects[i];
        
        bool selected = (i == editor->scene.selected_object);
        v4 text_color = selected ? (v4){1.0f, 0.8f, 0.2f, 1.0f} :
                       obj->active ? (v4){0.9f, 0.9f, 0.9f, 1.0f} :
                                    (v4){0.5f, 0.5f, 0.5f, 1.0f};
        
        // Selection highlight
        if (selected) {
            draw_rect(renderer, panel_x + 5, y_offset - 2, panel_w - 10, 20, (v4){0.3f, 0.3f, 0.5f, 1.0f});
        }
        
        // Object name with icon
        char display_name[128];
        const char* icon = obj->mesh_type < 0 ? "[>]" : "[#]";
        sprintf(display_name, "%s %s", icon, obj->name);
        
        // Clickable area
        if (editor->mouse_x >= panel_x + 10 && editor->mouse_x <= panel_x + panel_w - 10 &&
            editor->mouse_y >= y_offset && editor->mouse_y <= y_offset + 20 &&
            editor->mouse_left_pressed) {
            editor->scene.selected_object = i;
        }
        
        draw_text(renderer, display_name, panel_x + 10, y_offset, text_color);
        y_offset += 25;
    }
    
    // Add object button
    if (draw_button(renderer, "+ Add GameObject", panel_x + 10, panel_h - 40, panel_w - 20, 30, editor)) {
        char name[64];
        sprintf(name, "GameObject %d", editor->scene.object_count);
        scene_create_object(&editor->scene, name);
    }
}

void render_property_inspector(renderer_state* renderer, editor_state* editor) {
    f32 panel_x = 1280 - editor->inspector_width;
    f32 panel_y = 30;
    f32 panel_w = editor->inspector_width;
    f32 panel_h = 720 - 30 - editor->console_height;
    
    // Panel background
    draw_rect(renderer, panel_x, panel_y, panel_w, panel_h, (v4){0.15f, 0.15f, 0.15f, 1.0f});
    
    // Title
    draw_text(renderer, "PROPERTIES", panel_x + 10, panel_y + 20, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    
    if (editor->scene.selected_object < 0) {
        draw_text(renderer, "No Selection", panel_x + 10, panel_y + 60, (v4){0.5f, 0.5f, 0.5f, 1.0f});
        return;
    }
    
    game_object* obj = &editor->scene.objects[editor->scene.selected_object];
    f32 y = panel_y + 60;
    
    // Object name
    draw_text(renderer, "Name:", panel_x + 10, y, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    draw_rect(renderer, panel_x + 60, y - 2, panel_w - 70, 20, (v4){0.2f, 0.2f, 0.2f, 1.0f});
    draw_text(renderer, obj->name, panel_x + 65, y, (v4){1, 1, 1, 1});
    y += 30;
    
    // Active checkbox
    v4 check_color = obj->active ? (v4){0.2f, 0.8f, 0.2f, 1.0f} : (v4){0.3f, 0.3f, 0.3f, 1.0f};
    draw_rect(renderer, panel_x + 10, y, 15, 15, check_color);
    draw_text(renderer, "Active", panel_x + 30, y, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    if (editor->mouse_x >= panel_x + 10 && editor->mouse_x <= panel_x + 25 &&
        editor->mouse_y >= y && editor->mouse_y <= y + 15 &&
        editor->mouse_left_pressed) {
        obj->active = !obj->active;
    }
    y += 30;
    
    // Transform section
    draw_text(renderer, "TRANSFORM", panel_x + 10, y, (v4){0.8f, 0.8f, 1.0f, 1.0f});
    y += 25;
    
    // Position
    char pos_text[64];
    sprintf(pos_text, "X:%.1f Y:%.1f Z:%.1f", obj->position.x, obj->position.y, obj->position.z);
    draw_text(renderer, "Position", panel_x + 10, y, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    draw_text(renderer, pos_text, panel_x + 10, y + 20, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    y += 50;
    
    // Rotation
    sprintf(pos_text, "X:%.1f Y:%.1f Z:%.1f", obj->rotation.x, obj->rotation.y, obj->rotation.z);
    draw_text(renderer, "Rotation", panel_x + 10, y, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    draw_text(renderer, pos_text, panel_x + 10, y + 20, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    y += 50;
    
    // Scale
    sprintf(pos_text, "X:%.1f Y:%.1f Z:%.1f", obj->scale.x, obj->scale.y, obj->scale.z);
    draw_text(renderer, "Scale", panel_x + 10, y, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    draw_text(renderer, pos_text, panel_x + 10, y + 20, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    y += 50;
    
    // Rendering section
    draw_text(renderer, "RENDERING", panel_x + 10, y, (v4){1.0f, 0.8f, 0.8f, 1.0f});
    y += 25;
    
    // Color
    draw_text(renderer, "Color", panel_x + 10, y, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    draw_rect(renderer, panel_x + 60, y, 30, 20, obj->color);
    y += 30;
    
    // Mesh type
    const char* mesh_names[] = {"Cube", "Sphere", "Plane", "None"};
    int mesh_idx = obj->mesh_type < 0 ? 3 : obj->mesh_type;
    draw_text(renderer, "Mesh", panel_x + 10, y, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    draw_text(renderer, mesh_names[mesh_idx], panel_x + 60, y, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    y += 30;
    
    // Delete button
    if (draw_button(renderer, "Delete Object", panel_x + 10, panel_h - 40, panel_w - 20, 30, editor)) {
        scene_delete_object(&editor->scene, editor->scene.selected_object);
    }
}

void render_toolbar(renderer_state* renderer, editor_state* editor) {
    // Top toolbar
    draw_rect(renderer, 0, 0, 1280, 30, (v4){0.2f, 0.2f, 0.2f, 1.0f});
    
    // File menu
    if (draw_button(renderer, "File", 10, 5, 50, 20, editor)) {
        // Would show dropdown
    }
    
    // Edit menu  
    if (draw_button(renderer, "Edit", 70, 5, 50, 20, editor)) {
        // Would show dropdown
    }
    
    // GameObject menu
    if (draw_button(renderer, "GameObject", 130, 5, 100, 20, editor)) {
        scene_create_object(&editor->scene, "New Object");
    }
    
    // Play controls (center)
    f32 center_x = 640 - 75;
    if (editor->playing) {
        if (draw_button(renderer, "||", center_x, 5, 30, 20, editor)) {
            editor->playing = false;
        }
    } else {
        if (draw_button(renderer, ">", center_x, 5, 30, 20, editor)) {
            editor->playing = true;
        }
    }
    
    if (draw_button(renderer, "[]", center_x + 40, 5, 30, 20, editor)) {
        editor->playing = false;
        editor->play_time = 0;
    }
    
    // Tool mode buttons (right side)
    f32 tool_x = 1000;
    const char* tool_names[] = {"Select", "Move", "Rotate", "Scale"};
    for (int i = 0; i < 4; i++) {
        v4 color = (editor->tool_mode == i) ? 
                   (v4){0.4f, 0.5f, 0.9f, 1.0f} : 
                   (v4){0.2f, 0.3f, 0.6f, 1.0f};
        f32 x = tool_x + i * 60;
        if (editor->mouse_x >= x && editor->mouse_x <= x + 55 &&
            editor->mouse_y >= 5 && editor->mouse_y <= 25 &&
            editor->mouse_left_pressed) {
            editor->tool_mode = i;
        }
        draw_rect(renderer, x, 5, 55, 20, color);
        draw_text(renderer, tool_names[i], x + 5, 10, (v4){1, 1, 1, 1});
    }
}

void render_console(renderer_state* renderer, editor_state* editor) {
    f32 panel_y = 720 - editor->console_height;
    f32 panel_w = 1280;
    f32 panel_h = editor->console_height;
    
    // Panel background
    draw_rect(renderer, 0, panel_y, panel_w, panel_h, (v4){0.1f, 0.1f, 0.1f, 1.0f});
    
    // Title bar
    draw_rect(renderer, 0, panel_y, panel_w, 25, (v4){0.15f, 0.15f, 0.15f, 1.0f});
    draw_text(renderer, "CONSOLE", 10, panel_y + 7, (v4){0.9f, 0.9f, 0.9f, 1.0f});
    
    // Clear button
    if (draw_button(renderer, "Clear", panel_w - 60, panel_y + 3, 50, 19, editor)) {
        editor->console_line_count = 0;
    }
    
    // Console lines
    f32 y = panel_y + 30;
    for (int i = 0; i < editor->console_line_count && i < 10; i++) {
        draw_text(renderer, editor->console_lines[i], 10, y, (v4){0.8f, 0.8f, 0.8f, 1.0f});
        y += 15;
    }
}

void render_viewport(renderer_state* renderer, editor_state* editor) {
    // Viewport area (between panels)
    f32 vp_x = editor->hierarchy_width;
    f32 vp_y = 30;
    f32 vp_w = 1280 - editor->hierarchy_width - editor->inspector_width;
    f32 vp_h = 720 - 30 - editor->console_height;
    
    // Viewport background
    draw_rect(renderer, vp_x, vp_y, vp_w, vp_h, (v4){0.05f, 0.05f, 0.1f, 1.0f});
    
    // Draw 3D scene objects
    for (int i = 0; i < editor->scene.object_count; i++) {
        game_object* obj = &editor->scene.objects[i];
        if (!obj->active || obj->mesh_type < 0) continue;
        
        // Apply object transform
        m4x4 transform = m4x4_identity();
        transform = m4x4_translate(transform, obj->position);
        // Simple rotation (would need proper euler to matrix conversion)
        transform = m4x4_rotate_y(transform, obj->rotation.y * 3.14159f / 180.0f);
        transform = m4x4_rotate_x(transform, obj->rotation.x * 3.14159f / 180.0f);
        transform = m4x4_rotate_z(transform, obj->rotation.z * 3.14159f / 180.0f);
        transform = m4x4_scale(transform, obj->scale);
        
        renderer_set_model_matrix(renderer, &transform);
        
        // Selection highlight
        v4 color = obj->color;
        if (i == editor->scene.selected_object) {
            // Add selection outline effect
            color.x = fminf(1.0f, color.x + 0.3f);
            color.y = fminf(1.0f, color.y + 0.3f);
            color.z = fminf(1.0f, color.z + 0.3f);
        }
        renderer_set_color(renderer, color);
        
        // Draw mesh
        if (obj->mesh_type == 0) {
            renderer_draw_cube(renderer);
        }
        // Would add sphere, plane, etc.
    }
    
    // Viewport overlay info
    const char* mode_names[] = {"Perspective", "Top", "Front", "Side"};
    draw_text(renderer, mode_names[editor->viewport.viewport_mode], vp_x + 10, vp_y + 20, (v4){0.7f, 0.7f, 0.7f, 1.0f});
    
    if (editor->playing) {
        char play_info[64];
        sprintf(play_info, "PLAYING - Time: %.1f", editor->play_time);
        draw_text(renderer, play_info, vp_x + vp_w/2 - 50, vp_y + 20, (v4){0.2f, 1.0f, 0.2f, 1.0f});
    }
}

// ============================================================================
// MAIN
// ============================================================================

void console_log(editor_state* editor, const char* message) {
    if (editor->console_line_count < 10) {
        strcpy(editor->console_lines[editor->console_line_count++], message);
    } else {
        // Shift lines up
        for (int i = 0; i < 9; i++) {
            strcpy(editor->console_lines[i], editor->console_lines[i + 1]);
        }
        strcpy(editor->console_lines[9], message);
    }
}

int main(int argc, char** argv) {
    printf("==================================\n");
    printf("ENHANCED HANDMADE GAME EDITOR\n");
    printf("==================================\n");
    
    // Initialize platform
    platform_state platform;
    if (!platform_init(&platform, 1280, 720)) {
        printf("Failed to initialize platform!\n");
        return 1;
    }
    
    // Initialize renderer
    renderer_state* renderer = renderer_init(&platform);
    if (!renderer) {
        printf("Failed to initialize renderer!\n");
        platform_shutdown(&platform);
        return 1;
    }
    
    // Initialize editor
    editor_state editor = {0};
    editor.show_scene_hierarchy = true;
    editor.show_inspector = true;
    editor.show_console = true;
    editor.show_toolbar = true;
    editor.hierarchy_width = 250;
    editor.inspector_width = 300;
    editor.console_height = 150;
    
    // Initialize viewport
    editor.viewport.camera_position = (v3){5, 10, 15};
    editor.viewport.camera_rotation = (v3){-30, -30, 0};
    editor.viewport.camera_zoom = 1.0f;
    
    // Initialize scene
    scene_init(&editor.scene);
    console_log(&editor, "[Editor] Initialized successfully");
    console_log(&editor, "[Scene] Created default scene with 6 objects");
    
    // Main loop
    bool running = true;
    while (running) {
        // Handle input
        platform_poll_events(&platform);
        
        // Update mouse state
        editor.mouse_left_pressed = false;
        if (platform_mouse_left(&platform) && !editor.mouse_left) {
            editor.mouse_left_pressed = true;
        }
        editor.mouse_left = platform_mouse_left(&platform);
        editor.mouse_right = platform_mouse_right(&platform);
        editor.mouse_x = platform_mouse_x(&platform);
        editor.mouse_y = platform_mouse_y(&platform);
        
        // Handle keys
        if (platform_key_pressed(&platform, KEY_ESCAPE)) {
            running = false;
        }
        
        // Number keys for quick tool selection
        if (platform_key_pressed(&platform, KEY_1)) editor.tool_mode = 0;
        if (platform_key_pressed(&platform, KEY_2)) editor.tool_mode = 1;
        if (platform_key_pressed(&platform, KEY_3)) editor.tool_mode = 2;
        if (platform_key_pressed(&platform, KEY_4)) editor.tool_mode = 3;
        
        // Delete key
        if (platform_key_pressed(&platform, KEY_DELETE)) {
            if (editor.scene.selected_object >= 0) {
                char msg[128];
                sprintf(msg, "[Scene] Deleted object: %s", 
                        editor.scene.objects[editor.scene.selected_object].name);
                console_log(&editor, msg);
                scene_delete_object(&editor.scene, editor.scene.selected_object);
            }
        }
        
        // Update play mode
        if (editor.playing) {
            editor.play_time += 0.016f;  // ~60fps
            
            // Animate objects in play mode
            for (int i = 0; i < editor.scene.object_count; i++) {
                if (strcmp(editor.scene.objects[i].name, "Player") == 0) {
                    editor.scene.objects[i].rotation.y += 1.0f;
                }
            }
        }
        
        // Clear and render
        renderer_clear(renderer, (v4){0.1f, 0.1f, 0.15f, 1.0f}, true, true);
        
        // Set up camera
        v3 cam_pos = editor.viewport.camera_position;
        v3 cam_target = {0, 0, 0};
        v3 cam_up = {0, 1, 0};
        v3 cam_forward = v3_normalize(v3_sub(cam_target, cam_pos));
        renderer_set_camera(renderer, cam_pos, cam_forward, cam_up);
        
        renderer_begin_frame(renderer);
        renderer_use_shader(renderer, renderer->basic_shader);
        
        // Render all panels
        if (editor.show_toolbar) render_toolbar(renderer, &editor);
        if (editor.show_scene_hierarchy) render_scene_hierarchy(renderer, &editor);
        if (editor.show_inspector) render_property_inspector(renderer, &editor);
        if (editor.show_console) render_console(renderer, &editor);
        render_viewport(renderer, &editor);
        
        renderer_end_frame(renderer);
        renderer_present(renderer);
        
        // Cap at 60 FPS
        platform_sleep(16);
    }
    
    console_log(&editor, "[Editor] Shutting down...");
    
    renderer_shutdown(renderer);
    platform_shutdown(&platform);
    
    return 0;
}