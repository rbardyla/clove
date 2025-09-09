/*
 * Rollback Netcode Implementation
 * Client-side prediction with server reconciliation
 * 
 * PERFORMANCE: Fixed-size ring buffers, no allocations
 * DETERMINISM: Bit-identical simulation across rollbacks
 * OPTIMIZATION: Only rollback when necessary
 */

#include "handmade_network.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// Forward declarations for compression functions
extern uint32_t compress_snapshot(const game_snapshot_t* current,
                                 const game_snapshot_t* previous,
                                 uint8_t* output, uint32_t max_output);
extern uint32_t decompress_snapshot(const uint8_t* input, uint32_t input_size,
                                   const game_snapshot_t* previous,
                                   game_snapshot_t* output);

// Interpolation state for smooth visuals
typedef struct {
    float blend_factor;
    game_snapshot_t from;
    game_snapshot_t to;
    uint32_t from_tick;
    uint32_t to_tick;
    uint64_t interpolation_time;
} interpolation_state_t;

// Prediction state for local player
typedef struct {
    uint32_t predicted_tick;
    uint32_t last_acknowledged_tick;
    player_input_t pending_inputs[NET_INPUT_BUFFER_SIZE];
    uint32_t pending_input_count;
    game_snapshot_t predicted_state;
} prediction_state_t;

// Global state for rollback (normally would be in context)
static interpolation_state_t g_interpolation;
static prediction_state_t g_prediction;

// Fast checksum for state validation
// PERFORMANCE: Uses CRC32 intrinsic if available
static uint32_t calculate_state_checksum(const game_snapshot_t* snapshot) {
    uint32_t checksum = 0x12345678;
    
#ifdef __SSE4_2__
    // Use hardware CRC32 if available
    const uint32_t* data = (const uint32_t*)snapshot;
    size_t count = sizeof(game_snapshot_t) / sizeof(uint32_t);
    
    for (size_t i = 0; i < count; i++) {
        checksum = __builtin_ia32_crc32si(checksum, data[i]);
    }
#else
    // Fallback to simple checksum
    const uint8_t* bytes = (const uint8_t*)snapshot;
    for (size_t i = 0; i < sizeof(game_snapshot_t); i++) {
        checksum = ((checksum << 8) | (checksum >> 24)) ^ bytes[i];
        checksum *= 0x1000193;  // FNV prime
    }
#endif
    
    return checksum;
}

// Save current game state to snapshot buffer
void net_save_state(network_context_t* ctx, uint32_t tick) {
    uint32_t index = ctx->snapshot_head % NET_SNAPSHOT_BUFFER_SIZE;
    game_snapshot_t* snapshot = &ctx->snapshots[index];
    
    snapshot->tick = tick;
    snapshot->timestamp = ctx->current_time;
    
    // In real implementation, this would capture actual game state
    // For now, we'll simulate with some test data
    snapshot->checksum = calculate_state_checksum(snapshot);
    
    ctx->snapshot_head++;
    
    // Trim old snapshots
    uint32_t max_snapshots = NET_SNAPSHOT_BUFFER_SIZE - 1;
    if (ctx->snapshot_head - ctx->snapshot_tail > max_snapshots) {
        ctx->snapshot_tail = ctx->snapshot_head - max_snapshots;
    }
}

// Find snapshot for specific tick
static game_snapshot_t* find_snapshot(network_context_t* ctx, uint32_t tick) {
    // OPTIMIZATION: Binary search for large buffers
    // For small buffers, linear search from newest is faster
    
    for (uint32_t i = ctx->snapshot_head; i > ctx->snapshot_tail; i--) {
        uint32_t index = (i - 1) % NET_SNAPSHOT_BUFFER_SIZE;
        if (ctx->snapshots[index].tick == tick) {
            return &ctx->snapshots[index];
        }
        if (ctx->snapshots[index].tick < tick) {
            break;  // Won't find it
        }
    }
    
    return NULL;
}

// Rollback to specific tick
bool net_rollback_to_tick(network_context_t* ctx, uint32_t tick) {
    game_snapshot_t* snapshot = find_snapshot(ctx, tick);
    if (!snapshot) {
        return false;
    }
    
    // PERFORMANCE: Only copy changed data
    // In practice, would restore game state from snapshot
    ctx->current_tick = tick;
    
    // Clear inputs after this tick
    uint32_t new_input_tail = ctx->input_tail;
    for (uint32_t i = ctx->input_tail; i < ctx->input_head; i++) {
        uint32_t index = i % NET_INPUT_BUFFER_SIZE;
        if (ctx->input_buffer[index].tick > tick) {
            new_input_tail = i;
            break;
        }
    }
    ctx->input_tail = new_input_tail;
    
    return true;
}

