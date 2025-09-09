#ifndef HANDMADE_SAVE_H
#define HANDMADE_SAVE_H

/*
    Handmade Save/Load System
    Complete game state serialization with compression
    
    Features:
    - Binary format for speed
    - LZ4/zlib compression
    - Version migration
    - Zero allocations
    - Deterministic saves
    
    PERFORMANCE: Targets
    - Save time: <100ms typical
    - Load time: <200ms typical  
    - Compression: 10:1 ratio
    - File size: <1MB typical
*/

#include "../../src/handmade.h"

// Save system constants
#define SAVE_MAGIC_NUMBER 0x53444D48  // "HMDS" in hex
#define SAVE_VERSION 1
#define SAVE_MAX_SLOTS 10
#define SAVE_QUICKSAVE_SLOT 0
#define SAVE_AUTOSAVE_SLOT 1
#define SAVE_THUMBNAIL_WIDTH 128
#define SAVE_THUMBNAIL_HEIGHT 72
#define SAVE_MAX_PATH 256
#define SAVE_CHUNK_SIZE Kilobytes(64)
#define SAVE_BUFFER_SIZE Megabytes(4)

// Forward declarations
typedef struct memory_arena memory_arena;
typedef struct game_state game_state;

// Save compression types
typedef enum save_compression_type {
    SAVE_COMPRESSION_NONE = 0,
    SAVE_COMPRESSION_LZ4 = 1,    // Fast compression
    SAVE_COMPRESSION_ZLIB = 2,   // Better ratio
} save_compression_type;

// Save chunk types for different game systems
typedef enum save_chunk_type {
    SAVE_CHUNK_HEADER = 0,
    SAVE_CHUNK_METADATA = 1,
    SAVE_CHUNK_WORLD = 2,
    SAVE_CHUNK_PLAYER = 3,
    SAVE_CHUNK_NPCS = 4,
    SAVE_CHUNK_PHYSICS = 5,
    SAVE_CHUNK_AUDIO = 6,
    SAVE_CHUNK_SCRIPT = 7,
    SAVE_CHUNK_NODES = 8,
    SAVE_CHUNK_INVENTORY = 9,
    SAVE_CHUNK_QUESTS = 10,
    SAVE_CHUNK_END = 0xFFFFFFFF,
} save_chunk_type;

// Save file header (16 bytes, aligned)
typedef struct save_header {
    u32 magic;           // Magic number for validation
    u32 version;         // Save format version
    u64 timestamp;       // Unix timestamp
    u32 checksum;        // CRC32 of all data
    u8 compressed;       // Compression type used
    u8 reserved[3];      // Padding for alignment
} save_header;

// Save metadata (for UI display)
typedef struct save_metadata {
    f32 playtime_seconds;
    char level_name[64];
    char player_name[32];
    u32 player_level;
    u32 save_count;      // Number of times saved
    
    // Thumbnail data (RGB888)
    u8 thumbnail[SAVE_THUMBNAIL_WIDTH * SAVE_THUMBNAIL_HEIGHT * 3];
} save_metadata;

// Chunk header for streaming saves
typedef struct save_chunk_header {
    u32 type;            // Chunk type ID
    u32 uncompressed_size;
    u32 compressed_size; // Same as uncompressed if not compressed
    u32 checksum;        // CRC32 of chunk data
} save_chunk_header;

// Save slot information
typedef struct save_slot_info {
    b32 exists;
    char filename[SAVE_MAX_PATH];
    save_header header;
    save_metadata metadata;
    u64 file_size;
    u64 last_modified;
} save_slot_info;

// Serialization buffer for zero-copy saves
typedef struct save_buffer {
    u8 *data;
    u32 size;
    u32 capacity;
    u32 read_offset;     // For deserialization
    
    // PERFORMANCE: Track for profiling
    u32 bytes_written;
    u32 bytes_read;
    f32 compression_ratio;
} save_buffer;

// Version migration function pointer
typedef b32 (*save_migration_fn)(save_buffer *old_data, save_buffer *new_data, u32 old_version, u32 new_version);

