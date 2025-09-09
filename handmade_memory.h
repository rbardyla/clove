#ifndef HANDMADE_MEMORY_H
#define HANDMADE_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

#define KILOBYTES(n) ((n) * 1024ULL)
#define MEGABYTES(n) ((n) * 1024ULL * 1024ULL)
#define GIGABYTES(n) ((n) * 1024ULL * 1024ULL * 1024ULL)

#define ARENA_ALIGNMENT 16
#define ARENA_MAGIC 0xDEADBEEF12345678ULL
#define TEMP_MARK_MAGIC 0xFEEDFACE87654321ULL

// Memory statistics for profiling
typedef struct memory_stats {
    u64 total_allocated;
    u64 total_freed;
    u64 current_usage;
    u64 peak_usage;
    u32 allocation_count;
    u32 arena_count;
    f64 fragmentation_ratio;
} memory_stats;

// Arena allocator - zero malloc/free in hot paths
typedef struct arena {
    u8* base;           // Base memory pointer
    u64 size;           // Total size
    u64 used;           // Current usage
    u64 temp_count;     // Number of temp marks
    u64 magic;          // Corruption detection
    struct arena* next; // For chaining arenas
    memory_stats stats; // Per-arena statistics
} arena;

// Temporary memory mark for scoped allocations
typedef struct temp_memory {
    arena* arena;
    u64 used;
    u64 magic;
} temp_memory;

// Thread-local scratch arena for temporary allocations
typedef struct scratch_arena {
    arena* arena;
    u32 thread_id;
    u32 conflict_count;
} scratch_arena;

// Global memory system state
typedef struct memory_system {
    arena* permanent_arena;     // Never freed
    arena* frame_arena;         // Reset each frame
    arena* level_arena;         // Reset on level change
    scratch_arena* scratches;   // Thread-local scratch arenas
    u32 scratch_count;
    memory_stats global_stats;
    u64 frame_number;
} memory_system;

// Initialize memory system with pre-allocated backing
static inline memory_system memory_system_init(void* backing_buffer, u64 backing_size) {
    memory_system sys = {0};
    
    // Divide backing buffer into regions
    u64 permanent_size = backing_size / 4;  // 25%
    u64 frame_size = backing_size / 4;      // 25%
    u64 level_size = backing_size / 4;      // 25%
    u64 scratch_size = backing_size / 4;    // 25%
    
    u8* current = (u8*)backing_buffer;
    
    // Initialize permanent arena
    sys.permanent_arena = (arena*)current;
    sys.permanent_arena->base = current + sizeof(arena);
    sys.permanent_arena->size = permanent_size - sizeof(arena);
    sys.permanent_arena->magic = ARENA_MAGIC;
    current += permanent_size;
    
    // Initialize frame arena
    sys.frame_arena = (arena*)current;
    sys.frame_arena->base = current + sizeof(arena);
    sys.frame_arena->size = frame_size - sizeof(arena);
    sys.frame_arena->magic = ARENA_MAGIC;
    current += frame_size;
    
    // Initialize level arena
    sys.level_arena = (arena*)current;
    sys.level_arena->base = current + sizeof(arena);
    sys.level_arena->size = level_size - sizeof(arena);
    sys.level_arena->magic = ARENA_MAGIC;
    current += level_size;
    
    // Initialize scratch arenas (assume 4 threads)
    sys.scratch_count = 4;
    sys.scratches = (scratch_arena*)(current);
    current += sizeof(scratch_arena) * sys.scratch_count;
    
    u64 scratch_arena_size = (scratch_size - sizeof(scratch_arena) * sys.scratch_count) / sys.scratch_count;
    
    for (u32 i = 0; i < sys.scratch_count; i++) {
        sys.scratches[i].arena = (arena*)current;
        sys.scratches[i].arena->base = current + sizeof(arena);
        sys.scratches[i].arena->size = scratch_arena_size - sizeof(arena);
        sys.scratches[i].arena->magic = ARENA_MAGIC;
        sys.scratches[i].thread_id = i;
        current += scratch_arena_size;
    }
    
    return sys;
}

