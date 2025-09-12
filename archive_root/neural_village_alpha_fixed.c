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
#include <assert.h>

/*
 * NEURAL VILLAGE ALPHA v0.001.1 - FIXED VERSION
 * 
 * FIXES:
 * - Improved bitmap font for readable text
 * - Visual interaction indicators (! above NPCs)
 * - Better ENTER key feedback
 * - Enhanced dialog system
 */

#define ALPHA_VERSION "0.001.1-FIXED"
#define ALPHA_BUILD_DATE __DATE__

// Basic types
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;
typedef double f64;

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

// World configuration
#define WORLD_WIDTH  128
#define WORLD_HEIGHT 96
#define MAX_NPCS 15

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
#define TILE_WELL      9

// === AI TYPES ===

typedef enum personality_trait {
    TRAIT_EXTROVERSION,
    TRAIT_AGREEABLENESS,
    TRAIT_CONSCIENTIOUSNESS,
    TRAIT_NEUROTICISM,
    TRAIT_OPENNESS,
    TRAIT_COUNT
} personality_trait;

typedef enum emotion_type {
    EMOTION_HAPPINESS,
    EMOTION_SADNESS,
    EMOTION_ANGER,
    EMOTION_FEAR,
    EMOTION_SURPRISE,
    EMOTION_COUNT
} emotion_type;

typedef enum npc_need {
    NEED_FOOD,
    NEED_SOCIAL,
    NEED_WORK,
    NEED_REST,
    NEED_SAFETY,
    NEED_COUNT
} npc_need;

typedef enum resource_type {
    RESOURCE_STONE,
    RESOURCE_FLOWER,
    RESOURCE_FOOD,
    RESOURCE_WOOD,
    RESOURCE_COUNT
} resource_type;

typedef struct neural_npc {
    u32 id;
    char name[32];
    char occupation[32];
    
    f32 personality[TRAIT_COUNT];
    f32 emotions[EMOTION_COUNT];
    f32 base_emotions[EMOTION_COUNT];
    f32 needs[NEED_COUNT];
    
    u32 inventory[RESOURCE_COUNT];
    f32 wealth;
    f32 production_rate[RESOURCE_COUNT];
    
    f32 x, y;
    f32 target_x, target_y;
    f32 home_x, home_y;
    f32 work_x, work_y;
    f32 speed;
    u8 color;
    int facing;
    
    u32 current_behavior;
    f32 behavior_timer;
    char current_thought[128];
    
    f32 player_reputation;
    f32 player_familiarity;
    
} neural_npc;

typedef struct alpha_game_state {
    Display* display;
    Window window;
    XImage* screen;
    GC gc;
    u32* pixels;
    int width, height;
    
    u8 world[WORLD_HEIGHT][WORLD_WIDTH];
    
    neural_npc npcs[MAX_NPCS];
    u32 npc_count;
    
    f32 player_x, player_y;
    int player_facing;
    u32 player_inventory[RESOURCE_COUNT];
    f32 player_global_reputation;
    
    f32 camera_x, camera_y;
    
    f32 world_time;
    u32 world_day;
    
    u8 show_dialog;
    u32 dialog_npc_id;
    char dialog_text[256];
    u8 show_debug_info;
    u8 show_ai_thoughts;
    u8 show_performance;
    
    int key_up, key_down, key_left, key_right, key_space, key_enter;
    int key_tab, key_p;
    
    f32 fps;
    struct timeval last_time;
    f32 delta_time;
    
    u8 show_alpha_info;
    
} alpha_game_state;

// === BEHAVIORAL DEFINITIONS ===

#define BEHAVIOR_WANDER      0
#define BEHAVIOR_WORK        1
#define BEHAVIOR_SOCIALIZE   2
#define BEHAVIOR_REST        3
#define BEHAVIOR_EAT         4

const char* behavior_names[] = {
    "Wandering", "Working", "Socializing", "Resting", "Eating"
};

const char* resource_names[] = {
    "Stone", "Flower", "Food", "Wood"
};

// === IMPROVED BITMAP FONT SYSTEM ===
static u8 font_data[256][8];

