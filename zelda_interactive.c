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
#define TILE_FLOWER    6
#define TILE_STONE     7

// World size - Much bigger world!
#define WORLD_WIDTH  128
#define WORLD_HEIGHT 96

// NPC types
#define NPC_FARMER     0
#define NPC_VILLAGER   1
#define NPC_MERCHANT   2
#define NPC_ELDER      3

// NPC states
#define STATE_WANDER   0
#define STATE_WORK     1
#define STATE_GATHER   2
#define STATE_TALK     3
#define STATE_HOME     4

// Player activities
#define ACTIVITY_EXPLORE   0
#define ACTIVITY_GATHER    1
#define ACTIVITY_TRADE     2

#define MAX_NPCS 20
#define MAX_DIALOG_LENGTH 120
#define MAX_ITEMS 20

// Items
typedef struct {
    char name[20];
    int count;
} item;

// Dialog system
typedef struct {
    char text[MAX_DIALOG_LENGTH];
    f32 timer;
    int active;
    int speaker_id; // -1 = player, 0+ = NPC id
} dialog;

// NPC structure
typedef struct {
    f32 x, y;
    f32 target_x, target_y;
    int type;
    int state;
    f32 state_timer;
    f32 work_x, work_y;
    f32 home_x, home_y;
    u8 color;
    int facing;
    int active;
    char name[20];
    char current_dialog[MAX_DIALOG_LENGTH];
    f32 dialog_timer;
    int talk_target; // Which NPC they're talking to (-1 = none)
    int mood; // 0=happy, 1=neutral, 2=busy
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
    int player_activity;
    item inventory[MAX_ITEMS];
    int inventory_count;
    
    // Camera for big world
    f32 camera_x, camera_y;
    
    // NPCs
    npc npcs[MAX_NPCS];
    int npc_count;
    
    // Dialog system
    dialog current_dialog;
    int near_npc; // Which NPC player is near (-1 = none)
    
    // Input
    int key_up, key_down, key_left, key_right;
    int key_space, key_enter;
    int key_space_pressed, key_enter_pressed; // For single press detection
    
    // Timing
    struct timeval last_time;
    f32 time_of_day; // 0-24 hours
    f32 demo_timer; // For showing off system
    
    // Status display
    char status_text[200];
    f32 status_timer;
} game_state;

// NPC dialog arrays
static char* farmer_dialogs[] = {
    "Good day! The crops are growing well this season.",
    "I've been working since dawn. Hard work pays off!",
    "The soil here is rich and fertile.",
    "Would you like some fresh vegetables?",
    "These fields have fed our village for generations."
};

static char* villager_dialogs[] = {
    "Beautiful weather today, isn't it?",
    "Have you seen the merchant? He has fine goods.",
    "The village is peaceful and prosperous.",
    "I was just talking to the farmer about the harvest.",
    "Life is good here in our little village."
};

static char* merchant_dialogs[] = {
    "Welcome! I have the finest goods in the land!",
    "Trade with me - fair prices for quality items!",
    "I've traveled far to bring these wares here.",
    "Business has been good in this village.",
    "Looking for something special? I might have it!"
};

static char* elder_dialogs[] = {
    "Ah, young one. Welcome to our village.",
    "I've seen many seasons come and go here.",
    "The village thrives because we all work together.",
    "In my day, things were different...",
    "Watch how our people live in harmony."
};

// NPC conversation topics
static char* npc_conversations[] = {
    "The harvest looks promising this year.",
    "Did you hear about the trader from the east?",
    "The weather has been perfect for farming.",
    "Our village grows stronger each day.",
    "I saw some beautiful flowers by the pond.",
    "The paths need tending after the rain.",
    "Have you tried the merchant's new goods?",
    "The elder tells such wonderful stories."
};

