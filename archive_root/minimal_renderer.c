#include "minimal_renderer.h"
#include <GL/gl.h>
#include <string.h>
#include <stdio.h>

void renderer_init(renderer *r, u32 width, u32 height) {
    r->width = width;
    r->height = height;
    r->pixels_drawn = 0;
    r->primitives_drawn = 0;
    
    // Set up OpenGL state for 2D rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void renderer_shutdown(renderer *r) {
    // Nothing to clean up for basic OpenGL renderer
    (void)r;
}

static void setup_2d_projection(renderer *r) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Set up orthographic projection with (0,0) at top-left
    // This matches what the GUI expects
    glOrtho(0.0, (double)r->width, (double)r->height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void set_gl_color(color32 color) {
    glColor4f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
}

void renderer_fill_rect(renderer *r, int x, int y, int w, int h, color32 color) {
    setup_2d_projection(r);
    set_gl_color(color);
    
    glBegin(GL_QUADS);
        glVertex2i(x, y);
        glVertex2i(x + w, y);
        glVertex2i(x + w, y + h);
        glVertex2i(x, y + h);
    glEnd();
    
    r->primitives_drawn++;
    r->pixels_drawn += w * h;
}

void renderer_draw_rect(renderer *r, int x, int y, int w, int h, color32 color) {
    setup_2d_projection(r);
    set_gl_color(color);
    
    glBegin(GL_LINE_LOOP);
        glVertex2i(x, y);
        glVertex2i(x + w - 1, y);
        glVertex2i(x + w - 1, y + h - 1);
        glVertex2i(x, y + h - 1);
    glEnd();
    
    r->primitives_drawn++;
}

void renderer_text(renderer *r, int x, int y, const char *text, color32 color) {
    setup_2d_projection(r);
    set_gl_color(color);
    
    // Simple bitmap text rendering using OpenGL points
    // This is very basic - each character is 8x12 pixels
    int char_width = 8;
    int char_height = 12;
    
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        char c = text[i];
        int char_x = x + i * char_width;
        
        // Very simple font rendering - just draw some pixels for common characters
        switch (c) {
            case 'A':
                // Draw a simple 'A' pattern
                for (int py = 1; py < 11; py++) {
                    glVertex2i(char_x, y + py);      // Left line
                    glVertex2i(char_x + 6, y + py);  // Right line
                }
                for (int px = 1; px < 6; px++) {
                    glVertex2i(char_x + px, y + 1);  // Top line
                    glVertex2i(char_x + px, y + 6);  // Middle line
                }
                break;
                
            case 'B':
                // Draw a simple 'B' pattern
                for (int py = 1; py < 11; py++) {
                    glVertex2i(char_x, y + py);      // Left line
                }
                for (int px = 1; px < 5; px++) {
                    glVertex2i(char_x + px, y + 1);  // Top line
                    glVertex2i(char_x + px, y + 6);  // Middle line
                    glVertex2i(char_x + px, y + 10); // Bottom line
                }
                glVertex2i(char_x + 5, y + 2);
                glVertex2i(char_x + 5, y + 4);
                glVertex2i(char_x + 5, y + 7);
                glVertex2i(char_x + 5, y + 9);
                break;
                
            case 'C':
                // Draw a simple 'C' pattern
                for (int py = 2; py < 10; py++) {
                    glVertex2i(char_x, y + py);      // Left line
                }
                for (int px = 1; px < 6; px++) {
                    glVertex2i(char_x + px, y + 1);  // Top line
                    glVertex2i(char_x + px, y + 10); // Bottom line
                }
                break;
                
            case ' ':
                // Space - don't draw anything
                break;
                
            default:
                // For unknown characters, draw a simple rectangle
                for (int py = 1; py < 11; py++) {
                    for (int px = 0; px < 6; px++) {
                        if (py == 1 || py == 10 || px == 0 || px == 5) {
                            glVertex2i(char_x + px, y + py);
                        }
                    }
                }
                break;
        }
    }
    
    glEnd();
    
    r->primitives_drawn++;
    r->pixels_drawn += len * char_width * char_height;
}

void renderer_text_size(renderer *r, const char *text, int *w, int *h) {
    (void)r; // Unused
    
    // Simple fixed-width font metrics
    int char_width = 8;
    int char_height = 12;
    
    *w = strlen(text) * char_width;
    *h = char_height;
}

// Additional functions for performance overlay
void renderer_rect(renderer *r, int x, int y, int w, int h, color32 color) {
    renderer_fill_rect(r, x, y, w, h, color);
}

void renderer_rect_outline(renderer *r, int x, int y, int w, int h, color32 color, int thickness) {
    (void)thickness; // Simple implementation ignores thickness
    renderer_draw_rect(r, x, y, w, h, color);
}

void renderer_line(renderer *r, int x1, int y1, int x2, int y2, color32 color) {
    setup_2d_projection(r);
    set_gl_color(color);
    
    glBegin(GL_LINES);
        glVertex2i(x1, y1);
        glVertex2i(x2, y2);
    glEnd();
    
    r->primitives_drawn++;
}

// color32_make is now defined in the header as inline