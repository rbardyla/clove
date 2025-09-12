#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>

// Basic types
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;

// NES palette
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

#define WORLD_WIDTH  128
#define WORLD_HEIGHT 96
#define MAX_NPCS 10
#define MAX_MEMORIES 20

// Tile types
#define TILE_EMPTY     0
#define TILE_GRASS     1
#define TILE_TREE      2
#define TILE_WATER     3
#define TILE_HOUSE     4
#define TILE_DIRT      5
#define TILE_FLOWER    6
#define TILE_STONE     7
#define TILE_FARM      8

// Item types
#define ITEM_NONE     0
#define ITEM_FLOWER   1
#define ITEM_STONE    2
#define ITEM_WOOD     3

// Memory types
typedef enum {
    MEM_FIRST_MEETING,
    MEM_FRIENDLY_CHAT,
    MEM_RECEIVED_GIFT,
    MEM_HELPED,
    MEM_QUEST
} memory_type;

// Memory structure
typedef struct {
    memory_type type;
    f32 game_time;
    f32 emotional_impact;
    char detail[64];
    int times_recalled;
} memory;

// NPC with learning
typedef struct {
    u32 id;
    char name[32];
    char job[32];
    
    f32 x, y;
    f32 vx, vy;
    u8 color;
    
    // Personality
    f32 friendliness;
    f32 trust;
    f32 mood;
    
    // Learning
    memory memories[MAX_MEMORIES];
    u32 memory_count;
    u32 times_met;
    
    // Current state
    char thought[128];
    char action[32];
} npc;

// Game state
typedef struct {
    u8 world[WORLD_HEIGHT][WORLD_WIDTH];
    npc npcs[MAX_NPCS];
    u32 npc_count;
    
    f32 player_x, player_y;
    f32 player_vx, player_vy;
    
    // Inventory
    u32 inventory[10];
    u32 flowers_collected;
    u32 stones_collected;
    u32 wood_collected;
    
    u8 show_debug;
    u8 dialog_active;
    u32 dialog_npc_id;
    char dialog_text[256];
    f32 dialog_timer;
    
    f32 game_time;
    
    // X11
    Display* display;
    Window window;
    GC gc;
    int screen;
    
    // Input
    u8 keys_held[4];  // W A S D
    
    FILE* log_file;
} game_state;

// Improved 8x8 font - more readable characters
static u8 font_8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // "
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // #
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // $
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // %
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // &
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // /
    {0x7C,0xCE,0xDE,0xF6,0xE6,0xC6,0x7C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00}, // 2
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, // 3
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 4
    {0xFE,0xC0,0xC0,0xFC,0x06,0xC6,0x7C,0x00}, // 5
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, // 6
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 8
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // <
    {0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00}, // =
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // >
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, // ?
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x7C,0x00}, // @
    {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // A
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // B
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // C
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // D
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // E
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // F
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // G
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // J
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // K
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // L
    {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, // M
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // N
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // O
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // P
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xCE,0x7C,0x0E}, // Q
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // R
    {0x7C,0xC6,0xE0,0x78,0x0E,0xC6,0x7C,0x00}, // S
    {0x7E,0x7E,0x5A,0x18,0x18,0x18,0x3C,0x00}, // T
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // U
    {0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // V
    {0xC6,0xC6,0xC6,0xD6,0xD6,0xFE,0x6C,0x00}, // W
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // Y
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // backslash
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // a
    {0xE0,0x60,0x60,0x7C,0x66,0x66,0xDC,0x00}, // b
    {0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00}, // c
    {0x1C,0x0C,0x0C,0x7C,0xCC,0xCC,0x76,0x00}, // d
    {0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00}, // e
    {0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00}, // f
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // g
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // h
    {0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00}, // i
    {0x0C,0x00,0x1C,0x0C,0x0C,0xCC,0xCC,0x78}, // j
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // k
    {0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00}, // l
    {0x00,0x00,0xCC,0xFE,0xFE,0xD6,0xC6,0x00}, // m
    {0x00,0x00,0xF8,0xCC,0xCC,0xCC,0xCC,0x00}, // n
    {0x00,0x00,0x78,0xCC,0xCC,0xCC,0x78,0x00}, // o
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // p
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // q
    {0x00,0x00,0xDC,0x76,0x66,0x60,0xF0,0x00}, // r
    {0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00}, // s
    {0x10,0x30,0x7C,0x30,0x30,0x34,0x18,0x00}, // t
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // u
    {0x00,0x00,0xCC,0xCC,0xCC,0x78,0x30,0x00}, // v
    {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, // w
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // x
    {0x00,0x00,0xCC,0xCC,0xCC,0x7C,0x0C,0xF8}, // y
    {0x00,0x00,0xFC,0x98,0x30,0x64,0xFC,0x00}, // z
    {0x1C,0x30,0x30,0xE0,0x30,0x30,0x1C,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0xE0,0x30,0x30,0x1C,0x30,0x30,0xE0,0x00}, // }
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}  // Block
};

// Draw a single character scaled 3x for better readability
void draw_char(game_state* game, int x, int y, char c, u32 color) {
    // Convert to font index
    if (c < 32) c = 32;
    if (c > 127) c = 32;
    int idx = c - 32;
    if (idx < 0 || idx >= 96) idx = 0;
    
    u8* bitmap = font_8x8[idx];
    
    // Draw at 3x scale for readability
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (1 << (7 - col))) {
                XSetForeground(game->display, game->gc, color);
                // 3x3 pixel blocks
                XFillRectangle(game->display, game->window, game->gc,
                             x + col * 3, y + row * 3, 3, 3);
            }
        }
    }
}

