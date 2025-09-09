#include "handmade.h"
#include "memory.h"
#include "neural_math.h"
#include "lstm.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
    Simple LSTM Test
    Demonstrates basic LSTM functionality without complex NPC systems
*/

void TestBasicLSTM(memory_arena *Arena)
{
    printf("=== Basic LSTM Test ===\n\n");
    
    // Create a simple LSTM cell
    u32 InputSize = 4;
    u32 HiddenSize = 8;
    
    lstm_cell Cell = CreateLSTMCell(Arena, InputSize, HiddenSize);
    printf("Created LSTM cell: %u inputs, %u hidden units\n", InputSize, HiddenSize);
    
    // Create state
    lstm_state State = {0};
    State.CellState = AllocateVector(Arena, HiddenSize);
    State.HiddenState = AllocateVector(Arena, HiddenSize);
    State.ForgetGate = AllocateVector(Arena, HiddenSize);
    State.InputGate = AllocateVector(Arena, HiddenSize);
    State.CandidateValues = AllocateVector(Arena, HiddenSize);
    State.OutputGate = AllocateVector(Arena, HiddenSize);
    State.ConcatenatedInput = PushArray(Arena, InputSize + HiddenSize, f32);
    
    // Initialize states to zero
    InitializeVectorZero(&State.CellState);
    InitializeVectorZero(&State.HiddenState);
    
    // Test inputs
    f32 Input1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    f32 Input2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    f32 Input3[] = {0.0f, 0.0f, 1.0f, 0.0f};
    f32 Output[8];
    
    printf("\nProcessing sequence:\n");
    
    // Process first input
    printf("Input 1: [1, 0, 0, 0]\n");
    LSTMCellForward(&Cell, &State, Input1, Output);
    printf("  Hidden state: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.3f%s", Output[i], i < 3 ? ", " : "");
    }
    printf(", ...]\n");
    printf("  Cell state magnitude: %.3f\n", State.CellState.Data[0]);
    
    // Process second input
    printf("\nInput 2: [0, 1, 0, 0]\n");
    LSTMCellForward(&Cell, &State, Input2, Output);
    printf("  Hidden state: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.3f%s", Output[i], i < 3 ? ", " : "");
    }
    printf(", ...]\n");
    printf("  Cell state magnitude: %.3f\n", State.CellState.Data[0]);
    
    // Process third input
    printf("\nInput 3: [0, 0, 1, 0]\n");
    LSTMCellForward(&Cell, &State, Input3, Output);
    printf("  Hidden state: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.3f%s", Output[i], i < 3 ? ", " : "");
    }
    printf(", ...]\n");
    printf("  Cell state magnitude: %.3f\n", State.CellState.Data[0]);
    
    // Show that it remembers the sequence
    printf("\nRepeating Input 1: [1, 0, 0, 0]\n");
    LSTMCellForward(&Cell, &State, Input1, Output);
    printf("  Hidden state: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.3f%s", Output[i], i < 3 ? ", " : "");
    }
    printf(", ...]\n");
    printf("  Note: Output is different due to memory of previous inputs\n");
    
    // Reset state and process again
    printf("\n=== After Reset ===\n");
    ResetLSTMState(&State);
    
    printf("Input 1 (after reset): [1, 0, 0, 0]\n");
    LSTMCellForward(&Cell, &State, Input1, Output);
    printf("  Hidden state: [");
    for(u32 i = 0; i < 4; ++i)
    {
        printf("%.3f%s", Output[i], i < 3 ? ", " : "");
    }
    printf(", ...]\n");
    printf("  Note: Same as first time - memory was cleared\n");
    
    // Performance stats
    if(Cell.ForwardCycles > 0)
    {
        printf("\n=== Performance ===\n");
        printf("Average cycles per forward pass: %.0f\n", 
               (f64)Cell.ForwardCycles / State.TimeStep);
        printf("Estimated latency: %.3f microseconds (at 2.4GHz)\n",
               (f64)Cell.ForwardCycles / State.TimeStep / 2400.0);
    }
}

