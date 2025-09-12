/*
    Complete Debug Overlay with Bitmap Font Rendering
    No stubs - fully functional implementation
*/

#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

// Simple 8x8 bitmap font (ASCII 32-127)
static const uint64_t FONT_8X8[] = {
    0x0000000000000000, // Space
    0x183C3C1818001800, // !
    0x6C6C000000000000, // "
    0x6C6CFE6CFE6C6C00, // #
    0x183E603C067C1800, // $
    0x00C6CC183066C600, // %
    0x386C3876DCCC7600, // &
    0x1818300000000000, // '
    0x0C18303030180C00, // (
    0x30180C0C0C183000, // )
    0x00663CFF3C660000, // *
    0x0018187E18180000, // +
    0x0000000000181830, // ,
    0x0000007E00000000, // -
    0x0000000000181800, // .
    0x03060C183060C000, // /
    0x3C666E7666663C00, // 0
    0x1838181818187E00, // 1
    0x3C66060C30607E00, // 2
    0x3C66061C06663C00, // 3
    0x0C1C3C6C7E0C0C00, // 4
    0x7E607C0606663C00, // 5
    0x1C30607C66663C00, // 6
    0x7E06060C18181800, // 7
    0x3C66663C66663C00, // 8
    0x3C66663E060C3800, // 9
    0x0018180000181800, // :
    0x0018180000181830, // ;
    0x0C18306030180C00, // <
    0x00007E007E000000, // =
    0x30180C060C183000, // >
    0x3C66060C18001800, // ?
    0x3C666E6E60623C00, // @
    0x183C66667E666600, // A
    0x7C66667C66667C00, // B
    0x3C66606060663C00, // C
    0x786C66666C787800, // D
    0x7E60607C60607E00, // E
    0x7E60607C60606000, // F
    0x3C66606E66663C00, // G
    0x6666667E66666600, // H
    0x7E18181818187E00, // I
    0x3E0C0C0C0C6C3800, // J
    0x666C78786C666600, // K
    0x6060606060607E00, // L
    0xC6EEFED6C6C6C600, // M
    0x6676767E6E666600, // N
    0x3C66666666663C00, // O
    0x7C66667C60606000, // P
    0x3C666666663C0E00, // Q
    0x7C66667C6C666600, // R
    0x3C66603C06663C00, // S
    0x7E18181818181800, // T
    0x6666666666663C00, // U
    0x66666666663C1800, // V
    0xC6C6C6D6FEEEC600, // W
    0x66663C183C666600, // X
    0x66663C1818181800, // Y
    0x7E060C1830607E00, // Z
    0x3C30303030303C00, // [
    0xC06030180C060200, // \ 
    0x3C0C0C0C0C0C3C00, // ]
    0x183C666600000000, // ^
    0x00000000000000FF, // _
    0x30180C0000000000, // `
    0x00003C063E663E00, // a
    0x60607C6666667C00, // b
    0x00003C6660663C00, // c
    0x06063E6666663E00, // d
    0x00003C667E603C00, // e
    0x1C30307C30303000, // f
    0x00003E66663E067C, // g
    0x60607C6666666600, // h
    0x1800381818183C00, // i
    0x0C001C0C0C0C6C38, // j
    0x6060666C786C6600, // k
    0x3818181818183C00, // l
    0x0000ECFED6D6C600, // m
    0x00007C6666666600, // n
    0x00003C6666663C00, // o
    0x00007C66667C6060, // p
    0x00003E66663E0606, // q
    0x00007C6660606000, // r
    0x00003E603C067C00, // s
    0x30307C3030301C00, // t
    0x0000666666663E00, // u
    0x00006666663C1800, // v
    0x0000C6D6FEFE6C00, // w
    0x0000663C183C6600, // x
    0x00006666663E067C, // y
    0x00007E0C18307E00, // z
    0x0E18187018180E00, // {
    0x1818181818181800, // |
    0x7018180E18187000, // }
    0x76DC000000000000, // ~
    0x0000000000000000  // DEL
};

