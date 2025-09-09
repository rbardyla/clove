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

// Tile types
#define TILE_EMPTY     0
#define TILE_GRASS     1
#define TILE_TREE      2
#define TILE_WATER     3
#define TILE_HOUSE     4
#define TILE_DIRT      5

// World size (64x60 tiles = 512x480 pixels)
#define WORLD_WIDTH  64
#define WORLD_HEIGHT 60

// Game state
typedef struct {
    Display* display;
    Window window;
    XImage* screen;
    GC gc;
    u32* pixels;
    int width, height;
    
    // World
    u8 world[WORLD_HEIGHT][WORLD_WIDTH];
    
    // Player
    f32 player_x, player_y;
    int player_facing; // 0=down, 1=up, 2=left, 3=right
    
    // Input
    int key_up, key_down, key_left, key_right;
    
    // Timing
    struct timeval last_time;
} game_state;

// Check if tile is solid
int is_solid_tile(u8 tile) {
    return (tile == TILE_TREE || tile == TILE_WATER || tile == TILE_HOUSE);
}

// Get tile at world position
u8 get_tile(game_state* game, int tile_x, int tile_y) {
    if (tile_x < 0 || tile_x >= WORLD_WIDTH || 
        tile_y < 0 || tile_y >= WORLD_HEIGHT) {
        return TILE_TREE; // World boundary is solid
    }
    return game->world[tile_y][tile_x];
}

// Initialize world with simple pattern
void init_world(game_state* game) {
    // Fill with grass
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            game->world[y][x] = TILE_GRASS;
        }
    }
    
    // Add border trees
    for (int x = 0; x < WORLD_WIDTH; x++) {
        game->world[0][x] = TILE_TREE;
        game->world[WORLD_HEIGHT-1][x] = TILE_TREE;
    }
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        game->world[y][0] = TILE_TREE;
        game->world[y][WORLD_WIDTH-1] = TILE_TREE;
    }
    
    // Add some scattered trees
    game->world[10][15] = TILE_TREE;
    game->world[10][16] = TILE_TREE;
    game->world[20][25] = TILE_TREE;
    game->world[35][40] = TILE_TREE;
    
    // Add a small pond
    game->world[15][20] = TILE_WATER;
    game->world[15][21] = TILE_WATER;
    game->world[16][20] = TILE_WATER;
    game->world[16][21] = TILE_WATER;
    
    // Add a house
    game->world[25][30] = TILE_HOUSE;
    game->world[25][31] = TILE_HOUSE;
    game->world[26][30] = TILE_HOUSE;
    game->world[26][31] = TILE_HOUSE;
    
    // Add dirt path
    for (int x = 10; x < 50; x++) {
        game->world[30][x] = TILE_DIRT;
    }
    
    printf("✓ World initialized (%dx%d tiles)\n", WORLD_WIDTH, WORLD_HEIGHT);
    printf("  - Grass fields with trees\n");
    printf("  - Small pond\n");
    printf("  - House structure\n");
    printf("  - Dirt path\n");
}

// Initialize display
int init_display(game_state* game) {
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(game->display);
    game->width = WORLD_WIDTH * 8;   // 8 pixels per tile
    game->height = WORLD_HEIGHT * 8; // 8 pixels per tile
    
    game->window = XCreateSimpleWindow(
        game->display, RootWindow(game->display, screen),
        0, 0, game->width, game->height, 1,
        BlackPixel(game->display, screen),
        WhitePixel(game->display, screen)
    );
    
    XSelectInput(game->display, game->window, 
                 ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
    XMapWindow(game->display, game->window);
    XStoreName(game->display, game->window, "NES Zelda Clone - Tile World");
    
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    
    // Create screen buffer
    game->pixels = malloc(game->width * game->height * sizeof(u32));
    game->screen = XCreateImage(game->display, DefaultVisual(game->display, screen),
                               DefaultDepth(game->display, screen), ZPixmap, 0,
                               (char*)game->pixels, game->width, game->height, 32, 0);
    
    // Initialize player at center
    game->player_x = game->width / 2;
    game->player_y = game->height / 2;
    game->player_facing = 0;
    
    gettimeofday(&game->last_time, NULL);
    
    printf("✓ Display initialized: %dx%d pixels\n", game->width, game->height);
    return 1;
}

// Draw pixel with NES palette
void draw_pixel(game_state* game, int x, int y, u8 color_index) {
    if (x >= 0 && x < game->width && y >= 0 && y < game->height) {
        game->pixels[y * game->width + x] = nes_palette[color_index];
    }
}

// Draw 8x8 tile
void draw_tile(game_state* game, int x, int y, u8 tile_type) {
    u8 color = 0x21; // Default blue-green
    
    switch (tile_type) {
        case TILE_GRASS: color = 0x2A; break; // Green
        case TILE_TREE:  color = 0x08; break; // Dark green
        case TILE_WATER: color = 0x11; break; // Blue
        case TILE_HOUSE: color = 0x16; break; // Brown
        case TILE_DIRT:  color = 0x17; break; // Light brown
    }
    
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            draw_pixel(game, x + dx, y + dy, color);
        }
    }
    
    // Add simple details for trees
    if (tile_type == TILE_TREE) {
        // Tree trunk
        for (int dy = 5; dy < 8; dy++) {
            for (int dx = 3; dx < 5; dx++) {
                draw_pixel(game, x + dx, y + dy, 0x16); // Brown
            }
        }
    }
    
    // Add simple details for house
    if (tile_type == TILE_HOUSE) {
        // Door
        draw_pixel(game, x + 3, y + 6, 0x0F); // Black
        draw_pixel(game, x + 4, y + 6, 0x0F);
        draw_pixel(game, x + 3, y + 7, 0x0F);
        draw_pixel(game, x + 4, y + 7, 0x0F);
    }
}

