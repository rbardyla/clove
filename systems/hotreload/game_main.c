#define GAME_DLL
#include "handmade_hotreload.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// PERFORMANCE: Game code - zero allocations after init
// MEMORY: All state stored in platform-provided memory

// ============================================================================
// GAME STATE
// ============================================================================

typedef struct {
    Vec2 position;
    Vec2 velocity;
    Color color;
    float size;
    float phase;      // For animation
    bool active;
} Entity;

typedef struct {
    Vec2 position;
    Vec2 target;
    float speed;
    Color color;
    
    // Neural state for NPCs
    float memory[16];     // Simple memory buffer
    float weights[16];    // Neural weights
    float activation;
    uint32_t think_timer;
} NPC;

typedef struct {
    GameStateHeader header;
    
    // Player
    Vec2 player_pos;
    Vec2 player_vel;
    Color player_color;
    
    // Entities
    #define MAX_ENTITIES 1024
    Entity* entities;
    uint32_t entity_count;
    
    // NPCs with memory
    #define MAX_NPCS 64
    NPC* npcs;
    uint32_t npc_count;
    
    // Particles for visual effects
    #define MAX_PARTICLES 4096
    struct Particle {
        Vec2 pos;
        Vec2 vel;
        Color color;
        float life;
    }* particles;
    uint32_t particle_count;
    
    // Wave animation state
    float wave_phase;
    float pulse_phase;
    
    // Debug visualization
    bool show_debug;
    uint32_t selected_npc;
    
    // Performance tracking
    uint64_t update_cycles;
    uint64_t render_cycles;
} GameState;

// ============================================================================
// NEURAL PROCESSING
// ============================================================================

static float sigmoid(float x) {
    // PERFORMANCE: Fast sigmoid approximation
    float abs_x = x < 0 ? -x : x;
    float y = 1.0f / (1.0f + abs_x);
    return x >= 0 ? y : 1.0f - y;
}

static void npc_think(NPC* npc, GameState* state, float dt) {
    // PERFORMANCE: Simple neural update - could be SIMD vectorized
    
    // Input layer: distance to player, current position
    float inputs[4] = {
        (state->player_pos.x - npc->position.x) * 0.001f,
        (state->player_pos.y - npc->position.y) * 0.001f,
        npc->position.x * 0.001f,
        npc->position.y * 0.001f
    };
    
    // Process through memory
    float activation = 0.0f;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            npc->memory[i*4 + j] += inputs[i] * npc->weights[i*4 + j] * dt;
            activation += npc->memory[i*4 + j];
        }
    }
    
    npc->activation = sigmoid(activation);
    
    // Decay memory slowly
    for (int i = 0; i < 16; i++) {
        npc->memory[i] *= 0.99f;
    }
    
    // Update behavior based on activation
    float angle = npc->activation * 6.28318f;  // 2*PI
    npc->target.x = npc->position.x + cosf(angle) * 100.0f;
    npc->target.y = npc->position.y + sinf(angle) * 100.0f;
    
    // Modify color based on neural state
    npc->color.r = 0.3f + npc->activation * 0.7f;
    npc->color.b = 1.0f - npc->activation * 0.5f;
}

// ============================================================================
// GAME EXPORTS
// ============================================================================