// Draw text string
void draw_text(game_state* game, int x, int y, const char* text, u32 color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_char(game, x + i * 25, y, text[i], color);  // 25 pixel spacing for 3x scale
    }
}

// Check if tile is solid
int is_solid_tile(u8 tile) {
    return tile == TILE_TREE || tile == TILE_WATER || tile == TILE_HOUSE;
}

// Log learning event
void log_event(game_state* game, const char* npc_name, const char* event) {
    if (!game->log_file) {
        game->log_file = fopen("learning.log", "a");
    }
    if (game->log_file) {
        fprintf(game->log_file, "[%.1f] %s: %s\n", game->game_time, npc_name, event);
        fflush(game->log_file);
    }
    printf("[LEARN] %s: %s\n", npc_name, event);
}

// Add memory
void add_memory(npc* n, memory_type type, const char* detail, f32 impact, game_state* game) {
    if (n->memory_count >= MAX_MEMORIES) {
        // Shift all memories left, removing the oldest
        for (u32 i = 0; i < MAX_MEMORIES - 1; i++) {
            n->memories[i] = n->memories[i + 1];
        }
        n->memory_count = MAX_MEMORIES - 1;
    }
    
    memory* m = &n->memories[n->memory_count];
    m->type = type;
    m->game_time = game->game_time;
    m->emotional_impact = impact;
    strncpy(m->detail, detail, 63);
    m->detail[63] = '\0';  // Ensure null termination
    m->times_recalled = 0;
    n->memory_count++;
    
    n->trust += impact * 10.0f;
    if (n->trust > 100.0f) n->trust = 100.0f;
    if (n->trust < -100.0f) n->trust = -100.0f;
    
    char log_msg[128];
    snprintf(log_msg, 128, "Memory: %s (trust=%.0f)", detail, n->trust);
    log_event(game, n->name, log_msg);
}

