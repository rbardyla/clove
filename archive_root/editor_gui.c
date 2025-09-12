/*
    Editor GUI Implementation
    =========================
    
    Simple immediate-mode GUI system integrated with our renderer.
    Focuses on essential editor functionality with good performance.
*/

#include "editor_gui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// CORE SYSTEM
// ============================================================================

EditorGUI* EditorGUI_Create(PlatformState* platform, Renderer* renderer) {
    EditorGUI* gui = PushStruct(&platform->permanent_arena, EditorGUI);
    
    gui->renderer = renderer;
    gui->temp_arena = &platform->frame_arena;
    
    // Set up default theme
    gui->theme = EditorGUI_DarkTheme();
    
    // Initialize panel configurations
    gui->panels[PANEL_HIERARCHY] = (PanelConfig){
        .visible = true, .x = 0, .y = 30, .width = 250, .height = 400,
        .min_width = 200, .min_height = 200, .resizable = true, .movable = false
    };
    strcpy(gui->panels[PANEL_HIERARCHY].title, "Scene Hierarchy");
    
    gui->panels[PANEL_INSPECTOR] = (PanelConfig){
        .visible = true, .x = 800, .y = 30, .width = 300, .height = 500,
        .min_width = 200, .min_height = 300, .resizable = true, .movable = false
    };
    strcpy(gui->panels[PANEL_INSPECTOR].title, "Inspector");
    
    gui->panels[PANEL_CONSOLE] = (PanelConfig){
        .visible = true, .x = 250, .y = 600, .width = 550, .height = 150,
        .min_width = 300, .min_height = 100, .resizable = true, .movable = false
    };
    strcpy(gui->panels[PANEL_CONSOLE].title, "Console");
    
    gui->panels[PANEL_PERFORMANCE] = (PanelConfig){
        .visible = true, .x = 50, .y = 50, .width = 300, .height = 200,
        .min_width = 250, .min_height = 150, .resizable = true, .movable = true
    };
    strcpy(gui->panels[PANEL_PERFORMANCE].title, "Performance");
    
    // Initialize other systems
    gui->console_auto_scroll = true;
    strcpy(gui->current_directory, "assets/");
    
    // Create a simple quad mesh for UI rendering
    // This would be implemented using the renderer API
    // gui->quad_mesh = Render->CreateMesh(...);
    
    EditorGUI_Log(gui, "Editor GUI initialized");
    
    return gui;
}

void EditorGUI_Destroy(EditorGUI* gui) {
    if (gui && gui->renderer) {
        // Clean up resources
        EditorGUI_Log(gui, "Editor GUI destroyed");
    }
}

void EditorGUI_BeginFrame(EditorGUI* gui, PlatformInput* input) {
    // Update input state
    gui->last_mouse_pos = gui->mouse_pos;
    gui->mouse_pos = (Vec2){(f32)input->mouse_x, (f32)input->mouse_y};
    
    gui->mouse_clicked[0] = input->mouse[MOUSE_LEFT].pressed;
    gui->mouse_clicked[1] = input->mouse[MOUSE_RIGHT].pressed;  
    gui->mouse_clicked[2] = input->mouse[MOUSE_MIDDLE].pressed;
    
    gui->mouse_down[0] = input->mouse[MOUSE_LEFT].down;
    gui->mouse_down[1] = input->mouse[MOUSE_RIGHT].down;
    gui->mouse_down[2] = input->mouse[MOUSE_MIDDLE].down;
    
    // Reset widget state
    gui->hot_widget_id = 0;
    if (!gui->mouse_down[0]) {
        gui->active_widget_id = 0;
    }
    
    // Reset temp arena
    gui->temp_arena->used = 0;
}

