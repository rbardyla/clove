// handmade_nodes.c - Core node system implementation
// PERFORMANCE: Fixed memory pools, zero allocations during execution
// CACHE: Node layout optimized for sequential access during execution

#include "handmade_nodes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Global state - single instance for simplicity
static struct {
    node_registry_t registry;
    void *memory_pool;
    memory_index pool_size;
    memory_index pool_used;
    
    // Free lists for graphs
    node_graph_t *graphs[256];
    i32 graph_count;
    
    // Performance tracking
    u64 total_nodes_created;
    u64 total_connections_created;
    u64 total_executions;
} g_nodes;

// Memory allocation from pool
static void* pool_alloc(memory_index size) {
    // PERFORMANCE: Align to cache line for better performance
    size = AlignCacheLine(size);
    
    if (g_nodes.pool_used + size > g_nodes.pool_size) {
        return NULL;  // Out of memory
    }
    
    void *result = (u8*)g_nodes.memory_pool + g_nodes.pool_used;
    g_nodes.pool_used += size;
    
    // Zero the memory
    memset(result, 0, size);
    
    return result;
}

// Initialize node system
void nodes_init(void *memory_pool, memory_index pool_size) {
    memset(&g_nodes, 0, sizeof(g_nodes));
    g_nodes.memory_pool = memory_pool;
    g_nodes.pool_size = pool_size;
    
    // Initialize type registry with built-in colors
    for (i32 i = 0; i < PIN_TYPE_COUNT; ++i) {
        g_nodes.registry.categories[i].color = 0xFF808080;  // Default gray
    }
}

void nodes_shutdown(void) {
    // Nothing to free - using fixed memory pool
    memset(&g_nodes, 0, sizeof(g_nodes));
}

// Graph management
node_graph_t* node_graph_create(const char *name) {
    node_graph_t *graph = (node_graph_t*)pool_alloc(sizeof(node_graph_t));
    if (!graph) return NULL;
    
    strncpy(graph->name, name, 63);
    graph->id = g_nodes.graph_count++;
    
    // Allocate node pool
    graph->node_capacity = MAX_NODES_PER_GRAPH;
    graph->nodes = (node_t*)pool_alloc(sizeof(node_t) * graph->node_capacity);
    if (!graph->nodes) return NULL;
    
    // Allocate connection pool
    graph->connection_capacity = MAX_CONNECTIONS_PER_GRAPH;
    graph->connections = (node_connection_t*)pool_alloc(sizeof(node_connection_t) * graph->connection_capacity);
    if (!graph->connections) return NULL;
    
    // Allocate execution order array
    graph->execution_order = (i32*)pool_alloc(sizeof(i32) * MAX_NODES_PER_GRAPH);
    if (!graph->execution_order) return NULL;
    
    // Initialize free lists
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        graph->free_node_list[i] = MAX_NODES_PER_GRAPH - 1 - i;
    }
    graph->free_node_count = MAX_NODES_PER_GRAPH;
    
    for (i32 i = 0; i < MAX_CONNECTIONS_PER_GRAPH; ++i) {
        graph->free_connection_list[i] = MAX_CONNECTIONS_PER_GRAPH - 1 - i;
    }
    graph->free_connection_count = MAX_CONNECTIONS_PER_GRAPH;
    
    // Default view
    graph->view_zoom = 1.0f;
    graph->needs_recompile = true;
    
    // Add to global list
    g_nodes.graphs[g_nodes.graph_count - 1] = graph;
    
    return graph;
}

void node_graph_destroy(node_graph_t *graph) {
    // Just mark as unused - memory stays in pool
    if (!graph) return;
    
    // Clear all nodes
    for (i32 i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i].type && graph->nodes[i].type->on_destroy) {
            graph->nodes[i].type->on_destroy(&graph->nodes[i]);
        }
    }
    
    // Remove from global list
    for (i32 i = 0; i < g_nodes.graph_count; ++i) {
        if (g_nodes.graphs[i] == graph) {
            g_nodes.graphs[i] = g_nodes.graphs[--g_nodes.graph_count];
            break;
        }
    }
}

