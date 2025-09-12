#ifndef HANDMADE_PROFILER_H
#define HANDMADE_PROFILER_H

#include "handmade_memory.h"
#include <time.h>
#include <x86intrin.h>  // For RDTSC

// Profiler configuration
#define PROFILER_MAX_THREADS 16
#define PROFILER_MAX_TIMERS 256
#define PROFILER_MAX_COUNTERS 128
#define PROFILER_MAX_EVENTS 65536
#define PROFILER_MAX_FRAME_HISTORY 120

// Timer types
typedef enum profile_timer_type {
    TIMER_FRAME,
    TIMER_UPDATE,
    TIMER_RENDER,
    TIMER_PHYSICS,
    TIMER_AI,
    TIMER_AUDIO,
    TIMER_INPUT,
    TIMER_NETWORK,
    TIMER_MEMORY,
    TIMER_CUSTOM,
} profile_timer_type;

// Profile event for timeline
typedef struct profile_event {
    const char* name;
    u64 start_cycles;
    u64 end_cycles;
    u32 thread_id;
    u32 depth;
    u32 color;
} profile_event;

// Timer statistics
typedef struct profile_timer {
    const char* name;
    u64 total_cycles;
    u64 min_cycles;
    u64 max_cycles;
    u64 last_cycles;
    u32 call_count;
    u32 type;
    f64 total_ms;
    f64 average_ms;
    f64 min_ms;
    f64 max_ms;
} profile_timer;

// Performance counter
typedef struct profile_counter {
    const char* name;
    u64 value;
    u64 min_value;
    u64 max_value;
    u64 total_value;
    u32 sample_count;
} profile_counter;

// Frame statistics
typedef struct frame_stats {
    u64 frame_number;
    u64 start_cycles;
    u64 end_cycles;
    f64 frame_time_ms;
    f64 fps;
    u32 draw_calls;
    u32 triangles;
    u32 state_changes;
    u64 memory_allocated;
    u64 cache_misses;
} frame_stats;

// Thread profiler state
typedef struct thread_profiler {
    u32 thread_id;
    u32 timer_stack_depth;
    u64 timer_stack[32];  // Stack of timer start times
    const char* timer_names[32];
    
    profile_event* events;
    u32 event_count;
    u32 event_capacity;
} thread_profiler;

// Global profiler state
typedef struct profiler {
    // Timing
    u64 cpu_frequency;
    u64 start_time;
    u64 frame_start;
    
    // Timers
    profile_timer timers[PROFILER_MAX_TIMERS];
    u32 timer_count;
    
    // Counters
    profile_counter counters[PROFILER_MAX_COUNTERS];
    u32 counter_count;
    
    // Frame history
    frame_stats frame_history[PROFILER_MAX_FRAME_HISTORY];
    u32 frame_index;
    u64 total_frames;
    
    // Thread data
    thread_profiler thread_data[PROFILER_MAX_THREADS];
    u32 thread_count;
    
    // Memory tracking
    u64 allocation_count;
    u64 total_allocated;
    u64 current_allocated;
    u64 peak_allocated;
    
    // Enabled state
    int enabled;
    int capture_events;
} profiler;

// Global profiler instance
static profiler g_profiler = {0};

// Get CPU frequency (cycles per second)
static u64 profiler_get_cpu_frequency(void) {
    u64 start = __rdtsc();
    struct timespec ts = {0, 100000000}; // 100ms
    nanosleep(&ts, NULL);
    u64 end = __rdtsc();
    return (end - start) * 10; // Scale to per second
}

// Initialize profiler
static void profiler_init(void) {
    g_profiler.cpu_frequency = profiler_get_cpu_frequency();
    g_profiler.start_time = __rdtsc();
    g_profiler.enabled = 1;
    
    // Initialize timers
    for (u32 i = 0; i < PROFILER_MAX_TIMERS; i++) {
        g_profiler.timers[i].min_cycles = UINT64_MAX;
    }
    
    // Initialize counters
    for (u32 i = 0; i < PROFILER_MAX_COUNTERS; i++) {
        g_profiler.counters[i].min_value = UINT64_MAX;
    }
    
    printf("Profiler initialized. CPU frequency: %.2f GHz\n", 
           g_profiler.cpu_frequency / 1e9);
}

