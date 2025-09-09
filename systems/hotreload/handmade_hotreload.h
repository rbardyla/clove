#ifndef HANDMADE_HOTRELOAD_H
#define HANDMADE_HOTRELOAD_H

#include <stdint.h>
#include <stdbool.h>

// PERFORMANCE: Platform/Game API boundary - designed for cache efficiency
// MEMORY: All pointers use fixed base addresses for reload stability

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    float r, g, b, a;
} Color;

// MEMORY: Input state - 64 bytes aligned (1 cache line)
typedef struct {
    // Keyboard state
    uint64_t keys_down;      // Bitmask for first 64 keys
    uint64_t keys_pressed;    // Keys pressed this frame
    
    // Mouse state
    Vec2 mouse_pos;
    Vec2 mouse_delta;
    uint32_t mouse_buttons;   // Button state bitmask
    float mouse_wheel;
    
    // Timing
    float dt;                 // Delta time in seconds
    float time;              // Total time since start
    
    uint8_t _padding[8];     // Align to 64 bytes
} GameInput;

// MEMORY: Renderer commands - struct-of-arrays for SIMD
typedef struct {
    // Command buffer - fixed size, no allocations
    #define MAX_RENDER_COMMANDS 65536
    
    // Arrays for SIMD processing (32-byte aligned for AVX)
    Vec2* positions;         // Entity positions
    Vec2* sizes;            // Entity sizes  
    Color* colors;          // Entity colors
    uint32_t* texture_ids;  // Texture indices
    Vec2* tex_coords;       // Texture coordinates
    
    uint32_t command_count;
    uint32_t vertex_count;
    
    // Text rendering
    char* text_buffer;      // Fixed 1MB text buffer
    uint32_t text_offset;   // Current offset in buffer
} RenderCommands;

// MEMORY: Platform-provided memory blocks
typedef struct {
    // Fixed memory regions - addresses stable across reloads
    void* permanent_storage;      // 256MB - Never cleared
    uint64_t permanent_size;
    
    void* transient_storage;      // 128MB - Cleared each frame
    uint64_t transient_size;
    
    void* debug_storage;          // 16MB - Debug/profiling data
    uint64_t debug_size;
    
    // Arena allocators for game use
    uint64_t permanent_used;
    uint64_t transient_used;
    uint64_t debug_used;
} GameMemory;

// PERFORMANCE: Platform services - function pointers for indirection
typedef struct {
    // File I/O (memory mapped)
    void* (*load_file)(const char* path, uint64_t* size);
    void (*free_file)(void* data);
    
    // Debug output
    void (*debug_print)(const char* fmt, ...);
    
    // Performance counters
    uint64_t (*get_cycles)(void);
    uint64_t (*get_wall_clock)(void);
    
    // Thread pool (optional)
    void (*queue_work)(void (*work_proc)(void*), void* data);
    void (*complete_all_work)(void);
} PlatformAPI;

// MEMORY: Game state header - persisted across reloads
typedef struct {
    uint32_t version;           // State version for migrations
    uint32_t magic;            // 0xDEADBEEF for validation
    uint64_t frame_count;      // Total frames processed
    uint64_t reload_count;     // Number of hot reloads
    
    // Game-specific state follows...
} GameStateHeader;

// ============================================================================
// GAME EXPORTS - These functions must be implemented by game code
// ============================================================================

// Called once when game library first loads
typedef void (*GameInitialize_t)(GameMemory* memory, PlatformAPI* platform);

// Called before hot reload to prepare state
typedef void (*GamePrepareReload_t)(GameMemory* memory);

// Called after hot reload to restore state  
typedef void (*GameCompleteReload_t)(GameMemory* memory);

// Main update and render - called every frame
typedef void (*GameUpdateAndRender_t)(GameMemory* memory, 
                                      GameInput* input,
                                      RenderCommands* commands);

// Required export signature
#ifdef GAME_DLL
    #define GAME_EXPORT __attribute__((visibility("default")))
#else
    #define GAME_EXPORT
#endif

// ============================================================================
// HOT RELOAD SYSTEM
// ============================================================================

typedef struct {
    void* dll_handle;
    char* dll_path;
    char* temp_dll_path;
    uint64_t last_write_time;
    
    // Exported functions
    GameInitialize_t Initialize;
    GamePrepareReload_t PrepareReload;
    GameCompleteReload_t CompleteReload;
    GameUpdateAndRender_t UpdateAndRender;
    
    bool is_valid;
    bool needs_reload;
} GameCode;

typedef struct {
    int inotify_fd;
    int watch_descriptor;
    char* watched_path;
    
    // Double buffering for atomic swaps
    GameCode code_buffer[2];
    uint32_t current_buffer;
    
    // Reload statistics
    uint64_t reload_start_cycles;
    uint64_t last_reload_cycles;
    uint32_t reload_count;
    float average_reload_ms;
} HotReloadState;

// Hot reload functions
HotReloadState* hotreload_init(const char* game_dll_path);
bool hotreload_check_and_reload(HotReloadState* state);
void hotreload_shutdown(HotReloadState* state);

// ============================================================================
// INLINE UTILITIES
// ============================================================================

// MEMORY: Arena allocation from game memory
static inline void* game_push_size(GameMemory* memory, uint64_t size, bool permanent) {
    // PERFORMANCE: No branching in hot path
    uint64_t* used = permanent ? &memory->permanent_used : &memory->transient_used;
    void* base = permanent ? memory->permanent_storage : memory->transient_storage;
    uint64_t max = permanent ? memory->permanent_size : memory->transient_size;
    
    uint64_t aligned_size = (size + 63) & ~63; // 64-byte align
    
    if (*used + aligned_size > max) {
        return 0; // Out of memory
    }
    
    void* result = (uint8_t*)base + *used;
    *used += aligned_size;
    
    return result;
}

#define PUSH_STRUCT(memory, type, permanent) \
    (type*)game_push_size(memory, sizeof(type), permanent)

#define PUSH_ARRAY(memory, type, count, permanent) \
    (type*)game_push_size(memory, sizeof(type) * (count), permanent)

// PERFORMANCE: Cycle counting
static inline uint64_t read_cpu_timer(void) {
    uint32_t low, high;
    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | low;
}

#endif // HANDMADE_HOTRELOAD_H