// Check if tile is solid
int is_solid_tile(u8 tile) {
    return (tile == TILE_TREE || tile == TILE_WATER || tile == TILE_HOUSE);
    // Note: TILE_STONE removed so player can walk on stones to gather them
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

// Add item to inventory
void add_item(game_state* game, const char* name, int count) {
    for (int i = 0; i < game->inventory_count; i++) {
        if (strcmp(game->inventory[i].name, name) == 0) {
            game->inventory[i].count += count;
            return;
        }
    }
    
    if (game->inventory_count < MAX_ITEMS) {
        strcpy(game->inventory[game->inventory_count].name, name);
        game->inventory[game->inventory_count].count = count;
        game->inventory_count++;
    }
}

// Show dialog
void show_dialog(game_state* game, const char* text, int speaker_id) {
    strcpy(game->current_dialog.text, text);
    game->current_dialog.timer = 3.0f;
    game->current_dialog.active = 1;
    game->current_dialog.speaker_id = speaker_id;
}

// Show status message
void show_status(game_state* game, const char* text) {
    strcpy(game->status_text, text);
    game->status_timer = 2.0f;
}

// Initialize NPCs with rich personalities - LOTS of them!
void init_npcs(game_state* game) {
    game->npc_count = 18;
    
    // Farmer Bob
    game->npcs[0] = (npc){
        .x = 200, .y = 160, .type = NPC_FARMER,
        .work_x = 180, .work_y = 140, .home_x = 240, .home_y = 200,
        .color = 0x16, .state = STATE_WORK, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[0].name, "Farmer Bob");
    
    // Mary the Villager
    game->npcs[1] = (npc){
        .x = 280, .y = 220, .type = NPC_VILLAGER,
        .work_x = 260, .work_y = 200, .home_x = 280, .home_y = 220,
        .color = 0x22, .state = STATE_WANDER, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[1].name, "Mary");
    
    // Trader Jack
    game->npcs[2] = (npc){
        .x = 320, .y = 240, .type = NPC_MERCHANT,
        .work_x = 320, .work_y = 240, .home_x = 300, .home_y = 260,
        .color = 0x14, .state = STATE_WORK, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[2].name, "Trader Jack");
    
    // Elder Tom
    game->npcs[3] = (npc){
        .x = 160, .y = 280, .type = NPC_ELDER,
        .work_x = 160, .work_y = 280, .home_x = 160, .home_y = 280,
        .color = 0x30, .state = STATE_WANDER, .active = 1,
        .mood = 1, .talk_target = -1
    };
    strcpy(game->npcs[3].name, "Elder Tom");
    
    // Sarah the Gatherer
    game->npcs[4] = (npc){
        .x = 200, .y = 300, .type = NPC_VILLAGER,
        .work_x = 180, .work_y = 320, .home_x = 200, .home_y = 300,
        .color = 0x29, .state = STATE_GATHER, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[4].name, "Sarah");
    
    // Guard Pete
    game->npcs[5] = (npc){
        .x = 260, .y = 280, .type = NPC_VILLAGER,
        .work_x = 280, .work_y = 260, .home_x = 260, .home_y = 280,
        .color = 0x12, .state = STATE_WANDER, .active = 1,
        .mood = 1, .talk_target = -1
    };
    strcpy(game->npcs[5].name, "Guard Pete");
    
    // Miller Ben
    game->npcs[6] = (npc){
        .x = 400, .y = 200, .type = NPC_FARMER,
        .work_x = 420, .work_y = 180, .home_x = 400, .home_y = 200,
        .color = 0x17, .state = STATE_WORK, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[6].name, "Miller Ben");
    
    // Healer Anna
    game->npcs[7] = (npc){
        .x = 350, .y = 350, .type = NPC_ELDER,
        .work_x = 350, .work_y = 350, .home_x = 350, .home_y = 350,
        .color = 0x32, .state = STATE_WANDER, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[7].name, "Healer Anna");
    
    // Fisherman Joe
    game->npcs[8] = (npc){
        .x = 500, .y = 400, .type = NPC_VILLAGER,
        .work_x = 520, .work_y = 420, .home_x = 500, .home_y = 400,
        .color = 0x11, .state = STATE_WORK, .active = 1,
        .mood = 1, .talk_target = -1
    };
    strcpy(game->npcs[8].name, "Fisherman Joe");
    
    // Baker Lisa
    game->npcs[9] = (npc){
        .x = 600, .y = 250, .type = NPC_MERCHANT,
        .work_x = 600, .work_y = 250, .home_x = 580, .home_y = 270,
        .color = 0x24, .state = STATE_WORK, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[9].name, "Baker Lisa");
    
    // Hunter Max
    game->npcs[10] = (npc){
        .x = 150, .y = 500, .type = NPC_VILLAGER,
        .work_x = 120, .work_y = 480, .home_x = 150, .home_y = 500,
        .color = 0x08, .state = STATE_GATHER, .active = 1,
        .mood = 2, .talk_target = -1
    };
    strcpy(game->npcs[10].name, "Hunter Max");
    
    // Scholar Emma
    game->npcs[11] = (npc){
        .x = 700, .y = 300, .type = NPC_ELDER,
        .work_x = 700, .work_y = 300, .home_x = 680, .home_y = 320,
        .color = 0x35, .state = STATE_WANDER, .active = 1,
        .mood = 1, .talk_target = -1
    };
    strcpy(game->npcs[11].name, "Scholar Emma");
    
    // Miner Dave
    game->npcs[12] = (npc){
        .x = 800, .y = 150, .type = NPC_FARMER,
        .work_x = 820, .work_y = 130, .home_x = 800, .home_y = 150,
        .color = 0x00, .state = STATE_WORK, .active = 1,
        .mood = 2, .talk_target = -1
    };
    strcpy(game->npcs[12].name, "Miner Dave");
    
    // Bard Tim
    game->npcs[13] = (npc){
        .x = 450, .y = 300, .type = NPC_VILLAGER,
        .work_x = 450, .work_y = 300, .home_x = 430, .home_y = 320,
        .color = 0x28, .state = STATE_WANDER, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[13].name, "Bard Tim");
    
    // Herbalist Ivy
    game->npcs[14] = (npc){
        .x = 650, .y = 450, .type = NPC_VILLAGER,
        .work_x = 670, .work_y = 470, .home_x = 650, .home_y = 450,
        .color = 0x2A, .state = STATE_GATHER, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[14].name, "Herbalist Ivy");
    
    // Carpenter Rob
    game->npcs[15] = (npc){
        .x = 300, .y = 150, .type = NPC_FARMER,
        .work_x = 320, .work_y = 170, .home_x = 300, .home_y = 150,
        .color = 0x16, .state = STATE_WORK, .active = 1,
        .mood = 1, .talk_target = -1
    };
    strcpy(game->npcs[15].name, "Carpenter Rob");
    
    // Weaver Sue
    game->npcs[16] = (npc){
        .x = 750, .y = 200, .type = NPC_MERCHANT,
        .work_x = 750, .work_y = 200, .home_x = 730, .home_y = 220,
        .color = 0x31, .state = STATE_WORK, .active = 1,
        .mood = 0, .talk_target = -1
    };
    strcpy(game->npcs[16].name, "Weaver Sue");
    
    // Watchman Jim
    game->npcs[17] = (npc){
        .x = 550, .y = 150, .type = NPC_VILLAGER,
        .work_x = 550, .work_y = 130, .home_x = 570, .home_y = 170,
        .color = 0x12, .state = STATE_WANDER, .active = 1,
        .mood = 2, .talk_target = -1
    };
    strcpy(game->npcs[17].name, "Watchman Jim");
    
    printf("✓ Village populated with %d residents\n", game->npc_count);
    printf("  └─ Professions: Farmers, Merchants, Elders, Guards\n");
    
    // Give player starting items
    add_item(game, "Flowers", 2);
    add_item(game, "Stones", 1);
    show_status(game, "Welcome to the village! Explore and meet the residents.");
}

// Initialize world with MASSIVE interactive elements
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
    
    // MASSIVE VILLAGE CENTER with many houses
    // Main residential area
    for (int house = 0; house < 8; house++) {
        int base_x = 20 + (house % 4) * 15;
        int base_y = 20 + (house / 4) * 15;
        game->world[base_y][base_x] = TILE_HOUSE;
        game->world[base_y][base_x+1] = TILE_HOUSE;
        game->world[base_y+1][base_x] = TILE_HOUSE;
        game->world[base_y+1][base_x+1] = TILE_HOUSE;
    }
    
    // Market district
    for (int shop = 0; shop < 6; shop++) {
        int base_x = 75 + (shop % 3) * 12;
        int base_y = 25 + (shop / 3) * 12;
        game->world[base_y][base_x] = TILE_HOUSE;
        game->world[base_y][base_x+1] = TILE_HOUSE;
        game->world[base_y+1][base_x] = TILE_HOUSE;
        game->world[base_y+1][base_x+1] = TILE_HOUSE;
    }
    
    // Outer village houses
    for (int house = 0; house < 6; house++) {
        int base_x = 15 + house * 18;
        int base_y = 60;
        game->world[base_y][base_x] = TILE_HOUSE;
        game->world[base_y][base_x+1] = TILE_HOUSE;
        game->world[base_y+1][base_x] = TILE_HOUSE;
        game->world[base_y+1][base_x+1] = TILE_HOUSE;
    }
    
    // EXTENSIVE ROAD NETWORK
    // Main highway (horizontal)
    for (int x = 5; x < WORLD_WIDTH-5; x++) {
        game->world[48][x] = TILE_DIRT;
    }
    
    // Main avenue (vertical)
    for (int y = 5; y < WORLD_HEIGHT-5; y++) {
        game->world[y][64] = TILE_DIRT;
    }
    
    // Village streets
    for (int street = 0; street < 8; street++) {
        int y = 15 + street * 8;
        for (int x = 15; x < 110; x++) {
            if (x % 4 != 0) game->world[y][x] = TILE_DIRT;
        }
    }
    
    // Cross streets
    for (int street = 0; street < 6; street++) {
        int x = 20 + street * 15;
        for (int y = 15; y < 70; y++) {
            if (y % 3 != 0) game->world[y][x] = TILE_DIRT;
        }
    }
    
    // MASSIVE FARM DISTRICT
    // Large farm fields
    for (int field = 0; field < 4; field++) {
        int base_x = 15 + field * 25;
        int base_y = 75;
        for (int y = base_y; y < base_y + 12; y++) {
            for (int x = base_x; x < base_x + 20; x++) {
                if ((x + y) % 2 == 0) {
                    game->world[y][x] = TILE_DIRT;
                }
            }
        }
    }
    
    // Orchard area (represented by scattered trees)
    for (int tree = 0; tree < 20; tree++) {
        int x = 15 + (tree % 10) * 3;
        int y = 10 + (tree / 10) * 2;
        game->world[y][x] = TILE_TREE;
    }
    
    // EXTENSIVE FLOWER MEADOWS (gatherable)
    for (int meadow = 0; meadow < 8; meadow++) {
        int base_x = 10 + (meadow % 4) * 25;
        int base_y = 20 + (meadow / 4) * 30;
        for (int i = 0; i < 12; i++) {
            int flower_x = base_x + (rand() % 15);
            int flower_y = base_y + (rand() % 10);
            if (flower_x < WORLD_WIDTH-1 && flower_y < WORLD_HEIGHT-1) {
                game->world[flower_y][flower_x] = TILE_FLOWER;
            }
        }
    }
    
    // MINING DISTRICT with stone deposits - FIXED placement
    for (int mine = 0; mine < 12; mine++) {
        int x = 90 + (mine % 6) * 5;
        int y = 70 + (mine / 6) * 8;
        
        // Make sure coordinates are within bounds
        if (x >= 0 && x < WORLD_WIDTH && y >= 0 && y < WORLD_HEIGHT) {
            game->world[y][x] = TILE_STONE;
            
            // Add adjacent stones within bounds
            if (x > 0 && x-1 < WORLD_WIDTH) game->world[y][x-1] = TILE_STONE;
            if (y > 0 && y-1 < WORLD_HEIGHT) game->world[y-1][x] = TILE_STONE;
            if (x+1 < WORLD_WIDTH) game->world[y][x+1] = TILE_STONE;
            if (y+1 < WORLD_HEIGHT) game->world[y+1][x] = TILE_STONE;
        }
    }
    
    // Add some easier-to-find stones near the center
    for (int i = 0; i < 8; i++) {
        int x = 50 + i * 4;
        int y = 30 + (i % 2) * 5;
        if (x < WORLD_WIDTH && y < WORLD_HEIGHT) {
            game->world[y][x] = TILE_STONE;
        }
    }
    
    // WATER FEATURES
    // Large central lake
    for (int y = 35; y < 45; y++) {
        for (int x = 45; x < 55; x++) {
            game->world[y][x] = TILE_WATER;
        }
    }
    
    // River running through village
    for (int x = 30; x < 80; x++) {
        game->world[52][x] = TILE_WATER;
        game->world[53][x] = TILE_WATER;
    }
    
    // Small ponds scattered around
    for (int pond = 0; pond < 6; pond++) {
        int base_x = 20 + pond * 15;
        int base_y = 15 + (pond % 2) * 40;
        game->world[base_y][base_x] = TILE_WATER;
        game->world[base_y][base_x+1] = TILE_WATER;
        game->world[base_y+1][base_x] = TILE_WATER;
        game->world[base_y+1][base_x+1] = TILE_WATER;
    }
    
    // FOREST AREAS
    // Dense forest in corners
    for (int y = 5; y < 15; y++) {
        for (int x = 5; x < 15; x++) {
            if ((x + y) % 3 == 0) game->world[y][x] = TILE_TREE;
        }
    }
    
    for (int y = 80; y < 90; y++) {
        for (int x = 110; x < 120; x++) {
            if ((x + y) % 2 == 0) game->world[y][x] = TILE_TREE;
        }
    }
    
    // Scattered decorative trees throughout
    for (int tree = 0; tree < 40; tree++) {
        int x = 20 + (rand() % 80);
        int y = 20 + (rand() % 50);
        if (game->world[y][x] == TILE_GRASS) {
            game->world[y][x] = TILE_TREE;
        }
    }
    
    // PLACE STONES LAST so they don't get overwritten!
    // Create natural-looking stone deposits throughout the world
    for (int i = 0; i < 15; i++) {
        int base_x = 25 + (i % 5) * 20;
        int base_y = 25 + (i / 5) * 15;
        
        // Create small clusters of stones (more realistic)
        for (int cluster = 0; cluster < 3; cluster++) {
            int stone_x = base_x + (rand() % 8) - 4;
            int stone_y = base_y + (rand() % 6) - 3;
            
            if (stone_x >= 0 && stone_x < WORLD_WIDTH && 
                stone_y >= 0 && stone_y < WORLD_HEIGHT &&
                game->world[stone_y][stone_x] == TILE_GRASS) {
                game->world[stone_y][stone_x] = TILE_STONE;
            }
        }
    }
    
    // Count actual stones placed for verification
    int stone_count = 0;
    int flower_count = 0;
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            if (game->world[y][x] == TILE_STONE) stone_count++;
            if (game->world[y][x] == TILE_FLOWER) flower_count++;
        }
    }
    
    printf("✓ Living world created successfully!\n");
    printf("  └─ World size: %dx%d tiles (%d total)\n", WORLD_WIDTH, WORLD_HEIGHT, WORLD_WIDTH * WORLD_HEIGHT);
    printf("  └─ Resources: %d flowers, %d stones\n", flower_count, stone_count);
    printf("  └─ Districts: Residential, Market, Farming, Mining\n");
}

