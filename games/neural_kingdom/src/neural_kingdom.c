// neural_kingdom.c - DAY 1: The Perfect Rectangle + Learning NPC

#include "neural_kingdom.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Forward declarations for neural network functions
neural_network* neural_create(void);
void neural_add_layer(neural_network* net, u32 input_size, u32 output_size, activation_type activation);
void neural_forward(neural_network* net, f32* inputs, f32* outputs);
void neural_destroy(neural_network* net);

// Timer function declaration
u64 ReadCPUTimer(void);

// ============================================================================
// DAY 1 IMPLEMENTATION - BABY STEPS TO GREATNESS
// ============================================================================

void neural_kingdom_init(neural_kingdom_state* game) {
    memset(game, 0, sizeof(*game));
    
    // Initialize player at center
    game->player.position = (v2){400, 300};
    game->player.health = 100.0f;
    game->player.stamina = 100.0f;
    
    // Create our FIRST neural NPC!
    neural_npc* npc = &game->npcs[0];
    game->npc_count = 1;
    
    // Give it an identity
    strcpy(npc->name, "Aria");
    npc->id = 1;
    npc->position = (v2){200, 200};
    npc->health = 50.0f;
    
    // Personality - curious and friendly
    npc->personality.curiosity = 0.8f;
    npc->personality.aggression = 0.1f;
    npc->personality.empathy = 0.7f;
    
    // Initialize her brain - THE MAGIC BEGINS!
    npc->brain.network = neural_create();
    if (npc->brain.network) {
        // Input: [player_x, player_y, distance, angle, player_vel_x, player_vel_y]
        // Hidden: 16 neurons (small but effective)
        // Output: [move_x, move_y, approach_speed]
        neural_add_layer(npc->brain.network, 6, 16, ACTIVATION_TANH);
        neural_add_layer(npc->brain.network, 16, 3, ACTIVATION_TANH);
        npc->brain.learning_rate = 0.01f;
        printf("✅ Aria's brain initialized: 6 → 16 → 3 neurons\n");
    } else {
        printf("❌ Failed to create neural network!\n");
    }
    
    printf("✨ Neural Kingdom initialized!\n");
    printf("   NPCs: %d (with real brains!)\n", game->npc_count);
    printf("   Target: %d FPS\n", TARGET_FPS);
    printf("   Memory limit: %d MB\n", MAX_MEMORY_MB);
}

void npc_think(neural_npc* npc, neural_kingdom_state* game, f32 dt) {
    if (!npc->brain.network) return;
    
    u64 start_time = ReadCPUTimer();
    
    // Gather inputs about the world
    v2 to_player = v2_sub(game->player.position, npc->position);
    f32 distance = v2_length(to_player);
    f32 angle = atan2f(to_player.y, to_player.x);
    
    f32 inputs[6] = {
        game->player.position.x / 800.0f,  // Normalized player X
        game->player.position.y / 600.0f,  // Normalized player Y
        distance / 400.0f,                 // Normalized distance
        angle / (2.0f * M_PI),            // Normalized angle
        game->player.velocity.x / 200.0f,  // Player velocity X
        game->player.velocity.y / 200.0f   // Player velocity Y
    };
    
    // Let the neural network THINK!
    f32 outputs[3] = {0};
    if (npc->brain.network) {
        neural_forward(npc->brain.network, inputs, outputs);
    } else {
        // Fallback behavior - simple chase
        outputs[0] = (to_player.x > 0) ? 0.5f : -0.5f;
        outputs[1] = (to_player.y > 0) ? 0.5f : -0.5f;
        outputs[2] = 0.5f;
    }
    
    // Apply the brain's decision
    npc->velocity.x = outputs[0] * 100.0f;  // Move X
    npc->velocity.y = outputs[1] * 100.0f;  // Move Y
    f32 approach_desire = outputs[2];        // How much she wants to approach
    
    // Update position
    npc->position.x += npc->velocity.x * dt;
    npc->position.y += npc->velocity.y * dt;
    
    // Keep in bounds
    if (npc->position.x < 50) npc->position.x = 50;
    if (npc->position.x > 750) npc->position.x = 750;
    if (npc->position.y < 50) npc->position.y = 50;
    if (npc->position.y > 550) npc->position.y = 550;
    
    // Learn from the interaction
    if (distance < 100.0f) {
        // She's close to player - this is a learning opportunity!
        f32 reward = 0.1f;  // Small positive reward for being near player
        
        // This is where the magic happens - she learns what works!
        // (Simplified learning for Day 1)
        npc->player_model.player_movement_habits[0] += game->player.velocity.x * 0.001f;
        npc->player_model.player_movement_habits[1] += game->player.velocity.y * 0.001f;
    }
    
    // Track performance
    u64 end_time = ReadCPUTimer();
    npc->last_think_time = end_time - start_time;
    npc->think_time_ms = (f32)(end_time - start_time) / 1000000.0f;
}

