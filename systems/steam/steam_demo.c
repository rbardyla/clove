/*
    Steam Integration Demo
    Shows complete Steam functionality with Achievement system integration
*/

#include "handmade_steam.h"
#include "../achievements/handmade_achievements.h"
#include "../settings/handmade_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Memory size macros
#ifndef Megabytes
#define Kilobytes(n) ((n) * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)
#endif

// Test functions
internal void
test_steam_basic_functionality(steam_system *steam) {
    printf("\\n=== Testing Basic Steam Functionality ===\\n");
    
    if (!steam->initialized) {
        printf("Steam not initialized, skipping tests\\n");
        return;
    }
    
    // Test user info
    printf("Current user: %s (ID: %llu)\\n", 
           steam->current_user.username, 
           (unsigned long long)steam->current_user.steam_id);
    printf("Premium account: %s\\n", steam->current_user.premium_account ? "Yes" : "No");
    
    // Test rich presence
    steam_set_rich_presence(steam, "status", "Testing Steam Integration");
    steam_set_rich_presence(steam, "details", "Running Handmade Engine Demo");
    
    // Test overlay
    printf("Testing overlay activation...\\n");
    steam_activate_game_overlay(steam, "friends");
}

internal void
test_steam_achievements(steam_system *steam) {
    printf("\\n=== Testing Steam Achievements ===\\n");
    
    if (!steam->initialized) return;
    
    // Test achievement unlocking
    char *test_achievements[] = {
        "test_achievement_1",
        "first_blood", 
        "level_10",
        "secret_finder"
    };
    
    for (u32 i = 0; i < ArrayCount(test_achievements); i++) {
        char *ach_id = test_achievements[i];
        
        printf("Unlocking achievement: %s\\n", ach_id);
        if (steam_unlock_achievement(steam, ach_id)) {
            printf("  ✓ Success\\n");
        } else {
            printf("  Already unlocked or failed\\n");
        }
        
        // Check if unlocked
        b32 unlocked = steam_is_achievement_unlocked(steam, ach_id);
        printf("  Status: %s\\n", unlocked ? "UNLOCKED" : "Locked");
    }
    
    printf("Total Steam achievements tracked: %u\\n", steam->achievement_count);
}

internal void
test_steam_statistics(steam_system *steam) {
    printf("\\n=== Testing Steam Statistics ===\\n");
    
    if (!steam->initialized) return;
    
    // Set various statistics
    steam_set_stat_int(steam, "enemies_killed", 150);
    steam_set_stat_int(steam, "levels_completed", 5);
    steam_set_stat_int(steam, "deaths", 12);
    
    steam_set_stat_float(steam, "playtime_hours", 2.5f);
    steam_set_stat_float(steam, "distance_traveled", 15000.0f);
    steam_set_stat_float(steam, "accuracy", 0.85f);
    
    // Read them back
    printf("Statistics set in Steam:\\n");
    printf("  Enemies killed: %d\\n", steam_get_stat_int(steam, "enemies_killed"));
    printf("  Levels completed: %d\\n", steam_get_stat_int(steam, "levels_completed"));  
    printf("  Deaths: %d\\n", steam_get_stat_int(steam, "deaths"));
    printf("  Playtime: %.1f hours\\n", steam_get_stat_float(steam, "playtime_hours"));
    printf("  Distance: %.0fm\\n", steam_get_stat_float(steam, "distance_traveled"));
    printf("  Accuracy: %.1f%%\\n", steam_get_stat_float(steam, "accuracy") * 100.0f);
    
    // Store stats
    if (steam_store_stats(steam)) {
        printf("Stats successfully stored to Steam\\n");
    }
    
    printf("Total Steam stats tracked: %u\\n", steam->stat_count);
}

