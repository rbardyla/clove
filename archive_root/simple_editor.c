/*
    Simple Handmade Game Editor
    Built on the working renderer foundation
    
    Features:
    - Full screen window (1280x720)  
    - Basic input handling
    - Simple immediate mode GUI
    - Real-time game development
*/

#include "systems/renderer/handmade_renderer.h"
#include "systems/renderer/handmade_platform.h" 
#include "systems/renderer/handmade_opengl.h"
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

// Simple tile/sprite palette
#define PALETTE_SIZE 8
#define GRID_WIDTH 16
#define GRID_HEIGHT 12

typedef struct {
    v3 color;
    char name[32];
} palette_entry;

// Editor state
typedef struct {
    bool show_menu;
    bool show_game_view;
    bool show_inspector; 
    bool show_console;
    bool show_palette;
    bool show_canvas;
    
    // Input
    bool keys[256];
    int mouse_x, mouse_y;
    bool mouse_left, mouse_right;
    bool mouse_left_pressed;  // For detecting clicks
    
    // Game state
    bool game_running;
    float cube_rotation;
    
    // Palette system
    palette_entry palette[PALETTE_SIZE];
    int selected_palette_index;
    
    // Drawing canvas (simple grid)
    int canvas[GRID_HEIGHT][GRID_WIDTH];  // Indices into palette
    
} editor_state;

// Simple bitmap font system - handmade 8x8 font
static const unsigned char font_8x8[128][8] = {
    // ASCII 32-47 (Space, punctuation)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x06,0x00}, // ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    
    // ASCII 48-57 (Numbers)
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9
    
    // ASCII 58-64 (Punctuation)
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x06,0x00}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    
    // ASCII 65-90 (Uppercase letters)
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // E
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // L
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z
};

// Draw bitmap text with proper font
void draw_text(renderer_state *renderer, const char *text, int x, int y, v3 color) {
    int char_width = 8;
    int char_height = 8;
    float scale = 1.5f;  // Much larger scale for readability
    
    // Convert to 3D coordinates
    float start_x = (float)x / 100.0f - 6.0f;
    float start_y = 4.0f - (float)y / 100.0f;
    
    int len = strlen(text);
    for (int i = 0; i < len && i < 100; i++) {
        char c = text[i];
        if (c == ' ') continue;  // Skip spaces
        
        // Clamp character to valid range
        if (c < 32 || c > 126) c = '?';
        
        // Get font data for character
        const unsigned char *glyph = font_8x8[c - 32];
        
        float char_x = start_x + (float)(i * char_width) / 200.0f * scale;
        
        // Draw each pixel of the character
        for (int py = 0; py < 8; py++) {
            unsigned char row = glyph[py];
            for (int px = 0; px < 8; px++) {
                if (row & (0x80 >> px)) {  // Check if pixel is set
                    float pixel_x = char_x + (float)px / 400.0f * scale;
                    float pixel_y = start_y - (float)py / 400.0f * scale;
                    
                    // Draw pixel as small cube
                    m4x4 pixel_transform = m4x4_translation(
                        pixel_x, pixel_y, 0.01f);
                    pixel_transform = m4x4_multiply(pixel_transform, 
                        m4x4_scale(0.01f * scale, 0.01f * scale, 0.01f));
                    
                    renderer_set_uniform_v3(renderer->basic_shader, "objectColor", color);
                    renderer_draw_mesh(renderer, renderer->cube_mesh, pixel_transform);
                }
            }
        }
    }
}

