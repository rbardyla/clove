// sprite_assets.h - Sprite asset loading and management for Crystal Dungeons
// Handles sprite sheets, animations, and asset pipeline

#ifndef SPRITE_ASSETS_H
#define SPRITE_ASSETS_H

#include "game_types.h"

// ============================================================================
// SPRITE SHEET DEFINITIONS
// ============================================================================

#define SPRITE_SIZE 16
#define SHEET_WIDTH 256
#define SHEET_HEIGHT 256
#define SPRITES_PER_ROW (SHEET_WIDTH / SPRITE_SIZE)
#define MAX_SPRITE_SHEETS 16
#define MAX_ANIMATIONS 256
#define MAX_FRAMES_PER_ANIMATION 32

// Sprite sheet IDs
typedef enum {
    SHEET_PLAYER = 0,
    SHEET_ENEMIES,
    SHEET_TILES,
    SHEET_ITEMS,
    SHEET_EFFECTS,
    SHEET_UI,
    SHEET_COUNT
} sprite_sheet_id;

// ============================================================================
// SPRITE DEFINITIONS
// ============================================================================

// Player sprites
typedef enum {
    // Standing (4 directions)
    SPRITE_PLAYER_STAND_DOWN = 0,
    SPRITE_PLAYER_STAND_UP = 1,
    SPRITE_PLAYER_STAND_LEFT = 2,
    SPRITE_PLAYER_STAND_RIGHT = 3,
    
    // Walking (4 directions x 2 frames)
    SPRITE_PLAYER_WALK_DOWN_1 = 4,
    SPRITE_PLAYER_WALK_DOWN_2 = 5,
    SPRITE_PLAYER_WALK_UP_1 = 6,
    SPRITE_PLAYER_WALK_UP_2 = 7,
    SPRITE_PLAYER_WALK_LEFT_1 = 8,
    SPRITE_PLAYER_WALK_LEFT_2 = 9,
    SPRITE_PLAYER_WALK_RIGHT_1 = 10,
    SPRITE_PLAYER_WALK_RIGHT_2 = 11,
    
    // Attacking (4 directions)
    SPRITE_PLAYER_ATTACK_DOWN = 12,
    SPRITE_PLAYER_ATTACK_UP = 13,
    SPRITE_PLAYER_ATTACK_LEFT = 14,
    SPRITE_PLAYER_ATTACK_RIGHT = 15,
} player_sprite;

// Enemy sprites
typedef enum {
    // Slime (2 frames)
    SPRITE_SLIME_1 = 0,
    SPRITE_SLIME_2 = 1,
    
    // Skeleton (4 directions x 2 frames)
    SPRITE_SKELETON_DOWN_1 = 2,
    SPRITE_SKELETON_DOWN_2 = 3,
    SPRITE_SKELETON_UP_1 = 4,
    SPRITE_SKELETON_UP_2 = 5,
    SPRITE_SKELETON_LEFT_1 = 6,
    SPRITE_SKELETON_LEFT_2 = 7,
    SPRITE_SKELETON_RIGHT_1 = 8,
    SPRITE_SKELETON_RIGHT_2 = 9,
    
    // Bat (2 frames)
    SPRITE_BAT_1 = 10,
    SPRITE_BAT_2 = 11,
    
    // Knight (4 frames)
    SPRITE_KNIGHT_STAND = 12,
    SPRITE_KNIGHT_WALK_1 = 13,
    SPRITE_KNIGHT_WALK_2 = 14,
    SPRITE_KNIGHT_ATTACK = 15,
    
    // Wizard
    SPRITE_WIZARD_STAND = 16,
    SPRITE_WIZARD_CAST = 17,
    
    // Dragon boss (4x4 tiles)
    SPRITE_DRAGON_BASE = 32,  // Takes up 16 sprites (4x4)
} enemy_sprite;

// Tile sprites
typedef enum {
    SPRITE_FLOOR = 0,
    SPRITE_WALL = 1,
    SPRITE_WALL_TOP = 2,
    SPRITE_WATER_1 = 3,
    SPRITE_WATER_2 = 4,
    SPRITE_LAVA_1 = 5,
    SPRITE_LAVA_2 = 6,
    SPRITE_PIT = 7,
    SPRITE_STAIRS_UP = 8,
    SPRITE_STAIRS_DOWN = 9,
    SPRITE_DOOR_CLOSED = 10,
    SPRITE_DOOR_OPEN = 11,
    SPRITE_DOOR_LOCKED = 12,
    SPRITE_DOOR_BOSS = 13,
    SPRITE_CHEST_CLOSED = 14,
    SPRITE_CHEST_OPEN = 15,
    SPRITE_SWITCH_OFF = 16,
    SPRITE_SWITCH_ON = 17,
    SPRITE_PRESSURE_PLATE = 18,
    SPRITE_PUSHABLE_BLOCK = 19,
    SPRITE_CRACKED_WALL = 20,
    SPRITE_TORCH_1 = 21,
    SPRITE_TORCH_2 = 22,
    SPRITE_STATUE = 23,
    SPRITE_GRASS = 24,
    SPRITE_BUSH = 25,
} tile_sprite;