void EditorGUI_EndFrame(EditorGUI* gui) {
    // Render all visible panels
    if (gui->panels[PANEL_HIERARCHY].visible) {
        EditorGUI_DrawHierarchyPanel(gui);
    }
    
    if (gui->panels[PANEL_INSPECTOR].visible) {
        EditorGUI_DrawInspectorPanel(gui);
    }
    
    if (gui->panels[PANEL_CONSOLE].visible) {
        EditorGUI_DrawConsolePanel(gui);
    }
    
    if (gui->panels[PANEL_PERFORMANCE].visible) {
        EditorGUI_DrawPerformancePanel(gui);
    }
}

// ============================================================================
// PANEL IMPLEMENTATIONS
// ============================================================================

void EditorGUI_DrawHierarchyPanel(EditorGUI* gui) {
    PanelConfig* panel = &gui->panels[PANEL_HIERARCHY];
    
    // Draw panel background
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.panel_bg);
    EditorGUI_DrawRectOutline(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.border, 1.0f);
    
    // Draw header
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, 25, gui->theme.header_bg);
    EditorGUI_DrawText(gui, panel->title, panel->x + 5, panel->y + 5, gui->theme.text_normal);
    
    // Draw hierarchy items (simplified)
    f32 item_y = panel->y + 30;
    f32 item_height = 20;
    
    static bool cube_expanded = true;
    static bool light_expanded = false;
    
    if (EditorGUI_TreeNode(gui, "Scene Root", &cube_expanded, panel->x + 5, item_y)) {
        item_y += item_height;
        EditorGUI_Text(gui, "  - Spinning Cube", panel->x + 15, item_y, gui->theme.text_normal);
        
        item_y += item_height;
        if (EditorGUI_TreeNode(gui, "  - Lights", &light_expanded, panel->x + 15, item_y)) {
            item_y += item_height;
            EditorGUI_Text(gui, "    - Directional Light", panel->x + 25, item_y, gui->theme.text_normal);
        }
    }
}

void EditorGUI_DrawInspectorPanel(EditorGUI* gui) {
    PanelConfig* panel = &gui->panels[PANEL_INSPECTOR];
    
    // Draw panel background
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.panel_bg);
    EditorGUI_DrawRectOutline(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.border, 1.0f);
    
    // Draw header
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, 25, gui->theme.header_bg);
    EditorGUI_DrawText(gui, panel->title, panel->x + 5, panel->y + 5, gui->theme.text_normal);
    
    // Draw property fields (simplified)
    f32 prop_y = panel->y + 35;
    f32 prop_height = 25;
    f32 label_width = 80;
    
    EditorGUI_Text(gui, "Transform:", panel->x + 5, prop_y, gui->theme.text_highlight);
    prop_y += prop_height;
    
    static f32 pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    EditorGUI_SliderFloat(gui, "Pos X", &pos_x, -10.0f, 10.0f, panel->x + 5, prop_y, panel->width - 10);
    prop_y += prop_height;
    EditorGUI_SliderFloat(gui, "Pos Y", &pos_y, -10.0f, 10.0f, panel->x + 5, prop_y, panel->width - 10);
    prop_y += prop_height;
    EditorGUI_SliderFloat(gui, "Pos Z", &pos_z, -10.0f, 10.0f, panel->x + 5, prop_y, panel->width - 10);
    
    prop_y += prop_height + 10;
    EditorGUI_Text(gui, "Rendering:", panel->x + 5, prop_y, gui->theme.text_highlight);
    prop_y += prop_height;
    
    static bool wireframe = false;
    EditorGUI_Checkbox(gui, "Wireframe", &wireframe, panel->x + 5, prop_y);
}

