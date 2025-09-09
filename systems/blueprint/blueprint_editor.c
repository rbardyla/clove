// blueprint_editor.c - Visual blueprint editor using GUI system
// PERFORMANCE: Immediate mode rendering with efficient batching
// FEATURES: Node palette, visual connections, drag & drop, undo/redo

#include "handmade_blueprint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations
void blueprint_init_standard_nodes(blueprint_context* ctx);
blueprint_node* blueprint_create_node_from_template(blueprint_graph* graph, blueprint_context* ctx, 
                                                   node_type type, v2 position);

// ============================================================================
// VISUAL CONSTANTS
// ============================================================================

#define GRID_SIZE 20.0f
#define GRID_MAJOR_SIZE 100.0f
#define NODE_MIN_WIDTH 120.0f
#define NODE_MIN_HEIGHT 60.0f
#define PIN_RADIUS 6.0f
#define PIN_SPACING 20.0f
#define CONNECTION_THICKNESS 3.0f
#define SELECTION_THICKNESS 2.0f

// ============================================================================
// COORDINATE TRANSFORMATION
// ============================================================================

// Convert world coordinates to screen coordinates
static v2 world_to_screen(blueprint_context* ctx, v2 world_pos) {
    blueprint_graph* graph = ctx->active_graph;
    if (!graph) return world_pos;
    
    v2 screen_pos;
    screen_pos.x = (world_pos.x + graph->view_offset.x) * graph->view_scale;
    screen_pos.y = (world_pos.y + graph->view_offset.y) * graph->view_scale;
    return screen_pos;
}

// Convert screen coordinates to world coordinates
static v2 screen_to_world(blueprint_context* ctx, v2 screen_pos) {
    blueprint_graph* graph = ctx->active_graph;
    if (!graph) return screen_pos;
    
    v2 world_pos;
    world_pos.x = (screen_pos.x / graph->view_scale) - graph->view_offset.x;
    world_pos.y = (screen_pos.y / graph->view_scale) - graph->view_offset.y;
    return world_pos;
}

// ============================================================================
// GRID RENDERING
// ============================================================================

static void render_grid(blueprint_context* ctx) {
    if (!ctx->active_graph) return;
    
    gui_context* gui = ctx->gui;
    blueprint_graph* graph = ctx->active_graph;
    
    v2 canvas_min = gui_get_content_region_avail(gui);
    v2 canvas_max = gui_get_content_region_max(gui);
    
    color32 grid_color = ctx->gui->theme.graph_grid;
    color32 major_grid_color = gui->theme.border;
    
    f32 grid_step = GRID_SIZE * graph->view_scale;
    f32 major_grid_step = GRID_MAJOR_SIZE * graph->view_scale;
    
    // Only draw grid if it's not too small
    if (grid_step > 4.0f) {
        // Calculate grid offset based on view
        f32 grid_offset_x = fmodf(graph->view_offset.x * graph->view_scale, grid_step);
        f32 grid_offset_y = fmodf(graph->view_offset.y * graph->view_scale, grid_step);
        
        // Vertical grid lines
        for (f32 x = canvas_min.x + grid_offset_x; x < canvas_max.x; x += grid_step) {
            b32 is_major = (fmodf(x - grid_offset_x, major_grid_step) < 0.1f);
            color32 color = is_major ? major_grid_color : grid_color;
            gui_draw_line(gui, (v2){x, canvas_min.y}, (v2){x, canvas_max.y}, color, 1.0f);
        }
        
        // Horizontal grid lines
        for (f32 y = canvas_min.y + grid_offset_y; y < canvas_max.y; y += grid_step) {
            b32 is_major = (fmodf(y - grid_offset_y, major_grid_step) < 0.1f);
            color32 color = is_major ? major_grid_color : grid_color;
            gui_draw_line(gui, (v2){canvas_min.x, y}, (v2){canvas_max.x, y}, color, 1.0f);
        }
    }
}

// ============================================================================
// NODE RENDERING
// ============================================================================

static v2 get_pin_position(blueprint_node* node, blueprint_pin* pin) {
    v2 pin_pos = node->position;
    
    // Calculate pin position based on direction and index
    if (pin->direction == PIN_INPUT) {
        pin_pos.x -= PIN_RADIUS;
        pin_pos.y += 30.0f; // Offset from top
        
        // Find pin index
        for (u32 i = 0; i < node->input_pin_count; i++) {
            if (&node->input_pins[i] == pin) {
                pin_pos.y += i * PIN_SPACING;
                break;
            }
        }
    } else {
        pin_pos.x += node->size.x + PIN_RADIUS;
        pin_pos.y += 30.0f;
        
        // Find pin index
        for (u32 i = 0; i < node->output_pin_count; i++) {
            if (&node->output_pins[i] == pin) {
                pin_pos.y += i * PIN_SPACING;
                break;
            }
        }
    }
    
    return pin_pos;
}

