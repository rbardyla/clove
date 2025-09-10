#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Extended AI system with dynamic quest generation
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

typedef enum npc_need {
    NEED_FOOD,
    NEED_SOCIAL,
    NEED_WORK,
    NEED_REST,
    NEED_SAFETY,
    NEED_COUNT
} npc_need;

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
    char last_topic[32];
} social_relationship;

typedef enum quest_type {
    QUEST_DELIVER_ITEM,      // "Bring me X from Y"
    QUEST_GATHER_RESOURCE,   // "I need X quantity of Y"
    QUEST_SOCIAL_FAVOR,      // "Tell X that I said Y"
    QUEST_MEDIATION,         // "Help me resolve conflict with X"
    QUEST_INFORMATION,       // "Find out about X"
    QUEST_EMOTIONAL_SUPPORT, // "I need someone to talk to"
    QUEST_COUNT
} quest_type;

typedef enum quest_urgency {
    URGENCY_LOW,      // "When you have time..."
    URGENCY_MEDIUM,   // "I could really use help with..."
    URGENCY_HIGH,     // "I desperately need..."
    URGENCY_CRITICAL  // "Please help me immediately!"
} quest_urgency;

typedef struct dynamic_quest {
    quest_type type;
    quest_urgency urgency;
    u32 giver_id;
    u32 target_npc_id;    // For social quests
    char item_needed[32];
    u32 quantity_needed;
    f32 emotional_weight; // How much this quest means to the NPC (0-1)
    f32 time_limit;       // Game hours until quest expires
    f32 reward_value;     // How much they'll pay/like you more
    char description[256];
    char motivation[128]; // Why they need this
    u8 active;
    u8 completed;
    f32 generation_time;  // When quest was created
} dynamic_quest;

typedef struct neural_npc {
    u32 id;
    char name[32];
    char occupation[32];
    f32 personality[TRAIT_COUNT];
    f32 emotions[EMOTION_COUNT];
    f32 base_emotions[EMOTION_COUNT];
    social_relationship relationships[8];
    u32 relationship_count;
    f32 needs[NEED_COUNT];
    
    // Quest system
    dynamic_quest* active_quest_given;   // Quest they gave to player
    dynamic_quest pending_quests[3];     // Quests they want to give
    u32 pending_quest_count;
    
    // Resources and economics
    u32 inventory_stone;
    u32 inventory_flower;
    u32 inventory_food;
    u32 inventory_wood;
    f32 wealth;
    
    // Behavioral state
    u32 current_behavior;
    char current_thought[128];
    f32 player_reputation;
    f32 player_familiarity;
    
    // Quest generation parameters
    f32 quest_generation_cooldown;
    f32 last_quest_time;
    u32 total_quests_given;
} neural_npc;

const char* trait_names[] = {
    "Extroversion", "Agreeableness", "Conscientiousness", "Neuroticism", "Openness"
};

const char* emotion_names[] = {
    "Happiness", "Sadness", "Anger", "Fear", "Surprise"
};

const char* quest_type_names[] = {
    "Delivery", "Gathering", "Social Favor", "Mediation", "Information", "Emotional Support"
};

const char* urgency_names[] = {
    "Low", "Medium", "High", "Critical"
};

// === QUEST GENERATION ENGINE ===

quest_urgency calculate_quest_urgency(neural_npc* npc, quest_type type) {
    f32 urgency_score = 0.0f;
    
    // Base urgency from emotional state
    urgency_score += npc->emotions[EMOTION_SADNESS] * 0.4f;
    urgency_score += npc->emotions[EMOTION_FEAR] * 0.5f;
    urgency_score += npc->emotions[EMOTION_ANGER] * 0.3f;
    
    // Need-based urgency
    switch (type) {
        case QUEST_GATHER_RESOURCE:
            urgency_score += (npc->needs[NEED_FOOD] + npc->needs[NEED_WORK]) * 0.5f;
            break;
        case QUEST_SOCIAL_FAVOR:
        case QUEST_EMOTIONAL_SUPPORT:
            urgency_score += npc->needs[NEED_SOCIAL] * 0.6f;
            break;
        case QUEST_MEDIATION:
            urgency_score += npc->emotions[EMOTION_ANGER] * 0.7f;
            break;
    }
    
    // Personality affects urgency expression
    urgency_score *= (1.0f + npc->personality[TRAIT_NEUROTICISM] * 0.5f);
    urgency_score *= (1.0f - npc->personality[TRAIT_CONSCIENTIOUSNESS] * 0.2f); // Organized people plan better
    
    if (urgency_score > 0.8f) return URGENCY_CRITICAL;
    else if (urgency_score > 0.6f) return URGENCY_HIGH;
    else if (urgency_score > 0.3f) return URGENCY_MEDIUM;
    else return URGENCY_LOW;
}

