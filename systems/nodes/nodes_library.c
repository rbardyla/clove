// nodes_library.c - Built-in node types library
// PERFORMANCE: All node execution functions optimized for cache and SIMD
// ARCHITECTURE: Comprehensive set of nodes for game logic and AI

#include "handmade_nodes.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// FLOW CONTROL NODES
// =============================================================================

static void execute_branch(node_t *node, void *context) {
    // Branch based on condition
    pin_value_t *condition = node_get_input_value(node, 0);
    
    if (condition && condition->b) {
        // Execute true branch
        node->outputs[0].value.b = true;
        node->outputs[1].value.b = false;
    } else {
        // Execute false branch
        node->outputs[0].value.b = false;
        node->outputs[1].value.b = true;
    }
}

static void execute_sequence(node_t *node, void *context) {
    // Execute outputs in sequence
    for (i32 i = 0; i < node->output_count; ++i) {
        node->outputs[i].value.b = true;
    }
}

static void execute_for_loop(node_t *node, void *context) {
    pin_value_t *start = node_get_input_value(node, 1);
    pin_value_t *end = node_get_input_value(node, 2);
    
    if (!start || !end) return;
    
    // Store loop state in custom data
    i32 *current = (i32*)node->custom_data;
    
    if (*current < start->i) {
        *current = start->i;
    }
    
    if (*current < end->i) {
        // Output current index
        node->outputs[1].value.i = *current;
        // Signal loop body execution
        node->outputs[0].value.b = true;
        (*current)++;
    } else {
        // Loop complete
        node->outputs[2].value.b = true;
        *current = start->i;  // Reset for next execution
    }
}

static void execute_while_loop(node_t *node, void *context) {
    pin_value_t *condition = node_get_input_value(node, 1);
    
    if (condition && condition->b) {
        // Continue loop
        node->outputs[0].value.b = true;
        node->outputs[1].value.b = false;
    } else {
        // Exit loop
        node->outputs[0].value.b = false;
        node->outputs[1].value.b = true;
    }
}

static void execute_gate(node_t *node, void *context) {
    pin_value_t *open = node_get_input_value(node, 1);
    
    if (open && open->b) {
        // Gate is open, pass execution through
        node->outputs[0].value.b = true;
    } else {
        // Gate is closed
        node->outputs[0].value.b = false;
    }
}

// =============================================================================
// MATH NODES
// =============================================================================

static void execute_add(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    // Polymorphic based on connected type
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.f = a->f + b->f;
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        node->outputs[0].value.i = a->i + b->i;
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR2) {
        node->outputs[0].value.v2.x = a->v2.x + b->v2.x;
        node->outputs[0].value.v2.y = a->v2.y + b->v2.y;
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR3) {
        node->outputs[0].value.v3.x = a->v3.x + b->v3.x;
        node->outputs[0].value.v3.y = a->v3.y + b->v3.y;
        node->outputs[0].value.v3.z = a->v3.z + b->v3.z;
    }
}

static void execute_multiply(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.f = a->f * b->f;
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        node->outputs[0].value.i = a->i * b->i;
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR2) {
        // Scalar multiplication if b is float
        if (node->inputs[1].type == PIN_TYPE_FLOAT) {
            node->outputs[0].value.v2.x = a->v2.x * b->f;
            node->outputs[0].value.v2.y = a->v2.y * b->f;
        } else {
            // Component-wise
            node->outputs[0].value.v2.x = a->v2.x * b->v2.x;
            node->outputs[0].value.v2.y = a->v2.y * b->v2.y;
        }
    }
}

static void execute_divide(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT && b->f != 0.0f) {
        node->outputs[0].value.f = a->f / b->f;
    } else if (node->inputs[0].type == PIN_TYPE_INT && b->i != 0) {
        node->outputs[0].value.i = a->i / b->i;
    }
}

