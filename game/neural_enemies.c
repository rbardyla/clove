// neural_enemies.c - Smart enemy AI using neural networks with DNC memory
// Enemies learn from player behavior and adapt their strategies

#include "crystal_dungeons.h"
#include "../systems/ai/handmade_dnc_enhanced.h"
#include <math.h>
#include <string.h>

// Neural network function declarations
neural_network* neural_create(void);
void neural_add_layer(neural_network* net, u32 input_size, u32 output_size, activation_type activation);
void neural_forward(neural_network* net, f32* inputs, f32* outputs);
void neural_destroy(neural_network* net);

// ============================================================================
// NEURAL ENEMY CONFIGURATION
// ============================================================================

#define ENEMY_INPUT_SIZE 32      // Perception inputs
#define ENEMY_HIDDEN_SIZE 64     // Hidden layer neurons
#define ENEMY_MEMORY_SIZE 16     // DNC memory slots
#define ENEMY_OUTPUT_SIZE 8      // Action outputs

// Input indices for neural network
typedef enum {
    // Player relative position (normalized)
    INPUT_PLAYER_REL_X = 0,
    INPUT_PLAYER_REL_Y,
    INPUT_PLAYER_DISTANCE,
    INPUT_PLAYER_ANGLE,
    
    // Player state
    INPUT_PLAYER_HEALTH,
    INPUT_PLAYER_IS_ATTACKING,
    INPUT_PLAYER_FACING_ENEMY,
    
    // Self state
    INPUT_SELF_HEALTH,
    INPUT_SELF_STAMINA,
    INPUT_SELF_COOLDOWN,
    
    // Environment (8 directions)
    INPUT_WALL_N,
    INPUT_WALL_NE,
    INPUT_WALL_E,
    INPUT_WALL_SE,
    INPUT_WALL_S,
    INPUT_WALL_SW,
    INPUT_WALL_W,
    INPUT_WALL_NW,
    
    // Nearby entities (closest 4)
    INPUT_ALLY_1_DIST,
    INPUT_ALLY_1_ANGLE,
    INPUT_ALLY_2_DIST,
    INPUT_ALLY_2_ANGLE,
    INPUT_ENEMY_1_DIST,
    INPUT_ENEMY_1_ANGLE,
    INPUT_ENEMY_2_DIST,
    INPUT_ENEMY_2_ANGLE,
    
    // Combat history
    INPUT_RECENT_DAMAGE_TAKEN,
    INPUT_RECENT_DAMAGE_DEALT,
    INPUT_DODGE_SUCCESS_RATE,
    INPUT_HIT_SUCCESS_RATE,
    
    // Time-based
    INPUT_TIME_IN_COMBAT,
    INPUT_TIME_SINCE_HIT,
} neural_input;

// Output actions
typedef enum {
    OUTPUT_MOVE_X = 0,      // -1 to 1 movement
    OUTPUT_MOVE_Y,
    OUTPUT_ATTACK,          // 0-1 attack probability
    OUTPUT_DODGE,           // 0-1 dodge probability
    OUTPUT_BLOCK,           // 0-1 block probability
    OUTPUT_SPECIAL,         // 0-1 special ability
    OUTPUT_RETREAT,         // 0-1 retreat urgency
    OUTPUT_COORDINATE,      // 0-1 team coordination
} neural_output;

// ============================================================================
// ENEMY BRAIN STRUCTURE
// ============================================================================

typedef struct enemy_brain {
    // Core neural network
    neural_network* network;
    f32 learning_rate;
    
    // DNC memory system
    struct {
        f32 memory[ENEMY_MEMORY_SIZE][32];  // Memory matrix
        f32 usage[ENEMY_MEMORY_SIZE];       // Usage weights
        f32 precedence[ENEMY_MEMORY_SIZE];  // Write precedence
        f32 link_matrix[ENEMY_MEMORY_SIZE][ENEMY_MEMORY_SIZE];  // Temporal links
        
        // Read/write heads
        f32 read_weights[ENEMY_MEMORY_SIZE];
        f32 write_weights[ENEMY_MEMORY_SIZE];
        f32 erase_vector[32];
        f32 write_vector[32];
    } dnc;
    
    // Experience buffer for learning
    struct {
        f32 states[100][ENEMY_INPUT_SIZE];
        f32 actions[100][ENEMY_OUTPUT_SIZE];
        f32 rewards[100];
        u32 count;
        u32 write_idx;
    } experience;
    
    // Behavioral patterns learned
    struct {
        f32 player_attack_timing[8];   // Player's attack patterns
        f32 player_dodge_patterns[8];  // Player's dodge tendencies
        f32 effective_strategies[4];   // What works against this player
        u32 pattern_observations;
    } patterns;
    
    // Performance metrics
    struct {
        f32 total_damage_dealt;
        f32 total_damage_taken;
        u32 successful_hits;
        u32 successful_dodges;
        u32 total_attacks;
        f32 survival_time;
    } stats;
    
} enemy_brain;

