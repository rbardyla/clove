#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

// Enhanced types for multi-NPC system
typedef unsigned int u32;
typedef float f32;

#define MAX_NPCS 8
#define MAX_RELATIONSHIPS 32
#define MEMORY_SIZE 32

// Relationship between two NPCs
typedef struct {
    u32 npc_a, npc_b;
    f32 friendship;     // -1.0 to 1.0
    f32 trust;          // 0.0 to 1.0  
    f32 respect;        // 0.0 to 1.0
    u32 interactions;   // total interaction count
    f32 last_interaction_outcome; // -1.0 to 1.0
} npc_relationship;

// Enhanced neural network for social behavior
typedef struct {
    f32 input_weights[64];      // 8 inputs * 8 hidden
    f32 social_weights[64];     // 8 social context * 8 hidden  
    f32 output_weights[40];     // 8 hidden * 5 outputs
    f32 biases[8];
    f32 hidden[8];
    f32 output[5];              // greet, talk, trade, help, leave
} social_neural_net;

// Enhanced NPC with social capabilities
typedef struct {
    char name[32];
    f32 personality[6];         // friendly, curious, cautious, energetic, generous, competitive
    f32 mood[4];               // happy, angry, fearful, excited
    f32 skills[4];             // combat, trade, magic, social
    social_neural_net brain;
    f32 memory[MEMORY_SIZE];   // episodic memory
    u32 interaction_count;
    f32 social_energy;         // 0.0 to 1.0, depletes with interactions
    f32 reputation;            // -1.0 to 1.0, community standing
    u32 location;              // 0=tavern, 1=market, 2=temple, 3=training
} enhanced_npc;

// Social world state
typedef struct {
    enhanced_npc npcs[MAX_NPCS];
    npc_relationship relationships[MAX_RELATIONSHIPS];
    u32 npc_count;
    u32 relationship_count;
    u32 time_step;
    f32 world_events[4];       // festival, conflict, trade_boom, danger
} social_world;

// Initialize enhanced NPC
void InitializeEnhancedNPC(enhanced_npc *npc, const char *name, u32 archetype) {
    strncpy(npc->name, name, 31);
    npc->name[31] = 0;
    
    // Archetype-based personality initialization
    switch(archetype) {
        case 0: // Merchant
            npc->personality[0] = 0.7f; // friendly
            npc->personality[1] = 0.6f; // curious
            npc->personality[2] = 0.4f; // cautious
            npc->personality[3] = 0.8f; // energetic
            npc->personality[4] = 0.3f; // generous
            npc->personality[5] = 0.7f; // competitive
            npc->skills[1] = 0.9f; // trade
            break;
        case 1: // Guard
            npc->personality[0] = 0.5f;
            npc->personality[1] = 0.3f;
            npc->personality[2] = 0.8f;
            npc->personality[3] = 0.6f;
            npc->personality[4] = 0.7f;
            npc->personality[5] = 0.4f;
            npc->skills[0] = 0.9f; // combat
            break;
        case 2: // Scholar
            npc->personality[0] = 0.4f;
            npc->personality[1] = 0.9f;
            npc->personality[2] = 0.6f;
            npc->personality[3] = 0.3f;
            npc->personality[4] = 0.8f;
            npc->personality[5] = 0.2f;
            npc->skills[2] = 0.9f; // magic
            break;
        case 3: // Innkeeper
            npc->personality[0] = 0.9f;
            npc->personality[1] = 0.7f;
            npc->personality[2] = 0.3f;
            npc->personality[3] = 0.5f;
            npc->personality[4] = 0.6f;
            npc->personality[5] = 0.1f;
            npc->skills[3] = 0.8f; // social
            break;
        default: // Random
            for(int i = 0; i < 6; i++) {
                npc->personality[i] = (f32)rand() / RAND_MAX;
            }
    }
    
    // Initialize mood based on personality
    for(int i = 0; i < 4; i++) {
        npc->mood[i] = npc->personality[i % 6] * 0.7f + 0.2f;
    }
    
    // Initialize neural network with personality bias
    for(int i = 0; i < 64; i++) {
        npc->brain.input_weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.4f;
        npc->brain.social_weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.3f;
    }
    for(int i = 0; i < 40; i++) {
        npc->brain.output_weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.5f;
    }
    for(int i = 0; i < 8; i++) {
        npc->brain.biases[i] = npc->personality[i % 6] * 0.1f - 0.05f;
    }
    
    // Initialize other stats
    npc->social_energy = 1.0f;
    npc->reputation = 0.0f;
    npc->interaction_count = 0;
    npc->location = rand() % 4;
    memset(npc->memory, 0, sizeof(npc->memory));
}

