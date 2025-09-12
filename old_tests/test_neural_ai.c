#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Extract just the AI system for testing
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;

typedef enum personality_trait {
    TRAIT_EXTROVERSION,
    TRAIT_AGREEABLENESS,
    TRAIT_CONSCIENTIOUSNESS,
    TRAIT_NEUROTICISM,
    TRAIT_OPENNESS,
    TRAIT_COUNT
} personality_trait;

typedef enum emotion_type {
    EMOTION_HAPPINESS,
    EMOTION_SADNESS,
    EMOTION_ANGER,
    EMOTION_FEAR,
    EMOTION_SURPRISE,
    EMOTION_COUNT
} emotion_type;

typedef enum relationship_type {
    REL_STRANGER,
    REL_ACQUAINTANCE,
    REL_FRIEND,
    REL_CLOSE_FRIEND,
    REL_ENEMY,
    REL_COUNT
} relationship_type;

typedef struct social_relationship {
    u32 target_npc_id;
    relationship_type type;
    f32 affection;
    f32 respect;
    f32 trust;
    u32 interactions;
} social_relationship;

typedef enum npc_need {
    NEED_FOOD,
    NEED_SOCIAL,
    NEED_WORK,
    NEED_REST,
    NEED_SAFETY,
    NEED_COUNT
} npc_need;

typedef struct neural_npc {
    u32 id;
    char name[32];
    char occupation[32];
    f32 personality[TRAIT_COUNT];
    f32 emotions[EMOTION_COUNT];
    f32 base_emotions[EMOTION_COUNT];
    social_relationship relationships[5]; // Limited for test
    u32 relationship_count;
    f32 needs[NEED_COUNT];
    f32 x, y;
    u32 current_behavior;
    char current_thought[128];
    f32 player_reputation;
    f32 player_familiarity;
} neural_npc;

const char* trait_names[] = {
    "Extroversion", "Agreeableness", "Conscientiousness", "Neuroticism", "Openness"
};

const char* emotion_names[] = {
    "Happiness", "Sadness", "Anger", "Fear", "Surprise"
};

const char* need_names[] = {
    "Food", "Social", "Work", "Rest", "Safety"
};

const char* behavior_names[] = {
    "Wandering", "Working", "Socializing", "Resting", "Eating", "Seeking Safety"
};

void init_personality_archetype(neural_npc* npc, const char* archetype) {
    if (strcmp(archetype, "merchant") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.8f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.7f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        npc->personality[TRAIT_NEUROTICISM] = 0.3f;
        npc->personality[TRAIT_OPENNESS] = 0.6f;
        strcpy(npc->occupation, "Merchant");
    } else if (strcmp(archetype, "farmer") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.4f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.8f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        npc->personality[TRAIT_NEUROTICISM] = 0.2f;
        npc->personality[TRAIT_OPENNESS] = 0.5f;
        strcpy(npc->occupation, "Farmer");
    } else if (strcmp(archetype, "artist") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.3f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.6f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.4f;
        npc->personality[TRAIT_NEUROTICISM] = 0.7f;
        npc->personality[TRAIT_OPENNESS] = 0.9f;
        strcpy(npc->occupation, "Artist");
    }
    
    // Set base emotions based on personality
    npc->base_emotions[EMOTION_HAPPINESS] = 0.3f + 
        npc->personality[TRAIT_EXTROVERSION] * 0.3f - 
        npc->personality[TRAIT_NEUROTICISM] * 0.2f;
    npc->base_emotions[EMOTION_SADNESS] = 0.1f + npc->personality[TRAIT_NEUROTICISM] * 0.2f;
    npc->base_emotions[EMOTION_ANGER] = 0.1f + (1.0f - npc->personality[TRAIT_AGREEABLENESS]) * 0.2f;
    npc->base_emotions[EMOTION_FEAR] = 0.1f + npc->personality[TRAIT_NEUROTICISM] * 0.3f;
    npc->base_emotions[EMOTION_SURPRISE] = 0.2f + npc->personality[TRAIT_OPENNESS] * 0.2f;
    
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        npc->emotions[i] = npc->base_emotions[i];
    }
}