// Save system state
typedef struct save_system {
    // Fixed memory pools
    u8 *save_memory;     // Main save buffer
    u32 save_memory_size;
    u8 *compress_memory; // Compression workspace
    u32 compress_memory_size;
    
    // Active buffers
    save_buffer write_buffer;
    save_buffer read_buffer;
    save_buffer compress_buffer;
    
    // Slot management
    save_slot_info slots[SAVE_MAX_SLOTS];
    i32 current_slot;
    
    // Autosave state
    f32 autosave_timer;
    f32 autosave_interval;
    b32 autosave_enabled;
    
    // Version migration
    save_migration_fn migration_table[16];  // Support up to 16 versions
    u32 migration_count;
    
    // Performance metrics
    f32 last_save_time;
    f32 last_load_time;
    u64 total_bytes_saved;
    u64 total_bytes_loaded;
    
    // State flags
    b32 is_saving;
    b32 is_loading;
    b32 save_corrupted;
} save_system;

// Core API functions
save_system *save_system_init(void *memory, u32 memory_size);
void save_system_shutdown(save_system *system);

// Save/Load operations
b32 save_game(save_system *system, game_state *game, i32 slot);
b32 load_game(save_system *system, game_state *game, i32 slot);
b32 quicksave(save_system *system, game_state *game);
b32 quickload(save_system *system, game_state *game);

// Autosave management
void save_update_autosave(save_system *system, game_state *game, f32 dt);
void save_enable_autosave(save_system *system, f32 interval_seconds);
void save_disable_autosave(save_system *system);

// Slot management
void save_enumerate_slots(save_system *system);
b32 save_delete_slot(save_system *system, i32 slot);
b32 save_copy_slot(save_system *system, i32 src_slot, i32 dst_slot);
save_slot_info *save_get_slot_info(save_system *system, i32 slot);

// Serialization primitives
void save_write_u8(save_buffer *buffer, u8 value);
void save_write_u16(save_buffer *buffer, u16 value);
void save_write_u32(save_buffer *buffer, u32 value);
void save_write_u64(save_buffer *buffer, u64 value);
void save_write_f32(save_buffer *buffer, f32 value);
void save_write_f64(save_buffer *buffer, f64 value);
void save_write_bytes(save_buffer *buffer, void *data, u32 size);
void save_write_string(save_buffer *buffer, char *str);

// Deserialization primitives
u8 save_read_u8(save_buffer *buffer);
u16 save_read_u16(save_buffer *buffer);
u32 save_read_u32(save_buffer *buffer);
u64 save_read_u64(save_buffer *buffer);
f32 save_read_f32(save_buffer *buffer);
f64 save_read_f64(save_buffer *buffer);
void save_read_bytes(save_buffer *buffer, void *data, u32 size);
void save_read_string(save_buffer *buffer, char *str, u32 max_size);

// Compression functions
u32 save_compress_lz4(u8 *src, u32 src_size, u8 *dst, u32 dst_capacity);
u32 save_decompress_lz4(u8 *src, u32 src_size, u8 *dst, u32 dst_capacity);
u32 save_compress_zlib(u8 *src, u32 src_size, u8 *dst, u32 dst_capacity, i32 level);
u32 save_decompress_zlib(u8 *src, u32 src_size, u8 *dst, u32 dst_capacity);

// CRC32 checksum
u32 save_crc32(u8 *data, u32 size);

// Version migration
void save_register_migration(save_system *system, u32 version, save_migration_fn fn);
b32 save_migrate_data(save_system *system, save_buffer *data, u32 old_version, u32 new_version);

// Debug/Development
void save_dump_info(save_system *system);
void save_validate_integrity(save_system *system, i32 slot);

// Internal helper functions (defined in implementation files)
void save_buffer_reset(save_buffer *buffer);
b32 write_chunk(save_buffer *buffer, save_chunk_type type, u8 *data, u32 size, b32 compress);

// Platform-specific file I/O (implemented per platform)
b32 platform_save_write_file(char *path, void *data, u32 size);
b32 platform_save_read_file(char *path, void *data, u32 max_size, u32 *actual_size);
b32 platform_save_delete_file(char *path);
b32 platform_save_file_exists(char *path);
u64 platform_save_get_file_time(char *path);

#endif // HANDMADE_SAVE_H