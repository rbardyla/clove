#include "handmade.h"
#include "memory.h"
#include "neural_math.h"
#include "lstm.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/*
    LSTM Example: NPC Memory System
    
    Demonstrates:
    - Creating NPCs with persistent memory
    - Processing sequential interactions
    - Emotional state evolution
    - Memory pooling for multiple NPCs
    - Performance benchmarking
*/

// Interaction types for NPCs
typedef enum interaction_type
{
    INTERACTION_GREETING,
    INTERACTION_GIFT,
    INTERACTION_INSULT,
    INTERACTION_HELP,
    INTERACTION_TRADE,
    INTERACTION_QUEST,
    INTERACTION_FAREWELL,
    INTERACTION_COUNT
} interaction_type;

// Convert interaction to input vector
void EncodeInteraction(interaction_type Type, f32 Time, f32 Intensity, f32 *Output, u32 Size)
{
    // Zero the output
    for(u32 i = 0; i < Size; ++i)
    {
        Output[i] = 0.0f;
    }
    
    // One-hot encode interaction type
    if(Type < INTERACTION_COUNT && Type < Size)
    {
        Output[Type] = 1.0f;
    }
    
    // Add temporal features
    if(Size > INTERACTION_COUNT)
    {
        Output[INTERACTION_COUNT] = sinf(Time * 0.1f);      // Time of day
        Output[INTERACTION_COUNT + 1] = cosf(Time * 0.1f);
        Output[INTERACTION_COUNT + 2] = Intensity;          // Interaction intensity
    }
}

// Decode emotional state to readable format
void DecodeEmotionalState(f32 *EmotionalVector, u32 Size)
{
    const char *EmotionNames[] = {
        "Joy", "Sadness", "Anger", "Fear",
        "Trust", "Disgust", "Surprise", "Anticipation"
    };
    
    printf("Emotional State:\n");
    for(u32 i = 0; i < Size && i < 8; ++i)
    {
        f32 Value = EmotionalVector[i];
        printf("  %-12s: ", EmotionNames[i]);
        
        // Visual bar representation
        i32 BarLength = (i32)(fabsf(Value) * 20);
        if(Value < 0)
        {
            for(i32 j = 0; j < 20 - BarLength; ++j) printf(" ");
            for(i32 j = 0; j < BarLength; ++j) printf("-");
            printf("|");
        }
        else
        {
            printf("|");
            for(i32 j = 0; j < BarLength; ++j) printf("+");
        }
        printf(" %.2f\n", Value);
    }
}

// Simulate a conversation with an NPC
void SimulateConversation(npc_memory_context *NPC, lstm_network *Network)
{
    printf("\n=== Conversation with %s (NPC #%u) ===\n", NPC->Name, NPC->NPCId);
    
    // Sequence of interactions
    interaction_type Sequence[] = {
        INTERACTION_GREETING,
        INTERACTION_HELP,
        INTERACTION_GIFT,
        INTERACTION_TRADE,
        INTERACTION_INSULT,  // Negative interaction
        INTERACTION_HELP,     // Try to recover
        INTERACTION_FAREWELL
    };
    
    f32 Intensities[] = {0.5f, 0.7f, 0.9f, 0.3f, -0.8f, 0.6f, 0.4f};
    
    u32 InputSize = 16;  // Interaction encoding size
    u32 SequenceLength = sizeof(Sequence) / sizeof(Sequence[0]);
    
    // Process each interaction
    for(u32 i = 0; i < SequenceLength; ++i)
    {
        printf("\nInteraction %u: ", i + 1);
        switch(Sequence[i])
        {
            case INTERACTION_GREETING: printf("Greeting"); break;
            case INTERACTION_GIFT: printf("Giving gift"); break;
            case INTERACTION_INSULT: printf("Insult"); break;
            case INTERACTION_HELP: printf("Offering help"); break;
            case INTERACTION_TRADE: printf("Trading"); break;
            case INTERACTION_QUEST: printf("Quest discussion"); break;
            case INTERACTION_FAREWELL: printf("Farewell"); break;
            default: printf("Unknown"); break;
        }
        printf(" (intensity: %.1f)\n", Intensities[i]);
        
        // Encode interaction
        f32 *Input = (f32 *)alloca(InputSize * sizeof(f32));
        EncodeInteraction(Sequence[i], (f32)i, Intensities[i], Input, InputSize);
        
        // Process through LSTM
        UpdateNPCMemory(NPC, Network, Input, 1);
        
        // Show emotional response
        DecodeEmotionalState(NPC->EmotionalVector, 8);
        
        // Show how mood is evolving
        printf("Mood trend: ");
        f32 MoodSum = 0;
        for(u32 j = 0; j < 8; ++j)
        {
            MoodSum += NPC->Mood[j];
        }
        if(MoodSum > 0.5f) printf("Positive");
        else if(MoodSum < -0.5f) printf("Negative");
        else printf("Neutral");
        printf(" (%.2f)\n", MoodSum);
    }
    
    printf("\n=== Final NPC State ===\n");
    printf("Total interactions: %u\n", NPC->InteractionCount);
    printf("Personality stability: ");
    f32 PersonalityMagnitude = 0;
    for(u32 i = 0; i < 16; ++i)
    {
        PersonalityMagnitude += NPC->Personality[i] * NPC->Personality[i];
    }
    printf("%.2f\n", sqrtf(PersonalityMagnitude));
}