// Find relationship between two NPCs
npc_relationship* FindRelationship(social_world *world, u32 npc_a, u32 npc_b) {
    for(u32 i = 0; i < world->relationship_count; i++) {
        npc_relationship *rel = &world->relationships[i];
        if((rel->npc_a == npc_a && rel->npc_b == npc_b) || 
           (rel->npc_a == npc_b && rel->npc_b == npc_a)) {
            return rel;
        }
    }
    return NULL;
}

// Create new relationship
npc_relationship* CreateRelationship(social_world *world, u32 npc_a, u32 npc_b) {
    if(world->relationship_count >= MAX_RELATIONSHIPS) return NULL;
    
    npc_relationship *rel = &world->relationships[world->relationship_count++];
    rel->npc_a = npc_a;
    rel->npc_b = npc_b;
    rel->friendship = 0.0f;
    rel->trust = 0.5f;
    rel->respect = 0.5f;
    rel->interactions = 0;
    rel->last_interaction_outcome = 0.0f;
    
    return rel;
}

// Enhanced tanh with more dynamic range
f32 enhanced_tanh(f32 x) {
    if (x > 3.0f) return 0.995f;
    if (x < -3.0f) return -0.995f;
    f32 exp2x = expf(2.0f * x);
    return (exp2x - 1.0f) / (exp2x + 1.0f);
}

// Process social neural network
void ProcessSocialThinking(enhanced_npc *npc, social_world *world, u32 npc_id, u32 target_npc) {
    // Prepare input vector [8 elements]
    f32 inputs[8];
    inputs[0] = npc->mood[0];           // happiness
    inputs[1] = npc->mood[1];           // anger  
    inputs[2] = npc->social_energy;     // energy level
    inputs[3] = npc->reputation;        // reputation
    inputs[4] = world->world_events[0]; // current world state
    inputs[5] = world->world_events[1];
    inputs[6] = world->world_events[2];
    inputs[7] = world->world_events[3];
    
    // Prepare social context [8 elements]
    f32 social_context[8];
    npc_relationship *rel = FindRelationship(world, npc_id, target_npc);
    if(rel) {
        social_context[0] = rel->friendship;
        social_context[1] = rel->trust;
        social_context[2] = rel->respect;
        social_context[3] = (f32)rel->interactions / 100.0f; // normalize
        social_context[4] = rel->last_interaction_outcome;
    } else {
        memset(social_context, 0, 5 * sizeof(f32));
    }
    
    // Add target NPC characteristics
    enhanced_npc *target = &world->npcs[target_npc];
    social_context[5] = target->reputation;
    social_context[6] = target->social_energy;
    social_context[7] = target->personality[0]; // target friendliness
    
    // Forward pass through network
    for(int h = 0; h < 8; h++) {
        f32 sum = npc->brain.biases[h];
        
        // Input layer
        for(int i = 0; i < 8; i++) {
            sum += inputs[i] * npc->brain.input_weights[h * 8 + i];
        }
        
        // Social context layer
        for(int i = 0; i < 8; i++) {
            sum += social_context[i] * npc->brain.social_weights[h * 8 + i];
        }
        
        // Add personality influence
        sum += npc->personality[h % 6] * 0.3f;
        
        npc->brain.hidden[h] = enhanced_tanh(sum);
    }
    
    // Output layer
    for(int o = 0; o < 5; o++) {
        f32 sum = 0;
        for(int h = 0; h < 8; h++) {
            sum += npc->brain.hidden[h] * npc->brain.output_weights[o * 8 + h];
        }
        npc->brain.output[o] = 1.0f / (1.0f + expf(-sum)); // sigmoid
    }
    
    // Update memory
    int mem_slot = npc->interaction_count % MEMORY_SIZE;
    npc->memory[mem_slot] = (inputs[0] + social_context[0]) * 0.5f;
}

