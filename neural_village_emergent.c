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

/*
 * NEURAL VILLAGE - EMERGENT PERSONALITIES
 * 
 * NPCs have backstories, not scripts.
 * They express themselves based on:
 * - Their unique history
 * - Current emotional state  
 * - Relationship with you
 * - Recent experiences
 * 
 * No two conversations will be the same!
 */

typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;

// NES colors
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
#define MAX_MEMORIES 30
#define MAX_TOPICS 20

// Emotion dimensions
typedef enum {
    EMO_HAPPY,
    EMO_SAD,
    EMO_ANGRY,
    EMO_AFRAID,
    EMO_SURPRISED,
    EMO_DISGUSTED,
    EMO_CURIOUS,
    EMO_LONELY,
    EMO_COUNT
} emotion_type;

// Experience types that shape personality
typedef enum {
    EXP_TRAUMA,
    EXP_JOY,
    EXP_LOSS,
    EXP_ACHIEVEMENT,
    EXP_BETRAYAL,
    EXP_LOVE,
    EXP_DISCOVERY,
    EXP_MUNDANE
} experience_type;

// Topics NPCs can think/talk about
typedef enum {
    TOPIC_SELF,
    TOPIC_FAMILY,
    TOPIC_WORK,
    TOPIC_DREAMS,
    TOPIC_FEARS,
    TOPIC_VILLAGE,
    TOPIC_NATURE,
    TOPIC_PAST,
    TOPIC_FUTURE,
    TOPIC_PHILOSOPHY,
    TOPIC_PLAYER
} topic_type;

// A single experience/memory
typedef struct {
    experience_type type;
    f32 intensity;      // How strong was this experience (0-1)
    f32 age;           // How long ago (affects fading)
    char description[128];
    topic_type related_topic;
    emotion_type triggered_emotion;
} experience;

// NPC with emergent personality
typedef struct {
    char name[32];
    
    // Backstory defines who they are
    char backstory[512];
    char childhood[256];
    char defining_moment[256];
    char secret[128];
    char dream[128];
    char fear[128];
    
    // Core personality (shaped by backstory)
    f32 openness;       // 0-1: How willing to share
    f32 stability;      // 0-1: Emotional stability
    f32 optimism;       // -1 to 1: Worldview
    f32 introversion;   // 0-1: Social energy
    f32 trust_nature;   // 0-1: How easily they trust
    
    // Current emotional state (changes constantly)
    f32 emotions[EMO_COUNT];
    f32 emotional_momentum[EMO_COUNT];  // Rate of change
    
    // Experiences shape them over time
    experience memories[MAX_MEMORIES];
    u32 memory_count;
    
    // Relationship with player
    f32 familiarity;    // 0-100: How well they know you
    f32 trust;          // -100 to 100: Do they trust you
    f32 affection;      // -100 to 100: Do they like you
    u32 conversations;  // Times we've talked
    f32 last_chat_time;
    
    // Current state
    f32 x, y, vx, vy;
    f32 energy;         // 0-1: Tired vs energetic
    f32 stress;         // 0-1: Calm vs stressed
    topic_type current_focus;  // What's on their mind
    
    // Expression state
    char current_thought[256];
    f32 thought_emotion;  // Emotional charge of thought
    u32 color;
} npc;

// Game state
typedef struct {
    u8 world[WORLD_HEIGHT][WORLD_WIDTH];
    npc npcs[MAX_NPCS];
    u32 npc_count;
    
    f32 player_x, player_y;
    f32 player_vx, player_vy;
    
    // Inventory for gathering
    u32 flowers_collected;
    u32 stones_collected;
    
    u8 show_debug;
    u8 dialog_active;
    u32 dialog_npc_id;
    char dialog_text[512];
    f32 dialog_timer;
    
    f32 game_time;
    f32 day_time;  // 0-24 hour cycle
    
    Display* display;
    Window window;
    GC gc;
    int screen;
    
    u8 keys_held[4];
    
    FILE* expression_log;
} game_state;

// Complete 8x8 font
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

void draw_char(game_state* game, int x, int y, char c, u32 color) {
    if (c < 32) c = 32;
    if (c > 127) c = 32;
    int idx = c - 32;
    if (idx >= 96) idx = 0;
    
    u8* bitmap = font_8x8[idx];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (1 << (7 - col))) {
                XSetForeground(game->display, game->gc, color);
                XFillRectangle(game->display, game->window, game->gc,
                             x + col * 3, y + row * 3, 3, 3);
            }
        }
    }
}

void draw_text(game_state* game, int x, int y, const char* text, u32 color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_char(game, x + i * 25, y, text[i], color);
    }
}

// Log emergent expressions
void log_expression(game_state* game, const char* npc_name, const char* expression, f32 emotion) {
    if (!game->expression_log) {
        game->expression_log = fopen("emergent_expressions.log", "a");
    }
    if (game->expression_log) {
        fprintf(game->expression_log, "[%.1f] %s (emotion:%.2f): %s\n", 
                game->game_time, npc_name, emotion, expression);
        fflush(game->expression_log);
    }
}

