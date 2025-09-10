#include "handmade_profiler_enhanced.h"
#include "handmade_renderer.h"
#include "handmade_font.h"
#include <math.h>
#include <string.h>
#include <stdarg.h>

// AAA-Quality Real-Time Profiler Display
// Renders directly to screen with minimal overhead

// Type definitions
typedef struct flamegraph_node {
    const char* name;
    f64 self_time;
    f64 total_time;
    u32 call_count;
    struct flamegraph_node* children[16];
    u32 child_count;
} flamegraph_node;

// Forward declarations
static void draw_filled_rect(f32 x, f32 y, f32 width, f32 height, u32 color);
static void draw_rect_outline(f32 x, f32 y, f32 width, f32 height, u32 color, f32 thickness);
static void draw_line(f32 x0, f32 y0, f32 x1, f32 y1, u32 color, f32 thickness);
static void draw_text(f32 x, f32 y, const char* text, u32 color, f32 size);
static void draw_text_formatted(f32 x, f32 y, u32 color, f32 size, const char* fmt, ...);
static void draw_text_clipped(f32 x, f32 y, f32 max_width, const char* text, u32 color, f32 size);
static void draw_tooltip(f32 x, f32 y, const char* fmt, ...);
static int is_point_in_rect(f32 px, f32 py, f32 x, f32 y, f32 width, f32 height);
void draw_flamegraph_node(flamegraph_node* node, f32 x, f32 y, f32 width, f32 height, u32 depth);

// Color scheme
#define COLOR_BACKGROUND    0x1E1E1EFF
#define COLOR_GRID          0x2A2A2AFF
#define COLOR_TEXT          0xE0E0E0FF
#define COLOR_HIGHLIGHT     0x569CD6FF
#define COLOR_WARNING       0xDCDC00FF
#define COLOR_ERROR         0xFF4444FF
#define COLOR_SUCCESS       0x44FF44FF

// Timeline colors (hashed from function names)
static u32 timeline_colors[] = {
    0x569CD6FF, // Blue
    0x4EC9B0FF, // Cyan
    0x608B4EFF, // Green
    0xDCDCAA88, // Yellow
    0xCE9178FF, // Orange
    0xD16969FF, // Red
    0xC586C0FF, // Purple
    0x9CDCFEFF, // Light Blue
};

// Display state
typedef struct profiler_display_state {
    // View configuration
    f32 timeline_zoom;
    f32 timeline_pan;
    u32 selected_thread;
    u32 selected_frame;
    
    // Hover info
    profile_event* hovered_event;
    f32 hover_x, hover_y;
    
    // Animation
    f32 fps_graph_scroll;
    f32 memory_graph_scroll;
    
    // Cached rendering data
    f32* flamegraph_heights;
    u32 flamegraph_node_count;
    
} profiler_display_state;

static profiler_display_state g_display_state = {
    .timeline_zoom = 1.0f,
    .timeline_pan = 0.0f,
    .selected_thread = 0,
    .selected_frame = 0,
};

// === Main Overlay Function ===

void profiler_draw_overlay(profiler_draw_params* params) {
    profiler_system* prof = &g_profiler_system;
    
    if (!prof->enabled) return;
    
    // Draw semi-transparent background
    draw_filled_rect(params->x, params->y, params->width, params->height,
                    COLOR_BACKGROUND & 0xFFFFFFCC);
    
    // Draw border
    draw_rect_outline(params->x, params->y, params->width, params->height,
                     COLOR_HIGHLIGHT, 2.0f);
    
    f32 y_offset = params->y + 10;
    
    // Title bar
    draw_text_formatted(params->x + 10, y_offset, COLOR_TEXT, 16,
                       "PROFILER | Frame %llu | %.1f FPS | %.2fms",
                       prof->frame_number, prof->average_fps,
                       prof->current_frame.duration_ms);
    y_offset += 25;
    
    // Draw sections based on params
    if (params->show_statistics) {
        profiler_draw_statistics(params->x + 10, y_offset);
        y_offset += 150;
    }
    
    if (params->show_timeline) {
        profiler_draw_timeline(params->x + 10, y_offset, 
                              params->width - 20, 200);
        y_offset += 210;
    }
    
    if (params->show_flamegraph) {
        profiler_draw_flamegraph(params->x + 10, y_offset,
                                params->width - 20, 150);
        y_offset += 160;
    }
    
    if (params->show_memory_graph) {
        profiler_draw_memory_graph(params->x + 10, y_offset,
                                  params->width / 2 - 15, 100);
    }
    
    if (params->show_network_graph) {
        profiler_draw_network_graph(params->x + params->width / 2 + 5, y_offset,
                                   params->width / 2 - 15, 100);
    }
}

