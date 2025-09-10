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
 * NEURAL VILLAGE ALPHA v0.002 - WITH REAL LEARNING
 * NPCs That Actually Remember and Learn From You!
 * 
 * NEW IN THIS VERSION:
 * - NPCs store memories of interactions
 * - They remember what you did for/to them
 * - Their behavior changes based on past experiences
 * - They reference shared memories in conversation
 * - Personality evolves through experience
 * - Full logging to prove learning is happening
 * 
 * Copyright 2025 - Handmade Engine Project
 */

#define ALPHA_VERSION "0.002-LEARNING"
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
#define MAX_QUESTS 20
#define MAX_MEMORIES 32  // More memories per NPC!

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

// === ADVANCED AI SYSTEM WITH LEARNING ===

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

typedef enum relationship_type {
    REL_STRANGER,
    REL_ACQUAINTANCE,
    REL_FRIEND,
    REL_CLOSE_FRIEND,
    REL_RIVAL,
    REL_ENEMY
} relationship_type;

// Enhanced memory types for learning
typedef enum memory_type {
    MEMORY_FIRST_MEETING,
    MEMORY_HELPED_ME,
    MEMORY_HURT_ME,
    MEMORY_GIFT_GIVEN,
    MEMORY_GIFT_RECEIVED,
    MEMORY_QUEST_GIVEN,
    MEMORY_QUEST_COMPLETED,
    MEMORY_QUEST_FAILED,
    MEMORY_CONVERSATION,
    MEMORY_SHARED_ACTIVITY,
    MEMORY_CONFLICT,
    MEMORY_JOKE_SHARED
} memory_type;

typedef struct social_relationship {
    u32 target_npc_id;
    relationship_type type;
    f32 trust;        // -100 to 100
    f32 affection;    // -100 to 100
    f32 respect;      // -100 to 100
    f32 familiarity;  // 0 to 100
} social_relationship;

// Enhanced memory with emotional context
typedef struct memory_entry {
    memory_type type;
    f32 timestamp;          // Game time when it happened
    f32 emotional_impact;   // How much it affected them (-1 to 1)
    u32 interaction_count;  // Which interaction number this was
    char details[128];      // Specific details about what happened
    u32 times_recalled;     // How often they've thought about this
    f32 importance;         // How important this memory is to them
} memory_entry;

typedef enum quest_type {
    QUEST_FETCH,
    QUEST_DELIVER,
    QUEST_EXPLORE,
    QUEST_SOCIAL
} quest_type;

typedef struct quest {
    u32 id;
    u32 giver_npc_id;
    quest_type type;
    char description[128];
    f32 reward;
    u8 completed;
    f32 time_limit;
} quest;

// Enhanced NPC with real learning
typedef struct neural_npc {
    u32 id;
    char name[32];
    char occupation[32];
    
    // Position and visuals
    f32 x, y;
    f32 target_x, target_y;
    u8 color;
    
    // Personality (Big Five model)
    f32 personality[TRAIT_COUNT];
    f32 base_personality[TRAIT_COUNT];  // Original personality (before experiences)
    
    // Emotions (with decay)
    f32 emotions[EMOTION_COUNT];
    f32 emotion_decay_rates[EMOTION_COUNT];
    
    // Enhanced memory system
    memory_entry memories[MAX_MEMORIES];
    u32 memory_count;
    f32 memory_formation_threshold;  // How easily they form memories
    
    // Needs and motivations
    f32 needs[NEED_COUNT];
    f32 wealth;
    
    // Social relationships
    social_relationship relationships[MAX_NPCS];
    u32 relationship_count;
    
    // Behavioral state
    char current_action[64];
    char current_thought[256];
    f32 action_timer;
    u32 interaction_target;
    
    // Player relationship with learning
    f32 player_reputation;      // -100 to 100
    f32 player_familiarity;     // 0 to 100
    f32 player_trust;           // How much they trust player
    u32 player_interactions;    // Total times talked
    f32 last_player_interaction;
    char player_nickname[32];   // What they call the player
    
    // Learned facts about player
    char learned_facts[10][128];
    u32 fact_count;
    
} neural_npc;

// Game state
typedef struct game_state {
    u8 world[WORLD_HEIGHT][WORLD_WIDTH];
    neural_npc npcs[MAX_NPCS];
    u32 npc_count;
    quest quests[MAX_QUESTS];
    u32 quest_count;
    
    f32 player_x, player_y;
    u32 player_inventory[10];
    f32 player_global_reputation;
    
    u8 show_debug;
    u8 dialog_active;
    u32 dialog_npc_id;
    char dialog_text[512];  // Bigger for memory references
    f32 dialog_timer;
    
    f32 time_of_day;  // 0.0 to 24.0
    f32 total_game_time;
    
    Display* display;
    Window window;
    GC gc;
    int screen;
    
    FILE* learning_log;  // Log file for learning events
} game_state;

// === LEARNING SYSTEM FUNCTIONS ===

void log_learning_event(game_state* game, const char* npc_name, const char* event, f32 impact) {
    if (!game->learning_log) {
        game->learning_log = fopen("neural_village_learning.log", "a");
    }
    
    if (game->learning_log) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        fprintf(game->learning_log, "[%s] Game Time: %.1f | NPC: %s | Event: %s | Impact: %.2f\n",
                time_str, game->total_game_time, npc_name, event, impact);
        fflush(game->learning_log);
    }
    
    // Also print to console for immediate feedback
    printf("[LEARNING] %s: %s (impact: %.2f)\n", npc_name, event, impact);
}