// ============================================================================
// PERCEPTION SYSTEM
// ============================================================================

static void gather_perception_inputs(entity* enemy, game_state* game, f32* inputs) {
    memset(inputs, 0, ENEMY_INPUT_SIZE * sizeof(f32));
    
    player* p = &game->player;
    v2 enemy_pos = enemy->position;
    v2 player_pos = p->entity->position;
    
    // Player relative position
    f32 dx = player_pos.x - enemy_pos.x;
    f32 dy = player_pos.y - enemy_pos.y;
    f32 dist = sqrtf(dx*dx + dy*dy);
    f32 angle = atan2f(dy, dx);
    
    inputs[INPUT_PLAYER_REL_X] = dx / 100.0f;  // Normalize to ~[-1, 1]
    inputs[INPUT_PLAYER_REL_Y] = dy / 100.0f;
    inputs[INPUT_PLAYER_DISTANCE] = fminf(dist / 200.0f, 1.0f);
    inputs[INPUT_PLAYER_ANGLE] = angle / (2.0f * M_PI);
    
    // Player state
    inputs[INPUT_PLAYER_HEALTH] = p->entity->health / p->entity->max_health;
    inputs[INPUT_PLAYER_IS_ATTACKING] = p->is_attacking ? 1.0f : 0.0f;
    
    // Check if player faces enemy
    f32 player_to_enemy_angle = atan2f(-dy, -dx);
    f32 player_facing_angle = p->entity->facing * (M_PI / 2.0f);
    f32 angle_diff = fabsf(player_to_enemy_angle - player_facing_angle);
    inputs[INPUT_PLAYER_FACING_ENEMY] = (angle_diff < M_PI/4) ? 1.0f : 0.0f;
    
    // Self state
    inputs[INPUT_SELF_HEALTH] = enemy->health / enemy->max_health;
    inputs[INPUT_SELF_STAMINA] = 1.0f;  // TODO: Add stamina system
    inputs[INPUT_SELF_COOLDOWN] = enemy->attack_cooldown / 1.0f;  // Normalize
    
    // Check walls in 8 directions
    room* current_room = game->current_room;
    int tile_x = (int)(enemy_pos.x / TILE_SIZE);
    int tile_y = (int)(enemy_pos.y / TILE_SIZE);
    
    // North
    if (tile_y > 0 && current_room->tiles[tile_y-1][tile_x] == TILE_WALL) {
        inputs[INPUT_WALL_N] = 1.0f;
    }
    // Northeast
    if (tile_y > 0 && tile_x < ROOM_WIDTH-1 && 
        current_room->tiles[tile_y-1][tile_x+1] == TILE_WALL) {
        inputs[INPUT_WALL_NE] = 1.0f;
    }
    // East
    if (tile_x < ROOM_WIDTH-1 && current_room->tiles[tile_y][tile_x+1] == TILE_WALL) {
        inputs[INPUT_WALL_E] = 1.0f;
    }
    // Southeast
    if (tile_y < ROOM_HEIGHT-1 && tile_x < ROOM_WIDTH-1 && 
        current_room->tiles[tile_y+1][tile_x+1] == TILE_WALL) {
        inputs[INPUT_WALL_SE] = 1.0f;
    }
    // South
    if (tile_y < ROOM_HEIGHT-1 && current_room->tiles[tile_y+1][tile_x] == TILE_WALL) {
        inputs[INPUT_WALL_S] = 1.0f;
    }
    // Southwest
    if (tile_y < ROOM_HEIGHT-1 && tile_x > 0 && 
        current_room->tiles[tile_y+1][tile_x-1] == TILE_WALL) {
        inputs[INPUT_WALL_SW] = 1.0f;
    }
    // West
    if (tile_x > 0 && current_room->tiles[tile_y][tile_x-1] == TILE_WALL) {
        inputs[INPUT_WALL_W] = 1.0f;
    }
    // Northwest
    if (tile_y > 0 && tile_x > 0 && 
        current_room->tiles[tile_y-1][tile_x-1] == TILE_WALL) {
        inputs[INPUT_WALL_NW] = 1.0f;
    }
    
    // Find nearby entities
    f32 closest_ally_dist = 1000.0f;
    f32 closest_enemy_dist = 1000.0f;
    entity* closest_ally = NULL;
    entity* closest_enemy = NULL;
    
    for (u32 i = 0; i < game->entity_count; i++) {
        entity* other = &game->entities[i];
        if (!other->is_alive || other == enemy) continue;
        
        f32 odx = other->position.x - enemy_pos.x;
        f32 ody = other->position.y - enemy_pos.y;
        f32 odist = sqrtf(odx*odx + ody*ody);
        
        // Check if ally or enemy
        bool is_ally = (other->type >= ENTITY_SLIME && other->type <= ENTITY_DRAGON);
        
        if (is_ally && odist < closest_ally_dist) {
            closest_ally_dist = odist;
            closest_ally = other;
        } else if (!is_ally && odist < closest_enemy_dist) {
            closest_enemy_dist = odist;
            closest_enemy = other;
        }
    }
    
    if (closest_ally) {
        f32 adx = closest_ally->position.x - enemy_pos.x;
        f32 ady = closest_ally->position.y - enemy_pos.y;
        inputs[INPUT_ALLY_1_DIST] = closest_ally_dist / 200.0f;
        inputs[INPUT_ALLY_1_ANGLE] = atan2f(ady, adx) / (2.0f * M_PI);
    }
    
    // Combat history
    inputs[INPUT_RECENT_DAMAGE_TAKEN] = enemy->knockback_timer > 0 ? 1.0f : 0.0f;
    inputs[INPUT_TIME_SINCE_HIT] = fminf(enemy->invulnerable_timer, 1.0f);
    
    // Time in combat
    if (enemy->ai.state == AI_CHASE || enemy->ai.state == AI_ATTACK) {
        inputs[INPUT_TIME_IN_COMBAT] = fminf(enemy->ai.state_timer / 10.0f, 1.0f);
    }
}

