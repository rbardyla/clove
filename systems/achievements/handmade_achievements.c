/*
    Handmade Achievement System - Core Implementation
    Complete game achievement framework with real-time tracking
*/

#include "handmade_achievements.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define internal static

// Hash function for achievement/stat lookup
internal u32
achievements_hash_string(char *str) {
    u32 hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (u32)*str++;
    }
    return hash;
}

// Get current timestamp
u64
achievements_get_timestamp() {
    return (u64)time(NULL);
}

// Initialize achievement system
achievement_system *
achievements_init(void *memory, u32 memory_size) {
    if (!memory || memory_size < sizeof(achievement_system)) {
        return NULL;
    }
    
    achievement_system *system = (achievement_system *)memory;
    memset(system, 0, sizeof(achievement_system));
    
    system->memory = (u8 *)memory;
    system->memory_size = memory_size;
    
    // Initialize default paths
    strncpy(system->save_path, "achievements.dat", 255);
    strncpy(system->stats_path, "stats.dat", 255);
    
    // Initialize UI state
    system->notifications_enabled = 1;
    system->show_locked = 1;
    system->show_progress = 1;
    system->filter_category = CATEGORY_STORY; // Show all by default
    
    // Initialize categories
    system->category_count = 10; // Number of default categories
    
    char *category_names[] = {
        "Story", "Combat", "Exploration", "Collection", "Skill",
        "Social", "Speedrun", "Hidden", "Meta", "Custom"
    };
    
    char *category_descriptions[] = {
        "Story and campaign progress",
        "Combat and fighting achievements", 
        "World exploration and discovery",
        "Item and collectible gathering",
        "Skill-based challenges",
        "Multiplayer and social features",
        "Speed running and time trials",
        "Secret and hidden achievements",
        "Meta achievements about achievements",
        "Custom game-specific achievements"
    };
    
    for (u32 i = 0; i < system->category_count; i++) {
        achievement_category_info *cat = &system->categories[i];
        strncpy(cat->name, category_names[i], ACHIEVEMENTS_STRING_MAX - 1);
        strncpy(cat->description, category_descriptions[i], ACHIEVEMENTS_STRING_MAX - 1);
        cat->category = (achievement_category)i;
        cat->expanded = (i == 0); // Expand story category by default
    }
    
    system->session_start_time = (f32)achievements_get_timestamp();
    system->auto_save_enabled = 1;
    
    printf("Achievement system initialized (%u KB allocated)\\n", 
           memory_size / 1024);
    
    return system;
}

// Shutdown achievement system
void
achievements_shutdown(achievement_system *system) {
    if (!system) return;
    
    // Auto-save before shutdown
    if (system->auto_save_enabled) {
        achievements_save(system);
        achievements_save_stats(system);
    }
    
    printf("Achievement system shutdown. Session: %u achievements unlocked\\n",
           system->achievements_this_session);
}

// Register a new statistic
void
achievements_register_stat(achievement_system *system, char *name, char *display_name, stat_type type) {
    if (system->stat_count >= ACHIEVEMENTS_MAX_STATS) {
        printf("Warning: Maximum statistics reached\\n");
        return;
    }
    
    u32 index = system->stat_count++;
    game_stat *stat = &system->stats[index];
    
    strncpy(stat->name, name, ACHIEVEMENTS_STRING_MAX - 1);
    strncpy(stat->display_name, display_name, ACHIEVEMENTS_STRING_MAX - 1);
    stat->type = type;
    stat->hash = achievements_hash_string(name);
    stat->last_update = achievements_get_timestamp();
    stat->tracked = 1;
    
    // Initialize default values
    switch (type) {
        case STAT_INT:
            stat->value.int_value = 0;
            stat->default_value.int_default = 0;
            break;
        case STAT_FLOAT:
            stat->value.float_value = 0.0f;
            stat->default_value.float_default = 0.0f;
            break;
        case STAT_TIME:
            stat->value.time_value = 0;
            stat->default_value.time_default = 0;
            break;
        case STAT_BOOL:
            stat->value.bool_value = 0;
            stat->default_value.bool_default = 0;
            break;
    }
}

// Find statistic by name
game_stat *
achievements_find_stat(achievement_system *system, char *name) {
    u32 hash = achievements_hash_string(name);
    
    for (u32 i = 0; i < system->stat_count; i++) {
        if (system->stats[i].hash == hash && 
            strcmp(system->stats[i].name, name) == 0) {
            return &system->stats[i];
        }
    }
    return NULL;
}