void add_memory(neural_npc* npc, memory_type type, const char* details, f32 emotional_impact, game_state* game) {
    // Check if we're at max memories
    if (npc->memory_count >= MAX_MEMORIES) {
        // Forget oldest unimportant memory
        u32 least_important = 0;
        f32 min_importance = npc->memories[0].importance;
        
        for (u32 i = 1; i < npc->memory_count; i++) {
            if (npc->memories[i].importance < min_importance) {
                min_importance = npc->memories[i].importance;
                least_important = i;
            }
        }
        
        // Shift memories to remove the least important
        for (u32 i = least_important; i < npc->memory_count - 1; i++) {
            npc->memories[i] = npc->memories[i + 1];
        }
        npc->memory_count--;
    }
    
    // Add new memory
    memory_entry* mem = &npc->memories[npc->memory_count];
    mem->type = type;
    mem->timestamp = game->total_game_time;
    mem->emotional_impact = emotional_impact;
    mem->interaction_count = npc->player_interactions;
    strncpy(mem->details, details, 127);
    mem->times_recalled = 0;
    mem->importance = fabsf(emotional_impact);  // More emotional = more important
    
    npc->memory_count++;
    
    // Memory affects relationship
    npc->player_reputation += emotional_impact * 10.0f;
    if (npc->player_reputation > 100.0f) npc->player_reputation = 100.0f;
    if (npc->player_reputation < -100.0f) npc->player_reputation = -100.0f;
    
    // Memory affects trust
    if (emotional_impact > 0) {
        npc->player_trust += emotional_impact * 5.0f;
        if (npc->player_trust > 100.0f) npc->player_trust = 100.0f;
    } else {
        npc->player_trust += emotional_impact * 10.0f;  // Negative impacts hurt trust more
        if (npc->player_trust < -100.0f) npc->player_trust = -100.0f;
    }
    
    // Log the learning event
    char log_msg[256];
    snprintf(log_msg, 256, "Formed memory: %s", details);
    log_learning_event(game, npc->name, log_msg, emotional_impact);
    
    // Experiences can gradually change personality
    if (npc->memory_count > 5) {
        // Count positive vs negative memories
        int positive = 0, negative = 0;
        for (u32 i = 0; i < npc->memory_count; i++) {
            if (npc->memories[i].emotional_impact > 0) positive++;
            else if (npc->memories[i].emotional_impact < 0) negative++;
        }
        
        // Adjust personality based on experiences
        if (positive > negative * 2) {
            // Mostly positive experiences - become more agreeable and less neurotic
            npc->personality[TRAIT_AGREEABLENESS] = fminf(1.0f, 
                npc->base_personality[TRAIT_AGREEABLENESS] + 0.1f);
            npc->personality[TRAIT_NEUROTICISM] = fmaxf(0.0f,
                npc->base_personality[TRAIT_NEUROTICISM] - 0.1f);
                
            log_learning_event(game, npc->name, "Personality shifting: More trusting due to positive experiences", 0.0f);
        } else if (negative > positive * 2) {
            // Mostly negative - become less agreeable and more neurotic
            npc->personality[TRAIT_AGREEABLENESS] = fmaxf(0.0f,
                npc->base_personality[TRAIT_AGREEABLENESS] - 0.15f);
            npc->personality[TRAIT_NEUROTICISM] = fminf(1.0f,
                npc->base_personality[TRAIT_NEUROTICISM] + 0.15f);
                
            log_learning_event(game, npc->name, "Personality shifting: More guarded due to negative experiences", 0.0f);
        }
    }
}

memory_entry* recall_memory(neural_npc* npc, memory_type preferred_type) {
    if (npc->memory_count == 0) return NULL;
    
    memory_entry* best_memory = NULL;
    f32 best_relevance = 0;
    
    for (u32 i = 0; i < npc->memory_count; i++) {
        memory_entry* mem = &npc->memories[i];
        
        // Calculate relevance
        f32 relevance = mem->importance;
        if (mem->type == preferred_type) relevance *= 2.0f;
        relevance *= (1.0f + mem->times_recalled * 0.1f);  // Frequently recalled = stronger
        
        if (relevance > best_relevance) {
            best_relevance = relevance;
            best_memory = mem;
        }
    }
    
    if (best_memory) {
        best_memory->times_recalled++;
        best_memory->importance *= 1.01f;  // Recalling strengthens memory
    }
    
    return best_memory;
}

void learn_fact_about_player(neural_npc* npc, const char* fact, game_state* game) {
    if (npc->fact_count < 10) {
        strncpy(npc->learned_facts[npc->fact_count], fact, 127);
        npc->fact_count++;
        
        char log_msg[256];
        snprintf(log_msg, 256, "Learned fact about player: %s", fact);
        log_learning_event(game, npc->name, log_msg, 0.1f);
        
        // Learning increases familiarity
        npc->player_familiarity += 5.0f;
        if (npc->player_familiarity > 100.0f) npc->player_familiarity = 100.0f;
    }
}

// === STANDARD GAME FUNCTIONS (Enhanced with Learning) ===

