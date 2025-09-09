// nodes_editor.c - Node graph editor UI
// PERFORMANCE: Immediate mode GUI, no allocations per frame
// FEATURES: Context menu, copy/paste, undo/redo, search

#include "handmade_nodes.h"
#include "../gui/handmade_gui.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Editor state
typedef struct {
    node_graph_t *graph;
    gui_context *gui;
    
    // Interaction state
    bool dragging_node;
    i32 dragged_node_id;
    f32 drag_offset_x, drag_offset_y;
    
    bool connecting;
    i32 connect_from_node;
    i32 connect_from_pin;
    bool connect_from_output;
    
    bool selecting;
    i32 selection_start_x, selection_start_y;
    
    // Context menu
    bool show_context_menu;
    i32 context_x, context_y;
    char search_buffer[64];
    bool search_active;
    
    // Clipboard
    struct {
        i32 node_ids[256];
        i32 node_count;
        node_t nodes[256];
    } clipboard;
    
    // Undo/Redo
    struct {
        enum {
            UNDO_CREATE_NODE,
            UNDO_DELETE_NODE,
            UNDO_MOVE_NODE,
            UNDO_CONNECT,
            UNDO_DISCONNECT
        } type;
        
        union {
            struct {
                node_t node;
                i32 node_id;
            } node_data;
            
            struct {
                node_connection_t connection;
            } connection_data;
            
            struct {
                i32 node_id;
                f32 old_x, old_y;
                f32 new_x, new_y;
            } move_data;
        };
    } undo_stack[256], redo_stack[256];
    i32 undo_count, redo_count;
    
    // Property inspector
    bool show_inspector;
    i32 inspected_node_id;
    
    // Performance
    bool show_performance;
    node_performance_stats_t perf_stats;
    
} editor_state_t;

static editor_state_t g_editor;

// Forward declarations
static void show_context_menu(gui_context *gui);
static void show_node_search(gui_context *gui);
static void show_inspector(gui_context *gui);
static void add_undo_action(i32 type, void *data);
static void perform_undo(void);
static void perform_redo(void);

// Initialize editor
void node_editor_init(node_graph_t *graph, gui_context *gui) {
    memset(&g_editor, 0, sizeof(g_editor));
    g_editor.graph = graph;
    g_editor.gui = gui;
    g_editor.inspected_node_id = -1;
}

// Handle keyboard shortcuts
static void handle_shortcuts(gui_context *gui) {
    // Copy (Ctrl+C)
    if (gui->key_pressed['C'] && gui->key_pressed[0x11]) {  // 0x11 = Ctrl
        g_editor.clipboard.node_count = 0;
        
        for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
            i32 node_id = g_editor.graph->selected_nodes[i];
            node_t *node = node_find_by_id(g_editor.graph, node_id);
            if (node) {
                g_editor.clipboard.node_ids[g_editor.clipboard.node_count] = node_id;
                g_editor.clipboard.nodes[g_editor.clipboard.node_count] = *node;
                g_editor.clipboard.node_count++;
            }
        }
    }
    
    // Paste (Ctrl+V)
    if (gui->key_pressed['V'] && gui->key_pressed[0x11]) {
        f32 paste_x = (f32)gui->mouse_x / g_editor.graph->view_zoom + g_editor.graph->view_x;
        f32 paste_y = (f32)gui->mouse_y / g_editor.graph->view_zoom + g_editor.graph->view_y;
        
        // Clear selection
        g_editor.graph->selected_count = 0;
        
        // Paste nodes with offset
        for (i32 i = 0; i < g_editor.clipboard.node_count; ++i) {
            node_t *template = &g_editor.clipboard.nodes[i];
            node_t *new_node = node_create(g_editor.graph, template->type_id,
                                          paste_x + template->x - g_editor.clipboard.nodes[0].x,
                                          paste_y + template->y - g_editor.clipboard.nodes[0].y);
            if (new_node) {
                // Copy custom data
                memcpy(new_node->custom_data, template->custom_data, sizeof(new_node->custom_data));
                
                // Add to selection
                g_editor.graph->selected_nodes[g_editor.graph->selected_count++] = new_node->id;
                new_node->selected = true;
            }
        }
    }
    
    // Delete (Delete key)
    if (gui->key_pressed[0x7F]) {  // Delete key
        for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
            i32 node_id = g_editor.graph->selected_nodes[i];
            node_t *node = node_find_by_id(g_editor.graph, node_id);
            if (node) {
                // Add to undo stack
                struct {
                    node_t node;
                    i32 node_id;
                } undo_data = { *node, node_id };
                add_undo_action(UNDO_DELETE_NODE, &undo_data);
                
                node_destroy(g_editor.graph, node);
            }
        }
        g_editor.graph->selected_count = 0;
    }
    
    // Undo (Ctrl+Z)
    if (gui->key_pressed['Z'] && gui->key_pressed[0x11]) {
        perform_undo();
    }
    
    // Redo (Ctrl+Y)
    if (gui->key_pressed['Y'] && gui->key_pressed[0x11]) {
        perform_redo();
    }
    
    // Select All (Ctrl+A)
    if (gui->key_pressed['A'] && gui->key_pressed[0x11]) {
        g_editor.graph->selected_count = 0;
        for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
            if (g_editor.graph->nodes[i].type) {
                g_editor.graph->selected_nodes[g_editor.graph->selected_count++] = i;
                g_editor.graph->nodes[i].selected = true;
            }
        }
    }
    
    // Quick Add (Q)
    if (gui->key_pressed['Q']) {
        g_editor.show_context_menu = true;
        g_editor.context_x = gui->mouse_x;
        g_editor.context_y = gui->mouse_y;
        g_editor.search_active = true;
    }
}