// Generate dialog
void generate_dialog(npc* n, game_state* game) {
    if (n->times_met == 0) {
        snprintf(game->dialog_text, 255, "%s: Hello! I'm %s the %s.", 
                n->name, n->name, n->job);
        add_memory(n, MEM_FIRST_MEETING, "Met someone new", 0.2f, game);
        n->times_met = 1;
        return;
    }
    
    if (n->times_met == 1) {
        snprintf(game->dialog_text, 255, "%s: Hey! You're back! Nice to see you.", n->name);
        n->times_met = 2;
        return;
    }
    
    // Check if player has gifts
    if (game->flowers_collected > 0 && n->trust < 80) {
        snprintf(game->dialog_text, 255, "%s: Oh! You have flowers! How nice!", n->name);
        add_memory(n, MEM_RECEIVED_GIFT, "Got flowers", 0.5f, game);
        game->flowers_collected--;
        // Trust already increased in add_memory, no need to add more
        return;
    }
    
    if (n->trust > 50.0f) {
        if (n->memory_count > 2) {
            snprintf(game->dialog_text, 255, "%s: Friend! Remember when %s?", 
                    n->name, n->memories[n->memory_count-1].detail);
        } else {
            snprintf(game->dialog_text, 255, "%s: Good to see you friend!", n->name);
        }
    } else if (n->trust < -20.0f) {
        snprintf(game->dialog_text, 255, "%s: What do you want?", n->name);
    } else {
        snprintf(game->dialog_text, 255, "%s: Hello again!", n->name);
    }
    
    add_memory(n, MEM_FRIENDLY_CHAT, "Chatted", 0.1f, game);
    n->times_met++;
}

// Initialize NPC
void init_npc(npc* n, u32 id, const char* name, const char* job) {
    n->id = id;
    strncpy(n->name, name, 31);
    n->name[31] = '\0';  // Ensure null termination
    strncpy(n->job, job, 31);
    n->job[31] = '\0';  // Ensure null termination
    
    n->x = 300.0f + (rand() % 400);
    n->y = 200.0f + (rand() % 300);
    n->vx = 0;
    n->vy = 0;
    
    n->friendliness = 0.3f + (rand() % 70) / 100.0f;
    n->trust = 0;
    n->mood = 0.5f;
    
    n->memory_count = 0;
    n->times_met = 0;
    
    strcpy(n->thought, "Nice day today.");
    strcpy(n->action, "standing");
    
    if (strcmp(job, "Farmer") == 0) n->color = 0x1A;
    else if (strcmp(job, "Merchant") == 0) n->color = 0x16;
    else if (strcmp(job, "Guard") == 0) n->color = 0x14;
    else n->color = 0x12;
}

// Update NPC
void update_npc(npc* n, f32 dt, game_state* game) {
    if (rand() % 100 < 2) {
        n->vx = (rand() % 3 - 1) * 30.0f;
        n->vy = (rand() % 3 - 1) * 30.0f;
    }
    
    n->vx *= 0.95f;
    n->vy *= 0.95f;
    
    n->x += n->vx * dt;
    n->y += n->vy * dt;
    
    if (n->x < 100) { n->x = 100; n->vx = 0; }
    if (n->x > 900) { n->x = 900; n->vx = 0; }
    if (n->y < 100) { n->y = 100; n->vy = 0; }
    if (n->y > 600) { n->y = 600; n->vy = 0; }
    
    if (fabsf(n->vx) > 1 || fabsf(n->vy) > 1) {
        strcpy(n->action, "walking");
    } else {
        strcpy(n->action, "standing");
    }
    
    if (n->trust > 50) {
        strcpy(n->thought, "Life is good!");
    } else if (n->trust < -20) {
        strcpy(n->thought, "Hmm...");
    } else {
        strcpy(n->thought, "Nice day.");
    }
}

// Initialize game
void init_game(game_state* game) {
    memset(game, 0, sizeof(game_state));
    
    // Create world with resources
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            int r = rand() % 100;
            if (r < 5) {
                game->world[y][x] = TILE_TREE;
            } else if (r < 8) {
                game->world[y][x] = TILE_FLOWER;
            } else if (r < 10) {
                game->world[y][x] = TILE_STONE;
            } else {
                game->world[y][x] = TILE_GRASS;
            }
        }
    }
    
    // Add village houses
    for (int i = 0; i < 5; i++) {
        int hx = 40 + (i % 3) * 15;
        int hy = 30 + (i / 3) * 10;
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 6; x++) {
                if (hx+x < WORLD_WIDTH && hy+y < WORLD_HEIGHT) {
                    game->world[hy+y][hx+x] = TILE_HOUSE;
                }
            }
        }
    }
    
    // Create NPCs
    game->npc_count = 10;
    init_npc(&game->npcs[0], 0, "Tom", "Farmer");
    init_npc(&game->npcs[1], 1, "Sara", "Merchant");
    init_npc(&game->npcs[2], 2, "Ben", "Farmer");
    init_npc(&game->npcs[3], 3, "Luna", "Artist");
    init_npc(&game->npcs[4], 4, "Rex", "Guard");
    init_npc(&game->npcs[5], 5, "Elena", "Farmer");
    init_npc(&game->npcs[6], 6, "Marcus", "Merchant");
    init_npc(&game->npcs[7], 7, "Rose", "Artist");
    init_npc(&game->npcs[8], 8, "Jack", "Farmer");
    init_npc(&game->npcs[9], 9, "Anna", "Villager");
    
    game->player_x = 400;
    game->player_y = 300;
    game->player_vx = 0;
    game->player_vy = 0;
    
    game->game_time = 0;
}