// Item sprites
typedef enum {
    SPRITE_SWORD_WOOD = 0,
    SPRITE_SWORD_IRON = 1,
    SPRITE_SWORD_CRYSTAL = 2,
    SPRITE_SHIELD_WOOD = 3,
    SPRITE_SHIELD_IRON = 4,
    SPRITE_BOW = 5,
    SPRITE_ARROW = 6,
    SPRITE_BOOMERANG = 7,
    SPRITE_HOOKSHOT = 8,
    SPRITE_BOMB = 9,
    SPRITE_LANTERN = 10,
    SPRITE_HAMMER = 11,
    SPRITE_WAND_FIRE = 12,
    SPRITE_WAND_ICE = 13,
    SPRITE_HEART_FULL = 14,
    SPRITE_HEART_HALF = 15,
    SPRITE_HEART_EMPTY = 16,
    SPRITE_RUPEE_GREEN = 17,
    SPRITE_RUPEE_BLUE = 18,
    SPRITE_RUPEE_RED = 19,
    SPRITE_KEY = 20,
    SPRITE_BOSS_KEY = 21,
    SPRITE_MAP = 22,
    SPRITE_COMPASS = 23,
    SPRITE_CRYSTAL_SHARD = 24,
} item_sprite;

// Effect sprites
typedef enum {
    SPRITE_EXPLOSION_1 = 0,
    SPRITE_EXPLOSION_2 = 1,
    SPRITE_EXPLOSION_3 = 2,
    SPRITE_EXPLOSION_4 = 3,
    SPRITE_SLASH_H = 4,
    SPRITE_SLASH_V = 5,
    SPRITE_MAGIC_SPARKLE_1 = 6,
    SPRITE_MAGIC_SPARKLE_2 = 7,
    SPRITE_SMOKE_1 = 8,
    SPRITE_SMOKE_2 = 9,
    SPRITE_DUST_1 = 10,
    SPRITE_DUST_2 = 11,
} effect_sprite;

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================

typedef struct sprite_frame {
    u32 sheet_id;
    u32 sprite_index;
    f32 duration;  // In seconds
} sprite_frame;

typedef struct sprite_animation {
    char name[32];
    sprite_frame frames[MAX_FRAMES_PER_ANIMATION];
    u32 frame_count;
    bool loop;
    f32 total_duration;
} sprite_animation;

typedef struct animation_state {
    sprite_animation* current_anim;
    u32 current_frame;
    f32 frame_timer;
    bool is_playing;
    f32 playback_speed;
} animation_state;

// ============================================================================
// SPRITE SHEET STRUCTURE
// ============================================================================

typedef struct sprite_sheet {
    u32 texture_id;
    u32 width;
    u32 height;
    u32 sprite_width;
    u32 sprite_height;
    u32 sprites_per_row;
    u32 total_sprites;
    char name[64];
    bool is_loaded;
} sprite_sheet;

// ============================================================================
// SPRITE ASSET MANAGER
// ============================================================================

typedef struct sprite_assets {
    sprite_sheet sheets[MAX_SPRITE_SHEETS];
    u32 sheet_count;
    
    sprite_animation animations[MAX_ANIMATIONS];
    u32 animation_count;
    
    // Procedural sprite generation
    struct {
        u32* pixel_buffer;
        u32 buffer_size;
    } generator;
    
    // Performance
    u32 sprites_rendered;
    u32 draw_calls;
} sprite_assets;

// ============================================================================
// CORE API
// ============================================================================

// Asset manager
void sprite_assets_init(sprite_assets* assets);
void sprite_assets_shutdown(sprite_assets* assets);

// Sprite sheet loading (procedural for now, file loading later)
void sprite_assets_generate_sheets(sprite_assets* assets);
sprite_sheet* sprite_assets_get_sheet(sprite_assets* assets, sprite_sheet_id id);

// Procedural sprite generation
void sprite_generate_player(u32* pixels, u32 sprite_index);
void sprite_generate_enemy(u32* pixels, enemy_sprite sprite);
void sprite_generate_tile(u32* pixels, tile_sprite sprite);
void sprite_generate_item(u32* pixels, item_sprite sprite);
void sprite_generate_effect(u32* pixels, effect_sprite sprite);
void sprite_generate_ui(u32* pixels, u32 sprite_index);

// Animation management
sprite_animation* sprite_create_animation(sprite_assets* assets, const char* name);
void sprite_animation_add_frame(sprite_animation* anim, u32 sheet_id, 
                               u32 sprite_index, f32 duration);
sprite_animation* sprite_get_animation(sprite_assets* assets, const char* name);

// Animation playback
void animation_play(animation_state* state, sprite_animation* anim);
void animation_stop(animation_state* state);
void animation_update(animation_state* state, f32 dt);
sprite_frame* animation_get_current_frame(animation_state* state);

// Sprite rendering helpers
rect sprite_get_uv_rect(sprite_sheet* sheet, u32 sprite_index);
v2 sprite_index_to_coords(u32 sprite_index, u32 sprites_per_row);

// ============================================================================
// PREDEFINED ANIMATIONS
// ============================================================================

void sprite_assets_create_default_animations(sprite_assets* assets);
sprite_animation* create_player_walk_animation(sprite_assets* assets, int dir);
sprite_animation* create_slime_idle_animation(sprite_assets* assets);
sprite_animation* create_water_animation(sprite_assets* assets);
sprite_animation* create_torch_animation(sprite_assets* assets);
sprite_animation* create_explosion_animation(sprite_assets* assets);

#endif // SPRITE_ASSETS_H