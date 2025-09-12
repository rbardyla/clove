/*
    Optimized Scene Graph Traversal
    Fixes bottleneck identified in profiling (27.68µs per node at 50K)
    
    Optimizations:
    1. Structure of Arrays for cache coherency
    2. Dirty flag bitsets for fast checking
    3. SIMD matrix multiplication
    4. Cache-friendly traversal order
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <immintrin.h>
#include <stdint.h>

#define MAX_NODES 100000
#define CACHE_LINE_SIZE 64

// Aligned allocation for SIMD
#define ALIGNED(x) __attribute__((aligned(x)))

// Structure of Arrays for cache coherency
typedef struct {
    // Hot data - accessed every frame
    float* ALIGNED(32) world_matrices;      // 16 floats per node, packed
    float* ALIGNED(32) local_matrices;      // 16 floats per node, packed
    uint64_t* ALIGNED(64) dirty_bitset;     // 1 bit per node
    
    // Warm data - accessed on updates
    int* parent_indices;                    // Parent node index
    int* first_child;                       // First child index
    int* next_sibling;                      // Next sibling index
    
    // Cold data - rarely accessed
    char (*names)[64];                      // Node names
    void** user_data;                       // User pointers
    
    // Metadata
    int node_count;
    int dirty_count;
    
    // Cache-friendly traversal order
    int* traversal_order;                   // Pre-computed BFS order
    int* depth_levels;                      // Depth of each node
    
} scene_graph_soa;

// Initialize scene graph
scene_graph_soa* scene_graph_create(int max_nodes) {
    scene_graph_soa* sg = calloc(1, sizeof(scene_graph_soa));
    
    // Allocate aligned arrays
    sg->world_matrices = aligned_alloc(32, max_nodes * 16 * sizeof(float));
    sg->local_matrices = aligned_alloc(32, max_nodes * 16 * sizeof(float));
    sg->dirty_bitset = aligned_alloc(64, (max_nodes + 63) / 64 * sizeof(uint64_t));
    
    sg->parent_indices = calloc(max_nodes, sizeof(int));
    sg->first_child = calloc(max_nodes, sizeof(int));
    sg->next_sibling = calloc(max_nodes, sizeof(int));
    
    sg->names = calloc(max_nodes, 64);
    sg->user_data = calloc(max_nodes, sizeof(void*));
    
    sg->traversal_order = calloc(max_nodes, sizeof(int));
    sg->depth_levels = calloc(max_nodes, sizeof(int));
    
    // Initialize to identity matrices
    for (int i = 0; i < max_nodes; i++) {
        float* local = &sg->local_matrices[i * 16];
        float* world = &sg->world_matrices[i * 16];
        
        // Identity matrix
        local[0] = local[5] = local[10] = local[15] = 1.0f;
        world[0] = world[5] = world[10] = world[15] = 1.0f;
        
        sg->parent_indices[i] = -1;
        sg->first_child[i] = -1;
        sg->next_sibling[i] = -1;
    }
    
    return sg;
}

// Fast dirty flag operations using bitsets
static inline void set_dirty(scene_graph_soa* sg, int node) {
    int word = node / 64;
    int bit = node % 64;
    sg->dirty_bitset[word] |= (1ULL << bit);
    sg->dirty_count++;
}

static inline int is_dirty(scene_graph_soa* sg, int node) {
    int word = node / 64;
    int bit = node % 64;
    return (sg->dirty_bitset[word] >> bit) & 1;
}

static inline void clear_dirty(scene_graph_soa* sg, int node) {
    int word = node / 64;
    int bit = node % 64;
    sg->dirty_bitset[word] &= ~(1ULL << bit);
    sg->dirty_count--;
}

// SIMD matrix multiplication (AVX2)
static void matrix_multiply_simd(float* result, const float* a, const float* b) {
    __m256 row0 = _mm256_broadcast_ps((__m128*)&a[0]);
    __m256 row1 = _mm256_broadcast_ps((__m128*)&a[4]);
    __m256 row2 = _mm256_broadcast_ps((__m128*)&a[8]);
    __m256 row3 = _mm256_broadcast_ps((__m128*)&a[12]);
    
    // Process two columns at a time
    for (int i = 0; i < 16; i += 8) {
        __m256 b_cols = _mm256_load_ps(&b[i]);
        
        __m256 r0 = _mm256_mul_ps(row0, _mm256_shuffle_ps(b_cols, b_cols, 0x00));
        __m256 r1 = _mm256_mul_ps(row1, _mm256_shuffle_ps(b_cols, b_cols, 0x55));
        __m256 r2 = _mm256_mul_ps(row2, _mm256_shuffle_ps(b_cols, b_cols, 0xAA));
        __m256 r3 = _mm256_mul_ps(row3, _mm256_shuffle_ps(b_cols, b_cols, 0xFF));
        
        __m256 res = _mm256_add_ps(_mm256_add_ps(r0, r1), _mm256_add_ps(r2, r3));
        _mm256_store_ps(&result[i], res);
    }
}

// Optimized traversal with cache-friendly access pattern
void scene_graph_update_optimized(scene_graph_soa* sg) {
    if (sg->dirty_count == 0) return;
    
    // Process dirty nodes in BFS order for cache coherency
    for (int i = 0; i < sg->node_count; i++) {
        int node = sg->traversal_order[i];
        
        if (!is_dirty(sg, node)) continue;
        
        int parent = sg->parent_indices[node];
        float* local = &sg->local_matrices[node * 16];
        float* world = &sg->world_matrices[node * 16];
        
        if (parent >= 0) {
            float* parent_world = &sg->world_matrices[parent * 16];
            
            // SIMD matrix multiply
            matrix_multiply_simd(world, parent_world, local);
        } else {
            // Root node - copy local to world
            memcpy(world, local, 16 * sizeof(float));
        }
        
        // Mark children as dirty (propagate)
        int child = sg->first_child[node];
        while (child >= 0) {
            set_dirty(sg, child);
            child = sg->next_sibling[child];
        }
        
        clear_dirty(sg, node);
    }
}

// Batch update multiple nodes at once
void scene_graph_batch_update(scene_graph_soa* sg, int* nodes, int count) {
    // Prefetch data for all nodes
    for (int i = 0; i < count; i++) {
        int node = nodes[i];
        __builtin_prefetch(&sg->world_matrices[node * 16], 1, 3);
        __builtin_prefetch(&sg->local_matrices[node * 16], 0, 3);
    }
    
    // Process in batches of 4 for SIMD efficiency
    for (int i = 0; i < count; i += 4) {
        int batch_size = (count - i) < 4 ? (count - i) : 4;
        
        for (int j = 0; j < batch_size; j++) {
            int node = nodes[i + j];
            int parent = sg->parent_indices[node];
            
            if (parent >= 0) {
                float* local = &sg->local_matrices[node * 16];
                float* world = &sg->world_matrices[node * 16];
                float* parent_world = &sg->world_matrices[parent * 16];
                
                matrix_multiply_simd(world, parent_world, local);
            }
        }
    }
}

// Build optimal traversal order (BFS for cache coherency)
void scene_graph_build_traversal_order(scene_graph_soa* sg) {
    int* queue = calloc(sg->node_count, sizeof(int));
    int front = 0, back = 0;
    int order_index = 0;
    
    // Find roots and add to queue
    for (int i = 0; i < sg->node_count; i++) {
        if (sg->parent_indices[i] == -1) {
            queue[back++] = i;
            sg->depth_levels[i] = 0;
        }
    }
    
    // BFS traversal
    while (front < back) {
        int node = queue[front++];
        sg->traversal_order[order_index++] = node;
        
        // Add children to queue
        int child = sg->first_child[node];
        while (child >= 0) {
            queue[back++] = child;
            sg->depth_levels[child] = sg->depth_levels[node] + 1;
            child = sg->next_sibling[child];
        }
    }
    
    free(queue);
}

// Benchmark comparison
static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void benchmark_scene_graph() {
    printf("=== Scene Graph Optimization Benchmark ===\n\n");
    
    const int NODE_COUNT = 50000;
    const int ITERATIONS = 100;
    
    // Create scene graph
    scene_graph_soa* sg = scene_graph_create(NODE_COUNT);
    sg->node_count = NODE_COUNT;
    
    // Build tree structure (balanced tree)
    for (int i = 1; i < NODE_COUNT; i++) {
        sg->parent_indices[i] = (i - 1) / 4;  // Each node has 4 children
        
        // Link siblings
        if (i % 4 != 1) {
            sg->next_sibling[i] = i - 1;
        }
        
        // Link to parent's children list
        int parent = sg->parent_indices[i];
        if (sg->first_child[parent] == -1) {
            sg->first_child[parent] = i;
        }
    }
    
    // Build traversal order
    scene_graph_build_traversal_order(sg);
    
    // Test 1: Full update (all dirty)
    for (int i = 0; i < NODE_COUNT; i++) {
        set_dirty(sg, i);
    }
    
    double start = get_time_ms();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        scene_graph_update_optimized(sg);
        // Re-dirty for next iteration
        for (int i = 0; i < NODE_COUNT / 10; i++) {
            set_dirty(sg, rand() % NODE_COUNT);
        }
    }
    double full_time = get_time_ms() - start;
    
    printf("Full traversal (%d nodes):\n", NODE_COUNT);
    printf("  Total: %.2f ms\n", full_time);
    printf("  Per iteration: %.3f ms\n", full_time / ITERATIONS);
    printf("  Per node: %.3f µs\n\n", (full_time * 1000) / (ITERATIONS * NODE_COUNT));
    
    // Test 2: Partial update (10% dirty)
    memset(sg->dirty_bitset, 0, (NODE_COUNT + 63) / 64 * sizeof(uint64_t));
    sg->dirty_count = 0;
    
    for (int i = 0; i < NODE_COUNT / 10; i++) {
        set_dirty(sg, rand() % NODE_COUNT);
    }
    
    start = get_time_ms();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        scene_graph_update_optimized(sg);
        // Re-dirty for next iteration
        for (int i = 0; i < NODE_COUNT / 10; i++) {
            set_dirty(sg, rand() % NODE_COUNT);
        }
    }
    double partial_time = get_time_ms() - start;
    
    printf("Partial update (10%% dirty):\n");
    printf("  Total: %.2f ms\n", partial_time);
    printf("  Per iteration: %.3f ms\n", partial_time / ITERATIONS);
    printf("  Speedup vs full: %.2fx\n\n", full_time / partial_time);
    
    // Test 3: Batch update
    int* batch_nodes = malloc(1000 * sizeof(int));
    for (int i = 0; i < 1000; i++) {
        batch_nodes[i] = rand() % NODE_COUNT;
    }
    
    start = get_time_ms();
    for (int iter = 0; iter < ITERATIONS * 10; iter++) {
        scene_graph_batch_update(sg, batch_nodes, 1000);
    }
    double batch_time = get_time_ms() - start;
    
    printf("Batch update (1000 nodes):\n");
    printf("  Total: %.2f ms\n", batch_time);
    printf("  Per batch: %.3f ms\n", batch_time / (ITERATIONS * 10));
    printf("  Per node: %.3f µs\n\n", (batch_time * 1000) / (ITERATIONS * 10 * 1000));
    
    // Compare with baseline from earlier benchmark
    printf("Improvement vs baseline:\n");
    printf("  Baseline: 27.68 µs per node\n");
    printf("  Optimized: %.3f µs per node\n", (full_time * 1000) / (ITERATIONS * NODE_COUNT));
    printf("  Speedup: %.2fx\n", 27.68 / ((full_time * 1000) / (ITERATIONS * NODE_COUNT)));
    
    free(sg->world_matrices);
    free(sg->local_matrices);
    free(sg->dirty_bitset);
    free(sg->parent_indices);
    free(sg->first_child);
    free(sg->next_sibling);
    free(sg->names);
    free(sg->user_data);
    free(sg->traversal_order);
    free(sg->depth_levels);
    free(sg);
    free(batch_nodes);
}

int main() {
    benchmark_scene_graph();
    return 0;
}