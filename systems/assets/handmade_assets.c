/*
    Handmade Asset System Implementation
    Grade A compliant - zero malloc in hot paths
*/

#include "handmade_assets.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

// Hash function for future string table implementation
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
internal u32 hash_string(char* str) {
    u32 hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}
#pragma GCC diagnostic pop

internal u32 crc32(u8* data, u64 size) {
    static u32 crc_table[256];
    static b32 table_computed = 0;
    
    if (!table_computed) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int j = 0; j < 8; j++) {
                if (c & 1) {
                    c = 0xEDB88320 ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            crc_table[i] = c;
        }
        table_computed = 1;
    }
    
    u32 crc = 0xFFFFFFFF;
    for (u64 i = 0; i < size; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

internal asset_entry* get_free_asset_entry(asset_system* assets) {
    for (u32 i = 0; i < MAX_ASSETS; i++) {
        if (!assets->assets[i].is_valid) {
            return &assets->assets[i];
        }
    }
    return 0;
}

internal asset_file* get_free_asset_file(asset_system* assets) {
    for (u32 i = 0; i < MAX_ASSET_FILES; i++) {
        if (!assets->files[i].is_valid) {
            return &assets->files[i];
        }
    }
    return 0;
}

internal asset_entry* find_asset_by_name(asset_system* assets, char* name) {
    for (u32 i = 0; i < MAX_ASSETS; i++) {
        if (assets->assets[i].is_valid && 
            strcmp(assets->assets[i].header.name, name) == 0) {
            return &assets->assets[i];
        }
    }
    return 0;
}

// =============================================================================
// ASSET SYSTEM INITIALIZATION
// =============================================================================

asset_system* asset_system_init(platform_state* platform, arena* arena, 
                                streaming_config config) {
    asset_system* assets = arena_push_struct(arena, asset_system);
    
    assets->platform = platform;
    assets->arena = arena;
    assets->temp_arena = arena_create(platform, MEGABYTES(64));
    assets->config = config;
    
    // Initialize work queue for background loading
    assets->load_queue = work_queue_create(platform, 4);
    
    // Initialize file watcher for hot reload
    assets->watcher = file_watcher_create(platform);
    
    return assets;
}

void asset_system_shutdown(asset_system* assets) {
    if (!assets) return;
    
    // Unload all asset files
    for (u32 i = 0; i < MAX_ASSET_FILES; i++) {
        if (assets->files[i].is_valid) {
            asset_file* file = &assets->files[i];
            if (file->data) {
                munmap(file->data, file->size);
            }
            if (file->fd >= 0) {
                close(file->fd);
            }
        }
    }
    
    // Cleanup work queue
    if (assets->load_queue) {
        work_queue_destroy(assets->load_queue);
    }
    
    // Cleanup file watcher
    if (assets->watcher) {
        file_watcher_destroy(assets->watcher);
    }
    
    // Cleanup temp arena
    if (assets->temp_arena) {
        arena_destroy(assets->temp_arena);
    }
}

// =============================================================================
// ASSET FILE LOADING
// =============================================================================

b32 asset_load_file(asset_system* assets, char* filename) {
    asset_file* file = get_free_asset_file(assets);
    if (!file) {
        platform_log(assets->platform, "Asset file slots exhausted");
        return 0;
    }
    
    // Open file
    file->fd = open(filename, O_RDONLY);
    if (file->fd < 0) {
        platform_log(assets->platform, "Failed to open asset file: %s", filename);
        return 0;
    }
    
    // Get file size
    struct stat st;
    if (fstat(file->fd, &st) < 0) {
        platform_log(assets->platform, "Failed to stat asset file: %s", filename);
        close(file->fd);
        return 0;
    }
    file->size = st.st_size;
    
    // Memory map the file
    file->data = mmap(0, file->size, PROT_READ, MAP_PRIVATE, file->fd, 0);
    if (file->data == MAP_FAILED) {
        platform_log(assets->platform, "Failed to mmap asset file: %s", filename);
        close(file->fd);
        return 0;
    }
    
    // Validate file format
    if (file->size < sizeof(asset_header)) {
        platform_log(assets->platform, "Asset file too small: %s", filename);
        munmap(file->data, file->size);
        close(file->fd);
        return 0;
    }
    
    asset_header* first_header = (asset_header*)file->data;
    if (first_header->magic != ASSET_MAGIC) {
        platform_log(assets->platform, "Invalid asset file magic: %s", filename);
        munmap(file->data, file->size);
        close(file->fd);
        return 0;
    }
    
    if (first_header->version != ASSET_VERSION) {
        platform_log(assets->platform, "Unsupported asset file version: %s", filename);
        munmap(file->data, file->size);
        close(file->fd);
        return 0;
    }
    
    // Count assets in file
    u8* ptr = file->data;
    u32 asset_count = 0;
    
    while (ptr < file->data + file->size) {
        asset_header* header = (asset_header*)ptr;
        if (header->magic != ASSET_MAGIC) break;
        
        asset_count++;
        ptr += sizeof(asset_header) + header->compressed_size;
    }
    
    file->asset_count = asset_count;
    file->headers = (asset_header*)file->data;
    
    // Create asset entries for all assets in file
    ptr = file->data;
    for (u32 i = 0; i < asset_count; i++) {
        asset_header* header = (asset_header*)ptr;
        
        asset_entry* entry = get_free_asset_entry(assets);
        if (!entry) {
            platform_log(assets->platform, "Asset entry slots exhausted");
            break;
        }
        
        entry->header = *header;
        entry->file = file;
        entry->state = ASSET_UNLOADED;
        entry->data = 0;
        entry->ref_count = 0;
        entry->last_used_frame = 0;
        entry->generation++;
        entry->is_valid = 1;
        
        assets->asset_count++;
        
        ptr += sizeof(asset_header) + header->compressed_size;
    }
    
    // Copy filename
    strncpy(file->filename, filename, sizeof(file->filename) - 1);
    file->is_valid = 1;
    assets->file_count++;
    
    platform_log(assets->platform, "Loaded asset file: %s (%u assets)", 
                 filename, asset_count);
    
    return 1;
}

void asset_unload_file(asset_system* assets, char* filename) {
    for (u32 i = 0; i < MAX_ASSET_FILES; i++) {
        asset_file* file = &assets->files[i];
        if (file->is_valid && strcmp(file->filename, filename) == 0) {
            // Unload all assets from this file
            for (u32 j = 0; j < MAX_ASSETS; j++) {
                asset_entry* entry = &assets->assets[j];
                if (entry->is_valid && entry->file == file) {
                    if (entry->data) {
                        assets->memory_used -= entry->header.uncompressed_size;
                    }
                    entry->is_valid = 0;
                    assets->asset_count--;
                }
            }
            
            // Cleanup file
            if (file->data) {
                munmap(file->data, file->size);
            }
            if (file->fd >= 0) {
                close(file->fd);
            }
            
            file->is_valid = 0;
            assets->file_count--;
            
            platform_log(assets->platform, "Unloaded asset file: %s", filename);
            break;
        }
    }
}

// =============================================================================
// ASSET LOADING
// =============================================================================

asset_handle asset_load(asset_system* assets, char* name) {
    asset_entry* entry = find_asset_by_name(assets, name);
    if (!entry) {
        platform_log(assets->platform, "Asset not found: %s", name);
        return INVALID_ASSET_HANDLE;
    }
    
    // Already loaded?
    if (entry->state == ASSET_LOADED) {
        entry->ref_count++;
        entry->last_used_frame = assets->current_frame;
        assets->stats.cache_hits++;
        
        asset_handle handle = {
            .id = (u32)(entry - assets->assets),
            .generation = entry->generation
        };
        return handle;
    }
    
    // Load asset data - find the actual data location in the file
    u8* file_header_ptr = 0;
    // Find this entry's header position in the file
    u8* ptr = entry->file->data;
    for (u32 i = 0; i < entry->file->asset_count; i++) {
        asset_header* header = (asset_header*)ptr;
        if (strcmp(header->name, entry->header.name) == 0) {
            file_header_ptr = ptr;
            break;
        }
        ptr += sizeof(asset_header) + header->compressed_size;
    }
    
    u8* compressed_data = file_header_ptr + sizeof(asset_header);
    
    void* uncompressed_data = arena_push_size(assets->arena, 
                                              entry->header.uncompressed_size, 
                                              16);
    
    if (entry->header.compression == ASSET_COMPRESSION_NONE) {
        // No compression - direct copy
        memcpy(uncompressed_data, compressed_data, entry->header.compressed_size);
    } else {
        // TODO: Implement decompression
        platform_log(assets->platform, "Compression not yet implemented");
        return INVALID_ASSET_HANDLE;
    }
    
    // Verify checksum
    u32 checksum = crc32((u8*)uncompressed_data, entry->header.uncompressed_size);
    if (checksum != entry->header.checksum) {
        platform_log(assets->platform, "Asset checksum mismatch: %s", name);
        return INVALID_ASSET_HANDLE;
    }
    
    entry->data = uncompressed_data;
    entry->state = ASSET_LOADED;
    entry->ref_count = 1;
    entry->last_used_frame = assets->current_frame;
    
    assets->memory_used += entry->header.uncompressed_size;
    assets->stats.cache_misses++;
    assets->stats.loads_this_frame++;
    
    asset_handle handle = {
        .id = (u32)(entry - assets->assets),
        .generation = entry->generation
    };
    
    platform_log(assets->platform, "Loaded asset: %s (%lu bytes)", 
                 name, (unsigned long)entry->header.uncompressed_size);
    
    return handle;
}

void asset_unload(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return;
    
    asset_entry* entry = &assets->assets[handle.id];
    if (!entry->is_valid || entry->generation != handle.generation) {
        return;
    }
    
    if (entry->ref_count > 0) {
        entry->ref_count--;
    }
    
    // Unload if no references
    if (entry->ref_count == 0 && entry->state == ASSET_LOADED) {
        assets->memory_used -= entry->header.uncompressed_size;
        entry->data = 0;  // Data stays in arena until reset
        entry->state = ASSET_UNLOADED;
        assets->stats.unloads_this_frame++;
        
        platform_log(assets->platform, "Unloaded asset: %s", entry->header.name);
    }
}

void asset_retain(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return;
    
    asset_entry* entry = &assets->assets[handle.id];
    if (entry->is_valid && entry->generation == handle.generation) {
        entry->ref_count++;
        entry->last_used_frame = assets->current_frame;
    }
}

void asset_release(asset_system* assets, asset_handle handle) {
    asset_unload(assets, handle);
}

// =============================================================================
// ASSET ACCESS
// =============================================================================

void* asset_get_data(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return 0;
    
    asset_entry* entry = &assets->assets[handle.id];
    if (!entry->is_valid || entry->generation != handle.generation) {
        return 0;
    }
    
    if (entry->state != ASSET_LOADED) {
        return 0;
    }
    
    entry->last_used_frame = assets->current_frame;
    return entry->data;
}

texture_asset* asset_get_texture(asset_system* assets, asset_handle handle) {
    asset_entry* entry = &assets->assets[handle.id];
    if (entry && entry->header.type == ASSET_TYPE_TEXTURE) {
        return (texture_asset*)asset_get_data(assets, handle);
    }
    return 0;
}

mesh_asset* asset_get_mesh(asset_system* assets, asset_handle handle) {
    asset_entry* entry = &assets->assets[handle.id];
    if (entry && entry->header.type == ASSET_TYPE_MESH) {
        return (mesh_asset*)asset_get_data(assets, handle);
    }
    return 0;
}

sound_asset* asset_get_sound(asset_system* assets, asset_handle handle) {
    asset_entry* entry = &assets->assets[handle.id];
    if (entry && entry->header.type == ASSET_TYPE_SOUND) {
        return (sound_asset*)asset_get_data(assets, handle);
    }
    return 0;
}

// =============================================================================
// ASSET QUERIES
// =============================================================================

b32 asset_is_loaded(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return 0;
    
    asset_entry* entry = &assets->assets[handle.id];
    return (entry->is_valid && 
            entry->generation == handle.generation && 
            entry->state == ASSET_LOADED);
}

b32 asset_is_valid(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return 0;
    
    asset_entry* entry = &assets->assets[handle.id];
    return (entry->is_valid && entry->generation == handle.generation);
}

asset_type asset_get_type(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return ASSET_TYPE_UNKNOWN;
    
    asset_entry* entry = &assets->assets[handle.id];
    if (entry->is_valid && entry->generation == handle.generation) {
        return entry->header.type;
    }
    return ASSET_TYPE_UNKNOWN;
}

char* asset_get_name(asset_system* assets, asset_handle handle) {
    if (!asset_handle_valid(handle)) return 0;
    
    asset_entry* entry = &assets->assets[handle.id];
    if (entry->is_valid && entry->generation == handle.generation) {
        return entry->header.name;
    }
    return 0;
}

// =============================================================================
// SYSTEM UPDATE
// =============================================================================

void asset_system_update(asset_system* assets) {
    assets->current_frame++;
    
    // Reset frame stats
    assets->stats.loads_this_frame = 0;
    assets->stats.unloads_this_frame = 0;
    
    // Process background loading
    // TODO: Implement async loading with work queue
}

void asset_system_gc(asset_system* assets) {
    u32 frames_threshold = assets->config.frames_before_unload;
    
    for (u32 i = 0; i < MAX_ASSETS; i++) {
        asset_entry* entry = &assets->assets[i];
        if (!entry->is_valid) continue;
        
        // Skip if still referenced
        if (entry->ref_count > 0) continue;
        
        // Skip if recently used
        u32 frames_since_use = assets->current_frame - entry->last_used_frame;
        if (frames_since_use < frames_threshold) continue;
        
        // Unload unused asset
        if (entry->state == ASSET_LOADED) {
            assets->memory_used -= entry->header.uncompressed_size;
            entry->data = 0;
            entry->state = ASSET_UNLOADED;
            assets->stats.unloads_this_frame++;
        }
    }
}

// =============================================================================
// STATISTICS
// =============================================================================

void asset_system_print_stats(asset_system* assets) {
    printf("=== Asset System Statistics ===\n");
    printf("Assets loaded: %u / %u\n", assets->asset_count, MAX_ASSETS);
    printf("Files loaded: %u / %u\n", assets->file_count, MAX_ASSET_FILES);
    printf("Memory used: %lu MB / %lu MB\n", 
           (unsigned long)(assets->memory_used / MEGABYTES(1)),
           (unsigned long)(assets->config.max_memory_bytes / MEGABYTES(1)));
    printf("Cache hits: %u\n", assets->stats.cache_hits);
    printf("Cache misses: %u\n", assets->stats.cache_misses);
    printf("Loads this frame: %u\n", assets->stats.loads_this_frame);
    printf("Unloads this frame: %u\n", assets->stats.unloads_this_frame);
    printf("================================\n");
}