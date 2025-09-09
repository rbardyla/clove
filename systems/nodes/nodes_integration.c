// nodes_integration.c - Engine integration for node system
// FEATURES: Script binding, hot reload, save/load, C code export

#include "handmade_nodes.h"
#include "../script/handmade_script.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// SCRIPT SYSTEM INTEGRATION
// =============================================================================

// Expose node graph to script system
void bind_node_graph_to_script(node_graph_t *graph, void *script_context) {
    // Register node graph as script object
    // This would integrate with your existing script system
    
    // Example bindings:
    // script_register_function("CreateNode", script_create_node);
    // script_register_function("ConnectNodes", script_connect_nodes);
    // script_register_function("ExecuteGraph", script_execute_graph);
}

// Script function to create node
static void* script_create_node(void *context, const char *type_name, f32 x, f32 y) {
    node_graph_t *graph = (node_graph_t*)context;
    i32 type_id = node_get_type_id(type_name);
    if (type_id >= 0) {
        return node_create(graph, type_id, x, y);
    }
    return NULL;
}

// Script function to connect nodes
static bool script_connect_nodes(void *context, void *source_node, i32 source_pin,
                                 void *target_node, i32 target_pin) {
    node_graph_t *graph = (node_graph_t*)context;
    node_t *source = (node_t*)source_node;
    node_t *target = (node_t*)target_node;
    
    if (source && target) {
        return node_connect(graph, source->id, source_pin, target->id, target_pin) != NULL;
    }
    return false;
}

// =============================================================================
// HOT RELOAD SUPPORT
// =============================================================================

typedef struct {
    node_graph_t *graphs[32];
    i32 graph_count;
    u64 last_modified[32];
    char filenames[32][256];
} hot_reload_state_t;

static hot_reload_state_t g_hot_reload;

// Register graph for hot reload
void register_graph_hot_reload(node_graph_t *graph, const char *filename) {
    if (g_hot_reload.graph_count >= 32) return;
    
    g_hot_reload.graphs[g_hot_reload.graph_count] = graph;
    strncpy(g_hot_reload.filenames[g_hot_reload.graph_count], filename, 255);
    
    // Get initial modification time
    // Platform-specific file stat would go here
    g_hot_reload.last_modified[g_hot_reload.graph_count] = 0;
    
    g_hot_reload.graph_count++;
}

// Check and reload modified graphs
void check_graph_hot_reload(void) {
    for (i32 i = 0; i < g_hot_reload.graph_count; ++i) {
        // Check file modification time
        // Platform-specific implementation
        u64 current_modified = 0;  // Would get actual file time
        
        if (current_modified > g_hot_reload.last_modified[i]) {
            // Reload graph
            node_graph_t *new_graph = node_graph_load(g_hot_reload.filenames[i]);
            if (new_graph) {
                // Preserve runtime state
                node_graph_t *old_graph = g_hot_reload.graphs[i];
                
                // Copy execution counts and performance data
                for (i32 j = 0; j < MAX_NODES_PER_GRAPH; ++j) {
                    if (old_graph->nodes[j].type && new_graph->nodes[j].type) {
                        new_graph->nodes[j].execution_count = old_graph->nodes[j].execution_count;
                        new_graph->nodes[j].last_execution_cycles = old_graph->nodes[j].last_execution_cycles;
                    }
                }
                
                // Replace graph
                *old_graph = *new_graph;
                g_hot_reload.last_modified[i] = current_modified;
            }
        }
    }
}

// =============================================================================
// SAVE/LOAD SYSTEM
// =============================================================================

// File format version
#define NODE_FILE_VERSION 1
#define NODE_FILE_MAGIC 0x4E4F4445  // "NODE"

typedef struct {
    u32 magic;
    u32 version;
    u32 node_count;
    u32 connection_count;
    char name[64];
} node_file_header_t;

