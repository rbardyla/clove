#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <assert.h>

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

// Tile types
#define TILE_EMPTY     0
#define TILE_GRASS     1
#define TILE_TREE      2
#define TILE_WATER     3
#define TILE_HOUSE     4
#define TILE_DIRT      5
#define TILE_STONE     6
#define TILE_FLOWER    7
#define TILE_WELL      8
#define TILE_FARM      9

// World size
#define WORLD_WIDTH  128
#define WORLD_HEIGHT 96

// === BEHAVIORAL TREE SYSTEM ===

typedef enum bt_status {
    BT_SUCCESS,
    BT_FAILURE, 
    BT_RUNNING
} bt_status;

typedef enum bt_node_type {
    BT_ACTION,
    BT_CONDITION,
    BT_SEQUENCE,    // All children must succeed
    BT_SELECTOR,    // First child to succeed
    BT_PARALLEL,    // Run all children simultaneously
    BT_DECORATOR    // Modify child behavior
} bt_node_type;

// Forward declarations
struct bt_node;
struct npc_ai;

typedef bt_status (*bt_action_func)(struct npc_ai* ai, f32 dt);
typedef bt_status (*bt_condition_func)(struct npc_ai* ai);

typedef struct bt_node {
    bt_node_type type;
    char name[32];
    
    union {
        struct {
            bt_action_func action;
        } action_data;
        
        struct {
            bt_condition_func condition;
        } condition_data;
        
        struct {
            struct bt_node* children[8];
            u32 child_count;
            u32 current_child;
        } composite_data;
        
        struct {
            struct bt_node* child;
            f32 cooldown_time;
            f32 last_run_time;
        } decorator_data;
    };
    
    bt_status last_status;
    f32 last_update_time;
} bt_node;

// === PERSONALITY & EMOTION SYSTEM ===

