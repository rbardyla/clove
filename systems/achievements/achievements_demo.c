/*
    Achievement System Demo
    Shows complete achievement functionality and performance
*/

#include "handmade_achievements.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Simple test functions to simulate gameplay
internal void
simulate_combat_gameplay(achievement_system *system) {
    printf("\\n=== Simulating Combat Gameplay ===\\n");
    
    // Simulate killing enemies
    for (i32 i = 0; i < 150; i++) {
        achievements_add_stat_int(system, "enemies_killed", 1);
        
        // Vary weapon usage
        if (i % 3 == 0) {
            achievements_add_stat_int(system, "sword_kills", 1);
        } else if (i % 3 == 1) {
            achievements_add_stat_int(system, "bow_kills", 1);
        } else {
            achievements_add_stat_int(system, "magic_kills", 1);
        }
        
        // Occasional critical hits
        if (i % 7 == 0) {
            achievements_add_stat_int(system, "critical_hits", 1);
        }
        
        // Damage tracking
        achievements_add_stat_float(system, "damage_dealt", 25.0f + (f32)(rand() % 50));
    }
    
    // Boss fights
    achievements_add_stat_int(system, "bosses_killed", 3);
    achievements_add_stat_float(system, "damage_dealt", 2500.0f);
    
    // Set a high combo
    achievements_set_stat_int(system, "max_combo", 47);
    
    printf("Combat stats updated - check for achievement unlocks!\\n");
}

internal void
simulate_exploration_gameplay(achievement_system *system) {
    printf("\\n=== Simulating Exploration Gameplay ===\\n");
    
    // Travel distance
    achievements_set_stat_float(system, "distance_traveled", 15000.0f);
    
    // Discover areas
    achievements_set_stat_int(system, "areas_discovered", 45);
    achievements_set_stat_float(system, "areas_discovered_percent", 78.5f);
    
    // Find secrets
    achievements_set_stat_int(system, "secrets_found", 12);
    
    // Visit landmarks
    achievements_set_stat_int(system, "landmarks_visited", 22);
    
    printf("Exploration stats updated!\\n");
}

internal void
simulate_collection_gameplay(achievement_system *system) {
    printf("\\n=== Simulating Collection Gameplay ===\\n");
    
    // Collect various items
    achievements_set_stat_int(system, "items_collected", 750);
    achievements_set_stat_int(system, "chests_opened", 65);
    achievements_set_stat_int(system, "coins_collected", 15000);
    achievements_set_stat_int(system, "gems_collected", 150);
    
    // Rare items
    achievements_set_stat_int(system, "rare_items_found", 8);
    achievements_set_stat_int(system, "legendary_items_found", 2);
    
    // Equipment variety
    achievements_set_stat_int(system, "unique_weapons", 35);
    achievements_set_stat_int(system, "unique_armor_sets", 15);
    
    // Collection progress
    achievements_set_stat_float(system, "collection_percentage", 67.3f);
    
    printf("Collection stats updated!\\n");
}

internal void
simulate_character_progression(achievement_system *system) {
    printf("\\n=== Simulating Character Progression ===\\n");
    
    // Level up character
    achievements_set_stat_int(system, "player_level", 28);
    
    // Skills and crafting
    achievements_set_stat_int(system, "skills_learned", 45);
    achievements_set_stat_int(system, "skill_points_earned", 280);
    achievements_set_stat_int(system, "items_crafted", 85);
    
    printf("Character progression updated!\\n");
}

internal void
simulate_story_completion(achievement_system *system) {
    printf("\\n=== Simulating Story Completion ===\\n");
    
    // Manually unlock story achievements
    achievements_unlock(system, "first_steps");
    achievements_unlock(system, "chapter_1");
    achievements_unlock(system, "chapter_2");
    
    printf("Story achievements unlocked!\\n");
}

internal void
test_manual_unlocks(achievement_system *system) {
    printf("\\n=== Testing Manual Achievement Unlocks ===\\n");
    
    achievements_unlock(system, "perfectionist");
    achievements_unlock(system, "mountain_climber");
    achievements_unlock(system, "secret_hunter");
    achievements_unlock(system, "helping_hand");
    
    printf("Manual unlocks completed!\\n");
}