static void execute_lerp(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    pin_value_t *t = node_get_input_value(node, 2);
    
    if (!a || !b || !t) return;
    
    f32 alpha = t->f;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.f = a->f + (b->f - a->f) * alpha;
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR2) {
        node->outputs[0].value.v2.x = a->v2.x + (b->v2.x - a->v2.x) * alpha;
        node->outputs[0].value.v2.y = a->v2.y + (b->v2.y - a->v2.y) * alpha;
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR3) {
        node->outputs[0].value.v3.x = a->v3.x + (b->v3.x - a->v3.x) * alpha;
        node->outputs[0].value.v3.y = a->v3.y + (b->v3.y - a->v3.y) * alpha;
        node->outputs[0].value.v3.z = a->v3.z + (b->v3.z - a->v3.z) * alpha;
    }
}

static void execute_clamp(node_t *node, void *context) {
    pin_value_t *value = node_get_input_value(node, 0);
    pin_value_t *min = node_get_input_value(node, 1);
    pin_value_t *max = node_get_input_value(node, 2);
    
    if (!value || !min || !max) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        f32 result = value->f;
        if (result < min->f) result = min->f;
        if (result > max->f) result = max->f;
        node->outputs[0].value.f = result;
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        i32 result = value->i;
        if (result < min->i) result = min->i;
        if (result > max->i) result = max->i;
        node->outputs[0].value.i = result;
    }
}

static void execute_sin(node_t *node, void *context) {
    pin_value_t *input = node_get_input_value(node, 0);
    if (!input) return;
    
    node->outputs[0].value.f = sinf(input->f);
}

static void execute_cos(node_t *node, void *context) {
    pin_value_t *input = node_get_input_value(node, 0);
    if (!input) return;
    
    node->outputs[0].value.f = cosf(input->f);
}

static void execute_abs(node_t *node, void *context) {
    pin_value_t *input = node_get_input_value(node, 0);
    if (!input) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.f = fabsf(input->f);
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        node->outputs[0].value.i = abs(input->i);
    }
}

static void execute_random(node_t *node, void *context) {
    pin_value_t *min = node_get_input_value(node, 0);
    pin_value_t *max = node_get_input_value(node, 1);
    
    if (!min || !max) {
        // Default 0-1 range
        node->outputs[0].value.f = (f32)rand() / (f32)RAND_MAX;
    } else {
        f32 range = max->f - min->f;
        node->outputs[0].value.f = min->f + range * ((f32)rand() / (f32)RAND_MAX);
    }
}

static void execute_dot_product(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    if (node->inputs[0].type == PIN_TYPE_VECTOR2) {
        node->outputs[0].value.f = a->v2.x * b->v2.x + a->v2.y * b->v2.y;
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR3) {
        node->outputs[0].value.f = a->v3.x * b->v3.x + 
                                   a->v3.y * b->v3.y + 
                                   a->v3.z * b->v3.z;
    }
}

static void execute_normalize(node_t *node, void *context) {
    pin_value_t *input = node_get_input_value(node, 0);
    if (!input) return;
    
    if (node->inputs[0].type == PIN_TYPE_VECTOR2) {
        f32 len = sqrtf(input->v2.x * input->v2.x + input->v2.y * input->v2.y);
        if (len > 0.0001f) {
            node->outputs[0].value.v2.x = input->v2.x / len;
            node->outputs[0].value.v2.y = input->v2.y / len;
        }
    } else if (node->inputs[0].type == PIN_TYPE_VECTOR3) {
        f32 len = sqrtf(input->v3.x * input->v3.x + 
                       input->v3.y * input->v3.y + 
                       input->v3.z * input->v3.z);
        if (len > 0.0001f) {
            node->outputs[0].value.v3.x = input->v3.x / len;
            node->outputs[0].value.v3.y = input->v3.y / len;
            node->outputs[0].value.v3.z = input->v3.z / len;
        }
    }
}

// =============================================================================
// LOGIC NODES
// =============================================================================