// Handle input
void handle_input(game_state* game, XEvent* event) {
    if (event->type == KeyPress) {
        KeySym key = XLookupKeysym(&event->xkey, 0);
        
        if (key == XK_w || key == XK_W || key == XK_Up) game->keys_held[0] = 1;
        if (key == XK_a || key == XK_A || key == XK_Left) game->keys_held[1] = 1;
        if (key == XK_s || key == XK_S || key == XK_Down) game->keys_held[2] = 1;
        if (key == XK_d || key == XK_D || key == XK_Right) game->keys_held[3] = 1;
        
        if (key == XK_Tab) {
            game->show_debug = !game->show_debug;
        }
        else if (key == XK_space) {
            // Gather resources in 3x3 area around player
            int px = (int)(game->player_x / 8);
            int py = (int)(game->player_y / 8);
            
            int gathered = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int tx = px + dx;
                    int ty = py + dy;
                    
                    if (tx >= 0 && tx < WORLD_WIDTH && ty >= 0 && ty < WORLD_HEIGHT) {
                        u8 tile = game->world[ty][tx];
                        
                        if (tile == TILE_FLOWER) {
                            game->flowers_collected++;
                            game->world[ty][tx] = TILE_GRASS;
                            gathered = 1;
                        } else if (tile == TILE_STONE) {
                            game->stones_collected++;
                            game->world[ty][tx] = TILE_GRASS;
                            gathered = 1;
                        }
                    }
                }
            }
            
            if (gathered) {
                snprintf(game->dialog_text, 255, "Gathered resources!");
                game->dialog_active = 1;
                game->dialog_timer = 1.0f;
            }
        }
        else if (key == XK_Return) {
            npc* nearest = NULL;
            f32 min_dist = 100.0f;
            
            for (u32 i = 0; i < game->npc_count; i++) {
                f32 dx = game->npcs[i].x - game->player_x;
                f32 dy = game->npcs[i].y - game->player_y;
                f32 dist_sq = dx*dx + dy*dy;  // Use squared distance
                
                if (dist_sq < min_dist * min_dist) {
                    min_dist = sqrtf(dist_sq);  // Only sqrt when needed
                    nearest = &game->npcs[i];
                }
            }
            
            if (nearest) {
                game->dialog_active = 1;
                game->dialog_timer = 4.0f;
                game->dialog_npc_id = nearest->id;
                generate_dialog(nearest, game);
            }
        }
        else if (key == XK_Escape) {
            if (game->dialog_active) {
                game->dialog_active = 0;
            }
        }
    }
    else if (event->type == KeyRelease) {
        KeySym key = XLookupKeysym(&event->xkey, 0);
        
        if (key == XK_w || key == XK_W || key == XK_Up) game->keys_held[0] = 0;
        if (key == XK_a || key == XK_A || key == XK_Left) game->keys_held[1] = 0;
        if (key == XK_s || key == XK_S || key == XK_Down) game->keys_held[2] = 0;
        if (key == XK_d || key == XK_D || key == XK_Right) game->keys_held[3] = 0;
    }
}