// Show context menu
static void show_context_menu(gui_context *gui) {
    if (!g_editor.show_context_menu) return;
    
    // Background panel
    gui_begin_panel(gui, g_editor.context_x, g_editor.context_y, 200, 300, "Add Node");
    
    // Search box
    if (g_editor.search_active) {
        gui_text_input(gui, g_editor.search_buffer, sizeof(g_editor.search_buffer));
    }
    
    // Category filters
    static bool show_categories[NODE_CATEGORY_COUNT] = {
        true, true, true, true, true, true, true, true, true
    };
    
    gui_begin_layout(gui, LAYOUT_VERTICAL, 5);
    
    // List node types
    extern node_registry_t *get_registry(void);  // Access to global registry
    node_registry_t *registry = get_registry();
    
    for (i32 i = 0; i < registry->type_count; ++i) {
        node_type_t *type = &registry->types[i];
        
        // Filter by search
        if (g_editor.search_buffer[0] != '\0') {
            // Case-insensitive search
            char lower_name[64];
            strcpy(lower_name, type->name);
            for (char *p = lower_name; *p; ++p) *p = tolower(*p);
            
            char lower_search[64];
            strcpy(lower_search, g_editor.search_buffer);
            for (char *p = lower_search; *p; ++p) *p = tolower(*p);
            
            if (strstr(lower_name, lower_search) == NULL) {
                continue;
            }
        }
        
        // Filter by category
        if (!show_categories[type->category]) {
            continue;
        }
        
        // Show node type button
        if (gui_button(gui, type->name)) {
            // Create node at context menu position
            f32 world_x = (f32)g_editor.context_x / g_editor.graph->view_zoom + g_editor.graph->view_x;
            f32 world_y = (f32)g_editor.context_y / g_editor.graph->view_zoom + g_editor.graph->view_y;
            
            node_t *new_node = node_create(g_editor.graph, i, world_x, world_y);
            if (new_node) {
                // Add to undo stack
                struct {
                    node_t node;
                    i32 node_id;
                } undo_data = { *new_node, new_node->id };
                add_undo_action(UNDO_CREATE_NODE, &undo_data);
            }
            
            // Close menu
            g_editor.show_context_menu = false;
            g_editor.search_active = false;
            g_editor.search_buffer[0] = '\0';
        }
    }
    
    gui_end_layout(gui);
    gui_end_panel(gui);
    
    // Close menu if clicked outside
    if (gui->mouse_clicked) {
        if (gui->mouse_x < g_editor.context_x || 
            gui->mouse_x > g_editor.context_x + 200 ||
            gui->mouse_y < g_editor.context_y ||
            gui->mouse_y > g_editor.context_y + 300) {
            g_editor.show_context_menu = false;
            g_editor.search_active = false;
            g_editor.search_buffer[0] = '\0';
        }
    }
}