// Set integer statistic
void
achievements_set_stat_int(achievement_system *system, char *name, i32 value) {
    game_stat *stat = achievements_find_stat(system, name);
    if (!stat) return;
    
    i32 old_value = stat->value.int_value;
    stat->value.int_value = value;
    stat->session_change = (f32)(value - old_value);
    stat->total_change += stat->session_change;
    stat->last_update = achievements_get_timestamp();
    
    // Trigger achievement progress update
    achievements_update_progress(system, name);
}

// Add to integer statistic
void
achievements_add_stat_int(achievement_system *system, char *name, i32 delta) {
    game_stat *stat = achievements_find_stat(system, name);
    if (!stat) return;
    
    stat->value.int_value += delta;
    stat->session_change += (f32)delta;
    stat->total_change += (f32)delta;
    stat->last_update = achievements_get_timestamp();
    
    achievements_update_progress(system, name);
}

// Set float statistic  
void
achievements_set_stat_float(achievement_system *system, char *name, f32 value) {
    game_stat *stat = achievements_find_stat(system, name);
    if (!stat) return;
    
    f32 old_value = stat->value.float_value;
    stat->value.float_value = value;
    stat->session_change = value - old_value;
    stat->total_change += stat->session_change;
    stat->last_update = achievements_get_timestamp();
    
    achievements_update_progress(system, name);
}

// Add to float statistic
void
achievements_add_stat_float(achievement_system *system, char *name, f32 delta) {
    game_stat *stat = achievements_find_stat(system, name);
    if (!stat) return;
    
    stat->value.float_value += delta;
    stat->session_change += delta;
    stat->total_change += delta;
    stat->last_update = achievements_get_timestamp();
    
    achievements_update_progress(system, name);
}

// Get integer statistic
i32
achievements_get_stat_int(achievement_system *system, char *name) {
    game_stat *stat = achievements_find_stat(system, name);
    return stat ? stat->value.int_value : 0;
}

// Get float statistic
f32
achievements_get_stat_float(achievement_system *system, char *name) {
    game_stat *stat = achievements_find_stat(system, name);
    return stat ? stat->value.float_value : 0.0f;
}

// Register a new achievement
u32
achievements_register(achievement_system *system, char *id, char *name, char *description,
                     achievement_type type, achievement_category category,
                     f32 target_value, char *required_stat) {
    if (system->achievement_count >= ACHIEVEMENTS_MAX_COUNT) {
        printf("Warning: Maximum achievements reached\\n");
        return 0;
    }
    
    u32 index = system->achievement_count++;
    achievement *ach = &system->achievements[index];
    memset(ach, 0, sizeof(achievement));
    
    strncpy(ach->id, id, ACHIEVEMENTS_STRING_MAX - 1);
    strncpy(ach->name, name, ACHIEVEMENTS_STRING_MAX - 1);
    strncpy(ach->description, description, ACHIEVEMENTS_DESCRIPTION_MAX - 1);
    
    ach->type = type;
    ach->category = category;
    ach->target_value = target_value;
    ach->rarity = RARITY_COMMON; // Default rarity
    
    if (required_stat) {
        strncpy(ach->required_stat, required_stat, ACHIEVEMENTS_STRING_MAX - 1);
        ach->flags |= ACHIEVEMENT_TRACKED;
    }
    
    ach->created_time = achievements_get_timestamp();
    ach->hash = achievements_hash_string(id);
    
    // Default reward (can be customized later)
    ach->reward.type = REWARD_XP;
    ach->reward.amount = 100; // Base XP reward
    
    // Add to category
    if (category < ACHIEVEMENTS_MAX_CATEGORIES) {
        achievement_category_info *cat = &system->categories[category];
        if (cat->achievement_count < 128) {
            cat->achievements[cat->achievement_count++] = ach;
            cat->total_count++;
        }
    }
    
    return ach->hash;
}

// Register simple unlock achievement
u32
achievements_register_unlock(achievement_system *system, char *id, char *name, char *description,
                            achievement_category category) {
    return achievements_register(system, id, name, description, 
                               ACHIEVEMENT_UNLOCK, category, 1.0f, NULL);
}

// Register progress-based achievement
u32
achievements_register_progress(achievement_system *system, char *id, char *name, char *description,
                             achievement_category category, char *stat_name, f32 target_value) {
    return achievements_register(system, id, name, description,
                               ACHIEVEMENT_PROGRESS, category, target_value, stat_name);
}

// Register counter achievement
u32
achievements_register_counter(achievement_system *system, char *id, char *name, char *description,
                            achievement_category category, char *stat_name, i32 target_count) {
    return achievements_register(system, id, name, description,
                               ACHIEVEMENT_COUNTER, category, (f32)target_count, stat_name);
}