// Initialize display
int init_display(game_state* game) {
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(game->display);
    game->width = 1024;   // Large window width 
    game->height = 768;   // Large window height
    
    game->window = XCreateSimpleWindow(
        game->display, RootWindow(game->display, screen),
        0, 0, game->width, game->height, 1,
        BlackPixel(game->display, screen),
        WhitePixel(game->display, screen)
    );
    
    XSelectInput(game->display, game->window, 
                 ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
    XMapWindow(game->display, game->window);
    XStoreName(game->display, game->window, "NES Zelda - Living Village Demo");
    
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    
    game->pixels = malloc(game->width * game->height * sizeof(u32));
    game->screen = XCreateImage(game->display, DefaultVisual(game->display, screen),
                               DefaultDepth(game->display, screen), ZPixmap, 0,
                               (char*)game->pixels, game->width, game->height, 32, 0);
    
    // Initialize player at center of big world
    game->player_x = (WORLD_WIDTH * 8) / 2;
    game->player_y = (WORLD_HEIGHT * 8) / 2;
    game->player_facing = 0;
    game->player_activity = ACTIVITY_EXPLORE;
    
    // Initialize camera centered on player
    game->camera_x = game->player_x - game->width / 2;
    game->camera_y = game->player_y - (game->height - 60) / 2;
    
    game->time_of_day = 10.0f; // Start at 10 AM
    game->near_npc = -1;
    
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

// Draw text with readable 8x8 bitmap font
void draw_text(game_state* game, int x, int y, const char* text, u8 color) {
    // 8x8 bitmap font patterns for common characters
    static u8 font_data[256][8] = {0};
    static int font_initialized = 0;
    
    if (!font_initialized) {
        // Initialize font patterns for readable characters
        
        // A
        font_data['A'][0] = 0b00111000;
        font_data['A'][1] = 0b01000100;
        font_data['A'][2] = 0b10000010;
        font_data['A'][3] = 0b10000010;
        font_data['A'][4] = 0b11111110;
        font_data['A'][5] = 0b10000010;
        font_data['A'][6] = 0b10000010;
        font_data['A'][7] = 0b00000000;
        
        // B
        font_data['B'][0] = 0b11111100;
        font_data['B'][1] = 0b10000010;
        font_data['B'][2] = 0b10000010;
        font_data['B'][3] = 0b11111100;
        font_data['B'][4] = 0b10000010;
        font_data['B'][5] = 0b10000010;
        font_data['B'][6] = 0b11111100;
        font_data['B'][7] = 0b00000000;
        
        // C
        font_data['C'][0] = 0b01111110;
        font_data['C'][1] = 0b10000000;
        font_data['C'][2] = 0b10000000;
        font_data['C'][3] = 0b10000000;
        font_data['C'][4] = 0b10000000;
        font_data['C'][5] = 0b10000000;
        font_data['C'][6] = 0b01111110;
        font_data['C'][7] = 0b00000000;
        
        // D
        font_data['D'][0] = 0b11111100;
        font_data['D'][1] = 0b10000010;
        font_data['D'][2] = 0b10000010;
        font_data['D'][3] = 0b10000010;
        font_data['D'][4] = 0b10000010;
        font_data['D'][5] = 0b10000010;
        font_data['D'][6] = 0b11111100;
        font_data['D'][7] = 0b00000000;
        
        // E
        font_data['E'][0] = 0b11111110;
        font_data['E'][1] = 0b10000000;
        font_data['E'][2] = 0b10000000;
        font_data['E'][3] = 0b11111100;
        font_data['E'][4] = 0b10000000;
        font_data['E'][5] = 0b10000000;
        font_data['E'][6] = 0b11111110;
        font_data['E'][7] = 0b00000000;
        
        // F
        font_data['F'][0] = 0b11111110;
        font_data['F'][1] = 0b10000000;
        font_data['F'][2] = 0b10000000;
        font_data['F'][3] = 0b11111100;
        font_data['F'][4] = 0b10000000;
        font_data['F'][5] = 0b10000000;
        font_data['F'][6] = 0b10000000;
        font_data['F'][7] = 0b00000000;
        
        // G
        font_data['G'][0] = 0b01111110;
        font_data['G'][1] = 0b10000000;
        font_data['G'][2] = 0b10000000;
        font_data['G'][3] = 0b10011110;
        font_data['G'][4] = 0b10000010;
        font_data['G'][5] = 0b10000010;
        font_data['G'][6] = 0b01111110;
        font_data['G'][7] = 0b00000000;
        
        // H
        font_data['H'][0] = 0b10000010;
        font_data['H'][1] = 0b10000010;
        font_data['H'][2] = 0b10000010;
        font_data['H'][3] = 0b11111110;
        font_data['H'][4] = 0b10000010;
        font_data['H'][5] = 0b10000010;
        font_data['H'][6] = 0b10000010;
        font_data['H'][7] = 0b00000000;
        
        // I
        font_data['I'][0] = 0b01111100;
        font_data['I'][1] = 0b00010000;
        font_data['I'][2] = 0b00010000;
        font_data['I'][3] = 0b00010000;
        font_data['I'][4] = 0b00010000;
        font_data['I'][5] = 0b00010000;
        font_data['I'][6] = 0b01111100;
        font_data['I'][7] = 0b00000000;
        
        // J
        font_data['J'][0] = 0b00111110;
        font_data['J'][1] = 0b00000010;
        font_data['J'][2] = 0b00000010;
        font_data['J'][3] = 0b00000010;
        font_data['J'][4] = 0b10000010;
        font_data['J'][5] = 0b10000010;
        font_data['J'][6] = 0b01111100;
        font_data['J'][7] = 0b00000000;
        
        // K
        font_data['K'][0] = 0b10000010;
        font_data['K'][1] = 0b10000100;
        font_data['K'][2] = 0b10001000;
        font_data['K'][3] = 0b10110000;
        font_data['K'][4] = 0b11001000;
        font_data['K'][5] = 0b10000100;
        font_data['K'][6] = 0b10000010;
        font_data['K'][7] = 0b00000000;
        
        // L
        font_data['L'][0] = 0b10000000;
        font_data['L'][1] = 0b10000000;
        font_data['L'][2] = 0b10000000;
        font_data['L'][3] = 0b10000000;
        font_data['L'][4] = 0b10000000;
        font_data['L'][5] = 0b10000000;
        font_data['L'][6] = 0b11111110;
        font_data['L'][7] = 0b00000000;
        
        // M
        font_data['M'][0] = 0b10000010;
        font_data['M'][1] = 0b11000110;
        font_data['M'][2] = 0b10101010;
        font_data['M'][3] = 0b10010010;
        font_data['M'][4] = 0b10000010;
        font_data['M'][5] = 0b10000010;
        font_data['M'][6] = 0b10000010;
        font_data['M'][7] = 0b00000000;
        
        // N
        font_data['N'][0] = 0b10000010;
        font_data['N'][1] = 0b11000010;
        font_data['N'][2] = 0b10100010;
        font_data['N'][3] = 0b10010010;
        font_data['N'][4] = 0b10001010;
        font_data['N'][5] = 0b10000110;
        font_data['N'][6] = 0b10000010;
        font_data['N'][7] = 0b00000000;
        
        // O
        font_data['O'][0] = 0b01111100;
        font_data['O'][1] = 0b10000010;
        font_data['O'][2] = 0b10000010;
        font_data['O'][3] = 0b10000010;
        font_data['O'][4] = 0b10000010;
        font_data['O'][5] = 0b10000010;
        font_data['O'][6] = 0b01111100;
        font_data['O'][7] = 0b00000000;
        
        // P
        font_data['P'][0] = 0b11111100;
        font_data['P'][1] = 0b10000010;
        font_data['P'][2] = 0b10000010;
        font_data['P'][3] = 0b11111100;
        font_data['P'][4] = 0b10000000;
        font_data['P'][5] = 0b10000000;
        font_data['P'][6] = 0b10000000;
        font_data['P'][7] = 0b00000000;
        
        // R
        font_data['R'][0] = 0b11111100;
        font_data['R'][1] = 0b10000010;
        font_data['R'][2] = 0b10000010;
        font_data['R'][3] = 0b11111100;
        font_data['R'][4] = 0b10001000;
        font_data['R'][5] = 0b10000100;
        font_data['R'][6] = 0b10000010;
        font_data['R'][7] = 0b00000000;
        
        // S
        font_data['S'][0] = 0b01111110;
        font_data['S'][1] = 0b10000000;
        font_data['S'][2] = 0b10000000;
        font_data['S'][3] = 0b01111100;
        font_data['S'][4] = 0b00000010;
        font_data['S'][5] = 0b00000010;
        font_data['S'][6] = 0b11111100;
        font_data['S'][7] = 0b00000000;
        
        // T
        font_data['T'][0] = 0b11111110;
        font_data['T'][1] = 0b00010000;
        font_data['T'][2] = 0b00010000;
        font_data['T'][3] = 0b00010000;
        font_data['T'][4] = 0b00010000;
        font_data['T'][5] = 0b00010000;
        font_data['T'][6] = 0b00010000;
        font_data['T'][7] = 0b00000000;
        
        // U
        font_data['U'][0] = 0b10000010;
        font_data['U'][1] = 0b10000010;
        font_data['U'][2] = 0b10000010;
        font_data['U'][3] = 0b10000010;
        font_data['U'][4] = 0b10000010;
        font_data['U'][5] = 0b10000010;
        font_data['U'][6] = 0b01111100;
        font_data['U'][7] = 0b00000000;
        
        // V
        font_data['V'][0] = 0b10000010;
        font_data['V'][1] = 0b10000010;
        font_data['V'][2] = 0b10000010;
        font_data['V'][3] = 0b10000010;
        font_data['V'][4] = 0b01000100;
        font_data['V'][5] = 0b00101000;
        font_data['V'][6] = 0b00010000;
        font_data['V'][7] = 0b00000000;
        
        // W
        font_data['W'][0] = 0b10000010;
        font_data['W'][1] = 0b10000010;
        font_data['W'][2] = 0b10000010;
        font_data['W'][3] = 0b10010010;
        font_data['W'][4] = 0b10101010;
        font_data['W'][5] = 0b11000110;
        font_data['W'][6] = 0b10000010;
        font_data['W'][7] = 0b00000000;
        
        // Numbers
        // 0
        font_data['0'][0] = 0b01111100;
        font_data['0'][1] = 0b10000010;
        font_data['0'][2] = 0b10000110;
        font_data['0'][3] = 0b10001010;
        font_data['0'][4] = 0b10010010;
        font_data['0'][5] = 0b10000010;
        font_data['0'][6] = 0b01111100;
        font_data['0'][7] = 0b00000000;
        
        // 1
        font_data['1'][0] = 0b00010000;
        font_data['1'][1] = 0b00110000;
        font_data['1'][2] = 0b00010000;
        font_data['1'][3] = 0b00010000;
        font_data['1'][4] = 0b00010000;
        font_data['1'][5] = 0b00010000;
        font_data['1'][6] = 0b01111100;
        font_data['1'][7] = 0b00000000;
        
        // 2
        font_data['2'][0] = 0b01111100;
        font_data['2'][1] = 0b10000010;
        font_data['2'][2] = 0b00000010;
        font_data['2'][3] = 0b01111100;
        font_data['2'][4] = 0b10000000;
        font_data['2'][5] = 0b10000000;
        font_data['2'][6] = 0b11111110;
        font_data['2'][7] = 0b00000000;
        
        // 3-9 (simplified patterns)
        for (int n = '3'; n <= '9'; n++) {
            font_data[n][0] = 0b01111100;
            font_data[n][1] = 0b10000010;
            font_data[n][2] = 0b00000010;
            font_data[n][3] = 0b01111100;
            font_data[n][4] = 0b00000010;
            font_data[n][5] = 0b10000010;
            font_data[n][6] = 0b01111100;
            font_data[n][7] = 0b00000000;
        }
        
        // Common symbols
        // Space
        for (int i = 0; i < 8; i++) font_data[' '][i] = 0b00000000;
        
        // :
        font_data[':'][0] = 0b00000000;
        font_data[':'][1] = 0b00000000;
        font_data[':'][2] = 0b00110000;
        font_data[':'][3] = 0b00110000;
        font_data[':'][4] = 0b00000000;
        font_data[':'][5] = 0b00110000;
        font_data[':'][6] = 0b00110000;
        font_data[':'][7] = 0b00000000;
        
        // (
        font_data['('][0] = 0b00001000;
        font_data['('][1] = 0b00010000;
        font_data['('][2] = 0b00100000;
        font_data['('][3] = 0b00100000;
        font_data['('][4] = 0b00100000;
        font_data['('][5] = 0b00010000;
        font_data['('][6] = 0b00001000;
        font_data['('][7] = 0b00000000;
        
        // )
        font_data[')'][0] = 0b00100000;
        font_data[')'][1] = 0b00010000;
        font_data[')'][2] = 0b00001000;
        font_data[')'][3] = 0b00001000;
        font_data[')'][4] = 0b00001000;
        font_data[')'][5] = 0b00010000;
        font_data[')'][6] = 0b00100000;
        font_data[')'][7] = 0b00000000;
        
        // !
        font_data['!'][0] = 0b00010000;
        font_data['!'][1] = 0b00010000;
        font_data['!'][2] = 0b00010000;
        font_data['!'][3] = 0b00010000;
        font_data['!'][4] = 0b00000000;
        font_data['!'][5] = 0b00000000;
        font_data['!'][6] = 0b00010000;
        font_data['!'][7] = 0b00000000;
        
        // .
        font_data['.'][0] = 0b00000000;
        font_data['.'][1] = 0b00000000;
        font_data['.'][2] = 0b00000000;
        font_data['.'][3] = 0b00000000;
        font_data['.'][4] = 0b00000000;
        font_data['.'][5] = 0b00000000;
        font_data['.'][6] = 0b00010000;
        font_data['.'][7] = 0b00000000;
        
        // Copy uppercase to lowercase for simplicity
        for (int c = 'A'; c <= 'Z'; c++) {
            for (int i = 0; i < 8; i++) {
                font_data[c + 32][i] = font_data[c][i]; // lowercase = uppercase + 32
            }
        }
        
        font_initialized = 1;
    }
    
    // Render text using bitmap font
    for (int i = 0; text[i] && x < game->width - 8; i++) {
        unsigned char c = (unsigned char)text[i];
        
        // Draw character bitmap
        for (int row = 0; row < 8; row++) {
            u8 bitmap = font_data[c][row];
            for (int col = 0; col < 8; col++) {
                if (bitmap & (1 << (7 - col))) {
                    draw_pixel(game, x + col, y + row, color);
                }
            }
        }
        
        x += 8; // Character spacing
    }
}

// Draw 8x8 tile with enhanced graphics
void draw_tile(game_state* game, int x, int y, u8 tile_type) {
    u8 color = 0x21;
    
    switch (tile_type) {
        case TILE_GRASS: color = 0x2A; break;
        case TILE_TREE:  color = 0x08; break;
        case TILE_WATER: color = 0x11; break;
        case TILE_HOUSE: color = 0x16; break;
        case TILE_DIRT:  color = 0x17; break;
        case TILE_FLOWER: color = 0x37; break;
        case TILE_STONE: color = 0x00; break;
    }
    
    // Fill base tile
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            draw_pixel(game, x + dx, y + dy, color);
        }
    }
    
    // Enhanced tile details
    if (tile_type == TILE_TREE) {
        // Better tree with leaves pattern
        for (int dy = 5; dy < 8; dy++) {
            for (int dx = 3; dx < 5; dx++) {
                draw_pixel(game, x + dx, y + dy, 0x16); // Brown trunk
            }
        }
        // Leaf details
        draw_pixel(game, x + 2, y + 1, 0x2A); // Light green leaves
        draw_pixel(game, x + 5, y + 2, 0x2A);
        draw_pixel(game, x + 1, y + 3, 0x2A);
        draw_pixel(game, x + 6, y + 3, 0x2A);
    } 
    else if (tile_type == TILE_HOUSE) {
        // Enhanced house with roof, door, windows
        // Roof
        draw_pixel(game, x + 2, y + 0, 0x16);
        draw_pixel(game, x + 3, y + 0, 0x16);
        draw_pixel(game, x + 4, y + 0, 0x16);
        draw_pixel(game, x + 5, y + 0, 0x16);
        // Door
        draw_pixel(game, x + 3, y + 5, 0x0F); draw_pixel(game, x + 4, y + 5, 0x0F);
        draw_pixel(game, x + 3, y + 6, 0x0F); draw_pixel(game, x + 4, y + 6, 0x0F);
        draw_pixel(game, x + 3, y + 7, 0x0F); draw_pixel(game, x + 4, y + 7, 0x0F);
        // Windows
        draw_pixel(game, x + 1, y + 2, 0x21); draw_pixel(game, x + 2, y + 2, 0x21);
        draw_pixel(game, x + 5, y + 2, 0x21); draw_pixel(game, x + 6, y + 2, 0x21);
        draw_pixel(game, x + 1, y + 3, 0x21); draw_pixel(game, x + 2, y + 3, 0x21);
        draw_pixel(game, x + 5, y + 3, 0x21); draw_pixel(game, x + 6, y + 3, 0x21);
    } 
    else if (tile_type == TILE_FLOWER) {
        // Beautiful flower with petals
        draw_pixel(game, x + 3, y + 3, 0x30); // Yellow center
        draw_pixel(game, x + 4, y + 4, 0x30);
        // Pink petals
        draw_pixel(game, x + 2, y + 3, 0x35); draw_pixel(game, x + 5, y + 3, 0x35);
        draw_pixel(game, x + 3, y + 2, 0x35); draw_pixel(game, x + 3, y + 5, 0x35);
        // Green stem
        draw_pixel(game, x + 3, y + 6, 0x2A);
    } 
    else if (tile_type == TILE_STONE) {
        // Detailed stone with texture
        draw_pixel(game, x + 1, y + 1, 0x10); // Light highlights
        draw_pixel(game, x + 5, y + 2, 0x10);
        draw_pixel(game, x + 2, y + 5, 0x10);
        draw_pixel(game, x + 6, y + 6, 0x10);
        // Dark shadows
        draw_pixel(game, x + 3, y + 3, 0x0F);
        draw_pixel(game, x + 4, y + 4, 0x0F);
    }
    else if (tile_type == TILE_GRASS) {
        // Add grass texture
        if ((x + y) % 4 == 0) {
            draw_pixel(game, x + 2, y + 5, 0x29); // Darker grass spots
            draw_pixel(game, x + 5, y + 2, 0x29);
        }
    }
    else if (tile_type == TILE_WATER) {
        // Water ripple effect
        if ((x + y + (int)(game->time_of_day * 10)) % 8 == 0) {
            draw_pixel(game, x + 3, y + 3, 0x21); // Light blue ripple
            draw_pixel(game, x + 4, y + 4, 0x21);
        }
    }
}