// Alignment helper
static inline u64 align_forward(u64 addr, u64 align) {
    u64 modulo = addr & (align - 1);
    if (modulo) {
        addr += align - modulo;
    }
    return addr;
}

// Core arena allocation
static inline void* arena_alloc(arena* a, u64 size) {
    assert(a->magic == ARENA_MAGIC);
    
    // Align allocation
    u64 aligned_used = align_forward(a->used, ARENA_ALIGNMENT);
    u64 aligned_size = align_forward(size, ARENA_ALIGNMENT);
    
    if (aligned_used + aligned_size > a->size) {
        // Arena exhausted - in production, chain to next arena
        assert(0 && "Arena exhausted!");
        return NULL;
    }
    
    void* result = a->base + aligned_used;
    a->used = aligned_used + aligned_size;
    
    // Update statistics
    a->stats.total_allocated += aligned_size;
    a->stats.current_usage = a->used;
    if (a->stats.current_usage > a->stats.peak_usage) {
        a->stats.peak_usage = a->stats.current_usage;
    }
    a->stats.allocation_count++;
    
    // Clear memory for safety
    memset(result, 0, size);
    
    return result;
}

// Allocate aligned memory
static inline void* arena_alloc_aligned(arena* a, u64 size, u64 align) {
    assert(a->magic == ARENA_MAGIC);
    assert((align & (align - 1)) == 0); // Power of 2
    
    u64 aligned_used = align_forward(a->used, align);
    u64 aligned_size = align_forward(size, align);
    
    if (aligned_used + aligned_size > a->size) {
        assert(0 && "Arena exhausted!");
        return NULL;
    }
    
    void* result = a->base + aligned_used;
    a->used = aligned_used + aligned_size;
    
    a->stats.total_allocated += aligned_size;
    a->stats.current_usage = a->used;
    if (a->stats.current_usage > a->stats.peak_usage) {
        a->stats.peak_usage = a->stats.current_usage;
    }
    
    memset(result, 0, size);
    return result;
}

// Array allocation with count
#define arena_alloc_array(arena, type, count) \
    (type*)arena_alloc(arena, sizeof(type) * (count))

// Aligned array allocation
#define arena_alloc_array_aligned(arena, type, count, align) \
    (type*)arena_alloc_aligned(arena, sizeof(type) * (count), align)

// Reset arena (doesn't free memory, just resets usage)
static inline void arena_reset(arena* a) {
    assert(a->magic == ARENA_MAGIC);
    a->used = 0;
    a->temp_count = 0;
    a->stats.current_usage = 0;
}

// Begin temporary memory scope
static inline temp_memory temp_memory_begin(arena* a) {
    temp_memory temp = {
        .arena = a,
        .used = a->used,
        .magic = TEMP_MARK_MAGIC
    };
    a->temp_count++;
    return temp;
}

// End temporary memory scope (rollback)
static inline void temp_memory_end(temp_memory temp) {
    assert(temp.magic == TEMP_MARK_MAGIC);
    assert(temp.arena->magic == ARENA_MAGIC);
    assert(temp.arena->temp_count > 0);
    
    temp.arena->used = temp.used;
    temp.arena->temp_count--;
    temp.arena->stats.current_usage = temp.used;
}

// Get thread-local scratch arena
static inline scratch_arena* get_scratch_arena(memory_system* sys) {
    // Simple thread ID simulation - in production use pthread_self() or similar
    u32 thread_id = 0; // Would be: pthread_self() % sys->scratch_count
    return &sys->scratches[thread_id];
}

// Scratch allocation pattern
#define SCRATCH_BLOCK(sys) \
    for (scratch_arena* scratch = get_scratch_arena(sys); scratch; scratch = NULL) \
        for (temp_memory scratch_temp = temp_memory_begin(scratch->arena); \
             scratch_temp.arena; \
             temp_memory_end(scratch_temp), scratch_temp.arena = NULL)

// Frame boundary operations
static inline void memory_frame_begin(memory_system* sys) {
    sys->frame_number++;
    arena_reset(sys->frame_arena);
}