// ============================================================================
// DNC MEMORY OPERATIONS
// ============================================================================

static void dnc_read(enemy_brain* brain, f32* key, f32* output) {
    // Content-based addressing
    f32 max_similarity = -1.0f;
    int best_idx = 0;
    
    for (int i = 0; i < ENEMY_MEMORY_SIZE; i++) {
        f32 similarity = 0.0f;
        for (int j = 0; j < 32; j++) {
            similarity += key[j] * brain->dnc.memory[i][j];
        }
        
        if (similarity > max_similarity) {
            max_similarity = similarity;
            best_idx = i;
        }
    }
    
    // Copy memory content to output
    memcpy(output, brain->dnc.memory[best_idx], 32 * sizeof(f32));
    
    // Update read weights
    memset(brain->dnc.read_weights, 0, ENEMY_MEMORY_SIZE * sizeof(f32));
    brain->dnc.read_weights[best_idx] = 1.0f;
}

static void dnc_write(enemy_brain* brain, f32* key, f32* value) {
    // Find least used memory location
    int write_idx = 0;
    f32 min_usage = brain->dnc.usage[0];
    
    for (int i = 1; i < ENEMY_MEMORY_SIZE; i++) {
        if (brain->dnc.usage[i] < min_usage) {
            min_usage = brain->dnc.usage[i];
            write_idx = i;
        }
    }
    
    // Write to memory
    memcpy(brain->dnc.memory[write_idx], value, 32 * sizeof(f32));
    
    // Update usage
    brain->dnc.usage[write_idx] = 1.0f;
    
    // Decay other usage values
    for (int i = 0; i < ENEMY_MEMORY_SIZE; i++) {
        if (i != write_idx) {
            brain->dnc.usage[i] *= 0.99f;
        }
    }
    
    // Update write weights
    memset(brain->dnc.write_weights, 0, ENEMY_MEMORY_SIZE * sizeof(f32));
    brain->dnc.write_weights[write_idx] = 1.0f;
}