// Get best social action
int GetSocialAction(enhanced_npc *npc) {
    int best_action = 0;
    f32 best_score = npc->brain.output[0];
    
    for(int i = 1; i < 5; i++) {
        if(npc->brain.output[i] > best_score) {
            best_score = npc->brain.output[i];
            best_action = i;
        }
    }
    
    return best_action;
}

// Execute social interaction between two NPCs
void ExecuteSocialInteraction(social_world *world, u32 npc_a_id, u32 npc_b_id) {
    enhanced_npc *npc_a = &world->npcs[npc_a_id];
    enhanced_npc *npc_b = &world->npcs[npc_b_id];
    
    // Both NPCs process the social situation
    ProcessSocialThinking(npc_a, world, npc_a_id, npc_b_id);
    ProcessSocialThinking(npc_b, world, npc_b_id, npc_a_id);
    
    int action_a = GetSocialAction(npc_a);
    int action_b = GetSocialAction(npc_b);
    
    // Get or create relationship
    npc_relationship *rel = FindRelationship(world, npc_a_id, npc_b_id);
    if(!rel) {
        rel = CreateRelationship(world, npc_a_id, npc_b_id);
    }
    
    // Calculate interaction outcome
    f32 outcome = 0.0f;
    const char* actions[] = {"greet", "talk", "trade", "help", "leave"};
    
    // Compatibility matrix for actions
    f32 compatibility[5][5] = {
        {0.3f, 0.5f, 0.2f, 0.8f, -0.2f}, // greet vs [greet,talk,trade,help,leave]
        {0.5f, 0.7f, 0.4f, 0.6f, -0.3f}, // talk
        {0.2f, 0.4f, 0.9f, 0.3f, -0.4f}, // trade
        {0.8f, 0.6f, 0.3f, 0.9f, -0.1f}, // help
        {-0.2f, -0.3f, -0.4f, -0.1f, 0.1f} // leave
    };
    
    outcome = compatibility[action_a][action_b];
    
    // Adjust outcome based on personalities
    f32 personality_match = 0.0f;
    for(int i = 0; i < 6; i++) {
        personality_match += fabsf(npc_a->personality[i] - npc_b->personality[i]);
    }
    personality_match = 1.0f - (personality_match / 6.0f); // invert so similar = good
    outcome = outcome * 0.7f + personality_match * 0.3f;
    
    // Update relationship
    if(rel) {
        rel->friendship += outcome * 0.1f;
        rel->trust += (outcome > 0 ? outcome * 0.05f : outcome * 0.1f);
        rel->respect += outcome * 0.03f;
        rel->interactions++;
        rel->last_interaction_outcome = outcome;
        
        // Clamp values
        rel->friendship = fmaxf(-1.0f, fminf(1.0f, rel->friendship));
        rel->trust = fmaxf(0.0f, fminf(1.0f, rel->trust));
        rel->respect = fmaxf(0.0f, fminf(1.0f, rel->respect));
    }
    
    // Update NPC states
    npc_a->social_energy -= 0.1f;
    npc_b->social_energy -= 0.1f;
    npc_a->mood[0] += outcome * 0.1f; // happiness
    npc_b->mood[0] += outcome * 0.1f;
    npc_a->interaction_count++;
    npc_b->interaction_count++;
    
    // Update reputation based on public actions
    if(action_a == 3 || action_b == 3) { // helping
        npc_a->reputation += 0.02f;
        npc_b->reputation += 0.02f;
    }
    
    // Print interaction
    printf("  %s (%s) <-> %s (%s) | Outcome: %.2f | Friendship: %.2f\n",
           npc_a->name, actions[action_a], npc_b->name, actions[action_b], 
           outcome, rel ? rel->friendship : 0.0f);
}