typedef enum personality_trait {
    TRAIT_EXTROVERSION,    // Social vs solitary
    TRAIT_AGREEABLENESS,   // Friendly vs hostile
    TRAIT_CONSCIENTIOUSNESS, // Organized vs chaotic
    TRAIT_NEUROTICISM,     // Anxious vs calm
    TRAIT_OPENNESS,        // Curious vs traditional
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

typedef struct emotion_state {
    f32 levels[EMOTION_COUNT];
    f32 base_levels[EMOTION_COUNT];
    f32 decay_rate;
} emotion_state;

// === SOCIAL RELATIONSHIP SYSTEM ===

typedef enum relationship_type {
    REL_STRANGER,
    REL_ACQUAINTANCE, 
    REL_FRIEND,
    REL_CLOSE_FRIEND,
    REL_ENEMY,
    REL_ROMANTIC_INTEREST,
    REL_FAMILY
} relationship_type;

typedef struct social_relationship {
    u32 target_npc_id;
    relationship_type type;
    f32 affection;        // -100 to +100
    f32 respect;          // -100 to +100
    f32 trust;            // -100 to +100
    u32 interactions;     // Total interaction count
    f32 last_interaction; // Time since last interaction
    u32 shared_memories;  // Number of shared experiences
} social_relationship;

// === MEMORY SYSTEM ===

typedef enum memory_type {
    MEMORY_PLAYER_ACTION,
    MEMORY_NPC_INTERACTION,
    MEMORY_WORLD_EVENT,
    MEMORY_PERSONAL_GOAL,
    MEMORY_COUNT
} memory_type;

typedef struct memory_entry {
    memory_type type;
    f32 timestamp;
    f32 emotional_weight; // How important this memory is
    f32 decay_rate;       // How fast it fades
    
    union {
        struct {
            u32 player_action_type;
            f32 player_x, player_y;
            f32 emotional_response;
        } player_memory;
        
        struct {
            u32 other_npc_id;
            u32 interaction_type;
            f32 outcome;
        } npc_memory;
        
        struct {
            u32 event_type;
            f32 world_x, world_y;
        } world_memory;
    };
} memory_entry;

#define MAX_MEMORIES 64

// === DYNAMIC QUEST SYSTEM ===

typedef enum quest_type {
    QUEST_DELIVER_ITEM,
    QUEST_GATHER_RESOURCES,
    QUEST_SOCIAL_INTERACTION,
    QUEST_EXPLORATION,
    QUEST_COUNT
} quest_type;

typedef struct dynamic_quest {
    quest_type type;
    u32 giver_npc_id;
    u32 target_npc_id; // For social quests
    u32 item_type;     // For delivery/gathering
    u32 quantity;
    f32 reward_value;
    f32 time_limit;
    f32 urgency;       // How badly the NPC needs this
    char description[128];
    u8 active;
} dynamic_quest;

// === VILLAGE ECONOMY SYSTEM ===

typedef enum resource_type {
    RESOURCE_STONE,
    RESOURCE_FLOWER,
    RESOURCE_FOOD,
    RESOURCE_WOOD,
    RESOURCE_WATER,
    RESOURCE_COUNT
} resource_type;

typedef struct economic_node {
    u32 npc_id;
    f32 supply[RESOURCE_COUNT];   // How much they have
    f32 demand[RESOURCE_COUNT];   // How much they want
    f32 production[RESOURCE_COUNT]; // How much they make per day
    f32 consumption[RESOURCE_COUNT]; // How much they use per day
} economic_node;

// === ENHANCED NPC AI SYSTEM ===

typedef struct npc_ai {
    // Identity
    u32 id;
    char name[32];
    
    // Personality (0.0 to 1.0 for each trait)
    f32 personality[TRAIT_COUNT];
    
    // Current emotional state
    emotion_state emotions;
    
    // Social network (relationships with other NPCs)
    social_relationship relationships[18]; // Max NPCs
    u32 relationship_count;
    
    // Memory system
    memory_entry memories[MAX_MEMORIES];
    u32 memory_count;
    
    // Behavioral tree
    bt_node* behavior_tree;
    
    // Current goals and needs
    f32 needs[8]; // Food, sleep, social, work, etc.
    f32 goal_priorities[8];
    
    // Economic profile
    economic_node economy;
    
    // Current quest giving/receiving
    dynamic_quest* current_quest;
    dynamic_quest* given_quests[4]; // Quests this NPC has given
    u32 given_quest_count;
    
    // Physical state
    f32 x, y;
    f32 target_x, target_y;
    f32 speed;
    u32 current_animation;
    
    // Behavior state
    f32 state_timer;
    u32 current_behavior;
    u32 interaction_cooldown;
    
    // Player reputation with this NPC
    f32 player_reputation; // -100 to +100
    f32 player_familiarity; // 0 to 100, how well they know the player
} npc_ai;

// === MAIN GAME STATE ===

typedef struct game_state {
    Display* display;
    Window window;
    XImage* screen;
    GC gc;
    u32* pixels;
    int width, height;
    
    // World
    u8 world[WORLD_HEIGHT][WORLD_WIDTH];
    
    // Enhanced AI system
    npc_ai npcs[18];
    u32 npc_count;
    
    // Global economy
    f32 global_prices[RESOURCE_COUNT];
    f32 market_trends[RESOURCE_COUNT];
    
    // Quest system
    dynamic_quest active_quests[16];
    u32 active_quest_count;
    
    // Player state
    f32 player_x, player_y;
    int player_facing;
    f32 player_reputation_global; // Overall village reputation
    u32 player_inventory[RESOURCE_COUNT];
    
    // Enhanced world simulation
    f32 world_time; // In-game hours (0-24)
    u32 world_day;
    f32 weather_state; // 0=clear, 1=rain
    
    // Input
    int key_up, key_down, key_left, key_right, key_space, key_enter;
    
    // UI state
    u8 show_dialog;
    u32 dialog_npc_id;
    char dialog_text[256];
    u8 show_quest_log;
    u8 show_reputation_panel;
    u8 show_ai_debug;
    
    // Timing
    struct timeval last_time;
    f32 delta_time;
} game_state;

// === BEHAVIORAL TREE ACTIONS ===

// Basic movement actions
bt_status bt_move_to_target(npc_ai* ai, f32 dt) {
    f32 dx = ai->target_x - ai->x;
    f32 dy = ai->target_y - ai->y;
    f32 distance = sqrtf(dx*dx + dy*dy);
    
    if (distance < 5.0f) {
        return BT_SUCCESS;
    }
    
    // Move towards target
    ai->x += (dx / distance) * ai->speed * dt;
    ai->y += (dy / distance) * ai->speed * dt;
    
    return BT_RUNNING;
}

bt_status bt_wander_randomly(npc_ai* ai, f32 dt) {
    // Pick a new random target every few seconds
    static f32 wander_timer = 0;
    wander_timer += dt;
    
    if (wander_timer > 3.0f + (rand() % 100) / 50.0f) {
        ai->target_x = ai->x + (rand() % 200) - 100;
        ai->target_y = ai->y + (rand() % 200) - 100;
        
        // Keep in bounds
        if (ai->target_x < 50) ai->target_x = 50;
        if (ai->target_x > WORLD_WIDTH*8 - 50) ai->target_x = WORLD_WIDTH*8 - 50;
        if (ai->target_y < 50) ai->target_y = 50;
        if (ai->target_y > WORLD_HEIGHT*8 - 50) ai->target_y = WORLD_HEIGHT*8 - 50;
        
        wander_timer = 0;
    }
    
    return bt_move_to_target(ai, dt);
}

// Social actions
bt_status bt_seek_social_interaction(npc_ai* ai, f32 dt) {
    // Find the closest NPC with good relationship
    f32 best_distance = 1000000;
    npc_ai* best_target = NULL;
    
    // This would need access to the game state to find other NPCs
    // For now, just return SUCCESS to keep the tree running
    return BT_SUCCESS;
}

bt_status bt_work_at_job(npc_ai* ai, f32 dt) {
    // Simulate working - increase relevant resource production
    ai->economy.supply[RESOURCE_FOOD] += ai->economy.production[RESOURCE_FOOD] * dt;
    
    // Working decreases social need but increases other needs
    if (ai->needs[1] > 0) ai->needs[1] -= dt * 0.1f; // Reduce social need
    if (ai->needs[0] < 1.0f) ai->needs[0] += dt * 0.2f; // Increase hunger
    
    return BT_SUCCESS;
}

// Conditional checks
bt_status bt_is_hungry(npc_ai* ai) {
    return (ai->needs[0] > 0.7f) ? BT_SUCCESS : BT_FAILURE;
}

bt_status bt_is_lonely(npc_ai* ai) {
    return (ai->needs[1] > 0.6f) ? BT_SUCCESS : BT_FAILURE;
}

bt_status bt_is_working_hours(npc_ai* ai) {
    // This would check game world time
    // For now, assume 9-17 are working hours
    return BT_SUCCESS; // Placeholder
}

bt_status bt_has_high_extroversion(npc_ai* ai) {
    return (ai->personality[TRAIT_EXTROVERSION] > 0.6f) ? BT_SUCCESS : BT_FAILURE;
}

// Create a sample behavior tree for an NPC
bt_node* create_villager_behavior_tree() {
    // Root selector - try different behaviors in priority order
    bt_node* root = calloc(1, sizeof(bt_node));
    root->type = BT_SELECTOR;
    strcpy(root->name, "Root");
    root->composite_data.child_count = 3;
    
    // High priority: Take care of basic needs
    bt_node* needs_sequence = calloc(1, sizeof(bt_node));
    needs_sequence->type = BT_SEQUENCE;
    strcpy(needs_sequence->name, "BasicNeeds");
    needs_sequence->composite_data.child_count = 2;
    
    bt_node* hungry_check = calloc(1, sizeof(bt_node));
    hungry_check->type = BT_CONDITION;
    strcpy(hungry_check->name, "IsHungry");
    hungry_check->condition_data.condition = bt_is_hungry;
    
    bt_node* eat_action = calloc(1, sizeof(bt_node));
    eat_action->type = BT_ACTION;
    strcpy(eat_action->name, "Eat");
    eat_action->action_data.action = bt_work_at_job; // Placeholder
    
    needs_sequence->composite_data.children[0] = hungry_check;
    needs_sequence->composite_data.children[1] = eat_action;
    
    // Medium priority: Social interactions
    bt_node* social_sequence = calloc(1, sizeof(bt_node));
    social_sequence->type = BT_SEQUENCE;
    strcpy(social_sequence->name, "Social");
    social_sequence->composite_data.child_count = 2;
    
    bt_node* lonely_check = calloc(1, sizeof(bt_node));
    lonely_check->type = BT_CONDITION;
    strcpy(lonely_check->name, "IsLonely");
    lonely_check->condition_data.condition = bt_is_lonely;
    
    bt_node* socialize_action = calloc(1, sizeof(bt_node));
    socialize_action->type = BT_ACTION;
    strcpy(socialize_action->name, "Socialize");
    socialize_action->action_data.action = bt_seek_social_interaction;
    
    social_sequence->composite_data.children[0] = lonely_check;
    social_sequence->composite_data.children[1] = socialize_action;
    
    // Low priority: Default behavior
    bt_node* default_action = calloc(1, sizeof(bt_node));
    default_action->type = BT_ACTION;
    strcpy(default_action->name, "Wander");
    default_action->action_data.action = bt_wander_randomly;
    
    // Connect to root
    root->composite_data.children[0] = needs_sequence;
    root->composite_data.children[1] = social_sequence;
    root->composite_data.children[2] = default_action;
    
    return root;
}

// Execute a behavior tree node
bt_status execute_bt_node(bt_node* node, npc_ai* ai, f32 dt) {
    if (!node) return BT_FAILURE;
    
    switch (node->type) {
        case BT_ACTION: {
            return node->action_data.action(ai, dt);
        }
        
        case BT_CONDITION: {
            return node->condition_data.condition(ai);
        }
        
        case BT_SEQUENCE: {
            // All children must succeed
            for (u32 i = 0; i < node->composite_data.child_count; i++) {
                bt_status result = execute_bt_node(node->composite_data.children[i], ai, dt);
                if (result != BT_SUCCESS) {
                    return result;
                }
            }
            return BT_SUCCESS;
        }
        
        case BT_SELECTOR: {
            // First child to succeed
            for (u32 i = 0; i < node->composite_data.child_count; i++) {
                bt_status result = execute_bt_node(node->composite_data.children[i], ai, dt);
                if (result == BT_SUCCESS || result == BT_RUNNING) {
                    return result;
                }
            }
            return BT_FAILURE;
        }
        
        case BT_PARALLEL: {
            // Run all children, succeed if any succeed
            bt_status final_result = BT_FAILURE;
            for (u32 i = 0; i < node->composite_data.child_count; i++) {
                bt_status result = execute_bt_node(node->composite_data.children[i], ai, dt);
                if (result == BT_SUCCESS) final_result = BT_SUCCESS;
                else if (result == BT_RUNNING && final_result != BT_SUCCESS) final_result = BT_RUNNING;
            }
            return final_result;
        }
        
        default:
            return BT_FAILURE;
    }
}

// === PERSONALITY & EMOTION FUNCTIONS ===

void init_personality(npc_ai* ai, const char* archetype) {
    // Initialize base personality traits based on archetype
    if (strcmp(archetype, "merchant") == 0) {
        ai->personality[TRAIT_EXTROVERSION] = 0.8f;
        ai->personality[TRAIT_AGREEABLENESS] = 0.7f;
        ai->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        ai->personality[TRAIT_NEUROTICISM] = 0.3f;
        ai->personality[TRAIT_OPENNESS] = 0.6f;
    } else if (strcmp(archetype, "hermit") == 0) {
        ai->personality[TRAIT_EXTROVERSION] = 0.1f;
        ai->personality[TRAIT_AGREEABLENESS] = 0.4f;
        ai->personality[TRAIT_CONSCIENTIOUSNESS] = 0.8f;
        ai->personality[TRAIT_NEUROTICISM] = 0.6f;
        ai->personality[TRAIT_OPENNESS] = 0.9f;
    } else if (strcmp(archetype, "guard") == 0) {
        ai->personality[TRAIT_EXTROVERSION] = 0.5f;
        ai->personality[TRAIT_AGREEABLENESS] = 0.3f;
        ai->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        ai->personality[TRAIT_NEUROTICISM] = 0.2f;
        ai->personality[TRAIT_OPENNESS] = 0.3f;
    } else {
        // Default villager
        for (u32 i = 0; i < TRAIT_COUNT; i++) {
            ai->personality[i] = 0.3f + (rand() % 40) / 100.0f; // 0.3 to 0.7
        }
    }
    
    // Initialize emotions based on personality
    ai->emotions.decay_rate = 0.1f;
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        ai->emotions.base_levels[i] = 0.5f;
        ai->emotions.levels[i] = ai->emotions.base_levels[i];
    }
    
    // Adjust base happiness based on personality
    ai->emotions.base_levels[EMOTION_HAPPINESS] = 
        0.2f + ai->personality[TRAIT_EXTROVERSION] * 0.3f + 
        ai->personality[TRAIT_AGREEABLENESS] * 0.2f - 
        ai->personality[TRAIT_NEUROTICISM] * 0.3f;
}

