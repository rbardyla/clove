// blueprint_nodes.c - Standard node library with comprehensive node implementations
// PERFORMANCE: Cache-friendly node templates with minimal overhead
// COVERAGE: All essential nodes for game logic, math, flow control

#include "handmade_blueprint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// NODE EXECUTION FUNCTIONS
// ============================================================================

// Event nodes
static void execute_begin_play(blueprint_context* ctx, blueprint_node* node) {
    // BeginPlay - executed once at start
    // No special logic needed, just continues execution
}

static void execute_tick(blueprint_context* ctx, blueprint_node* node) {
    // Tick - executed every frame
    // Delta time would be provided via input pin
    if (node->input_pin_count > 0) {
        blueprint_pin* dt_pin = &node->input_pins[0];
        // Process delta time if needed
    }
}

static void execute_custom_event(blueprint_context* ctx, blueprint_node* node) {
    // Custom event - triggered externally
    blueprint_log_debug(ctx, "Custom event '%s' triggered", node->name);
}

// Flow control nodes
static void execute_branch(blueprint_context* ctx, blueprint_node* node) {
    // Branch based on condition input
    if (node->input_pin_count > 0) {
        blueprint_pin* condition_pin = &node->input_pins[0];
        b32 condition = condition_pin->current_value.bool_val;
        
        // Set execution flow based on condition
        // This is handled by the compiler/VM, not the node directly
        blueprint_log_debug(ctx, "Branch condition: %s", condition ? "true" : "false");
    }
}

static void execute_sequence(blueprint_context* ctx, blueprint_node* node) {
    // Execute multiple outputs in sequence
    blueprint_log_debug(ctx, "Sequence node executed");
}

typedef struct delay_state {
    f64 start_time;
    f32 duration;
    b32 is_active;
} delay_state;

static void execute_delay(blueprint_context* ctx, blueprint_node* node) {
    // Delay execution for specified time
    if (node->input_pin_count > 0 && node->output_pin_count > 0) {
        blueprint_pin* duration_pin = &node->input_pins[0];
        blueprint_pin* exec_out = &node->output_pins[0];
        f32 duration = duration_pin->current_value.float_val;
        
        // Initialize delay state if not present
        if (!node->user_data) {
            node->user_data = malloc(sizeof(delay_state));
            memset(node->user_data, 0, sizeof(delay_state));
        }
        
        delay_state* state = (delay_state*)node->user_data;
        
        // Start delay if not active
        if (!state->is_active) {
            state->start_time = blueprint_get_time();  // Assumes this function exists
            state->duration = duration;
            state->is_active = true;
            blueprint_log_debug(ctx, "Delay started: %.2f seconds", duration);
        }
        
        // Check if delay has elapsed
        f64 current_time = blueprint_get_time();
        if (current_time - state->start_time >= state->duration) {
            // Delay complete, trigger execution output
            exec_out->current_value.bool_val = true;
            state->is_active = false;
            blueprint_log_debug(ctx, "Delay completed");
        } else {
            // Still waiting
            exec_out->current_value.bool_val = false;
        }
    }
}

// Math operations - PERFORMANCE: Optimized for common operations
static void execute_add(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 2 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        // Type-specific addition
        switch (a_pin->type) {
            case BP_TYPE_INT: {
                i32 a = a_pin->current_value.int_val;
                i32 b = b_pin->current_value.int_val;
                result_pin->current_value.int_val = a + b;
                break;
            }
            case BP_TYPE_FLOAT: {
                f32 a = a_pin->current_value.float_val;
                f32 b = b_pin->current_value.float_val;
                result_pin->current_value.float_val = a + b;
                break;
            }
            case BP_TYPE_VEC2: {
                v2 a = a_pin->current_value.vec2_val;
                v2 b = b_pin->current_value.vec2_val;
                result_pin->current_value.vec2_val = (v2){a.x + b.x, a.y + b.y};
                break;
            }
            case BP_TYPE_VEC3: {
                v3 a = a_pin->current_value.vec3_val;
                v3 b = b_pin->current_value.vec3_val;
                result_pin->current_value.vec3_val = (v3){a.x + b.x, a.y + b.y, a.z + b.z};
                break;
            }
            case BP_TYPE_VEC4: {
                v4 a = a_pin->current_value.vec4_val;
                v4 b = b_pin->current_value.vec4_val;
                result_pin->current_value.vec4_val = (v4){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
                break;
            }
            default:
                break;
        }
    }
}

