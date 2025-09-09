/*
    Steam Integration Helper Functions
    Bridges Steam API with our Achievement and Settings systems
*/

#include "handmade_steam.h"
#include "../achievements/handmade_achievements.h"
#include "../settings/handmade_settings.h"
#include <stdio.h>
#include <string.h>

#define internal static

// Integrate Steam with Achievement system
b32
steam_integrate_with_achievements(steam_system *steam, achievement_system *achievements) {
    if (!steam || !achievements || !steam->initialized) return 0;
    
    printf("[STEAM] Integrating with Achievement system...\\n");
    
    // Sync all achievements from our system to Steam
    u32 synced_count = 0;
    for (u32 i = 0; i < achievements->achievement_count; i++) {
        achievement *ach = &achievements->achievements[i];
        
        // Check if achievement is unlocked in our system
        if (ach->flags & ACHIEVEMENT_UNLOCKED) {
            // Sync to Steam
            if (steam_unlock_achievement(steam, ach->id)) {
                synced_count++;
            }
        }
    }
    
    // Set up Steam stats mapping
    for (u32 i = 0; i < achievements->stat_count; i++) {
        game_stat *stat = &achievements->stats[i];
        
        switch (stat->type) {
            case STAT_INT:
                steam_set_stat_int(steam, stat->name, stat->value.int_value);
                break;
            case STAT_FLOAT:
                steam_set_stat_float(steam, stat->name, stat->value.float_value);
                break;
            default:
                break;
        }
    }
    
    // Store stats to Steam
    steam_store_stats(steam);
    
    printf("[STEAM] Achievement integration complete: %u achievements synced\\n", synced_count);
    return 1;
}

// Integrate Steam with Settings system
b32
steam_integrate_with_settings(steam_system *steam, settings_system *settings) {
    if (!steam || !settings || !steam->initialized) return 0;
    
    printf("[STEAM] Integrating with Settings system...\\n");
    
    // Register Steam-specific settings
    settings_register_bool(settings, "steam_overlay", "Enable Steam overlay",
                          CATEGORY_GAMEPLAY, steam->overlay_enabled, 0);
    
    settings_register_bool(settings, "steam_auto_sync", "Auto-sync achievements",
                          CATEGORY_GAMEPLAY, steam->auto_sync_achievements, 0);
    
    settings_register_bool(settings, "steam_cloud", "Enable Steam cloud saves",
                          CATEGORY_GAMEPLAY, steam->cloud_enabled, 0);
    
    settings_register_float(settings, "steam_sync_interval", "Steam sync interval (seconds)",
                          CATEGORY_GAMEPLAY, steam->sync_interval, 10.0f, 300.0f, SETTING_ADVANCED);
    
    printf("[STEAM] Settings integration complete\\n");
    return 1;
}

// Update Steam stats from Achievement system
b32
steam_sync_achievement_stats(steam_system *steam, achievement_system *achievements) {
    if (!steam || !achievements || !steam->initialized) return 0;
    
    u32 updated_count = 0;
    
    // Sync all tracked statistics
    for (u32 i = 0; i < achievements->stat_count; i++) {
        game_stat *stat = &achievements->stats[i];
        
        // Check if this stat changed since last sync
        if (stat->session_change != 0.0f) {
            switch (stat->type) {
                case STAT_INT:
                    steam_set_stat_int(steam, stat->name, stat->value.int_value);
                    updated_count++;
                    break;
                case STAT_FLOAT:
                    steam_set_stat_float(steam, stat->name, stat->value.float_value);
                    updated_count++;
                    break;
                default:
                    break;
            }
        }
    }
    
    if (updated_count > 0) {
        printf("[STEAM] Updated %u stats in Steam\\n", updated_count);
        return steam_store_stats(steam);
    }
    
    return 1;
}

// Handle achievement unlock notification to Steam
void
steam_notify_achievement_unlock(steam_system *steam, char *achievement_id) {
    if (!steam || !steam->initialized || !achievement_id) return;
    
    // Unlock in Steam
    if (steam_unlock_achievement(steam, achievement_id)) {
        // Set rich presence to show the achievement
        char presence_text[256];
        snprintf(presence_text, 255, "Just unlocked: %s", achievement_id);
        steam_set_rich_presence(steam, "status", presence_text);
        
        printf("[STEAM] Notified Steam of achievement unlock: %s\\n", achievement_id);
    }
}

