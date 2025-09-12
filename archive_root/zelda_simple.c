#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>

// Basic types
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;

// NES palette (authentic 8-bit colors)
static u32 nes_palette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

// Game state
typedef struct {
    Display* display;
    Window window;
    XImage* screen;
    GC gc;
    u32* pixels;
    int width, height;
    
    // Player
    f32 player_x, player_y;
    int player_facing; // 0=down, 1=up, 2=left, 3=right
    
    // Input
    int key_up, key_down, key_left, key_right;
    
    // Timing
    struct timeval last_time;
} game_state;

// Initialize display
int init_display(game_state* game) {
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(game->display);
    game->width = 512;  // NES resolution scaled 2x (256x240 -> 512x480)
    game->height = 480;
    
    game->window = XCreateSimpleWindow(
        game->display, RootWindow(game->display, screen),
        0, 0, game->width, game->height, 1,
        BlackPixel(game->display, screen),
        WhitePixel(game->display, screen)
    );
    
    XSelectInput(game->display, game->window, 
                 ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
    XMapWindow(game->display, game->window);
    XStoreName(game->display, game->window, "NES Zelda Clone - Handmade Engine");
    
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    
    // Create screen buffer
    game->pixels = malloc(game->width * game->height * sizeof(u32));
    game->screen = XCreateImage(game->display, DefaultVisual(game->display, screen),
                               DefaultDepth(game->display, screen), ZPixmap, 0,
                               (char*)game->pixels, game->width, game->height, 32, 0);
    
    // Initialize player
    game->player_x = game->width / 2;
    game->player_y = game->height / 2;
    game->player_facing = 0;
    
    gettimeofday(&game->last_time, NULL);
    
    printf("✓ Display initialized: %dx%d\n", game->width, game->height);
    return 1;
}

// Clear screen with NES color
void clear_screen(game_state* game, u8 color_index) {
    u32 color = nes_palette[color_index];
    for (int i = 0; i < game->width * game->height; i++) {
        game->pixels[i] = color;
    }
}

// Draw pixel with NES palette
void draw_pixel(game_state* game, int x, int y, u8 color_index) {
    if (x >= 0 && x < game->width && y >= 0 && y < game->height) {
        game->pixels[y * game->width + x] = nes_palette[color_index];
    }
}

// Draw 8x8 sprite (simple colored rectangle for now)
void draw_sprite_8x8(game_state* game, int x, int y, u8 color_index) {
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            draw_pixel(game, x + dx, y + dy, color_index);
        }
    }
}

// Draw Link (simple 16x16 colored rectangles)
void draw_player(game_state* game) {
    int x = (int)game->player_x - 8;
    int y = (int)game->player_y - 8;
    
    // Body color based on facing direction
    u8 tunic_color = 0x2A; // Green
    u8 skin_color = 0x27;  // Peach
    
    // Simple Link sprite (2x2 8x8 tiles)
    draw_sprite_8x8(game, x, y, skin_color);     // Head
    draw_sprite_8x8(game, x + 8, y, skin_color); // Head
    draw_sprite_8x8(game, x, y + 8, tunic_color); // Body
    draw_sprite_8x8(game, x + 8, y + 8, tunic_color); // Body
    
    // Add a simple face dot
    draw_pixel(game, x + 4, y + 4, 0x0F); // Black dot for eye
    draw_pixel(game, x + 12, y + 4, 0x0F); // Black dot for eye
}

// Update game logic
void update_game(game_state* game, f32 dt) {
    f32 speed = 100.0f; // pixels per second
    
    // Move player based on input
    if (game->key_left) {
        game->player_x -= speed * dt;
        game->player_facing = 2;
    }
    if (game->key_right) {
        game->player_x += speed * dt;
        game->player_facing = 3;
    }
    if (game->key_up) {
        game->player_y -= speed * dt;
        game->player_facing = 1;
    }
    if (game->key_down) {
        game->player_y += speed * dt;
        game->player_facing = 0;
    }
    
    // Keep player on screen
    if (game->player_x < 8) game->player_x = 8;
    if (game->player_x > game->width - 8) game->player_x = game->width - 8;
    if (game->player_y < 8) game->player_y = 8;
    if (game->player_y > game->height - 8) game->player_y = game->height - 8;
}

// Render frame
void render_frame(game_state* game) {
    // Clear with NES blue-green background
    clear_screen(game, 0x21);
    
    // Draw some simple ground tiles
    for (int y = game->height - 32; y < game->height; y += 8) {
        for (int x = 0; x < game->width; x += 8) {
            draw_sprite_8x8(game, x, y, 0x28); // Brown ground
        }
    }
    
    // Draw player
    draw_player(game);
    
    // Present to screen
    XPutImage(game->display, game->window, game->gc, game->screen,
              0, 0, 0, 0, game->width, game->height);
}

// Handle input
void handle_input(game_state* game, XEvent* event) {
    if (event->type == KeyPress || event->type == KeyRelease) {
        KeySym key = XLookupKeysym(&event->xkey, 0);
        int pressed = (event->type == KeyPress);
        
        switch (key) {
            case XK_w: case XK_Up:    game->key_up = pressed; break;
            case XK_s: case XK_Down:  game->key_down = pressed; break;
            case XK_a: case XK_Left:  game->key_left = pressed; break;
            case XK_d: case XK_Right: game->key_right = pressed; break;
            case XK_Escape: exit(0); break;
        }
    }
}

// Calculate delta time
f32 get_delta_time(game_state* game) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    f32 dt = (current_time.tv_sec - game->last_time.tv_sec) +
             (current_time.tv_usec - game->last_time.tv_usec) / 1000000.0f;
    
    game->last_time = current_time;
    return dt;
}

int main() {
    printf("========================================\n");
    printf("   NES ZELDA CLONE - HANDMADE ENGINE\n");
    printf("========================================\n");
    printf("Controls: WASD or Arrow Keys to move\n");
    printf("         ESC to quit\n\n");
    
    game_state game = {0};
    
    if (!init_display(&game)) {
        return 1;
    }
    
    printf("✓ Game initialized successfully\n");
    printf("✓ NES palette loaded (%d colors)\n", 64);
    printf("✓ Starting main loop...\n\n");
    
    // Main game loop
    while (1) {
        XEvent event;
        while (XPending(game.display)) {
            XNextEvent(game.display, &event);
            handle_input(&game, &event);
            
            if (event.type == Expose) {
                render_frame(&game);
            }
        }
        
        f32 dt = get_delta_time(&game);
        update_game(&game, dt);
        render_frame(&game);
        
        // Cap at 60 FPS
        usleep(16667); // ~60 FPS
    }
    
    XCloseDisplay(game.display);
    free(game.pixels);
    return 0;
}