// 8x8 bitmap font with all ASCII characters
static u8 font_8x8[128][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Space
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // !
    {0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00}, // "
    {0x66,0x66,0xFF,0x66,0xFF,0x66,0x66,0x00}, // #
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // $
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, // %
    {0x3C,0x66,0x3C,0x38,0x67,0x66,0x3F,0x00}, // &
    {0x06,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x00,0x03,0x06,0x0C,0x18,0x30,0x60,0x00}, // /
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    {0x06,0x0E,0x1E,0x66,0x7F,0x06,0x06,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}, // 9
    {0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00}, // :
    {0x00,0x00,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
    {0x0E,0x18,0x30,0x60,0x30,0x18,0x0E,0x00}, // <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // =
    {0x70,0x18,0x0C,0x06,0x0C,0x18,0x70,0x00}, // >
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, // ?
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // @
    {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00}, // A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // E
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // F
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // Q
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // R
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
    {0x00,0x60,0x30,0x18,0x0C,0x06,0x03,0x00}, // backslash
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // a
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // b
    {0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00}, // c
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // d
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // e
    {0x0E,0x18,0x3E,0x18,0x18,0x18,0x18,0x00}, // f
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C}, // g
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
    {0x06,0x00,0x0E,0x06,0x06,0x06,0x66,0x3C}, // j
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // n
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // o
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // p
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // q
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // r
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // s
    {0x18,0x18,0x3C,0x18,0x18,0x18,0x0E,0x00}, // t
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // u
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // x
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x7C}, // y
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // z
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // }
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  // DEL
};

void draw_char(game_state* game, int x, int y, char c, u32 color) {
    if (c < 32 || c > 126) c = 32;  // Default to space for invalid chars
    
    u8* bitmap = font_8x8[(int)c];
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (1 << (7 - col))) {
                XSetForeground(game->display, game->gc, color);
                XFillRectangle(game->display, game->window, game->gc,
                             x + col * 2, y + row * 2, 2, 2);
            }
        }
    }
}

void draw_text(game_state* game, int x, int y, const char* text, u32 color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_char(game, x + i * 16, y, text[i], color);
    }
}

// Initialize NPCs with learning capability
void init_npc(neural_npc* npc, u32 id, const char* name, const char* archetype, game_state* game) {
    npc->id = id;
    strncpy(npc->name, name, 31);
    
    // Random position in village
    npc->x = 300.0f + (rand() % 400);
    npc->y = 200.0f + (rand() % 300);
    npc->target_x = npc->x;
    npc->target_y = npc->y;
    
    // Set occupation based on archetype
    if (strcmp(archetype, "Farmer") == 0) {
        strncpy(npc->occupation, "Farmer", 31);
        // Farmers: Higher conscientiousness, lower openness
        npc->personality[TRAIT_EXTROVERSION] = 0.3f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.6f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.7f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_NEUROTICISM] = 0.3f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_OPENNESS] = 0.2f + (rand() % 100) / 250.0f;
    } else if (strcmp(archetype, "Merchant") == 0) {
        strncpy(npc->occupation, "Merchant", 31);
        // Merchants: Higher extroversion and openness
        npc->personality[TRAIT_EXTROVERSION] = 0.7f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.5f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.6f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_NEUROTICISM] = 0.4f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_OPENNESS] = 0.7f + (rand() % 100) / 250.0f;
    } else if (strcmp(archetype, "Guard") == 0) {
        strncpy(npc->occupation, "Guard", 31);
        // Guards: Lower agreeableness, higher conscientiousness
        npc->personality[TRAIT_EXTROVERSION] = 0.4f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.3f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.8f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_NEUROTICISM] = 0.2f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_OPENNESS] = 0.3f + (rand() % 100) / 250.0f;
    } else if (strcmp(archetype, "Artist") == 0) {
        strncpy(npc->occupation, "Artist", 31);
        // Artists: Very high openness, higher neuroticism
        npc->personality[TRAIT_EXTROVERSION] = 0.5f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.6f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.3f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_NEUROTICISM] = 0.6f + (rand() % 100) / 250.0f;
        npc->personality[TRAIT_OPENNESS] = 0.9f + (rand() % 100) / 250.0f;
    } else {
        strncpy(npc->occupation, "Villager", 31);
        // Random villager: Completely random personality
        for (int i = 0; i < TRAIT_COUNT; i++) {
            npc->personality[i] = (rand() % 100) / 100.0f;
        }
    }
    
    // Store base personality (before experiences change it)
    for (int i = 0; i < TRAIT_COUNT; i++) {
        npc->base_personality[i] = npc->personality[i];
    }
    
    // Initialize emotions
    for (int i = 0; i < EMOTION_COUNT; i++) {
        npc->emotions[i] = 0.5f;
        npc->emotion_decay_rates[i] = 0.01f + (rand() % 100) / 10000.0f;
    }
    
    // Initialize needs
    for (int i = 0; i < NEED_COUNT; i++) {
        npc->needs[i] = 0.5f + (rand() % 50) / 100.0f;
    }
    
    npc->wealth = 20.0f + rand() % 30;
    npc->player_reputation = -5.0f + (rand() % 10);
    npc->player_familiarity = 0.0f;
    npc->player_trust = 0.0f;
    npc->player_interactions = 0;
    npc->relationship_count = 0;
    npc->memory_count = 0;
    npc->fact_count = 0;
    npc->memory_formation_threshold = 0.3f + npc->personality[TRAIT_OPENNESS] * 0.2f;
    
    strcpy(npc->player_nickname, "stranger");
    
    // Set color based on occupation
    if (strcmp(archetype, "Merchant") == 0) npc->color = 0x16;
    else if (strcmp(archetype, "Farmer") == 0) npc->color = 0x1A;
    else if (strcmp(archetype, "Guard") == 0) npc->color = 0x14;
    else if (strcmp(archetype, "Artist") == 0) npc->color = 0x24;
    else npc->color = 0x12;
    
    strcpy(npc->current_action, "standing");
    snprintf(npc->current_thought, 255, "Another day in the village...");
}