internal void
performance_test_achievements(achievement_system *system) {
    printf("\\n=== Achievement System Performance Test ===\\n");
    
    clock_t start, end;
    
    // Test stat updates (hot path)
    start = clock();
    for (i32 i = 0; i < 100000; i++) {
        achievements_add_stat_int(system, "enemies_killed", 1);
    }
    end = clock();
    double stat_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Stat Updates (100,000 ops): %.2f ms (%.3f μs per update)\\n", 
           stat_time, (stat_time * 1000.0) / 100000.0);
    
    // Test achievement lookups
    start = clock();
    for (i32 i = 0; i < 50000; i++) {
        achievements_find(system, "slayer");
        achievements_find(system, "explorer");
        achievements_find(system, "hoarder");
    }
    end = clock();
    double lookup_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Achievement Lookups (150,000 ops): %.2f ms (%.3f μs per lookup)\\n",
           lookup_time, (lookup_time * 1000.0) / 150000.0);
    
    // Test progress calculations
    start = clock();
    for (i32 i = 0; i < 10000; i++) {
        achievements_get_progress(system, "slayer");
        achievements_get_progress(system, "destroyer");
        achievements_get_progress(system, "collector");
    }
    end = clock();
    double progress_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Progress Calculations (30,000 ops): %.2f ms (%.3f μs per calc)\\n",
           progress_time, (progress_time * 1000.0) / 30000.0);
    
    printf("\\nMemory Usage:\\n");
    printf("  System size: %lu bytes\\n", sizeof(achievement_system));
    printf("  Per achievement: %lu bytes\\n", sizeof(achievement));
    printf("  Per statistic: %lu bytes\\n", sizeof(game_stat));
    printf("  Total achievements: %u\\n", system->achievement_count);
    printf("  Total statistics: %u\\n", system->stat_count);
    printf("  Memory efficiency: %.1f achievements/KB\\n", 
           system->achievement_count / (sizeof(achievement_system) / 1024.0f));
}

// Demo main function
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    printf("=== Handmade Achievement System Demo ===\\n\\n");
    
    // Initialize achievement system
    u32 memory_size = Megabytes(1); // 1MB for achievement system
    void *memory = malloc(memory_size);
    if (!memory) {
        printf("Failed to allocate memory\\n");
        return 1;
    }
    
    achievement_system *achievements = achievements_init(memory, memory_size);
    if (!achievements) {
        printf("Failed to initialize achievement system\\n");
        free(memory);
        return 1;
    }
    
    // Register all default achievements and stats
    achievements_register_all_defaults(achievements);
    
    // Load existing progress (if any)
    achievements_load(achievements);
    achievements_load_stats(achievements);
    
    printf("\\n=== Initial Achievement State ===\\n");
    achievements_print_stats(achievements);
    
    // Simulate various gameplay scenarios
    simulate_combat_gameplay(achievements);
    simulate_exploration_gameplay(achievements);
    simulate_collection_gameplay(achievements);
    simulate_character_progression(achievements);
    simulate_story_completion(achievements);
    test_manual_unlocks(achievements);
    
    // Show current state after simulation
    printf("\\n=== Achievement State After Simulation ===\\n");
    achievements_print_stats(achievements);
    
    // Test specific achievement queries
    printf("\\n=== Achievement Status Queries ===\\n");
    char *test_achievements[] = {
        "first_blood", "slayer", "destroyer", "wanderer", "explorer",
        "hoarder", "collector", "novice", "adept", "achiever"
    };
    
    for (u32 i = 0; i < ArrayCount(test_achievements); i++) {
        char *id = test_achievements[i];
        b32 unlocked = achievements_is_unlocked(achievements, id);
        f32 progress = achievements_get_progress(achievements, id);
        
        printf("  %s: %s", id, unlocked ? "UNLOCKED" : "Locked");
        if (!unlocked && progress > 0) {
            printf(" (%.1f%% progress)", progress);
        }
        printf("\\n");
    }
    
    // Show some key statistics
    printf("\\n=== Key Statistics ===\\n");
    printf("  Enemies Killed: %d\\n", achievements_get_stat_int(achievements, "enemies_killed"));
    printf("  Distance Traveled: %.0fm\\n", achievements_get_stat_float(achievements, "distance_traveled"));
    printf("  Items Collected: %d\\n", achievements_get_stat_int(achievements, "items_collected"));
    printf("  Player Level: %d\\n", achievements_get_stat_int(achievements, "player_level"));
    printf("  Max Combo: %d\\n", achievements_get_stat_int(achievements, "max_combo"));
    
    // Performance testing
    performance_test_achievements(achievements);
    
    // Test file I/O
    printf("\\n=== Testing File I/O ===\\n");
    if (achievements_save(achievements)) {
        printf("✓ Achievements saved successfully\\n");
    }
    
    if (achievements_save_stats(achievements)) {
        printf("✓ Statistics saved successfully\\n");  
    }
    
    // Export to readable format
    if (achievements_export_readable(achievements, "achievement_export.txt")) {
        printf("✓ Achievement export created\\n");
    }
    
    // Test notification system
    printf("\\n=== Testing Notification System ===\\n");
    printf("Active notifications: %u\\n", achievements->notification_count);
    
    // Simulate some time passing for notifications
    for (i32 i = 0; i < 10; i++) {
        achievements_update(achievements, 0.1f); // 100ms per update
    }
    
    printf("Notifications after 1 second: %u\\n", achievements->notification_count);
    
    // Final statistics
    printf("\\n=== Final Summary ===\\n");
    printf("Total Achievements: %u\\n", achievements->achievement_count);
    printf("Unlocked This Session: %u\\n", achievements->achievements_this_session);
    printf("Overall Completion: %.1f%%\\n", achievements->overall_completion);
    printf("Achievement Points: %u\\n", achievements->points_earned);
    
    // Cleanup
    achievements_shutdown(achievements);
    free(memory);
    
    printf("\\nAchievement system demo completed successfully!\\n");
    
    return 0;
}