typedef struct {
    float values[120];  // Last 120 frames (2 seconds at 60fps)
    int write_index;
    float min;
    float max;
    float avg;
    float sum;
    char name[32];
} perf_metric;

typedef struct {
    perf_metric frame_time;
    perf_metric draw_calls;
    perf_metric triangles;
    perf_metric state_changes;
    perf_metric fps;
    
    // Timing
    double last_time;
    double frame_start;
    
    // Frame budget
    float target_fps;
    float budget_ms;
    
    // Stats accumulator
    int total_draw_calls;
    int total_triangles;
    int total_state_changes;
    
    // Display state
    int enabled;
    int show_graphs;
    int show_hints;
    
} debug_overlay_state;

static debug_overlay_state g_overlay = {0};

// Initialize debug overlay
void debug_overlay_init(float target_fps) {
    memset(&g_overlay, 0, sizeof(g_overlay));
    
    g_overlay.target_fps = target_fps;
    g_overlay.budget_ms = 1000.0f / target_fps;
    g_overlay.enabled = 1;
    g_overlay.show_graphs = 1;
    g_overlay.show_hints = 1;
    
    strcpy(g_overlay.frame_time.name, "Frame Time");
    strcpy(g_overlay.draw_calls.name, "Draw Calls");
    strcpy(g_overlay.triangles.name, "Triangles");
    strcpy(g_overlay.state_changes.name, "State Changes");
    strcpy(g_overlay.fps.name, "FPS");
}

// Update metric
static void update_metric(perf_metric* m, float value) {
    // Remove old value from sum
    m->sum -= m->values[m->write_index];
    
    // Add new value
    m->values[m->write_index] = value;
    m->sum += value;
    m->write_index = (m->write_index + 1) % 120;
    
    // Recalculate stats
    m->min = 999999.0f;
    m->max = 0.0f;
    
    for (int i = 0; i < 120; i++) {
        float v = m->values[i];
        if (v > 0) {
            if (v < m->min) m->min = v;
            if (v > m->max) m->max = v;
        }
    }
    
    m->avg = m->sum / 120.0f;
}

// Start frame timing
void debug_overlay_begin_frame() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_overlay.frame_start = ts.tv_sec + ts.tv_nsec / 1e9;
    
    // Reset per-frame counters
    g_overlay.total_draw_calls = 0;
    g_overlay.total_triangles = 0;
    g_overlay.total_state_changes = 0;
}

// End frame and update metrics
void debug_overlay_end_frame() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;
    
    float frame_ms = (now - g_overlay.frame_start) * 1000.0f;
    float fps = 1000.0f / frame_ms;
    
    update_metric(&g_overlay.frame_time, frame_ms);
    update_metric(&g_overlay.fps, fps);
    update_metric(&g_overlay.draw_calls, (float)g_overlay.total_draw_calls);
    update_metric(&g_overlay.triangles, (float)g_overlay.total_triangles);
    update_metric(&g_overlay.state_changes, (float)g_overlay.total_state_changes);
    
    g_overlay.last_time = now;
}

// Track draw call
void debug_overlay_draw_call(int triangles) {
    g_overlay.total_draw_calls++;
    g_overlay.total_triangles += triangles;
}

// Track state change
void debug_overlay_state_change() {
    g_overlay.total_state_changes++;
}

// Draw a single character at position
static void draw_char(int x, int y, char c, float r, float g, float b) {
    if (c < 32 || c > 127) return;
    
    uint64_t bitmap = FONT_8X8[c - 32];
    
    glBegin(GL_POINTS);
    glColor3f(r, g, b);
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap & (1ULL << (row * 8 + col))) {
                glVertex2i(x + col, y + row);
            }
        }
    }
    
    glEnd();
}