// Show property inspector
static void show_inspector(gui_context *gui) {
    if (!g_editor.show_inspector || g_editor.inspected_node_id < 0) return;
    
    node_t *node = node_find_by_id(g_editor.graph, g_editor.inspected_node_id);
    if (!node || !node->type) return;
    
    // Inspector panel
    i32 panel_width = 250;
    i32 panel_x = gui->platform->window_width - panel_width - 10;
    
    gui_begin_panel(gui, panel_x, 10, panel_width, 400, "Inspector");
    gui_begin_layout(gui, LAYOUT_VERTICAL, 10);
    
    // Node info
    gui_text(gui, "Node: %s", node->type->name);
    gui_text(gui, "ID: %d", node->id);
    gui_text(gui, "Position: %.0f, %.0f", node->x, node->y);
    
    gui_separator(gui);
    
    // Properties
    if (node->type->property_count > 0) {
        gui_label(gui, "Properties:");
        
        for (i32 i = 0; i < node->type->property_count; ++i) {
            void *prop_ptr = (u8*)node->custom_data + node->type->properties[i].offset;
            
            switch (node->type->properties[i].type) {
                case PIN_TYPE_FLOAT: {
                    f32 *value = (f32*)prop_ptr;
                    gui_slider(gui, node->type->properties[i].name, value, -100.0f, 100.0f);
                    break;
                }
                case PIN_TYPE_INT: {
                    i32 *value = (i32*)prop_ptr;
                    gui_slider_int(gui, node->type->properties[i].name, value, -100, 100);
                    break;
                }
                case PIN_TYPE_BOOL: {
                    bool *value = (bool*)prop_ptr;
                    gui_checkbox(gui, node->type->properties[i].name, value);
                    break;
                }
                default:
                    gui_text(gui, "%s: [unsupported type]", node->type->properties[i].name);
                    break;
            }
        }
    }
    
    gui_separator(gui);
    
    // Performance stats
    if (node->execution_count > 0) {
        gui_label(gui, "Performance:");
        f32 avg_us = (f32)(node->last_execution_cycles / 3000.0);  // Assuming 3GHz
        gui_text(gui, "Last: %.2f us", avg_us);
        gui_text(gui, "Executions: %d", node->execution_count);
    }
    
    gui_separator(gui);
    
    // Debug
    if (gui_checkbox(gui, "Breakpoint", &node->has_breakpoint)) {
        // Breakpoint toggled
    }
    
    if (node->debug_message[0] != '\0') {
        gui_label(gui, "Debug Output:");
        gui_text(gui, "%s", node->debug_message);
    }
    
    gui_end_layout(gui);
    gui_end_panel(gui);
}

// Alignment tools
static void align_nodes_horizontal(void) {
    if (g_editor.graph->selected_count < 2) return;
    
    // Find average Y position
    f32 avg_y = 0.0f;
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[i]);
        if (node) avg_y += node->y;
    }
    avg_y /= g_editor.graph->selected_count;
    
    // Align all selected nodes
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[i]);
        if (node) {
            // Add to undo
            struct {
                i32 node_id;
                f32 old_x, old_y;
                f32 new_x, new_y;
            } undo_data = { node->id, node->x, node->y, node->x, avg_y };
            add_undo_action(UNDO_MOVE_NODE, &undo_data);
            
            node->y = avg_y;
        }
    }
}

static void align_nodes_vertical(void) {
    if (g_editor.graph->selected_count < 2) return;
    
    // Find average X position
    f32 avg_x = 0.0f;
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[i]);
        if (node) avg_x += node->x;
    }
    avg_x /= g_editor.graph->selected_count;
    
    // Align all selected nodes
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[i]);
        if (node) {
            // Add to undo
            struct {
                i32 node_id;
                f32 old_x, old_y;
                f32 new_x, new_y;
            } undo_data = { node->id, node->x, node->y, avg_x, node->y };
            add_undo_action(UNDO_MOVE_NODE, &undo_data);
            
            node->x = avg_x;
        }
    }
}

