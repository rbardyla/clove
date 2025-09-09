// crystal_dungeons.h - Zelda-inspired action-adventure game
// A tech demo showcasing the handmade engine's capabilities
// Top-down perspective, dungeons, puzzles, combat, exploration

#ifndef CRYSTAL_DUNGEONS_H
#define CRYSTAL_DUNGEONS_H

#include <stdint.h>
#include <stdbool.h>
#include "../src/handmade.h"
#include "../systems/physics/handmade_physics.h"
#include "../systems/ai/handmade_neural.h"
#include "../systems/audio/handmade_audio.h"

// ============================================================================
// GAME CONSTANTS
// ============================================================================

#define TILE_SIZE 16.0f
#define ROOM_WIDTH 16        // Tiles
#define ROOM_HEIGHT 11       // Classic NES Zelda screen size
#define MAX_ROOMS 256
#define MAX_ENTITIES 1024
#define MAX_ITEMS 64
#define MAX_INVENTORY 24

// ============================================================================
// CORE TYPES
// ============================================================================

typedef enum {
    DIR_NORTH = 0,
    DIR_EAST = 1,
    DIR_SOUTH = 2,
    DIR_WEST = 3,
    DIR_COUNT
} direction;

typedef enum {
    // Terrain
    TILE_FLOOR = 0,
    TILE_WALL,
    TILE_WATER,
    TILE_LAVA,
    TILE_PIT,
    TILE_STAIRS_UP,
    TILE_STAIRS_DOWN,
    
    // Interactive
    TILE_DOOR_LOCKED,
    TILE_DOOR_OPEN,
    TILE_DOOR_BOSS,
    TILE_CHEST,
    TILE_SWITCH,
    TILE_PRESSURE_PLATE,
    TILE_PUSHABLE_BLOCK,
    TILE_CRACKED_WALL,    // Bombable
    
    // Decorative
    TILE_TORCH,
    TILE_STATUE,
    TILE_GRASS,
    TILE_BUSH,
    
    TILE_COUNT
} tile_type;

typedef enum {
    ENTITY_NONE = 0,
    
    // Player
    ENTITY_PLAYER,
    
    // Enemies
    ENTITY_SLIME,
    ENTITY_SKELETON,
    ENTITY_BAT,
    ENTITY_KNIGHT,
    ENTITY_WIZARD,
    ENTITY_DRAGON,      // Boss
    
    // NPCs
    ENTITY_OLD_MAN,     // Gives hints
    ENTITY_MERCHANT,
    ENTITY_FAIRY,       // Heals player
    
    // Projectiles
    ENTITY_SWORD_BEAM,
    ENTITY_ARROW,
    ENTITY_MAGIC_BOLT,
    ENTITY_FIREBALL,
    ENTITY_BOOMERANG,
    
    // Pickups
    ENTITY_HEART,
    ENTITY_RUPEE,       // Currency (legally distinct!)
    ENTITY_KEY,
    ENTITY_BOMB,
    ENTITY_ARROW_PICKUP,
    
    // Objects
    ENTITY_POT,         // Can be lifted and thrown
    ENTITY_CRATE,
    ENTITY_CRYSTAL,     // Main collectible
    
    ENTITY_COUNT
} entity_type;

typedef enum {
    ITEM_NONE = 0,
    
    // Equipment
    ITEM_SWORD_WOOD,
    ITEM_SWORD_IRON,
    ITEM_SWORD_CRYSTAL,
    ITEM_SHIELD_WOOD,
    ITEM_SHIELD_IRON,
    ITEM_TUNIC_GREEN,
    ITEM_TUNIC_BLUE,
    ITEM_TUNIC_RED,
    
    // Tools
    ITEM_BOW,
    ITEM_BOOMERANG,
    ITEM_HOOKSHOT,
    ITEM_BOMBS,
    ITEM_LANTERN,
    ITEM_HAMMER,
    
    // Magic items
    ITEM_WAND_FIRE,
    ITEM_WAND_ICE,
    ITEM_BOOTS_SPEED,
    ITEM_GLOVES_POWER,  // Lift heavy objects
    ITEM_CAPE_INVISIBLE,
    
    // Quest items
    ITEM_MAP,
    ITEM_COMPASS,
    ITEM_BOSS_KEY,
    ITEM_CRYSTAL_SHARD,  // Collect 8 to win
    
    ITEM_COUNT
} item_type;

