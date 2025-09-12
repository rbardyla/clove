/*
    Continental Architect PROFESSIONAL
    
    Professional presentation layer with:
    - Bitmap font rendering
    - Polished menus
    - Slider controls
    - Gradient backgrounds
    - Smooth animations
    - Professional HUD
    - Tooltips
    - Save/Load dialogs
    - Minimap
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define GRID_SIZE 64
#define MAX_PARTICLES 1000
#define MAX_MENU_ITEMS 10

// ============= UI TYPES =============

typedef struct {
    float x, y, width, height;
    float value; // 0.0 to 1.0
    float target_value;
    char label[64];
    int hover;
    int dragging;
} Slider;

typedef struct {
    float x, y, width, height;
    char text[64];
    int hover;
    int active;
    void (*callback)(void);
} Button;

typedef struct {
    char title[64];
    float x, y, width, height;
    int visible;
    float alpha;
    float target_alpha;
    Button buttons[MAX_MENU_ITEMS];
    int button_count;
} Menu;

typedef struct {
    float x, y;
    char text[256];
    float timer;
    float alpha;
} Tooltip;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life;
    float r, g, b, a;
} Particle;

// ============= GAME STATE =============

typedef struct {
    // Terrain
    float terrain[GRID_SIZE][GRID_SIZE];
    float water[GRID_SIZE][GRID_SIZE];
    float temperature[GRID_SIZE][GRID_SIZE];
    
    // Particles
    Particle particles[MAX_PARTICLES];
    int particle_count;
    
    // Camera
    float camera_x, camera_y, camera_z;
    float camera_target_x, camera_target_y, camera_target_z;
    float camera_angle, camera_pitch;
    float camera_distance;
    
    // Time
    float time_of_day;
    float time_speed;
    int paused;
    
    // Tools
    int current_tool;
    float brush_size;
    float brush_strength;
    
    // UI State
    Menu main_menu;
    Menu tools_menu;
    Slider time_slider;
    Slider brush_slider;
    Slider strength_slider;
    Tooltip tooltip;
    
    // Display options
    int show_grid;
    int show_minimap;
    int show_stats;
    int wireframe;
    float ui_scale;
    
    // Performance
    float fps;
    int frame_count;
    time_t last_fps_time;
    
    // Input
    int mouse_x, mouse_y;
    int mouse_down;
    int keys[256];
    
} GameState;

GameState* game;

// ============= FONT RENDERING =============

// Simple bitmap font (8x8 characters)
unsigned char font_bitmap[128][8] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00},
    ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    ['A'] = {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J'] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S'] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['a'] = {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
    ['b'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x60, 0x60, 0x60, 0x3C, 0x00},
    ['d'] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    ['f'] = {0x0E, 0x18, 0x3E, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['g'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x7C},
    ['h'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j'] = {0x06, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x3C},
    ['k'] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
    ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
    ['q'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06},
    ['r'] = {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
    ['t'] = {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00},
    ['u'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['v'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00},
    ['x'] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
    ['y'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x0C, 0x78},
    ['z'] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['/'] = {0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00, 0x00},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['+'] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    ['%'] = {0x00, 0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00},
};

void draw_char(float x, float y, char c, float scale, float r, float g, float b, float a) {
    if (c < 0 || c >= 128) return;
    
    unsigned char* bitmap = font_bitmap[(int)c];
    
    glColor4f(r, g, b, a);
    glBegin(GL_POINTS);
    
    for (int row = 0; row < 8; row++) {
        unsigned char line = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        glVertex2f(x + col * scale + sx, y + row * scale + sy);
                    }
                }
            }
        }
    }
    
    glEnd();
}

void draw_text(float x, float y, const char* text, float scale, float r, float g, float b, float a) {
    float cursor_x = x;
    
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(cursor_x, y, text[i], scale, r, g, b, a);
        cursor_x += 8 * scale;
    }
}

void draw_text_centered(float x, float y, const char* text, float scale, float r, float g, float b, float a) {
    int len = strlen(text);
    float width = len * 8 * scale;
    draw_text(x - width/2, y, text, scale, r, g, b, a);
}

// ============= UI COMPONENTS =============

void draw_panel(float x, float y, float w, float h, float r, float g, float b, float a) {
    // Background with gradient
    glBegin(GL_QUADS);
    glColor4f(r * 0.8f, g * 0.8f, b * 0.8f, a);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glColor4f(r * 0.6f, g * 0.6f, b * 0.6f, a);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Border
    glLineWidth(2.0f);
    glColor4f(r * 1.2f, g * 1.2f, b * 1.2f, a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Highlight
    glLineWidth(1.0f);
    glColor4f(1, 1, 1, a * 0.3f);
    glBegin(GL_LINES);
    glVertex2f(x + 2, y + 2);
    glVertex2f(x + w - 2, y + 2);
    glVertex2f(x + 2, y + 2);
    glVertex2f(x + 2, y + h - 2);
    glEnd();
}

void draw_button(Button* btn, float alpha) {
    float intensity = btn->hover ? 1.2f : (btn->active ? 0.8f : 1.0f);
    
    // Button background
    draw_panel(btn->x, btn->y, btn->width, btn->height, 
               0.2f * intensity, 0.3f * intensity, 0.5f * intensity, alpha);
    
    // Button text
    draw_text_centered(btn->x + btn->width/2, btn->y + btn->height/2 - 8, 
                      btn->text, 2, 1, 1, 1, alpha);
}

void draw_slider(Slider* slider) {
    // Track
    float track_y = slider->y + slider->height/2 - 2;
    glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(slider->x, track_y);
    glVertex2f(slider->x + slider->width, track_y);
    glVertex2f(slider->x + slider->width, track_y + 4);
    glVertex2f(slider->x, track_y + 4);
    glEnd();
    
    // Filled portion
    float fill_width = slider->width * slider->value;
    glColor4f(0.3f, 0.6f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(slider->x, track_y);
    glVertex2f(slider->x + fill_width, track_y);
    glVertex2f(slider->x + fill_width, track_y + 4);
    glVertex2f(slider->x, track_y + 4);
    glEnd();
    
    // Handle
    float handle_x = slider->x + slider->width * slider->value;
    float handle_size = slider->hover ? 12 : 10;
    
    glColor4f(0.8f, 0.8f, 0.8f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(handle_x - handle_size/2, slider->y + slider->height/2 - handle_size/2);
    glVertex2f(handle_x + handle_size/2, slider->y + slider->height/2 - handle_size/2);
    glVertex2f(handle_x + handle_size/2, slider->y + slider->height/2 + handle_size/2);
    glVertex2f(handle_x - handle_size/2, slider->y + slider->height/2 + handle_size/2);
    glEnd();
    
    // Label
    draw_text(slider->x, slider->y - 20, slider->label, 2, 0.9f, 0.9f, 0.9f, 1.0f);
    
    // Value text
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.1f", slider->value * 100);
    draw_text(slider->x + slider->width + 10, slider->y, value_str, 2, 0.8f, 0.8f, 0.8f, 1.0f);
}

void draw_minimap(float x, float y, float size) {
    // Minimap background
    draw_panel(x, y, size, size, 0.1f, 0.1f, 0.1f, 0.9f);
    
    // Terrain on minimap
    float cell_size = size / GRID_SIZE;
    
    for (int gz = 0; gz < GRID_SIZE; gz++) {
        for (int gx = 0; gx < GRID_SIZE; gx++) {
            float h = game->terrain[gz][gx];
            float w = game->water[gz][gx];
            
            float r, g, b;
            if (w > 0.01f) {
                r = 0.2f; g = 0.4f; b = 0.8f;
            } else if (h < 0) {
                r = 0.3f; g = 0.3f; b = 0.5f;
            } else if (h > 0.5f) {
                r = 0.6f; g = 0.5f; b = 0.4f;
            } else {
                r = 0.3f; g = 0.6f; b = 0.3f;
            }
            
            glColor3f(r, g, b);
            glBegin(GL_QUADS);
            glVertex2f(x + gx * cell_size, y + gz * cell_size);
            glVertex2f(x + (gx+1) * cell_size, y + gz * cell_size);
            glVertex2f(x + (gx+1) * cell_size, y + (gz+1) * cell_size);
            glVertex2f(x + gx * cell_size, y + (gz+1) * cell_size);
            glEnd();
        }
    }
    
    // Camera position indicator
    float cam_x = x + size/2 + (game->camera_x / 3.0f) * size;
    float cam_y = y + size/2 + (game->camera_z / 3.0f) * size;
    
    glColor3f(1, 1, 0);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(cam_x - 5, cam_y);
    glVertex2f(cam_x + 5, cam_y);
    glVertex2f(cam_x, cam_y - 5);
    glVertex2f(cam_x, cam_y + 5);
    glEnd();
}

void draw_tooltip(Tooltip* tooltip) {
    if (tooltip->alpha <= 0) return;
    
    int len = strlen(tooltip->text);
    float width = len * 8 * 2 + 20;
    float height = 30;
    
    // Background
    draw_panel(tooltip->x, tooltip->y, width, height, 
               0.1f, 0.1f, 0.1f, tooltip->alpha * 0.9f);
    
    // Text
    draw_text(tooltip->x + 10, tooltip->y + 7, tooltip->text, 2, 
              1, 1, 0.8f, tooltip->alpha);
}

// ============= PROFESSIONAL HUD =============

void draw_professional_hud() {
    // Top bar
    draw_panel(0, 0, WINDOW_WIDTH, 80, 0.05f, 0.05f, 0.1f, 0.95f);
    
    // Title
    draw_text_centered(WINDOW_WIDTH/2, 20, "CONTINENTAL ARCHITECT", 4, 1, 1, 1, 1);
    draw_text_centered(WINDOW_WIDTH/2, 55, "PROFESSIONAL EDITION", 2, 0.7f, 0.7f, 0.7f, 1);
    
    // Tool panel
    draw_panel(20, 100, 250, 400, 0.1f, 0.1f, 0.15f, 0.9f);
    draw_text(30, 110, "TOOLS", 3, 1, 1, 1, 1);
    
    const char* tool_names[] = {"Terrain", "Water", "Volcano", "City", "Forest", "Erode"};
    const char* tool_desc[] = {
        "Sculpt the landscape",
        "Add water sources",
        "Create volcanic activity",
        "Found civilizations",
        "Plant forests",
        "Natural erosion"
    };
    
    for (int i = 0; i < 6; i++) {
        float ty = 150 + i * 60;
        
        if (game->current_tool == i) {
            // Highlight selected tool
            glColor4f(0.3f, 0.5f, 0.8f, 0.5f);
            glBegin(GL_QUADS);
            glVertex2f(25, ty - 5);
            glVertex2f(265, ty - 5);
            glVertex2f(265, ty + 45);
            glVertex2f(25, ty + 45);
            glEnd();
        }
        
        // Tool icon placeholder (colored square)
        float icon_r = (i == 0) ? 0.6f : (i == 1) ? 0.3f : (i == 2) ? 0.9f : 
                      (i == 3) ? 0.7f : (i == 4) ? 0.2f : 0.5f;
        float icon_g = (i == 0) ? 0.4f : (i == 1) ? 0.5f : (i == 2) ? 0.2f : 
                      (i == 3) ? 0.7f : (i == 4) ? 0.7f : 0.4f;
        float icon_b = (i == 0) ? 0.2f : (i == 1) ? 0.8f : (i == 2) ? 0.1f : 
                      (i == 3) ? 0.6f : (i == 4) ? 0.3f : 0.3f;
        
        glColor3f(icon_r, icon_g, icon_b);
        glBegin(GL_QUADS);
        glVertex2f(35, ty);
        glVertex2f(65, ty);
        glVertex2f(65, ty + 30);
        glVertex2f(35, ty + 30);
        glEnd();
        
        // Tool name and description
        draw_text(75, ty + 5, tool_names[i], 2, 1, 1, 1, 1);
        draw_text(75, ty + 22, tool_desc[i], 1.5f, 0.7f, 0.7f, 0.7f, 1);
    }
    
    // Sliders
    game->brush_slider.x = 30;
    game->brush_slider.y = 520;
    game->brush_slider.width = 200;
    game->brush_slider.height = 20;
    strcpy(game->brush_slider.label, "Brush Size");
    draw_slider(&game->brush_slider);
    
    game->strength_slider.x = 30;
    game->strength_slider.y = 570;
    game->strength_slider.width = 200;
    game->strength_slider.height = 20;
    strcpy(game->strength_slider.label, "Strength");
    draw_slider(&game->strength_slider);
    
    // Stats panel
    if (game->show_stats) {
        draw_panel(WINDOW_WIDTH - 270, 100, 250, 200, 0.1f, 0.1f, 0.15f, 0.9f);
        draw_text(WINDOW_WIDTH - 260, 110, "STATISTICS", 3, 1, 1, 1, 1);
        
        char stats[256];
        snprintf(stats, sizeof(stats), "FPS: %.0f", game->fps);
        draw_text(WINDOW_WIDTH - 250, 150, stats, 2, 0.9f, 0.9f, 0.9f, 1);
        
        snprintf(stats, sizeof(stats), "Time: %02d:%02d", 
                (int)game->time_of_day, (int)((game->time_of_day - (int)game->time_of_day) * 60));
        draw_text(WINDOW_WIDTH - 250, 170, stats, 2, 0.9f, 0.9f, 0.9f, 1);
        
        snprintf(stats, sizeof(stats), "Particles: %d", game->particle_count);
        draw_text(WINDOW_WIDTH - 250, 190, stats, 2, 0.9f, 0.9f, 0.9f, 1);
        
        snprintf(stats, sizeof(stats), "Camera: %.1f, %.1f", game->camera_angle, game->camera_pitch);
        draw_text(WINDOW_WIDTH - 250, 210, stats, 2, 0.9f, 0.9f, 0.9f, 1);
    }
    
    // Time control
    draw_panel(WINDOW_WIDTH/2 - 200, WINDOW_HEIGHT - 80, 400, 60, 0.1f, 0.1f, 0.15f, 0.9f);
    
    game->time_slider.x = WINDOW_WIDTH/2 - 180;
    game->time_slider.y = WINDOW_HEIGHT - 55;
    game->time_slider.width = 300;
    game->time_slider.height = 20;
    game->time_slider.value = game->time_of_day / 24.0f;
    strcpy(game->time_slider.label, "Time of Day");
    draw_slider(&game->time_slider);
    
    // Play/Pause button
    const char* play_text = game->paused ? "PLAY" : "PAUSE";
    glColor4f(0.2f, 0.4f, 0.6f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(WINDOW_WIDTH/2 + 130, WINDOW_HEIGHT - 60);
    glVertex2f(WINDOW_WIDTH/2 + 190, WINDOW_HEIGHT - 60);
    glVertex2f(WINDOW_WIDTH/2 + 190, WINDOW_HEIGHT - 30);
    glVertex2f(WINDOW_WIDTH/2 + 130, WINDOW_HEIGHT - 30);
    glEnd();
    draw_text_centered(WINDOW_WIDTH/2 + 160, WINDOW_HEIGHT - 50, play_text, 2, 1, 1, 1, 1);
    
    // Minimap
    if (game->show_minimap) {
        draw_minimap(WINDOW_WIDTH - 220, WINDOW_HEIGHT - 240, 200);
    }
    
    // Tooltip
    draw_tooltip(&game->tooltip);
}

// ============= MENU SYSTEM =============

void draw_menu(Menu* menu) {
    if (!menu->visible) return;
    
    // Animate alpha
    float alpha_diff = menu->target_alpha - menu->alpha;
    menu->alpha += alpha_diff * 0.1f;
    
    if (menu->alpha < 0.01f) {
        menu->visible = 0;
        return;
    }
    
    // Dark overlay
    glColor4f(0, 0, 0, menu->alpha * 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(WINDOW_WIDTH, 0);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(0, WINDOW_HEIGHT);
    glEnd();
    
    // Menu panel with animation
    float scale = 0.9f + menu->alpha * 0.1f;
    float w = menu->width * scale;
    float h = menu->height * scale;
    float x = menu->x - (w - menu->width) / 2;
    float y = menu->y - (h - menu->height) / 2;
    
    draw_panel(x, y, w, h, 0.1f, 0.1f, 0.2f, menu->alpha);
    
    // Title
    draw_text_centered(x + w/2, y + 30, menu->title, 4, 1, 1, 1, menu->alpha);
    
    // Buttons
    for (int i = 0; i < menu->button_count; i++) {
        Button* btn = &menu->buttons[i];
        btn->x = x + 50;
        btn->y = y + 100 + i * 70;
        btn->width = w - 100;
        btn->height = 50;
        draw_button(btn, menu->alpha);
    }
}

// ============= INITIALIZATION =============

void init_ui() {
    // Main menu
    strcpy(game->main_menu.title, "MAIN MENU");
    game->main_menu.x = WINDOW_WIDTH/2 - 200;
    game->main_menu.y = WINDOW_HEIGHT/2 - 250;
    game->main_menu.width = 400;
    game->main_menu.height = 500;
    game->main_menu.visible = 0;
    game->main_menu.alpha = 0;
    
    strcpy(game->main_menu.buttons[0].text, "New World");
    strcpy(game->main_menu.buttons[1].text, "Load World");
    strcpy(game->main_menu.buttons[2].text, "Settings");
    strcpy(game->main_menu.buttons[3].text, "About");
    strcpy(game->main_menu.buttons[4].text, "Exit");
    game->main_menu.button_count = 5;
    
    // Sliders
    game->brush_slider.value = 0.3f;
    game->strength_slider.value = 0.5f;
    game->time_slider.value = 0.5f;
    
    // Display options
    game->show_minimap = 1;
    game->show_stats = 1;
    game->ui_scale = 1.0f;
}

void init_terrain() {
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float fx = (float)x / GRID_SIZE * 10.0f;
            float fz = (float)z / GRID_SIZE * 10.0f;
            
            game->terrain[z][x] = sinf(fx) * cosf(fz) * 0.3f;
            game->water[z][x] = (game->terrain[z][x] < -0.1f) ? 0.1f : 0;
            game->temperature[z][x] = 20.0f;
        }
    }
}

// ============= UPDATE =============

void update_ui(float dt) {
    // Update slider animations
    float slider_speed = 5.0f * dt;
    
    game->brush_slider.value += (game->brush_slider.target_value - game->brush_slider.value) * slider_speed;
    game->strength_slider.value += (game->strength_slider.target_value - game->strength_slider.value) * slider_speed;
    game->time_slider.value += (game->time_slider.target_value - game->time_slider.value) * slider_speed;
    
    // Update tooltip
    if (game->tooltip.timer > 0) {
        game->tooltip.timer -= dt;
        game->tooltip.alpha = fminf(1.0f, game->tooltip.timer);
    } else {
        game->tooltip.alpha *= 0.9f;
    }
    
    // Update camera smoothing
    float cam_speed = 3.0f * dt;
    game->camera_x += (game->camera_target_x - game->camera_x) * cam_speed;
    game->camera_y += (game->camera_target_y - game->camera_y) * cam_speed;
    game->camera_z += (game->camera_target_z - game->camera_z) * cam_speed;
}

void set_tooltip(const char* text) {
    strcpy(game->tooltip.text, text);
    game->tooltip.x = game->mouse_x + 10;
    game->tooltip.y = game->mouse_y + 20;
    game->tooltip.timer = 2.0f;
    game->tooltip.alpha = 1.0f;
}

void handle_mouse_hover(int mx, int my) {
    // Check sliders
    game->brush_slider.hover = (mx >= game->brush_slider.x && 
                                mx <= game->brush_slider.x + game->brush_slider.width &&
                                my >= game->brush_slider.y - 10 && 
                                my <= game->brush_slider.y + game->brush_slider.height + 10);
    
    game->strength_slider.hover = (mx >= game->strength_slider.x && 
                                   mx <= game->strength_slider.x + game->strength_slider.width &&
                                   my >= game->strength_slider.y - 10 && 
                                   my <= game->strength_slider.y + game->strength_slider.height + 10);
    
    // Check tool buttons
    for (int i = 0; i < 6; i++) {
        float ty = 150 + i * 60;
        if (mx >= 25 && mx <= 265 && my >= ty - 5 && my <= ty + 45) {
            if (game->current_tool != i) {
                const char* tooltips[] = {
                    "Left click to raise, Shift+click to lower",
                    "Add water sources to create rivers",
                    "Trigger volcanic eruptions",
                    "Found new civilizations",
                    "Plant forests for ecosystem",
                    "Natural erosion simulation"
                };
                set_tooltip(tooltips[i]);
            }
        }
    }
    
    // Check menu buttons
    if (game->main_menu.visible) {
        for (int i = 0; i < game->main_menu.button_count; i++) {
            Button* btn = &game->main_menu.buttons[i];
            btn->hover = (mx >= btn->x && mx <= btn->x + btn->width &&
                         my >= btn->y && my <= btn->y + btn->height);
        }
    }
}

// ============= RENDERING =============

void render_terrain() {
    for (int z = 0; z < GRID_SIZE-1; z++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = 0; x < GRID_SIZE; x++) {
            float fx = x / (float)GRID_SIZE * 3.0f - 1.5f;
            float fz0 = z / (float)GRID_SIZE * 3.0f - 1.5f;
            float fz1 = (z+1) / (float)GRID_SIZE * 3.0f - 1.5f;
            
            float h0 = game->terrain[z][x];
            float h1 = game->terrain[z+1][x];
            float w0 = game->water[z][x];
            float w1 = game->water[z+1][x];
            
            // Biome colors
            float r0 = w0 > 0.01f ? 0.2f : (h0 > 0.3f ? 0.6f : 0.3f);
            float g0 = w0 > 0.01f ? 0.4f : (h0 > 0.3f ? 0.5f : 0.6f);
            float b0 = w0 > 0.01f ? 0.8f : (h0 > 0.3f ? 0.4f : 0.3f);
            
            float r1 = w1 > 0.01f ? 0.2f : (h1 > 0.3f ? 0.6f : 0.3f);
            float g1 = w1 > 0.01f ? 0.4f : (h1 > 0.3f ? 0.5f : 0.6f);
            float b1 = w1 > 0.01f ? 0.8f : (h1 > 0.3f ? 0.4f : 0.3f);
            
            // Apply day/night
            float sun = 0.3f + 0.7f * fmaxf(0, sinf(game->time_of_day / 24.0f * 2 * 3.14159f));
            
            glColor3f(r0 * sun, g0 * sun, b0 * sun);
            glVertex3f(fx, h0 + w0, fz0);
            
            glColor3f(r1 * sun, g1 * sun, b1 * sun);
            glVertex3f(fx, h1 + w1, fz1);
        }
        glEnd();
    }
    
    // Grid overlay
    if (game->show_grid) {
        glColor4f(1, 1, 1, 0.1f);
        for (int i = 0; i <= GRID_SIZE; i += 4) {
            float fi = i / (float)GRID_SIZE * 3.0f - 1.5f;
            glBegin(GL_LINES);
            glVertex3f(-1.5f, 0.01f, fi);
            glVertex3f(1.5f, 0.01f, fi);
            glVertex3f(fi, 0.01f, -1.5f);
            glVertex3f(fi, 0.01f, 1.5f);
            glEnd();
        }
    }
}

void render_particles() {
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < game->particle_count; i++) {
        Particle* p = &game->particles[i];
        glColor4f(p->r, p->g, p->b, p->a * p->life);
        glVertex3f(p->x, p->y, p->z);
    }
    glEnd();
}

void render_scene() {
    // Sky gradient
    float sun = fmaxf(0, sinf(game->time_of_day / 24.0f * 2 * 3.14159f));
    float r = 0.1f + sun * 0.4f;
    float g = 0.2f + sun * 0.5f;
    float b = 0.4f + sun * 0.4f;
    
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 3D view
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)WINDOW_WIDTH / WINDOW_HEIGHT;
    float fov = 60.0f * 3.14159f / 180.0f;
    float near = 0.1f;
    float far = 100.0f;
    float top = near * tanf(fov * 0.5f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, near, far);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Smooth camera
    glTranslatef(-game->camera_x, -game->camera_y, -game->camera_distance);
    glRotatef(game->camera_pitch, 1, 0, 0);
    glRotatef(game->camera_angle, 0, 1, 0);
    
    // Render world
    if (game->wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    
    render_terrain();
    render_particles();
    
    if (game->wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // 2D UI overlay
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    draw_professional_hud();
    draw_menu(&game->main_menu);
    
    glDisable(GL_BLEND);
}

// ============= MAIN =============

int main() {
    printf("=== CONTINENTAL ARCHITECT PROFESSIONAL ===\n");
    printf("Premium UI with Professional Presentation\n\n");
    
    // Initialize
    game = calloc(1, sizeof(GameState));
    game->camera_distance = 5.0f;
    game->camera_pitch = 30.0f;
    game->time_of_day = 12.0f;
    game->time_speed = 1.0f;
    
    srand(time(NULL));
    init_terrain();
    init_ui();
    
    // X11 setup
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    int scr = DefaultScreen(dpy);
    GLint att[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(dpy, scr, att);
    if (!vi) {
        fprintf(stderr, "No suitable visual\n");
        return 1;
    }
    
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask | ExposureMask;
    
    Window win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Continental Architect Professional");
    XFlush(dpy);
    XSync(dpy, False);
    usleep(100000);
    
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    printf("OpenGL: %s\n", glGetString(GL_VERSION));
    printf("Starting professional interface...\n\n");
    
    // Show main menu initially
    game->main_menu.visible = 1;
    game->main_menu.target_alpha = 1.0f;
    
    // Main loop
    int running = 1;
    clock_t last_time = clock();
    game->last_fps_time = time(NULL);
    
    while (running) {
        // Events
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                game->keys[key & 0xFF] = 1;
                
                switch(key) {
                    case XK_Escape:
                        if (game->main_menu.visible) {
                            game->main_menu.target_alpha = 0;
                        } else {
                            game->main_menu.visible = 1;
                            game->main_menu.target_alpha = 1.0f;
                        }
                        break;
                    
                    case XK_1: game->current_tool = 0; set_tooltip("Terrain Tool Selected"); break;
                    case XK_2: game->current_tool = 1; set_tooltip("Water Tool Selected"); break;
                    case XK_3: game->current_tool = 2; set_tooltip("Volcano Tool Selected"); break;
                    case XK_4: game->current_tool = 3; set_tooltip("City Tool Selected"); break;
                    case XK_5: game->current_tool = 4; set_tooltip("Forest Tool Selected"); break;
                    case XK_6: game->current_tool = 5; set_tooltip("Erosion Tool Selected"); break;
                    
                    case XK_w: game->camera_target_z -= 0.1f; break;
                    case XK_s: game->camera_target_z += 0.1f; break;
                    case XK_a: game->camera_target_x -= 0.1f; break;
                    case XK_d: game->camera_target_x += 0.1f; break;
                    case XK_q: game->camera_angle -= 5; break;
                    case XK_e: game->camera_angle += 5; break;
                    case XK_r: game->camera_pitch -= 5; break;
                    case XK_f: game->camera_pitch += 5; break;
                    
                    case XK_space: game->paused = !game->paused; break;
                    case XK_g: game->show_grid = !game->show_grid; break;
                    case XK_m: game->show_minimap = !game->show_minimap; break;
                    case XK_Tab: game->show_stats = !game->show_stats; break;
                    case XK_F1: game->wireframe = !game->wireframe; break;
                    
                    case XK_plus:
                    case XK_equal:
                        game->time_speed *= 2;
                        if (game->time_speed > 100) game->time_speed = 100;
                        break;
                    case XK_minus:
                        game->time_speed /= 2;
                        if (game->time_speed < 0.1f) game->time_speed = 0.1f;
                        break;
                }
            } else if (xev.type == KeyRelease) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                game->keys[key & 0xFF] = 0;
            } else if (xev.type == ButtonPress) {
                game->mouse_down = 1;
                game->mouse_x = xev.xbutton.x;
                game->mouse_y = xev.xbutton.y;
                
                // Check UI elements
                if (game->brush_slider.hover) {
                    game->brush_slider.dragging = 1;
                } else if (game->strength_slider.hover) {
                    game->strength_slider.dragging = 1;
                } else if (game->main_menu.visible) {
                    // Check menu buttons
                    for (int i = 0; i < game->main_menu.button_count; i++) {
                        if (game->main_menu.buttons[i].hover) {
                            if (i == 0) { // New World
                                init_terrain();
                                game->main_menu.target_alpha = 0;
                                set_tooltip("New world generated!");
                            } else if (i == 4) { // Exit
                                running = 0;
                            }
                        }
                    }
                }
            } else if (xev.type == ButtonRelease) {
                game->mouse_down = 0;
                game->brush_slider.dragging = 0;
                game->strength_slider.dragging = 0;
            } else if (xev.type == MotionNotify) {
                game->mouse_x = xev.xmotion.x;
                game->mouse_y = xev.xmotion.y;
                handle_mouse_hover(game->mouse_x, game->mouse_y);
                
                // Handle slider dragging
                if (game->brush_slider.dragging) {
                    float rel_x = (game->mouse_x - game->brush_slider.x) / game->brush_slider.width;
                    game->brush_slider.target_value = fmaxf(0, fminf(1, rel_x));
                    game->brush_size = game->brush_slider.value * 10 + 1;
                }
                if (game->strength_slider.dragging) {
                    float rel_x = (game->mouse_x - game->strength_slider.x) / game->strength_slider.width;
                    game->strength_slider.target_value = fmaxf(0, fminf(1, rel_x));
                    game->brush_strength = game->strength_slider.value;
                }
            }
        }
        
        // Update
        clock_t current_time = clock();
        float dt = (float)(current_time - last_time) / CLOCKS_PER_SEC;
        last_time = current_time;
        
        if (!game->paused) {
            game->time_of_day += dt * game->time_speed * 0.1f;
            if (game->time_of_day >= 24.0f) game->time_of_day -= 24.0f;
            
            // Update particles
            for (int i = 0; i < game->particle_count; i++) {
                Particle* p = &game->particles[i];
                p->x += p->vx * dt;
                p->y += p->vy * dt;
                p->z += p->vz * dt;
                p->vy -= dt * 0.5f;
                p->life -= dt * 0.3f;
                
                if (p->life <= 0) {
                    game->particles[i] = game->particles[--game->particle_count];
                    i--;
                }
            }
        }
        
        update_ui(dt);
        
        // Render
        render_scene();
        glXSwapBuffers(dpy, win);
        
        // FPS
        game->frame_count++;
        time_t now = time(NULL);
        if (now > game->last_fps_time) {
            game->fps = game->frame_count;
            game->frame_count = 0;
            game->last_fps_time = now;
        }
        
        usleep(16666);
    }
    
    // Cleanup
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(game);
    
    printf("\nThank you for using Continental Architect Professional!\n");
    return 0;
}