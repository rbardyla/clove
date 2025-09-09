#ifndef HANDMADE_NEURAL_NPC_H
#define HANDMADE_NEURAL_NPC_H

#include "handmade_memory.h"
#include "handmade_entity_soa.h"
#include "handmade_octree.h"
#include <immintrin.h>
#include <math.h>

// Neural network configuration by LOD
typedef enum neural_lod {
    NEURAL_LOD_HERO     = 0,  // Main characters - full network, 60Hz
    NEURAL_LOD_COMPLEX  = 1,  // Important NPCs - medium network, 30Hz  
    NEURAL_LOD_SIMPLE   = 2,  // Background NPCs - small network, 10Hz
    NEURAL_LOD_CROWD    = 3,  // Crowd agents - shared brain, 1Hz
    NEURAL_LOD_COUNT    = 4
} neural_lod;

// Network architecture per LOD
typedef struct neural_config {
    u32 input_size;
    u32 hidden_layers;
    u32 hidden_size;
    u32 output_size;
    u32 update_frequency;  // Updates per second
    f32 activation_threshold;
} neural_config;

static const neural_config g_neural_configs[NEURAL_LOD_COUNT] = {
    [NEURAL_LOD_HERO]    = {128, 3, 64, 32, 60, 0.1f},  // Complex decision making
    [NEURAL_LOD_COMPLEX] = {64,  2, 32, 16, 30, 0.2f},  // Standard NPCs
    [NEURAL_LOD_SIMPLE]  = {32,  1, 16, 8,  10, 0.3f},  // Basic behaviors
    [NEURAL_LOD_CROWD]   = {16,  1, 8,  4,  1,  0.5f}   // Minimal processing
};

// SIMD-aligned weight matrix
typedef struct weight_matrix {
    f32* weights;      // Aligned for AVX2
    f32* biases;       // Aligned for AVX2
    u32 rows;
    u32 cols;
    u32 stride;        // Padded for SIMD
} weight_matrix;

// Neural network instance (shared between multiple NPCs)
typedef struct neural_brain {
    neural_config config;
    weight_matrix* layers;     // Array of weight matrices
    f32* activation_cache;     // Reusable activation buffer
    u32 layer_count;
    u32 ref_count;            // Number of NPCs using this brain
    u64 last_update_frame;
    u32 brain_id;
} neural_brain;

// NPC neural state (per-entity)
typedef struct neural_state {
    u32 brain_id;             // Which brain to use
    neural_lod lod_level;     // Current LOD
    
    // Input sensors (SoA layout)
    f32* visual_input;        // What the NPC sees
    f32* audio_input;         // What the NPC hears  
    f32* spatial_input;       // Position/navigation data
    f32* memory_input;        // Short-term memory
    
    // Output actions (SoA layout)
    f32* movement_output;     // Movement decisions
    f32* action_output;       // Action probabilities
    f32* emotion_output;      // Emotional state
    
    // Temporal coherence
    u64 last_update_frame;
    u32 update_counter;
    f32 decision_confidence;
} neural_state;

// Brain pool for memory efficiency
typedef struct brain_pool {
    neural_brain* brains;
    u32 brain_count;
    u32 brain_capacity;
    
    // Statistics
    u64 inference_count;
    f64 total_inference_time;
    u32 cache_hits;
    u32 cache_misses;
} brain_pool;

// NPC manager (integrates with entity system)
typedef struct neural_npc_system {
    // Memory
    arena* permanent_arena;
    arena* frame_arena;
    
    // Brain management
    brain_pool pools[NEURAL_LOD_COUNT];
    
    // NPC states (SoA)
    neural_state* npc_states;
    u32 npc_count;
    u32 npc_capacity;
    
    // Update scheduling
    u32* update_queues[NEURAL_LOD_COUNT];
    u32 queue_sizes[NEURAL_LOD_COUNT];
    
    // Batch processing buffers
    f32* batch_input_buffer;   // SIMD aligned
    f32* batch_output_buffer;  // SIMD aligned
    u32 batch_size;
    
    // LOD management
    f32 lod_distances[NEURAL_LOD_COUNT];
    v3 camera_position;
    
    // Performance metrics
    u64 frame_number;
    f64 neural_time_ms;
    u32 neurons_processed;
} neural_npc_system;