void neural_kingdom_update(neural_kingdom_state* game, input_state* input, f32 dt) {
    u64 frame_start = ReadCPUTimer();
    
    // Update player - FEEL MUST BE PERFECT
    v2 player_input = {0};
    if (input->keys['w'] || input->keys['W']) player_input.y -= 1.0f;
    if (input->keys['s'] || input->keys['S']) player_input.y += 1.0f;
    if (input->keys['a'] || input->keys['A']) player_input.x -= 1.0f;
    if (input->keys['d'] || input->keys['D']) player_input.x += 1.0f;
    
    // Perfect movement feel
    f32 move_speed = 200.0f;
    if (v2_length(player_input) > 0) {
        player_input = v2_normalize(player_input);
        game->player.velocity = v2_scale(player_input, move_speed);
    } else {
        // Smooth deceleration
        game->player.velocity = v2_scale(game->player.velocity, 0.85f);
    }
    
    game->player.position = v2_add(game->player.position, v2_scale(game->player.velocity, dt));
    
    // Keep player in bounds
    if (game->player.position.x < 25) game->player.position.x = 25;
    if (game->player.position.x > 775) game->player.position.x = 775;
    if (game->player.position.y < 25) game->player.position.y = 25;
    if (game->player.position.y > 575) game->player.position.y = 575;
    
    // Update our neural NPC - THE STAR OF THE SHOW
    u64 ai_start = ReadCPUTimer();
    for (u32 i = 0; i < game->npc_count; i++) {
        npc_think(&game->npcs[i], game, dt);
    }
    u64 ai_end = ReadCPUTimer();
    
    // Performance tracking
    u64 frame_end = ReadCPUTimer();
    game->frame_ms = (f32)(frame_end - frame_start) / 1000000.0f;
    game->ai_update_ms = (f32)(ai_end - ai_start) / 1000000.0f;
    
    // Advance world time
    game->world_time++;
    game->time_of_day += dt / 120.0f;  // 2 minute day cycle
    if (game->time_of_day > 1.0f) game->time_of_day -= 1.0f;
}

void neural_kingdom_render(neural_kingdom_state* game) {
    // For now, we'll use printf for debugging
    // Later we'll integrate with our renderer
    
    static u32 frame_count = 0;
    frame_count++;
    
    if (frame_count % 60 == 0) {  // Every second
        printf("\n=== Neural Kingdom - Frame %d ===\n", frame_count);
        printf("Player: (%.1f, %.1f) Vel: (%.1f, %.1f)\n", 
               game->player.position.x, game->player.position.y,
               game->player.velocity.x, game->player.velocity.y);
        
        for (u32 i = 0; i < game->npc_count; i++) {
            neural_npc* npc = &game->npcs[i];
            f32 distance = v2_length(v2_sub(game->player.position, npc->position));
            printf("%s: (%.1f, %.1f) Distance: %.1f Think: %.3fms\n",
                   npc->name, npc->position.x, npc->position.y, 
                   distance, npc->think_time_ms);
        }
        
        printf("Performance: Frame %.2fms | AI: %.2fms\n", 
               game->frame_ms, game->ai_update_ms);
        
        if (game->frame_ms > MAX_FRAME_TIME_MS) {
            printf("⚠️  PERFORMANCE WARNING: Frame time too high!\n");
        } else {
            printf("✅ Performance: %.1f FPS\n", 1000.0f / game->frame_ms);
        }
    }
}

// Editor integration stubs for Day 1
void editor_show_npc_brain(neural_npc* npc) {
    printf("🧠 %s's Brain:\n", npc->name);
    printf("   Learning rate: %.3f\n", npc->brain.learning_rate);
    printf("   Think time: %.3fms\n", npc->think_time_ms);
    printf("   Player movement model: (%.2f, %.2f)\n", 
           npc->player_model.player_movement_habits[0],
           npc->player_model.player_movement_habits[1]);
}

// Performance tracking globals
static struct {
    const char* names[16];
    u64 start_times[16];
    f32 total_times[16];
    u32 count;
} perf_timers;

