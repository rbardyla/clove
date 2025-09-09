// nodes_advanced.c - Advanced node system features
// FEATURES: Subgraphs, compilation to bytecode, templates, live editing

#include "handmade_nodes.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// SUBGRAPH SYSTEM
// =============================================================================

typedef struct {
    node_graph_t *parent_graph;
    node_graph_t *subgraph;
    i32 input_node_id;   // Special node for inputs
    i32 output_node_id;  // Special node for outputs
} subgraph_instance_t;

static subgraph_instance_t g_subgraphs[MAX_SUBGRAPHS];
static i32 g_subgraph_count = 0;

// Create subgraph from selected nodes
node_graph_t* create_subgraph_from_selection(node_graph_t *parent, const char *name) {
    if (!parent || parent->selected_count == 0) return NULL;
    
    // Create new graph
    node_graph_t *subgraph = node_graph_create(name);
    if (!subgraph) return NULL;
    
    // Track remapping of node IDs
    i32 id_map[MAX_NODES_PER_GRAPH];
    memset(id_map, -1, sizeof(id_map));
    
    // Copy selected nodes to subgraph
    for (i32 i = 0; i < parent->selected_count; ++i) {
        i32 old_id = parent->selected_nodes[i];
        node_t *old_node = node_find_by_id(parent, old_id);
        if (!old_node) continue;
        
        // Create node in subgraph
        node_t *new_node = node_create(subgraph, old_node->type_id,
                                       old_node->x, old_node->y);
        if (new_node) {
            // Copy custom data
            memcpy(new_node->custom_data, old_node->custom_data, sizeof(new_node->custom_data));
            
            // Store ID mapping
            id_map[old_id] = new_node->id;
        }
    }
    
    // Copy connections between selected nodes
    for (i32 i = 0; i < parent->connection_count; ++i) {
        node_connection_t *conn = &parent->connections[i];
        
        // Check if both nodes are in selection
        i32 new_source = id_map[conn->source_node];
        i32 new_target = id_map[conn->target_node];
        
        if (new_source >= 0 && new_target >= 0) {
            node_connect(subgraph, new_source, conn->source_pin,
                        new_target, conn->target_pin);
        }
    }
    
    // Create input/output interface nodes
    // These special nodes represent the subgraph's pins
    node_type_t input_type = {0};
    strcpy(input_type.name, "Subgraph Input");
    input_type.category = NODE_CATEGORY_CUSTOM;
    input_type.output_count = 8;  // Max 8 outputs from input node
    for (i32 i = 0; i < 8; ++i) {
        sprintf(input_type.output_templates[i].name, "In %d", i);
        input_type.output_templates[i].type = PIN_TYPE_ANY;
    }
    
    node_type_t output_type = {0};
    strcpy(output_type.name, "Subgraph Output");
    output_type.category = NODE_CATEGORY_CUSTOM;
    output_type.input_count = 8;  // Max 8 inputs to output node
    for (i32 i = 0; i < 8; ++i) {
        sprintf(output_type.input_templates[i].name, "Out %d", i);
        output_type.input_templates[i].type = PIN_TYPE_ANY;
    }
    
    // Register these types temporarily
    node_register_type(&input_type);
    node_register_type(&output_type);
    
    // Store subgraph info
    if (g_subgraph_count < MAX_SUBGRAPHS) {
        g_subgraphs[g_subgraph_count].parent_graph = parent;
        g_subgraphs[g_subgraph_count].subgraph = subgraph;
        g_subgraph_count++;
    }
    
    // Delete original nodes from parent
    for (i32 i = 0; i < parent->selected_count; ++i) {
        node_t *node = node_find_by_id(parent, parent->selected_nodes[i]);
        if (node) {
            node_destroy(parent, node);
        }
    }
    parent->selected_count = 0;
    
    // Create subgraph node in parent
    // TODO: Create special subgraph node type that references this subgraph
    
    return subgraph;
}

// Execute subgraph as a single node
void execute_subgraph(node_t *subgraph_node, node_graph_t *subgraph, 
                     node_execution_context_t *context) {
    // Map inputs from parent node to subgraph input node
    // Execute subgraph
    // Map outputs from subgraph output node to parent node
    
    node_execution_context_t sub_context = *context;
    sub_context.graph = subgraph;
    
    executor_execute_graph(subgraph, &sub_context);
}

