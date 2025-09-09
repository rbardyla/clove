#include "handmade.h"
#include "memory.h"
#include "neural_math.h"
#include "lstm.h"
#include "dnc.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

// Missing macro definitions for standalone compilation
#define ArenaPushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define ArenaPushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
#define ArenaPushArrayAligned(Arena, Count, type, Alignment) (type *)PushSizeAligned(Arena, (Count)*sizeof(type), Alignment)

// Functions defined in dnc.c
extern u64 ReadCPUFrequency(void);
extern memory_pool *CreateMemoryPool(memory_arena *Arena, memory_index PoolSize, u32 BlockSize);

/*
    DNC Example: NPCs with Persistent Memory
    
    This demonstrates how NPCs can use the Differentiable Neural Computer
    to form lasting memories of interactions, relationships, and experiences.
    
    Scenario: A village with NPCs that remember:
    - Who they've met
    - What was discussed
    - Emotional context of interactions
    - Temporal relationships between events
    
    The DNC allows NPCs to:
    1. Store memories by content (what happened)
    2. Retrieve memories by similarity (related experiences)
    3. Follow temporal chains (what happened next)
    4. Allocate new memory slots dynamically
*/

// Interaction types for encoding
typedef enum interaction_type
{
    INTERACTION_GREETING,
    INTERACTION_QUEST_GIVE,
    INTERACTION_QUEST_COMPLETE,
    INTERACTION_TRADE,
    INTERACTION_COMBAT,
    INTERACTION_FRIENDSHIP,
    INTERACTION_HOSTILE,
    INTERACTION_NEUTRAL
} interaction_type;

// Simple NPC personality traits
typedef struct npc_personality
{
    f32 Friendliness;    // -1 (hostile) to 1 (friendly)
    f32 Curiosity;       // 0 (indifferent) to 1 (curious)
    f32 Generosity;      // 0 (selfish) to 1 (generous)
    f32 Patience;        // 0 (impatient) to 1 (patient)
    f32 Courage;         // 0 (cowardly) to 1 (brave)
} npc_personality;

// Village NPC with DNC memory
typedef struct village_npc
{
    npc_dnc_context *Memory;
    npc_personality Personality;
    char Name[64];
    u32 NPCId;
    
    // Current state
    f32 CurrentMood[8];        // Emotional state vector
    f32 RelationshipScores[100]; // Relationships with other NPCs/player
    u32 NumRelationships;
    
    // Interaction history
    u32 TotalInteractions;
    f64 LastInteractionTime;
} village_npc;

// Encode an interaction into a vector the DNC can process
void EncodeInteraction(f32 *Output, interaction_type Type, 
                       u32 TargetId, f32 *EmotionalContext,
                       const char *Message, u32 VectorSize)
{
    // Clear output
    memset(Output, 0, VectorSize * sizeof(f32));
    
    // Encode interaction type (one-hot)
    if(Type < 8) Output[Type] = 1.0f;
    
    // Encode target ID (simple hash)
    Output[8] = (f32)(TargetId % 100) / 100.0f;
    Output[9] = (f32)(TargetId / 100) / 100.0f;
    
    // Encode emotional context
    for(u32 i = 0; i < 8 && i + 10 < VectorSize; ++i)
    {
        Output[10 + i] = EmotionalContext[i];
    }
    
    // Encode message (simple character frequency)
    if(Message)
    {
        u32 Len = strlen(Message);
        for(u32 i = 0; i < Len && i < 32; ++i)
        {
            u32 CharIndex = 18 + (Message[i] % 32);
            if(CharIndex < VectorSize)
            {
                Output[CharIndex] += 0.1f;
            }
        }
    }
    
    // Add temporal marker
    Output[VectorSize - 1] = (f32)(ReadCPUTimer() & 0xFFFF) / 65536.0f;
}