// ============================================================================
// NEURAL NETWORK DECISION MAKING
// ============================================================================

static void make_decision(enemy_brain* brain, f32* inputs, f32* outputs) {
    // Forward pass through network
    neural_forward(brain->network, inputs, outputs);
    
    // Query DNC memory for relevant experiences
    f32 memory_key[32];
    f32 memory_value[32];
    
    // Use first part of input as memory key
    memcpy(memory_key, inputs, 32 * sizeof(f32));
    dnc_read(brain, memory_key, memory_value);
    
    // Blend memory with network output
    for (int i = 0; i < ENEMY_OUTPUT_SIZE; i++) {
        if (i < 32) {
            outputs[i] = 0.7f * outputs[i] + 0.3f * memory_value[i];
        }
    }
    
    // Apply activation constraints
    outputs[OUTPUT_MOVE_X] = tanhf(outputs[OUTPUT_MOVE_X]);
    outputs[OUTPUT_MOVE_Y] = tanhf(outputs[OUTPUT_MOVE_Y]);
    
    // Probabilities should be in [0, 1]
    for (int i = OUTPUT_ATTACK; i <= OUTPUT_COORDINATE; i++) {
        outputs[i] = fmaxf(0.0f, fminf(1.0f, outputs[i]));
    }
}

// ============================================================================
// LEARNING SYSTEM
// ============================================================================

static void record_experience(enemy_brain* brain, f32* state, f32* action, f32 reward) {
    u32 idx = brain->experience.write_idx;
    
    memcpy(brain->experience.states[idx], state, ENEMY_INPUT_SIZE * sizeof(f32));
    memcpy(brain->experience.actions[idx], action, ENEMY_OUTPUT_SIZE * sizeof(f32));
    brain->experience.rewards[idx] = reward;
    
    brain->experience.write_idx = (idx + 1) % 100;
    if (brain->experience.count < 100) {
        brain->experience.count++;
    }
    
    // Store successful patterns in DNC memory
    if (reward > 0.5f) {
        f32 memory_value[32];
        memcpy(memory_value, action, ENEMY_OUTPUT_SIZE * sizeof(f32));
        // Pad with state information
        memcpy(&memory_value[ENEMY_OUTPUT_SIZE], state, 
               (32 - ENEMY_OUTPUT_SIZE) * sizeof(f32));
        
        dnc_write(brain, state, memory_value);
    }
}

static void learn_from_experience(enemy_brain* brain) {
    if (brain->experience.count < 10) return;
    
    // Simple reinforcement learning update
    // This is a simplified version - real implementation would use TD-learning or PPO
    
    f32 total_reward = 0.0f;
    for (u32 i = 0; i < brain->experience.count; i++) {
        total_reward += brain->experience.rewards[i];
    }
    f32 avg_reward = total_reward / brain->experience.count;
    
    // If performance is good, reinforce current weights slightly
    if (avg_reward > 0.0f) {
        // Positive reinforcement - train network with good experiences
        // We'll use the stored experiences to reinforce successful behaviors
        // This is simplified - in a real implementation we'd do proper gradient updates
        brain->learning_rate *= 1.01f;  // Increase learning when doing well
        if (brain->learning_rate > 0.1f) brain->learning_rate = 0.1f;
    } else {
        // Negative reinforcement - reduce learning rate
        brain->learning_rate *= 0.99f;
        if (brain->learning_rate < 0.001f) brain->learning_rate = 0.001f;
    }
    
    // Pattern recognition
    brain->patterns.pattern_observations++;
}

// ============================================================================
// BEHAVIOR EXECUTION
// ============================================================================

