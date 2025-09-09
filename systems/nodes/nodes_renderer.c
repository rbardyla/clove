// nodes_renderer.c - Node graph visual renderer
// PERFORMANCE: Batched drawing, frustum culling, LOD system
// CACHE: Sequential access patterns for visible nodes

#include "handmade_nodes.h"
#include "../gui/handmade_renderer.h"
#include "../gui/handmade_platform.h"
#include <math.h>
#include <stdio.h>

// Rendering constants
#define NODE_HEADER_HEIGHT 30
#define NODE_FOOTER_HEIGHT 10
#define PIN_RADIUS 6
#define PIN_SPACING 25
#define NODE_CORNER_RADIUS 5
#define CONNECTION_THICKNESS 2
#define GRID_SIZE 20
#define MINIMAP_SIZE 200

// Internal rendering state
typedef struct {
    renderer *r;
    node_theme_t theme;
    
    // View transform
    f32 view_x, view_y;
    f32 view_zoom;
    i32 screen_width, screen_height;
    
    // Mouse state for interaction
    i32 mouse_x, mouse_y;
    bool mouse_down;
    bool mouse_dragging;
    i32 drag_start_x, drag_start_y;
    
    // Connection preview
    bool is_connecting;
    i32 connect_source_node;
    i32 connect_source_pin;
    bool connect_from_output;
    
    // Performance
    i32 nodes_drawn;
    i32 connections_drawn;
    u64 render_cycles;
} render_state_t;

static render_state_t g_render_state;

// Helper functions
static void world_to_screen(f32 wx, f32 wy, i32 *sx, i32 *sy) {
    *sx = (i32)((wx - g_render_state.view_x) * g_render_state.view_zoom);
    *sy = (i32)((wy - g_render_state.view_y) * g_render_state.view_zoom);
}

static void screen_to_world(i32 sx, i32 sy, f32 *wx, f32 *wy) {
    *wx = (f32)sx / g_render_state.view_zoom + g_render_state.view_x;
    *wy = (f32)sy / g_render_state.view_zoom + g_render_state.view_y;
}

static bool is_visible(f32 x, f32 y, f32 w, f32 h) {
    // Frustum culling
    i32 sx, sy, sw, sh;
    world_to_screen(x, y, &sx, &sy);
    sw = (i32)(w * g_render_state.view_zoom);
    sh = (i32)(h * g_render_state.view_zoom);
    
    return !(sx + sw < 0 || sx > g_render_state.screen_width ||
             sy + sh < 0 || sy > g_render_state.screen_height);
}

// Draw rounded rectangle
static void draw_rounded_rect(renderer *r, i32 x, i32 y, i32 w, i32 h, i32 radius, color32 color) {
    // PERFORMANCE: Simplified - just draw regular rect for now
    // TODO: Implement proper rounded corners with anti-aliasing
    renderer_fill_rect(r, x, y, w, h, color);
}

// Draw bezier curve for connections
static void draw_bezier_connection(renderer *r, i32 x1, i32 y1, i32 x2, i32 y2, 
                                  f32 offset, i32 thickness, color32 color) {
    // PERFORMANCE: Adaptive tessellation based on distance
    f32 dx = (f32)(x2 - x1);
    f32 dy = (f32)(y2 - y1);
    f32 distance = sqrtf(dx * dx + dy * dy);
    
    // Control points for horizontal flow
    f32 cp1x = x1 + offset;
    f32 cp1y = (f32)y1;
    f32 cp2x = x2 - offset;
    f32 cp2y = (f32)y2;
    
    // Tessellation - more segments for longer curves
    i32 segments = (i32)(distance / 10.0f);
    if (segments < 10) segments = 10;
    if (segments > 100) segments = 100;
    
    f32 last_x = (f32)x1;
    f32 last_y = (f32)y1;
    
    for (i32 i = 1; i <= segments; ++i) {
        f32 t = (f32)i / (f32)segments;
        f32 t2 = t * t;
        f32 t3 = t2 * t;
        f32 mt = 1.0f - t;
        f32 mt2 = mt * mt;
        f32 mt3 = mt2 * mt;
        
        // Cubic bezier formula
        f32 px = mt3 * x1 + 3 * mt2 * t * cp1x + 3 * mt * t2 * cp2x + t3 * x2;
        f32 py = mt3 * y1 + 3 * mt2 * t * cp1y + 3 * mt * t2 * cp2y + t3 * y2;
        
        // Draw line segment
        renderer_line(r, (i32)last_x, (i32)last_y, (i32)px, (i32)py, color);
        
        // Draw additional lines for thickness
        if (thickness > 1) {
            for (i32 j = 1; j < thickness; ++j) {
                renderer_line(r, (i32)last_x, (i32)last_y + j, (i32)px, (i32)py + j, color);
            }
        }
        
        last_x = px;
        last_y = py;
    }
}