void node_graph_clear(node_graph_t *graph) {
    if (!graph) return;
    
    // Reset counts
    graph->node_count = 0;
    graph->connection_count = 0;
    graph->execution_order_count = 0;
    graph->selected_count = 0;
    graph->needs_recompile = true;
    
    // Reset free lists
    graph->free_node_count = MAX_NODES_PER_GRAPH;
    graph->free_connection_count = MAX_CONNECTIONS_PER_GRAPH;
    
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        graph->free_node_list[i] = MAX_NODES_PER_GRAPH - 1 - i;
    }
    
    for (i32 i = 0; i < MAX_CONNECTIONS_PER_GRAPH; ++i) {
        graph->free_connection_list[i] = MAX_CONNECTIONS_PER_GRAPH - 1 - i;
    }
}

// Node operations
node_t* node_create(node_graph_t *graph, i32 type_id, f32 x, f32 y) {
    if (!graph || graph->free_node_count == 0) return NULL;
    if (type_id < 0 || type_id >= g_nodes.registry.type_count) return NULL;
    
    // Get free node slot
    i32 node_index = graph->free_node_list[--graph->free_node_count];
    node_t *node = &graph->nodes[node_index];
    
    // Initialize node
    memset(node, 0, sizeof(node_t));
    node->id = node_index;
    node->type_id = type_id;
    node->type = &g_nodes.registry.types[type_id];
    node->x = x;
    node->y = y;
    
    // Copy pin templates from type
    node->input_count = node->type->input_count;
    node->output_count = node->type->output_count;
    
    for (i32 i = 0; i < node->input_count; ++i) {
        node->inputs[i] = node->type->input_templates[i];
        node->inputs[i].visual_index = i;
    }
    
    for (i32 i = 0; i < node->output_count; ++i) {
        node->outputs[i] = node->type->output_templates[i];
        node->outputs[i].visual_index = i;
    }
    
    // Calculate size
    node->width = node->type->width;
    node->height = node->type->min_height + 
                  Maximum(node->input_count, node->output_count) * 20;
    
    // Call creation callback
    if (node->type->on_create) {
        node->type->on_create(node);
    }
    
    graph->node_count++;
    graph->needs_recompile = true;
    g_nodes.total_nodes_created++;
    
    return node;
}

void node_destroy(node_graph_t *graph, node_t *node) {
    if (!graph || !node) return;
    
    // Disconnect all connections
    node_disconnect_all(graph, node);
    
    // Call destruction callback
    if (node->type && node->type->on_destroy) {
        node->type->on_destroy(node);
    }
    
    // Return to free list
    graph->free_node_list[graph->free_node_count++] = node->id;
    
    // Clear node
    memset(node, 0, sizeof(node_t));
    
    graph->node_count--;
    graph->needs_recompile = true;
}

node_t* node_find_by_id(node_graph_t *graph, i32 id) {
    if (!graph || id < 0 || id >= MAX_NODES_PER_GRAPH) return NULL;
    
    node_t *node = &graph->nodes[id];
    if (node->type == NULL) return NULL;  // Node not in use
    
    return node;
}