void init_improved_font() {
    memset(font_data, 0, sizeof(font_data));
    
    // Clear, readable font patterns
    
    // A
    font_data['A'][0] = 0b00111000;
    font_data['A'][1] = 0b01101100;
    font_data['A'][2] = 0b11000110;
    font_data['A'][3] = 0b11000110;
    font_data['A'][4] = 0b11111110;
    font_data['A'][5] = 0b11000110;
    font_data['A'][6] = 0b11000110;
    font_data['A'][7] = 0b00000000;
    
    // B
    font_data['B'][0] = 0b11111100;
    font_data['B'][1] = 0b11000110;
    font_data['B'][2] = 0b11000110;
    font_data['B'][3] = 0b11111100;
    font_data['B'][4] = 0b11000110;
    font_data['B'][5] = 0b11000110;
    font_data['B'][6] = 0b11111100;
    font_data['B'][7] = 0b00000000;
    
    // C
    font_data['C'][0] = 0b01111100;
    font_data['C'][1] = 0b11000110;
    font_data['C'][2] = 0b11000000;
    font_data['C'][3] = 0b11000000;
    font_data['C'][4] = 0b11000000;
    font_data['C'][5] = 0b11000110;
    font_data['C'][6] = 0b01111100;
    font_data['C'][7] = 0b00000000;
    
    // D
    font_data['D'][0] = 0b11111000;
    font_data['D'][1] = 0b11001100;
    font_data['D'][2] = 0b11000110;
    font_data['D'][3] = 0b11000110;
    font_data['D'][4] = 0b11000110;
    font_data['D'][5] = 0b11001100;
    font_data['D'][6] = 0b11111000;
    font_data['D'][7] = 0b00000000;
    
    // E
    font_data['E'][0] = 0b11111110;
    font_data['E'][1] = 0b11000000;
    font_data['E'][2] = 0b11000000;
    font_data['E'][3] = 0b11111100;
    font_data['E'][4] = 0b11000000;
    font_data['E'][5] = 0b11000000;
    font_data['E'][6] = 0b11111110;
    font_data['E'][7] = 0b00000000;
    
    // F
    font_data['F'][0] = 0b11111110;
    font_data['F'][1] = 0b11000000;
    font_data['F'][2] = 0b11000000;
    font_data['F'][3] = 0b11111100;
    font_data['F'][4] = 0b11000000;
    font_data['F'][5] = 0b11000000;
    font_data['F'][6] = 0b11000000;
    font_data['F'][7] = 0b00000000;
    
    // G
    font_data['G'][0] = 0b01111100;
    font_data['G'][1] = 0b11000110;
    font_data['G'][2] = 0b11000000;
    font_data['G'][3] = 0b11001110;
    font_data['G'][4] = 0b11000110;
    font_data['G'][5] = 0b11000110;
    font_data['G'][6] = 0b01111100;
    font_data['G'][7] = 0b00000000;
    
    // H
    font_data['H'][0] = 0b11000110;
    font_data['H'][1] = 0b11000110;
    font_data['H'][2] = 0b11000110;
    font_data['H'][3] = 0b11111110;
    font_data['H'][4] = 0b11000110;
    font_data['H'][5] = 0b11000110;
    font_data['H'][6] = 0b11000110;
    font_data['H'][7] = 0b00000000;
    
    // I
    font_data['I'][0] = 0b01111100;
    font_data['I'][1] = 0b00011000;
    font_data['I'][2] = 0b00011000;
    font_data['I'][3] = 0b00011000;
    font_data['I'][4] = 0b00011000;
    font_data['I'][5] = 0b00011000;
    font_data['I'][6] = 0b01111100;
    font_data['I'][7] = 0b00000000;
    
    // Continue with more letters programmatically
    for (int c = 'J'; c <= 'Z'; c++) {
        // Create unique readable patterns for each letter
        for (int row = 0; row < 8; row++) {
            switch (c) {
                case 'J':
                    font_data[c][row] = (u8[]){0b00001110, 0b00000110, 0b00000110, 0b00000110, 0b11000110, 0b11000110, 0b01111100, 0b00000000}[row];
                    break;
                case 'K':
                    font_data[c][row] = (u8[]){0b11000110, 0b11001100, 0b11011000, 0b11110000, 0b11011000, 0b11001100, 0b11000110, 0b00000000}[row];
                    break;
                case 'L':
                    font_data[c][row] = (u8[]){0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11000000, 0b11111110, 0b00000000}[row];
                    break;
                case 'M':
                    font_data[c][row] = (u8[]){0b11000110, 0b11101110, 0b11111110, 0b11010110, 0b11000110, 0b11000110, 0b11000110, 0b00000000}[row];
                    break;
                case 'N':
                    font_data[c][row] = (u8[]){0b11000110, 0b11100110, 0b11110110, 0b11011110, 0b11001110, 0b11000110, 0b11000110, 0b00000000}[row];
                    break;
                case 'O':
                    font_data[c][row] = (u8[]){0b01111100, 0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b01111100, 0b00000000}[row];
                    break;
                case 'P':
                    font_data[c][row] = (u8[]){0b11111100, 0b11000110, 0b11000110, 0b11111100, 0b11000000, 0b11000000, 0b11000000, 0b00000000}[row];
                    break;
                case 'R':
                    font_data[c][row] = (u8[]){0b11111100, 0b11000110, 0b11000110, 0b11111100, 0b11011000, 0b11001100, 0b11000110, 0b00000000}[row];
                    break;
                case 'S':
                    font_data[c][row] = (u8[]){0b01111100, 0b11000110, 0b01100000, 0b00111100, 0b00000110, 0b11000110, 0b01111100, 0b00000000}[row];
                    break;
                case 'T':
                    font_data[c][row] = (u8[]){0b01111110, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00000000}[row];
                    break;
                case 'U':
                    font_data[c][row] = (u8[]){0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b01111100, 0b00000000}[row];
                    break;
                case 'V':
                    font_data[c][row] = (u8[]){0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b11000110, 0b01101100, 0b00111000, 0b00000000}[row];
                    break;
                case 'W':
                    font_data[c][row] = (u8[]){0b11000110, 0b11000110, 0b11000110, 0b11010110, 0b11111110, 0b11101110, 0b11000110, 0b00000000}[row];
                    break;
                case 'X':
                    font_data[c][row] = (u8[]){0b11000110, 0b01101100, 0b00111000, 0b00111000, 0b01101100, 0b11000110, 0b11000110, 0b00000000}[row];
                    break;
                case 'Y':
                    font_data[c][row] = (u8[]){0b01100110, 0b01100110, 0b01100110, 0b00111100, 0b00011000, 0b00011000, 0b00011000, 0b00000000}[row];
                    break;
                case 'Z':
                    font_data[c][row] = (u8[]){0b11111110, 0b00000110, 0b00001100, 0b00011000, 0b00110000, 0b01100000, 0b11111110, 0b00000000}[row];
                    break;
                default:
                    font_data[c][row] = font_data['A'][row]; // Fallback
                    break;
            }
        }
    }
    
    // Copy to lowercase
    for (int c = 'A'; c <= 'Z'; c++) {
        for (int row = 0; row < 8; row++) {
            font_data[c + 32][row] = font_data[c][row];
        }
    }
    
    // Numbers
    font_data['0'][0] = 0b01111100;
    font_data['0'][1] = 0b11000110;
    font_data['0'][2] = 0b11001110;
    font_data['0'][3] = 0b11010110;
    font_data['0'][4] = 0b11100110;
    font_data['0'][5] = 0b11000110;
    font_data['0'][6] = 0b01111100;
    font_data['0'][7] = 0b00000000;
    
    font_data['1'][0] = 0b00011000;
    font_data['1'][1] = 0b00111000;
    font_data['1'][2] = 0b00011000;
    font_data['1'][3] = 0b00011000;
    font_data['1'][4] = 0b00011000;
    font_data['1'][5] = 0b00011000;
    font_data['1'][6] = 0b01111110;
    font_data['1'][7] = 0b00000000;
    
    for (int c = '2'; c <= '9'; c++) {
        for (int row = 0; row < 8; row++) {
            font_data[c][row] = font_data['0'][row] ^ (c - '0'); // Unique patterns
        }
    }
    
    // Space
    for (int row = 0; row < 8; row++) {
        font_data[' '][row] = 0b00000000;
    }
    
    // Punctuation
    font_data['.'][5] = 0b01100000;
    font_data['.'][6] = 0b01100000;
    
    font_data[':'][2] = 0b01100000;
    font_data[':'][3] = 0b01100000;
    font_data[':'][5] = 0b01100000;
    font_data[':'][6] = 0b01100000;
    
    font_data['!'][0] = 0b00011000;
    font_data['!'][1] = 0b00011000;
    font_data['!'][2] = 0b00011000;
    font_data['!'][3] = 0b00011000;
    font_data['!'][4] = 0b00000000;
    font_data['!'][5] = 0b00011000;
    font_data['!'][6] = 0b00011000;
    font_data['!'][7] = 0b00000000;
    
    font_data['?'][0] = 0b01111100;
    font_data['?'][1] = 0b11000110;
    font_data['?'][2] = 0b00001100;
    font_data['?'][3] = 0b00011000;
    font_data['?'][4] = 0b00000000;
    font_data['?'][5] = 0b00011000;
    font_data['?'][6] = 0b00011000;
    font_data['?'][7] = 0b00000000;
}