void generate_delivery_quest(neural_npc* giver, neural_npc* npcs, u32 npc_count, dynamic_quest* quest) {
    quest->type = QUEST_DELIVER_ITEM;
    quest->giver_id = giver->id;
    quest->urgency = calculate_quest_urgency(giver, QUEST_DELIVER_ITEM);
    
    // Find a target NPC (preferably one they have a relationship with)
    u32 target_id = 0;
    f32 best_relationship = -200.0f;
    
    for (u32 i = 0; i < giver->relationship_count; i++) {
        if (giver->relationships[i].affection > best_relationship) {
            best_relationship = giver->relationships[i].affection;
            target_id = giver->relationships[i].target_npc_id;
        }
    }
    
    // If no relationships, pick random NPC
    if (best_relationship < -100.0f) {
        target_id = rand() % npc_count;
        if (target_id == giver->id) target_id = (target_id + 1) % npc_count;
    }
    
    quest->target_npc_id = target_id;
    
    // Choose item based on what giver has
    if (giver->inventory_flower > 0) {
        strcpy(quest->item_needed, "flower");
        quest->quantity_needed = 1;
        giver->inventory_flower--;
    } else if (giver->inventory_food > 0) {
        strcpy(quest->item_needed, "food");
        quest->quantity_needed = 1;
        giver->inventory_food--;
    } else {
        strcpy(quest->item_needed, "stone");
        quest->quantity_needed = 1;
        giver->inventory_stone = 1; // They'll give you one
    }
    
    // Generate motivation based on relationship and personality
    if (best_relationship > 40.0f) {
        snprintf(quest->motivation, 127, "I want to do something nice for %s", npcs[target_id].name);
    } else if (giver->emotions[EMOTION_SADNESS] > 0.6f) {
        snprintf(quest->motivation, 127, "I've been feeling down and want to make amends");
    } else {
        snprintf(quest->motivation, 127, "I owe %s a favor and want to pay it back", npcs[target_id].name);
    }
    
    // Generate description
    snprintf(quest->description, 255, "Could you deliver this %s to %s for me? %s.",
             quest->item_needed, npcs[target_id].name, quest->motivation);
    
    quest->emotional_weight = 0.3f + (giver->personality[TRAIT_AGREEABLENESS] * 0.4f);
    quest->reward_value = 10.0f + quest->emotional_weight * 20.0f;
    quest->time_limit = 24.0f; // 1 day
    quest->active = 1;
    quest->completed = 0;
}

void generate_gathering_quest(neural_npc* giver, dynamic_quest* quest) {
    quest->type = QUEST_GATHER_RESOURCE;
    quest->giver_id = giver->id;
    quest->urgency = calculate_quest_urgency(giver, QUEST_GATHER_RESOURCE);
    
    // What they need depends on their occupation and current needs
    if (strcmp(giver->occupation, "Farmer") == 0) {
        if (rand() % 2) {
            strcpy(quest->item_needed, "stone");
            quest->quantity_needed = 3 + rand() % 3;
            strcpy(quest->motivation, "I need stones to build a new fence for my crops");
        } else {
            strcpy(quest->item_needed, "wood");
            quest->quantity_needed = 2 + rand() % 2;
            strcpy(quest->motivation, "I need wood to repair my farming tools");
        }
    } else if (strcmp(giver->occupation, "Merchant") == 0) {
        strcpy(quest->item_needed, "flower");
        quest->quantity_needed = 5 + rand() % 5;
        strcpy(quest->motivation, "Flowers sell well in the market - I'll split the profits!");
    } else if (strcmp(giver->occupation, "Artist") == 0) {
        strcpy(quest->item_needed, "flower");
        quest->quantity_needed = 2 + rand() % 2;
        strcpy(quest->motivation, "I need beautiful flowers for my next painting");
    } else {
        // Generic villager
        const char* items[] = {"stone", "flower", "wood"};
        strcpy(quest->item_needed, items[rand() % 3]);
        quest->quantity_needed = 2 + rand() % 3;
        strcpy(quest->motivation, "I just need these for a project I'm working on");
    }
    
    snprintf(quest->description, 255, "I need %u %s. %s. Can you help me gather them?",
             quest->quantity_needed, quest->item_needed, quest->motivation);
    
    quest->emotional_weight = 0.2f + (giver->needs[NEED_WORK] * 0.5f);
    quest->reward_value = quest->quantity_needed * 5.0f + quest->emotional_weight * 15.0f;
    quest->time_limit = 48.0f; // 2 days
    quest->active = 1;
    quest->completed = 0;
}

