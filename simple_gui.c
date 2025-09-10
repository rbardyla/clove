#include "simple_gui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static u64 gui_hash_string(const char *str) {
    u64 hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void simple_gui_init(simple_gui *gui, renderer *r) {
    memset(gui, 0, sizeof(simple_gui));
    gui->r = r;
}

void simple_gui_begin_frame(simple_gui *gui, PlatformState *platform) {
    // Update input
    gui->mouse_x = (i32)platform->input.mouse_x;
    gui->mouse_y = (i32)platform->input.mouse_y;
    
    bool was_down = gui->mouse_left_down;
    gui->mouse_left_down = platform->input.mouse[MOUSE_LEFT].down;
    gui->mouse_left_clicked = !was_down && gui->mouse_left_down;
    
    // Reset per-frame state
    if (!gui->mouse_left_down) {
        gui->active_id = 0;
    }
    gui->hot_id = 0;
    gui->widgets_drawn = 0;
}

void simple_gui_end_frame(simple_gui *gui) {
    // Nothing needed for now
}

bool simple_gui_button(simple_gui *gui, i32 x, i32 y, const char *text) {
    u64 id = gui_hash_string(text);
    
    // Button dimensions
    int text_w, text_h;
    renderer_text_size(gui->r, text, &text_w, &text_h);
    i32 button_w = text_w + 16;
    i32 button_h = 24;
    
    // Check if mouse is over button
    bool hovered = gui->mouse_x >= x && gui->mouse_x < x + button_w &&
                  gui->mouse_y >= y && gui->mouse_y < y + button_h;
    
    if (hovered) {
        gui->hot_id = id;
        if (gui->mouse_left_clicked) {
            gui->active_id = id;
        }
    }
    
    bool clicked = (gui->active_id == id && gui->mouse_left_clicked && hovered);
    
    // Choose color based on state
    color32 button_color = rgb(60, 60, 60);
    if (gui->active_id == id && hovered) {
        button_color = rgb(40, 40, 40);
    } else if (hovered) {
        button_color = rgb(80, 80, 80);
    }
    
    // Draw button
    renderer_fill_rect(gui->r, x, y, button_w, button_h, button_color);
    renderer_draw_rect(gui->r, x, y, button_w, button_h, rgb(100, 100, 100));
    
    // Draw text centered
    i32 text_x = x + (button_w - text_w) / 2;
    i32 text_y = y + (button_h - text_h) / 2;
    renderer_text(gui->r, text_x, text_y, text, rgb(220, 220, 220));
    
    gui->widgets_drawn++;
    return clicked;
}

bool simple_gui_checkbox(simple_gui *gui, i32 x, i32 y, const char *text, bool *value) {
    u64 id = gui_hash_string(text) + (u64)value; // Include pointer for unique ID
    
    i32 box_size = 16;
    
    // Check if mouse is over checkbox
    bool hovered = gui->mouse_x >= x && gui->mouse_x < x + box_size &&
                  gui->mouse_y >= y && gui->mouse_y < y + box_size;
    
    if (hovered && gui->mouse_left_clicked) {
        *value = !*value;
    }
    
    // Draw checkbox box
    color32 bg_color = hovered ? rgb(80, 80, 80) : rgb(50, 50, 50);
    renderer_fill_rect(gui->r, x, y, box_size, box_size, bg_color);
    renderer_draw_rect(gui->r, x, y, box_size, box_size, rgb(100, 100, 100));
    
    // Draw checkmark if checked
    if (*value) {
        i32 padding = 3;
        renderer_fill_rect(gui->r, x + padding, y + padding, 
                          box_size - 2 * padding, box_size - 2 * padding, 
                          rgb(100, 160, 240));
    }
    
    // Draw text
    renderer_text(gui->r, x + box_size + 8, y + 2, text, rgb(220, 220, 220));
    
    gui->widgets_drawn++;
    return *value;
}

void simple_gui_text(simple_gui *gui, i32 x, i32 y, const char *text) {
    renderer_text(gui->r, x, y, text, rgb(220, 220, 220));
    gui->widgets_drawn++;
}

void simple_gui_performance(simple_gui *gui, i32 x, i32 y) {
    // Draw performance info background
    i32 bg_w = 280;
    i32 bg_h = 60;
    
    renderer_fill_rect(gui->r, x, y, bg_w, bg_h, rgba(30, 30, 30, 200));
    renderer_draw_rect(gui->r, x, y, bg_w, bg_h, rgb(80, 80, 80));
    
    // Performance text
    char perf_text[256];
    snprintf(perf_text, sizeof(perf_text), 
             "Widgets: %u", gui->widgets_drawn);
    
    simple_gui_text(gui, x + 8, y + 8, "Performance:");
    simple_gui_text(gui, x + 8, y + 24, perf_text);
    simple_gui_text(gui, x + 8, y + 40, "Simple GUI System");
}