// Add experience to NPC
void add_experience(npc* n, experience_type type, const char* desc, f32 intensity, game_state* game) {
    if (n->memory_count >= MAX_MEMORIES) {
        // Forget oldest mundane memory
        int oldest_mundane = -1;
        for (int i = 0; i < n->memory_count; i++) {
            if (n->memories[i].type == EXP_MUNDANE) {
                oldest_mundane = i;
                break;
            }
        }
        
        if (oldest_mundane >= 0) {
            // Remove it
            for (int i = oldest_mundane; i < n->memory_count - 1; i++) {
                n->memories[i] = n->memories[i + 1];
            }
            n->memory_count--;
        } else {
            // All memories are important, forget the oldest
            for (int i = 0; i < n->memory_count - 1; i++) {
                n->memories[i] = n->memories[i + 1];
            }
            n->memory_count--;
        }
    }
    
    experience* exp = &n->memories[n->memory_count++];
    exp->type = type;
    exp->intensity = intensity;
    exp->age = 0;
    strncpy(exp->description, desc, 127);
    exp->description[127] = '\0';
    
    // Experience affects emotions
    switch (type) {
        case EXP_JOY:
            exp->triggered_emotion = EMO_HAPPY;
            n->emotions[EMO_HAPPY] += intensity * 0.5f;
            n->optimism += intensity * 0.01f;
            break;
        case EXP_TRAUMA:
            exp->triggered_emotion = EMO_AFRAID;
            n->emotions[EMO_AFRAID] += intensity * 0.7f;
            n->emotions[EMO_SAD] += intensity * 0.3f;
            n->stability -= intensity * 0.02f;
            break;
        case EXP_LOSS:
            exp->triggered_emotion = EMO_SAD;
            n->emotions[EMO_SAD] += intensity * 0.8f;
            break;
        case EXP_BETRAYAL:
            exp->triggered_emotion = EMO_ANGRY;
            n->emotions[EMO_ANGRY] += intensity * 0.6f;
            n->trust_nature -= intensity * 0.03f;
            break;
        case EXP_LOVE:
            exp->triggered_emotion = EMO_HAPPY;
            n->emotions[EMO_HAPPY] += intensity * 0.7f;
            n->openness += intensity * 0.02f;
            break;
        case EXP_DISCOVERY:
            exp->triggered_emotion = EMO_CURIOUS;
            n->emotions[EMO_CURIOUS] += intensity * 0.8f;
            break;
        default:
            exp->triggered_emotion = EMO_HAPPY;
            break;
    }
    
    // Cap emotions
    for (int i = 0; i < EMO_COUNT; i++) {
        if (n->emotions[i] > 1.0f) n->emotions[i] = 1.0f;
        if (n->emotions[i] < 0.0f) n->emotions[i] = 0.0f;
    }
    
    // Cap personality traits
    if (n->optimism > 1.0f) n->optimism = 1.0f;
    if (n->optimism < -1.0f) n->optimism = -1.0f;
    if (n->stability > 1.0f) n->stability = 1.0f;
    if (n->stability < 0.0f) n->stability = 0.0f;
    if (n->trust_nature > 1.0f) n->trust_nature = 1.0f;
    if (n->trust_nature < 0.0f) n->trust_nature = 0.0f;
    if (n->openness > 1.0f) n->openness = 1.0f;
    if (n->openness < 0.0f) n->openness = 0.0f;
}