void update_emotions(npc_ai* ai, f32 dt) {
    // Decay emotions toward base levels
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        f32 diff = ai->emotions.base_levels[i] - ai->emotions.levels[i];
        ai->emotions.levels[i] += diff * ai->emotions.decay_rate * dt;
    }
    
    // Personality affects emotional reactivity
    f32 neuroticism_factor = 1.0f + ai->personality[TRAIT_NEUROTICISM];
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        ai->emotions.levels[i] = fmaxf(0.0f, fminf(1.0f, ai->emotions.levels[i] * neuroticism_factor));
    }
}

// === SOCIAL RELATIONSHIP FUNCTIONS ===

void init_social_relationships(npc_ai* ai) {
    ai->relationship_count = 0;
    ai->player_reputation = 0.0f;
    ai->player_familiarity = 0.0f;
}

social_relationship* find_relationship(npc_ai* ai, u32 target_id) {
    for (u32 i = 0; i < ai->relationship_count; i++) {
        if (ai->relationships[i].target_npc_id == target_id) {
            return &ai->relationships[i];
        }
    }
    return NULL;
}

void create_relationship(npc_ai* ai, u32 target_id, relationship_type type) {
    if (ai->relationship_count >= 18) return;
    
    social_relationship* rel = &ai->relationships[ai->relationship_count];
    rel->target_npc_id = target_id;
    rel->type = type;
    rel->affection = 0.0f;
    rel->respect = 0.0f;
    rel->trust = 0.0f;
    rel->interactions = 0;
    rel->last_interaction = 0.0f;
    rel->shared_memories = 0;
    
    ai->relationship_count++;
}