static void render_pin(blueprint_context* ctx, blueprint_node* node, blueprint_pin* pin) {
    gui_context* gui = ctx->gui;
    
    v2 world_pos = get_pin_position(node, pin);
    v2 screen_pos = world_to_screen(ctx, world_pos);
    
    // Pin color based on type
    color32 pin_color = 0xFFFFFFFF; // Default white
    switch (pin->type) {
        case BP_TYPE_EXEC: pin_color = 0xFFFFFFFF; break;      // White for execution
        case BP_TYPE_BOOL: pin_color = 0xFF8B0000; break;      // Dark red for bool
        case BP_TYPE_INT: pin_color = 0xFF00CED1; break;       // Dark turquoise for int
        case BP_TYPE_FLOAT: pin_color = 0xFF9ACD32; break;     // Yellow green for float
        case BP_TYPE_VEC3: pin_color = 0xFFFFD700; break;      // Gold for vectors
        case BP_TYPE_STRING: pin_color = 0xFFFF1493; break;    // Deep pink for string
        default: break;
    }
    
    // Draw pin circle
    gui_draw_circle_filled(gui, screen_pos, PIN_RADIUS, pin_color, 12);
    
    // Draw pin border
    color32 border_color = pin->has_connection ? 0xFFFFFFFF : 0xFF808080;
    gui_draw_circle(gui, screen_pos, PIN_RADIUS, border_color, 12, 1.0f);
    
    // Draw pin label
    if (pin->direction == PIN_INPUT) {
        v2 text_pos = (v2){screen_pos.x + PIN_RADIUS + 5, screen_pos.y - 8};
        gui_draw_text(gui, text_pos, 0xFFFFFFFF, pin->name, NULL);
    } else {
        // Right-aligned for output pins
        v2 text_pos = (v2){screen_pos.x - PIN_RADIUS - 50, screen_pos.y - 8};
        gui_draw_text(gui, text_pos, 0xFFFFFFFF, pin->name, NULL);
    }
}

static void render_node(blueprint_context* ctx, blueprint_node* node) {
    gui_context* gui = ctx->gui;
    
    v2 world_min = node->position;
    v2 world_max = (v2){node->position.x + node->size.x, node->position.y + node->size.y};
    
    v2 screen_min = world_to_screen(ctx, world_min);
    v2 screen_max = world_to_screen(ctx, world_max);
    
    // Adjust node height based on pin count
    u32 max_pins = (node->input_pin_count > node->output_pin_count) ? 
                   node->input_pin_count : node->output_pin_count;
    f32 min_height = 40.0f + max_pins * PIN_SPACING;
    if (screen_max.y - screen_min.y < min_height) {
        screen_max.y = screen_min.y + min_height;
        node->size.y = min_height;
    }
    
    // Node background
    color32 node_color = node->color;
    if (node->flags & NODE_FLAG_SELECTED) {
        // Lighten selected nodes
        node_color = 0xFF606060;
    }
    if (node->flags & NODE_FLAG_ERROR) {
        // Error nodes are red
        node_color = 0xFF8B0000;
    }
    
    gui_draw_rect_filled(gui, screen_min, screen_max, node_color, node->rounding);
    
    // Node border
    color32 border_color = (node->flags & NODE_FLAG_SELECTED) ? 0xFFFFFFFF : 0xFF404040;
    f32 border_thickness = (node->flags & NODE_FLAG_SELECTED) ? SELECTION_THICKNESS : 1.0f;
    gui_draw_rect(gui, screen_min, screen_max, border_color, node->rounding, border_thickness);
    
    // Node title
    v2 title_pos = (v2){screen_min.x + 10, screen_min.y + 10};
    color32 title_color = 0xFFFFFFFF;
    gui_draw_text(gui, title_pos, title_color, node->display_name, NULL);
    
    // Breakpoint indicator
    if (node->flags & NODE_FLAG_BREAKPOINT) {
        v2 bp_pos = (v2){screen_max.x - 15, screen_min.y + 5};
        gui_draw_circle_filled(gui, bp_pos, 5.0f, 0xFFFF0000, 8);
    }
    
    // Render pins
    for (u32 i = 0; i < node->input_pin_count; i++) {
        render_pin(ctx, node, &node->input_pins[i]);
    }
    
    for (u32 i = 0; i < node->output_pin_count; i++) {
        render_pin(ctx, node, &node->output_pins[i]);
    }
    
    // Performance overlay if enabled
    if (ctx->editor.show_performance_overlay && node->execution_count > 0) {
        char perf_text[64];
        snprintf(perf_text, sizeof(perf_text), "%.2fms (%llu)", 
                node->avg_execution_time, node->execution_count);
        
        v2 perf_pos = (v2){screen_min.x + 10, screen_max.y - 20};
        gui_draw_text(gui, perf_pos, 0xFFFFFF00, perf_text, NULL);
    }
}