// Connection operations
node_connection_t* node_connect(node_graph_t *graph, 
                                i32 source_node_id, i32 source_pin_id,
                                i32 target_node_id, i32 target_pin_id) {
    if (!graph || graph->free_connection_count == 0) return NULL;
    
    // Validate nodes
    node_t *source = node_find_by_id(graph, source_node_id);
    node_t *target = node_find_by_id(graph, target_node_id);
    if (!source || !target) return NULL;
    
    // Validate pins
    if (source_pin_id < 0 || source_pin_id >= source->output_count) return NULL;
    if (target_pin_id < 0 || target_pin_id >= target->input_count) return NULL;
    
    node_pin_t *source_pin = &source->outputs[source_pin_id];
    node_pin_t *target_pin = &target->inputs[target_pin_id];
    
    // Check type compatibility (execution pins always compatible with each other)
    if (source_pin->type != target_pin->type && 
        source_pin->type != PIN_TYPE_ANY && 
        target_pin->type != PIN_TYPE_ANY) {
        // Special case: execution pins
        if (!(source_pin->type == PIN_TYPE_EXECUTION && target_pin->type == PIN_TYPE_EXECUTION)) {
            return NULL;  // Type mismatch
        }
    }
    
    // Check for existing connection to this input
    for (i32 i = 0; i < graph->connection_count; ++i) {
        node_connection_t *conn = &graph->connections[i];
        if (conn->target_node == target_node_id && conn->target_pin == target_pin_id) {
            // Input already connected - disconnect old connection
            node_disconnect(graph, conn);
            break;
        }
    }
    
    // Get free connection slot
    i32 conn_index = graph->free_connection_list[--graph->free_connection_count];
    node_connection_t *connection = &graph->connections[conn_index];
    
    // Initialize connection
    connection->id = conn_index;
    connection->source_node = source_node_id;
    connection->source_pin = source_pin_id;
    connection->target_node = target_node_id;
    connection->target_pin = target_pin_id;
    connection->color = pin_type_to_color(source_pin->type);
    connection->curve_offset = 50.0f;
    
    // Update pin connection info
    if (source_pin->connection_count < 8) {
        source_pin->connections[source_pin->connection_count++] = conn_index;
    }
    if (target_pin->connection_count < 8) {
        target_pin->connections[target_pin->connection_count++] = conn_index;
    }
    
    graph->connection_count++;
    graph->needs_recompile = true;
    g_nodes.total_connections_created++;
    
    return connection;
}

void node_disconnect(node_graph_t *graph, node_connection_t *connection) {
    if (!graph || !connection) return;
    
    // Remove from pin connection lists
    node_t *source = node_find_by_id(graph, connection->source_node);
    node_t *target = node_find_by_id(graph, connection->target_node);
    
    if (source && connection->source_pin < source->output_count) {
        node_pin_t *pin = &source->outputs[connection->source_pin];
        for (i32 i = 0; i < pin->connection_count; ++i) {
            if (pin->connections[i] == connection->id) {
                pin->connections[i] = pin->connections[--pin->connection_count];
                break;
            }
        }
    }
    
    if (target && connection->target_pin < target->input_count) {
        node_pin_t *pin = &target->inputs[connection->target_pin];
        for (i32 i = 0; i < pin->connection_count; ++i) {
            if (pin->connections[i] == connection->id) {
                pin->connections[i] = pin->connections[--pin->connection_count];
                break;
            }
        }
    }
    
    // Return to free list
    graph->free_connection_list[graph->free_connection_count++] = connection->id;
    
    // Clear connection
    memset(connection, 0, sizeof(node_connection_t));
    
    graph->connection_count--;
    graph->needs_recompile = true;
}

void node_disconnect_all(node_graph_t *graph, node_t *node) {
    if (!graph || !node) return;
    
    // Disconnect all input connections
    for (i32 i = 0; i < node->input_count; ++i) {
        node_pin_t *pin = &node->inputs[i];
        while (pin->connection_count > 0) {
            i32 conn_id = pin->connections[0];
            if (conn_id >= 0 && conn_id < MAX_CONNECTIONS_PER_GRAPH) {
                node_disconnect(graph, &graph->connections[conn_id]);
            }
        }
    }
    
    // Disconnect all output connections
    for (i32 i = 0; i < node->output_count; ++i) {
        node_pin_t *pin = &node->outputs[i];
        while (pin->connection_count > 0) {
            i32 conn_id = pin->connections[0];
            if (conn_id >= 0 && conn_id < MAX_CONNECTIONS_PER_GRAPH) {
                node_disconnect(graph, &graph->connections[conn_id]);
            }
        }
    }
}

