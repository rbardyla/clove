// editor_game_demo.c - Integrated Editor + Game Demo
// Step-by-step development with visual debugging

#include "game/game_types.h"
#include "game/crystal_dungeons.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

// ============================================================================
// EDITOR STRUCTURES
// ============================================================================

typedef struct editor_panel {
    rect bounds;
    char title[64];
    bool is_visible;
    bool is_focused;
} editor_panel;

typedef struct editor_state {
    // Panels
    editor_panel viewport;
    editor_panel inspector;
    editor_panel hierarchy;
    editor_panel console;
    
    // Editor state
    bool show_grid;
    bool show_physics;
    bool show_collision_boxes;
    bool show_ai_debug;
    bool paused;
    f32 time_scale;
    
    // Tutorial mode
    bool tutorial_mode;
    int tutorial_step;
    char tutorial_text[256];
    
    // Selected entity
    entity* selected_entity;
    
    // Console log
    char console_lines[20][128];
    int console_count;
} editor_state;

// ============================================================================
// PLATFORM LAYER
// ============================================================================

typedef struct platform_state {
    Display* display;
    Window window;
    GC gc;
    XImage* backbuffer;
    u32* pixels;
    int width;
    int height;
    bool running;
    input_state input;
    v2 mouse_pos;
} platform_state;

static platform_state g_platform;
static editor_state g_editor;
static game_state* g_game;

// Platform initialization
static bool platform_init(int width, int height) {
    g_platform.width = width;
    g_platform.height = height;
    
    g_platform.display = XOpenDisplay(NULL);
    if (!g_platform.display) {
        printf("Failed to open X display\n");
        return false;
    }
    
    int screen = DefaultScreen(g_platform.display);
    Window root = RootWindow(g_platform.display, screen);
    
    g_platform.window = XCreateSimpleWindow(
        g_platform.display, root,
        0, 0, width, height, 1,
        BlackPixel(g_platform.display, screen),
        WhitePixel(g_platform.display, screen)
    );
    
    XSelectInput(g_platform.display, g_platform.window,
                 ExposureMask | KeyPressMask | KeyReleaseMask | 
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    
    XMapWindow(g_platform.display, g_platform.window);
    XStoreName(g_platform.display, g_platform.window, "Crystal Dungeons - Editor");
    
    g_platform.gc = XCreateGC(g_platform.display, g_platform.window, 0, NULL);
    
    // Create backbuffer
    g_platform.pixels = (u32*)calloc(width * height, sizeof(u32));
    
    Visual* visual = DefaultVisual(g_platform.display, screen);
    g_platform.backbuffer = XCreateImage(
        g_platform.display, visual,
        DefaultDepth(g_platform.display, screen),
        ZPixmap, 0, (char*)g_platform.pixels,
        width, height, 32, 0
    );
    
    g_platform.running = true;
    return true;
}

// ============================================================================
// RENDERING
// ============================================================================

static void render_clear(u32 color) {
    for (int i = 0; i < g_platform.width * g_platform.height; i++) {
        g_platform.pixels[i] = color;
    }
}

static void render_rect(int x, int y, int w, int h, u32 color) {
    for (int py = y; py < y + h && py < g_platform.height; py++) {
        if (py < 0) continue;
        for (int px = x; px < x + w && px < g_platform.width; px++) {
            if (px < 0) continue;
            g_platform.pixels[py * g_platform.width + px] = color;
        }
    }
}

static void render_rect_outline(int x, int y, int w, int h, u32 color) {
    // Top & bottom
    for (int px = x; px < x + w && px < g_platform.width; px++) {
        if (px >= 0) {
            if (y >= 0 && y < g_platform.height)
                g_platform.pixels[y * g_platform.width + px] = color;
            if (y + h - 1 >= 0 && y + h - 1 < g_platform.height)
                g_platform.pixels[(y + h - 1) * g_platform.width + px] = color;
        }
    }
    // Left & right
    for (int py = y; py < y + h && py < g_platform.height; py++) {
        if (py >= 0) {
            if (x >= 0 && x < g_platform.width)
                g_platform.pixels[py * g_platform.width + x] = color;
            if (x + w - 1 >= 0 && x + w - 1 < g_platform.width)
                g_platform.pixels[py * g_platform.width + (x + w - 1)] = color;
        }
    }
}

static void render_line(int x0, int y0, int x1, int y1, u32 color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        if (x0 >= 0 && x0 < g_platform.width && y0 >= 0 && y0 < g_platform.height) {
            g_platform.pixels[y0 * g_platform.width + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void render_text(int x, int y, const char* text, u32 color) {
    // Simple text rendering - just draw rectangles for characters
    int cx = x;
    for (const char* c = text; *c; c++) {
        if (*c != ' ') {
            // Draw a small rectangle for each character
            render_rect(cx, y, 6, 8, color);
        }
        cx += 8;
    }
}

static void render_circle(int cx, int cy, int r, u32 color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r && x*x + y*y >= (r-1)*(r-1)) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < g_platform.width && py >= 0 && py < g_platform.height) {
                    g_platform.pixels[py * g_platform.width + px] = color;
                }
            }
        }
    }
}