// ============================================================================
// CONNECTION RENDERING
// ============================================================================

static void render_connection(blueprint_context* ctx, blueprint_connection* conn) {
    gui_context* gui = ctx->gui;
    blueprint_graph* graph = ctx->active_graph;
    
    // Find source and destination nodes
    blueprint_node* from_node = blueprint_get_node(graph, conn->from_node);
    blueprint_node* to_node = blueprint_get_node(graph, conn->to_node);
    
    if (!from_node || !to_node) return;
    
    // Find pins
    blueprint_pin* from_pin = blueprint_get_pin(from_node, conn->from_pin);
    blueprint_pin* to_pin = blueprint_get_pin(to_node, conn->to_pin);
    
    if (!from_pin || !to_pin) return;
    
    // Get pin positions
    v2 from_world = get_pin_position(from_node, from_pin);
    v2 to_world = get_pin_position(to_node, to_pin);
    
    v2 from_screen = world_to_screen(ctx, from_world);
    v2 to_screen = world_to_screen(ctx, to_world);
    
    // Calculate bezier control points for smooth curves
    f32 distance = fabsf(to_screen.x - from_screen.x);
    f32 offset = distance * 0.5f;
    if (offset < 50.0f) offset = 50.0f;
    if (offset > 200.0f) offset = 200.0f;
    
    v2 control1 = (v2){from_screen.x + offset, from_screen.y};
    v2 control2 = (v2){to_screen.x - offset, to_screen.y};
    
    // Store control points for hit testing
    conn->control_points[0] = from_screen;
    conn->control_points[1] = control1;
    conn->control_points[2] = control2;
    conn->control_points[3] = to_screen;
    
    // Connection color based on pin type
    color32 connection_color = conn->color;
    switch (from_pin->type) {
        case BP_TYPE_EXEC: connection_color = 0xFFFFFFFF; break;
        case BP_TYPE_BOOL: connection_color = 0xFF8B0000; break;
        case BP_TYPE_INT: connection_color = 0xFF00CED1; break;
        case BP_TYPE_FLOAT: connection_color = 0xFF9ACD32; break;
        case BP_TYPE_VEC3: connection_color = 0xFFFFD700; break;
        default: connection_color = 0xFFAAAAAA; break;
    }
    
    // Highlight selected connections
    if (conn->is_selected) {
        connection_color = 0xFFFFFFFF;
    }
    
    // Draw bezier curve using line segments
    u32 segments = 20;
    for (u32 i = 0; i < segments; i++) {
        f32 t1 = (f32)i / (f32)segments;
        f32 t2 = (f32)(i + 1) / (f32)segments;
        
        v2 p1 = blueprint_bezier_curve(from_screen, control1, control2, to_screen, t1);
        v2 p2 = blueprint_bezier_curve(from_screen, control1, control2, to_screen, t2);
        
        f32 thickness = conn->is_selected ? conn->thickness + 1.0f : conn->thickness;
        gui_draw_line(gui, p1, p2, connection_color, thickness);
    }
    
    // Data flow visualization if enabled
    if (ctx->editor.show_data_flow) {
        // Animate a dot along the connection to show data flow
        f32 time = (f32)ctx->frame_start_time * 2.0f; // Animation speed
        f32 t = fmodf(time, 1.0f);
        
        v2 flow_pos = blueprint_bezier_curve(from_screen, control1, control2, to_screen, t);
        gui_draw_circle_filled(gui, flow_pos, 4.0f, 0xFFFFFF00, 8);
    }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

static blueprint_node* find_node_at_position(blueprint_context* ctx, v2 world_pos) {
    blueprint_graph* graph = ctx->active_graph;
    if (!graph) return NULL;
    
    for (u32 i = 0; i < graph->node_count; i++) {
        blueprint_node* node = &graph->nodes[i];
        
        v2 node_min = node->position;
        v2 node_max = (v2){node->position.x + node->size.x, node->position.y + node->size.y};
        
        if (blueprint_point_in_rect(world_pos, node_min, node_max)) {
            return node;
        }
    }
    
    return NULL;
}

static blueprint_pin* find_pin_at_position(blueprint_context* ctx, v2 world_pos, blueprint_node** out_node) {
    blueprint_graph* graph = ctx->active_graph;
    if (!graph) return NULL;
    
    for (u32 i = 0; i < graph->node_count; i++) {
        blueprint_node* node = &graph->nodes[i];
        
        // Check input pins
        for (u32 j = 0; j < node->input_pin_count; j++) {
            blueprint_pin* pin = &node->input_pins[j];
            v2 pin_world = get_pin_position(node, pin);
            
            f32 distance = sqrtf((world_pos.x - pin_world.x) * (world_pos.x - pin_world.x) + 
                                (world_pos.y - pin_world.y) * (world_pos.y - pin_world.y));
            
            if (distance <= PIN_RADIUS + 5.0f) {
                if (out_node) *out_node = node;
                return pin;
            }
        }
        
        // Check output pins
        for (u32 j = 0; j < node->output_pin_count; j++) {
            blueprint_pin* pin = &node->output_pins[j];
            v2 pin_world = get_pin_position(node, pin);
            
            f32 distance = sqrtf((world_pos.x - pin_world.x) * (world_pos.x - pin_world.x) + 
                                (world_pos.y - pin_world.y) * (world_pos.y - pin_world.y));
            
            if (distance <= PIN_RADIUS + 5.0f) {
                if (out_node) *out_node = node;
                return pin;
            }
        }
    }
    
    return NULL;
}

static void handle_node_dragging(blueprint_context* ctx) {
    gui_context* gui = ctx->gui;
    editor_state* editor = &ctx->editor;
    blueprint_graph* graph = ctx->active_graph;
    
    if (!graph) return;
    
    v2 mouse_pos = gui_get_mouse_pos(gui);
    v2 world_pos = screen_to_world(ctx, mouse_pos);
    
    if (gui_is_mouse_clicked(gui, 0)) { // Left mouse button
        // Check if clicking on a pin for connection
        blueprint_node* pin_node = NULL;
        blueprint_pin* clicked_pin = find_pin_at_position(ctx, world_pos, &pin_node);
        
        if (clicked_pin) {
            // Start connection
            editor->is_connecting = true;
            editor->connect_from_node = pin_node->id;
            editor->connect_from_pin = clicked_pin->id;
            editor->connect_preview_end = world_pos;
        } else {
            // Check if clicking on a node
            blueprint_node* clicked_node = find_node_at_position(ctx, world_pos);
            
            if (clicked_node) {
                // Start dragging
                editor->is_dragging = true;
                editor->drag_start = world_pos;
                editor->drag_offset = (v2){
                    world_pos.x - clicked_node->position.x,
                    world_pos.y - clicked_node->position.y
                };
                
                // Select node if not already selected
                if (!(clicked_node->flags & NODE_FLAG_SELECTED)) {
                    // Clear other selections if not holding Ctrl
                    if (!gui_is_key_down(gui, 341)) { // Left Ctrl
                        for (u32 i = 0; i < graph->node_count; i++) {
                            graph->nodes[i].flags &= ~NODE_FLAG_SELECTED;
                        }
                        editor->selected_node_count = 0;
                    }
                    
                    clicked_node->flags |= NODE_FLAG_SELECTED;
                    if (editor->selected_node_count < BLUEPRINT_MAX_NODES) {
                        editor->selected_nodes[editor->selected_node_count++] = clicked_node->id;
                    }
                }
            }
        }
    }
    
    // Handle mouse dragging
    if (gui_is_mouse_down(gui, 0)) {
        if (editor->is_connecting) {
            // Update connection preview
            editor->connect_preview_end = world_pos;
        } else if (editor->is_dragging) {
            // Move selected nodes
            v2 drag_delta = (v2){
                world_pos.x - editor->drag_start.x,
                world_pos.y - editor->drag_start.y
            };
            
            for (u32 i = 0; i < editor->selected_node_count; i++) {
                blueprint_node* node = blueprint_get_node(graph, editor->selected_nodes[i]);
                if (node) {
                    node->position.x = editor->drag_start.x + drag_delta.x - editor->drag_offset.x;
                    node->position.y = editor->drag_start.y + drag_delta.y - editor->drag_offset.y;
                    
                    // Update position in structure of arrays
                    for (u32 j = 0; j < graph->node_count; j++) {
                        if (graph->node_ids[j] == node->id) {
                            graph->node_positions[j] = node->position;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Handle mouse release
    if (gui_is_mouse_released(gui, 0)) {
        if (editor->is_connecting) {
            // Try to complete connection
            blueprint_node* target_node = NULL;
            blueprint_pin* target_pin = find_pin_at_position(ctx, world_pos, &target_node);
            
            if (target_pin && target_node) {
                // Find source pin
                blueprint_node* source_node = blueprint_get_node(graph, editor->connect_from_node);
                blueprint_pin* source_pin = blueprint_get_pin(source_node, editor->connect_from_pin);
                
                if (source_pin && blueprint_can_connect_pins(source_pin, target_pin)) {
                    // Create connection
                    connection_id conn_id = blueprint_create_connection(graph,
                        editor->connect_from_node, editor->connect_from_pin,
                        target_node->id, target_pin->id);
                    
                    if (conn_id != 0) {
                        blueprint_log_debug(ctx, "Created connection from %s to %s",
                                           source_pin->name, target_pin->name);
                        
                        // Mark pins as connected
                        source_pin->has_connection = true;
                        target_pin->has_connection = true;
                    }
                }
            }
            
            editor->is_connecting = false;
        }
        
        editor->is_dragging = false;
    }
}

static void handle_keyboard_input(blueprint_context* ctx) {
    gui_context* gui = ctx->gui;
    editor_state* editor = &ctx->editor;
    blueprint_graph* graph = ctx->active_graph;
    
    if (!graph) return;
    
    // Delete selected nodes
    if (gui_is_key_pressed(gui, 261)) { // Delete key
        blueprint_editor_delete_selected(ctx);
    }
    
    // Copy selected nodes
    if (gui_is_key_down(gui, 341) && gui_is_key_pressed(gui, 67)) { // Ctrl+C
        blueprint_editor_copy_selected(ctx);
    }
    
    // Paste nodes
    if (gui_is_key_down(gui, 341) && gui_is_key_pressed(gui, 86)) { // Ctrl+V
        blueprint_editor_paste(ctx);
    }
    
    // Select all
    if (gui_is_key_down(gui, 341) && gui_is_key_pressed(gui, 65)) { // Ctrl+A
        blueprint_editor_select_all(ctx);
    }
    
    // Toggle breakpoint on selected nodes
    if (gui_is_key_pressed(gui, 290)) { // F9
        for (u32 i = 0; i < editor->selected_node_count; i++) {
            blueprint_toggle_breakpoint(ctx, editor->selected_nodes[i]);
        }
    }
    
    // Show node palette
    if (gui_is_key_pressed(gui, 32)) { // Space
        editor->show_node_palette = !editor->show_node_palette;
    }
}

// ============================================================================
// MAIN EDITOR FUNCTIONS
// ============================================================================

void blueprint_editor_update(blueprint_context* ctx, f32 dt) {
    if (!ctx->active_graph) return;
    
    handle_node_dragging(ctx);
    handle_keyboard_input(ctx);
    
    // TODO: Update any animated elements
}

void blueprint_editor_render(blueprint_context* ctx) {
    gui_context* gui = ctx->gui;
    blueprint_graph* graph = ctx->active_graph;
    
    if (!graph) {
        gui_text(gui, "No active blueprint graph");
        return;
    }
    
    // Main editor window
    if (gui_begin_window(gui, "Blueprint Editor", NULL, 
                        GUI_WINDOW_NONE)) {
        
        // Toolbar
        if (gui_button(gui, "Compile")) {
            blueprint_compile_graph(ctx, graph);
        }
        gui_same_line(gui, 0);
        
        if (gui_button(gui, "Execute")) {
            blueprint_execute_graph(ctx, graph);
        }
        gui_same_line(gui, 0);
        
        if (gui_button(gui, "Fit to View")) {
            blueprint_fit_graph_to_view(ctx);
        }
        gui_same_line(gui, 0);
        
        gui_checkbox(gui, "Show Performance", &ctx->editor.show_performance_overlay);
        gui_same_line(gui, 0);
        gui_checkbox(gui, "Show Data Flow", &ctx->editor.show_data_flow);
        
        gui_separator(gui);
        
        // Graph canvas
        v2 canvas_size = gui_get_content_region_avail(gui);
        v2 canvas_pos = gui_get_cursor_pos(gui);
        
        // Draw background
        color32 bg_color = gui->theme.graph_bg;
        gui_draw_rect_filled(gui, canvas_pos, 
                           (v2){canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y}, 
                           bg_color, 0);
        
        // Render grid
        render_grid(ctx);
        
        // Render connections first (so they appear behind nodes)
        for (u32 i = 0; i < graph->connection_count; i++) {
            render_connection(ctx, &graph->connections[i]);
        }
        
        // Render connection preview
        editor_state* editor = &ctx->editor;
        if (editor->is_connecting) {
            // Find source pin
            blueprint_node* source_node = blueprint_get_node(graph, editor->connect_from_node);
            blueprint_pin* source_pin = blueprint_get_pin(source_node, editor->connect_from_pin);
            
            if (source_pin) {
                v2 from_world = get_pin_position(source_node, source_pin);
                v2 from_screen = world_to_screen(ctx, from_world);
                v2 to_screen = world_to_screen(ctx, editor->connect_preview_end);
                
                // Draw preview connection
                f32 distance = fabsf(to_screen.x - from_screen.x);
                f32 offset = distance * 0.5f;
                if (offset < 50.0f) offset = 50.0f;
                
                v2 control1 = (v2){from_screen.x + offset, from_screen.y};
                v2 control2 = (v2){to_screen.x - offset, to_screen.y};
                
                // Draw with dashed style
                for (u32 i = 0; i < 20; i++) {
                    f32 t1 = (f32)i / 20.0f;
                    f32 t2 = (f32)(i + 1) / 20.0f;
                    
                    v2 p1 = blueprint_bezier_curve(from_screen, control1, control2, to_screen, t1);
                    v2 p2 = blueprint_bezier_curve(from_screen, control1, control2, to_screen, t2);
                    
                    if (i % 2 == 0) { // Dashed effect
                        gui_draw_line(gui, p1, p2, 0xFFAAAAAA, 2.0f);
                    }
                }
            }
        }
        
        // Render nodes
        for (u32 i = 0; i < graph->node_count; i++) {
            render_node(ctx, &graph->nodes[i]);
        }
        
        // Selection rectangle
        if (graph->is_selecting) {
            v2 sel_min = world_to_screen(ctx, graph->selection_min);
            v2 sel_max = world_to_screen(ctx, graph->selection_max);
            
            gui_draw_rect(gui, sel_min, sel_max, 0xFFFFFFFF, 0, 1.0f);
            gui_draw_rect_filled(gui, sel_min, sel_max, 0x40FFFFFF, 0);
        }
        
        gui_end_window(gui);
    }
    
    // Show additional panels
    if (editor->show_node_palette) {
        blueprint_show_node_palette(ctx, &editor->show_node_palette);
    }
    
    // Always show graph outliner and properties for development
    b32 show_outliner = true;
    blueprint_show_graph_outliner(ctx, &show_outliner);
    
    b32 show_properties = true;
    blueprint_show_property_panel(ctx, &show_properties);
    
    if (ctx->debug_mode) {
        b32 show_debug = true;
        blueprint_show_debug_panel(ctx, &show_debug);
    }
}

// ============================================================================
// ADDITIONAL PANELS
// ============================================================================

void blueprint_show_node_palette(blueprint_context* ctx, b32* p_open) {
    gui_context* gui = ctx->gui;
    
    if (!ctx->node_templates) {
        blueprint_init_standard_nodes(ctx);
    }
    
    if (gui_begin_window(gui, "Node Palette", p_open, GUI_WINDOW_NONE)) {
        // Search filter
        gui_input_text(gui, "Search", ctx->editor.search_buffer, sizeof(ctx->editor.search_buffer));
        
        gui_separator(gui);
        
        // Node categories
        if (gui_tree_node(gui, "Events")) {
            if (gui_button(gui, "Begin Play")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {100, 100}; // TODO: Use mouse position
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_BEGIN_PLAY, spawn_pos);
                }
            }
            if (gui_button(gui, "Tick")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {100, 200};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_TICK, spawn_pos);
                }
            }
            gui_tree_pop(gui);
        }
        
        if (gui_tree_node(gui, "Flow Control")) {
            if (gui_button(gui, "Branch")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {300, 100};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_BRANCH, spawn_pos);
                }
            }
            if (gui_button(gui, "Sequence")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {300, 200};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_SEQUENCE, spawn_pos);
                }
            }
            gui_tree_pop(gui);
        }
        
        if (gui_tree_node(gui, "Math")) {
            if (gui_button(gui, "Add")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {500, 100};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_ADD, spawn_pos);
                }
            }
            if (gui_button(gui, "Multiply")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {500, 200};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_MULTIPLY, spawn_pos);
                }
            }
            gui_tree_pop(gui);
        }
        
        if (gui_tree_node(gui, "Debug")) {
            if (gui_button(gui, "Print")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {700, 100};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_PRINT, spawn_pos);
                }
            }
            if (gui_button(gui, "Breakpoint")) {
                if (ctx->active_graph) {
                    v2 spawn_pos = {700, 200};
                    blueprint_create_node_from_template(ctx->active_graph, ctx, 
                                                       NODE_TYPE_BREAKPOINT, spawn_pos);
                }
            }
            gui_tree_pop(gui);
        }
        
        gui_end_window(gui);
    }
}