static void distribute_nodes_horizontal(void) {
    if (g_editor.graph->selected_count < 3) return;
    
    // Find min and max X
    f32 min_x = 1e9f, max_x = -1e9f;
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[i]);
        if (node) {
            if (node->x < min_x) min_x = node->x;
            if (node->x > max_x) max_x = node->x;
        }
    }
    
    // Distribute evenly
    f32 spacing = (max_x - min_x) / (g_editor.graph->selected_count - 1);
    
    // Sort nodes by X position
    // Simple bubble sort for small selection
    for (i32 i = 0; i < g_editor.graph->selected_count - 1; ++i) {
        for (i32 j = 0; j < g_editor.graph->selected_count - i - 1; ++j) {
            node_t *a = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[j]);
            node_t *b = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[j + 1]);
            if (a && b && a->x > b->x) {
                i32 temp = g_editor.graph->selected_nodes[j];
                g_editor.graph->selected_nodes[j] = g_editor.graph->selected_nodes[j + 1];
                g_editor.graph->selected_nodes[j + 1] = temp;
            }
        }
    }
    
    // Apply distribution
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.graph->selected_nodes[i]);
        if (node) {
            f32 new_x = min_x + i * spacing;
            
            // Add to undo
            struct {
                i32 node_id;
                f32 old_x, old_y;
                f32 new_x, new_y;
            } undo_data = { node->id, node->x, node->y, new_x, node->y };
            add_undo_action(UNDO_MOVE_NODE, &undo_data);
            
            node->x = new_x;
        }
    }
}

// Undo/Redo system
static void add_undo_action(i32 type, void *data) {
    if (g_editor.undo_count >= 256) {
        // Shift stack
        for (i32 i = 0; i < 255; ++i) {
            g_editor.undo_stack[i] = g_editor.undo_stack[i + 1];
        }
        g_editor.undo_count = 255;
    }
    
    g_editor.undo_stack[g_editor.undo_count].type = type;
    
    switch (type) {
        case UNDO_CREATE_NODE:
        case UNDO_DELETE_NODE:
            memcpy(&g_editor.undo_stack[g_editor.undo_count].node_data, data,
                   sizeof(g_editor.undo_stack[0].node_data));
            break;
        case UNDO_CONNECT:
        case UNDO_DISCONNECT:
            memcpy(&g_editor.undo_stack[g_editor.undo_count].connection_data, data,
                   sizeof(g_editor.undo_stack[0].connection_data));
            break;
        case UNDO_MOVE_NODE:
            memcpy(&g_editor.undo_stack[g_editor.undo_count].move_data, data,
                   sizeof(g_editor.undo_stack[0].move_data));
            break;
    }
    
    g_editor.undo_count++;
    
    // Clear redo stack
    g_editor.redo_count = 0;
}

static void perform_undo(void) {
    if (g_editor.undo_count == 0) return;
    
    g_editor.undo_count--;
    
    // Copy to redo stack
    g_editor.redo_stack[g_editor.redo_count++] = g_editor.undo_stack[g_editor.undo_count];
    
    // Perform undo
    switch (g_editor.undo_stack[g_editor.undo_count].type) {
        case UNDO_CREATE_NODE: {
            // Delete the created node
            i32 node_id = g_editor.undo_stack[g_editor.undo_count].node_data.node_id;
            node_t *node = node_find_by_id(g_editor.graph, node_id);
            if (node) {
                node_destroy(g_editor.graph, node);
            }
            break;
        }
        case UNDO_DELETE_NODE: {
            // Recreate the deleted node
            node_t *template = &g_editor.undo_stack[g_editor.undo_count].node_data.node;
            node_t *new_node = node_create(g_editor.graph, template->type_id,
                                          template->x, template->y);
            if (new_node) {
                *new_node = *template;  // Restore all data
            }
            break;
        }
        case UNDO_MOVE_NODE: {
            // Move node back to old position
            i32 node_id = g_editor.undo_stack[g_editor.undo_count].move_data.node_id;
            node_t *node = node_find_by_id(g_editor.graph, node_id);
            if (node) {
                node->x = g_editor.undo_stack[g_editor.undo_count].move_data.old_x;
                node->y = g_editor.undo_stack[g_editor.undo_count].move_data.old_y;
            }
            break;
        }
        // TODO: Handle connection undo
    }
}

