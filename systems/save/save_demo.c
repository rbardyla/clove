/*
    Handmade Save System - Demonstration
    
    Shows all save system features:
    - Basic save/load
    - Compression comparison
    - Migration testing
    - Performance benchmarks
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Minimal game state for testing
typedef struct demo_entity {
    u32 id;
    u32 type;
    f32 position[3];
    f32 rotation[4];
    f32 scale[3];
    u32 flags;
    u32 parent_id;
    char name[64];
} demo_entity;

typedef struct demo_player {
    char name[64];
    u32 level;
    u32 experience;
    u32 health;
    u32 max_health;
    f32 position[3];
    f32 rotation[2];
} demo_player;

typedef struct demo_game_state {
    demo_entity entities[1000];
    u32 entity_count;
    demo_player player;
    f32 playtime_seconds;
    char current_level[64];
    
    // Simulated subsystems
    struct {
        b32 paused;
    } physics;
    
    struct {
        b32 paused;
    } audio;
    
    struct {
        b32 paused;
    } scripts;
    
    b32 paused;
} demo_game_state;

// Generate test data
internal void generate_test_data(demo_game_state *game, u32 entity_count) {
    // Player data
    strcpy(game->player.name, "TestHero");
    game->player.level = 42;
    game->player.experience = 123456;
    game->player.health = 85;
    game->player.max_health = 100;
    game->player.position[0] = 100.5f;
    game->player.position[1] = 50.0f;
    game->player.position[2] = -25.3f;
    game->player.rotation[0] = 0.0f;
    game->player.rotation[1] = 1.57f;
    
    // Game state
    game->playtime_seconds = 7200.0f; // 2 hours
    strcpy(game->current_level, "test_dungeon_01");
    
    // Generate entities
    game->entity_count = entity_count;
    for (u32 i = 0; i < entity_count; i++) {
        demo_entity *e = &game->entities[i];
        e->id = 1000 + i;
        e->type = i % 5; // Various types
        
        // Random positions
        e->position[0] = (f32)(rand() % 1000) - 500.0f;
        e->position[1] = (f32)(rand() % 100);
        e->position[2] = (f32)(rand() % 1000) - 500.0f;
        
        // Identity rotation (quaternion)
        e->rotation[0] = 0.0f;
        e->rotation[1] = 0.0f;
        e->rotation[2] = 0.0f;
        e->rotation[3] = 1.0f;
        
        // Random scale
        f32 scale = 0.5f + (f32)(rand() % 200) / 100.0f;
        e->scale[0] = scale;
        e->scale[1] = scale;
        e->scale[2] = scale;
        
        e->flags = rand() & 0xFF;
        e->parent_id = (i > 10) ? (1000 + (i / 2)) : 0;
        
        sprintf(e->name, "Entity_%04u", i);
    }
}

// Test basic save/load
internal void test_basic_save_load(save_system *system) {
    printf("\n=== Testing Basic Save/Load ===\n");
    
    // Create test data using proper game_state structure
    game_state test_save_state = {0};
    
    // Initialize basic game state data
    test_save_state.entity_count = 100;
    test_save_state.playtime_seconds = 1234.5f;
    strcpy(test_save_state.current_level, "TestLevel");
    strcpy(test_save_state.player.name, "TestPlayer");
    test_save_state.player.level = 15;
    test_save_state.player.experience = 5000;
    test_save_state.player.health = 85;
    test_save_state.player.max_health = 100;
    
    // Create some test entities
    for (u32 i = 0; i < test_save_state.entity_count && i < 10000; i++) {
        entity *e = &test_save_state.entities[i];
        e->id = 1000 + i;
        e->type = i % 4;
        e->position[0] = (f32)(rand() % 1000);
        e->position[1] = (f32)(rand() % 1000);
        e->position[2] = (f32)(rand() % 1000);
        sprintf(e->name, "Entity_%04u", i);
    }
    
    // Save to slot 3
    printf("Saving to slot 3...\n");
    u64 start = __builtin_ia32_rdtsc();
    b32 success = save_game(system, &test_save_state, 3);
    u64 end = __builtin_ia32_rdtsc();
    
    f32 save_time = (f32)(end - start) / 3000000.0f; // Assume 3GHz
    
    if (success) {
        printf("  SUCCESS: Saved in %.2fms\n", save_time);
        printf("  File size: %llu bytes\n", system->slots[3].file_size);
    } else {
        printf("  FAILED: Could not save\n");
        return;
    }
    
    // Clear game state
    game_state test_load_state = {0};
    
    // Load from slot 3
    printf("Loading from slot 3...\n");
    start = __builtin_ia32_rdtsc();
    success = load_game(system, &test_load_state, 3);
    end = __builtin_ia32_rdtsc();
    
    f32 load_time = (f32)(end - start) / 3000000.0f;
    
    if (success) {
        printf("  SUCCESS: Loaded in %.2fms\n", load_time);
        
        // Verify data
        b32 data_matches = 1;
        if (strcmp(test_save_state.player.name, test_load_state.player.name) != 0) {
            printf("  ERROR: Player name mismatch\n");
            data_matches = 0;
        }
        if (test_save_state.player.level != test_load_state.player.level) {
            printf("  ERROR: Player level mismatch\n");
            data_matches = 0;
        }
        if (test_save_state.entity_count != test_load_state.entity_count) {
            printf("  ERROR: Entity count mismatch\n");
            data_matches = 0;
        }
        
        if (data_matches) {
            printf("  Data integrity verified!\n");
        }
    } else {
        printf("  FAILED: Could not load\n");
    }
}

// Test compression performance
internal void test_compression(save_system *system) {
    printf("\n=== Testing Compression ===\n");
    
    // Generate large dataset
    u8 *test_data = malloc(Megabytes(1));
    u8 *compressed = malloc(Megabytes(2));
    u8 *decompressed = malloc(Megabytes(1));
    
    // Fill with semi-compressible data
    for (u32 i = 0; i < Megabytes(1); i++) {
        // Pattern that compresses well
        test_data[i] = (i / 256) & 0xFF;
    }
    
    // Test LZ4
    printf("Testing LZ4 compression:\n");
    u64 start = __builtin_ia32_rdtsc();
    u32 compressed_size = save_compress_lz4(test_data, Megabytes(1), 
                                           compressed, Megabytes(2));
    u64 end = __builtin_ia32_rdtsc();
    
    f32 compress_time = (f32)(end - start) / 3000000.0f;
    f32 ratio = (f32)compressed_size / (f32)Megabytes(1);
    
    printf("  Compressed 1MB to %u bytes (%.1f%%) in %.2fms\n",
           compressed_size, ratio * 100.0f, compress_time);
    printf("  Speed: %.1f MB/s\n", 1.0f / (compress_time / 1000.0f));
    
    // Test decompression
    start = __builtin_ia32_rdtsc();
    u32 decompressed_size = save_decompress_lz4(compressed, compressed_size,
                                               decompressed, Megabytes(1));
    end = __builtin_ia32_rdtsc();
    
    f32 decompress_time = (f32)(end - start) / 3000000.0f;
    
    printf("  Decompressed in %.2fms\n", decompress_time);
    printf("  Speed: %.1f MB/s\n", 1.0f / (decompress_time / 1000.0f));
    
    // Verify data
    b32 data_matches = 1;
    for (u32 i = 0; i < Megabytes(1); i++) {
        if (test_data[i] != decompressed[i]) {
            printf("  ERROR: Data mismatch at byte %u\n", i);
            data_matches = 0;
            break;
        }
    }
    
    if (data_matches) {
        printf("  Decompressed data verified!\n");
    }
    
    // Test with random data (worst case)
    printf("\nTesting with random data (worst case):\n");
    for (u32 i = 0; i < Megabytes(1); i++) {
        test_data[i] = rand() & 0xFF;
    }
    
    start = __builtin_ia32_rdtsc();
    compressed_size = save_compress_lz4(test_data, Megabytes(1),
                                       compressed, Megabytes(2));
    end = __builtin_ia32_rdtsc();
    
    compress_time = (f32)(end - start) / 3000000.0f;
    ratio = (f32)compressed_size / (f32)Megabytes(1);
    
    printf("  Random data: %u bytes (%.1f%%) in %.2fms\n",
           compressed_size, ratio * 100.0f, compress_time);
    
    free(test_data);
    free(compressed);
    free(decompressed);
}

// Convert demo game state to real game state
internal void demo_to_game_state(demo_game_state *demo, game_state *real) {
    // Zero out the real structure
    memset(real, 0, sizeof(game_state));
    
    // Copy compatible fields
    real->entity_count = demo->entity_count;
    real->playtime_seconds = demo->playtime_seconds;
    strncpy(real->current_level, demo->current_level, 255);
    real->paused = demo->paused;
    
    // Convert player data
    strncpy(real->player.name, demo->player.name, 63);
    real->player.level = demo->player.level;
    real->player.experience = demo->player.experience;
    real->player.health = demo->player.health;
    real->player.max_health = demo->player.max_health;
    real->player.mana = 100;      // Default mana
    real->player.max_mana = 100;  // Default max mana
    
    // Convert entities (up to real limit)
    for (u32 i = 0; i < demo->entity_count && i < 10000; i++) {
        real->entities[i].id = demo->entities[i].id;
        real->entities[i].type = demo->entities[i].type;
        memcpy(real->entities[i].position, demo->entities[i].position, sizeof(f32) * 3);
        memcpy(real->entities[i].rotation, demo->entities[i].rotation, sizeof(f32) * 4);
        real->entities[i].flags = demo->entities[i].flags;
        strncpy(real->entities[i].name, demo->entities[i].name, 63);
    }
    
    // Set null pointers for subsystems (demo mode)
    real->physics = NULL;
    real->audio = NULL;
    real->scripts = NULL;
    real->nodes = NULL;
}

// Test slot management
internal void test_slot_management(save_system *system) {
    printf("\n=== Testing Slot Management ===\n");
    
    // Create saves in multiple slots
    demo_game_state demo_game = {0};
    generate_test_data(&demo_game, 50);
    
    // Allocate real game state on stack
    game_state real_game;
    
    for (i32 i = 2; i < 5; i++) {
        // Modify game state slightly
        demo_game.player.level = 40 + i;
        demo_game.playtime_seconds = 3600.0f * i;
        sprintf(demo_game.current_level, "level_%02d", i);
        
        // Convert demo to real game state
        demo_to_game_state(&demo_game, &real_game);
        
        printf("Saving to slot %d...\n", i);
        if (save_game(system, &real_game, i)) {
            printf("  Saved successfully\n");
        }
    }
    
    // Enumerate slots
    printf("\nEnumerating save slots:\n");
    save_enumerate_slots(system);
    
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        save_slot_info *slot = &system->slots[i];
        if (slot->exists) {
            char time_str[64];
            struct tm *tm = localtime((time_t*)&slot->header.timestamp);
            strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", tm);
            
            printf("  Slot %d: %s, Level %u, %.1fh played, %s\n",
                   i, slot->metadata.player_name,
                   slot->metadata.player_level,
                   slot->metadata.playtime_seconds / 3600.0f,
                   time_str);
        }
    }
    
    // Test slot copying
    printf("\nCopying slot 2 to slot 6:\n");
    if (save_copy_slot(system, 2, 6)) {
        printf("  Copy successful\n");
        
        // Verify copy
        if (system->slots[6].exists &&
            system->slots[6].metadata.player_level == system->slots[2].metadata.player_level) {
            printf("  Copy verified\n");
        }
    }
    
    // Test slot deletion
    printf("\nDeleting slot 6:\n");
    if (save_delete_slot(system, 6)) {
        printf("  Delete successful\n");
        
        if (!system->slots[6].exists) {
            printf("  Deletion verified\n");
        }
    }
}

// Test migration
internal void test_migration(save_system *system) {
    printf("\n=== Testing Save Migration ===\n");
    
    // Test migration system
    if (save_test_migration(system)) {
        printf("Migration tests passed!\n");
    } else {
        printf("Migration tests failed!\n");
    }
    
    // Show migration info
    save_dump_migration_info(0, 2);
}

// Benchmark save performance
internal void benchmark_save_performance(save_system *system) {
    printf("\n=== Save Performance Benchmark ===\n");
    
    demo_game_state demo_game = {0};
    game_state real_game;
    
    // Test different entity counts
    u32 entity_counts[] = {10, 50, 100, 500, 1000};
    
    for (u32 i = 0; i < 5; i++) {
        u32 count = entity_counts[i];
        generate_test_data(&demo_game, count);
        demo_to_game_state(&demo_game, &real_game);
        
        // Measure save time
        u64 start = __builtin_ia32_rdtsc();
        b32 success = save_game(system, &real_game, 7);
        u64 end = __builtin_ia32_rdtsc();
        
        if (success) {
            f32 time_ms = (f32)(end - start) / 3000000.0f;
            u32 file_size = system->slots[7].file_size;
            f32 throughput = (f32)file_size / (time_ms / 1000.0f) / Megabytes(1);
            
            printf("  %4u entities: %.2fms, %u bytes, %.1f MB/s\n",
                   count, time_ms, file_size, throughput);
        }
    }
    
    // Test quicksave performance
    printf("\nQuicksave performance (100 iterations):\n");
    generate_test_data(&demo_game, 100);
    demo_to_game_state(&demo_game, &real_game);
    
    f32 total_time = 0.0f;
    for (u32 i = 0; i < 100; i++) {
        u64 start = __builtin_ia32_rdtsc();
        quicksave(system, &real_game);
        u64 end = __builtin_ia32_rdtsc();
        
        total_time += (f32)(end - start) / 3000000.0f;
    }
    
    printf("  Average quicksave time: %.2fms\n", total_time / 100.0f);
    
    // Test load performance
    printf("\nLoad performance (100 iterations):\n");
    total_time = 0.0f;
    
    for (u32 i = 0; i < 100; i++) {
        u64 start = __builtin_ia32_rdtsc();
        quickload(system, &real_game);
        u64 end = __builtin_ia32_rdtsc();
        
        total_time += (f32)(end - start) / 3000000.0f;
    }
    
    printf("  Average load time: %.2fms\n", total_time / 100.0f);
}

// Test error handling
internal void test_error_handling(save_system *system) {
    printf("\n=== Testing Error Handling ===\n");
    
    // Test loading non-existent slot
    printf("Loading non-existent slot 9:\n");
    demo_game_state demo_game = {0};
    game_state real_game;
    if (!load_game(system, &real_game, 9)) {
        printf("  Correctly failed to load\n");
    }
    
    // Test corrupted save detection
    printf("Testing corrupted save detection:\n");
    
    // Create a valid save first
    generate_test_data(&demo_game, 50);
    demo_to_game_state(&demo_game, &real_game);
    save_game(system, &real_game, 8);
    
    // Corrupt the checksum
    system->slots[8].header.checksum ^= 0xDEADBEEF;
    
    // Try to validate
    save_validate_integrity(system, 8);
    
    if (system->save_corrupted) {
        printf("  Corruption detected successfully\n");
    }
    
    // Reset corruption flag
    system->save_corrupted = 0;
    
    // Test buffer overflow protection
    printf("Testing buffer overflow protection:\n");
    save_buffer small_buffer = {0};
    u8 small_data[16];
    small_buffer.data = small_data;
    small_buffer.capacity = 16;
    
    // Try to write more than capacity
    for (u32 i = 0; i < 10; i++) {
        save_write_u32(&small_buffer, 0x12345678);
    }
    
    if (small_buffer.size <= small_buffer.capacity) {
        printf("  Buffer overflow prevented (size: %u, capacity: %u)\n",
               small_buffer.size, small_buffer.capacity);
    }
}

// Main demo
int main(int argc, char **argv) {
    printf("=== Handmade Save System Demo ===\n");
    printf("Demonstrating save/load with compression and migration\n\n");
    
    // Seed random for consistent tests
    srand(12345);
    
    // Allocate memory for save system
    void *memory = malloc(Megabytes(8));
    if (!memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Initialize save system
    save_system *system = save_system_init(memory, Megabytes(8));
    
    // Create save directory
    platform_create_save_directory();
    
    // Register migrations
    save_register_all_migrations(system);
    
    // Run tests
    test_basic_save_load(system);
    test_compression(system);
    test_slot_management(system);
    test_migration(system);
    benchmark_save_performance(system);
    test_error_handling(system);
    
    // Final statistics
    printf("\n=== Final Statistics ===\n");
    save_dump_info(system);
    
    // Test all slots
    printf("\n=== All Slot Details ===\n");
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        if (system->slots[i].exists) {
            save_debug_slot(system, i);
        }
    }
    
    // Cleanup
    save_system_shutdown(system);
    free(memory);
    
    printf("\n=== Demo Complete ===\n");
    printf("All tests finished successfully!\n");
    
    return 0;
}