#include "handmade_profiler_enhanced.h"
#include "handmade_debugger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

// AAA-Quality Profiler Demo
// Demonstrates all profiling and debugging features

// Simulation data structures
typedef struct game_entity {
    f32 x, y, z;
    f32 vx, vy, vz;
    f32 mass;
    int active;
} game_entity;

typedef struct simulation_state {
    game_entity entities[1000];
    u32 entity_count;
    f32 world_time;
    u64 frame_number;
    
    // Threading
    pthread_t worker_threads[4];
    int shutdown_requested;
    
    // Memory pools
    void* physics_memory;
    void* render_memory;
    void* ai_memory;
    
} simulation_state;

static simulation_state g_simulation = {0};

// Forward declarations
void* physics_thread(void* arg);
void* ai_thread(void* arg);
void* network_thread(void* arg);
void simulate_frame(simulation_state* sim);
void render_frame(simulation_state* sim);
void update_physics(simulation_state* sim);
void update_ai(simulation_state* sim);
void process_input(simulation_state* sim);
void stress_test_memory(void);
void stress_test_gpu(void);
void simulate_network_activity(void);

int main(int argc, char** argv) {
    printf("=== AAA-Quality Profiler Demo ===\n\n");
    
    // Initialize profiler system
    profiler_init_params prof_params = {0};
    prof_params.thread_count = 4;
    prof_params.event_buffer_size = MEGABYTES(4);
    prof_params.recording_buffer_size = MEGABYTES(64);
    prof_params.enable_gpu_profiling = 1;
    prof_params.enable_network_profiling = 1;
    prof_params.enable_memory_tracking = 1;
    prof_params.target_overhead_percent = 0.5;
    
    profiler_system_init(&prof_params);
    
    // Initialize debugger
    debugger_context debug_ctx;
    debugger_init(&debug_ctx);
    
    // Add some watches
    DBG_WATCH(g_simulation.entity_count);
    DBG_WATCH(g_simulation.world_time);
    
    // Setup simulation
    g_simulation.entity_count = 500;
    for (u32 i = 0; i < g_simulation.entity_count; i++) {
        game_entity* entity = &g_simulation.entities[i];
        entity->x = (f32)(rand() % 1000);
        entity->y = (f32)(rand() % 1000);
        entity->z = (f32)(rand() % 1000);
        entity->mass = 1.0f + (f32)(rand() % 100) / 100.0f;
        entity->active = 1;
    }
    
    // Allocate memory pools (tracked)
    g_simulation.physics_memory = malloc(MEGABYTES(16));
    PROFILE_ALLOC(g_simulation.physics_memory, MEGABYTES(16));
    
    g_simulation.render_memory = malloc(MEGABYTES(32));
    PROFILE_ALLOC(g_simulation.render_memory, MEGABYTES(32));
    
    g_simulation.ai_memory = malloc(MEGABYTES(8));
    PROFILE_ALLOC(g_simulation.ai_memory, MEGABYTES(8));
    
    // Start worker threads
    pthread_create(&g_simulation.worker_threads[0], NULL, physics_thread, &g_simulation);
    pthread_create(&g_simulation.worker_threads[1], NULL, ai_thread, &g_simulation);
    pthread_create(&g_simulation.worker_threads[2], NULL, network_thread, &g_simulation);
    
    // Start recording
    profiler_start_recording();
    
    printf("Running simulation for 10 seconds...\n");
    printf("Press Ctrl+C to interrupt and see profiler report\n\n");
    
    // Main loop
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    
    u32 demo_frames = 0;
    while (demo_frames < 600) { // 10 seconds at 60 FPS
        profiler_begin_frame();
        
        // Simulate one frame
        simulate_frame(&g_simulation);
        
        // Add some artificial load
        if (demo_frames % 60 == 0) {
            stress_test_memory();
        }
        
        if (demo_frames % 120 == 0) {
            stress_test_gpu();
        }
        
        if (demo_frames % 30 == 0) {
            simulate_network_activity();
        }
        
        profiler_end_frame();
        
        demo_frames++;
        
        // Sleep to maintain ~60 FPS
        usleep(16666);
        
        // Show progress
        if (demo_frames % 60 == 0) {
            printf("Frame %u/600 (%.1f%% complete)\n", 
                   demo_frames, (f32)demo_frames / 600.0f * 100.0f);
        }
    }
    
    // Stop recording
    profiler_stop_recording();
    
    // Signal shutdown
    g_simulation.shutdown_requested = 1;
    
    // Wait for threads
    for (int i = 0; i < 3; i++) {
        pthread_join(g_simulation.worker_threads[i], NULL);
    }
    
    // Free tracked memory
    PROFILE_FREE(g_simulation.physics_memory);
    free(g_simulation.physics_memory);
    
    PROFILE_FREE(g_simulation.render_memory);
    free(g_simulation.render_memory);
    
    PROFILE_FREE(g_simulation.ai_memory);
    free(g_simulation.ai_memory);
    
    // Generate reports
    printf("\n=== PROFILER REPORTS ===\n");
    
    // Export Chrome trace
    profiler_export_chrome_trace("demo_trace.json");
    
    // Export flamegraph
    profiler_export_flamegraph("demo_flame.txt");
    
    // Show statistics
    printf("\nFrame Statistics:\n");
    printf("Average FPS: %.1f\n", profiler_get_average_fps());
    printf("Current Memory: %.2f MB\n", profiler_get_current_memory() / (1024.0 * 1024.0));
    printf("Peak Memory: %.2f MB\n", profiler_get_peak_memory() / (1024.0 * 1024.0));
    
    printf("\nTop Functions by Time:\n");
    printf("Update Physics: %.3f ms\n", profiler_get_timer_ms("update_physics"));
    printf("Render Frame: %.3f ms\n", profiler_get_timer_ms("render_frame"));
    printf("Update AI: %.3f ms\n", profiler_get_timer_ms("update_ai"));
    printf("Process Input: %.3f ms\n", profiler_get_timer_ms("process_input"));
    
    // Cleanup
    debugger_shutdown(&debug_ctx);
    profiler_shutdown();
    
    printf("\nDemo complete! Check these files:\n");
    printf("  demo_trace.json - Chrome tracing format (chrome://tracing)\n");
    printf("  demo_flame.txt - Flamegraph data (use flamegraph.pl)\n");
    printf("  profile_recording_*.dat - Full recording data\n");
    
    return 0;
}

