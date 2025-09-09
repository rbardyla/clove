#ifndef HANDMADE_ACHIEVEMENTS_H
#define HANDMADE_ACHIEVEMENTS_H

/*
    Handmade Achievement System
    Complete game achievement framework with progression tracking
    
    Features:
    - Hierarchical achievement categories  
    - Progress tracking with statistics
    - Unlockable rewards and bonuses
    - Steam API compatibility layer
    - Achievement notifications
    - Persistent statistics
    
    PERFORMANCE: Targets
    - Achievement check: <0.1μs
    - Progress update: <0.5μs  
    - UI rendering: <0.2ms
    - Memory usage: <128KB
*/

#include "../../src/handmade.h"

// Achievement system constants
#define ACHIEVEMENTS_MAGIC_NUMBER 0x41434856  // "VHCA" in hex
#define ACHIEVEMENTS_VERSION 1
#define ACHIEVEMENTS_MAX_COUNT 512
#define ACHIEVEMENTS_MAX_CATEGORIES 32
#define ACHIEVEMENTS_MAX_STATS 256
#define ACHIEVEMENTS_STRING_MAX 128
#define ACHIEVEMENTS_DESCRIPTION_MAX 256

// Forward declarations
typedef struct input_state input_state;
typedef struct gui_context gui_context;
typedef struct render_state render_state;

// Achievement types
typedef enum achievement_type {
    ACHIEVEMENT_UNLOCK = 0,     // Simple unlock achievement
    ACHIEVEMENT_PROGRESS = 1,   // Progress-based (collect X items)
    ACHIEVEMENT_COUNTER = 2,    // Counter-based (kill 1000 enemies) 
    ACHIEVEMENT_SEQUENCE = 3,   // Sequence of actions
    ACHIEVEMENT_MILESTONE = 4,  // Time or level milestone
    ACHIEVEMENT_HIDDEN = 5,     // Hidden until unlocked
    ACHIEVEMENT_META = 6        // Meta achievement (unlock other achievements)
} achievement_type;

// Achievement categories
typedef enum achievement_category {
    CATEGORY_STORY = 0,
    CATEGORY_COMBAT = 1,
    CATEGORY_EXPLORATION = 2,
    CATEGORY_COLLECTION = 3,
    CATEGORY_SKILL = 4,
    CATEGORY_SOCIAL = 5,
    CATEGORY_SPEEDRUN = 6,
    CATEGORY_HIDDEN = 7,
    CATEGORY_META = 8,
    CATEGORY_CUSTOM = 9
} achievement_category;

// Achievement rarity/difficulty
typedef enum achievement_rarity {
    RARITY_COMMON = 0,     // 50%+ of players
    RARITY_UNCOMMON = 1,   // 25-50% of players
    RARITY_RARE = 2,       // 5-25% of players
    RARITY_EPIC = 3,       // 1-5% of players
    RARITY_LEGENDARY = 4   // <1% of players
} achievement_rarity;

// Achievement flags
#define ACHIEVEMENT_UNLOCKED     (1 << 0)
#define ACHIEVEMENT_SECRET       (1 << 1)
#define ACHIEVEMENT_STEAM        (1 << 2)
#define ACHIEVEMENT_TRACKED      (1 << 3)
#define ACHIEVEMENT_NOTIFICATION (1 << 4)
#define ACHIEVEMENT_REWARD       (1 << 5)

// Statistic data types
typedef enum stat_type {
    STAT_INT = 0,
    STAT_FLOAT = 1,
    STAT_TIME = 2,
    STAT_BOOL = 3
} stat_type;

// Achievement reward types
typedef enum reward_type {
    REWARD_NONE = 0,
    REWARD_XP = 1,
    REWARD_COINS = 2,
    REWARD_ITEM = 3,
    REWARD_UNLOCK = 4,
    REWARD_COSMETIC = 5,
    REWARD_TITLE = 6
} reward_type;

// Achievement progress info
typedef struct achievement_progress {
    f32 current;
    f32 target;
    f32 percentage;
    u32 milestones_hit;
    u64 first_progress_time;
    u64 last_update_time;
} achievement_progress;

// Achievement reward
typedef struct achievement_reward {
    reward_type type;
    i32 amount;
    char item_id[64];
    char title[ACHIEVEMENTS_STRING_MAX];
    char description[ACHIEVEMENTS_STRING_MAX];
} achievement_reward;

// Main achievement structure
typedef struct achievement {
    char id[ACHIEVEMENTS_STRING_MAX];
    char name[ACHIEVEMENTS_STRING_MAX];
    char description[ACHIEVEMENTS_DESCRIPTION_MAX];
    char icon_path[ACHIEVEMENTS_STRING_MAX];
    
    achievement_type type;
    achievement_category category;
    achievement_rarity rarity;
    u32 flags;
    
    achievement_progress progress;
    achievement_reward reward;
    
    // Requirements
    f32 target_value;
    char required_stat[ACHIEVEMENTS_STRING_MAX];
    char prerequisite_achievement[ACHIEVEMENTS_STRING_MAX];
    
    // Timing
    u64 unlock_time;
    u64 created_time;
    f32 estimated_completion_time; // in hours
    
    // Steam integration
    char steam_id[64];
    u32 steam_api_id;
    
    // Internal tracking
    u32 hash;
    b32 dirty; // needs save
} achievement;

// Game statistics
typedef struct game_stat {
    char name[ACHIEVEMENTS_STRING_MAX];
    char display_name[ACHIEVEMENTS_STRING_MAX];
    stat_type type;
    
    union {
        i32 int_value;
        f32 float_value;
        u64 time_value;
        b32 bool_value;
    } value;
    
    union {
        i32 int_default;
        f32 float_default;
        u64 time_default;
        b32 bool_default;
    } default_value;
    
    // Aggregation info
    f32 session_change;
    f32 total_change;
    u64 last_update;
    
    u32 hash;
    b32 tracked;
} game_stat;

