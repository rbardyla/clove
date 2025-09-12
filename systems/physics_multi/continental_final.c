/*
    Continental Architect - Final Working Version
    Simplified for guaranteed functionality
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

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define GRID_SIZE 50

float terrain[GRID_SIZE][GRID_SIZE];
float camera_angle = 0;
int mouse_x = 0, mouse_y = 0;
int mouse_down = 0;
int tool = 1; // 1=raise, 2=lower

void init_terrain() {
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            terrain[y][x] = sinf(x * 0.2f) * cosf(y * 0.2f) * 0.2f;
        }
    }
}

void modify_terrain(int mx, int my, int raise) {
    int cx = (mx * GRID_SIZE) / WINDOW_WIDTH;
    int cy = (my * GRID_SIZE) / WINDOW_HEIGHT;
    
    if (cx < 3 || cx >= GRID_SIZE-3 || cy < 3 || cy >= GRID_SIZE-3)
        return;
    
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            float d = sqrtf(dx*dx + dy*dy);
            if (d < 3) {
                float amt = (3 - d) / 3 * 0.05f;
                if (raise)
                    terrain[cy+dy][cx+dx] += amt;
                else
                    terrain[cy+dy][cx+dx] -= amt;
                    
                // Clamp
                if (terrain[cy+dy][cx+dx] > 1.0f) terrain[cy+dy][cx+dx] = 1.0f;
                if (terrain[cy+dy][cx+dx] < -1.0f) terrain[cy+dy][cx+dx] = -1.0f;
            }
        }
    }
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 3D view
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-2, 2, -1.5, 1.5, -10, 10);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, -0.3f, 0);
    glRotatef(20, 1, 0, 0);
    glRotatef(camera_angle, 0, 1, 0);
    
    // Draw terrain as lines for simplicity
    glColor3f(0, 1, 0);
    
    // Draw horizontal lines
    for (int y = 0; y < GRID_SIZE; y++) {
        glBegin(GL_LINE_STRIP);
        for (int x = 0; x < GRID_SIZE; x++) {
            float fx = (float)x / GRID_SIZE * 3.0f - 1.5f;
            float fy = (float)y / GRID_SIZE * 3.0f - 1.5f;
            float h = terrain[y][x];
            
            // Color by height
            glColor3f(0.2f + h, 0.7f - fabs(h)*0.5f, 0.2f);
            glVertex3f(fx, h, fy);
        }
        glEnd();
    }
    
    // Draw vertical lines
    for (int x = 0; x < GRID_SIZE; x++) {
        glBegin(GL_LINE_STRIP);
        for (int y = 0; y < GRID_SIZE; y++) {
            float fx = (float)x / GRID_SIZE * 3.0f - 1.5f;
            float fy = (float)y / GRID_SIZE * 3.0f - 1.5f;
            float h = terrain[y][x];
            
            glColor3f(0.2f + h, 0.7f - fabs(h)*0.5f, 0.2f);
            glVertex3f(fx, h, fy);
        }
        glEnd();
    }
    
    // Simple UI
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Tool indicator
    glColor3f(1, 1, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(10, 10);
    glVertex2f(150, 10);
    glVertex2f(150, 30);
    glVertex2f(10, 30);
    glEnd();
    
    // Tool selection box
    glBegin(GL_QUADS);
    float tx = 10 + (tool-1) * 70;
    glVertex2f(tx, 10);
    glVertex2f(tx+60, 10);
    glVertex2f(tx+60, 30);
    glVertex2f(tx, 30);
    glEnd();
}

int main() {
    printf("=== Continental Architect ===\n");
    printf("1/2: Raise/Lower tool\n");
    printf("Mouse: Click and drag\n");
    printf("ESC: Quit\n\n");
    
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        printf("Cannot open display\n");
        return 1;
    }
    
    int scr = DefaultScreen(dpy);
    GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None};
    XVisualInfo* vi = glXChooseVisual(dpy, scr, att);
    
    if (!vi) {
        printf("No visual found\n");
        return 1;
    }
    
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | ButtonPressMask | ButtonReleaseMask | 
                     PointerMotionMask | ExposureMask;
    
    Window win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Continental Architect");
    
    // Flush and sync to ensure window is mapped
    XFlush(dpy);
    XSync(dpy, False);
    usleep(100000); // Give X server time to map window
    
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.15f, 0.3f, 1.0f);
    
    init_terrain();
    
    printf("Running...\n");
    
    int running = 1;
    int frames = 0;
    time_t start_time = time(NULL);
    
    while (running) {
        // Non-blocking event handling
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                if (key == XK_Escape || key == XK_q) {
                    running = 0;
                } else if (key == XK_1) {
                    tool = 1;
                    printf("Tool: Raise\n");
                } else if (key == XK_2) {
                    tool = 2;
                    printf("Tool: Lower\n");
                }
            } else if (xev.type == ButtonPress) {
                mouse_down = 1;
                mouse_x = xev.xbutton.x;
                mouse_y = xev.xbutton.y;
            } else if (xev.type == ButtonRelease) {
                mouse_down = 0;
            } else if (xev.type == MotionNotify) {
                mouse_x = xev.xmotion.x;
                mouse_y = xev.xmotion.y;
                if (mouse_down) {
                    modify_terrain(mouse_x, mouse_y, tool == 1);
                }
            }
        }
        
        camera_angle += 0.5f;
        
        render();
        glXSwapBuffers(dpy, win);
        
        frames++;
        time_t now = time(NULL);
        if (now > start_time) {
            printf("FPS: %d\n", frames);
            frames = 0;
            start_time = now;
        }
        
        usleep(16666); // 60 FPS
    }
    
    printf("Exiting cleanly\n");
    
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    
    return 0;
}