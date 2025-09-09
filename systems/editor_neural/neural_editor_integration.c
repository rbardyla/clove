/*
 * NEURAL EDITOR INTEGRATION EXAMPLE
 * ==================================
 * Shows how to integrate neural features into the editor hot path
 * while maintaining 60+ FPS performance
 */

#include "handmade_editor_neural.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

// Bottleneck type constants
#define BOTTLENECK_CPU 0
#define BOTTLENECK_GPU 1
#define BOTTLENECK_MEMORY 2
#define BOTTLENECK_BANDWIDTH 3

// Forward declarations for internal functions
void selection_compute_features(void* pred, v3* positions, u32* types, 
                               u32 count, u32 clicked_id);
void generator_encode_scene(void* gen, v3* positions, u32* types, u32 count);
void generator_decode_scene(void* gen);

// Example editor types (would come from main editor)
typedef struct editor_object {
    u32 id;
    v3 position;
    v3 scale;
    u32 type;
    u8 selected;
} editor_object;

typedef struct editor_state {
    editor_object objects[1000];
    u32 object_count;
    
    v3 camera_pos;
    v3 camera_dir;
    f32 camera_speed;
    
    v3 cursor_world_pos;
    u32 placing_object_type;
    
    // Neural system
    editor_neural_system* neural;
    
    // Performance tracking
    u64 frame_start_cycles;
    f32 frame_time_ms;
    u32 frame_count;
} editor_state;

// ============================================================================
// INTEGRATION PATTERN 1: Predictive Object Placement
// ============================================================================

// PERFORMANCE: Called when user enters placement mode - <0.1ms
void handle_placement_mode(editor_state* editor) {
    // Get placement suggestions from neural network
    u32 suggestion_count = 0;
    v3* suggestions = neural_predict_placement(
        editor->neural,
        editor->cursor_world_pos,
        editor->placing_object_type,
        &suggestion_count
    );
    
    // PERFORMANCE: Render ghost objects at suggested positions
    // This runs in the hot path, so we pre-compute once per frame
    for (u32 i = 0; i < suggestion_count; i++) {
        // Draw semi-transparent preview at suggestion[i]
        // Alpha = confidence score
        f32 alpha = editor->neural->placement->confidence_scores[i];
        
        printf("[PLACEMENT] Suggestion %d: (%.1f, %.1f, %.1f) confidence: %.2f\n",
               i, suggestions[i].x, suggestions[i].y, suggestions[i].z, alpha);
    }
    
    // Snap to best suggestion if close enough
    if (suggestion_count > 0) {
        v3 best = suggestions[0];
        f32 dist_sq = (editor->cursor_world_pos.x - best.x) * (editor->cursor_world_pos.x - best.x) +
                      (editor->cursor_world_pos.y - best.y) * (editor->cursor_world_pos.y - best.y) +
                      (editor->cursor_world_pos.z - best.z) * (editor->cursor_world_pos.z - best.z);
        
        if (dist_sq < 4.0f) { // Within 2 units
            // Soft snap - blend toward suggestion
            editor->cursor_world_pos.x = editor->cursor_world_pos.x * 0.7f + best.x * 0.3f;
            editor->cursor_world_pos.y = editor->cursor_world_pos.y * 0.7f + best.y * 0.3f;
            editor->cursor_world_pos.z = editor->cursor_world_pos.z * 0.7f + best.z * 0.3f;
        }
    }
}

// Called when user confirms placement
void confirm_placement(editor_state* editor, v3 final_pos) {
    // Create the object
    if (editor->object_count < 1000) {
        editor->objects[editor->object_count].id = editor->object_count;
        editor->objects[editor->object_count].position = final_pos;
        editor->objects[editor->object_count].type = editor->placing_object_type;
        editor->objects[editor->object_count].scale = (v3){1, 1, 1};
        editor->object_count++;
        
        // Train the neural network with actual placement
        neural_record_placement(editor->neural, final_pos, editor->placing_object_type);
    }
}