// === Timeline Visualization ===

void profiler_draw_timeline(f32 x, f32 y, f32 width, f32 height) {
    profiler_system* prof = &g_profiler_system;
    profiler_display_state* display = &g_display_state;
    
    // Background
    draw_filled_rect(x, y, width, height, 0x0A0A0AFF);
    
    // Grid lines
    f32 grid_spacing = 50.0f * display->timeline_zoom;
    f32 grid_start = fmodf(display->timeline_pan, grid_spacing);
    
    for (f32 gx = grid_start; gx < width; gx += grid_spacing) {
        draw_line(x + gx, y, x + gx, y + height, COLOR_GRID, 1.0f);
    }
    
    // Time labels
    f32 ms_per_pixel = 16.67f / (width * display->timeline_zoom);
    for (f32 gx = grid_start; gx < width; gx += grid_spacing) {
        f32 time_ms = (gx - display->timeline_pan) * ms_per_pixel;
        draw_text_formatted(x + gx + 2, y + 2, COLOR_TEXT & 0xFFFFFF88, 10,
                           "%.1fms", time_ms);
    }
    
    // Draw events for each thread
    f32 thread_height = (height - 20) / MAX_PROFILER_THREADS;
    
    for (u32 t = 0; t < MAX_PROFILER_THREADS; t++) {
        thread_profiler_state* thread = &prof->thread_states[t];
        if (thread->event_buffer.events == NULL) continue;
        
        f32 thread_y = y + 20 + t * thread_height;
        
        // Thread label
        draw_text_formatted(x + 2, thread_y, COLOR_TEXT, 10,
                           "Thread %u", t);
        
        // Draw events
        u64 frame_start = prof->frame_start_tsc;
        f32 tsc_to_pixel = width * display->timeline_zoom / 
                          (prof->current_frame.duration_cycles);
        
        struct event_ring_buffer* buffer = &thread->event_buffer;
        u64 read_pos = atomic_load(&buffer->read_pos);
        u64 write_pos = atomic_load(&buffer->write_pos);
        
        // Stack for hierarchical events
        struct {
            f32 x_start;
            u32 depth;
            u32 color;
        } event_stack[MAX_TIMER_STACK_DEPTH];
        u32 stack_depth = 0;
        
        while (read_pos != write_pos) {
            profile_event* event = &buffer->events[read_pos % buffer->capacity];
            
            f32 event_x = x + display->timeline_pan + 
                         (event->timestamp - frame_start) * tsc_to_pixel;
            
            if (event->type == EVENT_PUSH) {
                // Push event onto stack
                if (stack_depth < MAX_TIMER_STACK_DEPTH) {
                    event_stack[stack_depth].x_start = event_x;
                    event_stack[stack_depth].depth = event->depth;
                    event_stack[stack_depth].color = event->color;
                    stack_depth++;
                }
            } else if (event->type == EVENT_POP && stack_depth > 0) {
                // Pop event and draw rectangle
                stack_depth--;
                f32 x_start = event_stack[stack_depth].x_start;
                f32 x_end = event_x;
                f32 rect_y = thread_y + 15 + event_stack[stack_depth].depth * 12;
                f32 rect_height = 10;
                
                // Only draw if visible
                if (x_end > x && x_start < x + width) {
                    u32 color = timeline_colors[profiler_hash_string(event->name) % 8];
                    draw_filled_rect(x_start, rect_y, x_end - x_start, rect_height, color);
                    
                    // Draw label if wide enough
                    if (x_end - x_start > 50) {
                        draw_text_clipped(x_start + 2, rect_y + 1, 
                                        x_end - x_start - 4, event->name,
                                        COLOR_TEXT, 8);
                    }
                    
                    // Check hover
                    if (is_point_in_rect(display->hover_x, display->hover_y,
                                        x_start, rect_y, x_end - x_start, rect_height)) {
                        display->hovered_event = event;
                        
                        // Draw tooltip
                        f64 duration_ms = cycles_to_ms(event->duration_cycles);
                        draw_tooltip(display->hover_x, display->hover_y - 30,
                                   "%s\n%.3fms", event->name, duration_ms);
                    }
                }
            }
            
            read_pos++;
        }
    }
    
    // Draw frame boundaries
    draw_line(x, y + 15, x, y + height, COLOR_WARNING, 2.0f);
    draw_line(x + width, y + 15, x + width, y + height, COLOR_WARNING, 2.0f);
}