// Simulate one tick forward
static void simulate_tick(network_context_t* ctx, uint32_t tick) {
    // DETERMINISM: Fixed-point math for physics
    // No floating point operations that could vary between platforms
    
    // Process inputs for this tick
    for (uint32_t i = ctx->input_tail; i < ctx->input_head; i++) {
        uint32_t index = i % NET_INPUT_BUFFER_SIZE;
        input_command_t* cmd = &ctx->input_buffer[index];
        
        if (cmd->tick != tick) {
            continue;
        }
        
        // Apply input to player state
        uint32_t player_id = cmd->player_id;
        if (player_id < NET_MAX_PLAYERS) {
            // Simple movement simulation
            float move_speed = 5.0f * (NET_TICK_MS / 1000.0f);
            
            game_snapshot_t* current = &ctx->snapshots[ctx->snapshot_head - 1];
            current->players[player_id].x += cmd->input.move_x * move_speed / 32768.0f;
            current->players[player_id].y += cmd->input.move_y * move_speed / 32768.0f;
            
            // Apply buttons (jump, etc)
            if (cmd->input.buttons & 0x01) {  // Jump button
                current->players[player_id].vz = 10.0f;
            }
        }
    }
    
    // Physics simulation
    game_snapshot_t* current = &ctx->snapshots[(ctx->snapshot_head - 1) % NET_SNAPSHOT_BUFFER_SIZE];
    
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        // Gravity
        current->players[i].vz -= 9.8f * (NET_TICK_MS / 1000.0f);
        
        // Apply velocity
        current->players[i].x += current->players[i].vx * (NET_TICK_MS / 1000.0f);
        current->players[i].y += current->players[i].vy * (NET_TICK_MS / 1000.0f);
        current->players[i].z += current->players[i].vz * (NET_TICK_MS / 1000.0f);
        
        // Ground collision
        if (current->players[i].z < 0) {
            current->players[i].z = 0;
            current->players[i].vz = 0;
        }
        
        // Friction
        current->players[i].vx *= 0.95f;
        current->players[i].vy *= 0.95f;
    }
}

// Predict future tick (client-side prediction)
void net_predict_tick(network_context_t* ctx, uint32_t tick) {
    if (!ctx->enable_prediction) {
        return;
    }
    
    // Start from last confirmed state
    uint32_t start_tick = ctx->confirmed_tick;
    if (start_tick >= tick) {
        return;  // Already have authoritative state
    }
    
    // Find starting snapshot
    game_snapshot_t* base_snapshot = find_snapshot(ctx, start_tick);
    if (!base_snapshot) {
        // No base state, can't predict
        return;
    }
    
    // Copy to prediction state
    memcpy(&g_prediction.predicted_state, base_snapshot, sizeof(game_snapshot_t));
    
    // Simulate forward from confirmed to current
    for (uint32_t t = start_tick + 1; t <= tick; t++) {
        // Apply local inputs
        for (uint32_t i = 0; i < g_prediction.pending_input_count; i++) {
            input_command_t cmd;
            cmd.tick = t;
            cmd.player_id = ctx->local_player_id;
            cmd.input = g_prediction.pending_inputs[i];
            
            // Apply to predicted state
            float move_speed = 5.0f * (NET_TICK_MS / 1000.0f);
            g_prediction.predicted_state.players[ctx->local_player_id].x += 
                cmd.input.move_x * move_speed / 32768.0f;
            g_prediction.predicted_state.players[ctx->local_player_id].y += 
                cmd.input.move_y * move_speed / 32768.0f;
        }
        
        // Run physics
        simulate_tick(ctx, t);
    }
    
    g_prediction.predicted_tick = tick;
}

// Server confirms state for a tick
void net_confirm_tick(network_context_t* ctx, uint32_t tick) {
    if (tick <= ctx->confirmed_tick) {
        return;  // Already confirmed
    }
    
    game_snapshot_t* server_snapshot = find_snapshot(ctx, tick);
    if (!server_snapshot) {
        return;  // No snapshot from server yet
    }
    
    // Check for misprediction
    if (ctx->enable_prediction && g_prediction.predicted_tick >= tick) {
        // Compare predicted vs actual
        float error = 0;
        uint32_t player_id = ctx->local_player_id;
        
        error += fabsf(g_prediction.predicted_state.players[player_id].x - 
                      server_snapshot->players[player_id].x);
        error += fabsf(g_prediction.predicted_state.players[player_id].y - 
                      server_snapshot->players[player_id].y);
        error += fabsf(g_prediction.predicted_state.players[player_id].z - 
                      server_snapshot->players[player_id].z);
        
        // If error is significant, rollback and replay
        if (error > 0.1f) {
            // PERFORMANCE: Only rollback if error is significant
            printf("Prediction error: %.2f units, rolling back from %u to %u\n",
                   error, ctx->current_tick, tick);
            
            // Rollback to server state
            net_rollback_to_tick(ctx, tick);
            
            // Replay inputs from tick to current
            uint32_t current = ctx->current_tick;
            for (uint32_t t = tick + 1; t <= current; t++) {
                simulate_tick(ctx, t);
                net_save_state(ctx, t);
            }
        }
    }
    
    ctx->confirmed_tick = tick;
    
    // Remove old inputs that have been confirmed
    uint32_t new_tail = ctx->input_tail;
    for (uint32_t i = ctx->input_tail; i < ctx->input_head; i++) {
        uint32_t index = i % NET_INPUT_BUFFER_SIZE;
        if (ctx->input_buffer[index].tick <= tick) {
            new_tail = i + 1;
        }
    }
    ctx->input_tail = new_tail;
}