// =============================================================================
// BYTECODE COMPILATION
// =============================================================================

typedef enum {
    OP_NOP = 0,
    OP_LOAD_CONST,
    OP_LOAD_VAR,
    OP_STORE_VAR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_CALL,
    OP_JUMP,
    OP_JUMP_IF,
    OP_RETURN,
    OP_PUSH,
    OP_POP
} opcode_e;

typedef struct {
    opcode_e op;
    union {
        f32 f;
        i32 i;
        void *ptr;
    } arg;
} instruction_t;

typedef struct {
    instruction_t *code;
    i32 code_size;
    i32 code_capacity;
    
    // Constant pool
    pin_value_t constants[256];
    i32 constant_count;
    
    // Variable storage
    pin_value_t variables[256];
    i32 variable_count;
    
    // Execution stack
    pin_value_t stack[256];
    i32 stack_top;
} bytecode_vm_t;

static bytecode_vm_t g_vm;

// Compile node graph to bytecode
void compile_to_bytecode(node_graph_t *graph) {
    if (!graph) return;
    
    // Ensure graph is compiled (topologically sorted)
    if (graph->needs_recompile) {
        node_graph_compile(graph);
    }
    
    // Allocate bytecode buffer
    if (!g_vm.code) {
        g_vm.code_capacity = 4096;
        g_vm.code = (instruction_t*)malloc(sizeof(instruction_t) * g_vm.code_capacity);
    }
    g_vm.code_size = 0;
    
    // Generate bytecode for each node in execution order
    for (i32 i = 0; i < graph->execution_order_count; ++i) {
        i32 node_id = graph->execution_order[i];
        node_t *node = &graph->nodes[node_id];
        
        if (!node->type) continue;
        
        // Generate instructions based on node type
        if (strcmp(node->type->name, "Add") == 0) {
            // Load inputs
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_LOAD_VAR, {.i = node->inputs[0].value.i}};
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_LOAD_VAR, {.i = node->inputs[1].value.i}};
            // Add
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_ADD, {0}};
            // Store result
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_STORE_VAR, {.i = node_id}};
        } else if (strcmp(node->type->name, "Multiply") == 0) {
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_LOAD_VAR, {.i = node->inputs[0].value.i}};
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_LOAD_VAR, {.i = node->inputs[1].value.i}};
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_MUL, {0}};
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_STORE_VAR, {.i = node_id}};
        } else if (strcmp(node->type->name, "Branch") == 0) {
            // Conditional jump
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_LOAD_VAR, {.i = node->inputs[1].value.i}};
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_JUMP_IF, {.i = 0}};  // Jump target to be patched
        } else {
            // Generic node - call its execute function
            g_vm.code[g_vm.code_size++] = (instruction_t){OP_CALL, {.ptr = node->type->execute}};
        }
    }
    
    // Add return instruction
    g_vm.code[g_vm.code_size++] = (instruction_t){OP_RETURN, {0}};
}

// Execute bytecode
void execute_bytecode(bytecode_vm_t *vm) {
    if (!vm || !vm->code) return;
    
    vm->stack_top = -1;
    i32 pc = 0;  // Program counter
    
    while (pc < vm->code_size) {
        instruction_t *inst = &vm->code[pc];
        
        switch (inst->op) {
            case OP_NOP:
                break;
                
            case OP_LOAD_CONST:
                vm->stack[++vm->stack_top] = vm->constants[inst->arg.i];
                break;
                
            case OP_LOAD_VAR:
                vm->stack[++vm->stack_top] = vm->variables[inst->arg.i];
                break;
                
            case OP_STORE_VAR:
                vm->variables[inst->arg.i] = vm->stack[vm->stack_top--];
                break;
                
            case OP_ADD: {
                pin_value_t b = vm->stack[vm->stack_top--];
                pin_value_t a = vm->stack[vm->stack_top--];
                pin_value_t result;
                result.f = a.f + b.f;
                vm->stack[++vm->stack_top] = result;
                break;
            }
                
            case OP_MUL: {
                pin_value_t b = vm->stack[vm->stack_top--];
                pin_value_t a = vm->stack[vm->stack_top--];
                pin_value_t result;
                result.f = a.f * b.f;
                vm->stack[++vm->stack_top] = result;
                break;
            }
                
            case OP_JUMP:
                pc = inst->arg.i;
                continue;
                
            case OP_JUMP_IF: {
                pin_value_t cond = vm->stack[vm->stack_top--];
                if (cond.b) {
                    pc = inst->arg.i;
                    continue;
                }
                break;
            }
                
            case OP_CALL: {
                // Call native function
                void (*func)(node_t*, void*) = (void(*)(node_t*, void*))inst->arg.ptr;
                // TODO: Set up node context and call
                break;
            }
                
            case OP_RETURN:
                return;
                
            default:
                // Unknown opcode
                return;
        }
        
        pc++;
    }
}

