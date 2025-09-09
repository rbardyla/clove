// handmade_nodes.h - Node-based visual programming system
// PERFORMANCE: Zero allocations during execution, cache-friendly layout
// ARCHITECTURE: Data flow with execution pins for control flow

#ifndef HANDMADE_NODES_H
#define HANDMADE_NODES_H

#include "../../src/handmade.h"
#include "../gui/handmade_renderer.h"
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct node_t node_t;
typedef struct node_graph_t node_graph_t;
typedef struct node_pin_t node_pin_t;
typedef struct node_connection_t node_connection_t;

// Node system limits - fixed memory pools
#define MAX_NODES_PER_GRAPH 4096
#define MAX_CONNECTIONS_PER_GRAPH 8192
#define MAX_PINS_PER_NODE 32
#define MAX_NODE_CATEGORIES 32
#define MAX_NODE_TYPES 256
#define MAX_SUBGRAPHS 64
#define MAX_STACK_SIZE 1024
#define MAX_NODE_NAME_LENGTH 64
#define MAX_PIN_NAME_LENGTH 32

// Pin data types
typedef enum {
    PIN_TYPE_EXECUTION,  // Control flow
    PIN_TYPE_BOOL,
    PIN_TYPE_INT,
    PIN_TYPE_FLOAT,
    PIN_TYPE_VECTOR2,
    PIN_TYPE_VECTOR3,
    PIN_TYPE_VECTOR4,
    PIN_TYPE_STRING,
    PIN_TYPE_ENTITY,     // Game entity reference
    PIN_TYPE_OBJECT,     // Generic object pointer
    PIN_TYPE_COLOR,
    PIN_TYPE_MATRIX,
    PIN_TYPE_ARRAY,      // Dynamic array
    PIN_TYPE_ANY,        // Polymorphic
    PIN_TYPE_COUNT
} pin_type_e;

// Pin direction
typedef enum {
    PIN_INPUT,
    PIN_OUTPUT
} pin_direction_e;

// Node categories for organization
typedef enum {
    NODE_CATEGORY_FLOW,
    NODE_CATEGORY_MATH,
    NODE_CATEGORY_LOGIC,
    NODE_CATEGORY_VARIABLE,
    NODE_CATEGORY_EVENT,
    NODE_CATEGORY_GAME,
    NODE_CATEGORY_AI,
    NODE_CATEGORY_DEBUG,
    NODE_CATEGORY_CUSTOM,
    NODE_CATEGORY_COUNT
} node_category_e;

// Node execution state
typedef enum {
    NODE_STATE_IDLE,
    NODE_STATE_EXECUTING,
    NODE_STATE_COMPLETED,
    NODE_STATE_ERROR,
    NODE_STATE_BREAKPOINT
} node_state_e;

// Value storage for pins - union for type safety with zero overhead
typedef union {
    // Execution has no data
    bool b;
    i32 i;
    f32 f;
    struct { f32 x, y; } v2;
    struct { f32 x, y, z; } v3;
    struct { f32 x, y, z, w; } v4;
    struct { u8 r, g, b, a; } color;
    struct { f32 m[16]; } matrix;  // 4x4 matrix
    void *ptr;  // For string, entity, object, array
    u64 raw;    // Raw storage for type punning
} pin_value_t;

// Pin definition
typedef struct node_pin_t {
    char name[MAX_PIN_NAME_LENGTH];
    pin_type_e type;
    pin_direction_e direction;
    pin_value_t value;           // Current value
    pin_value_t default_value;   // Default/reset value
    
    // Visual properties
    u32 color;                   // Pin color based on type
    i32 visual_index;            // Position in node (0, 1, 2...)
    
    // Connection info
    i32 connection_count;        // Number of connections
    i32 connections[8];          // Connection indices (max 8 for outputs)
    
    // Flags
    u32 flags;
    #define PIN_FLAG_HIDDEN      0x01
    #define PIN_FLAG_REQUIRED    0x02
    #define PIN_FLAG_ARRAY       0x04
    #define PIN_FLAG_CONSTANT    0x08
} node_pin_t;

