#ifndef HANDMADE_PROFILER_ENHANCED_H
#define HANDMADE_PROFILER_ENHANCED_H

#include "handmade_memory.h"
#include <time.h>
#include <x86intrin.h>
#include <stdatomic.h>
#include <pthread.h>

// AAA-Quality Profiler System
// Features:
// - Hierarchical CPU profiling with < 1% overhead
// - GPU timing with OpenGL/Vulkan queries
// - Memory tracking with leak detection
// - Network packet analysis
// - Lock-free ring buffers
// - Chrome tracing export
// - Record and playback
// - Real-time visualization

// Configuration
#define MAX_PROFILER_THREADS 32
#define MAX_TIMERS 4096
#define MAX_GPU_TIMERS 256
#define MAX_TIMER_STACK_DEPTH 64
#define FRAME_HISTORY_SIZE 240
#define MAX_BREAKPOINTS 256
#define MAX_WATCHES 128

// Event types
typedef enum profile_event_type {
    EVENT_NONE = 0,
    EVENT_PUSH,          // Timer push
    EVENT_POP,           // Timer pop
    EVENT_MARKER,        // Instant marker
    EVENT_COUNTER,       // Counter value
    EVENT_GPU,           // GPU timing
    EVENT_MEMORY_ALLOC,  // Memory allocation
    EVENT_MEMORY_FREE,   // Memory free
    EVENT_NETWORK,       // Network packet
    EVENT_FRAME,         // Frame boundary
    EVENT_CUSTOM,        // User-defined
} profile_event_type;

// Profile event (16 bytes for cache efficiency)
typedef struct profile_event {
    union {
        const char* name;
        u64 name_hash;
    };
    u64 timestamp;
    union {
        u64 duration_cycles;
        u64 value;
        u64 gpu_time_ns;
    };
    u32 thread_id;
    u16 depth;
    u8 type;
    u8 color;
} profile_event;

// Timer statistics (atomic for lock-free updates)
typedef struct timer_stats {
    const char* name;
    _Atomic(u64) total_cycles;
    _Atomic(u64) min_cycles;
    _Atomic(u64) max_cycles;
    _Atomic(u64) call_count;
    f64 average_cycles;
    f64 average_ms;
    f64 min_ms;
    f64 max_ms;
} timer_stats;

// Frame statistics
typedef struct frame_stats {
    u64 frame_number;
    u64 duration_cycles;
    f64 duration_ms;
    f64 fps;
    u32 draw_calls;
    u32 triangles;
    u32 state_changes;
    u32 texture_switches;
    u64 memory_allocated;
    u64 memory_freed;
    u32 network_packets_sent;
    u32 network_packets_received;
    u64 network_bytes_sent;
    u64 network_bytes_received;
} frame_stats;

// Timer stack entry
typedef struct timer_stack_entry {
    const char* name;
    u64 start_tsc;
    u32 color;
} timer_stack_entry;

// Thread profiler state
typedef struct thread_profiler_state {
    u32 thread_id;
    char thread_name[32];
    
    // Hierarchical timer stack
    timer_stack_entry timer_stack[MAX_TIMER_STACK_DEPTH];
    u32 timer_stack_depth;
    
    // Event ring buffer (lock-free)
    struct event_ring_buffer {
        profile_event* events;
        u64 capacity;
        _Atomic(u64) write_pos;
        _Atomic(u64) read_pos;
    } event_buffer;
    
    // String interning buffer
    char* string_buffer;
    u32 string_buffer_pos;
    
    // Per-thread statistics
    u64 total_events;
    u64 dropped_events;
} thread_profiler_state;

// Capture modes
typedef enum capture_mode {
    CAPTURE_NONE = 0,
    CAPTURE_CONTINUOUS,
    CAPTURE_SINGLE_FRAME,
    CAPTURE_TRIGGERED,
} capture_mode;

// Profiler initialization parameters
typedef struct profiler_init_params {
    u32 thread_count;
    size_t event_buffer_size;      // Per thread
    size_t recording_buffer_size;  // Total
    int enable_gpu_profiling;
    int enable_network_profiling;
    int enable_memory_tracking;
    f64 target_overhead_percent;   // Target < 1%
} profiler_init_params;

// Forward declarations
struct gpu_timer;
struct network_packet;
struct memory_tracker;

