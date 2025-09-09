/*
    Settings UI Implementation
    Immediate-mode GUI for settings management
*/

#include "handmade_settings.h"
#include <stdio.h>
#include <string.h>

#define internal static

// Stub GUI context and functions for demo
typedef struct gui_context {
    i32 x, y;           // Current position
    i32 width, height;  // Window size
    u32 hot_id;         // Currently hovered element
    u32 active_id;      // Currently active element
    b32 mouse_down;
    i32 mouse_x, mouse_y;
} gui_context;

// Simple UI element positioning
internal void
gui_layout_vertical(gui_context *gui, i32 spacing) {
    gui->y += spacing;
}

internal void
gui_set_position(gui_context *gui, i32 x, i32 y) {
    gui->x = x;
    gui->y = y;
}

// Check if point is inside rectangle
internal b32
point_in_rect(i32 px, i32 py, i32 rx, i32 ry, i32 rw, i32 rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

// Generate unique ID for UI elements
internal u32
gui_get_id(char *str) {
    u32 hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (u32)*str++;
    }
    return hash;
}

// Draw text (stub)
internal void
gui_text(gui_context *gui, char *text, i32 x, i32 y) {
    printf("TEXT[%d,%d]: %s\n", x, y, text);
}

// Draw button
internal b32
gui_button(gui_context *gui, char *text, i32 x, i32 y, i32 w, i32 h) {
    u32 id = gui_get_id(text);
    b32 hot = point_in_rect(gui->mouse_x, gui->mouse_y, x, y, w, h);
    b32 active = (gui->active_id == id);
    
    if (hot) gui->hot_id = id;
    
    if (hot && gui->mouse_down && gui->active_id == 0) {
        gui->active_id = id;
    }
    
    b32 clicked = 0;
    if (active && !gui->mouse_down) {
        if (hot) clicked = 1;
        gui->active_id = 0;
    }
    
    printf("BUTTON[%d,%d,%dx%d]%s%s: %s\n", x, y, w, h, 
           hot ? " HOT" : "", active ? " ACTIVE" : "", text);
    
    return clicked;
}

// Draw checkbox
internal b32
gui_checkbox(gui_context *gui, char *text, b32 *value, i32 x, i32 y) {
    char button_text[256];
    snprintf(button_text, 256, "[%c] %s", *value ? 'X' : ' ', text);
    
    if (gui_button(gui, button_text, x, y, 200, 25)) {
        *value = !*value;
        return 1;
    }
    return 0;
}

// Draw slider
internal b32
gui_slider_int(gui_context *gui, char *text, i32 *value, i32 min_val, i32 max_val, i32 x, i32 y) {
    char slider_text[256];
    snprintf(slider_text, 256, "%s: %d", text, *value);
    
    gui_text(gui, slider_text, x, y);
    
    // Simple slider implementation
    i32 slider_x = x + 150;
    i32 slider_w = 100;
    i32 slider_h = 20;
    
    u32 id = gui_get_id(text);
    b32 hot = point_in_rect(gui->mouse_x, gui->mouse_y, slider_x, y, slider_w, slider_h);
    b32 active = (gui->active_id == id);
    
    if (hot) gui->hot_id = id;
    
    if (hot && gui->mouse_down && gui->active_id == 0) {
        gui->active_id = id;
    }
    
    b32 changed = 0;
    if (active && gui->mouse_down) {
        // Calculate new value based on mouse position
        f32 t = (f32)(gui->mouse_x - slider_x) / (f32)slider_w;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        
        i32 new_value = (i32)(min_val + t * (max_val - min_val));
        if (new_value != *value) {
            *value = new_value;
            changed = 1;
        }
    }
    
    if (active && !gui->mouse_down) {
        gui->active_id = 0;
    }
    
    printf("SLIDER[%d,%d,%dx%d]%s%s: %s = %d\n", 
           slider_x, y, slider_w, slider_h,
           hot ? " HOT" : "", active ? " ACTIVE" : "", text, *value);
    
    return changed;
}

// Draw float slider
internal b32
gui_slider_float(gui_context *gui, char *text, f32 *value, f32 min_val, f32 max_val, i32 x, i32 y) {
    char slider_text[256];
    snprintf(slider_text, 256, "%s: %.2f", text, *value);
    
    gui_text(gui, slider_text, x, y);
    
    // Simple slider implementation
    i32 slider_x = x + 150;
    i32 slider_w = 100;
    i32 slider_h = 20;
    
    u32 id = gui_get_id(text);
    b32 hot = point_in_rect(gui->mouse_x, gui->mouse_y, slider_x, y, slider_w, slider_h);
    b32 active = (gui->active_id == id);
    
    if (hot) gui->hot_id = id;
    
    if (hot && gui->mouse_down && gui->active_id == 0) {
        gui->active_id = id;
    }
    
    b32 changed = 0;
    if (active && gui->mouse_down) {
        // Calculate new value based on mouse position
        f32 t = (f32)(gui->mouse_x - slider_x) / (f32)slider_w;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        
        f32 new_value = min_val + t * (max_val - min_val);
        if (new_value != *value) {
            *value = new_value;
            changed = 1;
        }
    }
    
    if (active && !gui->mouse_down) {
        gui->active_id = 0;
    }
    
    printf("SLIDER[%d,%d,%dx%d]%s%s: %s = %.2f\n", 
           slider_x, y, slider_w, slider_h,
           hot ? " HOT" : "", active ? " ACTIVE" : "", text, *value);
    
    return changed;
}

