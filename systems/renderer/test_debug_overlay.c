/*
    Test program for debug overlay with stress testing
    Shows real performance metrics with actual rendering load
*/

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include "debug_overlay_complete.c"

// Test parameters
static struct {
    int triangle_count;
    int draw_call_count;
    int state_change_freq;
    float rotation;
    int stress_mode;
} g_test = {
    .triangle_count = 1000,
    .draw_call_count = 100,
    .state_change_freq = 10,
    .rotation = 0.0f,
    .stress_mode = 0
};

// Draw a test mesh
static void draw_test_mesh(int triangles) {
    glBegin(GL_TRIANGLES);
    
    for (int i = 0; i < triangles; i++) {
        float angle = (i * 2.0f * M_PI) / triangles;
        float r = (i % 3) / 3.0f;
        float g = ((i + 1) % 3) / 3.0f;
        float b = ((i + 2) % 3) / 3.0f;
        
        glColor3f(r, g, b);
        glVertex3f(cosf(angle) * 0.5f, sinf(angle) * 0.5f, 0.0f);
        glVertex3f(cosf(angle + 0.1f) * 0.6f, sinf(angle + 0.1f) * 0.6f, 0.1f);
        glVertex3f(cosf(angle - 0.1f) * 0.4f, sinf(angle - 0.1f) * 0.4f, -0.1f);
    }
    
    glEnd();
}

// Simulate state changes
static void simulate_state_change() {
    static int state = 0;
    state = (state + 1) % 4;
    
    switch (state) {
        case 0:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case 1:
            glDisable(GL_BLEND);
            break;
        case 2:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case 3:
            glDisable(GL_CULL_FACE);
            break;
    }
    
    debug_overlay_state_change();
}

// Render frame
static void render_frame() {
    debug_overlay_begin_frame();
    
    // Clear
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up 3D view
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Manual perspective matrix (replacing gluPerspective)
    float fov = 45.0f * M_PI / 180.0f;
    float aspect = 1280.0f / 720.0f;
    float near = 0.1f;
    float far = 100.0f;
    float f = 1.0f / tanf(fov / 2.0f);
    
    float matrix[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (far+near)/(near-far), (2*far*near)/(near-far),
        0, 0, -1, 0
    };
    glMultMatrixf(matrix);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(g_test.rotation, 0.0f, 1.0f, 0.0f);
    
    // Draw test geometry
    for (int i = 0; i < g_test.draw_call_count; i++) {
        // State changes
        if (i % g_test.state_change_freq == 0) {
            simulate_state_change();
        }
        
        // Position each mesh
        glPushMatrix();
        float x = (i % 10 - 5) * 0.5f;
        float y = ((i / 10) % 10 - 5) * 0.5f;
        float z = ((i / 100) - 5) * 0.5f;
        glTranslatef(x, y, z);
        glScalef(0.3f, 0.3f, 0.3f);
        
        // Draw
        draw_test_mesh(g_test.triangle_count);
        debug_overlay_draw_call(g_test.triangle_count * 3);
        
        glPopMatrix();
    }
    
    // Update rotation
    g_test.rotation += 1.0f;
    
    // Stress test modes
    if (g_test.stress_mode == 1) {
        // CPU stress: lots of small draw calls
        for (int i = 0; i < 1000; i++) {
            glBegin(GL_TRIANGLES);
            glVertex3f(0, 0, 0);
            glVertex3f(0.01f, 0, 0);
            glVertex3f(0, 0.01f, 0);
            glEnd();
            debug_overlay_draw_call(1);
        }
    } else if (g_test.stress_mode == 2) {
        // GPU stress: huge triangle count
        draw_test_mesh(100000);
        debug_overlay_draw_call(100000 * 3);
    }
    
    debug_overlay_end_frame();
    
    // Get viewport for overlay
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Render debug overlay
    debug_overlay_render(viewport[2], viewport[3]);
}

int main() {
    // Open X11 display
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    Window root = DefaultRootWindow(display);
    
    // GLX attributes
    GLint attrs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(display, 0, attrs);
    if (!vi) {
        fprintf(stderr, "No suitable visual\n");
        return 1;
    }
    
    // Create window
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask;
    
    Window window = XCreateWindow(
        display, root,
        0, 0, 1280, 720,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );
    
    XMapWindow(display, window);
    XStoreName(display, window, "Debug Overlay Test - F1:Toggle F2:Graphs F3:Hints");
    
    // Create GL context
    GLXContext glc = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, window, glc);
    
    // Initialize OpenGL
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    // Initialize debug overlay
    debug_overlay_init(60.0f);
    
    printf("Debug Overlay Test Program\n");
    printf("==========================\n");
    printf("Controls:\n");
    printf("  F1 - Toggle overlay\n");
    printf("  F2 - Toggle graphs\n");
    printf("  F3 - Toggle hints\n");
    printf("  1-5 - Stress test modes\n");
    printf("  +/- - Adjust triangle count\n");
    printf("  [/] - Adjust draw calls\n");
    printf("  ESC - Exit\n\n");
    
    // Main loop
    XEvent event;
    int running = 1;
    
    while (running) {
        // Handle events
        while (XPending(display)) {
            XNextEvent(display, &event);
            
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                
                switch (key) {
                    case XK_Escape:
                        running = 0;
                        break;
                        
                    case XK_F1:
                        debug_overlay_toggle();
                        printf("Overlay: toggled\n");
                        break;
                        
                    case XK_F2:
                        debug_overlay_toggle_graphs();
                        printf("Graphs: toggled\n");
                        break;
                        
                    case XK_F3:
                        debug_overlay_toggle_hints();
                        printf("Hints: toggled\n");
                        break;
                        
                    case XK_1:
                        g_test.stress_mode = 0;
                        printf("Normal mode\n");
                        break;
                        
                    case XK_2:
                        g_test.stress_mode = 1;
                        printf("CPU stress mode (many small draws)\n");
                        break;
                        
                    case XK_3:
                        g_test.stress_mode = 2;
                        printf("GPU stress mode (high triangle count)\n");
                        break;
                        
                    case XK_plus:
                    case XK_equal:
                        g_test.triangle_count *= 2;
                        printf("Triangles per mesh: %d\n", g_test.triangle_count);
                        break;
                        
                    case XK_minus:
                        g_test.triangle_count /= 2;
                        if (g_test.triangle_count < 1) g_test.triangle_count = 1;
                        printf("Triangles per mesh: %d\n", g_test.triangle_count);
                        break;
                        
                    case XK_bracketright:
                        g_test.draw_call_count *= 2;
                        printf("Draw calls: %d\n", g_test.draw_call_count);
                        break;
                        
                    case XK_bracketleft:
                        g_test.draw_call_count /= 2;
                        if (g_test.draw_call_count < 1) g_test.draw_call_count = 1;
                        printf("Draw calls: %d\n", g_test.draw_call_count);
                        break;
                }
            }
        }
        
        // Render
        render_frame();
        glXSwapBuffers(display, window);
        
        // Small delay to not burn CPU
        usleep(1000);
    }
    
    // Cleanup
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, glc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    
    return 0;
}