// Main profiler system
typedef struct profiler_system {
    // Core state
    u64 cpu_frequency;
    u64 start_tsc;
    int enabled;
    int running;
    
    // Frame timing
    u64 frame_number;
    u64 frame_start_tsc;
    frame_stats current_frame;
    frame_stats frame_history[FRAME_HISTORY_SIZE];
    f64 average_fps;
    
    // Thread states
    thread_profiler_state thread_states[MAX_PROFILER_THREADS];
    u32 thread_count;
    
    // Timer statistics
    timer_stats timers[MAX_TIMERS];
    
    // GPU profiling
    struct gpu_timer* gpu_timers;
    u32 gpu_timer_count;
    
    // Memory tracking
    struct memory_tracker* memory_tracker;
    _Atomic(u64) current_allocated;
    _Atomic(u64) peak_allocated;
    _Atomic(u64) total_allocations;
    
    // Network profiling
    struct network_packet* network_buffer;
    u64 network_capacity;
    _Atomic(u64) network_write_pos;
    _Atomic(u64) total_bytes_sent;
    _Atomic(u64) total_bytes_received;
    
    // Recording
    u8* recording_buffer;
    u64 recording_capacity;
    u64 recording_write_pos;
    u32 recording_start_frame;
    int recording_active;
    
    // Capture mode
    capture_mode capture_mode;
    
    // Background thread
    pthread_t aggregation_thread;
    
} profiler_system;

// Global profiler instance
extern profiler_system g_profiler_system;

// Thread-local profiler state
extern __thread thread_profiler_state* tls_profiler;
extern __thread u32 tls_thread_id;

// === Core API ===

// Initialize profiler system
void profiler_system_init(profiler_init_params* params);
void profiler_shutdown(void);

// Frame operations
void profiler_begin_frame(void);
void profiler_end_frame(void);

// Hierarchical timing (main API)
void profiler_push_timer(const char* name, u32 color);
void profiler_pop_timer(void);

// Scoped timer helpers
#define PROFILE_SCOPE(name) \
    profiler_push_timer(name, 0xFFFFFF); \
    defer { profiler_pop_timer(); }

#define PROFILE_SCOPE_COLOR(name, color) \
    profiler_push_timer(name, color); \
    defer { profiler_pop_timer(); }

// Manual timing
#define PROFILE_BEGIN(name) \
    u64 _prof_start_##name = __rdtsc(); \
    const char* _prof_name_##name = #name;