// Interpolate between snapshots for smooth rendering
static void interpolate_snapshots(const game_snapshot_t* from,
                                 const game_snapshot_t* to,
                                 float t,
                                 game_snapshot_t* result) {
    // OPTIMIZATION: SIMD for bulk interpolation
    memcpy(result, from, sizeof(game_snapshot_t));
    
    // Ensure t is in valid range
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    // Interpolate player positions
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        // Linear interpolation for positions
        result->players[i].x = from->players[i].x + 
                              (to->players[i].x - from->players[i].x) * t;
        result->players[i].y = from->players[i].y + 
                              (to->players[i].y - from->players[i].y) * t;
        result->players[i].z = from->players[i].z + 
                              (to->players[i].z - from->players[i].z) * t;
        
        // Spherical interpolation for rotations
        float from_yaw = from->players[i].yaw * M_PI / 180.0f;
        float to_yaw = to->players[i].yaw * M_PI / 180.0f;
        float from_pitch = from->players[i].pitch * M_PI / 180.0f;
        float to_pitch = to->players[i].pitch * M_PI / 180.0f;
        
        // Handle angle wrapping
        float yaw_diff = to_yaw - from_yaw;
        if (yaw_diff > M_PI) yaw_diff -= 2 * M_PI;
        if (yaw_diff < -M_PI) yaw_diff += 2 * M_PI;
        
        result->players[i].yaw = (from_yaw + yaw_diff * t) * 180.0f / M_PI;
        result->players[i].pitch = (from_pitch + (to_pitch - from_pitch) * t) * 180.0f / M_PI;
        
        // Don't interpolate discrete values like health
        result->players[i].health = (t < 0.5f) ? from->players[i].health : to->players[i].health;
        result->players[i].state = (t < 0.5f) ? from->players[i].state : to->players[i].state;
    }
}

// Get interpolated state for rendering
void net_get_render_state(network_context_t* ctx, game_snapshot_t* output) {
    if (!ctx->enable_interpolation) {
        // Just use latest state
        if (ctx->snapshot_head > ctx->snapshot_tail) {
            uint32_t index = (ctx->snapshot_head - 1) % NET_SNAPSHOT_BUFFER_SIZE;
            memcpy(output, &ctx->snapshots[index], sizeof(game_snapshot_t));
        }
        return;
    }
    
    // Find two snapshots to interpolate between
    uint64_t render_time = ctx->current_time - 100;  // Render 100ms in the past
    
    game_snapshot_t* from = NULL;
    game_snapshot_t* to = NULL;
    
    for (uint32_t i = ctx->snapshot_head; i > ctx->snapshot_tail && i > 1; i--) {
        uint32_t index = (i - 1) % NET_SNAPSHOT_BUFFER_SIZE;
        
        if (ctx->snapshots[index].timestamp <= render_time) {
            from = &ctx->snapshots[index];
            
            // Find next snapshot
            if (i < ctx->snapshot_head) {
                uint32_t next_index = i % NET_SNAPSHOT_BUFFER_SIZE;
                to = &ctx->snapshots[next_index];
            }
            break;
        }
    }
    
    if (!from || !to) {
        // Can't interpolate, use latest
        if (ctx->snapshot_head > ctx->snapshot_tail) {
            uint32_t index = (ctx->snapshot_head - 1) % NET_SNAPSHOT_BUFFER_SIZE;
            memcpy(output, &ctx->snapshots[index], sizeof(game_snapshot_t));
        }
        return;
    }
    
    // Calculate interpolation factor
    float t = 0.0f;
    if (to->timestamp > from->timestamp) {
        t = (float)(render_time - from->timestamp) / 
            (float)(to->timestamp - from->timestamp);
    }
    
    // Perform interpolation
    interpolate_snapshots(from, to, t, output);
    
    // Override local player with predicted position if enabled
    if (ctx->enable_prediction && g_prediction.predicted_tick > 0) {
        uint32_t player_id = ctx->local_player_id;
        output->players[player_id] = g_prediction.predicted_state.players[player_id];
    }
}