// =============================================================================
// NODE TEMPLATES AND PRESETS
// =============================================================================

typedef struct {
    char name[64];
    char description[256];
    node_type_t types[32];
    i32 type_count;
    struct {
        i32 source_node_index;
        i32 source_pin;
        i32 target_node_index;
        i32 target_pin;
    } connections[64];
    i32 connection_count;
} node_template_t;

static node_template_t g_templates[32];
static i32 g_template_count = 0;

// Create template from selection
void create_template_from_selection(node_graph_t *graph, const char *name) {
    if (!graph || graph->selected_count == 0) return;
    if (g_template_count >= 32) return;
    
    node_template_t *template = &g_templates[g_template_count++];
    strncpy(template->name, name, 63);
    template->type_count = 0;
    template->connection_count = 0;
    
    // Store node types
    for (i32 i = 0; i < graph->selected_count; ++i) {
        node_t *node = node_find_by_id(graph, graph->selected_nodes[i]);
        if (node && node->type) {
            template->types[template->type_count++] = *node->type;
        }
    }
    
    // Store connections
    // TODO: Store internal connections between selected nodes
}

// Instantiate template
void instantiate_template(node_graph_t *graph, i32 template_index, f32 x, f32 y) {
    if (template_index < 0 || template_index >= g_template_count) return;
    
    node_template_t *template = &g_templates[template_index];
    
    // Create nodes
    i32 created_ids[32];
    for (i32 i = 0; i < template->type_count; ++i) {
        i32 type_id = node_get_type_id(template->types[i].name);
        if (type_id >= 0) {
            node_t *node = node_create(graph, type_id, x + i * 150, y);
            if (node) {
                created_ids[i] = node->id;
            }
        }
    }
    
    // Create connections
    for (i32 i = 0; i < template->connection_count; ++i) {
        node_connect(graph,
                    created_ids[template->connections[i].source_node_index],
                    template->connections[i].source_pin,
                    created_ids[template->connections[i].target_node_index],
                    template->connections[i].target_pin);
    }
}

// =============================================================================
// LIVE EDITING
// =============================================================================

typedef struct {
    bool enabled;
    node_graph_t *graph;
    node_graph_t *live_copy;  // Copy for live editing
    bool has_changes;
} live_edit_state_t;

static live_edit_state_t g_live_edit;

// Enable live editing
void enable_live_editing(node_graph_t *graph) {
    if (!graph) return;
    
    g_live_edit.enabled = true;
    g_live_edit.graph = graph;
    
    // Create copy of graph for live editing
    g_live_edit.live_copy = node_graph_duplicate(graph);
    g_live_edit.has_changes = false;
}

// Apply live changes
void apply_live_changes(void) {
    if (!g_live_edit.enabled || !g_live_edit.has_changes) return;
    
    // Copy live graph back to main graph
    // This should be done carefully to preserve running state
    
    // Only copy changed nodes
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        node_t *live_node = &g_live_edit.live_copy->nodes[i];
        node_t *main_node = &g_live_edit.graph->nodes[i];
        
        if (live_node->type && main_node->type) {
            // Check if node changed
            if (memcmp(live_node, main_node, sizeof(node_t)) != 0) {
                // Copy changes but preserve execution state
                f32 old_x = main_node->x;
                f32 old_y = main_node->y;
                i32 exec_count = main_node->execution_count;
                u64 last_cycles = main_node->last_execution_cycles;
                
                *main_node = *live_node;
                
                // Restore runtime state
                main_node->execution_count = exec_count;
                main_node->last_execution_cycles = last_cycles;
            }
        }
    }
    
    g_live_edit.graph->needs_recompile = true;
    g_live_edit.has_changes = false;
}