static inline void memory_frame_end(memory_system* sys) {
    // Update global statistics
    sys->global_stats.current_usage = 
        sys->permanent_arena->used + 
        sys->frame_arena->used + 
        sys->level_arena->used;
    
    if (sys->global_stats.current_usage > sys->global_stats.peak_usage) {
        sys->global_stats.peak_usage = sys->global_stats.current_usage;
    }
    
    // Calculate fragmentation
    u64 total_size = sys->permanent_arena->size + sys->frame_arena->size + sys->level_arena->size;
    sys->global_stats.fragmentation_ratio = 
        1.0 - ((f64)sys->global_stats.current_usage / (f64)total_size);
}

// Level change operations
static inline void memory_level_begin(memory_system* sys) {
    arena_reset(sys->level_arena);
}

// Debug helpers
#ifdef HANDMADE_DEBUG
    #define DEBUG_ARENA_CHECK(a) assert((a)->magic == ARENA_MAGIC)
    #define DEBUG_TEMP_CHECK(t) assert((t).magic == TEMP_MARK_MAGIC)
#else
    #define DEBUG_ARENA_CHECK(a)
    #define DEBUG_TEMP_CHECK(t)
#endif

// Memory statistics reporting
static inline void memory_print_stats(memory_system* sys) {
    printf("=== Memory Statistics ===\n");
    printf("Frame: %llu\n", sys->frame_number);
    printf("Current Usage: %.2f MB\n", sys->global_stats.current_usage / (1024.0 * 1024.0));
    printf("Peak Usage: %.2f MB\n", sys->global_stats.peak_usage / (1024.0 * 1024.0));
    printf("Fragmentation: %.1f%%\n", sys->global_stats.fragmentation_ratio * 100.0);
    printf("\nArena Usage:\n");
    printf("  Permanent: %.2f MB / %.2f MB\n", 
           sys->permanent_arena->used / (1024.0 * 1024.0),
           sys->permanent_arena->size / (1024.0 * 1024.0));
    printf("  Frame: %.2f MB / %.2f MB\n",
           sys->frame_arena->used / (1024.0 * 1024.0),
           sys->frame_arena->size / (1024.0 * 1024.0));
    printf("  Level: %.2f MB / %.2f MB\n",
           sys->level_arena->used / (1024.0 * 1024.0),
           sys->level_arena->size / (1024.0 * 1024.0));
}

// Pool allocator for fixed-size objects
typedef struct pool_allocator {
    void* memory;
    u64 block_size;
    u64 block_count;
    u64* free_list;
    u32 free_count;
    u32 allocated_count;
} pool_allocator;

static inline pool_allocator pool_init(arena* a, u64 block_size, u64 block_count) {
    pool_allocator pool = {0};
    pool.block_size = align_forward(block_size, ARENA_ALIGNMENT);
    pool.block_count = block_count;
    
    // Allocate memory for blocks
    pool.memory = arena_alloc(a, pool.block_size * block_count);
    
    // Allocate free list
    pool.free_list = arena_alloc_array(a, u64, block_count);
    pool.free_count = block_count;
    
    // Initialize free list
    for (u64 i = 0; i < block_count; i++) {
        pool.free_list[i] = i;
    }
    
    return pool;
}

static inline void* pool_alloc(pool_allocator* pool) {
    if (pool->free_count == 0) {
        return NULL;
    }
    
    u64 index = pool->free_list[--pool->free_count];
    pool->allocated_count++;
    
    void* result = (u8*)pool->memory + (index * pool->block_size);
    memset(result, 0, pool->block_size);
    return result;
}

static inline void pool_free(pool_allocator* pool, void* ptr) {
    u64 offset = (u8*)ptr - (u8*)pool->memory;
    u64 index = offset / pool->block_size;
    
    assert(index < pool->block_count);
    assert(pool->free_count < pool->block_count);
    
    pool->free_list[pool->free_count++] = index;
    pool->allocated_count--;
}

#endif // HANDMADE_MEMORY_H