void blueprint_show_graph_outliner(blueprint_context* ctx, b32* p_open) {
    gui_context* gui = ctx->gui;
    
    if (gui_begin_window(gui, "Graph Outliner", p_open, GUI_WINDOW_NONE)) {
        
        // Graph list
        gui_text(gui, "Graphs (%u):", ctx->graph_count);
        gui_separator(gui);
        
        for (u32 i = 0; i < ctx->graph_count; i++) {
            blueprint_graph* graph = &ctx->graphs[i];
            
            b32 is_active = (graph == ctx->active_graph);
            
            if (gui_selectable(gui, graph->name, is_active)) {
                blueprint_set_active_graph(ctx, graph);
            }
        }
        
        gui_separator(gui);
        
        if (gui_button(gui, "New Graph")) {
            char graph_name[64];
            snprintf(graph_name, sizeof(graph_name), "Graph_%u", ctx->graph_count + 1);
            blueprint_graph* new_graph = blueprint_create_graph(ctx, graph_name);
            if (new_graph) {
                blueprint_set_active_graph(ctx, new_graph);
            }
        }
        
        gui_end_window(gui);
    }
}

void blueprint_show_property_panel(blueprint_context* ctx, b32* p_open) {
    gui_context* gui = ctx->gui;
    editor_state* editor = &ctx->editor;
    
    if (gui_begin_window(gui, "Properties", p_open, GUI_WINDOW_NONE)) {
        
        if (editor->selected_node_count > 0) {
            gui_text(gui, "Selected Nodes (%u):", editor->selected_node_count);
            gui_separator(gui);
            
            // Show properties for first selected node
            blueprint_node* node = blueprint_get_node(ctx->active_graph, editor->selected_nodes[0]);
            if (node) {
                gui_text(gui, "Type: %s", blueprint_type_to_string(node->type));
                gui_input_text(gui, "Name", node->name, BLUEPRINT_MAX_NODE_NAME);
                gui_input_text(gui, "Display Name", node->display_name, BLUEPRINT_MAX_NODE_NAME);
                
                gui_separator(gui);
                gui_text(gui, "Position: (%.1f, %.1f)", node->position.x, node->position.y);
                gui_text(gui, "Size: (%.1f, %.1f)", node->size.x, node->size.y);
                
                gui_separator(gui);
                gui_text(gui, "Input Pins: %u", node->input_pin_count);
                gui_text(gui, "Output Pins: %u", node->output_pin_count);
                
                if (node->execution_count > 0) {
                    gui_separator(gui);
                    gui_text(gui, "Performance:");
                    gui_text(gui, "Executions: %llu", node->execution_count);
                    gui_text(gui, "Avg Time: %.3f ms", node->avg_execution_time);
                    gui_text(gui, "Total Time: %.3f ms", node->total_execution_time);
                }
            }
        } else {
            gui_text(gui, "No nodes selected");
        }
        
        gui_end_window(gui);
    }
}

