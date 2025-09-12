/*
    Testable version of main.c - same logic but with conditional GL calls
*/

#include "handmade_platform.h"
#include <stdio.h>
#include <math.h>

// Conditional GL header
#ifdef USE_OPENGL
#include <GL/gl.h>
#else
// Stub GL functions for testing
static void glEnable(int cap) { (void)cap; }
static void glDepthFunc(int func) { (void)func; }
static void glViewport(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static void glClearColor(float r, float g, float b, float a) { (void)r; (void)g; (void)b; (void)a; }
static void glClear(int mask) { (void)mask; }
static void glBegin(int mode) { (void)mode; }
static void glEnd(void) { }
static void glColor3f(float r, float g, float b) { (void)r; (void)g; (void)b; }
static void glVertex2f(float x, float y) { (void)x; (void)y; }
#define GL_DEPTH_TEST 0x0B71
#define GL_LESS 0x0201
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_TRIANGLES 0x0004
#define GL_POINTS 0x0000
#endif

// Global state for our minimal application
typedef struct {
    bool initialized;
    f32 time_accumulator;
    f32 background_color[3];  // RGB background color
} AppState;

static AppState g_app_state = {0};

// Required Game* functions that platform expects
void GameInit(PlatformState* platform) {
    printf("GameInit called\n");
    
    g_app_state.initialized = true;
    g_app_state.time_accumulator = 0.0f;
    g_app_state.background_color[0] = 0.2f;  // Dark blue background
    g_app_state.background_color[1] = 0.3f;
    g_app_state.background_color[2] = 0.4f;
    
    // Set up OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    printf("OpenGL initialized\n");
    printf("Window size: %dx%d\n", platform->window.width, platform->window.height);
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_app_state.initialized) return;
    
    g_app_state.time_accumulator += dt;
    
    // Simple color animation based on time
    g_app_state.background_color[0] = 0.2f + 0.1f * sinf(g_app_state.time_accumulator * 0.5f);
    g_app_state.background_color[1] = 0.3f + 0.1f * sinf(g_app_state.time_accumulator * 0.7f);
    g_app_state.background_color[2] = 0.4f + 0.1f * sinf(g_app_state.time_accumulator * 0.3f);
    
    // Handle basic input
    if (platform->input.keys[KEY_ESCAPE].pressed) {
        platform->window.should_close = true;
    }
    
    if (platform->input.keys[KEY_SPACE].pressed) {
        printf("Space pressed! Time: %.2f seconds\n", g_app_state.time_accumulator);
    }
}

void GameRender(PlatformState* platform) {
    if (!g_app_state.initialized) return;
    
    // Set viewport
    glViewport(0, 0, platform->window.width, platform->window.height);
    
    // Clear with animated background color
    glClearColor(
        g_app_state.background_color[0],
        g_app_state.background_color[1], 
        g_app_state.background_color[2],
        1.0f
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Simple triangle to prove OpenGL is working
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);  // Red
        glVertex2f(0.0f, 0.5f);
        
        glColor3f(0.0f, 1.0f, 0.0f);  // Green
        glVertex2f(-0.5f, -0.5f);
        
        glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        glVertex2f(0.5f, -0.5f);
    glEnd();
    
    // Draw simple text using OpenGL (just a few pixels to show something is working)
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_POINTS);
        // Spell "HI" with pixels
        // H
        glVertex2f(-0.8f, 0.8f);
        glVertex2f(-0.8f, 0.7f);
        glVertex2f(-0.8f, 0.6f);
        glVertex2f(-0.75f, 0.7f);
        glVertex2f(-0.7f, 0.8f);
        glVertex2f(-0.7f, 0.7f);
        glVertex2f(-0.7f, 0.6f);
        
        // I
        glVertex2f(-0.6f, 0.8f);
        glVertex2f(-0.6f, 0.7f);
        glVertex2f(-0.6f, 0.6f);
    glEnd();
}

void GameShutdown(PlatformState* platform) {
    (void)platform;  // Suppress warning
    printf("GameShutdown called\n");
    g_app_state.initialized = false;
}

void GameOnReload(PlatformState* platform) {
    (void)platform;  // Suppress warning
    printf("GameOnReload called\n");
    // Nothing to do for hot reload in this minimal version
}