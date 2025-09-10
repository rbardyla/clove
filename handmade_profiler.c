#define _GNU_SOURCE
#include "handmade_profiler_enhanced.h"
#include "handmade_memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#if PROFILER_ENABLE_GPU
#include <GL/gl.h>
#include <GL/glx.h>
#endif

// AAA-Quality Profiler Implementation
// Zero-copy ring buffers, lock-free data structures, < 1% overhead

// Thread-local storage for per-thread profiling
__thread thread_profiler_state* tls_profiler = NULL;
__thread u32 tls_thread_id = 0;

// Global profiler instance
profiler_system g_profiler_system = {0};

// event_ring_buffer is defined in header

// GPU timer implementation
#if PROFILER_ENABLE_GPU
typedef struct gpu_timer {
    GLuint query_objects[2];
    u64 start_time;
    u64 end_time;
    const char* name;
    int active;
} gpu_timer;
#else
typedef struct gpu_timer {
    u32 dummy; // Placeholder when GPU disabled
} gpu_timer;
#endif

// Network packet capture
typedef struct network_packet {
    u64 timestamp;
    u32 source_ip;
    u32 dest_ip;
    u16 source_port;
    u16 dest_port;
    u32 size;
    u8 protocol;
    f64 latency_ms;
} network_packet;

// Memory allocation record
typedef struct memory_record {
    void* address;
    size_t size;
    u64 timestamp;
    u32 thread_id;
    const char* file;
    u32 line;
    u32 frame_number;
    struct memory_record* next;
} memory_record;

// Hash table for memory tracking
#define MEMORY_HASH_SIZE 16384
typedef struct memory_tracker {
    memory_record* buckets[MEMORY_HASH_SIZE];
    _Atomic(u64) total_allocated;
    _Atomic(u64) allocation_count;
    pthread_mutex_t locks[MEMORY_HASH_SIZE];
} memory_tracker;