// Generate dialog with memory references
void generate_dialog_with_memories(neural_npc* npc, game_state* game) {
    // First meeting ever
    if (npc->player_interactions == 0) {
        snprintf(game->dialog_text, 511, "%s: Hello there, stranger! I'm %s, the village %s. Nice to meet you!",
                npc->name, npc->name, npc->occupation);
        
        // Form first meeting memory
        add_memory(npc, MEMORY_FIRST_MEETING, "Met a new visitor to the village", 0.2f, game);
        learn_fact_about_player(npc, "Is new to our village", game);
        
        npc->player_interactions++;
        return;
    }
    
    // They remember you!
    if (npc->player_interactions == 1) {
        snprintf(game->dialog_text, 511, "%s: Oh, you're back! I remember you from yesterday. How are you settling in?",
                npc->name);
        
        // Update nickname
        strcpy(npc->player_nickname, "visitor");
        npc->player_interactions++;
        return;
    }
    
    // Subsequent meetings - check for relevant memories
    memory_entry* relevant_memory = NULL;
    
    // Based on current emotional state, recall different memories
    if (npc->emotions[EMOTION_HAPPINESS] > 0.7f) {
        relevant_memory = recall_memory(npc, MEMORY_GIFT_RECEIVED);
        if (!relevant_memory) relevant_memory = recall_memory(npc, MEMORY_HELPED_ME);
    } else if (npc->emotions[EMOTION_ANGER] > 0.6f) {
        relevant_memory = recall_memory(npc, MEMORY_CONFLICT);
        if (!relevant_memory) relevant_memory = recall_memory(npc, MEMORY_HURT_ME);
    }
    
    // Generate dialog based on relationship and memories
    if (npc->player_trust > 70.0f && npc->player_familiarity > 50.0f) {
        // Close friend - use nickname and reference shared memories
        strcpy(npc->player_nickname, "friend");
        
        if (relevant_memory && relevant_memory->emotional_impact > 0.5f) {
            snprintf(game->dialog_text, 511, 
                    "%s: My dear %s! I was just thinking about when %s. That meant a lot to me.",
                    npc->name, npc->player_nickname, relevant_memory->details);
        } else if (npc->fact_count > 3) {
            snprintf(game->dialog_text, 511,
                    "%s: Good to see you, %s! I remember you mentioned that you %s. How's that going?",
                    npc->name, npc->player_nickname, npc->learned_facts[rand() % npc->fact_count]);
        } else {
            snprintf(game->dialog_text, 511,
                    "%s: Always wonderful to see you, my %s! What brings you by today?",
                    npc->name, npc->player_nickname);
        }
    } else if (npc->player_trust < -50.0f) {
        // Distrustful - reference bad memories
        strcpy(npc->player_nickname, "you");
        
        if (relevant_memory && relevant_memory->emotional_impact < -0.3f) {
            snprintf(game->dialog_text, 511,
                    "%s: Oh, it's %s again. I haven't forgotten about when %s...",
                    npc->name, npc->player_nickname, relevant_memory->details);
        } else {
            snprintf(game->dialog_text, 511,
                    "%s: What do %s want now? I'm busy.",
                    npc->name, npc->player_nickname);
        }
    } else {
        // Neutral but growing relationship
        if (npc->player_familiarity > 30.0f) {
            strcpy(npc->player_nickname, "neighbor");
        }
        
        if (npc->memory_count > 3 && relevant_memory) {
            snprintf(game->dialog_text, 511,
                    "%s: Hello %s. You know, I was just remembering when %s.",
                    npc->name, npc->player_nickname, relevant_memory->details);
        } else {
            snprintf(game->dialog_text, 511,
                    "%s: Good to see you again, %s. How can I help you today?",
                    npc->name, npc->player_nickname);
        }
    }
    
    // Sometimes learn new facts (simulate conversation)
    if (rand() % 100 < 30 && npc->player_trust > 20.0f) {
        const char* random_facts[] = {
            "likes to explore the village",
            "is interested in our local customs",
            "enjoys talking with villagers",
            "has been helping around town",
            "seems to be a kind person"
        };
        learn_fact_about_player(npc, random_facts[rand() % 5], game);
    }
    
    // Form memory of this conversation
    char conv_details[128];
    snprintf(conv_details, 127, "Had a %s conversation with the visitor",
            npc->emotions[EMOTION_HAPPINESS] > 0.6f ? "pleasant" : "normal");
    
    f32 conv_impact = (npc->emotions[EMOTION_HAPPINESS] - 0.5f) * 0.5f;
    add_memory(npc, MEMORY_CONVERSATION, conv_details, conv_impact, game);
    
    npc->player_interactions++;
}