// Find achievement by ID
achievement *
achievements_find(achievement_system *system, char *achievement_id) {
    u32 hash = achievements_hash_string(achievement_id);
    
    for (u32 i = 0; i < system->achievement_count; i++) {
        if (system->achievements[i].hash == hash &&
            strcmp(system->achievements[i].id, achievement_id) == 0) {
            return &system->achievements[i];
        }
    }
    return NULL;
}

// Check if achievement is unlocked
b32
achievements_is_unlocked(achievement_system *system, char *achievement_id) {
    achievement *ach = achievements_find(system, achievement_id);
    return ach ? (ach->flags & ACHIEVEMENT_UNLOCKED) != 0 : 0;
}

// Get achievement progress percentage
f32
achievements_get_progress(achievement_system *system, char *achievement_id) {
    achievement *ach = achievements_find(system, achievement_id);
    if (!ach || ach->flags & ACHIEVEMENT_UNLOCKED) return 100.0f;
    
    return ach->progress.percentage;
}

// Unlock achievement
b32
achievements_unlock(achievement_system *system, char *achievement_id) {
    achievement *ach = achievements_find(system, achievement_id);
    if (!ach || (ach->flags & ACHIEVEMENT_UNLOCKED)) {
        return 0; // Already unlocked or not found
    }
    
    achievements_trigger_unlock(system, ach);
    return 1;
}

// Trigger achievement unlock with full ceremony
void
achievements_trigger_unlock(achievement_system *system, achievement *ach) {
    ach->flags |= ACHIEVEMENT_UNLOCKED;
    ach->unlock_time = achievements_get_timestamp();
    ach->progress.current = ach->target_value;
    ach->progress.percentage = 100.0f;
    ach->dirty = 1;
    
    // Update system stats
    system->total_unlocked++;
    system->achievements_this_session++;
    system->points_earned += ach->reward.amount;
    
    // Update category completion
    achievement_category_info *cat = &system->categories[ach->category];
    cat->unlocked_count++;
    cat->completion_percentage = (f32)cat->unlocked_count / (f32)cat->total_count * 100.0f;
    
    // Calculate overall completion
    system->overall_completion = (f32)system->total_unlocked / (f32)system->achievement_count * 100.0f;
    
    // Show notification
    if (system->notifications_enabled) {
        achievements_show_notification(system, ach);
    }
    
    // Steam integration
    if (system->steam_enabled && ach->steam_id[0]) {
        // Would call Steam API here
        printf("[STEAM] Achievement unlocked: %s\\n", ach->steam_id);
    }
    
    printf("🏆 Achievement Unlocked: %s\\n", ach->name);
    printf("   %s\\n", ach->description);
    
    if (ach->reward.type != REWARD_NONE) {
        printf("   Reward: ");
        switch (ach->reward.type) {
            case REWARD_XP:
                printf("%d XP\\n", ach->reward.amount);
                break;
            case REWARD_COINS:
                printf("%d Coins\\n", ach->reward.amount);
                break;
            case REWARD_ITEM:
                printf("Item: %s\\n", ach->reward.item_id);
                break;
            default:
                printf("Special reward\\n");
                break;
        }
    }
}

// Update progress for achievements tracking a specific stat
void
achievements_update_progress(achievement_system *system, char *stat_name) {
    game_stat *stat = achievements_find_stat(system, stat_name);
    if (!stat) return;
    
    for (u32 i = 0; i < system->achievement_count; i++) {
        achievement *ach = &system->achievements[i];
        
        // Skip already unlocked achievements
        if (ach->flags & ACHIEVEMENT_UNLOCKED) continue;
        
        // Skip achievements not tracking this stat
        if (strcmp(ach->required_stat, stat_name) != 0) continue;
        
        f32 current_value = 0.0f;
        
        switch (stat->type) {
            case STAT_INT:
                current_value = (f32)stat->value.int_value;
                break;
            case STAT_FLOAT:
                current_value = stat->value.float_value;
                break;
            case STAT_TIME:
                current_value = (f32)stat->value.time_value;
                break;
            case STAT_BOOL:
                current_value = stat->value.bool_value ? 1.0f : 0.0f;
                break;
        }
        
        // Update progress
        ach->progress.current = current_value;
        if (ach->progress.first_progress_time == 0 && current_value > 0) {
            ach->progress.first_progress_time = achievements_get_timestamp();
        }
        ach->progress.last_update_time = achievements_get_timestamp();
        
        // Calculate percentage
        if (ach->target_value > 0) {
            ach->progress.percentage = (current_value / ach->target_value) * 100.0f;
            if (ach->progress.percentage > 100.0f) ach->progress.percentage = 100.0f;
        }
        
        // Check for unlock
        if (current_value >= ach->target_value) {
            achievements_trigger_unlock(system, ach);
        }
        
        ach->dirty = 1;
    }
}