// Initialize profiler system
void profiler_system_init(profiler_init_params* params) {
    profiler_system* prof = &g_profiler_system;
    
    // Calculate CPU frequency accurately
    prof->cpu_frequency = profiler_calculate_cpu_frequency();
    prof->start_tsc = __rdtsc();
    prof->enabled = 1;
    prof->capture_mode = CAPTURE_NONE;  // Initialize capture mode
    
    // Initialize thread pool for background processing
    prof->thread_count = params->thread_count ? params->thread_count : 4;
    
    // Allocate ring buffers for each thread
    size_t event_buffer_size = params->event_buffer_size ? 
                               params->event_buffer_size : MEGABYTES(16);
    
    for (u32 i = 0; i < MAX_PROFILER_THREADS; i++) {
        thread_profiler_state* thread = &prof->thread_states[i];
        
        // Allocate event ring buffer
        thread->event_buffer.capacity = event_buffer_size / sizeof(profile_event);
        thread->event_buffer.events = mmap(NULL, event_buffer_size,
                                          PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        // Initialize thread-local hierarchical timers
        thread->timer_stack_depth = 0;
        thread->thread_id = i;
        
        // Allocate scratch buffer for string interning
        thread->string_buffer = mmap(NULL, MEGABYTES(1),
                                    PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    
    // Initialize memory tracker
    prof->memory_tracker = calloc(1, sizeof(memory_tracker));
    for (u32 i = 0; i < MEMORY_HASH_SIZE; i++) {
        pthread_mutex_init(&prof->memory_tracker->locks[i], NULL);
    }
    
    // Initialize GPU profiling (OpenGL queries)
#if PROFILER_ENABLE_GPU
    if (params->enable_gpu_profiling) {
        prof->gpu_timers = calloc(MAX_GPU_TIMERS, sizeof(gpu_timer));
        for (u32 i = 0; i < MAX_GPU_TIMERS; i++) {
            glGenQueries(2, prof->gpu_timers[i].query_objects);
        }
    }
#else
    (void)params->enable_gpu_profiling; // Suppress warning
#endif
    
    // Initialize network profiling
    if (params->enable_network_profiling) {
        prof->network_buffer = mmap(NULL, MEGABYTES(8),
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        prof->network_capacity = MEGABYTES(8) / sizeof(network_packet);
    }
    
    // Initialize recording buffer
    if (params->recording_buffer_size) {
        prof->recording_buffer = mmap(NULL, params->recording_buffer_size,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        prof->recording_capacity = params->recording_buffer_size;
    }
    
    // Start background aggregation thread
    pthread_create(&prof->aggregation_thread, NULL, profiler_aggregation_thread, prof);
    
    printf("[PROFILER] Initialized with CPU frequency: %.2f GHz\n", 
           prof->cpu_frequency / 1e9);
}

// Calculate CPU frequency using high-precision timing
u64 profiler_calculate_cpu_frequency(void) {
    struct timespec start_time, end_time;
    u64 start_tsc, end_tsc;
    
    // Warm up CPU
    for (volatile int i = 0; i < 1000000; i++);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    start_tsc = __rdtsc();
    
    // Measure over 100ms for accuracy
    usleep(100000);
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    end_tsc = __rdtsc();
    
    u64 elapsed_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                     (end_time.tv_nsec - start_time.tv_nsec);
    u64 elapsed_cycles = end_tsc - start_tsc;
    
    return elapsed_cycles * 1000000000ULL / elapsed_ns;
}

// Get thread-local profiler state
thread_profiler_state* profiler_get_thread_state(void) {
    if (!tls_profiler) {
        // Lazy initialization
        static _Atomic(u32) next_thread_id = 0;
        tls_thread_id = atomic_fetch_add(&next_thread_id, 1);
        tls_profiler = &g_profiler_system.thread_states[tls_thread_id];
        tls_profiler->thread_id = tls_thread_id;
    }
    return tls_profiler;
}

// Begin hierarchical timer (zero overhead when disabled)
void profiler_push_timer(const char* name, u32 color) {
    if (!g_profiler_system.enabled) return;
    
    thread_profiler_state* thread = profiler_get_thread_state();
    
    // Check stack depth
    if (thread->timer_stack_depth >= MAX_TIMER_STACK_DEPTH) return;
    
    u64 timestamp = __rdtsc();
    
    // Push to timer stack
    timer_stack_entry* entry = &thread->timer_stack[thread->timer_stack_depth++];
    entry->name = name;
    entry->start_tsc = timestamp;
    entry->color = color;
    
    // Record event if capturing
    if (g_profiler_system.capture_mode != CAPTURE_NONE) {
        profile_event* event = profiler_allocate_event(thread);
        if (event) {
            event->type = EVENT_PUSH;
            event->name = name;
            event->timestamp = timestamp;
            event->thread_id = thread->thread_id;
            event->depth = thread->timer_stack_depth - 1;
            event->color = color;
        }
    }
}

// End hierarchical timer
void profiler_pop_timer(void) {
    if (!g_profiler_system.enabled) return;
    
    thread_profiler_state* thread = profiler_get_thread_state();
    
    if (thread->timer_stack_depth == 0) return;
    
    u64 timestamp = __rdtsc();
    
    // Pop from timer stack
    thread->timer_stack_depth--;
    timer_stack_entry* entry = &thread->timer_stack[thread->timer_stack_depth];
    
    u64 elapsed = timestamp - entry->start_tsc;
    
    // Update statistics
    profiler_update_timer_stats(entry->name, elapsed);
    
    // Record event if capturing
    if (g_profiler_system.capture_mode != CAPTURE_NONE) {
        profile_event* event = profiler_allocate_event(thread);
        if (event) {
            event->type = EVENT_POP;
            event->name = entry->name;
            event->timestamp = timestamp;
            event->thread_id = thread->thread_id;
            event->depth = thread->timer_stack_depth;
            event->duration_cycles = elapsed;
        }
    }
}

// Allocate event from ring buffer (lock-free)
profile_event* profiler_allocate_event(thread_profiler_state* thread) {
    struct event_ring_buffer* buffer = &thread->event_buffer;
    
    u64 write_pos = atomic_load(&buffer->write_pos);
    u64 read_pos = atomic_load(&buffer->read_pos);
    
    // Check if buffer is full
    if ((write_pos + 1) % buffer->capacity == read_pos) {
        return NULL; // Buffer full, drop event
    }
    
    profile_event* event = &buffer->events[write_pos % buffer->capacity];
    atomic_store(&buffer->write_pos, (write_pos + 1) % buffer->capacity);
    
    return event;
}

// Update timer statistics (lock-free using atomics)
void profiler_update_timer_stats(const char* name, u64 elapsed_cycles) {
    // Hash timer name to find slot
    u32 hash = profiler_hash_string(name) % MAX_TIMERS;
    
    timer_stats* timer = &g_profiler_system.timers[hash];
    
    // Store name on first use
    if (timer->name == NULL) {
        timer->name = name;
        atomic_store(&timer->min_cycles, UINT64_MAX);  // Initialize min to max value
    }
    
    // Update using atomic operations
    atomic_fetch_add(&timer->total_cycles, elapsed_cycles);
    atomic_fetch_add(&timer->call_count, 1);
    
    // Update min/max (may have race conditions but acceptable for stats)
    u64 current_min = atomic_load(&timer->min_cycles);
    if (elapsed_cycles < current_min) {
        atomic_store(&timer->min_cycles, elapsed_cycles);
    }
    
    u64 current_max = atomic_load(&timer->max_cycles);
    if (elapsed_cycles > current_max) {
        atomic_store(&timer->max_cycles, elapsed_cycles);
    }
    
    timer->name = name; // String is interned, safe to store pointer
}

// GPU timer operations  
void profiler_gpu_begin(const char* name) {
#if !PROFILER_ENABLE_GPU
    (void)name; // Suppress unused parameter warning
    return;
#else
    if (!g_profiler_system.gpu_timers) return;
    
    // Find free timer
    gpu_timer* timer = NULL;
    for (u32 i = 0; i < MAX_GPU_TIMERS; i++) {
        if (!g_profiler_system.gpu_timers[i].active) {
            timer = &g_profiler_system.gpu_timers[i];
            timer->active = 1;
            timer->name = name;
            break;
        }
    }
    
    if (!timer) return;
    
    // Start GPU query
    glQueryCounter(timer->query_objects[0], GL_TIMESTAMP);
    timer->start_time = __rdtsc();
#endif
}

void profiler_gpu_end(const char* name) {
#if !PROFILER_ENABLE_GPU
    (void)name;
    return;
#else
    if (!g_profiler_system.gpu_timers) return;
    
    // Find matching timer
    gpu_timer* timer = NULL;
    for (u32 i = 0; i < MAX_GPU_TIMERS; i++) {
        if (g_profiler_system.gpu_timers[i].active &&
            g_profiler_system.gpu_timers[i].name == name) {
            timer = &g_profiler_system.gpu_timers[i];
            break;
        }
    }
    
    if (!timer) return;
    
    // End GPU query
    glQueryCounter(timer->query_objects[1], GL_TIMESTAMP);
    timer->end_time = __rdtsc();
    
    // Get results (may stall)
    GLuint64 start_time, end_time;
    glGetQueryObjectui64v(timer->query_objects[0], GL_QUERY_RESULT, &start_time);
    glGetQueryObjectui64v(timer->query_objects[1], GL_QUERY_RESULT, &end_time);
    
    u64 gpu_elapsed_ns = end_time - start_time;
    
    // Record GPU timing
    if (g_profiler_system.capture_mode != CAPTURE_NONE) {
        thread_profiler_state* thread = profiler_get_thread_state();
        profile_event* event = profiler_allocate_event(thread);
        if (event) {
            event->type = EVENT_GPU;
            event->name = name;
            event->timestamp = timer->start_time;
            event->duration_cycles = timer->end_time - timer->start_time;
            event->gpu_time_ns = gpu_elapsed_ns;
        }
    }
    
    timer->active = 0;
#endif
}

// Memory tracking
void profiler_track_allocation(void* ptr, size_t size, const char* file, u32 line) {
    if (!g_profiler_system.memory_tracker) return;
    
    memory_tracker* tracker = g_profiler_system.memory_tracker;
    
    // Hash address for bucket
    u32 bucket = ((uintptr_t)ptr >> 4) % MEMORY_HASH_SIZE;
    
    // Create allocation record
    memory_record* record = malloc(sizeof(memory_record));
    record->address = ptr;
    record->size = size;
    record->timestamp = __rdtsc();
    record->thread_id = tls_thread_id;
    record->file = file;
    record->line = line;
    record->frame_number = g_profiler_system.frame_number;
    
    // Insert into hash table
    pthread_mutex_lock(&tracker->locks[bucket]);
    record->next = tracker->buckets[bucket];
    tracker->buckets[bucket] = record;
    pthread_mutex_unlock(&tracker->locks[bucket]);
    
    // Update statistics
    atomic_fetch_add(&tracker->total_allocated, size);
    atomic_fetch_add(&tracker->allocation_count, 1);
}

void profiler_track_free(void* ptr) {
    if (!g_profiler_system.memory_tracker || !ptr) return;
    
    memory_tracker* tracker = g_profiler_system.memory_tracker;
    
    // Hash address for bucket
    u32 bucket = ((uintptr_t)ptr >> 4) % MEMORY_HASH_SIZE;
    
    pthread_mutex_lock(&tracker->locks[bucket]);
    
    memory_record** current = &tracker->buckets[bucket];
    while (*current) {
        if ((*current)->address == ptr) {
            memory_record* to_free = *current;
            *current = (*current)->next;
            
            // Update statistics
            atomic_fetch_sub(&tracker->total_allocated, to_free->size);
            
            free(to_free);
            break;
        }
        current = &(*current)->next;
    }
    
    pthread_mutex_unlock(&tracker->locks[bucket]);
}

// Network profiling
void profiler_record_packet(u32 src_ip, u32 dst_ip, u16 src_port, u16 dst_port,
                           u32 size, u8 protocol, f64 latency_ms) {
    if (!g_profiler_system.network_buffer) return;
    
    u64 pos = atomic_fetch_add(&g_profiler_system.network_write_pos, 1);
    
    if (pos >= g_profiler_system.network_capacity) {
        return; // Buffer full
    }
    
    network_packet* packet = &g_profiler_system.network_buffer[pos];
    packet->timestamp = __rdtsc();
    packet->source_ip = src_ip;
    packet->dest_ip = dst_ip;
    packet->source_port = src_port;
    packet->dest_port = dst_port;
    packet->size = size;
    packet->protocol = protocol;
    packet->latency_ms = latency_ms;
    
    // Update bandwidth statistics
    atomic_fetch_add(&g_profiler_system.total_bytes_sent, size);
}

// Frame operations
void profiler_begin_frame(void) {
    profiler_system* prof = &g_profiler_system;
    
    prof->frame_start_tsc = __rdtsc();
    
    // Reset per-frame counters
    prof->current_frame.draw_calls = 0;
    prof->current_frame.triangles = 0;
    prof->current_frame.state_changes = 0;
    prof->current_frame.texture_switches = 0;
    
    // Clear thread event buffers if needed
    if (prof->capture_mode == CAPTURE_SINGLE_FRAME) {
        for (u32 i = 0; i < MAX_PROFILER_THREADS; i++) {
            atomic_store(&prof->thread_states[i].event_buffer.read_pos, 0);
            atomic_store(&prof->thread_states[i].event_buffer.write_pos, 0);
        }
    }
}

void profiler_end_frame(void) {
    profiler_system* prof = &g_profiler_system;
    
    u64 frame_end_tsc = __rdtsc();
    u64 elapsed = frame_end_tsc - prof->frame_start_tsc;
    
    // Update frame stats
    prof->current_frame.duration_cycles = elapsed;
    prof->current_frame.duration_ms = cycles_to_ms(elapsed);
    prof->current_frame.fps = 1000.0 / prof->current_frame.duration_ms;
    
    // Add to history ring buffer
    u32 history_idx = prof->frame_number % FRAME_HISTORY_SIZE;
    prof->frame_history[history_idx] = prof->current_frame;
    
    prof->frame_number++;
    
    // Handle single frame capture
    if (prof->capture_mode == CAPTURE_SINGLE_FRAME) {
        prof->capture_mode = CAPTURE_NONE;
        profiler_export_chrome_trace("profile_capture.json");
    }
}

// Export to Chrome tracing format
void profiler_export_chrome_trace(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "{\n");
    fprintf(file, "  \"traceEvents\": [\n");
    
    int first = 1;
    
    // Export events from all threads
    for (u32 t = 0; t < MAX_PROFILER_THREADS; t++) {
        thread_profiler_state* thread = &g_profiler_system.thread_states[t];
        struct event_ring_buffer* buffer = &thread->event_buffer;
        
        u64 read_pos = atomic_load(&buffer->read_pos);
        u64 write_pos = atomic_load(&buffer->write_pos);
        
        while (read_pos != write_pos) {
            profile_event* event = &buffer->events[read_pos % buffer->capacity];
            
            if (!first) fprintf(file, ",\n");
            first = 0;
            
            f64 timestamp_us = cycles_to_us(event->timestamp - g_profiler_system.start_tsc);
            
            if (event->type == EVENT_PUSH) {
                fprintf(file, "    {\"name\": \"%s\", \"cat\": \"function\", "
                       "\"ph\": \"B\", \"ts\": %.3f, \"pid\": 1, \"tid\": %u}",
                       event->name, timestamp_us, event->thread_id);
            } else if (event->type == EVENT_POP) {
                fprintf(file, "    {\"name\": \"%s\", \"cat\": \"function\", "
                       "\"ph\": \"E\", \"ts\": %.3f, \"pid\": 1, \"tid\": %u}",
                       event->name, timestamp_us, event->thread_id);
            } else if (event->type == EVENT_GPU) {
                f64 duration_us = event->gpu_time_ns / 1000.0;
                fprintf(file, "    {\"name\": \"%s\", \"cat\": \"gpu\", "
                       "\"ph\": \"X\", \"ts\": %.3f, \"dur\": %.3f, "
                       "\"pid\": 1, \"tid\": 999}",
                       event->name, timestamp_us, duration_us);
            }
            
            read_pos++;
        }
    }
    
    fprintf(file, "\n  ],\n");
    
    // Export metadata
    fprintf(file, "  \"displayTimeUnit\": \"ms\",\n");
    fprintf(file, "  \"systemTraceEvents\": \"SystemTraceData\",\n");
    
    // Export thread names
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"thread_name\": {\n");
    for (u32 t = 0; t < MAX_PROFILER_THREADS; t++) {
        if (t > 0) fprintf(file, ",\n");
        fprintf(file, "      \"%u\": \"Thread %u\"", t, t);
    }
    fprintf(file, "\n    }\n");
    fprintf(file, "  }\n");
    fprintf(file, "}\n");
    
    fclose(file);
    printf("[PROFILER] Chrome trace exported to %s\n", filename);
}

// Background aggregation thread
void* profiler_aggregation_thread(void* arg) {
    profiler_system* prof = (profiler_system*)arg;
    
    // Set thread name
    pthread_setname_np(pthread_self(), "ProfilerAggregator");
    
    while (prof->running) {
        usleep(16666); // Run at 60Hz
        
        // Aggregate timer statistics
        for (u32 i = 0; i < MAX_TIMERS; i++) {
            timer_stats* timer = &prof->timers[i];
            if (timer->call_count == 0) continue;
            
            // Calculate averages
            timer->average_cycles = timer->total_cycles / timer->call_count;
            timer->average_ms = cycles_to_ms(timer->average_cycles);
        }
        
        // Calculate frame statistics
        f64 total_fps = 0;
        u32 frame_count = 0;
        
        for (u32 i = 0; i < FRAME_HISTORY_SIZE; i++) {
            if (prof->frame_history[i].fps > 0) {
                total_fps += prof->frame_history[i].fps;
                frame_count++;
            }
        }
        
        if (frame_count > 0) {
            prof->average_fps = total_fps / frame_count;
        }
        
        // Detect memory leaks
        if (prof->memory_tracker) {
            profiler_detect_leaks();
        }
    }
    
    return NULL;
}

// Memory leak detection
void profiler_detect_leaks(void) {
    memory_tracker* tracker = g_profiler_system.memory_tracker;
    
    // Find allocations older than N frames
    u32 current_frame = g_profiler_system.frame_number;
    u32 leak_threshold_frames = 600; // 10 seconds at 60 FPS
    
    for (u32 bucket = 0; bucket < MEMORY_HASH_SIZE; bucket++) {
        pthread_mutex_lock(&tracker->locks[bucket]);
        
        memory_record* record = tracker->buckets[bucket];
        while (record) {
            if (current_frame - record->frame_number > leak_threshold_frames) {
                // Potential leak detected
                printf("[LEAK] Potential memory leak: %zu bytes allocated at %s:%u "
                       "(frame %u, current %u)\n",
                       record->size, record->file, record->line,
                       record->frame_number, current_frame);
            }
            record = record->next;
        }
        
        pthread_mutex_unlock(&tracker->locks[bucket]);
    }
}

// Utility functions
u32 profiler_hash_string(const char* str) {
    u32 hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

f64 cycles_to_ms(u64 cycles) {
    return (f64)cycles / (f64)g_profiler_system.cpu_frequency * 1000.0;
}

f64 cycles_to_us(u64 cycles) {
    return (f64)cycles / (f64)g_profiler_system.cpu_frequency * 1000000.0;
}

// Recording and playback
void profiler_start_recording(void) {
    profiler_system* prof = &g_profiler_system;
    
    if (!prof->recording_buffer) return;
    
    prof->recording_active = 1;
    prof->recording_write_pos = 0;
    prof->recording_start_frame = prof->frame_number;
    prof->capture_mode = CAPTURE_CONTINUOUS;  // Fix: Set capture mode!
    
    printf("[PROFILER] Started recording\n");
}

void profiler_stop_recording(void) {
    profiler_system* prof = &g_profiler_system;
    
    if (!prof->recording_active) return;
    
    prof->recording_active = 0;
    prof->capture_mode = CAPTURE_NONE;  // Fix: Reset capture mode!
    
    // Save recording to file
    char filename[256];
    snprintf(filename, sizeof(filename), "profile_recording_%u.dat", 
             prof->recording_start_frame);
    
    FILE* file = fopen(filename, "wb");
    if (file) {
        // Write header
        u32 magic = 0x50524F46; // "PROF"
        u32 version = 1;
        fwrite(&magic, sizeof(u32), 1, file);
        fwrite(&version, sizeof(u32), 1, file);
        fwrite(&prof->recording_write_pos, sizeof(u64), 1, file);
        
        // Write data
        fwrite(prof->recording_buffer, 1, prof->recording_write_pos, file);
        
        fclose(file);
        printf("[PROFILER] Recording saved to %s (%lu bytes)\n", 
               filename, (unsigned long)prof->recording_write_pos);
    }
}

void profiler_record_frame_data(void* data, size_t size) {
    profiler_system* prof = &g_profiler_system;
    
    if (!prof->recording_active) return;
    
    if (prof->recording_write_pos + size > prof->recording_capacity) {
        profiler_stop_recording();
        return;
    }
    
    memcpy(prof->recording_buffer + prof->recording_write_pos, data, size);
    prof->recording_write_pos += size;
}

// Shutdown
void profiler_shutdown(void) {
    profiler_system* prof = &g_profiler_system;
    
    prof->running = 0;
    
    // Wait for aggregation thread
    pthread_join(prof->aggregation_thread, NULL);
    
    // Export final trace
    profiler_export_chrome_trace("final_trace.json");
    
    // Free resources
    for (u32 i = 0; i < MAX_PROFILER_THREADS; i++) {
        thread_profiler_state* thread = &prof->thread_states[i];
        if (thread->event_buffer.events) {
            munmap(thread->event_buffer.events, 
                   thread->event_buffer.capacity * sizeof(profile_event));
        }
        if (thread->string_buffer) {
            munmap(thread->string_buffer, MEGABYTES(1));
        }
    }
    
    // Free GPU timers
#if PROFILER_ENABLE_GPU
    if (prof->gpu_timers) {
        for (u32 i = 0; i < MAX_GPU_TIMERS; i++) {
            glDeleteQueries(2, prof->gpu_timers[i].query_objects);
        }
        free(prof->gpu_timers);
    }
#endif
    
    // Free memory tracker
    if (prof->memory_tracker) {
        for (u32 bucket = 0; bucket < MEMORY_HASH_SIZE; bucket++) {
            memory_record* record = prof->memory_tracker->buckets[bucket];
            while (record) {
                memory_record* next = record->next;
                free(record);
                record = next;
            }
            pthread_mutex_destroy(&prof->memory_tracker->locks[bucket]);
        }
        free(prof->memory_tracker);
    }
    
    // Free network buffer
    if (prof->network_buffer) {
        munmap(prof->network_buffer, prof->network_capacity * sizeof(network_packet));
    }
    
    // Free recording buffer
    if (prof->recording_buffer) {
        munmap(prof->recording_buffer, prof->recording_capacity);
    }
    
    printf("[PROFILER] Shutdown complete\n");
}

// === Query API Implementation ===

void profiler_counter_internal(const char* name, u64 value) {
    if (!g_profiler_system.enabled) return;
    
    // For now just add to frame stats
    if (strcmp(name, "triangles_drawn") == 0) {
        g_profiler_system.current_frame.triangles = (u32)value;
    } else if (strcmp(name, "draw_calls") == 0) {
        g_profiler_system.current_frame.draw_calls = (u32)value;
    }
}

f64 profiler_get_timer_ms(const char* name) {
    u32 hash = profiler_hash_string(name) % MAX_TIMERS;
    timer_stats* timer = &g_profiler_system.timers[hash];
    
    u64 total_cycles = atomic_load(&timer->total_cycles);
    u64 call_count = atomic_load(&timer->call_count);
    
    if (call_count == 0) return 0.0;
    
    f64 average_cycles = (f64)total_cycles / (f64)call_count;
    return cycles_to_ms((u64)average_cycles);
}

u64 profiler_get_timer_calls(const char* name) {
    u32 hash = profiler_hash_string(name) % MAX_TIMERS;
    return g_profiler_system.timers[hash].call_count;
}

f64 profiler_get_average_fps(void) {
    return g_profiler_system.average_fps;
}

u64 profiler_get_current_memory(void) {
    return g_profiler_system.current_allocated;
}

u64 profiler_get_peak_memory(void) {
    return g_profiler_system.peak_allocated;
}

frame_stats* profiler_get_frame_stats(u32 frame_offset) {
    if (frame_offset >= FRAME_HISTORY_SIZE) return NULL;
    
    u32 idx = (g_profiler_system.frame_number + frame_offset) % FRAME_HISTORY_SIZE;
    return &g_profiler_system.frame_history[idx];
}

void profiler_export_flamegraph(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    // Simple flamegraph export
    for (u32 i = 0; i < MAX_TIMERS; i++) {
        timer_stats* timer = &g_profiler_system.timers[i];
        if (timer->call_count > 0 && timer->name) {
            fprintf(file, "%s %u\n", timer->name, (u32)(timer->average_ms * 1000));
        }
    }
    
    fclose(file);
    printf("[PROFILER] Flamegraph exported to %s\n", filename);
}