GAME_EXPORT void GameInitialize(GameMemory* memory, PlatformAPI* platform) {
    // MEMORY: Setup game state in permanent storage
    GameState* state = (GameState*)memory->permanent_storage;
    
    // Check if this is first init or reload
    if (state->header.magic != 0xDEADBEEF) {
        // First time initialization
        memset(state, 0, sizeof(GameState));
        
        state->header.magic = 0xDEADBEEF;
        state->header.version = 1;
        
        // Start allocation after GameState
        memory->permanent_used = sizeof(GameState);
        
        // Allocate arrays from permanent memory
        state->entities = PUSH_ARRAY(memory, Entity, MAX_ENTITIES, true);
        state->npcs = PUSH_ARRAY(memory, NPC, MAX_NPCS, true);
        state->particles = PUSH_ARRAY(memory, struct Particle, MAX_PARTICLES, true);
        
        // Check allocations
        if (!state->entities || !state->npcs || !state->particles) {
            platform->debug_print("[GAME] ERROR: Failed to allocate memory!\n");
            return;
        }
        
        // Zero the arrays
        memset(state->entities, 0, sizeof(Entity) * MAX_ENTITIES);
        memset(state->npcs, 0, sizeof(NPC) * MAX_NPCS);
        memset(state->particles, 0, sizeof(struct Particle) * MAX_PARTICLES);
        
        // Initialize player
        state->player_pos.x = 640;
        state->player_pos.y = 360;
        state->player_color = (Color){0.8f, 0.2f, 0.9f, 1.0f};  // Changed to purple!
        
        // Create some initial entities
        for (int i = 0; i < 10; i++) {
            Entity* e = &state->entities[state->entity_count++];
            e->position.x = 100 + i * 100;
            e->position.y = 200;
            e->velocity.x = (i - 5) * 10;
            e->velocity.y = 0;
            e->size = 20 + i * 2;
            e->color = (Color){
                0.5f + (i * 0.05f),
                0.3f,
                0.8f - (i * 0.05f),
                1.0f
            };
            e->phase = i * 0.5f;
            e->active = true;
        }
        
        // Create NPCs with random neural weights
        for (int i = 0; i < 8; i++) {
            NPC* npc = &state->npcs[state->npc_count++];
            npc->position.x = 200 + i * 120;
            npc->position.y = 400;
            npc->target = npc->position;
            npc->speed = 50.0f + i * 10.0f;
            npc->color = (Color){0.6f, 0.4f, 0.8f, 1.0f};
            
            // Initialize neural weights (normally would be random)
            for (int j = 0; j < 16; j++) {
                // Simple pattern for reproducibility
                npc->weights[j] = sinf(i * j * 0.1f) * 0.5f;
            }
        }
        
        platform->debug_print("[GAME] Initialized with %u entities, %u NPCs\n", 
                            state->entity_count, state->npc_count);
    } else {
        platform->debug_print("[GAME] Reloaded - state preserved\n");
    }
}

GAME_EXPORT void GamePrepareReload(GameMemory* memory) {
    // MEMORY: Prepare state for reload
    GameState* state = (GameState*)memory->permanent_storage;
    
    if (state) {
        state->header.reload_count++;
        // Could serialize complex state here if needed
    }
}

GAME_EXPORT void GameCompleteReload(GameMemory* memory) {
    // MEMORY: Restore state after reload
    GameState* state = (GameState*)memory->permanent_storage;
    
    if (state) {
        // Reconnect pointers if needed
        // For now, our arrays are at fixed offsets so they're fine
        
        // Add visual feedback for reload
        state->pulse_phase = 3.14159f;  // Start pulse animation
    }
}