// Get or create timer
static u32 profiler_get_timer_index(const char* name) {
    // Search existing timers
    for (u32 i = 0; i < g_profiler.timer_count; i++) {
        if (g_profiler.timers[i].name == name) {
            return i;
        }
    }
    
    // Create new timer
    if (g_profiler.timer_count < PROFILER_MAX_TIMERS) {
        u32 index = g_profiler.timer_count++;
        g_profiler.timers[index].name = name;
        g_profiler.timers[index].min_cycles = UINT64_MAX;
        return index;
    }
    
    return 0; // Fallback to first timer
}

// Get or create counter
static u32 profiler_get_counter_index(const char* name) {
    // Search existing counters
    for (u32 i = 0; i < g_profiler.counter_count; i++) {
        if (g_profiler.counters[i].name == name) {
            return i;
        }
    }
    
    // Create new counter
    if (g_profiler.counter_count < PROFILER_MAX_COUNTERS) {
        u32 index = g_profiler.counter_count++;
        g_profiler.counters[index].name = name;
        g_profiler.counters[index].min_value = UINT64_MAX;
        return index;
    }
    
    return 0;
}

// Timer scope (RAII-style)
typedef struct profile_scope {
    u32 timer_index;
    u64 start_cycles;
} profile_scope;

static inline profile_scope profile_scope_begin(const char* name) {
    profile_scope scope = {0};
    
    if (!g_profiler.enabled) return scope;
    
    scope.timer_index = profiler_get_timer_index(name);
    scope.start_cycles = __rdtsc();
    
    // Add to thread's timer stack
    u32 thread_id = 0; // Would be pthread_self() in production
    if (thread_id < PROFILER_MAX_THREADS) {
        thread_profiler* tp = &g_profiler.thread_data[thread_id];
        if (tp->timer_stack_depth < 32) {
            tp->timer_stack[tp->timer_stack_depth] = scope.start_cycles;
            tp->timer_names[tp->timer_stack_depth] = name;
            tp->timer_stack_depth++;
        }
    }
    
    return scope;
}

static inline void profile_scope_end(profile_scope scope) {
    if (!g_profiler.enabled) return;
    
    u64 end_cycles = __rdtsc();
    u64 elapsed = end_cycles - scope.start_cycles;
    
    profile_timer* timer = &g_profiler.timers[scope.timer_index];
    timer->total_cycles += elapsed;
    timer->last_cycles = elapsed;
    timer->call_count++;
    
    if (elapsed < timer->min_cycles) timer->min_cycles = elapsed;
    if (elapsed > timer->max_cycles) timer->max_cycles = elapsed;
    
    // Convert to milliseconds
    f64 elapsed_ms = (f64)elapsed / (f64)g_profiler.cpu_frequency * 1000.0;
    timer->total_ms += elapsed_ms;
    timer->average_ms = timer->total_ms / timer->call_count;
    
    if (elapsed_ms < timer->min_ms || timer->min_ms == 0) timer->min_ms = elapsed_ms;
    if (elapsed_ms > timer->max_ms) timer->max_ms = elapsed_ms;
    
    // Pop from thread's timer stack
    u32 thread_id = 0;
    if (thread_id < PROFILER_MAX_THREADS) {
        thread_profiler* tp = &g_profiler.thread_data[thread_id];
        if (tp->timer_stack_depth > 0) {
            tp->timer_stack_depth--;
        }
    }
}

