#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
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

// NPC types
#define NPC_FARMER     0
#define NPC_VILLAGER   1
#define NPC_MERCHANT   2

// NPC states
#define STATE_WANDER   0
#define STATE_WORK     1
#define STATE_GATHER   2
#define STATE_HOME     3

#define MAX_NPCS 10

// NPC structure
typedef struct {
    f32 x, y;
    f32 target_x, target_y;
    int type;
    int state;
    f32 state_timer;
    f32 work_x, work_y;  // Work location
    f32 home_x, home_y;  // Home location
    u8 color;
    int facing;
    int active;
} npc;

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
    int player_facing;
    
    // NPCs
    npc npcs[MAX_NPCS];
    int npc_count;
    
    // Input
    int key_up, key_down, key_left, key_right;
    
    // Timing
    struct timeval last_time;
    f32 time_of_day; // 0-24 hours
} game_state;

// Check if tile is solid
int is_solid_tile(u8 tile) {
    return (tile == TILE_TREE || tile == TILE_WATER || tile == TILE_HOUSE);
}

// Get tile at world position
u8 get_tile(game_state* game, int tile_x, int tile_y) {
    if (tile_x < 0 || tile_x >= WORLD_WIDTH || 
        tile_y < 0 || tile_y >= WORLD_HEIGHT) {
        return TILE_TREE;
    }
    return game->world[tile_y][tile_x];
}

// Distance between two points
f32 distance(f32 x1, f32 y1, f32 x2, f32 y2) {
    f32 dx = x2 - x1;
    f32 dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

// Initialize NPCs
void init_npcs(game_state* game) {
    game->npc_count = 5;
    
    // Farmer near house
    game->npcs[0] = (npc){
        .x = 240, .y = 200, .type = NPC_FARMER,
        .work_x = 220, .work_y = 180, .home_x = 240, .home_y = 200,
        .color = 0x16, .state = STATE_WANDER, .active = 1
    };
    
    // Village wanderer
    game->npcs[1] = (npc){
        .x = 180, .y = 300, .type = NPC_VILLAGER,
        .work_x = 160, .work_y = 280, .home_x = 180, .home_y = 300,
        .color = 0x22, .state = STATE_WANDER, .active = 1
    };
    
    // Merchant
    game->npcs[2] = (npc){
        .x = 300, .y = 250, .type = NPC_MERCHANT,
        .work_x = 320, .work_y = 240, .home_x = 300, .home_y = 250,
        .color = 0x14, .state = STATE_WORK, .active = 1
    };
    
    // Worker
    game->npcs[3] = (npc){
        .x = 160, .y = 200, .type = NPC_VILLAGER,
        .work_x = 140, .work_y = 220, .home_x = 160, .home_y = 200,
        .color = 0x29, .state = STATE_GATHER, .active = 1
    };
    
    // Guard
    game->npcs[4] = (npc){
        .x = 260, .y = 240, .type = NPC_VILLAGER,
        .work_x = 280, .work_y = 240, .home_x = 260, .home_y = 240,
        .color = 0x12, .state = STATE_WANDER, .active = 1
    };
    
    printf("✓ %d NPCs initialized\n", game->npc_count);
    printf("  - Farmer (works fields)\n");
    printf("  - Villagers (wander/gather)\n");
    printf("  - Merchant (trades)\n");
    printf("  - Worker (gathers resources)\n");
    printf("  - Guard (patrols)\n");
}

// Initialize world with village
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
    
    // Village center with houses
    game->world[25][30] = TILE_HOUSE; // Main house
    game->world[25][31] = TILE_HOUSE;
    game->world[26][30] = TILE_HOUSE;
    game->world[26][31] = TILE_HOUSE;
    
    game->world[20][25] = TILE_HOUSE; // Second house
    game->world[20][26] = TILE_HOUSE;
    game->world[21][25] = TILE_HOUSE;
    game->world[21][26] = TILE_HOUSE;
    
    game->world[30][35] = TILE_HOUSE; // Third house
    game->world[30][36] = TILE_HOUSE;
    game->world[31][35] = TILE_HOUSE;
    game->world[31][36] = TILE_HOUSE;
    
    // Village paths
    for (int x = 15; x < 45; x++) {
        game->world[28][x] = TILE_DIRT; // Main road
    }
    for (int y = 22; y < 35; y++) {
        game->world[y][32] = TILE_DIRT; // Cross road
    }
    
    // Farm fields (dirt patches)
    for (int y = 18; y < 25; y++) {
        for (int x = 20; x < 28; x++) {
            if ((x + y) % 3 == 0) {
                game->world[y][x] = TILE_DIRT;
            }
        }
    }
    
    // Village pond
    game->world[35][20] = TILE_WATER;
    game->world[35][21] = TILE_WATER;
    game->world[36][20] = TILE_WATER;
    game->world[36][21] = TILE_WATER;
    
    // Scattered trees around village
    game->world[15][15] = TILE_TREE;
    game->world[40][45] = TILE_TREE;
    game->world[45][25] = TILE_TREE;
    game->world[10][40] = TILE_TREE;
    
    printf("✓ Village world initialized\n");
    printf("  - 3 houses with dirt road connections\n");
    printf("  - Farm fields for NPCs to work\n");
    printf("  - Village pond\n");
    printf("  - Forest border\n");
}