GAME_EXPORT void GameUpdateAndRender(GameMemory* memory, GameInput* input, RenderCommands* commands) {
    // PERFORMANCE: Main update loop - must run at 60 FPS
    GameState* state = (GameState*)memory->permanent_storage;
    if (!state) return;
    
    uint64_t update_start = read_cpu_timer();
    
    // Update wave animation
    state->wave_phase += input->dt * 2.0f;
    state->pulse_phase *= 0.95f;  // Decay pulse
    
    // Player movement (WASD) - using lower 6 bits of ASCII
    const float player_speed = 300.0f;
    if (input->keys_down & (1ULL << ('w' & 63))) state->player_vel.y -= player_speed * input->dt;
    if (input->keys_down & (1ULL << ('s' & 63))) state->player_vel.y += player_speed * input->dt;
    if (input->keys_down & (1ULL << ('a' & 63))) state->player_vel.x -= player_speed * input->dt;
    if (input->keys_down & (1ULL << ('d' & 63))) state->player_vel.x += player_speed * input->dt;
    
    // Apply friction
    state->player_vel.x *= 0.9f;
    state->player_vel.y *= 0.9f;
    
    // Update position
    state->player_pos.x += state->player_vel.x * input->dt;
    state->player_pos.y += state->player_vel.y * input->dt;
    
    // Keep player on screen
    if (state->player_pos.x < 20) state->player_pos.x = 20;
    if (state->player_pos.x > 1260) state->player_pos.x = 1260;
    if (state->player_pos.y < 20) state->player_pos.y = 20;
    if (state->player_pos.y > 700) state->player_pos.y = 700;
    
    // Toggle debug with Tab
    if (input->keys_pressed & (1ULL << 9)) {  // Tab key
        state->show_debug = !state->show_debug;
    }
    
    // Update entities
    for (uint32_t i = 0; i < state->entity_count; i++) {
        Entity* e = &state->entities[i];
        if (!e->active) continue;
        
        // Physics update
        e->position.x += e->velocity.x * input->dt;
        e->position.y += e->velocity.y * input->dt;
        
        // Bounce off walls
        if (e->position.x < 0 || e->position.x > 1280) {
            e->velocity.x = -e->velocity.x;
            e->position.x = e->position.x < 0 ? 0 : 1280;
        }
        if (e->position.y < 0 || e->position.y > 720) {
            e->velocity.y = -e->velocity.y;
            e->position.y = e->position.y < 0 ? 0 : 720;
        }
        
        // Animate
        e->phase += input->dt * 3.0f;
        
        // Gravity towards player (weak)
        float dx = state->player_pos.x - e->position.x;
        float dy = state->player_pos.y - e->position.y;
        float dist_sq = dx*dx + dy*dy;
        if (dist_sq > 100) {
            float force = 5000.0f / dist_sq;
            e->velocity.x += dx * force * input->dt;
            e->velocity.y += dy * force * input->dt;
        }
    }
    
    // Update NPCs with neural processing
    for (uint32_t i = 0; i < state->npc_count; i++) {
        NPC* npc = &state->npcs[i];
        
        // Think every few frames
        if (++npc->think_timer > 10) {
            npc_think(npc, state, input->dt);
            npc->think_timer = 0;
        }
        
        // Move towards target
        float dx = npc->target.x - npc->position.x;
        float dy = npc->target.y - npc->position.y;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist > 5.0f) {
            dx /= dist;
            dy /= dist;
            npc->position.x += dx * npc->speed * input->dt;
            npc->position.y += dy * npc->speed * input->dt;
        }
    }
    
    // Spawn particles at player position when moving
    float speed_sq = state->player_vel.x * state->player_vel.x + 
                    state->player_vel.y * state->player_vel.y;
    if (speed_sq > 100 && state->particle_count < MAX_PARTICLES - 10) {
        for (int i = 0; i < 3; i++) {
            struct Particle* p = &state->particles[state->particle_count++];
            p->pos = state->player_pos;
            p->vel.x = -state->player_vel.x * 0.5f + (i - 1) * 20;
            p->vel.y = -state->player_vel.y * 0.5f + (i - 1) * 20;
            p->color = state->player_color;
            p->color.a = 0.7f;
            p->life = 1.0f;
        }
    }
    
    // Update particles
    for (uint32_t i = 0; i < state->particle_count; i++) {
        struct Particle* p = &state->particles[i];
        p->pos.x += p->vel.x * input->dt;
        p->pos.y += p->vel.y * input->dt;
        p->vel.x *= 0.98f;
        p->vel.y *= 0.98f;
        p->life -= input->dt * 2.0f;
        p->color.a = p->life * 0.7f;
    }
    
    // Remove dead particles
    for (uint32_t i = 0; i < state->particle_count;) {
        if (state->particles[i].life <= 0) {
            state->particles[i] = state->particles[--state->particle_count];
        } else {
            i++;
        }
    }
    
    state->update_cycles = read_cpu_timer() - update_start;
    
    // ========================================================================
    // RENDERING
    // ========================================================================
    
    uint64_t render_start = read_cpu_timer();
    
    // Draw wave background effect
    for (int i = 0; i < 20; i++) {
        float wave_offset = sinf(state->wave_phase + i * 0.3f) * 30;
        float pulse = sinf(state->pulse_phase) * 0.2f;
        
        commands->positions[commands->command_count] = (Vec2){0, i * 36 + wave_offset};
        commands->sizes[commands->command_count] = (Vec2){1280, 36};
        commands->colors[commands->command_count] = (Color){
            0.05f + pulse,
            0.05f + i * 0.005f,
            0.08f + pulse,
            1.0f
        };
        commands->command_count++;
    }
    
    // Draw particles
    for (uint32_t i = 0; i < state->particle_count; i++) {
        commands->positions[commands->command_count] = state->particles[i].pos;
        commands->sizes[commands->command_count] = (Vec2){4, 4};
        commands->colors[commands->command_count] = state->particles[i].color;
        commands->command_count++;
    }
    
    // Draw entities with animated size
    for (uint32_t i = 0; i < state->entity_count; i++) {
        Entity* e = &state->entities[i];
        if (!e->active) continue;
        
        float animated_size = e->size * (1.0f + sinf(e->phase) * 0.2f);
        
        commands->positions[commands->command_count] = (Vec2){
            e->position.x - animated_size/2,
            e->position.y - animated_size/2
        };
        commands->sizes[commands->command_count] = (Vec2){animated_size, animated_size};
        commands->colors[commands->command_count] = e->color;
        commands->command_count++;
    }
    
    // Draw NPCs
    for (uint32_t i = 0; i < state->npc_count; i++) {
        NPC* npc = &state->npcs[i];
        
        // Body
        commands->positions[commands->command_count] = (Vec2){
            npc->position.x - 15,
            npc->position.y - 15
        };
        commands->sizes[commands->command_count] = (Vec2){30, 30};
        commands->colors[commands->command_count] = npc->color;
        commands->command_count++;
        
        // Neural activation indicator
        if (state->show_debug) {
            float indicator_size = 10 + npc->activation * 20;
            commands->positions[commands->command_count] = (Vec2){
                npc->position.x - indicator_size/2,
                npc->position.y - 40
            };
            commands->sizes[commands->command_count] = (Vec2){indicator_size, 5};
            commands->colors[commands->command_count] = (Color){
                npc->activation,
                1.0f - npc->activation,
                0.2f,
                0.8f
            };
            commands->command_count++;
        }
    }
    
    // Draw player with glow effect
    float glow_size = 40 + sinf(state->wave_phase * 2) * 5;
    commands->positions[commands->command_count] = (Vec2){
        state->player_pos.x - glow_size/2,
        state->player_pos.y - glow_size/2
    };
    commands->sizes[commands->command_count] = (Vec2){glow_size, glow_size};
    commands->colors[commands->command_count] = (Color){
        state->player_color.r * 0.3f,
        state->player_color.g * 0.3f,
        state->player_color.b * 0.3f,
        0.3f
    };
    commands->command_count++;
    
    // Player body
    commands->positions[commands->command_count] = (Vec2){
        state->player_pos.x - 20,
        state->player_pos.y - 20
    };
    commands->sizes[commands->command_count] = (Vec2){40, 40};
    commands->colors[commands->command_count] = state->player_color;
    commands->command_count++;
    
    // Debug info overlay
    if (state->show_debug) {
        // Draw memory visualization
        float mem_percent = (float)memory->permanent_used / memory->permanent_size;
        commands->positions[commands->command_count] = (Vec2){10, 10};
        commands->sizes[commands->command_count] = (Vec2){200 * mem_percent, 20};
        commands->colors[commands->command_count] = (Color){0.8f, 0.2f, 0.2f, 0.7f};
        commands->command_count++;
        
        // Frame counter
        commands->positions[commands->command_count] = (Vec2){10, 40};
        commands->sizes[commands->command_count] = (Vec2){
            5 + (state->header.frame_count % 100) * 2, 
            10
        };
        commands->colors[commands->command_count] = (Color){0.2f, 0.8f, 0.2f, 0.7f};
        commands->command_count++;
        
        // Reload indicator
        if (state->header.reload_count > 0) {
            commands->positions[commands->command_count] = (Vec2){10, 60};
            commands->sizes[commands->command_count] = (Vec2){
                10 * state->header.reload_count,
                10
            };
            commands->colors[commands->command_count] = (Color){0.8f, 0.8f, 0.2f, 0.7f};
            commands->command_count++;
        }
    }
    
    state->render_cycles = read_cpu_timer() - render_start;
    state->header.frame_count++;
}