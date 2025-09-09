// blueprint_compiler.c - High-performance bytecode compiler for blueprint graphs
// PERFORMANCE: Single-pass compilation with optimal bytecode generation
// TARGET: Compile 10,000+ node graphs in <10ms

#include "handmade_blueprint.h"
#include <string.h>
#include <assert.h>

// ============================================================================
// COMPILER DATA STRUCTURES
// ============================================================================

// Compilation context
typedef struct compiler_context {
    blueprint_context* bp_ctx;
    blueprint_graph* graph;
    
    // Bytecode generation
    bp_instruction* bytecode;
    u32 bytecode_capacity;
    u32 bytecode_count;
    
    // Constant pool
    blueprint_value* constants;
    u32 constant_count;
    
    // Variable mapping
    u32* variable_indices;  // Maps graph variable indices to VM indices
    
    // Node compilation state
    node_id* compiled_nodes;
    u32 compiled_node_count;
    b32* node_visited;      // For topological sorting
    b32* node_in_progress;  // For cycle detection
    
    // Jump patching
    struct {
        u32 instruction_index;
        node_id target_node;
    } pending_jumps[1024];
    u32 pending_jump_count;
    
    // Node address mapping - maps node_id to bytecode address
    struct {
        node_id node;
        u32 address;
    } node_addresses[BLUEPRINT_MAX_NODES];
    u32 node_address_count;
    
    // Error reporting
    char error_buffer[512];
    b32 has_error;
} compiler_context;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Emit bytecode instruction - PERFORMANCE: Direct array access with bounds check
static void emit_instruction(compiler_context* ctx, bp_opcode opcode, u32 op1, u32 op2, u32 op3) {
    if (ctx->bytecode_count >= ctx->bytecode_capacity) {
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), 
                "Bytecode buffer overflow at instruction %u", ctx->bytecode_count);
        return;
    }
    
    bp_instruction* inst = &ctx->bytecode[ctx->bytecode_count++];
    inst->opcode = opcode;
    inst->operand1 = op1;
    inst->operand2 = op2;
    inst->operand3 = op3;
}

// Add constant to pool and return index
static u32 add_constant(compiler_context* ctx, blueprint_value value) {
    if (ctx->constant_count >= BLUEPRINT_MAX_CONSTANTS) {
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), "Constant pool overflow");
        return 0;
    }
    
    // Check if constant already exists to avoid duplicates
    for (u32 i = 0; i < ctx->constant_count; i++) {
        // Simple memcmp for now - could be optimized per type
        if (memcmp(&ctx->constants[i], &value, sizeof(blueprint_value)) == 0) {
            return i;
        }
    }
    
    ctx->constants[ctx->constant_count] = value;
    return ctx->constant_count++;
}

// Map node ID to bytecode address
static void set_node_address(compiler_context* ctx, node_id node, u32 address) {
    if (ctx->node_address_count >= BLUEPRINT_MAX_NODES) {
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), "Node address table overflow");
        return;
    }
    
    ctx->node_addresses[ctx->node_address_count].node = node;
    ctx->node_addresses[ctx->node_address_count].address = address;
    ctx->node_address_count++;
}

// Get bytecode address for node
static u32 get_node_address(compiler_context* ctx, node_id node) {
    for (u32 i = 0; i < ctx->node_address_count; i++) {
        if (ctx->node_addresses[i].node == node) {
            return ctx->node_addresses[i].address;
        }
    }
    return 0xFFFFFFFF; // Invalid address
}

// Add pending jump to be patched later
static void add_pending_jump(compiler_context* ctx, u32 instruction_index, node_id target_node) {
    if (ctx->pending_jump_count >= 1024) {
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), "Too many pending jumps");
        return;
    }
    
    ctx->pending_jumps[ctx->pending_jump_count].instruction_index = instruction_index;
    ctx->pending_jumps[ctx->pending_jump_count].target_node = target_node;
    ctx->pending_jump_count++;
}