static void execute_multiply(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 2 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        switch (a_pin->type) {
            case BP_TYPE_INT: {
                i32 a = a_pin->current_value.int_val;
                i32 b = b_pin->current_value.int_val;
                result_pin->current_value.int_val = a * b;
                break;
            }
            case BP_TYPE_FLOAT: {
                f32 a = a_pin->current_value.float_val;
                f32 b = b_pin->current_value.float_val;
                result_pin->current_value.float_val = a * b;
                break;
            }
            case BP_TYPE_VEC3: {
                v3 a = a_pin->current_value.vec3_val;
                f32 b = b_pin->current_value.float_val; // Scalar multiply
                result_pin->current_value.vec3_val = (v3){a.x * b, a.y * b, a.z * b};
                break;
            }
            default:
                break;
        }
    }
}

static void execute_sin(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* output_pin = &node->output_pins[0];
        
        f32 input = input_pin->current_value.float_val;
        output_pin->current_value.float_val = sinf(input);
    }
}

static void execute_cos(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* output_pin = &node->output_pins[0];
        
        f32 input = input_pin->current_value.float_val;
        output_pin->current_value.float_val = cosf(input);
    }
}

static void execute_sqrt(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* output_pin = &node->output_pins[0];
        
        f32 input = input_pin->current_value.float_val;
        output_pin->current_value.float_val = sqrtf(input);
    }
}

static void execute_abs(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* output_pin = &node->output_pins[0];
        
        if (input_pin->type == BP_TYPE_FLOAT) {
            f32 input = input_pin->current_value.float_val;
            output_pin->current_value.float_val = fabsf(input);
        } else if (input_pin->type == BP_TYPE_INT) {
            i32 input = input_pin->current_value.int_val;
            output_pin->current_value.int_val = abs(input);
        }
    }
}

static void execute_clamp(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 3 && node->output_pin_count >= 1) {
        blueprint_pin* value_pin = &node->input_pins[0];
        blueprint_pin* min_pin = &node->input_pins[1];
        blueprint_pin* max_pin = &node->input_pins[2];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        if (value_pin->type == BP_TYPE_FLOAT) {
            f32 value = value_pin->current_value.float_val;
            f32 min_val = min_pin->current_value.float_val;
            f32 max_val = max_pin->current_value.float_val;
            
            if (value < min_val) value = min_val;
            if (value > max_val) value = max_val;
            
            result_pin->current_value.float_val = value;
        }
    }
}

static void execute_lerp(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 3 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* t_pin = &node->input_pins[2];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        if (a_pin->type == BP_TYPE_FLOAT) {
            f32 a = a_pin->current_value.float_val;
            f32 b = b_pin->current_value.float_val;
            f32 t = t_pin->current_value.float_val;
            
            result_pin->current_value.float_val = a + t * (b - a);
        } else if (a_pin->type == BP_TYPE_VEC3) {
            v3 a = a_pin->current_value.vec3_val;
            v3 b = b_pin->current_value.vec3_val;
            f32 t = t_pin->current_value.float_val;
            
            result_pin->current_value.vec3_val = (v3){
                a.x + t * (b.x - a.x),
                a.y + t * (b.y - a.y),
                a.z + t * (b.z - a.z)
            };
        }
    }
}

// Vector math operations
static void execute_vec_dot(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 2 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        if (a_pin->type == BP_TYPE_VEC3) {
            v3 a = a_pin->current_value.vec3_val;
            v3 b = b_pin->current_value.vec3_val;
            
            f32 dot = a.x * b.x + a.y * b.y + a.z * b.z;
            result_pin->current_value.float_val = dot;
        }
    }
}

