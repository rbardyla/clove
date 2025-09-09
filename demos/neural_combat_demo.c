#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

typedef unsigned int u32;
typedef float f32;

#define MAX_COMBATANTS 8
#define COMBAT_MEMORY_SIZE 64
#define MAX_COMBAT_MOVES 16

// Combat move types
typedef enum {
    MOVE_ATTACK = 0,
    MOVE_DEFEND,
    MOVE_DODGE,
    MOVE_FEINT,
    MOVE_COUNTER,
    MOVE_SPECIAL,
    MOVE_RETREAT,
    MOVE_WAIT,
    MOVE_COUNT
} combat_move_type;

// Combat statistics for learning
typedef struct {
    f32 damage_dealt;
    f32 damage_taken;
    f32 accuracy;
    f32 dodge_rate;
    f32 counter_success;
    u32 total_moves;
    u32 wins;
    u32 losses;
} combat_stats;

// Neural network for combat decisions
typedef struct {
    f32 input_weights[96];      // 12 inputs * 8 hidden neurons
    f32 combat_weights[64];     // 8 combat experience * 8 hidden
    f32 output_weights[64];     // 8 hidden * 8 moves
    f32 biases[8];
    f32 hidden[8];
    f32 output[8];
    f32 learning_rate;
    f32 experience_weight;      // How much combat history influences decisions
} combat_neural_net;

// Combat entity with neural AI
typedef struct {
    char name[32];
    
    // Combat attributes
    f32 health;
    f32 max_health;
    f32 attack_power;
    f32 defense;
    f32 agility;
    f32 stamina;
    f32 max_stamina;
    
    // Neural combat AI
    combat_neural_net brain;
    combat_stats stats;
    f32 combat_memory[COMBAT_MEMORY_SIZE];  // Recent combat patterns
    u32 memory_index;
    
    // Combat state
    f32 combat_stance;      // 0=defensive, 1=aggressive
    f32 fatigue;            // Affects performance
    f32 fear_level;         // Affects decision making
    f32 confidence;         // Built from wins/losses
    
    // Strategy adaptation
    f32 opponent_patterns[MAX_COMBATANTS][MOVE_COUNT];  // Learned opponent behaviors
    f32 move_effectiveness[MOVE_COUNT];                 // Success rate per move type
    
    // Personality in combat
    f32 aggression;         // 0.0 to 1.0
    f32 patience;           // 0.0 to 1.0 
    f32 cunning;            // 0.0 to 1.0
    f32 discipline;         // 0.0 to 1.0
} combat_npc;

// Combat encounter state
typedef struct {
    combat_npc fighters[MAX_COMBATANTS];
    u32 fighter_count;
    u32 current_turn;
    u32 round_number;
    f32 battlefield_conditions[4];  // lighting, terrain, weather, noise
    combat_move_type last_moves[MAX_COMBATANTS];
    f32 move_outcomes[MAX_COMBATANTS];  // Success/failure of last moves
} combat_encounter;