void TestSequenceMemory(memory_arena *Arena)
{
    printf("\n=== Sequence Memory Test ===\n\n");
    
    // Create a small network
    u32 InputSize = 8;
    u32 HiddenSizes[] = {16};
    u32 NumLayers = 1;
    u32 OutputSize = 4;
    
    lstm_network Network = CreateLSTMNetwork(Arena, InputSize, HiddenSizes,
                                             NumLayers, OutputSize);
    
    printf("Created LSTM network: %u -> %u -> %u\n", 
           InputSize, HiddenSizes[0], OutputSize);
    
    // Create two different patterns
    f32 PatternA[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    f32 PatternB[8] = {0, 1, 0, 1, 0, 1, 0, 1};
    f32 Output[4];
    
    // Train on pattern A repeatedly
    printf("\nTraining on Pattern A (1010...):\n");
    for(u32 i = 0; i < 10; ++i)
    {
        LSTMNetworkForward(&Network, 0, PatternA, 1, Output);
        if(i % 3 == 0)
        {
            printf("  Iteration %2u: Output=[%.2f, %.2f, %.2f, %.2f]\n",
                   i, Output[0], Output[1], Output[2], Output[3]);
        }
    }
    
    // Now switch to pattern B
    printf("\nSwitching to Pattern B (0101...):\n");
    for(u32 i = 0; i < 5; ++i)
    {
        LSTMNetworkForward(&Network, 0, PatternB, 1, Output);
        printf("  Iteration %2u: Output=[%.2f, %.2f, %.2f, %.2f]\n",
               i, Output[0], Output[1], Output[2], Output[3]);
    }
    
    // Go back to pattern A - should produce different output due to memory
    printf("\nBack to Pattern A (with memory of B):\n");
    LSTMNetworkForward(&Network, 0, PatternA, 1, Output);
    printf("  Output=[%.2f, %.2f, %.2f, %.2f]\n",
           Output[0], Output[1], Output[2], Output[3]);
    printf("  Note: Different from initial Pattern A due to memory\n");
    
    // Test with a sequence
    printf("\n=== Processing Sequences ===\n");
    f32 Sequence[3 * 8];  // 3 timesteps
    for(u32 t = 0; t < 3; ++t)
    {
        for(u32 i = 0; i < 8; ++i)
        {
            // Create a changing pattern
            Sequence[t * 8 + i] = (f32)((t + i) % 2);
        }
    }
    
    printf("Processing 3-step sequence:\n");
    LSTMNetworkForward(&Network, 0, Sequence, 3, Output);
    printf("  Final output: [%.2f, %.2f, %.2f, %.2f]\n",
           Output[0], Output[1], Output[2], Output[3]);
    
    // Show network stats
    PrintLSTMStats(&Network);
}

void TestGateVisualization(memory_arena *Arena)
{
    printf("\n=== Gate Visualization ===\n\n");
    
    // Create LSTM with visible gate behavior
    u32 InputSize = 2;
    u32 HiddenSize = 4;
    
    lstm_cell Cell = CreateLSTMCell(Arena, InputSize, HiddenSize);
    
    lstm_state State = {0};
    State.CellState = AllocateVector(Arena, HiddenSize);
    State.HiddenState = AllocateVector(Arena, HiddenSize);
    State.ForgetGate = AllocateVector(Arena, HiddenSize);
    State.InputGate = AllocateVector(Arena, HiddenSize);
    State.CandidateValues = AllocateVector(Arena, HiddenSize);
    State.OutputGate = AllocateVector(Arena, HiddenSize);
    State.ConcatenatedInput = PushArray(Arena, InputSize + HiddenSize, f32);
    
    InitializeVectorZero(&State.CellState);
    InitializeVectorZero(&State.HiddenState);
    
    // Different input patterns to see gate behavior
    f32 Inputs[][2] = {
        {1.0f, 0.0f},   // Strong first signal
        {0.0f, 1.0f},   // Strong second signal
        {0.5f, 0.5f},   // Mixed signal
        {-1.0f, 0.0f},  // Negative signal
        {0.0f, 0.0f}    // No signal (test forgetting)
    };
    
    const char *InputNames[] = {
        "Strong A", "Strong B", "Mixed", "Negative", "Zero"
    };
    
    f32 Output[4];
    
    for(u32 step = 0; step < 5; ++step)
    {
        printf("Step %u: %s [%.1f, %.1f]\n", 
               step, InputNames[step], Inputs[step][0], Inputs[step][1]);
        
        LSTMCellForward(&Cell, &State, Inputs[step], Output);
        
        // Show gate activations
        printf("  Gates:\n");
        printf("    Forget: [");
        for(u32 i = 0; i < HiddenSize; ++i)
        {
            printf("%.2f%s", State.ForgetGate.Data[i], i < HiddenSize-1 ? ", " : "");
        }
        printf("]\n");
        
        printf("    Input:  [");
        for(u32 i = 0; i < HiddenSize; ++i)
        {
            printf("%.2f%s", State.InputGate.Data[i], i < HiddenSize-1 ? ", " : "");
        }
        printf("]\n");
        
        printf("    Output: [");
        for(u32 i = 0; i < HiddenSize; ++i)
        {
            printf("%.2f%s", State.OutputGate.Data[i], i < HiddenSize-1 ? ", " : "");
        }
        printf("]\n");
        
        printf("  Cell State: [");
        for(u32 i = 0; i < HiddenSize; ++i)
        {
            printf("%.2f%s", State.CellState.Data[i], i < HiddenSize-1 ? ", " : "");
        }
        printf("]\n\n");
    }
    
    printf("Gate behavior analysis:\n");
    printf("- Forget gate controls how much previous state to keep\n");
    printf("- Input gate controls how much new information to store\n");
    printf("- Output gate controls what to output based on cell state\n");
    printf("- Cell state accumulates information over time\n");
}

int main(int argc, char **argv)
{
    printf("LSTM Simple Test Program\n");
    printf("========================\n\n");
    
    // Initialize
    srand(time(NULL));
    
    // Create memory arena (16 MB should be plenty)
    memory_index ArenaSize = Megabytes(16);
    void *ArenaMemory = malloc(ArenaSize);
    
    if(!ArenaMemory)
    {
        printf("Failed to allocate memory!\n");
        return 1;
    }
    
    memory_arena Arena = {0};
    InitializeArena(&Arena, ArenaSize, ArenaMemory);
    
    // Run tests
    TestBasicLSTM(&Arena);
    TestSequenceMemory(&Arena);
    TestGateVisualization(&Arena);
    
    // Final stats
    printf("\n=== Final Statistics ===\n");
    printf("Memory used: %.2f KB / %.2f MB (%.1f%%)\n",
           (f32)Arena.Used / 1024.0f,
           (f32)Arena.Size / (1024.0f * 1024.0f),
           100.0f * (f32)Arena.Used / (f32)Arena.Size);
    
    free(ArenaMemory);
    return 0;
}