// Initialize neural NPC system
static neural_npc_system* neural_npc_init(arena* permanent, arena* frame, u32 max_npcs) {
    neural_npc_system* sys = arena_alloc(permanent, sizeof(neural_npc_system));
    sys->permanent_arena = permanent;
    sys->frame_arena = frame;
    sys->npc_capacity = max_npcs;
    
    // Allocate NPC states (SoA)
    sys->npc_states = arena_alloc_array(permanent, neural_state, max_npcs);
    
    // Initialize brain pools for each LOD
    for (u32 lod = 0; lod < NEURAL_LOD_COUNT; lod++) {
        brain_pool* pool = &sys->pools[lod];
        pool->brain_capacity = 32;  // Start with 32 unique brains per LOD
        pool->brains = arena_alloc_array(permanent, neural_brain, pool->brain_capacity);
        
        // Allocate update queues
        sys->update_queues[lod] = arena_alloc_array(permanent, u32, max_npcs);
    }
    
    // Allocate batch processing buffers (SIMD aligned)
    sys->batch_size = 256;  // Process 256 NPCs at once
    sys->batch_input_buffer = arena_alloc_array_aligned(permanent, f32, 
                                                        sys->batch_size * 128, 32);
    sys->batch_output_buffer = arena_alloc_array_aligned(permanent, f32,
                                                         sys->batch_size * 32, 32);
    
    // Set LOD distances
    sys->lod_distances[NEURAL_LOD_HERO] = 10.0f;
    sys->lod_distances[NEURAL_LOD_COMPLEX] = 50.0f;
    sys->lod_distances[NEURAL_LOD_SIMPLE] = 200.0f;
    sys->lod_distances[NEURAL_LOD_CROWD] = 1000.0f;
    
    return sys;
}

// Create or get shared brain
static u32 neural_brain_create(neural_npc_system* sys, neural_lod lod) {
    brain_pool* pool = &sys->pools[lod];
    
    // Find existing brain with capacity or create new
    for (u32 i = 0; i < pool->brain_count; i++) {
        if (pool->brains[i].ref_count < 100) {  // Max 100 NPCs per brain
            pool->brains[i].ref_count++;
            return i;
        }
    }
    
    // Create new brain
    if (pool->brain_count >= pool->brain_capacity) {
        return 0;  // Fallback to first brain
    }
    
    neural_brain* brain = &pool->brains[pool->brain_count];
    brain->config = g_neural_configs[lod];
    brain->brain_id = pool->brain_count;
    brain->layer_count = brain->config.hidden_layers + 1;
    
    // Allocate weight matrices
    brain->layers = arena_alloc_array(sys->permanent_arena, weight_matrix, brain->layer_count);
    
    // Initialize first layer (input -> hidden)
    weight_matrix* layer = &brain->layers[0];
    layer->rows = brain->config.hidden_size;
    layer->cols = brain->config.input_size;
    layer->stride = (layer->cols + 7) & ~7;  // Align to 8 floats for AVX2
    layer->weights = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                               layer->rows * layer->stride, 32);
    layer->biases = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                              layer->rows, 32);
    
    // Initialize hidden layers
    for (u32 i = 1; i < brain->config.hidden_layers; i++) {
        layer = &brain->layers[i];
        layer->rows = brain->config.hidden_size;
        layer->cols = brain->config.hidden_size;
        layer->stride = (layer->cols + 7) & ~7;
        layer->weights = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                                   layer->rows * layer->stride, 32);
        layer->biases = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                                  layer->rows, 32);
    }
    
    // Initialize output layer
    layer = &brain->layers[brain->layer_count - 1];
    layer->rows = brain->config.output_size;
    layer->cols = brain->config.hidden_size;
    layer->stride = (layer->cols + 7) & ~7;
    layer->weights = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                               layer->rows * layer->stride, 32);
    layer->biases = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                              layer->rows, 32);
    
    // Initialize weights with small random values
    for (u32 i = 0; i < brain->layer_count; i++) {
        layer = &brain->layers[i];
        for (u32 j = 0; j < layer->rows * layer->stride; j++) {
            layer->weights[j] = ((f32)rand() / RAND_MAX - 0.5f) * 0.1f;
        }
        for (u32 j = 0; j < layer->rows; j++) {
            layer->biases[j] = 0.0f;
        }
    }
    
    // Allocate activation cache
    u32 max_activations = brain->config.hidden_size * brain->config.hidden_layers;
    brain->activation_cache = arena_alloc_array_aligned(sys->permanent_arena, f32,
                                                        max_activations, 32);
    
    brain->ref_count = 1;
    pool->brain_count++;
    
    return brain->brain_id;
}