void EditorGUI_DrawConsolePanel(EditorGUI* gui) {
    PanelConfig* panel = &gui->panels[PANEL_CONSOLE];
    
    // Draw panel background
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.panel_bg);
    EditorGUI_DrawRectOutline(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.border, 1.0f);
    
    // Draw header
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, 25, gui->theme.header_bg);
    EditorGUI_DrawText(gui, panel->title, panel->x + 5, panel->y + 5, gui->theme.text_normal);
    
    // Draw console log entries
    f32 log_y = panel->y + 30;
    f32 line_height = 15;
    i32 max_lines = (i32)((panel->height - 35) / line_height);
    
    i32 start_index = gui->console_log_count > max_lines ? gui->console_log_count - max_lines : 0;
    
    for (i32 i = start_index; i < gui->console_log_count && i < start_index + max_lines; i++) {
        LogEntry* entry = &gui->console_logs[i % 256];
        EditorGUI_DrawText(gui, entry->message, panel->x + 5, log_y, entry->color);
        log_y += line_height;
    }
    
    // Clear button
    if (EditorGUI_Button(gui, "Clear", panel->x + panel->width - 50, panel->y + 2, 45, 20)) {
        EditorGUI_ClearLog(gui);
    }
}

void EditorGUI_DrawPerformancePanel(EditorGUI* gui) {
    PanelConfig* panel = &gui->panels[PANEL_PERFORMANCE];
    
    // Draw panel background
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.panel_bg);
    EditorGUI_DrawRectOutline(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.border, 1.0f);
    
    // Draw header
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, 25, gui->theme.header_bg);
    EditorGUI_DrawText(gui, panel->title, panel->x + 5, panel->y + 5, gui->theme.text_normal);
    
    // Performance stats
    f32 text_y = panel->y + 35;
    f32 line_height = 18;
    
    char fps_text[64];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.0f", gui->current_fps);
    EditorGUI_DrawText(gui, fps_text, panel->x + 5, text_y, gui->theme.text_normal);
    text_y += line_height;
    
    char frame_text[64];
    snprintf(frame_text, sizeof(frame_text), "Frame: %.2fms", gui->avg_frame_time * 1000.0f);
    EditorGUI_DrawText(gui, frame_text, panel->x + 5, text_y, gui->theme.text_normal);
    text_y += line_height;
    
    // Draw frame time graph
    f32 graph_x = panel->x + 5;
    f32 graph_y = text_y + 10;
    f32 graph_w = panel->width - 10;
    f32 graph_h = 60;
    
    EditorGUI_DrawRect(gui, graph_x, graph_y, graph_w, graph_h, (Vec4){0.1f, 0.1f, 0.1f, 1.0f});
    EditorGUI_DrawRectOutline(gui, graph_x, graph_y, graph_w, graph_h, gui->theme.border, 1.0f);
    
    // Draw frame time bars
    f32 bar_width = graph_w / 120.0f;
    for (i32 i = 0; i < 120; i++) {
        f32 frame_time = gui->frame_times[i];
        f32 normalized_height = (frame_time / 0.033f) * graph_h; // Normalize to 33ms max
        f32 bar_x = graph_x + i * bar_width;
        f32 bar_y = graph_y + graph_h - normalized_height;
        
        Vec4 bar_color = {0.2f, 0.8f, 0.2f, 0.8f}; // Green
        if (frame_time > 0.016f) bar_color = (Vec4){0.8f, 0.8f, 0.2f, 0.8f}; // Yellow
        if (frame_time > 0.033f) bar_color = (Vec4){0.8f, 0.2f, 0.2f, 0.8f}; // Red
        
        EditorGUI_DrawRect(gui, bar_x, bar_y, bar_width - 1, normalized_height, bar_color);
    }
}

void EditorGUI_DrawMaterialEditorPanel(EditorGUI* gui) {
    // TODO: Implement material editor
}

void EditorGUI_DrawAssetBrowserPanel(EditorGUI* gui) {
    // TODO: Implement asset browser
}

// ============================================================================
// WIDGET IMPLEMENTATIONS  
// ============================================================================