// ============================================================================
// ENTITY SYSTEM
// ============================================================================

typedef struct entity {
    entity_type type;
    v2 position;
    v2 velocity;
    v2 size;
    
    // State
    f32 health;
    f32 max_health;
    direction facing;
    bool is_alive;
    bool is_active;
    
    // Animation
    u32 sprite_id;
    u32 animation_frame;
    f32 animation_timer;
    
    // Combat
    f32 damage;
    f32 knockback_timer;
    v2 knockback_velocity;
    f32 invulnerable_timer;
    f32 attack_cooldown;
    
    // AI (for enemies)
    struct {
        enum {
            AI_IDLE,
            AI_PATROL,
            AI_CHASE,
            AI_ATTACK,
            AI_FLEE,
            AI_SPECIAL
        } state;
        
        v2 home_position;
        v2 target_position;
        f32 think_timer;
        f32 state_timer;
        
        // Neural network for smart enemies
        neural_network* brain;
    } ai;
    
    // Physics
    rect collision_box;
    bool is_solid;
    bool can_push;
    bool can_be_pushed;
    
    // Interaction
    void (*on_interact)(struct entity* self, struct entity* other);
    void (*on_damage)(struct entity* self, f32 damage, struct entity* source);
    void (*on_death)(struct entity* self);
    
} entity;

// ============================================================================
// ROOM/DUNGEON SYSTEM
// ============================================================================

typedef struct room {
    tile_type tiles[ROOM_HEIGHT][ROOM_WIDTH];
    
    // Connections
    struct room* neighbors[DIR_COUNT];
    bool has_door[DIR_COUNT];
    bool door_locked[DIR_COUNT];
    
    // Entities in this room
    entity* entities;
    u32 entity_count;
    
    // Room properties
    bool is_cleared;    // All enemies defeated
    bool is_dark;       // Needs lantern
    bool has_secret;    // Hidden passage
    
    // Puzzle elements
    struct {
        bool active;
        u32 switches_total;
        u32 switches_pressed;
        bool solved;
    } puzzle;
    
    // Visual theme
    u32 tileset_id;
    color32 ambient_light;
    
} room;

typedef struct dungeon {
    room rooms[MAX_ROOMS];
    u32 room_count;
    
    room* entrance;
    room* boss_room;
    room* current_room;
    
    // Dungeon properties
    char name[64];
    u32 floor_number;
    u32 crystals_found;
    u32 crystals_total;
    
    // Map data
    bool map_revealed[MAX_ROOMS];
    bool has_map;
    bool has_compass;
    bool has_boss_key;
    
} dungeon;

// ============================================================================
// PLAYER STATE
// ============================================================================

typedef struct player {
    entity* entity;
    
    // Stats
    f32 max_health;
    f32 magic;
    f32 max_magic;
    u32 rupees;
    u32 arrows;
    u32 bombs;
    u32 keys;
    
    // Equipment
    item_type equipped_sword;
    item_type equipped_shield;
    item_type equipped_item_a;
    item_type equipped_item_b;
    item_type equipped_tunic;
    
    // Inventory
    struct {
        item_type type;
        u32 quantity;
    } inventory[MAX_INVENTORY];
    
    // Abilities (unlocked through progression)
    bool can_swim;
    bool can_push_heavy;
    bool can_see_secrets;
    bool can_walk_on_lava;
    
    // Combat state
    bool is_attacking;
    f32 sword_swing_angle;
    rect sword_hitbox;
    
    // Interaction
    entity* held_object;    // Pot, crate, etc.
    entity* nearby_npc;
    
} player;

// ============================================================================
// GAME STATE
// ============================================================================

typedef enum {
    GAME_STATE_TITLE,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_INVENTORY,
    GAME_STATE_DIALOGUE,
    GAME_STATE_TRANSITION,  // Room transition
    GAME_STATE_GAME_OVER,
    GAME_STATE_VICTORY,
} game_state_type;