void generate_social_quest(neural_npc* giver, neural_npc* npcs, u32 npc_count, dynamic_quest* quest) {
    quest->type = QUEST_SOCIAL_FAVOR;
    quest->giver_id = giver->id;
    quest->urgency = calculate_quest_urgency(giver, QUEST_SOCIAL_FAVOR);
    
    // Find someone they have a relationship with
    u32 target_id = 0;
    f32 best_relationship = -200.0f;
    
    for (u32 i = 0; i < giver->relationship_count; i++) {
        // Look for someone they care about (positive or negative)
        f32 relationship_strength = fabsf(giver->relationships[i].affection);
        if (relationship_strength > best_relationship) {
            best_relationship = relationship_strength;
            target_id = giver->relationships[i].target_npc_id;
        }
    }
    
    if (best_relationship < 10.0f) {
        // No strong relationships, pick random
        target_id = rand() % npc_count;
        if (target_id == giver->id) target_id = (target_id + 1) % npc_count;
    }
    
    quest->target_npc_id = target_id;
    
    // Find the relationship to this target
    social_relationship* rel = NULL;
    for (u32 i = 0; i < giver->relationship_count; i++) {
        if (giver->relationships[i].target_npc_id == target_id) {
            rel = &giver->relationships[i];
            break;
        }
    }
    
    // Generate message based on relationship
    if (rel && rel->affection > 40.0f) {
        // Good relationship - positive message
        snprintf(quest->motivation, 127, "I want %s to know how much I appreciate them", npcs[target_id].name);
        snprintf(quest->description, 255, "Could you tell %s that I think they're wonderful? %s.",
                 npcs[target_id].name, quest->motivation);
    } else if (rel && rel->affection < -20.0f) {
        // Bad relationship - apology
        snprintf(quest->motivation, 127, "I've been too proud to apologize to %s directly", npcs[target_id].name);
        snprintf(quest->description, 255, "Could you tell %s that I'm sorry for our disagreement? %s.",
                 npcs[target_id].name, quest->motivation);
    } else {
        // Neutral - invitation or question
        snprintf(quest->motivation, 127, "I'd like to get to know %s better", npcs[target_id].name);
        snprintf(quest->description, 255, "Could you ask %s if they'd like to have dinner together? %s.",
                 npcs[target_id].name, quest->motivation);
    }
    
    quest->emotional_weight = 0.4f + (giver->personality[TRAIT_EXTROVERSION] * 0.3f);
    quest->reward_value = 15.0f + quest->emotional_weight * 25.0f;
    quest->time_limit = 12.0f; // Half day - social things are time-sensitive
    quest->active = 1;
    quest->completed = 0;
}

void generate_emotional_support_quest(neural_npc* giver, dynamic_quest* quest) {
    quest->type = QUEST_EMOTIONAL_SUPPORT;
    quest->giver_id = giver->id;
    quest->urgency = calculate_quest_urgency(giver, QUEST_EMOTIONAL_SUPPORT);
    
    // Generate motivation based on current emotional state
    if (giver->emotions[EMOTION_SADNESS] > 0.7f) {
        strcpy(quest->motivation, "I've been feeling really sad lately and need someone to talk to");
    } else if (giver->emotions[EMOTION_FEAR] > 0.6f) {
        strcpy(quest->motivation, "I'm worried about things and need reassurance");
    } else if (giver->emotions[EMOTION_ANGER] > 0.6f) {
        strcpy(quest->motivation, "I'm frustrated and need to vent to someone");
    } else {
        strcpy(quest->motivation, "I'm feeling lonely and could use some company");
    }
    
    snprintf(quest->description, 255, "Could you just sit and talk with me for a while? %s.",
             quest->motivation);
    
    quest->emotional_weight = 0.6f + (giver->personality[TRAIT_NEUROTICISM] * 0.3f);
    quest->reward_value = 5.0f + quest->emotional_weight * 15.0f; // Lower material reward but higher relationship gain
    quest->time_limit = 6.0f; // Short time limit - emotional needs are urgent
    quest->active = 1;
    quest->completed = 0;
}