// Update game
void update_game(game_state* game, f32 dt) {
    game->game_time += dt;
    
    // Update player movement
    f32 speed = 300.0f;
    
    if (game->keys_held[0]) game->player_vy -= speed * dt;
    if (game->keys_held[1]) game->player_vx -= speed * dt;
    if (game->keys_held[2]) game->player_vy += speed * dt;
    if (game->keys_held[3]) game->player_vx += speed * dt;
    
    game->player_vx *= 0.9f;
    game->player_vy *= 0.9f;
    
    game->player_x += game->player_vx * dt;
    game->player_y += game->player_vy * dt;
    
    // Collision with world bounds
    if (game->player_x < 16) { game->player_x = 16; game->player_vx = 0; }
    if (game->player_x > 1008) { game->player_x = 1008; game->player_vx = 0; }
    if (game->player_y < 16) { game->player_y = 16; game->player_vy = 0; }
    if (game->player_y > 752) { game->player_y = 752; game->player_vy = 0; }
    
    // Update NPCs
    for (u32 i = 0; i < game->npc_count; i++) {
        update_npc(&game->npcs[i], dt, game);
    }
    
    // Update dialog
    if (game->dialog_active) {
        game->dialog_timer -= dt;
        if (game->dialog_timer <= 0) {
            game->dialog_active = 0;
        }
    }
}

// Render game
void render_game(game_state* game) {
    // Clear
    XSetForeground(game->display, game->gc, 0x000000);
    XFillRectangle(game->display, game->window, game->gc, 0, 0, 1024, 768);
    
    int cam_x = (int)(game->player_x - 512);
    int cam_y = (int)(game->player_y - 384);
    
    // Draw world
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            int screen_x = x * 8 - cam_x;
            int screen_y = y * 8 - cam_y;
            
            if (screen_x < -8 || screen_x > 1024 || screen_y < -8 || screen_y > 768) continue;
            
            u32 color = nes_palette[0x1A];
            u8 tile = game->world[y][x];
            
            if (tile == TILE_TREE) color = nes_palette[0x18];
            else if (tile == TILE_HOUSE) color = nes_palette[0x16];
            else if (tile == TILE_FLOWER) color = nes_palette[0x24];
            else if (tile == TILE_STONE) color = nes_palette[0x00];
            else if (tile == TILE_WATER) color = nes_palette[0x2C];
            
            XSetForeground(game->display, game->gc, color);
            XFillRectangle(game->display, game->window, game->gc, screen_x, screen_y, 8, 8);
        }
    }
    
    // Draw NPCs
    for (u32 i = 0; i < game->npc_count; i++) {
        npc* n = &game->npcs[i];
        int screen_x = (int)(n->x - cam_x);
        int screen_y = (int)(n->y - cam_y);
        
        if (screen_x < -16 || screen_x > 1024 || screen_y < -16 || screen_y > 768) continue;
        
        XSetForeground(game->display, game->gc, nes_palette[n->color]);
        XFillRectangle(game->display, game->window, game->gc, screen_x - 8, screen_y - 8, 16, 16);
        
        // Show name if close
        f32 dx = n->x - game->player_x;
        f32 dy = n->y - game->player_y;
        f32 dist = sqrtf(dx*dx + dy*dy);
        
        if (dist < 100.0f) {
            // Draw ! indicator
            XSetForeground(game->display, game->gc, nes_palette[0x25]);
            XFillRectangle(game->display, game->window, game->gc, screen_x - 2, screen_y - 25, 4, 10);
        }
    }
    
    // Draw player
    XSetForeground(game->display, game->gc, nes_palette[0x11]);
    XFillRectangle(game->display, game->window, game->gc, 504, 376, 16, 16);
    
    // Draw inventory
    char inv_text[128];
    snprintf(inv_text, 127, "Flowers:%d Stones:%d", 
            game->flowers_collected, game->stones_collected);
    draw_text(game, 10, 10, inv_text, nes_palette[0x30]);
    
    // Draw dialog
    if (game->dialog_active) {
        XSetForeground(game->display, game->gc, nes_palette[0x0F]);
        XFillRectangle(game->display, game->window, game->gc, 50, 550, 924, 150);
        
        XSetForeground(game->display, game->gc, nes_palette[0x30]);
        XDrawRectangle(game->display, game->window, game->gc, 50, 550, 924, 150);
        
        // Word wrap dialog text
        int max_chars = 36;  // Characters per line with 3x font
        int pos = 0;
        int line = 0;
        int len = strlen(game->dialog_text);
        
        while (pos < len && line < 4) {
            char line_text[128];
            int line_end = pos + max_chars;
            if (line_end > len) line_end = len;
            
            // Find word boundary
            if (line_end < len) {
                for (int i = line_end; i > pos; i--) {
                    if (game->dialog_text[i] == ' ') {
                        line_end = i;
                        break;
                    }
                }
            }
            
            int line_len = line_end - pos;
            int safe_len = line_len < 127 ? line_len : 127;
            strncpy(line_text, game->dialog_text + pos, safe_len);
            line_text[safe_len] = '\0';
            
            draw_text(game, 70, 570 + line * 30, line_text, nes_palette[0x30]);
            
            pos = line_end + 1;
            line++;
        }
    }
    
    // Draw debug
    if (game->show_debug) {
        XSetForeground(game->display, game->gc, 0x000000);
        XFillRectangle(game->display, game->window, game->gc, 5, 40, 500, 25 + game->npc_count * 30);
        
        draw_text(game, 10, 45, "DEBUG", nes_palette[0x25]);
        
        for (u32 i = 0; i < game->npc_count && i < 8; i++) {  // Show first 8 NPCs
            npc* n = &game->npcs[i];
            char debug[128];
            snprintf(debug, 127, "%s T:%.0f M:%d", 
                    n->name, n->trust, n->memory_count);
            draw_text(game, 10, 75 + i * 30, debug, nes_palette[0x30]);
        }
    }
    
    // Draw controls
    draw_text(game, 10, 740, "MOVE:WASD GATHER:SPACE TALK:ENTER", nes_palette[0x30]);
    
    XFlush(game->display);
}