// ============================================================================
// INTEGRATION PATTERN 2: Smart Multi-Selection
// ============================================================================

// PERFORMANCE: Predict which objects user wants to select - <0.05ms
void handle_selection_prediction(editor_state* editor, u32 clicked_id) {
    // Prepare object data for neural network
    v3 positions[1000];
    u32 types[1000];
    
    for (u32 i = 0; i < editor->object_count; i++) {
        positions[i] = editor->objects[i].position;
        types[i] = editor->objects[i].type;
    }
    
    // Update selection features
    editor->neural->selection->features.distances[0] = 0; // Reset
    selection_compute_features(
        editor->neural->selection,
        positions,
        types,
        editor->object_count,
        clicked_id
    );
    
    // Get selection predictions
    selection_predictor* sel = editor->neural->selection;
    
    // PERFORMANCE: Only check objects in attention list (spatial culling)
    printf("[SELECTION] Checking %d nearby objects\n", sel->attention_count);
    
    for (u32 i = 0; i < sel->attention_count; i++) {
        u32 obj_idx = sel->attention_list[i];
        f32 score = sel->selection_scores[obj_idx];
        
        if (score > 0.7f) {
            // Highlight as likely selection
            printf("  Object %d: %.1f%% likely\n", obj_idx, score * 100);
        }
    }
}

// ============================================================================
// INTEGRATION PATTERN 3: Procedural Scene Generation
// ============================================================================

// PERFORMANCE: Generate scene layout based on user's style - <1ms
void generate_scene_section(editor_state* editor, f32 density, f32 variation) {
    procedural_generator* gen = editor->neural->generator;
    
    // First, learn from existing scene
    v3 positions[1000];
    u32 types[1000];
    
    for (u32 i = 0; i < editor->object_count; i++) {
        positions[i] = editor->objects[i].position;
        types[i] = editor->objects[i].type;
    }
    
    // Encode current scene into latent space
    generator_encode_scene(gen, positions, types, editor->object_count);
    
    // Set generation parameters
    gen->state.density = density;
    gen->state.variation = variation;
    gen->state.symmetry = 0.5f;
    
    // Generate new objects
    generator_decode_scene(gen);
    
    printf("[GENERATOR] Created %d objects:\n", gen->generated_count);
    
    // Add generated objects to scene
    for (u32 i = 0; i < gen->generated_count && editor->object_count < 1000; i++) {
        editor->objects[editor->object_count].id = editor->object_count;
        editor->objects[editor->object_count].position = gen->generated_positions[i];
        editor->objects[editor->object_count].type = gen->generated_types[i];
        editor->objects[editor->object_count].scale = (v3){1, 1, 1};
        editor->object_count++;
        
        printf("  Type %d at (%.1f, %.1f, %.1f)\n",
               gen->generated_types[i],
               gen->generated_positions[i].x,
               gen->generated_positions[i].y,
               gen->generated_positions[i].z);
    }
}

// ============================================================================
// INTEGRATION PATTERN 4: Performance Prediction
// ============================================================================