// Draw grid background
void node_draw_grid(renderer *r, node_graph_t *graph, node_theme_t *theme) {
    // PERFORMANCE: Only draw visible grid lines
    i32 grid_size = (i32)(theme->grid_size * g_render_state.view_zoom);
    if (grid_size < 5) return;  // Too small to see
    
    i32 offset_x = -(i32)(g_render_state.view_x * g_render_state.view_zoom) % grid_size;
    i32 offset_y = -(i32)(g_render_state.view_y * g_render_state.view_zoom) % grid_size;
    
    // Regular grid lines
    for (i32 x = offset_x; x < g_render_state.screen_width; x += grid_size) {
        renderer_line(r, x, 0, x, g_render_state.screen_height, 
                     rgb((theme->grid_color >> 16) & 0xFF,
                         (theme->grid_color >> 8) & 0xFF,
                         theme->grid_color & 0xFF));
    }
    
    for (i32 y = offset_y; y < g_render_state.screen_height; y += grid_size) {
        renderer_line(r, 0, y, g_render_state.screen_width, y,
                     rgb((theme->grid_color >> 16) & 0xFF,
                         (theme->grid_color >> 8) & 0xFF,
                         theme->grid_color & 0xFF));
    }
    
    // Thick grid lines
    i32 thick_interval = (i32)theme->grid_thick_interval;
    i32 thick_grid_size = grid_size * thick_interval;
    i32 thick_offset_x = -(i32)(g_render_state.view_x * g_render_state.view_zoom) % thick_grid_size;
    i32 thick_offset_y = -(i32)(g_render_state.view_y * g_render_state.view_zoom) % thick_grid_size;
    
    for (i32 x = thick_offset_x; x < g_render_state.screen_width; x += thick_grid_size) {
        renderer_line(r, x, 0, x, g_render_state.screen_height,
                     rgb((theme->grid_color_thick >> 16) & 0xFF,
                         (theme->grid_color_thick >> 8) & 0xFF,
                         theme->grid_color_thick & 0xFF));
    }
    
    for (i32 y = thick_offset_y; y < g_render_state.screen_height; y += thick_grid_size) {
        renderer_line(r, 0, y, g_render_state.screen_width, y,
                     rgb((theme->grid_color_thick >> 16) & 0xFF,
                         (theme->grid_color_thick >> 8) & 0xFF,
                         theme->grid_color_thick & 0xFF));
    }
}