// Generate emergent dialog based on personality and state
void generate_emergent_dialog(npc* n, game_state* game) {
    // Build context from current state
    f32 dominant_emotion_value = 0;
    emotion_type dominant_emotion = EMO_HAPPY;
    for (int i = 0; i < EMO_COUNT; i++) {
        if (n->emotions[i] > dominant_emotion_value) {
            dominant_emotion_value = n->emotions[i];
            dominant_emotion = i;
        }
    }
    
    // First meeting - introduce based on personality
    if (n->conversations == 0) {
        if (n->openness > 0.7f) {
            // Open person - shares freely
            snprintf(game->dialog_text, 511, "%s: Oh, hello! I'm %s. %s", 
                    n->name, n->name, n->dream);
        } else if (n->openness < 0.3f) {
            // Closed person - guarded
            snprintf(game->dialog_text, 511, "%s: ...Yes? I'm %s. What do you want?", 
                    n->name, n->name);
        } else {
            // Moderate - polite
            snprintf(game->dialog_text, 511, "%s: Hello. I'm %s. Nice to meet you.", 
                    n->name, n->name);
        }
        
        add_experience(n, EXP_MUNDANE, "Met someone new", 0.2f, game);
        n->conversations++;
        n->familiarity += 5;
        return;
    }
    
    // Subsequent meetings - emergent expression
    char expression[512];
    
    // Check emotional state
    if (dominant_emotion == EMO_SAD && dominant_emotion_value > 0.6f) {
        // Sad - might share or hide based on personality
        if (n->openness > 0.5f && n->trust > 30) {
            // Shares sadness
            if (n->memory_count > 0) {
                // Reference a sad memory
                for (int i = n->memory_count - 1; i >= 0; i--) {
                    if (n->memories[i].triggered_emotion == EMO_SAD) {
                        snprintf(expression, 511, "I've been thinking about %s. It still hurts.", 
                                n->memories[i].description);
                        break;
                    }
                }
            } else {
                snprintf(expression, 511, "I'm not having the best day, to be honest.");
            }
        } else {
            // Hides sadness
            snprintf(expression, 511, "Oh... hello. I'm fine. Just tired.");
        }
    } else if (dominant_emotion == EMO_HAPPY && dominant_emotion_value > 0.7f) {
        // Happy - expression varies by introversion
        if (n->introversion < 0.4f) {
            // Extrovert - shares joy
            snprintf(expression, 511, "What a wonderful day! I feel so alive!");
        } else {
            // Introvert - quieter joy
            snprintf(expression, 511, "Today has been... really nice actually.");
        }
    } else if (dominant_emotion == EMO_CURIOUS && dominant_emotion_value > 0.5f) {
        // Curious - asks questions
        if (n->familiarity < 30) {
            snprintf(expression, 511, "I've been wondering... what brings you to our village?");
        } else {
            snprintf(expression, 511, "Tell me, what do you think about %s?", 
                    n->current_focus == TOPIC_FUTURE ? "the future" : "life here");
        }
    } else if (dominant_emotion == EMO_LONELY && dominant_emotion_value > 0.5f) {
        // Lonely - reaches out
        if (n->trust > 20) {
            snprintf(expression, 511, "It's... it's good to see you. I've been alone with my thoughts.");
        } else {
            snprintf(expression, 511, "Oh, you again. I suppose some company is nice.");
        }
    } else if (n->stress > 0.7f) {
        // Stressed - shows strain
        if (n->stability > 0.6f) {
            // Stable - manages it
            snprintf(expression, 511, "Sorry, I'm a bit overwhelmed today. But I'll manage.");
        } else {
            // Unstable - struggles
            snprintf(expression, 511, "I... I can't... Everything is just too much right now!");
        }
    } else if (n->trust > 70 && n->familiarity > 50) {
        // Close friend - shares secrets or deep thoughts
        if (rand() % 100 < 30 && n->openness > 0.5f) {
            snprintf(expression, 511, "Can I tell you something? %s", n->secret);
        } else {
            snprintf(expression, 511, "You know, %s", n->defining_moment);
        }
    } else if (n->trust < -30) {
        // Distrusts player
        if (dominant_emotion == EMO_ANGRY) {
            snprintf(expression, 511, "You again. Haven't you done enough?");
        } else {
            snprintf(expression, 511, "Please... just leave me alone.");
        }
    } else {
        // Default - based on current focus/personality
        switch (n->current_focus) {
            case TOPIC_DREAMS:
                if (n->optimism > 0.3f) {
                    snprintf(expression, 511, "I've been thinking about my dreams. %s", n->dream);
                } else {
                    snprintf(expression, 511, "Dreams... do they ever come true? I wonder.");
                }
                break;
            case TOPIC_FEARS:
                if (n->openness > 0.6f && n->trust > 40) {
                    snprintf(expression, 511, "Sometimes I worry... %s", n->fear);
                } else {
                    snprintf(expression, 511, "We all have fears, don't we?");
                }
                break;
            case TOPIC_PAST:
                snprintf(expression, 511, "You know, %s", n->childhood);
                break;
            case TOPIC_WORK:
                if (n->energy < 0.3f) {
                    snprintf(expression, 511, "Work has been exhausting lately.");
                } else {
                    snprintf(expression, 511, "Keeping busy with work, as always.");
                }
                break;
            default:
                // Personality-based default
                if (n->optimism > 0.5f) {
                    snprintf(expression, 511, "Life has its challenges, but we persevere!");
                } else if (n->optimism < -0.3f) {
                    snprintf(expression, 511, "Same troubles, different day...");
                } else {
                    snprintf(expression, 511, "Another day in the village.");
                }
                break;
        }
    }
    
    // Format with name
    snprintf(game->dialog_text, 511, "%s: %s", n->name, expression);
    
    // Log this emergent expression
    log_expression(game, n->name, expression, dominant_emotion_value);
    
    // Update relationship
    n->conversations++;
    n->familiarity += 2.0f;
    if (n->familiarity > 100) n->familiarity = 100;
    
    // Talking affects emotions
    if (n->emotions[EMO_LONELY] > 0) {
        n->emotions[EMO_LONELY] -= 0.2f;
    }
    n->emotions[EMO_HAPPY] += 0.1f;
    
    // Random chance to build/lose trust based on interaction
    if (rand() % 100 < 20) {
        if (dominant_emotion == EMO_HAPPY || dominant_emotion == EMO_CURIOUS) {
            n->trust += 5;
            n->affection += 3;
        }
    }
    
    n->last_chat_time = game->game_time;
}

