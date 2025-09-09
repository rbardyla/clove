#ifndef HANDMADE_DEBUGGER_H
#define HANDMADE_DEBUGGER_H

#include "handmade_memory.h"
#include "handmade_entity_soa.h"
#include "handmade_neural_npc.h"
#include "handmade_profiler.h"

// Visual Debugger Interface - The Secret Weapon
// This is what makes demos mind-blowing

typedef struct breakpoint {
    void* address;
    u32 line_number;
    const char* file;
    const char* condition;
    u32 hit_count;
    int enabled;
} breakpoint;

typedef struct watch_expression {
    const char* expression;
    void* address;
    size_t size;
    const char* type_name;
    char value_str[256];
    int expanded;
} watch_expression;

typedef struct memory_view {
    void* base_address;
    size_t view_size;
    u32 bytes_per_row;
    u32 highlight_start;
    u32 highlight_size;
    int show_ascii;
} memory_view;

typedef struct neural_debugger {
    // Neural network visualization
    neural_brain* selected_brain;
    u32 selected_layer;
    
    // Weight matrix heatmap
    f32* weight_visualization;
    u32 viz_width;
    u32 viz_height;
    
    // Activation history
    f32 activation_history[1024];
    u32 history_index;
    
    // Decision path tracing
    struct {
        const char* decision;
        f32 confidence;
        u64 timestamp;
    } decision_history[64];
    u32 decision_count;
} neural_debugger;

typedef struct physics_debugger {
    // Collision visualization
    int show_colliders;
    int show_contacts;
    int show_velocities;
    int show_forces;
    
    // Performance overlay
    struct {
        f32 broad_phase_ms;
        f32 narrow_phase_ms;
        f32 integration_ms;
        u32 active_bodies;
        u32 sleeping_bodies;
        u32 contact_pairs;
    } stats;
    
    // Constraint visualization
    int show_constraints;
    v4 constraint_color;
} physics_debugger;

typedef struct entity_debugger {
    // Entity inspection
    entity_handle selected_entity;
    u32 component_mask;
    
    // Component data view
    struct {
        const char* name;
        void* data;
        size_t size;
        void (*draw_func)(void* data);
    } components[32];
    u32 component_count;
    
    // Spatial visualization
    int show_octree;
    int show_entity_bounds;
    int show_entity_ids;
} entity_debugger;

typedef struct profiler_debugger {
    // Frame timeline
    profile_event frame_events[PROFILER_MAX_EVENTS];
    u32 event_count;
    
    // Flame graph
    struct flame_node {
        const char* name;
        f64 self_time_ms;
        f64 total_time_ms;
        u32 call_count;
        struct flame_node* children[16];
        u32 child_count;
    } flame_root;
    
    // Memory graph
    f32 memory_history[256];
    u32 history_index;
    
    // Cache statistics
    u64 cache_hits;
    u64 cache_misses;
    f64 hit_rate;
} profiler_debugger;

typedef struct debugger_state {
    // Core debugging
    breakpoint breakpoints[256];
    u32 breakpoint_count;
    
    watch_expression watches[128];
    u32 watch_count;
    
    memory_view memory_views[8];
    u32 memory_view_count;
    
    // System debuggers
    neural_debugger neural;
    physics_debugger physics;
    entity_debugger entity;
    profiler_debugger profiler;
    
    // UI state
    int show_debugger;
    int debugger_width;
    v2 debugger_pos;
    
    // Execution control
    int paused;
    int single_step;
    int step_over;
    f32 time_scale;
    
    // Recording
    int recording;
    u8* record_buffer;
    size_t record_size;
    size_t record_capacity;
} debugger_state;

// Initialize debugger
static debugger_state* debugger_init(arena* permanent_arena) {
    debugger_state* dbg = arena_alloc(permanent_arena, sizeof(debugger_state));
    
    // Default configuration
    dbg->debugger_width = 400;
    dbg->time_scale = 1.0f;
    dbg->physics.constraint_color = (v4){1.0f, 1.0f, 0.0f, 0.5f};
    
    // Allocate visualization buffers
    dbg->neural.weight_visualization = arena_alloc_array(permanent_arena, f32, 256 * 256);
    dbg->record_capacity = MEGABYTES(64);
    dbg->record_buffer = arena_alloc(permanent_arena, dbg->record_capacity);
    
    return dbg;
}