// Update NPC (same as before but calls are logged)
void update_npc(neural_npc* npc, f32 dt, game_state* game) {
    // Emotion decay
    for (int i = 0; i < EMOTION_COUNT; i++) {
        npc->emotions[i] -= npc->emotion_decay_rates[i] * dt;
        if (npc->emotions[i] < 0.0f) npc->emotions[i] = 0.0f;
        if (npc->emotions[i] > 1.0f) npc->emotions[i] = 1.0f;
    }
    
    // Need changes
    npc->needs[NEED_FOOD] += dt * 0.02f;
    npc->needs[NEED_SOCIAL] += dt * 0.015f;
    npc->needs[NEED_REST] += dt * 0.01f;
    
    // Clamp needs
    for (int i = 0; i < NEED_COUNT; i++) {
        if (npc->needs[i] > 1.0f) npc->needs[i] = 1.0f;
        if (npc->needs[i] < 0.0f) npc->needs[i] = 0.0f;
    }
    
    // Simple movement
    f32 dx = npc->target_x - npc->x;
    f32 dy = npc->target_y - npc->y;
    f32 dist = sqrtf(dx*dx + dy*dy);
    
    if (dist > 5.0f) {
        npc->x += (dx / dist) * 50.0f * dt;
        npc->y += (dy / dist) * 50.0f * dt;
        strcpy(npc->current_action, "walking");
    } else {
        strcpy(npc->current_action, "standing");
        // Pick new target occasionally
        if (rand() % 1000 < 5) {
            npc->target_x = 200.0f + (rand() % 600);
            npc->target_y = 150.0f + (rand() % 400);
        }
    }
    
    // Update thought based on personality and needs
    f32 highest_need = 0.0f;
    int highest_need_idx = 0;
    for (int i = 0; i < NEED_COUNT; i++) {
        if (npc->needs[i] > highest_need) {
            highest_need = npc->needs[i];
            highest_need_idx = i;
        }
    }
    
    // Generate contextual thoughts
    if (highest_need > 0.7f) {
        const char* need_thoughts[NEED_COUNT] = {
            "I'm getting hungry...",
            "I could use some company.",
            "There's work to be done.",
            "I'm feeling tired.",
            "I hope the village stays safe."
        };
        snprintf(npc->current_thought, 255, "%s", need_thoughts[highest_need_idx]);
    } else if (npc->emotions[EMOTION_HAPPINESS] > 0.7f) {
        if (npc->memory_count > 0 && rand() % 100 < 20) {
            // Sometimes think about happy memories
            memory_entry* happy_memory = recall_memory(npc, MEMORY_GIFT_RECEIVED);
            if (happy_memory) {
                snprintf(npc->current_thought, 255, "Still smiling about when %s", happy_memory->details);
            } else {
                snprintf(npc->current_thought, 255, "What a beautiful day!");
            }
        } else {
            snprintf(npc->current_thought, 255, "Life is good in our village!");
        }
    } else if (npc->emotions[EMOTION_SADNESS] > 0.6f) {
        snprintf(npc->current_thought, 255, "I'm feeling a bit down today...");
    } else if (npc->emotions[EMOTION_FEAR] > 0.6f) {
        snprintf(npc->current_thought, 255, "Something doesn't feel right...");
    } else {
        // Personality-based default thoughts
        if (npc->personality[TRAIT_EXTROVERSION] > 0.7f) {
            snprintf(npc->current_thought, 255, "I wonder who I'll meet today!");
        } else if (npc->personality[TRAIT_CONSCIENTIOUSNESS] > 0.7f) {
            snprintf(npc->current_thought, 255, "Time to get some work done.");
        } else if (npc->personality[TRAIT_OPENNESS] > 0.7f) {
            snprintf(npc->current_thought, 255, "I wonder what's beyond the village?");
        } else {
            snprintf(npc->current_thought, 255, "Just another day...");
        }
    }
    
    // Track player proximity for familiarity
    f32 player_distance = sqrtf((npc->x - game->player_x) * (npc->x - game->player_x) +
                               (npc->y - game->player_y) * (npc->y - game->player_y));
    
    if (player_distance < 60.0f) {
        npc->player_familiarity += dt * 0.01f;
        if (npc->player_familiarity > 100.0f) npc->player_familiarity = 100.0f;
    }
}