// Disable live editing
void disable_live_editing(void) {
    if (!g_live_edit.enabled) return;
    
    if (g_live_edit.live_copy) {
        node_graph_destroy(g_live_edit.live_copy);
        g_live_edit.live_copy = NULL;
    }
    
    g_live_edit.enabled = false;
}

// =============================================================================
// VISUAL DEBUGGING
// =============================================================================

typedef struct {
    bool enabled;
    bool show_values;
    bool show_flow;
    bool show_performance;
    f32 flow_speed;
    
    // Value display cache
    struct {
        i32 node_id;
        i32 pin_index;
        char value_str[64];
    } value_displays[256];
    i32 value_display_count;
} visual_debug_state_t;

static visual_debug_state_t g_visual_debug = {
    .flow_speed = 1.0f
};

// Enable visual debugging
void enable_visual_debugging(bool show_values, bool show_flow, bool show_performance) {
    g_visual_debug.enabled = true;
    g_visual_debug.show_values = show_values;
    g_visual_debug.show_flow = show_flow;
    g_visual_debug.show_performance = show_performance;
}

// Update value displays
void update_value_displays(node_graph_t *graph) {
    if (!g_visual_debug.enabled || !g_visual_debug.show_values) return;
    
    g_visual_debug.value_display_count = 0;
    
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        node_t *node = &graph->nodes[i];
        if (!node->type) continue;
        
        // Display output values
        for (i32 j = 0; j < node->output_count; ++j) {
            if (g_visual_debug.value_display_count >= 256) break;
            
            struct {
                i32 node_id;
                i32 pin_index;
                char value_str[64];
            } *display = &g_visual_debug.value_displays[g_visual_debug.value_display_count++];
            
            display->node_id = i;
            display->pin_index = j;
            
            // Format value based on type
            pin_value_t *val = &node->outputs[j].value;
            switch (node->outputs[j].type) {
                case PIN_TYPE_FLOAT:
                    snprintf(display->value_str, 64, "%.2f", val->f);
                    break;
                case PIN_TYPE_INT:
                    snprintf(display->value_str, 64, "%d", val->i);
                    break;
                case PIN_TYPE_BOOL:
                    snprintf(display->value_str, 64, "%s", val->b ? "true" : "false");
                    break;
                case PIN_TYPE_VECTOR2:
                    snprintf(display->value_str, 64, "(%.1f, %.1f)", val->v2.x, val->v2.y);
                    break;
                case PIN_TYPE_VECTOR3:
                    snprintf(display->value_str, 64, "(%.1f, %.1f, %.1f)", 
                            val->v3.x, val->v3.y, val->v3.z);
                    break;
                default:
                    strcpy(display->value_str, "...");
                    break;
            }
        }
    }
}

// =============================================================================
// NODE VERSIONING
// =============================================================================

typedef struct {
    i32 version;
    char migration_notes[256];
    void (*migrate_func)(node_t *old_node, node_t *new_node);
} node_version_info_t;

typedef struct {
    char type_name[64];
    i32 current_version;
    node_version_info_t versions[8];
    i32 version_count;
} node_version_registry_t;

static node_version_registry_t g_version_registry[MAX_NODE_TYPES];
static i32 g_version_registry_count = 0;

// Register node version
void register_node_version(const char *type_name, i32 version,
                           void (*migrate_func)(node_t*, node_t*)) {
    // Find or create registry entry
    node_version_registry_t *reg = NULL;
    for (i32 i = 0; i < g_version_registry_count; ++i) {
        if (strcmp(g_version_registry[i].type_name, type_name) == 0) {
            reg = &g_version_registry[i];
            break;
        }
    }
    
    if (!reg && g_version_registry_count < MAX_NODE_TYPES) {
        reg = &g_version_registry[g_version_registry_count++];
        strncpy(reg->type_name, type_name, 63);
        reg->current_version = version;
        reg->version_count = 0;
    }
    
    if (!reg) return;
    
    // Add version info
    if (reg->version_count < 8) {
        reg->versions[reg->version_count].version = version;
        reg->versions[reg->version_count].migrate_func = migrate_func;
        reg->version_count++;
        
        if (version > reg->current_version) {
            reg->current_version = version;
        }
    }
}