static void execute_vec_cross(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 2 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        if (a_pin->type == BP_TYPE_VEC3) {
            v3 a = a_pin->current_value.vec3_val;
            v3 b = b_pin->current_value.vec3_val;
            
            v3 cross = {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
            
            result_pin->current_value.vec3_val = cross;
        }
    }
}

static void execute_vec_normalize(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        if (input_pin->type == BP_TYPE_VEC3) {
            v3 vec = input_pin->current_value.vec3_val;
            f32 length = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            
            if (length > 0.0f) {
                result_pin->current_value.vec3_val = (v3){
                    vec.x / length,
                    vec.y / length,
                    vec.z / length
                };
            } else {
                result_pin->current_value.vec3_val = (v3){0, 0, 0};
            }
        }
    }
}

static void execute_vec_length(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        if (input_pin->type == BP_TYPE_VEC3) {
            v3 vec = input_pin->current_value.vec3_val;
            f32 length = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            result_pin->current_value.float_val = length;
        }
    }
}

// Comparison operations
static void execute_equals(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 2 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        b32 equal = false;
        
        switch (a_pin->type) {
            case BP_TYPE_INT: {
                equal = (a_pin->current_value.int_val == b_pin->current_value.int_val);
                break;
            }
            case BP_TYPE_FLOAT: {
                f32 a = a_pin->current_value.float_val;
                f32 b = b_pin->current_value.float_val;
                equal = (fabsf(a - b) < 0.0001f); // Float comparison with epsilon
                break;
            }
            case BP_TYPE_BOOL: {
                equal = (a_pin->current_value.bool_val == b_pin->current_value.bool_val);
                break;
            }
            default:
                break;
        }
        
        result_pin->current_value.bool_val = equal;
    }
}

static void execute_less(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 2 && node->output_pin_count >= 1) {
        blueprint_pin* a_pin = &node->input_pins[0];
        blueprint_pin* b_pin = &node->input_pins[1];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        b32 less = false;
        
        if (a_pin->type == BP_TYPE_INT) {
            less = (a_pin->current_value.int_val < b_pin->current_value.int_val);
        } else if (a_pin->type == BP_TYPE_FLOAT) {
            less = (a_pin->current_value.float_val < b_pin->current_value.float_val);
        }
        
        result_pin->current_value.bool_val = less;
    }
}

// Variable operations
static void execute_get_variable(blueprint_context* ctx, blueprint_node* node) {
    if (node->output_pin_count >= 1 && ctx->active_graph) {
        blueprint_pin* result_pin = &node->output_pins[0];
        blueprint_graph* graph = ctx->active_graph;
        
        // Look up variable by name in the graph
        for (u32 i = 0; i < graph->variable_count; i++) {
            if (strcmp(graph->variables[i].name, node->name) == 0) {
                // Found the variable, copy its value
                result_pin->current_value = graph->variables[i].value;
                blueprint_log_debug(ctx, "Get variable: %s = %d", node->name, 
                    result_pin->current_value.int_val);
                return;
            }
        }
        
        // Variable not found, use default value
        switch (result_pin->type) {
            case BP_TYPE_INT: result_pin->current_value.int_val = 0; break;
            case BP_TYPE_FLOAT: result_pin->current_value.float_val = 0.0f; break;
            case BP_TYPE_BOOL: result_pin->current_value.bool_val = false; break;
            default: break;
        }
        
        blueprint_log_debug(ctx, "Variable not found: %s", node->name);
    }
}

static void execute_set_variable(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && ctx->active_graph) {
        blueprint_pin* value_pin = &node->input_pins[0];
        blueprint_graph* graph = ctx->active_graph;
        
        // Look up variable by name and update its value
        for (u32 i = 0; i < graph->variable_count; i++) {
            if (strcmp(graph->variables[i].name, node->name) == 0) {
                // Found the variable, update its value
                graph->variables[i].value = value_pin->current_value;
                blueprint_log_debug(ctx, "Set variable: %s = %d", node->name,
                    value_pin->current_value.int_val);
                return;
            }
        }
        
        // Variable not found, create it if we have space
        if (graph->variable_count < 256) {  // Reasonable limit
            blueprint_variable* var = &graph->variables[graph->variable_count++];
            strncpy(var->name, node->name, 63);
            var->name[63] = '\0';
            var->type = value_pin->type;
            var->value = value_pin->current_value;
            var->default_value = value_pin->current_value;
            var->is_editable = true;
            var->is_public = false;
            blueprint_log_debug(ctx, "Created new variable: %s", node->name);
        } else {
            blueprint_log_debug(ctx, "Variable limit reached, cannot create: %s", node->name);
        }
    }
}

