/*
    Handmade AAA Asset Streaming System
    
    Production-quality streaming for 100GB+ games with 2GB memory budget
    - Virtual texture system with 4K page tiles
    - Multi-threaded streaming with priority queues  
    - Memory management with LRU eviction
    - Predictive prefetching based on camera movement
    - Zero-hitch streaming guarantees
    - On-the-fly compression/decompression
    - Automatic LOD generation and switching
    
    Inspired by: GTA V, Cyberpunk 2077, Unreal Engine 5
*/

#ifndef HANDMADE_STREAMING_H
#define HANDMADE_STREAMING_H

#include "handmade_platform.h"
#include "handmade_threading.h"
#include <stdatomic.h>

// Vector math types
typedef struct v3 { f32 x, y, z; } v3;
typedef struct m4x4 { f32 m[16]; } m4x4;

// Forward declarations
typedef struct SpatialNode SpatialNode;

// Configuration
#define STREAMING_MEMORY_BUDGET     GIGABYTES(2)
#define VIRTUAL_TEXTURE_PAGE_SIZE   4096  // 4K tiles
#define VIRTUAL_TEXTURE_CACHE_SIZE  GIGABYTES(1)
#define STREAMING_THREAD_COUNT      4
#define PREFETCH_RADIUS            500.0f  // World units
#define MAX_STREAMING_REQUESTS     1024
#define MAX_RESIDENT_ASSETS        4096
#define COMPRESSION_BLOCK_SIZE     65536
#define LOD_LEVELS                 5
#define STREAMING_RING_SIZE        32  // Concentric rings for predictive loading

// Virtual texture configuration
#define VT_PAGE_SIZE_BITS          12  // 4096 = 2^12
#define VT_PAGE_SIZE               (1 << VT_PAGE_SIZE_BITS)
#define VT_PAGE_MASK               (VT_PAGE_SIZE - 1)
#define VT_MAX_MIP_LEVELS          14  // Up to 16K x 16K
#define VT_INDIRECTION_SIZE        2048
#define VT_CACHE_PAGES             16384  // 64MB with 4K pages

// Streaming priorities
typedef enum {
    STREAM_PRIORITY_CRITICAL = 0,   // Player visible, close
    STREAM_PRIORITY_HIGH = 1,       // Player visible, medium distance
    STREAM_PRIORITY_NORMAL = 2,     // Potentially visible soon
    STREAM_PRIORITY_PREFETCH = 3,   // Predictive loading
    STREAM_PRIORITY_LOW = 4,        // Background streaming
    STREAM_PRIORITY_COUNT
} StreamPriority;

// Asset types with streaming support
typedef enum {
    STREAM_TYPE_TEXTURE = 0,
    STREAM_TYPE_MESH,
    STREAM_TYPE_AUDIO,
    STREAM_TYPE_ANIMATION,
    STREAM_TYPE_WORLD_CHUNK,
    STREAM_TYPE_COUNT
} StreamAssetType;

// Compression methods
typedef enum {
    COMPRESS_NONE = 0,
    COMPRESS_LZ4,      // Fast decompression
    COMPRESS_ZSTD,     // Better ratio
    COMPRESS_BC7,      // GPU texture compression
    COMPRESS_ASTC,     // Mobile GPU compression
} CompressionType;

// LOD information
typedef struct {
    u32 vertex_count;
    u32 index_count;
    f32 screen_size_threshold;  // Switch when object is this size on screen
    u32 data_offset;
    u32 data_size;
    u32 compressed_size;
    CompressionType compression;
} LODInfo;

// Virtual texture page
typedef struct {
    u16 x, y;           // Page coordinates in virtual space
    u8 mip_level;       // Mipmap level
    u8 format;          // Pixel format
    atomic_int ref_count;
    u64 last_access_frame;
    u32 cache_index;    // Index in physical cache
    void* data;         // Page data (when loaded)
    atomic_bool locked; // Prevent eviction
} VirtualTexturePage;

// Virtual texture
typedef struct {
    u32 width, height;
    u32 page_count_x, page_count_y;
    u8 mip_count;
    u32 format;
    
    // Page table (sparse)
    VirtualTexturePage** pages;  // 2D array of page pointers
    
    // Indirection texture (for GPU)
    u32 indirection_texture_id;
    u8* indirection_data;
    
    // Statistics
    atomic_uint pages_resident;
    atomic_uint pages_requested;
    atomic_uint pages_evicted;
} VirtualTexture;