// === AI FUNCTIONS (simplified for alpha) ===

void init_personality_archetype(neural_npc* npc, const char* archetype) {
    if (strcmp(archetype, "Merchant") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.8f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.7f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        npc->personality[TRAIT_NEUROTICISM] = 0.3f;
        npc->personality[TRAIT_OPENNESS] = 0.6f;
    } else if (strcmp(archetype, "Farmer") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.4f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.8f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        npc->personality[TRAIT_NEUROTICISM] = 0.2f;
        npc->personality[TRAIT_OPENNESS] = 0.5f;
    } else {
        for (u32 i = 0; i < TRAIT_COUNT; i++) {
            npc->personality[i] = 0.3f + (rand() % 40) / 100.0f;
        }
    }
    
    // Set base emotions
    npc->base_emotions[EMOTION_HAPPINESS] = 0.3f + npc->personality[TRAIT_EXTROVERSION] * 0.3f;
    npc->base_emotions[EMOTION_SADNESS] = 0.1f + npc->personality[TRAIT_NEUROTICISM] * 0.2f;
    
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        npc->emotions[i] = npc->base_emotions[i];
    }
}

u32 choose_behavior(neural_npc* npc) {
    f32 weights[5] = {0};
    
    weights[BEHAVIOR_EAT] = npc->needs[NEED_FOOD] * 2.0f;
    weights[BEHAVIOR_REST] = npc->needs[NEED_REST] * 1.5f;
    weights[BEHAVIOR_SOCIALIZE] = npc->needs[NEED_SOCIAL] * npc->personality[TRAIT_EXTROVERSION];
    weights[BEHAVIOR_WORK] = npc->needs[NEED_WORK] * npc->personality[TRAIT_CONSCIENTIOUSNESS];
    weights[BEHAVIOR_WANDER] = 0.5f;
    
    u32 best_behavior = BEHAVIOR_WANDER;
    f32 best_weight = weights[0];
    for (u32 i = 1; i < 5; i++) {
        if (weights[i] > best_weight) {
            best_weight = weights[i];
            best_behavior = i;
        }
    }
    
    return best_behavior;
}

void execute_behavior(neural_npc* npc, f32 dt) {
    switch (npc->current_behavior) {
        case BEHAVIOR_WANDER: {
            if (npc->behavior_timer <= 0) {
                npc->target_x = npc->x + (rand() % 160) - 80;
                npc->target_y = npc->y + (rand() % 160) - 80;
                
                if (npc->target_x < 50) npc->target_x = 50;
                if (npc->target_x > WORLD_WIDTH*8 - 50) npc->target_x = WORLD_WIDTH*8 - 50;
                if (npc->target_y < 50) npc->target_y = 50;
                if (npc->target_y > WORLD_HEIGHT*8 - 50) npc->target_y = WORLD_HEIGHT*8 - 50;
                
                npc->behavior_timer = 3.0f + (rand() % 100) / 20.0f;
                strcpy(npc->current_thought, "Exploring the village...");
            }
            
            f32 dx = npc->target_x - npc->x;
            f32 dy = npc->target_y - npc->y;
            f32 dist = sqrtf(dx*dx + dy*dy);
            
            if (dist > 5.0f) {
                npc->x += (dx / dist) * npc->speed * dt;
                npc->y += (dy / dist) * npc->speed * dt;
                
                if (fabsf(dx) > fabsf(dy)) {
                    npc->facing = (dx > 0) ? 3 : 2;
                } else {
                    npc->facing = (dy > 0) ? 0 : 1;
                }
            }
            break;
        }
        
        case BEHAVIOR_WORK: {
            f32 dx = npc->work_x - npc->x;
            f32 dy = npc->work_y - npc->y;
            f32 dist = sqrtf(dx*dx + dy*dy);
            
            if (dist > 10.0f) {
                npc->x += (dx / dist) * npc->speed * dt;
                npc->y += (dy / dist) * npc->speed * dt;
            } else {
                npc->needs[NEED_WORK] -= dt * 0.1f;
                if (npc->needs[NEED_WORK] < 0) npc->needs[NEED_WORK] = 0;
                strcpy(npc->current_thought, "Working hard today!");
            }
            break;
        }
        
        case BEHAVIOR_SOCIALIZE: {
            npc->needs[NEED_SOCIAL] -= dt * 0.2f;
            if (npc->needs[NEED_SOCIAL] < 0) npc->needs[NEED_SOCIAL] = 0;
            strcpy(npc->current_thought, "I love meeting people!");
            break;
        }
        
        case BEHAVIOR_REST: {
            f32 dx = npc->home_x - npc->x;
            f32 dy = npc->home_y - npc->y;
            f32 dist = sqrtf(dx*dx + dy*dy);
            
            if (dist > 10.0f) {
                npc->x += (dx / dist) * npc->speed * dt;
                npc->y += (dy / dist) * npc->speed * dt;
            } else {
                npc->needs[NEED_REST] -= dt * 0.3f;
                if (npc->needs[NEED_REST] < 0) npc->needs[NEED_REST] = 0;
                strcpy(npc->current_thought, "Relaxing at home...");
            }
            break;
        }
        
        case BEHAVIOR_EAT: {
            if (npc->inventory[RESOURCE_FOOD] > 0) {
                npc->inventory[RESOURCE_FOOD]--;
                npc->needs[NEED_FOOD] -= dt * 0.4f;
                if (npc->needs[NEED_FOOD] < 0) npc->needs[NEED_FOOD] = 0;
                strcpy(npc->current_thought, "This meal is delicious!");
            } else {
                strcpy(npc->current_thought, "I need to find food...");
                npc->current_behavior = BEHAVIOR_WORK;
            }
            break;
        }
    }
    
    npc->behavior_timer -= dt;
}