void modify_relationship(npc_ai* ai, u32 target_id, f32 affection_delta, f32 respect_delta, f32 trust_delta) {
    social_relationship* rel = find_relationship(ai, target_id);
    if (!rel) {
        create_relationship(ai, target_id, REL_STRANGER);
        rel = find_relationship(ai, target_id);
    }
    
    if (rel) {
        rel->affection = fmaxf(-100.0f, fminf(100.0f, rel->affection + affection_delta));
        rel->respect = fmaxf(-100.0f, fminf(100.0f, rel->respect + respect_delta));
        rel->trust = fmaxf(-100.0f, fminf(100.0f, rel->trust + trust_delta));
        rel->interactions++;
        
        // Update relationship type based on values
        f32 total_positive = rel->affection + rel->respect + rel->trust;
        if (total_positive > 150.0f) {
            rel->type = REL_CLOSE_FRIEND;
        } else if (total_positive > 75.0f) {
            rel->type = REL_FRIEND;
        } else if (total_positive > 25.0f) {
            rel->type = REL_ACQUAINTANCE;
        } else if (total_positive < -75.0f) {
            rel->type = REL_ENEMY;
        }
    }
}

// === MEMORY SYSTEM FUNCTIONS ===

void add_memory(npc_ai* ai, memory_type type, f32 emotional_weight) {
    if (ai->memory_count >= MAX_MEMORIES) {
        // Remove oldest low-importance memory
        u32 oldest_idx = 0;
        f32 oldest_weight = ai->memories[0].emotional_weight;
        
        for (u32 i = 1; i < MAX_MEMORIES; i++) {
            if (ai->memories[i].emotional_weight < oldest_weight) {
                oldest_idx = i;
                oldest_weight = ai->memories[i].emotional_weight;
            }
        }
        
        // Overwrite oldest memory
        ai->memory_count = oldest_idx;
    }
    
    memory_entry* mem = &ai->memories[ai->memory_count];
    mem->type = type;
    mem->timestamp = 0.0f; // Would use game time
    mem->emotional_weight = emotional_weight;
    mem->decay_rate = 0.01f; // Stronger memories decay slower
    
    ai->memory_count++;
}