// Node type definition - shared by all instances
typedef struct node_type_t {
    char name[MAX_NODE_NAME_LENGTH];
    char tooltip[256];
    node_category_e category;
    
    // Pin templates
    i32 input_count;
    i32 output_count;
    node_pin_t input_templates[MAX_PINS_PER_NODE/2];
    node_pin_t output_templates[MAX_PINS_PER_NODE/2];
    
    // Visual properties
    u32 color;
    i32 width;
    i32 min_height;
    
    // Execution function
    void (*execute)(struct node_t *node, void *context);
    
    // Optional callbacks
    void (*on_create)(struct node_t *node);
    void (*on_destroy)(struct node_t *node);
    void (*on_property_changed)(struct node_t *node, i32 property_index);
    
    // Property definitions for inspector
    i32 property_count;
    struct {
        char name[32];
        pin_type_e type;
        i32 offset;  // Offset into node->custom_data
    } properties[16];
    
    // Flags
    u32 flags;
    #define NODE_TYPE_FLAG_PURE          0x01  // No side effects
    #define NODE_TYPE_FLAG_COMPACT        0x02  // Minimal visual size
    #define NODE_TYPE_FLAG_NO_DELETE      0x04  // Can't be deleted
    #define NODE_TYPE_FLAG_NO_DUPLICATE   0x08  // Can't be duplicated
    #define NODE_TYPE_FLAG_LATENT         0x10  // Async execution
} node_type_t;

// Node instance
typedef struct node_t {
    i32 id;                      // Unique ID within graph
    i32 type_id;                 // Index into type registry
    node_type_t *type;           // Type definition
    
    // Position and size
    f32 x, y;                    // Position in graph
    i32 width, height;           // Computed size
    
    // Pins (copied from type at creation)
    i32 input_count;
    i32 output_count;
    node_pin_t inputs[MAX_PINS_PER_NODE/2];
    node_pin_t outputs[MAX_PINS_PER_NODE/2];
    
    // Runtime state
    node_state_e state;
    u64 last_execution_cycles;   // Performance tracking
    i32 execution_count;         // How many times executed this frame
    
    // Custom data for node-specific storage
    u8 custom_data[256];        // Node-specific data
    
    // Visual state
    bool selected;
    bool collapsed;
    f32 animation_t;            // For smooth animations
    
    // Debug
    char debug_message[128];
    bool has_breakpoint;
    
    // Cache line padding
    u8 padding[CACHE_LINE_SIZE - ((sizeof(i32)*4 + sizeof(void*) + sizeof(f32)*4 + 
                                   sizeof(node_pin_t)*MAX_PINS_PER_NODE + 
                                   256 + sizeof(bool)*3 + sizeof(f32) + 128 + sizeof(bool)) % CACHE_LINE_SIZE)];
} node_t CACHE_ALIGN;

// Connection between pins
typedef struct node_connection_t {
    i32 id;
    i32 source_node;
    i32 source_pin;
    i32 target_node;
    i32 target_pin;
    
    // Visual properties for rendering
    f32 curve_offset;           // Bezier curve offset
    u32 color;                  // Connection color
    f32 animation_t;            // For animated flow
    bool selected;
    
    // Performance tracking
    u64 last_transfer_cycles;   // Cycles for last value transfer
} node_connection_t;

// Execution context for node evaluation
typedef struct node_execution_context_t {
    node_graph_t *graph;
    void *user_data;            // Game-specific context
    
    // Execution stack for control flow
    i32 stack[MAX_STACK_SIZE];
    i32 stack_top;
    
    // Performance tracking
    u64 start_cycles;
    u64 total_cycles;
    i32 nodes_executed;
    
    // Debug support
    bool step_mode;
    bool break_on_next;
    i32 current_node;
} node_execution_context_t;

// Node graph - contains nodes and connections
typedef struct node_graph_t {
    char name[64];
    i32 id;
    
    // Nodes
    i32 node_count;
    i32 node_capacity;
    node_t *nodes;              // Pool allocated
    i32 free_node_list[MAX_NODES_PER_GRAPH];
    i32 free_node_count;
    
    // Connections
    i32 connection_count;
    i32 connection_capacity;
    node_connection_t *connections;  // Pool allocated
    i32 free_connection_list[MAX_CONNECTIONS_PER_GRAPH];
    i32 free_connection_count;
    
    // Execution order (topologically sorted)
    i32 *execution_order;
    i32 execution_order_count;
    bool needs_recompile;
    
    // View state
    f32 view_x, view_y;         // Camera position
    f32 view_zoom;              // Zoom level
    
    // Selection
    i32 selected_nodes[256];
    i32 selected_count;
    rect selection_rect;
    bool is_selecting;
    
    // Subgraphs
    i32 parent_graph;
    i32 subgraph_ids[MAX_SUBGRAPHS];
    i32 subgraph_count;
    
    // Performance stats
    u64 last_execution_cycles;
    f32 last_execution_ms;
    i32 peak_nodes_per_frame;
    
    // Memory pools
    void *node_pool;
    void *connection_pool;
    memory_index pool_size;
} node_graph_t;