void blueprint_show_debug_panel(blueprint_context* ctx, b32* p_open) {
    gui_context* gui = ctx->gui;
    blueprint_vm* vm = &ctx->vm;
    
    if (gui_begin_window(gui, "Blueprint Debug", p_open, GUI_WINDOW_NONE)) {
        
        gui_text(gui, "VM State:");
        gui_text(gui, "Running: %s", vm->is_running ? "Yes" : "No");
        gui_text(gui, "Paused: %s", vm->is_paused ? "Yes" : "No");
        gui_text(gui, "PC: %u", vm->program_counter);
        gui_text(gui, "Stack: %u/%u", vm->value_stack_top, vm->value_stack_size);
        gui_text(gui, "Instructions: %llu", vm->instructions_executed);
        gui_text(gui, "Execution Time: %.2f ms", vm->execution_time);
        
        gui_separator(gui);
        
        if (gui_button(gui, "Step")) {
            vm->single_step = true;
            vm->is_paused = false;
        }
        gui_same_line(gui, 0);
        
        if (gui_button(gui, vm->is_paused ? "Continue" : "Pause")) {
            vm->is_paused = !vm->is_paused;
        }
        gui_same_line(gui, 0);
        
        if (gui_button(gui, "Reset")) {
            vm->program_counter = 0;
            vm->value_stack_top = 0;
            vm->call_stack_top = 0;
            vm->is_running = false;
            vm->is_paused = false;
        }
        
        gui_separator(gui);
        gui_text(gui, "Breakpoints (%u):", vm->breakpoint_count);
        
        for (u32 i = 0; i < vm->breakpoint_count; i++) {
            gui_text(gui, "  Node %u", vm->breakpoints[i]);
        }
        
        gui_end_window(gui);
    }
}