// Patch all pending jumps
static void patch_jumps(compiler_context* ctx) {
    for (u32 i = 0; i < ctx->pending_jump_count; i++) {
        u32 inst_index = ctx->pending_jumps[i].instruction_index;
        node_id target = ctx->pending_jumps[i].target_node;
        u32 target_address = get_node_address(ctx, target);
        
        if (target_address == 0xFFFFFFFF) {
            ctx->has_error = true;
            snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), 
                    "Cannot resolve jump target node %u", target);
            return;
        }
        
        ctx->bytecode[inst_index].operand1 = target_address;
    }
}

// ============================================================================
// NODE COMPILATION
// ============================================================================

// Get execution pin from node
static blueprint_pin* get_exec_output_pin(blueprint_node* node) {
    for (u32 i = 0; i < node->output_pin_count; i++) {
        if (node->output_pins[i].type == BP_TYPE_EXEC) {
            return &node->output_pins[i];
        }
    }
    return NULL;
}

// Get all execution pins that connect to this node
static u32 get_exec_input_connections(compiler_context* ctx, node_id node_id, blueprint_connection** out_connections, u32 max_connections) {
    u32 count = 0;
    
    for (u32 i = 0; i < ctx->graph->connection_count && count < max_connections; i++) {
        blueprint_connection* conn = &ctx->graph->connections[i];
        
        if (conn->to_node == node_id) {
            // Check if this is an execution connection
            blueprint_node* from_node = blueprint_get_node(ctx->graph, conn->from_node);
            if (from_node) {
                blueprint_pin* from_pin = blueprint_get_pin(from_node, conn->from_pin);
                if (from_pin && from_pin->type == BP_TYPE_EXEC) {
                    out_connections[count++] = conn;
                }
            }
        }
    }
    
    return count;
}