// Create unique backstory for each NPC
void create_backstory(npc* n, const char* name, int personality_seed) {
    strncpy(n->name, name, 31);
    n->name[31] = '\0';
    
    // Use seed for consistent but unique personality
    srand(personality_seed);
    
    // Generate personality from "genetics" + "upbringing"
    n->openness = 0.2f + (rand() % 60) / 100.0f;
    n->stability = 0.3f + (rand() % 50) / 100.0f;
    n->optimism = -0.5f + (rand() % 100) / 100.0f;
    n->introversion = (rand() % 100) / 100.0f;
    n->trust_nature = 0.2f + (rand() % 60) / 100.0f;
    
    // Create backstory based on personality
    if (strcmp(name, "Elena") == 0) {
        strcpy(n->backstory, "Lost her parents young. Raised by grandmother who taught her herbalism.");
        strcpy(n->childhood, "I remember grandmother's garden, full of healing herbs and stories.");
        strcpy(n->defining_moment, "The plague came when I was twelve. I couldn't save them all.");
        strcpy(n->secret, "Sometimes I still hear my mother's voice in the wind.");
        strcpy(n->dream, "I want to discover a cure that could have saved them.");
        strcpy(n->fear, "What if I'm not strong enough when people need me?");
        n->stability -= 0.1f;  // Trauma affected her
        n->emotions[EMO_SAD] = 0.3f;  // Baseline sadness
    } else if (strcmp(name, "Marcus") == 0) {
        strcpy(n->backstory, "Former soldier turned merchant. Saw too much war.");
        strcpy(n->childhood, "Father was a blacksmith. I was meant to forge, not fight.");
        strcpy(n->defining_moment, "I had to choose: follow orders or save innocents. I chose conscience.");
        strcpy(n->secret, "I still wake up screaming some nights.");
        strcpy(n->dream, "A world where shields are only decorations.");
        strcpy(n->fear, "That the violence inside me will return.");
        n->trust_nature -= 0.2f;  // War made him cautious
        n->emotions[EMO_ANGRY] = 0.2f;  // Suppressed anger
    } else if (strcmp(name, "Luna") == 0) {
        strcpy(n->backstory, "Artist who sees colors others cannot. Considered strange.");
        strcpy(n->childhood, "I painted on cave walls. Mother said I was touched by spirits.");
        strcpy(n->defining_moment, "I painted the mayor's death before it happened. Now they fear me.");
        strcpy(n->secret, "The paintings show me things. Future? Past? I don't know.");
        strcpy(n->dream, "To paint something so beautiful it heals hearts.");
        strcpy(n->fear, "That my visions are madness, not gift.");
        n->openness += 0.3f;  // Very open
        n->emotions[EMO_CURIOUS] = 0.5f;  // Always wondering
    } else if (strcmp(name, "Tom") == 0) {
        strcpy(n->backstory, "Simple farmer. Loves the land. Lost wife to fever.");
        strcpy(n->childhood, "Pa taught me to read the weather in cloud shapes.");
        strcpy(n->defining_moment, "Sara's last words: 'Keep planting seeds, my love.'");
        strcpy(n->secret, "I talk to her grave every morning.");
        strcpy(n->dream, "To see our planned orchard bloom.");
        strcpy(n->fear, "Forgetting the sound of her laugh.");
        n->stability += 0.1f;  // Farming grounded him
        n->emotions[EMO_SAD] = 0.4f;
        n->emotions[EMO_LONELY] = 0.6f;
    } else if (strcmp(name, "Rose") == 0) {
        strcpy(n->backstory, "Noble runaway. Fled arranged marriage. Living free but hunted.");
        strcpy(n->childhood, "Gold cages are still cages. I learned that young.");
        strcpy(n->defining_moment, "I jumped from the tower window. Better dead than enslaved.");
        strcpy(n->secret, "My father's men are still searching. I see them sometimes.");
        strcpy(n->dream, "To love whom I choose, live how I choose.");
        strcpy(n->fear, "They'll find me and drag me back to that life.");
        n->emotions[EMO_AFRAID] = 0.3f;
        n->trust_nature -= 0.3f;  // Hard to trust
    } else if (strcmp(name, "Ben") == 0) {
        strcpy(n->backstory, "Village drunk turned philosopher. Seeking redemption.");
        strcpy(n->childhood, "My father drank. I swore I'd be different. I wasn't.");
        strcpy(n->defining_moment, "Woke up in a ditch. My daughter crying. 'Not again, papa.'");
        strcpy(n->secret, "Three years sober but the thirst never leaves.");
        strcpy(n->dream, "To be the father she deserves.");
        strcpy(n->fear, "One bad day and I'll lose everything again.");
        n->stability -= 0.2f;
        n->optimism += 0.2f;  // Fighting to stay positive
    } else if (strcmp(name, "Sara") == 0) {
        strcpy(n->backstory, "Traveling merchant. Collects stories more than coins.");
        strcpy(n->childhood, "Caravan life. New horizon every dawn. Home was movement.");
        strcpy(n->defining_moment, "Found a dying man's journal. His stories became mine to tell.");
        strcpy(n->secret, "I've never stayed anywhere longer than a season. Until now.");
        strcpy(n->dream, "To write a book of all the lives I've glimpsed.");
        strcpy(n->fear, "Roots. What if I grow them and can't pull free?");
        n->introversion -= 0.3f;  // Extroverted
        n->emotions[EMO_CURIOUS] = 0.6f;
    } else if (strcmp(name, "Rex") == 0) {
        strcpy(n->backstory, "Guard with a poet's heart. Protects what he cannot have.");
        strcpy(n->childhood, "Mother read me epic poems. I wanted to be a hero.");
        strcpy(n->defining_moment, "I saved the mayor's daughter. She married someone 'suitable.'");
        strcpy(n->secret, "I write her poems she'll never read.");
        strcpy(n->dream, "One day, courage enough to speak my heart.");
        strcpy(n->fear, "Dying with these words unspoken.");
        n->emotions[EMO_LONELY] = 0.5f;
        n->emotions[EMO_SAD] = 0.3f;
    } else if (strcmp(name, "Anna") == 0) {
        strcpy(n->backstory, "Village healer's apprentice. Sees death too often.");
        strcpy(n->childhood, "Played with dolls. Now I close real eyes forever.");
        strcpy(n->defining_moment, "The child I couldn't save looked just like my sister.");
        strcpy(n->secret, "I've started seeing the dead in shadows.");
        strcpy(n->dream, "To save just one more than I lose.");
        strcpy(n->fear, "That I'm breaking. That I'll become numb to it all.");
        n->stability -= 0.15f;
        n->emotions[EMO_SAD] = 0.4f;
    } else if (strcmp(name, "Jack") == 0) {
        strcpy(n->backstory, "Young dreamer. Wants adventure but fears leaving home.");
        strcpy(n->childhood, "Climbed every tree. Explored every cave. Village feels small now.");
        strcpy(n->defining_moment, "Found an old map in the ruins. X marks... something.");
        strcpy(n->secret, "I've packed my bag twelve times. Never left once.");
        strcpy(n->dream, "To see the ocean. To know if it's really endless.");
        strcpy(n->fear, "What if the world is disappointing? What if I am?");
        n->emotions[EMO_CURIOUS] = 0.7f;
        n->optimism += 0.3f;
    }
    
    // Starting emotional state based on backstory
    n->energy = 0.5f + (rand() % 50) / 100.0f;
    n->stress = 0.2f + (rand() % 30) / 100.0f;
    
    // Random starting position
    n->x = 200 + rand() % 600;
    n->y = 200 + rand() % 400;
    n->vx = 0;
    n->vy = 0;
    
    // Visual
    n->color = 0x10 + (rand() % 16);
    
    // Reset seed for main game
    srand(time(NULL) + rand());
}