// ============================================================================
// EDITOR RENDERING
// ============================================================================

static void editor_render_panel(editor_panel* panel) {
    if (!panel->is_visible) return;
    
    // Panel background
    u32 bg_color = panel->is_focused ? 0xFF303030 : 0xFF202020;
    render_rect((int)panel->bounds.min.x, (int)panel->bounds.min.y,
                (int)(panel->bounds.max.x - panel->bounds.min.x),
                (int)(panel->bounds.max.y - panel->bounds.min.y),
                bg_color);
    
    // Panel border
    render_rect_outline((int)panel->bounds.min.x, (int)panel->bounds.min.y,
                        (int)(panel->bounds.max.x - panel->bounds.min.x),
                        (int)(panel->bounds.max.y - panel->bounds.min.y),
                        0xFF505050);
    
    // Title bar
    render_rect((int)panel->bounds.min.x, (int)panel->bounds.min.y,
                (int)(panel->bounds.max.x - panel->bounds.min.x), 20,
                0xFF404040);
    
    // Title text
    render_text((int)panel->bounds.min.x + 5, (int)panel->bounds.min.y + 6,
                panel->title, 0xFFFFFFFF);
}

static void editor_render_grid(int grid_size) {
    u32 grid_color = 0xFF303030;
    
    // Vertical lines
    for (int x = 0; x < g_editor.viewport.bounds.max.x; x += grid_size) {
        render_line(x, (int)g_editor.viewport.bounds.min.y,
                   x, (int)g_editor.viewport.bounds.max.y, grid_color);
    }
    
    // Horizontal lines
    for (int y = 0; y < g_editor.viewport.bounds.max.y; y += grid_size) {
        render_line((int)g_editor.viewport.bounds.min.x, y,
                   (int)g_editor.viewport.bounds.max.x, y, grid_color);
    }
}