// Compile a single node to bytecode
static void compile_node(compiler_context* ctx, blueprint_node* node) {
    if (ctx->has_error) return;
    
    // Mark current bytecode position as node address
    set_node_address(ctx, node->id, ctx->bytecode_count);
    
    switch (node->type) {
        case NODE_TYPE_BEGIN_PLAY: {
            // BeginPlay is entry point - no special bytecode needed
            // The VM will start execution here
            break;
        }
        
        case NODE_TYPE_TICK: {
            // Tick node - called every frame
            // For now, just continue execution
            break;
        }
        
        case NODE_TYPE_ADD: {
            // Math operation: pop two values, add, push result
            // Assumes input pins are already loaded to stack
            emit_instruction(ctx, BP_OP_ADD, 0, 0, 0);
            break;
        }
        
        case NODE_TYPE_MULTIPLY: {
            emit_instruction(ctx, BP_OP_MUL, 0, 0, 0);
            break;
        }
        
        case NODE_TYPE_SUBTRACT: {
            emit_instruction(ctx, BP_OP_SUB, 0, 0, 0);
            break;
        }
        
        case NODE_TYPE_DIVIDE: {
            emit_instruction(ctx, BP_OP_DIV, 0, 0, 0);
            break;
        }
        
        case NODE_TYPE_BRANCH: {
            // Conditional branch - pop condition, jump based on result
            // Operand1 will be patched with target address
            u32 branch_inst = ctx->bytecode_count;
            emit_instruction(ctx, BP_OP_JUMP_IF_FALSE, 0, 0, 0);
            
            // Find the "False" execution output and add pending jump
            // Look for connections from the "False" output pin
            for (u32 i = 0; i < ctx->graph->connection_count; i++) {
                blueprint_connection* conn = &ctx->graph->connections[i];
                
                // Check if this is the False branch output (usually pin index 1)
                if (conn->from_node == node->id && conn->from_pin == 1 && 
                    conn->data_type == BP_TYPE_EXEC) {
                    // Add pending jump for false branch
                    add_pending_jump(ctx, branch_inst, conn->to_node);
                    break;
                }
            }
            
            // Find the "True" execution output for fall-through
            for (u32 i = 0; i < ctx->graph->connection_count; i++) {
                blueprint_connection* conn = &ctx->graph->connections[i];
                
                // Check if this is the True branch output (usually pin index 0)
                if (conn->from_node == node->id && conn->from_pin == 0 && 
                    conn->data_type == BP_TYPE_EXEC) {
                    // Continue to true branch node
                    compile_node(ctx, conn->to_node);
                    break;
                }
            }
            
            break;
        }
        
        case NODE_TYPE_PRINT: {
            // Debug print - for now emit a breakpoint
            emit_instruction(ctx, BP_OP_BREAK, node->id, 0, 0);
            break;
        }
        
        case NODE_TYPE_GET_VARIABLE: {
            // Load variable value to stack
            // Find variable index by node name
            u32 var_index = 0;
            for (u32 i = 0; i < ctx->graph->variable_count; i++) {
                if (strcmp(ctx->graph->variables[i].name, node->name) == 0) {
                    var_index = i;
                    break;
                }
            }
            emit_instruction(ctx, BP_OP_LOAD_VAR, var_index, 0, 0);
            break;
        }
        
        case NODE_TYPE_SET_VARIABLE: {
            // Store stack top to variable
            // Find variable index by node name
            u32 var_index = 0;
            for (u32 i = 0; i < ctx->graph->variable_count; i++) {
                if (strcmp(ctx->graph->variables[i].name, node->name) == 0) {
                    var_index = i;
                    break;
                }
            }
            emit_instruction(ctx, BP_OP_STORE_VAR, var_index, 0, 0);
            break;
        }
        
        case NODE_TYPE_CAST: {
            // Type casting - determine target type from output pin
            blueprint_type target_type = BP_TYPE_FLOAT;
            
            // Get the output pin type
            if (node->output_pin_count > 0) {
                target_type = node->output_pins[0].type;
            }
            
            emit_instruction(ctx, BP_OP_CAST, (u32)target_type, 0, 0);
            break;
        }
        
        default: {
            // Unknown node type - emit NOP
            emit_instruction(ctx, BP_OP_NOP, 0, 0, 0);
            break;
        }
    }
    
    // Load input pin values to stack before node execution
    // This happens in reverse order so first pin ends up on top
    for (i32 i = (i32)node->input_pin_count - 1; i >= 0; i--) {
        blueprint_pin* pin = &node->input_pins[i];
        
        if (pin->type == BP_TYPE_EXEC) continue; // Skip execution pins
        
        if (pin->has_connection) {
            // Value will be provided by connected output
            // For now, just load the current value
            u32 const_index = add_constant(ctx, pin->current_value);
            emit_instruction(ctx, BP_OP_LOAD_CONST, const_index, 0, 0);
        } else {
            // Use default value
            u32 const_index = add_constant(ctx, pin->default_value);
            emit_instruction(ctx, BP_OP_LOAD_CONST, const_index, 0, 0);
        }
    }
    
    // Follow execution flow to next nodes
    blueprint_pin* exec_pin = get_exec_output_pin(node);
    if (exec_pin && exec_pin->has_connection) {
        // Find connected node and add jump
        for (u32 i = 0; i < ctx->graph->connection_count; i++) {
            blueprint_connection* conn = &ctx->graph->connections[i];
            if (conn->from_node == node->id && conn->from_pin == exec_pin->id) {
                // Add unconditional jump to next node
                u32 jump_inst = ctx->bytecode_count;
                emit_instruction(ctx, BP_OP_JUMP, 0, 0, 0);
                add_pending_jump(ctx, jump_inst, conn->to_node);
                break;
            }
        }
    }
}

// ============================================================================
// TOPOLOGICAL SORTING
// ============================================================================