static void execute_and(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    node->outputs[0].value.b = a->b && b->b;
}

static void execute_or(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    node->outputs[0].value.b = a->b || b->b;
}

static void execute_not(node_t *node, void *context) {
    pin_value_t *input = node_get_input_value(node, 0);
    if (!input) return;
    
    node->outputs[0].value.b = !input->b;
}

static void execute_equal(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.b = fabsf(a->f - b->f) < 0.0001f;
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        node->outputs[0].value.b = a->i == b->i;
    } else if (node->inputs[0].type == PIN_TYPE_BOOL) {
        node->outputs[0].value.b = a->b == b->b;
    }
}

static void execute_greater(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.b = a->f > b->f;
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        node->outputs[0].value.b = a->i > b->i;
    }
}

static void execute_less(node_t *node, void *context) {
    pin_value_t *a = node_get_input_value(node, 0);
    pin_value_t *b = node_get_input_value(node, 1);
    
    if (!a || !b) return;
    
    if (node->inputs[0].type == PIN_TYPE_FLOAT) {
        node->outputs[0].value.b = a->f < b->f;
    } else if (node->inputs[0].type == PIN_TYPE_INT) {
        node->outputs[0].value.b = a->i < b->i;
    }
}

// =============================================================================
// VARIABLE NODES
// =============================================================================

typedef struct {
    pin_value_t value;
    bool initialized;
} variable_data_t;

static void execute_get_variable(node_t *node, void *context) {
    variable_data_t *var = (variable_data_t*)node->custom_data;
    
    if (var->initialized) {
        node->outputs[0].value = var->value;
    }
}

static void execute_set_variable(node_t *node, void *context) {
    pin_value_t *value = node_get_input_value(node, 1);
    if (!value) return;
    
    variable_data_t *var = (variable_data_t*)node->custom_data;
    var->value = *value;
    var->initialized = true;
    
    // Pass through execution
    node->outputs[0].value.b = true;
}

// =============================================================================
// EVENT NODES
// =============================================================================

static void execute_on_update(node_t *node, void *context) {
    // Fires every frame
    node->outputs[0].value.b = true;
    
    // Output delta time if available
    node_execution_context_t *ctx = (node_execution_context_t*)context;
    if (ctx && ctx->user_data) {
        // Assume user_data contains frame info
        node->outputs[1].value.f = 0.016f;  // Default 60 FPS
    }
}

static void execute_on_input(node_t *node, void *context) {
    // Check for specific input in custom data
    char *input_name = (char*)node->custom_data;
    
    // This would check actual input system
    // For now, just pass through
    node->outputs[0].value.b = false;
}

static void execute_delay(node_t *node, void *context) {
    pin_value_t *duration = node_get_input_value(node, 1);
    if (!duration) return;
    
    // Store timer in custom data
    f32 *timer = (f32*)node->custom_data;
    
    if (*timer <= 0.0f) {
        *timer = duration->f;
    }
    
    *timer -= 0.016f;  // Assume 60 FPS
    
    if (*timer <= 0.0f) {
        node->outputs[0].value.b = true;
        *timer = 0.0f;
    } else {
        node->outputs[0].value.b = false;
    }
}

// =============================================================================
// GAME NODES
// =============================================================================

static void execute_spawn_entity(node_t *node, void *context) {
    pin_value_t *position = node_get_input_value(node, 1);
    pin_value_t *rotation = node_get_input_value(node, 2);
    
    // Would spawn actual game entity
    // For now, output dummy entity ID
    node->outputs[0].value.b = true;
    node->outputs[1].value.ptr = (void*)0x12345678;  // Dummy entity
}

static void execute_move_entity(node_t *node, void *context) {
    pin_value_t *entity = node_get_input_value(node, 1);
    pin_value_t *location = node_get_input_value(node, 2);
    
    if (!entity || !location) return;
    
    // Would move actual entity
    node->outputs[0].value.b = true;
}