// Check if NPC should generate a quest
u8 should_generate_quest(neural_npc* npc, f32 current_time) {
    // Cooldown check
    if (current_time - npc->last_quest_time < npc->quest_generation_cooldown) {
        return 0;
    }
    
    // Don't generate if they already have a pending quest with player
    if (npc->active_quest_given && npc->active_quest_given->active) {
        return 0;
    }
    
    // Check if they have strong enough motivation
    f32 motivation_score = 0.0f;
    
    // High needs create motivation
    for (u32 i = 0; i < NEED_COUNT; i++) {
        motivation_score += npc->needs[i] * 0.2f;
    }
    
    // Strong emotions create motivation
    motivation_score += npc->emotions[EMOTION_SADNESS] * 0.3f;
    motivation_score += npc->emotions[EMOTION_FEAR] * 0.2f;
    motivation_score += npc->emotions[EMOTION_ANGER] * 0.2f;
    
    // Personality affects quest generation likelihood
    motivation_score += npc->personality[TRAIT_EXTROVERSION] * 0.2f; // Extroverts ask for help more
    motivation_score += npc->personality[TRAIT_AGREEABLENESS] * 0.1f; // Agreeable people more likely to ask nicely
    motivation_score -= npc->personality[TRAIT_CONSCIENTIOUSNESS] * 0.1f; // Conscientious people handle things themselves
    
    // Player relationship affects willingness to ask
    if (npc->player_reputation > 20.0f) {
        motivation_score += 0.3f; // They trust the player
    } else if (npc->player_reputation < -20.0f) {
        motivation_score -= 0.4f; // They don't trust the player
    }
    
    return (motivation_score > 0.5f + (rand() % 20) / 100.0f); // Some randomness
}

// Generate a quest for an NPC
void generate_quest_for_npc(neural_npc* npc, neural_npc* npcs, u32 npc_count, f32 current_time) {
    if (npc->pending_quest_count >= 3) return; // Already has too many pending quests
    
    dynamic_quest* quest = &npc->pending_quests[npc->pending_quest_count];
    memset(quest, 0, sizeof(dynamic_quest));
    quest->generation_time = current_time;
    
    // Choose quest type based on NPC's current situation
    f32 type_weights[QUEST_COUNT] = {0};
    
    // Delivery quests - good for social NPCs with items
    type_weights[QUEST_DELIVER_ITEM] = npc->personality[TRAIT_AGREEABLENESS] * 0.3f;
    if (npc->inventory_flower > 0 || npc->inventory_food > 0 || npc->inventory_stone > 0) {
        type_weights[QUEST_DELIVER_ITEM] += 0.4f;
    }
    
    // Gathering quests - good for work-focused NPCs
    type_weights[QUEST_GATHER_RESOURCE] = npc->needs[NEED_WORK] * 0.5f + 
                                         npc->personality[TRAIT_CONSCIENTIOUSNESS] * 0.3f;
    
    // Social quests - good for extroverted NPCs with relationships
    type_weights[QUEST_SOCIAL_FAVOR] = npc->personality[TRAIT_EXTROVERSION] * 0.4f +
                                      npc->needs[NEED_SOCIAL] * 0.3f;
    if (npc->relationship_count > 0) {
        type_weights[QUEST_SOCIAL_FAVOR] += 0.3f;
    }
    
    // Emotional support - good for sad/anxious NPCs
    type_weights[QUEST_EMOTIONAL_SUPPORT] = npc->emotions[EMOTION_SADNESS] * 0.6f +
                                           npc->emotions[EMOTION_FEAR] * 0.4f +
                                           npc->personality[TRAIT_NEUROTICISM] * 0.2f;
    
    // Find best quest type
    quest_type best_type = QUEST_DELIVER_ITEM;
    f32 best_weight = type_weights[0];
    for (u32 i = 1; i < QUEST_COUNT - 2; i++) { // Skip MEDIATION and INFORMATION for now
        if (type_weights[i] > best_weight) {
            best_weight = type_weights[i];
            best_type = (quest_type)i;
        }
    }
    
    // Generate the chosen quest type
    switch (best_type) {
        case QUEST_DELIVER_ITEM:
            generate_delivery_quest(npc, npcs, npc_count, quest);
            break;
        case QUEST_GATHER_RESOURCE:
            generate_gathering_quest(npc, quest);
            break;
        case QUEST_SOCIAL_FAVOR:
            generate_social_quest(npc, npcs, npc_count, quest);
            break;
        case QUEST_EMOTIONAL_SUPPORT:
            generate_emotional_support_quest(npc, quest);
            break;
        default:
            generate_gathering_quest(npc, quest); // Default fallback
            break;
    }
    
    npc->pending_quest_count++;
    npc->last_quest_time = current_time;
    npc->quest_generation_cooldown = 20.0f + (rand() % 400) / 10.0f; // 20-60 seconds
    npc->total_quests_given++;
    
    printf("\n🎯 NEW QUEST GENERATED!\n");
    printf("Giver: %s the %s\n", npc->name, npc->occupation);
    printf("Type: %s (%s urgency)\n", quest_type_names[quest->type], urgency_names[quest->urgency]);
    printf("Description: %s\n", quest->description);
    printf("Reward: %.1f reputation points\n", quest->reward_value);
    printf("Time Limit: %.1f hours\n", quest->time_limit);
    printf("Emotional Weight: %.2f (how much this means to them)\n", quest->emotional_weight);
}