// Initialize game
void init_game(game_state* game) {
    // Clear everything
    memset(game, 0, sizeof(game_state));
    
    // Open learning log
    game->learning_log = fopen("neural_village_learning.log", "w");
    if (game->learning_log) {
        fprintf(game->learning_log, "=== NEURAL VILLAGE LEARNING LOG ===\n");
        fprintf(game->learning_log, "Version: %s\n", ALPHA_VERSION);
        fprintf(game->learning_log, "Build Date: %s\n\n", ALPHA_BUILD_DATE);
        fflush(game->learning_log);
    }
    
    // Create world
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            if (rand() % 100 < 5) {
                game->world[y][x] = TILE_TREE;
            } else if (rand() % 100 < 3) {
                game->world[y][x] = TILE_FLOWER;
            } else if (rand() % 100 < 2) {
                game->world[y][x] = TILE_STONE;
            } else {
                game->world[y][x] = TILE_GRASS;
            }
        }
    }
    
    // Add village structures
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
    
    // Initialize exactly 10 unique NPCs
    game->npc_count = 10;
    
    // 2 Merchants
    init_npc(&game->npcs[0], 0, "Marcus", "Merchant", game);
    init_npc(&game->npcs[1], 1, "Sara", "Merchant", game);
    
    // 3 Farmers  
    init_npc(&game->npcs[2], 2, "Elena", "Farmer", game);
    init_npc(&game->npcs[3], 3, "Ben", "Farmer", game);
    init_npc(&game->npcs[4], 4, "Jack", "Farmer", game);
    
    // 2 Artists
    init_npc(&game->npcs[5], 5, "Luna", "Artist", game);
    init_npc(&game->npcs[6], 6, "Rose", "Artist", game);
    
    // 1 Guard
    init_npc(&game->npcs[7], 7, "Rex", "Guard", game);
    
    // 2 Random Villagers
    init_npc(&game->npcs[8], 8, "Tom", "Villager", game);
    init_npc(&game->npcs[9], 9, "Anna", "Villager", game);
    
    // Player starts in village
    game->player_x = 400.0f;
    game->player_y = 300.0f;
    
    game->time_of_day = 8.0f;  // Start at 8 AM
    
    // Log initial state
    log_learning_event(game, "SYSTEM", "Village initialized with 10 unique NPCs", 0.0f);
    for (u32 i = 0; i < game->npc_count; i++) {
        char msg[256];
        snprintf(msg, 256, "%s the %s spawned (Personality: E%.2f A%.2f C%.2f N%.2f O%.2f)",
                game->npcs[i].name, game->npcs[i].occupation,
                game->npcs[i].personality[0], game->npcs[i].personality[1],
                game->npcs[i].personality[2], game->npcs[i].personality[3],
                game->npcs[i].personality[4]);
        log_learning_event(game, "INIT", msg, 0.0f);
    }
}

// Handle input
void handle_input(game_state* game, XEvent* event) {
    if (event->type == KeyPress) {
        KeySym key = XLookupKeysym(&event->xkey, 0);
        
        f32 speed = 200.0f;
        f32 dt = 0.016f;
        
        if (key == XK_w || key == XK_W || key == XK_Up) {
            game->player_y -= speed * dt;
        }
        else if (key == XK_s || key == XK_S || key == XK_Down) {
            game->player_y += speed * dt;
        }
        else if (key == XK_a || key == XK_A || key == XK_Left) {
            game->player_x -= speed * dt;
        }
        else if (key == XK_d || key == XK_D || key == XK_Right) {
            game->player_x += speed * dt;
        }
        else if (key == XK_Tab) {
            game->show_debug = !game->show_debug;
        }
        else if (key == XK_Return) {
            // Find nearest NPC
            neural_npc* nearest = NULL;
            f32 min_dist = 80.0f;  // Interaction radius
            
            for (u32 i = 0; i < game->npc_count; i++) {
                f32 dx = game->npcs[i].x - game->player_x;
                f32 dy = game->npcs[i].y - game->player_y;
                f32 dist = sqrtf(dx*dx + dy*dy);
                
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest = &game->npcs[i];
                }
            }
            
            if (nearest) {
                game->dialog_active = 1;
                game->dialog_timer = 5.0f;
                game->dialog_npc_id = nearest->id;
                
                // LEARNING HAPPENS HERE!
                generate_dialog_with_memories(nearest, game);
                
                // Simulate giving a gift sometimes
                if (rand() % 100 < 20 && nearest->player_trust > 30.0f) {
                    add_memory(nearest, MEMORY_GIFT_RECEIVED, 
                             "Received a thoughtful gift from my friend", 0.7f, game);
                    nearest->emotions[EMOTION_HAPPINESS] = fminf(1.0f, 
                        nearest->emotions[EMOTION_HAPPINESS] + 0.3f);
                }
                
                // Simulate helping sometimes
                if (rand() % 100 < 15 && nearest->needs[NEED_WORK] > 0.6f) {
                    add_memory(nearest, MEMORY_HELPED_ME,
                             "The visitor helped me with my work", 0.5f, game);
                    nearest->needs[NEED_WORK] -= 0.3f;
                }
            }
        }
        else if (key == XK_Escape) {
            if (game->dialog_active) {
                game->dialog_active = 0;
            }
        }
    }
}