// Type conversion
static void execute_cast(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 1) {
        blueprint_pin* input_pin = &node->input_pins[0];
        blueprint_pin* output_pin = &node->output_pins[0];
        
        // Cast input value to output type
        blueprint_cast_value(&input_pin->current_value, input_pin->type, output_pin->type);
        output_pin->current_value = input_pin->current_value;
    }
}

static void execute_make_vec3(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 3 && node->output_pin_count >= 1) {
        blueprint_pin* x_pin = &node->input_pins[0];
        blueprint_pin* y_pin = &node->input_pins[1];
        blueprint_pin* z_pin = &node->input_pins[2];
        blueprint_pin* result_pin = &node->output_pins[0];
        
        result_pin->current_value.vec3_val = (v3){
            x_pin->current_value.float_val,
            y_pin->current_value.float_val,
            z_pin->current_value.float_val
        };
    }
}

static void execute_break_vec3(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1 && node->output_pin_count >= 3) {
        blueprint_pin* input_pin = &node->input_pins[0];
        v3 vec = input_pin->current_value.vec3_val;
        
        node->output_pins[0].current_value.float_val = vec.x;
        node->output_pins[1].current_value.float_val = vec.y;
        node->output_pins[2].current_value.float_val = vec.z;
    }
}

// Debug operations
static void execute_print(blueprint_context* ctx, blueprint_node* node) {
    if (node->input_pin_count >= 1) {
        blueprint_pin* value_pin = &node->input_pins[0];
        
        char output_buffer[256];
        switch (value_pin->type) {
            case BP_TYPE_INT: {
                snprintf(output_buffer, sizeof(output_buffer), "%d", value_pin->current_value.int_val);
                break;
            }
            case BP_TYPE_FLOAT: {
                snprintf(output_buffer, sizeof(output_buffer), "%.3f", value_pin->current_value.float_val);
                break;
            }
            case BP_TYPE_BOOL: {
                snprintf(output_buffer, sizeof(output_buffer), "%s", 
                        value_pin->current_value.bool_val ? "true" : "false");
                break;
            }
            case BP_TYPE_VEC3: {
                v3 vec = value_pin->current_value.vec3_val;
                snprintf(output_buffer, sizeof(output_buffer), "(%.3f, %.3f, %.3f)", vec.x, vec.y, vec.z);
                break;
            }
            default: {
                snprintf(output_buffer, sizeof(output_buffer), "<unknown type>");
                break;
            }
        }
        
        blueprint_log_debug(ctx, "PRINT: %s", output_buffer);
        
        // Also log to GUI if available
        if (ctx->gui) {
            gui_log(ctx->gui, "[BLUEPRINT] %s", output_buffer);
        }
    }
}

static void execute_breakpoint(blueprint_context* ctx, blueprint_node* node) {
    blueprint_log_debug(ctx, "Breakpoint hit at node '%s'", node->name);
    
    // Set VM to paused state
    ctx->vm.is_paused = true;
    
    // Mark node as having active breakpoint
    node->flags |= NODE_FLAG_BREAKPOINT;
}

// ============================================================================
// NODE TEMPLATE CREATION
// ============================================================================

static blueprint_node create_node_template(node_type type, const char* name, const char* display_name, 
                                          node_exec_func execute_func, node_flags flags, color32 color) {
    blueprint_node template = {0};
    
    template.type = type;
    template.flags = flags;
    template.execute = execute_func;
    template.color = color;
    template.rounding = 4.0f;
    template.size = (v2){120, 60}; // Default size
    
    strncpy(template.name, name, BLUEPRINT_MAX_NODE_NAME - 1);
    strncpy(template.display_name, display_name, BLUEPRINT_MAX_NODE_NAME - 1);
    
    return template;
}