// Panel system implementation
bool simple_gui_begin_panel(simple_gui *gui, gui_panel *panel) {
    if (!*panel->open) return false;
    
    // Draw panel background
    renderer_fill_rect(gui->r, panel->x, panel->y, panel->width, panel->height, rgba(40, 40, 40, 240));
    renderer_draw_rect(gui->r, panel->x, panel->y, panel->width, panel->height, rgb(100, 100, 100));
    
    // Draw title bar
    i32 title_height = 24;
    renderer_fill_rect(gui->r, panel->x, panel->y, panel->width, title_height, rgb(60, 60, 60));
    renderer_draw_rect(gui->r, panel->x, panel->y, panel->width, title_height, rgb(80, 80, 80));
    simple_gui_text(gui, panel->x + 8, panel->y + 4, panel->title);
    
    // Close button
    i32 close_size = 16;
    i32 close_x = panel->x + panel->width - close_size - 4;
    i32 close_y = panel->y + 4;
    
    bool close_hovered = gui->mouse_x >= close_x && gui->mouse_x < close_x + close_size &&
                        gui->mouse_y >= close_y && gui->mouse_y < close_y + close_size;
    
    color32 close_color = close_hovered ? rgb(180, 80, 80) : rgb(120, 120, 120);
    renderer_fill_rect(gui->r, close_x, close_y, close_size, close_size, close_color);
    simple_gui_text(gui, close_x + 4, close_y + 2, "X");
    
    if (close_hovered && gui->mouse_left_clicked) {
        *panel->open = false;
        return false;
    }
    
    // Update cursor for panel content
    gui->cursor_x = panel->x + 8;
    gui->cursor_y = panel->y + title_height + 8;
    
    return true;
}

void simple_gui_end_panel(simple_gui *gui, gui_panel *panel) {
    // Nothing needed for now
}

// Tree node implementation
bool simple_gui_tree_node(simple_gui *gui, i32 x, i32 y, gui_tree_node *node) {
    u64 id = gui_hash_string(node->label) + (u64)node;
    
    i32 indent = node->depth * 16;
    i32 arrow_size = 12;
    i32 text_x = x + indent + arrow_size + 4;
    i32 arrow_x = x + indent;
    
    // Draw expand/collapse arrow if has children (for now, assume all nodes can expand)
    bool arrow_hovered = gui->mouse_x >= arrow_x && gui->mouse_x < arrow_x + arrow_size &&
                        gui->mouse_y >= y && gui->mouse_y < y + arrow_size;
    
    color32 arrow_color = arrow_hovered ? rgb(200, 200, 200) : rgb(150, 150, 150);
    
    // Draw arrow (simple triangle)
    if (node->expanded) {
        // Down arrow (expanded)
        simple_gui_text(gui, arrow_x, y, "v");
    } else {
        // Right arrow (collapsed)
        simple_gui_text(gui, arrow_x, y, ">");
    }
    
    // Draw selection background
    if (node->selected) {
        int text_w, text_h;
        renderer_text_size(gui->r, node->label, &text_w, &text_h);
        renderer_fill_rect(gui->r, text_x - 2, y - 2, text_w + 4, text_h + 4, rgb(80, 120, 200));
    }
    
    // Draw node text
    color32 text_color = node->selected ? rgb(255, 255, 255) : rgb(220, 220, 220);
    renderer_text(gui->r, text_x, y, node->label, text_color);
    
    // Check for clicks
    int text_w, text_h;
    renderer_text_size(gui->r, node->label, &text_w, &text_h);
    bool text_clicked = gui->mouse_x >= text_x && gui->mouse_x < text_x + text_w &&
                       gui->mouse_y >= y && gui->mouse_y < y + text_h && gui->mouse_left_clicked;
    
    bool arrow_clicked = arrow_hovered && gui->mouse_left_clicked;
    
    if (arrow_clicked) {
        node->expanded = !node->expanded;
    }
    
    if (text_clicked) {
        node->selected = !node->selected;
    }
    
    gui->widgets_drawn++;
    return text_clicked;
}

// Property editor implementations
bool simple_gui_property_float(simple_gui *gui, i32 x, i32 y, const char *label, f32 *value) {
    // Draw label
    simple_gui_text(gui, x, y, label);
    
    // Draw value (for now, just display it - we'll add editing later)
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.2f", *value);
    simple_gui_text(gui, x + 120, y, value_str);
    
    gui->widgets_drawn++;
    return false; // Not editable yet
}

bool simple_gui_property_int(simple_gui *gui, i32 x, i32 y, const char *label, i32 *value) {
    // Draw label
    simple_gui_text(gui, x, y, label);
    
    // Draw value
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%d", *value);
    simple_gui_text(gui, x + 120, y, value_str);
    
    gui->widgets_drawn++;
    return false; // Not editable yet
}