// Depth-first search for topological sorting
static void topological_sort_dfs(compiler_context* ctx, node_id node_id) {
    if (ctx->has_error) return;
    
    u32 node_index = 0;
    blueprint_node* node = NULL;
    
    // Find node index in graph arrays
    for (u32 i = 0; i < ctx->graph->node_count; i++) {
        if (ctx->graph->node_ids[i] == node_id) {
            node_index = i;
            node = &ctx->graph->nodes[i];
            break;
        }
    }
    
    if (!node) {
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), 
                "Node %u not found during topological sort", node_id);
        return;
    }
    
    if (ctx->node_in_progress[node_index]) {
        // Cycle detected
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), 
                "Circular dependency detected involving node %u", node_id);
        return;
    }
    
    if (ctx->node_visited[node_index]) {
        return; // Already processed
    }
    
    ctx->node_in_progress[node_index] = true;
    
    // Visit all nodes that this node depends on (via execution flow)
    for (u32 i = 0; i < ctx->graph->connection_count; i++) {
        blueprint_connection* conn = &ctx->graph->connections[i];
        
        // If this node receives execution from another node, process that first
        if (conn->to_node == node_id && conn->data_type == BP_TYPE_EXEC) {
            topological_sort_dfs(ctx, conn->from_node);
        }
    }
    
    ctx->node_in_progress[node_index] = false;
    ctx->node_visited[node_index] = true;
    
    // Add to execution order
    if (ctx->graph->execution_order_count < ctx->graph->node_capacity) {
        ctx->graph->execution_order[ctx->graph->execution_order_count++] = node_id;
    }
}

// Build execution order using topological sorting
static void build_execution_order(compiler_context* ctx) {
    // Initialize tracking arrays
    ctx->node_visited = (b32*)calloc(ctx->graph->node_count, sizeof(b32));
    ctx->node_in_progress = (b32*)calloc(ctx->graph->node_count, sizeof(b32));
    
    if (!ctx->node_visited || !ctx->node_in_progress) {
        ctx->has_error = true;
        snprintf(ctx->error_buffer, sizeof(ctx->error_buffer), "Failed to allocate sorting arrays");
        return;
    }
    
    // Reset execution order
    ctx->graph->execution_order_count = 0;
    
    // Find entry points (nodes with no incoming execution connections)
    for (u32 i = 0; i < ctx->graph->node_count; i++) {
        node_id current_id = ctx->graph->node_ids[i];
        
        // Check if this node has incoming execution connections
        b32 has_incoming_exec = false;
        for (u32 j = 0; j < ctx->graph->connection_count; j++) {
            blueprint_connection* conn = &ctx->graph->connections[j];
            if (conn->to_node == current_id && conn->data_type == BP_TYPE_EXEC) {
                has_incoming_exec = true;
                break;
            }
        }
        
        // If no incoming execution, this is an entry point
        if (!has_incoming_exec) {
            topological_sort_dfs(ctx, current_id);
        }
    }
    
    // Process any remaining unvisited nodes (isolated components)
    for (u32 i = 0; i < ctx->graph->node_count; i++) {
        if (!ctx->node_visited[i]) {
            topological_sort_dfs(ctx, ctx->graph->node_ids[i]);
        }
    }
    
    free(ctx->node_visited);
    free(ctx->node_in_progress);
    ctx->node_visited = NULL;
    ctx->node_in_progress = NULL;
}

// ============================================================================
// MAIN COMPILATION FUNCTION
// ============================================================================