// Initialize display
int init_display(game_state* game) {
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(game->display);
    game->width = WORLD_WIDTH * 8;
    game->height = WORLD_HEIGHT * 8;
    
    game->window = XCreateSimpleWindow(
        game->display, RootWindow(game->display, screen),
        0, 0, game->width, game->height, 1,
        BlackPixel(game->display, screen),
        WhitePixel(game->display, screen)
    );
    
    XSelectInput(game->display, game->window, 
                 ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
    XMapWindow(game->display, game->window);
    XStoreName(game->display, game->window, "NES Zelda Clone - Village Life");
    
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    
    game->pixels = malloc(game->width * game->height * sizeof(u32));
    game->screen = XCreateImage(game->display, DefaultVisual(game->display, screen),
                               DefaultDepth(game->display, screen), ZPixmap, 0,
                               (char*)game->pixels, game->width, game->height, 32, 0);
    
    // Initialize player
    game->player_x = game->width / 2;
    game->player_y = game->height / 2;
    game->player_facing = 0;
    
    game->time_of_day = 8.0f; // Start at 8 AM
    
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
    u8 color = 0x21;
    
    switch (tile_type) {
        case TILE_GRASS: color = 0x2A; break;
        case TILE_TREE:  color = 0x08; break;
        case TILE_WATER: color = 0x11; break;
        case TILE_HOUSE: color = 0x16; break;
        case TILE_DIRT:  color = 0x17; break;
    }
    
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            draw_pixel(game, x + dx, y + dy, color);
        }
    }
    
    // Add details
    if (tile_type == TILE_TREE) {
        // Tree trunk
        for (int dy = 5; dy < 8; dy++) {
            for (int dx = 3; dx < 5; dx++) {
                draw_pixel(game, x + dx, y + dy, 0x16);
            }
        }
    }
    
    if (tile_type == TILE_HOUSE) {
        // Door
        draw_pixel(game, x + 3, y + 6, 0x0F);
        draw_pixel(game, x + 4, y + 6, 0x0F);
        draw_pixel(game, x + 3, y + 7, 0x0F);
        draw_pixel(game, x + 4, y + 7, 0x0F);
        // Window
        draw_pixel(game, x + 1, y + 2, 0x21);
        draw_pixel(game, x + 6, y + 2, 0x21);
    }
}

// Draw character (player or NPC)
void draw_character(game_state* game, f32 x, f32 y, u8 color, int is_player) {
    int px = (int)x - 8;
    int py = (int)y - 8;
    
    u8 skin_color = 0x27;
    
    // Draw simple 16x16 character
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 16; dx++) {
            u8 draw_color = color;
            
            // Head area
            if (dy < 8) {
                draw_color = skin_color;
                // Eyes for player
                if (is_player && ((dx == 4 || dx == 12) && dy == 4)) {
                    draw_color = 0x0F;
                }
            }
            
            draw_pixel(game, px + dx, py + dy, draw_color);
        }
    }
}

// Check collision
int check_collision(game_state* game, f32 x, f32 y) {
    int tile_x1 = (int)(x - 8) / 8;
    int tile_y1 = (int)(y - 8) / 8;
    int tile_x2 = (int)(x + 7) / 8;
    int tile_y2 = (int)(y + 7) / 8;
    
    return is_solid_tile(get_tile(game, tile_x1, tile_y1)) ||
           is_solid_tile(get_tile(game, tile_x2, tile_y1)) ||
           is_solid_tile(get_tile(game, tile_x1, tile_y2)) ||
           is_solid_tile(get_tile(game, tile_x2, tile_y2));
}