// Update NPC emotions and thoughts
void update_npc_mind(npc* n, f32 dt, game_state* game) {
    // Emotional decay/momentum
    for (int i = 0; i < EMO_COUNT; i++) {
        // Emotions naturally decay toward baseline
        f32 baseline = 0.1f;
        if (i == EMO_LONELY) {
            // Loneliness grows over time without interaction
            if (game->game_time - n->last_chat_time > 30.0f) {
                baseline = 0.4f;
            }
        }
        
        f32 decay_rate = 0.1f * dt;
        if (n->emotions[i] > baseline) {
            n->emotions[i] -= decay_rate;
        } else if (n->emotions[i] < baseline) {
            n->emotions[i] += decay_rate;
        }
        
        // Apply momentum
        n->emotions[i] += n->emotional_momentum[i] * dt;
        n->emotional_momentum[i] *= 0.95f;  // Momentum decays
        
        // Clamp
        if (n->emotions[i] > 1.0f) n->emotions[i] = 1.0f;
        if (n->emotions[i] < 0.0f) n->emotions[i] = 0.0f;
    }
    
    // Energy and stress affect emotions
    if (n->energy < 0.3f) {
        n->emotions[EMO_ANGRY] += 0.05f * dt;
        n->emotions[EMO_HAPPY] -= 0.05f * dt;
    }
    
    if (n->stress > 0.7f) {
        n->emotions[EMO_AFRAID] += 0.08f * dt;
        n->emotions[EMO_ANGRY] += 0.05f * dt;
    }
    
    // Update current focus based on emotions and personality
    if (n->emotions[EMO_SAD] > 0.6f) {
        n->current_focus = TOPIC_PAST;
    } else if (n->emotions[EMO_CURIOUS] > 0.5f) {
        n->current_focus = (rand() % 2) ? TOPIC_PHILOSOPHY : TOPIC_FUTURE;
    } else if (n->emotions[EMO_AFRAID] > 0.5f) {
        n->current_focus = TOPIC_FEARS;
    } else if (n->emotions[EMO_HAPPY] > 0.6f) {
        n->current_focus = TOPIC_DREAMS;
    } else {
        // Random focus change occasionally
        if (rand() % 1000 < 5) {
            n->current_focus = rand() % 11;
        }
    }
    
    // Generate internal thoughts (what they're thinking but not saying)
    f32 dominant_value = 0;
    emotion_type dominant = EMO_HAPPY;
    for (int i = 0; i < EMO_COUNT; i++) {
        if (n->emotions[i] > dominant_value) {
            dominant_value = n->emotions[i];
            dominant = i;
        }
    }
    
    // Think about current focus
    switch (n->current_focus) {
        case TOPIC_PAST:
            if (n->memory_count > 0 && rand() % 100 < 30) {
                snprintf(n->current_thought, 255, "Remembering: %s", 
                        n->memories[rand() % n->memory_count].description);
            } else {
                strcpy(n->current_thought, "The past haunts me...");
            }
            break;
        case TOPIC_DREAMS:
            snprintf(n->current_thought, 255, "If only... %s", n->dream);
            break;
        case TOPIC_FEARS:
            snprintf(n->current_thought, 255, "Worried: %s", n->fear);
            break;
        case TOPIC_WORK:
            strcpy(n->current_thought, "Must keep working...");
            break;
        case TOPIC_PHILOSOPHY:
            strcpy(n->current_thought, "What is the meaning of it all?");
            break;
        default:
            strcpy(n->current_thought, "...");
            break;
    }
    
    n->thought_emotion = dominant_value;
    
    // Age memories
    for (int i = 0; i < n->memory_count; i++) {
        n->memories[i].age += dt;
        // Old memories fade in intensity
        if (n->memories[i].age > 100.0f) {
            n->memories[i].intensity *= 0.99f;
        }
    }
    
    // Energy regenerates with low stress
    if (n->stress < 0.3f) {
        n->energy += 0.1f * dt;
        if (n->energy > 1.0f) n->energy = 1.0f;
    }
    
    // Stress decays slowly
    n->stress -= 0.05f * dt;
    if (n->stress < 0) n->stress = 0;
}