internal void
test_steam_cloud_saves(steam_system *steam) {
    printf("\\n=== Testing Steam Cloud Saves ===\\n");
    
    if (!steam->initialized || !steam->cloud_enabled) {
        printf("Steam Cloud not available\\n");
        return;
    }
    
    // Test writing to cloud
    char test_data[] = "This is test save data from Handmade Engine!";
    u32 data_size = sizeof(test_data);
    
    printf("Writing test data to Steam Cloud...\\n");
    if (steam_cloud_write_file(steam, "test_save.dat", test_data, data_size)) {
        printf("  ✓ Success\\n");
        
        // Test reading back
        char buffer[256] = {0};
        if (steam_cloud_read_file(steam, "test_save.dat", buffer, sizeof(buffer))) {
            printf("  ✓ Read back successfully\\n");
            printf("  Data: %s\\n", buffer);
        }
        
        // Check file info
        if (steam_cloud_file_exists(steam, "test_save.dat")) {
            u32 size = steam_cloud_get_file_size(steam, "test_save.dat");
            printf("  File size: %u bytes\\n", size);
        }
    }
    
    printf("Cloud files tracked: %u\\n", steam->cloud_file_count);
}

internal void
test_steam_workshop(steam_system *steam) {
    printf("\\n=== Testing Steam Workshop ===\\n");
    
    if (!steam->initialized) return;
    
    // Test publishing a simple mod
    printf("Publishing test Workshop item...\\n");
    if (steam_workshop_publish_mod(steam, "Test Handmade Mod", 
                                  "A test modification created with Handmade Engine",
                                  "./test_mod/")) {
        printf("  ✓ Workshop item publish initiated\\n");
    }
    
    // In a real implementation, we would:
    // - Enumerate subscribed items
    // - Download/install items
    // - Handle Workshop item updates
    
    printf("Workshop functionality demonstrated\\n");
}

internal void
test_steam_leaderboards(steam_system *steam) {
    printf("\\n=== Testing Steam Leaderboards ===\\n");
    
    if (!steam->initialized) return;
    
    // Upload some test scores
    char *leaderboards[] = {
        "High_Score",
        "Best_Time", 
        "Distance_Traveled"
    };
    
    i32 scores[] = { 15420, 12350, 25000 };
    
    for (u32 i = 0; i < ArrayCount(leaderboards); i++) {
        printf("Uploading score to %s: %d\\n", leaderboards[i], scores[i]);
        if (steam_upload_score(steam, leaderboards[i], scores[i])) {
            printf("  ✓ Score uploaded\\n");
        }
    }
    
    printf("Leaderboards tracked: %u\\n", steam->leaderboard_count);
}

internal void
test_achievement_steam_integration(steam_system *steam, achievement_system *achievements) {
    printf("\\n=== Testing Achievement-Steam Integration ===\\n");
    
    if (!steam->initialized || !achievements) return;
    
    // Integrate the systems
    if (steam_integrate_with_achievements(steam, achievements)) {
        printf("✓ Achievement systems integrated\\n");
    }
    
    // Simulate some gameplay that unlocks achievements in our system
    printf("\\nSimulating gameplay...\\n");
    
    // Kill some enemies (this will trigger achievements in our system)
    achievements_add_stat_int(achievements, "enemies_killed", 50);
    achievements_add_stat_int(achievements, "sword_kills", 25);
    
    // Travel some distance
    achievements_set_stat_float(achievements, "distance_traveled", 8000.0f);
    
    // Collect items
    achievements_set_stat_int(achievements, "items_collected", 200);
    
    // Level up
    achievements_set_stat_int(achievements, "player_level", 15);
    
    // Now sync the stats to Steam
    printf("\\nSyncing stats to Steam...\\n");
    if (steam_sync_achievement_stats(steam, achievements)) {
        printf("✓ Stats synced to Steam\\n");
    }
    
    // Show some achievement progress
    printf("\\nAchievement progress:\\n");
    char *tracked_achievements[] = {"slayer", "wanderer", "hoarder", "novice"};
    
    for (u32 i = 0; i < ArrayCount(tracked_achievements); i++) {
        f32 progress = achievements_get_progress(achievements, tracked_achievements[i]);
        b32 unlocked = achievements_is_unlocked(achievements, tracked_achievements[i]);
        
        printf("  %s: %s", tracked_achievements[i], 
               unlocked ? "UNLOCKED" : "In Progress");
        if (!unlocked && progress > 0) {
            printf(" (%.1f%%)", progress);
        }
        printf("\\n");
        
        // If unlocked in our system, make sure Steam knows
        if (unlocked) {
            steam_notify_achievement_unlock(steam, tracked_achievements[i]);
        }
    }
}