// PERFORMANCE: Predict frame time before changes - <0.02ms
void predict_performance_impact(editor_state* editor, u32 objects_to_add) {
    // Compute current scene statistics
    scene_stats stats = {0};
    stats.object_count = editor->object_count + objects_to_add;
    stats.triangle_count = stats.object_count * 12; // Assume cubes
    stats.material_count = 4;
    stats.light_count = 2;
    
    // Compute bounds
    f32 min_x = 1000, max_x = -1000;
    f32 min_y = 1000, max_y = -1000;
    f32 min_z = 1000, max_z = -1000;
    
    for (u32 i = 0; i < editor->object_count; i++) {
        v3 p = editor->objects[i].position;
        if (p.x < min_x) min_x = p.x;
        if (p.x > max_x) max_x = p.x;
        if (p.y < min_y) min_y = p.y;
        if (p.y > max_y) max_y = p.y;
        if (p.z < min_z) min_z = p.z;
        if (p.z > max_z) max_z = p.z;
    }
    
    stats.scene_bounds[0] = min_x;
    stats.scene_bounds[1] = min_y;
    stats.scene_bounds[2] = min_z;
    stats.scene_bounds[3] = max_x;
    stats.scene_bounds[4] = max_y;
    stats.scene_bounds[5] = max_z;
    
    f32 volume = (max_x - min_x) * (max_y - min_y) * (max_z - min_z);
    stats.object_density = stats.object_count / (volume + 0.001f);
    
    // Get prediction
    f32 predicted_ms = neural_predict_frame_time(editor->neural, &stats);
    
    printf("[PERFORMANCE] Predicted frame time: %.2f ms\n", predicted_ms);
    
    if (predicted_ms > 16.67f) {
        printf("  WARNING: May drop below 60 FPS!\n");
        printf("  Bottleneck: %s\n",
               editor->neural->performance->predicted_bottleneck == BOTTLENECK_CPU ? "CPU" :
               editor->neural->performance->predicted_bottleneck == BOTTLENECK_GPU ? "GPU" :
               editor->neural->performance->predicted_bottleneck == BOTTLENECK_MEMORY ? "Memory" :
               "Bandwidth");
    }
    
    // Record actual frame time for training
    neural_record_frame_time(editor->neural, editor->frame_time_ms, &stats);
}

// ============================================================================
// INTEGRATION PATTERN 5: Adaptive LOD
// ============================================================================

// PERFORMANCE: Compute per-object LOD levels - <0.1ms for 1000 objects
void update_lod_levels(editor_state* editor) {
    // Prepare object data
    v3 positions[1000];
    f32 sizes[1000];
    
    for (u32 i = 0; i < editor->object_count; i++) {
        positions[i] = editor->objects[i].position;
        // Compute bounding sphere radius
        sizes[i] = editor->objects[i].scale.x; // Simplified
    }
    
    // Update attention model
    neural_update_attention(
        editor->neural,
        editor->cursor_world_pos, // Focus point
        editor->camera_speed
    );
    
    // Compute LOD levels
    u8* lod_levels = neural_compute_lod_levels(
        editor->neural,
        positions,
        sizes,
        editor->object_count,
        editor->camera_pos,
        editor->camera_dir
    );
    
    // Apply LOD levels
    u32 lod_histogram[8] = {0};
    for (u32 i = 0; i < editor->object_count; i++) {
        // In real implementation, would switch mesh LOD here
        lod_histogram[lod_levels[i]]++;
    }
    
    printf("[LOD] Distribution: ");
    for (u32 i = 0; i < 8; i++) {
        printf("L%d:%d ", i, lod_histogram[i]);
    }
    printf("\n");
    
    // Check prefetch list
    if (editor->neural->lod->prefetch_count > 0) {
        printf("[LOD] Prefetching %d objects for higher detail\n",
               editor->neural->lod->prefetch_count);
    }
}

// ============================================================================
// MAIN FRAME UPDATE - Shows integration in hot path
// ============================================================================

