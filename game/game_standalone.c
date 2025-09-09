// game_standalone.c - Standalone Crystal Dungeons executable
// This version compiles independently with minimal dependencies

#include "game_types.h"
#include "crystal_dungeons.h"
#include "sprite_assets.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#include <unistd.h>

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
} platform_state;

static platform_state g_platform;

// Initialize X11 window
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
    XStoreName(g_platform.display, g_platform.window, "Crystal Dungeons");
    
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

static void platform_shutdown(void) {
    if (g_platform.backbuffer) {
        g_platform.backbuffer->data = NULL;  // Prevent double-free
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
}

static void platform_handle_events(void) {
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
                
                // Map arrow keys
                if (key == XK_Up) g_platform.input.keys['w'] = true;
                if (key == XK_Down) g_platform.input.keys['s'] = true;
                if (key == XK_Left) g_platform.input.keys['a'] = true;
                if (key == XK_Right) g_platform.input.keys['d'] = true;
                if (key == XK_space) g_platform.input.keys[' '] = true;
                break;
            }
            
            case KeyRelease: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key < 256) {
                    g_platform.input.keys[key] = false;
                }
                
                // Map arrow keys
                if (key == XK_Up) g_platform.input.keys['w'] = false;
                if (key == XK_Down) g_platform.input.keys['s'] = false;
                if (key == XK_Left) g_platform.input.keys['a'] = false;
                if (key == XK_Right) g_platform.input.keys['d'] = false;
                if (key == XK_space) g_platform.input.keys[' '] = false;
                break;
            }
            
            case MotionNotify: {
                g_platform.input.mouse_position.x = event.xmotion.x;
                g_platform.input.mouse_position.y = event.xmotion.y;
                break;
            }
            
            case ButtonPress: {
                if (event.xbutton.button <= 3) {
                    g_platform.input.mouse_buttons[event.xbutton.button - 1] = true;
                }
                break;
            }
            
            case ButtonRelease: {
                if (event.xbutton.button <= 3) {
                    g_platform.input.mouse_buttons[event.xbutton.button - 1] = false;
                }
                break;
            }
        }
    }
}

static void platform_present(void) {
    XPutImage(g_platform.display, g_platform.window, g_platform.gc,
              g_platform.backbuffer, 0, 0, 0, 0,
              g_platform.width, g_platform.height);
    XFlush(g_platform.display);
}

// ============================================================================
// SIMPLE RENDERER
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

static void render_sprite(int x, int y, int size, u32* sprite_data) {
    for (int sy = 0; sy < size; sy++) {
        int py = y + sy;
        if (py < 0 || py >= g_platform.height) continue;
        
        for (int sx = 0; sx < size; sx++) {
            int px = x + sx;
            if (px < 0 || px >= g_platform.width) continue;
            
            u32 pixel = sprite_data[sy * size + sx];
            if ((pixel & 0xFF000000) != 0) {  // Check alpha
                g_platform.pixels[py * g_platform.width + px] = pixel;
            }
        }
    }
}

static void render_text(int x, int y, const char* text, u32 color) {
    // Simple debug text rendering (just draw rectangles for now)
    int cx = x;
    for (const char* c = text; *c; c++) {
        if (*c != ' ') {
            render_rect(cx, y, 6, 8, color);
        }
        cx += 8;
    }
}

// ============================================================================
// GAME IMPLEMENTATION (simplified)
// ============================================================================

// Include the game implementation
#include "crystal_dungeons.c"
#include "sprite_assets.c"