// Add breakpoint
static void debugger_add_breakpoint(debugger_state* dbg, void* address, const char* file, u32 line) {
    if (dbg->breakpoint_count >= 256) return;
    
    breakpoint* bp = &dbg->breakpoints[dbg->breakpoint_count++];
    bp->address = address;
    bp->file = file;
    bp->line_number = line;
    bp->enabled = 1;
    bp->hit_count = 0;
}

// Add watch
static void debugger_add_watch(debugger_state* dbg, const char* expression, void* address, size_t size) {
    if (dbg->watch_count >= 128) return;
    
    watch_expression* watch = &dbg->watches[dbg->watch_count++];
    watch->expression = expression;
    watch->address = address;
    watch->size = size;
}

// Update neural visualization
static void debugger_update_neural(debugger_state* dbg, neural_brain* brain) {
    dbg->neural.selected_brain = brain;
    
    if (!brain) return;
    
    // Visualize weight matrix as heatmap
    weight_matrix* layer = &brain->layers[dbg->neural.selected_layer];
    u32 total_weights = layer->rows * layer->cols;
    
    f32 min_weight = FLT_MAX;
    f32 max_weight = -FLT_MAX;
    
    // Find range
    for (u32 i = 0; i < total_weights; i++) {
        f32 w = layer->weights[i];
        if (w < min_weight) min_weight = w;
        if (w > max_weight) max_weight = w;
    }
    
    // Normalize to 0-1 for visualization
    f32 range = max_weight - min_weight;
    if (range > 0.0001f) {
        for (u32 i = 0; i < total_weights; i++) {
            dbg->neural.weight_visualization[i] = 
                (layer->weights[i] - min_weight) / range;
        }
    }
    
    dbg->neural.viz_width = layer->cols;
    dbg->neural.viz_height = layer->rows;
}

// Record decision
static void debugger_record_decision(debugger_state* dbg, const char* decision, f32 confidence) {
    if (dbg->neural.decision_count >= 64) {
        // Shift history
        for (u32 i = 0; i < 63; i++) {
            dbg->neural.decision_history[i] = dbg->neural.decision_history[i + 1];
        }
        dbg->neural.decision_count = 63;
    }
    
    u32 idx = dbg->neural.decision_count++;
    dbg->neural.decision_history[idx].decision = decision;
    dbg->neural.decision_history[idx].confidence = confidence;
    dbg->neural.decision_history[idx].timestamp = __rdtsc();
}

// Update physics visualization
static void debugger_update_physics(debugger_state* dbg, physics_soa* physics, u32 count) {
    dbg->physics.stats.active_bodies = 0;
    dbg->physics.stats.sleeping_bodies = 0;
    
    for (u32 i = 0; i < count; i++) {
        f32 vel_sq = physics->velocities_x[i] * physics->velocities_x[i] +
                    physics->velocities_y[i] * physics->velocities_y[i] +
                    physics->velocities_z[i] * physics->velocities_z[i];
        
        if (vel_sq < 0.01f) {
            dbg->physics.stats.sleeping_bodies++;
        } else {
            dbg->physics.stats.active_bodies++;
        }
    }
}