static void editor_render_game_viewport() {
    if (!g_game || !g_game->current_room) return;
    
    int vp_x = (int)g_editor.viewport.bounds.min.x;
    int vp_y = (int)g_editor.viewport.bounds.min.y + 20; // Below title bar
    
    // Render tiles
    for (int y = 0; y < ROOM_HEIGHT; y++) {
        for (int x = 0; x < ROOM_WIDTH; x++) {
            tile_type tile = g_game->current_room->tiles[y][x];
            u32 color = 0xFF101010; // Default floor
            
            switch (tile) {
                case TILE_WALL: color = 0xFF404040; break;
                case TILE_WATER: color = 0xFF0040A0; break;
                case TILE_LAVA: color = 0xFFA04000; break;
                case TILE_DOOR_OPEN: color = 0xFF604020; break;
                case TILE_CHEST: color = 0xFF806020; break;
                default: break;
            }
            
            render_rect(vp_x + x * TILE_SIZE, vp_y + y * TILE_SIZE,
                       TILE_SIZE, TILE_SIZE, color);
            
            // Tile borders for grid
            if (g_editor.show_grid) {
                render_rect_outline(vp_x + x * TILE_SIZE, vp_y + y * TILE_SIZE,
                                   TILE_SIZE, TILE_SIZE, 0xFF202020);
            }
        }
    }
    
    // Render entities
    for (u32 i = 0; i < g_game->entity_count; i++) {
        entity* e = &g_game->entities[i];
        if (!e->is_alive) continue;
        
        u32 color = 0xFFFFFFFF;
        int size = 12;
        
        // Entity color based on type
        switch (e->type) {
            case ENTITY_PLAYER:
                color = 0xFF00FF00; // Green
                size = 14;
                break;
            case ENTITY_SLIME: color = 0xFF40FF40; break;
            case ENTITY_SKELETON: color = 0xFFE0E0E0; break;
            case ENTITY_BAT: color = 0xFF800080; break;
            case ENTITY_KNIGHT: color = 0xFF808080; break;
            case ENTITY_WIZARD: color = 0xFF0080FF; break;
            case ENTITY_DRAGON: 
                color = 0xFFFF0000;
                size = 24;
                break;
            case ENTITY_HEART: color = 0xFFFF0080; break;
            case ENTITY_RUPEE: color = 0xFF00FF80; break;
            case ENTITY_KEY: color = 0xFFFFFF00; break;
            default: break;
        }
        
        // Draw entity
        int ex = vp_x + (int)e->position.x;
        int ey = vp_y + (int)e->position.y;
        render_rect(ex - size/2, ey - size/2, size, size, color);
        
        // Draw collision box if enabled
        if (g_editor.show_collision_boxes) {
            render_rect_outline(ex + (int)e->collision_box.min.x,
                               ey + (int)e->collision_box.min.y,
                               (int)(e->collision_box.max.x - e->collision_box.min.x),
                               (int)(e->collision_box.max.y - e->collision_box.min.y),
                               0xFF00FFFF);
        }
        
        // Draw velocity vector if physics debug is on
        if (g_editor.show_physics && (e->velocity.x != 0 || e->velocity.y != 0)) {
            render_line(ex, ey,
                       ex + (int)(e->velocity.x * 0.5f),
                       ey + (int)(e->velocity.y * 0.5f),
                       0xFFFF00FF);
        }
        
        // Draw AI state if AI debug is on
        if (g_editor.show_ai_debug && e->ai.brain) {
            const char* state_text = "?";
            switch (e->ai.state) {
                case AI_IDLE: state_text = "IDLE"; break;
                case AI_PATROL: state_text = "PATROL"; break;
                case AI_CHASE: state_text = "CHASE"; break;
                case AI_ATTACK: state_text = "ATTACK"; break;
                case AI_FLEE: state_text = "FLEE"; break;
                default: break;
            }
            render_text(ex - 20, ey - 20, state_text, 0xFFFFFF00);
        }
        
        // Highlight selected entity
        if (e == g_editor.selected_entity) {
            render_rect_outline(ex - size/2 - 2, ey - size/2 - 2,
                               size + 4, size + 4, 0xFFFFFF00);
        }
    }
}

static void editor_render_inspector() {
    if (!g_editor.inspector.is_visible) return;
    
    int x = (int)g_editor.inspector.bounds.min.x + 5;
    int y = (int)g_editor.inspector.bounds.min.y + 30;
    
    if (g_editor.selected_entity) {
        entity* e = g_editor.selected_entity;
        char buffer[128];
        
        snprintf(buffer, sizeof(buffer), "Entity Type: %d", e->type);
        render_text(x, y, buffer, 0xFFFFFFFF);
        y += 20;
        
        snprintf(buffer, sizeof(buffer), "Position: %.1f, %.1f", e->position.x, e->position.y);
        render_text(x, y, buffer, 0xFFFFFFFF);
        y += 20;
        
        snprintf(buffer, sizeof(buffer), "Velocity: %.1f, %.1f", e->velocity.x, e->velocity.y);
        render_text(x, y, buffer, 0xFFFFFFFF);
        y += 20;
        
        snprintf(buffer, sizeof(buffer), "Health: %.1f / %.1f", e->health, e->max_health);
        render_text(x, y, buffer, 0xFFFFFFFF);
        y += 20;
        
        if (e->ai.brain) {
            snprintf(buffer, sizeof(buffer), "AI State: %d", e->ai.state);
            render_text(x, y, buffer, 0xFFFFFFFF);
            y += 20;
        }
    } else {
        render_text(x, y, "No entity selected", 0xFF808080);
        y += 20;
        render_text(x, y, "Click on an entity to inspect", 0xFF808080);
    }
}