bool node_can_connect(node_graph_t *graph,
                     i32 source_node_id, i32 source_pin_id,
                     i32 target_node_id, i32 target_pin_id) {
    if (!graph) return false;
    
    // Can't connect to self
    if (source_node_id == target_node_id) return false;
    
    node_t *source = node_find_by_id(graph, source_node_id);
    node_t *target = node_find_by_id(graph, target_node_id);
    if (!source || !target) return false;
    
    if (source_pin_id < 0 || source_pin_id >= source->output_count) return false;
    if (target_pin_id < 0 || target_pin_id >= target->input_count) return false;
    
    node_pin_t *source_pin = &source->outputs[source_pin_id];
    node_pin_t *target_pin = &target->inputs[target_pin_id];
    
    // Check type compatibility
    if (source_pin->type != target_pin->type && 
        source_pin->type != PIN_TYPE_ANY && 
        target_pin->type != PIN_TYPE_ANY) {
        // Special case: execution pins
        if (!(source_pin->type == PIN_TYPE_EXECUTION && target_pin->type == PIN_TYPE_EXECUTION)) {
            return false;
        }
    }
    
    // TODO: Check for cycles
    
    return true;
}

// Pin operations
pin_value_t* node_get_input_value(node_t *node, i32 pin_index) {
    if (!node || pin_index < 0 || pin_index >= node->input_count) return NULL;
    return &node->inputs[pin_index].value;
}

void node_set_output_value(node_t *node, i32 pin_index, pin_value_t *value) {
    if (!node || !value || pin_index < 0 || pin_index >= node->output_count) return;
    node->outputs[pin_index].value = *value;
}

// Graph compilation - topological sort
void node_graph_compile(node_graph_t *graph) {
    if (!graph || !graph->needs_recompile) return;
    
    // PERFORMANCE: Simple topological sort for DAG
    // Reset execution order
    graph->execution_order_count = 0;
    
    // Mark all nodes as unvisited
    bool visited[MAX_NODES_PER_GRAPH] = {0};
    i32 in_degree[MAX_NODES_PER_GRAPH] = {0};
    
    // Calculate in-degrees
    for (i32 i = 0; i < graph->connection_count; ++i) {
        node_connection_t *conn = &graph->connections[i];
        if (conn->target_node >= 0) {
            in_degree[conn->target_node]++;
        }
    }
    
    // Queue for nodes with no dependencies
    i32 queue[MAX_NODES_PER_GRAPH];
    i32 queue_front = 0, queue_back = 0;
    
    // Add all nodes with no inputs to queue
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type && in_degree[i] == 0) {
            queue[queue_back++] = i;
        }
    }
    
    // Process queue
    while (queue_front < queue_back) {
        i32 node_id = queue[queue_front++];
        
        // Add to execution order
        graph->execution_order[graph->execution_order_count++] = node_id;
        
        // Reduce in-degree of connected nodes
        node_t *node = &graph->nodes[node_id];
        for (i32 i = 0; i < node->output_count; ++i) {
            node_pin_t *pin = &node->outputs[i];
            for (i32 j = 0; j < pin->connection_count; ++j) {
                i32 conn_id = pin->connections[j];
                if (conn_id >= 0 && conn_id < MAX_CONNECTIONS_PER_GRAPH) {
                    node_connection_t *conn = &graph->connections[conn_id];
                    if (--in_degree[conn->target_node] == 0) {
                        queue[queue_back++] = conn->target_node;
                    }
                }
            }
        }
    }
    
    graph->needs_recompile = false;
}