// Update NPC physics
void update_npc_physics(npc* n, f32 dt) {
    // Wander based on personality
    if (rand() % 100 < 2) {
        if (n->introversion > 0.7f) {
            // Introverts move less
            n->vx = (rand() % 3 - 1) * 20.0f;
            n->vy = (rand() % 3 - 1) * 20.0f;
        } else {
            // Extroverts move more
            n->vx = (rand() % 3 - 1) * 40.0f;
            n->vy = (rand() % 3 - 1) * 40.0f;
        }
    }
    
    // Apply friction
    n->vx *= 0.93f;
    n->vy *= 0.93f;
    
    // Update position
    n->x += n->vx * dt;
    n->y += n->vy * dt;
    
    // Boundaries
    if (n->x < 50) { n->x = 50; n->vx = 0; }
    if (n->x > 950) { n->x = 950; n->vx = 0; }
    if (n->y < 50) { n->y = 50; n->vy = 0; }
    if (n->y > 700) { n->y = 700; n->vy = 0; }
}

// Initialize world
void init_world(game_state* game) {
    // Simple grass world
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            game->world[y][x] = 1;  // Grass
            if (rand() % 100 < 5) game->world[y][x] = 2;  // Trees
            if (rand() % 100 < 3) game->world[y][x] = 3;  // Flowers
            if (rand() % 100 < 2) game->world[y][x] = 5;  // Stones
        }
    }
    
    // Add some houses
    for (int i = 0; i < 5; i++) {
        int hx = 30 + (i % 3) * 20;
        int hy = 20 + (i / 3) * 15;
        for (int y = 0; y < 6; y++) {
            for (int x = 0; x < 8; x++) {
                if (hx+x < WORLD_WIDTH && hy+y < WORLD_HEIGHT) {
                    game->world[hy+y][hx+x] = 4;  // House
                }
            }
        }
    }
}