bool draw_button(renderer_state *renderer, const char *text, int x, int y, int w, int h, bool clicked, editor_state *editor) {
    // Convert 2D screen coordinates to 3D world coordinates for rendering
    // We'll position buttons in a 2D plane at z=0
    
    // Calculate 3D box coordinates (screen space to world space)
    // For simplicity, map screen coordinates directly to world coordinates
    float left = (float)x / 100.0f - 6.0f;   // Scale and center
    float right = (float)(x + w) / 100.0f - 6.0f;
    float bottom = 4.0f - (float)(y + h) / 100.0f;  // Flip Y and scale 
    float top = 4.0f - (float)y / 100.0f;
    
    v3 min = {left, bottom, -0.1f};
    v3 max = {right, top, 0.1f};
    
    // Mouse hit testing - check if mouse is inside button bounds
    bool mouse_inside = (editor->mouse_x >= x && editor->mouse_x <= x + w &&
                        editor->mouse_y >= y && editor->mouse_y <= y + h);
    
    // Choose button color based on state
    v3 button_color;
    if (clicked) {
        button_color = (v3){0.9f, 0.9f, 0.9f};  // Light gray when clicked
    } else if (mouse_inside) {
        button_color = (v3){0.7f, 0.9f, 1.0f};  // Light blue when hovered
    } else {
        button_color = (v3){0.5f, 0.7f, 0.9f};  // Default blue
    }
    
    // Draw the button using cube mesh since renderer_draw_box isn't implemented
    float center_x = (left + right) * 0.5f;
    float center_y = (bottom + top) * 0.5f;
    float width = right - left;
    float height = top - bottom;
    
    m4x4 button_transform = m4x4_translation(center_x, center_y, 0.0f);
    button_transform = m4x4_multiply(button_transform, m4x4_scale(width * 0.5f, height * 0.5f, 0.1f));
    
    // Set button color uniform
    renderer_set_uniform_v3(renderer->basic_shader, "objectColor", button_color);
    renderer_draw_mesh(renderer, renderer->cube_mesh, button_transform);
    
    // Draw button outline
    v3 outline_color = {0.2f, 0.2f, 0.2f};
    // Draw outline using lines (simple wireframe)
    renderer_draw_line(renderer, (v3){left, bottom, 0.0f}, (v3){right, bottom, 0.0f}, outline_color);
    renderer_draw_line(renderer, (v3){right, bottom, 0.0f}, (v3){right, top, 0.0f}, outline_color);
    renderer_draw_line(renderer, (v3){right, top, 0.0f}, (v3){left, top, 0.0f}, outline_color);
    renderer_draw_line(renderer, (v3){left, top, 0.0f}, (v3){left, bottom, 0.0f}, outline_color);
    
    bool button_clicked = mouse_inside && editor->mouse_left_pressed;
    
    // Debug output for button interaction
    static int debug_counter = 0;
    if (button_clicked || (mouse_inside && debug_counter++ % 60 == 0)) {
        printf("[GUI] Button '%s' at (%d,%d) - Mouse: (%d,%d) %s %s\n", 
               text, x, y, editor->mouse_x, editor->mouse_y,
               mouse_inside ? "INSIDE" : "outside",
               button_clicked ? "CLICKED!" : "");
    }
    
    return button_clicked;
}

// Draw a palette color swatch
bool draw_palette_swatch(renderer_state *renderer, palette_entry *entry, int x, int y, int size, bool selected, bool hovered, editor_state *editor) {
    // Convert to 3D coordinates
    float left = (float)x / 100.0f - 6.0f;
    float right = (float)(x + size) / 100.0f - 6.0f;
    float bottom = 4.0f - (float)(y + size) / 100.0f;
    float top = 4.0f - (float)y / 100.0f;
    
    // Mouse hit testing
    bool mouse_inside = (editor->mouse_x >= x && editor->mouse_x <= x + size &&
                        editor->mouse_y >= y && editor->mouse_y <= y + size);
    
    // Choose swatch color
    v3 swatch_color = entry->color;
    if (selected) {
        // Add white tint to selected color
        swatch_color = (v3){
            fminf(1.0f, swatch_color.x + 0.3f),
            fminf(1.0f, swatch_color.y + 0.3f),
            fminf(1.0f, swatch_color.z + 0.3f)
        };
    } else if (mouse_inside) {
        // Slightly brighten on hover
        swatch_color = (v3){
            fminf(1.0f, swatch_color.x + 0.1f),
            fminf(1.0f, swatch_color.y + 0.1f),
            fminf(1.0f, swatch_color.z + 0.1f)
        };
    }
    
    // Draw the color swatch
    float center_x = (left + right) * 0.5f;
    float center_y = (bottom + top) * 0.5f;
    float width = right - left;
    float height = top - bottom;
    
    m4x4 swatch_transform = m4x4_translation(center_x, center_y, 0.0f);
    swatch_transform = m4x4_multiply(swatch_transform, m4x4_scale(width * 0.5f, height * 0.5f, 0.1f));
    
    renderer_set_uniform_v3(renderer->basic_shader, "objectColor", swatch_color);
    renderer_draw_mesh(renderer, renderer->cube_mesh, swatch_transform);
    
    // Draw selection/hover outline
    v3 outline_color;
    if (selected) {
        outline_color = (v3){1.0f, 1.0f, 1.0f};  // White for selected
    } else if (mouse_inside) {
        outline_color = (v3){0.8f, 0.8f, 0.8f};  // Light gray for hover
    } else {
        outline_color = (v3){0.3f, 0.3f, 0.3f};  // Dark gray for normal
    }
    
    // Draw outline
    renderer_draw_line(renderer, (v3){left, bottom, 0.01f}, (v3){right, bottom, 0.01f}, outline_color);
    renderer_draw_line(renderer, (v3){right, bottom, 0.01f}, (v3){right, top, 0.01f}, outline_color);
    renderer_draw_line(renderer, (v3){right, top, 0.01f}, (v3){left, top, 0.01f}, outline_color);
    renderer_draw_line(renderer, (v3){left, top, 0.01f}, (v3){left, bottom, 0.01f}, outline_color);
    
    return mouse_inside && editor->mouse_left_pressed;
}