// Scoped timer macro
#define PROFILE_SCOPE(name) \
    profile_scope _prof_##__LINE__ = profile_scope_begin(name); \
    defer { profile_scope_end(_prof_##__LINE__); }

// Manual timer
#define PROFILE_BEGIN(name) \
    u64 _prof_start_##name = __rdtsc(); \
    const char* _prof_name_##name = #name;

#define PROFILE_END(name) \
    do { \
        u64 _prof_end = __rdtsc(); \
        u64 _prof_elapsed = _prof_end - _prof_start_##name; \
        u32 _prof_index = profiler_get_timer_index(_prof_name_##name); \
        profile_timer* _prof_timer = &g_profiler.timers[_prof_index]; \
        _prof_timer->total_cycles += _prof_elapsed; \
        _prof_timer->last_cycles = _prof_elapsed; \
        _prof_timer->call_count++; \
        if (_prof_elapsed < _prof_timer->min_cycles) _prof_timer->min_cycles = _prof_elapsed; \
        if (_prof_elapsed > _prof_timer->max_cycles) _prof_timer->max_cycles = _prof_elapsed; \
    } while(0)

// Counter operations
static inline void profile_counter_add(const char* name, u64 value) {
    if (!g_profiler.enabled) return;
    
    u32 index = profiler_get_counter_index(name);
    profile_counter* counter = &g_profiler.counters[index];
    
    counter->value += value;
    counter->total_value += value;
    counter->sample_count++;
    
    if (value < counter->min_value) counter->min_value = value;
    if (value > counter->max_value) counter->max_value = value;
}

static inline void profile_counter_set(const char* name, u64 value) {
    if (!g_profiler.enabled) return;
    
    u32 index = profiler_get_counter_index(name);
    profile_counter* counter = &g_profiler.counters[index];
    
    counter->value = value;
    counter->total_value += value;
    counter->sample_count++;
    
    if (value < counter->min_value) counter->min_value = value;
    if (value > counter->max_value) counter->max_value = value;
}

// Frame profiling
static inline void profile_frame_begin(void) {
    if (!g_profiler.enabled) return;
    
    g_profiler.frame_start = __rdtsc();
    
    // Reset frame counters
    frame_stats* frame = &g_profiler.frame_history[g_profiler.frame_index];
    frame->frame_number = g_profiler.total_frames;
    frame->start_cycles = g_profiler.frame_start;
    frame->draw_calls = 0;
    frame->triangles = 0;
    frame->state_changes = 0;
}

static inline void profile_frame_end(void) {
    if (!g_profiler.enabled) return;
    
    u64 frame_end = __rdtsc();
    frame_stats* frame = &g_profiler.frame_history[g_profiler.frame_index];
    
    frame->end_cycles = frame_end;
    u64 elapsed = frame_end - g_profiler.frame_start;
    frame->frame_time_ms = (f64)elapsed / (f64)g_profiler.cpu_frequency * 1000.0;
    frame->fps = 1000.0 / frame->frame_time_ms;
    
    // Update frame index
    g_profiler.frame_index = (g_profiler.frame_index + 1) % PROFILER_MAX_FRAME_HISTORY;
    g_profiler.total_frames++;
}

// Memory tracking
static inline void profile_memory_alloc(u64 size) {
    if (!g_profiler.enabled) return;
    
    g_profiler.allocation_count++;
    g_profiler.total_allocated += size;
    g_profiler.current_allocated += size;
    
    if (g_profiler.current_allocated > g_profiler.peak_allocated) {
        g_profiler.peak_allocated = g_profiler.current_allocated;
    }
}

static inline void profile_memory_free(u64 size) {
    if (!g_profiler.enabled) return;
    
    g_profiler.current_allocated -= size;
}

// Calculate statistics
static void profiler_calculate_stats(void) {
    // Calculate average FPS over history
    f64 total_fps = 0;
    f64 min_fps = 1000000.0;
    f64 max_fps = 0;
    
    u32 history_count = (g_profiler.total_frames < PROFILER_MAX_FRAME_HISTORY) ? 
                        g_profiler.total_frames : PROFILER_MAX_FRAME_HISTORY;
    
    for (u32 i = 0; i < history_count; i++) {
        f64 fps = g_profiler.frame_history[i].fps;
        total_fps += fps;
        if (fps < min_fps) min_fps = fps;
        if (fps > max_fps) max_fps = fps;
    }
    
    f64 avg_fps = total_fps / history_count;
    
    printf("\n=== Frame Statistics (last %u frames) ===\n", history_count);
    printf("Average FPS: %.1f\n", avg_fps);
    printf("Min FPS: %.1f\n", min_fps);
    printf("Max FPS: %.1f\n", max_fps);
}

// Print profiler report
static void profiler_print_report(void) {
    if (!g_profiler.enabled) return;
    
    printf("\n==================================================\n");
    printf("              PERFORMANCE REPORT\n");
    printf("==================================================\n");
    
    // Frame statistics
    profiler_calculate_stats();
    
    // Timer statistics
    printf("\n=== Timer Statistics ===\n");
    printf("%-30s %8s %8s %8s %8s %8s\n", 
           "Timer", "Calls", "Total(ms)", "Avg(ms)", "Min(ms)", "Max(ms)");
    printf("%-30s %8s %8s %8s %8s %8s\n",
           "-----", "-----", "---------", "-------", "-------", "-------");
    
    for (u32 i = 0; i < g_profiler.timer_count; i++) {
        profile_timer* timer = &g_profiler.timers[i];
        if (timer->call_count > 0) {
            printf("%-30s %8u %8.2f %8.3f %8.3f %8.3f\n",
                   timer->name, timer->call_count, timer->total_ms,
                   timer->average_ms, timer->min_ms, timer->max_ms);
        }
    }
    
    // Counter statistics
    if (g_profiler.counter_count > 0) {
        printf("\n=== Counter Statistics ===\n");
        printf("%-30s %12s %12s %12s %12s\n",
               "Counter", "Current", "Min", "Max", "Average");
        printf("%-30s %12s %12s %12s %12s\n",
               "-------", "-------", "---", "---", "-------");
        
        for (u32 i = 0; i < g_profiler.counter_count; i++) {
            profile_counter* counter = &g_profiler.counters[i];
            if (counter->sample_count > 0) {
                u64 average = counter->total_value / counter->sample_count;
                printf("%-30s %12llu %12llu %12llu %12llu\n",
                       counter->name, counter->value, counter->min_value,
                       counter->max_value, average);
            }
        }
    }
    
    // Memory statistics
    printf("\n=== Memory Statistics ===\n");
    printf("Total Allocations: %llu\n", g_profiler.allocation_count);
    printf("Total Allocated: %.2f MB\n", g_profiler.total_allocated / (1024.0 * 1024.0));
    printf("Current Allocated: %.2f MB\n", g_profiler.current_allocated / (1024.0 * 1024.0));
    printf("Peak Allocated: %.2f MB\n", g_profiler.peak_allocated / (1024.0 * 1024.0));
    
    printf("\n==================================================\n");
}

// Flame graph data export (for external visualization)
static void profiler_export_flamegraph(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    // Export in folded stack format for flamegraph.pl
    for (u32 t = 0; t < PROFILER_MAX_THREADS; t++) {
        thread_profiler* tp = &g_profiler.thread_data[t];
        
        for (u32 i = 0; i < tp->event_count; i++) {
            profile_event* event = &tp->events[i];
            u64 duration = event->end_cycles - event->start_cycles;
            
            // Build stack trace
            fprintf(file, "thread_%u", event->thread_id);
            for (u32 d = 0; d <= event->depth; d++) {
                fprintf(file, ";%s", event->name);
            }
            fprintf(file, " %llu\n", duration);
        }
    }
    
    fclose(file);
    printf("Flamegraph data exported to %s\n", filename);
}

// Chrome tracing format export
static void profiler_export_chrome_trace(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "[\n");
    
    int first = 1;
    for (u32 t = 0; t < PROFILER_MAX_THREADS; t++) {
        thread_profiler* tp = &g_profiler.thread_data[t];
        
        for (u32 i = 0; i < tp->event_count; i++) {
            profile_event* event = &tp->events[i];
            
            if (!first) fprintf(file, ",\n");
            first = 0;
            
            f64 start_us = (f64)(event->start_cycles - g_profiler.start_time) / 
                          (f64)g_profiler.cpu_frequency * 1e6;
            f64 duration_us = (f64)(event->end_cycles - event->start_cycles) / 
                             (f64)g_profiler.cpu_frequency * 1e6;
            
            fprintf(file, "  {\"name\": \"%s\", \"cat\": \"function\", \"ph\": \"X\", "
                   "\"ts\": %.3f, \"dur\": %.3f, \"tid\": %u, \"pid\": 1}",
                   event->name, start_us, duration_us, event->thread_id);
        }
    }
    
    fprintf(file, "\n]\n");
    fclose(file);
    printf("Chrome trace exported to %s\n", filename);
}

// Real-time display (for imgui or console)
static void profiler_display_realtime(void) {
    if (!g_profiler.enabled) return;
    
    // Get latest frame stats
    u32 last_frame = (g_profiler.frame_index + PROFILER_MAX_FRAME_HISTORY - 1) % 
                     PROFILER_MAX_FRAME_HISTORY;
    frame_stats* frame = &g_profiler.frame_history[last_frame];
    
    printf("\r");
    printf("FPS: %6.1f | Frame: %5.2fms | Draw: %4u | Tris: %6u | Mem: %4.1fMB",
           frame->fps, frame->frame_time_ms, frame->draw_calls, 
           frame->triangles, g_profiler.current_allocated / (1024.0 * 1024.0));
    fflush(stdout);
}

// Note: defer macro removed as it requires C++ and this is C code
// Use profile_scope_begin/end pairs or PROFILE_BEGIN/END macros instead

#endif // HANDMADE_PROFILER_H