// === Flamegraph Visualization ===

void profiler_draw_flamegraph(f32 x, f32 y, f32 width, f32 height) {
    profiler_system* prof = &g_profiler_system;
    
    // Background
    draw_filled_rect(x, y, width, height, 0x0A0A0AFF);
    
    // Build flamegraph tree from timer statistics
    flamegraph_node root = {0};
    root.name = "Frame";
    root.total_time = prof->current_frame.duration_ms;
    
    // Aggregate timers into tree structure
    for (u32 i = 0; i < MAX_TIMERS; i++) {
        timer_stats* timer = &prof->timers[i];
        if (timer->call_count == 0) continue;
        
        // Simple single-level for now (could be enhanced with hierarchical data)
        if (root.child_count < 16) {
            flamegraph_node* node = calloc(1, sizeof(flamegraph_node));
            node->name = timer->name;
            node->total_time = timer->average_ms;
            node->call_count = timer->call_count;
            root.children[root.child_count++] = node;
        }
    }
    
    // Draw flamegraph recursively
    draw_flamegraph_node(&root, x, y, width, height, 0);
    
    // Cleanup
    for (u32 i = 0; i < root.child_count; i++) {
        free(root.children[i]);
    }
}

void draw_flamegraph_node(flamegraph_node* node, f32 x, f32 y, 
                          f32 width, f32 height, u32 depth) {
    if (width < 2 || height < 2) return;
    
    // Color based on depth and name
    u32 color = timeline_colors[(depth + profiler_hash_string(node->name)) % 8];
    
    // Draw rectangle
    draw_filled_rect(x, y, width, height, color);
    draw_rect_outline(x, y, width, height, 0x000000FF, 1.0f);
    
    // Draw label if wide enough
    if (width > 50) {
        char label[256];
        snprintf(label, sizeof(label), "%s (%.2fms)", 
                node->name, node->total_time);
        draw_text_clipped(x + 2, y + 2, width - 4, label, COLOR_TEXT, 10);
    }
    
    // Draw children
    if (node->child_count > 0 && height > 20) {
        f32 child_y = y + height - 15;
        f32 child_height = 15;
        f32 child_x = x;
        
        f32 total_child_time = 0;
        for (u32 i = 0; i < node->child_count; i++) {
            total_child_time += node->children[i]->total_time;
        }
        
        for (u32 i = 0; i < node->child_count; i++) {
            f32 child_width = width * (node->children[i]->total_time / total_child_time);
            draw_flamegraph_node(node->children[i], child_x, child_y,
                               child_width, child_height, depth + 1);
            child_x += child_width;
        }
    }
}

// === Statistics Display ===