int main() {
    printf("\n=== NEURAL VILLAGE COMPLETE ===\n");
    printf("• Readable text (3x scale font)\n");
    printf("• Smooth controls\n");
    printf("• Resource gathering (SPACE key)\n");
    printf("• NPCs learn and remember\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    game_state* game = malloc(sizeof(game_state));
    if (!game) {
        printf("Failed to allocate memory for game state\n");
        return 1;
    }
    init_game(game);
    
    // Setup X11
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 1;
    }
    
    game->screen = DefaultScreen(game->display);
    game->window = XCreateSimpleWindow(game->display,
                                      RootWindow(game->display, game->screen),
                                      100, 100, 1024, 768, 1,
                                      BlackPixel(game->display, game->screen),
                                      WhitePixel(game->display, game->screen));
    
    XStoreName(game->display, game->window, "Neural Village Complete");
    XSelectInput(game->display, game->window, ExposureMask | KeyPressMask | KeyReleaseMask);
    XMapWindow(game->display, game->window);
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    if (!game->gc) {
        printf("Failed to create graphics context\n");
        XDestroyWindow(game->display, game->window);
        XCloseDisplay(game->display);
        free(game);
        return 1;
    }
    
    // Main loop
    struct timeval last_time, current_time;
    gettimeofday(&last_time, NULL);
    
    XEvent event;
    int running = 1;
    
    while (running) {
        while (XPending(game->display)) {
            XNextEvent(game->display, &event);
            
            if (event.type == Expose) {
                render_game(game);
            } else if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Escape && !game->dialog_active) {
                    running = 0;
                } else {
                    handle_input(game, &event);
                }
            } else if (event.type == KeyRelease) {
                handle_input(game, &event);
            }
        }
        
        gettimeofday(&current_time, NULL);
        f32 dt = (current_time.tv_sec - last_time.tv_sec) +
                (current_time.tv_usec - last_time.tv_usec) / 1000000.0f;
        last_time = current_time;
        
        if (dt > 0.1f) dt = 0.1f;
        
        update_game(game, dt);
        render_game(game);
        
        usleep(16000);
    }
    
    printf("\nThanks for playing!\n");
    printf("Memories saved to: learning.log\n");
    
    if (game->log_file) fclose(game->log_file);
    
    XFreeGC(game->display, game->gc);
    XDestroyWindow(game->display, game->window);
    XCloseDisplay(game->display);
    
    free(game);
    
    return 0;
}