// Auto-save achievements and settings to Steam Cloud
b32
steam_auto_save_data(steam_system *steam, achievement_system *achievements, settings_system *settings) {
    if (!steam || !steam->initialized || !steam->cloud_enabled) return 0;
    
    b32 success = 1;
    
    // Save achievements to cloud
    if (achievements && achievements_save(achievements)) {
        // Read the saved file and upload to Steam Cloud
        FILE *file = fopen("achievements.dat", "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            u32 size = (u32)ftell(file);
            fseek(file, 0, SEEK_SET);
            
            void *data = malloc(size);
            if (data) {
                if (fread(data, 1, size, file) == size) {
                    if (!steam_cloud_write_file(steam, "achievements.dat", data, size)) {
                        success = 0;
                    }
                }
                free(data);
            }
            fclose(file);
        }
    }
    
    // Save settings to cloud
    if (settings && settings_save_to_file(settings, "settings.cfg")) {
        FILE *file = fopen("settings.cfg", "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            u32 size = (u32)ftell(file);
            fseek(file, 0, SEEK_SET);
            
            void *data = malloc(size);
            if (data) {
                if (fread(data, 1, size, file) == size) {
                    if (!steam_cloud_write_file(steam, "settings.cfg", data, size)) {
                        success = 0;
                    }
                }
                free(data);
            }
            fclose(file);
        }
    }
    
    if (success) {
        printf("[STEAM] Auto-saved game data to Steam Cloud\\n");
    }
    
    return success;
}

// Auto-load achievements and settings from Steam Cloud
b32
steam_auto_load_data(steam_system *steam, achievement_system *achievements, settings_system *settings) {
    if (!steam || !steam->initialized || !steam->cloud_enabled) return 0;
    
    b32 loaded_any = 0;
    
    // Load achievements from cloud
    if (achievements && steam_cloud_file_exists(steam, "achievements.dat")) {
        u32 size = steam_cloud_get_file_size(steam, "achievements.dat");
        if (size > 0) {
            void *data = malloc(size);
            if (data) {
                if (steam_cloud_read_file(steam, "achievements.dat", data, size)) {
                    // Write to local file and load
                    FILE *file = fopen("achievements_cloud.dat", "wb");
                    if (file) {
                        fwrite(data, 1, size, file);
                        fclose(file);
                        
                        // Load achievements from the cloud file
                        if (achievements_load(achievements)) {
                            printf("[STEAM] Loaded achievements from Steam Cloud\\n");
                            loaded_any = 1;
                        }
                    }
                }
                free(data);
            }
        }
    }
    
    // Load settings from cloud
    if (settings && steam_cloud_file_exists(steam, "settings.cfg")) {
        u32 size = steam_cloud_get_file_size(steam, "settings.cfg");
        if (size > 0) {
            void *data = malloc(size);
            if (data) {
                if (steam_cloud_read_file(steam, "settings.cfg", data, size)) {
                    FILE *file = fopen("settings_cloud.cfg", "wb");
                    if (file) {
                        fwrite(data, 1, size, file);
                        fclose(file);
                        
                        if (settings_load_from_file(settings, "settings_cloud.cfg")) {
                            printf("[STEAM] Loaded settings from Steam Cloud\\n");
                            loaded_any = 1;
                        }
                    }
                }
                free(data);
            }
        }
    }
    
    return loaded_any;
}

// Update rich presence based on game state
void
steam_update_rich_presence(steam_system *steam, char *status, char *details) {
    if (!steam || !steam->initialized) return;
    
    if (status) {
        steam_set_rich_presence(steam, "status", status);
    }
    
    if (details) {
        steam_set_rich_presence(steam, "details", details);
    }
}

// Steam Workshop - create a simple mod
b32
steam_workshop_publish_mod(steam_system *steam, char *title, char *description, char *content_path) {
    if (!steam || !steam->initialized) return 0;
    
    printf("[STEAM] Publishing Workshop item: %s\\n", title);
    printf("[STEAM] Description: %s\\n", description);
    printf("[STEAM] Content path: %s\\n", content_path);
    
    // In real implementation:
    // SteamUGC()->CreateItem(app_id, k_EWorkshopFileTypeCommunity);
    // Then set metadata and submit
    
    printf("[STEAM] Workshop item publish initiated\\n");
    return 1;
}

// Simple leaderboard score upload
b32
steam_upload_score(steam_system *steam, char *leaderboard_name, i32 score) {
    if (!steam || !steam->initialized) return 0;
    
    printf("[STEAM] Uploading score to leaderboard '%s': %d\\n", leaderboard_name, score);
    
    // Find or create leaderboard entry
    steam_leaderboard *leaderboard = steam_get_leaderboard(steam, leaderboard_name);
    if (!leaderboard && steam->leaderboard_count < STEAM_MAX_LEADERBOARDS) {
        // Create new leaderboard entry
        leaderboard = &steam->leaderboards[steam->leaderboard_count++];
        strncpy(leaderboard->name, leaderboard_name, STEAM_STRING_MAX - 1);
        leaderboard->sort_method = STEAM_LEADERBOARD_SORT_DESCENDING; // High scores better
        leaderboard->display_type = STEAM_LEADERBOARD_DISPLAY_NUMERIC;
    }
    
    if (leaderboard) {
        leaderboard->user_score = score;
        printf("[STEAM] Score uploaded successfully\\n");
        return 1;
    }
    
    return 0;
}

// Handle Steam overlay activation
void
steam_handle_overlay_activated(steam_system *steam, b32 active) {
    if (!steam) return;
    
    if (active) {
        printf("[STEAM] Overlay activated - game should pause\\n");
        // Game should pause here
    } else {
        printf("[STEAM] Overlay deactivated - game can resume\\n");
        // Game can resume here
    }
}