void profiler_draw_statistics(f32 x, f32 y) {
    profiler_system* prof = &g_profiler_system;
    
    f32 col1_x = x;
    f32 col2_x = x + 200;
    f32 col3_x = x + 400;
    f32 line_height = 16;
    f32 current_y = y;
    
    // Frame statistics
    draw_text(col1_x, current_y, "FRAME STATISTICS", COLOR_HIGHLIGHT, 12);
    current_y += line_height;
    
    draw_text_formatted(col1_x, current_y, COLOR_TEXT, 10,
                       "Current: %.2fms", prof->current_frame.duration_ms);
    draw_text_formatted(col2_x, current_y, COLOR_TEXT, 10,
                       "Average: %.2fms", 1000.0f / prof->average_fps);
    current_y += line_height;
    
    // Find min/max frame times
    f64 min_frame = 1000.0;
    f64 max_frame = 0.0;
    for (u32 i = 0; i < FRAME_HISTORY_SIZE; i++) {
        if (prof->frame_history[i].duration_ms > 0) {
            if (prof->frame_history[i].duration_ms < min_frame)
                min_frame = prof->frame_history[i].duration_ms;
            if (prof->frame_history[i].duration_ms > max_frame)
                max_frame = prof->frame_history[i].duration_ms;
        }
    }
    
    draw_text_formatted(col1_x, current_y, COLOR_TEXT, 10,
                       "Min: %.2fms", min_frame);
    draw_text_formatted(col2_x, current_y, COLOR_TEXT, 10,
                       "Max: %.2fms", max_frame);
    current_y += line_height * 1.5f;
    
    // Top timers
    draw_text(col1_x, current_y, "TOP FUNCTIONS", COLOR_HIGHLIGHT, 12);
    current_y += line_height;
    
    // Sort timers by total time
    struct {
        timer_stats* timer;
        f64 total_ms;
    } sorted_timers[16];
    u32 sorted_count = 0;
    
    for (u32 i = 0; i < MAX_TIMERS && sorted_count < 16; i++) {
        timer_stats* timer = &prof->timers[i];
        if (timer->call_count > 0) {
            // Insert sorted
            f64 total_ms = timer->average_ms * timer->call_count;
            u32 insert_pos = sorted_count;
            for (u32 j = 0; j < sorted_count; j++) {
                if (total_ms > sorted_timers[j].total_ms) {
                    insert_pos = j;
                    break;
                }
            }
            
            // Shift and insert
            for (u32 j = sorted_count; j > insert_pos; j--) {
                sorted_timers[j] = sorted_timers[j - 1];
            }
            sorted_timers[insert_pos].timer = timer;
            sorted_timers[insert_pos].total_ms = total_ms;
            sorted_count++;
        }
    }
    
    // Display top 5
    for (u32 i = 0; i < 5 && i < sorted_count; i++) {
        timer_stats* timer = sorted_timers[i].timer;
        
        // Color based on performance
        u32 color = COLOR_TEXT;
        if (sorted_timers[i].total_ms > 5.0) color = COLOR_ERROR;
        else if (sorted_timers[i].total_ms > 2.0) color = COLOR_WARNING;
        
        draw_text_formatted(col1_x, current_y, color, 10,
                           "%s", timer->name);
        draw_text_formatted(col2_x, current_y, color, 10,
                           "%.2fms", timer->average_ms);
        draw_text_formatted(col3_x, current_y, color, 10,
                           "%u calls", timer->call_count);
        current_y += line_height;
    }
}

// === Memory Graph ===