bool EditorGUI_Button(EditorGUI* gui, const char* label, f32 x, f32 y, f32 w, f32 h) {
    static i32 widget_id = 1000; // Simple ID generation
    widget_id++;
    
    // Check if mouse is over button
    bool mouse_over = (gui->mouse_pos.x >= x && gui->mouse_pos.x <= x + w &&
                       gui->mouse_pos.y >= y && gui->mouse_pos.y <= y + h);
    
    Vec4 button_color = gui->theme.button_bg;
    if (mouse_over) {
        gui->hot_widget_id = widget_id;
        button_color = gui->theme.button_hot;
        
        if (gui->mouse_clicked[0]) {
            gui->active_widget_id = widget_id;
        }
    }
    
    if (gui->active_widget_id == widget_id) {
        button_color = gui->theme.button_active;
    }
    
    // Draw button
    EditorGUI_DrawRect(gui, x, y, w, h, button_color);
    EditorGUI_DrawRectOutline(gui, x, y, w, h, gui->theme.border, 1.0f);
    EditorGUI_DrawText(gui, label, x + 5, y + 3, gui->theme.text_normal);
    
    // Return true if clicked
    return (gui->active_widget_id == widget_id && gui->mouse_clicked[0] && mouse_over);
}

bool EditorGUI_Checkbox(EditorGUI* gui, const char* label, bool* value, f32 x, f32 y) {
    static i32 widget_id = 2000;
    widget_id++;
    
    f32 box_size = 15;
    bool mouse_over = (gui->mouse_pos.x >= x && gui->mouse_pos.x <= x + box_size &&
                       gui->mouse_pos.y >= y && gui->mouse_pos.y <= y + box_size);
    
    bool clicked = false;
    if (mouse_over) {
        gui->hot_widget_id = widget_id;
        if (gui->mouse_clicked[0]) {
            *value = !*value;
            clicked = true;
        }
    }
    
    // Draw checkbox
    Vec4 box_color = mouse_over ? gui->theme.button_hot : gui->theme.button_bg;
    EditorGUI_DrawRect(gui, x, y, box_size, box_size, box_color);
    EditorGUI_DrawRectOutline(gui, x, y, box_size, box_size, gui->theme.border, 1.0f);
    
    if (*value) {
        // Draw checkmark (simple X)
        EditorGUI_DrawLine(gui, x + 3, y + 3, x + box_size - 3, y + box_size - 3, gui->theme.text_normal, 2.0f);
        EditorGUI_DrawLine(gui, x + box_size - 3, y + 3, x + 3, y + box_size - 3, gui->theme.text_normal, 2.0f);
    }
    
    // Draw label
    EditorGUI_DrawText(gui, label, x + box_size + 5, y, gui->theme.text_normal);
    
    return clicked;
}

bool EditorGUI_SliderFloat(EditorGUI* gui, const char* label, f32* value, f32 min, f32 max, f32 x, f32 y, f32 w) {
    static i32 widget_id = 3000;
    widget_id++;
    
    f32 slider_height = 20;
    f32 label_width = 60;
    f32 slider_x = x + label_width;
    f32 slider_w = w - label_width - 60; // Leave space for value text
    
    bool mouse_over = (gui->mouse_pos.x >= slider_x && gui->mouse_pos.x <= slider_x + slider_w &&
                       gui->mouse_pos.y >= y && gui->mouse_pos.y <= y + slider_height);
    
    bool changed = false;
    if (mouse_over) {
        gui->hot_widget_id = widget_id;
        if (gui->mouse_clicked[0]) {
            gui->active_widget_id = widget_id;
        }
    }
    
    if (gui->active_widget_id == widget_id && gui->mouse_down[0]) {
        f32 mouse_rel = (gui->mouse_pos.x - slider_x) / slider_w;
        mouse_rel = mouse_rel < 0.0f ? 0.0f : (mouse_rel > 1.0f ? 1.0f : mouse_rel);
        *value = min + mouse_rel * (max - min);
        changed = true;
    }
    
    // Draw slider
    EditorGUI_DrawText(gui, label, x, y + 2, gui->theme.text_normal);
    EditorGUI_DrawRect(gui, slider_x, y, slider_w, slider_height, gui->theme.button_bg);
    EditorGUI_DrawRectOutline(gui, slider_x, y, slider_w, slider_height, gui->theme.border, 1.0f);
    
    // Draw slider handle
    f32 handle_pos = (*value - min) / (max - min);
    f32 handle_x = slider_x + handle_pos * slider_w - 3;
    EditorGUI_DrawRect(gui, handle_x, y - 2, 6, slider_height + 4, gui->theme.selection);
    
    // Draw value text
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%.2f", *value);
    EditorGUI_DrawText(gui, value_text, slider_x + slider_w + 5, y + 2, gui->theme.text_normal);
    
    return changed;
}