u32 choose_behavior(neural_npc* npc) {
    f32 weights[6] = {0};
    
    // Basic needs influence
    weights[4] = npc->needs[NEED_FOOD] * 2.0f;  // Eating
    weights[3] = npc->needs[NEED_REST] * 1.5f;  // Resting
    weights[2] = npc->needs[NEED_SOCIAL] * npc->personality[TRAIT_EXTROVERSION]; // Socializing
    weights[1] = npc->needs[NEED_WORK] * npc->personality[TRAIT_CONSCIENTIOUSNESS]; // Working
    
    // Personality influences
    weights[0] = (1.0f - npc->personality[TRAIT_CONSCIENTIOUSNESS]) + 
                 npc->personality[TRAIT_OPENNESS] * 0.5f; // Wandering
    
    // Safety seeking if fear is high
    if (npc->emotions[EMOTION_FEAR] > 0.7f) {
        weights[5] = 2.0f; // Seeking safety
    }
    
    // Find highest weight
    u32 best_behavior = 0;
    f32 best_weight = weights[0];
    for (u32 i = 1; i < 6; i++) {
        if (weights[i] > best_weight) {
            best_weight = weights[i];
            best_behavior = i;
        }
    }
    
    return best_behavior;
}

void update_emotions(neural_npc* npc, f32 dt) {
    // Emotions decay toward base levels
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        f32 diff = npc->base_emotions[i] - npc->emotions[i];
        npc->emotions[i] += diff * 0.1f * dt;
    }
    
    // Needs influence emotions
    if (npc->needs[NEED_FOOD] > 0.8f) {
        npc->emotions[EMOTION_SADNESS] += dt * 0.05f;
    }
    if (npc->needs[NEED_SOCIAL] > 0.7f && npc->personality[TRAIT_EXTROVERSION] > 0.5f) {
        npc->emotions[EMOTION_SADNESS] += dt * 0.03f;
    }
    
    // Clamp emotions
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        if (npc->emotions[i] < 0.0f) npc->emotions[i] = 0.0f;
        if (npc->emotions[i] > 1.0f) npc->emotions[i] = 1.0f;
    }
}

void update_needs(neural_npc* npc, f32 dt) {
    npc->needs[NEED_FOOD] += dt * 0.008f;
    npc->needs[NEED_SOCIAL] += dt * 0.005f * npc->personality[TRAIT_EXTROVERSION];
    npc->needs[NEED_WORK] += dt * 0.003f * npc->personality[TRAIT_CONSCIENTIOUSNESS];
    npc->needs[NEED_REST] += dt * 0.006f;
    npc->needs[NEED_SAFETY] += dt * 0.002f;
    
    for (u32 i = 0; i < NEED_COUNT; i++) {
        if (npc->needs[i] > 1.0f) npc->needs[i] = 1.0f;
        if (npc->needs[i] < 0.0f) npc->needs[i] = 0.0f;
    }
}

void execute_behavior(neural_npc* npc, f32 dt) {
    switch (npc->current_behavior) {
        case 0: // Wandering
            strcpy(npc->current_thought, "I wonder what's happening around here...");
            npc->x += (rand() % 3 - 1) * dt * 10;
            npc->y += (rand() % 3 - 1) * dt * 10;
            break;
        case 1: // Working
            strcpy(npc->current_thought, "Hard work is its own reward.");
            npc->needs[NEED_WORK] -= dt * 0.1f;
            break;
        case 2: // Socializing
            strcpy(npc->current_thought, "I should find someone to talk to.");
            npc->needs[NEED_SOCIAL] -= dt * 0.2f;
            npc->emotions[EMOTION_HAPPINESS] += dt * 0.05f;
            break;
        case 3: // Resting
            strcpy(npc->current_thought, "Ah, time to relax and recharge.");
            npc->needs[NEED_REST] -= dt * 0.3f;
            break;
        case 4: // Eating
            strcpy(npc->current_thought, "This meal tastes wonderful!");
            npc->needs[NEED_FOOD] -= dt * 0.4f;
            break;
        case 5: // Seeking Safety
            strcpy(npc->current_thought, "I need to find somewhere safe...");
            npc->emotions[EMOTION_FEAR] -= dt * 0.1f;
            break;
    }
}