static void perform_redo(void) {
    if (g_editor.redo_count == 0) return;
    
    g_editor.redo_count--;
    
    // Copy back to undo stack
    g_editor.undo_stack[g_editor.undo_count++] = g_editor.redo_stack[g_editor.redo_count];
    
    // Perform redo
    switch (g_editor.redo_stack[g_editor.redo_count].type) {
        case UNDO_CREATE_NODE: {
            // Recreate the node
            node_t *template = &g_editor.redo_stack[g_editor.redo_count].node_data.node;
            node_t *new_node = node_create(g_editor.graph, template->type_id,
                                          template->x, template->y);
            if (new_node) {
                *new_node = *template;
            }
            break;
        }
        case UNDO_DELETE_NODE: {
            // Delete the node again
            i32 node_id = g_editor.redo_stack[g_editor.redo_count].node_data.node_id;
            node_t *node = node_find_by_id(g_editor.graph, node_id);
            if (node) {
                node_destroy(g_editor.graph, node);
            }
            break;
        }
        case UNDO_MOVE_NODE: {
            // Move node to new position
            i32 node_id = g_editor.redo_stack[g_editor.redo_count].move_data.node_id;
            node_t *node = node_find_by_id(g_editor.graph, node_id);
            if (node) {
                node->x = g_editor.redo_stack[g_editor.redo_count].move_data.new_x;
                node->y = g_editor.redo_stack[g_editor.redo_count].move_data.new_y;
            }
            break;
        }
    }
}