// Achievement category info
typedef struct achievement_category_info {
    char name[ACHIEVEMENTS_STRING_MAX];
    char description[ACHIEVEMENTS_STRING_MAX];
    achievement_category category;
    
    u32 total_count;
    u32 unlocked_count;
    f32 completion_percentage;
    
    achievement *achievements[128]; // Pointers to achievements in this category
    u32 achievement_count;
    
    b32 expanded; // UI state
} achievement_category_info;

// Achievement notification
typedef struct achievement_notification {
    achievement *achievement;
    char message[ACHIEVEMENTS_DESCRIPTION_MAX];
    f32 display_time;
    f32 fade_time;
    b32 active;
    u32 animation_state;
} achievement_notification;

// Achievement system state
typedef struct achievement_system {
    // Memory
    u8 *memory;
    u32 memory_size;
    
    // All achievements
    achievement achievements[ACHIEVEMENTS_MAX_COUNT];
    u32 achievement_count;
    
    // Statistics
    game_stat stats[ACHIEVEMENTS_MAX_STATS];
    u32 stat_count;
    
    // Categories
    achievement_category_info categories[ACHIEVEMENTS_MAX_CATEGORIES];
    u32 category_count;
    
    // Runtime state
    achievement_notification notifications[8];
    u32 notification_count;
    
    // Progress tracking
    u32 total_unlocked;
    f32 overall_completion;
    u32 points_earned;
    
    // Session stats
    u32 achievements_this_session;
    f32 session_start_time;
    
    // UI state
    b32 ui_visible;
    b32 notifications_enabled;
    achievement_category filter_category;
    b32 show_locked;
    b32 show_progress;
    
    // File I/O
    char save_path[256];
    char stats_path[256];
    u64 last_save_time;
    b32 auto_save_enabled;
    
    // Steam integration
    b32 steam_enabled;
    void *steam_context;
} achievement_system;

// =============================================================================
// CORE API
// =============================================================================

// System management
achievement_system *achievements_init(void *memory, u32 memory_size);
void achievements_shutdown(achievement_system *system);
void achievements_update(achievement_system *system, f32 dt);

// Achievement registration
u32 achievements_register(achievement_system *system, 
                         char *id, char *name, char *description,
                         achievement_type type, achievement_category category,
                         f32 target_value, char *required_stat);

u32 achievements_register_unlock(achievement_system *system,
                               char *id, char *name, char *description,
                               achievement_category category);

u32 achievements_register_progress(achievement_system *system,
                                 char *id, char *name, char *description,
                                 achievement_category category,
                                 char *stat_name, f32 target_value);

u32 achievements_register_counter(achievement_system *system,
                                char *id, char *name, char *description,
                                achievement_category category,
                                char *stat_name, i32 target_count);

// Achievement management
b32 achievements_unlock(achievement_system *system, char *achievement_id);
b32 achievements_is_unlocked(achievement_system *system, char *achievement_id);
f32 achievements_get_progress(achievement_system *system, char *achievement_id);
achievement *achievements_find(achievement_system *system, char *achievement_id);

// Statistics API
void achievements_register_stat(achievement_system *system, 
                              char *name, char *display_name, stat_type type);

void achievements_set_stat_int(achievement_system *system, char *name, i32 value);
void achievements_set_stat_float(achievement_system *system, char *name, f32 value);
void achievements_add_stat_int(achievement_system *system, char *name, i32 delta);
void achievements_add_stat_float(achievement_system *system, char *name, f32 delta);

i32 achievements_get_stat_int(achievement_system *system, char *name);
f32 achievements_get_stat_float(achievement_system *system, char *name);
game_stat *achievements_find_stat(achievement_system *system, char *name);

// Progress tracking
void achievements_update_progress(achievement_system *system, char *stat_name);
void achievements_check_unlock_conditions(achievement_system *system);
void achievements_trigger_unlock(achievement_system *system, achievement *ach);

// File I/O
b32 achievements_save(achievement_system *system);
b32 achievements_load(achievement_system *system);
b32 achievements_save_stats(achievement_system *system);
b32 achievements_load_stats(achievement_system *system);

// Notifications
void achievements_show_notification(achievement_system *system, achievement *ach);
void achievements_update_notifications(achievement_system *system, f32 dt);
void achievements_clear_notifications(achievement_system *system);

// UI System
void achievements_show_ui(achievement_system *system);
void achievements_hide_ui(achievement_system *system);
void achievements_render_ui(achievement_system *system, gui_context *gui);
void achievements_render_notifications(achievement_system *system, render_state *render);

// Steam Integration
void achievements_init_steam(achievement_system *system, void *steam_context);
void achievements_sync_steam(achievement_system *system);
void achievements_set_steam_stat(achievement_system *system, char *name, f32 value);

// Utility functions
void achievements_reset_all(achievement_system *system);
void achievements_unlock_all(achievement_system *system); // Debug only
u32 achievements_get_total_points(achievement_system *system);
f32 achievements_get_completion_percentage(achievement_system *system);
void achievements_print_stats(achievement_system *system);
u64 achievements_get_timestamp();

// Default achievements registration
void achievements_register_story_achievements(achievement_system *system);
void achievements_register_combat_achievements(achievement_system *system);
void achievements_register_exploration_achievements(achievement_system *system);
void achievements_register_collection_achievements(achievement_system *system);
void achievements_register_skill_achievements(achievement_system *system);
void achievements_register_meta_achievements(achievement_system *system);
void achievements_register_all_defaults(achievement_system *system);

#endif // HANDMADE_ACHIEVEMENTS_H