static void editor_render_console() {
    if (!g_editor.console.is_visible) return;
    
    int x = (int)g_editor.console.bounds.min.x + 5;
    int y = (int)g_editor.console.bounds.min.y + 30;
    
    // Render console lines
    for (int i = 0; i < g_editor.console_count && i < 20; i++) {
        render_text(x, y + i * 12, g_editor.console_lines[i], 0xFF00FF00);
    }
}

static void editor_render_tutorial() {
    if (!g_editor.tutorial_mode) return;
    
    // Tutorial overlay
    render_rect(10, 10, 400, 100, 0xCC000000);
    render_rect_outline(10, 10, 400, 100, 0xFFFFFF00);
    
    render_text(20, 20, "TUTORIAL", 0xFFFFFF00);
    render_text(20, 40, g_editor.tutorial_text, 0xFFFFFFFF);
    
    char step_text[64];
    snprintf(step_text, sizeof(step_text), "Step %d", g_editor.tutorial_step);
    render_text(20, 80, step_text, 0xFF808080);
}

// ============================================================================
// EDITOR LOGIC
// ============================================================================

static void editor_log(const char* message) {
    if (g_editor.console_count < 20) {
        strncpy(g_editor.console_lines[g_editor.console_count], message, 127);
        g_editor.console_count++;
    } else {
        // Shift lines up
        for (int i = 0; i < 19; i++) {
            strcpy(g_editor.console_lines[i], g_editor.console_lines[i + 1]);
        }
        strncpy(g_editor.console_lines[19], message, 127);
    }
}

static void editor_init() {
    // Initialize panels
    g_editor.viewport.bounds.min = v2_make(200, 50);
    g_editor.viewport.bounds.max = v2_make(800, 450);
    strcpy(g_editor.viewport.title, "Game Viewport");
    g_editor.viewport.is_visible = true;
    g_editor.viewport.is_focused = true;
    
    g_editor.hierarchy.bounds.min = v2_make(10, 50);
    g_editor.hierarchy.bounds.max = v2_make(190, 450);
    strcpy(g_editor.hierarchy.title, "Hierarchy");
    g_editor.hierarchy.is_visible = true;
    
    g_editor.inspector.bounds.min = v2_make(810, 50);
    g_editor.inspector.bounds.max = v2_make(990, 450);
    strcpy(g_editor.inspector.title, "Inspector");
    g_editor.inspector.is_visible = true;
    
    g_editor.console.bounds.min = v2_make(10, 460);
    g_editor.console.bounds.max = v2_make(990, 590);
    strcpy(g_editor.console.title, "Console");
    g_editor.console.is_visible = true;
    
    // Default settings
    g_editor.show_grid = true;
    g_editor.show_collision_boxes = false;
    g_editor.show_physics = false;
    g_editor.show_ai_debug = false;
    g_editor.paused = false;
    g_editor.time_scale = 1.0f;
    
    // Tutorial
    g_editor.tutorial_mode = true;
    g_editor.tutorial_step = 1;
    strcpy(g_editor.tutorial_text, "Welcome! Press WASD to move the player.");
    
    editor_log("Editor initialized");
    editor_log("Press F1 for help");
}