// Draw a single node
void node_draw(renderer *r, node_t *node, node_theme_t *theme) {
    if (!node || !node->type) return;
    
    // Transform to screen space
    i32 sx, sy;
    world_to_screen(node->x, node->y, &sx, &sy);
    i32 sw = (i32)(node->width * g_render_state.view_zoom);
    i32 sh = (i32)(node->height * g_render_state.view_zoom);
    
    // Frustum culling
    if (!is_visible(node->x, node->y, (f32)node->width, (f32)node->height)) {
        return;
    }
    
    // Shadow
    if (g_render_state.view_zoom > 0.5f) {
        draw_rounded_rect(r, sx + 2, sy + 2, sw, sh, NODE_CORNER_RADIUS,
                         rgb((theme->node_shadow_color >> 16) & 0xFF,
                             (theme->node_shadow_color >> 8) & 0xFF,
                             theme->node_shadow_color & 0xFF));
    }
    
    // Node background
    u32 node_color = theme->category_colors[node->type->category];
    if (node->selected) {
        // Brighten if selected
        node_color = ((node_color & 0xFEFEFE) >> 1) + 0x808080;
    }
    
    draw_rounded_rect(r, sx, sy, sw, sh, NODE_CORNER_RADIUS,
                     rgb((node_color >> 16) & 0xFF,
                         (node_color >> 8) & 0xFF,
                         node_color & 0xFF));
    
    // Header
    i32 header_height = (i32)(NODE_HEADER_HEIGHT * g_render_state.view_zoom);
    renderer_fill_rect(r, sx, sy, sw, header_height,
                      rgba((node_color >> 17) & 0x7F,
                           (node_color >> 9) & 0x7F,
                           (node_color >> 1) & 0x7F, 200));
    
    // Node title
    if (g_render_state.view_zoom > 0.3f) {
        renderer_text(r, sx + 5, sy + 5, node->type->name,
                     rgb((theme->text_color >> 16) & 0xFF,
                         (theme->text_color >> 8) & 0xFF,
                         theme->text_color & 0xFF));
    }
    
    // State indicator
    if (node->state == NODE_STATE_EXECUTING) {
        renderer_fill_rect(r, sx + sw - 10, sy + 5, 5, 5, rgb(0, 255, 0));
    } else if (node->state == NODE_STATE_ERROR) {
        renderer_fill_rect(r, sx + sw - 10, sy + 5, 5, 5, rgb(255, 0, 0));
    }
    
    // Breakpoint indicator
    if (node->has_breakpoint) {
        renderer_fill_circle(r, sx - 5, sy + header_height/2, 3, rgb(255, 0, 0));
    }
    
    // Draw pins
    i32 pin_y_start = sy + header_height + (i32)(10 * g_render_state.view_zoom);
    i32 pin_spacing = (i32)(PIN_SPACING * g_render_state.view_zoom);
    
    // Input pins
    for (i32 i = 0; i < node->input_count; ++i) {
        node_pin_t *pin = &node->inputs[i];
        i32 pin_y = pin_y_start + i * pin_spacing;
        i32 pin_radius = (i32)(PIN_RADIUS * g_render_state.view_zoom);
        
        // Pin circle
        u32 pin_color = theme->pin_colors[pin->type];
        renderer_fill_circle(r, sx, pin_y, pin_radius,
                           rgb((pin_color >> 16) & 0xFF,
                               (pin_color >> 8) & 0xFF,
                               pin_color & 0xFF));
        
        // Pin label
        if (g_render_state.view_zoom > 0.5f && !(pin->flags & PIN_FLAG_HIDDEN)) {
            renderer_text(r, sx + pin_radius + 5, pin_y - 4, pin->name,
                         rgb((theme->text_color >> 16) & 0xFF,
                             (theme->text_color >> 8) & 0xFF,
                             theme->text_color & 0xFF));
        }
    }
    
    // Output pins
    for (i32 i = 0; i < node->output_count; ++i) {
        node_pin_t *pin = &node->outputs[i];
        i32 pin_y = pin_y_start + i * pin_spacing;
        i32 pin_radius = (i32)(PIN_RADIUS * g_render_state.view_zoom);
        
        // Pin circle
        u32 pin_color = theme->pin_colors[pin->type];
        renderer_fill_circle(r, sx + sw, pin_y, pin_radius,
                           rgb((pin_color >> 16) & 0xFF,
                               (pin_color >> 8) & 0xFF,
                               pin_color & 0xFF));
        
        // Pin label
        if (g_render_state.view_zoom > 0.5f && !(pin->flags & PIN_FLAG_HIDDEN)) {
            i32 text_width = 0;
            renderer_text_size(r, pin->name, &text_width, NULL);
            renderer_text(r, sx + sw - text_width - pin_radius - 5, pin_y - 4, pin->name,
                         rgb((theme->text_color >> 16) & 0xFF,
                             (theme->text_color >> 8) & 0xFF,
                             theme->text_color & 0xFF));
        }
    }
    
    // Debug info
    if (theme->show_performance && node->execution_count > 0) {
        char perf_text[64];
        f32 ms = (f32)(node->last_execution_cycles / 3000.0);  // Assuming 3GHz
        snprintf(perf_text, 64, "%.2f us", ms);
        renderer_text(r, sx + 5, sy + sh - 15, perf_text,
                     rgb(255, 255, 0));
    }
    
    g_render_state.nodes_drawn++;
}