// === Simulation Functions ===

void simulate_frame(simulation_state* sim) {
    PROFILE_SCOPE("simulate_frame");
    
    // Update world time
    sim->world_time += 1.0f / 60.0f;
    sim->frame_number++;
    
    // Process input
    process_input(sim);
    
    // Update physics (main thread portion)
    update_physics(sim);
    
    // Update AI (main thread portion)
    update_ai(sim);
    
    // Render
    render_frame(sim);
    
    // Update profiler counters
    profiler_counter("active_entities", sim->entity_count);
    profiler_counter("world_time", (u64)(sim->world_time * 1000));
}

void process_input(simulation_state* sim) {
    PROFILE_SCOPE_COLOR("process_input", 0x569CD6FF);
    
    // Simulate input processing overhead
    for (volatile int i = 0; i < 10000; i++);
    
    // Occasionally spawn new entities
    if (sim->frame_number % 120 == 0 && sim->entity_count < 950) {
        game_entity* entity = &sim->entities[sim->entity_count++];
        entity->x = (f32)(rand() % 1000);
        entity->y = (f32)(rand() % 1000);
        entity->z = (f32)(rand() % 1000);
        entity->mass = 1.0f;
        entity->active = 1;
    }
}

void update_physics(simulation_state* sim) {
    PROFILE_SCOPE_COLOR("update_physics", 0x4EC9B0FF);
    
    // Simulate physics calculations
    {
        PROFILE_SCOPE("collision_detection");
        
        // Broad phase
        for (volatile int i = 0; i < 50000; i++);
        
        // Narrow phase
        for (volatile int i = 0; i < 25000; i++);
    }
    
    // Integration
    {
        PROFILE_SCOPE("integration");
        
        for (u32 i = 0; i < sim->entity_count; i++) {
            game_entity* entity = &sim->entities[i];
            if (!entity->active) continue;
            
            // Simple physics integration
            entity->x += entity->vx / 60.0f;
            entity->y += entity->vy / 60.0f;
            entity->z += entity->vz / 60.0f;
            
            // Add some artificial calculation overhead
            for (volatile int j = 0; j < 100; j++);
        }
    }
}

