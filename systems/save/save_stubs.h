/*
    Stub definitions for save system compilation
    These would be replaced by actual engine headers
*/

#ifndef SAVE_STUBS_H
#define SAVE_STUBS_H

#include "handmade_save.h"

// Game state structures (simplified)
typedef struct entity {
    u32 id;
    u32 type;
    f32 position[3];
    f32 rotation[4];
    f32 scale[3];
    u32 flags;
    u32 parent_id;
    char name[64];
    
    // Type-specific data
    union {
        struct {
            u32 health;
            u32 state;
            char dialogue_id[64];
        } npc_data;
        
        struct {
            u32 item_id;
            u32 quantity;
            f32 durability;
        } item_data;
        
        struct {
            u32 trigger_id;
            u8 activated;
            char script[256];
        } trigger_data;
    };
    
    void *npc_brain;  // Neural network pointer
} entity;

// Entity types
enum {
    ENTITY_TYPE_STATIC = 0,
    ENTITY_TYPE_NPC = 1,
    ENTITY_TYPE_ITEM = 2,
    ENTITY_TYPE_TRIGGER = 3,
    ENTITY_TYPE_PLAYER = 4,
};

// Player state
typedef struct player_state {
    char name[64];
    u32 level;
    u32 experience;
    u32 health;
    u32 max_health;
    u32 mana;
    u32 max_mana;
    u32 stamina;
    u32 max_stamina;
    
    f32 position[3];
    f32 rotation[2];
    
    u32 strength;
    u32 dexterity;
    u32 intelligence;
    u32 wisdom;
    
    struct {
        u32 item_id;
        u32 quantity;
        u32 slot;
        f32 durability;
    } inventory[100];
    u32 inventory_count;
    
    u32 equipment[10];
    
    struct {
        u32 quest_id;
        u32 stage;
        u32 flags;
    } quests[50];
    u32 quest_count;
} player_state;

#define EQUIPMENT_SLOT_COUNT 10

// Physics structures
typedef struct rigid_body {
    u32 entity_id;
    f32 mass;
    f32 position[3];
    f32 velocity[3];
    f32 angular_velocity[3];
    f32 inertia_tensor[9];
    u32 collision_shape;
    u8 is_static;
    u8 is_trigger;
} rigid_body;

typedef struct physics_constraint {
    u32 type;
    u32 body_a;
    u32 body_b;
    f32 anchor_a[3];
    f32 anchor_b[3];
    f32 stiffness;
    f32 damping;
} physics_constraint;

typedef struct physics_world {
    f32 gravity[3];
    f32 air_resistance;
    u32 simulation_rate;
    
    rigid_body bodies[1000];
    u32 body_count;
    
    physics_constraint constraints[100];
    u32 constraint_count;
    
    b32 paused;
} physics_world;

// Audio structures
typedef struct sound_instance {
    char name[64];
    u32 entity_id;
    f32 volume;
    f32 pitch;
    f32 position[3];
    u8 looping;
    u8 persistent;
    f32 play_position;
} sound_instance;

typedef struct reverb_zone {
    f32 position[3];
    f32 radius;
    f32 intensity;
    u32 preset;
} reverb_zone;

typedef struct audio_system {
    f32 master_volume;
    f32 music_volume;
    f32 sfx_volume;
    f32 voice_volume;
    
    char current_music[256];
    f32 music_position;
    u8 music_looping;
    
    sound_instance sounds[256];
    u32 active_sounds;
    
    reverb_zone reverb_zones[32];
    u32 reverb_zone_count;
    
    b32 paused;
} audio_system;

// Script structures
enum {
    SCRIPT_VAR_NUMBER = 0,
    SCRIPT_VAR_STRING = 1,
    SCRIPT_VAR_BOOL = 2,
    SCRIPT_VAR_ENTITY = 3,
};

