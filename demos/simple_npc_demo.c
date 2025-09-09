#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

// Simple types
typedef unsigned int u32;
typedef float f32;

// Simple neural network for demonstration
typedef struct {
    f32 weights[32];
    f32 biases[8];
    f32 hidden[8];
    f32 output[4];
} simple_neural_net;

typedef struct {
    char name[32];
    f32 personality[4];  // friendly, curious, cautious, energetic
    f32 mood[4];        // current emotional state
    simple_neural_net brain;
    f32 memory[16];     // simple episodic memory
    u32 interaction_count;
} simple_npc;

// Initialize NPC with random personality
void InitializeNPC(simple_npc *npc, const char *name) {
    strncpy(npc->name, name, 31);
    npc->name[31] = 0;
    
    // Random personality traits (0.0 to 1.0)
    for(int i = 0; i < 4; i++) {
        npc->personality[i] = (f32)rand() / RAND_MAX;
        npc->mood[i] = npc->personality[i] * 0.8f + 0.1f;
    }
    
    // Initialize simple brain
    for(int i = 0; i < 32; i++) {
        npc->brain.weights[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.2f;
    }
    for(int i = 0; i < 8; i++) {
        npc->brain.biases[i] = ((f32)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    
    // Clear memory
    memset(npc->memory, 0, sizeof(npc->memory));
    npc->interaction_count = 0;
}

// Simple tanh approximation
f32 fast_tanh(f32 x) {
    if (x > 2.0f) return 1.0f;
    if (x < -2.0f) return -1.0f;
    f32 x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Process NPC decision making
void ProcessNPCThinking(simple_npc *npc, f32 *inputs) {
    // Simple feedforward network
    // Input: 4 sensory inputs + 4 personality + 4 mood = 12 inputs
    // Hidden: 8 neurons
    // Output: 4 actions (greet, talk, trade, leave)
    
    // Forward pass
    for(int h = 0; h < 8; h++) {
        f32 sum = npc->brain.biases[h];
        
        // Process inputs (sensory + personality + mood)
        for(int i = 0; i < 4; i++) {
            sum += inputs[i] * npc->brain.weights[h * 4 + i];
        }
        for(int i = 0; i < 4; i++) {
            sum += npc->personality[i] * npc->brain.weights[h * 4 + i] * 0.5f;
        }
        for(int i = 0; i < 4; i++) {
            sum += npc->mood[i] * npc->brain.weights[h * 4 + i] * 0.3f;
        }
        
        npc->brain.hidden[h] = fast_tanh(sum);
    }
    
    // Output layer
    for(int o = 0; o < 4; o++) {
        f32 sum = 0;
        for(int h = 0; h < 8; h++) {
            sum += npc->brain.hidden[h] * npc->brain.weights[24 + o * 2 + (h % 2)];
        }
        npc->brain.output[o] = 1.0f / (1.0f + expf(-sum)); // sigmoid
    }
    
    // Update memory with this interaction
    int mem_slot = npc->interaction_count % 16;
    npc->memory[mem_slot] = (inputs[0] + inputs[1] + inputs[2] + inputs[3]) / 4.0f;
    
    npc->interaction_count++;
}

// Get NPC action
int GetNPCAction(simple_npc *npc) {
    int best_action = 0;
    f32 best_score = npc->brain.output[0];
    
    for(int i = 1; i < 4; i++) {
        if(npc->brain.output[i] > best_score) {
            best_score = npc->brain.output[i];
            best_action = i;
        }
    }
    
    return best_action;
}

// Generate NPC response
const char* GenerateResponse(simple_npc *npc, int action) {
    static char response[256];
    const char* greetings[] = {"Hello there!", "Good day!", "Well met!", "Greetings, friend!"};
    const char* conversations[] = {"How are you today?", "What brings you here?", "Lovely weather, isn't it?", "I've been thinking..."};
    const char* trades[] = {"I have some goods to trade.", "Interested in making a deal?", "My wares are the finest!", "What do you need?"};
    const char* farewells[] = {"I must be going now.", "Farewell!", "Until we meet again!", "Safe travels!"};
    
    switch(action) {
        case 0: // Greet
            snprintf(response, 256, "%s: %s (Friendliness: %.2f)", 
                    npc->name, greetings[npc->interaction_count % 4], npc->mood[0]);
            break;
        case 1: // Talk
            snprintf(response, 256, "%s: %s (Curiosity: %.2f)", 
                    npc->name, conversations[npc->interaction_count % 4], npc->mood[1]);
            break;
        case 2: // Trade
            snprintf(response, 256, "%s: %s (Energy: %.2f)", 
                    npc->name, trades[npc->interaction_count % 4], npc->mood[3]);
            break;
        case 3: // Leave
            snprintf(response, 256, "%s: %s (Caution: %.2f)", 
                    npc->name, farewells[npc->interaction_count % 4], npc->mood[2]);
            break;
        default:
            snprintf(response, 256, "%s: *confused*", npc->name);
    }
    
    return response;
}

// Demo scenarios
void RunNPCDemo() {
    printf("============================================\n");
    printf("  Handmade Neural NPC Demo\n");
    printf("============================================\n");
    printf("\nCreating NPCs with unique personalities...\n\n");
    
    // Create different NPCs
    simple_npc npcs[4];
    const char* names[] = {"Aria", "Björn", "Celia", "Dmitri"};
    
    for(int i = 0; i < 4; i++) {
        InitializeNPC(&npcs[i], names[i]);
        printf("%s personality: Friendly(%.2f) Curious(%.2f) Cautious(%.2f) Energetic(%.2f)\n",
               npcs[i].name, npcs[i].personality[0], npcs[i].personality[1], 
               npcs[i].personality[2], npcs[i].personality[3]);
    }
    
    printf("\n--- Interactive Demo ---\n");
    
    // Simulate different scenarios
    f32 scenarios[5][4] = {
        {0.8f, 0.2f, 0.1f, 0.9f}, // Player approaches friendly
        {0.3f, 0.9f, 0.4f, 0.6f}, // Player asks questions
        {0.5f, 0.7f, 0.8f, 0.3f}, // Player seems suspicious
        {0.9f, 0.5f, 0.2f, 0.8f}, // Player offers trade
        {0.2f, 0.3f, 0.9f, 0.1f}  // Player acts threatening
    };
    
    const char* scenario_names[] = {
        "Friendly Approach", "Curious Questioning", "Suspicious Behavior", 
        "Trade Offer", "Threatening Gesture"
    };
    
    for(int scenario = 0; scenario < 5; scenario++) {
        printf("\n=== Scenario %d: %s ===\n", scenario + 1, scenario_names[scenario]);
        
        for(int i = 0; i < 4; i++) {
            ProcessNPCThinking(&npcs[i], scenarios[scenario]);
            int action = GetNPCAction(&npcs[i]);
            printf("  %s\n", GenerateResponse(&npcs[i], action));
            
            // Show neural activity
            printf("    [Brain activity: %.2f %.2f %.2f %.2f] -> Action: %d\n",
                   npcs[i].brain.output[0], npcs[i].brain.output[1], 
                   npcs[i].brain.output[2], npcs[i].brain.output[3], action);
        }
    }
    
    printf("\n--- Memory Demonstration ---\n");
    for(int i = 0; i < 4; i++) {
        printf("%s memory trace: ", npcs[i].name);
        for(int m = 0; m < 5; m++) {
            printf("%.2f ", npcs[i].memory[m]);
        }
        printf("(interactions: %d)\n", npcs[i].interaction_count);
    }
    
    printf("\n============================================\n");
    printf("Demo complete! Each NPC responded uniquely\n");
    printf("based on their personality and the neural\n");
    printf("processing of the situation.\n");
    printf("============================================\n");
}

int main() {
    // Seed random for different personalities each run
    srand((unsigned int)time(NULL));
    
    RunNPCDemo();
    
    return 0;
}