// Test multiple NPCs with shared memory
void TestMultipleNPCs(memory_arena *Arena)
{
    printf("\n=== Testing Multiple NPCs with LSTM Memory ===\n");
    
    // Network configuration
    u32 InputSize = 16;
    u32 HiddenSizes[] = {64, 32};
    u32 NumLayers = 2;
    u32 OutputSize = 8;  // 8 emotional dimensions
    
    // Create LSTM network
    lstm_network Network = CreateLSTMNetwork(Arena, InputSize, HiddenSizes,
                                             NumLayers, OutputSize);
    
    // Create NPC memory pool
    u32 MaxNPCs = 10;
    npc_memory_pool *Pool = CreateNPCMemoryPool(Arena, MaxNPCs, &Network);
    
    printf("Created NPC pool: %u NPCs, %.2f KB per NPC\n",
           MaxNPCs, (f32)Pool->MemoryPerNPC / 1024.0f);
    
    // Create several NPCs
    const char *NPCNames[] = {
        "Guard Captain",
        "Merchant",
        "Innkeeper",
        "Blacksmith",
        "Wizard"
    };
    
    npc_memory_context *NPCs[5];
    for(u32 i = 0; i < 5; ++i)
    {
        NPCs[i] = AllocateNPC(Pool, NPCNames[i]);
        printf("Created NPC: %s (ID: %u)\n", NPCs[i]->Name, NPCs[i]->NPCId);
    }
    
    // Simulate interactions with different NPCs
    for(u32 i = 0; i < 3; ++i)
    {
        SimulateConversation(NPCs[i], &Network);
    }
    
    // Show network statistics
    PrintLSTMStats(&Network);
}

