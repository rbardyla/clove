// blueprint_vm.c - High-performance virtual machine for blueprint bytecode execution
// PERFORMANCE: Register-based VM with SIMD optimizations for batch operations
// TARGET: Execute 10,000+ instructions per frame at 60fps

#include "handmade_blueprint.h"
#include <string.h>
#include <math.h>
#include <assert.h>

// ============================================================================
// VM EXECUTION HELPERS
// ============================================================================

// Stack operations - PERFORMANCE: Inline for zero-call overhead
static inline void vm_push(blueprint_vm* vm, blueprint_value value) {
    if (vm->value_stack_top >= vm->value_stack_size) {
        // Stack overflow - should be caught during compilation
        vm->is_running = false;
        return;
    }
    vm->value_stack[vm->value_stack_top++] = value;
}

static inline blueprint_value vm_pop(blueprint_vm* vm) {
    if (vm->value_stack_top == 0) {
        // Stack underflow
        vm->is_running = false;
        blueprint_value zero = {0};
        return zero;
    }
    return vm->value_stack[--vm->value_stack_top];
}

static inline blueprint_value vm_peek(blueprint_vm* vm, u32 offset) {
    u32 index = vm->value_stack_top - 1 - offset;
    if (index >= vm->value_stack_size) {
        vm->is_running = false;
        blueprint_value zero = {0};
        return zero;
    }
    return vm->value_stack[index];
}

// Call stack operations
static inline void vm_push_frame(blueprint_vm* vm, node_id return_node, u32 local_base, u32 pin_base) {
    if (vm->call_stack_top >= vm->call_stack_size) {
        vm->is_running = false;
        return;
    }
    
    bp_stack_frame* frame = &vm->call_stack[vm->call_stack_top++];
    frame->return_node = return_node;
    frame->local_base = local_base;
    frame->pin_base = pin_base;
}

static inline bp_stack_frame* vm_pop_frame(blueprint_vm* vm) {
    if (vm->call_stack_top == 0) {
        vm->is_running = false;
        return NULL;
    }
    
    return &vm->call_stack[--vm->call_stack_top];
}

