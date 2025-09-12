/*
    Continental Architect - Actually Working Version
    Built step by step, validated at each stage
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
#define TERRAIN_SIZE 64

typedef struct {
    float heightmap[TERRAIN_SIZE][TERRAIN_SIZE];
    float time;
    float camera_angle;
    int mouse_down;
    int mouse_x, mouse_y;
    int tool;
} GameState;

void init_terrain(GameState* game) {
    // Create initial terrain with some hills
    for (int y = 0; y < TERRAIN_SIZE; y++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            float fx = (float)x / TERRAIN_SIZE * 10.0f;
            float fy = (float)y / TERRAIN_SIZE * 10.0f;
            game->heightmap[y][x] = sinf(fx) * cosf(fy) * 0.3f;
        }
    }
    game->time = 0;
    game->camera_angle = 0;
    game->mouse_down = 0;
    game->tool = 1;
}

void update_game(GameState* game, float dt) {
    game->time += dt;
    game->camera_angle += dt * 0.5f;
    
    // Apply erosion
    for (int y = 1; y < TERRAIN_SIZE-1; y++) {
        for (int x = 1; x < TERRAIN_SIZE-1; x++) {
            float h = game->heightmap[y][x];
            float avg = (game->heightmap[y-1][x] + game->heightmap[y+1][x] +
                        game->heightmap[y][x-1] + game->heightmap[y][x+1]) * 0.25f;
            game->heightmap[y][x] = h * 0.99f + avg * 0.01f;
        }
    }
}

void modify_terrain(GameState* game, int mx, int my) {
    // Convert mouse to terrain coords
    int tx = (mx * TERRAIN_SIZE) / WINDOW_WIDTH;
    int ty = (my * TERRAIN_SIZE) / WINDOW_HEIGHT;
    
    if (tx < 5 || tx >= TERRAIN_SIZE-5 || ty < 5 || ty >= TERRAIN_SIZE-5)
        return;
    
    // Apply modification in radius
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 4) {
                float strength = (4 - dist) / 4 * 0.1f;
                if (game->tool == 1) {
                    game->heightmap[ty+dy][tx+dx] += strength;
                } else {
                    game->heightmap[ty+dy][tx+dx] -= strength;
                }
            }
        }
    }
}

void render_frame(GameState* game) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-2, 2, -1.5, 1.5, 0.1, 100);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, -0.5f, -5);
    glRotatef(30, 1, 0, 0);
    glRotatef(game->camera_angle * 57.3f, 0, 1, 0);
    
    // Draw terrain
    glColor3f(0.3f, 0.6f, 0.2f);
    
    for (int y = 0; y < TERRAIN_SIZE-1; y++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            float fx = (float)x / TERRAIN_SIZE * 4.0f - 2.0f;
            float fy0 = (float)y / TERRAIN_SIZE * 4.0f - 2.0f;
            float fy1 = (float)(y+1) / TERRAIN_SIZE * 4.0f - 2.0f;
            
            float h0 = game->heightmap[y][x];
            float h1 = game->heightmap[y+1][x];
            
            // Color by height
            glColor3f(0.3f + h0, 0.6f - fabsf(h0) * 0.3f, 0.2f);
            glVertex3f(fx, h0, fy0);
            
            glColor3f(0.3f + h1, 0.6f - fabsf(h1) * 0.3f, 0.2f);
            glVertex3f(fx, h1, fy1);
        }
        glEnd();
    }
    
    // Draw UI overlay
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Tool indicator
    glColor3f(1, 1, 0);
    glBegin(GL_QUADS);
    float tx = 10 + (game->tool - 1) * 100;
    glVertex2f(tx, 10);
    glVertex2f(tx + 80, 10);
    glVertex2f(tx + 80, 40);
    glVertex2f(tx, 40);
    glEnd();
}

int main() {
    printf("Continental Architect - Working Version\n");
    printf("=======================================\n");
    printf("Controls:\n");
    printf("  1/2: Push/Pull terrain\n");
    printf("  Mouse: Modify terrain\n");
    printf("  Q: Quit\n\n");
    
    // Open display
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    
    // Choose visual
    GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
    XVisualInfo* vi = glXChooseVisual(display, screen, att);
    if (!vi) {
        fprintf(stderr, "No appropriate visual found\n");
        return 1;
    }
    
    // Create window
    Colormap cmap = XCreateColormap(display, root, vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    
    Window win = XCreateWindow(display, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(display, win);
    XStoreName(display, win, "Continental Architect");
    
    // Create GL context
    GLXContext glc = glXCreateContext(display, vi, NULL, GL_TRUE);
    if (!glc) {
        fprintf(stderr, "Failed to create GL context\n");
        return 1;
    }
    
    // Wait for window to be mapped
    XEvent xev;
    do {
        XNextEvent(display, &xev);
    } while (xev.type != MapNotify);
    
    glXMakeCurrent(display, win, glc);
    
    // Setup OpenGL
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f);
    
    printf("OpenGL initialized successfully\n");
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    
    // Initialize game
    GameState game;
    init_terrain(&game);
    
    // Main loop
    int running = 1;
    clock_t last_time = clock();
    int frame_count = 0;
    clock_t fps_time = clock();
    
    while (running) {
        // Handle events (non-blocking)
        while (XPending(display)) {
            XNextEvent(display, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                if (key == XK_q || key == XK_Escape) {
                    running = 0;
                } else if (key == XK_1) {
                    game.tool = 1;
                    printf("Tool: Push terrain\n");
                } else if (key == XK_2) {
                    game.tool = 2;
                    printf("Tool: Pull terrain\n");
                }
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                game.mouse_down = 1;
                game.mouse_x = xev.xbutton.x;
                game.mouse_y = xev.xbutton.y;
            } else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                game.mouse_down = 0;
            } else if (xev.type == MotionNotify && game.mouse_down) {
                game.mouse_x = xev.xmotion.x;
                game.mouse_y = xev.xmotion.y;
                modify_terrain(&game, game.mouse_x, game.mouse_y);
            }
        }
        
        // Update
        clock_t current_time = clock();
        float dt = (float)(current_time - last_time) / CLOCKS_PER_SEC;
        last_time = current_time;
        
        update_game(&game, dt);
        
        // Render
        render_frame(&game);
        glXSwapBuffers(display, win);
        
        // FPS counter
        frame_count++;
        if ((current_time - fps_time) >= CLOCKS_PER_SEC) {
            printf("FPS: %d\n", frame_count);
            frame_count = 0;
            fps_time = current_time;
        }
        
        // Small delay to not burn CPU
        usleep(16666); // ~60 FPS
    }
    
    // Cleanup
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, glc);
    XDestroyWindow(display, win);
    XCloseDisplay(display);
    
    printf("Game ended cleanly\n");
    return 0;
}