// Save graph to file
void node_graph_save(node_graph_t *graph, const char *filename) {
    if (!graph || !filename) return;
    
    FILE *file = fopen(filename, "wb");
    if (!file) return;
    
    // Write header
    node_file_header_t header = {0};
    header.magic = NODE_FILE_MAGIC;
    header.version = NODE_FILE_VERSION;
    header.node_count = graph->node_count;
    header.connection_count = graph->connection_count;
    strncpy(header.name, graph->name, 63);
    
    fwrite(&header, sizeof(header), 1, file);
    
    // Write nodes
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            node_t *node = &graph->nodes[i];
            
            // Write node data
            fwrite(&node->id, sizeof(i32), 1, file);
            fwrite(&node->type_id, sizeof(i32), 1, file);
            
            // Write type name for validation
            char type_name[64] = {0};
            strncpy(type_name, node->type->name, 63);
            fwrite(type_name, 64, 1, file);
            
            fwrite(&node->x, sizeof(f32), 1, file);
            fwrite(&node->y, sizeof(f32), 1, file);
            fwrite(&node->width, sizeof(i32), 1, file);
            fwrite(&node->height, sizeof(i32), 1, file);
            
            // Write custom data
            fwrite(node->custom_data, sizeof(node->custom_data), 1, file);
            
            // Write pin values
            for (i32 j = 0; j < node->input_count; ++j) {
                fwrite(&node->inputs[j].value, sizeof(pin_value_t), 1, file);
                fwrite(&node->inputs[j].default_value, sizeof(pin_value_t), 1, file);
            }
            
            for (i32 j = 0; j < node->output_count; ++j) {
                fwrite(&node->outputs[j].value, sizeof(pin_value_t), 1, file);
                fwrite(&node->outputs[j].default_value, sizeof(pin_value_t), 1, file);
            }
        }
    }
    
    // Write connections
    for (i32 i = 0; i < graph->connection_count; ++i) {
        node_connection_t *conn = &graph->connections[i];
        if (conn->id != 0) {
            fwrite(&conn->source_node, sizeof(i32), 1, file);
            fwrite(&conn->source_pin, sizeof(i32), 1, file);
            fwrite(&conn->target_node, sizeof(i32), 1, file);
            fwrite(&conn->target_pin, sizeof(i32), 1, file);
        }
    }
    
    // Write view state
    fwrite(&graph->view_x, sizeof(f32), 1, file);
    fwrite(&graph->view_y, sizeof(f32), 1, file);
    fwrite(&graph->view_zoom, sizeof(f32), 1, file);
    
    fclose(file);
}

// Load graph from file
node_graph_t* node_graph_load(const char *filename) {
    if (!filename) return NULL;
    
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;
    
    // Read header
    node_file_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return NULL;
    }
    
    // Validate header
    if (header.magic != NODE_FILE_MAGIC || header.version != NODE_FILE_VERSION) {
        fclose(file);
        return NULL;
    }
    
    // Create graph
    node_graph_t *graph = node_graph_create(header.name);
    if (!graph) {
        fclose(file);
        return NULL;
    }
    
    // Read nodes
    for (u32 i = 0; i < header.node_count; ++i) {
        i32 node_id, type_id;
        char type_name[64];
        f32 x, y;
        i32 width, height;
        
        fread(&node_id, sizeof(i32), 1, file);
        fread(&type_id, sizeof(i32), 1, file);
        fread(type_name, 64, 1, file);
        fread(&x, sizeof(f32), 1, file);
        fread(&y, sizeof(f32), 1, file);
        fread(&width, sizeof(i32), 1, file);
        fread(&height, sizeof(i32), 1, file);
        
        // Find type by name (in case IDs changed)
        i32 actual_type_id = node_get_type_id(type_name);
        if (actual_type_id < 0) {
            // Type not found, use original ID
            actual_type_id = type_id;
        }
        
        // Create node
        node_t *node = node_create(graph, actual_type_id, x, y);
        if (node) {
            node->width = width;
            node->height = height;
            
            // Read custom data
            fread(node->custom_data, sizeof(node->custom_data), 1, file);
            
            // Read pin values
            for (i32 j = 0; j < node->input_count; ++j) {
                fread(&node->inputs[j].value, sizeof(pin_value_t), 1, file);
                fread(&node->inputs[j].default_value, sizeof(pin_value_t), 1, file);
            }
            
            for (i32 j = 0; j < node->output_count; ++j) {
                fread(&node->outputs[j].value, sizeof(pin_value_t), 1, file);
                fread(&node->outputs[j].default_value, sizeof(pin_value_t), 1, file);
            }
        }
    }
    
    // Read connections
    for (u32 i = 0; i < header.connection_count; ++i) {
        i32 source_node, source_pin, target_node, target_pin;
        fread(&source_node, sizeof(i32), 1, file);
        fread(&source_pin, sizeof(i32), 1, file);
        fread(&target_node, sizeof(i32), 1, file);
        fread(&target_pin, sizeof(i32), 1, file);
        
        node_connect(graph, source_node, source_pin, target_node, target_pin);
    }
    
    // Read view state
    fread(&graph->view_x, sizeof(f32), 1, file);
    fread(&graph->view_y, sizeof(f32), 1, file);
    fread(&graph->view_zoom, sizeof(f32), 1, file);
    
    fclose(file);
    
    graph->needs_recompile = true;
    return graph;
}