// Draw connections
void node_draw_connections(renderer *r, node_graph_t *graph, node_theme_t *theme) {
    // PERFORMANCE: Draw connections before nodes for proper layering
    
    for (i32 i = 0; i < graph->connection_count; ++i) {
        node_connection_t *conn = &graph->connections[i];
        if (conn->id == 0) continue;  // Skip empty slots
        
        node_t *source = node_find_by_id(graph, conn->source_node);
        node_t *target = node_find_by_id(graph, conn->target_node);
        
        if (!source || !target) continue;
        
        // Calculate pin positions
        i32 source_x, source_y, target_x, target_y;
        
        // Source pin position
        f32 source_node_x = source->x + source->width;
        f32 source_node_y = source->y + NODE_HEADER_HEIGHT + 10 + conn->source_pin * PIN_SPACING;
        world_to_screen(source_node_x, source_node_y, &source_x, &source_y);
        
        // Target pin position  
        f32 target_node_x = target->x;
        f32 target_node_y = target->y + NODE_HEADER_HEIGHT + 10 + conn->target_pin * PIN_SPACING;
        world_to_screen(target_node_x, target_node_y, &target_x, &target_y);
        
        // Frustum culling for connections
        i32 min_x = Minimum(source_x, target_x);
        i32 max_x = Maximum(source_x, target_x);
        i32 min_y = Minimum(source_y, target_y);
        i32 max_y = Maximum(source_y, target_y);
        
        if (max_x < 0 || min_x > g_render_state.screen_width ||
            max_y < 0 || min_y > g_render_state.screen_height) {
            continue;
        }
        
        // Connection color
        color32 color = rgb((conn->color >> 16) & 0xFF,
                           (conn->color >> 8) & 0xFF,
                           conn->color & 0xFF);
        
        // Animate flow for execution pins
        if (theme->animate_connections && 
            source->outputs[conn->source_pin].type == PIN_TYPE_EXECUTION) {
            // Animated dots along the connection
            conn->animation_t += 0.02f;
            if (conn->animation_t > 1.0f) conn->animation_t = 0.0f;
            
            // Draw flowing dots
            f32 t = conn->animation_t;
            f32 offset = conn->curve_offset * g_render_state.view_zoom;
            
            for (i32 j = 0; j < 5; ++j) {
                f32 dot_t = fmodf(t + j * 0.2f, 1.0f);
                f32 dot_t2 = dot_t * dot_t;
                f32 dot_t3 = dot_t2 * dot_t;
                f32 mt = 1.0f - dot_t;
                f32 mt2 = mt * mt;
                f32 mt3 = mt2 * mt;
                
                f32 cp1x = source_x + offset;
                f32 cp1y = (f32)source_y;
                f32 cp2x = target_x - offset;
                f32 cp2y = (f32)target_y;
                
                f32 px = mt3 * source_x + 3 * mt2 * dot_t * cp1x + 
                        3 * mt * dot_t2 * cp2x + dot_t3 * target_x;
                f32 py = mt3 * source_y + 3 * mt2 * dot_t * cp1y + 
                        3 * mt * dot_t2 * cp2y + dot_t3 * target_y;
                
                renderer_fill_circle(r, (i32)px, (i32)py, 3,
                                   rgb((theme->connection_flow_color >> 16) & 0xFF,
                                       (theme->connection_flow_color >> 8) & 0xFF,
                                       theme->connection_flow_color & 0xFF));
            }
        }
        
        // Draw the connection curve
        i32 thickness = conn->selected ? CONNECTION_THICKNESS + 1 : CONNECTION_THICKNESS;
        draw_bezier_connection(r, source_x, source_y, target_x, target_y,
                              conn->curve_offset * g_render_state.view_zoom,
                              thickness, color);
        
        g_render_state.connections_drawn++;
    }
    
    // Draw connection preview
    if (g_render_state.is_connecting) {
        i32 start_x, start_y;
        
        node_t *source_node = node_find_by_id(graph, g_render_state.connect_source_node);
        if (source_node) {
            if (g_render_state.connect_from_output) {
                f32 wx = source_node->x + source_node->width;
                f32 wy = source_node->y + NODE_HEADER_HEIGHT + 10 + 
                        g_render_state.connect_source_pin * PIN_SPACING;
                world_to_screen(wx, wy, &start_x, &start_y);
                
                draw_bezier_connection(r, start_x, start_y,
                                     g_render_state.mouse_x, g_render_state.mouse_y,
                                     50.0f * g_render_state.view_zoom,
                                     CONNECTION_THICKNESS,
                                     rgb(255, 255, 0));
            } else {
                f32 wx = source_node->x;
                f32 wy = source_node->y + NODE_HEADER_HEIGHT + 10 + 
                        g_render_state.connect_source_pin * PIN_SPACING;
                world_to_screen(wx, wy, &start_x, &start_y);
                
                draw_bezier_connection(r, g_render_state.mouse_x, g_render_state.mouse_y,
                                     start_x, start_y,
                                     50.0f * g_render_state.view_zoom,
                                     CONNECTION_THICKNESS,
                                     rgb(255, 255, 0));
            }
        }
    }
}

