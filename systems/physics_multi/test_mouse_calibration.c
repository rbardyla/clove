// Mouse Calibration Test Program
// Helps identify the exact mouse offset issue

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

typedef struct {
    Display* display;
    Window window;
    GLXContext context;
    int mouse_x, mouse_y;
    int click_x, click_y;
    int has_click;
} TestApp;

static TestApp* app = NULL;

void draw_crosshair(float x, float y, float r, float g, float b) {
    glColor3f(r, g, b);
    glLineWidth(1);
    glBegin(GL_LINES);
    // Horizontal
    glVertex2f(x - 20, y);
    glVertex2f(x + 20, y);
    // Vertical
    glVertex2f(x, y - 20);
    glVertex2f(x, y + 20);
    glEnd();
    
    // Draw circle around center
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 16; i++) {
        float angle = i * 3.14159f * 2.0f / 16.0f;
        glVertex2f(x + 10 * cosf(angle), y + 10 * sinf(angle));
    }
    glEnd();
}

void draw_grid() {
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_LINES);
    
    // Vertical lines every 50 pixels
    for (int x = 0; x <= WINDOW_WIDTH; x += 50) {
        glVertex2f(x, 0);
        glVertex2f(x, WINDOW_HEIGHT);
    }
    
    // Horizontal lines every 50 pixels
    for (int y = 0; y <= WINDOW_HEIGHT; y += 50) {
        glVertex2f(0, y);
        glVertex2f(WINDOW_WIDTH, y);
    }
    glEnd();
    
    // Draw coordinate labels at grid intersections
    glColor3f(0.5f, 0.5f, 0.5f);
    for (int x = 0; x <= WINDOW_WIDTH; x += 100) {
        for (int y = 0; y <= WINDOW_HEIGHT; y += 100) {
            // Draw small square at intersection
            glBegin(GL_QUADS);
            glVertex2f(x - 2, y - 2);
            glVertex2f(x + 2, y - 2);
            glVertex2f(x + 2, y + 2);
            glVertex2f(x - 2, y + 2);
            glEnd();
        }
    }
}

void draw_test_targets() {
    // Draw clickable targets at known positions
    float targets[][2] = {
        {100, 100}, {200, 100}, {300, 100},
        {100, 200}, {200, 200}, {300, 200},
        {100, 300}, {200, 300}, {300, 300}
    };
    
    for (int i = 0; i < 9; i++) {
        float x = targets[i][0];
        float y = targets[i][1];
        
        // Target box
        glColor3f(0.3f, 0.3f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(x - 20, y - 20);
        glVertex2f(x + 20, y - 20);
        glVertex2f(x + 20, y + 20);
        glVertex2f(x - 20, y + 20);
        glEnd();
        
        // Target center
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_POINTS);
        glVertex2f(x, y);
        glEnd();
        
        // Target border
        glColor3f(0.6f, 0.6f, 0.8f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x - 20, y - 20);
        glVertex2f(x + 20, y - 20);
        glVertex2f(x + 20, y + 20);
        glVertex2f(x - 20, y + 20);
        glEnd();
    }
}