// Streaming request
typedef struct StreamRequest {
    u64 asset_id;
    StreamAssetType type;
    StreamPriority priority;
    u32 lod_level;
    
    // Virtual texture specific
    VirtualTexturePage* vt_page;
    
    // Request state
    atomic_int status;  // 0=pending, 1=loading, 2=complete, 3=failed
    f32 distance_to_camera;
    u64 request_frame;
    
    // Callback when complete
    void (*callback)(struct StreamRequest* request, void* data);
    void* callback_data;
    
    // Linked list for queue
    struct StreamRequest* next;
    struct StreamRequest* prev;
} StreamRequest;

// Asset header (on disk)
typedef struct {
    u32 magic;          // 'HMAS' - Handmade Asset
    u32 version;
    u64 asset_id;
    StreamAssetType type;
    u32 flags;
    
    // Compression
    CompressionType compression;
    u64 uncompressed_size;
    u64 compressed_size;
    
    // LOD information
    u32 lod_count;
    LODInfo lods[LOD_LEVELS];
    
    // Dependencies
    u32 dependency_count;
    u64 dependencies[16];  // Asset IDs this depends on
    
    // Metadata
    char name[64];
    u32 checksum;
} AssetHeader;

// Resident asset in memory
typedef struct ResidentAsset {
    u64 asset_id;
    StreamAssetType type;
    u32 current_lod;
    
    // Memory management
    void* data;
    usize size;
    atomic_int ref_count;
    u64 last_access_frame;
    
    // LOD data pointers
    void* lod_data[LOD_LEVELS];
    u32 lod_sizes[LOD_LEVELS];
    
    // LRU list
    struct ResidentAsset* lru_next;
    struct ResidentAsset* lru_prev;
    
    // Hash table
    struct ResidentAsset* hash_next;
} ResidentAsset;

// Memory pool for streaming
typedef struct {
    u8* base;
    usize size;
    usize used;
    
    // Free list allocator
    struct FreeBlock* free_list;
    
    // Statistics
    atomic_uint allocations;
    atomic_uint deallocations;
    atomic_uint peak_usage;
    atomic_uint fragmentation_bytes;
} StreamingMemoryPool;

// Priority queue for requests
typedef struct {
    StreamRequest* requests[STREAM_PRIORITY_COUNT];
    atomic_uint counts[STREAM_PRIORITY_COUNT];
    pthread_mutex_t locks[STREAM_PRIORITY_COUNT];
} StreamPriorityQueue;

// Streaming ring for predictive loading
typedef struct {
    f32 inner_radius;
    f32 outer_radius;
    StreamPriority priority;
    u32 max_assets;
} StreamingRing;

// Camera prediction
typedef struct {
    v3 position;
    v3 velocity;
    v3 predicted_positions[8];  // Next 8 frames
    f32 fov;
    f32 aspect_ratio;
    m4x4 view_projection;
} CameraPrediction;

// Streaming statistics
typedef struct {
    atomic_uint total_requests;
    atomic_uint completed_requests;
    atomic_uint failed_requests;
    atomic_uint cache_hits;
    atomic_uint cache_misses;
    atomic_uint bytes_loaded;
    atomic_uint bytes_evicted;
    f32 average_load_time_ms;
    f32 peak_load_time_ms;
    usize current_memory_usage;
    usize peak_memory_usage;
} StreamingStats;

// Main streaming system
typedef struct StreamingSystem {
    // Configuration
    usize memory_budget;
    u32 thread_count;
    
    // Memory management
    StreamingMemoryPool memory_pool;
    
    // Asset management
    ResidentAsset* resident_assets[MAX_RESIDENT_ASSETS];
    u32 resident_count;
    ResidentAsset* lru_head;
    ResidentAsset* lru_tail;
    
    // Hash table for fast lookup
    ResidentAsset* asset_hash_table[4096];
    pthread_mutex_t hash_lock;
    
    // Virtual texture system
    VirtualTexture* virtual_textures[256];
    u32 vt_count;
    
    // Virtual texture cache
    u8* vt_cache_memory;
    u32 vt_cache_pages_used;
    VirtualTexturePage* vt_page_pool;
    atomic_uint vt_page_pool_index;
    
    // Request management
    StreamPriorityQueue request_queue;
    StreamRequest request_pool[MAX_STREAMING_REQUESTS];
    atomic_uint request_pool_index;
    
    // Predictive loading
    CameraPrediction camera_prediction;
    StreamingRing streaming_rings[STREAMING_RING_SIZE];
    
    // Worker threads
    pthread_t streaming_threads[STREAMING_THREAD_COUNT];
    atomic_bool should_exit;
    
    // IO thread (separate from decompression)
    pthread_t io_thread;
    
    // Decompression threads
    pthread_t decompress_threads[2];
    
    // Statistics
    StreamingStats stats;
    u64 current_frame;
    
    // File handles cache
    struct {
        i32 fd;
        char path[256];
        u64 last_access;
    } file_cache[32];
    u32 file_cache_count;
    
    // Compression buffers
    u8* compress_buffer;
    usize compress_buffer_size;
    
    // Spatial indexing
    void* spatial_root;  // SpatialNode* (forward declared)
    
    // Async I/O
    void* async_io_pool;  // AsyncIORequest pool
    u32 async_io_count;
    pthread_mutex_t async_io_lock;
    
    // Defragmentation state
    void* defrag_state;  // DefragState*
    pthread_mutex_t defrag_lock;
} StreamingSystem;