// Initialize combat NPC with neural brain
void InitializeCombatNPC(combat_npc *npc, const char *name, u32 archetype) {
    strncpy(npc->name, name, 31);
    npc->name[31] = 0;
    
    // Set base attributes based on archetype
    switch(archetype) {
        case 0: // Warrior - High attack, medium defense
            npc->max_health = 120.0f;
            npc->attack_power = 18.0f;
            npc->defense = 12.0f;
            npc->agility = 8.0f;
            npc->aggression = 0.8f;
            npc->patience = 0.4f;
            npc->cunning = 0.5f;
            npc->discipline = 0.7f;
            break;
        case 1: // Rogue - High agility, low health
            npc->max_health = 80.0f;
            npc->attack_power = 14.0f;
            npc->defense = 6.0f;
            npc->agility = 16.0f;
            npc->aggression = 0.6f;
            npc->patience = 0.7f;
            npc->cunning = 0.9f;
            npc->discipline = 0.5f;
            break;
        case 2: // Paladin - Balanced, high discipline
            npc->max_health = 100.0f;
            npc->attack_power = 15.0f;
            npc->defense = 15.0f;
            npc->agility = 10.0f;
            npc->aggression = 0.5f;
            npc->patience = 0.8f;
            npc->cunning = 0.4f;
            npc->discipline = 0.9f;
            break;
        case 3: // Berserker - Very high attack, low defense
            npc->max_health = 100.0f;
            npc->attack_power = 22.0f;
            npc->defense = 8.0f;
            npc->agility = 12.0f;
            npc->aggression = 0.95f;
            npc->patience = 0.2f;
            npc->cunning = 0.3f;
            npc->discipline = 0.4f;
            break;
    }
    
    npc->health = npc->max_health;
    npc->max_stamina = 100.0f;
    npc->stamina = npc->max_stamina;
    
    // Initialize neural network with personality bias
    npc->brain.learning_rate = 0.01f + (f32)rand() / RAND_MAX * 0.02f;  // 0.01 to 0.03
    npc->brain.experience_weight = 0.3f;
    
    for(int i = 0; i < 96; i++) {
        npc->brain.input_weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.4f;
    }
    for(int i = 0; i < 64; i++) {
        npc->brain.combat_weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.3f;
        npc->brain.output_weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.5f;
    }
    for(int i = 0; i < 8; i++) {
        npc->brain.biases[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.2f;
    }
    
    // Initialize combat state
    npc->combat_stance = 0.5f;
    npc->fatigue = 0.0f;
    npc->fear_level = 0.1f;
    npc->confidence = 0.5f;
    
    // Clear opponent patterns and move effectiveness
    memset(npc->opponent_patterns, 0, sizeof(npc->opponent_patterns));
    for(int i = 0; i < MOVE_COUNT; i++) {
        npc->move_effectiveness[i] = 0.5f;  // Start neutral
    }
    
    // Initialize stats
    memset(&npc->stats, 0, sizeof(npc->stats));
    memset(npc->combat_memory, 0, sizeof(npc->combat_memory));
    npc->memory_index = 0;
}

// Enhanced activation function for combat decisions
f32 combat_tanh(f32 x) {
    if (x > 4.0f) return 0.999f;
    if (x < -4.0f) return -0.999f;
    f32 exp2x = expf(2.0f * x);
    return (exp2x - 1.0f) / (exp2x + 1.0f);
}

// Process neural combat decision
void ProcessCombatThinking(combat_npc *npc, combat_encounter *encounter, u32 npc_id, u32 target_id) {
    combat_npc *target = &encounter->fighters[target_id];
    
    // Prepare input vector [12 elements]
    f32 inputs[12];
    inputs[0] = npc->health / npc->max_health;                    // Health ratio
    inputs[1] = npc->stamina / npc->max_stamina;                  // Stamina ratio  
    inputs[2] = target->health / target->max_health;              // Target health
    inputs[3] = npc->fatigue;                                     // Current fatigue
    inputs[4] = npc->fear_level;                                  // Fear state
    inputs[5] = npc->confidence;                                  // Confidence level
    inputs[6] = encounter->battlefield_conditions[0];            // Lighting
    inputs[7] = encounter->battlefield_conditions[1];            // Terrain
    inputs[8] = (f32)encounter->round_number / 20.0f;            // Combat duration
    inputs[9] = (target->attack_power - npc->defense) / 20.0f;   // Threat level
    inputs[10] = (npc->attack_power - target->defense) / 20.0f;  // Damage potential
    inputs[11] = npc->combat_stance;                             // Current stance
    
    // Prepare combat experience vector [8 elements]
    f32 combat_experience[8];
    if(npc->stats.total_moves > 0) {
        combat_experience[0] = npc->stats.accuracy;
        combat_experience[1] = npc->stats.dodge_rate;
        combat_experience[2] = npc->stats.counter_success;
        combat_experience[3] = (f32)npc->stats.wins / (npc->stats.wins + npc->stats.losses + 1);
        combat_experience[4] = npc->opponent_patterns[target_id][MOVE_ATTACK];
        combat_experience[5] = npc->opponent_patterns[target_id][MOVE_DEFEND];
        combat_experience[6] = npc->opponent_patterns[target_id][MOVE_DODGE];
        combat_experience[7] = npc->move_effectiveness[encounter->last_moves[npc_id]];
    } else {
        // No experience yet - use personality
        combat_experience[0] = npc->discipline;
        combat_experience[1] = npc->agility / 20.0f;
        combat_experience[2] = npc->cunning;
        combat_experience[3] = 0.5f;  // Unknown win rate
        combat_experience[4] = npc->aggression;
        combat_experience[5] = npc->patience;
        combat_experience[6] = npc->cunning;
        combat_experience[7] = 0.5f;  // Unknown effectiveness
    }
    
    // Neural network forward pass
    for(int h = 0; h < 8; h++) {
        f32 sum = npc->brain.biases[h];
        
        // Process inputs
        for(int i = 0; i < 12; i++) {
            sum += inputs[i] * npc->brain.input_weights[h * 12 + (i % 12)];
        }
        
        // Process combat experience
        for(int i = 0; i < 8; i++) {
            sum += combat_experience[i] * npc->brain.combat_weights[h * 8 + i] * npc->brain.experience_weight;
        }
        
        // Add personality influence
        sum += npc->aggression * 0.2f * (h == 0 ? 1.0f : 0.0f);  // Bias toward attack
        sum += npc->patience * 0.2f * (h == 1 ? 1.0f : 0.0f);   // Bias toward defense
        sum += npc->cunning * 0.2f * (h == 3 ? 1.0f : 0.0f);    // Bias toward feint
        
        npc->brain.hidden[h] = combat_tanh(sum);
    }
    
    // Output layer
    for(int o = 0; o < 8; o++) {
        f32 sum = 0;
        for(int h = 0; h < 8; h++) {
            sum += npc->brain.hidden[h] * npc->brain.output_weights[o * 8 + h];
        }
        
        // Apply stamina and health penalties
        if(npc->stamina < 20.0f) {
            if(o == MOVE_ATTACK || o == MOVE_SPECIAL) sum -= 1.0f;  // Discourage expensive moves
        }
        if(npc->health < npc->max_health * 0.25f) {
            if(o == MOVE_RETREAT) sum += 0.5f;  // Encourage retreat when low health
        }
        
        npc->brain.output[o] = 1.0f / (1.0f + expf(-sum));  // Sigmoid activation
    }
    
    // Store memory of this decision context
    u32 mem_slot = npc->memory_index % COMBAT_MEMORY_SIZE;
    npc->combat_memory[mem_slot] = (inputs[0] + inputs[2] + combat_experience[3]) / 3.0f;
    npc->memory_index++;
}

// Get best combat move
combat_move_type GetCombatAction(combat_npc *npc) {
    combat_move_type best_move = MOVE_ATTACK;
    f32 best_score = npc->brain.output[0];
    
    for(int i = 1; i < MOVE_COUNT && i < 8; i++) {
        if(npc->brain.output[i] > best_score) {
            best_score = npc->brain.output[i];
            best_move = (combat_move_type)i;
        }
    }
    
    return best_move;
}

// Execute combat round between two fighters
f32 ExecuteCombatRound(combat_encounter *encounter, u32 attacker_id, u32 defender_id) {
    combat_npc *attacker = &encounter->fighters[attacker_id];
    combat_npc *defender = &encounter->fighters[defender_id];
    
    // Both fighters think about their moves
    ProcessCombatThinking(attacker, encounter, attacker_id, defender_id);
    ProcessCombatThinking(defender, encounter, defender_id, attacker_id);
    
    combat_move_type attacker_move = GetCombatAction(attacker);
    combat_move_type defender_move = GetCombatAction(defender);
    
    // Store moves for learning
    encounter->last_moves[attacker_id] = attacker_move;
    encounter->last_moves[defender_id] = defender_move;
    
    f32 damage_dealt = 0.0f;
    f32 attacker_success = 0.0f;
    f32 defender_success = 0.0f;
    
    const char* move_names[] = {"Attack", "Defend", "Dodge", "Feint", "Counter", "Special", "Retreat", "Wait"};
    
    // Combat resolution matrix
    if(attacker_move == MOVE_ATTACK) {
        f32 base_damage = attacker->attack_power * (0.8f + (f32)rand() / RAND_MAX * 0.4f);  // 80-120% damage
        
        if(defender_move == MOVE_DEFEND) {
            damage_dealt = fmaxf(0, base_damage - defender->defense);
            attacker_success = 0.6f;
            defender_success = 0.7f;
        } else if(defender_move == MOVE_DODGE) {
            f32 hit_chance = (attacker->agility + 10.0f) / (defender->agility + 15.0f);
            if((f32)rand() / RAND_MAX < hit_chance) {
                damage_dealt = base_damage;
                attacker_success = 0.8f;
                defender_success = 0.2f;
            } else {
                damage_dealt = 0.0f;
                attacker_success = 0.1f;
                defender_success = 0.9f;
            }
        } else if(defender_move == MOVE_COUNTER) {
            f32 counter_chance = defender->cunning * 0.6f;
            if((f32)rand() / RAND_MAX < counter_chance) {
                // Counter successful - defender deals damage
                damage_dealt = -(defender->attack_power * 0.8f);
                attacker_success = 0.0f;
                defender_success = 1.0f;
            } else {
                // Counter failed - full damage to defender
                damage_dealt = base_damage * 1.2f;
                attacker_success = 1.0f;
                defender_success = 0.0f;
            }
        } else {
            damage_dealt = base_damage;
            attacker_success = 0.8f;
            defender_success = 0.3f;
        }
    } else if(attacker_move == MOVE_FEINT) {
        if(defender_move == MOVE_DEFEND || defender_move == MOVE_COUNTER) {
            // Feint successful - follow-up attack
            damage_dealt = attacker->attack_power * 1.3f;
            attacker_success = 1.0f;
            defender_success = 0.1f;
        } else {
            // Feint failed
            damage_dealt = 0.0f;
            attacker_success = 0.3f;
            defender_success = 0.6f;
        }
    } else if(attacker_move == MOVE_SPECIAL) {
        if(attacker->stamina >= 30.0f) {
            damage_dealt = attacker->attack_power * 1.5f;
            attacker->stamina -= 30.0f;
            attacker_success = 0.9f;
            defender_success = 0.2f;
        } else {
            // Not enough stamina
            damage_dealt = 0.0f;
            attacker_success = 0.0f;
            defender_success = 0.5f;
        }
    }
    
    // Apply damage
    if(damage_dealt > 0) {
        defender->health -= damage_dealt;
        defender->health = fmaxf(0, defender->health);
    } else if(damage_dealt < 0) {
        attacker->health += damage_dealt;  // Negative damage = damage to attacker
        attacker->health = fmaxf(0, attacker->health);
        damage_dealt = -damage_dealt;  // For reporting purposes
    }
    
    // Update combat learning
    attacker->opponent_patterns[defender_id][defender_move] += 0.1f;
    defender->opponent_patterns[attacker_id][attacker_move] += 0.1f;
    
    attacker->move_effectiveness[attacker_move] = attacker->move_effectiveness[attacker_move] * 0.9f + attacker_success * 0.1f;
    defender->move_effectiveness[defender_move] = defender->move_effectiveness[defender_move] * 0.9f + defender_success * 0.1f;
    
    // Update stats
    attacker->stats.total_moves++;
    defender->stats.total_moves++;
    
    if(damage_dealt > 0) {
        attacker->stats.damage_dealt += damage_dealt;
        defender->stats.damage_taken += damage_dealt;
        attacker->stats.accuracy += 0.05f;
    }
    
    // Update psychological state
    if(attacker_success > 0.7f) {
        attacker->confidence += 0.02f;
        defender->fear_level += 0.01f;
    }
    if(defender_success > 0.7f) {
        defender->confidence += 0.02f;
        attacker->fear_level += 0.01f;
    }
    
    // Clamp values
    attacker->confidence = fmaxf(0.1f, fminf(1.0f, attacker->confidence));
    defender->confidence = fmaxf(0.1f, fminf(1.0f, defender->confidence));
    attacker->fear_level = fmaxf(0.0f, fminf(0.8f, attacker->fear_level));
    defender->fear_level = fmaxf(0.0f, fminf(0.8f, defender->fear_level));
    
    // Update stamina
    attacker->stamina += 5.0f;  // Small recovery per turn
    defender->stamina += 5.0f;
    attacker->stamina = fminf(attacker->max_stamina, attacker->stamina);
    defender->stamina = fminf(defender->max_stamina, defender->stamina);
    
    encounter->move_outcomes[attacker_id] = attacker_success;
    encounter->move_outcomes[defender_id] = defender_success;
    
    printf("  %s (%s) vs %s (%s) | Damage: %.1f | Health: %.1f/%.1f vs %.1f/%.1f\n",
           attacker->name, move_names[attacker_move],
           defender->name, move_names[defender_move],
           damage_dealt, 
           attacker->health, attacker->max_health,
           defender->health, defender->max_health);
    
    return damage_dealt;
}

// Run neural combat tournament
void RunNeuralCombatDemo() {
    printf("=============================================\n");
    printf("  Handmade Neural Combat AI System\n");
    printf("=============================================\n");
    
    combat_encounter tournament = {0};
    
    // Create fighters with different archetypes
    const char* names[] = {"Gareth", "Shadow", "Paladin", "Ragnar", "Lyanna", "Thorne"};
    const char* classes[] = {"Warrior", "Rogue", "Paladin", "Berserker", "Warrior", "Rogue"};
    
    tournament.fighter_count = 6;
    for(u32 i = 0; i < tournament.fighter_count; i++) {
        InitializeCombatNPC(&tournament.fighters[i], names[i], i % 4);
        printf("Created %s (%s) - HP:%.0f ATK:%.0f DEF:%.0f AGI:%.0f\n",
               tournament.fighters[i].name, classes[i],
               tournament.fighters[i].max_health,
               tournament.fighters[i].attack_power,
               tournament.fighters[i].defense,
               tournament.fighters[i].agility);
        printf("  Personality: Aggression(%.2f) Patience(%.2f) Cunning(%.2f) Discipline(%.2f)\n",
               tournament.fighters[i].aggression,
               tournament.fighters[i].patience,
               tournament.fighters[i].cunning,
               tournament.fighters[i].discipline);
    }
    
    // Set battlefield conditions
    tournament.battlefield_conditions[0] = 0.8f;  // Good lighting
    tournament.battlefield_conditions[1] = 0.6f;  // Moderate terrain
    tournament.battlefield_conditions[2] = 0.3f;  // Light rain
    tournament.battlefield_conditions[3] = 0.4f;  // Some noise
    
    printf("\nBattlefield: Light(%.1f) Terrain(%.1f) Weather(%.1f) Noise(%.1f)\n\n",
           tournament.battlefield_conditions[0], tournament.battlefield_conditions[1],
           tournament.battlefield_conditions[2], tournament.battlefield_conditions[3]);
    
    // Run multiple combat encounters
    for(int fight = 0; fight < 4; fight++) {
        printf("=== Combat Encounter %d ===\n", fight + 1);
        
        u32 fighter_a = (fight * 2) % tournament.fighter_count;
        u32 fighter_b = (fighter_a + 1) % tournament.fighter_count;
        
        // Reset health for new fight
        tournament.fighters[fighter_a].health = tournament.fighters[fighter_a].max_health;
        tournament.fighters[fighter_b].health = tournament.fighters[fighter_b].max_health;
        tournament.fighters[fighter_a].stamina = tournament.fighters[fighter_a].max_stamina;
        tournament.fighters[fighter_b].stamina = tournament.fighters[fighter_b].max_stamina;
        
        printf("%s vs %s - Fight begins!\n",
               tournament.fighters[fighter_a].name,
               tournament.fighters[fighter_b].name);
        
        tournament.round_number = 0;
        while(tournament.fighters[fighter_a].health > 0 && 
              tournament.fighters[fighter_b].health > 0 && 
              tournament.round_number < 15) {
            
            tournament.round_number++;
            printf("\n--- Round %d ---\n", tournament.round_number);
            
            // Determine initiative (higher agility goes first)
            u32 first = (tournament.fighters[fighter_a].agility >= tournament.fighters[fighter_b].agility) ? fighter_a : fighter_b;
            u32 second = (first == fighter_a) ? fighter_b : fighter_a;
            
            // Execute combat rounds
            ExecuteCombatRound(&tournament, first, second);
            
            if(tournament.fighters[second].health > 0) {
                ExecuteCombatRound(&tournament, second, first);
            }
        }
        
        // Determine winner
        u32 winner, loser;
        if(tournament.fighters[fighter_a].health > tournament.fighters[fighter_b].health) {
            winner = fighter_a;
            loser = fighter_b;
        } else {
            winner = fighter_b;
            loser = fighter_a;
        }
        
        tournament.fighters[winner].stats.wins++;
        tournament.fighters[loser].stats.losses++;
        
        printf("\n🏆 %s WINS! (%.1f HP remaining)\n",
               tournament.fighters[winner].name,
               tournament.fighters[winner].health);
        
        // Show learning progress
        printf("Learning Progress:\n");
        printf("  %s - Confidence: %.2f, Effectiveness: %.2f\n",
               tournament.fighters[winner].name,
               tournament.fighters[winner].confidence,
               tournament.fighters[winner].move_effectiveness[0]);
        printf("  %s - Confidence: %.2f, Effectiveness: %.2f\n\n",
               tournament.fighters[loser].name,
               tournament.fighters[loser].confidence,
               tournament.fighters[loser].move_effectiveness[0]);
    }
    
    // Final tournament results
    printf("=== Final Tournament Results ===\n");
    for(u32 i = 0; i < tournament.fighter_count; i++) {
        combat_npc *fighter = &tournament.fighters[i];
        f32 win_rate = (f32)fighter->stats.wins / (fighter->stats.wins + fighter->stats.losses + 1);
        printf("%s: %d wins, %d losses (%.1f%% win rate) | Confidence: %.2f\n",
               fighter->name, fighter->stats.wins, fighter->stats.losses,
               win_rate * 100.0f, fighter->confidence);
        printf("  Combat Stats: %.1f dmg dealt, %.1f dmg taken, %.0f total moves\n",
               fighter->stats.damage_dealt, fighter->stats.damage_taken, (f32)fighter->stats.total_moves);
    }
    
    printf("\n=== Neural Learning Analysis ===\n");
    for(u32 i = 0; i < tournament.fighter_count; i++) {
        combat_npc *fighter = &tournament.fighters[i];
        printf("%s learned move effectiveness:\n", fighter->name);
        const char* move_names[] = {"Attack", "Defend", "Dodge", "Feint", "Counter", "Special", "Retreat", "Wait"};
        for(int m = 0; m < MOVE_COUNT; m++) {
            printf("  %s: %.2f  ", move_names[m], fighter->move_effectiveness[m]);
            if((m + 1) % 4 == 0) printf("\n");
        }
        printf("\n");
    }
    
    printf("\n=============================================\n");
    printf("Neural combat simulation complete!\n");
    printf("NPCs adapted their fighting strategies through\n");
    printf("experience, learning which moves work best\n");
    printf("against different opponent types and situations.\n");
    printf("=============================================\n");
}

int main() {
    srand((unsigned int)time(NULL));
    RunNeuralCombatDemo();
    return 0;
}