static void editor_update_tutorial() {
    if (!g_editor.tutorial_mode) return;
    
    switch (g_editor.tutorial_step) {
        case 1:
            strcpy(g_editor.tutorial_text, "Use WASD to move the green player square");
            // Check if player moved
            if (g_game && g_game->player.entity) {
                v2 pos = g_game->player.entity->position;
                if (pos.x != ROOM_WIDTH * TILE_SIZE / 2 || 
                    pos.y != ROOM_HEIGHT * TILE_SIZE / 2) {
                    g_editor.tutorial_step = 2;
                    editor_log("Good! Player is moving.");
                }
            }
            break;
            
        case 2:
            strcpy(g_editor.tutorial_text, "Press SPACE to attack");
            if (g_platform.input.keys[' ']) {
                g_editor.tutorial_step = 3;
                editor_log("Attack registered!");
            }
            break;
            
        case 3:
            strcpy(g_editor.tutorial_text, "Press C to toggle collision boxes");
            if (g_platform.input.keys['c'] || g_platform.input.keys['C']) {
                g_editor.show_collision_boxes = !g_editor.show_collision_boxes;
                g_editor.tutorial_step = 4;
                editor_log("Collision boxes toggled");
            }
            break;
            
        case 4:
            strcpy(g_editor.tutorial_text, "Press P to toggle physics debug");
            if (g_platform.input.keys['p'] || g_platform.input.keys['P']) {
                g_editor.show_physics = !g_editor.show_physics;
                g_editor.tutorial_step = 5;
                editor_log("Physics debug toggled");
            }
            break;
            
        case 5:
            strcpy(g_editor.tutorial_text, "Click on entities to select them");
            if (g_editor.selected_entity) {
                g_editor.tutorial_step = 6;
                editor_log("Entity selected!");
            }
            break;
            
        case 6:
            strcpy(g_editor.tutorial_text, "Great! Press T to toggle tutorial off");
            if (g_platform.input.keys['t'] || g_platform.input.keys['T']) {
                g_editor.tutorial_mode = false;
                editor_log("Tutorial completed!");
            }
            break;
    }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

static void handle_input() {
    XEvent event;
    
    while (XPending(g_platform.display)) {
        XNextEvent(g_platform.display, &event);
        
        switch (event.type) {
            case KeyPress: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Escape) {
                    g_platform.running = false;
                }
                if (key < 256) {
                    g_platform.input.keys[key] = true;
                }
                
                // Editor shortcuts
                if (key == XK_F1) {
                    editor_log("F1: Help | G: Grid | C: Collisions | P: Physics | A: AI Debug");
                }
                if (key == 'g' || key == 'G') {
                    g_editor.show_grid = !g_editor.show_grid;
                    editor_log(g_editor.show_grid ? "Grid ON" : "Grid OFF");
                }
                if (key == 'c' || key == 'C') {
                    g_editor.show_collision_boxes = !g_editor.show_collision_boxes;
                    editor_log(g_editor.show_collision_boxes ? "Collision boxes ON" : "Collision boxes OFF");
                }
                if (key == 'p' || key == 'P') {
                    g_editor.show_physics = !g_editor.show_physics;
                    editor_log(g_editor.show_physics ? "Physics debug ON" : "Physics debug OFF");
                }
                if (key == 'a' || key == 'A') {
                    g_editor.show_ai_debug = !g_editor.show_ai_debug;
                    editor_log(g_editor.show_ai_debug ? "AI debug ON" : "AI debug OFF");
                }
                if (key == XK_space) {
                    g_editor.paused = !g_editor.paused;
                    editor_log(g_editor.paused ? "PAUSED" : "RESUMED");
                }
                break;
            }
            
            case KeyRelease: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key < 256) {
                    g_platform.input.keys[key] = false;
                }
                break;
            }
            
            case ButtonPress: {
                g_platform.mouse_pos.x = event.xbutton.x;
                g_platform.mouse_pos.y = event.xbutton.y;
                
                // Check if click is in viewport
                if (g_platform.mouse_pos.x >= g_editor.viewport.bounds.min.x &&
                    g_platform.mouse_pos.x <= g_editor.viewport.bounds.max.x &&
                    g_platform.mouse_pos.y >= g_editor.viewport.bounds.min.y + 20 &&
                    g_platform.mouse_pos.y <= g_editor.viewport.bounds.max.y) {
                    
                    // Try to select entity
                    int vp_x = (int)g_editor.viewport.bounds.min.x;
                    int vp_y = (int)g_editor.viewport.bounds.min.y + 20;
                    
                    for (u32 i = 0; i < g_game->entity_count; i++) {
                        entity* e = &g_game->entities[i];
                        if (!e->is_alive) continue;
                        
                        int ex = vp_x + (int)e->position.x;
                        int ey = vp_y + (int)e->position.y;
                        int size = (e->type == ENTITY_PLAYER) ? 14 : 12;
                        
                        if (g_platform.mouse_pos.x >= ex - size/2 &&
                            g_platform.mouse_pos.x <= ex + size/2 &&
                            g_platform.mouse_pos.y >= ey - size/2 &&
                            g_platform.mouse_pos.y <= ey + size/2) {
                            
                            g_editor.selected_entity = e;
                            editor_log("Entity selected");
                            break;
                        }
                    }
                }
                break;
            }
            
            case MotionNotify: {
                g_platform.mouse_pos.x = event.xmotion.x;
                g_platform.mouse_pos.y = event.xmotion.y;
                break;
            }
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    printf("Crystal Dungeons Editor - Starting...\n");
    
    // Initialize platform
    if (!platform_init(1000, 600)) {
        printf("Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize editor
    editor_init();
    
    // Initialize game
    g_game = (game_state*)calloc(1, sizeof(game_state));
    game_init(g_game);
    editor_log("Game initialized");
    
    // Timing
    struct timeval last_time, current_time;
    gettimeofday(&last_time, NULL);
    const f32 fixed_timestep = 1.0f / 60.0f;
    
    // Main loop
    while (g_platform.running) {
        // Handle input
        handle_input();
        
        // Calculate delta time
        gettimeofday(&current_time, NULL);
        f32 dt = (current_time.tv_sec - last_time.tv_sec) +
                 (current_time.tv_usec - last_time.tv_usec) / 1000000.0f;
        last_time = current_time;
        
        // Update game
        if (!g_editor.paused) {
            game_handle_input(g_game, &g_platform.input);
            game_update(g_game, fixed_timestep * g_editor.time_scale);
        }
        
        // Update editor
        editor_update_tutorial();
        
        // Render
        render_clear(0xFF181818);
        
        // Render editor panels
        editor_render_panel(&g_editor.viewport);
        editor_render_panel(&g_editor.hierarchy);
        editor_render_panel(&g_editor.inspector);
        editor_render_panel(&g_editor.console);
        
        // Render game in viewport
        editor_render_game_viewport();
        
        // Render inspector content
        editor_render_inspector();
        
        // Render console content
        editor_render_console();
        
        // Render tutorial overlay
        editor_render_tutorial();
        
        // Status bar
        char status[256];
        snprintf(status, sizeof(status), 
                "FPS: %d | Entities: %d | %s | TimeScale: %.1fx",
                (int)(1.0f / dt), g_game->entity_count,
                g_editor.paused ? "PAUSED" : "RUNNING",
                g_editor.time_scale);
        render_rect(0, g_platform.height - 20, g_platform.width, 20, 0xFF303030);
        render_text(10, g_platform.height - 14, status, 0xFFFFFFFF);
        
        // Present
        XPutImage(g_platform.display, g_platform.window, g_platform.gc,
                  g_platform.backbuffer, 0, 0, 0, 0,
                  g_platform.width, g_platform.height);
        XFlush(g_platform.display);
        
        // Frame limiting
        usleep(16666); // ~60 FPS
    }
    
    // Cleanup
    game_shutdown(g_game);
    free(g_game);
    
    if (g_platform.backbuffer) {
        g_platform.backbuffer->data = NULL;
        XDestroyImage(g_platform.backbuffer);
    }
    if (g_platform.pixels) {
        free(g_platform.pixels);
    }
    if (g_platform.gc) {
        XFreeGC(g_platform.display, g_platform.gc);
    }
    if (g_platform.window) {
        XDestroyWindow(g_platform.display, g_platform.window);
    }
    if (g_platform.display) {
        XCloseDisplay(g_platform.display);
    }
    
    printf("Editor shutdown complete\n");
    return 0;
}

// Game function stubs (same as before)
#include "game/crystal_dungeons.c"
#include "game/sprite_assets.c"

// Stub functions removed - using real implementations from included files