// Main editor update
void node_editor_update(gui_context *gui) {
    if (!g_editor.graph || !gui) return;
    
    // Handle shortcuts
    handle_shortcuts(gui);
    
    // Handle mouse input for graph interaction
    extern void node_graph_handle_mouse(node_graph_t *graph, i32 mouse_x, i32 mouse_y, 
                                        bool mouse_down, i32 mouse_wheel);
    node_graph_handle_mouse(g_editor.graph, gui->mouse_x, gui->mouse_y,
                           gui->mouse_down, gui->mouse_wheel);
    
    // Handle node selection and dragging
    if (gui->mouse_clicked) {
        // Check if clicking on a node
        extern node_t* node_at_position(node_graph_t *graph, i32 screen_x, i32 screen_y);
        node_t *clicked_node = node_at_position(g_editor.graph, gui->mouse_x, gui->mouse_y);
        
        if (clicked_node) {
            // Check for pin click
            extern node_pin_t* pin_at_position(node_graph_t *graph, node_t *node, 
                                              i32 screen_x, i32 screen_y,
                                              bool *is_output, i32 *pin_index);
            bool is_output;
            i32 pin_index;
            node_pin_t *clicked_pin = pin_at_position(g_editor.graph, clicked_node,
                                                      gui->mouse_x, gui->mouse_y,
                                                      &is_output, &pin_index);
            
            if (clicked_pin) {
                // Start connection
                g_editor.connecting = true;
                g_editor.connect_from_node = clicked_node->id;
                g_editor.connect_from_pin = pin_index;
                g_editor.connect_from_output = is_output;
            } else {
                // Start dragging node
                g_editor.dragging_node = true;
                g_editor.dragged_node_id = clicked_node->id;
                
                f32 world_x = (f32)gui->mouse_x / g_editor.graph->view_zoom + g_editor.graph->view_x;
                f32 world_y = (f32)gui->mouse_y / g_editor.graph->view_zoom + g_editor.graph->view_y;
                
                g_editor.drag_offset_x = world_x - clicked_node->x;
                g_editor.drag_offset_y = world_y - clicked_node->y;
                
                // Update selection
                if (!gui->key_pressed[0x10]) {  // Not holding Shift
                    // Clear selection if not clicking on already selected node
                    if (!clicked_node->selected) {
                        for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
                            node_t *sel_node = node_find_by_id(g_editor.graph,
                                                              g_editor.graph->selected_nodes[i]);
                            if (sel_node) sel_node->selected = false;
                        }
                        g_editor.graph->selected_count = 0;
                    }
                }
                
                // Add to selection
                if (!clicked_node->selected) {
                    clicked_node->selected = true;
                    g_editor.graph->selected_nodes[g_editor.graph->selected_count++] = clicked_node->id;
                }
                
                // Set as inspected node
                g_editor.inspected_node_id = clicked_node->id;
                g_editor.show_inspector = true;
            }
        } else {
            // Clicked on empty space
            if (!gui->key_pressed[0x10]) {  // Not holding Shift
                // Clear selection
                for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
                    node_t *node = node_find_by_id(g_editor.graph,
                                                   g_editor.graph->selected_nodes[i]);
                    if (node) node->selected = false;
                }
                g_editor.graph->selected_count = 0;
            }
            
            // Start box selection
            g_editor.selecting = true;
            g_editor.selection_start_x = gui->mouse_x;
            g_editor.selection_start_y = gui->mouse_y;
            g_editor.graph->selection_rect.x0 = gui->mouse_x;
            g_editor.graph->selection_rect.y0 = gui->mouse_y;
            g_editor.graph->is_selecting = true;
        }
    }
    
    // Handle dragging
    if (g_editor.dragging_node && gui->mouse_down) {
        node_t *node = node_find_by_id(g_editor.graph, g_editor.dragged_node_id);
        if (node) {
            f32 world_x = (f32)gui->mouse_x / g_editor.graph->view_zoom + g_editor.graph->view_x;
            f32 world_y = (f32)gui->mouse_y / g_editor.graph->view_zoom + g_editor.graph->view_y;
            
            f32 new_x = world_x - g_editor.drag_offset_x;
            f32 new_y = world_y - g_editor.drag_offset_y;
            
            // Snap to grid
            if (gui->key_pressed[0x11]) {  // Ctrl for snap
                new_x = roundf(new_x / 20.0f) * 20.0f;
                new_y = roundf(new_y / 20.0f) * 20.0f;
            }
            
            // Move all selected nodes
            f32 dx = new_x - node->x;
            f32 dy = new_y - node->y;
            
            for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
                node_t *sel_node = node_find_by_id(g_editor.graph,
                                                   g_editor.graph->selected_nodes[i]);
                if (sel_node) {
                    sel_node->x += dx;
                    sel_node->y += dy;
                }
            }
        }
    }
    
    // Handle connection
    if (g_editor.connecting && !gui->mouse_down) {
        // Try to complete connection
        extern node_t* node_at_position(node_graph_t *graph, i32 screen_x, i32 screen_y);
        node_t *target_node = node_at_position(g_editor.graph, gui->mouse_x, gui->mouse_y);
        
        if (target_node) {
            extern node_pin_t* pin_at_position(node_graph_t *graph, node_t *node,
                                              i32 screen_x, i32 screen_y,
                                              bool *is_output, i32 *pin_index);
            bool is_output;
            i32 pin_index;
            node_pin_t *target_pin = pin_at_position(g_editor.graph, target_node,
                                                     gui->mouse_x, gui->mouse_y,
                                                     &is_output, &pin_index);
            
            if (target_pin && is_output != g_editor.connect_from_output) {
                // Can connect input to output
                if (g_editor.connect_from_output) {
                    // From output to input
                    node_connection_t *conn = node_connect(g_editor.graph,
                                                          g_editor.connect_from_node,
                                                          g_editor.connect_from_pin,
                                                          target_node->id,
                                                          pin_index);
                    if (conn) {
                        // Add to undo
                        add_undo_action(UNDO_CONNECT, conn);
                    }
                } else {
                    // From input to output
                    node_connection_t *conn = node_connect(g_editor.graph,
                                                          target_node->id,
                                                          pin_index,
                                                          g_editor.connect_from_node,
                                                          g_editor.connect_from_pin);
                    if (conn) {
                        // Add to undo
                        add_undo_action(UNDO_CONNECT, conn);
                    }
                }
            }
        }
        
        g_editor.connecting = false;
    }
    
    // Handle box selection
    if (g_editor.selecting) {
        if (gui->mouse_down) {
            // Update selection rect
            g_editor.graph->selection_rect.x1 = gui->mouse_x;
            g_editor.graph->selection_rect.y1 = gui->mouse_y;
            
            // Normalize rect
            rect *r = &g_editor.graph->selection_rect;
            if (r->x1 < r->x0) {
                i32 temp = r->x0;
                r->x0 = r->x1;
                r->x1 = temp;
            }
            if (r->y1 < r->y0) {
                i32 temp = r->y0;
                r->y0 = r->y1;
                r->y1 = temp;
            }
        } else {
            // Complete selection
            g_editor.selecting = false;
            g_editor.graph->is_selecting = false;
            
            // Select nodes in rectangle
            for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
                if (g_editor.graph->nodes[i].type) {
                    node_t *node = &g_editor.graph->nodes[i];
                    
                    // Convert node position to screen
                    i32 sx = (i32)((node->x - g_editor.graph->view_x) * g_editor.graph->view_zoom);
                    i32 sy = (i32)((node->y - g_editor.graph->view_y) * g_editor.graph->view_zoom);
                    
                    if (sx >= g_editor.graph->selection_rect.x0 &&
                        sx <= g_editor.graph->selection_rect.x1 &&
                        sy >= g_editor.graph->selection_rect.y0 &&
                        sy <= g_editor.graph->selection_rect.y1) {
                        
                        if (!node->selected) {
                            node->selected = true;
                            g_editor.graph->selected_nodes[g_editor.graph->selected_count++] = i;
                        }
                    }
                }
            }
        }
    }
    
    // Release states
    if (!gui->mouse_down) {
        g_editor.dragging_node = false;
        g_editor.connecting = false;
    }
    
    // Right click for context menu
    if (gui->mouse_buttons[1]) {  // Right mouse button
        g_editor.show_context_menu = true;
        g_editor.context_x = gui->mouse_x;
        g_editor.context_y = gui->mouse_y;
    }
    
    // Show UI elements
    show_context_menu(gui);
    show_inspector(gui);
    
    // Toolbar
    gui_begin_panel(gui, 10, 10, 800, 40, NULL);
    gui_begin_layout(gui, LAYOUT_HORIZONTAL, 10);
    
    if (gui_button(gui, "Align H")) align_nodes_horizontal();
    if (gui_button(gui, "Align V")) align_nodes_vertical();
    if (gui_button(gui, "Distribute")) distribute_nodes_horizontal();
    gui_separator(gui);
    if (gui_button(gui, "Compile")) node_graph_compile(g_editor.graph);
    if (gui_button(gui, "Execute")) {
        node_execution_context_t ctx = {0};
        executor_execute_graph(g_editor.graph, &ctx);
    }
    gui_separator(gui);
    gui_checkbox(gui, "Performance", &g_editor.show_performance);
    gui_checkbox(gui, "Inspector", &g_editor.show_inspector);
    
    gui_end_layout(gui);
    gui_end_panel(gui);
}

// Comment box support
typedef struct {
    f32 x, y, width, height;
    char text[256];
    u32 color;
} comment_box_t;

static comment_box_t g_comments[32];
static i32 g_comment_count = 0;

void node_editor_add_comment(f32 x, f32 y, f32 width, f32 height, const char *text) {
    if (g_comment_count >= 32) return;
    
    comment_box_t *comment = &g_comments[g_comment_count++];
    comment->x = x;
    comment->y = y;
    comment->width = width;
    comment->height = height;
    strncpy(comment->text, text, 255);
    comment->color = 0x40FFFF00;  // Semi-transparent yellow
}

// Subgraph support
void node_editor_create_subgraph(const char *name) {
    // Create new graph for subgraph
    node_graph_t *subgraph = node_graph_create(name);
    if (!subgraph) return;
    
    // Move selected nodes to subgraph
    for (i32 i = 0; i < g_editor.graph->selected_count; ++i) {
        // TODO: Move nodes to subgraph
    }
    
    // Create subgraph node in main graph
    // TODO: Create special subgraph node type
}