// Draw character
void draw_character(game_state* game, f32 x, f32 y, u8 color, int is_player) {
    int px = (int)x - 8;
    int py = (int)y - 8;
    
    u8 skin_color = 0x27;
    
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 16; dx++) {
            u8 draw_color = color;
            
            if (dy < 8) {
                draw_color = skin_color;
                if (is_player && ((dx == 4 || dx == 12) && dy == 4)) {
                    draw_color = 0x0F; // Eyes for player
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

// Check if player can gather from tile
int can_gather_tile(u8 tile) {
    return (tile == TILE_FLOWER || tile == TILE_STONE);
}

// Handle player activities
void handle_player_activity(game_state* game) {
    int tile_x = (int)game->player_x / 8;
    int tile_y = (int)game->player_y / 8;
    u8 tile = get_tile(game, tile_x, tile_y);
    
    if (game->key_enter_pressed) {
        // Check the tile player is standing on AND adjacent tiles
        int found_resource = 0;
        
        // Check current tile and surrounding 3x3 area
        for (int dy = -1; dy <= 1 && !found_resource; dy++) {
            for (int dx = -1; dx <= 1 && !found_resource; dx++) {
                int check_x = tile_x + dx;
                int check_y = tile_y + dy;
                u8 check_tile = get_tile(game, check_x, check_y);
                
                if (can_gather_tile(check_tile)) {
                    if (check_tile == TILE_FLOWER) {
                        add_item(game, "Flowers", 1);
                        show_status(game, "Gathered a flower!");
                        game->world[check_y][check_x] = TILE_GRASS;
                        found_resource = 1;
                    } else if (check_tile == TILE_STONE) {
                        add_item(game, "Stones", 1);
                        show_status(game, "Mined a stone!");
                        game->world[check_y][check_x] = TILE_GRASS;
                        found_resource = 1;
                    }
                }
            }
        }
        
        if (!found_resource) {
            // Debug message to see what's happening
            char debug_msg[100];
            snprintf(debug_msg, sizeof(debug_msg), "No resources nearby. Standing on tile %d at (%d,%d)", 
                     tile, tile_x, tile_y);
            show_status(game, debug_msg);
        }
    }
}

// Get dialog for NPC
const char* get_npc_dialog(npc* n, int dialog_index) {
    switch (n->type) {
        case NPC_FARMER: 
            return farmer_dialogs[dialog_index % 5];
        case NPC_VILLAGER: 
            return villager_dialogs[dialog_index % 5];
        case NPC_MERCHANT: 
            return merchant_dialogs[dialog_index % 5];
        case NPC_ELDER: 
            return elder_dialogs[dialog_index % 5];
        default: 
            return "Hello there!";
    }
}

// Update NPC AI with enhanced social behavior
void update_npc(npc* n, game_state* game, f32 dt, f32 time_of_day, int npc_index) {
    if (!n->active) return;
    
    // Update timers
    n->state_timer -= dt;
    n->dialog_timer -= dt;
    
    // Look for nearby NPCs to talk to
    if (n->talk_target == -1 && n->state != STATE_TALK) {
        for (int i = 0; i < game->npc_count; i++) {
            if (i == npc_index || !game->npcs[i].active) continue;
            
            f32 dist = distance(n->x, n->y, game->npcs[i].x, game->npcs[i].y);
            if (dist < 30.0f && (rand() % 500) < 2) { // Small chance to start conversation
                n->talk_target = i;
                n->state = STATE_TALK;
                n->state_timer = 3.0f + (rand() % 200) / 100.0f;
                strcpy(n->current_dialog, npc_conversations[rand() % 8]);
                n->dialog_timer = 2.0f;
                break;
            }
        }
    }
    
    // State machine
    switch (n->state) {
        case STATE_WANDER:
            if (n->state_timer <= 0) {
                n->target_x = n->x + (rand() % 60 - 30);
                n->target_y = n->y + (rand() % 60 - 30);
                n->state_timer = 2.0f + (rand() % 300) / 100.0f;
                
                // Chance to switch to work during day
                if (time_of_day > 9 && time_of_day < 17 && (rand() % 100) < 25) {
                    n->state = STATE_WORK;
                    n->target_x = n->work_x;
                    n->target_y = n->work_y;
                    n->state_timer = 4.0f;
                }
            }
            break;
            
        case STATE_WORK:
            if (n->state_timer <= 0) {
                if (n->type == NPC_FARMER && (rand() % 100) < 60) {
                    n->state = STATE_GATHER;
                    n->target_x = n->work_x + (rand() % 40 - 20);
                    n->target_y = n->work_y + (rand() % 40 - 20);
                    n->state_timer = 2.5f;
                } else {
                    n->state = STATE_WANDER;
                    n->state_timer = 1.0f;
                }
            }
            break;
            
        case STATE_GATHER:
            if (n->state_timer <= 0) {
                n->state = (rand() % 100) < 50 ? STATE_WORK : STATE_WANDER;
                n->target_x = n->state == STATE_WORK ? n->work_x : n->x + (rand() % 40 - 20);
                n->target_y = n->state == STATE_WORK ? n->work_y : n->y + (rand() % 40 - 20);
                n->state_timer = 3.0f;
            }
            break;
            
        case STATE_TALK:
            if (n->state_timer <= 0 || n->talk_target == -1) {
                n->talk_target = -1;
                n->state = STATE_WANDER;
                n->state_timer = 1.0f;
                n->dialog_timer = 0;
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
    if (time_of_day < 6 || time_of_day > 21) {
        if (n->state != STATE_HOME) {
            n->state = STATE_HOME;
            n->target_x = n->home_x;
            n->target_y = n->home_y;
            n->state_timer = 1.0f;
            n->talk_target = -1;
        }
    }
    
    // Move towards target
    f32 dist = distance(n->x, n->y, n->target_x, n->target_y);
    if (dist > 6.0f && n->state != STATE_TALK) {
        f32 speed = 25.0f;
        f32 dx = (n->target_x - n->x) / dist;
        f32 dy = (n->target_y - n->y) / dist;
        
        f32 new_x = n->x + dx * speed * dt;
        f32 new_y = n->y + dy * speed * dt;
        
        // Basic bounds check
        if (new_x > 20 && new_x < WORLD_WIDTH * 8 - 20) {
            n->x = new_x;
        }
        if (new_y > 20 && new_y < WORLD_HEIGHT * 8 - 20) {
            n->y = new_y;
        }
    }
}

// Update game logic
void update_game(game_state* game, f32 dt) {
    // Update time
    game->time_of_day += dt / 10.0f; // 10 seconds = 1 hour for demo
    if (game->time_of_day >= 24.0f) {
        game->time_of_day = 0.0f;
    }
    
    game->demo_timer += dt;
    
    // Update dialog timer
    if (game->current_dialog.active) {
        game->current_dialog.timer -= dt;
        if (game->current_dialog.timer <= 0) {
            game->current_dialog.active = 0;
        }
    }
    
    // Update status timer
    if (game->status_timer > 0) {
        game->status_timer -= dt;
    }
    
    // Handle player input
    f32 speed = 70.0f;
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
    
    // Check collision and move
    if (!check_collision(game, new_x, game->player_y)) {
        game->player_x = new_x;
    }
    if (!check_collision(game, game->player_x, new_y)) {
        game->player_y = new_y;
    }
    
    // Keep in bounds
    if (game->player_x < 16) game->player_x = 16;
    if (game->player_x > WORLD_WIDTH * 8 - 16) game->player_x = WORLD_WIDTH * 8 - 16;
    if (game->player_y < 16) game->player_y = 16;
    if (game->player_y > WORLD_HEIGHT * 8 - 16) game->player_y = WORLD_HEIGHT * 8 - 16;
    
    // Update camera to follow player
    game->camera_x = game->player_x - game->width / 2;
    game->camera_y = game->player_y - (game->height - 60) / 2;
    
    // Keep camera in world bounds
    if (game->camera_x < 0) game->camera_x = 0;
    if (game->camera_y < 0) game->camera_y = 0;
    if (game->camera_x > WORLD_WIDTH * 8 - game->width) 
        game->camera_x = WORLD_WIDTH * 8 - game->width;
    if (game->camera_y > WORLD_HEIGHT * 8 - (game->height - 60)) 
        game->camera_y = WORLD_HEIGHT * 8 - (game->height - 60);
    
    // Check for nearby NPCs
    game->near_npc = -1;
    for (int i = 0; i < game->npc_count; i++) {
        if (game->npcs[i].active) {
            f32 dist = distance(game->player_x, game->player_y, game->npcs[i].x, game->npcs[i].y);
            if (dist < 25.0f) {
                game->near_npc = i;
                break;
            }
        }
    }
    
    // Handle NPC interaction
    if (game->key_space_pressed && game->near_npc >= 0) {
        npc* n = &game->npcs[game->near_npc];
        const char* dialog = get_npc_dialog(n, (int)game->demo_timer);
        show_dialog(game, dialog, game->near_npc);
        
        char greeting[100];
        snprintf(greeting, sizeof(greeting), "%s says hello!", n->name);
        show_status(game, greeting);
    }
    
    // Handle gathering
    handle_player_activity(game);
    
    // Update NPCs
    for (int i = 0; i < game->npc_count; i++) {
        update_npc(&game->npcs[i], game, dt, game->time_of_day, i);
    }
    
    // Reset key pressed flags
    game->key_space_pressed = 0;
    game->key_enter_pressed = 0;
}

// Render frame
void render_frame(game_state* game) {
    // Calculate visible tile range based on camera
    int start_tile_x = (int)(game->camera_x / 8);
    int start_tile_y = (int)(game->camera_y / 8);
    int end_tile_x = start_tile_x + (game->width / 8) + 2;
    int end_tile_y = start_tile_y + ((game->height - 60) / 8) + 2;
    
    // Clamp to world bounds
    if (start_tile_x < 0) start_tile_x = 0;
    if (start_tile_y < 0) start_tile_y = 0;
    if (end_tile_x >= WORLD_WIDTH) end_tile_x = WORLD_WIDTH - 1;
    if (end_tile_y >= WORLD_HEIGHT) end_tile_y = WORLD_HEIGHT - 1;
    
    // Draw visible world tiles
    for (int tile_y = start_tile_y; tile_y <= end_tile_y; tile_y++) {
        for (int tile_x = start_tile_x; tile_x <= end_tile_x; tile_x++) {
            u8 tile = game->world[tile_y][tile_x];
            int screen_x = tile_x * 8 - (int)game->camera_x;
            int screen_y = tile_y * 8 - (int)game->camera_y;
            
            if (screen_x >= -8 && screen_x < game->width && 
                screen_y >= -8 && screen_y < game->height - 60) {
                draw_tile(game, screen_x, screen_y, tile);
            }
        }
    }
    
    // Draw NPCs (only visible ones)
    for (int i = 0; i < game->npc_count; i++) {
        if (game->npcs[i].active) {
            int screen_x = (int)(game->npcs[i].x - game->camera_x);
            int screen_y = (int)(game->npcs[i].y - game->camera_y);
            
            if (screen_x >= -16 && screen_x < game->width + 16 && 
                screen_y >= -16 && screen_y < game->height - 60 + 16) {
                draw_character(game, screen_x, screen_y, game->npcs[i].color, 0);
            }
        }
    }
    
    // Draw player (relative to camera)
    int player_screen_x = (int)(game->player_x - game->camera_x);
    int player_screen_y = (int)(game->player_y - game->camera_y);
    draw_character(game, player_screen_x, player_screen_y, 0x2A, 1);
    
    // Draw UI at bottom
    int ui_y = game->height - 60;
    
    // Clear UI area
    for (int y = ui_y; y < game->height; y++) {
        for (int x = 0; x < game->width; x++) {
            draw_pixel(game, x, y, 0x0F); // Black background
        }
    }
    
    // Enhanced UI Layout
    
    // Dialog box with better styling
    if (game->current_dialog.active) {
        // Draw dialog background
        for (int y = ui_y + 2; y < ui_y + 28; y++) {
            for (int x = 2; x < game->width - 2; x++) {
                if (y == ui_y + 2 || y == ui_y + 27 || x == 2 || x == game->width - 3) {
                    draw_pixel(game, x, y, 0x30); // Border
                } else {
                    draw_pixel(game, x, y, 0x0F); // Dark background
                }
            }
        }
        
        char speaker_name[50] = "You";
        if (game->current_dialog.speaker_id >= 0) {
            strcpy(speaker_name, game->npcs[game->current_dialog.speaker_id].name);
        }
        
        draw_text(game, 8, ui_y + 6, speaker_name, 0x30);
        draw_text(game, 8, ui_y + 17, game->current_dialog.text, 0x20);
    }
    
    // Status messages with background
    if (game->status_timer > 0) {
        // Status background
        for (int x = 5; x < 400; x++) {
            for (int y = ui_y + 32; y < ui_y + 42; y++) {
                if (x == 5 || x == 399 || y == ui_y + 32 || y == ui_y + 41) {
                    draw_pixel(game, x, y, 0x37); // Pink border
                } else {
                    draw_pixel(game, x, y, 0x00); // Black background
                }
            }
        }
        draw_text(game, 8, ui_y + 35, game->status_text, 0x37);
    }
    
    // Enhanced inventory display
    char inv_text[120];
    snprintf(inv_text, sizeof(inv_text), "Inventory: ");
    if (game->inventory_count == 0) {
        strcat(inv_text, "Empty");
    } else {
        for (int i = 0; i < game->inventory_count; i++) {
            char item_str[30];
            snprintf(item_str, sizeof(item_str), "%s x%d", 
                     game->inventory[i].name, game->inventory[i].count);
            strcat(inv_text, item_str);
            if (i < game->inventory_count - 1) strcat(inv_text, ", ");
        }
    }
    draw_text(game, game->width - 350, ui_y + 5, inv_text, 0x25);
    
    // Time display with day/night indicator
    char time_str[60];
    char time_period[20];
    int hour = (int)game->time_of_day;
    if (hour < 6) strcpy(time_period, "Night");
    else if (hour < 12) strcpy(time_period, "Morning"); 
    else if (hour < 18) strcpy(time_period, "Day");
    else strcpy(time_period, "Evening");
    
    snprintf(time_str, sizeof(time_str), "Time: %02d:00 (%s)", hour, time_period);
    draw_text(game, game->width - 200, ui_y + 20, time_str, 0x25);
    
    // Active NPCs count
    int active_npcs = 0;
    for (int i = 0; i < game->npc_count; i++) {
        if (game->npcs[i].active) active_npcs++;
    }
    char npc_str[40];
    snprintf(npc_str, sizeof(npc_str), "Village Population: %d", active_npcs);
    draw_text(game, game->width - 200, ui_y + 35, npc_str, 0x29);
    
    // Controls with better formatting
    if (!game->current_dialog.active) {
        draw_text(game, 8, ui_y + 47, "Controls: WASD=Move  SPACE=Talk  ENTER=Gather  ESC=Quit", 0x10);
    }
    
    // Interaction hint with enhanced styling
    if (game->near_npc >= 0) {
        char hint[80];
        snprintf(hint, sizeof(hint), "> Press SPACE to talk to %s <", 
                 game->npcs[game->near_npc].name);
        draw_text(game, game->width / 2 - 120, ui_y + 30, hint, 0x35);
    }
    
    // World coordinates for debugging (can remove later)
    char pos_str[50];
    snprintf(pos_str, sizeof(pos_str), "Position: (%.0f, %.0f)", 
             game->player_x / 8, game->player_y / 8);
    draw_text(game, 8, ui_y + 5, pos_str, 0x10);
    
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
            case XK_space:
                game->key_space = pressed;
                if (pressed && !game->key_space_pressed) {
                    game->key_space_pressed = 1;
                }
                break;
            case XK_Return:
                game->key_enter = pressed;
                if (pressed && !game->key_enter_pressed) {
                    game->key_enter_pressed = 1;
                }
                break;
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
    printf("╔════════════════════════════════════════╗\n");
    printf("║        HANDMADE VILLAGE ENGINE         ║\n");
    printf("║      Living NES-Style World Demo      ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("🎮 Controls:\n");
    printf("   WASD/Arrows → Move around the village\n");
    printf("   SPACE → Talk to villagers\n");
    printf("   ENTER → Gather resources\n");
    printf("   ESC → Exit\n\n");
    
    printf("🌟 Features:\n");
    printf("   • Living village with 18+ NPCs\n");
    printf("   • Day/night cycle affects behavior\n");
    printf("   • Resource gathering system\n");
    printf("   • Dynamic conversations\n");
    printf("   • Multiple districts to explore\n\n");
    
    printf("Initializing village systems...\n");
    
    srand(time(NULL));
    game_state game = {0};
    
    init_world(&game);
    init_npcs(&game);
    
    if (!init_display(&game)) {
        printf("❌ Could not initialize display\n");
        return 1;
    }
    
    printf("\n🚀 Village simulation ready!\n");
    printf("   Experience a living world in just %d KB!\n\n", 
           (int)(sizeof(game_state) + WORLD_WIDTH * WORLD_HEIGHT) / 1024);
    
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