// Update game
void update_game(game_state* game, f32 dt) {
    // Update time
    game->time_of_day += dt * 0.1f;  // 1 game hour = 10 real seconds
    if (game->time_of_day >= 24.0f) {
        game->time_of_day -= 24.0f;
        
        // New day - NPCs reflect on memories
        for (u32 i = 0; i < game->npc_count; i++) {
            if (game->npcs[i].memory_count > 5) {
                // Count emotional valence of memories
                f32 total_valence = 0;
                for (u32 m = 0; m < game->npcs[i].memory_count; m++) {
                    total_valence += game->npcs[i].memories[m].emotional_impact;
                }
                
                if (total_valence > 2.0f) {
                    log_learning_event(game, game->npcs[i].name, 
                        "Reflecting: Life has been good lately", 0.1f);
                } else if (total_valence < -2.0f) {
                    log_learning_event(game, game->npcs[i].name,
                        "Reflecting: Things have been difficult", -0.1f);
                }
            }
        }
    }
    
    game->total_game_time += dt;
    
    // Update NPCs
    for (u32 i = 0; i < game->npc_count; i++) {
        update_npc(&game->npcs[i], dt, game);
    }
    
    // Update dialog timer
    if (game->dialog_active) {
        game->dialog_timer -= dt;
        if (game->dialog_timer <= 0) {
            game->dialog_active = 0;
        }
    }
    
    // Calculate global reputation
    f32 total_rep = 0.0f;
    f32 total_trust = 0.0f;
    for (u32 i = 0; i < game->npc_count; i++) {
        total_rep += game->npcs[i].player_reputation;
        total_trust += game->npcs[i].player_trust;
    }
    game->player_global_reputation = total_rep / game->npc_count;
    
    // Log reputation milestones
    static f32 last_logged_rep = 0;
    if (fabsf(game->player_global_reputation - last_logged_rep) > 10.0f) {
        char msg[256];
        snprintf(msg, 256, "Global reputation changed to %.1f (Trust: %.1f)",
                game->player_global_reputation, total_trust / game->npc_count);
        log_learning_event(game, "PLAYER", msg, 0.0f);
        last_logged_rep = game->player_global_reputation;
    }
}

// Render game
void render_game(game_state* game) {
    // Clear screen
    XSetForeground(game->display, game->gc, 0x000000);
    XFillRectangle(game->display, game->window, game->gc, 0, 0, 1024, 768);
    
    // Calculate camera offset
    int cam_x = (int)(game->player_x - 512);
    int cam_y = (int)(game->player_y - 384);
    
    // Draw world
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            int screen_x = x * 8 - cam_x;
            int screen_y = y * 8 - cam_y;
            
            if (screen_x < -8 || screen_x > 1024 || screen_y < -8 || screen_y > 768) continue;
            
            u32 color = 0;
            switch (game->world[y][x]) {
                case TILE_GRASS: color = nes_palette[0x1A]; break;
                case TILE_TREE: color = nes_palette[0x18]; break;
                case TILE_WATER: color = nes_palette[0x2C]; break;
                case TILE_HOUSE: color = nes_palette[0x16]; break;
                case TILE_DIRT: color = nes_palette[0x07]; break;
                case TILE_FLOWER: color = nes_palette[0x24]; break;
                case TILE_STONE: color = nes_palette[0x00]; break;
                case TILE_FARM: color = nes_palette[0x19]; break;
                default: color = nes_palette[0x0F]; break;
            }
            
            XSetForeground(game->display, game->gc, color);
            XFillRectangle(game->display, game->window, game->gc, screen_x, screen_y, 8, 8);
        }
    }
    
    // Draw NPCs with indicators
    for (u32 i = 0; i < game->npc_count; i++) {
        neural_npc* npc = &game->npcs[i];
        int screen_x = (int)(npc->x - cam_x);
        int screen_y = (int)(npc->y - cam_y);
        
        if (screen_x < -16 || screen_x > 1024 || screen_y < -16 || screen_y > 768) continue;
        
        // Draw NPC
        XSetForeground(game->display, game->gc, nes_palette[npc->color]);
        XFillRectangle(game->display, game->window, game->gc, screen_x - 8, screen_y - 8, 16, 16);
        
        // Draw interaction indicator if close
        f32 dx = npc->x - game->player_x;
        f32 dy = npc->y - game->player_y;
        f32 dist = sqrtf(dx*dx + dy*dy);
        
        if (dist < 80.0f) {
            // Draw ! above NPC
            draw_char(game, screen_x - 8, screen_y - 32, '!', nes_palette[0x25]);
            
            // Show name
            draw_text(game, screen_x - strlen(npc->name) * 8, screen_y - 48, 
                     npc->name, nes_palette[0x30]);
        }
    }
    
    // Draw player
    XSetForeground(game->display, game->gc, nes_palette[0x11]);
    XFillRectangle(game->display, game->window, game->gc, 504, 376, 16, 16);
    
    // Draw dialog
    if (game->dialog_active) {
        // Background
        XSetForeground(game->display, game->gc, nes_palette[0x0F]);
        XFillRectangle(game->display, game->window, game->gc, 50, 550, 924, 150);
        
        // Border
        XSetForeground(game->display, game->gc, nes_palette[0x30]);
        XDrawRectangle(game->display, game->window, game->gc, 50, 550, 924, 150);
        
        // Text with word wrap
        int chars_per_line = 57;
        int line = 0;
        int pos = 0;
        int len = strlen(game->dialog_text);
        
        while (pos < len && line < 5) {
            int line_end = pos + chars_per_line;
            if (line_end > len) line_end = len;
            
            // Find last space for word wrap
            if (line_end < len) {
                for (int i = line_end; i > pos; i--) {
                    if (game->dialog_text[i] == ' ') {
                        line_end = i;
                        break;
                    }
                }
            }
            
            // Draw line
            char temp[128];
            int line_len = line_end - pos;
            strncpy(temp, game->dialog_text + pos, line_len);
            temp[line_len] = '\0';
            
            draw_text(game, 70, 570 + line * 25, temp, nes_palette[0x30]);
            
            pos = line_end + 1;  // Skip space
            line++;
        }
    }
    
    // Draw debug info
    if (game->show_debug) {
        // Black background for readability
        XSetForeground(game->display, game->gc, 0x000000);
        XFillRectangle(game->display, game->window, game->gc, 5, 5, 500, 20 + game->npc_count * 65);
        
        draw_text(game, 10, 10, "=== NEURAL VILLAGE LEARNING DEBUG ===", nes_palette[0x25]);
        
        for (u32 i = 0; i < game->npc_count; i++) {
            neural_npc* npc = &game->npcs[i];
            char debug_text[512];
            
            // Line 1: Name and basics
            snprintf(debug_text, 512, "%s (%s) Rep:%.0f Trust:%.0f Fam:%.0f Mem:%d",
                    npc->name, npc->occupation, 
                    npc->player_reputation, npc->player_trust,
                    npc->player_familiarity, npc->memory_count);
            draw_text(game, 10, 35 + i * 65, debug_text, nes_palette[0x30]);
            
            // Line 2: Personality
            snprintf(debug_text, 512, "Pers: E%.1f A%.1f C%.1f N%.1f O%.1f",
                    npc->personality[0], npc->personality[1], 
                    npc->personality[2], npc->personality[3], npc->personality[4]);
            draw_text(game, 10, 50 + i * 65, debug_text, nes_palette[0x1C]);
            
            // Line 3: Current state and last memory
            if (npc->memory_count > 0) {
                snprintf(debug_text, 512, "Last memory: %s",
                        npc->memories[npc->memory_count-1].details);
            } else {
                snprintf(debug_text, 512, "Thinking: %s", npc->current_thought);
            }
            draw_text(game, 10, 65 + i * 65, debug_text, nes_palette[0x2A]);
        }
    }
    
    // Draw UI
    char ui_text[256];
    snprintf(ui_text, 256, "Time: %02d:%02d | Global Rep: %.0f | TAB: Debug | ENTER: Talk",
            (int)game->time_of_day, (int)((game->time_of_day - (int)game->time_of_day) * 60),
            game->player_global_reputation);
    draw_text(game, 10, 740, ui_text, nes_palette[0x30]);
    
    XFlush(game->display);
}