// Graph execution
void node_graph_execute(node_graph_t *graph, node_execution_context_t *context) {
    if (!graph || !context) return;
    
    // Compile if needed
    if (graph->needs_recompile) {
        node_graph_compile(graph);
    }
    
    context->graph = graph;
    context->start_cycles = ReadCPUTimer();
    context->nodes_executed = 0;
    
    // PERFORMANCE: Execute in topological order for cache efficiency
    for (i32 i = 0; i < graph->execution_order_count; ++i) {
        i32 node_id = graph->execution_order[i];
        node_t *node = &graph->nodes[node_id];
        
        if (!node->type || !node->type->execute) continue;
        
        // Transfer input values from connected outputs
        for (i32 j = 0; j < node->input_count; ++j) {
            node_pin_t *input_pin = &node->inputs[j];
            
            // Find connection to this input
            for (i32 k = 0; k < graph->connection_count; ++k) {
                node_connection_t *conn = &graph->connections[k];
                if (conn->target_node == node_id && conn->target_pin == j) {
                    // Transfer value from source
                    node_t *source = &graph->nodes[conn->source_node];
                    if (source && conn->source_pin < source->output_count) {
                        input_pin->value = source->outputs[conn->source_pin].value;
                    }
                    break;
                }
            }
        }
        
        // Execute node
        u64 node_start = ReadCPUTimer();
        
        if (context->step_mode && node->has_breakpoint) {
            context->break_on_next = true;
            context->current_node = node_id;
            return;  // Break execution
        }
        
        node->state = NODE_STATE_EXECUTING;
        node->type->execute(node, context);
        node->state = NODE_STATE_COMPLETED;
        
        node->last_execution_cycles = ReadCPUTimer() - node_start;
        node->execution_count++;
        context->nodes_executed++;
    }
    
    context->total_cycles = ReadCPUTimer() - context->start_cycles;
    graph->last_execution_cycles = context->total_cycles;
    
    // Update performance stats
    if (context->nodes_executed > graph->peak_nodes_per_frame) {
        graph->peak_nodes_per_frame = context->nodes_executed;
    }
    
    g_nodes.total_executions++;
}

// Type registry
void node_register_type(node_type_t *type) {
    if (!type || g_nodes.registry.type_count >= MAX_NODE_TYPES) return;
    
    i32 id = g_nodes.registry.type_count++;
    g_nodes.registry.types[id] = *type;
    
    // Add to category
    if (type->category < NODE_CATEGORY_COUNT) {
        i32 cat_index = type->category;
        i32 type_index = g_nodes.registry.categories[cat_index].type_count++;
        g_nodes.registry.categories[cat_index].type_indices[type_index] = id;
    }
}

node_type_t* node_find_type(const char *name) {
    for (i32 i = 0; i < g_nodes.registry.type_count; ++i) {
        if (strcmp(g_nodes.registry.types[i].name, name) == 0) {
            return &g_nodes.registry.types[i];
        }
    }
    return NULL;
}

node_type_t* node_get_type_by_id(i32 id) {
    if (id < 0 || id >= g_nodes.registry.type_count) return NULL;
    return &g_nodes.registry.types[id];
}

