/*
 * Handmade Script Engine Integration
 * 
 * Binds script VM to engine systems:
 * - Physics
 * - Audio
 * - Rendering
 * - Input
 * - Hot-reload
 */

#include "handmade_script.h"
#include "handmade_physics.h"
#include "handmade_audio.h"
#include "handmade_renderer.h"
#include "handmade_network.h"
#include "handmade_gui.h"
#include "handmade_hotreload.h"
#include <string.h>
#include <stdio.h>

// Forward declarations for engine functions
void script_register_stdlib(Script_VM* vm);

// Physics bindings
static Script_Value physics_create_body(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 2 || !script_is_number(argv[0]) || !script_is_number(argv[1])) {
        return script_nil();
    }
    
    // Create physics body at position
    float x = (float)argv[0].as.number;
    float y = (float)argv[1].as.number;
    float z = (argc >= 3 && script_is_number(argv[2])) ? (float)argv[2].as.number : 0.0f;
    
    // Call into physics system
    // Physics_Body* body = physics_create_body(x, y, z);
    // return script_userdata(body);
    
    return script_userdata(NULL); // Placeholder
}

static Script_Value physics_apply_force(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 4) return script_nil();
    
    // Apply force to body
    // Physics_Body* body = argv[0].as.userdata;
    // float fx = argv[1].as.number;
    // float fy = argv[2].as.number;
    // float fz = argv[3].as.number;
    // physics_apply_force(body, fx, fy, fz);
    
    return script_nil();
}

static Script_Value physics_set_velocity(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 4) return script_nil();
    
    // Set body velocity
    return script_nil();
}

static Script_Value physics_raycast(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 6) return script_nil();
    
    // Perform raycast
    Script_Value result = script_table(vm, 4);
    script_table_set(vm, result.as.table, "hit", script_bool(false));
    
    return result;
}

// Audio bindings
static Script_Value audio_play_sound(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 1 || !script_is_string(argv[0])) {
        return script_nil();
    }
    
    const char* filename = argv[0].as.string->data;
    float volume = (argc >= 2 && script_is_number(argv[1])) ? 
                   (float)argv[1].as.number : 1.0f;
    
    // audio_play_sound(filename, volume);
    
    return script_nil();
}

static Script_Value audio_set_volume(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_bool(false);
    }
    
    float volume = (float)argv[0].as.number;
    // audio_set_master_volume(volume);
    
    return script_bool(true);
}

static Script_Value audio_play_music(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 1 || !script_is_string(argv[0])) {
        return script_nil();
    }
    
    const char* filename = argv[0].as.string->data;
    bool loop = (argc >= 2) ? script_to_bool(argv[1]) : true;
    
    // audio_play_music(filename, loop);
    
    return script_nil();
}

// Rendering bindings
static Script_Value render_draw_sprite(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 3) return script_nil();
    
    // Draw sprite at position
    return script_nil();
}

static Script_Value render_draw_text(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 3 || !script_is_string(argv[0])) {
        return script_nil();
    }
    
    const char* text = argv[0].as.string->data;
    float x = (float)argv[1].as.number;
    float y = (float)argv[2].as.number;
    
    // render_draw_text(text, x, y);
    
    return script_nil();
}

static Script_Value render_set_camera(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 2) return script_nil();
    
    float x = (float)argv[0].as.number;
    float y = (float)argv[1].as.number;
    float z = (argc >= 3) ? (float)argv[2].as.number : 0.0f;
    
    // render_set_camera_position(x, y, z);
    
    return script_nil();
}

// Input bindings
static Script_Value input_is_key_pressed(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_string(argv[0])) {
        return script_bool(false);
    }
    
    const char* key = argv[0].as.string->data;
    
    // Map key string to keycode
    // bool pressed = input_is_key_pressed(keycode);
    
    return script_bool(false); // Placeholder
}

static Script_Value input_get_mouse_pos(Script_VM* vm, int argc, Script_Value* argv) {
    Script_Value result = script_table(vm, 2);
    
    // Get actual mouse position
    // int x, y;
    // input_get_mouse_position(&x, &y);
    
    script_table_set(vm, result.as.table, "x", script_number(0));
    script_table_set(vm, result.as.table, "y", script_number(0));
    
    return result;
}

static Script_Value input_is_mouse_button_pressed(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_bool(false);
    }
    
    int button = (int)argv[0].as.number;
    // bool pressed = input_is_mouse_button_pressed(button);
    
    return script_bool(false);
}

// Entity/GameObject bindings
static Script_Value entity_create(Script_VM* vm, int argc, Script_Value* argv) {
    Script_Value entity = script_table(vm, 16);
    
    // Initialize entity components
    script_table_set(vm, entity.as.table, "id", script_number(0));
    script_table_set(vm, entity.as.table, "x", script_number(0));
    script_table_set(vm, entity.as.table, "y", script_number(0));
    script_table_set(vm, entity.as.table, "vx", script_number(0));
    script_table_set(vm, entity.as.table, "vy", script_number(0));
    script_table_set(vm, entity.as.table, "health", script_number(100));
    
    return entity;
}

static Script_Value entity_destroy(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_table(argv[0])) {
        return script_bool(false);
    }
    
    // Mark entity for destruction
    script_table_set(vm, argv[0].as.table, "destroyed", script_bool(true));
    
    return script_bool(true);
}

static Script_Value entity_get_component(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 2 || !script_is_table(argv[0]) || !script_is_string(argv[1])) {
        return script_nil();
    }
    
    return script_table_get(vm, argv[0].as.table, argv[1].as.string->data);
}

static Script_Value entity_set_component(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 3 || !script_is_table(argv[0]) || !script_is_string(argv[1])) {
        return script_nil();
    }
    
    script_table_set(vm, argv[0].as.table, argv[1].as.string->data, argv[2]);
    return argv[2];
}