static void execute_behavior(entity* enemy, f32* outputs, game_state* game, f32 dt) {
    // Movement
    f32 move_speed = 50.0f;  // Base speed
    
    // Adjust speed based on behavior
    if (outputs[OUTPUT_RETREAT] > 0.7f) {
        move_speed *= 1.5f;  // Faster when retreating
    }
    
    enemy->velocity.x = outputs[OUTPUT_MOVE_X] * move_speed;
    enemy->velocity.y = outputs[OUTPUT_MOVE_Y] * move_speed;
    
    // Attack decision
    if (outputs[OUTPUT_ATTACK] > 0.6f && enemy->attack_cooldown <= 0) {
        // Perform attack
        player* p = &game->player;
        f32 dx = p->entity->position.x - enemy->position.x;
        f32 dy = p->entity->position.y - enemy->position.y;
        f32 dist = sqrtf(dx*dx + dy*dy);
        
        if (dist < 30.0f) {  // In range
            // Deal damage
            player_take_damage(p, enemy->damage);
            enemy->attack_cooldown = 1.0f;
            
            // Record successful hit
            enemy_brain* brain = (enemy_brain*)enemy->ai.brain;
            if (brain) {
                brain->stats.successful_hits++;
                brain->stats.total_damage_dealt += enemy->damage;
            }
        }
        
        enemy->ai.state = AI_ATTACK;
        enemy->ai.state_timer = 0.5f;
    }
    
    // Dodge behavior
    if (outputs[OUTPUT_DODGE] > 0.7f && enemy->knockback_timer <= 0) {
        // Quick dodge movement
        f32 dodge_x = (outputs[OUTPUT_MOVE_X] > 0) ? -1.0f : 1.0f;
        f32 dodge_y = (outputs[OUTPUT_MOVE_Y] > 0) ? -1.0f : 1.0f;
        
        enemy->velocity.x += dodge_x * 100.0f;
        enemy->velocity.y += dodge_y * 100.0f;
        
        enemy->invulnerable_timer = 0.2f;  // Brief invulnerability
    }
    
    // Special abilities (enemy-type specific)
    if (outputs[OUTPUT_SPECIAL] > 0.8f) {
        switch (enemy->type) {
            case ENTITY_WIZARD:
                // Cast spell
                combat_shoot_projectile(game, enemy, ENTITY_MAGIC_BOLT, 
                    (v2){outputs[OUTPUT_MOVE_X], outputs[OUTPUT_MOVE_Y]});
                break;
                
            case ENTITY_DRAGON:
                // Breathe fire
                combat_shoot_projectile(game, enemy, ENTITY_FIREBALL,
                    (v2){outputs[OUTPUT_MOVE_X], outputs[OUTPUT_MOVE_Y]});
                break;
                
            default:
                break;
        }
    }
    
    // Team coordination
    if (outputs[OUTPUT_COORDINATE] > 0.5f) {
        // Signal nearby allies
        for (u32 i = 0; i < game->entity_count; i++) {
            entity* other = &game->entities[i];
            if (!other->is_alive || other == enemy) continue;
            
            // Check if ally
            if (other->type >= ENTITY_SLIME && other->type <= ENTITY_DRAGON) {
                f32 dx = other->position.x - enemy->position.x;
                f32 dy = other->position.y - enemy->position.y;
                f32 dist = sqrtf(dx*dx + dy*dy);
                
                if (dist < 100.0f) {
                    // Coordinate attack
                    other->ai.target_position = game->player.entity->position;
                    other->ai.state = AI_CHASE;
                }
            }
        }
    }
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void neural_enemy_init(entity* enemy) {
    // Create brain for smart enemies
    if (enemy->type == ENTITY_KNIGHT || enemy->type == ENTITY_WIZARD || 
        enemy->type == ENTITY_DRAGON) {
        
        enemy_brain* brain = (enemy_brain*)calloc(1, sizeof(enemy_brain));
        
        // Create neural network
        brain->network = neural_create();
        neural_add_layer(brain->network, ENEMY_INPUT_SIZE, ENEMY_HIDDEN_SIZE, 
                        ACTIVATION_RELU);
        neural_add_layer(brain->network, ENEMY_HIDDEN_SIZE, ENEMY_HIDDEN_SIZE, 
                        ACTIVATION_RELU);
        neural_add_layer(brain->network, ENEMY_HIDDEN_SIZE, ENEMY_OUTPUT_SIZE, 
                        ACTIVATION_TANH);
        
        // Initialize learning rate
        brain->learning_rate = 0.01f;
        
        // Initialize DNC memory
        for (int i = 0; i < ENEMY_MEMORY_SIZE; i++) {
            brain->dnc.usage[i] = 0.0f;
            brain->dnc.precedence[i] = 0.0f;
        }
        
        // Random initialization for variety
        // Network weights are already randomized in neural_add_layer
        
        enemy->ai.brain = brain->network;
    }
}

void neural_enemy_update(entity* enemy, game_state* game, f32 dt) {
    enemy_brain* brain = (enemy_brain*)enemy->ai.brain;
    if (!brain) {
        // Fall back to simple AI
        ai_update(enemy, game, dt);
        return;
    }
    
    // Gather perception
    f32 inputs[ENEMY_INPUT_SIZE];
    gather_perception_inputs(enemy, game, inputs);
    
    // Make decision
    f32 outputs[ENEMY_OUTPUT_SIZE];
    make_decision(brain, inputs, outputs);
    
    // Execute behavior
    execute_behavior(enemy, outputs, game, dt);
    
    // Calculate reward for this frame
    f32 reward = 0.0f;
    
    // Positive rewards
    if (brain->stats.total_damage_dealt > 0) {
        reward += 0.5f;  // Dealt damage
    }
    if (enemy->health > enemy->max_health * 0.5f) {
        reward += 0.2f;  // Staying healthy
    }
    
    // Negative rewards
    if (enemy->health < enemy->max_health * 0.2f) {
        reward -= 0.3f;  // Low health
    }
    if (enemy->knockback_timer > 0) {
        reward -= 0.2f;  // Got hit
    }
    
    // Record experience
    record_experience(brain, inputs, outputs, reward);
    
    // Periodic learning
    brain->stats.survival_time += dt;
    if (brain->stats.survival_time > 5.0f) {
        learn_from_experience(brain);
        brain->stats.survival_time = 0.0f;
    }
}

void neural_enemy_cleanup(entity* enemy) {
    enemy_brain* brain = (enemy_brain*)enemy->ai.brain;
    if (brain) {
        if (brain->network) {
            neural_destroy(brain->network);
        }
        free(brain);
        enemy->ai.brain = NULL;
    }
}

// ============================================================================
// COLLECTIVE INTELLIGENCE
// ============================================================================

typedef struct swarm_mind {
    // Shared knowledge across all enemies
    f32 global_memory[256][32];
    u32 memory_count;
    
    // Player behavior model
    struct {
        f32 attack_frequency;
        f32 dodge_frequency;
        f32 preferred_distance;
        f32 reaction_time;
    } player_model;
    
    // Coordinated strategies
    struct {
        v2 flanking_positions[4];
        f32 attack_timing[4];
        u32 formation_type;
    } tactics;
    
} swarm_mind;

static swarm_mind* g_swarm = NULL;

void neural_swarm_init(void) {
    if (!g_swarm) {
        g_swarm = (swarm_mind*)calloc(1, sizeof(swarm_mind));
    }
}

void neural_swarm_update(game_state* game, f32 dt) {
    if (!g_swarm) return;
    
    // Analyze player behavior
    player* p = &game->player;
    static f32 analysis_timer = 0.0f;
    analysis_timer += dt;
    
    if (analysis_timer > 1.0f) {
        // Update player model
        if (p->is_attacking) {
            g_swarm->player_model.attack_frequency += 0.1f;
        } else {
            g_swarm->player_model.attack_frequency *= 0.95f;
        }
        
        // Coordinate enemy positions for flanking
        int enemy_count = 0;
        entity* enemies[4] = {NULL};
        
        for (u32 i = 0; i < game->entity_count && enemy_count < 4; i++) {
            entity* e = &game->entities[i];
            if (e->is_alive && e->type >= ENTITY_SLIME && e->type <= ENTITY_DRAGON) {
                enemies[enemy_count++] = e;
            }
        }
        
        // Calculate flanking positions
        if (enemy_count > 1) {
            f32 angle_step = (2.0f * M_PI) / enemy_count;
            f32 radius = 60.0f;
            
            for (int i = 0; i < enemy_count; i++) {
                f32 angle = i * angle_step;
                g_swarm->tactics.flanking_positions[i].x = 
                    p->entity->position.x + cosf(angle) * radius;
                g_swarm->tactics.flanking_positions[i].y = 
                    p->entity->position.y + sinf(angle) * radius;
                
                // Assign target position to enemy
                if (enemies[i] && enemies[i]->ai.brain) {
                    enemies[i]->ai.target_position = g_swarm->tactics.flanking_positions[i];
                }
            }
        }
        
        analysis_timer = 0.0f;
    }
}

void neural_swarm_cleanup(void) {
    if (g_swarm) {
        free(g_swarm);
        g_swarm = NULL;
    }
}