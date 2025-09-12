#include "handmade_renderer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Debug overlay for real-time performance monitoring

typedef struct {
    float values[60];  // Last 60 frames
    int write_index;
    float min;
    float max;
    float avg;
    char name[32];
} perf_metric;

typedef struct {
    perf_metric frame_time;
    perf_metric draw_calls;
    perf_metric triangles;
    perf_metric state_changes;
    perf_metric texture_switches;
    
    // Timing breakdown
    double cpu_start;
    double cpu_end;
    double gpu_start;
    double gpu_end;
    
    // Bottleneck detection
    enum {
        BOTTLENECK_NONE,
        BOTTLENECK_CPU_BOUND,
        BOTTLENECK_GPU_BOUND,
        BOTTLENECK_VSYNC_LIMITED
    } bottleneck;
    
    // Frame budget tracking  
    float target_fps;
    float budget_ms;
    float budget_used_percent;
    
} debug_overlay_state;

static debug_overlay_state* g_debug_overlay = NULL;

// Initialize debug overlay
void renderer_debug_overlay_init(float target_fps) {
    if (!g_debug_overlay) {
        g_debug_overlay = calloc(1, sizeof(debug_overlay_state));
    }
    
    g_debug_overlay->target_fps = target_fps;
    g_debug_overlay->budget_ms = 1000.0f / target_fps;
    
    strcpy(g_debug_overlay->frame_time.name, "Frame Time");
    strcpy(g_debug_overlay->draw_calls.name, "Draw Calls");
    strcpy(g_debug_overlay->triangles.name, "Triangles");
    strcpy(g_debug_overlay->state_changes.name, "State Changes");
    strcpy(g_debug_overlay->texture_switches.name, "Texture Switches");
}

// Update metric with new value
static void update_metric(perf_metric* metric, float value) {
    metric->values[metric->write_index] = value;
    metric->write_index = (metric->write_index + 1) % 60;
    
    // Calculate min/max/avg
    metric->min = 999999.0f;
    metric->max = 0.0f;
    metric->avg = 0.0f;
    
    for (int i = 0; i < 60; i++) {
        float v = metric->values[i];
        if (v < metric->min) metric->min = v;
        if (v > metric->max) metric->max = v;
        metric->avg += v;
    }
    metric->avg /= 60.0f;
}

// Record frame metrics
void renderer_debug_overlay_frame_start() {
    if (!g_debug_overlay) return;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_debug_overlay->cpu_start = ts.tv_sec + ts.tv_nsec / 1e9;
}

void renderer_debug_overlay_frame_end(renderer_stats* stats) {
    if (!g_debug_overlay) return;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_debug_overlay->cpu_end = ts.tv_sec + ts.tv_nsec / 1e9;
    
    float frame_ms = (g_debug_overlay->cpu_end - g_debug_overlay->cpu_start) * 1000.0f;
    
    // Update metrics
    update_metric(&g_debug_overlay->frame_time, frame_ms);
    update_metric(&g_debug_overlay->draw_calls, (float)stats->draw_calls);
    update_metric(&g_debug_overlay->triangles, (float)stats->triangles_rendered / 1000.0f); // In thousands
    update_metric(&g_debug_overlay->state_changes, (float)stats->state_changes);
    update_metric(&g_debug_overlay->texture_switches, (float)stats->texture_switches);
    
    // Detect bottleneck
    g_debug_overlay->budget_used_percent = (frame_ms / g_debug_overlay->budget_ms) * 100.0f;
    
    if (frame_ms < g_debug_overlay->budget_ms * 0.95f) {
        g_debug_overlay->bottleneck = BOTTLENECK_VSYNC_LIMITED;
    } else if (stats->gpu_time_ms > frame_ms * 0.8f) {
        g_debug_overlay->bottleneck = BOTTLENECK_GPU_BOUND;
    } else {
        g_debug_overlay->bottleneck = BOTTLENECK_CPU_BOUND;
    }
}