// Draw the drawing canvas
void draw_canvas(renderer_state *renderer, editor_state *editor) {
    int canvas_start_x = 500;  // Move right to avoid overlap
    int canvas_start_y = 100;
    int tile_size = 30;  // Bigger tiles for easier clicking
    
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int tile_x = canvas_start_x + x * tile_size;
            int tile_y = canvas_start_y + y * tile_size;
            
            // Get color from palette
            int palette_index = editor->canvas[y][x];
            v3 tile_color;
            if (palette_index >= 0 && palette_index < PALETTE_SIZE) {
                tile_color = editor->palette[palette_index].color;
            } else {
                tile_color = (v3){0.1f, 0.1f, 0.1f};  // Dark gray for empty
            }
            
            // Convert to 3D coordinates
            float left = (float)tile_x / 100.0f - 6.0f;
            float right = (float)(tile_x + tile_size) / 100.0f - 6.0f;
            float bottom = 4.0f - (float)(tile_y + tile_size) / 100.0f;
            float top = 4.0f - (float)tile_y / 100.0f;
            
            // Draw tile
            float center_x = (left + right) * 0.5f;
            float center_y = (bottom + top) * 0.5f;
            float width = right - left;
            float height = top - bottom;
            
            m4x4 tile_transform = m4x4_translation(center_x, center_y, -0.01f);
            tile_transform = m4x4_multiply(tile_transform, m4x4_scale(width * 0.5f, height * 0.5f, 0.05f));
            
            renderer_set_uniform_v3(renderer->basic_shader, "objectColor", tile_color);
            renderer_draw_mesh(renderer, renderer->cube_mesh, tile_transform);
            
            // Draw grid lines
            v3 grid_color = {0.4f, 0.4f, 0.4f};
            renderer_draw_line(renderer, (v3){left, bottom, 0.0f}, (v3){right, bottom, 0.0f}, grid_color);
            renderer_draw_line(renderer, (v3){right, bottom, 0.0f}, (v3){right, top, 0.0f}, grid_color);
            
            // Handle mouse interaction for drawing
            bool mouse_inside = (editor->mouse_x >= tile_x && editor->mouse_x <= tile_x + tile_size &&
                               editor->mouse_y >= tile_y && editor->mouse_y <= tile_y + tile_size);
            
            if (mouse_inside && editor->mouse_left_pressed) {
                editor->canvas[y][x] = editor->selected_palette_index;
                printf("[CANVAS] Painted tile (%d,%d) with palette %d\n", x, y, editor->selected_palette_index);
            }
        }
    }
}

void update_editor(editor_state *editor, platform_state *platform) {
    // Handle input
    keyboard_state *keyboard = &platform->input.keyboard;
    mouse_state *mouse = &platform->input.mouse;
    
    // Update mouse state
    platform_get_mouse_pos(platform, &editor->mouse_x, &editor->mouse_y);
    editor->mouse_left_pressed = platform_mouse_pressed(platform, MOUSE_LEFT);
    
    // Debug mouse input (print occasionally)
    static int mouse_debug_counter = 0;
    if (editor->mouse_left_pressed || mouse_debug_counter++ % 120 == 0) {
        printf("[MOUSE] Position: (%d, %d), Click: %s\n", 
               editor->mouse_x, editor->mouse_y, 
               editor->mouse_left_pressed ? "YES" : "no");
    }
    
    // Toggle menu with F1 
    if (platform_key_pressed(platform, KEY_F1)) {
        editor->show_menu = !editor->show_menu;
    }
    
    // Toggle palette with F2
    if (platform_key_pressed(platform, KEY_F2)) {
        editor->show_palette = !editor->show_palette;
        printf("[EDITOR] Palette %s\n", editor->show_palette ? "SHOWN" : "HIDDEN");
    }
    
    // Toggle canvas with F3
    if (platform_key_pressed(platform, KEY_F3)) {
        editor->show_canvas = !editor->show_canvas;
        printf("[EDITOR] Canvas %s\n", editor->show_canvas ? "SHOWN" : "HIDDEN");
    }
    
    // Toggle game with space
    if (platform_key_pressed(platform, KEY_SPACE)) {
        editor->game_running = !editor->game_running;
        printf("[EDITOR] Game %s\n", editor->game_running ? "STARTED" : "STOPPED");
    }
    
    // Palette selection with number keys
    for (int i = 0; i < PALETTE_SIZE && i < 8; i++) {
        if (platform_key_pressed(platform, KEY_1 + i)) {
            editor->selected_palette_index = i;
            printf("[PALETTE] Selected color %d: %s\n", i, editor->palette[i].name);
        }
    }
    
    // Update game simulation
    if (editor->game_running) {
        editor->cube_rotation += 1.0f;
    }
}