// ============================================================================
// EDITOR TOOLS
// ============================================================================

void blueprint_editor_delete_selected(blueprint_context* ctx) {
    editor_state* editor = &ctx->editor;
    blueprint_graph* graph = ctx->active_graph;
    
    if (!graph) return;
    
    // Delete selected nodes
    for (u32 i = 0; i < editor->selected_node_count; i++) {
        blueprint_destroy_node(graph, editor->selected_nodes[i]);
    }
    
    editor->selected_node_count = 0;
    blueprint_log_debug(ctx, "Deleted selected nodes");
}

void blueprint_editor_copy_selected(blueprint_context* ctx) {
    // TODO: Implement copy to clipboard
    blueprint_log_debug(ctx, "Copy not yet implemented");
}

void blueprint_editor_paste(blueprint_context* ctx) {
    // TODO: Implement paste from clipboard
    blueprint_log_debug(ctx, "Paste not yet implemented");
}

void blueprint_editor_select_all(blueprint_context* ctx) {
    editor_state* editor = &ctx->editor;
    blueprint_graph* graph = ctx->active_graph;
    
    if (!graph) return;
    
    // Select all nodes
    editor->selected_node_count = 0;
    for (u32 i = 0; i < graph->node_count && editor->selected_node_count < BLUEPRINT_MAX_NODES; i++) {
        graph->nodes[i].flags |= NODE_FLAG_SELECTED;
        editor->selected_nodes[editor->selected_node_count++] = graph->nodes[i].id;
    }
    
    blueprint_log_debug(ctx, "Selected all %u nodes", editor->selected_node_count);
}