// Check all unlock conditions (called periodically)
void
achievements_check_unlock_conditions(achievement_system *system) {
    for (u32 i = 0; i < system->achievement_count; i++) {
        achievement *ach = &system->achievements[i];
        
        if (ach->flags & ACHIEVEMENT_UNLOCKED) continue;
        
        b32 should_unlock = 0;
        
        switch (ach->type) {
            case ACHIEVEMENT_UNLOCK:
                // Manual unlock only
                break;
                
            case ACHIEVEMENT_PROGRESS:
            case ACHIEVEMENT_COUNTER:
                // Handled by achievements_update_progress
                break;
                
            case ACHIEVEMENT_MILESTONE:
                // Time-based milestone
                if (ach->target_value > 0) {
                    f32 elapsed = (f32)(achievements_get_timestamp() - system->session_start_time);
                    if (elapsed >= ach->target_value) {
                        should_unlock = 1;
                    }
                }
                break;
                
            case ACHIEVEMENT_META:
                // Meta achievement (unlock X other achievements)
                if (system->total_unlocked >= (u32)ach->target_value) {
                    should_unlock = 1;
                }
                break;
        }
        
        if (should_unlock) {
            achievements_trigger_unlock(system, ach);
        }
    }
}

// Add achievement notification
void
achievements_show_notification(achievement_system *system, achievement *ach) {
    if (system->notification_count >= ArrayCount(system->notifications)) {
        return; // Too many notifications
    }
    
    u32 index = system->notification_count++;
    achievement_notification *notif = &system->notifications[index];
    
    notif->achievement = ach;
    snprintf(notif->message, ACHIEVEMENTS_DESCRIPTION_MAX,
             "Achievement Unlocked: %s", ach->name);
    notif->display_time = 5.0f; // Show for 5 seconds
    notif->fade_time = 1.0f;    // Fade over 1 second
    notif->active = 1;
    notif->animation_state = 0; // Starting animation
}

// Update system state
void
achievements_update(achievement_system *system, f32 dt) {
    // Update notifications
    achievements_update_notifications(system, dt);
    
    // Periodic unlock condition checking (every second)
    static f32 check_timer = 0.0f;
    check_timer += dt;
    if (check_timer >= 1.0f) {
        achievements_check_unlock_conditions(system);
        check_timer = 0.0f;
    }
    
    // Auto-save (every 5 minutes)
    if (system->auto_save_enabled) {
        static f32 save_timer = 0.0f;
        save_timer += dt;
        if (save_timer >= 300.0f) { // 5 minutes
            achievements_save(system);
            achievements_save_stats(system);
            save_timer = 0.0f;
        }
    }
}

// Update notification display
void
achievements_update_notifications(achievement_system *system, f32 dt) {
    for (u32 i = 0; i < system->notification_count; i++) {
        achievement_notification *notif = &system->notifications[i];
        
        if (!notif->active) continue;
        
        notif->display_time -= dt;
        
        if (notif->display_time <= 0.0f) {
            notif->active = 0;
            // Shift remaining notifications down
            for (u32 j = i; j < system->notification_count - 1; j++) {
                system->notifications[j] = system->notifications[j + 1];
            }
            system->notification_count--;
            i--; // Recheck this index
        }
    }
}

// Get total completion percentage
f32
achievements_get_completion_percentage(achievement_system *system) {
    if (system->achievement_count == 0) return 100.0f;
    return (f32)system->total_unlocked / (f32)system->achievement_count * 100.0f;
}

// Get total achievement points
u32
achievements_get_total_points(achievement_system *system) {
    return system->points_earned;
}

// Print achievement statistics
void
achievements_print_stats(achievement_system *system) {
    printf("\\n=== Achievement Statistics ===\\n");
    printf("Total achievements: %u\\n", system->achievement_count);
    printf("Unlocked: %u (%.1f%%)\\n", system->total_unlocked, 
           achievements_get_completion_percentage(system));
    printf("Points earned: %u\\n", system->points_earned);
    printf("Session unlocks: %u\\n", system->achievements_this_session);
    
    printf("\\nBy category:\\n");
    for (u32 i = 0; i < system->category_count; i++) {
        achievement_category_info *cat = &system->categories[i];
        if (cat->total_count > 0) {
            printf("  %s: %u/%u (%.1f%%)\\n", cat->name,
                   cat->unlocked_count, cat->total_count, cat->completion_percentage);
        }
    }
}