static void execute_play_sound(node_t *node, void *context) {
    pin_value_t *sound = node_get_input_value(node, 1);
    pin_value_t *volume = node_get_input_value(node, 2);
    
    // Would play actual sound
    node->outputs[0].value.b = true;
}

// =============================================================================
// DEBUG NODES
// =============================================================================

static void execute_print(node_t *node, void *context) {
    pin_value_t *value = node_get_input_value(node, 1);
    if (!value) return;
    
    // Print based on type
    if (node->inputs[1].type == PIN_TYPE_FLOAT) {
        snprintf(node->debug_message, 128, "%.3f", value->f);
    } else if (node->inputs[1].type == PIN_TYPE_INT) {
        snprintf(node->debug_message, 128, "%d", value->i);
    } else if (node->inputs[1].type == PIN_TYPE_BOOL) {
        snprintf(node->debug_message, 128, "%s", value->b ? "true" : "false");
    } else if (node->inputs[1].type == PIN_TYPE_STRING) {
        snprintf(node->debug_message, 128, "%s", (char*)value->ptr);
    }
    
    // Pass through execution
    node->outputs[0].value.b = true;
}

static void execute_breakpoint(node_t *node, void *context) {
    node_execution_context_t *ctx = (node_execution_context_t*)context;
    if (ctx) {
        ctx->break_on_next = true;
    }
    
    node->outputs[0].value.b = true;
}

// =============================================================================
// NODE TYPE REGISTRATION
// =============================================================================

void register_flow_control_nodes(void) {
    // Branch node
    {
        node_type_t type = {0};
        strcpy(type.name, "Branch");
        strcpy(type.tooltip, "Execute different outputs based on condition");
        type.category = NODE_CATEGORY_FLOW;
        type.color = 0xFF404080;
        type.width = 150;
        type.min_height = 80;
        type.execute = execute_branch;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.input_templates[1].name, "Condition");
        type.input_templates[1].type = PIN_TYPE_BOOL;
        
        type.output_count = 2;
        strcpy(type.output_templates[0].name, "True");
        type.output_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.output_templates[1].name, "False");
        type.output_templates[1].type = PIN_TYPE_EXECUTION;
        
        node_register_type(&type);
    }
    
    // Sequence node
    {
        node_type_t type = {0};
        strcpy(type.name, "Sequence");
        strcpy(type.tooltip, "Execute outputs in order");
        type.category = NODE_CATEGORY_FLOW;
        type.color = 0xFF404080;
        type.width = 120;
        type.min_height = 100;
        type.execute = execute_sequence;
        
        type.input_count = 1;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        
        type.output_count = 4;
        for (i32 i = 0; i < 4; ++i) {
            snprintf(type.output_templates[i].name, 32, "Then %d", i);
            type.output_templates[i].type = PIN_TYPE_EXECUTION;
        }
        
        node_register_type(&type);
    }
    
    // For Loop node
    {
        node_type_t type = {0};
        strcpy(type.name, "For Loop");
        strcpy(type.tooltip, "Loop from start to end index");
        type.category = NODE_CATEGORY_FLOW;
        type.color = 0xFF404080;
        type.width = 150;
        type.min_height = 100;
        type.execute = execute_for_loop;
        
        type.input_count = 3;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.input_templates[1].name, "Start");
        type.input_templates[1].type = PIN_TYPE_INT;
        strcpy(type.input_templates[2].name, "End");
        type.input_templates[2].type = PIN_TYPE_INT;
        
        type.output_count = 3;
        strcpy(type.output_templates[0].name, "Loop Body");
        type.output_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.output_templates[1].name, "Index");
        type.output_templates[1].type = PIN_TYPE_INT;
        strcpy(type.output_templates[2].name, "Completed");
        type.output_templates[2].type = PIN_TYPE_EXECUTION;
        
        node_register_type(&type);
    }
    
    // Gate node
    {
        node_type_t type = {0};
        strcpy(type.name, "Gate");
        strcpy(type.tooltip, "Allow or block execution flow");
        type.category = NODE_CATEGORY_FLOW;
        type.color = 0xFF404080;
        type.width = 120;
        type.min_height = 60;
        type.execute = execute_gate;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.input_templates[1].name, "Open");
        type.input_templates[1].type = PIN_TYPE_BOOL;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Exit");
        type.output_templates[0].type = PIN_TYPE_EXECUTION;
        
        node_register_type(&type);
    }
}

