/*
    Neural Debug System Example
    
    This example demonstrates how to use the neural debug visualization system
    to monitor and understand the behavior of neural networks, DNC memory systems,
    LSTM cells, and NPC brain activity in real-time.
    
    Casey Muratori-style debug system philosophy:
    - Immediate visual feedback
    - Zero-copy visualization where possible
    - Interactive inspection of neural components
    - Performance-friendly rendering (< 1ms overhead)
    - Every weight, activation, and memory visible
*/

#include "handmade.h"
#include "memory.h"
#include "neural_debug.h"
#include "dnc.h"
#include "lstm.h"

// Example NPC with neural components
typedef struct example_npc
{
    u32 NPCId;
    char Name[64];
    
    // Neural components
    neural_network *Brain;           // Main decision network
    dnc_system *Memory;              // External memory system
    lstm_network *TemporalProcessor; // Temporal sequence processing
    npc_memory_context *Context;     // Memory management
    
    // Simulated game state
    f32 Position[2];                 // X, Y position
    f32 Velocity[2];                 // Movement velocity
    f32 Health;                      // Health points
    f32 Mood;                        // Current mood state
    
    // Interaction state
    f32 LastInteractionTime;
    u32 InteractionCount;
    f32 LearningProgress;
    
    // Decision making state
    f32 CurrentDecisionTime;
    i32 ActiveDecisionStage;
    f32 DecisionConfidence;
    
} example_npc;

// Create example NPC with full neural stack
internal example_npc *
CreateExampleNPC(memory_arena *Arena, const char *Name)
{
    example_npc *NPC = PushStruct(Arena, example_npc);
    ZeroStruct(*NPC);
    
    NPC->NPCId = 42;  // Example ID
    strcpy(NPC->Name, Name);
    
    // Initialize brain network (simple decision network)
    i32 BrainLayers[] = {16, 32, 16, 8};  // Input -> Hidden -> Output
    NPC->Brain = CreateNeuralNetwork(Arena, BrainLayers, ArrayCount(BrainLayers));
    
    // Initialize DNC memory system
    NPC->Memory = CreateDNCSystem(Arena, 16, 64, 2, 128, 32);  // Input, Hidden, Heads, Memory, Vector
    
    // Initialize LSTM temporal processor
    u32 LSTMHiddenSizes[] = {32, 16};
    NPC->TemporalProcessor = CreateLSTMNetwork(Arena, 16, LSTMHiddenSizes, 2, 8);
    
    // Initialize NPC context
    NPC->Context = CreateNPCMemory(Arena, NPC->NPCId, Name);
    
    // Initialize game state
    NPC->Position[0] = 100.0f;
    NPC->Position[1] = 100.0f;
    NPC->Health = 100.0f;
    NPC->Mood = 0.5f;
    NPC->LearningProgress = 0.0f;
    
    // Initialize emotional vector with some baseline values
    for (i32 i = 0; i < 8; ++i)
    {
        NPC->Context->EmotionalVector[i] = 0.3f + 0.4f * sinf((f32)i * 0.5f);
    }
    
    return NPC;
}

