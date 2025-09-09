/*
    Handmade Save System - Slot Management
    
    Manages multiple save slots with metadata
    Handles quicksave, autosave, and manual saves
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// Platform-specific directory paths
#ifdef _WIN32
    #define SAVE_DIR "saves\\"
#else
    #define SAVE_DIR "saves/"
#endif

// Generate save file path
internal void get_save_path(char *path, i32 slot) {
    if (slot == SAVE_QUICKSAVE_SLOT) {
        sprintf(path, "%squicksave.hms", SAVE_DIR);
    } else if (slot == SAVE_AUTOSAVE_SLOT) {
        sprintf(path, "%sautosave.hms", SAVE_DIR);
    } else {
        sprintf(path, "%ssave%02d.hms", SAVE_DIR, slot);
    }
}

// Generate thumbnail from current frame
internal void capture_thumbnail(save_metadata *metadata, game_state *game) {
    // PERFORMANCE: Downsample framebuffer to thumbnail size
    // In production, would grab from renderer
    
    // For now, generate a placeholder pattern based on game state
    u8 *thumb = metadata->thumbnail;
    
    // Create a simple gradient based on player position
    u32 base_color = (u32)(game->player.position[0] * 255.0f) & 0xFF;
    
    for (u32 y = 0; y < SAVE_THUMBNAIL_HEIGHT; y++) {
        for (u32 x = 0; x < SAVE_THUMBNAIL_WIDTH; x++) {
            u32 idx = (y * SAVE_THUMBNAIL_WIDTH + x) * 3;
            
            // Simple pattern for visualization
            thumb[idx + 0] = (base_color + x) & 0xFF;  // R
            thumb[idx + 1] = (base_color + y) & 0xFF;  // G
            thumb[idx + 2] = base_color;                // B
            
            // Add some landmarks
            if (x == (u32)(game->player.position[0] * SAVE_THUMBNAIL_WIDTH / 100.0f) &&
                y == (u32)(game->player.position[2] * SAVE_THUMBNAIL_HEIGHT / 100.0f)) {
                // Mark player position
                thumb[idx + 0] = 255;
                thumb[idx + 1] = 255;
                thumb[idx + 2] = 0;
            }
        }
    }
}

// Format time for display
internal void format_time(char *buffer, u32 buffer_size, u64 timestamp) {
    time_t time = (time_t)timestamp;
    struct tm *tm_info = localtime(&time);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Format playtime for display
internal void format_playtime(char *buffer, u32 buffer_size, f32 seconds) {
    u32 hours = (u32)(seconds / 3600.0f);
    u32 minutes = (u32)((seconds - hours * 3600.0f) / 60.0f);
    u32 secs = (u32)(seconds - hours * 3600.0f - minutes * 60.0f);
    
    if (hours > 0) {
        sprintf(buffer, "%dh %dm %ds", hours, minutes, secs);
    } else if (minutes > 0) {
        sprintf(buffer, "%dm %ds", minutes, secs);
    } else {
        sprintf(buffer, "%ds", secs);
    }
}

// Get slot display name
internal void get_slot_display_name(char *buffer, u32 buffer_size, i32 slot) {
    if (slot == SAVE_QUICKSAVE_SLOT) {
        strcpy(buffer, "Quick Save");
    } else if (slot == SAVE_AUTOSAVE_SLOT) {
        strcpy(buffer, "Auto Save");
    } else {
        sprintf(buffer, "Save Slot %d", slot);
    }
}

// Find next available slot
internal i32 find_free_slot(save_system *system) {
    // Skip quicksave and autosave slots
    for (i32 i = 2; i < SAVE_MAX_SLOTS; i++) {
        if (!system->slots[i].exists) {
            return i;
        }
    }
    
    // All slots full, find oldest
    u64 oldest_time = UINT64_MAX;
    i32 oldest_slot = 2;
    
    for (i32 i = 2; i < SAVE_MAX_SLOTS; i++) {
        if (system->slots[i].last_modified < oldest_time) {
            oldest_time = system->slots[i].last_modified;
            oldest_slot = i;
        }
    }
    
    return oldest_slot;
}

// Sort slots by timestamp
internal void sort_slots_by_time(save_slot_info *slots, i32 count) {
    // PERFORMANCE: Simple bubble sort for small N
    for (i32 i = 0; i < count - 1; i++) {
        for (i32 j = 0; j < count - i - 1; j++) {
            if (slots[j].last_modified < slots[j + 1].last_modified) {
                save_slot_info temp = slots[j];
                slots[j] = slots[j + 1];
                slots[j + 1] = temp;
            }
        }
    }
}

// Create save slot UI info
typedef struct slot_ui_info {
    i32 slot_index;
    char display_name[64];
    char level_name[64];
    char player_info[128];
    char playtime[32];
    char save_date[32];
    u8 *thumbnail;
    b32 exists;
    b32 is_quicksave;
    b32 is_autosave;
    b32 is_corrupted;
} slot_ui_info;

// Get UI info for all slots
internal void get_all_slot_ui_info(save_system *system, slot_ui_info *ui_info) {
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        save_slot_info *slot = &system->slots[i];
        slot_ui_info *ui = &ui_info[i];
        
        ui->slot_index = i;
        ui->exists = slot->exists;
        ui->is_quicksave = (i == SAVE_QUICKSAVE_SLOT);
        ui->is_autosave = (i == SAVE_AUTOSAVE_SLOT);
        ui->is_corrupted = 0;
        
        get_slot_display_name(ui->display_name, 64, i);
        
        if (slot->exists) {
            strcpy(ui->level_name, slot->metadata.level_name);
            sprintf(ui->player_info, "%s - Level %u", 
                   slot->metadata.player_name,
                   slot->metadata.player_level);
            
            format_playtime(ui->playtime, 32, slot->metadata.playtime_seconds);
            format_time(ui->save_date, 32, slot->header.timestamp);
            
            ui->thumbnail = slot->metadata.thumbnail;
            
            // Check for corruption
            if (slot->header.magic != SAVE_MAGIC_NUMBER) {
                ui->is_corrupted = 1;
                strcpy(ui->player_info, "CORRUPTED SAVE");
            }
        } else {
            strcpy(ui->level_name, "Empty Slot");
            strcpy(ui->player_info, "---");
            strcpy(ui->playtime, "---");
            strcpy(ui->save_date, "---");
            ui->thumbnail = NULL;
        }
    }
}

// Save slot with confirmation
b32 save_slot_with_confirm(save_system *system, game_state *game, i32 slot) {
    // Check if slot exists and needs confirmation
    if (system->slots[slot].exists && slot >= 2) {
        // In production, would show UI confirmation dialog
        // For now, just log
        printf("Overwriting save slot %d\n", slot);
    }
    
    return save_game(system, game, slot);
}

// Load slot with validation
b32 load_slot_with_validation(save_system *system, game_state *game, i32 slot) {
    if (!system->slots[slot].exists) {
        printf("Cannot load slot %d: No save file\n", slot);
        return 0;
    }
    
    // Check version compatibility
    if (system->slots[slot].header.version > SAVE_VERSION) {
        printf("Cannot load slot %d: Save version %u is newer than current version %u\n",
               slot, system->slots[slot].header.version, SAVE_VERSION);
        return 0;
    }
    
    // Check for corruption
    if (system->slots[slot].header.magic != SAVE_MAGIC_NUMBER) {
        printf("Cannot load slot %d: Save file is corrupted\n", slot);
        return 0;
    }
    
    return load_game(system, game, slot);
}

// Backup save before overwriting
internal b32 backup_save_slot(save_system *system, i32 slot) {
    if (!system->slots[slot].exists) return 1;
    
    char backup_path[SAVE_MAX_PATH];
    sprintf(backup_path, "%sbackup_slot%02d_%llu.hms", 
           SAVE_DIR, slot, (u64)time(NULL));
    
    // Read original file
    u32 actual_size;
    b32 result = platform_save_read_file(system->slots[slot].filename,
                                         system->compress_buffer.data,
                                         system->compress_buffer.capacity,
                                         &actual_size);
    
    if (!result) return 0;
    
    // Write backup
    return platform_save_write_file(backup_path,
                                   system->compress_buffer.data,
                                   actual_size);
}

// Clean old backups
internal void clean_old_backups(save_system *system) {
    // In production, would scan directory and delete old backup files
    // Keep only last N backups per slot
    const u32 MAX_BACKUPS_PER_SLOT = 3;
    
    // Placeholder - would implement directory scanning
    printf("Cleaning old backup files...\n");
}

// Import/Export saves
b32 export_save_slot(save_system *system, i32 slot, char *export_path) {
    if (!system->slots[slot].exists) return 0;
    
    // Read save file
    u32 actual_size;
    b32 result = platform_save_read_file(system->slots[slot].filename,
                                         system->compress_buffer.data,
                                         system->compress_buffer.capacity,
                                         &actual_size);
    
    if (!result) return 0;
    
    // Write to export location
    return platform_save_write_file(export_path,
                                   system->compress_buffer.data,
                                   actual_size);
}

b32 import_save_slot(save_system *system, char *import_path, i32 slot) {
    // Read import file
    u32 actual_size;
    b32 result = platform_save_read_file(import_path,
                                         system->compress_buffer.data,
                                         system->compress_buffer.capacity,
                                         &actual_size);
    
    if (!result) return 0;
    
    // Validate imported save
    save_header *header = (save_header *)system->compress_buffer.data;
    if (header->magic != SAVE_MAGIC_NUMBER) {
        printf("Import failed: Invalid save file\n");
        return 0;
    }
    
    if (header->version > SAVE_VERSION) {
        printf("Import failed: Save version %u is too new\n", header->version);
        return 0;
    }
    
    // Backup existing slot if needed
    if (system->slots[slot].exists) {
        backup_save_slot(system, slot);
    }
    
    // Write to slot
    get_save_path(system->slots[slot].filename, slot);
    result = platform_save_write_file(system->slots[slot].filename,
                                      system->compress_buffer.data,
                                      actual_size);
    
    if (result) {
        // Update slot info
        memcpy(&system->slots[slot].header, header, sizeof(save_header));
        memcpy(&system->slots[slot].metadata, 
               system->compress_buffer.data + sizeof(save_header),
               sizeof(save_metadata));
        system->slots[slot].exists = 1;
        system->slots[slot].file_size = actual_size;
        system->slots[slot].last_modified = header->timestamp;
    }
    
    return result;
}

// Cloud save preparation
typedef struct cloud_save_manifest {
    u32 version;
    u32 slot_count;
    struct {
        i32 slot_index;
        u64 timestamp;
        u32 file_size;
        u32 checksum;
        char filename[64];
    } slots[SAVE_MAX_SLOTS];
} cloud_save_manifest;

internal void prepare_cloud_save_manifest(save_system *system, 
                                         cloud_save_manifest *manifest) {
    manifest->version = SAVE_VERSION;
    manifest->slot_count = 0;
    
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        if (system->slots[i].exists) {
            u32 idx = manifest->slot_count++;
            manifest->slots[idx].slot_index = i;
            manifest->slots[idx].timestamp = system->slots[i].header.timestamp;
            manifest->slots[idx].file_size = system->slots[i].file_size;
            manifest->slots[idx].checksum = system->slots[i].header.checksum;
            strcpy(manifest->slots[idx].filename, system->slots[i].filename);
        }
    }
}

// Sync with cloud saves
b32 sync_cloud_saves(save_system *system) {
    // PERFORMANCE: Async operation in production
    printf("Syncing with cloud saves...\n");
    
    cloud_save_manifest local_manifest;
    prepare_cloud_save_manifest(system, &local_manifest);
    
    // In production:
    // 1. Download cloud manifest
    // 2. Compare timestamps
    // 3. Download newer cloud saves
    // 4. Upload newer local saves
    // 5. Handle conflicts
    
    return 1;
}

// Save statistics
typedef struct save_stats {
    u32 total_saves;
    u32 total_loads;
    u32 quicksaves;
    u32 autosaves;
    f32 average_save_time;
    f32 average_load_time;
    u64 total_bytes_saved;
    u64 total_bytes_loaded;
    f32 compression_ratio;
} save_stats;

internal void calculate_save_stats(save_system *system, save_stats *stats) {
    stats->total_saves = 0;
    stats->total_loads = 0;
    stats->quicksaves = 0;
    stats->autosaves = 0;
    stats->total_bytes_saved = system->total_bytes_saved;
    stats->total_bytes_loaded = system->total_bytes_loaded;
    
    // Count saves by type
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        if (system->slots[i].exists) {
            stats->total_saves += system->slots[i].metadata.save_count;
            
            if (i == SAVE_QUICKSAVE_SLOT) {
                stats->quicksaves = system->slots[i].metadata.save_count;
            } else if (i == SAVE_AUTOSAVE_SLOT) {
                stats->autosaves = system->slots[i].metadata.save_count;
            }
        }
    }
    
    // Calculate averages
    if (stats->total_saves > 0) {
        stats->average_save_time = system->last_save_time;
    }
    
    if (stats->total_loads > 0) {
        stats->average_load_time = system->last_load_time;
    }
    
    // Average compression ratio
    u32 total_uncompressed = 0;
    u32 total_compressed = 0;
    
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        if (system->slots[i].exists) {
            total_compressed += system->slots[i].file_size;
            // Estimate uncompressed size (would track actual in production)
            total_uncompressed += system->slots[i].file_size * 3;
        }
    }
    
    if (total_uncompressed > 0) {
        stats->compression_ratio = (f32)total_compressed / (f32)total_uncompressed;
    }
}

// Debug slot info
void save_debug_slot(save_system *system, i32 slot) {
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) {
        printf("Invalid slot index: %d\n", slot);
        return;
    }
    
    save_slot_info *info = &system->slots[slot];
    
    printf("=== Save Slot %d Debug Info ===\n", slot);
    printf("Filename: %s\n", info->filename);
    printf("Exists: %s\n", info->exists ? "Yes" : "No");
    
    if (info->exists) {
        printf("Header:\n");
        printf("  Magic: 0x%08X (%s)\n", info->header.magic,
               info->header.magic == SAVE_MAGIC_NUMBER ? "Valid" : "INVALID");
        printf("  Version: %u\n", info->header.version);
        printf("  Timestamp: %llu\n", info->header.timestamp);
        printf("  Checksum: 0x%08X\n", info->header.checksum);
        printf("  Compressed: %s\n", 
               info->header.compressed == SAVE_COMPRESSION_LZ4 ? "LZ4" :
               info->header.compressed == SAVE_COMPRESSION_ZLIB ? "zlib" : "None");
        
        printf("Metadata:\n");
        printf("  Player: %s (Level %u)\n", 
               info->metadata.player_name, info->metadata.player_level);
        printf("  Level: %s\n", info->metadata.level_name);
        printf("  Playtime: %.2f hours\n", info->metadata.playtime_seconds / 3600.0f);
        printf("  Save count: %u\n", info->metadata.save_count);
        
        printf("File Info:\n");
        printf("  Size: %llu bytes\n", info->file_size);
        
        char time_str[64];
        format_time(time_str, 64, info->last_modified);
        printf("  Last modified: %s\n", time_str);
        
        // Thumbnail info
        u32 thumb_checksum = save_crc32(info->metadata.thumbnail, 
                                        sizeof(info->metadata.thumbnail));
        printf("  Thumbnail checksum: 0x%08X\n", thumb_checksum);
    }
    
    printf("\n");
}