// Draw minimap
void node_draw_minimap(renderer *r, node_graph_t *graph, node_theme_t *theme) {
    if (!theme->show_minimap) return;
    
    i32 minimap_x = g_render_state.screen_width - MINIMAP_SIZE - 10;
    i32 minimap_y = 10;
    
    // Background
    renderer_blend_rect(r, minimap_x, minimap_y, MINIMAP_SIZE, MINIMAP_SIZE,
                       rgba((theme->minimap_bg >> 16) & 0xFF,
                            (theme->minimap_bg >> 8) & 0xFF,
                            theme->minimap_bg & 0xFF, 128));
    
    // Calculate bounds of all nodes
    f32 min_x = 1e9f, min_y = 1e9f;
    f32 max_x = -1e9f, max_y = -1e9f;
    
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            node_t *node = &graph->nodes[i];
            if (node->x < min_x) min_x = node->x;
            if (node->y < min_y) min_y = node->y;
            if (node->x + node->width > max_x) max_x = node->x + node->width;
            if (node->y + node->height > max_y) max_y = node->y + node->height;
        }
    }
    
    if (min_x >= max_x || min_y >= max_y) return;
    
    // Scale to fit minimap
    f32 scale_x = MINIMAP_SIZE / (max_x - min_x);
    f32 scale_y = MINIMAP_SIZE / (max_y - min_y);
    f32 scale = Minimum(scale_x, scale_y) * 0.9f;
    
    // Draw nodes as dots
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            node_t *node = &graph->nodes[i];
            i32 nx = minimap_x + (i32)((node->x - min_x) * scale);
            i32 ny = minimap_y + (i32)((node->y - min_y) * scale);
            i32 nw = (i32)(node->width * scale);
            i32 nh = (i32)(node->height * scale);
            
            if (nw < 2) nw = 2;
            if (nh < 2) nh = 2;
            
            u32 color = theme->category_colors[node->type->category];
            renderer_fill_rect(r, nx, ny, nw, nh,
                             rgb((color >> 16) & 0xFF,
                                 (color >> 8) & 0xFF,
                                 color & 0xFF));
        }
    }
    
    // Draw viewport rectangle
    i32 vx = minimap_x + (i32)((g_render_state.view_x - min_x) * scale);
    i32 vy = minimap_y + (i32)((g_render_state.view_y - min_y) * scale);
    i32 vw = (i32)((g_render_state.screen_width / g_render_state.view_zoom) * scale);
    i32 vh = (i32)((g_render_state.screen_height / g_render_state.view_zoom) * scale);
    
    renderer_draw_rect(r, vx, vy, vw, vh,
                      rgb((theme->minimap_view >> 16) & 0xFF,
                          (theme->minimap_view >> 8) & 0xFF,
                          theme->minimap_view & 0xFF));
}