// ============================================================================
// NODE LIBRARY INITIALIZATION
// ============================================================================

void blueprint_init_standard_nodes(blueprint_context* ctx) {
    // Allocate node template storage
    ctx->node_template_count = NODE_TYPE_COUNT;
    ctx->node_templates = (blueprint_node*)blueprint_alloc(ctx, 
        sizeof(blueprint_node) * ctx->node_template_count);
    
    if (!ctx->node_templates) {
        blueprint_log_debug(ctx, "Failed to allocate node templates");
        return;
    }
    
    // Event nodes - Blue color scheme
    color32 event_color = 0xFF4169E1; // Royal Blue
    ctx->node_templates[NODE_TYPE_BEGIN_PLAY] = create_node_template(
        NODE_TYPE_BEGIN_PLAY, "BeginPlay", "Begin Play", execute_begin_play, NODE_FLAG_NONE, event_color);
    
    ctx->node_templates[NODE_TYPE_TICK] = create_node_template(
        NODE_TYPE_TICK, "Tick", "Tick", execute_tick, NODE_FLAG_NONE, event_color);
    
    ctx->node_templates[NODE_TYPE_CUSTOM_EVENT] = create_node_template(
        NODE_TYPE_CUSTOM_EVENT, "CustomEvent", "Custom Event", execute_custom_event, NODE_FLAG_NONE, event_color);
    
    // Flow control nodes - Green color scheme
    color32 flow_color = 0xFF228B22; // Forest Green
    ctx->node_templates[NODE_TYPE_BRANCH] = create_node_template(
        NODE_TYPE_BRANCH, "Branch", "Branch", execute_branch, NODE_FLAG_NONE, flow_color);
    
    ctx->node_templates[NODE_TYPE_SEQUENCE] = create_node_template(
        NODE_TYPE_SEQUENCE, "Sequence", "Sequence", execute_sequence, NODE_FLAG_NONE, flow_color);
    
    ctx->node_templates[NODE_TYPE_DELAY] = create_node_template(
        NODE_TYPE_DELAY, "Delay", "Delay", execute_delay, NODE_FLAG_NONE, flow_color);
    
    // Math nodes - Purple color scheme
    color32 math_color = 0xFF9932CC; // Dark Orchid
    ctx->node_templates[NODE_TYPE_ADD] = create_node_template(
        NODE_TYPE_ADD, "Add", "Add", execute_add, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_MULTIPLY] = create_node_template(
        NODE_TYPE_MULTIPLY, "Multiply", "Multiply", execute_multiply, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_SIN] = create_node_template(
        NODE_TYPE_SIN, "Sin", "Sine", execute_sin, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_COS] = create_node_template(
        NODE_TYPE_COS, "Cos", "Cosine", execute_cos, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_SQRT] = create_node_template(
        NODE_TYPE_SQRT, "Sqrt", "Square Root", execute_sqrt, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_ABS] = create_node_template(
        NODE_TYPE_ABS, "Abs", "Absolute", execute_abs, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_CLAMP] = create_node_template(
        NODE_TYPE_CLAMP, "Clamp", "Clamp", execute_clamp, NODE_FLAG_PURE, math_color);
    
    ctx->node_templates[NODE_TYPE_LERP] = create_node_template(
        NODE_TYPE_LERP, "Lerp", "Linear Interpolation", execute_lerp, NODE_FLAG_PURE, math_color);
    
    // Vector math - Teal color scheme
    color32 vector_color = 0xFF20B2AA; // Light Sea Green
    ctx->node_templates[NODE_TYPE_VEC_DOT] = create_node_template(
        NODE_TYPE_VEC_DOT, "DotProduct", "Dot Product", execute_vec_dot, NODE_FLAG_PURE, vector_color);
    
    ctx->node_templates[NODE_TYPE_VEC_CROSS] = create_node_template(
        NODE_TYPE_VEC_CROSS, "CrossProduct", "Cross Product", execute_vec_cross, NODE_FLAG_PURE, vector_color);
    
    ctx->node_templates[NODE_TYPE_VEC_NORMALIZE] = create_node_template(
        NODE_TYPE_VEC_NORMALIZE, "Normalize", "Normalize", execute_vec_normalize, NODE_FLAG_PURE, vector_color);
    
    ctx->node_templates[NODE_TYPE_VEC_LENGTH] = create_node_template(
        NODE_TYPE_VEC_LENGTH, "Length", "Vector Length", execute_vec_length, NODE_FLAG_PURE, vector_color);
    
    // Comparison nodes - Orange color scheme
    color32 comparison_color = 0xFFFF8C00; // Dark Orange
    ctx->node_templates[NODE_TYPE_EQUALS] = create_node_template(
        NODE_TYPE_EQUALS, "Equals", "Equals", execute_equals, NODE_FLAG_PURE, comparison_color);
    
    ctx->node_templates[NODE_TYPE_LESS] = create_node_template(
        NODE_TYPE_LESS, "Less", "Less Than", execute_less, NODE_FLAG_PURE, comparison_color);
    
    // Variable nodes - Yellow color scheme
    color32 variable_color = 0xFFFFD700; // Gold
    ctx->node_templates[NODE_TYPE_GET_VARIABLE] = create_node_template(
        NODE_TYPE_GET_VARIABLE, "GetVar", "Get Variable", execute_get_variable, NODE_FLAG_PURE, variable_color);
    
    ctx->node_templates[NODE_TYPE_SET_VARIABLE] = create_node_template(
        NODE_TYPE_SET_VARIABLE, "SetVar", "Set Variable", execute_set_variable, NODE_FLAG_IMPURE, variable_color);
    
    // Type conversion - Gray color scheme
    color32 conversion_color = 0xFF708090; // Slate Gray
    ctx->node_templates[NODE_TYPE_CAST] = create_node_template(
        NODE_TYPE_CAST, "Cast", "Cast", execute_cast, NODE_FLAG_PURE, conversion_color);
    
    ctx->node_templates[NODE_TYPE_MAKE_VEC3] = create_node_template(
        NODE_TYPE_MAKE_VEC3, "MakeVec3", "Make Vector3", execute_make_vec3, NODE_FLAG_PURE, conversion_color);
    
    ctx->node_templates[NODE_TYPE_BREAK_VEC3] = create_node_template(
        NODE_TYPE_BREAK_VEC3, "BreakVec3", "Break Vector3", execute_break_vec3, NODE_FLAG_PURE, conversion_color);
    
    // Debug nodes - Red color scheme
    color32 debug_color = 0xFFDC143C; // Crimson
    ctx->node_templates[NODE_TYPE_PRINT] = create_node_template(
        NODE_TYPE_PRINT, "Print", "Print", execute_print, NODE_FLAG_IMPURE, debug_color);
    
    ctx->node_templates[NODE_TYPE_BREAKPOINT] = create_node_template(
        NODE_TYPE_BREAKPOINT, "Breakpoint", "Breakpoint", execute_breakpoint, NODE_FLAG_IMPURE, debug_color);
    
    blueprint_log_debug(ctx, "Initialized %u standard node templates", ctx->node_template_count);
}