// Decode DNC output into NPC response
void DecodeResponse(f32 *DNCOutput, u32 OutputSize,
                   f32 *EmotionalResponse, char *Message)
{
    // Extract emotional response (first 8 components)
    for(u32 i = 0; i < 8 && i < OutputSize; ++i)
    {
        EmotionalResponse[i] = DNCOutput[i];
    }
    
    // Generate message based on output patterns
    f32 Friendliness = DNCOutput[0];
    f32 Surprise = DNCOutput[1];
    f32 Interest = DNCOutput[2];
    
    if(Friendliness > 0.7f)
    {
        strcpy(Message, "Hello friend! I remember you!");
    }
    else if(Surprise > 0.7f)
    {
        strcpy(Message, "Oh! I wasn't expecting to see you again!");
    }
    else if(Interest > 0.5f)
    {
        strcpy(Message, "Interesting... tell me more.");
    }
    else if(Friendliness < -0.5f)
    {
        strcpy(Message, "You again? What do you want?");
    }
    else
    {
        strcpy(Message, "Yes? How can I help you?");
    }
}

// Simulate an interaction between player and NPC
void SimulateInteraction(village_npc *NPC, dnc_system *DNC,
                        interaction_type Type, const char *PlayerMessage)
{
    printf("\n--- Interaction with %s ---\n", NPC->Name);
    printf("Player: %s\n", PlayerMessage);
    
    // Encode the interaction
    f32 *InteractionVector = (f32 *)PushSizeAligned(DNC->Arena, DNC->MemoryVectorSize * sizeof(f32), 64);
    EncodeInteraction(InteractionVector, Type, 1, // Player ID = 1
                     NPC->CurrentMood, PlayerMessage, DNC->MemoryVectorSize);
    
    // Process through DNC
    f32 *Response = (f32 *)PushSizeAligned(DNC->Arena, DNC->OutputSize * sizeof(f32), 64);
    ProcessNPCInteraction(NPC->Memory, InteractionVector, Response);
    
    // Decode response
    f32 EmotionalResponse[8];
    char NPCMessage[256];
    DecodeResponse(Response, DNC->OutputSize, EmotionalResponse, NPCMessage);
    
    printf("%s: %s\n", NPC->Name, NPCMessage);
    
    // Update NPC mood based on interaction
    for(u32 i = 0; i < 8; ++i)
    {
        NPC->CurrentMood[i] = 0.9f * NPC->CurrentMood[i] + 0.1f * EmotionalResponse[i];
    }
    
    // Update interaction count
    NPC->TotalInteractions++;
    NPC->LastInteractionTime = (f64)ReadCPUTimer() / ReadCPUFrequency();
    
    // Show emotional state
    printf("Emotional state: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.2f ", NPC->CurrentMood[i]);
    }
    printf("...]\n");
}

// Demonstrate memory persistence
void DemonstrateMemoryPersistence(village_npc *NPC, dnc_system *DNC)
{
    printf("\n=== Testing Memory Persistence ===\n");
    
    // First meeting
    SimulateInteraction(NPC, DNC, INTERACTION_GREETING,
                       "Hello! I'm a traveler looking for quests.");
    
    // Give a quest
    SimulateInteraction(NPC, DNC, INTERACTION_QUEST_GIVE,
                       "Do you have any quests for me?");
    
    // Simulate time passing (would normally save/load here)
    printf("\n[Time passes... NPC memory persists]\n");
    
    // Return after quest
    SimulateInteraction(NPC, DNC, INTERACTION_QUEST_COMPLETE,
                       "I've completed your quest!");
    
    // Test if NPC remembers
    SimulateInteraction(NPC, DNC, INTERACTION_GREETING,
                       "Do you remember me?");
    
    // Show memory analysis
    memory_analysis Analysis = AnalyzeMemory(DNC);
    printf("\n=== Memory Analysis ===\n");
    printf("Memory Usage: %.1f%%\n", Analysis.AverageUsage * 100);
    printf("Memory Fragmentation: %.3f\n", Analysis.FragmentationScore);
    printf("Most Active Memory Slot: %u\n", Analysis.MostAccessedSlot);
}

// Demonstrate content-based retrieval
void DemonstrateContentRetrieval(village_npc *NPC, dnc_system *DNC)
{
    printf("\n=== Testing Content-Based Memory Retrieval ===\n");
    
    // Store several related memories
    const char *QuestConversations[] = {
        "I need someone to clear the goblin cave.",
        "The goblins have stolen our supplies.",
        "Please bring back our stolen goods from the goblins.",
        "Have you dealt with the goblin problem yet?"
    };
    
    // Have multiple quest-related conversations
    for(u32 i = 0; i < 4; ++i)
    {
        SimulateInteraction(NPC, DNC, INTERACTION_QUEST_GIVE,
                           QuestConversations[i]);
    }
    
    // Test retrieval with similar content
    printf("\n[Testing memory retrieval with similar content]\n");
    SimulateInteraction(NPC, DNC, INTERACTION_NEUTRAL,
                       "Tell me about the goblins again.");
    
    // The DNC should retrieve relevant memories about goblins
    // and generate an appropriate response
}