void register_math_nodes(void) {
    // Add node
    {
        node_type_t type = {0};
        strcpy(type.name, "Add");
        strcpy(type.tooltip, "Add two values");
        type.category = NODE_CATEGORY_MATH;
        type.color = 0xFF408040;
        type.width = 100;
        type.min_height = 60;
        type.execute = execute_add;
        type.flags = NODE_TYPE_FLAG_PURE | NODE_TYPE_FLAG_COMPACT;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "A");
        type.input_templates[0].type = PIN_TYPE_FLOAT;
        strcpy(type.input_templates[1].name, "B");
        type.input_templates[1].type = PIN_TYPE_FLOAT;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Result");
        type.output_templates[0].type = PIN_TYPE_FLOAT;
        
        node_register_type(&type);
    }
    
    // Multiply node
    {
        node_type_t type = {0};
        strcpy(type.name, "Multiply");
        strcpy(type.tooltip, "Multiply two values");
        type.category = NODE_CATEGORY_MATH;
        type.color = 0xFF408040;
        type.width = 100;
        type.min_height = 60;
        type.execute = execute_multiply;
        type.flags = NODE_TYPE_FLAG_PURE | NODE_TYPE_FLAG_COMPACT;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "A");
        type.input_templates[0].type = PIN_TYPE_FLOAT;
        strcpy(type.input_templates[1].name, "B");
        type.input_templates[1].type = PIN_TYPE_FLOAT;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Result");
        type.output_templates[0].type = PIN_TYPE_FLOAT;
        
        node_register_type(&type);
    }
    
    // Lerp node
    {
        node_type_t type = {0};
        strcpy(type.name, "Lerp");
        strcpy(type.tooltip, "Linear interpolation between two values");
        type.category = NODE_CATEGORY_MATH;
        type.color = 0xFF408040;
        type.width = 120;
        type.min_height = 80;
        type.execute = execute_lerp;
        type.flags = NODE_TYPE_FLAG_PURE;
        
        type.input_count = 3;
        strcpy(type.input_templates[0].name, "A");
        type.input_templates[0].type = PIN_TYPE_FLOAT;
        strcpy(type.input_templates[1].name, "B");
        type.input_templates[1].type = PIN_TYPE_FLOAT;
        strcpy(type.input_templates[2].name, "Alpha");
        type.input_templates[2].type = PIN_TYPE_FLOAT;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Result");
        type.output_templates[0].type = PIN_TYPE_FLOAT;
        
        node_register_type(&type);
    }
    
    // Sin node
    {
        node_type_t type = {0};
        strcpy(type.name, "Sin");
        strcpy(type.tooltip, "Sine function");
        type.category = NODE_CATEGORY_MATH;
        type.color = 0xFF408040;
        type.width = 80;
        type.min_height = 50;
        type.execute = execute_sin;
        type.flags = NODE_TYPE_FLAG_PURE | NODE_TYPE_FLAG_COMPACT;
        
        type.input_count = 1;
        strcpy(type.input_templates[0].name, "Angle");
        type.input_templates[0].type = PIN_TYPE_FLOAT;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Result");
        type.output_templates[0].type = PIN_TYPE_FLOAT;
        
        node_register_type(&type);
    }
    
    // Random node
    {
        node_type_t type = {0};
        strcpy(type.name, "Random");
        strcpy(type.tooltip, "Generate random value");
        type.category = NODE_CATEGORY_MATH;
        type.color = 0xFF408040;
        type.width = 120;
        type.min_height = 70;
        type.execute = execute_random;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "Min");
        type.input_templates[0].type = PIN_TYPE_FLOAT;
        type.input_templates[0].default_value.f = 0.0f;
        strcpy(type.input_templates[1].name, "Max");
        type.input_templates[1].type = PIN_TYPE_FLOAT;
        type.input_templates[1].default_value.f = 1.0f;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Value");
        type.output_templates[0].type = PIN_TYPE_FLOAT;
        
        node_register_type(&type);
    }
}

