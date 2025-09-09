// nodes_executor.c - High-performance node graph execution engine
// PERFORMANCE: Stack-based execution, parallel evaluation, caching
// CACHE: Sequential node access in execution order

#include "handmade_nodes.h"
#include <string.h>
#include <math.h>
#include <immintrin.h>  // For SIMD

// Execution stack for control flow
typedef struct {
    i32 nodes[MAX_STACK_SIZE];
    i32 top;
} execution_stack_t;

// Cache for unchanged paths
typedef struct {
    u64 hash;               // Hash of inputs
    pin_value_t outputs[MAX_PINS_PER_NODE/2];
    i32 output_count;
    u64 last_access_frame;
} execution_cache_entry_t;

typedef struct {
    execution_cache_entry_t entries[1024];  // Fixed size cache
    i32 entry_count;
    u64 current_frame;
    
    // Performance stats
    u64 cache_hits;
    u64 cache_misses;
} execution_cache_t;

// Thread pool for parallel execution
typedef struct {
    i32 thread_count;
    i32 work_queue[MAX_NODES_PER_GRAPH];
    i32 queue_head;
    i32 queue_tail;
    volatile i32 nodes_completed;
} thread_pool_t;

// Global execution state
static struct {
    execution_stack_t stack;
    execution_cache_t cache;
    thread_pool_t thread_pool;
    
    // Dependency tracking for parallel execution
    i32 dependencies[MAX_NODES_PER_GRAPH][MAX_NODES_PER_GRAPH];
    i32 dependency_count[MAX_NODES_PER_GRAPH];
    i32 dependents[MAX_NODES_PER_GRAPH][MAX_NODES_PER_GRAPH];
    i32 dependent_count[MAX_NODES_PER_GRAPH];
    
    // Performance tracking
    u64 total_cycles;
    u64 node_cycles[MAX_NODES_PER_GRAPH];
} g_exec;

// Initialize execution engine
void executor_init(void) {
    memset(&g_exec, 0, sizeof(g_exec));
    g_exec.thread_pool.thread_count = 1;  // Single-threaded for now
}

// Hash inputs for caching
static u64 hash_inputs(node_t *node) {
    // FNV-1a hash
    u64 hash = 14695981039346656037ULL;
    
    for (i32 i = 0; i < node->input_count; ++i) {
        pin_value_t *val = &node->inputs[i].value;
        u8 *bytes = (u8*)val;
        
        for (i32 j = 0; j < sizeof(pin_value_t); ++j) {
            hash ^= bytes[j];
            hash *= 1099511628211ULL;
        }
    }
    
    // Include node type in hash
    hash ^= node->type_id;
    hash *= 1099511628211ULL;
    
    return hash;
}

// Check cache for node results
static bool check_cache(node_t *node) {
    if (!node->type || (node->type->flags & NODE_TYPE_FLAG_PURE) == 0) {
        return false;  // Only cache pure functions
    }
    
    u64 hash = hash_inputs(node);
    
    // Linear search in cache
    for (i32 i = 0; i < g_exec.cache.entry_count; ++i) {
        execution_cache_entry_t *entry = &g_exec.cache.entries[i];
        
        if (entry->hash == hash && entry->output_count == node->output_count) {
            // Cache hit - copy outputs
            for (i32 j = 0; j < node->output_count; ++j) {
                node->outputs[j].value = entry->outputs[j];
            }
            
            entry->last_access_frame = g_exec.cache.current_frame;
            g_exec.cache.cache_hits++;
            return true;
        }
    }
    
    g_exec.cache.cache_misses++;
    return false;
}