// Initialize game
void init_game(game_state* game) {
    memset(game, 0, sizeof(game_state));
    
    init_world(game);
    
    // Create NPCs with backstories
    game->npc_count = 10;
    create_backstory(&game->npcs[0], "Elena", 12345);
    create_backstory(&game->npcs[1], "Marcus", 23456);
    create_backstory(&game->npcs[2], "Luna", 34567);
    create_backstory(&game->npcs[3], "Tom", 45678);
    create_backstory(&game->npcs[4], "Rose", 56789);
    create_backstory(&game->npcs[5], "Ben", 67890);
    create_backstory(&game->npcs[6], "Sara", 78901);
    create_backstory(&game->npcs[7], "Rex", 89012);
    create_backstory(&game->npcs[8], "Anna", 90123);
    create_backstory(&game->npcs[9], "Jack", 01234);
    
    // Give them some initial experiences
    add_experience(&game->npcs[0], EXP_LOSS, "Lost parents to plague", 0.9f, game);
    add_experience(&game->npcs[1], EXP_TRAUMA, "Saw horrors of war", 0.8f, game);
    add_experience(&game->npcs[2], EXP_DISCOVERY, "Discovered prophetic visions", 0.7f, game);
    add_experience(&game->npcs[3], EXP_LOSS, "Wife died of fever", 0.9f, game);
    add_experience(&game->npcs[4], EXP_ACHIEVEMENT, "Escaped arranged marriage", 0.8f, game);
    add_experience(&game->npcs[5], EXP_ACHIEVEMENT, "Three years sober", 0.7f, game);
    add_experience(&game->npcs[6], EXP_DISCOVERY, "Found dying man's journal", 0.6f, game);
    add_experience(&game->npcs[7], EXP_LOVE, "Fell for mayor's daughter", 0.8f, game);
    add_experience(&game->npcs[8], EXP_TRAUMA, "Couldn't save a child", 0.7f, game);
    add_experience(&game->npcs[9], EXP_DISCOVERY, "Found mysterious map", 0.5f, game);
    
    game->player_x = 500;
    game->player_y = 400;
    
    game->day_time = 8.0f;  // Start at 8 AM
    
    // Open expression log
    game->expression_log = fopen("emergent_expressions.log", "w");
    if (game->expression_log) {
        fprintf(game->expression_log, "=== EMERGENT PERSONALITY LOG ===\n");
        fprintf(game->expression_log, "NPCs express themselves based on:\n");
        fprintf(game->expression_log, "- Backstory\n");
        fprintf(game->expression_log, "- Current emotions\n");
        fprintf(game->expression_log, "- Relationship with player\n");
        fprintf(game->expression_log, "- Recent experiences\n\n");
        fflush(game->expression_log);
    }
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
            // Gather resources
            int px = (int)(game->player_x / 8);
            int py = (int)(game->player_y / 8);
            
            int gathered = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int tx = px + dx;
                    int ty = py + dy;
                    
                    if (tx >= 0 && tx < WORLD_WIDTH && ty >= 0 && ty < WORLD_HEIGHT) {
                        if (game->world[ty][tx] == 3) {  // Flower
                            game->flowers_collected++;
                            game->world[ty][tx] = 1;  // Grass
                            gathered = 1;
                        } else if (game->world[ty][tx] == 5) {  // Stone
                            game->stones_collected++;
                            game->world[ty][tx] = 1;  // Grass
                            gathered = 1;
                        }
                    }
                }
            }
            
            if (gathered) {
                snprintf(game->dialog_text, 511, "Gathered resources!");
                game->dialog_active = 1;
                game->dialog_timer = 1.0f;
            }
        }
        else if (key == XK_Return) {
            // Find nearest NPC
            npc* nearest = NULL;
            f32 min_dist = 100.0f;
            
            for (u32 i = 0; i < game->npc_count; i++) {
                f32 dx = game->npcs[i].x - game->player_x;
                f32 dy = game->npcs[i].y - game->player_y;
                f32 dist_sq = dx*dx + dy*dy;
                
                if (dist_sq < min_dist * min_dist) {
                    min_dist = sqrtf(dist_sq);
                    nearest = &game->npcs[i];
                }
            }
            
            if (nearest) {
                game->dialog_active = 1;
                game->dialog_timer = 6.0f;
                game->dialog_npc_id = nearest - game->npcs;
                generate_emergent_dialog(nearest, game);
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
    game->day_time += dt * 0.05f;  // 1 game hour = 20 real seconds
    if (game->day_time >= 24.0f) {
        game->day_time -= 24.0f;
        // New day - NPCs reflect
        for (int i = 0; i < game->npc_count; i++) {
            add_experience(&game->npcs[i], EXP_MUNDANE, "Another day passes", 0.1f, game);
        }
    }
    
    // Update player
    f32 speed = 300.0f;
    if (game->keys_held[0]) game->player_vy -= speed * dt;
    if (game->keys_held[1]) game->player_vx -= speed * dt;
    if (game->keys_held[2]) game->player_vy += speed * dt;
    if (game->keys_held[3]) game->player_vx += speed * dt;
    
    game->player_vx *= 0.9f;
    game->player_vy *= 0.9f;
    
    game->player_x += game->player_vx * dt;
    game->player_y += game->player_vy * dt;
    
    if (game->player_x < 16) game->player_x = 16;
    if (game->player_x > 1008) game->player_x = 1008;
    if (game->player_y < 16) game->player_y = 16;
    if (game->player_y > 752) game->player_y = 752;
    
    // Update NPCs
    for (int i = 0; i < game->npc_count; i++) {
        update_npc_mind(&game->npcs[i], dt, game);
        update_npc_physics(&game->npcs[i], dt);
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
    // Only clear background once at startup or when needed
    // Don't clear entire screen every frame to reduce flicker
    static int first_render = 1;
    if (first_render) {
        XSetForeground(game->display, game->gc, 0x000000);
        XFillRectangle(game->display, game->window, game->gc, 0, 0, 1024, 768);
        first_render = 0;
    }
    
    int cam_x = (int)(game->player_x - 512);
    int cam_y = (int)(game->player_y - 384);
    
    // Optimized world rendering - batch similar colors
    u32 current_color = 0xFFFFFFFF;  // Invalid color to force first set
    
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            int screen_x = x * 8 - cam_x;
            int screen_y = y * 8 - cam_y;
            
            // Skip if off screen
            if (screen_x < -8 || screen_x > 1024 || screen_y < -8 || screen_y > 768) continue;
            
            // Determine color
            u32 color = nes_palette[0x1A];  // Default grass
            if (game->world[y][x] == 2) color = nes_palette[0x18];  // Tree
            else if (game->world[y][x] == 3) color = nes_palette[0x24];  // Flower
            else if (game->world[y][x] == 4) color = nes_palette[0x16];  // House
            else if (game->world[y][x] == 5) color = nes_palette[0x00];  // Stone
            
            // Only change color if different (reduces X11 calls)
            if (color != current_color) {
                XSetForeground(game->display, game->gc, color);
                current_color = color;
            }
            
            XFillRectangle(game->display, game->window, game->gc, screen_x, screen_y, 8, 8);
        }
    }
    
    // Draw NPCs with emotion indicators
    for (int i = 0; i < game->npc_count; i++) {
        npc* n = &game->npcs[i];
        int screen_x = (int)(n->x - cam_x);
        int screen_y = (int)(n->y - cam_y);
        
        if (screen_x < -16 || screen_x > 1024 || screen_y < -16 || screen_y > 768) continue;
        
        // Body color based on dominant emotion
        u32 npc_color = nes_palette[n->color];
        if (n->emotions[EMO_ANGRY] > 0.6f) npc_color = nes_palette[0x16];  // Red
        if (n->emotions[EMO_SAD] > 0.6f) npc_color = nes_palette[0x2C];    // Blue
        if (n->emotions[EMO_HAPPY] > 0.7f) npc_color = nes_palette[0x2A];  // Yellow
        if (n->emotions[EMO_AFRAID] > 0.6f) npc_color = nes_palette[0x13]; // Purple
        
        XSetForeground(game->display, game->gc, npc_color);
        XFillRectangle(game->display, game->window, game->gc, screen_x - 8, screen_y - 8, 16, 16);
        
        // Show name if close
        f32 dx = n->x - game->player_x;
        f32 dy = n->y - game->player_y;
        if (dx*dx + dy*dy < 10000) {  // 100 pixel radius
            XSetForeground(game->display, game->gc, nes_palette[0x30]);
            XFillRectangle(game->display, game->window, game->gc, screen_x - 2, screen_y - 25, 4, 10);
        }
    }
    
    // Draw player
    XSetForeground(game->display, game->gc, nes_palette[0x11]);
    XFillRectangle(game->display, game->window, game->gc, 504, 376, 16, 16);
    
    // Draw dialog
    if (game->dialog_active) {
        XSetForeground(game->display, game->gc, nes_palette[0x0F]);
        XFillRectangle(game->display, game->window, game->gc, 30, 520, 964, 180);
        
        XSetForeground(game->display, game->gc, nes_palette[0x30]);
        XDrawRectangle(game->display, game->window, game->gc, 30, 520, 964, 180);
        
        // Word wrap
        int max_chars = 38;
        int pos = 0;
        int line = 0;
        int len = strlen(game->dialog_text);
        
        while (pos < len && line < 5) {
            char line_text[128];
            int line_end = pos + max_chars;
            if (line_end > len) line_end = len;
            
            if (line_end < len) {
                for (int i = line_end; i > pos; i--) {
                    if (game->dialog_text[i] == ' ') {
                        line_end = i;
                        break;
                    }
                }
            }
            
            int line_len = line_end - pos;
            if (line_len > 127) line_len = 127;
            strncpy(line_text, game->dialog_text + pos, line_len);
            line_text[line_len] = '\0';
            
            draw_text(game, 50, 540 + line * 30, line_text, nes_palette[0x30]);
            
            pos = line_end + 1;
            line++;
        }
    }
    
    // Draw debug
    if (game->show_debug) {
        // Calculate proper background size
        int debug_height = 60 + game->npc_count * 60;  // More space per NPC
        
        // Draw solid black background
        XSetForeground(game->display, game->gc, 0x000000);
        XFillRectangle(game->display, game->window, game->gc, 5, 50, 700, debug_height);
        
        // Draw border
        XSetForeground(game->display, game->gc, nes_palette[0x30]);
        XDrawRectangle(game->display, game->window, game->gc, 5, 50, 700, debug_height);
        
        // Title
        draw_text(game, 15, 60, "EMERGENT PERSONALITIES", nes_palette[0x25]);
        
        for (int i = 0; i < game->npc_count; i++) {
            npc* n = &game->npcs[i];
            
            // Find dominant emotion
            const char* emo_names[] = {"Happy", "Sad", "Angry", "Afraid", "Surprised", "Disgusted", "Curious", "Lonely"};
            f32 max_emo = 0;
            int max_idx = 0;
            for (int e = 0; e < EMO_COUNT; e++) {
                if (n->emotions[e] > max_emo) {
                    max_emo = n->emotions[e];
                    max_idx = e;
                }
            }
            
            int y_pos = 90 + i * 60;  // 60 pixels per NPC (more space)
            
            // NPC name and emotion
            char debug[256];
            snprintf(debug, 255, "%s: %s (%.0f%%)", 
                    n->name, emo_names[max_idx], max_emo * 100);
            draw_text(game, 15, y_pos, debug, nes_palette[0x30]);
            
            // Show current thought (truncated for display)
            char thought[35];  // Shorter to fit better
            strncpy(thought, n->current_thought, 34);
            thought[34] = '\0';
            if (strlen(n->current_thought) > 34) {
                thought[31] = '.';
                thought[32] = '.';
                thought[33] = '.';
            }
            draw_text(game, 15, y_pos + 30, thought, nes_palette[0x1C]);
        }
    }
    
    // Inventory display
    char inv[128];
    snprintf(inv, 127, "Flowers:%d Stones:%d", game->flowers_collected, game->stones_collected);
    draw_text(game, 10, 10, inv, nes_palette[0x30]);
    
    // Time and controls
    char info[128];
    snprintf(info, 127, "Time:%02d:%02d SPACE:Gather ENTER:Talk TAB:Debug", 
            (int)game->day_time, (int)((game->day_time - (int)game->day_time) * 60));
    draw_text(game, 10, 740, info, nes_palette[0x30]);
    
    // Use XSync instead of XFlush for better timing
    XSync(game->display, False);
}

