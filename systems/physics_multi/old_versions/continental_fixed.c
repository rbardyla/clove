/*
    Continental Architect - Fixed Version
    A working demonstration of multi-scale physics with proper rendering
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

typedef float f32;
typedef double f64;
typedef struct v3 { f32 x, y, z; } v3;

#define TERRAIN_SIZE 128
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

typedef struct {
    f32 heightmap[TERRAIN_SIZE][TERRAIN_SIZE];
    f32 geological_time;
    f32 time_scale;
    int tool_selected;
    f32 camera_distance;
    f32 camera_rotation;
    int mouse_x, mouse_y;
    int frame_count;
    struct timespec last_time;
} game_state;

void init_terrain(game_state* game) {
    // Initialize with some interesting terrain
    for (int y = 0; y < TERRAIN_SIZE; y++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            f32 fx = (f32)x / TERRAIN_SIZE - 0.5f;
            f32 fy = (f32)y / TERRAIN_SIZE - 0.5f;
            
            // Create some initial mountains
            f32 height = 0.0f;
            height += 0.3f * sinf(fx * 10.0f) * cosf(fy * 10.0f);
            height += 0.2f * sinf(fx * 20.0f) * sinf(fy * 20.0f);
            height += 0.1f * cosf(fx * 5.0f + fy * 5.0f);
            
            game->heightmap[y][x] = height;
        }
    }
    
    game->geological_time = 0.0f;
    game->time_scale = 1.0f;
    game->tool_selected = 1;
    game->camera_distance = 3.0f;
    game->camera_rotation = 0.0f;
}

void simulate_geology(game_state* game, f32 dt) {
    // Simple erosion simulation
    f32 erosion_rate = 0.001f * dt;
    
    for (int y = 1; y < TERRAIN_SIZE-1; y++) {
        for (int x = 1; x < TERRAIN_SIZE-1; x++) {
            f32 center = game->heightmap[y][x];
            f32 neighbors = (game->heightmap[y-1][x] + game->heightmap[y+1][x] +
                            game->heightmap[y][x-1] + game->heightmap[y][x+1]) / 4.0f;
            
            // Smooth toward neighbors (erosion)
            game->heightmap[y][x] = center * (1.0f - erosion_rate) + neighbors * erosion_rate;
        }
    }
    
    game->geological_time += dt * game->time_scale;
}

void apply_tool(game_state* game, int grid_x, int grid_y) {
    if (grid_x < 0 || grid_x >= TERRAIN_SIZE || grid_y < 0 || grid_y >= TERRAIN_SIZE)
        return;
    
    int radius = 5;
    f32 strength = 0.05f;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int x = grid_x + dx;
            int y = grid_y + dy;
            
            if (x >= 0 && x < TERRAIN_SIZE && y >= 0 && y < TERRAIN_SIZE) {
                f32 dist = sqrtf(dx*dx + dy*dy);
                if (dist <= radius) {
                    f32 factor = 1.0f - (dist / radius);
                    
                    switch (game->tool_selected) {
                        case 1: // Push up
                            game->heightmap[y][x] += strength * factor;
                            if (game->heightmap[y][x] > 2.0f) game->heightmap[y][x] = 2.0f;
                            break;
                        case 2: // Pull down
                            game->heightmap[y][x] -= strength * factor;
                            if (game->heightmap[y][x] < -2.0f) game->heightmap[y][x] = -2.0f;
                            break;
                    }
                }
            }
        }
    }
}

void render_terrain(game_state* game) {
    // Set up lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    f32 light_pos[] = {1.0f, 2.0f, 1.0f, 0.0f};
    f32 light_ambient[] = {0.2f, 0.2f, 0.3f, 1.0f};
    f32 light_diffuse[] = {0.8f, 0.8f, 0.7f, 1.0f};
    
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    
    // Render terrain as triangle strips
    f32 scale = 2.0f / TERRAIN_SIZE;
    
    for (int y = 0; y < TERRAIN_SIZE-1; y++) {
        glBegin(GL_TRIANGLE_STRIP);
        
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            f32 fx0 = x * scale - 1.0f;
            f32 fy0 = y * scale - 1.0f;
            f32 fy1 = (y+1) * scale - 1.0f;
            
            f32 h0 = game->heightmap[y][x] * 0.3f;
            f32 h1 = game->heightmap[y+1][x] * 0.3f;
            
            // Calculate normals (simplified)
            v3 n0 = {0, 1, 0};
            v3 n1 = {0, 1, 0};
            
            if (x > 0 && x < TERRAIN_SIZE-1) {
                f32 dx0 = game->heightmap[y][x+1] - game->heightmap[y][x-1];
                f32 dx1 = game->heightmap[y+1][x+1] - game->heightmap[y+1][x-1];
                n0.x = -dx0 * 2.0f;
                n1.x = -dx1 * 2.0f;
            }
            
            // Color based on height
            f32 c0 = 0.3f + h0;
            f32 c1 = 0.3f + h1;
            
            glColor3f(c0 * 0.4f, c0 * 0.6f, c0 * 0.3f);
            glNormal3f(n0.x, n0.y, n0.z);
            glVertex3f(fx0, h0, fy0);
            
            glColor3f(c1 * 0.4f, c1 * 0.6f, c1 * 0.3f);
            glNormal3f(n1.x, n1.y, n1.z);
            glVertex3f(fx0, h1, fy1);
        }
        
        glEnd();
    }
    
    glDisable(GL_LIGHTING);
}

void render_ui(game_state* game) {
    // Switch to 2D mode
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Draw UI background
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(10, 10);
    glVertex2f(250, 10);
    glVertex2f(250, 100);
    glVertex2f(10, 100);
    glEnd();
    
    // Draw text placeholder (in real version would render text)
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    // Simple "Continental Architect" text indicator
    glVertex2f(20, 30); glVertex2f(240, 30);
    glEnd();
    
    // Tool indicator
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    f32 tool_x = 20 + (game->tool_selected - 1) * 50;
    glVertex2f(tool_x, 50);
    glVertex2f(tool_x + 40, 50);
    glVertex2f(tool_x + 40, 70);
    glVertex2f(tool_x, 70);
    glEnd();
    
    // Time indicator
    char time_str[128];
    snprintf(time_str, sizeof(time_str), "Time: %.1f My", game->geological_time);
    
    // FPS counter
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    f64 delta = (current_time.tv_sec - game->last_time.tv_sec) + 
                (current_time.tv_nsec - game->last_time.tv_nsec) / 1e9;
    
    if (delta > 0.5) {
        f32 fps = game->frame_count / delta;
        printf("FPS: %.1f, Time: %.1f My, Tool: %d\n", fps, game->geological_time, game->tool_selected);
        game->frame_count = 0;
        game->last_time = current_time;
    }
    
    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

int main() {
    printf("Continental Architect - Fixed Version\n");
    printf("=====================================\n");
    printf("Controls:\n");
    printf("  1/2: Select tool (Push/Pull)\n");
    printf("  Mouse: Click and drag to modify terrain\n");
    printf("  +/-: Zoom in/out\n");
    printf("  Space: Toggle time acceleration\n");
    printf("  ESC: Exit\n\n");
    
    // Initialize X11 and OpenGL
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return 1;
    }
    
    int screen = DefaultScreen(display);
    
    int attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(display, screen, attribs);
    if (!vi) {
        fprintf(stderr, "No suitable visual found\n");
        return 1;
    }
    
    Window root = RootWindow(display, screen);
    XSetWindowAttributes swa = {0};
    swa.colormap = XCreateColormap(display, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    
    Window window = XCreateWindow(
        display, root,
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );
    
    XMapWindow(display, window);
    XStoreName(display, window, "Continental Architect");
    
    GLXContext ctx = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, window, ctx);
    
    // Initialize OpenGL
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.15f, 0.25f, 1.0f);
    
    // Initialize game
    game_state game = {0};
    init_terrain(&game);
    clock_gettime(CLOCK_MONOTONIC, &game.last_time);
    
    // Main loop
    int running = 1;
    int mouse_down = 0;
    
    while (running) {
        // Handle events
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            
            switch (event.type) {
                case KeyPress: {
                    KeySym key = XLookupKeysym(&event.xkey, 0);
                    switch (key) {
                        case XK_Escape:
                            running = 0;
                            break;
                        case XK_1:
                            game.tool_selected = 1;
                            printf("Tool: Push Terrain\n");
                            break;
                        case XK_2:
                            game.tool_selected = 2;
                            printf("Tool: Pull Terrain\n");
                            break;
                        case XK_space:
                            game.time_scale = (game.time_scale > 1.0f) ? 1.0f : 100.0f;
                            printf("Time scale: %.0fx\n", game.time_scale);
                            break;
                        case XK_plus:
                        case XK_equal:
                            game.camera_distance *= 0.9f;
                            if (game.camera_distance < 1.0f) game.camera_distance = 1.0f;
                            break;
                        case XK_minus:
                            game.camera_distance *= 1.1f;
                            if (game.camera_distance > 10.0f) game.camera_distance = 10.0f;
                            break;
                    }
                    break;
                }
                
                case ButtonPress:
                    if (event.xbutton.button == 1) {
                        mouse_down = 1;
                        game.mouse_x = event.xbutton.x;
                        game.mouse_y = event.xbutton.y;
                    }
                    break;
                    
                case ButtonRelease:
                    if (event.xbutton.button == 1) {
                        mouse_down = 0;
                    }
                    break;
                    
                case MotionNotify:
                    game.mouse_x = event.xmotion.x;
                    game.mouse_y = event.xmotion.y;
                    break;
            }
        }
        
        // Apply tool if mouse is down
        if (mouse_down) {
            // Convert mouse to terrain coordinates
            f32 mx = (f32)game.mouse_x / WINDOW_WIDTH * 2.0f - 1.0f;
            f32 my = (f32)game.mouse_y / WINDOW_HEIGHT * 2.0f - 1.0f;
            
            int grid_x = (int)((mx + 1.0f) * 0.5f * TERRAIN_SIZE);
            int grid_y = (int)((my + 1.0f) * 0.5f * TERRAIN_SIZE);
            
            apply_tool(&game, grid_x, grid_y);
        }
        
        // Update simulation
        simulate_geology(&game, 0.016f); // ~60 FPS
        game.camera_rotation += 0.002f;
        
        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Set up 3D camera
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        f32 aspect = (f32)WINDOW_WIDTH / WINDOW_HEIGHT;
        f32 fov = 45.0f * M_PI / 180.0f;
        f32 near = 0.1f;
        f32 far = 100.0f;
        f32 top = near * tanf(fov * 0.5f);
        f32 right = top * aspect;
        glFrustum(-right, right, -top, top, near, far);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Camera position
        f32 cam_x = sinf(game.camera_rotation) * game.camera_distance;
        f32 cam_z = cosf(game.camera_rotation) * game.camera_distance;
        f32 cam_y = game.camera_distance * 0.5f;
        
        // Look at terrain center
        glTranslatef(0, 0, -game.camera_distance);
        glRotatef(20.0f, 1, 0, 0);
        glRotatef(game.camera_rotation * 180.0f / M_PI, 0, 1, 0);
        
        render_terrain(&game);
        render_ui(&game);
        
        glXSwapBuffers(display, window);
        game.frame_count++;
        
        usleep(16666); // Cap at ~60 FPS
    }
    
    // Cleanup
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    
    printf("\nThanks for playing Continental Architect!\n");
    return 0;
}