// Store node results in cache
static void update_cache(node_t *node) {
    if (!node->type || (node->type->flags & NODE_TYPE_FLAG_PURE) == 0) {
        return;  // Only cache pure functions
    }
    
    u64 hash = hash_inputs(node);
    
    // Find slot to update or add new entry
    execution_cache_entry_t *entry = NULL;
    
    // Look for existing entry
    for (i32 i = 0; i < g_exec.cache.entry_count; ++i) {
        if (g_exec.cache.entries[i].hash == hash) {
            entry = &g_exec.cache.entries[i];
            break;
        }
    }
    
    // Add new entry if not found
    if (!entry && g_exec.cache.entry_count < 1024) {
        entry = &g_exec.cache.entries[g_exec.cache.entry_count++];
    } else if (!entry) {
        // Cache full - evict oldest entry
        u64 oldest_frame = g_exec.cache.current_frame;
        i32 oldest_index = 0;
        
        for (i32 i = 0; i < 1024; ++i) {
            if (g_exec.cache.entries[i].last_access_frame < oldest_frame) {
                oldest_frame = g_exec.cache.entries[i].last_access_frame;
                oldest_index = i;
            }
        }
        
        entry = &g_exec.cache.entries[oldest_index];
    }
    
    // Update entry
    entry->hash = hash;
    entry->output_count = node->output_count;
    for (i32 i = 0; i < node->output_count; ++i) {
        entry->outputs[i] = node->outputs[i].value;
    }
    entry->last_access_frame = g_exec.cache.current_frame;
}

// Build dependency graph for parallel execution
static void build_dependencies(node_graph_t *graph) {
    // Clear dependency info
    memset(g_exec.dependencies, 0, sizeof(g_exec.dependencies));
    memset(g_exec.dependency_count, 0, sizeof(g_exec.dependency_count));
    memset(g_exec.dependents, 0, sizeof(g_exec.dependents));
    memset(g_exec.dependent_count, 0, sizeof(g_exec.dependent_count));
    
    // Build dependency graph from connections
    for (i32 i = 0; i < graph->connection_count; ++i) {
        node_connection_t *conn = &graph->connections[i];
        if (conn->id == 0) continue;
        
        i32 source = conn->source_node;
        i32 target = conn->target_node;
        
        // Target depends on source
        g_exec.dependencies[target][g_exec.dependency_count[target]++] = source;
        
        // Source has target as dependent
        g_exec.dependents[source][g_exec.dependent_count[source]++] = target;
    }
}

// Execute single node with SIMD optimization where possible
static void execute_node_optimized(node_t *node, node_execution_context_t *context) {
    u64 start = ReadCPUTimer();
    
    // Check cache first
    if (check_cache(node)) {
        node->last_execution_cycles = ReadCPUTimer() - start;
        return;
    }
    
    // Transfer input values from connections
    // PERFORMANCE: This is done in the main execution loop now
    
    // Execute based on node type
    if (node->type && node->type->execute) {
        node->type->execute(node, context);
    }
    
    // Update cache with results
    update_cache(node);
    
    node->last_execution_cycles = ReadCPUTimer() - start;
    g_exec.node_cycles[node->id] = node->last_execution_cycles;
}

// Stack-based execution for control flow
static void push_node(i32 node_id) {
    if (g_exec.stack.top < MAX_STACK_SIZE - 1) {
        g_exec.stack.nodes[++g_exec.stack.top] = node_id;
    }
}

static i32 pop_node(void) {
    if (g_exec.stack.top >= 0) {
        return g_exec.stack.nodes[g_exec.stack.top--];
    }
    return -1;
}

