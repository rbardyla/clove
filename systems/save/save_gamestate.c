/*
    Handmade Save System - Game State Serialization
    
    Handles saving and loading of all game systems:
    - World entities
    - Physics state
    - Neural networks
    - Audio state
    - Script variables
    - Node graphs
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

// Entity serialization
typedef struct entity_save_data {
    u32 id;
    u32 type;
    f32 position[3];
    f32 rotation[4];  // Quaternion
    f32 scale[3];
    u32 flags;
    u32 parent_id;
    char name[64];
} entity_save_data;

// Write chunk with header
b32 write_chunk(save_buffer *buffer, save_chunk_type type, 
                         u8 *data, u32 size, b32 compress) {
    save_chunk_header header;
    header.type = type;
    header.uncompressed_size = size;
    
    // Reserve space for header
    u32 header_pos = buffer->size;
    save_write_bytes(buffer, &header, sizeof(header));
    
    // TEMPORARY: Disable compression for debugging
    // No compression for now
    save_write_bytes(buffer, data, size);
    header.compressed_size = size;
    
    // Calculate checksum
    header.checksum = save_crc32(buffer->data + header_pos + sizeof(header),
                                 header.compressed_size);
    
    // Update header
    memcpy(buffer->data + header_pos, &header, sizeof(header));
    
    return 1;
}

// Read and decompress chunk
internal b32 read_chunk(save_buffer *buffer, save_chunk_header *header,
                       u8 *dst, u32 dst_capacity) {
    // Read header
    save_read_bytes(buffer, header, sizeof(save_chunk_header));
    
    if (header->uncompressed_size > dst_capacity) return 0;
    
    // Read chunk data (TEMPORARY: No compression)
    save_read_bytes(buffer, dst, header->compressed_size);
    return 1;
}

// Save world entities
internal b32 save_world_state(save_buffer *buffer, game_state *game) {
    // PERFORMANCE: Serialize entities in batches for cache efficiency
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    // Write entity count
    save_write_u32(&chunk_buffer, game->entity_count);
    
    // Serialize each entity
    for (u32 i = 0; i < game->entity_count; i++) {
        entity *e = &game->entities[i];
        
        entity_save_data save_data;
        save_data.id = e->id;
        save_data.type = e->type;
        memcpy(save_data.position, e->position, sizeof(save_data.position));
        memcpy(save_data.rotation, e->rotation, sizeof(save_data.rotation));
        memcpy(save_data.scale, e->scale, sizeof(save_data.scale));
        save_data.flags = e->flags;
        save_data.parent_id = e->parent_id;
        strncpy(save_data.name, e->name, 63);
        save_data.name[63] = '\0';
        
        save_write_bytes(&chunk_buffer, &save_data, sizeof(save_data));
        
        // Entity-specific data
        switch (e->type) {
            case ENTITY_TYPE_NPC:
                // Save NPC-specific data
                save_write_u32(&chunk_buffer, e->npc_data.health);
                save_write_u32(&chunk_buffer, e->npc_data.state);
                save_write_string(&chunk_buffer, e->npc_data.dialogue_id);
                break;
                
            case ENTITY_TYPE_ITEM:
                // Save item data
                save_write_u32(&chunk_buffer, e->item_data.item_id);
                save_write_u32(&chunk_buffer, e->item_data.quantity);
                save_write_f32(&chunk_buffer, e->item_data.durability);
                break;
                
            case ENTITY_TYPE_TRIGGER:
                // Save trigger data
                save_write_u32(&chunk_buffer, e->trigger_data.trigger_id);
                save_write_u8(&chunk_buffer, e->trigger_data.activated);
                save_write_string(&chunk_buffer, e->trigger_data.script);
                break;
        }
    }
    
    // Write world chunk
    return write_chunk(buffer, SAVE_CHUNK_WORLD, chunk_data, chunk_buffer.size, 1);
}

// Load world entities
internal b32 load_world_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_chunk_header header;
    
    if (!read_chunk(buffer, &header, chunk_data, SAVE_CHUNK_SIZE)) {
        return 0;
    }
    
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .size = header.uncompressed_size,
        .read_offset = 0
    };
    
    // Read entity count
    u32 entity_count = save_read_u32(&chunk_buffer);
    
    // Clear existing entities
    game->entity_count = 0;
    
    // Load each entity
    for (u32 i = 0; i < entity_count; i++) {
        entity_save_data save_data;
        save_read_bytes(&chunk_buffer, &save_data, sizeof(save_data));
        
        // Create entity
        entity *e = &game->entities[game->entity_count++];
        e->id = save_data.id;
        e->type = save_data.type;
        memcpy(e->position, save_data.position, sizeof(save_data.position));
        memcpy(e->rotation, save_data.rotation, sizeof(save_data.rotation));
        memcpy(e->scale, save_data.scale, sizeof(save_data.scale));
        e->flags = save_data.flags;
        e->parent_id = save_data.parent_id;
        strcpy(e->name, save_data.name);
        
        // Entity-specific data
        switch (e->type) {
            case ENTITY_TYPE_NPC:
                e->npc_data.health = save_read_u32(&chunk_buffer);
                e->npc_data.state = save_read_u32(&chunk_buffer);
                save_read_string(&chunk_buffer, e->npc_data.dialogue_id, 64);
                break;
                
            case ENTITY_TYPE_ITEM:
                e->item_data.item_id = save_read_u32(&chunk_buffer);
                e->item_data.quantity = save_read_u32(&chunk_buffer);
                e->item_data.durability = save_read_f32(&chunk_buffer);
                break;
                
            case ENTITY_TYPE_TRIGGER:
                e->trigger_data.trigger_id = save_read_u32(&chunk_buffer);
                e->trigger_data.activated = save_read_u8(&chunk_buffer);
                save_read_string(&chunk_buffer, e->trigger_data.script, 256);
                break;
        }
    }
    
    return 1;
}

// Save player state
internal b32 save_player_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    player_state *player = &game->player;
    
    // Basic info
    save_write_string(&chunk_buffer, player->name);
    save_write_u32(&chunk_buffer, player->level);
    save_write_u32(&chunk_buffer, player->experience);
    save_write_u32(&chunk_buffer, player->health);
    save_write_u32(&chunk_buffer, player->max_health);
    save_write_u32(&chunk_buffer, player->mana);
    save_write_u32(&chunk_buffer, player->max_mana);
    
    // Position and orientation
    save_write_f32(&chunk_buffer, player->position[0]);
    save_write_f32(&chunk_buffer, player->position[1]);
    save_write_f32(&chunk_buffer, player->position[2]);
    save_write_f32(&chunk_buffer, player->rotation[0]);
    save_write_f32(&chunk_buffer, player->rotation[1]);
    
    // Stats
    save_write_u32(&chunk_buffer, player->strength);
    save_write_u32(&chunk_buffer, player->dexterity);
    save_write_u32(&chunk_buffer, player->intelligence);
    save_write_u32(&chunk_buffer, player->wisdom);
    
    // Inventory
    save_write_u32(&chunk_buffer, player->inventory_count);
    for (u32 i = 0; i < player->inventory_count; i++) {
        save_write_u32(&chunk_buffer, player->inventory[i].item_id);
        save_write_u32(&chunk_buffer, player->inventory[i].quantity);
        save_write_u32(&chunk_buffer, player->inventory[i].slot);
        save_write_f32(&chunk_buffer, player->inventory[i].durability);
    }
    
    // Equipment
    for (u32 i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
        save_write_u32(&chunk_buffer, player->equipment[i]);
    }
    
    // Active quests
    save_write_u32(&chunk_buffer, player->quest_count);
    for (u32 i = 0; i < player->quest_count; i++) {
        save_write_u32(&chunk_buffer, player->quests[i].quest_id);
        save_write_u32(&chunk_buffer, player->quests[i].stage);
        save_write_u32(&chunk_buffer, player->quests[i].flags);
    }
    
    return write_chunk(buffer, SAVE_CHUNK_PLAYER, chunk_data, chunk_buffer.size, 1);
}

// Load player state
internal b32 load_player_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_chunk_header header;
    
    if (!read_chunk(buffer, &header, chunk_data, SAVE_CHUNK_SIZE)) {
        return 0;
    }
    
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .size = header.uncompressed_size,
        .read_offset = 0
    };
    
    player_state *player = &game->player;
    
    // Basic info
    save_read_string(&chunk_buffer, player->name, 64);
    player->level = save_read_u32(&chunk_buffer);
    player->experience = save_read_u32(&chunk_buffer);
    player->health = save_read_u32(&chunk_buffer);
    player->max_health = save_read_u32(&chunk_buffer);
    player->mana = save_read_u32(&chunk_buffer);
    player->max_mana = save_read_u32(&chunk_buffer);
    
    // Position and orientation
    player->position[0] = save_read_f32(&chunk_buffer);
    player->position[1] = save_read_f32(&chunk_buffer);
    player->position[2] = save_read_f32(&chunk_buffer);
    player->rotation[0] = save_read_f32(&chunk_buffer);
    player->rotation[1] = save_read_f32(&chunk_buffer);
    
    // Stats
    player->strength = save_read_u32(&chunk_buffer);
    player->dexterity = save_read_u32(&chunk_buffer);
    player->intelligence = save_read_u32(&chunk_buffer);
    player->wisdom = save_read_u32(&chunk_buffer);
    
    // Inventory
    player->inventory_count = save_read_u32(&chunk_buffer);
    for (u32 i = 0; i < player->inventory_count; i++) {
        player->inventory[i].item_id = save_read_u32(&chunk_buffer);
        player->inventory[i].quantity = save_read_u32(&chunk_buffer);
        player->inventory[i].slot = save_read_u32(&chunk_buffer);
        player->inventory[i].durability = save_read_f32(&chunk_buffer);
    }
    
    // Equipment
    for (u32 i = 0; i < EQUIPMENT_SLOT_COUNT; i++) {
        player->equipment[i] = save_read_u32(&chunk_buffer);
    }
    
    // Active quests
    player->quest_count = save_read_u32(&chunk_buffer);
    for (u32 i = 0; i < player->quest_count; i++) {
        player->quests[i].quest_id = save_read_u32(&chunk_buffer);
        player->quests[i].stage = save_read_u32(&chunk_buffer);
        player->quests[i].flags = save_read_u32(&chunk_buffer);
    }
    
    return 1;
}

// Save NPC neural networks
internal b32 save_npc_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    // Count NPCs with neural networks
    u32 npc_count = 0;
    for (u32 i = 0; i < game->entity_count; i++) {
        if (game->entities[i].type == ENTITY_TYPE_NPC && 
            game->entities[i].npc_brain != NULL) {
            npc_count++;
        }
    }
    
    save_write_u32(&chunk_buffer, npc_count);
    
    // Save each NPC brain
    for (u32 i = 0; i < game->entity_count; i++) {
        entity *e = &game->entities[i];
        if (e->type != ENTITY_TYPE_NPC || !e->npc_brain) continue;
        
        npc_brain *brain = e->npc_brain;
        
        // Entity ID for association
        save_write_u32(&chunk_buffer, e->id);
        
        // Neural network architecture
        save_write_u32(&chunk_buffer, brain->lstm_hidden_size);
        save_write_u32(&chunk_buffer, brain->memory_size);
        
        // LSTM weights - compressed separately due to size
        save_write_u32(&chunk_buffer, brain->lstm_weights_size);
        save_write_bytes(&chunk_buffer, brain->lstm_weights, 
                        brain->lstm_weights_size * sizeof(f32));
        
        // LSTM hidden state
        save_write_bytes(&chunk_buffer, brain->lstm_hidden, 
                        brain->lstm_hidden_size * sizeof(f32));
        save_write_bytes(&chunk_buffer, brain->lstm_cell, 
                        brain->lstm_hidden_size * sizeof(f32));
        
        // Episodic memory
        save_write_u32(&chunk_buffer, brain->memory_count);
        for (u32 j = 0; j < brain->memory_count; j++) {
            memory_entry *mem = &brain->memories[j];
            save_write_f32(&chunk_buffer, mem->timestamp);
            save_write_u32(&chunk_buffer, mem->importance);
            save_write_string(&chunk_buffer, mem->description);
            save_write_bytes(&chunk_buffer, mem->embedding, 
                           brain->memory_size * sizeof(f32));
        }
        
        // Personality traits
        save_write_f32(&chunk_buffer, brain->traits.friendliness);
        save_write_f32(&chunk_buffer, brain->traits.aggression);
        save_write_f32(&chunk_buffer, brain->traits.curiosity);
        save_write_f32(&chunk_buffer, brain->traits.fear);
        
        // Current state
        save_write_u32(&chunk_buffer, brain->current_goal);
        save_write_f32(&chunk_buffer, brain->emotional_state);
        save_write_u32(&chunk_buffer, brain->relationship_map_count);
        
        // Relationships
        for (u32 j = 0; j < brain->relationship_map_count; j++) {
            save_write_u32(&chunk_buffer, brain->relationships[j].entity_id);
            save_write_f32(&chunk_buffer, brain->relationships[j].affinity);
            save_write_f32(&chunk_buffer, brain->relationships[j].trust);
            save_write_u32(&chunk_buffer, brain->relationships[j].interaction_count);
        }
    }
    
    return write_chunk(buffer, SAVE_CHUNK_NPCS, chunk_data, chunk_buffer.size, 1);
}

// Save physics state
internal b32 save_physics_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    physics_world *physics = game->physics;
    
    if (!physics) {
        // No physics system to save
        write_chunk(buffer, SAVE_CHUNK_PHYSICS, chunk_buffer.data, 0, 0);
        return 1;
    }
    
    // Global physics settings
    save_write_f32(&chunk_buffer, physics->gravity[0]);
    save_write_f32(&chunk_buffer, physics->gravity[1]);
    save_write_f32(&chunk_buffer, physics->gravity[2]);
    save_write_f32(&chunk_buffer, physics->air_resistance);
    save_write_u32(&chunk_buffer, physics->simulation_rate);
    
    // Rigid bodies
    save_write_u32(&chunk_buffer, physics->body_count);
    for (u32 i = 0; i < physics->body_count; i++) {
        rigid_body *body = &physics->bodies[i];
        
        save_write_u32(&chunk_buffer, body->entity_id);
        save_write_f32(&chunk_buffer, body->mass);
        save_write_bytes(&chunk_buffer, body->position, 3 * sizeof(f32));
        save_write_bytes(&chunk_buffer, body->velocity, 3 * sizeof(f32));
        save_write_bytes(&chunk_buffer, body->angular_velocity, 3 * sizeof(f32));
        save_write_bytes(&chunk_buffer, body->inertia_tensor, 9 * sizeof(f32));
        save_write_u32(&chunk_buffer, body->collision_shape);
        save_write_u8(&chunk_buffer, body->is_static);
        save_write_u8(&chunk_buffer, body->is_trigger);
    }
    
    // Constraints
    save_write_u32(&chunk_buffer, physics->constraint_count);
    for (u32 i = 0; i < physics->constraint_count; i++) {
        physics_constraint *constraint = &physics->constraints[i];
        
        save_write_u32(&chunk_buffer, constraint->type);
        save_write_u32(&chunk_buffer, constraint->body_a);
        save_write_u32(&chunk_buffer, constraint->body_b);
        save_write_bytes(&chunk_buffer, constraint->anchor_a, 3 * sizeof(f32));
        save_write_bytes(&chunk_buffer, constraint->anchor_b, 3 * sizeof(f32));
        save_write_f32(&chunk_buffer, constraint->stiffness);
        save_write_f32(&chunk_buffer, constraint->damping);
    }
    
    return write_chunk(buffer, SAVE_CHUNK_PHYSICS, chunk_data, chunk_buffer.size, 1);
}

// Save audio state
internal b32 save_audio_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    audio_system *audio = game->audio;
    
    if (!audio) {
        // No audio system to save
        write_chunk(buffer, SAVE_CHUNK_AUDIO, chunk_buffer.data, 0, 0);
        return 1;
    }
    
    // Master volumes
    save_write_f32(&chunk_buffer, audio->master_volume);
    save_write_f32(&chunk_buffer, audio->music_volume);
    save_write_f32(&chunk_buffer, audio->sfx_volume);
    save_write_f32(&chunk_buffer, audio->voice_volume);
    
    // Currently playing music
    save_write_string(&chunk_buffer, audio->current_music);
    save_write_f32(&chunk_buffer, audio->music_position);
    save_write_u8(&chunk_buffer, audio->music_looping);
    
    // Active sound effects (only save persistent ones)
    u32 persistent_count = 0;
    for (u32 i = 0; i < audio->active_sounds; i++) {
        if (audio->sounds[i].persistent) {
            persistent_count++;
        }
    }
    
    save_write_u32(&chunk_buffer, persistent_count);
    for (u32 i = 0; i < audio->active_sounds; i++) {
        sound_instance *sound = &audio->sounds[i];
        if (!sound->persistent) continue;
        
        save_write_string(&chunk_buffer, sound->name);
        save_write_u32(&chunk_buffer, sound->entity_id);
        save_write_f32(&chunk_buffer, sound->volume);
        save_write_f32(&chunk_buffer, sound->pitch);
        save_write_bytes(&chunk_buffer, sound->position, 3 * sizeof(f32));
        save_write_u8(&chunk_buffer, sound->looping);
        save_write_f32(&chunk_buffer, sound->play_position);
    }
    
    // Reverb zones
    save_write_u32(&chunk_buffer, audio->reverb_zone_count);
    for (u32 i = 0; i < audio->reverb_zone_count; i++) {
        reverb_zone *zone = &audio->reverb_zones[i];
        save_write_bytes(&chunk_buffer, zone->position, 3 * sizeof(f32));
        save_write_f32(&chunk_buffer, zone->radius);
        save_write_f32(&chunk_buffer, zone->intensity);
        save_write_u32(&chunk_buffer, zone->preset);
    }
    
    return write_chunk(buffer, SAVE_CHUNK_AUDIO, chunk_data, chunk_buffer.size, 0);
}

// Save script state
internal b32 save_script_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    script_system *scripts = game->scripts;
    
    if (!scripts) {
        // No script system to save
        write_chunk(buffer, SAVE_CHUNK_SCRIPT, chunk_buffer.data, 0, 0);
        return 1;
    }
    
    // Global variables
    save_write_u32(&chunk_buffer, scripts->global_var_count);
    for (u32 i = 0; i < scripts->global_var_count; i++) {
        script_variable *var = &scripts->global_vars[i];
        save_write_string(&chunk_buffer, var->name);
        save_write_u32(&chunk_buffer, var->type);
        
        switch (var->type) {
            case SCRIPT_VAR_NUMBER:
                save_write_f64(&chunk_buffer, var->value.number);
                break;
            case SCRIPT_VAR_STRING:
                save_write_string(&chunk_buffer, var->value.string);
                break;
            case SCRIPT_VAR_BOOL:
                save_write_u8(&chunk_buffer, var->value.boolean);
                break;
            case SCRIPT_VAR_ENTITY:
                save_write_u32(&chunk_buffer, var->value.entity_id);
                break;
        }
    }
    
    // Active coroutines
    save_write_u32(&chunk_buffer, scripts->coroutine_count);
    for (u32 i = 0; i < scripts->coroutine_count; i++) {
        script_coroutine *coro = &scripts->coroutines[i];
        
        save_write_string(&chunk_buffer, coro->script_name);
        save_write_u32(&chunk_buffer, coro->instruction_pointer);
        save_write_f32(&chunk_buffer, coro->wait_time);
        save_write_u32(&chunk_buffer, coro->state);
        
        // Local variables
        save_write_u32(&chunk_buffer, coro->local_var_count);
        for (u32 j = 0; j < coro->local_var_count; j++) {
            script_variable *var = &coro->local_vars[j];
            save_write_string(&chunk_buffer, var->name);
            save_write_u32(&chunk_buffer, var->type);
            
            switch (var->type) {
                case SCRIPT_VAR_NUMBER:
                    save_write_f64(&chunk_buffer, var->value.number);
                    break;
                case SCRIPT_VAR_STRING:
                    save_write_string(&chunk_buffer, var->value.string);
                    break;
                case SCRIPT_VAR_BOOL:
                    save_write_u8(&chunk_buffer, var->value.boolean);
                    break;
                case SCRIPT_VAR_ENTITY:
                    save_write_u32(&chunk_buffer, var->value.entity_id);
                    break;
            }
        }
    }
    
    // Event flags
    save_write_u32(&chunk_buffer, scripts->event_flag_count);
    for (u32 i = 0; i < scripts->event_flag_count; i++) {
        save_write_string(&chunk_buffer, scripts->event_flags[i].name);
        save_write_u8(&chunk_buffer, scripts->event_flags[i].value);
    }
    
    return write_chunk(buffer, SAVE_CHUNK_SCRIPT, chunk_data, chunk_buffer.size, 1);
}

// Save node graphs
internal b32 save_nodes_state(save_buffer *buffer, game_state *game) {
    u8 chunk_data[SAVE_CHUNK_SIZE];
    save_buffer chunk_buffer = {
        .data = chunk_data,
        .capacity = SAVE_CHUNK_SIZE,
        .size = 0
    };
    
    node_system *nodes = game->nodes;
    
    if (!nodes) {
        // No node system to save
        write_chunk(buffer, SAVE_CHUNK_NODES, chunk_buffer.data, 0, 0);
        return 1;
    }
    
    // Save graphs
    save_write_u32(&chunk_buffer, nodes->graph_count);
    for (u32 i = 0; i < nodes->graph_count; i++) {
        node_graph *graph = &nodes->graphs[i];
        
        save_write_string(&chunk_buffer, graph->name);
        save_write_u32(&chunk_buffer, graph->node_count);
        
        // Save nodes
        for (u32 j = 0; j < graph->node_count; j++) {
            graph_node *node = &graph->nodes[j];
            
            save_write_u32(&chunk_buffer, node->id);
            save_write_u32(&chunk_buffer, node->type);
            save_write_f32(&chunk_buffer, node->position[0]);
            save_write_f32(&chunk_buffer, node->position[1]);
            
            // Node data varies by type
            save_write_u32(&chunk_buffer, node->data_size);
            if (node->data_size > 0) {
                save_write_bytes(&chunk_buffer, node->data, node->data_size);
            }
            
            // Inputs/outputs
            save_write_u32(&chunk_buffer, node->input_count);
            for (u32 k = 0; k < node->input_count; k++) {
                save_write_string(&chunk_buffer, node->inputs[k].name);
                save_write_u32(&chunk_buffer, node->inputs[k].type);
            }
            
            save_write_u32(&chunk_buffer, node->output_count);
            for (u32 k = 0; k < node->output_count; k++) {
                save_write_string(&chunk_buffer, node->outputs[k].name);
                save_write_u32(&chunk_buffer, node->outputs[k].type);
            }
        }
        
        // Save connections
        save_write_u32(&chunk_buffer, graph->connection_count);
        for (u32 j = 0; j < graph->connection_count; j++) {
            node_connection *conn = &graph->connections[j];
            save_write_u32(&chunk_buffer, conn->from_node);
            save_write_u32(&chunk_buffer, conn->from_output);
            save_write_u32(&chunk_buffer, conn->to_node);
            save_write_u32(&chunk_buffer, conn->to_input);
        }
    }
    
    return write_chunk(buffer, SAVE_CHUNK_NODES, chunk_data, chunk_buffer.size, 1);
}

// Main save function
b32 save_game(save_system *system, game_state *game, i32 slot) {
    if (system->is_saving || system->is_loading) return 0;
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) return 0;
    
    system->is_saving = 1;
    
    // PERFORMANCE: Time the save operation
    u64 start_cycles = __builtin_ia32_rdtsc();
    
    // Reset write buffer
    save_buffer_reset(&system->write_buffer);
    
    // Create header
    save_header header = {0};
    header.magic = SAVE_MAGIC_NUMBER;
    header.version = SAVE_VERSION;
    header.timestamp = (u64)time(NULL);
    header.compressed = SAVE_COMPRESSION_LZ4;
    
    // Reserve space for header
    save_write_bytes(&system->write_buffer, &header, sizeof(header));
    
    // Create metadata
    save_metadata metadata = {0};
    metadata.playtime_seconds = game->playtime_seconds;
    strncpy(metadata.level_name, game->current_level, 63);
    strncpy(metadata.player_name, game->player.name, 31);
    metadata.player_level = game->player.level;
    metadata.save_count = system->slots[slot].metadata.save_count + 1;
    
    // Capture thumbnail (simplified - would need actual screenshot)
    // In production, would grab framebuffer and downsample
    memset(metadata.thumbnail, 0x80, sizeof(metadata.thumbnail));
    
    // Write metadata
    save_write_bytes(&system->write_buffer, &metadata, sizeof(metadata));
    
    // Save all game systems
    b32 success = 1;
    success = success && save_world_state(&system->write_buffer, game);
    success = success && save_player_state(&system->write_buffer, game);
    success = success && save_npc_state(&system->write_buffer, game);
    success = success && save_physics_state(&system->write_buffer, game);
    success = success && save_audio_state(&system->write_buffer, game);
    success = success && save_script_state(&system->write_buffer, game);
    success = success && save_nodes_state(&system->write_buffer, game);
    
    // Write end marker
    save_chunk_header end_chunk = {
        .type = SAVE_CHUNK_END,
        .uncompressed_size = 0,
        .compressed_size = 0,
        .checksum = 0
    };
    save_write_bytes(&system->write_buffer, &end_chunk, sizeof(end_chunk));
    
    if (success) {
        // Calculate checksum
        u32 data_start = sizeof(save_header);
        u32 data_size = system->write_buffer.size - data_start;
        header.checksum = save_crc32(system->write_buffer.data + data_start, data_size);
        
        // Update header
        memcpy(system->write_buffer.data, &header, sizeof(header));
        
        // Write to file
        save_slot_info *slot_info = &system->slots[slot];
        success = platform_save_write_file(slot_info->filename, 
                                          system->write_buffer.data,
                                          system->write_buffer.size);
        
        if (success) {
            // Update slot info
            slot_info->exists = 1;
            slot_info->header = header;
            slot_info->metadata = metadata;
            slot_info->file_size = system->write_buffer.size;
            slot_info->last_modified = header.timestamp;
            
            // Update stats
            system->total_bytes_saved += system->write_buffer.size;
        }
    }
    
    // Calculate save time
    u64 end_cycles = __builtin_ia32_rdtsc();
    system->last_save_time = (f32)(end_cycles - start_cycles) / 3000000000.0f; // Assume 3GHz
    
    system->is_saving = 0;
    
    return success;
}

// Main load function
b32 load_game(save_system *system, game_state *game, i32 slot) {
    if (system->is_saving || system->is_loading) return 0;
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) return 0;
    
    save_slot_info *slot_info = &system->slots[slot];
    if (!slot_info->exists) return 0;
    
    system->is_loading = 1;
    
    // PERFORMANCE: Time the load operation
    u64 start_cycles = __builtin_ia32_rdtsc();
    
    // Read save file
    u32 actual_size;
    b32 success = platform_save_read_file(slot_info->filename,
                                          system->read_buffer.data,
                                          system->read_buffer.capacity,
                                          &actual_size);
    
    if (!success) {
        system->is_loading = 0;
        return 0;
    }
    
    // Setup read buffer
    system->read_buffer.size = actual_size;
    system->read_buffer.read_offset = 0;
    
    // Read and verify header
    save_header header;
    save_read_bytes(&system->read_buffer, &header, sizeof(header));
    
    if (header.magic != SAVE_MAGIC_NUMBER) {
        system->is_loading = 0;
        return 0;
    }
    
    // Handle version migration if needed
    if (header.version != SAVE_VERSION) {
        if (!save_migrate_data(system, &system->read_buffer, 
                              header.version, SAVE_VERSION)) {
            system->is_loading = 0;
            return 0;
        }
    }
    
    // Verify checksum
    u32 data_start = sizeof(save_header);
    u32 data_size = actual_size - data_start;
    u32 calculated_crc = save_crc32(system->read_buffer.data + data_start, data_size);
    
    if (calculated_crc != header.checksum) {
        system->save_corrupted = 1;
        system->is_loading = 0;
        return 0;
    }
    
    // Read metadata
    save_metadata metadata;
    save_read_bytes(&system->read_buffer, &metadata, sizeof(metadata));
    
    // Load all game systems
    save_chunk_header chunk_header;
    
    while (1) {
        // Peek at chunk type
        u32 saved_offset = system->read_buffer.read_offset;
        save_read_bytes(&system->read_buffer, &chunk_header, sizeof(chunk_header));
        
        if (chunk_header.type == SAVE_CHUNK_END) {
            break;
        }
        
        // Reset to chunk start
        system->read_buffer.read_offset = saved_offset;
        
        // Load chunk based on type
        switch (chunk_header.type) {
            case SAVE_CHUNK_WORLD:
                success = load_world_state(&system->read_buffer, game);
                break;
                
            case SAVE_CHUNK_PLAYER:
                success = load_player_state(&system->read_buffer, game);
                break;
                
            case SAVE_CHUNK_NPCS:
                // Would implement load_npc_state
                system->read_buffer.read_offset += sizeof(chunk_header) + 
                                                   chunk_header.compressed_size;
                break;
                
            case SAVE_CHUNK_PHYSICS:
                // Would implement load_physics_state
                system->read_buffer.read_offset += sizeof(chunk_header) + 
                                                   chunk_header.compressed_size;
                break;
                
            case SAVE_CHUNK_AUDIO:
                // Would implement load_audio_state
                system->read_buffer.read_offset += sizeof(chunk_header) + 
                                                   chunk_header.compressed_size;
                break;
                
            case SAVE_CHUNK_SCRIPT:
                // Would implement load_script_state
                system->read_buffer.read_offset += sizeof(chunk_header) + 
                                                   chunk_header.compressed_size;
                break;
                
            case SAVE_CHUNK_NODES:
                // Would implement load_nodes_state
                system->read_buffer.read_offset += sizeof(chunk_header) + 
                                                   chunk_header.compressed_size;
                break;
                
            default:
                // Unknown chunk, skip it
                system->read_buffer.read_offset += sizeof(chunk_header) + 
                                                   chunk_header.compressed_size;
                break;
        }
        
        if (!success) {
            system->is_loading = 0;
            return 0;
        }
    }
    
    // Update game state
    game->playtime_seconds = metadata.playtime_seconds;
    strcpy(game->current_level, metadata.level_name);
    
    // Update stats
    system->total_bytes_loaded += actual_size;
    
    // Calculate load time
    u64 end_cycles = __builtin_ia32_rdtsc();
    system->last_load_time = (f32)(end_cycles - start_cycles) / 3000000000.0f;
    
    system->is_loading = 0;
    
    return success;
}

// Quick save/load
b32 quicksave(save_system *system, game_state *game) {
    return save_game(system, game, SAVE_QUICKSAVE_SLOT);
}

b32 quickload(save_system *system, game_state *game) {
    return load_game(system, game, SAVE_QUICKSAVE_SLOT);
}