void render_editor(editor_state *editor, renderer_state *renderer) {
    // Choose background color based on game state
    v4 bg_color;
    if (editor->game_running) {
        bg_color = (v4){0.3f, 0.2f, 0.1f, 1.0f};  // Dark orange when running
    } else if (editor->show_menu) {
        bg_color = (v4){0.1f, 0.2f, 0.3f, 1.0f};  // Dark blue when menu is shown
    } else {
        bg_color = (v4){0.1f, 0.1f, 0.2f, 1.0f};  // Very dark when hidden
    }
    
    // Clear background
    renderer_clear(renderer, bg_color, true, true);
    
    // Set up camera for 2D GUI rendering (orthographic-like view)
    v3 camera_pos = {0.0f, 0.0f, 8.0f};      // Back away from origin
    v3 camera_target = {0.0f, 0.0f, 0.0f};   // Look at origin
    v3 camera_up = {0.0f, 1.0f, 0.0f};       // Standard up vector
    v3 camera_forward = v3_normalize(v3_sub(camera_target, camera_pos));
    renderer_set_camera(renderer, camera_pos, camera_forward, camera_up);
    
    // Begin 3D rendering
    renderer_begin_frame(renderer);
    
    // Use basic shader for GUI elements
    renderer_use_shader(renderer, renderer->basic_shader);
    
    // Draw GUI elements if menu is shown
    if (editor->show_menu) {
        // Draw title
        draw_text(renderer, "HANDMADE GAME EDITOR", 10, 50, (v3){1.0f, 1.0f, 0.4f});
        
        // Draw instructions
        draw_text(renderer, "F1: Toggle Menu", 10, 100, (v3){0.8f, 0.8f, 0.8f});
        draw_text(renderer, "F2: Toggle Palette", 10, 130, (v3){0.8f, 0.8f, 0.8f});
        draw_text(renderer, "F3: Toggle Canvas", 10, 160, (v3){0.8f, 0.8f, 0.8f});
        draw_text(renderer, "SPACE: Start/Stop Game", 10, 190, (v3){0.8f, 0.8f, 0.8f});
        draw_text(renderer, "1-8: Select Palette Color", 10, 220, (v3){0.8f, 0.8f, 0.8f});
        draw_text(renderer, "ESC: Quit", 10, 250, (v3){0.8f, 0.8f, 0.8f});
        
        // Draw interactive buttons (adjusted for 1280x720 fullscreen)
        if (draw_button(renderer, "Start Game", 50, 300, 150, 50, false, editor)) {
            editor->game_running = true;
            printf("[BUTTON] Start Game clicked!\n");
        }
        
        if (draw_button(renderer, "Stop Game", 220, 300, 150, 50, false, editor)) {
            editor->game_running = false;
            printf("[BUTTON] Stop Game clicked!\n");
        }
        
        if (draw_button(renderer, "Clear Canvas", 50, 370, 150, 50, false, editor)) {
            // Clear the canvas
            for (int y = 0; y < GRID_HEIGHT; y++) {
                for (int x = 0; x < GRID_WIDTH; x++) {
                    editor->canvas[y][x] = -1;
                }
            }
            printf("[BUTTON] Canvas cleared!\n");
        }
    }
    
    // Draw palette panel (repositioned for fullscreen)
    if (editor->show_palette) {
        draw_text(renderer, "COLOR PALETTE", 10, 450, (v3){0.9f, 0.5f, 0.9f});
        
        for (int i = 0; i < PALETTE_SIZE; i++) {
            int swatch_x = 10 + (i % 4) * 100;  // More spacing
            int swatch_y = 490 + (i / 4) * 100;  // Lower position, more spacing
            bool selected = (i == editor->selected_palette_index);
            
            if (draw_palette_swatch(renderer, &editor->palette[i], swatch_x, swatch_y, 80, selected, false, editor)) {
                editor->selected_palette_index = i;
                printf("[PALETTE] Selected color %d: %s\n", i, editor->palette[i].name);
            }
            
            // Draw palette number and name
            char label[32];
            sprintf(label, "%d:%s", i + 1, editor->palette[i].name);
            v3 text_color = selected ? (v3){1.0f, 1.0f, 1.0f} : (v3){0.7f, 0.7f, 0.7f};
            draw_text(renderer, label, swatch_x, swatch_y + 85, text_color);
        }
    }
    
    // Draw canvas panel
    if (editor->show_canvas) {
        draw_text(renderer, "LEVEL EDITOR", 500, 50, (v3){0.5f, 0.9f, 0.5f});
        
        // Show current tool info
        char tool_info[64];
        sprintf(tool_info, "BRUSH: %s", editor->palette[editor->selected_palette_index].name);
        draw_text(renderer, tool_info, 500, 75, editor->palette[editor->selected_palette_index].color);
        
        draw_canvas(renderer, editor);
    }
    
    // Always show status at bottom
    char status[100];
    sprintf(status, "Game: %s | Selected: %s", 
            editor->game_running ? "RUNNING" : "STOPPED",
            editor->palette[editor->selected_palette_index].name);
    v3 status_color = editor->game_running ? (v3){0.3f, 1.0f, 0.3f} : (v3){0.8f, 0.8f, 0.8f};
    draw_text(renderer, status, 10, 680, status_color);
    
    // End frame
    renderer_end_frame(renderer);
}