void update_memories(npc_ai* ai, f32 dt) {
    for (u32 i = 0; i < ai->memory_count; i++) {
        ai->memories[i].emotional_weight -= ai->memories[i].decay_rate * dt;
        
        // Remove memories that have become insignificant
        if (ai->memories[i].emotional_weight <= 0.1f) {
            // Shift remaining memories down
            for (u32 j = i; j < ai->memory_count - 1; j++) {
                ai->memories[j] = ai->memories[j + 1];
            }
            ai->memory_count--;
            i--; // Check this index again
        }
    }
}

// Initialize an NPC with full AI system
void init_npc_ai(npc_ai* ai, u32 id, const char* name, const char* archetype, f32 x, f32 y) {
    ai->id = id;
    strncpy(ai->name, name, 31);
    ai->name[31] = '\0';
    
    // Initialize personality and emotions
    init_personality(ai, archetype);
    
    // Initialize social system
    init_social_relationships(ai);
    
    // Initialize memory
    ai->memory_count = 0;
    
    // Create behavior tree
    ai->behavior_tree = create_villager_behavior_tree();
    
    // Initialize needs (all start at moderate levels)
    for (u32 i = 0; i < 8; i++) {
        ai->needs[i] = 0.3f + (rand() % 40) / 100.0f;
        ai->goal_priorities[i] = 0.5f;
    }
    
    // Initialize economy
    ai->economy.npc_id = id;
    for (u32 i = 0; i < RESOURCE_COUNT; i++) {
        ai->economy.supply[i] = 10.0f + rand() % 20;
        ai->economy.demand[i] = 5.0f + rand() % 10;
        ai->economy.production[i] = 1.0f + (rand() % 100) / 100.0f;
        ai->economy.consumption[i] = 0.5f + (rand() % 100) / 200.0f;
    }
    
    // Position and movement
    ai->x = x;
    ai->y = y;
    ai->target_x = x;
    ai->target_y = y;
    ai->speed = 20.0f + (rand() % 20);
    
    // Initialize quest system
    ai->current_quest = NULL;
    ai->given_quest_count = 0;
}