// Initialize NPC with quest generation capabilities
void init_quest_npc(neural_npc* npc, u32 id, const char* name, const char* archetype) {
    npc->id = id;
    strncpy(npc->name, name, 31);
    npc->name[31] = '\0';
    strcpy(npc->occupation, archetype);
    
    // Initialize personality based on archetype
    if (strcmp(archetype, "Merchant") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.8f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.7f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        npc->personality[TRAIT_NEUROTICISM] = 0.3f;
        npc->personality[TRAIT_OPENNESS] = 0.6f;
    } else if (strcmp(archetype, "Farmer") == 0) {
        npc->personality[TRAIT_EXTROVERSION] = 0.4f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.8f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.9f;
        npc->personality[TRAIT_NEUROTICISM] = 0.2f;
        npc->personality[TRAIT_OPENNESS] = 0.5f;
    } else { // Artist
        npc->personality[TRAIT_EXTROVERSION] = 0.3f;
        npc->personality[TRAIT_AGREEABLENESS] = 0.6f;
        npc->personality[TRAIT_CONSCIENTIOUSNESS] = 0.4f;
        npc->personality[TRAIT_NEUROTICISM] = 0.7f;
        npc->personality[TRAIT_OPENNESS] = 0.9f;
    }
    
    // Initialize emotions
    for (u32 i = 0; i < EMOTION_COUNT; i++) {
        npc->base_emotions[i] = 0.3f + (rand() % 40) / 100.0f;
        npc->emotions[i] = npc->base_emotions[i];
    }
    
    // Initialize needs
    for (u32 i = 0; i < NEED_COUNT; i++) {
        npc->needs[i] = 0.3f + (rand() % 50) / 100.0f; // 0.3 to 0.8
    }
    
    // Initialize resources
    npc->inventory_stone = rand() % 3;
    npc->inventory_flower = rand() % 3;
    npc->inventory_food = 3 + rand() % 5;
    npc->inventory_wood = rand() % 2;
    npc->wealth = 20.0f + rand() % 50;
    
    // Initialize quest system
    npc->active_quest_given = NULL;
    npc->pending_quest_count = 0;
    npc->quest_generation_cooldown = 10.0f + rand() % 20;
    npc->last_quest_time = 0.0f;
    npc->total_quests_given = 0;
    
    // Initialize player relationship
    npc->player_reputation = -10.0f + (rand() % 20); // -10 to +10
    npc->player_familiarity = 0.0f;
    
    npc->relationship_count = 0;
    strcpy(npc->current_thought, "Living my best life...");
}

void create_relationship(neural_npc* npc1, neural_npc* npc2) {
    if (npc1->relationship_count < 8) {
        social_relationship* rel = &npc1->relationships[npc1->relationship_count];
        rel->target_npc_id = npc2->id;
        rel->affection = (rand() % 61) - 30; // -30 to +30
        rel->respect = rand() % 31; // 0 to +30
        rel->trust = rand() % 21; // 0 to +20
        rel->interactions = rand() % 5;
        strcpy(rel->last_topic, "general chat");
        npc1->relationship_count++;
    }
}