int main() {
    printf("=== Simple Handmade Game Editor ===\n");
    printf("Building editor on working renderer foundation...\n\n");
    
    // Initialize platform (same as working renderer)
    window_config config = {
        .title = "Simple Handmade Game Editor",
        .width = 1280,
        .height = 720,
        .fullscreen = false,
        .vsync = true,
        .resizable = true,
        .samples = 4
    };
    
    platform_state *platform = platform_init(&config, Megabytes(64), Megabytes(32));
    if (!platform) {
        printf("Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize renderer (same as working renderer)
    renderer_state *renderer = renderer_init(platform, Megabytes(128));
    if (!renderer) {
        printf("Failed to initialize renderer\n");
        platform_shutdown(platform);
        return 1;
    }
    
    // Initialize editor state
    editor_state editor = {0};
    editor.show_menu = true;
    editor.show_game_view = true;
    editor.show_palette = false;
    editor.show_canvas = false;
    editor.cube_rotation = 0.0f;
    editor.selected_palette_index = 0;
    
    // Initialize palette with some basic colors
    editor.palette[0] = (palette_entry){{0.8f, 0.2f, 0.2f}, "Red"};
    editor.palette[1] = (palette_entry){{0.2f, 0.8f, 0.2f}, "Green"};
    editor.palette[2] = (palette_entry){{0.2f, 0.2f, 0.8f}, "Blue"};
    editor.palette[3] = (palette_entry){{0.8f, 0.8f, 0.2f}, "Yellow"};
    editor.palette[4] = (palette_entry){{0.8f, 0.2f, 0.8f}, "Magenta"};
    editor.palette[5] = (palette_entry){{0.2f, 0.8f, 0.8f}, "Cyan"};
    editor.palette[6] = (palette_entry){{0.9f, 0.9f, 0.9f}, "White"};
    editor.palette[7] = (palette_entry){{0.1f, 0.1f, 0.1f}, "Black"};
    
    // Initialize canvas (all tiles to -1 = empty)
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            editor.canvas[y][x] = -1;
        }
    }
    
    printf("Editor initialized successfully!\n");
    printf("Controls:\n");
    printf("  F1: Toggle menu\n");
    printf("  F2: Toggle palette\n");
    printf("  F3: Toggle canvas\n");
    printf("  1-8: Select palette color\n");
    printf("  SPACE: Start/Stop game\n");
    printf("  ESC: Quit\n\n");
    
    // Main editor loop
    bool running = true;
    int frame_count = 0;
    
    while (running) {
        platform_poll_events(platform);
        
        // Check for quit (ESC key)
        if (platform_key_pressed(platform, KEY_ESCAPE)) {
            running = false;
        }
        
        // Update editor
        update_editor(&editor, platform);
        
        // Render frame
        render_editor(&editor, renderer);
        
        // Present frame to screen
        renderer_present(renderer);
        
        frame_count++;
        if (frame_count % 60 == 0) {
            printf("[EDITOR] Frame %d - Game %s\n", frame_count, 
                   editor.game_running ? "RUNNING" : "STOPPED");
        }
    }
    
    printf("\nShutting down editor...\n");
    renderer_shutdown(renderer);
    platform_shutdown(platform);
    printf("Editor shutdown complete.\n");
    
    return 0;
}