void blueprint_compile_graph(blueprint_context* bp_ctx, blueprint_graph* graph) {
    if (!bp_ctx || !graph) return;
    
    blueprint_begin_profile();
    
    compiler_context ctx = {0};
    ctx.bp_ctx = bp_ctx;
    ctx.graph = graph;
    
    // Allocate bytecode buffer
    ctx.bytecode_capacity = BLUEPRINT_MAX_BYTECODE / sizeof(bp_instruction);
    ctx.bytecode = (bp_instruction*)blueprint_alloc(bp_ctx, 
        sizeof(bp_instruction) * ctx.bytecode_capacity);
    
    if (!ctx.bytecode) {
        blueprint_log_debug(bp_ctx, "Failed to allocate bytecode buffer for graph '%s'", graph->name);
        return;
    }
    
    // Allocate constant pool
    ctx.constants = (blueprint_value*)blueprint_alloc(bp_ctx,
        sizeof(blueprint_value) * BLUEPRINT_MAX_CONSTANTS);
    
    if (!ctx.constants) {
        blueprint_log_debug(bp_ctx, "Failed to allocate constant pool for graph '%s'", graph->name);
        return;
    }
    
    blueprint_log_debug(bp_ctx, "Compiling graph '%s' with %u nodes, %u connections", 
                       graph->name, graph->node_count, graph->connection_count);
    
    // Build execution order using topological sorting
    build_execution_order(&ctx);
    if (ctx.has_error) {
        blueprint_log_debug(bp_ctx, "Compilation failed: %s", ctx.error_buffer);
        return;
    }
    
    blueprint_log_debug(bp_ctx, "Execution order built with %u nodes", graph->execution_order_count);
    
    // Compile nodes in execution order
    for (u32 i = 0; i < graph->execution_order_count; i++) {
        node_id current_id = graph->execution_order[i];
        blueprint_node* node = blueprint_get_node(graph, current_id);
        
        if (node) {
            compile_node(&ctx, node);
            if (ctx.has_error) {
                blueprint_log_debug(bp_ctx, "Compilation failed at node '%s': %s", 
                                   node->name, ctx.error_buffer);
                return;
            }
        }
    }
    
    // Patch all pending jumps
    patch_jumps(&ctx);
    if (ctx.has_error) {
        blueprint_log_debug(bp_ctx, "Jump patching failed: %s", ctx.error_buffer);
        return;
    }
    
    // Add halt instruction at the end
    emit_instruction(&ctx, BP_OP_HALT, 0, 0, 0);
    
    // Store compiled bytecode in graph
    graph->bytecode = (u8*)ctx.bytecode;
    graph->bytecode_size = ctx.bytecode_count * sizeof(bp_instruction);
    
    // Copy constants to VM
    if (bp_ctx->vm.constants && ctx.constant_count > 0) {
        memcpy(bp_ctx->vm.constants, ctx.constants, 
               ctx.constant_count * sizeof(blueprint_value));
        bp_ctx->vm.constant_count = ctx.constant_count;
    }
    
    f64 compile_time = blueprint_end_profile();
    
    blueprint_log_debug(bp_ctx, "Graph '%s' compiled successfully:", graph->name);
    blueprint_log_debug(bp_ctx, "  - %u instructions generated", ctx.bytecode_count);
    blueprint_log_debug(bp_ctx, "  - %u constants", ctx.constant_count);
    blueprint_log_debug(bp_ctx, "  - %u bytes bytecode", graph->bytecode_size);
    blueprint_log_debug(bp_ctx, "  - Compilation time: %.2f ms", compile_time);
    
    graph->needs_recompile = false;
}

// ============================================================================
// COMPILER VALIDATION
// ============================================================================