void update_ai(simulation_state* sim) {
    PROFILE_SCOPE_COLOR("update_ai", 0x608B4EFF);
    
    // Simulate AI processing
    {
        PROFILE_SCOPE("pathfinding");
        for (volatile int i = 0; i < 75000; i++);
    }
    
    {
        PROFILE_SCOPE("decision_making");
        for (volatile int i = 0; i < 30000; i++);
    }
    
    {
        PROFILE_SCOPE("behavior_trees");
        
        // Update every 10th entity per frame for AI
        u32 ai_start = (sim->frame_number * 50) % sim->entity_count;
        for (u32 i = 0; i < 50 && ai_start + i < sim->entity_count; i++) {
            game_entity* entity = &sim->entities[ai_start + i];
            if (!entity->active) continue;
            
            // Simple AI behavior
            entity->vx = sinf(sim->world_time + i) * 10.0f;
            entity->vy = cosf(sim->world_time + i) * 10.0f;
            
            for (volatile int j = 0; j < 500; j++);
        }
    }
}

void render_frame(simulation_state* sim) {
    PROFILE_SCOPE_COLOR("render_frame", 0xDCDCAA88);
    
    // Simulate GPU commands
    GPU_PROFILE_SCOPE("full_frame_render");
    
    {
        PROFILE_SCOPE("cull_objects");
        for (volatile int i = 0; i < 40000; i++);
    }
    
    {
        PROFILE_SCOPE("draw_geometry");
        GPU_PROFILE_SCOPE("draw_geometry");
        
        // Simulate draw calls
        for (u32 i = 0; i < sim->entity_count / 10; i++) {
            profiler_counter("draw_calls", 1);
            profiler_counter("triangles", 24); // Cube = 12 triangles
            
            for (volatile int j = 0; j < 200; j++);
        }
    }
    
    {
        PROFILE_SCOPE("post_processing");
        GPU_PROFILE_SCOPE("post_processing");
        
        for (volatile int i = 0; i < 60000; i++);
    }
}

// === Worker Threads ===

void* physics_thread(void* arg) {
    simulation_state* sim = (simulation_state*)arg;
    
    while (!sim->shutdown_requested) {
        PROFILE_SCOPE("physics_thread_work");
        
        // Simulate background physics work
        {
            PROFILE_SCOPE("constraint_solving");
            for (volatile int i = 0; i < 100000; i++);
        }
        
        usleep(5000); // 200 Hz
    }
    
    return NULL;
}

void* ai_thread(void* arg) {
    simulation_state* sim = (simulation_state*)arg;
    
    while (!sim->shutdown_requested) {
        PROFILE_SCOPE("ai_thread_work");
        
        // Simulate background AI work
        {
            PROFILE_SCOPE("neural_network_inference");
            for (volatile int i = 0; i < 80000; i++);
        }
        
        {
            PROFILE_SCOPE("world_state_analysis");
            for (volatile int i = 0; i < 40000; i++);
        }
        
        usleep(10000); // 100 Hz
    }
    
    return NULL;
}

void* network_thread(void* arg) {
    simulation_state* sim = (simulation_state*)arg;
    
    while (!sim->shutdown_requested) {
        PROFILE_SCOPE("network_thread_work");
        
        // Simulate network processing
        {
            PROFILE_SCOPE("packet_processing");
            
            // Simulate sending packets
            profiler_record_packet(0x7F000001, 0xC0A80101, 8080, 80,
                                  1024, 6, 25.5); // TCP packet
            
            profiler_network_send(1024);
            
            for (volatile int i = 0; i < 20000; i++);
        }
        
        {
            PROFILE_SCOPE("connection_management");
            for (volatile int i = 0; i < 15000; i++);
        }
        
        usleep(20000); // 50 Hz
    }
    
    return NULL;
}