bool EditorGUI_TreeNode(EditorGUI* gui, const char* label, bool* open, f32 x, f32 y) {
    static i32 widget_id = 4000;
    widget_id++;
    
    f32 node_height = 18;
    f32 arrow_size = 8;
    
    bool mouse_over = (gui->mouse_pos.x >= x && gui->mouse_pos.x <= x + 200 &&
                       gui->mouse_pos.y >= y && gui->mouse_pos.y <= y + node_height);
    
    if (mouse_over) {
        gui->hot_widget_id = widget_id;
        if (gui->mouse_clicked[0]) {
            *open = !*open;
        }
    }
    
    // Draw background if hovered
    if (mouse_over) {
        EditorGUI_DrawRect(gui, x, y, 200, node_height, (Vec4){0.3f, 0.3f, 0.4f, 0.3f});
    }
    
    // Draw arrow
    if (*open) {
        // Down arrow (opened)
        EditorGUI_DrawLine(gui, x, y + 4, x + arrow_size, y + 4, gui->theme.text_normal, 2.0f);
        EditorGUI_DrawLine(gui, x + 2, y + 6, x + arrow_size - 2, y + 6, gui->theme.text_normal, 2.0f);
        EditorGUI_DrawLine(gui, x + 4, y + 8, x + arrow_size - 4, y + 8, gui->theme.text_normal, 2.0f);
    } else {
        // Right arrow (closed) 
        EditorGUI_DrawLine(gui, x + 2, y + 2, x + 2, y + 10, gui->theme.text_normal, 2.0f);
        EditorGUI_DrawLine(gui, x + 4, y + 4, x + 4, y + 8, gui->theme.text_normal, 2.0f);
        EditorGUI_DrawLine(gui, x + 6, y + 6, x + 6, y + 6, gui->theme.text_normal, 2.0f);
    }
    
    // Draw label
    EditorGUI_DrawText(gui, label, x + arrow_size + 5, y + 1, gui->theme.text_normal);
    
    return *open;
}

// ============================================================================
// RENDERING PRIMITIVES (Using immediate mode OpenGL for now)
// ============================================================================