void blueprint_fit_graph_to_view(blueprint_context* ctx) {
    blueprint_graph* graph = ctx->active_graph;
    if (!graph || graph->node_count == 0) return;
    
    // Calculate bounding box of all nodes
    v2 min_pos = graph->nodes[0].position;
    v2 max_pos = (v2){graph->nodes[0].position.x + graph->nodes[0].size.x,
                     graph->nodes[0].position.y + graph->nodes[0].size.y};
    
    for (u32 i = 1; i < graph->node_count; i++) {
        blueprint_node* node = &graph->nodes[i];
        
        if (node->position.x < min_pos.x) min_pos.x = node->position.x;
        if (node->position.y < min_pos.y) min_pos.y = node->position.y;
        
        f32 node_max_x = node->position.x + node->size.x;
        f32 node_max_y = node->position.y + node->size.y;
        
        if (node_max_x > max_pos.x) max_pos.x = node_max_x;
        if (node_max_y > max_pos.y) max_pos.y = node_max_y;
    }
    
    // Add padding
    min_pos.x -= 50;
    min_pos.y -= 50;
    max_pos.x += 50;
    max_pos.y += 50;
    
    // Calculate required scale and offset
    v2 graph_size = (v2){max_pos.x - min_pos.x, max_pos.y - min_pos.y};
    v2 view_size = gui_get_content_region_avail(ctx->gui);
    
    f32 scale_x = view_size.x / graph_size.x;
    f32 scale_y = view_size.y / graph_size.y;
    
    graph->view_scale = (scale_x < scale_y) ? scale_x : scale_y;
    graph->view_scale = (graph->view_scale > 2.0f) ? 2.0f : graph->view_scale;
    graph->view_scale = (graph->view_scale < 0.1f) ? 0.1f : graph->view_scale;
    
    graph->view_offset.x = -min_pos.x;
    graph->view_offset.y = -min_pos.y;
    
    blueprint_log_debug(ctx, "Fit graph to view: scale=%.2f, offset=(%.1f, %.1f)", 
                       graph->view_scale, graph->view_offset.x, graph->view_offset.y);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

v2 blueprint_bezier_curve(v2 p0, v2 p1, v2 p2, v2 p3, f32 t) {
    f32 u = 1.0f - t;
    f32 tt = t * t;
    f32 uu = u * u;
    f32 uuu = uu * u;
    f32 ttt = tt * t;
    
    v2 point;
    point.x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x;
    point.y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y;
    
    return point;
}

b32 blueprint_point_in_rect(v2 point, v2 rect_min, v2 rect_max) {
    return (point.x >= rect_min.x && point.x <= rect_max.x &&
            point.y >= rect_min.y && point.y <= rect_max.y);
}