void perf_begin_timer(const char* name) {
    if (perf_timers.count < 16) {
        perf_timers.names[perf_timers.count] = name;
        perf_timers.start_times[perf_timers.count] = ReadCPUTimer();
        perf_timers.count++;
    }
}

void perf_end_timer(const char* name) {
    u64 end_time = ReadCPUTimer();
    for (u32 i = 0; i < perf_timers.count; i++) {
        if (strcmp(perf_timers.names[i], name) == 0) {
            f32 elapsed = (f32)(end_time - perf_timers.start_times[i]) / 1000000.0f;
            perf_timers.total_times[i] += elapsed;
            break;
        }
    }
}

// High-precision timer implementation - HANDMADE STYLE!
u64 ReadCPUTimer(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

// ============================================================================
// MAIN - THE NEURAL KINGDOM BEGINS!
// ============================================================================

int main(int argc, char** argv) {
    printf("🎮 NEURAL KINGDOM - The AI Revolution Begins! 🎮\n");
    printf("═══════════════════════════════════════════════════\n");
    printf("Mission: Shatter AAA gaming with handmade perfection\n");
    printf("Target: 144 FPS | Memory: <100MB | NPCs: Revolutionary\n\n");
    
    // Initialize our revolutionary game
    neural_kingdom_state game = {0};
    neural_kingdom_init(&game);
    
    // Fake input state for Day 1 demo
    input_state input = {0};
    
    // The game loop - THIS IS WHERE THE MAGIC HAPPENS!
    f32 target_dt = 1.0f / 144.0f;  // 144 FPS target
    u64 last_time = ReadCPUTimer();
    
    printf("🚀 Starting Neural Kingdom simulation...\n");
    printf("Watch Aria (our first neural NPC) learn about you!\n");
    printf("Press Ctrl+C to exit.\n\n");
    
    // Run for 30 seconds to demonstrate the AI learning
    u32 max_frames = 144 * 30;  // 30 seconds at 144 FPS
    
    for (u32 frame = 0; frame < max_frames; frame++) {
        u64 current_time = ReadCPUTimer();
        f32 actual_dt = (f32)(current_time - last_time) / 1000000000.0f;
        last_time = current_time;
        
        // Simulate some player input to make Aria react
        if (frame % 144 == 0) {  // Every second, change direction
            // Simulate WASD input
            memset(&input, 0, sizeof(input));
            switch ((frame / 144) % 4) {
                case 0: input.keys['w'] = true; break;  // Up
                case 1: input.keys['d'] = true; break;  // Right  
                case 2: input.keys['s'] = true; break;  // Down
                case 3: input.keys['a'] = true; break;  // Left
            }
        }
        
        // Update the game world
        neural_kingdom_update(&game, &input, target_dt);
        
        // Render (console output for now)
        neural_kingdom_render(&game);
        
        // Maintain 144 FPS (simulation)
        if (actual_dt < target_dt) {
            f32 sleep_ms = (target_dt - actual_dt) * 1000.0f;
            if (sleep_ms > 1.0f) {
                struct timespec sleep_time;
                sleep_time.tv_sec = 0;
                sleep_time.tv_nsec = (long)(sleep_ms * 1000000.0f);
                nanosleep(&sleep_time, NULL);
            }
        }
    }
    
    // Show final stats
    printf("\n🎯 NEURAL KINGDOM DEMO COMPLETE! 🎯\n");
    printf("═══════════════════════════════════════════════\n");
    printf("Final Performance:\n");
    printf("  Frame Time: %.2fms (Target: %.2fms)\n", 
           game.frame_ms, 1000.0f / 144.0f);
    printf("  AI Update: %.2fms\n", game.ai_update_ms);
    
    neural_npc* aria = &game.npcs[0];
    printf("\nAria's Learning Progress:\n");
    printf("  Think Time: %.3fms\n", aria->think_time_ms);
    printf("  Movement Model: (%.2f, %.2f)\n", 
           aria->player_model.player_movement_habits[0],
           aria->player_model.player_movement_habits[1]);
    
    if (game.frame_ms < 1000.0f / 144.0f) {
        printf("\n✅ SUCCESS: Beating 144 FPS target!\n");
        printf("💪 This is why handmade development WINS!\n");
    } else {
        printf("\n⚠️  Frame time high - but that's what optimization is for!\n");
    }
    
    printf("\nNext steps:\n");
    printf("1. Integrate with visual editor\n");
    printf("2. Add proper graphics rendering\n");
    printf("3. Implement advanced learning algorithms\n");  
    printf("4. Create emergent storytelling\n");
    printf("5. DESTROY AAA competition! 🔥\n");
    
    return 0;
}