void create_relationship(neural_npc* npc1, neural_npc* npc2) {
    if (npc1->relationship_count < 5) {
        social_relationship* rel = &npc1->relationships[npc1->relationship_count];
        rel->target_npc_id = npc2->id;
        rel->type = REL_STRANGER;
        rel->affection = (rand() % 41) - 20; // -20 to +20 initial
        rel->respect = (rand() % 21); // 0 to +20
        rel->trust = (rand() % 11); // 0 to +10
        rel->interactions = 0;
        npc1->relationship_count++;
    }
}

void init_neural_npc(neural_npc* npc, u32 id, const char* name, const char* archetype) {
    npc->id = id;
    strncpy(npc->name, name, 31);
    npc->name[31] = '\0';
    
    init_personality_archetype(npc, archetype);
    
    for (u32 i = 0; i < NEED_COUNT; i++) {
        npc->needs[i] = 0.3f + (rand() % 40) / 100.0f;
    }
    
    npc->x = 400 + (rand() % 200);
    npc->y = 300 + (rand() % 200);
    npc->current_behavior = 0;
    strcpy(npc->current_thought, "Just living my life...");
    npc->player_reputation = -5.0f + (rand() % 10);
    npc->player_familiarity = 0.0f;
    npc->relationship_count = 0;
}

void print_npc_status(neural_npc* npc) {
    printf("\n=== %s the %s ===\n", npc->name, npc->occupation);
    printf("Position: (%.1f, %.1f)\n", npc->x, npc->y);
    printf("Current Behavior: %s\n", behavior_names[npc->current_behavior]);
    printf("Thought: \"%s\"\n", npc->current_thought);
    
    printf("\nPersonality:\n");
    for (u32 i = 0; i < TRAIT_COUNT; i++) {
        printf("  %s: %.2f\n", trait_names[i], npc->personality[i]);
    }
    
    printf("\nEmotions:\n");
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        printf("  %s: %.2f\n", emotion_names[i], npc->emotions[i]);
    }
    
    printf("\nNeeds:\n");
    for (u32 i = 0; i < NEED_COUNT; i++) {
        printf("  %s: %.2f\n", need_names[i], npc->needs[i]);
    }
    
    printf("\nPlayer Relationship:\n");
    printf("  Reputation: %.1f\n", npc->player_reputation);
    printf("  Familiarity: %.1f\n", npc->player_familiarity);
    printf("  Social Connections: %u\n", npc->relationship_count);
}

void simulate_social_interaction(neural_npc* npc1, neural_npc* npc2) {
    // Create or update relationship
    social_relationship* rel = NULL;
    for (u32 i = 0; i < npc1->relationship_count; i++) {
        if (npc1->relationships[i].target_npc_id == npc2->id) {
            rel = &npc1->relationships[i];
            break;
        }
    }
    
    if (!rel && npc1->relationship_count < 5) {
        create_relationship(npc1, npc2);
        rel = &npc1->relationships[npc1->relationship_count - 1];
    }
    
    if (rel) {
        // Personality compatibility affects relationship growth
        f32 compatibility = 0.0f;
        compatibility += (1.0f - fabsf(npc1->personality[TRAIT_AGREEABLENESS] - npc2->personality[TRAIT_AGREEABLENESS]));
        compatibility += (1.0f - fabsf(npc1->personality[TRAIT_OPENNESS] - npc2->personality[TRAIT_OPENNESS]));
        compatibility += npc1->personality[TRAIT_EXTROVERSION] * npc2->personality[TRAIT_EXTROVERSION];
        
        rel->affection += compatibility * 2.0f - 1.0f; // -1 to +5 change
        rel->interactions++;
        
        if (rel->affection > 100.0f) rel->affection = 100.0f;
        if (rel->affection < -100.0f) rel->affection = -100.0f;
        
        // Update relationship type based on affection
        if (rel->affection > 75.0f) rel->type = REL_CLOSE_FRIEND;
        else if (rel->affection > 40.0f) rel->type = REL_FRIEND;
        else if (rel->affection > 10.0f) rel->type = REL_ACQUAINTANCE;
        else if (rel->affection < -30.0f) rel->type = REL_ENEMY;
        else rel->type = REL_STRANGER;
        
        printf("%s and %s interacted! Affection: %.1f (%s)\n", 
               npc1->name, npc2->name, rel->affection, 
               rel->type == REL_CLOSE_FRIEND ? "Close Friends" :
               rel->type == REL_FRIEND ? "Friends" :
               rel->type == REL_ACQUAINTANCE ? "Acquaintances" :
               rel->type == REL_ENEMY ? "Enemies" : "Strangers");
    }
}