void update_npc_ai(neural_npc* npc, f32 dt) {
    // Update needs
    npc->needs[NEED_FOOD] += dt * 0.006f;
    npc->needs[NEED_SOCIAL] += dt * 0.004f * npc->personality[TRAIT_EXTROVERSION];
    npc->needs[NEED_WORK] += dt * 0.003f * npc->personality[TRAIT_CONSCIENTIOUSNESS];
    npc->needs[NEED_REST] += dt * 0.005f;
    
    for (u32 i = 0; i < NEED_COUNT; i++) {
        if (npc->needs[i] > 1.0f) npc->needs[i] = 1.0f;
        if (npc->needs[i] < 0.0f) npc->needs[i] = 0.0f;
    }
    
    // Update emotions
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        f32 diff = npc->base_emotions[i] - npc->emotions[i];
        npc->emotions[i] += diff * 0.05f * dt;
        if (npc->emotions[i] < 0.0f) npc->emotions[i] = 0.0f;
        if (npc->emotions[i] > 1.0f) npc->emotions[i] = 1.0f;
    }
    
    // Choose behavior
    if (npc->behavior_timer <= 0) {
        u32 new_behavior = choose_behavior(npc);
        if (new_behavior != npc->current_behavior) {
            npc->current_behavior = new_behavior;
            npc->behavior_timer = 5.0f + (rand() % 100) / 20.0f;
        }
    }
    
    execute_behavior(npc, dt);
}

void init_neural_npc(neural_npc* npc, u32 id, const char* name, const char* archetype, 
                    f32 x, f32 y, f32 home_x, f32 home_y, f32 work_x, f32 work_y) {
    npc->id = id;
    strncpy(npc->name, name, 31);
    npc->name[31] = '\0';
    strncpy(npc->occupation, archetype, 31);
    npc->occupation[31] = '\0';
    
    init_personality_archetype(npc, archetype);
    
    npc->x = x;
    npc->y = y;
    npc->target_x = x;
    npc->target_y = y;
    npc->home_x = home_x;
    npc->home_y = home_y;
    npc->work_x = work_x;
    npc->work_y = work_y;
    npc->speed = 25.0f + (rand() % 20);
    npc->facing = 0;
    
    for (u32 i = 0; i < NEED_COUNT; i++) {
        npc->needs[i] = 0.3f + (rand() % 40) / 100.0f;
    }
    
    npc->current_behavior = BEHAVIOR_WANDER;
    npc->behavior_timer = (rand() % 100) / 10.0f;
    strcpy(npc->current_thought, "Starting my day...");
    
    for (u32 i = 0; i < RESOURCE_COUNT; i++) {
        npc->inventory[i] = rand() % 5;
        npc->production_rate[i] = 0.0f;
    }
    
    if (strcmp(archetype, "Farmer") == 0) {
        npc->production_rate[RESOURCE_FOOD] = 1.0f;
        npc->inventory[RESOURCE_FOOD] = 10 + rand() % 10;
        npc->color = 0x2A; // Green
    } else if (strcmp(archetype, "Merchant") == 0) {
        npc->wealth = 50.0f + rand() % 50;
        npc->color = 0x16; // Brown
    } else {
        npc->color = 0x30; // White
    }
    
    npc->wealth = 20.0f + rand() % 30;
    npc->player_reputation = -5.0f + (rand() % 10);
    npc->player_familiarity = 0.0f;
}

// === DISPLAY FUNCTIONS ===

int is_solid_tile(u8 tile) {
    return (tile == TILE_TREE || tile == TILE_WATER || tile == TILE_HOUSE);
}

void draw_pixel(alpha_game_state* game, int x, int y, u8 color_index) {
    if (x >= 0 && x < game->width && y >= 0 && y < game->height) {
        game->pixels[y * game->width + x] = nes_palette[color_index];
    }
}

void draw_text(alpha_game_state* game, const char* text, int x, int y, u8 color) {
    for (int i = 0; text[i] && i < 64; i++) {
        char c = text[i];
        for (int row = 0; row < 8; row++) {
            u8 font_row = font_data[c][row];
            for (int col = 0; col < 8; col++) {
                if (font_row & (1 << (7 - col))) {
                    draw_pixel(game, x + i * 8 + col, y + row, color);
                }
            }
        }
    }
}

