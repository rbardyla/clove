/*
 * HANDMADE EDITOR NEURAL SYSTEM
 * ==============================
 * Zero-dependency neural networks for intelligent editor features
 * 
 * PERFORMANCE TARGETS:
 * - Inference: < 0.1ms per prediction
 * - Memory: < 2MB total footprint
 * - Cache misses: < 5% in hot paths
 * - 60+ FPS with continuous predictions
 * 
 * FEATURES:
 * 1. Intelligent object placement prediction
 * 2. Smart selection grouping
 * 3. Procedural content generation
 * 4. Performance prediction
 * 5. Adaptive LOD determination
 */

#ifndef HANDMADE_EDITOR_NEURAL_H
#define HANDMADE_EDITOR_NEURAL_H

#include <stdint.h>
#include <immintrin.h>

// Basic types
typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;

// Math types
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;

// ============================================================================
// NEURAL MEMORY LAYOUT - Cache-coherent Structure of Arrays
// ============================================================================

// PERFORMANCE: All neural data in contiguous memory for cache coherency
typedef struct neural_memory_pool {
    u8* base;           // Base memory address
    u64 size;           // Total size
    u64 used;           // Current usage
    u64 temp_mark;      // Temporary allocation mark
} neural_memory_pool;

// CACHE: Fixed-size weight matrices for predictable memory access
#define MAX_NEURAL_LAYERS 8
#define MAX_NEURONS_PER_LAYER 256
#define SIMD_ALIGNMENT 32  // AVX2 alignment

typedef struct __attribute__((aligned(SIMD_ALIGNMENT))) {
    f32* weights;       // [output_size x input_size] - row major for cache
    f32* biases;        // [output_size]
    f32* outputs;       // [output_size]
    f32* gradients;     // [output_size] - for training
    u32 input_size;
    u32 output_size;
    u32 input_stride;   // Padded for SIMD alignment
    u32 output_stride;  // Padded for SIMD alignment
} neural_layer;

// ============================================================================
// PLACEMENT PREDICTOR - Predicts where users want to place objects
// ============================================================================

typedef struct placement_context {
    // Recent placement history (ring buffer)
    v3 recent_positions[32];
    u32 object_types[32];
    u32 history_index;
    u32 history_count;
    
    // Scene analysis (cached per frame)
    f32 density_map[16][16];     // 2D density grid
    f32 height_map[16][16];       // Average heights
    v3 center_of_mass;
    f32 scene_radius;
    
    // User patterns (learned over time)
    f32 grid_snap_tendency;      // 0-1 how often user snaps to grid
    f32 symmetry_tendency;       // 0-1 how often user places symmetrically
    f32 cluster_tendency;        // 0-1 how often user clusters objects
} placement_context;

typedef struct placement_predictor {
    neural_layer layers[3];       // 3-layer network: input->hidden->output
    placement_context context;
    
    // Input features (32 total)
    f32 input_features[32] __attribute__((aligned(32)));
    
    // Output predictions (8 suggestions)
    v3 predicted_positions[8];
    f32 confidence_scores[8];
    
    // Performance metrics
    u64 prediction_cycles;
    u32 cache_misses;
} placement_predictor;

// ============================================================================
// SELECTION PREDICTOR - ML-based object grouping and selection
// ============================================================================

typedef struct selection_features {
    // Spatial features
    f32 distances[8];         // Distances to nearest objects
    f32 angles[8];           // Angles between objects
    f32 sizes[8];            // Relative sizes
    
    // Visual features  
    f32 colors[12];          // RGB similarity
    f32 types[8];            // Object type similarity
    
    // Temporal features
    f32 selection_history[8]; // Previous selection patterns
    f32 time_since_last[8];  // Time since last selection
} selection_features;

typedef struct selection_predictor {
    neural_layer layers[4];   // Deeper network for complex patterns
    selection_features features;
    
    // Predictions
    f32* selection_scores;    // [MAX_OBJECTS] - probability of selection
    u32* suggested_groups[8]; // Suggested selection groups
    u32 group_sizes[8];
    
    // Attention mechanism - PERFORMANCE: Cache which objects to consider
    u32 attention_list[64];   // Objects likely to be selected next
    u32 attention_count;
} selection_predictor;

// ============================================================================
// PROCEDURAL GENERATOR - Neural scene layout generation
// ============================================================================

typedef struct generator_state {
    // Latent space representation
    f32 latent_vector[64] __attribute__((aligned(32)));
    
    // Generation parameters
    f32 density;
    f32 variation;
    f32 symmetry;
    f32 height_variation;
    
    // Style embeddings learned from user scenes
    f32 style_embedding[32] __attribute__((aligned(32)));
} generator_state;

typedef struct procedural_generator {
    // Encoder network (scene -> latent)
    neural_layer encoder[4];
    
    // Decoder network (latent -> objects)  
    neural_layer decoder[4];
    
    generator_state state;
    
    // Output buffer
    v3* generated_positions;
    u32* generated_types;
    u32 generated_count;
    u32 max_generate;
    
    // Training data (learn from user's scenes)
    f32* scene_embeddings;
    u32 embedding_count;
} procedural_generator;

// ============================================================================
// PERFORMANCE PREDICTOR - Predicts frame time from scene complexity
// ============================================================================

