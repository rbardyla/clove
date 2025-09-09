/*
    Achievement File I/O Implementation
    Binary format with validation and Steam sync
*/

#include "handmade_achievements.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal static

// Achievement file header
typedef struct achievements_file_header {
    u32 magic;              // ACHIEVEMENTS_MAGIC_NUMBER
    u32 version;            // ACHIEVEMENTS_VERSION
    u32 achievement_count;  // Number of achievements
    u32 stat_count;        // Number of statistics
    u32 checksum;          // CRC32 of data
    u64 save_timestamp;    // When saved
    u32 total_unlocked;    // Quick stats
    f32 completion_percentage;
    u32 points_earned;
} achievements_file_header;

// Simple CRC32 for data validation
internal u32
achievements_crc32(u8 *data, u32 size) {
    u32 crc = 0xFFFFFFFF;
    
    for (u32 i = 0; i < size; i++) {
        crc = crc ^ data[i];
        for (u32 j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

// Save achievements to file
b32
achievements_save(achievement_system *system) {
    FILE *file = fopen(system->save_path, "wb");
    if (!file) {
        printf("Error: Cannot open achievements file for writing: %s\\n", system->save_path);
        return 0;
    }
    
    // Write header
    achievements_file_header header = {0};
    header.magic = ACHIEVEMENTS_MAGIC_NUMBER;
    header.version = ACHIEVEMENTS_VERSION;
    header.achievement_count = system->achievement_count;
    header.stat_count = system->stat_count;
    header.save_timestamp = achievements_get_timestamp();
    header.total_unlocked = system->total_unlocked;
    header.completion_percentage = system->overall_completion;
    header.points_earned = system->points_earned;
    
    // Calculate checksum later
    fwrite(&header, sizeof(header), 1, file);
    
    // Write achievement data (only unlock status and progress)
    u32 saved_count = 0;
    for (u32 i = 0; i < system->achievement_count; i++) {
        achievement *ach = &system->achievements[i];
        
        // Write achievement ID and unlock data
        fwrite(ach->id, ACHIEVEMENTS_STRING_MAX, 1, file);
        fwrite(&ach->flags, sizeof(ach->flags), 1, file);
        fwrite(&ach->unlock_time, sizeof(ach->unlock_time), 1, file);
        fwrite(&ach->progress, sizeof(ach->progress), 1, file);
        
        saved_count++;
        ach->dirty = 0; // Mark as clean
    }
    
    fclose(file);
    system->last_save_time = achievements_get_timestamp();
    
    printf("Achievements saved: %u achievements, %u unlocked\\n", 
           saved_count, system->total_unlocked);
    return 1;
}

// Load achievements from file
b32
achievements_load(achievement_system *system) {
    FILE *file = fopen(system->save_path, "rb");
    if (!file) {
        printf("Achievement save file not found: %s (starting fresh)\\n", system->save_path);
        return 0; // Not an error for first run
    }
    
    // Read header
    achievements_file_header header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        printf("Error: Cannot read achievement file header\\n");
        fclose(file);
        return 0;
    }
    
    // Validate header
    if (header.magic != ACHIEVEMENTS_MAGIC_NUMBER) {
        printf("Error: Invalid achievement file magic number\\n");
        fclose(file);
        return 0;
    }
    
    if (header.version != ACHIEVEMENTS_VERSION) {
        printf("Warning: Achievement file version mismatch: %u (expected %u)\\n",
               header.version, ACHIEVEMENTS_VERSION);
        // Continue anyway, try to load what we can
    }
    
    printf("Loading achievements: %u achievements, %u unlocked (%.1f%% complete)\\n",
           header.achievement_count, header.total_unlocked, header.completion_percentage);
    
    // Load achievement data
    u32 loaded_count = 0;
    for (u32 i = 0; i < header.achievement_count && i < system->achievement_count; i++) {
        char id[ACHIEVEMENTS_STRING_MAX];
        u32 flags;
        u64 unlock_time;
        achievement_progress progress;
        
        if (fread(id, ACHIEVEMENTS_STRING_MAX, 1, file) != 1) break;
        if (fread(&flags, sizeof(flags), 1, file) != 1) break;
        if (fread(&unlock_time, sizeof(unlock_time), 1, file) != 1) break;
        if (fread(&progress, sizeof(progress), 1, file) != 1) break;
        
        // Find matching achievement in current system
        achievement *ach = achievements_find(system, id);
        if (ach) {
            ach->flags = flags;
            ach->unlock_time = unlock_time;
            ach->progress = progress;
            loaded_count++;
            
            // Update system counters if unlocked
            if (flags & ACHIEVEMENT_UNLOCKED) {
                system->total_unlocked++;
                system->points_earned += ach->reward.amount;
                
                // Update category
                achievement_category_info *cat = &system->categories[ach->category];
                cat->unlocked_count++;
            }
        } else {
            printf("Warning: Unknown achievement in save file: %s\\n", id);
        }
    }
    
    fclose(file);
    
    // Recalculate completion percentages
    system->overall_completion = achievements_get_completion_percentage(system);
    
    for (u32 i = 0; i < system->category_count; i++) {
        achievement_category_info *cat = &system->categories[i];
        if (cat->total_count > 0) {
            cat->completion_percentage = (f32)cat->unlocked_count / (f32)cat->total_count * 100.0f;
        }
    }
    
    printf("Loaded %u achievement states\\n", loaded_count);
    return 1;
}

// Save statistics to separate file
b32
achievements_save_stats(achievement_system *system) {
    FILE *file = fopen(system->stats_path, "wb");
    if (!file) {
        printf("Error: Cannot open statistics file for writing: %s\\n", system->stats_path);
        return 0;
    }
    
    // Simple header
    u32 magic = ACHIEVEMENTS_MAGIC_NUMBER;
    u32 version = ACHIEVEMENTS_VERSION;
    u32 stat_count = system->stat_count;
    u64 timestamp = achievements_get_timestamp();
    
    fwrite(&magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&stat_count, sizeof(stat_count), 1, file);
    fwrite(&timestamp, sizeof(timestamp), 1, file);
    
    // Write all statistics
    u32 saved_count = 0;
    for (u32 i = 0; i < system->stat_count; i++) {
        game_stat *stat = &system->stats[i];
        
        fwrite(stat->name, ACHIEVEMENTS_STRING_MAX, 1, file);
        fwrite(&stat->type, sizeof(stat->type), 1, file);
        fwrite(&stat->value, sizeof(stat->value), 1, file);
        fwrite(&stat->total_change, sizeof(stat->total_change), 1, file);
        fwrite(&stat->last_update, sizeof(stat->last_update), 1, file);
        
        saved_count++;
    }
    
    fclose(file);
    printf("Statistics saved: %u stats\\n", saved_count);
    return 1;
}

// Load statistics from file
b32
achievements_load_stats(achievement_system *system) {
    FILE *file = fopen(system->stats_path, "rb");
    if (!file) {
        printf("Statistics file not found: %s (starting fresh)\\n", system->stats_path);
        return 0; // Not an error for first run
    }
    
    // Read header
    u32 magic, version, stat_count;
    u64 timestamp;
    
    if (fread(&magic, sizeof(magic), 1, file) != 1 ||
        fread(&version, sizeof(version), 1, file) != 1 ||
        fread(&stat_count, sizeof(stat_count), 1, file) != 1 ||
        fread(&timestamp, sizeof(timestamp), 1, file) != 1) {
        printf("Error: Cannot read statistics file header\\n");
        fclose(file);
        return 0;
    }
    
    if (magic != ACHIEVEMENTS_MAGIC_NUMBER) {
        printf("Error: Invalid statistics file magic number\\n");
        fclose(file);
        return 0;
    }
    
    printf("Loading statistics: %u stats\\n", stat_count);
    
    // Load statistics data
    u32 loaded_count = 0;
    for (u32 i = 0; i < stat_count && i < system->stat_count; i++) {
        char name[ACHIEVEMENTS_STRING_MAX];
        stat_type type;
        union {
            i32 int_value;
            f32 float_value;
            u64 time_value;
            b32 bool_value;
        } value;
        f32 total_change;
        u64 last_update;
        
        if (fread(name, ACHIEVEMENTS_STRING_MAX, 1, file) != 1) break;
        if (fread(&type, sizeof(type), 1, file) != 1) break;
        if (fread(&value, sizeof(value), 1, file) != 1) break;
        if (fread(&total_change, sizeof(total_change), 1, file) != 1) break;
        if (fread(&last_update, sizeof(last_update), 1, file) != 1) break;
        
        // Find matching statistic
        game_stat *stat = achievements_find_stat(system, name);
        if (stat) {
            // Assign value based on type
            switch (stat->type) {
                case STAT_INT:
                    stat->value.int_value = value.int_value;
                    break;
                case STAT_FLOAT:
                    stat->value.float_value = value.float_value;
                    break;
                case STAT_TIME:
                    stat->value.time_value = value.time_value;
                    break;
                case STAT_BOOL:
                    stat->value.bool_value = value.bool_value;
                    break;
            }
            stat->total_change = total_change;
            stat->last_update = last_update;
            loaded_count++;
        } else {
            printf("Warning: Unknown statistic in save file: %s\\n", name);
        }
    }
    
    fclose(file);
    printf("Loaded %u statistic values\\n", loaded_count);
    return 1;
}

// Export achievements to human-readable format
b32
achievements_export_readable(achievement_system *system, char *path) {
    FILE *file = fopen(path, "w");
    if (!file) {
        printf("Error: Cannot create export file: %s\\n", path);
        return 0;
    }
    
    fprintf(file, "HANDMADE ENGINE - ACHIEVEMENT EXPORT\\n");
    fprintf(file, "Generated: %llu\\n\\n", (unsigned long long)achievements_get_timestamp());
    
    fprintf(file, "SUMMARY\\n");
    fprintf(file, "=======\\n");
    fprintf(file, "Total Achievements: %u\\n", system->achievement_count);
    fprintf(file, "Unlocked: %u (%.1f%%)\\n", system->total_unlocked, system->overall_completion);
    fprintf(file, "Points Earned: %u\\n", system->points_earned);
    fprintf(file, "Session Unlocks: %u\\n\\n", system->achievements_this_session);
    
    // Export by category
    for (u32 cat_idx = 0; cat_idx < system->category_count; cat_idx++) {
        achievement_category_info *cat = &system->categories[cat_idx];
        if (cat->total_count == 0) continue;
        
        fprintf(file, "%s (%u/%u - %.1f%%)\\n", cat->name, 
                cat->unlocked_count, cat->total_count, cat->completion_percentage);
        fprintf(file, "%s\\n", cat->description);
        
        for (u32 i = 0; i < cat->achievement_count; i++) {
            achievement *ach = cat->achievements[i];
            
            fprintf(file, "  [%c] %s\\n", 
                    (ach->flags & ACHIEVEMENT_UNLOCKED) ? 'X' : ' ', ach->name);
            fprintf(file, "      %s\\n", ach->description);
            
            if (ach->flags & ACHIEVEMENT_UNLOCKED) {
                fprintf(file, "      Unlocked: %llu\\n", (unsigned long long)ach->unlock_time);
            } else if (ach->progress.percentage > 0) {
                fprintf(file, "      Progress: %.1f%% (%.1f/%.1f)\\n",
                        ach->progress.percentage, ach->progress.current, ach->target_value);
            }
            fprintf(file, "\\n");
        }
        fprintf(file, "\\n");
    }
    
    // Export statistics
    fprintf(file, "STATISTICS\\n");
    fprintf(file, "==========\\n");
    for (u32 i = 0; i < system->stat_count; i++) {
        game_stat *stat = &system->stats[i];
        
        fprintf(file, "%s: ", stat->display_name);
        switch (stat->type) {
            case STAT_INT:
                fprintf(file, "%d\\n", stat->value.int_value);
                break;
            case STAT_FLOAT:
                fprintf(file, "%.2f\\n", stat->value.float_value);
                break;
            case STAT_TIME:
                fprintf(file, "%llu seconds\\n", (unsigned long long)stat->value.time_value);
                break;
            case STAT_BOOL:
                fprintf(file, "%s\\n", stat->value.bool_value ? "Yes" : "No");
                break;
        }
    }
    
    fclose(file);
    printf("Achievements exported to: %s\\n", path);
    return 1;
}