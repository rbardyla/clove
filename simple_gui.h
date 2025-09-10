#ifndef SIMPLE_GUI_H
#define SIMPLE_GUI_H

#include "handmade_platform.h"
#include "minimal_renderer.h"

// Simple immediate-mode GUI system
typedef struct {
    renderer *r;
    
    // Input state
    i32 mouse_x, mouse_y;
    bool mouse_left_down;
    bool mouse_left_clicked;
    
    // Widget state
    u64 hot_id;
    u64 active_id;
    
    // Layout
    i32 cursor_x, cursor_y;
    
    // Performance
    u32 widgets_drawn;
    f32 frame_time;
} simple_gui;

// Initialize GUI
void simple_gui_init(simple_gui *gui, renderer *r);

// Frame management
void simple_gui_begin_frame(simple_gui *gui, PlatformState *platform);
void simple_gui_end_frame(simple_gui *gui);

// Basic widgets
bool simple_gui_button(simple_gui *gui, i32 x, i32 y, const char *text);
bool simple_gui_checkbox(simple_gui *gui, i32 x, i32 y, const char *text, bool *value);
void simple_gui_text(simple_gui *gui, i32 x, i32 y, const char *text);
void simple_gui_performance(simple_gui *gui, i32 x, i32 y);

// Editor-specific widgets
typedef struct {
    i32 x, y, width, height;
    const char *title;
    bool *open;
    bool collapsed;
    bool resizable;
} gui_panel;

// Panel system
bool simple_gui_begin_panel(simple_gui *gui, gui_panel *panel);
void simple_gui_end_panel(simple_gui *gui, gui_panel *panel);

// Tree view for scene hierarchy
typedef struct {
    const char *label;
    bool expanded;
    i32 depth;
    bool selected;
} gui_tree_node;

bool simple_gui_tree_node(simple_gui *gui, i32 x, i32 y, gui_tree_node *node);

// Property editor
bool simple_gui_property_float(simple_gui *gui, i32 x, i32 y, const char *label, f32 *value);
bool simple_gui_property_int(simple_gui *gui, i32 x, i32 y, const char *label, i32 *value);
bool simple_gui_property_string(simple_gui *gui, i32 x, i32 y, const char *label, char *buffer, i32 buffer_size);

// Input field for editable properties
typedef struct {
    bool editing;
    char temp_buffer[128];
    u64 id;
} gui_input_field;

bool simple_gui_input_float(simple_gui *gui, i32 x, i32 y, i32 width, f32 *value, gui_input_field *field);
bool simple_gui_input_text(simple_gui *gui, i32 x, i32 y, i32 width, char *buffer, i32 buffer_size, gui_input_field *field);

// File browser
typedef struct {
    char path[256];
    char *files;  // Array of filenames
    i32 file_count;
    i32 selected_file;
} gui_file_browser;

void simple_gui_file_browser(simple_gui *gui, i32 x, i32 y, i32 w, i32 h, gui_file_browser *browser);

// Menu system
typedef struct {
    const char *label;
    bool enabled;
    bool (*callback)(void);
} gui_menu_item;

typedef struct {
    const char *label;
    gui_menu_item *items;
    i32 item_count;
    bool open;
} gui_menu;

void simple_gui_menu_bar(simple_gui *gui, i32 x, i32 y, gui_menu *menus, i32 menu_count);

// Toolbar
typedef struct {
    const char *label;
    bool active;
    bool (*callback)(void);
} gui_tool_button;

void simple_gui_toolbar(simple_gui *gui, i32 x, i32 y, gui_tool_button *tools, i32 tool_count);

// Drawing utilities
void simple_gui_separator(simple_gui *gui, i32 x, i32 y, i32 width);

#endif // SIMPLE_GUI_H