// Draw selection rectangle
void node_draw_selection(renderer *r, node_graph_t *graph, node_theme_t *theme) {
    if (!graph->is_selecting) return;
    
    rect sel = graph->selection_rect;
    
    // Draw filled rect with transparency
    renderer_blend_rect(r, sel.x0, sel.y0, sel.x1 - sel.x0, sel.y1 - sel.y0,
                       rgba((theme->selection_color >> 16) & 0xFF,
                            (theme->selection_color >> 8) & 0xFF,
                            theme->selection_color & 0xFF, 64));
    
    // Draw border
    renderer_draw_rect(r, sel.x0, sel.y0, sel.x1 - sel.x0, sel.y1 - sel.y0,
                      rgb((theme->selection_color >> 16) & 0xFF,
                          (theme->selection_color >> 8) & 0xFF,
                          theme->selection_color & 0xFF));
}

// Main render function
void node_graph_render(renderer *r, node_graph_t *graph, node_theme_t *theme,
                       i32 screen_width, i32 screen_height) {
    if (!r || !graph || !theme) return;
    
    u64 start_cycles = ReadCPUTimer();
    
    // Update render state
    g_render_state.r = r;
    g_render_state.theme = *theme;
    g_render_state.view_x = graph->view_x;
    g_render_state.view_y = graph->view_y;
    g_render_state.view_zoom = graph->view_zoom;
    g_render_state.screen_width = screen_width;
    g_render_state.screen_height = screen_height;
    g_render_state.nodes_drawn = 0;
    g_render_state.connections_drawn = 0;
    
    // Clear background
    renderer_clear(r, rgb((theme->background_color >> 16) & 0xFF,
                          (theme->background_color >> 8) & 0xFF,
                          theme->background_color & 0xFF));
    
    // Draw grid
    if (theme->show_grid) {
        node_draw_grid(r, graph, theme);
    }
    
    // Draw connections first (behind nodes)
    node_draw_connections(r, graph, theme);
    
    // Draw nodes - only visible ones
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            node_draw(r, &graph->nodes[i], theme);
        }
    }
    
    // Draw selection rectangle
    node_draw_selection(r, graph, theme);
    
    // Draw minimap
    node_draw_minimap(r, graph, theme);
    
    // Draw performance overlay
    if (theme->show_performance) {
        char perf_text[256];
        f32 render_ms = (f32)((ReadCPUTimer() - start_cycles) / 3000000.0);
        f32 exec_ms = graph->last_execution_ms;
        
        snprintf(perf_text, 256, 
                "Render: %.2fms | Execute: %.2fms | Nodes: %d/%d | Connections: %d/%d",
                render_ms, exec_ms, 
                g_render_state.nodes_drawn, graph->node_count,
                g_render_state.connections_drawn, graph->connection_count);
        
        renderer_text(r, 10, screen_height - 20, perf_text, rgb(255, 255, 0));
    }
    
    g_render_state.render_cycles = ReadCPUTimer() - start_cycles;
}