void EditorGUI_DrawRect(EditorGUI* gui, f32 x, f32 y, f32 w, f32 h, Vec4 color) {
    // TODO: Use proper renderer API
    glBegin(GL_QUADS);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void EditorGUI_DrawRectOutline(EditorGUI* gui, f32 x, f32 y, f32 w, f32 h, Vec4 color, f32 thickness) {
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void EditorGUI_DrawLine(EditorGUI* gui, f32 x1, f32 y1, f32 x2, f32 y2, Vec4 color, f32 thickness) {
    glLineWidth(thickness);
    glBegin(GL_LINES);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void EditorGUI_DrawText(EditorGUI* gui, const char* text, f32 x, f32 y, Vec4 color) {
    // TODO: Implement proper text rendering
    // For now, this is a placeholder - text would be rendered using bitmap fonts
    // or by integrating with a text rendering system
}

// Additional required functions
void EditorGUI_Text(EditorGUI* gui, const char* text, f32 x, f32 y, Vec4 color) {
    EditorGUI_DrawText(gui, text, x, y, color);
}

void EditorGUI_ShowPanel(EditorGUI* gui, PanelType type, bool show) {
    if (type >= 0 && type < PANEL_COUNT) {
        gui->panels[type].visible = show;
    }
}

bool EditorGUI_IsPanelVisible(EditorGUI* gui, PanelType type) {
    if (type >= 0 && type < PANEL_COUNT) {
        return gui->panels[type].visible;
    }
    return false;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void EditorGUI_Log(EditorGUI* gui, const char* message) {
    if (gui->console_log_count >= 256) {
        // Circular buffer
        gui->console_log_head = (gui->console_log_head + 1) % 256;
    } else {
        gui->console_log_count++;
    }
    
    LogEntry* entry = &gui->console_logs[(gui->console_log_head + gui->console_log_count - 1) % 256];
    strncpy(entry->message, message, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = 0;
    entry->color = (Vec4){0.9f, 0.9f, 0.9f, 1.0f};
    entry->level = 0;
    entry->timestamp = time(NULL);
}

void EditorGUI_LogWarning(EditorGUI* gui, const char* message) {
    EditorGUI_Log(gui, message);
    LogEntry* entry = &gui->console_logs[(gui->console_log_head + gui->console_log_count - 1) % 256];
    entry->color = (Vec4){1.0f, 0.8f, 0.2f, 1.0f};
    entry->level = 1;
}

void EditorGUI_LogError(EditorGUI* gui, const char* message) {
    EditorGUI_Log(gui, message);
    LogEntry* entry = &gui->console_logs[(gui->console_log_head + gui->console_log_count - 1) % 256];
    entry->color = (Vec4){1.0f, 0.3f, 0.3f, 1.0f};
    entry->level = 2;
}

void EditorGUI_ClearLog(EditorGUI* gui) {
    gui->console_log_count = 0;
    gui->console_log_head = 0;
}

void EditorGUI_UpdatePerformanceStats(EditorGUI* gui, f32 frame_time, f32 fps) {
    gui->current_fps = fps;
    gui->avg_frame_time = frame_time;
    
    gui->frame_times[gui->frame_time_index] = frame_time;
    gui->frame_time_index = (gui->frame_time_index + 1) % 120;
}

EditorTheme EditorGUI_DefaultTheme(void) {
    return (EditorTheme){
        .background = {0.9f, 0.9f, 0.9f, 1.0f},
        .panel_bg = {0.8f, 0.8f, 0.8f, 1.0f},
        .header_bg = {0.7f, 0.7f, 0.7f, 1.0f},
        .button_bg = {0.6f, 0.6f, 0.6f, 1.0f},
        .button_hot = {0.7f, 0.7f, 0.8f, 1.0f},
        .button_active = {0.5f, 0.5f, 0.7f, 1.0f},
        .text_normal = {0.1f, 0.1f, 0.1f, 1.0f},
        .text_highlight = {0.0f, 0.0f, 0.5f, 1.0f},
        .border = {0.4f, 0.4f, 0.4f, 1.0f},
        .selection = {0.3f, 0.5f, 0.8f, 1.0f}
    };
}

EditorTheme EditorGUI_DarkTheme(void) {
    return (EditorTheme){
        .background = {0.1f, 0.1f, 0.1f, 1.0f},
        .panel_bg = {0.15f, 0.15f, 0.15f, 1.0f},
        .header_bg = {0.1f, 0.1f, 0.1f, 1.0f},
        .button_bg = {0.25f, 0.25f, 0.25f, 1.0f},
        .button_hot = {0.35f, 0.35f, 0.35f, 1.0f},
        .button_active = {0.45f, 0.35f, 0.2f, 1.0f},
        .text_normal = {0.9f, 0.9f, 0.9f, 1.0f},
        .text_highlight = {0.7f, 0.9f, 1.0f, 1.0f},
        .border = {0.3f, 0.3f, 0.3f, 1.0f},
        .selection = {0.8f, 0.5f, 0.2f, 1.0f}
    };
}