// Simple neural network stubs
neural_network* neural_create(void) { return (neural_network*)calloc(1, sizeof(int)); }
void neural_destroy(neural_network* net) { if (net) free(net); }
void neural_forward(neural_network* net, f32* input, f32* output) {
    // Simple random behavior for now
    for (int i = 0; i < 8; i++) {
        output[i] = ((f32)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
}
void neural_add_layer(neural_network* net, int inputs, int outputs, int activation) {}
void neural_randomize(neural_network* net, f32 range) {}

// Audio stubs
void game_audio_init(void) {}
void game_audio_shutdown(void) {}
void game_audio_sword_swing(void) {}
void game_audio_enemy_hit(void) {}
void game_audio_player_hurt(void) {}
void game_audio_item_pickup(void) {}
void game_audio_door_open(void) {}
void game_audio_explosion(void) {}
void game_audio_magic(void) {}
void game_audio_play_music(int id) {}
void game_audio_stop_music(void) {}
void game_audio_update(f32* buffer, u32 count) {}

// ============================================================================
// MAIN GAME LOOP
// ============================================================================

int main(int argc, char** argv) {
    printf("Crystal Dungeons - Starting...\n");
    
    // Initialize platform
    if (!platform_init(800, 600)) {
        printf("Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize game
    game_state* game = (game_state*)calloc(1, sizeof(game_state));
    sprite_assets* assets = (sprite_assets*)calloc(1, sizeof(sprite_assets));
    
    printf("Initializing game...\n");
    game_init(game);
    
    printf("Initializing sprite assets...\n");
    sprite_assets_init(assets);
    
    printf("Game started! Use WASD/Arrow keys to move, Space to attack, ESC to quit.\n");
    
    // Timing
    struct timeval last_time, current_time;
    gettimeofday(&last_time, NULL);
    f32 accumulator = 0.0f;
    const f32 fixed_timestep = 1.0f / 60.0f;
    
    // Main game loop
    while (g_platform.running) {
        // Handle events
        platform_handle_events();
        
        // Calculate delta time
        gettimeofday(&current_time, NULL);
        f32 dt = (current_time.tv_sec - last_time.tv_sec) +
                 (current_time.tv_usec - last_time.tv_usec) / 1000000.0f;
        last_time = current_time;
        
        // Clamp delta time
        if (dt > 0.25f) dt = 0.25f;
        accumulator += dt;
        
        // Fixed timestep update
        while (accumulator >= fixed_timestep) {
            // Update input
            game_handle_input(game, &g_platform.input);
            
            // Update game
            game_update(game, fixed_timestep);
            
            accumulator -= fixed_timestep;
        }
        
        // Render
        render_clear(0xFF000000);
        
        // Render game (simplified)
        game_render(game);
        
        // Simple game rendering
        if (game->current_room) {
            // Render tiles
            for (int y = 0; y < ROOM_HEIGHT; y++) {
                for (int x = 0; x < ROOM_WIDTH; x++) {
                    tile_type tile = game->current_room->tiles[y][x];
                    u32 color = 0xFF202020;  // Default floor
                    
                    switch (tile) {
                        case TILE_WALL: color = 0xFF404040; break;
                        case TILE_WATER: color = 0xFF0040FF; break;
                        case TILE_LAVA: color = 0xFFFF4000; break;
                        case TILE_DOOR_OPEN: color = 0xFF804020; break;
                        case TILE_CHEST: color = 0xFFFFD700; break;
                        default: break;
                    }
                    
                    render_rect(x * TILE_SIZE, y * TILE_SIZE, 
                               TILE_SIZE, TILE_SIZE, color);
                }
            }
        }
        
        // Render entities
        for (u32 i = 0; i < game->entity_count; i++) {
            entity* e = &game->entities[i];
            if (!e->is_alive) continue;
            
            u32 color = 0xFFFFFFFF;
            int size = 12;
            
            switch (e->type) {
                case ENTITY_PLAYER: 
                    color = 0xFF00FF00;  // Green
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
            
            render_rect((int)e->position.x - size/2, 
                       (int)e->position.y - size/2,
                       size, size, color);
        }
        
        // Render HUD
        char hud_text[256];
        
        // Health
        render_text(10, 10, "Health:", 0xFFFFFFFF);
        int hearts = (int)(game->player.entity->health);
        for (int i = 0; i < hearts && i < 10; i++) {
            render_rect(80 + i * 20, 10, 16, 16, 0xFFFF0080);
        }
        
        // Rupees
        snprintf(hud_text, sizeof(hud_text), "Rupees: %d", game->player.rupees);
        render_text(10, 30, hud_text, 0xFF00FF80);
        
        // Keys
        snprintf(hud_text, sizeof(hud_text), "Keys: %d", game->player.keys);
        render_text(10, 50, hud_text, 0xFFFFFF00);
        
        // Debug info
        snprintf(hud_text, sizeof(hud_text), "FPS: %d", (int)(1.0f / dt));
        render_text(g_platform.width - 100, 10, hud_text, 0xFFFFFF00);
        
        // Present
        platform_present();
        
        // Frame rate limiting
        usleep(1000);  // Small sleep to prevent CPU spinning
    }
    
    printf("Shutting down...\n");
    
    // Cleanup
    game_shutdown(game);
    sprite_assets_shutdown(assets);
    free(game);
    free(assets);
    
    platform_shutdown();
    
    printf("Goodbye!\n");
    return 0;
}