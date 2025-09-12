#ifndef HANDMADE_ASSETS_H
#define HANDMADE_ASSETS_H

/*
    Handmade Asset System
    Zero-dependency, high-performance asset loading and streaming
    
    Features:
    - Custom binary format (.hma)
    - Memory-mapped files for instant loading
    - Arena-based memory management
    - SIMD-optimized decompression
    - Background streaming
    - Hot reload support
    - LRU cache with automatic unloading
*/

#include "../../src/handmade.h"
// Math types for assets (minimal definitions)
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;

// Asset system limits
#define MAX_ASSETS 65536
#define MAX_ASSET_FILES 256
#define ASSET_MAGIC 0x53414D48  // "HMAS"
#define ASSET_VERSION 1

// Asset types
typedef enum asset_type {
    ASSET_TYPE_UNKNOWN = 0,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MESH,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_SHADER,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_ANIMATION,
    ASSET_TYPE_FONT,
    ASSET_TYPE_COUNT
} asset_type;

// Asset compression
typedef enum asset_compression {
    ASSET_COMPRESSION_NONE = 0,
    ASSET_COMPRESSION_LZ4,
    ASSET_COMPRESSION_ZSTD,
    ASSET_COMPRESSION_COUNT
} asset_compression;

// Asset load state
typedef enum asset_load_state {
    ASSET_UNLOADED = 0,
    ASSET_LOADING,
    ASSET_LOADED,
    ASSET_ERROR
} asset_load_state;

// Asset header (stored at start of .hma files)
typedef struct asset_header {
    u32 magic;                  // ASSET_MAGIC
    u32 version;                // ASSET_VERSION
    asset_type type;            // Asset type
    asset_compression compression; // Compression method
    u64 uncompressed_size;      // Size after decompression
    u64 compressed_size;        // Size in file
    u64 data_offset;            // Offset to data in file
    u32 checksum;               // CRC32 of uncompressed data
    char name[256];             // Asset name/path
    u8 reserved[256];           // Future expansion
} asset_header;

// Asset handle (opaque to users)
typedef struct asset_handle {
    u32 id;                     // Asset ID
    u32 generation;             // For handle validation
} asset_handle;

// Texture asset data
typedef struct texture_asset {
    u32 width;
    u32 height;
    u32 channels;               // 1=R, 2=RG, 3=RGB, 4=RGBA
    u32 format;                 // GL format
    u8* pixels;                 // Pixel data
} texture_asset;

// Mesh asset data
typedef struct mesh_asset {
    u32 vertex_count;
    u32 index_count;
    u32 vertex_size;            // Bytes per vertex
    u8* vertices;               // Vertex data
    u32* indices;               // Index data
    
    // Bounding box
    v3 min_bounds;
    v3 max_bounds;
} mesh_asset;

// Sound asset data
typedef struct sound_asset {
    u32 sample_rate;
    u32 channels;
    u32 bits_per_sample;
    u32 sample_count;
    u8* samples;                // Audio samples
} sound_asset;

// Asset file (memory-mapped .hma file)
typedef struct asset_file {
    char filename[256];
    int fd;                     // File descriptor
    u8* data;                   // Memory-mapped data
    u64 size;                   // File size
    
    asset_header* headers;      // Array of asset headers
    u32 asset_count;            // Number of assets in file
    
    b32 is_valid;
} asset_file;

// Asset entry (runtime state)
typedef struct asset_entry {
    asset_header header;        // Asset metadata
    asset_file* file;           // Source file
    
    asset_load_state state;     // Current load state
    void* data;                 // Loaded asset data
    u32 ref_count;              // Reference count
    u32 last_used_frame;        // For LRU eviction
    
    u32 generation;             // Handle validation
    b32 is_valid;
} asset_entry;

// Streaming control
typedef struct streaming_config {
    u64 max_memory_bytes;       // Total memory budget
    u32 max_concurrent_loads;   // Max simultaneous loads
    u32 frames_before_unload;   // LRU timeout
    f32 load_distance;          // Distance-based loading
    b32 enable_compression;     // Decompress on load
} streaming_config;