// Update NPC AI
void update_npc(npc* n, f32 dt, f32 time_of_day) {
    if (!n->active) return;
    
    // Update state timer
    n->state_timer -= dt;
    
    // State machine
    switch (n->state) {
        case STATE_WANDER:
            if (n->state_timer <= 0) {
                // Pick random nearby target
                n->target_x = n->x + (rand() % 80 - 40);
                n->target_y = n->y + (rand() % 80 - 40);
                n->state_timer = 2.0f + (rand() % 400) / 100.0f; // 2-6 seconds
                
                // Sometimes switch to work during day
                if (time_of_day > 8 && time_of_day < 18 && (rand() % 100) < 30) {
                    n->state = STATE_WORK;
                    n->target_x = n->work_x;
                    n->target_y = n->work_y;
                    n->state_timer = 5.0f;
                }
            }
            break;
            
        case STATE_WORK:
            if (n->state_timer <= 0) {
                if (n->type == NPC_FARMER) {
                    // Farmer switches to gather
                    n->state = STATE_GATHER;
                    n->state_timer = 3.0f;
                } else {
                    // Others wander
                    n->state = STATE_WANDER;
                    n->state_timer = 1.0f;
                }
            }
            break;
            
        case STATE_GATHER:
            if (n->state_timer <= 0) {
                // Go back to work or wander
                if ((rand() % 100) < 60) {
                    n->state = STATE_WORK;
                    n->target_x = n->work_x;
                    n->target_y = n->work_y;
                } else {
                    n->state = STATE_WANDER;
                }
                n->state_timer = 2.0f;
            }
            break;
            
        case STATE_HOME:
            if (n->state_timer <= 0 && time_of_day > 6) {
                n->state = STATE_WANDER;
                n->state_timer = 1.0f;
            }
            break;
    }
    
    // Go home at night
    if (time_of_day < 6 || time_of_day > 20) {
        if (n->state != STATE_HOME) {
            n->state = STATE_HOME;
            n->target_x = n->home_x;
            n->target_y = n->home_y;
            n->state_timer = 1.0f;
        }
    }
    
    // Move towards target
    f32 dist = distance(n->x, n->y, n->target_x, n->target_y);
    if (dist > 4.0f) {
        f32 speed = 30.0f; // Slower than player
        f32 dx = (n->target_x - n->x) / dist;
        f32 dy = (n->target_y - n->y) / dist;
        
        f32 new_x = n->x + dx * speed * dt;
        f32 new_y = n->y + dy * speed * dt;
        
        // Simple collision check (basic)
        if (new_x > 16 && new_x < WORLD_WIDTH * 8 - 16) {
            n->x = new_x;
        }
        if (new_y > 16 && new_y < WORLD_HEIGHT * 8 - 16) {
            n->y = new_y;
        }
    }
}

// Update game logic
void update_game(game_state* game, f32 dt) {
    // Update time of day (24 hour cycle in 5 minutes)
    game->time_of_day += dt / 12.5f; // 5 minutes = 300 seconds = 24 hours
    if (game->time_of_day >= 24.0f) {
        game->time_of_day = 0.0f;
    }
    
    // Update player
    f32 speed = 80.0f;
    f32 new_x = game->player_x;
    f32 new_y = game->player_y;
    
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
    
    if (!check_collision(game, new_x, game->player_y)) {
        game->player_x = new_x;
    }
    if (!check_collision(game, game->player_x, new_y)) {
        game->player_y = new_y;
    }
    
    // Keep player in bounds
    if (game->player_x < 8) game->player_x = 8;
    if (game->player_x > game->width - 8) game->player_x = game->width - 8;
    if (game->player_y < 8) game->player_y = 8;
    if (game->player_y > game->height - 8) game->player_y = game->height - 8;
    
    // Update NPCs
    for (int i = 0; i < game->npc_count; i++) {
        update_npc(&game->npcs[i], dt, game->time_of_day);
    }
}

// Render frame
void render_frame(game_state* game) {
    // Draw world
    for (int tile_y = 0; tile_y < WORLD_HEIGHT; tile_y++) {
        for (int tile_x = 0; tile_x < WORLD_WIDTH; tile_x++) {
            u8 tile = game->world[tile_y][tile_x];
            draw_tile(game, tile_x * 8, tile_y * 8, tile);
        }
    }
    
    // Draw NPCs
    for (int i = 0; i < game->npc_count; i++) {
        if (game->npcs[i].active) {
            draw_character(game, game->npcs[i].x, game->npcs[i].y, game->npcs[i].color, 0);
        }
    }
    
    // Draw player on top
    draw_character(game, game->player_x, game->player_y, 0x2A, 1);
    
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
    printf("   NES ZELDA CLONE - LIVING VILLAGE\n");
    printf("========================================\n");
    printf("Controls: WASD or Arrow Keys to move\n");
    printf("         ESC to quit\n\n");
    
    srand(time(NULL));
    game_state game = {0};
    
    if (!init_display(&game)) {
        return 1;
    }
    
    init_world(&game);
    init_npcs(&game);
    
    printf("✓ Village life simulation active\n");
    printf("✓ NPCs have daily routines\n");
    printf("✓ Time of day affects behavior\n");
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
        
        usleep(16667); // 60 FPS
    }
    
    XCloseDisplay(game.display);
    free(game.pixels);
    return 0;
}