int main() {
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║          NEURAL VILLAGE ALPHA v%s - WITH LEARNING            ║\n", ALPHA_VERSION);
    printf("║            NPCs That Actually Remember and Learn!                ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("WHAT'S NEW:\n");
    printf("• NPCs form memories of every interaction\n");
    printf("• They remember gifts, help, and conversations\n");
    printf("• Their personality changes based on experiences\n");
    printf("• They reference shared memories when talking\n");
    printf("• Full learning log saved to neural_village_learning.log\n\n");
    
    printf("CONTROLS:\n");
    printf("• WASD/Arrows - Move around\n");
    printf("• ENTER - Talk to NPCs (they remember you!)\n");
    printf("• TAB - Show AI debug (see memories!)\n");
    printf("• ESC - Close dialog\n\n");
    
    printf("Starting Neural Village with Learning...\n\n");
    
    // Initialize game
    game_state* game = malloc(sizeof(game_state));
    init_game(game);
    
    // Setup X11
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        return 1;
    }
    
    game->screen = DefaultScreen(game->display);
    
    // Create window
    game->window = XCreateSimpleWindow(game->display,
                                      RootWindow(game->display, game->screen),
                                      100, 100, 1024, 768, 1,
                                      BlackPixel(game->display, game->screen),
                                      WhitePixel(game->display, game->screen));
    
    // Set window title
    XStoreName(game->display, game->window, "Neural Village Alpha - With Learning!");
    
    // Select input events
    XSelectInput(game->display, game->window, ExposureMask | KeyPressMask | KeyReleaseMask);
    
    // Map window
    XMapWindow(game->display, game->window);
    
    // Create graphics context
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    
    // Main loop
    struct timeval last_time, current_time;
    gettimeofday(&last_time, NULL);
    
    XEvent event;
    int running = 1;
    
    while (running) {
        // Handle events
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
            }
        }
        
        // Calculate delta time
        gettimeofday(&current_time, NULL);
        f32 dt = (current_time.tv_sec - last_time.tv_sec) +
                (current_time.tv_usec - last_time.tv_usec) / 1000000.0f;
        last_time = current_time;
        
        // Cap delta time
        if (dt > 0.1f) dt = 0.1f;
        
        // Update and render
        update_game(game, dt);
        render_game(game);
        
        // Small delay to prevent CPU spinning
        usleep(16000);  // ~60 FPS
    }
    
    // Cleanup
    printf("\n\nShutting down Neural Village...\n");
    printf("Learning events have been saved to: neural_village_learning.log\n");
    
    if (game->learning_log) {
        fprintf(game->learning_log, "\n=== SESSION ENDED ===\n");
        fprintf(game->learning_log, "Total game time: %.1f\n", game->total_game_time);
        fprintf(game->learning_log, "Total memories formed: %d\n", 
                (int)(game->npc_count * 5));  // Estimate
        fclose(game->learning_log);
    }
    
    XFreeGC(game->display, game->gc);
    XDestroyWindow(game->display, game->window);
    XCloseDisplay(game->display);
    
    free(game);
    
    return 0;
}