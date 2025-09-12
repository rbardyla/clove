#include "handmade_gui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Hash function for generating widget IDs
u64 HandmadeGUI_HashString(const char* str) {
    u64 hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

u64 HandmadeGUI_HashPointer(void* ptr) {
    return (u64)((uintptr_t)ptr);
}

bool HandmadeGUI_Init(HandmadeGUI* gui, Renderer* renderer) {
    if (!gui || !renderer) return false;
    
    memset(gui, 0, sizeof(HandmadeGUI));
    gui->renderer = renderer;
    gui->line_height = 20.0f;
    
    // Set default style
    HandmadeGUI_SetDefaultStyle(gui);
    
    gui->initialized = true;
    printf("HandmadeGUI initialized\n");
    return true;
}

void HandmadeGUI_Shutdown(HandmadeGUI* gui) {
    if (!gui || !gui->initialized) return;
    
    gui->initialized = false;
    printf("HandmadeGUI shutdown\n");
}

void HandmadeGUI_SetDefaultStyle(HandmadeGUI* gui) {
    if (!gui) return;
    
    gui->text_color = COLOR(0.9f, 0.9f, 0.9f, 1.0f);
    gui->button_color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    gui->button_hover_color = COLOR(0.4f, 0.4f, 0.4f, 1.0f);
    gui->button_active_color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    gui->panel_color = COLOR(0.2f, 0.2f, 0.2f, 0.9f);
    gui->border_color = COLOR(0.5f, 0.5f, 0.5f, 1.0f);
}

void HandmadeGUI_BeginFrame(HandmadeGUI* gui, PlatformState* platform) {
    if (!gui || !gui->initialized) return;
    
    // Update input state
    gui->mouse_position = V2((f32)platform->input.mouse_x, (f32)platform->input.mouse_y);
    
    bool was_left_down = gui->mouse_left_down;
    bool was_right_down = gui->mouse_right_down;
    
    gui->mouse_left_down = platform->input.mouse[MOUSE_LEFT].down;
    gui->mouse_right_down = platform->input.mouse[MOUSE_RIGHT].down;
    
    gui->mouse_left_clicked = !was_left_down && gui->mouse_left_down;
    gui->mouse_right_clicked = !was_right_down && gui->mouse_right_down;
    
    // Clear interaction state if mouse not pressed
    if (!gui->mouse_left_down) {
        gui->active_id = 0;
    }
    gui->hot_id = 0;
    
    // Reset frame stats
    gui->widgets_drawn = 0;
    
    // Reset cursor to top-left
    gui->cursor = V2(10.0f, (f32)gui->renderer->viewport_height - 30.0f);
}

void HandmadeGUI_EndFrame(HandmadeGUI* gui) {
    if (!gui || !gui->initialized) return;
    
    // Nothing needed for now
}

bool HandmadeGUI_IsMouseInRect(HandmadeGUI* gui, v2 position, v2 size) {
    if (!gui) return false;
    
    // Convert world coordinates to screen coordinates
    // For now, assume GUI is rendered in screen space (no camera transform)
    f32 screen_y = (f32)gui->renderer->viewport_height - position.y;
    
    return (gui->mouse_position.x >= position.x && 
            gui->mouse_position.x < position.x + size.x &&
            gui->mouse_position.y >= screen_y - size.y && 
            gui->mouse_position.y < screen_y);
}

void HandmadeGUI_DrawRect(HandmadeGUI* gui, v2 position, v2 size, Color color) {
    if (!gui || !gui->initialized) return;
    
    RendererDrawRect(gui->renderer, position, size, color);
}

void HandmadeGUI_DrawRectOutline(HandmadeGUI* gui, v2 position, v2 size, f32 thickness, Color color) {
    if (!gui || !gui->initialized) return;
    
    RendererDrawRectOutline(gui->renderer, position, size, thickness, color);
}

void HandmadeGUI_Text(HandmadeGUI* gui, v2 position, const char* text, f32 scale, Color color) {
    if (!gui || !gui->initialized || !text) return;
    
    RendererDrawText(gui->renderer, position, text, scale, color);
    gui->widgets_drawn++;
}

void HandmadeGUI_Label(HandmadeGUI* gui, v2 position, const char* text) {
    HandmadeGUI_Text(gui, position, text, 1.0f, gui->text_color);
}

bool HandmadeGUI_Button(HandmadeGUI* gui, v2 position, v2 size, const char* text) {
    if (!gui || !gui->initialized || !text) return false;
    
    u64 id = HandmadeGUI_HashString(text);
    
    // Check mouse interaction
    bool hovered = HandmadeGUI_IsMouseInRect(gui, position, size);
    bool clicked = false;
    
    if (hovered) {
        gui->hot_id = id;
        if (gui->mouse_left_clicked) {
            gui->active_id = id;
        }
    }
    
    // Check if button was clicked
    clicked = (gui->active_id == id && !gui->mouse_left_down && hovered);
    
    // Choose button color based on state
    Color button_color = gui->button_color;
    if (gui->active_id == id && hovered) {
        button_color = gui->button_active_color;
    } else if (hovered) {
        button_color = gui->button_hover_color;
    }
    
    // Draw button background
    HandmadeGUI_DrawRect(gui, position, size, button_color);
    
    // Draw button border
    HandmadeGUI_DrawRectOutline(gui, position, size, 1.0f, gui->border_color);
    
    // Draw button text (centered)
    v2 text_size = RendererGetTextSize(gui->renderer, text, 1.0f);
    v2 text_pos = V2(
        position.x + (size.x - text_size.x) * 0.5f,
        position.y + (size.y - text_size.y) * 0.5f
    );
    
    HandmadeGUI_Text(gui, text_pos, text, 1.0f, gui->text_color);
    
    gui->widgets_drawn++;
    return clicked;
}

bool HandmadeGUI_Checkbox(HandmadeGUI* gui, v2 position, const char* label, bool* value) {
    if (!gui || !gui->initialized || !label || !value) return false;
    
    u64 id = HandmadeGUI_HashString(label) + HandmadeGUI_HashPointer(value);
    
    f32 box_size = 16.0f;
    f32 spacing = 8.0f;
    
    // ID calculated for future state tracking (handmade philosophy: understand every line!)
    (void)id; // Mark as intentionally unused for now
    
    // Check mouse interaction with checkbox box
    bool box_hovered = HandmadeGUI_IsMouseInRect(gui, position, V2(box_size, box_size));
    
    if (box_hovered && gui->mouse_left_clicked) {
        *value = !*value;
    }
    
    // Draw checkbox box
    Color box_color = box_hovered ? gui->button_hover_color : gui->button_color;
    HandmadeGUI_DrawRect(gui, position, V2(box_size, box_size), box_color);
    HandmadeGUI_DrawRectOutline(gui, position, V2(box_size, box_size), 1.0f, gui->border_color);
    
    // Draw checkmark if checked
    if (*value) {
        f32 check_size = box_size - 6.0f;
        v2 check_pos = V2(position.x + 3.0f, position.y + 3.0f);
        HandmadeGUI_DrawRect(gui, check_pos, V2(check_size, check_size), gui->text_color);
    }
    
    // Draw label
    v2 label_pos = V2(position.x + box_size + spacing, position.y);
    HandmadeGUI_Text(gui, label_pos, label, 1.0f, gui->text_color);
    
    gui->widgets_drawn++;
    return *value;
}

bool HandmadeGUI_BeginPanel(HandmadeGUI* gui, GUIPanel* panel) {
    if (!gui || !gui->initialized || !panel) return false;
    
    // Check if panel should be visible
    if (panel->open && !*panel->open) return false;
    
    u64 panel_id = HandmadeGUI_HashString(panel->title ? panel->title : "Panel");
    
    // Handle panel dragging
    bool title_bar_hovered = false;
    if (panel->is_draggable && panel->title) {
        f32 title_height = 24.0f;
        v2 title_pos = V2(panel->position.x, panel->position.y + panel->size.y - title_height);
        v2 title_size = V2(panel->size.x, title_height);
        
        title_bar_hovered = HandmadeGUI_IsMouseInRect(gui, title_pos, title_size);
        
        if (title_bar_hovered && gui->mouse_left_clicked) {
            gui->active_id = panel_id;
            panel->drag_offset = V2(gui->mouse_position.x - panel->position.x,
                                   gui->mouse_position.y - panel->position.y);
        }
        
        if (gui->active_id == panel_id && gui->mouse_left_down) {
            panel->position.x = gui->mouse_position.x - panel->drag_offset.x;
            panel->position.y = gui->mouse_position.y - panel->drag_offset.y;
        }
    }
    
    // Draw panel background
    HandmadeGUI_DrawRect(gui, panel->position, panel->size, gui->panel_color);
    HandmadeGUI_DrawRectOutline(gui, panel->position, panel->size, 1.0f, gui->border_color);
    
    // Draw title bar if title is provided
    if (panel->title) {
        f32 title_height = 24.0f;
        v2 title_pos = V2(panel->position.x, panel->position.y + panel->size.y - title_height);
        v2 title_size = V2(panel->size.x, title_height);
        
        Color title_color = title_bar_hovered ? gui->button_hover_color : gui->button_color;
        HandmadeGUI_DrawRect(gui, title_pos, title_size, title_color);
        HandmadeGUI_DrawRectOutline(gui, title_pos, title_size, 1.0f, gui->border_color);
        
        // Draw title text
        v2 title_text_pos = V2(title_pos.x + 8.0f, title_pos.y + 4.0f);
        HandmadeGUI_Text(gui, title_text_pos, panel->title, 1.0f, gui->text_color);
        
        // Draw close button if requested
        if (panel->has_close_button && panel->open) {
            f32 close_size = 16.0f;
            v2 close_pos = V2(title_pos.x + title_size.x - close_size - 4.0f, 
                             title_pos.y + 4.0f);
            
            bool close_hovered = HandmadeGUI_IsMouseInRect(gui, close_pos, V2(close_size, close_size));
            
            if (close_hovered && gui->mouse_left_clicked) {
                *panel->open = false;
                return false;
            }
            
            Color close_color = close_hovered ? COLOR(0.8f, 0.3f, 0.3f, 1.0f) : gui->border_color;
            HandmadeGUI_DrawRect(gui, close_pos, V2(close_size, close_size), close_color);
            
            // Draw X
            v2 x_pos = V2(close_pos.x + 4.0f, close_pos.y + 2.0f);
            HandmadeGUI_Text(gui, x_pos, "X", 1.0f, gui->text_color);
        }
    }
    
    // Set cursor for panel content
    f32 title_height = panel->title ? 24.0f : 0.0f;
    gui->cursor = V2(panel->position.x + 8.0f, 
                    panel->position.y + panel->size.y - title_height - 8.0f);
    
    gui->widgets_drawn++;
    return true;
}

void HandmadeGUI_EndPanel(HandmadeGUI* gui) {
    // Nothing special needed for now
}

void HandmadeGUI_SetCursor(HandmadeGUI* gui, v2 position) {
    if (!gui) return;
    gui->cursor = position;
}

v2 HandmadeGUI_GetCursor(HandmadeGUI* gui) {
    return gui ? gui->cursor : V2(0, 0);
}

void HandmadeGUI_SameLine(HandmadeGUI* gui) {
    // SameLine means don't advance cursor vertically
    // This is handled by widgets that want to use same line
}

void HandmadeGUI_NewLine(HandmadeGUI* gui) {
    if (!gui) return;
    gui->cursor.y -= gui->line_height;
}

void HandmadeGUI_Separator(HandmadeGUI* gui, v2 position, f32 width) {
    if (!gui || !gui->initialized) return;
    
    f32 thickness = 1.0f;
    v2 size = V2(width, thickness);
    HandmadeGUI_DrawRect(gui, position, size, gui->border_color);
    
    gui->widgets_drawn++;
}

void HandmadeGUI_ShowDebugPanel(HandmadeGUI* gui, v2 position) {
    if (!gui || !gui->initialized) return;
    
    char debug_text[512];
    snprintf(debug_text, sizeof(debug_text), 
             "GUI Debug:\nWidgets: %u\nMouse: %.0f, %.0f\nHot: %lu\nActive: %lu",
             gui->widgets_drawn,
             gui->mouse_position.x, gui->mouse_position.y,
             gui->hot_id, gui->active_id);
    
    // Simple debug panel background
    v2 panel_size = V2(200.0f, 120.0f);
    HandmadeGUI_DrawRect(gui, position, panel_size, gui->panel_color);
    HandmadeGUI_DrawRectOutline(gui, position, panel_size, 1.0f, gui->border_color);
    
    // Draw debug text
    v2 text_pos = V2(position.x + 8.0f, position.y + panel_size.y - 20.0f);
    HandmadeGUI_Text(gui, text_pos, debug_text, 1.0f, gui->text_color);
    
    gui->widgets_drawn++;
}