void profiler_draw_memory_graph(f32 x, f32 y, f32 width, f32 height) {
    profiler_system* prof = &g_profiler_system;
    
    // Background
    draw_filled_rect(x, y, width, height, 0x0A0A0AFF);
    
    // Title
    draw_text(x + 5, y + 5, "MEMORY", COLOR_HIGHLIGHT, 10);
    
    // Current stats
    f64 current_mb = (f64)prof->current_allocated / (1024.0 * 1024.0);
    f64 peak_mb = (f64)prof->peak_allocated / (1024.0 * 1024.0);
    
    draw_text_formatted(x + 5, y + 20, COLOR_TEXT, 10,
                       "Current: %.1f MB", current_mb);
    draw_text_formatted(x + 5, y + 35, COLOR_TEXT, 10,
                       "Peak: %.1f MB", peak_mb);
    
    // Graph
    f32 graph_x = x + 5;
    f32 graph_y = y + 50;
    f32 graph_width = width - 10;
    f32 graph_height = height - 55;
    
    // Grid
    draw_rect_outline(graph_x, graph_y, graph_width, graph_height, COLOR_GRID, 1.0f);
    
    // Plot memory history
    f32 max_memory = peak_mb * 1.2f;
    f32 x_step = graph_width / FRAME_HISTORY_SIZE;
    
    for (u32 i = 1; i < FRAME_HISTORY_SIZE; i++) {
        u32 idx0 = (prof->frame_number + i - 1) % FRAME_HISTORY_SIZE;
        u32 idx1 = (prof->frame_number + i) % FRAME_HISTORY_SIZE;
        
        f64 mem0 = (f64)prof->frame_history[idx0].memory_allocated / (1024.0 * 1024.0);
        f64 mem1 = (f64)prof->frame_history[idx1].memory_allocated / (1024.0 * 1024.0);
        
        f32 y0 = graph_y + graph_height - (mem0 / max_memory) * graph_height;
        f32 y1 = graph_y + graph_height - (mem1 / max_memory) * graph_height;
        
        draw_line(graph_x + (i - 1) * x_step, y0,
                 graph_x + i * x_step, y1,
                 COLOR_SUCCESS, 2.0f);
    }
    
    // Allocation count
    draw_text_formatted(x + 5, y + height - 10, COLOR_TEXT & 0xFFFFFF88, 8,
                       "Allocations: %llu", prof->total_allocations);
}

// === Network Graph ===

void profiler_draw_network_graph(f32 x, f32 y, f32 width, f32 height) {
    profiler_system* prof = &g_profiler_system;
    
    // Background
    draw_filled_rect(x, y, width, height, 0x0A0A0AFF);
    
    // Title
    draw_text(x + 5, y + 5, "NETWORK", COLOR_HIGHLIGHT, 10);
    
    // Current stats
    f64 send_rate = (f64)prof->total_bytes_sent / 1024.0;
    f64 recv_rate = (f64)prof->total_bytes_received / 1024.0;
    
    draw_text_formatted(x + 5, y + 20, COLOR_TEXT, 10,
                       "Send: %.1f KB/s", send_rate);
    draw_text_formatted(x + 5, y + 35, COLOR_TEXT, 10,
                       "Recv: %.1f KB/s", recv_rate);
    
    // Graph
    f32 graph_x = x + 5;
    f32 graph_y = y + 50;
    f32 graph_width = width - 10;
    f32 graph_height = height - 55;
    
    // Grid
    draw_rect_outline(graph_x, graph_y, graph_width, graph_height, COLOR_GRID, 1.0f);
    
    // Plot bandwidth history
    f32 max_bandwidth = 100.0; // KB/s
    f32 x_step = graph_width / FRAME_HISTORY_SIZE;
    
    for (u32 i = 1; i < FRAME_HISTORY_SIZE; i++) {
        u32 idx0 = (prof->frame_number + i - 1) % FRAME_HISTORY_SIZE;
        u32 idx1 = (prof->frame_number + i) % FRAME_HISTORY_SIZE;
        
        // Send (green)
        f64 send0 = (f64)prof->frame_history[idx0].network_bytes_sent / 1024.0;
        f64 send1 = (f64)prof->frame_history[idx1].network_bytes_sent / 1024.0;
        
        f32 y0 = graph_y + graph_height - (send0 / max_bandwidth) * graph_height;
        f32 y1 = graph_y + graph_height - (send1 / max_bandwidth) * graph_height;
        
        draw_line(graph_x + (i - 1) * x_step, y0,
                 graph_x + i * x_step, y1,
                 COLOR_SUCCESS, 1.5f);
        
        // Receive (blue)
        f64 recv0 = (f64)prof->frame_history[idx0].network_bytes_received / 1024.0;
        f64 recv1 = (f64)prof->frame_history[idx1].network_bytes_received / 1024.0;
        
        y0 = graph_y + graph_height - (recv0 / max_bandwidth) * graph_height;
        y1 = graph_y + graph_height - (recv1 / max_bandwidth) * graph_height;
        
        draw_line(graph_x + (i - 1) * x_step, y0,
                 graph_x + i * x_step, y1,
                 COLOR_HIGHLIGHT, 1.5f);
    }
    
    // Packet count
    draw_text_formatted(x + 5, y + height - 10, COLOR_TEXT & 0xFFFFFF88, 8,
                       "Packets: %u/%u", 
                       prof->current_frame.network_packets_sent,
                       prof->current_frame.network_packets_received);
}