// Main execution function - optimized for cache efficiency
void executor_execute_graph(node_graph_t *graph, node_execution_context_t *context) {
    if (!graph || !context) return;
    
    u64 start = ReadCPUTimer();
    
    // Compile if needed
    if (graph->needs_recompile) {
        node_graph_compile(graph);
    }
    
    // Build dependencies for parallel execution
    build_dependencies(graph);
    
    // Increment cache frame
    g_exec.cache.current_frame++;
    
    // Reset stack
    g_exec.stack.top = -1;
    
    // PERFORMANCE: Execute in topological order for best cache usage
    // Nodes are already sorted during compilation
    
    for (i32 i = 0; i < graph->execution_order_count; ++i) {
        i32 node_id = graph->execution_order[i];
        node_t *node = &graph->nodes[node_id];
        
        if (!node->type) continue;
        
        // Handle debug stepping
        if (context->step_mode) {
            if (node->has_breakpoint || context->break_on_next) {
                context->current_node = node_id;
                context->break_on_next = false;
                break;  // Stop execution for debugging
            }
        }
        
        // Transfer values from connected outputs to inputs
        // PERFORMANCE: Do this just before execution for cache locality
        for (i32 j = 0; j < node->input_count; ++j) {
            node_pin_t *input = &node->inputs[j];
            
            // Find connection to this input
            for (i32 k = 0; k < graph->connection_count; ++k) {
                node_connection_t *conn = &graph->connections[k];
                
                if (conn->target_node == node_id && conn->target_pin == j) {
                    node_t *source_node = &graph->nodes[conn->source_node];
                    if (source_node->type && conn->source_pin < source_node->output_count) {
                        // PERFORMANCE: Direct memory copy for value transfer
                        input->value = source_node->outputs[conn->source_pin].value;
                    }
                    break;  // Only one connection per input
                }
            }
        }
        
        // Execute node
        node->state = NODE_STATE_EXECUTING;
        execute_node_optimized(node, context);
        node->state = NODE_STATE_COMPLETED;
        
        node->execution_count++;
        context->nodes_executed++;
    }
    
    // Update performance stats
    g_exec.total_cycles = ReadCPUTimer() - start;
    graph->last_execution_cycles = g_exec.total_cycles;
    graph->last_execution_ms = (f32)(g_exec.total_cycles / 3000000.0);  // Assuming 3GHz
}

// Parallel execution using thread pool (future enhancement)
void executor_execute_parallel(node_graph_t *graph, node_execution_context_t *context) {
    // TODO: Implement parallel execution using thread pool
    // For now, fall back to sequential
    executor_execute_graph(graph, context);
}

// SIMD-optimized value operations
void executor_simd_add_float4(pin_value_t *a, pin_value_t *b, pin_value_t *result) {
    // PERFORMANCE: Use SSE for 4-float operations
    __m128 va = _mm_set_ps(a->v4.w, a->v4.z, a->v4.y, a->v4.x);
    __m128 vb = _mm_set_ps(b->v4.w, b->v4.z, b->v4.y, b->v4.x);
    __m128 vr = _mm_add_ps(va, vb);
    
    _mm_store_ps((float*)&result->v4, vr);
}

void executor_simd_mul_float4(pin_value_t *a, pin_value_t *b, pin_value_t *result) {
    __m128 va = _mm_set_ps(a->v4.w, a->v4.z, a->v4.y, a->v4.x);
    __m128 vb = _mm_set_ps(b->v4.w, b->v4.z, b->v4.y, b->v4.x);
    __m128 vr = _mm_mul_ps(va, vb);
    
    _mm_store_ps((float*)&result->v4, vr);
}

void executor_simd_lerp_float4(pin_value_t *a, pin_value_t *b, f32 t, pin_value_t *result) {
    __m128 va = _mm_set_ps(a->v4.w, a->v4.z, a->v4.y, a->v4.x);
    __m128 vb = _mm_set_ps(b->v4.w, b->v4.z, b->v4.y, b->v4.x);
    __m128 vt = _mm_set1_ps(t);
    __m128 one_minus_t = _mm_set1_ps(1.0f - t);
    
    // result = a * (1-t) + b * t
    __m128 vr = _mm_add_ps(_mm_mul_ps(va, one_minus_t), _mm_mul_ps(vb, vt));
    
    _mm_store_ps((float*)&result->v4, vr);
}