i32 node_get_type_id(const char *name) {
    for (i32 i = 0; i < g_nodes.registry.type_count; ++i) {
        if (strcmp(g_nodes.registry.types[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Utility functions
const char* pin_type_to_string(pin_type_e type) {
    static const char *names[] = {
        "Execution", "Bool", "Int", "Float", "Vector2", "Vector3", "Vector4",
        "String", "Entity", "Object", "Color", "Matrix", "Array", "Any"
    };
    if (type >= 0 && type < PIN_TYPE_COUNT) {
        return names[type];
    }
    return "Unknown";
}

u32 pin_type_to_color(pin_type_e type) {
    static const u32 colors[] = {
        0xFFFFFFFF,  // Execution - white
        0xFFFF0000,  // Bool - red
        0xFF00FF00,  // Int - green
        0xFF00FFFF,  // Float - cyan
        0xFFFF00FF,  // Vector2 - magenta
        0xFFFF80FF,  // Vector3 - light magenta
        0xFFFFFF00,  // Vector4 - yellow
        0xFF8080FF,  // String - light blue
        0xFFFF8000,  // Entity - orange
        0xFF808080,  // Object - gray
        0xFF0080FF,  // Color - blue
        0xFF80FF80,  // Matrix - light green
        0xFFFF80FF,  // Array - pink
        0xFFFFFFFF,  // Any - white
    };
    if (type >= 0 && type < PIN_TYPE_COUNT) {
        return colors[type];
    }
    return 0xFF808080;  // Default gray
}

const char* node_category_to_string(node_category_e category) {
    static const char *names[] = {
        "Flow Control", "Math", "Logic", "Variables", "Events", 
        "Game", "AI", "Debug", "Custom"
    };
    if (category >= 0 && category < NODE_CATEGORY_COUNT) {
        return names[category];
    }
    return "Unknown";
}

// Performance monitoring
void node_get_performance_stats(node_graph_t *graph, node_performance_stats_t *stats) {
    if (!graph || !stats) return;
    
    memset(stats, 0, sizeof(node_performance_stats_t));
    stats->total_cycles = graph->last_execution_cycles;
    stats->nodes_executed = graph->execution_order_count;
    
    // Assuming 3GHz CPU for ms conversion
    stats->frame_ms = (f32)(stats->total_cycles / 3000000.0);
    
    // Collect per-node stats
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            stats->node_cycles[i] = graph->nodes[i].last_execution_cycles;
            stats->node_execution_counts[i] = graph->nodes[i].execution_count;
        }
    }
}

void node_reset_performance_stats(node_graph_t *graph) {
    if (!graph) return;
    
    graph->last_execution_cycles = 0;
    graph->peak_nodes_per_frame = 0;
    
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            graph->nodes[i].last_execution_cycles = 0;
            graph->nodes[i].execution_count = 0;
        }
    }
}

// Default themes
node_theme_t node_default_theme(void) {
    node_theme_t theme = {
        .background_color = 0xFF202020,
        .grid_color = 0xFF303030,
        .grid_color_thick = 0xFF404040,
        .selection_color = 0x80FFFF00,
        .connection_color = 0xFFAAAAAA,
        .connection_flow_color = 0xFFFFFF00,
        .node_shadow_color = 0x80000000,
        .text_color = 0xFFFFFFFF,
        .minimap_bg = 0x80000000,
        .minimap_view = 0x80FFFF00,
        .grid_size = 20.0f,
        .grid_thick_interval = 5.0f,
        .show_grid = true,
        .show_minimap = true,
        .show_performance = false,
        .animate_connections = true
    };
    
    // Pin colors
    for (i32 i = 0; i < PIN_TYPE_COUNT; ++i) {
        theme.pin_colors[i] = pin_type_to_color(i);
    }
    
    // Category colors
    theme.category_colors[NODE_CATEGORY_FLOW] = 0xFF404080;
    theme.category_colors[NODE_CATEGORY_MATH] = 0xFF408040;
    theme.category_colors[NODE_CATEGORY_LOGIC] = 0xFF804040;
    theme.category_colors[NODE_CATEGORY_VARIABLE] = 0xFF808040;
    theme.category_colors[NODE_CATEGORY_EVENT] = 0xFF804080;
    theme.category_colors[NODE_CATEGORY_GAME] = 0xFF408080;
    theme.category_colors[NODE_CATEGORY_AI] = 0xFF606060;
    theme.category_colors[NODE_CATEGORY_DEBUG] = 0xFF800080;
    theme.category_colors[NODE_CATEGORY_CUSTOM] = 0xFF404040;
    
    return theme;
}

node_theme_t node_dark_theme(void) {
    node_theme_t theme = node_default_theme();
    theme.background_color = 0xFF101010;
    theme.grid_color = 0xFF202020;
    theme.grid_color_thick = 0xFF303030;
    return theme;
}