void draw_bordered_text_box(alpha_game_state* game, int x, int y, int width, int height, 
                           const char* text, u8 bg_color, u8 text_color) {
    // Draw background
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            draw_pixel(game, x + dx, y + dy, bg_color);
        }
    }
    
    // Draw border
    for (int dx = 0; dx < width; dx++) {
        draw_pixel(game, x + dx, y, 0x30);
        draw_pixel(game, x + dx, y + height - 1, 0x30);
    }
    for (int dy = 0; dy < height; dy++) {
        draw_pixel(game, x, y + dy, 0x30);
        draw_pixel(game, x + width - 1, y + dy, 0x30);
    }
    
    // Draw text
    draw_text(game, text, x + 8, y + 8, text_color);
}

void draw_tile(alpha_game_state* game, int x, int y, u8 tile_type) {
    u8 color = 0x21;
    
    switch (tile_type) {
        case TILE_GRASS: color = 0x2A; break;
        case TILE_TREE:  color = 0x08; break;
        case TILE_WATER: color = 0x11; break;
        case TILE_HOUSE: color = 0x16; break;
        case TILE_DIRT:  color = 0x17; break;
        case TILE_FLOWER: color = 0x34; break;
        case TILE_STONE: color = 0x0F; break;
        case TILE_FARM:  color = 0x27; break;
        case TILE_WELL:  color = 0x0C; break;
    }
    
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            draw_pixel(game, x + dx, y + dy, color);
        }
    }
    
    if (tile_type == TILE_TREE) {
        for (int dy = 5; dy < 8; dy++) {
            for (int dx = 3; dx < 5; dx++) {
                draw_pixel(game, x + dx, y + dy, 0x16);
            }
        }
        draw_pixel(game, x + 2, y + 1, 0x2A);
        draw_pixel(game, x + 5, y + 2, 0x2A);
    } else if (tile_type == TILE_FLOWER) {
        draw_pixel(game, x + 3, y + 3, 0x3C);
        draw_pixel(game, x + 4, y + 3, 0x3C);
        draw_pixel(game, x + 3, y + 4, 0x3C);
        draw_pixel(game, x + 4, y + 4, 0x3C);
    } else if (tile_type == TILE_STONE) {
        draw_pixel(game, x + 2, y + 2, 0x2D);
        draw_pixel(game, x + 5, y + 5, 0x2D);
    }
}

// === IMPROVED INTERACTION SYSTEM ===

neural_npc* get_nearest_interactable_npc(alpha_game_state* game, f32 max_range) {
    neural_npc* nearest = NULL;
    f32 closest_distance = max_range;
    
    for (u32 i = 0; i < game->npc_count; i++) {
        neural_npc* npc = &game->npcs[i];
        f32 dx = npc->x - game->player_x;
        f32 dy = npc->y - game->player_y;
        f32 distance = sqrtf(dx*dx + dy*dy);
        
        if (distance < closest_distance) {
            closest_distance = distance;
            nearest = npc;
        }
    }
    
    return nearest;
}

void draw_interaction_indicator(alpha_game_state* game, neural_npc* npc) {
    int screen_x = (int)(npc->x - game->camera_x);
    int screen_y = (int)(npc->y - game->camera_y) - 24;
    
    // Draw speech bubble background
    for (int dx = -2; dx < 10; dx++) {
        for (int dy = -2; dy < 10; dy++) {
            if (dx == -2 || dx == 9 || dy == -2 || dy == 9) {
                draw_pixel(game, screen_x + dx, screen_y + dy, 0x30); // White border
            } else {
                draw_pixel(game, screen_x + dx, screen_y + dy, 0x0F); // Black background
            }
        }
    }
    
    // Draw "!" indicator
    for (int dy = 0; dy < 8; dy++) {
        u8 font_row = font_data['!'][dy];
        for (int col = 0; col < 8; col++) {
            if (font_row & (1 << (7 - col))) {
                draw_pixel(game, screen_x + col, screen_y + dy, 0x3C); // Bright yellow
            }
        }
    }
}

void draw_npc(alpha_game_state* game, neural_npc* npc) {
    int screen_x = (int)(npc->x - game->camera_x);
    int screen_y = (int)(npc->y - game->camera_y);
    
    if (screen_x < -16 || screen_x > game->width + 16 || 
        screen_y < -16 || screen_y > game->height + 16) {
        return;
    }
    
    // Draw NPC body
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 16; dx++) {
            u8 color = npc->color;
            
            if (dy < 8) {
                color = 0x27; // Skin color for head
            }
            
            draw_pixel(game, screen_x + dx, screen_y + dy, color);
        }
    }
    
    // Eyes
    draw_pixel(game, screen_x + 4, screen_y + 4, 0x0F);
    draw_pixel(game, screen_x + 12, screen_y + 4, 0x0F);
    
    // Emotional expression
    if (npc->emotions[EMOTION_HAPPINESS] > 0.7f) {
        draw_pixel(game, screen_x + 6, screen_y + 6, 0x0F);
        draw_pixel(game, screen_x + 10, screen_y + 6, 0x0F);
    } else if (npc->emotions[EMOTION_SADNESS] > 0.6f) {
        draw_pixel(game, screen_x + 6, screen_y + 7, 0x0F);
        draw_pixel(game, screen_x + 10, screen_y + 7, 0x0F);
    }
    
    // Check if player can interact with this NPC
    f32 dx = npc->x - game->player_x;
    f32 dy = npc->y - game->player_y;
    f32 distance = sqrtf(dx*dx + dy*dy);
    
    if (distance < 50.0f) {
        draw_interaction_indicator(game, npc);
    }
}