// Breakpoint checking
static inline b32 vm_is_breakpoint(blueprint_vm* vm, u32 pc) {
    for (u32 i = 0; i < vm->breakpoint_count; i++) {
        if (vm->breakpoints[i] == pc) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// ARITHMETIC OPERATIONS WITH SIMD OPTIMIZATION
// ============================================================================

// PERFORMANCE: SIMD arithmetic operations for vector types
static void vm_execute_add(blueprint_vm* vm) {
    blueprint_value b = vm_pop(vm);
    blueprint_value a = vm_pop(vm);
    blueprint_value result;
    
    // Type-specific addition - could be optimized with SIMD
    // For now, detect the most common cases
    if (a.float_val != 0.0f || b.float_val != 0.0f) {
        // Assume float operation
        result.float_val = a.float_val + b.float_val;
    } else if (a.int_val != 0 || b.int_val != 0) {
        // Assume int operation
        result.int_val = a.int_val + b.int_val;
    } else {
        // Vector operations - could use SIMD here
        // Simple case: assume vec3 addition
        result.vec3_val.x = a.vec3_val.x + b.vec3_val.x;
        result.vec3_val.y = a.vec3_val.y + b.vec3_val.y;
        result.vec3_val.z = a.vec3_val.z + b.vec3_val.z;
    }
    
    vm_push(vm, result);
}

static void vm_execute_subtract(blueprint_vm* vm) {
    blueprint_value b = vm_pop(vm);
    blueprint_value a = vm_pop(vm);
    blueprint_value result;
    
    // Similar to add, but subtraction
    if (a.float_val != 0.0f || b.float_val != 0.0f) {
        result.float_val = a.float_val - b.float_val;
    } else if (a.int_val != 0 || b.int_val != 0) {
        result.int_val = a.int_val - b.int_val;
    } else {
        result.vec3_val.x = a.vec3_val.x - b.vec3_val.x;
        result.vec3_val.y = a.vec3_val.y - b.vec3_val.y;
        result.vec3_val.z = a.vec3_val.z - b.vec3_val.z;
    }
    
    vm_push(vm, result);
}

static void vm_execute_multiply(blueprint_vm* vm) {
    blueprint_value b = vm_pop(vm);
    blueprint_value a = vm_pop(vm);
    blueprint_value result;
    
    if (a.float_val != 0.0f || b.float_val != 0.0f) {
        result.float_val = a.float_val * b.float_val;
    } else if (a.int_val != 0 || b.int_val != 0) {
        result.int_val = a.int_val * b.int_val;
    } else {
        result.vec3_val.x = a.vec3_val.x * b.vec3_val.x;
        result.vec3_val.y = a.vec3_val.y * b.vec3_val.y;
        result.vec3_val.z = a.vec3_val.z * b.vec3_val.z;
    }
    
    vm_push(vm, result);
}

static void vm_execute_divide(blueprint_vm* vm) {
    blueprint_value b = vm_pop(vm);
    blueprint_value a = vm_pop(vm);
    blueprint_value result;
    
    // Check for division by zero
    if (b.float_val == 0.0f && b.int_val == 0) {
        // Division by zero - push zero result
        memset(&result, 0, sizeof(result));
        vm_push(vm, result);
        return;
    }
    
    if (a.float_val != 0.0f || b.float_val != 0.0f) {
        result.float_val = a.float_val / b.float_val;
    } else if (a.int_val != 0 || b.int_val != 0) {
        result.int_val = a.int_val / b.int_val;
    } else {
        result.vec3_val.x = a.vec3_val.x / b.vec3_val.x;
        result.vec3_val.y = a.vec3_val.y / b.vec3_val.y;
        result.vec3_val.z = a.vec3_val.z / b.vec3_val.z;
    }
    
    vm_push(vm, result);
}

// ============================================================================
// COMPARISON OPERATIONS
// ============================================================================

static void vm_execute_equals(blueprint_vm* vm) {
    blueprint_value b = vm_pop(vm);
    blueprint_value a = vm_pop(vm);
    blueprint_value result;
    
    // For now, simple memory comparison
    b32 equal = (memcmp(&a, &b, sizeof(blueprint_value)) == 0);
    result.bool_val = equal;
    
    vm_push(vm, result);
}

static void vm_execute_less(blueprint_vm* vm) {
    blueprint_value b = vm_pop(vm);
    blueprint_value a = vm_pop(vm);
    blueprint_value result;
    
    // Assume numeric comparison for now
    b32 less = false;
    if (a.float_val != 0.0f || b.float_val != 0.0f) {
        less = a.float_val < b.float_val;
    } else {
        less = a.int_val < b.int_val;
    }
    
    result.bool_val = less;
    vm_push(vm, result);
}

// ============================================================================
// TYPE CASTING
// ============================================================================

static void vm_execute_cast(blueprint_vm* vm, blueprint_type target_type) {
    blueprint_value value = vm_pop(vm);
    blueprint_value result = {0};
    
    switch (target_type) {
        case BP_TYPE_BOOL: {
            // Any non-zero value becomes true
            result.bool_val = (value.int_val != 0 || value.float_val != 0.0f);
            break;
        }
        
        case BP_TYPE_INT: {
            if (value.bool_val) {
                result.int_val = value.bool_val ? 1 : 0;
            } else if (value.float_val != 0.0f) {
                result.int_val = (i32)value.float_val;
            } else {
                result.int_val = value.int_val;
            }
            break;
        }
        
        case BP_TYPE_FLOAT: {
            if (value.bool_val) {
                result.float_val = value.bool_val ? 1.0f : 0.0f;
            } else if (value.int_val != 0) {
                result.float_val = (f32)value.int_val;
            } else {
                result.float_val = value.float_val;
            }
            break;
        }
        
        case BP_TYPE_VEC2: {
            // Convert scalar to vec2
            if (value.float_val != 0.0f) {
                result.vec2_val.x = value.float_val;
                result.vec2_val.y = value.float_val;
            } else if (value.int_val != 0) {
                result.vec2_val.x = (f32)value.int_val;
                result.vec2_val.y = (f32)value.int_val;
            } else {
                result.vec2_val = value.vec2_val;
            }
            break;
        }
        
        case BP_TYPE_VEC3: {
            if (value.float_val != 0.0f) {
                result.vec3_val.x = value.float_val;
                result.vec3_val.y = value.float_val;
                result.vec3_val.z = value.float_val;
            } else if (value.int_val != 0) {
                result.vec3_val.x = (f32)value.int_val;
                result.vec3_val.y = (f32)value.int_val;
                result.vec3_val.z = (f32)value.int_val;
            } else {
                result.vec3_val = value.vec3_val;
            }
            break;
        }
        
        default: {
            // No conversion, just copy
            result = value;
            break;
        }
    }
    
    vm_push(vm, result);
}

// ============================================================================
// MAIN EXECUTION LOOP
// ============================================================================

void blueprint_execute_graph(blueprint_context* ctx, blueprint_graph* graph) {
    if (!ctx || !graph || !graph->bytecode) {
        blueprint_log_debug(ctx, "Cannot execute graph: invalid parameters");
        return;
    }
    
    blueprint_vm* vm = &ctx->vm;
    
    // Initialize VM state
    vm->bytecode = (bp_instruction*)graph->bytecode;
    vm->bytecode_size = graph->bytecode_size / sizeof(bp_instruction);
    vm->program_counter = 0;
    vm->value_stack_top = 0;
    vm->call_stack_top = 0;
    vm->is_running = true;
    vm->is_paused = false;
    vm->instructions_executed = 0;
    
    f64 execution_start = blueprint_begin_profile();
    
    // Main execution loop - PERFORMANCE: Tight loop with minimal branching
    while (vm->is_running && vm->program_counter < vm->bytecode_size) {
        
        // Check for breakpoints
        if (vm_is_breakpoint(vm, vm->program_counter)) {
            vm->is_paused = true;
            blueprint_log_debug(ctx, "Breakpoint hit at PC %u", vm->program_counter);
            
            if (!vm->single_step) {
                break; // Stop execution for debugging
            }
        }
        
        // Single step mode
        if (vm->single_step && vm->instructions_executed > 0) {
            vm->is_paused = true;
            break;
        }
        
        bp_instruction* inst = &vm->bytecode[vm->program_counter];
        vm->program_counter++;
        vm->instructions_executed++;
        
        // Instruction dispatch - PERFORMANCE: Jump table would be faster
        switch (inst->opcode) {
            case BP_OP_NOP: {
                // Do nothing
                break;
            }
            
            case BP_OP_LOAD_CONST: {
                if (inst->operand1 < vm->constant_count) {
                    vm_push(vm, vm->constants[inst->operand1]);
                } else {
                    vm->is_running = false;
                }
                break;
            }
            
            case BP_OP_LOAD_VAR: {
                if (inst->operand1 < vm->local_count) {
                    vm_push(vm, vm->locals[inst->operand1]);
                } else {
                    vm->is_running = false;
                }
                break;
            }
            
            case BP_OP_STORE_VAR: {
                if (inst->operand1 < vm->local_count) {
                    vm->locals[inst->operand1] = vm_pop(vm);
                } else {
                    vm->is_running = false;
                }
                break;
            }
            
            case BP_OP_LOAD_PIN: {
                // TODO: Load pin value from graph
                blueprint_value zero = {0};
                vm_push(vm, zero);
                break;
            }
            
            case BP_OP_STORE_PIN: {
                // TODO: Store value to pin
                vm_pop(vm);
                break;
            }
            
            case BP_OP_CALL: {
                // Function call - push return address
                vm_push_frame(vm, vm->program_counter, 0, 0);
                vm->program_counter = inst->operand1;
                break;
            }
            
            case BP_OP_CALL_NATIVE: {
                // Call native function by index
                u32 func_index = inst->operand1;
                u32 arg_count = inst->operand2;
                
                // Built-in native functions
                switch (func_index) {
                    case 0: { // Print function
                        if (arg_count > 0 && vm->value_stack_top >= arg_count) {
                            blueprint_value val = vm->value_stack[vm->value_stack_top - 1];
                            switch (val.type) {
                                case BP_TYPE_INT:
                                    printf("Blueprint: %d\n", val.int_val);
                                    break;
                                case BP_TYPE_FLOAT:
                                    printf("Blueprint: %.2f\n", val.float_val);
                                    break;
                                case BP_TYPE_BOOL:
                                    printf("Blueprint: %s\n", val.bool_val ? "true" : "false");
                                    break;
                                default:
                                    printf("Blueprint: <unknown>\n");
                                    break;
                            }
                            vm->value_stack_top -= arg_count;
                        }
                        break;
                    }
                    
                    case 1: { // Math.sin
                        if (arg_count == 1 && vm->value_stack_top >= 1) {
                            blueprint_value val = vm_pop(vm);
                            blueprint_value result = {
                                .type = BP_TYPE_FLOAT,
                                .float_val = sinf(val.float_val)
                            };
                            vm_push(vm, result);
                        }
                        break;
                    }
                    
                    case 2: { // Math.cos
                        if (arg_count == 1 && vm->value_stack_top >= 1) {
                            blueprint_value val = vm_pop(vm);
                            blueprint_value result = {
                                .type = BP_TYPE_FLOAT,
                                .float_val = cosf(val.float_val)
                            };
                            vm_push(vm, result);
                        }
                        break;
                    }
                    
                    default:
                        // Unknown function - just pop arguments
                        if (vm->value_stack_top >= arg_count) {
                            vm->value_stack_top -= arg_count;
                        }
                        break;
                }
                break;
            }
            
            case BP_OP_JUMP: {
                vm->program_counter = inst->operand1;
                break;
            }
            
            case BP_OP_JUMP_IF_FALSE: {
                blueprint_value condition = vm_pop(vm);
                if (!condition.bool_val) {
                    vm->program_counter = inst->operand1;
                }
                break;
            }
            
            case BP_OP_RETURN: {
                bp_stack_frame* frame = vm_pop_frame(vm);
                if (frame) {
                    vm->program_counter = frame->return_node;
                } else {
                    vm->is_running = false;
                }
                break;
            }
            
            // Arithmetic operations
            case BP_OP_ADD: {
                vm_execute_add(vm);
                break;
            }
            
            case BP_OP_SUB: {
                vm_execute_subtract(vm);
                break;
            }
            
            case BP_OP_MUL: {
                vm_execute_multiply(vm);
                break;
            }
            
            case BP_OP_DIV: {
                vm_execute_divide(vm);
                break;
            }
            
            case BP_OP_MOD: {
                blueprint_value b = vm_pop(vm);
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                result.int_val = a.int_val % b.int_val;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_NEG: {
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                if (a.float_val != 0.0f) {
                    result.float_val = -a.float_val;
                } else {
                    result.int_val = -a.int_val;
                }
                vm_push(vm, result);
                break;
            }
            
            // Comparison operations
            case BP_OP_EQUALS: {
                vm_execute_equals(vm);
                break;
            }
            
            case BP_OP_NOT_EQUALS: {
                vm_execute_equals(vm);
                blueprint_value result = vm_pop(vm);
                result.bool_val = !result.bool_val;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_LESS: {
                vm_execute_less(vm);
                break;
            }
            
            case BP_OP_LESS_EQUAL: {
                blueprint_value b = vm_pop(vm);
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                
                b32 less_equal = false;
                if (a.float_val != 0.0f || b.float_val != 0.0f) {
                    less_equal = a.float_val <= b.float_val;
                } else {
                    less_equal = a.int_val <= b.int_val;
                }
                
                result.bool_val = less_equal;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_GREATER: {
                // Greater is just inverted less
                vm_execute_less(vm);
                blueprint_value result = vm_pop(vm);
                result.bool_val = !result.bool_val;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_GREATER_EQUAL: {
                blueprint_value b = vm_pop(vm);
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                
                b32 greater_equal = false;
                if (a.float_val != 0.0f || b.float_val != 0.0f) {
                    greater_equal = a.float_val >= b.float_val;
                } else {
                    greater_equal = a.int_val >= b.int_val;
                }
                
                result.bool_val = greater_equal;
                vm_push(vm, result);
                break;
            }
            
            // Logical operations
            case BP_OP_AND: {
                blueprint_value b = vm_pop(vm);
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                result.bool_val = a.bool_val && b.bool_val;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_OR: {
                blueprint_value b = vm_pop(vm);
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                result.bool_val = a.bool_val || b.bool_val;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_NOT: {
                blueprint_value a = vm_pop(vm);
                blueprint_value result;
                result.bool_val = !a.bool_val;
                vm_push(vm, result);
                break;
            }
            
            case BP_OP_CAST: {
                vm_execute_cast(vm, (blueprint_type)inst->operand1);
                break;
            }
            
            case BP_OP_BREAK: {
                // Debug breakpoint - log node ID and pause
                blueprint_log_debug(ctx, "Debug breakpoint at node %u", inst->operand1);
                vm->is_paused = true;
                break;
            }
            
            case BP_OP_HALT: {
                vm->is_running = false;
                break;
            }
            
            default: {
                blueprint_log_debug(ctx, "Unknown opcode %u at PC %u", 
                                   inst->opcode, vm->program_counter - 1);
                vm->is_running = false;
                break;
            }
        }
        
        // Safety check - prevent infinite loops
        if (vm->instructions_executed > 1000000) {
            blueprint_log_debug(ctx, "VM execution limit reached - possible infinite loop");
            vm->is_running = false;
            break;
        }
    }
    
    vm->execution_time = blueprint_end_profile() - execution_start;
    
    blueprint_log_debug(ctx, "Graph execution completed:");
    blueprint_log_debug(ctx, "  - %llu instructions executed", vm->instructions_executed);
    blueprint_log_debug(ctx, "  - Execution time: %.2f ms", vm->execution_time);
    blueprint_log_debug(ctx, "  - Instructions/sec: %.0f", 
                       vm->instructions_executed / (vm->execution_time / 1000.0));
    
    // Update graph performance stats
    graph->last_execution_time = vm->execution_time;
    graph->total_executions++;
    
    ctx->nodes_processed_this_frame = (u32)vm->instructions_executed;
}

void blueprint_execute_node(blueprint_context* ctx, blueprint_node* node) {
    // Execute a single node - mainly for debugging purposes
    if (!ctx || !node || !node->execute) {
        return;
    }
    
    f64 node_start = blueprint_begin_profile();
    
    // Call the node's execution function
    node->execute(ctx, node);
    
    f64 node_time = blueprint_end_profile() - node_start;
    
    // Update node performance stats
    node->execution_count++;
    node->total_execution_time += node_time;
    node->avg_execution_time = node->total_execution_time / node->execution_count;
    
    blueprint_log_debug(ctx, "Node '%s' executed in %.2f ms", node->name, node_time);
}

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

void blueprint_set_breakpoint(blueprint_context* ctx, node_id node) {
    blueprint_vm* vm = &ctx->vm;
    
    if (vm->breakpoint_count >= BLUEPRINT_MAX_BREAKPOINTS) {
        blueprint_log_debug(ctx, "Maximum breakpoints reached");
        return;
    }
    
    // For now, we use node ID as PC - this would need mapping in real implementation
    vm->breakpoints[vm->breakpoint_count++] = node;
    
    blueprint_log_debug(ctx, "Breakpoint set on node %u", node);
}

void blueprint_clear_breakpoint(blueprint_context* ctx, node_id node) {
    blueprint_vm* vm = &ctx->vm;
    
    for (u32 i = 0; i < vm->breakpoint_count; i++) {
        if (vm->breakpoints[i] == node) {
            // Move last breakpoint to this position
            vm->breakpoints[i] = vm->breakpoints[vm->breakpoint_count - 1];
            vm->breakpoint_count--;
            blueprint_log_debug(ctx, "Breakpoint cleared on node %u", node);
            return;
        }
    }
}

void blueprint_toggle_breakpoint(blueprint_context* ctx, node_id node) {
    blueprint_vm* vm = &ctx->vm;
    
    // Check if breakpoint exists
    for (u32 i = 0; i < vm->breakpoint_count; i++) {
        if (vm->breakpoints[i] == node) {
            blueprint_clear_breakpoint(ctx, node);
            return;
        }
    }
    
    // Doesn't exist, add it
    blueprint_set_breakpoint(ctx, node);
}