// =============================================================================
// C CODE EXPORT
// =============================================================================

// Export graph as C code
void node_graph_export_c(node_graph_t *graph, const char *filename) {
    if (!graph || !filename) return;
    
    FILE *file = fopen(filename, "w");
    if (!file) return;
    
    // Compile graph first
    if (graph->needs_recompile) {
        node_graph_compile(graph);
    }
    
    // Write header
    fprintf(file, "// Generated from node graph: %s\n", graph->name);
    fprintf(file, "// Node count: %d, Connection count: %d\n\n", 
            graph->node_count, graph->connection_count);
    
    fprintf(file, "#include <math.h>\n");
    fprintf(file, "#include <stdbool.h>\n\n");
    
    // Generate function signature
    fprintf(file, "void execute_%s(void *context) {\n", graph->name);
    
    // Declare variables for each node output
    fprintf(file, "    // Node outputs\n");
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (graph->nodes[i].type) {
            node_t *node = &graph->nodes[i];
            for (i32 j = 0; j < node->output_count; ++j) {
                const char *type_str = "float";
                switch (node->outputs[j].type) {
                    case PIN_TYPE_BOOL: type_str = "bool"; break;
                    case PIN_TYPE_INT: type_str = "int"; break;
                    case PIN_TYPE_FLOAT: type_str = "float"; break;
                    case PIN_TYPE_VECTOR2: type_str = "vec2"; break;
                    case PIN_TYPE_VECTOR3: type_str = "vec3"; break;
                    default: break;
                }
                
                fprintf(file, "    %s node_%d_out_%d;\n", type_str, i, j);
            }
        }
    }
    
    fprintf(file, "\n    // Execution\n");
    
    // Generate code for each node in execution order
    for (i32 i = 0; i < graph->execution_order_count; ++i) {
        i32 node_id = graph->execution_order[i];
        node_t *node = &graph->nodes[node_id];
        
        if (!node->type) continue;
        
        fprintf(file, "    // %s (node %d)\n", node->type->name, node_id);
        
        // Generate code based on node type
        if (strcmp(node->type->name, "Add") == 0) {
            // Find input connections
            f32 a = 0, b = 0;
            for (i32 c = 0; c < graph->connection_count; ++c) {
                node_connection_t *conn = &graph->connections[c];
                if (conn->target_node == node_id) {
                    if (conn->target_pin == 0) {
                        fprintf(file, "    float a_%d = node_%d_out_%d;\n", 
                                node_id, conn->source_node, conn->source_pin);
                    } else if (conn->target_pin == 1) {
                        fprintf(file, "    float b_%d = node_%d_out_%d;\n",
                                node_id, conn->source_node, conn->source_pin);
                    }
                }
            }
            fprintf(file, "    node_%d_out_0 = a_%d + b_%d;\n", node_id, node_id, node_id);
            
        } else if (strcmp(node->type->name, "Multiply") == 0) {
            fprintf(file, "    // Multiply operation\n");
            // Similar to Add
            
        } else if (strcmp(node->type->name, "Sin") == 0) {
            fprintf(file, "    node_%d_out_0 = sinf(/* input */);\n", node_id);
            
        } else if (strcmp(node->type->name, "Branch") == 0) {
            fprintf(file, "    if (/* condition */) {\n");
            fprintf(file, "        // True branch\n");
            fprintf(file, "    } else {\n");
            fprintf(file, "        // False branch\n");
            fprintf(file, "    }\n");
            
        } else {
            fprintf(file, "    // TODO: Implement %s\n", node->type->name);
        }
        
        fprintf(file, "\n");
    }
    
    fprintf(file, "}\n");
    
    fclose(file);
}

