#ifndef HANDMADE_STEAM_H
#define HANDMADE_STEAM_H

/*
    Handmade Steam Integration
    Complete Steam API wrapper with zero external dependencies
    
    Features:
    - Achievement synchronization
    - Cloud save management
    - Steam Workshop integration
    - Leaderboards and statistics
    - Networking (P2P, lobbies)
    - Rich presence and overlay
    - Steam Input controller support
    
    PERFORMANCE: Targets
    - API calls: <1ms response
    - File sync: <100ms for small saves
    - Achievement sync: <50ms
    - Memory usage: <256KB
*/

#include "../../src/handmade.h"

// Steam integration constants
#define STEAM_MAGIC_NUMBER 0x53544D48  // "HMTS" in hex
#define STEAM_VERSION 1
#define STEAM_MAX_ACHIEVEMENTS 512
#define STEAM_MAX_STATS 256
#define STEAM_MAX_LEADERBOARDS 64
#define STEAM_MAX_WORKSHOP_ITEMS 1024
#define STEAM_STRING_MAX 256
#define STEAM_DESCRIPTION_MAX 512

// Forward declarations
typedef struct achievement_system achievement_system;
typedef struct settings_system settings_system;

// Steam initialization result
typedef enum steam_init_result {
    STEAM_INIT_SUCCESS = 0,
    STEAM_INIT_FAILED_NOT_RUNNING = 1,
    STEAM_INIT_FAILED_NO_APP_ID = 2,
    STEAM_INIT_FAILED_DIFFERENT_APP = 3,
    STEAM_INIT_FAILED_DIFFERENT_USER = 4,
    STEAM_INIT_FAILED_VERSIONMISMATCH = 5,
    STEAM_INIT_FAILED_GENERIC = 6
} steam_init_result;

// Steam user information
typedef struct steam_user {
    u64 steam_id;
    char username[STEAM_STRING_MAX];
    char display_name[STEAM_STRING_MAX];
    b32 logged_in;
    b32 premium_account;
    b32 vac_banned;
    b32 community_banned;
    u32 account_creation_time;
} steam_user;

// Steam achievement info
typedef struct steam_achievement {
    char id[STEAM_STRING_MAX];
    char name[STEAM_STRING_MAX];
    char description[STEAM_STRING_MAX];
    char icon_path[STEAM_STRING_MAX];
    b32 unlocked;
    b32 hidden;
    u64 unlock_time;
    f32 global_percent;
    b32 dirty; // Needs sync
} steam_achievement;

// Steam statistic
typedef struct steam_stat {
    char name[STEAM_STRING_MAX];
    char display_name[STEAM_STRING_MAX];
    
    enum {
        STEAM_STAT_INT,
        STEAM_STAT_FLOAT,
        STEAM_STAT_AVG_RATE
    } type;
    
    union {
        i32 int_value;
        f32 float_value;
    } value;
    
    b32 dirty;
} steam_stat;

// Steam leaderboard entry
typedef struct steam_leaderboard_entry {
    u64 steam_id;
    char username[STEAM_STRING_MAX];
    i32 global_rank;
    i32 score;
    i32 details[8]; // Additional details
    u64 upload_time;
} steam_leaderboard_entry;

// Steam leaderboard
typedef struct steam_leaderboard {
    char name[STEAM_STRING_MAX];
    char display_name[STEAM_STRING_MAX];
    u64 handle;
    i32 entry_count;
    
    enum {
        STEAM_LEADERBOARD_SORT_ASCENDING,
        STEAM_LEADERBOARD_SORT_DESCENDING
    } sort_method;
    
    enum {
        STEAM_LEADERBOARD_DISPLAY_NUMERIC,
        STEAM_LEADERBOARD_DISPLAY_TIME_SECONDS,
        STEAM_LEADERBOARD_DISPLAY_TIME_MILLISECONDS
    } display_type;
    
    steam_leaderboard_entry entries[100]; // Top 100 entries
    i32 user_rank;
    i32 user_score;
    
    b32 loaded;
} steam_leaderboard;