// Demonstrate temporal linking
void DemonstrateTemporalLinking(village_npc *NPC, dnc_system *DNC)
{
    printf("\n=== Testing Temporal Memory Linking ===\n");
    
    // Create a sequence of related events
    SimulateInteraction(NPC, DNC, INTERACTION_GREETING,
                       "I'm new in town.");
    
    SimulateInteraction(NPC, DNC, INTERACTION_FRIENDSHIP,
                       "Would you like to be friends?");
    
    SimulateInteraction(NPC, DNC, INTERACTION_TRADE,
                       "Can we trade items?");
    
    SimulateInteraction(NPC, DNC, INTERACTION_QUEST_GIVE,
                       "Since we're friends, I have a special quest for you.");
    
    // Test temporal recall
    printf("\n[Testing temporal sequence recall]\n");
    SimulateInteraction(NPC, DNC, INTERACTION_NEUTRAL,
                       "How did we become friends again?");
    
    // The DNC should be able to follow the temporal links
    // and recall the sequence of interactions
}

// Main example
internal void RunDNCExample(memory_arena *Arena)
{
    printf("===========================================\n");
    printf("    DNC Example: NPCs with True Memory    \n");
    printf("===========================================\n\n");
    
    // Create DNC system
    // Configuration for NPC memory:
    // - 64-dimensional input (interaction encoding)
    // - 128 hidden units in LSTM controller
    // - 2 read heads (for parallel memory access)
    // - 128 memory locations (long-term storage)
    // - 64-dimensional memory vectors
    
    printf("Creating DNC system...\n");
    dnc_system *DNC = CreateDNCSystem(Arena,
                                     64,    // Input size
                                     128,   // Controller hidden size
                                     2,     // Number of read heads
                                     128,   // Memory locations
                                     64);   // Memory vector size
    
    printf("DNC Configuration:\n");
    printf("  Memory: %u locations × %u dimensions\n", 
           DNC->MemoryLocations, DNC->MemoryVectorSize);
    printf("  Read Heads: %u\n", DNC->NumReadHeads);
    printf("  Controller: %u hidden units\n", DNC->ControllerHiddenSize);
    printf("  Total Output: %u dimensions\n\n", DNC->OutputSize);
    
    // Create village NPCs
    village_npc *Blacksmith = (village_npc *)PushSize_(Arena, sizeof(village_npc));
    strcpy(Blacksmith->Name, "Thorin the Blacksmith");
    Blacksmith->NPCId = 100;
    Blacksmith->Memory = CreateNPCWithDNC(Arena, Blacksmith->Name, DNC);
    
    // Set personality
    Blacksmith->Personality.Friendliness = 0.6f;
    Blacksmith->Personality.Curiosity = 0.4f;
    Blacksmith->Personality.Generosity = 0.7f;
    Blacksmith->Personality.Patience = 0.8f;
    Blacksmith->Personality.Courage = 0.9f;
    
    // Initialize mood
    for(u32 i = 0; i < 8; ++i)
    {
        Blacksmith->CurrentMood[i] = 0.5f; // Neutral
    }
    
    // Run demonstrations
    DemonstrateMemoryPersistence(Blacksmith, DNC);
    DemonstrateContentRetrieval(Blacksmith, DNC);
    DemonstrateTemporalLinking(Blacksmith, DNC);
    
    // Show final statistics
    #if HANDMADE_DEBUG
    PrintDNCStats(DNC);
    #endif
    
    // Performance report
    printf("\n=== Performance Report ===\n");
    f64 Frequency = (f64)ReadCPUFrequency();
    f64 TotalTimeMs = 1000.0 * DNC->TotalCycles / Frequency;
    f64 ControllerTimeMs = 1000.0 * DNC->ControllerCycles / Frequency;
    f64 MemoryTimeMs = 1000.0 * DNC->MemoryAccessCycles / Frequency;
    
    printf("Total Time: %.3f ms\n", TotalTimeMs);
    printf("Controller Time: %.3f ms (%.1f%%)\n", 
           ControllerTimeMs, 100.0 * ControllerTimeMs / TotalTimeMs);
    printf("Memory Access Time: %.3f ms (%.1f%%)\n",
           MemoryTimeMs, 100.0 * MemoryTimeMs / TotalTimeMs);
    printf("Average Step Time: %.3f ms\n", TotalTimeMs / DNC->StepCount);
    
    // Memory bandwidth estimate
    u64 BytesPerStep = DNC->MemoryLocations * DNC->MemoryVectorSize * sizeof(f32) * 2; // Read + Write
    f64 BandwidthGB = (f64)(BytesPerStep * DNC->StepCount) / (1024.0 * 1024.0 * 1024.0);
    printf("Memory Bandwidth Used: %.3f GB\n", BandwidthGB);
    
    printf("\n===========================================\n");
    printf("This is revolutionary! NPCs now have:\n");
    printf("- Persistent episodic memory\n");
    printf("- Content-based recall (remembers similar things)\n");
    printf("- Temporal understanding (remembers sequences)\n");
    printf("- Dynamic memory allocation (learns new things)\n");
    printf("- All running at %.1f ms per interaction!\n", TotalTimeMs / DNC->StepCount);
    printf("===========================================\n");
}