// SIMD matrix multiplication for neural inference
static void neural_matrix_multiply_avx2(const f32* input, const weight_matrix* layer,
                                        f32* output) {
    // Process 8 outputs at a time using AVX2
    u32 output_blocks = layer->rows / 8;
    u32 remaining = layer->rows % 8;
    
    // Clear output
    for (u32 i = 0; i < layer->rows; i++) {
        output[i] = layer->biases[i];
    }
    
    // Main SIMD loop
    for (u32 out_block = 0; out_block < output_blocks; out_block++) {
        __m256 acc[8];
        
        // Initialize accumulators with biases
        for (u32 i = 0; i < 8; i++) {
            acc[i] = _mm256_broadcast_ss(&layer->biases[out_block * 8 + i]);
        }
        
        // Compute dot products
        for (u32 in_idx = 0; in_idx < layer->cols; in_idx++) {
            __m256 input_val = _mm256_broadcast_ss(&input[in_idx]);
            
            for (u32 i = 0; i < 8; i++) {
                u32 weight_idx = (out_block * 8 + i) * layer->stride + in_idx;
                __m256 weight_val = _mm256_broadcast_ss(&layer->weights[weight_idx]);
                acc[i] = _mm256_fmadd_ps(input_val, weight_val, acc[i]);
            }
        }
        
        // Store results
        for (u32 i = 0; i < 8; i++) {
            f32 result[8];
            _mm256_store_ps(result, acc[i]);
            output[out_block * 8 + i] = result[0];
        }
    }
    
    // Handle remaining outputs
    for (u32 i = output_blocks * 8; i < layer->rows; i++) {
        f32 sum = layer->biases[i];
        for (u32 j = 0; j < layer->cols; j++) {
            sum += input[j] * layer->weights[i * layer->stride + j];
        }
        output[i] = sum;
    }
}

// ReLU activation (SIMD)
static void neural_relu_avx2(f32* values, u32 count) {
    __m256 zero = _mm256_setzero_ps();
    
    u32 simd_count = count / 8;
    for (u32 i = 0; i < simd_count; i++) {
        __m256 val = _mm256_load_ps(&values[i * 8]);
        val = _mm256_max_ps(val, zero);
        _mm256_store_ps(&values[i * 8], val);
    }
    
    // Handle remaining
    for (u32 i = simd_count * 8; i < count; i++) {
        values[i] = fmaxf(values[i], 0.0f);
    }
}

// Batch neural inference
static void neural_inference_batch(neural_npc_system* sys, neural_brain* brain,
                                   f32* batch_input, f32* batch_output, u32 batch_size) {
    // Process batch through network
    for (u32 batch_idx = 0; batch_idx < batch_size; batch_idx++) {
        f32* input = &batch_input[batch_idx * brain->config.input_size];
        f32* output = &batch_output[batch_idx * brain->config.output_size];
        
        f32* current_input = input;
        f32* current_output = brain->activation_cache;
        
        // Forward pass through layers
        for (u32 layer_idx = 0; layer_idx < brain->layer_count; layer_idx++) {
            weight_matrix* layer = &brain->layers[layer_idx];
            
            // Matrix multiply
            neural_matrix_multiply_avx2(current_input, layer, current_output);
            
            // Apply activation (ReLU for hidden, none for output)
            if (layer_idx < brain->layer_count - 1) {
                neural_relu_avx2(current_output, layer->rows);
                current_input = current_output;
                current_output += layer->rows;  // Next layer's output
            } else {
                // Copy to final output
                for (u32 i = 0; i < brain->config.output_size; i++) {
                    output[i] = current_output[i];
                }
            }
        }
    }
}