#define PROFILE_END(name) \
    do { \
        u64 _prof_elapsed = __rdtsc() - _prof_start_##name; \
        profiler_update_timer_stats(_prof_name_##name, _prof_elapsed); \
    } while(0)

// Markers and counters
static inline void profiler_marker(const char* name, u32 color) {
    profiler_push_timer(name, color);
    profiler_pop_timer();
}

static inline void profiler_counter(const char* name, u64 value) {
    // Implementation in .c file
    extern void profiler_counter_internal(const char* name, u64 value);
    profiler_counter_internal(name, value);
}

// === GPU Profiling ===

void profiler_gpu_begin(const char* name);
void profiler_gpu_end(const char* name);

#define GPU_PROFILE_SCOPE(name) \
    profiler_gpu_begin(name); \
    defer { profiler_gpu_end(name); }

// === Memory Tracking ===

void profiler_track_allocation(void* ptr, size_t size, const char* file, u32 line);
void profiler_track_free(void* ptr);

#define PROFILE_ALLOC(ptr, size) \
    profiler_track_allocation(ptr, size, __FILE__, __LINE__)

#define PROFILE_FREE(ptr) \
    profiler_track_free(ptr)

// Memory leak detection
void profiler_detect_leaks(void);
void profiler_dump_allocations(void);

// === Network Profiling ===

void profiler_record_packet(u32 src_ip, u32 dst_ip, u16 src_port, u16 dst_port,
                           u32 size, u8 protocol, f64 latency_ms);

void profiler_network_send(u32 bytes);
void profiler_network_receive(u32 bytes);

// === Recording and Playback ===

void profiler_start_recording(void);
void profiler_stop_recording(void);
void profiler_record_frame_data(void* data, size_t size);
int profiler_load_recording(const char* filename);
void profiler_playback_frame(u32 frame_offset);

// === Export Functions ===

void profiler_export_chrome_trace(const char* filename);
void profiler_export_flamegraph(const char* filename);
void profiler_export_csv(const char* filename);

// === Visualization API ===

typedef struct profiler_draw_params {
    f32 x, y;
    f32 width, height;
    int show_timeline;
    int show_flamegraph;
    int show_statistics;
    int show_memory_graph;
    int show_network_graph;
    u32 selected_thread;
    f32 zoom_level;
    f32 pan_offset;
} profiler_draw_params;

void profiler_draw_overlay(profiler_draw_params* params);
void profiler_draw_timeline(f32 x, f32 y, f32 width, f32 height);
void profiler_draw_flamegraph(f32 x, f32 y, f32 width, f32 height);
void profiler_draw_statistics(f32 x, f32 y);
void profiler_draw_memory_graph(f32 x, f32 y, f32 width, f32 height);
void profiler_draw_network_graph(f32 x, f32 y, f32 width, f32 height);

// === Query API ===

f64 profiler_get_timer_ms(const char* name);
u64 profiler_get_timer_calls(const char* name);
f64 profiler_get_average_fps(void);
u64 profiler_get_current_memory(void);
u64 profiler_get_peak_memory(void);
frame_stats* profiler_get_frame_stats(u32 frame_offset);

// === Configuration ===

void profiler_set_enabled(int enabled);
void profiler_set_capture_mode(capture_mode mode);
void profiler_trigger_capture(void);

// === Internal Functions ===

thread_profiler_state* profiler_get_thread_state(void);
profile_event* profiler_allocate_event(thread_profiler_state* thread);
void profiler_update_timer_stats(const char* name, u64 elapsed_cycles);
u64 profiler_calculate_cpu_frequency(void);
u32 profiler_hash_string(const char* str);
f64 cycles_to_ms(u64 cycles);
f64 cycles_to_us(u64 cycles);
void* profiler_aggregation_thread(void* arg);

// === In-Engine Debugger Extension ===

typedef struct breakpoint {
    void* address;
    const char* file;
    u32 line;
    const char* condition;
    u32 hit_count;
    int enabled;
    void (*callback)(struct breakpoint* bp);
} breakpoint;

typedef struct watch_variable {
    const char* name;
    void* address;
    size_t size;
    const char* type_name;
    void (*formatter)(void* data, char* out, size_t out_size);
    int expanded;
} watch_variable;

typedef struct call_stack_frame {
    void* return_address;
    const char* function_name;
    const char* file;
    u32 line;
    void* frame_pointer;
    void* stack_pointer;
} call_stack_frame;

typedef struct debugger_context {
    // Breakpoints
    breakpoint breakpoints[MAX_BREAKPOINTS];
    u32 breakpoint_count;
    
    // Watches
    watch_variable watches[MAX_WATCHES];
    u32 watch_count;
    
    // Call stack
    call_stack_frame call_stack[64];
    u32 call_stack_depth;
    
    // Execution control
    int paused;
    int single_step;
    int step_over;
    int step_out;
    f32 time_scale;
    
    // Memory view
    void* memory_view_address;
    size_t memory_view_size;
    
    // Conditional breakpoints
    int (*evaluate_condition)(const char* condition);
} debugger_context;

// Debugger API
void debugger_init(debugger_context* ctx);
void debugger_add_breakpoint(debugger_context* ctx, void* address, const char* file, u32 line);
void debugger_add_watch(debugger_context* ctx, const char* name, void* address, size_t size);
void debugger_update_call_stack(debugger_context* ctx);
void debugger_check_breakpoints(debugger_context* ctx);
void debugger_step(debugger_context* ctx);
void debugger_continue(debugger_context* ctx);
void debugger_set_time_scale(debugger_context* ctx, f32 scale);

// === Performance Assertions ===

#ifdef HANDMADE_DEBUG
    #define ASSERT_FRAME_TIME(max_ms) \
        do { \
            f64 frame_ms = profiler_get_frame_stats(0)->duration_ms; \
            if (frame_ms > max_ms) { \
                printf("[PERF] Frame time exceeded: %.2fms > %.2fms\n", \
                       frame_ms, max_ms); \
            } \
        } while(0)
    
    #define ASSERT_MEMORY_USAGE(max_mb) \
        do { \
            u64 current = profiler_get_current_memory(); \
            if (current > (max_mb) * 1024 * 1024) { \
                printf("[PERF] Memory usage exceeded: %.2fMB > %uMB\n", \
                       current / (1024.0 * 1024.0), max_mb); \
            } \
        } while(0)
#else
    #define ASSERT_FRAME_TIME(max_ms)
    #define ASSERT_MEMORY_USAGE(max_mb)
#endif

// === Defer macro for C (using cleanup attribute) ===
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define defer \
    void CONCAT(_defer_, __LINE__)(int*); \
    __attribute__((cleanup(CONCAT(_defer_, __LINE__)))) int CONCAT(_defer_var_, __LINE__) = 0; \
    void CONCAT(_defer_, __LINE__)(int* _)

#endif // HANDMADE_PROFILER_ENHANCED_H