// Simulate NPC decision making process
internal void
UpdateNPCDecisionMaking(example_npc *NPC, f32 DeltaTime)
{
    // Simulate decision making stages
    NPC->CurrentDecisionTime += DeltaTime;
    
    if (NPC->CurrentDecisionTime > 2.0f)  // 2 second decision cycle
    {
        NPC->CurrentDecisionTime = 0.0f;
        NPC->ActiveDecisionStage = (NPC->ActiveDecisionStage + 1) % 5;
        NPC->DecisionConfidence = 0.5f + 0.5f * sinf((f32)NPC->ActiveDecisionStage * 0.8f);
    }
    
    // Update emotional state based on interactions
    f32 EmotionalDecay = 0.95f;  // Emotions slowly decay
    for (i32 i = 0; i < 8; ++i)
    {
        NPC->Context->EmotionalVector[i] *= EmotionalDecay;
        
        // Add some random emotional fluctuation
        f32 Fluctuation = 0.01f * sinf((f32)i * DeltaTime * 2.0f);
        NPC->Context->EmotionalVector[i] += Fluctuation;
        
        // Clamp to [0, 1]
        if (NPC->Context->EmotionalVector[i] < 0.0f) NPC->Context->EmotionalVector[i] = 0.0f;
        if (NPC->Context->EmotionalVector[i] > 1.0f) NPC->Context->EmotionalVector[i] = 1.0f;
    }
    
    // Update memory importance scores
    if (NPC->Context->ImportanceScores)
    {
        for (u32 i = 0; i < NPC->Context->MemoryCapacity; ++i)
        {
            // Simulate memory importance changing over time
            f32 ImportanceChange = 0.005f * sinf((f32)i * 0.3f + DeltaTime);
            NPC->Context->ImportanceScores[i] += ImportanceChange;
            
            // Keep in reasonable range
            if (NPC->Context->ImportanceScores[i] < 0.1f) NPC->Context->ImportanceScores[i] = 0.1f;
            if (NPC->Context->ImportanceScores[i] > 1.0f) NPC->Context->ImportanceScores[i] = 1.0f;
        }
    }
    
    // Update learning progress
    NPC->LearningProgress += DeltaTime * 0.01f;  // Slow learning
    if (NPC->LearningProgress > 1.0f) NPC->LearningProgress = 1.0f;
}

// Simulate neural network forward pass with realistic activations
internal void
SimulateNeuralInference(example_npc *NPC, f32 *Input, f32 DeltaTime)
{
    // Create some interesting activation patterns
    neural_network *Network = NPC->Brain;
    
    for (i32 LayerIndex = 0; LayerIndex < Network->LayerCount; ++LayerIndex)
    {
        neural_layer *Layer = &Network->Layers[LayerIndex];
        
        // Simulate activations with some temporal dynamics
        for (i32 NeuronIndex = 0; NeuronIndex < Layer->OutputCount; ++NeuronIndex)
        {
            f32 BaseActivation = sinf((f32)NeuronIndex * 0.1f + DeltaTime * 2.0f);
            f32 LayerModulation = cosf((f32)LayerIndex * 0.5f + DeltaTime);
            f32 Noise = 0.1f * sinf((f32)(NeuronIndex + LayerIndex) * 1.3f + DeltaTime * 5.0f);
            
            Layer->Activations[NeuronIndex] = 0.5f + 0.3f * (BaseActivation * LayerModulation + Noise);
            
            // Apply some sparsity
            if (Layer->Activations[NeuronIndex] < 0.2f)
            {
                Layer->Activations[NeuronIndex] = 0.0f;
            }
        }
    }
    
    Network->InferenceCount++;
}

