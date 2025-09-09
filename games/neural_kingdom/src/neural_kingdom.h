// neural_kingdom.h - The game that will shatter AI standards
// ZERO dependencies, MAXIMUM performance, REVOLUTIONARY AI

#ifndef NEURAL_KINGDOM_H
#define NEURAL_KINGDOM_H

#include "../../../game/game_types.h"

// ============================================================================
// PERFORMANCE TARGETS - WE WILL HIT THESE!
// ============================================================================
#define TARGET_FPS 144
#define MAX_FRAME_TIME_MS (1000.0f / TARGET_FPS)
#define MAX_MEMORY_MB 100
#define MAX_NEURAL_NPCS 100
#define NEURAL_UPDATE_HZ 60

// ============================================================================
// NEURAL NPC - THE HEART OF OUR REVOLUTION
// ============================================================================

typedef struct npc_memory_entry {
    u64 timestamp;
    u32 event_type;
    u32 entity_id;
    v2 location;
    f32 emotional_impact;  // -1 (traumatic) to +1 (joyful)
    char context[64];       // What happened
} npc_memory_entry;

typedef struct npc_personality {
    f32 aggression;         // 0 = pacifist, 1 = warmonger
    f32 curiosity;          // 0 = cautious, 1 = explorer
    f32 loyalty;            // 0 = betrayer, 1 = devoted
    f32 intelligence;       // 0 = simple, 1 = genius
    f32 empathy;           // 0 = psychopath, 1 = saint
} npc_personality;

typedef struct npc_goals {
    enum {
        GOAL_NONE,
        GOAL_SURVIVE,
        GOAL_EXPLORE,
        GOAL_SOCIALIZE,
        GOAL_ACCUMULATE_WEALTH,
        GOAL_GAIN_POWER,
        GOAL_SEEK_KNOWLEDGE,
        GOAL_FIND_LOVE,
        GOAL_REVENGE,
        GOAL_PROTECT_SOMEONE
    } primary;
    
    u32 target_entity_id;   // Who/what is involved
    f32 urgency;            // 0 = whenever, 1 = NOW!
    f32 progress;           // 0 = just started, 1 = complete
} npc_goals;

typedef struct npc_relationship {
    u32 entity_id;
    f32 trust;              // -1 = enemy, +1 = best friend
    f32 fear;               // 0 = not scary, 1 = terrifying
    f32 respect;            // 0 = worthless, 1 = hero
    f32 affection;          // -1 = hate, +1 = love
    u32 interaction_count;
    u64 last_interaction;
} npc_relationship;

typedef struct neural_npc {
    // Identity
    u32 id;
    char name[32];
    npc_personality personality;
    
    // Physical state
    v2 position;
    v2 velocity;
    f32 health;
    f32 stamina;
    f32 fear_level;
    f32 anger_level;
    f32 happiness_level;
    
    // Neural brain - THE MAGIC!
    struct {
        neural_network* network;        // Decision making
        f32 learning_rate;               // How fast we adapt
        
        // DNC memory system
        npc_memory_entry memories[1000]; // Long-term memory
        u32 memory_count;
        
        // Working memory
        f32 short_term_memory[32];       // Current situation
        f32 attention_weights[32];       // What we're focusing on
    } brain;
    
    // Relationships - NPCs remember EVERYONE
    npc_relationship relationships[MAX_NEURAL_NPCS];
    u32 relationship_count;
    
    // Current behavior
    npc_goals current_goal;
    enum {
        STATE_IDLE,
        STATE_WALKING,
        STATE_TALKING,
        STATE_FIGHTING,
        STATE_FLEEING,
        STATE_LEARNING,
        STATE_PLANNING
    } state;
    
    // Learning from player
    struct {
        f32 player_attack_patterns[16];  // Recognized patterns
        f32 player_movement_habits[16];  // How they move
        f32 player_dialogue_style[8];    // How they talk
        u32 player_death_count;          // Times killed player
        u32 death_by_player_count;       // Times killed by player
    } player_model;
    
    // Emergent storytelling
    bool has_quest_for_player;
    char quest_description[256];
    f32 quest_importance;
    
    // Performance metrics
    u64 last_think_time;
    f32 think_time_ms;
} neural_npc;

// ============================================================================
// WORLD STATE - LIVING, BREATHING, LEARNING
// ============================================================================

typedef struct neural_kingdom_state {
    // NPCs - Our living world
    neural_npc npcs[MAX_NEURAL_NPCS];
    u32 npc_count;
    
    // Player
    struct {
        v2 position;
        v2 velocity;
        f32 health;
        f32 stamina;
        
        // Player's reputation in the world
        f32 reputation_good_evil;    // -1 = evil, +1 = good
        f32 reputation_weak_strong;  // -1 = weak, +1 = strong  
        f32 reputation_stupid_smart; // -1 = stupid, +1 = smart
        
        u32 npcs_killed;
        u32 npcs_helped;
        u32 quests_completed;
    } player;
    
    // World simulation
    u64 world_time;
    f32 time_of_day;  // 0 = midnight, 0.5 = noon, 1 = midnight
    
    // Performance tracking
    f32 frame_ms;
    f32 ai_update_ms;
    f32 physics_ms;
    f32 render_ms;
    u64 total_memory_used;
    
    // Debug visualization
    bool show_neural_activity;
    bool show_npc_memories;
    bool show_relationships;
    bool show_goal_planning;
} neural_kingdom_state;

// ============================================================================
// CORE SYSTEMS
// ============================================================================

// Initialization
void neural_kingdom_init(neural_kingdom_state* game);

// The Holy Trinity of Game Development
void neural_kingdom_update(neural_kingdom_state* game, input_state* input, f32 dt);
void neural_kingdom_render(neural_kingdom_state* game);
void neural_kingdom_audio(neural_kingdom_state* game, f32* buffer, u32 samples);

// Neural NPC Systems - WHERE THE MAGIC HAPPENS
void npc_init(neural_npc* npc, const char* name, npc_personality personality);
void npc_think(neural_npc* npc, neural_kingdom_state* game, f32 dt);
void npc_learn_from_interaction(neural_npc* npc, u32 other_id, f32 outcome);
void npc_form_memory(neural_npc* npc, u32 event_type, const char* context);
void npc_update_relationships(neural_npc* npc, neural_kingdom_state* game);
void npc_generate_emergent_quest(neural_npc* npc, neural_kingdom_state* game);

// Performance Monitoring - WE WILL BE FASTEST
void perf_begin_timer(const char* name);
void perf_end_timer(const char* name);
void perf_report(neural_kingdom_state* game);

// Editor Integration
void editor_show_npc_brain(neural_npc* npc);
void editor_show_memory_timeline(neural_npc* npc);
void editor_show_relationship_graph(neural_kingdom_state* game);

// ============================================================================
// THE REVOLUTION STARTS HERE
// ============================================================================

#define NK_ASSERT(x) do { if(!(x)) { *(int*)0 = 0; } } while(0)
#define NK_PROFILE(name) perf_begin_timer(name); defer { perf_end_timer(name); }

// This game will prove:
// 1. Handmade > Unity/Unreal
// 2. Real AI > Scripted NPCs  
// 3. Performance AND quality are possible
// 4. One developer can outdo AAA teams

#endif // NEURAL_KINGDOM_H