typedef struct game_state {
    game_state_type current_state;
    
    // Core systems
    player player;
    dungeon* current_dungeon;
    room* current_room;
    
    // Entity management
    entity entities[MAX_ENTITIES];
    u32 entity_count;
    
    // Camera
    struct {
        v2 position;
        v2 target;
        f32 zoom;
        rect bounds;
        bool is_transitioning;
        f32 transition_timer;
        direction transition_direction;
    } camera;
    
    // Input
    struct {
        v2 movement;
        bool attack_pressed;
        bool use_item_a_pressed;
        bool use_item_b_pressed;
        bool interact_pressed;
        bool inventory_pressed;
    } input;
    
    // UI
    struct {
        bool show_hud;
        bool show_minimap;
        f32 dialogue_timer;
        char dialogue_text[256];
        entity* dialogue_speaker;
    } ui;
    
    // Audio
    struct {
        u32 music_track;
        f32 music_volume;
        f32 sfx_volume;
    } audio;
    
    // Save data
    struct {
        u32 crystals_collected;
        u32 dungeons_completed;
        f32 play_time;
        u32 enemies_defeated;
        u32 deaths;
    } stats;
    
    // Performance
    f32 delta_time;
    f32 time_accumulator;
    u32 frame_count;
    
} game_state;

// ============================================================================
// CORE GAME FUNCTIONS
// ============================================================================

// Initialization
void game_init(game_state* game);
void game_shutdown(game_state* game);

// Main loop
void game_update(game_state* game, f32 dt);
void game_render(game_state* game);
void game_handle_input(game_state* game, input_state* input);

// Player
void player_init(player* p);
void player_update(player* p, f32 dt);
void player_attack(player* p);
void player_use_item(player* p, item_type item);
void player_take_damage(player* p, f32 damage);
void player_collect_item(player* p, item_type item, u32 quantity);

// Entity management
entity* entity_create(game_state* game, entity_type type, v2 position);
void entity_destroy(game_state* game, entity* e);
void entity_update(entity* e, f32 dt);
void entity_render(entity* e);
bool entity_check_collision(entity* a, entity* b);
void entity_apply_knockback(entity* e, v2 direction, f32 force);

// AI
void ai_update(entity* e, game_state* game, f32 dt);
void ai_patrol(entity* e, f32 dt);
void ai_chase_player(entity* e, player* p, f32 dt);
void ai_attack_pattern(entity* e, player* p);

// Room/Dungeon
room* room_create(void);
void room_generate(room* r, u32 seed);
void room_load(game_state* game, room* r);
void room_unload(game_state* game, room* r);
void room_transition(game_state* game, direction dir);
bool room_is_cleared(room* r);

dungeon* dungeon_generate(u32 floor_number, u32 seed);
void dungeon_destroy(dungeon* d);
room* dungeon_get_room(dungeon* d, i32 x, i32 y);

// Combat
void combat_sword_swing(game_state* game, player* p);
void combat_shoot_projectile(game_state* game, entity* source, 
                            entity_type projectile_type, v2 direction);
void combat_explosion(game_state* game, v2 position, f32 radius, f32 damage);

// Items & Inventory
void inventory_add_item(player* p, item_type item, u32 quantity);
bool inventory_has_item(player* p, item_type item);
void inventory_use_item(player* p, item_type item);
void inventory_render(game_state* game);

// Puzzles
void puzzle_init_switches(room* r, u32 switch_count);
void puzzle_activate_switch(room* r, u32 switch_id);
bool puzzle_is_solved(room* r);
void puzzle_on_solve(room* r);

// UI
void ui_render_hud(game_state* game);
void ui_render_minimap(game_state* game);
void ui_show_dialogue(game_state* game, const char* text, entity* speaker);
void ui_render_dialogue(game_state* game);

// Save/Load
void game_save(game_state* game, const char* filename);
void game_load(game_state* game, const char* filename);

// ============================================================================
// SPECIAL MECHANICS
// ============================================================================

// Lifting and throwing objects
void player_lift_object(player* p, entity* object);
void player_throw_object(player* p, v2 direction);

// Hookshot mechanics
void hookshot_fire(game_state* game, player* p, v2 direction);
bool hookshot_can_attach(tile_type tile);

// Bomb mechanics  
void bomb_place(game_state* game, v2 position);
void bomb_explode(game_state* game, v2 position);
bool wall_is_bombable(room* r, i32 x, i32 y);

// Secret detection
void reveal_secrets(room* r, player* p);
bool check_secret_condition(room* r);

#endif // CRYSTAL_DUNGEONS_H