// Run social world simulation
void RunSocialWorldDemo() {
    printf("=============================================\n");
    printf("  Handmade Neural Social NPC System\n");
    printf("=============================================\n");
    
    social_world world = {0};
    
    // Create diverse NPCs
    const char* names[] = {"Elena", "Marcus", "Sage", "Gilda", "Thorin", "Lydia", "Caine", "Vera"};
    const char* archetypes[] = {"Merchant", "Guard", "Scholar", "Innkeeper"};
    
    world.npc_count = 6;
    for(u32 i = 0; i < world.npc_count; i++) {
        InitializeEnhancedNPC(&world.npcs[i], names[i], i % 4);
        printf("Created %s (%s) - Personality: F%.2f C%.2f Ca%.2f E%.2f G%.2f Co%.2f\n",
               world.npcs[i].name, archetypes[i % 4],
               world.npcs[i].personality[0], world.npcs[i].personality[1],
               world.npcs[i].personality[2], world.npcs[i].personality[3],
               world.npcs[i].personality[4], world.npcs[i].personality[5]);
    }
    
    // Set world events
    world.world_events[0] = 0.8f; // festival
    world.world_events[1] = 0.2f; // conflict
    world.world_events[2] = 0.6f; // trade boom
    world.world_events[3] = 0.1f; // danger
    
    printf("\nWorld State: Festival(%.1f) Conflict(%.1f) Trade(%.1f) Danger(%.1f)\n\n",
           world.world_events[0], world.world_events[1], 
           world.world_events[2], world.world_events[3]);
    
    // Simulate social interactions over time
    for(int time_step = 0; time_step < 5; time_step++) {
        printf("=== Time Step %d ===\n", time_step + 1);
        
        // Random pairwise interactions
        for(int interaction = 0; interaction < 8; interaction++) {
            u32 npc_a = rand() % world.npc_count;
            u32 npc_b = rand() % world.npc_count;
            
            if(npc_a != npc_b && world.npcs[npc_a].social_energy > 0.1f && 
               world.npcs[npc_b].social_energy > 0.1f) {
                ExecuteSocialInteraction(&world, npc_a, npc_b);
            }
        }
        
        // Restore some social energy
        for(u32 i = 0; i < world.npc_count; i++) {
            world.npcs[i].social_energy += 0.2f;
            world.npcs[i].social_energy = fminf(1.0f, world.npcs[i].social_energy);
        }
        
        printf("\n");
    }
    
    // Show final social network
    printf("=== Final Social Network ===\n");
    for(u32 i = 0; i < world.relationship_count; i++) {
        npc_relationship *rel = &world.relationships[i];
        printf("%s <-> %s: Friendship(%.2f) Trust(%.2f) Respect(%.2f) [%d interactions]\n",
               world.npcs[rel->npc_a].name, world.npcs[rel->npc_b].name,
               rel->friendship, rel->trust, rel->respect, rel->interactions);
    }
    
    printf("\n=== NPC Final States ===\n");
    for(u32 i = 0; i < world.npc_count; i++) {
        enhanced_npc *npc = &world.npcs[i];
        printf("%s: Reputation(%.2f) Energy(%.2f) Happiness(%.2f) Interactions(%d)\n",
               npc->name, npc->reputation, npc->social_energy, 
               npc->mood[0], npc->interaction_count);
    }
    
    printf("\n=============================================\n");
    printf("Social simulation complete! NPCs developed\n");
    printf("complex relationships through neural-driven\n");
    printf("social interactions and personality dynamics.\n");
    printf("=============================================\n");
}

int main() {
    srand((unsigned int)time(NULL));
    RunSocialWorldDemo();
    return 0;
}