// Benchmark sequence processing
void BenchmarkSequenceProcessing(memory_arena *Arena)
{
    printf("\n=== Benchmarking LSTM Sequence Processing ===\n");
    
    // Test different sequence lengths
    u32 SequenceLengths[] = {1, 5, 10, 20, 50, 100};
    u32 NumTests = sizeof(SequenceLengths) / sizeof(SequenceLengths[0]);
    
    // Create network
    u32 InputSize = 32;
    u32 HiddenSizes[] = {128, 64};
    u32 NumLayers = 2;
    u32 OutputSize = 16;
    
    lstm_network Network = CreateLSTMNetwork(Arena, InputSize, HiddenSizes,
                                             NumLayers, OutputSize);
    
    // Create test data
    f32 *TestInput = PushArray(Arena, LSTM_MAX_SEQUENCE_LENGTH * InputSize, f32);
    f32 *TestOutput = PushArray(Arena, OutputSize, f32);
    
    // Initialize with random data
    for(u32 i = 0; i < LSTM_MAX_SEQUENCE_LENGTH * InputSize; ++i)
    {
        TestInput[i] = ((f32)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
    
    printf("\nSequence Length | Time (ms) | Throughput (seq/s) | GFLOPS\n");
    printf("----------------|-----------|--------------------|---------\n");
    
    for(u32 test = 0; test < NumTests; ++test)
    {
        u32 SeqLen = SequenceLengths[test];
        
        // Warmup
        for(u32 i = 0; i < 100; ++i)
        {
            LSTMNetworkForward(&Network, 0, TestInput, SeqLen, TestOutput);
        }
        
        // Benchmark
        u32 NumIterations = 1000;
        u64 StartCycles = ReadCPUTimer();
        
        for(u32 i = 0; i < NumIterations; ++i)
        {
            LSTMNetworkForward(&Network, 0, TestInput, SeqLen, TestOutput);
        }
        
        u64 TotalCycles = ReadCPUTimer() - StartCycles;
        f64 CyclesPerSeq = (f64)TotalCycles / NumIterations;
        f64 TimeMs = CyclesPerSeq / 2.4e6;  // Assume 2.4GHz
        f64 SeqPerSecond = 1000.0 / TimeMs;
        
        // Estimate FLOPS (rough)
        u64 FlopsPerTimestep = 0;
        for(u32 l = 0; l < NumLayers; ++l)
        {
            u32 Hidden = HiddenSizes[l];
            u32 Input = (l == 0) ? InputSize : HiddenSizes[l-1];
            FlopsPerTimestep += 8 * Hidden * (Input + Hidden);  // 4 gates, 2 ops per MAC
        }
        u64 TotalFlops = FlopsPerTimestep * SeqLen;
        f64 GFLOPS = (f64)TotalFlops / CyclesPerSeq * 2.4;  // 2.4 GHz
        
        printf("%15u | %9.3f | %18.1f | %7.2f\n",
               SeqLen, TimeMs, SeqPerSecond, GFLOPS);
    }
}

// Test memory persistence
void TestMemoryPersistence(memory_arena *Arena)
{
    printf("\n=== Testing LSTM Memory Persistence ===\n");
    
    // Create simple network
    u32 InputSize = 8;
    u32 HiddenSizes[] = {32};
    u32 NumLayers = 1;
    u32 OutputSize = 4;
    
    lstm_network Network = CreateLSTMNetwork(Arena, InputSize, HiddenSizes,
                                             NumLayers, OutputSize);
    
    // Create NPC
    npc_memory_context *NPC = CreateNPCMemory(Arena, 0, "Memory Test NPC");
    
    // Pattern to remember
    f32 Pattern1[] = {1, 0, 1, 0, 1, 0, 1, 0};
    f32 Pattern2[] = {0, 1, 0, 1, 0, 1, 0, 1};
    f32 Output[4];
    
    printf("Teaching patterns to NPC...\n");
    
    // Teach pattern 1 multiple times
    for(u32 i = 0; i < 5; ++i)
    {
        UpdateNPCMemory(NPC, &Network, Pattern1, 1);
        printf("  Pattern 1 iteration %u: State magnitude = %.2f\n", 
               i + 1, NPC->EmotionalVector[0]);
    }
    
    // Teach pattern 2
    for(u32 i = 0; i < 5; ++i)
    {
        UpdateNPCMemory(NPC, &Network, Pattern2, 1);
        printf("  Pattern 2 iteration %u: State magnitude = %.2f\n",
               i + 1, NPC->EmotionalVector[0]);
    }
    
    // Test if it remembers pattern 1
    printf("\nTesting memory of Pattern 1:\n");
    UpdateNPCMemory(NPC, &Network, Pattern1, 1);
    printf("  Response: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.2f%s", NPC->EmotionalVector[i], i < 3 ? ", " : "");
    }
    printf("]\n");
    
    // Reset and test again
    printf("\nResetting LSTM state...\n");
    ResetLSTMState(&Network.Layers[0].States[0]);
    
    printf("After reset - testing Pattern 1:\n");
    UpdateNPCMemory(NPC, &Network, Pattern1, 1);
    printf("  Response: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.2f%s", NPC->EmotionalVector[i], i < 3 ? ", " : "");
    }
    printf("]\n");
    
    printf("\nNote: After reset, the response is different, showing memory was lost.\n");
}

int main(int argc, char **argv)
{
    printf("LSTM Neural Network Example\n");
    printf("===========================\n");
    
    // Initialize random seed
    srand(time(NULL));
    
    // Create main memory arena (64 MB)
    memory_index ArenaSize = Megabytes(64);
    void *ArenaMemory = malloc(ArenaSize);
    
    if(!ArenaMemory)
    {
        printf("Failed to allocate memory arena!\n");
        return 1;
    }
    
    memory_arena Arena = {0};
    InitializeArena(&Arena, ArenaSize, ArenaMemory);
    
    printf("Initialized memory arena: %.2f MB\n\n", (f32)ArenaSize / (1024.0f * 1024.0f));
    
    // Run tests
    if(argc > 1 && argv[1][0] == 'b')
    {
        // Benchmark mode
        printf("Running benchmarks...\n");
        BenchmarkLSTM(&Arena);
        BenchmarkSequenceProcessing(&Arena);
    }
    else if(argc > 1 && argv[1][0] == 'm')
    {
        // Memory persistence test
        TestMemoryPersistence(&Arena);
    }
    else
    {
        // Default: NPC interaction demo
        TestMultipleNPCs(&Arena);
    }
    
    // Print final memory usage
    printf("\n=== Memory Statistics ===\n");
    printf("Arena used: %.2f MB / %.2f MB (%.1f%%)\n",
           (f32)Arena.Used / (1024.0f * 1024.0f),
           (f32)Arena.Size / (1024.0f * 1024.0f),
           100.0f * (f32)Arena.Used / (f32)Arena.Size);
    
    free(ArenaMemory);
    return 0;
}