// Draw dropdown/enum selector
internal b32
gui_dropdown(gui_context *gui, char *text, i32 *selected, char **options, i32 option_count, i32 x, i32 y) {
    char dropdown_text[256];
    if (*selected >= 0 && *selected < option_count) {
        snprintf(dropdown_text, 256, "%s: %s", text, options[*selected]);
    } else {
        snprintf(dropdown_text, 256, "%s: <invalid>", text);
    }
    
    if (gui_button(gui, dropdown_text, x, y, 250, 25)) {
        // Cycle through options (simple implementation)
        *selected = (*selected + 1) % option_count;
        return 1;
    }
    
    return 0;
}

// Render category header
internal void
gui_category_header(gui_context *gui, settings_category *category, i32 x, i32 y) {
    char header_text[256];
    snprintf(header_text, 256, "%s %s", category->expanded ? "[-]" : "[+]", category->name);
    
    if (gui_button(gui, header_text, x, y, 300, 30)) {
        category->expanded = !category->expanded;
    }
    
    gui_layout_vertical(gui, 35);
}

// Render individual setting
internal void
gui_render_setting(gui_context *gui, settings_system *system, setting *s, i32 x, i32 y) {
    b32 changed = 0;
    
    switch (s->type) {
        case SETTING_BOOL: {
            b32 value = s->current_value.boolean;
            if (gui_checkbox(gui, s->name, &value, x + 20, y)) {
                s->current_value.boolean = value;
                changed = 1;
            }
        } break;
        
        case SETTING_INT: {
            i32 value = s->current_value.integer;
            if (gui_slider_int(gui, s->name, &value, 
                              s->min_value.integer, s->max_value.integer, x + 20, y)) {
                s->current_value.integer = value;
                changed = 1;
            }
        } break;
        
        case SETTING_FLOAT: {
            f32 value = s->current_value.floating;
            if (gui_slider_float(gui, s->name, &value,
                                s->min_value.floating, s->max_value.floating, x + 20, y)) {
                s->current_value.floating = value;
                changed = 1;
            }
        } break;
        
        case SETTING_ENUM: {
            i32 value = s->current_value.enumeration;
            char *options[16];
            for (i32 i = 0; i < s->constraint_data.list.count; i++) {
                options[i] = s->constraint_data.list.options[i];
            }
            
            if (gui_dropdown(gui, s->name, &value, options, 
                           s->constraint_data.list.count, x + 20, y)) {
                s->current_value.enumeration = value;
                changed = 1;
            }
        } break;
        
        case SETTING_STRING: {
            // Simple text display for now
            char text_display[256];
            snprintf(text_display, 256, "%s: \"%s\"", s->name, s->current_value.string);
            gui_text(gui, text_display, x + 20, y);
        } break;
    }
    
    if (changed) {
        system->changes_pending++;
        
        // Call change callback
        if (s->on_change) {
            // Would pass old value here in real implementation
            setting_value dummy_old = {0};
            s->on_change(s, dummy_old, s->current_value);
        }
    }
    
    gui_layout_vertical(gui, 30);
}

// Main settings UI rendering
void
settings_render_ui(settings_system *system, gui_context *gui) {
    if (!system->ui_visible) return;
    
    printf("\n=== Settings Menu ===\n");
    
    gui_set_position(gui, 50, 50);
    
    // Title
    gui_text(gui, "Game Settings", gui->x, gui->y);
    gui_layout_vertical(gui, 40);
    
    // Profile selector
    char profile_text[256];
    snprintf(profile_text, 256, "Profile: %s", system->profiles[system->active_profile].name);
    gui_text(gui, profile_text, gui->x, gui->y);
    gui_layout_vertical(gui, 30);
    
    // Category tabs (simple vertical list for now)
    printf("Categories:\n");
    for (u32 i = 0; i < system->category_count; i++) {
        settings_category *category = &system->categories[i];
        
        gui_category_header(gui, category, gui->x, gui->y);
        
        if (category->expanded) {
            // Render settings in this category
            for (u32 j = 0; j < category->setting_count; j++) {
                setting *s = category->settings[j];
                
                // Skip hidden/advanced settings based on filter
                if (s->flags & SETTING_HIDDEN) continue;
                if ((s->flags & SETTING_ADVANCED) && !system->filter.show_advanced) continue;
                if ((s->flags & SETTING_READONLY) && !system->filter.show_readonly) continue;
                
                gui_render_setting(gui, system, s, gui->x, gui->y);
            }
        }
    }
    
    // Control buttons
    gui_layout_vertical(gui, 20);
    
    if (gui_button(gui, "Reset to Defaults", gui->x, gui->y, 150, 30)) {
        settings_reset_to_defaults(system);
    }
    gui->x += 160;
    
    if (gui_button(gui, "Apply Changes", gui->x, gui->y, 120, 30)) {
        settings_auto_save(system);
    }
    gui->x += 130;
    
    if (gui_button(gui, "Close", gui->x, gui->y, 80, 30)) {
        settings_hide_menu(system);
    }
    
    printf("=== End Settings Menu ===\n\n");
}

// Handle settings menu input
b32
settings_handle_input(settings_system *system, input_state *input) {
    if (!system->ui_visible) {
        return 0;
    }
    
    // Simple input handling stub
    // In real implementation, this would process keyboard/mouse events
    return 1; // Consumed input
}

// settings_update function is implemented in handmade_settings.c