// Asset system state
typedef struct asset_system {
    // Platform
    platform_state* platform;
    
    // Memory
    arena* arena;
    arena* temp_arena;
    
    // Asset storage
    asset_entry assets[MAX_ASSETS];
    asset_file files[MAX_ASSET_FILES];
    u32 asset_count;
    u32 file_count;
    
    // Streaming
    streaming_config config;
    work_queue* load_queue;     // Background loading
    u64 memory_used;            // Current memory usage
    u32 current_frame;          // Frame counter
    
    // Hot reload
    file_watcher* watcher;
    u32 dirty_assets[1024];
    u32 dirty_count;
    
    // Statistics
    struct {
        u32 loads_this_frame;
        u32 unloads_this_frame;
        u32 cache_hits;
        u32 cache_misses;
        f64 total_load_time;
        f64 total_decompress_time;
    } stats;
} asset_system;

// =============================================================================
// ASSET SYSTEM API
// =============================================================================

// Initialization
asset_system* asset_system_init(platform_state* platform, arena* arena, 
                                streaming_config config);
void asset_system_shutdown(asset_system* assets);

// Asset file management
b32 asset_load_file(asset_system* assets, char* filename);
void asset_unload_file(asset_system* assets, char* filename);
void asset_reload_file(asset_system* assets, char* filename);

// Asset loading
asset_handle asset_load(asset_system* assets, char* name);
asset_handle asset_load_async(asset_system* assets, char* name);
void asset_unload(asset_system* assets, asset_handle handle);
void asset_retain(asset_system* assets, asset_handle handle);
void asset_release(asset_system* assets, asset_handle handle);

// Asset access
void* asset_get_data(asset_system* assets, asset_handle handle);
texture_asset* asset_get_texture(asset_system* assets, asset_handle handle);
mesh_asset* asset_get_mesh(asset_system* assets, asset_handle handle);
sound_asset* asset_get_sound(asset_system* assets, asset_handle handle);

// Asset queries
b32 asset_is_loaded(asset_system* assets, asset_handle handle);
b32 asset_is_valid(asset_system* assets, asset_handle handle);
asset_type asset_get_type(asset_system* assets, asset_handle handle);
char* asset_get_name(asset_system* assets, asset_handle handle);

// System update
void asset_system_update(asset_system* assets);
void asset_system_gc(asset_system* assets);  // Garbage collect unused assets

// Hot reload
void asset_system_enable_hot_reload(asset_system* assets, b32 enable);
void asset_system_check_hot_reload(asset_system* assets);

// Statistics and debug
void asset_system_print_stats(asset_system* assets);
// Forward declaration to avoid struct dependency
typedef struct gui_context gui_context;
void asset_system_debug_gui(asset_system* assets, gui_context* gui);

// =============================================================================
// ASSET COMPILER API (for tools)
// =============================================================================

typedef struct asset_compiler {
    arena* arena;
    char output_path[256];
    asset_compression compression;
    u32 compression_level;
} asset_compiler;

// Compiler functions
asset_compiler* asset_compiler_create(arena* arena);
void asset_compiler_set_output(asset_compiler* compiler, char* path);
void asset_compiler_set_compression(asset_compiler* compiler, 
                                   asset_compression compression, u32 level);

// Add assets to pack
b32 asset_compiler_add_texture(asset_compiler* compiler, char* name, char* path);
b32 asset_compiler_add_mesh(asset_compiler* compiler, char* name, char* path);
b32 asset_compiler_add_sound(asset_compiler* compiler, char* name, char* path);

// Build asset pack
b32 asset_compiler_build(asset_compiler* compiler);
void asset_compiler_destroy(asset_compiler* compiler);

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

// Invalid handle constant
#define INVALID_ASSET_HANDLE ((asset_handle){0, 0})

// Handle validation
static inline b32 asset_handle_valid(asset_handle handle) {
    return handle.id != 0;
}

// Asset type names
static inline char* asset_type_name(asset_type type) {
    switch (type) {
        case ASSET_TYPE_TEXTURE: return "Texture";
        case ASSET_TYPE_MESH: return "Mesh";
        case ASSET_TYPE_SOUND: return "Sound";
        case ASSET_TYPE_SHADER: return "Shader";
        case ASSET_TYPE_MATERIAL: return "Material";
        case ASSET_TYPE_ANIMATION: return "Animation";
        case ASSET_TYPE_FONT: return "Font";
        default: return "Unknown";
    }
}

#endif // HANDMADE_ASSETS_H