// Matrix operations with SIMD
void executor_simd_mat4_mul(pin_value_t *a, pin_value_t *b, pin_value_t *result) {
    // PERFORMANCE: 4x4 matrix multiplication with SSE
    f32 *ma = a->matrix.m;
    f32 *mb = b->matrix.m;
    f32 *mr = result->matrix.m;
    
    for (i32 i = 0; i < 4; ++i) {
        __m128 row = _mm_setzero_ps();
        
        for (i32 j = 0; j < 4; ++j) {
            __m128 bcast = _mm_set1_ps(ma[i * 4 + j]);
            __m128 col = _mm_load_ps(&mb[j * 4]);
            row = _mm_add_ps(row, _mm_mul_ps(bcast, col));
        }
        
        _mm_store_ps(&mr[i * 4], row);
    }
}

// Batch execution for data flow graphs
void executor_batch_execute(node_graph_t *graph, void **contexts, i32 batch_size) {
    // PERFORMANCE: Process multiple data items through the graph at once
    // This improves cache usage and allows SIMD across items
    
    if (!graph || !contexts || batch_size <= 0) return;
    
    // Compile if needed
    if (graph->needs_recompile) {
        node_graph_compile(graph);
    }
    
    // Process each node for all items in batch
    for (i32 n = 0; n < graph->execution_order_count; ++n) {
        i32 node_id = graph->execution_order[n];
        node_t *node = &graph->nodes[node_id];
        
        if (!node->type || !node->type->execute) continue;
        
        // Execute node for each item in batch
        // CACHE: Process all items for one node before moving to next node
        for (i32 b = 0; b < batch_size; ++b) {
            node_execution_context_t *ctx = (node_execution_context_t*)contexts[b];
            ctx->graph = graph;
            
            // Transfer inputs
            for (i32 j = 0; j < node->input_count; ++j) {
                node_pin_t *input = &node->inputs[j];
                
                for (i32 k = 0; k < graph->connection_count; ++k) {
                    node_connection_t *conn = &graph->connections[k];
                    
                    if (conn->target_node == node_id && conn->target_pin == j) {
                        node_t *source = &graph->nodes[conn->source_node];
                        if (source->type && conn->source_pin < source->output_count) {
                            input->value = source->outputs[conn->source_pin].value;
                        }
                        break;
                    }
                }
            }
            
            // Execute
            node->type->execute(node, ctx);
        }
    }
}

// Debug stepping control
void executor_step_into(node_execution_context_t *context) {
    context->step_mode = true;
    context->break_on_next = true;
}

void executor_step_over(node_execution_context_t *context) {
    context->step_mode = true;
    context->break_on_next = false;
    // Continue execution until next node at same level
}

void executor_continue(node_execution_context_t *context) {
    context->step_mode = false;
    context->break_on_next = false;
}

// Performance profiling
void executor_start_profiling(void) {
    memset(g_exec.node_cycles, 0, sizeof(g_exec.node_cycles));
    g_exec.total_cycles = 0;
}

void executor_stop_profiling(void) {
    // Generate profiling report
    printf("Node Execution Profile:\n");
    printf("Total cycles: %llu\n", g_exec.total_cycles);
    printf("Cache hits: %llu, misses: %llu (%.1f%% hit rate)\n",
           g_exec.cache.cache_hits, g_exec.cache.cache_misses,
           100.0f * g_exec.cache.cache_hits / 
           (g_exec.cache.cache_hits + g_exec.cache.cache_misses));
}

// Clear execution cache
void executor_clear_cache(void) {
    g_exec.cache.entry_count = 0;
    g_exec.cache.cache_hits = 0;
    g_exec.cache.cache_misses = 0;
}

// Get cache statistics
void executor_get_cache_stats(u64 *hits, u64 *misses, i32 *entries) {
    if (hits) *hits = g_exec.cache.cache_hits;
    if (misses) *misses = g_exec.cache.cache_misses;
    if (entries) *entries = g_exec.cache.entry_count;
}