// Update NPC LOD based on distance
static neural_lod neural_compute_lod(neural_npc_system* sys, v3 npc_position) {
    f32 dx = npc_position.x - sys->camera_position.x;
    f32 dy = npc_position.y - sys->camera_position.y;
    f32 dz = npc_position.z - sys->camera_position.z;
    f32 dist_sq = dx*dx + dy*dy + dz*dz;
    
    for (u32 lod = 0; lod < NEURAL_LOD_COUNT; lod++) {
        if (dist_sq < sys->lod_distances[lod] * sys->lod_distances[lod]) {
            return (neural_lod)lod;
        }
    }
    
    return NEURAL_LOD_CROWD;
}

// Schedule NPCs for update based on frequency
static void neural_schedule_updates(neural_npc_system* sys, entity_storage* entities) {
    // Clear queues
    for (u32 lod = 0; lod < NEURAL_LOD_COUNT; lod++) {
        sys->queue_sizes[lod] = 0;
    }
    
    // Schedule NPCs based on LOD and update frequency
    for (u32 i = 0; i < sys->npc_count; i++) {
        neural_state* state = &sys->npc_states[i];
        
        // Get NPC position from entity system
        v3 position = {
            entities->transforms.positions_x[i],
            entities->transforms.positions_y[i],
            entities->transforms.positions_z[i]
        };
        
        // Update LOD
        neural_lod new_lod = neural_compute_lod(sys, position);
        if (new_lod != state->lod_level) {
            // LOD changed, switch brain
            sys->pools[state->lod_level].brains[state->brain_id].ref_count--;
            state->brain_id = neural_brain_create(sys, new_lod);
            state->lod_level = new_lod;
        }
        
        // Check if update needed based on frequency
        neural_config* config = &g_neural_configs[state->lod_level];
        u64 frames_per_update = 60 / config->update_frequency;  // Assuming 60 FPS
        
        if (sys->frame_number - state->last_update_frame >= frames_per_update) {
            // Add to update queue
            u32 queue_idx = sys->queue_sizes[state->lod_level]++;
            sys->update_queues[state->lod_level][queue_idx] = i;
            state->last_update_frame = sys->frame_number;
        }
    }
}

// Process neural updates for one LOD
static void neural_process_lod(neural_npc_system* sys, neural_lod lod,
                               entity_storage* entities) {
    brain_pool* pool = &sys->pools[lod];
    u32 queue_size = sys->queue_sizes[lod];
    
    if (queue_size == 0) return;
    
    // Process in batches
    u32 batch_count = (queue_size + sys->batch_size - 1) / sys->batch_size;
    
    for (u32 batch = 0; batch < batch_count; batch++) {
        u32 batch_start = batch * sys->batch_size;
        u32 batch_end = batch_start + sys->batch_size;
        if (batch_end > queue_size) batch_end = queue_size;
        u32 current_batch_size = batch_end - batch_start;
        
        // Group by brain for efficient processing
        for (u32 brain_id = 0; brain_id < pool->brain_count; brain_id++) {
            neural_brain* brain = &pool->brains[brain_id];
            if (brain->ref_count == 0) continue;
            
            u32 brain_batch_count = 0;
            
            // Collect NPCs using this brain
            for (u32 i = batch_start; i < batch_end; i++) {
                u32 npc_idx = sys->update_queues[lod][i];
                neural_state* state = &sys->npc_states[npc_idx];
                
                if (state->brain_id == brain_id) {
                    // Prepare input (gather from sensors)
                    f32* input_ptr = &sys->batch_input_buffer[brain_batch_count * brain->config.input_size];
                    
                    // Visual input (simplified - distance to nearest entities)
                    v3 npc_pos = {
                        entities->transforms.positions_x[npc_idx],
                        entities->transforms.positions_y[npc_idx],
                        entities->transforms.positions_z[npc_idx]
                    };
                    
                    // Fill input buffer (example sensors)
                    for (u32 j = 0; j < brain->config.input_size; j++) {
                        input_ptr[j] = sinf(sys->frame_number * 0.01f + j * 0.1f + npc_idx);
                    }
                    
                    brain_batch_count++;
                }
            }
            
            if (brain_batch_count > 0) {
                // Run batch inference
                neural_inference_batch(sys, brain, sys->batch_input_buffer,
                                      sys->batch_output_buffer, brain_batch_count);
                
                // Apply outputs back to NPCs
                u32 output_idx = 0;
                for (u32 i = batch_start; i < batch_end; i++) {
                    u32 npc_idx = sys->update_queues[lod][i];
                    neural_state* state = &sys->npc_states[npc_idx];
                    
                    if (state->brain_id == brain_id) {
                        f32* output_ptr = &sys->batch_output_buffer[output_idx * brain->config.output_size];
                        
                        // Apply movement decisions
                        f32 move_x = output_ptr[0] * 2.0f - 1.0f;  // [-1, 1]
                        f32 move_z = output_ptr[1] * 2.0f - 1.0f;
                        
                        entities->physics.velocities_x[npc_idx] = move_x * 5.0f;
                        entities->physics.velocities_z[npc_idx] = move_z * 5.0f;
                        
                        // Store decision confidence
                        state->decision_confidence = fabsf(output_ptr[2]);
                        
                        output_idx++;
                    }
                }
                
                // Update statistics
                pool->inference_count += brain_batch_count;
                sys->neurons_processed += brain_batch_count * 
                    (brain->config.input_size * brain->config.hidden_size +
                     brain->config.hidden_size * brain->config.output_size);
            }
        }
    }
}