// Steam Workshop item
typedef struct steam_workshop_item {
    u64 published_file_id;
    char title[STEAM_STRING_MAX];
    char description[STEAM_DESCRIPTION_MAX];
    char preview_url[STEAM_STRING_MAX];
    char file_path[STEAM_STRING_MAX];
    
    u64 creator_id;
    char creator_name[STEAM_STRING_MAX];
    
    u64 file_size;
    u64 preview_size;
    u32 creation_time;
    u32 update_time;
    
    enum {
        STEAM_WORKSHOP_VISIBILITY_PUBLIC,
        STEAM_WORKSHOP_VISIBILITY_FRIENDS_ONLY,
        STEAM_WORKSHOP_VISIBILITY_PRIVATE
    } visibility;
    
    b32 subscribed;
    b32 downloaded;
    b32 installed;
    f32 download_progress;
    
    char tags[10][64]; // Up to 10 tags
    u32 tag_count;
    
    u32 upvotes;
    u32 downvotes;
    f32 score;
} steam_workshop_item;

// Steam friend information
typedef struct steam_friend {
    u64 steam_id;
    char username[STEAM_STRING_MAX];
    char display_name[STEAM_STRING_MAX];
    
    enum {
        STEAM_FRIEND_STATUS_OFFLINE,
        STEAM_FRIEND_STATUS_ONLINE,
        STEAM_FRIEND_STATUS_BUSY,
        STEAM_FRIEND_STATUS_AWAY,
        STEAM_FRIEND_STATUS_SNOOZE,
        STEAM_FRIEND_STATUS_LOOKING_TO_TRADE,
        STEAM_FRIEND_STATUS_LOOKING_TO_PLAY
    } status;
    
    char status_message[STEAM_STRING_MAX];
    char game_name[STEAM_STRING_MAX];
    u32 game_app_id;
    
    b32 in_game;
    b32 playing_same_game;
} steam_friend;

// Steam cloud file
typedef struct steam_cloud_file {
    char filename[STEAM_STRING_MAX];
    u32 file_size;
    u64 timestamp;
    b32 exists;
    b32 persisted;
} steam_cloud_file;

// Steam networking lobby
typedef struct steam_lobby {
    u64 lobby_id;
    char name[STEAM_STRING_MAX];
    u32 max_members;
    u32 member_count;
    u64 members[16]; // Steam IDs of members
    u64 owner_id;
    
    enum {
        STEAM_LOBBY_TYPE_PRIVATE,
        STEAM_LOBBY_TYPE_FRIENDS_ONLY,
        STEAM_LOBBY_TYPE_PUBLIC,
        STEAM_LOBBY_TYPE_INVISIBLE
    } type;
    
    b32 joinable;
    char metadata[32][256]; // Key-value pairs
    u32 metadata_count;
} steam_lobby;

// Main Steam system
typedef struct steam_system {
    // Initialization
    b32 initialized;
    u32 app_id;
    steam_init_result init_result;
    
    // User information
    steam_user current_user;
    steam_friend friends[100];
    u32 friend_count;
    
    // Achievements & Stats
    steam_achievement achievements[STEAM_MAX_ACHIEVEMENTS];
    u32 achievement_count;
    steam_stat stats[STEAM_MAX_STATS];
    u32 stat_count;
    
    // Leaderboards
    steam_leaderboard leaderboards[STEAM_MAX_LEADERBOARDS];
    u32 leaderboard_count;
    
    // Workshop
    steam_workshop_item subscribed_items[STEAM_MAX_WORKSHOP_ITEMS];
    u32 subscribed_item_count;
    
    // Cloud storage
    steam_cloud_file cloud_files[64];
    u32 cloud_file_count;
    b32 cloud_enabled;
    u64 cloud_quota_total;
    u64 cloud_quota_used;
    
    // Networking
    steam_lobby current_lobby;
    b32 in_lobby;
    b32 hosting_lobby;
    
    // Rich presence
    char rich_presence_status[STEAM_STRING_MAX];
    char rich_presence_details[STEAM_STRING_MAX];
    
    // Settings
    b32 overlay_enabled;
    b32 auto_sync_achievements;
    b32 auto_sync_stats;
    f32 sync_interval; // seconds
    
    // Callbacks and state
    void *steam_api_context;
    f32 last_callback_time;
    f32 last_sync_time;
    
    // Memory management
    u8 *memory;
    u32 memory_size;
} steam_system;

// =============================================================================
// CORE API
// =============================================================================