bool simple_gui_property_string(simple_gui *gui, i32 x, i32 y, const char *label, char *buffer, i32 buffer_size) {
    // Draw label
    simple_gui_text(gui, x, y, label);
    
    // Draw value
    simple_gui_text(gui, x + 120, y, buffer);
    
    gui->widgets_drawn++;
    return false; // Not editable yet
}

// File browser implementation
void simple_gui_file_browser(simple_gui *gui, i32 x, i32 y, i32 w, i32 h, gui_file_browser *browser) {
    // Draw background
    renderer_fill_rect(gui->r, x, y, w, h, rgba(30, 30, 30, 240));
    renderer_draw_rect(gui->r, x, y, w, h, rgb(80, 80, 80));
    
    // Draw current path
    simple_gui_text(gui, x + 4, y + 4, "Path:");
    simple_gui_text(gui, x + 40, y + 4, browser->path);
    
    // For now, show placeholder files
    const char *placeholder_files[] = {
        "scene1.data",
        "player.prefab",
        "materials/",
        "textures/",
        "scripts/",
        "audio.wav"
    };
    i32 file_count = sizeof(placeholder_files) / sizeof(placeholder_files[0]);
    
    i32 file_y = y + 24;
    for (i32 i = 0; i < file_count && file_y < y + h - 20; i++) {
        bool selected = (i == browser->selected_file);
        
        if (selected) {
            renderer_fill_rect(gui->r, x + 4, file_y - 2, w - 8, 16, rgb(80, 120, 200));
        }
        
        color32 file_color = selected ? rgb(255, 255, 255) : rgb(200, 200, 200);
        renderer_text(gui->r, x + 8, file_y, placeholder_files[i], file_color);
        
        // Check for file selection
        bool file_clicked = gui->mouse_x >= x + 4 && gui->mouse_x < x + w - 4 &&
                           gui->mouse_y >= file_y - 2 && gui->mouse_y < file_y + 14 && 
                           gui->mouse_left_clicked;
        
        if (file_clicked) {
            browser->selected_file = i;
        }
        
        file_y += 18;
    }
    
    gui->widgets_drawn++;
}

// Menu system implementation
void simple_gui_menu_bar(simple_gui *gui, i32 x, i32 y, gui_menu *menus, i32 menu_count) {
    // Draw menu bar background
    i32 menu_height = 24;
    renderer_fill_rect(gui->r, x, y, gui->r->width, menu_height, rgb(50, 50, 50));
    renderer_draw_rect(gui->r, x, y, gui->r->width, 1, rgb(80, 80, 80));
    
    i32 menu_x = x + 4;
    
    for (i32 i = 0; i < menu_count; i++) {
        gui_menu *menu = &menus[i];
        
        // Calculate menu width
        int text_w, text_h;
        renderer_text_size(gui->r, menu->label, &text_w, &text_h);
        i32 menu_w = text_w + 16;
        
        // Check if hovered
        bool hovered = gui->mouse_x >= menu_x && gui->mouse_x < menu_x + menu_w &&
                      gui->mouse_y >= y && gui->mouse_y < y + menu_height;
        
        // Draw menu background if hovered or open
        if (hovered || menu->open) {
            renderer_fill_rect(gui->r, menu_x, y, menu_w, menu_height, rgb(70, 70, 70));
        }
        
        // Draw menu text
        simple_gui_text(gui, menu_x + 8, y + 4, menu->label);
        
        // Handle click to open/close menu
        if (hovered && gui->mouse_left_clicked) {
            menu->open = !menu->open;
        }
        
        menu_x += menu_w + 4;
    }
    
    gui->widgets_drawn++;
}

// Toolbar implementation
void simple_gui_toolbar(simple_gui *gui, i32 x, i32 y, gui_tool_button *tools, i32 tool_count) {
    i32 button_size = 32;
    i32 spacing = 4;
    
    // Draw toolbar background
    i32 toolbar_w = tool_count * (button_size + spacing) + spacing;
    i32 toolbar_h = button_size + spacing * 2;
    
    renderer_fill_rect(gui->r, x, y, toolbar_w, toolbar_h, rgb(45, 45, 45));
    renderer_draw_rect(gui->r, x, y, toolbar_w, toolbar_h, rgb(80, 80, 80));
    
    i32 button_x = x + spacing;
    i32 button_y = y + spacing;
    
    for (i32 i = 0; i < tool_count; i++) {
        gui_tool_button *tool = &tools[i];
        
        // Check if hovered
        bool hovered = gui->mouse_x >= button_x && gui->mouse_x < button_x + button_size &&
                      gui->mouse_y >= button_y && gui->mouse_y < button_y + button_size;
        
        // Choose button color
        color32 button_color = rgb(60, 60, 60);
        if (tool->active) {
            button_color = rgb(80, 120, 200);  // Active tool color
        } else if (hovered) {
            button_color = rgb(80, 80, 80);    // Hovered color
        }
        
        // Draw button
        renderer_fill_rect(gui->r, button_x, button_y, button_size, button_size, button_color);
        renderer_draw_rect(gui->r, button_x, button_y, button_size, button_size, rgb(100, 100, 100));
        
        // Draw tool label (centered)
        int text_w, text_h;
        renderer_text_size(gui->r, tool->label, &text_w, &text_h);
        i32 text_x = button_x + (button_size - text_w) / 2;
        i32 text_y = button_y + (button_size - text_h) / 2;
        
        color32 text_color = tool->active ? rgb(255, 255, 255) : rgb(220, 220, 220);
        renderer_text(gui->r, text_x, text_y, tool->label, text_color);
        
        // Handle click
        if (hovered && gui->mouse_left_clicked && tool->callback) {
            tool->callback();
        }
        
        button_x += button_size + spacing;
    }
    
    gui->widgets_drawn++;
}