// Main update function
static void neural_npc_update(neural_npc_system* sys, entity_storage* entities, f32 dt) {
    sys->frame_number++;
    
    u64 start = __rdtsc();
    
    // Schedule updates based on LOD
    neural_schedule_updates(sys, entities);
    
    // Process each LOD level
    for (u32 lod = 0; lod < NEURAL_LOD_COUNT; lod++) {
        neural_process_lod(sys, lod, entities);
    }
    
    u64 end = __rdtsc();
    sys->neural_time_ms = (f64)(end - start) / 2.59e6;  // Assuming 2.59 GHz
}

// Add NPC to system
static u32 neural_npc_add(neural_npc_system* sys, v3 position, neural_lod initial_lod) {
    if (sys->npc_count >= sys->npc_capacity) return UINT32_MAX;
    
    u32 npc_idx = sys->npc_count++;
    neural_state* state = &sys->npc_states[npc_idx];
    
    state->lod_level = initial_lod;
    state->brain_id = neural_brain_create(sys, initial_lod);
    state->last_update_frame = 0;
    state->decision_confidence = 1.0f;
    
    // Allocate sensor inputs
    neural_config* config = &g_neural_configs[initial_lod];
    state->visual_input = arena_alloc_array(sys->permanent_arena, f32, config->input_size / 4);
    state->audio_input = arena_alloc_array(sys->permanent_arena, f32, config->input_size / 4);
    state->spatial_input = arena_alloc_array(sys->permanent_arena, f32, config->input_size / 4);
    state->memory_input = arena_alloc_array(sys->permanent_arena, f32, config->input_size / 4);
    
    // Allocate action outputs
    state->movement_output = arena_alloc_array(sys->permanent_arena, f32, config->output_size / 3);
    state->action_output = arena_alloc_array(sys->permanent_arena, f32, config->output_size / 3);
    state->emotion_output = arena_alloc_array(sys->permanent_arena, f32, config->output_size / 3);
    
    return npc_idx;
}

// Debug statistics
static void neural_npc_print_stats(neural_npc_system* sys) {
    printf("\n=== Neural NPC Statistics ===\n");
    printf("Active NPCs: %u\n", sys->npc_count);
    printf("Frame: %llu\n", (unsigned long long)sys->frame_number);
    printf("Neural Processing: %.3f ms\n", sys->neural_time_ms);
    printf("Neurons Processed: %u\n", sys->neurons_processed);
    
    printf("\nLOD Distribution:\n");
    for (u32 lod = 0; lod < NEURAL_LOD_COUNT; lod++) {
        printf("  LOD %u: %u NPCs (%u Hz updates)\n", 
               lod, sys->queue_sizes[lod], g_neural_configs[lod].update_frequency);
    }
    
    printf("\nBrain Pool Usage:\n");
    for (u32 lod = 0; lod < NEURAL_LOD_COUNT; lod++) {
        brain_pool* pool = &sys->pools[lod];
        printf("  LOD %u: %u brains, %llu inferences\n",
               lod, pool->brain_count, (unsigned long long)pool->inference_count);
    }
    
    f64 neurons_per_ms = sys->neurons_processed / fmax(sys->neural_time_ms, 0.001);
    printf("\nPerformance: %.0f neurons/ms\n", neurons_per_ms);
}

#endif // HANDMADE_NEURAL_NPC_H