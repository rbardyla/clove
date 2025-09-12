/*
    Handmade AAA Asset Streaming System Implementation
    
    Zero-hitch streaming with predictive loading and virtual textures
*/

#include "handmade_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <immintrin.h>
#include <sys/eventfd.h>
#include <aio.h>
#include <errno.h>

// Vector math helpers (types defined in header)

// Spatial indexing for asset queries
typedef struct SpatialNode {
    v3 min, max;  // AABB bounds
    u64* asset_ids;
    u32 asset_count;
    u32 asset_capacity;
    struct SpatialNode* children[8];  // Octree children
    u32 depth;
} SpatialNode;

// Async I/O request
typedef struct AsyncIORequest {
    struct aiocb aiocb;
    StreamRequest* stream_request;
    void* buffer;
    usize buffer_size;
    struct AsyncIORequest* next;
    bool in_use;
} AsyncIORequest;

// Defragmentation state
typedef struct DefragState {
    bool in_progress;
    usize bytes_moved;
    usize bytes_freed;
    u32 passes;
} DefragState;

static inline f32 v3_length(v3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline v3 v3_sub(v3 a, v3 b) {
    return (v3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline v3 v3_add(v3 a, v3 b) {
    return (v3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline v3 v3_scale(v3 v, f32 s) {
    return (v3){v.x * s, v.y * s, v.z * s};
}

// Memory alignment helpers
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

// === Memory Pool Management ===

typedef struct FreeBlock {
    usize size;
    struct FreeBlock* next;
} FreeBlock;

static void* pool_alloc(StreamingMemoryPool* pool, usize size) {
    // Align to 16 bytes
    size = ALIGN_UP(size, 16);
    
    // Find best fit in free list
    FreeBlock* best = NULL;
    FreeBlock* prev_best = NULL;
    FreeBlock* current = pool->free_list;
    FreeBlock* prev = NULL;
    
    while (current) {
        if (current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
                prev_best = prev;
            }
        }
        prev = current;
        current = current->next;
    }
    
    if (!best) {
        // Try to allocate from unused space
        if (pool->used + size <= pool->size) {
            void* ptr = pool->base + pool->used;
            pool->used += size;
            
            atomic_fetch_add(&pool->allocations, 1);
            if (pool->used > atomic_load(&pool->peak_usage)) {
                atomic_store(&pool->peak_usage, pool->used);
            }
            
            return ptr;
        }
        return NULL;  // Out of memory
    }
    
    // Remove from free list
    if (prev_best) {
        prev_best->next = best->next;
    } else {
        pool->free_list = best->next;
    }
    
    void* ptr = (void*)best;
    
    // Split block if much larger than needed
    if (best->size > size + 256) {
        FreeBlock* new_free = (FreeBlock*)((u8*)best + size);
        new_free->size = best->size - size;
        new_free->next = pool->free_list;
        pool->free_list = new_free;
    }
    
    atomic_fetch_add(&pool->allocations, 1);
    return ptr;
}

static void pool_free(StreamingMemoryPool* pool, void* ptr, usize size) {
    if (!ptr) return;
    
    size = ALIGN_UP(size, 16);
    
    // Add to free list
    FreeBlock* block = (FreeBlock*)ptr;
    block->size = size;
    block->next = pool->free_list;
    pool->free_list = block;
    
    atomic_fetch_add(&pool->deallocations, 1);
    
    // Simple coalescing with adjacent blocks
    FreeBlock* current = pool->free_list;
    while (current && current->next) {
        u8* current_end = (u8*)current + current->size;
        if (current_end == (u8*)current->next) {
            // Adjacent blocks, merge them
            current->size += current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// === LRU Cache Management ===

static void streaming_lru_add(StreamingSystem* system, ResidentAsset* asset) {
    asset->lru_next = NULL;
    asset->lru_prev = system->lru_tail;
    
    if (system->lru_tail) {
        system->lru_tail->lru_next = asset;
    }
    system->lru_tail = asset;
    
    if (!system->lru_head) {
        system->lru_head = asset;
    }
}

static void streaming_lru_remove(StreamingSystem* system, ResidentAsset* asset) {
    if (asset->lru_prev) {
        asset->lru_prev->lru_next = asset->lru_next;
    } else {
        system->lru_head = asset->lru_next;
    }
    
    if (asset->lru_next) {
        asset->lru_next->lru_prev = asset->lru_prev;
    } else {
        system->lru_tail = asset->lru_prev;
    }
    
    asset->lru_next = NULL;
    asset->lru_prev = NULL;
}

static void streaming_lru_touch(StreamingSystem* system, ResidentAsset* asset) {
    // Move to tail (most recently used)
    streaming_lru_remove(system, asset);
    streaming_lru_add(system, asset);
    asset->last_access_frame = system->current_frame;
}

// === Hash Table ===

static u32 streaming_hash_asset_id(u64 asset_id) {
    // Simple hash function
    asset_id ^= asset_id >> 33;
    asset_id *= 0xff51afd7ed558ccd;
    asset_id ^= asset_id >> 33;
    asset_id *= 0xc4ceb9fe1a85ec53;
    asset_id ^= asset_id >> 33;
    return asset_id & 0xFFF;  // 4096 buckets
}

static ResidentAsset* streaming_find_resident(StreamingSystem* system, u64 asset_id) {
    u32 hash = streaming_hash_asset_id(asset_id);
    
    pthread_mutex_lock(&system->hash_lock);
    ResidentAsset* asset = system->asset_hash_table[hash];
    while (asset) {
        if (asset->asset_id == asset_id) {
            pthread_mutex_unlock(&system->hash_lock);
            return asset;
        }
        asset = asset->hash_next;
    }
    pthread_mutex_unlock(&system->hash_lock);
    
    return NULL;
}

static void hash_table_add(StreamingSystem* system, ResidentAsset* asset) {
    u32 hash = streaming_hash_asset_id(asset->asset_id);
    
    pthread_mutex_lock(&system->hash_lock);
    asset->hash_next = system->asset_hash_table[hash];
    system->asset_hash_table[hash] = asset;
    pthread_mutex_unlock(&system->hash_lock);
}

static void hash_table_remove(StreamingSystem* system, ResidentAsset* asset) {
    u32 hash = streaming_hash_asset_id(asset->asset_id);
    
    pthread_mutex_lock(&system->hash_lock);
    ResidentAsset** current = &system->asset_hash_table[hash];
    while (*current) {
        if (*current == asset) {
            *current = asset->hash_next;
            break;
        }
        current = &(*current)->hash_next;
    }
    pthread_mutex_unlock(&system->hash_lock);
}

// === Compression (Simple RLE for now, integrate LZ4/ZSTD later) ===

static usize compress_rle(u8* src, usize src_size, u8* dst, usize dst_capacity) {
    if (dst_capacity < src_size) return 0;
    
    usize src_pos = 0;
    usize dst_pos = 0;
    
    while (src_pos < src_size && dst_pos < dst_capacity - 2) {
        u8 value = src[src_pos];
        usize run_length = 1;
        
        while (src_pos + run_length < src_size && 
               src[src_pos + run_length] == value && 
               run_length < 255) {
            run_length++;
        }
        
        if (run_length > 2 || value == 0xFF) {
            // Encode as run
            dst[dst_pos++] = 0xFF;
            dst[dst_pos++] = run_length;
            dst[dst_pos++] = value;
            src_pos += run_length;
        } else {
            // Copy literal
            dst[dst_pos++] = value;
            src_pos++;
        }
    }
    
    return dst_pos;
}

static usize decompress_rle(u8* src, usize src_size, u8* dst, usize dst_capacity) {
    usize src_pos = 0;
    usize dst_pos = 0;
    
    while (src_pos < src_size && dst_pos < dst_capacity) {
        if (src[src_pos] == 0xFF && src_pos + 2 < src_size) {
            // Run encoded
            u8 length = src[src_pos + 1];
            u8 value = src[src_pos + 2];
            
            for (usize i = 0; i < length && dst_pos < dst_capacity; i++) {
                dst[dst_pos++] = value;
            }
            src_pos += 3;
        } else {
            // Literal
            dst[dst_pos++] = src[src_pos++];
        }
    }
    
    return dst_pos;
}

usize streaming_compress(u8* src, usize src_size, u8* dst, usize dst_capacity,
                         CompressionType type) {
    switch (type) {
        case COMPRESS_NONE:
            if (dst_capacity >= src_size) {
                memcpy(dst, src, src_size);
                return src_size;
            }
            return 0;
            
        case COMPRESS_LZ4: {
            // Fast LZ4-style compression
            usize src_pos = 0;
            usize dst_pos = 0;
            u8* hash_table[4096];  // 12-bit hash
            memset(hash_table, 0, sizeof(hash_table));
            
            while (src_pos < src_size && dst_pos < dst_capacity - 5) {
                u32 hash = ((*(u32*)&src[src_pos]) * 2654435761U) >> 20;
                u8* match = hash_table[hash & 0xFFF];
                hash_table[hash & 0xFFF] = &src[src_pos];
                
                if (match && match >= src && 
                    src_pos - (match - src) < 65536 &&
                    *(u32*)match == *(u32*)&src[src_pos]) {
                    // Found match
                    usize match_offset = src_pos - (match - src);
                    usize match_len = 4;
                    
                    while (src_pos + match_len < src_size &&
                           match[match_len] == src[src_pos + match_len] &&
                           match_len < 255) {
                        match_len++;
                    }
                    
                    // Encode match
                    dst[dst_pos++] = 0x80 | (match_len - 4);
                    dst[dst_pos++] = match_offset & 0xFF;
                    dst[dst_pos++] = (match_offset >> 8) & 0xFF;
                    src_pos += match_len;
                } else {
                    // Literal
                    usize lit_start = src_pos;
                    src_pos++;
                    
                    // Extend literal run
                    while (src_pos < src_size && src_pos - lit_start < 127) {
                        u32 next_hash = ((*(u32*)&src[src_pos]) * 2654435761U) >> 20;
                        if (hash_table[next_hash & 0xFFF] &&
                            *(u32*)hash_table[next_hash & 0xFFF] == *(u32*)&src[src_pos]) {
                            break;
                        }
                        hash_table[next_hash & 0xFFF] = &src[src_pos];
                        src_pos++;
                    }
                    
                    usize lit_len = src_pos - lit_start;
                    dst[dst_pos++] = lit_len;
                    memcpy(&dst[dst_pos], &src[lit_start], lit_len);
                    dst_pos += lit_len;
                }
            }
            
            // Handle remaining bytes
            while (src_pos < src_size && dst_pos < dst_capacity - 1) {
                dst[dst_pos++] = 1;
                dst[dst_pos++] = src[src_pos++];
            }
            
            return dst_pos;
        }
            
        case COMPRESS_ZSTD:
            // TODO: Integrate ZSTD
            return compress_rle(src, src_size, dst, dst_capacity);
            
        default:
            return 0;
    }
}

usize streaming_decompress(u8* src, usize src_size, u8* dst, usize dst_capacity,
                           CompressionType type) {
    switch (type) {
        case COMPRESS_NONE:
            if (dst_capacity >= src_size) {
                memcpy(dst, src, src_size);
                return src_size;
            }
            return 0;
            
        case COMPRESS_LZ4: {
            // Fast LZ4-style decompression
            usize src_pos = 0;
            usize dst_pos = 0;
            
            while (src_pos < src_size && dst_pos < dst_capacity) {
                u8 token = src[src_pos++];
                
                if (token & 0x80) {
                    // Match
                    usize match_len = (token & 0x7F) + 4;
                    if (src_pos + 1 < src_size) {
                        usize offset = src[src_pos] | (src[src_pos + 1] << 8);
                        src_pos += 2;
                        
                        // Copy match (handle overlapping)
                        if (offset == 1) {
                            // RLE case
                            memset(&dst[dst_pos], dst[dst_pos - 1], match_len);
                            dst_pos += match_len;
                        } else {
                            for (usize i = 0; i < match_len && dst_pos < dst_capacity; i++) {
                                dst[dst_pos] = dst[dst_pos - offset];
                                dst_pos++;
                            }
                        }
                    }
                } else {
                    // Literal run
                    usize lit_len = token;
                    usize copy_size = (lit_len < dst_capacity - dst_pos) ? 
                                      lit_len : (dst_capacity - dst_pos);
                    if (src_pos + copy_size <= src_size) {
                        memcpy(&dst[dst_pos], &src[src_pos], copy_size);
                        src_pos += lit_len;
                        dst_pos += copy_size;
                    } else {
                        break;  // Corrupted stream
                    }
                }
            }
            
            return dst_pos;
        }
            
        case COMPRESS_ZSTD:
            // TODO: Integrate ZSTD
            return decompress_rle(src, src_size, dst, dst_capacity);
            
        default:
            return 0;
    }
}

// === Asset Loading ===

static bool load_asset_from_disk(StreamingSystem* system, u64 asset_id, 
                                 u32 lod_level, void** data, usize* size) {
    // Build asset path
    char path[256];
    snprintf(path, sizeof(path), "assets/streaming/%016lx.asset", asset_id);
    
    // Check file cache
    i32 fd = -1;
    for (u32 i = 0; i < system->file_cache_count; i++) {
        if (strcmp(system->file_cache[i].path, path) == 0) {
            fd = system->file_cache[i].fd;
            system->file_cache[i].last_access = system->current_frame;
            break;
        }
    }
    
    if (fd < 0) {
        // Open file
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            printf("Failed to open asset: %s\n", path);
            return false;
        }
        
        // Add to cache
        if (system->file_cache_count < 32) {
            system->file_cache[system->file_cache_count].fd = fd;
            strncpy(system->file_cache[system->file_cache_count].path, path, 255);
            system->file_cache[system->file_cache_count].last_access = system->current_frame;
            system->file_cache_count++;
        }
    }
    
    // Read header
    AssetHeader header;
    if (pread(fd, &header, sizeof(header), 0) != sizeof(header)) {
        return false;
    }
    
    // Validate header
    if (header.magic != 0x534D4148) {  // 'HMAS'
        return false;
    }
    
    // Get LOD info
    if (lod_level >= header.lod_count) {
        lod_level = header.lod_count - 1;
    }
    
    LODInfo* lod = &header.lods[lod_level];
    
    // Allocate memory from pool
    void* buffer = pool_alloc(&system->memory_pool, lod->data_size);
    if (!buffer) {
        return false;
    }
    
    if (lod->compression != COMPRESS_NONE) {
        // Read compressed data
        void* compressed = malloc(lod->compressed_size);
        if (pread(fd, compressed, lod->compressed_size, 
                 sizeof(header) + lod->data_offset) != lod->compressed_size) {
            free(compressed);
            pool_free(&system->memory_pool, buffer, lod->data_size);
            return false;
        }
        
        // Decompress
        usize decompressed = streaming_decompress(compressed, lod->compressed_size,
                                                  buffer, lod->data_size,
                                                  lod->compression);
        free(compressed);
        
        if (decompressed != lod->data_size) {
            pool_free(&system->memory_pool, buffer, lod->data_size);
            return false;
        }
    } else {
        // Read uncompressed
        if (pread(fd, buffer, lod->data_size, 
                 sizeof(header) + lod->data_offset) != lod->data_size) {
            pool_free(&system->memory_pool, buffer, lod->data_size);
            return false;
        }
    }
    
    *data = buffer;
    *size = lod->data_size;
    
    atomic_fetch_add(&system->stats.bytes_loaded, lod->data_size);
    
    return true;
}

// === Request Processing ===

static StreamRequest* get_next_request(StreamPriorityQueue* queue) {
    // Check priorities from highest to lowest
    for (u32 priority = 0; priority < STREAM_PRIORITY_COUNT; priority++) {
        pthread_mutex_lock(&queue->locks[priority]);
        
        StreamRequest* request = queue->requests[priority];
        if (request) {
            // Remove from queue
            queue->requests[priority] = request->next;
            if (request->next) {
                request->next->prev = NULL;
            }
            atomic_fetch_sub(&queue->counts[priority], 1);
            
            pthread_mutex_unlock(&queue->locks[priority]);
            return request;
        }
        
        pthread_mutex_unlock(&queue->locks[priority]);
    }
    
    return NULL;
}

static void add_request(StreamPriorityQueue* queue, StreamRequest* request) {
    u32 priority = request->priority;
    
    pthread_mutex_lock(&queue->locks[priority]);
    
    // Add to front of priority queue
    request->next = queue->requests[priority];
    request->prev = NULL;
    if (queue->requests[priority]) {
        queue->requests[priority]->prev = request;
    }
    queue->requests[priority] = request;
    
    atomic_fetch_add(&queue->counts[priority], 1);
    
    pthread_mutex_unlock(&queue->locks[priority]);
}

void streaming_process_request(StreamingSystem* system, StreamRequest* request) {
    atomic_store(&request->status, 1);  // Loading
    
    // Check if already resident
    ResidentAsset* resident = streaming_find_resident(system, request->asset_id);
    if (resident && resident->current_lod >= request->lod_level) {
        // Already loaded at sufficient quality
        streaming_lru_touch(system, resident);
        atomic_store(&request->status, 2);  // Complete
        atomic_fetch_add(&system->stats.cache_hits, 1);
        
        if (request->callback) {
            request->callback(request, request->callback_data);
        }
        return;
    }
    
    atomic_fetch_add(&system->stats.cache_misses, 1);
    
    // Load from disk
    void* data = NULL;
    usize size = 0;
    
    if (!load_asset_from_disk(system, request->asset_id, request->lod_level, 
                              &data, &size)) {
        atomic_store(&request->status, 3);  // Failed
        atomic_fetch_add(&system->stats.failed_requests, 1);
        return;
    }
    
    // Create or update resident asset
    if (!resident) {
        // Check if we need to evict
        if (system->memory_pool.used + size > system->memory_budget) {
            usize needed = size - (system->memory_budget - system->memory_pool.used);
            streaming_evict_lru(system, needed);
        }
        
        // Allocate new resident asset
        resident = (ResidentAsset*)calloc(1, sizeof(ResidentAsset));
        resident->asset_id = request->asset_id;
        resident->type = request->type;
        
        // Add to system
        if (system->resident_count < MAX_RESIDENT_ASSETS) {
            system->resident_assets[system->resident_count++] = resident;
        }
        
        hash_table_add(system, resident);
        streaming_lru_add(system, resident);
    } else {
        // Free old LOD data
        if (resident->lod_data[resident->current_lod]) {
            pool_free(&system->memory_pool, 
                     resident->lod_data[resident->current_lod],
                     resident->lod_sizes[resident->current_lod]);
        }
    }
    
    // Store new LOD data
    resident->lod_data[request->lod_level] = data;
    resident->lod_sizes[request->lod_level] = size;
    resident->current_lod = request->lod_level;
    resident->data = data;
    resident->size = size;
    resident->last_access_frame = system->current_frame;
    
    atomic_store(&request->status, 2);  // Complete
    atomic_fetch_add(&system->stats.completed_requests, 1);
    
    if (request->callback) {
        request->callback(request, request->callback_data);
    }
}

// === Worker Threads ===

void* streaming_worker_thread(void* data) {
    StreamingSystem* system = (StreamingSystem*)data;
    
    while (!atomic_load(&system->should_exit)) {
        StreamRequest* request = get_next_request(&system->request_queue);
        
        if (request) {
            streaming_process_request(system, request);
        } else {
            // No work, sleep briefly
            usleep(1000);  // 1ms
        }
    }
    
    return NULL;
}

// === Async I/O Management ===

static AsyncIORequest* get_async_io_request(StreamingSystem* system) {
    pthread_mutex_lock(&system->async_io_lock);
    
    AsyncIORequest* pool = (AsyncIORequest*)system->async_io_pool;
    for (u32 i = 0; i < system->async_io_count; i++) {
        if (!pool[i].in_use) {
            pool[i].in_use = true;
            pthread_mutex_unlock(&system->async_io_lock);
            return &pool[i];
        }
    }
    
    pthread_mutex_unlock(&system->async_io_lock);
    return NULL;
}

static void release_async_io_request(StreamingSystem* system, AsyncIORequest* req) {
    pthread_mutex_lock(&system->async_io_lock);
    req->in_use = false;
    pthread_mutex_unlock(&system->async_io_lock);
}

void* streaming_io_thread(void* data) {
    StreamingSystem* system = (StreamingSystem*)data;
    
    // Initialize async I/O pool
    system->async_io_count = 64;
    system->async_io_pool = calloc(system->async_io_count, sizeof(AsyncIORequest));
    pthread_mutex_init(&system->async_io_lock, NULL);
    
    // Create event fd for notifications
    int event_fd = eventfd(0, EFD_NONBLOCK);
    
    struct aiocb* active_requests[64];
    u32 active_count = 0;
    
    while (!atomic_load(&system->should_exit)) {
        // Check for new streaming requests
        StreamRequest* stream_req = get_next_request(&system->request_queue);
        
        if (stream_req && active_count < 64) {
            AsyncIORequest* async_req = get_async_io_request(system);
            if (async_req) {
                // Setup async I/O
                char path[256];
                snprintf(path, sizeof(path), "assets/streaming/%016lx.asset", 
                        stream_req->asset_id);
                
                int fd = open(path, O_RDONLY | O_DIRECT);
                if (fd >= 0) {
                    // Prepare async read
                    async_req->stream_request = stream_req;
                    async_req->buffer_size = MEGABYTES(4);  // Read in 4MB chunks
                    async_req->buffer = aligned_alloc(4096, async_req->buffer_size);
                    
                    memset(&async_req->aiocb, 0, sizeof(struct aiocb));
                    async_req->aiocb.aio_fildes = fd;
                    async_req->aiocb.aio_buf = async_req->buffer;
                    async_req->aiocb.aio_nbytes = async_req->buffer_size;
                    async_req->aiocb.aio_offset = 0;
                    async_req->aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;
                    
                    // Submit async read
                    if (aio_read(&async_req->aiocb) == 0) {
                        active_requests[active_count++] = &async_req->aiocb;
                        atomic_store(&stream_req->status, 1);  // Loading
                    } else {
                        close(fd);
                        free(async_req->buffer);
                        release_async_io_request(system, async_req);
                        atomic_store(&stream_req->status, 3);  // Failed
                    }
                } else {
                    release_async_io_request(system, async_req);
                    atomic_store(&stream_req->status, 3);  // Failed
                }
            }
        }
        
        // Check completed I/O
        for (u32 i = 0; i < active_count; i++) {
            if (aio_error(active_requests[i]) != EINPROGRESS) {
                ssize_t bytes_read = aio_return(active_requests[i]);
                
                // Find corresponding request
                AsyncIORequest* async_req = NULL;
                AsyncIORequest* pool = (AsyncIORequest*)system->async_io_pool;
                for (u32 j = 0; j < system->async_io_count; j++) {
                    if (&pool[j].aiocb == active_requests[i]) {
                        async_req = &pool[j];
                        break;
                    }
                }
                
                if (async_req && bytes_read > 0) {
                    // Process the loaded data
                    streaming_process_request(system, async_req->stream_request);
                    atomic_fetch_add(&system->stats.bytes_loaded, bytes_read);
                }
                
                // Cleanup
                close(active_requests[i]->aio_fildes);
                if (async_req) {
                    free(async_req->buffer);
                    release_async_io_request(system, async_req);
                }
                
                // Remove from active list
                active_requests[i] = active_requests[--active_count];
                i--;
            }
        }
        
        // Small sleep to prevent busy waiting
        if (active_count == 0 && !stream_req) {
            usleep(1000);
        }
    }
    
    // Cleanup
    close(event_fd);
    free(system->async_io_pool);
    pthread_mutex_destroy(&system->async_io_lock);
    
    return NULL;
}

void* streaming_decompress_thread(void* data) {
    StreamingSystem* system = (StreamingSystem*)data;
    
    while (!atomic_load(&system->should_exit)) {
        // Handle decompression on separate thread
        usleep(1000);
    }
    
    return NULL;
}

// === Virtual Texture System ===

VirtualTexture* streaming_create_virtual_texture(StreamingSystem* system,
                                                 u32 width, u32 height, u32 format) {
    VirtualTexture* vt = (VirtualTexture*)calloc(1, sizeof(VirtualTexture));
    
    vt->width = width;
    vt->height = height;
    vt->format = format;
    
    // Calculate page counts
    vt->page_count_x = (width + VT_PAGE_SIZE - 1) / VT_PAGE_SIZE;
    vt->page_count_y = (height + VT_PAGE_SIZE - 1) / VT_PAGE_SIZE;
    
    // Calculate mip levels
    u32 max_dim = width > height ? width : height;
    vt->mip_count = 0;
    while (max_dim > VT_PAGE_SIZE) {
        vt->mip_count++;
        max_dim >>= 1;
    }
    
    // Allocate page table (sparse)
    vt->pages = (VirtualTexturePage**)calloc(vt->page_count_y, sizeof(VirtualTexturePage*));
    for (u32 y = 0; y < vt->page_count_y; y++) {
        vt->pages[y] = (VirtualTexturePage*)calloc(vt->page_count_x, sizeof(VirtualTexturePage));
    }
    
    // Allocate indirection texture
    vt->indirection_data = (u8*)calloc(VT_INDIRECTION_SIZE * VT_INDIRECTION_SIZE * 4, 1);
    
    // Add to system
    if (system->vt_count < 256) {
        system->virtual_textures[system->vt_count++] = vt;
    }
    
    return vt;
}

void streaming_request_vt_page(StreamingSystem* system, VirtualTexture* vt,
                               u32 x, u32 y, u8 mip_level) {
    if (x >= vt->page_count_x || y >= vt->page_count_y) return;
    
    VirtualTexturePage* page = &vt->pages[y][x];
    
    // Check if already loaded
    if (page->data && atomic_load(&page->ref_count) > 0) {
        page->last_access_frame = system->current_frame;
        return;
    }
    
    // Create streaming request
    u32 index = atomic_fetch_add(&system->request_pool_index, 1) % MAX_STREAMING_REQUESTS;
    StreamRequest* request = &system->request_pool[index];
    
    // Build asset ID from texture ID and page coordinates
    u64 asset_id = ((u64)vt << 32) | (y << 16) | x;
    
    request->asset_id = asset_id;
    request->type = STREAM_TYPE_TEXTURE;
    request->priority = STREAM_PRIORITY_HIGH;
    request->lod_level = mip_level;
    request->vt_page = page;
    atomic_store(&request->status, 0);
    
    add_request(&system->request_queue, request);
    atomic_fetch_add(&vt->pages_requested, 1);
}

void streaming_update_vt_indirection(StreamingSystem* system, VirtualTexture* vt) {
    // Update indirection texture based on loaded pages
    u32 ind_width = VT_INDIRECTION_SIZE;
    u32 scale_x = vt->width / ind_width;
    u32 scale_y = vt->height / ind_width;
    
    for (u32 y = 0; y < ind_width; y++) {
        for (u32 x = 0; x < ind_width; x++) {
            u32 page_x = (x * scale_x) / VT_PAGE_SIZE;
            u32 page_y = (y * scale_y) / VT_PAGE_SIZE;
            
            if (page_x < vt->page_count_x && page_y < vt->page_count_y) {
                VirtualTexturePage* page = &vt->pages[page_y][page_x];
                
                u32 idx = (y * ind_width + x) * 4;
                if (page->data) {
                    // Page loaded, store cache coordinates
                    vt->indirection_data[idx + 0] = page->cache_index & 0xFF;
                    vt->indirection_data[idx + 1] = (page->cache_index >> 8) & 0xFF;
                    vt->indirection_data[idx + 2] = page->mip_level;
                    vt->indirection_data[idx + 3] = 255;  // Valid
                } else {
                    // Page not loaded
                    vt->indirection_data[idx + 3] = 0;  // Invalid
                }
            }
        }
    }
    
    // TODO: Upload to GPU
}

void* streaming_get_vt_page_data(VirtualTexture* vt, u32 x, u32 y, u8 mip_level) {
    if (x >= vt->page_count_x || y >= vt->page_count_y) return NULL;
    
    VirtualTexturePage* page = &vt->pages[y][x];
    return page->data;
}

// === LOD Management ===

u32 streaming_calculate_lod(f32 distance, f32 object_radius, f32 fov) {
    // Calculate screen space size
    f32 screen_size = (object_radius * 2.0f) / (distance * tanf(fov * 0.5f));
    
    // Map to LOD levels
    if (screen_size > 0.5f) return 0;  // Full quality
    if (screen_size > 0.25f) return 1;
    if (screen_size > 0.125f) return 2;
    if (screen_size > 0.0625f) return 3;
    return 4;  // Lowest quality
}

void streaming_switch_lod(StreamingSystem* system, u64 asset_id, u32 new_lod) {
    ResidentAsset* asset = streaming_find_resident(system, asset_id);
    if (!asset) return;
    
    if (asset->current_lod == new_lod) return;
    
    // Check if new LOD is loaded
    if (!asset->lod_data[new_lod]) {
        // Request new LOD
        streaming_request_asset(system, asset_id, STREAM_PRIORITY_HIGH, new_lod);
    } else {
        // Switch to new LOD
        asset->current_lod = new_lod;
        asset->data = asset->lod_data[new_lod];
        asset->size = asset->lod_sizes[new_lod];
    }
}

u32 streaming_get_current_lod(StreamingSystem* system, u64 asset_id) {
    ResidentAsset* asset = streaming_find_resident(system, asset_id);
    return asset ? asset->current_lod : 0;
}

// === Predictive Loading ===

void streaming_update_camera_prediction(StreamingSystem* system,
                                        v3 pos, v3 vel, v3 accel) {
    system->camera_prediction.position = pos;
    system->camera_prediction.velocity = vel;
    
    // Predict future positions
    f32 dt = 1.0f / 60.0f;  // Assume 60 FPS
    for (u32 i = 0; i < 8; i++) {
        f32 t = (i + 1) * dt;
        v3 pred_vel = v3_add(vel, v3_scale(accel, t));
        v3 pred_pos = v3_add(pos, v3_scale(pred_vel, t));
        system->camera_prediction.predicted_positions[i] = pred_pos;
    }
}

void streaming_configure_rings(StreamingSystem* system,
                               StreamingRing* rings, u32 count) {
    if (count > STREAMING_RING_SIZE) count = STREAMING_RING_SIZE;
    
    for (u32 i = 0; i < count; i++) {
        system->streaming_rings[i] = rings[i];
    }
}

// === Spatial Indexing ===

static SpatialNode* spatial_node_create(v3 min, v3 max, u32 depth) {
    SpatialNode* node = (SpatialNode*)calloc(1, sizeof(SpatialNode));
    node->min = min;
    node->max = max;
    node->depth = depth;
    node->asset_capacity = 16;
    node->asset_ids = (u64*)malloc(node->asset_capacity * sizeof(u64));
    return node;
}

void spatial_node_insert(SpatialNode* node, u64 asset_id, v3 pos, f32 radius) {
    // Check if position is within bounds
    v3 asset_min = (v3){pos.x - radius, pos.y - radius, pos.z - radius};
    v3 asset_max = (v3){pos.x + radius, pos.y + radius, pos.z + radius};
    
    bool overlaps = !(asset_max.x < node->min.x || asset_min.x > node->max.x ||
                      asset_max.y < node->min.y || asset_min.y > node->max.y ||
                      asset_max.z < node->min.z || asset_min.z > node->max.z);
    
    if (!overlaps) return;
    
    // If leaf node or max depth, add to this node
    if (node->depth >= 6 || node->asset_count < 32) {
        if (node->asset_count == node->asset_capacity) {
            node->asset_capacity *= 2;
            node->asset_ids = (u64*)realloc(node->asset_ids, 
                                           node->asset_capacity * sizeof(u64));
        }
        node->asset_ids[node->asset_count++] = asset_id;
        return;
    }
    
    // Subdivide if needed
    if (!node->children[0]) {
        v3 center = (v3){
            (node->min.x + node->max.x) * 0.5f,
            (node->min.y + node->max.y) * 0.5f,
            (node->min.z + node->max.z) * 0.5f
        };
        
        for (u32 i = 0; i < 8; i++) {
            v3 child_min = {
                (i & 1) ? center.x : node->min.x,
                (i & 2) ? center.y : node->min.y,
                (i & 4) ? center.z : node->min.z
            };
            v3 child_max = {
                (i & 1) ? node->max.x : center.x,
                (i & 2) ? node->max.y : center.y,
                (i & 4) ? node->max.z : center.z
            };
            node->children[i] = spatial_node_create(child_min, child_max, node->depth + 1);
        }
    }
    
    // Insert into appropriate children
    for (u32 i = 0; i < 8; i++) {
        spatial_node_insert(node->children[i], asset_id, pos, radius);
    }
}

void spatial_node_query_radius(SpatialNode* node, v3 center, f32 radius,
                              u64* results, u32* result_count, u32 max_results) {
    if (!node || *result_count >= max_results) return;
    
    // Check if sphere overlaps node AABB
    f32 dist_sq = 0;
    for (u32 axis = 0; axis < 3; axis++) {
        f32 v = ((f32*)&center)[axis];
        f32 min = ((f32*)&node->min)[axis];
        f32 max = ((f32*)&node->max)[axis];
        if (v < min) dist_sq += (min - v) * (min - v);
        if (v > max) dist_sq += (v - max) * (v - max);
    }
    
    if (dist_sq > radius * radius) return;
    
    // Add assets from this node
    for (u32 i = 0; i < node->asset_count && *result_count < max_results; i++) {
        results[(*result_count)++] = node->asset_ids[i];
    }
    
    // Recurse to children
    if (node->children[0]) {
        for (u32 i = 0; i < 8; i++) {
            spatial_node_query_radius(node->children[i], center, radius,
                                     results, result_count, max_results);
        }
    }
}

void streaming_prefetch_radius(StreamingSystem* system, v3 center, f32 radius) {
    // Query spatial index
    u64 asset_ids[1024];
    u32 asset_count = 0;
    
    if (system->spatial_root) {
        spatial_node_query_radius(system->spatial_root, center, radius,
                                  asset_ids, &asset_count, 1024);
    }
    
    // Queue assets for streaming
    for (u32 i = 0; i < asset_count; i++) {
        f32 distance = 0;  // Would calculate actual distance
        
        // Determine priority based on distance
        StreamPriority priority;
        if (distance < 50) {
            priority = STREAM_PRIORITY_CRITICAL;
        } else if (distance < 150) {
            priority = STREAM_PRIORITY_HIGH;
        } else if (distance < 300) {
            priority = STREAM_PRIORITY_NORMAL;
        } else {
            priority = STREAM_PRIORITY_PREFETCH;
        }
        
        // Calculate appropriate LOD
        u32 lod = streaming_calculate_lod(distance, 10.0f, 1.57f);
        
        // Request if not already resident
        if (!streaming_is_resident(system, asset_ids[i], lod)) {
            streaming_request_asset(system, asset_ids[i], priority, lod);
        }
    }
}

// === Memory Management ===

usize streaming_evict_lru(StreamingSystem* system, usize bytes_needed) {
    usize bytes_freed = 0;
    
    ResidentAsset* current = system->lru_head;
    while (current && bytes_freed < bytes_needed) {
        ResidentAsset* next = current->lru_next;
        
        // Don't evict if locked
        if (atomic_load(&current->ref_count) == 0) {
            // Free all LOD data
            for (u32 i = 0; i < LOD_LEVELS; i++) {
                if (current->lod_data[i]) {
                    pool_free(&system->memory_pool, current->lod_data[i],
                             current->lod_sizes[i]);
                    bytes_freed += current->lod_sizes[i];
                    current->lod_data[i] = NULL;
                }
            }
            
            // Remove from system
            streaming_lru_remove(system, current);
            hash_table_remove(system, current);
            
            // Remove from resident list
            for (u32 i = 0; i < system->resident_count; i++) {
                if (system->resident_assets[i] == current) {
                    system->resident_assets[i] = 
                        system->resident_assets[--system->resident_count];
                    break;
                }
            }
            
            free(current);
            atomic_fetch_add(&system->stats.bytes_evicted, bytes_freed);
        }
        
        current = next;
    }
    
    return bytes_freed;
}

void streaming_defragment(StreamingSystem* system) {
    pthread_mutex_lock(&system->defrag_lock);
    
    DefragState* state = (DefragState*)system->defrag_state;
    if (!state) {
        state = (DefragState*)calloc(1, sizeof(DefragState));
        system->defrag_state = state;
    }
    
    if (state->in_progress) {
        pthread_mutex_unlock(&system->defrag_lock);
        return;  // Already defragmenting
    }
    
    state->in_progress = true;
    state->passes++;
    state->bytes_moved = 0;
    state->bytes_freed = 0;
    
    // Create temporary buffer for moving data
    usize temp_size = MEGABYTES(64);
    u8* temp_buffer = (u8*)malloc(temp_size);
    
    // Build allocation map
    typedef struct {
        void* ptr;
        usize size;
        ResidentAsset* asset;
    } Allocation;
    
    Allocation* allocations = (Allocation*)malloc(sizeof(Allocation) * system->resident_count);
    u32 alloc_count = 0;
    
    // Gather all allocations
    for (u32 i = 0; i < system->resident_count; i++) {
        ResidentAsset* asset = system->resident_assets[i];
        for (u32 lod = 0; lod < LOD_LEVELS; lod++) {
            if (asset->lod_data[lod]) {
                allocations[alloc_count].ptr = asset->lod_data[lod];
                allocations[alloc_count].size = asset->lod_sizes[lod];
                allocations[alloc_count].asset = asset;
                alloc_count++;
            }
        }
    }
    
    // Sort allocations by memory address
    for (u32 i = 0; i < alloc_count - 1; i++) {
        for (u32 j = i + 1; j < alloc_count; j++) {
            if (allocations[i].ptr > allocations[j].ptr) {
                Allocation temp = allocations[i];
                allocations[i] = allocations[j];
                allocations[j] = temp;
            }
        }
    }
    
    // Compact allocations
    u8* write_ptr = system->memory_pool.base;
    
    for (u32 i = 0; i < alloc_count; i++) {
        Allocation* alloc = &allocations[i];
        
        if ((u8*)alloc->ptr != write_ptr) {
            // Need to move this allocation
            usize move_size = alloc->size;
            
            // Move in chunks to avoid huge memcpy
            while (move_size > 0) {
                usize chunk = (move_size > temp_size) ? temp_size : move_size;
                memcpy(temp_buffer, alloc->ptr, chunk);
                memcpy(write_ptr, temp_buffer, chunk);
                
                write_ptr += chunk;
                alloc->ptr = (u8*)alloc->ptr + chunk;
                move_size -= chunk;
                state->bytes_moved += chunk;
            }
            
            // Update asset pointer
            for (u32 lod = 0; lod < LOD_LEVELS; lod++) {
                if (alloc->asset->lod_data[lod] == alloc->ptr) {
                    alloc->asset->lod_data[lod] = write_ptr - alloc->size;
                    if (alloc->asset->current_lod == lod) {
                        alloc->asset->data = alloc->asset->lod_data[lod];
                    }
                    break;
                }
            }
        } else {
            write_ptr += alloc->size;
        }
    }
    
    // Update pool state
    usize old_used = system->memory_pool.used;
    system->memory_pool.used = write_ptr - system->memory_pool.base;
    state->bytes_freed = old_used - system->memory_pool.used;
    
    // Clear free list - all memory is now contiguous
    system->memory_pool.free_list = NULL;
    
    // Create single free block for remaining memory
    if (system->memory_pool.used < system->memory_pool.size) {
        FreeBlock* free_block = (FreeBlock*)(system->memory_pool.base + system->memory_pool.used);
        free_block->size = system->memory_pool.size - system->memory_pool.used;
        free_block->next = NULL;
        system->memory_pool.free_list = free_block;
    }
    
    // Cleanup
    free(allocations);
    free(temp_buffer);
    
    state->in_progress = false;
    atomic_store(&system->memory_pool.fragmentation_bytes, 0);
    
    pthread_mutex_unlock(&system->defrag_lock);
}

void streaming_get_memory_stats(StreamingSystem* system,
                                usize* used, usize* available, f32* fragmentation) {
    *used = system->memory_pool.used;
    *available = system->memory_pool.size - system->memory_pool.used;
    
    // Calculate fragmentation
    usize free_bytes = 0;
    usize free_blocks = 0;
    FreeBlock* block = system->memory_pool.free_list;
    while (block) {
        free_bytes += block->size;
        free_blocks++;
        block = block->next;
    }
    
    if (free_bytes > 0) {
        *fragmentation = 1.0f - ((f32)free_bytes / (f32)*available);
    } else {
        *fragmentation = 0.0f;
    }
}

// === Core API ===

void streaming_init(StreamingSystem* system, usize memory_budget) {
    memset(system, 0, sizeof(StreamingSystem));
    
    system->memory_budget = memory_budget;
    system->thread_count = STREAMING_THREAD_COUNT;
    
    // Initialize memory pool
    system->memory_pool.base = (u8*)malloc(memory_budget);
    system->memory_pool.size = memory_budget;
    system->memory_pool.used = 0;
    system->memory_pool.free_list = NULL;
    
    // Initialize virtual texture cache
    system->vt_cache_memory = (u8*)malloc(VIRTUAL_TEXTURE_CACHE_SIZE);
    system->vt_page_pool = (VirtualTexturePage*)calloc(VT_CACHE_PAGES, 
                                                       sizeof(VirtualTexturePage));
    
    // Initialize compression buffer
    system->compress_buffer_size = MEGABYTES(16);
    system->compress_buffer = (u8*)malloc(system->compress_buffer_size);
    
    // Initialize locks
    pthread_mutex_init(&system->hash_lock, NULL);
    pthread_mutex_init(&system->defrag_lock, NULL);
    for (u32 i = 0; i < STREAM_PRIORITY_COUNT; i++) {
        pthread_mutex_init(&system->request_queue.locks[i], NULL);
    }
    
    // Initialize spatial index (world bounds: -10km to 10km)
    v3 world_min = {-10000.0f, -10000.0f, -10000.0f};
    v3 world_max = {10000.0f, 10000.0f, 10000.0f};
    system->spatial_root = spatial_node_create(world_min, world_max, 0);
    
    // Initialize defragmentation state
    system->defrag_state = calloc(1, sizeof(DefragState));
    
    // Configure default streaming rings
    system->streaming_rings[0] = (StreamingRing){0, 50, STREAM_PRIORITY_CRITICAL, 100};
    system->streaming_rings[1] = (StreamingRing){50, 150, STREAM_PRIORITY_HIGH, 200};
    system->streaming_rings[2] = (StreamingRing){150, 300, STREAM_PRIORITY_NORMAL, 400};
    system->streaming_rings[3] = (StreamingRing){300, 500, STREAM_PRIORITY_PREFETCH, 800};
    
    // Start worker threads
    for (u32 i = 0; i < system->thread_count; i++) {
        pthread_create(&system->streaming_threads[i], NULL, 
                      streaming_worker_thread, system);
    }
    
    // Start IO thread
    pthread_create(&system->io_thread, NULL, streaming_io_thread, system);
    
    // Start decompression threads
    for (u32 i = 0; i < 2; i++) {
        pthread_create(&system->decompress_threads[i], NULL,
                      streaming_decompress_thread, system);
    }
}

void streaming_shutdown(StreamingSystem* system) {
    // Signal threads to exit
    atomic_store(&system->should_exit, true);
    
    // Wait for threads
    for (u32 i = 0; i < system->thread_count; i++) {
        pthread_join(system->streaming_threads[i], NULL);
    }
    pthread_join(system->io_thread, NULL);
    for (u32 i = 0; i < 2; i++) {
        pthread_join(system->decompress_threads[i], NULL);
    }
    
    // Close file handles
    for (u32 i = 0; i < system->file_cache_count; i++) {
        close(system->file_cache[i].fd);
    }
    
    // Free resident assets
    for (u32 i = 0; i < system->resident_count; i++) {
        free(system->resident_assets[i]);
    }
    
    // Free virtual textures
    for (u32 i = 0; i < system->vt_count; i++) {
        VirtualTexture* vt = system->virtual_textures[i];
        for (u32 y = 0; y < vt->page_count_y; y++) {
            free(vt->pages[y]);
        }
        free(vt->pages);
        free(vt->indirection_data);
        free(vt);
    }
    
    // Free memory pools
    free(system->memory_pool.base);
    free(system->vt_cache_memory);
    free(system->vt_page_pool);
    free(system->compress_buffer);
    
    // Free spatial index
    if (system->spatial_root) {
        // Would need recursive free of octree nodes
        // For now just free root
        SpatialNode* root = (SpatialNode*)system->spatial_root;
        if (root->asset_ids) free(root->asset_ids);
        free(root);
    }
    
    // Free defrag state
    if (system->defrag_state) {
        free(system->defrag_state);
    }
    
    // Destroy locks
    pthread_mutex_destroy(&system->hash_lock);
    pthread_mutex_destroy(&system->defrag_lock);
    for (u32 i = 0; i < STREAM_PRIORITY_COUNT; i++) {
        pthread_mutex_destroy(&system->request_queue.locks[i]);
    }
}

void streaming_update(StreamingSystem* system, v3 camera_pos, v3 camera_vel, f32 dt) {
    system->current_frame++;
    
    // Update camera prediction
    v3 zero_accel = {0, 0, 0};
    streaming_update_camera_prediction(system, camera_pos, camera_vel, zero_accel);
    
    // Process streaming rings for predictive loading
    for (u32 ring_idx = 0; ring_idx < STREAMING_RING_SIZE; ring_idx++) {
        StreamingRing* ring = &system->streaming_rings[ring_idx];
        if (ring->outer_radius == 0) continue;
        
        // Query spatial index for assets in ring
        u64 ring_assets[512];
        u32 ring_asset_count = 0;
        
        if (system->spatial_root) {
            spatial_node_query_radius((SpatialNode*)system->spatial_root, 
                                     camera_pos, ring->outer_radius,
                                     ring_assets, &ring_asset_count, 512);
        }
        
        // Process assets in this ring
        for (u32 i = 0; i < ring_asset_count && i < ring->max_assets; i++) {
            // Calculate distance (simplified - would use actual asset position)
            f32 distance = ring->inner_radius + 
                          (ring->outer_radius - ring->inner_radius) * 0.5f;
            
            // Calculate appropriate LOD
            u32 lod = streaming_calculate_lod(distance, 10.0f, 1.57f);
            
            // Queue streaming request if not resident
            if (!streaming_is_resident(system, ring_assets[i], lod)) {
                streaming_request_asset(system, ring_assets[i], ring->priority, lod);
            }
        }
    }
    
    // Check memory pressure and trigger defrag if needed
    usize used, available;
    f32 fragmentation;
    streaming_get_memory_stats(system, &used, &available, &fragmentation);
    
    if (fragmentation > 0.3f && available < MEGABYTES(256)) {
        // High fragmentation and low memory - defragment
        streaming_defragment(system);
    }
    
    // Update statistics
    if (atomic_load(&system->stats.completed_requests) > 0) {
        // Track streaming performance
        usize total_reqs = atomic_load(&system->stats.total_requests);
        usize completed = atomic_load(&system->stats.completed_requests);
        // usize failed = atomic_load(&system->stats.failed_requests);
        
        if (total_reqs > 0) {
            f32 success_rate = (f32)completed / (f32)total_reqs;
            f32 cache_hit_rate = (f32)atomic_load(&system->stats.cache_hits) / 
                                (f32)(atomic_load(&system->stats.cache_hits) + 
                                      atomic_load(&system->stats.cache_misses) + 1);
            
            // Log performance issues
            if (success_rate < 0.95f) {
                printf("Streaming: Low success rate %.1f%%\n", success_rate * 100.0f);
            }
            if (cache_hit_rate < 0.7f) {
                printf("Streaming: Low cache hit rate %.1f%%\n", cache_hit_rate * 100.0f);
            }
        }
    }
    
    // Periodic maintenance
    if (system->current_frame % 600 == 0) {  // Every 10 seconds at 60 FPS
        // Close old file handles
        for (u32 i = 0; i < system->file_cache_count; i++) {
            if (system->current_frame - system->file_cache[i].last_access > 3600) {
                close(system->file_cache[i].fd);
                // Remove from cache
                system->file_cache[i] = system->file_cache[--system->file_cache_count];
                i--;
            }
        }
    }
}

StreamRequest* streaming_request_asset(StreamingSystem* system, u64 asset_id,
                                       StreamPriority priority, u32 lod_level) {
    // Get request from pool
    u32 index = atomic_fetch_add(&system->request_pool_index, 1) % MAX_STREAMING_REQUESTS;
    StreamRequest* request = &system->request_pool[index];
    
    // Initialize request
    request->asset_id = asset_id;
    request->type = STREAM_TYPE_TEXTURE;  // TODO: Determine from asset
    request->priority = priority;
    request->lod_level = lod_level;
    request->request_frame = system->current_frame;
    atomic_store(&request->status, 0);  // Pending
    request->callback = NULL;
    request->callback_data = NULL;
    
    // Add to queue
    add_request(&system->request_queue, request);
    atomic_fetch_add(&system->stats.total_requests, 1);
    
    return request;
}

bool streaming_is_resident(StreamingSystem* system, u64 asset_id, u32 lod_level) {
    ResidentAsset* asset = streaming_find_resident(system, asset_id);
    return asset && asset->current_lod >= lod_level;
}

void* streaming_get_asset_data(StreamingSystem* system, u64 asset_id, u32 lod_level) {
    ResidentAsset* asset = streaming_find_resident(system, asset_id);
    if (!asset) return NULL;
    
    streaming_lru_touch(system, asset);
    
    if (asset->current_lod >= lod_level) {
        return asset->lod_data[lod_level] ? asset->lod_data[lod_level] : asset->data;
    }
    
    return NULL;
}

void streaming_lock_asset(StreamingSystem* system, u64 asset_id) {
    ResidentAsset* asset = streaming_find_resident(system, asset_id);
    if (asset) {
        atomic_fetch_add(&asset->ref_count, 1);
    }
}

void streaming_unlock_asset(StreamingSystem* system, u64 asset_id) {
    ResidentAsset* asset = streaming_find_resident(system, asset_id);
    if (asset && atomic_load(&asset->ref_count) > 0) {
        atomic_fetch_sub(&asset->ref_count, 1);
    }
}

// === Statistics ===

StreamingStats streaming_get_stats(StreamingSystem* system) {
    StreamingStats stats = system->stats;
    stats.current_memory_usage = system->memory_pool.used;
    stats.peak_memory_usage = atomic_load(&system->memory_pool.peak_usage);
    return stats;
}

void streaming_reset_stats(StreamingSystem* system) {
    memset(&system->stats, 0, sizeof(StreamingStats));
}

// === Debug ===

void streaming_debug_draw(StreamingSystem* system, void* renderer) {
    // TODO: Implement debug visualization
    // - Show streaming rings
    // - Show loaded assets
    // - Show memory usage
    // - Show request queue status
}

void streaming_dump_state(StreamingSystem* system, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "=== Streaming System State ===\n");
    fprintf(f, "Frame: %lu\n", system->current_frame);
    fprintf(f, "Memory: %zu / %zu bytes\n", system->memory_pool.used, system->memory_budget);
    fprintf(f, "Resident Assets: %u\n", system->resident_count);
    fprintf(f, "Virtual Textures: %u\n", system->vt_count);
    
    fprintf(f, "\n=== Statistics ===\n");
    fprintf(f, "Total Requests: %u\n", atomic_load(&system->stats.total_requests));
    fprintf(f, "Completed: %u\n", atomic_load(&system->stats.completed_requests));
    fprintf(f, "Failed: %u\n", atomic_load(&system->stats.failed_requests));
    fprintf(f, "Cache Hits: %u\n", atomic_load(&system->stats.cache_hits));
    fprintf(f, "Cache Misses: %u\n", atomic_load(&system->stats.cache_misses));
    fprintf(f, "Bytes Loaded: %u\n", atomic_load(&system->stats.bytes_loaded));
    fprintf(f, "Bytes Evicted: %u\n", atomic_load(&system->stats.bytes_evicted));
    
    fprintf(f, "\n=== Request Queue ===\n");
    for (u32 i = 0; i < STREAM_PRIORITY_COUNT; i++) {
        fprintf(f, "Priority %u: %u requests\n", i, 
                atomic_load(&system->request_queue.counts[i]));
    }
    
    fprintf(f, "\n=== Resident Assets ===\n");
    for (u32 i = 0; i < system->resident_count; i++) {
        ResidentAsset* asset = system->resident_assets[i];
        fprintf(f, "Asset %016lx: LOD %u, Size %zu, RefCount %d, LastAccess %lu\n",
                asset->asset_id, asset->current_lod, asset->size,
                atomic_load(&asset->ref_count), asset->last_access_frame);
    }
    
    fclose(f);
}