int main() {
    printf("========================================\n");
    printf("   DYNAMIC QUEST GENERATION SYSTEM\n");
    printf("========================================\n");
    
    srand(time(NULL));
    
    // Create test NPCs
    neural_npc npcs[4];
    init_quest_npc(&npcs[0], 0, "Marcus", "Merchant");
    init_quest_npc(&npcs[1], 1, "Elena", "Farmer");
    init_quest_npc(&npcs[2], 2, "Luna", "Artist");
    init_quest_npc(&npcs[3], 3, "Ben", "Farmer");
    
    // Create some relationships
    create_relationship(&npcs[0], &npcs[1]); // Marcus knows Elena
    create_relationship(&npcs[1], &npcs[3]); // Elena knows Ben  
    create_relationship(&npcs[2], &npcs[0]); // Luna knows Marcus
    
    printf("Initialized 4 NPCs with quest generation capabilities!\n\n");
    
    // Show NPC status
    for (u32 i = 0; i < 4; i++) {
        neural_npc* npc = &npcs[i];
        printf("=== %s the %s ===\n", npc->name, npc->occupation);
        printf("Resources: Stone:%u Flower:%u Food:%u Wood:%u Wealth:%.0f\n",
               npc->inventory_stone, npc->inventory_flower, npc->inventory_food, 
               npc->inventory_wood, npc->wealth);
        printf("Player Rep: %.1f  Relationships: %u\n", npc->player_reputation, npc->relationship_count);
        printf("High Needs: ");
        for (u32 j = 0; j < NEED_COUNT; j++) {
            if (npc->needs[j] > 0.6f) {
                printf("%s:%.1f ", emotion_names[j], npc->needs[j] * 100);
            }
        }
        printf("\nHigh Emotions: ");
        for (u32 j = 0; j < EMOTION_COUNT; j++) {
            if (npc->emotions[j] > 0.6f) {
                printf("%s:%.1f ", emotion_names[j], npc->emotions[j] * 100);
            }
        }
        printf("\n\n");
    }
    
    printf("========================================\n");
    printf("   RUNNING QUEST GENERATION SIMULATION\n");
    printf("========================================\n");
    
    f32 current_time = 0.0f;
    
    // Run simulation for 10 cycles
    for (u32 cycle = 0; cycle < 10; cycle++) {
        printf("\n--- Time: %.1f hours (Cycle %u) ---\n", current_time, cycle + 1);
        
        // Check each NPC for quest generation
        for (u32 i = 0; i < 4; i++) {
            neural_npc* npc = &npcs[i];
            
            // Simulate need/emotion changes over time
            npc->needs[NEED_FOOD] += 0.05f + (rand() % 10) / 200.0f;
            npc->needs[NEED_SOCIAL] += 0.03f + (rand() % 10) / 300.0f;
            npc->emotions[EMOTION_SADNESS] += (rand() % 10) / 500.0f;
            
            // Cap values
            for (u32 j = 0; j < NEED_COUNT; j++) {
                if (npc->needs[j] > 1.0f) npc->needs[j] = 1.0f;
            }
            for (u32 j = 0; j < EMOTION_COUNT; j++) {
                if (npc->emotions[j] > 1.0f) npc->emotions[j] = 1.0f;
            }
            
            // Check for quest generation
            if (should_generate_quest(npc, current_time)) {
                generate_quest_for_npc(npc, npcs, 4, current_time);
            }
        }
        
        current_time += 5.0f; // 5 hours per cycle
    }
    
    printf("\n========================================\n");
    printf("   QUEST GENERATION SUMMARY\n");
    printf("========================================\n");
    
    u32 total_quests = 0;
    for (u32 i = 0; i < 4; i++) {
        neural_npc* npc = &npcs[i];
        printf("%s the %s: %u quests generated, %u pending\n",
               npc->name, npc->occupation, npc->total_quests_given, npc->pending_quest_count);
        total_quests += npc->total_quests_given;
        
        // Show pending quests
        for (u32 j = 0; j < npc->pending_quest_count; j++) {
            dynamic_quest* quest = &npc->pending_quests[j];
            printf("  -> %s: %s (Reward: %.0f)\n",
                   quest_type_names[quest->type], quest->description, quest->reward_value);
        }
    }
    
    printf("\n✓ Dynamic Quest System Demonstration Complete!\n");
    printf("✓ Generated %u unique quests based on NPC personalities\n", total_quests);
    printf("✓ Quest types vary by occupation and emotional state\n");
    printf("✓ Urgency calculated from needs and personality\n");
    printf("✓ Rewards scale with emotional investment\n");
    printf("✓ Social quests leverage existing relationships\n");
    printf("✓ Each quest has meaningful motivation and context\n");
    
    return 0;
}