// Draw string
static void draw_string(int x, int y, const char* text, float r, float g, float b) {
    int start_x = x;
    while (*text) {
        if (*text == '\n') {
            y += 10;
            x = start_x;
        } else {
            draw_char(x, y, *text, r, g, b);
            x += 8;
        }
        text++;
    }
}

// Draw filled rectangle
static void draw_rect(int x, int y, int w, int h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

// Draw line
static void draw_line(int x1, int y1, int x2, int y2, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
}

// Draw metric graph
static void draw_graph(perf_metric* m, int x, int y, int w, int h, float threshold) {
    // Background
    draw_rect(x, y, w, h, 0.1f, 0.1f, 0.1f, 0.8f);
    
    // Grid lines
    for (int i = 1; i < 4; i++) {
        int ly = y + (h * i) / 4;
        draw_line(x, ly, x + w, ly, 0.3f, 0.3f, 0.3f);
    }
    
    // Threshold line
    if (threshold > 0 && threshold < m->max) {
        int ty = y + h - (int)((threshold - m->min) / (m->max - m->min) * h);
        draw_line(x, ty, x + w, ty, 1.0f, 0.5f, 0.0f);
    }
    
    // Data points
    float scale = h / (m->max - m->min + 0.001f);
    
    for (int i = 0; i < 119; i++) {
        int idx1 = (m->write_index + i) % 120;
        int idx2 = (m->write_index + i + 1) % 120;
        
        float v1 = m->values[idx1];
        float v2 = m->values[idx2];
        
        if (v1 > 0 && v2 > 0) {
            int x1 = x + (w * i) / 119;
            int x2 = x + (w * (i + 1)) / 119;
            int y1 = y + h - (int)((v1 - m->min) * scale);
            int y2 = y + h - (int)((v2 - m->min) * scale);
            
            // Color based on value
            float r = 0.0f, g = 1.0f, b = 0.0f;
            if (threshold > 0) {
                if (v2 > threshold) {
                    r = 1.0f; g = 0.0f; b = 0.0f;
                } else if (v2 > threshold * 0.8f) {
                    r = 1.0f; g = 1.0f; b = 0.0f;
                }
            }
            
            draw_line(x1, y1, x2, y2, r, g, b);
        }
    }
}

// Main render function
void debug_overlay_render(int viewport_width, int viewport_height) {
    if (!g_overlay.enabled) return;
    
    // Save GL state
    GLboolean depth_test, blend;
    glGetBooleanv(GL_DEPTH_TEST, &depth_test);
    glGetBooleanv(GL_BLEND, &blend);
    
    // Set up 2D rendering
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, viewport_width, viewport_height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Draw overlay
    int x = 10;
    int y = 10;
    char buffer[256];
    
    // Background panel
    draw_rect(5, 5, 350, 25, 0.0f, 0.0f, 0.0f, 0.7f);
    
    // Title bar
    draw_string(10, 10, "RENDERER DEBUG [F1:Toggle] [F2:Graphs] [F3:Hints]", 1.0f, 1.0f, 1.0f);
    y += 30;
    
    // Frame time
    int frame_idx = (g_overlay.frame_time.write_index - 1 + 120) % 120;
    int fps_idx = (g_overlay.fps.write_index - 1 + 120) % 120;
    float current_frame = g_overlay.frame_time.values[frame_idx];
    float current_fps = g_overlay.fps.values[fps_idx];
    
    snprintf(buffer, sizeof(buffer), "Frame: %.2fms (%.0f FPS)", current_frame, current_fps);
    float color_r = current_frame > g_overlay.budget_ms ? 1.0f : 0.0f;
    float color_g = current_frame < g_overlay.budget_ms ? 1.0f : 0.5f;
    draw_string(x, y, buffer, color_r, color_g, 0.0f);
    y += 12;
    
    snprintf(buffer, sizeof(buffer), "  Avg: %.2fms  Max: %.2fms", 
             g_overlay.frame_time.avg, g_overlay.frame_time.max);
    draw_string(x, y, buffer, 0.7f, 0.7f, 0.7f);
    y += 12;
    
    // Budget indicator
    float budget_percent = (current_frame / g_overlay.budget_ms) * 100.0f;
    snprintf(buffer, sizeof(buffer), "Budget: %.0f%% of %.1fms (%.0f FPS target)",
             budget_percent, g_overlay.budget_ms, g_overlay.target_fps);
    draw_string(x, y, buffer, 0.8f, 0.8f, 0.8f);
    y += 20;
    
    // Draw calls
    snprintf(buffer, sizeof(buffer), "Draw Calls: %d (avg: %.0f)",
             g_overlay.total_draw_calls, g_overlay.draw_calls.avg);
    draw_string(x, y, buffer, 1.0f, 1.0f, 1.0f);
    y += 12;
    
    // Triangles
    snprintf(buffer, sizeof(buffer), "Triangles: %dk (avg: %.0fk)",
             g_overlay.total_triangles / 1000, g_overlay.triangles.avg / 1000);
    draw_string(x, y, buffer, 1.0f, 1.0f, 1.0f);
    y += 12;
    
    // State changes
    snprintf(buffer, sizeof(buffer), "State Changes: %d (avg: %.0f)",
             g_overlay.total_state_changes, g_overlay.state_changes.avg);
    draw_string(x, y, buffer, 1.0f, 1.0f, 1.0f);
    y += 20;
    
    // Graphs
    if (g_overlay.show_graphs) {
        draw_string(x, y, "Frame Time (ms)", 0.8f, 0.8f, 0.8f);
        y += 12;
        draw_graph(&g_overlay.frame_time, x, y, 300, 60, g_overlay.budget_ms);
        y += 70;
        
        draw_string(x, y, "FPS", 0.8f, 0.8f, 0.8f);
        y += 12;
        draw_graph(&g_overlay.fps, x, y, 300, 60, g_overlay.target_fps);
        y += 70;
    }
    
    // Optimization hints
    if (g_overlay.show_hints) {
        draw_string(x, y, "OPTIMIZATION HINTS:", 1.0f, 0.8f, 0.0f);
        y += 12;
        
        if (g_overlay.draw_calls.avg > 1000) {
            snprintf(buffer, sizeof(buffer), "! High draw calls - use instancing/batching");
            draw_string(x, y, buffer, 1.0f, 0.5f, 0.0f);
            y += 12;
        }
        
        if (g_overlay.triangles.avg > 5000000) {
            snprintf(buffer, sizeof(buffer), "! High triangle count - implement LODs");
            draw_string(x, y, buffer, 1.0f, 0.5f, 0.0f);
            y += 12;
        }
        
        if (g_overlay.state_changes.avg > 100) {
            snprintf(buffer, sizeof(buffer), "! Many state changes - sort by material");
            draw_string(x, y, buffer, 1.0f, 0.5f, 0.0f);
            y += 12;
        }
        
        if (g_overlay.frame_time.max > g_overlay.budget_ms * 2) {
            snprintf(buffer, sizeof(buffer), "! Frame spikes detected - profile CPU/GPU");
            draw_string(x, y, buffer, 1.0f, 0.5f, 0.0f);
            y += 12;
        }
    }
    
    // Restore GL state
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    if (depth_test) glEnable(GL_DEPTH_TEST);
    if (!blend) glDisable(GL_BLEND);
}

// Toggle overlay
void debug_overlay_toggle() {
    g_overlay.enabled = !g_overlay.enabled;
}

// Toggle graphs
void debug_overlay_toggle_graphs() {
    g_overlay.show_graphs = !g_overlay.show_graphs;
}

// Toggle hints
void debug_overlay_toggle_hints() {
    g_overlay.show_hints = !g_overlay.show_hints;
}