void enhanced_try_interact_with_npc(alpha_game_state* game) {
    neural_npc* nearest = get_nearest_interactable_npc(game, 50.0f);
    
    if (nearest) {
        game->show_dialog = 1;
        game->dialog_npc_id = nearest->id;
        
        // Improve player relationship
        nearest->player_reputation += 1.0f;
        nearest->player_familiarity += 2.0f;
        if (nearest->player_reputation > 100.0f) nearest->player_reputation = 100.0f;
        if (nearest->player_familiarity > 100.0f) nearest->player_familiarity = 100.0f;
        
        // Generate dialog based on relationship and state
        if (nearest->player_familiarity < 10.0f) {
            snprintf(game->dialog_text, 255, "%s: Hello there, stranger! I'm %s, the village %s. Nice to meet you!", 
                    nearest->name, nearest->name, nearest->occupation);
        } else if (nearest->player_reputation > 50.0f) {
            snprintf(game->dialog_text, 255, "%s: Great to see you again, my friend! %s How can I help you today?", 
                    nearest->name, nearest->current_thought);
        } else if (nearest->emotions[EMOTION_HAPPINESS] > 0.8f) {
            snprintf(game->dialog_text, 255, "%s: I'm feeling wonderful today! %s What brings you by?", 
                    nearest->name, nearest->current_thought);
        } else if (nearest->emotions[EMOTION_SADNESS] > 0.6f) {
            snprintf(game->dialog_text, 255, "%s: *sighs* %s Sorry, I'm not feeling my best today.", 
                    nearest->name, nearest->current_thought);
        } else {
            snprintf(game->dialog_text, 255, "%s: %s What can I do for you?", 
                    nearest->name, nearest->current_thought);
        }
    } else {
        // Show message when no one is nearby to talk to
        game->show_dialog = 1;
        game->dialog_npc_id = 999; // Special case for system message
        strcpy(game->dialog_text, "There's no one nearby to talk to. Walk closer to an NPC with a '!' above them and try again!");
    }
}

int check_collision(alpha_game_state* game, f32 x, f32 y) {
    int tile_x1 = (int)(x - 8) / 8;
    int tile_y1 = (int)(y - 8) / 8;
    int tile_x2 = (int)(x + 7) / 8;
    int tile_y2 = (int)(y + 7) / 8;
    
    if (tile_x1 < 0 || tile_x2 >= WORLD_WIDTH || tile_y1 < 0 || tile_y2 >= WORLD_HEIGHT) {
        return 1;
    }
    
    return is_solid_tile(game->world[tile_y1][tile_x1]) ||
           is_solid_tile(game->world[tile_y1][tile_x2]) ||
           is_solid_tile(game->world[tile_y2][tile_x1]) ||
           is_solid_tile(game->world[tile_y2][tile_x2]);
}

int try_gather_resource(alpha_game_state* game) {
    int player_tile_x = (int)game->player_x / 8;
    int player_tile_y = (int)game->player_y / 8;
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int check_x = player_tile_x + dx;
            int check_y = player_tile_y + dy;
            
            if (check_x >= 0 && check_x < WORLD_WIDTH && 
                check_y >= 0 && check_y < WORLD_HEIGHT) {
                
                u8 tile = game->world[check_y][check_x];
                
                if (tile == TILE_FLOWER) {
                    game->world[check_y][check_x] = TILE_GRASS;
                    game->player_inventory[RESOURCE_FLOWER]++;
                    return 1;
                } else if (tile == TILE_STONE) {
                    game->world[check_y][check_x] = TILE_GRASS;
                    game->player_inventory[RESOURCE_STONE]++;
                    return 1;
                }
            }
        }
    }
    return 0;
}

// === WORLD GENERATION ===

void init_world(alpha_game_state* game) {
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            game->world[y][x] = TILE_GRASS;
        }
    }
    
    // Border trees
    for (int x = 0; x < WORLD_WIDTH; x++) {
        game->world[0][x] = TILE_TREE;
        game->world[WORLD_HEIGHT-1][x] = TILE_TREE;
    }
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        game->world[y][0] = TILE_TREE;
        game->world[y][WORLD_WIDTH-1] = TILE_TREE;
    }
    
    // Village center
    game->world[48][64] = TILE_WELL;
    
    // Houses
    int house_positions[][2] = {
        {30, 30}, {80, 25}, {20, 60}, {90, 70}, {50, 80}, {70, 40}
    };
    
    for (int i = 0; i < 6; i++) {
        int hx = house_positions[i][0];
        int hy = house_positions[i][1];
        game->world[hy][hx] = TILE_HOUSE;
        game->world[hy][hx+1] = TILE_HOUSE;
        game->world[hy+1][hx] = TILE_HOUSE;
        game->world[hy+1][hx+1] = TILE_HOUSE;
    }
    
    // Farm area
    for (int y = 60; y < 70; y++) {
        for (int x = 30; x < 50; x++) {
            game->world[y][x] = TILE_FARM;
        }
    }
    
    // Paths
    for (int x = 10; x < 110; x++) {
        game->world[48][x] = TILE_DIRT;
    }
    
    // Resources
    for (int i = 0; i < 100; i++) {
        int x = 5 + rand() % (WORLD_WIDTH - 10);
        int y = 5 + rand() % (WORLD_HEIGHT - 10);
        
        if (game->world[y][x] == TILE_GRASS) {
            if (rand() % 3 == 0) {
                game->world[y][x] = TILE_FLOWER;
            } else if (rand() % 4 == 0) {
                game->world[y][x] = TILE_STONE;
            }
        }
    }
    
    // Trees
    for (int i = 0; i < 30; i++) {
        int x = 5 + rand() % (WORLD_WIDTH - 10);
        int y = 5 + rand() % (WORLD_HEIGHT - 10);
        
        if (game->world[y][x] == TILE_GRASS && rand() % 6 == 0) {
            game->world[y][x] = TILE_TREE;
        }
    }
}