// Node type registry
typedef struct node_registry_t {
    node_type_t types[MAX_NODE_TYPES];
    i32 type_count;
    
    // Category organization
    struct {
        char name[32];
        u32 color;
        i32 type_indices[64];
        i32 type_count;
    } categories[MAX_NODE_CATEGORIES];
} node_registry_t;

// Visual theme for nodes
typedef struct node_theme_t {
    u32 background_color;
    u32 grid_color;
    u32 grid_color_thick;
    u32 selection_color;
    u32 connection_color;
    u32 connection_flow_color;
    u32 node_shadow_color;
    u32 text_color;
    u32 minimap_bg;
    u32 minimap_view;
    
    // Per-type colors
    u32 pin_colors[PIN_TYPE_COUNT];
    u32 category_colors[NODE_CATEGORY_COUNT];
    
    f32 grid_size;
    f32 grid_thick_interval;
    bool show_grid;
    bool show_minimap;
    bool show_performance;
    bool animate_connections;
} node_theme_t;

// Core API
void nodes_init(void *memory_pool, memory_index pool_size);
void nodes_shutdown(void);

// Graph management
node_graph_t* node_graph_create(const char *name);
void node_graph_destroy(node_graph_t *graph);
void node_graph_clear(node_graph_t *graph);
node_graph_t* node_graph_duplicate(node_graph_t *graph);

// Node operations
node_t* node_create(node_graph_t *graph, i32 type_id, f32 x, f32 y);
void node_destroy(node_graph_t *graph, node_t *node);
node_t* node_duplicate(node_graph_t *graph, node_t *node);
node_t* node_find_by_id(node_graph_t *graph, i32 id);

// Connection operations
node_connection_t* node_connect(node_graph_t *graph, 
                                i32 source_node, i32 source_pin,
                                i32 target_node, i32 target_pin);
void node_disconnect(node_graph_t *graph, node_connection_t *connection);
void node_disconnect_all(node_graph_t *graph, node_t *node);
bool node_can_connect(node_graph_t *graph,
                     i32 source_node, i32 source_pin,
                     i32 target_node, i32 target_pin);

// Pin operations
pin_value_t* node_get_input_value(node_t *node, i32 pin_index);
void node_set_output_value(node_t *node, i32 pin_index, pin_value_t *value);
node_pin_t* node_find_pin(node_t *node, const char *name, pin_direction_e direction);

// Execution
void node_graph_execute(node_graph_t *graph, node_execution_context_t *context);
void node_graph_compile(node_graph_t *graph);  // Topological sort
void node_execute_single(node_t *node, node_execution_context_t *context);

// Type registry
void node_register_type(node_type_t *type);
node_type_t* node_find_type(const char *name);
node_type_t* node_get_type_by_id(i32 id);
i32 node_get_type_id(const char *name);

// Serialization
void node_graph_save(node_graph_t *graph, const char *filename);
node_graph_t* node_graph_load(const char *filename);
void node_graph_export_c(node_graph_t *graph, const char *filename);

// Utility functions
const char* pin_type_to_string(pin_type_e type);
u32 pin_type_to_color(pin_type_e type);
const char* node_category_to_string(node_category_e category);

// Performance monitoring
typedef struct {
    u64 total_cycles;
    u64 node_cycles[MAX_NODES_PER_GRAPH];
    i32 node_execution_counts[MAX_NODES_PER_GRAPH];
    f32 frame_ms;
    i32 nodes_executed;
    i32 cache_misses;
} node_performance_stats_t;

void node_get_performance_stats(node_graph_t *graph, node_performance_stats_t *stats);
void node_reset_performance_stats(node_graph_t *graph);

// Default theme
node_theme_t node_default_theme(void);
node_theme_t node_dark_theme(void);

#endif // HANDMADE_NODES_H