void draw_info() {
    // Draw text info about mouse position
    char info[256];
    snprintf(info, sizeof(info), "Mouse: (%d, %d)", app->mouse_x, app->mouse_y);
    
    // Background for text
    glColor4f(0.1f, 0.1f, 0.1f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(10, 10);
    glVertex2f(200, 10);
    glVertex2f(200, 60);
    glVertex2f(10, 60);
    glEnd();
    
    // Simple text rendering using lines
    glColor3f(1, 1, 1);
    glRasterPos2f(15, 30);
    for (int i = 0; info[i] != '\0'; i++) {
        // This is a placeholder - real text would go here
    }
    
    if (app->has_click) {
        snprintf(info, sizeof(info), "Last click: (%d, %d)", app->click_x, app->click_y);
        glRasterPos2f(15, 50);
    }
}

int main() {
    app = calloc(1, sizeof(TestApp));
    
    app->display = XOpenDisplay(NULL);
    if (!app->display) {
        printf("Cannot open display\n");
        return 1;
    }
    
    int screen = DefaultScreen(app->display);
    Window root = RootWindow(app->display, screen);
    
    GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
    XVisualInfo* vi = glXChooseVisual(app->display, 0, att);
    
    if (!vi) {
        printf("No appropriate visual found\n");
        return 1;
    }
    
    Colormap cmap = XCreateColormap(app->display, root, vi->visual, AllocNone);
    
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    
    app->window = XCreateWindow(app->display, root, 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
                               0, vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);
    
    XMapWindow(app->display, app->window);
    XStoreName(app->display, app->window, "Mouse Calibration Test");
    
    app->context = glXCreateContext(app->display, vi, NULL, GL_TRUE);
    glXMakeCurrent(app->display, app->window, app->context);
    
    // Wait for window to be mapped
    XEvent xev;
    do {
        XNextEvent(app->display, &xev);
    } while (xev.type != MapNotify);
    
    printf("=== MOUSE CALIBRATION TEST ===\n");
    printf("Click on the blue target boxes\n");
    printf("Expected vs Actual positions will be shown\n");
    printf("Press ESC to exit\n\n");
    
    int running = 1;
    while (running) {
        while (XPending(app->display)) {
            XNextEvent(app->display, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                if (key == XK_Escape) {
                    running = 0;
                }
            } else if (xev.type == ButtonPress) {
                app->click_x = xev.xbutton.x;
                app->click_y = xev.xbutton.y;
                app->has_click = 1;
                
                printf("Click at: (%d, %d)\n", app->click_x, app->click_y);
                
                // Check which target was clicked
                float targets[][2] = {
                    {100, 100}, {200, 100}, {300, 100},
                    {100, 200}, {200, 200}, {300, 200},
                    {100, 300}, {200, 300}, {300, 300}
                };
                
                for (int i = 0; i < 9; i++) {
                    float tx = targets[i][0];
                    float ty = targets[i][1];
                    
                    if (app->click_x >= tx - 20 && app->click_x <= tx + 20 &&
                        app->click_y >= ty - 20 && app->click_y <= ty + 20) {
                        printf("  -> Hit target at (%.0f, %.0f)\n", tx, ty);
                        printf("  -> Offset: (%d, %d)\n", 
                               (int)(app->click_x - tx), (int)(app->click_y - ty));
                    }
                }
            } else if (xev.type == MotionNotify) {
                app->mouse_x = xev.xmotion.x;
                app->mouse_y = xev.xmotion.y;
            } else if (xev.type == ConfigureNotify) {
                glViewport(0, 0, xev.xconfigure.width, xev.xconfigure.height);
            }
        }
        
        // Render
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);  // Y-down coordinate system
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Draw grid
        draw_grid();
        
        // Draw test targets
        draw_test_targets();
        
        // Draw mouse position crosshair
        draw_crosshair(app->mouse_x, app->mouse_y, 1, 0, 0);
        
        // Draw click position if we have one
        if (app->has_click) {
            draw_crosshair(app->click_x, app->click_y, 0, 1, 0);
        }
        
        // Draw corner markers to verify coordinate system
        glColor3f(1, 1, 0);
        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2f(0, 0);  // Top-left
        glVertex2f(WINDOW_WIDTH, 0);  // Top-right
        glVertex2f(0, WINDOW_HEIGHT);  // Bottom-left
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);  // Bottom-right
        glEnd();
        
        // Draw info
        draw_info();
        
        glXSwapBuffers(app->display, app->window);
        usleep(16666);
    }
    
    glXMakeCurrent(app->display, None, NULL);
    glXDestroyContext(app->display, app->context);
    XDestroyWindow(app->display, app->window);
    XCloseDisplay(app->display);
    
    free(app);
    return 0;
}