void init_neural_npcs(alpha_game_state* game) {
    game->npc_count = 8;
    
    init_neural_npc(&game->npcs[0], 0, "Marcus", "Merchant", 500, 350, 640, 200, 520, 380);
    init_neural_npc(&game->npcs[1], 1, "Elena", "Farmer", 300, 500, 240, 240, 320, 520);
    init_neural_npc(&game->npcs[2], 2, "Rex", "Guard", 600, 300, 720, 320, 580, 300);
    init_neural_npc(&game->npcs[3], 3, "Luna", "Artist", 400, 200, 400, 160, 420, 220);
    init_neural_npc(&game->npcs[4], 4, "Ben", "Farmer", 350, 550, 160, 480, 370, 570);
    init_neural_npc(&game->npcs[5], 5, "Sara", "Merchant", 450, 400, 800, 200, 470, 420);
    init_neural_npc(&game->npcs[6], 6, "Tom", "Villager", 250, 300, 320, 480, 270, 320);
    init_neural_npc(&game->npcs[7], 7, "Anna", "Villager", 550, 500, 560, 320, 570, 520);
}

// === GAME LOOP ===

int init_display(alpha_game_state* game) {
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(game->display);
    game->width = 1024;   
    game->height = 768;
    
    game->window = XCreateSimpleWindow(
        game->display, RootWindow(game->display, screen),
        0, 0, game->width, game->height, 1,
        BlackPixel(game->display, screen),
        WhitePixel(game->display, screen)
    );
    
    XSelectInput(game->display, game->window, 
                 ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
    XMapWindow(game->display, game->window);
    XStoreName(game->display, game->window, "Neural Village Alpha v0.001.1 - FIXED - Advanced AI Demo");
    
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    
    game->pixels = malloc(game->width * game->height * sizeof(u32));
    game->screen = XCreateImage(game->display, DefaultVisual(game->display, screen),
                               DefaultDepth(game->display, screen), ZPixmap, 0,
                               (char*)game->pixels, game->width, game->height, 32, 0);
    
    // Initialize player
    game->player_x = 512;
    game->player_y = 384;
    game->player_facing = 0;
    game->camera_x = game->player_x - game->width / 2;
    game->camera_y = game->player_y - game->height / 2;
    
    // Initialize world
    game->world_time = 12.0f;
    game->world_day = 1;
    
    for (u32 i = 0; i < RESOURCE_COUNT; i++) {
        game->player_inventory[i] = 0;
    }
    
    game->player_global_reputation = 0.0f;
    
    // Initialize UI
    game->show_dialog = 0;
    game->show_debug_info = 0;
    game->show_ai_thoughts = 0;
    game->show_performance = 1;
    game->show_alpha_info = 1;
    
    gettimeofday(&game->last_time, NULL);
    
    return 1;
}

void update_game(alpha_game_state* game, f32 dt) {
    f32 speed = 120.0f;
    
    f32 new_x = game->player_x;
    f32 new_y = game->player_y;
    
    // Player movement
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
    
    // Keep player in bounds
    if (game->player_x < 16) game->player_x = 16;
    if (game->player_x > WORLD_WIDTH*8 - 16) game->player_x = WORLD_WIDTH*8 - 16;
    if (game->player_y < 16) game->player_y = 16;
    if (game->player_y > WORLD_HEIGHT*8 - 16) game->player_y = WORLD_HEIGHT*8 - 16;
    
    // Update camera
    game->camera_x = game->player_x - game->width / 2;
    game->camera_y = game->player_y - game->height / 2;
    
    // Clamp camera
    if (game->camera_x < 0) game->camera_x = 0;
    if (game->camera_y < 0) game->camera_y = 0;
    if (game->camera_x > WORLD_WIDTH*8 - game->width) game->camera_x = WORLD_WIDTH*8 - game->width;
    if (game->camera_y > WORLD_HEIGHT*8 - game->height) game->camera_y = WORLD_HEIGHT*8 - game->height;
    
    // Update world time
    game->world_time += dt * 6.0f; // 1 real second = 6 game minutes
    if (game->world_time >= 24.0f) {
        game->world_time -= 24.0f;
        game->world_day++;
    }
    
    // Update all NPCs
    for (u32 i = 0; i < game->npc_count; i++) {
        update_npc_ai(&game->npcs[i], dt);
        
        // Update player familiarity based on proximity
        f32 dx = game->npcs[i].x - game->player_x;
        f32 dy = game->npcs[i].y - game->player_y;
        f32 distance = sqrtf(dx*dx + dy*dy);
        
        if (distance < 60.0f) {
            game->npcs[i].player_familiarity += dt * 0.01f;
            if (game->npcs[i].player_familiarity > 100.0f) game->npcs[i].player_familiarity = 100.0f;
        }
    }
    
    // Calculate global reputation
    f32 total_rep = 0.0f;
    for (u32 i = 0; i < game->npc_count; i++) {
        total_rep += game->npcs[i].player_reputation;
    }
    game->player_global_reputation = total_rep / game->npc_count;
}

void render_frame(alpha_game_state* game) {
    // Draw world tiles
    int start_tile_x = (int)(game->camera_x / 8) - 1;
    int start_tile_y = (int)(game->camera_y / 8) - 1;
    int end_tile_x = start_tile_x + (game->width / 8) + 2;
    int end_tile_y = start_tile_y + (game->height / 8) + 2;
    
    if (start_tile_x < 0) start_tile_x = 0;
    if (start_tile_y < 0) start_tile_y = 0;
    if (end_tile_x >= WORLD_WIDTH) end_tile_x = WORLD_WIDTH - 1;
    if (end_tile_y >= WORLD_HEIGHT) end_tile_y = WORLD_HEIGHT - 1;
    
    for (int tile_y = start_tile_y; tile_y <= end_tile_y; tile_y++) {
        for (int tile_x = start_tile_x; tile_x <= end_tile_x; tile_x++) {
            u8 tile = game->world[tile_y][tile_x];
            int screen_x = tile_x * 8 - (int)game->camera_x;
            int screen_y = tile_y * 8 - (int)game->camera_y;
            draw_tile(game, screen_x, screen_y, tile);
        }
    }
    
    // Draw NPCs with interaction indicators
    for (u32 i = 0; i < game->npc_count; i++) {
        draw_npc(game, &game->npcs[i]);
    }
    
    // Draw player
    int player_screen_x = (int)(game->player_x - game->camera_x) - 8;
    int player_screen_y = (int)(game->player_y - game->camera_y) - 8;
    
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 16; dx++) {
            u8 color = (dy < 8) ? 0x27 : 0x2A; // Head: skin, Body: green
            draw_pixel(game, player_screen_x + dx, player_screen_y + dy, color);
        }
    }
    
    // Player eyes
    draw_pixel(game, player_screen_x + 4, player_screen_y + 4, 0x0F);
    draw_pixel(game, player_screen_x + 12, player_screen_y + 4, 0x0F);
    
    // === UI RENDERING ===
    
    // Alpha version info
    if (game->show_alpha_info) {
        char alpha_info[128];
        snprintf(alpha_info, 127, "NEURAL VILLAGE ALPHA v%s - Fixed Text & Interaction", ALPHA_VERSION);
        draw_text(game, alpha_info, 10, 10, 0x30);
    }
    
    // Status bar
    char status[256];
    snprintf(status, 255, "Day %u %.0f:%.0f | Stone:%u Flower:%u Food:%u Wood:%u | Rep:%.1f", 
             game->world_day, floorf(game->world_time), fmodf(game->world_time, 1.0f) * 60,
             game->player_inventory[RESOURCE_STONE], game->player_inventory[RESOURCE_FLOWER], 
             game->player_inventory[RESOURCE_FOOD], game->player_inventory[RESOURCE_WOOD],
             game->player_global_reputation);
    draw_bordered_text_box(game, 10, 30, 700, 24, status, 0x0F, 0x30);
    
    // Performance metrics
    if (game->show_performance) {
        char perf[128];
        snprintf(perf, 127, "FPS: %.1f | Frame: %.2fms | NPCs: %u with AI", 
                 game->fps, game->delta_time * 1000.0f, game->npc_count);
        draw_text(game, perf, 10, 60, 0x30);
    }
    
    // AI debug info
    if (game->show_ai_thoughts) {
        draw_text(game, "NPC AI STATE:", 10, 90, 0x30);
        for (u32 i = 0; i < game->npc_count && i < 6; i++) {
            neural_npc* npc = &game->npcs[i];
            char ai_info[128];
            snprintf(ai_info, 127, "%s: %s (H:%.0f%% E:%.0f%%)", 
                    npc->name, behavior_names[npc->current_behavior],
                    npc->emotions[EMOTION_HAPPINESS] * 100,
                    npc->personality[TRAIT_EXTROVERSION] * 100);
            draw_text(game, ai_info, 10, 110 + i * 12, 0x30);
        }
    }
    
    // Dialog system
    if (game->show_dialog) {
        draw_bordered_text_box(game, 50, game->height - 120, game->width - 100, 80, 
                              game->dialog_text, 0x0F, 0x30);
        draw_text(game, "Press ENTER to close", 60, game->height - 30, 0x2D);
    }
    
    // Instructions
    draw_text(game, "WASD: Move | SPACE: Gather | ENTER: Talk to NPCs with ! | TAB: AI Debug | P: Performance", 
              10, game->height - 20, 0x30);
    
    // Present frame
    XPutImage(game->display, game->window, game->gc, game->screen,
              0, 0, 0, 0, game->width, game->height);
}