void editor_frame_update(editor_state* editor) {
    u64 frame_start = __rdtsc();
    
    // Reset per-frame neural stats
    editor->neural->inferences_this_frame = 0;
    
    // UPDATE PHASE 1: Input prediction (runs every frame)
    if (editor->placing_object_type != 0) {
        handle_placement_mode(editor);
    }
    
    // UPDATE PHASE 2: LOD update (runs every frame)
    update_lod_levels(editor);
    
    // UPDATE PHASE 3: Performance monitoring (every 10 frames)
    if ((editor->frame_count % 10) == 0) {
        predict_performance_impact(editor, 0);
    }
    
    // UPDATE PHASE 4: Selection prediction (on demand)
    // Would be triggered by mouse hover
    
    // Measure frame time
    u64 frame_end = __rdtsc();
    u64 frame_cycles = frame_end - frame_start;
    
    // Get neural stats
    u64 neural_inference_cycles, neural_training_cycles;
    f32 neural_time_ms;
    neural_get_stats(editor->neural, 
                    &neural_inference_cycles,
                    &neural_training_cycles,
                    &neural_time_ms);
    
    // Assuming 3GHz CPU
    editor->frame_time_ms = frame_cycles / 3000000.0f;
    
    // Print performance every 60 frames
    if ((editor->frame_count % 60) == 0) {
        printf("\n[FRAME %d] Performance:\n", editor->frame_count);
        printf("  Total: %.2f ms\n", editor->frame_time_ms);
        printf("  Neural: %.2f ms (%d inferences)\n",
               neural_time_ms, editor->neural->inferences_this_frame);
        printf("  Neural %%: %.1f%%\n", 
               (neural_time_ms / editor->frame_time_ms) * 100.0f);
        
        if (editor->frame_time_ms > 16.67f) {
            printf("  WARNING: Below 60 FPS!\n");
        }
    }
    
    editor->frame_count++;
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

int main() {
    printf("=== NEURAL EDITOR INTEGRATION DEMO ===\n\n");
    
    // Create editor state
    editor_state editor = {0};
    
    // Initialize neural system with 8MB budget
    editor.neural = neural_editor_create(8);
    if (!editor.neural) {
        printf("Failed to initialize neural system\n");
        return 1;
    }
    
    // Create some initial objects
    for (u32 i = 0; i < 10; i++) {
        editor.objects[i].id = i;
        editor.objects[i].position = (v3){
            (f32)(rand() % 20 - 10),
            0,
            (f32)(rand() % 20 - 10)
        };
        editor.objects[i].type = rand() % 4;
        editor.objects[i].scale = (v3){1, 1, 1};
    }
    editor.object_count = 10;
    
    // Set camera
    editor.camera_pos = (v3){0, 10, 20};
    editor.camera_dir = (v3){0, -0.5f, -0.866f};
    editor.camera_speed = 0.0f;
    
    // Set cursor
    editor.cursor_world_pos = (v3){5, 0, 5};
    editor.placing_object_type = 1; // Placing type 1 objects
    
    printf("Starting neural editor simulation...\n\n");
    
    // Simulate frames
    for (u32 frame = 0; frame < 120; frame++) {
        // Move cursor around
        editor.cursor_world_pos.x = 5.0f + sinf(frame * 0.1f) * 10.0f;
        editor.cursor_world_pos.z = 5.0f + cosf(frame * 0.1f) * 10.0f;
        
        // Vary camera speed
        editor.camera_speed = fabsf(sinf(frame * 0.05f)) * 5.0f;
        
        // Run frame update
        editor_frame_update(&editor);
        
        // Every 30 frames, place an object
        if ((frame % 30) == 29) {
            confirm_placement(&editor, editor.cursor_world_pos);
            printf("\n[FRAME %d] Placed object at (%.1f, %.1f, %.1f)\n\n",
                   frame, editor.cursor_world_pos.x,
                   editor.cursor_world_pos.y,
                   editor.cursor_world_pos.z);
        }
        
        // Every 60 frames, generate some objects
        if ((frame % 60) == 59) {
            printf("\n[FRAME %d] Generating procedural content...\n", frame);
            generate_scene_section(&editor, 0.5f, 0.3f);
            printf("\n");
        }
    }
    
    printf("\n=== SIMULATION COMPLETE ===\n");
    printf("Final object count: %d\n", editor.object_count);
    printf("Average frame time: %.2f ms\n", editor.frame_time_ms);
    
    // Cleanup
    neural_editor_destroy(editor.neural);
    
    return 0;
}