// Benchmark DNC operations
void BenchmarkDNC(memory_arena *Arena)
{
    printf("\n=== DNC Benchmark ===\n");
    
    // Create test DNC
    dnc_system *DNC = CreateDNCSystem(Arena, 32, 64, 2, 64, 32);
    
    // Prepare test input
    f32 *TestInput = (f32 *)PushSizeAligned(Arena, 32 * sizeof(f32), 64);
    f32 *TestOutput = (f32 *)PushSizeAligned(Arena, DNC->OutputSize * sizeof(f32), 64);
    for(u32 i = 0; i < 32; ++i)
    {
        TestInput[i] = (f32)rand() / RAND_MAX;
    }
    
    // Warmup
    for(u32 i = 0; i < 10; ++i)
    {
        DNCForward(DNC, TestInput, TestOutput);
    }
    
    // Benchmark
    u32 NumIterations = 100;
    u64 StartCycles = ReadCPUTimer();
    
    for(u32 i = 0; i < NumIterations; ++i)
    {
        DNCForward(DNC, TestInput, TestOutput);
    }
    
    u64 TotalCycles = ReadCPUTimer() - StartCycles;
    f64 TimeMs = 1000.0 * TotalCycles / (f64)ReadCPUFrequency();
    f64 TimePerStep = TimeMs / NumIterations;
    
    printf("Iterations: %u\n", NumIterations);
    printf("Total Time: %.3f ms\n", TimeMs);
    printf("Time per Step: %.3f ms\n", TimePerStep);
    printf("Throughput: %.1f steps/second\n", 1000.0 / TimePerStep);
    
    // Measure memory operations
    printf("\nMemory Operations:\n");
    printf("  Reads: %u\n", DNC->Memory.TotalReads);
    printf("  Writes: %u\n", DNC->Memory.TotalWrites);
    printf("  Cycles per Read: %lu\n", 
           DNC->Memory.AccessCycles / DNC->Memory.TotalReads);
    printf("  Cycles per Write: %lu\n",
           DNC->Memory.AccessCycles / DNC->Memory.TotalWrites);
}

// Entry point for standalone compilation
#if DNC_STANDALONE
int main(int argc, char **argv)
{
    // Initialize memory
    memory_arena Arena = {};
    Arena.Size = Megabytes(256);
    Arena.Base = malloc(Arena.Size);
    Arena.Used = 0;
    
    if(!Arena.Base)
    {
        fprintf(stderr, "Failed to allocate memory arena\n");
        return 1;
    }
    
    // Run example
    RunDNCExample(&Arena);
    
    // Run benchmark
    BenchmarkDNC(&Arena);
    
    // Cleanup
    free(Arena.Base);
    
    return 0;
}
#endif