// Get node template by type
blueprint_node* blueprint_get_node_template(blueprint_context* ctx, node_type type) {
    if (type >= 0 && type < ctx->node_template_count && ctx->node_templates) {
        return &ctx->node_templates[type];
    }
    return NULL;
}

// Create node from template with proper pin setup
blueprint_node* blueprint_create_node_from_template(blueprint_graph* graph, blueprint_context* ctx, 
                                                   node_type type, v2 position) {
    blueprint_node* node = blueprint_create_node(graph, type, position);
    if (!node) return NULL;
    
    blueprint_node* template = blueprint_get_node_template(ctx, type);
    if (!template) return node;
    
    // Copy template properties
    node->flags = template->flags;
    node->execute = template->execute;
    node->color = template->color;
    node->rounding = template->rounding;
    strcpy(node->display_name, template->display_name);
    
    // Setup pins based on node type
    switch (type) {
        case NODE_TYPE_BEGIN_PLAY: {
            blueprint_add_output_pin(node, "Exec", BP_TYPE_EXEC, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_TICK: {
            blueprint_add_input_pin(node, "Exec", BP_TYPE_EXEC, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "Delta Time", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Exec", BP_TYPE_EXEC, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_ADD: {
            blueprint_add_input_pin(node, "A", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "B", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Result", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_MULTIPLY: {
            blueprint_add_input_pin(node, "A", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "B", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Result", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_BRANCH: {
            blueprint_add_input_pin(node, "Exec", BP_TYPE_EXEC, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "Condition", BP_TYPE_BOOL, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "True", BP_TYPE_EXEC, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "False", BP_TYPE_EXEC, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_PRINT: {
            blueprint_add_input_pin(node, "Exec", BP_TYPE_EXEC, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "Value", BP_TYPE_STRING, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Exec", BP_TYPE_EXEC, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_MAKE_VEC3: {
            blueprint_add_input_pin(node, "X", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "Y", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_input_pin(node, "Z", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Vector", BP_TYPE_VEC3, PIN_FLAG_NONE);
            break;
        }
        
        case NODE_TYPE_BREAK_VEC3: {
            blueprint_add_input_pin(node, "Vector", BP_TYPE_VEC3, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "X", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Y", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Z", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            break;
        }
        
        default: {
            // Default setup for unknown node types
            blueprint_add_input_pin(node, "Input", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            blueprint_add_output_pin(node, "Output", BP_TYPE_FLOAT, PIN_FLAG_NONE);
            break;
        }
    }
    
    return node;
}

// Implementation of type casting
void blueprint_cast_value(blueprint_value* value, blueprint_type from_type, blueprint_type to_type) {
    if (from_type == to_type) return; // No casting needed
    
    // Store original value for conversion
    blueprint_value original = *value;
    
    switch (to_type) {
        case BP_TYPE_BOOL: {
            switch (from_type) {
                case BP_TYPE_INT: value->bool_val = (original.int_val != 0); break;
                case BP_TYPE_FLOAT: value->bool_val = (original.float_val != 0.0f); break;
                default: value->bool_val = false; break;
            }
            break;
        }
        
        case BP_TYPE_INT: {
            switch (from_type) {
                case BP_TYPE_BOOL: value->int_val = original.bool_val ? 1 : 0; break;
                case BP_TYPE_FLOAT: value->int_val = (i32)original.float_val; break;
                default: value->int_val = 0; break;
            }
            break;
        }
        
        case BP_TYPE_FLOAT: {
            switch (from_type) {
                case BP_TYPE_BOOL: value->float_val = original.bool_val ? 1.0f : 0.0f; break;
                case BP_TYPE_INT: value->float_val = (f32)original.int_val; break;
                default: value->float_val = 0.0f; break;
            }
            break;
        }
        
        case BP_TYPE_VEC3: {
            switch (from_type) {
                case BP_TYPE_FLOAT: {
                    f32 val = original.float_val;
                    value->vec3_val = (v3){val, val, val};
                    break;
                }
                default: value->vec3_val = (v3){0, 0, 0}; break;
            }
            break;
        }
        
        default: {
            // Default: clear the value
            memset(value, 0, sizeof(blueprint_value));
            break;
        }
    }
}

b32 blueprint_can_cast(blueprint_type from, blueprint_type to) {
    if (from == to) return true;
    
    // Define casting rules
    switch (from) {
        case BP_TYPE_BOOL: {
            return (to == BP_TYPE_INT || to == BP_TYPE_FLOAT);
        }
        case BP_TYPE_INT: {
            return (to == BP_TYPE_BOOL || to == BP_TYPE_FLOAT);
        }
        case BP_TYPE_FLOAT: {
            return (to == BP_TYPE_BOOL || to == BP_TYPE_INT || to == BP_TYPE_VEC3);
        }
        default: {
            return false;
        }
    }
}