int main() {
    printf("\n=== NEURAL VILLAGE - EMERGENT PERSONALITIES ===\n");
    printf("NPCs have backstories, not scripts!\n");
    printf("They express themselves based on:\n");
    printf("• Their unique history\n");
    printf("• Current emotional state\n");
    printf("• Relationship with you\n");
    printf("• Recent experiences\n\n");
    printf("Every conversation is unique!\n\n");
    
    srand(time(NULL));
    
    game_state* game = malloc(sizeof(game_state));
    if (!game) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    init_game(game);
    
    // X11 setup
    game->display = XOpenDisplay(NULL);
    if (!game->display) {
        printf("Cannot open display\n");
        free(game);
        return 1;
    }
    
    game->screen = DefaultScreen(game->display);
    game->window = XCreateSimpleWindow(game->display,
                                      RootWindow(game->display, game->screen),
                                      100, 100, 1024, 768, 1,
                                      BlackPixel(game->display, game->screen),
                                      WhitePixel(game->display, game->screen));
    
    XStoreName(game->display, game->window, "Neural Village - Emergent Personalities");
    XSelectInput(game->display, game->window, ExposureMask | KeyPressMask | KeyReleaseMask);
    XMapWindow(game->display, game->window);
    
    game->gc = XCreateGC(game->display, game->window, 0, NULL);
    if (!game->gc) {
        printf("Failed to create GC\n");
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
                // Don't render on expose - let main loop handle it
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
        
        // Better frame timing - target 60 FPS but don't force it
        struct timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 16666667;  // 16.67ms = 60 FPS
        nanosleep(&sleep_time, NULL);
    }
    
    // Cleanup
    printf("\nEmergent expressions saved to: emergent_expressions.log\n");
    printf("Every NPC expressed themselves uniquely!\n");
    
    if (game->expression_log) fclose(game->expression_log);
    
    XFreeGC(game->display, game->gc);
    XDestroyWindow(game->display, game->window);
    XCloseDisplay(game->display);
    
    free(game);
    
    return 0;
}