// Network bindings
static Script_Value net_send_message(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 2 || !script_is_number(argv[0]) || !script_is_string(argv[1])) {
        return script_bool(false);
    }
    
    int client_id = (int)argv[0].as.number;
    const char* message = argv[1].as.string->data;
    
    // network_send_message(client_id, message);
    
    return script_bool(true);
}

static Script_Value net_broadcast(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_string(argv[0])) {
        return script_bool(false);
    }
    
    const char* message = argv[0].as.string->data;
    // network_broadcast(message);
    
    return script_bool(true);
}

// Game-specific event system
typedef struct {
    Script_VM* vm;
    Script_Value callback;
    char event_name[64];
} Event_Handler;

static Event_Handler event_handlers[256];
static int event_handler_count = 0;

static Script_Value event_register(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 2 || !script_is_string(argv[0]) || !script_is_function(argv[1])) {
        return script_bool(false);
    }
    
    if (event_handler_count >= 256) {
        return script_bool(false);
    }
    
    Event_Handler* handler = &event_handlers[event_handler_count++];
    handler->vm = vm;
    handler->callback = argv[1];
    strncpy(handler->event_name, argv[0].as.string->data, 63);
    handler->event_name[63] = '\0';
    
    return script_bool(true);
}

static Script_Value event_trigger(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 1 || !script_is_string(argv[0])) {
        return script_nil();
    }
    
    const char* event_name = argv[0].as.string->data;
    
    // Find and call all handlers for this event
    for (int i = 0; i < event_handler_count; i++) {
        if (strcmp(event_handlers[i].event_name, event_name) == 0) {
            Script_Value result;
            script_call(event_handlers[i].vm, event_handlers[i].callback, 
                       argc - 1, argv + 1, &result);
        }
    }
    
    return script_nil();
}

// Main integration function
void script_integrate_engine(Script_VM* vm) {
    // Register standard library first
    script_register_stdlib(vm);
    
    // Physics library
    Script_Value physics = script_table(vm, 8);
    script_table_set(vm, physics.as.table, "create_body", script_native(physics_create_body));
    script_table_set(vm, physics.as.table, "apply_force", script_native(physics_apply_force));
    script_table_set(vm, physics.as.table, "set_velocity", script_native(physics_set_velocity));
    script_table_set(vm, physics.as.table, "raycast", script_native(physics_raycast));
    script_set_global(vm, "physics", physics);
    
    // Audio library
    Script_Value audio = script_table(vm, 4);
    script_table_set(vm, audio.as.table, "play_sound", script_native(audio_play_sound));
    script_table_set(vm, audio.as.table, "set_volume", script_native(audio_set_volume));
    script_table_set(vm, audio.as.table, "play_music", script_native(audio_play_music));
    script_set_global(vm, "audio", audio);
    
    // Rendering library
    Script_Value render = script_table(vm, 4);
    script_table_set(vm, render.as.table, "draw_sprite", script_native(render_draw_sprite));
    script_table_set(vm, render.as.table, "draw_text", script_native(render_draw_text));
    script_table_set(vm, render.as.table, "set_camera", script_native(render_set_camera));
    script_set_global(vm, "render", render);
    
    // Input library
    Script_Value input = script_table(vm, 4);
    script_table_set(vm, input.as.table, "is_key_pressed", script_native(input_is_key_pressed));
    script_table_set(vm, input.as.table, "get_mouse_pos", script_native(input_get_mouse_pos));
    script_table_set(vm, input.as.table, "is_mouse_button_pressed", 
                    script_native(input_is_mouse_button_pressed));
    script_set_global(vm, "input", input);
    
    // Entity system
    Script_Value entity = script_table(vm, 4);
    script_table_set(vm, entity.as.table, "create", script_native(entity_create));
    script_table_set(vm, entity.as.table, "destroy", script_native(entity_destroy));
    script_table_set(vm, entity.as.table, "get_component", script_native(entity_get_component));
    script_table_set(vm, entity.as.table, "set_component", script_native(entity_set_component));
    script_set_global(vm, "entity", entity);
    
    // Network library
    Script_Value net = script_table(vm, 2);
    script_table_set(vm, net.as.table, "send", script_native(net_send_message));
    script_table_set(vm, net.as.table, "broadcast", script_native(net_broadcast));
    script_set_global(vm, "net", net);
    
    // Event system
    Script_Value event = script_table(vm, 2);
    script_table_set(vm, event.as.table, "register", script_native(event_register));
    script_table_set(vm, event.as.table, "trigger", script_native(event_trigger));
    script_set_global(vm, "event", event);
    
    // Game constants
    script_set_global(vm, "SCREEN_WIDTH", script_number(1920));
    script_set_global(vm, "SCREEN_HEIGHT", script_number(1080));
    script_set_global(vm, "FIXED_TIMESTEP", script_number(1.0/60.0));
}

// Hot-reload support
void script_hotreload_update(Script_VM* vm, const char* filename) {
    // Save current state
    size_t state_size = 1024 * 1024; // 1MB
    void* state_buffer = malloc(state_size);
    
    if (script_save_state(vm, state_buffer, &state_size)) {
        // Reload script
        Script_Compile_Result result = script_compile_file(vm, filename);
        
        if (!result.error_message) {
            // Restore state
            script_load_state(vm, state_buffer, state_size);
            
            // Run new code
            script_run(vm, result.function);
            
            printf("Hot-reloaded: %s\n", filename);
        } else {
            printf("Hot-reload error: %s\n", result.error_message);
        }
        
        script_free_compile_result(vm, &result);
    }
    
    free(state_buffer);
}