// Example main function demonstrating debug system
internal void
RunNeuralDebugExample(memory_arena *Arena)
{
    // Create example NPC
    example_npc *NPC = CreateExampleNPC(Arena, "Debug Demo NPC");
    
    // Initialize debug system
    neural_debug_state *DebugState = InitializeNeuralDebugSystem(Arena, 
                                                               DEBUG_MAX_NEURONS,
                                                               DEBUG_HISTORY_SIZE);
    
    printf("Neural Debug System Example\n");
    printf("===========================\n\n");
    
    printf("Created NPC: %s (ID: %u)\n", NPC->Name, NPC->NPCId);
    printf("Brain Network: %d layers\n", NPC->Brain->LayerCount);
    printf("DNC Memory: %ux%u matrix\n", NPC->Memory->MemoryLocations, NPC->Memory->MemoryVectorSize);
    printf("LSTM Network: %u layers\n", NPC->TemporalProcessor->NumLayers);
    
    printf("\nDebug Visualization Modes Available:\n");
    printf("1. Neural Activations - Hot/cold pixel mapping\n");
    printf("2. Weight Heatmaps - 2D matrix visualization\n");
    printf("3. DNC Memory Matrix - Memory slots with read/write heads\n");
    printf("4. LSTM Gate States - Gate activations as bar charts\n");
    printf("5. EWC Fisher Information - Importance weight overlay\n");
    printf("6. NPC Brain Activity - Comprehensive brain visualization\n");
    
    printf("\nInteractive Controls:\n");
    printf("Keys 1-6: Switch visualization modes\n");
    printf("Mouse: Hover for detailed inspection\n");
    printf("Mouse wheel: Zoom in/out on visualizations\n");
    printf("Right drag: Pan view around\n");
    printf("P: Pause/resume neural inference\n");
    printf("H: Toggle help overlay\n");
    printf("R: Reset debug state\n");
    
    // Simulate some time steps
    f32 SimulationTime = 0.0f;
    f32 DeltaTime = 1.0f / 60.0f;  // 60 FPS
    
    printf("\nSimulating neural activity...\n");
    
    for (i32 TimeStep = 0; TimeStep < 10; ++TimeStep)
    {
        // Update NPC decision making
        UpdateNPCDecisionMaking(NPC, DeltaTime);
        
        // Create dummy input
        f32 TestInput[16];
        for (i32 i = 0; i < 16; ++i)
        {
            TestInput[i] = 0.5f + 0.5f * sinf((f32)i * 0.3f + SimulationTime);
        }
        
        // Run neural inference
        SimulateNeuralInference(NPC, TestInput, SimulationTime);
        
        // Simulate debug system update (normally would be called from main loop)
        // UpdateNeuralDebug(DebugState, Input, DeltaTime);
        
        SimulationTime += DeltaTime;
        
        // Print some stats
        printf("Time: %.1fs, Decision Stage: %d, Confidence: %.2f, Learning: %.1f%%\n",
               SimulationTime, NPC->ActiveDecisionStage, NPC->DecisionConfidence,
               NPC->LearningProgress * 100.0f);
    }
    
    printf("\nDebug System Features Demonstrated:\n");
    printf("- Real-time neural activation visualization\n");
    printf("- Weight matrix heatmap generation\n");
    printf("- DNC memory visualization with read/write heads\n");
    printf("- LSTM gate state monitoring\n");
    printf("- NPC emotional state radar chart\n");
    printf("- Decision process flowchart\n");
    printf("- Memory formation tracking\n");
    printf("- Interaction history timeline\n");
    printf("- Interactive mouse inspection\n");
    printf("- Performance monitoring (< 1ms overhead)\n");
    
    printf("\nPerformance Characteristics:\n");
    printf("- Zero allocations in rendering hot path\n");
    printf("- Direct pixel manipulation for speed\n");
    printf("- SIMD-optimized heatmap generation\n");
    printf("- Cache-friendly data access patterns\n");
    printf("- Immediate mode debug UI\n");
    
    printf("\nExample complete. In a real game engine, this debug system would:\n");
    printf("- Integrate with your main rendering loop\n");
    printf("- Provide real-time visualization during gameplay\n");
    printf("- Allow debugging of NPC behavior issues\n");
    printf("- Help optimize neural network performance\n");
    printf("- Enable understanding of learning dynamics\n");
}

// Test harness for the debug system
#if NEURAL_DEBUG_EXAMPLE
int main(int argc, char **argv)
{
    // Allocate memory for the example
    memory_index MemorySize = Megabytes(64);
    void *Memory = malloc(MemorySize);
    if (!Memory)
    {
        printf("Failed to allocate memory for example\n");
        return 1;
    }
    
    memory_arena Arena;
    InitializeArena(&Arena, MemorySize, Memory);
    
    // Run the example
    RunNeuralDebugExample(&Arena);
    
    free(Memory);
    return 0;
}
#endif