b32 blueprint_validate_graph(blueprint_graph* graph, char* error_buffer, u32 buffer_size) {
    if (!graph) {
        if (error_buffer) snprintf(error_buffer, buffer_size, "Graph is NULL");
        return false;
    }
    
    // Check for orphaned execution nodes
    u32 entry_points = 0;
    for (u32 i = 0; i < graph->node_count; i++) {
        blueprint_node* node = &graph->nodes[i];
        
        if (node->type == NODE_TYPE_BEGIN_PLAY || 
            node->type == NODE_TYPE_TICK ||
            node->type == NODE_TYPE_CUSTOM_EVENT) {
            entry_points++;
        }
    }
    
    if (entry_points == 0) {
        if (error_buffer) snprintf(error_buffer, buffer_size, "Graph has no entry points");
        return false;
    }
    
    // Validate connections
    for (u32 i = 0; i < graph->connection_count; i++) {
        blueprint_connection* conn = &graph->connections[i];
        
        blueprint_node* from_node = blueprint_get_node(graph, conn->from_node);
        blueprint_node* to_node = blueprint_get_node(graph, conn->to_node);
        
        if (!from_node) {
            if (error_buffer) snprintf(error_buffer, buffer_size, 
                                      "Connection %u references invalid from_node %u", i, conn->from_node);
            return false;
        }
        
        if (!to_node) {
            if (error_buffer) snprintf(error_buffer, buffer_size,
                                      "Connection %u references invalid to_node %u", i, conn->to_node);
            return false;
        }
        
        // Validate pins exist
        blueprint_pin* from_pin = blueprint_get_pin(from_node, conn->from_pin);
        blueprint_pin* to_pin = blueprint_get_pin(to_node, conn->to_pin);
        
        if (!from_pin) {
            if (error_buffer) snprintf(error_buffer, buffer_size,
                                      "Connection %u references invalid from_pin %u", i, conn->from_pin);
            return false;
        }
        
        if (!to_pin) {
            if (error_buffer) snprintf(error_buffer, buffer_size,
                                      "Connection %u references invalid to_pin %u", i, conn->to_pin);
            return false;
        }
        
        // Validate pin compatibility
        if (!blueprint_can_connect_pins(from_pin, to_pin)) {
            if (error_buffer) snprintf(error_buffer, buffer_size,
                                      "Connection %u: incompatible pin types %s -> %s",
                                      i, blueprint_type_to_string(from_pin->type),
                                      blueprint_type_to_string(to_pin->type));
            return false;
        }
    }
    
    return true;
}

b32 blueprint_validate_node(blueprint_node* node, char* error_buffer, u32 buffer_size) {
    if (!node) {
        if (error_buffer) snprintf(error_buffer, buffer_size, "Node is NULL");
        return false;
    }
    
    // Validate node has required pins for its type
    switch (node->type) {
        case NODE_TYPE_ADD:
        case NODE_TYPE_SUBTRACT:
        case NODE_TYPE_MULTIPLY:
        case NODE_TYPE_DIVIDE: {
            if (node->input_pin_count < 2) {
                if (error_buffer) snprintf(error_buffer, buffer_size,
                                          "Math node '%s' requires at least 2 input pins", node->name);
                return false;
            }
            if (node->output_pin_count < 1) {
                if (error_buffer) snprintf(error_buffer, buffer_size,
                                          "Math node '%s' requires at least 1 output pin", node->name);
                return false;
            }
            break;
        }
        
        case NODE_TYPE_BRANCH: {
            // Branch should have condition input and two execution outputs
            b32 has_condition = false;
            u32 exec_outputs = 0;
            
            for (u32 i = 0; i < node->input_pin_count; i++) {
                if (node->input_pins[i].type == BP_TYPE_BOOL) {
                    has_condition = true;
                    break;
                }
            }
            
            for (u32 i = 0; i < node->output_pin_count; i++) {
                if (node->output_pins[i].type == BP_TYPE_EXEC) {
                    exec_outputs++;
                }
            }
            
            if (!has_condition) {
                if (error_buffer) snprintf(error_buffer, buffer_size,
                                          "Branch node '%s' requires boolean condition input", node->name);
                return false;
            }
            
            if (exec_outputs < 2) {
                if (error_buffer) snprintf(error_buffer, buffer_size,
                                          "Branch node '%s' requires 2 execution outputs", node->name);
                return false;
            }
            break;
        }
    }
    
    return true;
}

b32 blueprint_validate_connection(blueprint_connection* connection, char* error_buffer, u32 buffer_size) {
    if (!connection) {
        if (error_buffer) snprintf(error_buffer, buffer_size, "Connection is NULL");
        return false;
    }
    
    // Basic validation - detailed validation requires graph context
    if (connection->from_node == connection->to_node) {
        if (error_buffer) snprintf(error_buffer, buffer_size,
                                  "Connection cannot connect node to itself");
        return false;
    }
    
    return true;
}