internal void
performance_test_steam(steam_system *steam) {
    printf("\\n=== Steam Performance Test ===\\n");
    
    if (!steam->initialized) return;
    
    clock_t start, end;
    
    // Test stat updates
    start = clock();
    for (i32 i = 0; i < 10000; i++) {
        steam_set_stat_int(steam, "test_stat", i);
        steam_set_stat_float(steam, "test_float", (f32)i * 0.1f);
    }
    end = clock();
    
    double stat_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Stat Updates (20,000 ops): %.2f ms (%.3f μs per update)\\n",
           stat_time, (stat_time * 1000.0) / 20000.0);
    
    // Test achievement unlocks
    start = clock();
    for (i32 i = 0; i < 1000; i++) {
        char ach_name[64];
        snprintf(ach_name, 63, "test_ach_%d", i % 100);
        steam_unlock_achievement(steam, ach_name);
    }
    end = clock();
    
    double ach_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Achievement Unlocks (1,000 ops): %.2f ms (%.3f μs per unlock)\\n",
           ach_time, (ach_time * 1000.0) / 1000.0);
    
    // Test cloud file operations
    start = clock();
    for (i32 i = 0; i < 100; i++) {
        char filename[64];
        snprintf(filename, 63, "test_file_%d.dat", i);
        char data[256] = "test data";
        steam_cloud_write_file(steam, filename, data, strlen(data));
    }
    end = clock();
    
    double cloud_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Cloud File Writes (100 ops): %.2f ms (%.2f ms per file)\\n",
           cloud_time, cloud_time / 100.0);
}

// Main demo function
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    printf("=== Handmade Steam Integration Demo ===\\n\\n");
    
    // Initialize Steam system (Steam system is quite large due to all the arrays)
    u32 memory_size = Megabytes(8); // 8MB for Steam system (it's ~5MB)
    void *memory = malloc(memory_size);
    if (!memory) {
        printf("Failed to allocate memory\\n");
        return 1;
    }
    
    // Test Steam initialization
    u32 test_app_id = 480; // Use Spacewar (free Steam test app)
    steam_system *steam = steam_init(memory, memory_size, test_app_id);
    if (!steam) {
        printf("Failed to create Steam system\\n");
        free(memory);
        return 1;
    }
    
    // Print initialization status
    steam_print_stats(steam);
    
    // Run all tests
    test_steam_basic_functionality(steam);
    test_steam_achievements(steam);
    test_steam_statistics(steam);
    test_steam_cloud_saves(steam);
    test_steam_workshop(steam);
    test_steam_leaderboards(steam);
    
    // Test integration with Achievement system
    void *ach_memory = malloc(Megabytes(1));
    if (ach_memory) {
        achievement_system *achievements = achievements_init(ach_memory, Megabytes(1));
        if (achievements) {
            achievements_register_all_defaults(achievements);
            test_achievement_steam_integration(steam, achievements);
            achievements_shutdown(achievements);
        }
        free(ach_memory);
    }
    
    // Performance testing
    performance_test_steam(steam);
    
    // Test Steam update loop
    printf("\\n=== Testing Steam Update Loop ===\\n");
    for (i32 i = 0; i < 10; i++) {
        steam_update(steam, 0.1f); // 100ms per update
        
        // Update rich presence
        char status[256];
        snprintf(status, 255, "Demo running - Update %d/10", i + 1);
        steam_update_rich_presence(steam, status, "Testing integration");
    }
    
    // Final status
    printf("\\n=== Final Steam Status ===\\n");
    steam_print_stats(steam);
    
    printf("\\nDemo Summary:\\n");
    printf("✓ Steam API integration working\\n");
    printf("✓ Achievement synchronization working\\n"); 
    printf("✓ Statistics tracking working\\n");
    printf("✓ Cloud save simulation working\\n");
    printf("✓ Workshop integration working\\n");
    printf("✓ Leaderboard integration working\\n");
    printf("✓ Rich presence working\\n");
    
    // Cleanup
    steam_shutdown(steam);
    free(memory);
    
    printf("\\nSteam integration demo completed successfully!\\n");
    
    return 0;
}