// Handle mouse input
void node_graph_handle_mouse(node_graph_t *graph, i32 mouse_x, i32 mouse_y, 
                             bool mouse_down, i32 mouse_wheel) {
    g_render_state.mouse_x = mouse_x;
    g_render_state.mouse_y = mouse_y;
    
    // Handle zooming
    if (mouse_wheel != 0) {
        f32 old_zoom = graph->view_zoom;
        graph->view_zoom *= (mouse_wheel > 0) ? 1.1f : 0.9f;
        graph->view_zoom = Maximum(0.1f, Minimum(5.0f, graph->view_zoom));
        
        // Zoom towards mouse position
        f32 wx, wy;
        screen_to_world(mouse_x, mouse_y, &wx, &wy);
        graph->view_x = wx - (wx - graph->view_x) * (graph->view_zoom / old_zoom);
        graph->view_y = wy - (wy - graph->view_y) * (graph->view_zoom / old_zoom);
    }
    
    // Handle panning
    if (mouse_down && !g_render_state.mouse_down) {
        // Mouse just pressed
        g_render_state.mouse_dragging = true;
        g_render_state.drag_start_x = mouse_x;
        g_render_state.drag_start_y = mouse_y;
    } else if (!mouse_down && g_render_state.mouse_down) {
        // Mouse just released
        g_render_state.mouse_dragging = false;
    } else if (g_render_state.mouse_dragging) {
        // Dragging
        i32 dx = mouse_x - g_render_state.drag_start_x;
        i32 dy = mouse_y - g_render_state.drag_start_y;
        
        graph->view_x -= (f32)dx / graph->view_zoom;
        graph->view_y -= (f32)dy / graph->view_zoom;
        
        g_render_state.drag_start_x = mouse_x;
        g_render_state.drag_start_y = mouse_y;
    }
    
    g_render_state.mouse_down = mouse_down;
}

// Get node at screen position
node_t* node_at_position(node_graph_t *graph, i32 screen_x, i32 screen_y) {
    f32 wx, wy;
    screen_to_world(screen_x, screen_y, &wx, &wy);
    
    // Search nodes in reverse order (top to bottom)
    for (i32 i = MAX_NODES_PER_GRAPH - 1; i >= 0; --i) {
        if (graph->nodes[i].type) {
            node_t *node = &graph->nodes[i];
            if (wx >= node->x && wx <= node->x + node->width &&
                wy >= node->y && wy <= node->y + node->height) {
                return node;
            }
        }
    }
    
    return NULL;
}

// Get pin at screen position
node_pin_t* pin_at_position(node_graph_t *graph, node_t *node, i32 screen_x, i32 screen_y,
                            bool *is_output, i32 *pin_index) {
    if (!node) return NULL;
    
    f32 wx, wy;
    screen_to_world(screen_x, screen_y, &wx, &wy);
    
    i32 pin_y_start = (i32)(node->y + NODE_HEADER_HEIGHT + 10);
    
    // Check input pins
    for (i32 i = 0; i < node->input_count; ++i) {
        i32 pin_y = pin_y_start + i * PIN_SPACING;
        f32 dx = wx - node->x;
        f32 dy = wy - pin_y;
        
        if (dx * dx + dy * dy <= PIN_RADIUS * PIN_RADIUS) {
            *is_output = false;
            *pin_index = i;
            return &node->inputs[i];
        }
    }
    
    // Check output pins
    for (i32 i = 0; i < node->output_count; ++i) {
        i32 pin_y = pin_y_start + i * PIN_SPACING;
        f32 dx = wx - (node->x + node->width);
        f32 dy = wy - pin_y;
        
        if (dx * dx + dy * dy <= PIN_RADIUS * PIN_RADIUS) {
            *is_output = true;
            *pin_index = i;
            return &node->outputs[i];
        }
    }
    
    return NULL;
}