// PERFORMANCE: Compact scene statistics for fast inference
typedef struct scene_stats {
    u32 object_count;
    u32 triangle_count;
    u32 material_count;
    u32 light_count;
    
    f32 overdraw_estimate;
    f32 shadow_complexity;
    f32 transparency_ratio;
    f32 texture_memory_mb;
    
    // Spatial statistics
    f32 scene_bounds[6];      // min/max xyz
    f32 object_density;       // Objects per cubic unit
    f32 depth_complexity;     // Average depth layers
} scene_stats;

typedef struct performance_predictor {
    neural_layer layers[3];
    scene_stats stats;
    
    // Predictions
    f32 predicted_frame_ms;
    f32 predicted_gpu_ms;
    f32 predicted_cpu_ms;
    f32 confidence;
    
    // Historical data for online learning
    f32 actual_frame_times[64];
    scene_stats historical_stats[64];
    u32 history_index;
    
    // Bottleneck analysis
    u32 predicted_bottleneck;  // 0=CPU, 1=GPU, 2=Memory, 3=Bandwidth
} performance_predictor;

// ============================================================================
// ADAPTIVE LOD - Neural LOD selection based on user behavior
// ============================================================================

typedef struct lod_context {
    // Camera behavior patterns
    f32 avg_camera_speed;
    f32 avg_zoom_level;
    f32 focus_stability;      // How stable is user's focus
    
    // User attention model
    v3 attention_point;        // Where user is looking
    f32 attention_radius;      // Area of focus
    f32 attention_duration;    // How long focused
    
    // Performance constraints
    f32 target_frame_ms;
    f32 current_frame_ms;
    f32 performance_headroom;
} lod_context;

typedef struct adaptive_lod {
    neural_layer layers[3];
    lod_context context;
    
    // Per-object LOD decisions
    u8* lod_levels;           // [MAX_OBJECTS] - 0-7 LOD level
    f32* importance_scores;   // [MAX_OBJECTS] - importance for quality
    
    // Global LOD bias
    f32 global_lod_bias;      // -1 to +1, negative = higher quality
    
    // Predictive pre-loading
    u32 prefetch_list[32];    // Objects likely to need higher LOD soon
    u32 prefetch_count;
} adaptive_lod;

// ============================================================================
// MAIN NEURAL EDITOR SYSTEM
// ============================================================================

typedef struct editor_neural_system {
    // Memory management
    neural_memory_pool pool;
    
    // Neural subsystems
    placement_predictor* placement;
    selection_predictor* selection;
    procedural_generator* generator;
    performance_predictor* performance;
    adaptive_lod* lod;
    
    // Shared computation buffers (avoid allocations)
    f32* temp_buffer_a;       // [4096] aligned for SIMD
    f32* temp_buffer_b;       // [4096] aligned for SIMD
    
    // Performance tracking
    u64 total_inference_cycles;
    u64 total_training_cycles;
    u32 inferences_this_frame;
    f32 neural_time_ms;
    
    // Training control
    u32 online_learning_enabled : 1;
    u32 collect_training_data : 1;
    u32 _reserved : 30;
} editor_neural_system;

// ============================================================================
// PUBLIC API - Zero dependencies, full control
// ============================================================================

// Initialize neural system with memory budget
editor_neural_system* neural_editor_create(u64 memory_budget_mb);

// Destroy and free all resources
void neural_editor_destroy(editor_neural_system* sys);

// PLACEMENT PREDICTION
v3* neural_predict_placement(editor_neural_system* sys, 
                            v3 cursor_world_pos,
                            u32 object_type,
                            u32* suggestion_count);

void neural_record_placement(editor_neural_system* sys,
                            v3 actual_position,
                            u32 object_type);

// SELECTION PREDICTION  
u32* neural_predict_selection(editor_neural_system* sys,
                             u32 clicked_object_id,
                             u32* selected_count);

void neural_record_selection(editor_neural_system* sys,
                            u32* selected_ids,
                            u32 count);

// PROCEDURAL GENERATION
void neural_generate_layout(editor_neural_system* sys,
                           f32 density,
                           f32 variation,
                           v3* out_positions,
                           u32* out_types,
                           u32* out_count);

void neural_learn_user_style(editor_neural_system* sys,
                            v3* positions,
                            u32* types,
                            u32 count);

// PERFORMANCE PREDICTION
f32 neural_predict_frame_time(editor_neural_system* sys,
                             scene_stats* stats);

void neural_record_frame_time(editor_neural_system* sys,
                             f32 actual_ms,
                             scene_stats* stats);

// ADAPTIVE LOD
u8* neural_compute_lod_levels(editor_neural_system* sys,
                             v3* object_positions,
                             f32* object_sizes,
                             u32 object_count,
                             v3 camera_pos,
                             v3 camera_dir);

void neural_update_attention(editor_neural_system* sys,
                            v3 focus_point,
                            f32 camera_speed);

// Training and persistence
void neural_train_online(editor_neural_system* sys, f32 learning_rate);
void neural_save_weights(editor_neural_system* sys, const char* path);
void neural_load_weights(editor_neural_system* sys, const char* path);

// Performance monitoring
void neural_get_stats(editor_neural_system* sys,
                     u64* inference_cycles,
                     u64* training_cycles,
                     f32* time_ms);

#endif // HANDMADE_EDITOR_NEURAL_H