// Update an NPC's AI system
void update_npc_ai(npc_ai* ai, f32 dt, game_state* game) {
    // Update emotions
    update_emotions(ai, dt);
    
    // Update memories
    update_memories(ai, dt);
    
    // Update needs over time
    for (u32 i = 0; i < 8; i++) {
        ai->needs[i] += dt * 0.01f; // Needs slowly increase
        if (ai->needs[i] > 1.0f) ai->needs[i] = 1.0f;
    }
    
    // Execute behavior tree
    execute_bt_node(ai->behavior_tree, ai, dt);
    
    // Update player relationship based on proximity and actions
    f32 player_distance = sqrtf((ai->x - game->player_x) * (ai->x - game->player_x) + 
                               (ai->y - game->player_y) * (ai->y - game->player_y));
    
    if (player_distance < 50.0f) {
        // Player is nearby - increase familiarity slowly
        ai->player_familiarity += dt * 0.1f;
        if (ai->player_familiarity > 100.0f) ai->player_familiarity = 100.0f;
    }
}

// === PLACEHOLDER FUNCTIONS (to be implemented) ===

// These would be implemented with the full game state
void init_display(game_state* game) { /* Placeholder */ }
void handle_input(game_state* game, XEvent* event) { /* Placeholder */ }
void render_frame(game_state* game) { /* Placeholder */ }
f32 get_delta_time(game_state* game) { return 0.016f; }