// Draw debug overlay (immediate mode)
static void debugger_draw(debugger_state* dbg) {
    if (!dbg->show_debugger) return;
    
    // Draw background
    draw_rect(dbg->debugger_pos.x, dbg->debugger_pos.y, 
              dbg->debugger_width, 600, 
              (v4){0.1f, 0.1f, 0.1f, 0.9f});
    
    f32 y = dbg->debugger_pos.y + 20;
    
    // Execution controls
    if (dbg->paused) {
        draw_text(dbg->debugger_pos.x + 10, y, "PAUSED", (v4){1, 0, 0, 1});
    } else {
        draw_text(dbg->debugger_pos.x + 10, y, "RUNNING", (v4){0, 1, 0, 1});
    }
    y += 20;
    
    // Time scale
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "Time Scale: %.2fx", dbg->time_scale);
    draw_text(dbg->debugger_pos.x + 10, y, time_str, (v4){1, 1, 1, 1});
    y += 30;
    
    // Neural debugger
    if (dbg->neural.selected_brain) {
        draw_text(dbg->debugger_pos.x + 10, y, "=== NEURAL DEBUG ===", (v4){0.5f, 1, 0.5f, 1});
        y += 20;
        
        // Draw weight heatmap
        for (u32 row = 0; row < dbg->neural.viz_height && row < 16; row++) {
            for (u32 col = 0; col < dbg->neural.viz_width && col < 32; col++) {
                f32 weight = dbg->neural.weight_visualization[row * dbg->neural.viz_width + col];
                v4 color = {weight, 1.0f - weight, 0.0f, 1.0f};
                draw_rect(dbg->debugger_pos.x + 10 + col * 10,
                         y + row * 10, 8, 8, color);
            }
        }
        y += 170;
        
        // Recent decisions
        draw_text(dbg->debugger_pos.x + 10, y, "Recent Decisions:", (v4){1, 1, 1, 1});
        y += 15;
        
        for (u32 i = 0; i < dbg->neural.decision_count && i < 5; i++) {
            char dec_str[128];
            snprintf(dec_str, sizeof(dec_str), "  %s (%.1f%%)", 
                    dbg->neural.decision_history[i].decision,
                    dbg->neural.decision_history[i].confidence * 100.0f);
            draw_text(dbg->debugger_pos.x + 10, y, dec_str, (v4){0.8f, 0.8f, 0.8f, 1});
            y += 15;
        }
        y += 10;
    }
    
    // Physics debugger
    draw_text(dbg->debugger_pos.x + 10, y, "=== PHYSICS DEBUG ===", (v4){0.5f, 0.5f, 1, 1});
    y += 20;
    
    char phys_str[256];
    snprintf(phys_str, sizeof(phys_str), 
             "Active: %u | Sleeping: %u | Contacts: %u",
             dbg->physics.stats.active_bodies,
             dbg->physics.stats.sleeping_bodies,
             dbg->physics.stats.contact_pairs);
    draw_text(dbg->debugger_pos.x + 10, y, phys_str, (v4){1, 1, 1, 1});
    y += 20;
    
    // Watches
    if (dbg->watch_count > 0) {
        draw_text(dbg->debugger_pos.x + 10, y, "=== WATCHES ===", (v4){1, 1, 0.5f, 1});
        y += 20;
        
        for (u32 i = 0; i < dbg->watch_count && i < 5; i++) {
            watch_expression* watch = &dbg->watches[i];
            
            // Format value based on size
            if (watch->size == sizeof(f32)) {
                snprintf(watch->value_str, sizeof(watch->value_str), 
                        "%s = %.3f", watch->expression, *(f32*)watch->address);
            } else if (watch->size == sizeof(u32)) {
                snprintf(watch->value_str, sizeof(watch->value_str),
                        "%s = %u", watch->expression, *(u32*)watch->address);
            }
            
            draw_text(dbg->debugger_pos.x + 10, y, watch->value_str, (v4){1, 1, 1, 1});
            y += 15;
        }
    }
}

// Record frame for replay
static void debugger_record_frame(debugger_state* dbg, void* frame_data, size_t size) {
    if (!dbg->recording) return;
    
    if (dbg->record_size + size > dbg->record_capacity) {
        dbg->recording = 0;  // Stop recording when full
        return;
    }
    
    memcpy(dbg->record_buffer + dbg->record_size, frame_data, size);
    dbg->record_size += size;
}

// Keyboard shortcuts
static void debugger_handle_input(debugger_state* dbg, int key) {
    switch (key) {
        case 'P':  // Pause/Resume
            dbg->paused = !dbg->paused;
            break;
        case 'S':  // Single step
            dbg->single_step = 1;
            break;
        case 'R':  // Start/Stop recording
            dbg->recording = !dbg->recording;
            if (dbg->recording) {
                dbg->record_size = 0;
            }
            break;
        case '+':  // Speed up
            dbg->time_scale *= 2.0f;
            if (dbg->time_scale > 8.0f) dbg->time_scale = 8.0f;
            break;
        case '-':  // Slow down
            dbg->time_scale *= 0.5f;
            if (dbg->time_scale < 0.125f) dbg->time_scale = 0.125f;
            break;
        case 'D':  // Toggle debugger
            dbg->show_debugger = !dbg->show_debugger;
            break;
    }
}

// Helper to draw primitives (would be in renderer)
static void draw_rect(f32 x, f32 y, f32 w, f32 h, v4 color) {
    // Implementation depends on renderer
}

static void draw_text(f32 x, f32 y, const char* text, v4 color) {
    // Implementation depends on renderer
}

#endif // HANDMADE_DEBUGGER_H