// === Core API ===

// Initialize streaming system
void streaming_init(StreamingSystem* system, usize memory_budget);
void streaming_shutdown(StreamingSystem* system);

// Update streaming system (call every frame)
void streaming_update(StreamingSystem* system, v3 camera_pos, v3 camera_vel, f32 dt);

// Request asset streaming
StreamRequest* streaming_request_asset(StreamingSystem* system, u64 asset_id, 
                                       StreamPriority priority, u32 lod_level);

// Check if asset is resident
bool streaming_is_resident(StreamingSystem* system, u64 asset_id, u32 lod_level);

// Get resident asset data
void* streaming_get_asset_data(StreamingSystem* system, u64 asset_id, u32 lod_level);

// Manually lock/unlock asset in memory
void streaming_lock_asset(StreamingSystem* system, u64 asset_id);
void streaming_unlock_asset(StreamingSystem* system, u64 asset_id);

// === Virtual Texture API ===

// Create virtual texture
VirtualTexture* streaming_create_virtual_texture(StreamingSystem* system, 
                                                 u32 width, u32 height, u32 format);

// Request virtual texture page
void streaming_request_vt_page(StreamingSystem* system, VirtualTexture* vt,
                               u32 x, u32 y, u8 mip_level);

// Update indirection texture (call after loading pages)
void streaming_update_vt_indirection(StreamingSystem* system, VirtualTexture* vt);

// Get page data
void* streaming_get_vt_page_data(VirtualTexture* vt, u32 x, u32 y, u8 mip_level);

// === LOD API ===

// Calculate appropriate LOD level
u32 streaming_calculate_lod(f32 distance, f32 object_radius, f32 fov);

// Switch LOD for resident asset
void streaming_switch_lod(StreamingSystem* system, u64 asset_id, u32 new_lod);

// Get current LOD level
u32 streaming_get_current_lod(StreamingSystem* system, u64 asset_id);

// === Predictive Loading ===

// Update camera prediction
void streaming_update_camera_prediction(StreamingSystem* system, 
                                        v3 pos, v3 vel, v3 accel);

// Configure streaming rings
void streaming_configure_rings(StreamingSystem* system, 
                               StreamingRing* rings, u32 count);

// Prefetch assets in radius
void streaming_prefetch_radius(StreamingSystem* system, v3 center, f32 radius);

// === Memory Management ===

// Evict least recently used assets
usize streaming_evict_lru(StreamingSystem* system, usize bytes_needed);

// Defragment memory pool
void streaming_defragment(StreamingSystem* system);

// Get memory statistics
void streaming_get_memory_stats(StreamingSystem* system, 
                                usize* used, usize* available, f32* fragmentation);

// === Compression ===

// Compress asset data
usize streaming_compress(u8* src, usize src_size, u8* dst, usize dst_capacity,
                         CompressionType type);

// Decompress asset data
usize streaming_decompress(u8* src, usize src_size, u8* dst, usize dst_capacity,
                           CompressionType type);

// === Statistics ===

// Get streaming statistics
StreamingStats streaming_get_stats(StreamingSystem* system);

// Reset statistics
void streaming_reset_stats(StreamingSystem* system);

// === Debug ===

// Debug visualization
void streaming_debug_draw(StreamingSystem* system, void* renderer);

// Dump streaming state
void streaming_dump_state(StreamingSystem* system, const char* filename);

// === Worker Thread Functions (Internal) ===

// IO thread function
void* streaming_io_thread(void* data);

// Decompression thread function
void* streaming_decompress_thread(void* data);

// Streaming worker thread
void* streaming_worker_thread(void* data);

// Process single request
void streaming_process_request(StreamingSystem* system, StreamRequest* request);

// Load asset from disk
bool streaming_load_asset(StreamingSystem* system, u64 asset_id, 
                          u32 lod_level, void** data, usize* size);

// === Internal Functions ===
// These are exposed in the header for testing purposes.
// In production, they would be static or in an internal header.

// Spatial indexing functions (for testing)
void spatial_node_insert(SpatialNode* node, u64 asset_id, v3 pos, f32 radius);
void spatial_node_query_radius(SpatialNode* node, v3 center, f32 radius,
                               u64* results, u32* result_count, u32 max_results);

#endif // HANDMADE_STREAMING_H