// Enhanced input field implementations
bool simple_gui_input_float(simple_gui *gui, i32 x, i32 y, i32 width, f32 *value, gui_input_field *field) {
    u64 id = (u64)field;
    
    // Input field area
    i32 height = 20;
    bool hovered = gui->mouse_x >= x && gui->mouse_x < x + width &&
                  gui->mouse_y >= y && gui->mouse_y < y + height;
    
    // Start editing on click
    if (hovered && gui->mouse_left_clicked) {
        if (!field->editing) {
            field->editing = true;
            field->id = id;
            snprintf(field->temp_buffer, sizeof(field->temp_buffer), "%.3f", *value);
        }
    }
    
    // Stop editing if clicked outside
    if (field->editing && gui->mouse_left_clicked && !hovered) {
        field->editing = false;
        *value = (f32)atof(field->temp_buffer);  // Convert back to float
    }
    
    // Draw input field
    color32 bg_color = field->editing ? rgb(60, 60, 80) : (hovered ? rgb(55, 55, 55) : rgb(45, 45, 45));
    renderer_fill_rect(gui->r, x, y, width, height, bg_color);
    renderer_draw_rect(gui->r, x, y, width, height, rgb(100, 100, 100));
    
    // Draw text content
    const char *display_text = field->editing ? field->temp_buffer : "";
    if (!field->editing) {
        snprintf(field->temp_buffer, sizeof(field->temp_buffer), "%.3f", *value);
        display_text = field->temp_buffer;
    }
    
    renderer_text(gui->r, x + 4, y + 2, display_text, rgb(220, 220, 220));
    
    // Draw cursor if editing
    if (field->editing) {
        i32 cursor_x = x + 4 + (i32)strlen(field->temp_buffer) * 8;  // Approximate character width
        renderer_fill_rect(gui->r, cursor_x, y + 2, 1, 16, rgb(255, 255, 255));
    }
    
    gui->widgets_drawn++;
    return field->editing;
}

bool simple_gui_input_text(simple_gui *gui, i32 x, i32 y, i32 width, char *buffer, i32 buffer_size, gui_input_field *field) {
    u64 id = (u64)field;
    
    // Input field area
    i32 height = 20;
    bool hovered = gui->mouse_x >= x && gui->mouse_x < x + width &&
                  gui->mouse_y >= y && gui->mouse_y < y + height;
    
    // Start editing on click
    if (hovered && gui->mouse_left_clicked) {
        if (!field->editing) {
            field->editing = true;
            field->id = id;
            strncpy(field->temp_buffer, buffer, sizeof(field->temp_buffer) - 1);
            field->temp_buffer[sizeof(field->temp_buffer) - 1] = '\0';
        }
    }
    
    // Stop editing if clicked outside
    if (field->editing && gui->mouse_left_clicked && !hovered) {
        field->editing = false;
        strncpy(buffer, field->temp_buffer, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
    
    // Draw input field
    color32 bg_color = field->editing ? rgb(60, 60, 80) : (hovered ? rgb(55, 55, 55) : rgb(45, 45, 45));
    renderer_fill_rect(gui->r, x, y, width, height, bg_color);
    renderer_draw_rect(gui->r, x, y, width, height, rgb(100, 100, 100));
    
    // Draw text content
    const char *display_text = field->editing ? field->temp_buffer : buffer;
    renderer_text(gui->r, x + 4, y + 2, display_text, rgb(220, 220, 220));
    
    // Draw cursor if editing
    if (field->editing) {
        i32 cursor_x = x + 4 + (i32)strlen(field->temp_buffer) * 8;  // Approximate character width
        renderer_fill_rect(gui->r, cursor_x, y + 2, 1, 16, rgb(255, 255, 255));
    }
    
    gui->widgets_drawn++;
    return field->editing;
}