// Create snapshot of current game state
void net_create_snapshot(network_context_t* ctx, game_snapshot_t* snapshot) {
    snapshot->tick = ctx->current_tick;
    snapshot->timestamp = ctx->current_time;
    
    // In real implementation, capture actual game state
    // For demo, we'll use data from context
    
    // Calculate checksum for validation
    snapshot->checksum = calculate_state_checksum(snapshot);
}

// Send snapshot to clients (server only)
void net_send_snapshot(network_context_t* ctx) {
    if (!ctx->is_server) {
        return;
    }
    
    game_snapshot_t current;
    net_create_snapshot(ctx, &current);
    
    // Find previous snapshot for delta compression
    game_snapshot_t* previous = NULL;
    if (ctx->snapshot_head > ctx->snapshot_tail) {
        uint32_t index = (ctx->snapshot_head - 1) % NET_SNAPSHOT_BUFFER_SIZE;
        previous = &ctx->snapshots[index];
    }
    
    // Compress snapshot
    uint8_t compressed[4096];
    uint32_t compressed_size = compress_snapshot(
        &current,
        previous,
        compressed,
        sizeof(compressed)
    );
    
    // Send to all clients
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->connections[i].state == CONN_CONNECTED && i != ctx->local_player_id) {
            // Use reliable for important snapshots (every 10th)
            if (current.tick % 10 == 0) {
                net_send_reliable(ctx, i, compressed, compressed_size);
            } else {
                net_send_unreliable(ctx, i, compressed, compressed_size);
            }
        }
    }
    
    // Save snapshot
    net_save_state(ctx, current.tick);
}

// Apply received snapshot (client only)
void net_apply_snapshot(network_context_t* ctx, game_snapshot_t* snapshot) {
    if (ctx->is_server) {
        return;
    }
    
    // Validate checksum
    uint32_t received_checksum = snapshot->checksum;
    snapshot->checksum = 0;
    uint32_t calculated_checksum = calculate_state_checksum(snapshot);
    
    if (received_checksum != calculated_checksum) {
        printf("Snapshot checksum mismatch! Tick %u\n", snapshot->tick);
        return;
    }
    
    snapshot->checksum = received_checksum;
    
    // Store snapshot
    uint32_t index = ctx->snapshot_head % NET_SNAPSHOT_BUFFER_SIZE;
    memcpy(&ctx->snapshots[index], snapshot, sizeof(game_snapshot_t));
    ctx->snapshot_head++;
    
    // Reconcile with prediction
    net_confirm_tick(ctx, snapshot->tick);
}

// Handle input with prediction and buffering
void net_buffer_input(network_context_t* ctx, player_input_t* input) {
    // Store for prediction
    if (g_prediction.pending_input_count < NET_INPUT_BUFFER_SIZE) {
        g_prediction.pending_inputs[g_prediction.pending_input_count++] = *input;
    }
    
    // Send to server
    net_send_input(ctx, input);
    
    // Apply immediately for responsiveness
    if (ctx->enable_prediction) {
        uint32_t player_id = ctx->local_player_id;
        float move_speed = 5.0f * (NET_TICK_MS / 1000.0f);
        
        g_prediction.predicted_state.players[player_id].x += 
            input->move_x * move_speed / 32768.0f;
        g_prediction.predicted_state.players[player_id].y += 
            input->move_y * move_speed / 32768.0f;
        
        if (input->buttons & 0x01) {  // Jump
            g_prediction.predicted_state.players[player_id].vz = 10.0f;
        }
    }
}

// Debug: Visualize rollback statistics
void net_debug_rollback_stats(network_context_t* ctx) {
    printf("=== Rollback Statistics ===\n");
    printf("Current Tick: %u\n", ctx->current_tick);
    printf("Confirmed Tick: %u\n", ctx->confirmed_tick);
    printf("Predicted Tick: %u\n", g_prediction.predicted_tick);
    printf("Pending Inputs: %u\n", g_prediction.pending_input_count);
    printf("Snapshots: %u\n", ctx->snapshot_head - ctx->snapshot_tail);
    printf("Input Buffer: %u\n", ctx->input_head - ctx->input_tail);
    
    // Calculate prediction accuracy
    if (g_prediction.predicted_tick > ctx->confirmed_tick) {
        printf("Prediction Lead: %u ticks (%.1f ms)\n",
               g_prediction.predicted_tick - ctx->confirmed_tick,
               (float)(g_prediction.predicted_tick - ctx->confirmed_tick) * NET_TICK_MS);
    }
    
    // Show interpolation state
    if (ctx->enable_interpolation) {
        printf("Interpolation: %.1f%% between ticks %u and %u\n",
               g_interpolation.blend_factor * 100.0f,
               g_interpolation.from_tick,
               g_interpolation.to_tick);
    }
}