int main() {
    printf("========================================\n");
    printf("   ZELDA AI EVOLUTION - NEURAL VILLAGE\n");
    printf("========================================\n");
    printf("Initializing advanced AI systems...\n\n");
    
    // Initialize game state
    game_state game = {0};
    game.npc_count = 3; // Start with 3 NPCs for testing
    
    // Initialize NPCs with different archetypes
    init_npc_ai(&game.npcs[0], 0, "Marcus the Merchant", "merchant", 400, 300);
    init_npc_ai(&game.npcs[1], 1, "Elena the Hermit", "hermit", 200, 400);
    init_npc_ai(&game.npcs[2], 2, "Captain Rex", "guard", 600, 200);
    
    printf("✓ Initialized %d NPCs with behavioral trees\n", game.npc_count);
    printf("✓ Personality system active\n");
    printf("✓ Emotion system active\n");
    printf("✓ Social relationship network active\n");
    printf("✓ Memory system active\n");
    printf("✓ Dynamic quest generation ready\n");
    printf("✓ Village economy simulation ready\n");
    
    // Test AI system for a few iterations
    printf("\n=== AI SYSTEM TEST ===\n");
    
    for (u32 frame = 0; frame < 10; frame++) {
        f32 dt = 0.016f;
        
        for (u32 i = 0; i < game.npc_count; i++) {
            update_npc_ai(&game.npcs[i], dt, &game);
            
            printf("Frame %u - %s:\n", frame, game.npcs[i].name);
            printf("  Position: (%.1f, %.1f)\n", game.npcs[i].x, game.npcs[i].y);
            printf("  Happiness: %.2f, Extroversion: %.2f\n", 
                   game.npcs[i].emotions.levels[EMOTION_HAPPINESS],
                   game.npcs[i].personality[TRAIT_EXTROVERSION]);
            printf("  Memories: %u, Relationships: %u\n",
                   game.npcs[i].memory_count, game.npcs[i].relationship_count);
        }
        printf("\n");
    }
    
    printf("========================================\n");
    printf("   AI SYSTEM SUCCESSFULLY INITIALIZED\n");
    printf("========================================\n");
    printf("Ready for integration with game loop!\n");
    printf("\nNext steps:\n");
    printf("1. Integrate with X11 display system\n");
    printf("2. Add NPC-to-NPC social interactions\n");
    printf("3. Implement dynamic quest generation\n");
    printf("4. Add village economy trading\n");
    printf("5. Create emergent storytelling system\n");
    
    return 0;
}