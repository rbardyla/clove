/*
    Platform stub functions for save system testing
    Minimal Linux implementation for save demos
*/

#include "handmade_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define SAVE_DIR "saves/"

// Create save directory if it doesn't exist
void platform_create_save_directory(void) {
    struct stat st = {0};
    if (stat(SAVE_DIR, &st) == -1) {
        mkdir(SAVE_DIR, 0700);
    }
}

// Write file to disk
b32 platform_save_write_file(char *path, void *data, u32 size) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    
    return written == size;
}

// Read file from disk  
b32 platform_save_read_file(char *path, void *data, u32 max_size, u32 *actual_size) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size > max_size) {
        fclose(file);
        return 0;
    }
    
    size_t read = fread(data, 1, file_size, file);
    fclose(file);
    
    if (actual_size) *actual_size = (u32)read;
    return read == file_size;
}

// Delete file
b32 platform_save_delete_file(char *path) {
    return remove(path) == 0;
}

// Check if file exists
b32 platform_save_file_exists(char *path) {
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

// Get file modification time
u64 platform_save_get_file_time(char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (u64)st.st_mtime;
    }
    return 0;
}

// Debug function for save slots  
void save_debug_slot(save_system *system, i32 slot) {
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) {
        printf("Invalid slot %d\n", slot);
        return;
    }
    
    save_slot_info *info = &system->slots[slot];
    printf("=== Slot %d Debug Info ===\n", slot);
    
    if (!info->exists) {
        printf("  Slot is empty\n");
        return;
    }
    
    printf("  Filename: %s\n", info->filename);
    printf("  Timestamp: %lu\n", info->header.timestamp);
    printf("  Version: %u\n", info->header.version);
    printf("  Compressed: %s\n", info->header.compressed ? "Yes" : "No");
    printf("  Level: %s\n", info->metadata.level_name);
    printf("  Player: %s (Level %u)\n", info->metadata.player_name, info->metadata.player_level);
    printf("  Playtime: %.1fs\n", info->metadata.playtime_seconds);
    printf("  Size: %lu bytes\n", info->file_size);
    printf("  Save count: %u\n", info->metadata.save_count);
}

// Test migration function
b32 save_test_migration(save_system *system) {
    (void)system;
    printf("Migration test passed (stub)\n");
    return 1;
}

// Dump migration info
void save_dump_migration_info(u32 old_version, u32 new_version) {
    printf("Migration info: v%u -> v%u (stub)\n", old_version, new_version);
}