// System management
steam_system *steam_init(void *memory, u32 memory_size, u32 app_id);
void steam_shutdown(steam_system *system);
void steam_update(steam_system *system, f32 dt);
b32 steam_is_running();

// User management
b32 steam_get_user_info(steam_system *system);
void steam_refresh_friends_list(steam_system *system);
steam_friend *steam_find_friend(steam_system *system, u64 steam_id);

// Achievement integration
b32 steam_sync_achievements(steam_system *system, achievement_system *achievements);
b32 steam_unlock_achievement(steam_system *system, char *achievement_id);
b32 steam_is_achievement_unlocked(steam_system *system, char *achievement_id);
void steam_clear_achievement(steam_system *system, char *achievement_id); // Debug only

// Statistics integration
b32 steam_sync_stats(steam_system *system);
void steam_set_stat_int(steam_system *system, char *name, i32 value);
void steam_set_stat_float(steam_system *system, char *name, f32 value);
i32 steam_get_stat_int(steam_system *system, char *name);
f32 steam_get_stat_float(steam_system *system, char *name);
b32 steam_store_stats(steam_system *system);

// Cloud save management
b32 steam_cloud_write_file(steam_system *system, char *filename, void *data, u32 size);
b32 steam_cloud_read_file(steam_system *system, char *filename, void *buffer, u32 buffer_size);
b32 steam_cloud_delete_file(steam_system *system, char *filename);
b32 steam_cloud_file_exists(steam_system *system, char *filename);
u32 steam_cloud_get_file_size(steam_system *system, char *filename);
void steam_cloud_refresh_file_list(steam_system *system);

// Leaderboards
b32 steam_find_leaderboard(steam_system *system, char *leaderboard_name);
b32 steam_upload_leaderboard_score(steam_system *system, char *leaderboard_name, i32 score, i32 *details, i32 detail_count);
b32 steam_download_leaderboard_entries(steam_system *system, char *leaderboard_name, i32 start_rank, i32 end_rank);
steam_leaderboard *steam_get_leaderboard(steam_system *system, char *leaderboard_name);

// Workshop integration
b32 steam_workshop_enumerate_subscribed_items(steam_system *system);
steam_workshop_item *steam_workshop_get_item(steam_system *system, u64 published_file_id);
b32 steam_workshop_download_item(steam_system *system, u64 published_file_id);
b32 steam_workshop_create_item(steam_system *system, char *title, char *description, char *content_path);
b32 steam_workshop_update_item(steam_system *system, u64 published_file_id, char *title, char *description);

// Networking
b32 steam_create_lobby(steam_system *system, u32 max_members, char *lobby_name);
b32 steam_join_lobby(steam_system *system, u64 lobby_id);
b32 steam_leave_lobby(steam_system *system);
void steam_refresh_lobby_list(steam_system *system);
b32 steam_send_lobby_message(steam_system *system, void *data, u32 size);

// Rich presence
void steam_set_rich_presence(steam_system *system, char *key, char *value);
void steam_clear_rich_presence(steam_system *system);

// Overlay and UI
void steam_activate_game_overlay(steam_system *system, char *dialog);
void steam_activate_game_overlay_to_user(steam_system *system, char *dialog, u64 steam_id);
void steam_activate_game_overlay_to_web_page(steam_system *system, char *url);

// Input (Steam Controller)
b32 steam_input_init(steam_system *system);
void steam_input_shutdown(steam_system *system);
void steam_input_update(steam_system *system);

// Utility functions
char *steam_get_error_string(steam_init_result result);
void steam_print_stats(steam_system *system);
b32 steam_restart_app_if_necessary(u32 app_id);

// Integration helpers
b32 steam_integrate_with_achievements(steam_system *steam, achievement_system *achievements);
b32 steam_integrate_with_settings(steam_system *steam, settings_system *settings);
b32 steam_sync_achievement_stats(steam_system *steam, achievement_system *achievements);
void steam_notify_achievement_unlock(steam_system *steam, char *achievement_id);
void steam_update_rich_presence(steam_system *steam, char *status, char *details);
b32 steam_workshop_publish_mod(steam_system *steam, char *title, char *description, char *content_path);
b32 steam_upload_score(steam_system *steam, char *leaderboard_name, i32 score);

#endif // HANDMADE_STEAM_H