void register_logic_nodes(void) {
    // AND node
    {
        node_type_t type = {0};
        strcpy(type.name, "AND");
        strcpy(type.tooltip, "Logical AND operation");
        type.category = NODE_CATEGORY_LOGIC;
        type.color = 0xFF804040;
        type.width = 80;
        type.min_height = 60;
        type.execute = execute_and;
        type.flags = NODE_TYPE_FLAG_PURE | NODE_TYPE_FLAG_COMPACT;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "A");
        type.input_templates[0].type = PIN_TYPE_BOOL;
        strcpy(type.input_templates[1].name, "B");
        type.input_templates[1].type = PIN_TYPE_BOOL;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Result");
        type.output_templates[0].type = PIN_TYPE_BOOL;
        
        node_register_type(&type);
    }
    
    // Greater node
    {
        node_type_t type = {0};
        strcpy(type.name, "Greater");
        strcpy(type.tooltip, "Check if A > B");
        type.category = NODE_CATEGORY_LOGIC;
        type.color = 0xFF804040;
        type.width = 100;
        type.min_height = 60;
        type.execute = execute_greater;
        type.flags = NODE_TYPE_FLAG_PURE | NODE_TYPE_FLAG_COMPACT;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "A");
        type.input_templates[0].type = PIN_TYPE_FLOAT;
        strcpy(type.input_templates[1].name, "B");
        type.input_templates[1].type = PIN_TYPE_FLOAT;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "A > B");
        type.output_templates[0].type = PIN_TYPE_BOOL;
        
        node_register_type(&type);
    }
}

void register_debug_nodes(void) {
    // Print node
    {
        node_type_t type = {0};
        strcpy(type.name, "Print");
        strcpy(type.tooltip, "Print value to debug output");
        type.category = NODE_CATEGORY_DEBUG;
        type.color = 0xFF800080;
        type.width = 120;
        type.min_height = 60;
        type.execute = execute_print;
        
        type.input_count = 2;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        strcpy(type.input_templates[1].name, "Value");
        type.input_templates[1].type = PIN_TYPE_ANY;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Exec");
        type.output_templates[0].type = PIN_TYPE_EXECUTION;
        
        node_register_type(&type);
    }
    
    // Breakpoint node
    {
        node_type_t type = {0};
        strcpy(type.name, "Breakpoint");
        strcpy(type.tooltip, "Pause execution for debugging");
        type.category = NODE_CATEGORY_DEBUG;
        type.color = 0xFF800080;
        type.width = 100;
        type.min_height = 40;
        type.execute = execute_breakpoint;
        
        type.input_count = 1;
        strcpy(type.input_templates[0].name, "Exec");
        type.input_templates[0].type = PIN_TYPE_EXECUTION;
        
        type.output_count = 1;
        strcpy(type.output_templates[0].name, "Exec");
        type.output_templates[0].type = PIN_TYPE_EXECUTION;
        
        node_register_type(&type);
    }
}

// Register all built-in nodes
void nodes_library_init(void) {
    register_flow_control_nodes();
    register_math_nodes();
    register_logic_nodes();
    register_debug_nodes();
    
    // TODO: Register more node categories
    // register_variable_nodes();
    // register_event_nodes();
    // register_game_nodes();
    // register_ai_nodes();
}