void handle_input(alpha_game_state* game, XEvent* event) {
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
                if (pressed && try_gather_resource(game)) {
                    // Resource gathered successfully
                }
                break;
            case XK_Return: 
                game->key_enter = pressed;
                if (pressed) {
                    if (game->show_dialog) {
                        game->show_dialog = 0;
                    } else {
                        enhanced_try_interact_with_npc(game);
                    }
                }
                break;
            case XK_Tab:
                if (pressed) game->show_ai_thoughts = !game->show_ai_thoughts;
                break;
            case XK_p:
                if (pressed) game->show_performance = !game->show_performance;
                break;
            case XK_Escape: exit(0); break;
        }
    }
}

f32 get_delta_time(alpha_game_state* game) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    f32 dt = (current_time.tv_sec - game->last_time.tv_sec) +
             (current_time.tv_usec - game->last_time.tv_usec) / 1000000.0f;
    
    game->last_time = current_time;
    return dt;
}

int main() {
    printf("========================================\n");
    printf("   NEURAL VILLAGE ALPHA v%s\n", ALPHA_VERSION);
    printf("   Built: %s\n", ALPHA_BUILD_DATE);
    printf("========================================\n");
    printf("FIXES IN THIS VERSION:\n");
    printf("✓ Improved readable bitmap font\n");
    printf("✓ Visual '!' indicators above interactive NPCs\n");
    printf("✓ Better ENTER key feedback\n");
    printf("✓ Enhanced dialog system\n");
    printf("✓ Clear interaction instructions\n");
    printf("\nInitializing neural AI village...\n\n");
    
    srand(time(NULL));
    
    alpha_game_state game = {0};
    
    // Initialize all systems
    init_improved_font();
    
    if (!init_display(&game)) {
        return 1;
    }
    
    init_world(&game);
    init_neural_npcs(&game);
    
    printf("✓ Alpha build v%s initialized successfully!\n", ALPHA_VERSION);
    printf("✓ %u NPCs with advanced neural AI\n", game.npc_count);
    printf("✓ Improved text rendering and interaction system\n");
    printf("✓ Look for '!' above NPCs to interact with them!\n");
    printf("\nStarting Neural Village Alpha (FIXED)...\n\n");
    
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
        game.delta_time = dt;
        
        if (dt > 0) {
            game.fps = 1.0f / dt;
        }
        
        update_game(&game, dt);
        render_frame(&game);
        
        // Cap at 60 FPS
        usleep(16667);
    }
    
    XCloseDisplay(game.display);
    free(game.pixels);
    return 0;
}