typedef struct script_variable {
    char name[64];
    u32 type;
    union {
        f64 number;
        char string[256];
        u8 boolean;
        u32 entity_id;
    } value;
} script_variable;

typedef struct script_coroutine {
    char script_name[256];
    u32 instruction_pointer;
    f32 wait_time;
    u32 state;
    
    script_variable local_vars[64];
    u32 local_var_count;
} script_coroutine;

typedef struct script_system {
    script_variable global_vars[256];
    u32 global_var_count;
    
    script_coroutine coroutines[64];
    u32 coroutine_count;
    
    struct {
        char name[64];
        u8 value;
    } event_flags[256];
    u32 event_flag_count;
    
    b32 paused;
} script_system;

// Node system structures
typedef struct graph_node {
    u32 id;
    u32 type;
    f32 position[2];
    
    void *data;
    u32 data_size;
    
    struct {
        char name[32];
        u32 type;
    } inputs[8];
    u32 input_count;
    
    struct {
        char name[32];
        u32 type;
    } outputs[8];
    u32 output_count;
} graph_node;

typedef struct node_connection {
    u32 from_node;
    u32 from_output;
    u32 to_node;
    u32 to_input;
} node_connection;

typedef struct node_graph {
    char name[64];
    graph_node nodes[256];
    u32 node_count;
    
    node_connection connections[512];
    u32 connection_count;
} node_graph;

typedef struct node_system {
    node_graph graphs[32];
    u32 graph_count;
} node_system;

// Neural network structures
typedef struct memory_entry {
    f32 timestamp;
    u32 importance;
    char description[256];
    f32 embedding[128];
} memory_entry;

typedef struct npc_brain {
    u32 lstm_hidden_size;
    u32 memory_size;
    
    f32 *lstm_weights;
    u32 lstm_weights_size;
    
    f32 lstm_hidden[256];
    f32 lstm_cell[256];
    
    memory_entry memories[100];
    u32 memory_count;
    
    struct {
        f32 friendliness;
        f32 aggression;
        f32 curiosity;
        f32 fear;
    } traits;
    
    u32 current_goal;
    f32 emotional_state;
    
    struct {
        u32 entity_id;
        f32 affinity;
        f32 trust;
        u32 interaction_count;
    } relationships[50];
    u32 relationship_map_count;
} npc_brain;

// Game state
typedef struct game_state {
    entity entities[10000];
    u32 entity_count;
    
    player_state player;
    physics_world *physics;
    audio_system *audio;
    script_system *scripts;
    node_system *nodes;
    
    f32 playtime_seconds;
    char current_level[256];
    
    b32 paused;
    
    struct {
        u32 resolution_width;
        u32 resolution_height;
        b32 fullscreen;
        b32 vsync;
        u32 texture_quality;
        u32 shadow_quality;
        f32 render_scale;
        
        f32 master_volume;
        f32 music_volume;
        f32 sfx_volume;
        f32 voice_volume;
        b32 surround_sound;
        
        u32 key_bindings[256];
        f32 mouse_sensitivity;
        b32 invert_y;
        
        u32 difficulty;
        b32 auto_save;
        f32 auto_save_interval;
        b32 show_tutorials;
        b32 show_subtitles;
        
        b32 multi_threading;
        u32 thread_count;
        b32 gpu_particles;
    } settings;
} game_state;

// Input structures
enum {
    KEY_F5 = 116,
    KEY_F6 = 117,
    KEY_F9 = 120,
};

typedef struct key_state {
    b32 down;
    b32 pressed;
    b32 released;
} key_state;

typedef struct input_state {
    key_state keys[256];
} input_state;

// Render structures
typedef struct render_state {
    u32 width;
    u32 height;
} render_state;

// Migration functions
void save_register_all_migrations(save_system *system);

// Missing standard functions
#ifndef sprintf
#include <stdio.h>
#endif

#ifndef time
#include <time.h>
#endif

#endif // SAVE_STUBS_H