// Draw Link (simple 16x16 character)
void draw_player(game_state* game) {
    int x = (int)game->player_x - 8;
    int y = (int)game->player_y - 8;
    
    u8 tunic_color = 0x2A; // Green
    u8 skin_color = 0x27;  // Peach
    
    // Simple Link sprite (2x2 8x8 tiles)
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 16; dx++) {
            u8 color = tunic_color;
            
            // Head area
            if (dy < 8) {
                color = skin_color;
                // Eyes
                if ((dx == 4 || dx == 12) && dy == 4) {
                    color = 0x0F; // Black
                }
            }
            
            draw_pixel(game, x + dx, y + dy, color);
        }
    }
}

// Check collision at position
int check_collision(game_state* game, f32 x, f32 y) {
    // Check corners of player's 16x16 bounding box
    int tile_x1 = (int)(x - 8) / 8;
    int tile_y1 = (int)(y - 8) / 8;
    int tile_x2 = (int)(x + 7) / 8;
    int tile_y2 = (int)(y + 7) / 8;
    
    return is_solid_tile(get_tile(game, tile_x1, tile_y1)) ||
           is_solid_tile(get_tile(game, tile_x2, tile_y1)) ||
           is_solid_tile(get_tile(game, tile_x1, tile_y2)) ||
           is_solid_tile(get_tile(game, tile_x2, tile_y2));
}

// Update game logic
void update_game(game_state* game, f32 dt) {
    f32 speed = 80.0f; // pixels per second
    
    f32 new_x = game->player_x;
    f32 new_y = game->player_y;
    
    // Calculate new position based on input
    if (game->key_left) {
        new_x -= speed * dt;
        game->player_facing = 2;
    }
    if (game->key_right) {
        new_x += speed * dt;
        game->player_facing = 3;
    }
    if (game->key_up) {
        new_y -= speed * dt;
        game->player_facing = 1;
    }
    if (game->key_down) {
        new_y += speed * dt;
        game->player_facing = 0;
    }
    
    // Check collision and only move if valid
    if (!check_collision(game, new_x, game->player_y)) {
        game->player_x = new_x;
    }
    if (!check_collision(game, game->player_x, new_y)) {
        game->player_y = new_y;
    }
    
    // Keep player in world bounds
    if (game->player_x < 8) game->player_x = 8;
    if (game->player_x > game->width - 8) game->player_x = game->width - 8;
    if (game->player_y < 8) game->player_y = 8;
    if (game->player_y > game->height - 8) game->player_y = game->height - 8;
}

// Render frame
void render_frame(game_state* game) {
    // Draw world tiles
    for (int tile_y = 0; tile_y < WORLD_HEIGHT; tile_y++) {
        for (int tile_x = 0; tile_x < WORLD_WIDTH; tile_x++) {
            u8 tile = game->world[tile_y][tile_x];
            draw_tile(game, tile_x * 8, tile_y * 8, tile);
        }
    }
    
    // Draw player on top
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
    printf("   NES ZELDA CLONE - TILE WORLD\n");
    printf("========================================\n");
    printf("Controls: WASD or Arrow Keys to move\n");
    printf("         ESC to quit\n\n");
    
    game_state game = {0};
    
    if (!init_display(&game)) {
        return 1;
    }
    
    init_world(&game);
    
    printf("✓ Game initialized successfully\n");
    printf("✓ Collision detection active\n");
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