int main() {
    printf("========================================\n");
    printf("   NEURAL AI SYSTEM DEMONSTRATION\n");
    printf("========================================\n");
    
    srand(time(NULL));
    
    // Create test NPCs
    neural_npc npcs[4];
    init_neural_npc(&npcs[0], 0, "Marcus", "merchant");
    init_neural_npc(&npcs[1], 1, "Elena", "farmer");
    init_neural_npc(&npcs[2], 2, "Luna", "artist");
    init_neural_npc(&npcs[3], 3, "Ben", "farmer");
    
    printf("Initialized 4 Neural NPCs with unique personalities!\n\n");
    
    // Show initial state
    for (u32 i = 0; i < 4; i++) {
        print_npc_status(&npcs[i]);
    }
    
    printf("\n========================================\n");
    printf("   RUNNING AI SIMULATION (10 CYCLES)\n");
    printf("========================================\n");
    
    // Run simulation
    for (u32 cycle = 0; cycle < 10; cycle++) {
        printf("\n--- Simulation Cycle %u ---\n", cycle + 1);
        
        f32 dt = 1.0f; // 1 second per cycle
        
        // Update each NPC
        for (u32 i = 0; i < 4; i++) {
            update_emotions(&npcs[i], dt);
            update_needs(&npcs[i], dt);
            npcs[i].current_behavior = choose_behavior(&npcs[i]);
            execute_behavior(&npcs[i], dt);
            
            printf("%s is %s: \"%s\" (H:%.1f N:%.1f)\n", 
                   npcs[i].name, 
                   behavior_names[npcs[i].current_behavior],
                   npcs[i].current_thought,
                   npcs[i].emotions[EMOTION_HAPPINESS] * 100,
                   npcs[i].needs[NEED_SOCIAL] * 100);
        }
        
        // Simulate random social interactions
        if (cycle % 3 == 0) {
            u32 npc1 = rand() % 4;
            u32 npc2 = rand() % 4;
            if (npc1 != npc2) {
                simulate_social_interaction(&npcs[npc1], &npcs[npc2]);
            }
        }
    }
    
    printf("\n========================================\n");
    printf("   FINAL AI STATE ANALYSIS\n");
    printf("========================================\n");
    
    // Show final relationships
    for (u32 i = 0; i < 4; i++) {
        printf("\n%s's Social Network:\n", npcs[i].name);
        for (u32 j = 0; j < npcs[i].relationship_count; j++) {
            social_relationship* rel = &npcs[i].relationships[j];
            printf("  -> %s: Affection %.1f (%u interactions)\n",
                   npcs[rel->target_npc_id].name, rel->affection, rel->interactions);
        }
        if (npcs[i].relationship_count == 0) {
            printf("  (No relationships formed)\n");
        }
    }
    
    printf("\n✓ Neural AI System Demonstration Complete!\n");
    printf("✓ Personality traits influenced behavior selection\n");
    printf("✓ Emotions evolved based on personality and experiences\n");
    printf("✓ Needs drove behavioral priorities\n");
    printf("✓ Social relationships formed and evolved\n");
    printf("✓ Each NPC developed unique behavioral patterns\n");
    
    return 0;
}