// === Helper Drawing Functions ===

static void draw_filled_rect(f32 x, f32 y, f32 width, f32 height, u32 color) {
    // Implementation depends on renderer
    // This would call into handmade_renderer.h functions
    renderer_draw_filled_rect(x, y, width, height, color);
}

static void draw_rect_outline(f32 x, f32 y, f32 width, f32 height, u32 color, f32 thickness) {
    renderer_draw_rect_outline(x, y, width, height, color, thickness);
}

static void draw_line(f32 x0, f32 y0, f32 x1, f32 y1, u32 color, f32 thickness) {
    renderer_draw_line(x0, y0, x1, y1, color, thickness);
}

static void draw_text(f32 x, f32 y, const char* text, u32 color, f32 size) {
    renderer_draw_text(x, y, text, color, size);
}

static void draw_text_formatted(f32 x, f32 y, u32 color, f32 size, const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    renderer_draw_text(x, y, buffer, color, size);
}

static void draw_text_clipped(f32 x, f32 y, f32 max_width, const char* text, u32 color, f32 size) {
    // Draw text with clipping to max_width
    renderer_draw_text_clipped(x, y, max_width, text, color, size);
}

static void draw_tooltip(f32 x, f32 y, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Calculate tooltip size
    f32 text_width = strlen(buffer) * 8;
    f32 text_height = 20;
    
    // Draw background
    draw_filled_rect(x, y, text_width + 10, text_height + 5, 0x000000DD);
    draw_rect_outline(x, y, text_width + 10, text_height + 5, COLOR_HIGHLIGHT, 1.0f);
    
    // Draw text
    draw_text(x + 5, y + 2, buffer, COLOR_TEXT, 10);
}

static int is_point_in_rect(f32 px, f32 py, f32 x, f32 y, f32 width, f32 height) {
    return px >= x && px <= x + width && py >= y && py <= y + height;
}

// === Input Handling ===

void profiler_handle_input(f32 mouse_x, f32 mouse_y, int mouse_buttons, int key) {
    profiler_display_state* display = &g_display_state;
    
    // Update hover position
    display->hover_x = mouse_x;
    display->hover_y = mouse_y;
    
    // Handle timeline zoom with mouse wheel
    if (key == KEY_MOUSE_WHEEL_UP) {
        display->timeline_zoom *= 1.1f;
        if (display->timeline_zoom > 10.0f) display->timeline_zoom = 10.0f;
    } else if (key == KEY_MOUSE_WHEEL_DOWN) {
        display->timeline_zoom /= 1.1f;
        if (display->timeline_zoom < 0.1f) display->timeline_zoom = 0.1f;
    }
    
    // Handle timeline pan with mouse drag
    static f32 drag_start_x = 0;
    static int dragging = 0;
    
    if (mouse_buttons & MOUSE_LEFT) {
        if (!dragging) {
            dragging = 1;
            drag_start_x = mouse_x - display->timeline_pan;
        } else {
            display->timeline_pan = mouse_x - drag_start_x;
        }
    } else {
        dragging = 0;
    }
    
    // Keyboard shortcuts
    switch (key) {
        case 'P': // Toggle profiler
            g_profiler_system.enabled = !g_profiler_system.enabled;
            break;
        case 'C': // Capture single frame
            g_profiler_system.capture_mode = CAPTURE_SINGLE_FRAME;
            break;
        case 'R': // Start/stop recording
            if (g_profiler_system.recording_active) {
                profiler_stop_recording();
            } else {
                profiler_start_recording();
            }
            break;
        case 'E': // Export chrome trace
            profiler_export_chrome_trace("profile.json");
            break;
        case 'T': // Cycle through threads
            display->selected_thread = (display->selected_thread + 1) % MAX_PROFILER_THREADS;
            break;
    }
}