// Migrate node to latest version
void migrate_node_version(node_t *node) {
    if (!node || !node->type) return;
    
    // Find version registry
    node_version_registry_t *reg = NULL;
    for (i32 i = 0; i < g_version_registry_count; ++i) {
        if (strcmp(g_version_registry[i].type_name, node->type->name) == 0) {
            reg = &g_version_registry[i];
            break;
        }
    }
    
    if (!reg) return;
    
    // Check if migration needed
    i32 node_version = *(i32*)node->custom_data;  // Assume version stored at start
    
    if (node_version < reg->current_version) {
        // Find migration path
        for (i32 v = node_version + 1; v <= reg->current_version; ++v) {
            for (i32 i = 0; i < reg->version_count; ++i) {
                if (reg->versions[i].version == v && reg->versions[i].migrate_func) {
                    // Create temporary new node
                    node_t temp_node = *node;
                    
                    // Run migration
                    reg->versions[i].migrate_func(node, &temp_node);
                    
                    // Copy back
                    *node = temp_node;
                    
                    // Update version
                    *(i32*)node->custom_data = v;
                }
            }
        }
    }
}

// =============================================================================
// PERFORMANCE PROFILER OVERLAY
// =============================================================================

void draw_performance_overlay(renderer *r, node_graph_t *graph, i32 x, i32 y) {
    if (!g_visual_debug.enabled || !g_visual_debug.show_performance) return;
    
    // Get performance stats
    node_performance_stats_t stats;
    node_get_performance_stats(graph, &stats);
    
    // Draw background
    renderer_blend_rect(r, x, y, 300, 200, rgba(0, 0, 0, 200));
    
    // Draw title
    renderer_text(r, x + 10, y + 10, "Performance Profile", rgb(255, 255, 255));
    
    // Draw stats
    char text[256];
    snprintf(text, 256, "Frame Time: %.2f ms", stats.frame_ms);
    renderer_text(r, x + 10, y + 30, text, rgb(255, 255, 0));
    
    snprintf(text, 256, "Nodes Executed: %d", stats.nodes_executed);
    renderer_text(r, x + 10, y + 45, text, rgb(255, 255, 255));
    
    // Find hottest nodes
    typedef struct {
        i32 node_id;
        u64 cycles;
    } hot_node_t;
    
    hot_node_t hot_nodes[10];
    i32 hot_count = 0;
    
    for (i32 i = 0; i < MAX_NODES_PER_GRAPH; ++i) {
        if (stats.node_cycles[i] > 0) {
            // Insert into hot list
            hot_node_t entry = { i, stats.node_cycles[i] };
            
            i32 insert_pos = hot_count;
            for (i32 j = 0; j < hot_count; ++j) {
                if (entry.cycles > hot_nodes[j].cycles) {
                    insert_pos = j;
                    break;
                }
            }
            
            if (insert_pos < 10) {
                // Shift and insert
                for (i32 j = Minimum(hot_count, 9); j > insert_pos; --j) {
                    hot_nodes[j] = hot_nodes[j - 1];
                }
                hot_nodes[insert_pos] = entry;
                if (hot_count < 10) hot_count++;
            }
        }
    }
    
    // Draw hot nodes
    renderer_text(r, x + 10, y + 70, "Hottest Nodes:", rgb(255, 128, 128));
    for (i32 i = 0; i < hot_count; ++i) {
        node_t *node = node_find_by_id(graph, hot_nodes[i].node_id);
        if (node && node->type) {
            f32 ms = (f32)(hot_nodes[i].cycles / 3000000.0);
            snprintf(text, 256, "%d. %s: %.3f ms", i + 1, node->type->name, ms);
            
            // Color code by performance
            color32 color = rgb(255, 255, 255);
            if (ms > 1.0f) color = rgb(255, 0, 0);
            else if (ms > 0.5f) color = rgb(255, 128, 0);
            else if (ms > 0.1f) color = rgb(255, 255, 0);
            
            renderer_text(r, x + 20, y + 85 + i * 12, text, color);
        }
    }
}