// Render debug overlay
void renderer_debug_overlay_render(renderer_state* renderer) {
    if (!g_debug_overlay) return;
    
    // Set up 2D rendering
    // Assume we have immediate-mode text rendering
    
    int y = 10;
    int line_height = 20;
    char buffer[256];
    
    // Title
    snprintf(buffer, sizeof(buffer), "=== RENDERER DEBUG ===");
    renderer_draw_text(renderer, 10, y, buffer, 0xFFFFFFFF);
    y += line_height * 1.5;
    
    // Frame time with graph
    snprintf(buffer, sizeof(buffer), "Frame: %.2f ms (avg: %.2f, max: %.2f)",
             g_debug_overlay->frame_time.values[g_debug_overlay->frame_time.write_index - 1],
             g_debug_overlay->frame_time.avg,
             g_debug_overlay->frame_time.max);
    renderer_draw_text(renderer, 10, y, buffer, 0xFFFFFFFF);
    y += line_height;
    
    // FPS
    float current_fps = 1000.0f / g_debug_overlay->frame_time.avg;
    snprintf(buffer, sizeof(buffer), "FPS: %.0f / %.0f (%.0f%% budget)",
             current_fps, g_debug_overlay->target_fps, g_debug_overlay->budget_used_percent);
    u32 color = g_debug_overlay->budget_used_percent > 100 ? 0xFF0000FF : 0xFF00FF00;
    renderer_draw_text(renderer, 10, y, buffer, color);
    y += line_height;
    
    // Bottleneck indicator
    const char* bottleneck_str = "Unknown";
    switch (g_debug_overlay->bottleneck) {
        case BOTTLENECK_CPU_BOUND: bottleneck_str = "CPU Bound"; color = 0xFF8800FF; break;
        case BOTTLENECK_GPU_BOUND: bottleneck_str = "GPU Bound"; color = 0xFF0088FF; break;
        case BOTTLENECK_VSYNC_LIMITED: bottleneck_str = "VSync Limited"; color = 0xFF00FF00; break;
        default: break;
    }
    snprintf(buffer, sizeof(buffer), "Bottleneck: %s", bottleneck_str);
    renderer_draw_text(renderer, 10, y, buffer, color);
    y += line_height * 1.5;
    
    // Draw calls
    snprintf(buffer, sizeof(buffer), "Draw Calls: %.0f (avg: %.0f)",
             g_debug_overlay->draw_calls.values[g_debug_overlay->draw_calls.write_index - 1],
             g_debug_overlay->draw_calls.avg);
    renderer_draw_text(renderer, 10, y, buffer, 0xFFFFFFFF);
    y += line_height;
    
    // Triangles
    snprintf(buffer, sizeof(buffer), "Triangles: %.0fK (avg: %.0fK)",
             g_debug_overlay->triangles.values[g_debug_overlay->triangles.write_index - 1],
             g_debug_overlay->triangles.avg);
    renderer_draw_text(renderer, 10, y, buffer, 0xFFFFFFFF);
    y += line_height;
    
    // State changes
    snprintf(buffer, sizeof(buffer), "State Changes: %.0f",
             g_debug_overlay->state_changes.values[g_debug_overlay->state_changes.write_index - 1]);
    renderer_draw_text(renderer, 10, y, buffer, 0xFFFFFFFF);
    y += line_height * 1.5;
    
    // Draw frame time graph
    draw_metric_graph(renderer, &g_debug_overlay->frame_time, 10, y, 200, 60);
    y += 70;
    
    // Optimization hints
    snprintf(buffer, sizeof(buffer), "=== OPTIMIZATION HINTS ===");
    renderer_draw_text(renderer, 10, y, buffer, 0xFFFFFFFF);
    y += line_height;
    
    if (g_debug_overlay->bottleneck == BOTTLENECK_CPU_BOUND) {
        if (g_debug_overlay->draw_calls.avg > 1000) {
            snprintf(buffer, sizeof(buffer), "! High draw calls - consider batching");
            renderer_draw_text(renderer, 10, y, buffer, 0xFFFF00FF);
            y += line_height;
        }
        if (g_debug_overlay->state_changes.avg > 100) {
            snprintf(buffer, sizeof(buffer), "! Many state changes - sort by material");
            renderer_draw_text(renderer, 10, y, buffer, 0xFFFF00FF);
            y += line_height;
        }
    } else if (g_debug_overlay->bottleneck == BOTTLENECK_GPU_BOUND) {
        if (g_debug_overlay->triangles.avg > 1000) {
            snprintf(buffer, sizeof(buffer), "! High triangle count - use LODs");
            renderer_draw_text(renderer, 10, y, buffer, 0xFFFF00FF);
            y += line_height;
        }
    }
}

// Draw a metric graph
static void draw_metric_graph(renderer_state* renderer, perf_metric* metric, 
                              int x, int y, int width, int height) {
    // Draw background
    renderer_draw_rect(renderer, x, y, width, height, 0x40000000);
    
    // Draw grid lines
    for (int i = 0; i <= 4; i++) {
        int line_y = y + (height * i) / 4;
        renderer_draw_line(renderer, x, line_y, x + width, line_y, 0x40FFFFFF);
    }
    
    // Draw data
    float scale = height / (metric->max - metric->min + 0.001f);
    
    for (int i = 0; i < 59; i++) {
        int idx1 = (metric->write_index + i) % 60;
        int idx2 = (metric->write_index + i + 1) % 60;
        
        float v1 = metric->values[idx1];
        float v2 = metric->values[idx2];
        
        int x1 = x + (width * i) / 59;
        int x2 = x + (width * (i + 1)) / 59;
        int y1 = y + height - (v1 - metric->min) * scale;
        int y2 = y + height - (v2 - metric->min) * scale;
        
        // Color based on value vs target
        u32 color = 0xFF00FF00;  // Green
        if (metric == &g_debug_overlay->frame_time) {
            if (v2 > g_debug_overlay->budget_ms) color = 0xFF0000FF;  // Red
            else if (v2 > g_debug_overlay->budget_ms * 0.8f) color = 0xFFFF00FF;  // Yellow
        }
        
        renderer_draw_line(renderer, x1, y1, x2, y2, color);
    }
    
    // Draw labels
    char label[64];
    snprintf(label, sizeof(label), "%.1f", metric->max);
    renderer_draw_text(renderer, x + width + 5, y, label, 0xFFFFFFFF);
    
    snprintf(label, sizeof(label), "%.1f", metric->min);
    renderer_draw_text(renderer, x + width + 5, y + height - 10, label, 0xFFFFFFFF);
}

// Cleanup
void renderer_debug_overlay_shutdown() {
    if (g_debug_overlay) {
        free(g_debug_overlay);
        g_debug_overlay = NULL;
    }
}