// === Stress Tests ===

void stress_test_memory(void) {
    PROFILE_SCOPE("stress_test_memory");
    
    // Allocate and free many small objects
    void* ptrs[1000];
    
    for (int i = 0; i < 1000; i++) {
        size_t size = 64 + (rand() % 512);
        ptrs[i] = malloc(size);
        PROFILE_ALLOC(ptrs[i], size);
        
        // Write some data
        memset(ptrs[i], 0xAA, size);
    }
    
    // Free half randomly
    for (int i = 0; i < 500; i++) {
        int idx = rand() % 1000;
        if (ptrs[idx]) {
            PROFILE_FREE(ptrs[idx]);
            free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }
    
    // Free the rest
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i]) {
            PROFILE_FREE(ptrs[i]);
            free(ptrs[i]);
        }
    }
}

void stress_test_gpu(void) {
    PROFILE_SCOPE("stress_test_gpu");
    
    // Simulate heavy GPU workload
    GPU_PROFILE_SCOPE("gpu_stress_test");
    
    {
        PROFILE_SCOPE("generate_geometry");
        for (volatile int i = 0; i < 200000; i++);
    }
    
    {
        PROFILE_SCOPE("upload_textures");
        for (volatile int i = 0; i < 150000; i++);
    }
    
    {
        PROFILE_SCOPE("compute_shaders");
        for (volatile int i = 0; i < 300000; i++);
    }
}

void simulate_network_activity(void) {
    PROFILE_SCOPE("simulate_network_activity");
    
    // Simulate various network operations
    for (int i = 0; i < 10; i++) {
        u32 src = 0x7F000001;
        u32 dst = 0xC0A80100 + (rand() % 255);
        u16 src_port = 8080;
        u16 dst_port = 80 + (rand() % 8000);
        u32 size = 64 + (rand() % 1400);
        f64 latency = 1.0 + (f64)(rand() % 50);
        
        profiler_record_packet(src, dst, src_port, dst_port, size, 6, latency);
        
        if (rand() % 2) {
            profiler_network_send(size);
        } else {
            profiler_network_receive(size);
        }
    }
}

// === Demo Control ===

void demo_show_help(void) {
    printf("\nDemo Controls:\n");
    printf("  P - Pause/Resume profiler\n");
    printf("  C - Capture single frame\n");
    printf("  R - Start/Stop recording\n");
    printf("  E - Export Chrome trace\n");
    printf("  M - Check for memory leaks\n");
    printf("  S - Show statistics\n");
    printf("  H - Show this help\n");
    printf("  Q - Quit demo\n");
}

void demo_handle_input(char key) {
    switch (key) {
        case 'P':
        case 'p':
            g_profiler_system.enabled = !g_profiler_system.enabled;
            printf("Profiler %s\n", g_profiler_system.enabled ? "enabled" : "disabled");
            break;
            
        case 'C':
        case 'c':
            profiler_set_capture_mode(CAPTURE_SINGLE_FRAME);
            printf("Single frame capture triggered\n");
            break;
            
        case 'R':
        case 'r':
            if (g_profiler_system.recording_active) {
                profiler_stop_recording();
                printf("Recording stopped\n");
            } else {
                profiler_start_recording();
                printf("Recording started\n");
            }
            break;
            
        case 'E':
        case 'e':
            profiler_export_chrome_trace("live_trace.json");
            printf("Chrome trace exported to live_trace.json\n");
            break;
            
        case 'M':
        case 'm':
            profiler_detect_leaks();
            break;
            
        case 'S':
        case 's':
            printf("\nCurrent Statistics:\n");
            printf("FPS: %.1f\n", profiler_get_average_fps());
            printf("Memory: %.2f MB\n", profiler_get_current_memory() / (1024.0 * 1024.0));
            printf("Entities: %u\n", g_simulation.entity_count);
            break;
            
        case 'H':
        case 'h':
            demo_show_help();
            break;
            
        case 'Q':
        case 'q':
            exit(0);
            break;
    }
}