// =============================================================================
// ENGINE API EXPOSURE
// =============================================================================

// Register all engine APIs as nodes
void register_engine_api_nodes(void) {
    // Entity manipulation
    {
        node_type_t type = {0};
        strcpy(type.name, "Get Entity Position");
        type.category = NODE_CATEGORY_GAME;
        type.color = 0xFF408080;
        type.width = 150;
        type.min_height = 60;
        
        type.input_count = 1;
        strcpy(type.input_templates[0].name, "Entity");
        type.input_templates[0].type = PIN_TYPE_ENTITY;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Position");
        type.output_templates[0].type = PIN_TYPE_VECTOR3;
        
        // type.execute = execute_get_entity_position;
        node_register_type(&type);
    }
    
    // Physics
    {
        node_type_t type = {0};
        strcpy(type.name, "Raycast");
        type.category = NODE_CATEGORY_GAME;
        type.color = 0xFF408080;
        type.width = 180;
        type.min_height = 100;
        
        type.input_count = 3;
        strcpy(type.input_templates[0].name, "Origin");
        type.input_templates[0].type = PIN_TYPE_VECTOR3;
        strcpy(type.input_templates[1].name, "Direction");
        type.input_templates[1].type = PIN_TYPE_VECTOR3;
        strcpy(type.input_templates[2].name, "Distance");
        type.input_templates[2].type = PIN_TYPE_FLOAT;
        
        type.output_count = 3;
        strcpy(type.output_templates[0].name, "Hit");
        type.output_templates[0].type = PIN_TYPE_BOOL;
        strcpy(type.output_templates[1].name, "Hit Point");
        type.output_templates[1].type = PIN_TYPE_VECTOR3;
        strcpy(type.output_templates[2].name, "Hit Entity");
        type.output_templates[2].type = PIN_TYPE_ENTITY;
        
        // type.execute = execute_raycast;
        node_register_type(&type);
    }
    
    // Audio
    {
        node_type_t type = {0};
        strcpy(type.name, "Play Sound 3D");
        type.category = NODE_CATEGORY_GAME;
        type.color = 0xFF408080;
        type.width = 150;
        type.min_height = 80;
        
        type.input_count = 4;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.input_templates[1].name, "Sound");
        type.input_templates[1].type = PIN_TYPE_OBJECT;
        strcpy(type.input_templates[2].name, "Position");
        type.input_templates[2].type = PIN_TYPE_VECTOR3;
        strcpy(type.input_templates[3].name, "Volume");
        type.input_templates[3].type = PIN_TYPE_FLOAT;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Exec");
        type.output_templates[0].type = PIN_TYPE_EXECUTION;
        
        // type.execute = execute_play_sound_3d;
        node_register_type(&type);
    }
    
    // Continue registering more engine APIs...
}

// =============================================================================
// INITIALIZATION
// =============================================================================

// Initialize all integration systems
void nodes_integration_init(void) {
    // Register engine API nodes
    register_engine_api_nodes();
    
    // Initialize hot reload
    memset(&g_hot_reload, 0, sizeof(g_hot_reload));
    
    // Set up auto-save timer
    // Platform-specific timer setup
}