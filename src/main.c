#include "handmade.h"
#include "memory.h"
#include "neural_debug.h"
#include <math.h>

/*
    Handmade Neural Engine
    
    Architecture:
    - Direct neural computation with zero allocations in hot paths
    - SIMD-accelerated matrix operations
    - Cache-aware data layouts
    - Deterministic execution for replay debugging
    
    Memory Layout:
    [Permanent Storage]
        - Neural network weights (aligned to cache lines)
        - Network architecture metadata
        - Fixed entity storage
    [Transient Storage]
        - Frame scratch memory
        - Activation buffers
        - Gradient buffers for training
*/

// Forward declarations
int LinuxMain(int ArgumentCount, char **Arguments);

// Simple neural network layer structure for main.c
typedef struct neural_layer
{
    i32 InputSize;
    i32 OutputSize;
    f32 *Weights;      // [OutputSize][InputSize] - row-major for cache efficiency
    f32 *Biases;       // [OutputSize]
    f32 *Activations;  // [OutputSize] - reused across forward passes
    f32 *Gradients;    // [OutputSize] - for backprop
} neural_layer;

// Complete neural network for main.c
typedef struct neural_network
{
    i32 NumLayers;
    neural_layer *Layers;
    
    // Memory pools for activations
    memory_arena *ActivationArena;
    
    // Performance counters
    u64 ForwardPassCycles;
    u64 BackwardPassCycles;
    u32 InferenceCount;
} neural_network;

// ReLU activation with SIMD
internal void
ReLU_SIMD(f32 *Values, i32 Count)
{
    // PERFORMANCE: AVX2 vectorized - 8 floats per iteration
    // CACHE: Sequential access pattern
    
#if defined(__AVX2__)
    __m256 Zero = _mm256_setzero_ps();
    
    i32 SimdCount = Count & ~7;  // Round down to multiple of 8
    for(i32 i = 0; i < SimdCount; i += 8)
    {
        __m256 Val = _mm256_loadu_ps(Values + i);
        __m256 Result = _mm256_max_ps(Val, Zero);
        _mm256_storeu_ps(Values + i, Result);
    }
    
    // Handle remaining elements
    for(i32 i = SimdCount; i < Count; ++i)
    {
        Values[i] = Values[i] > 0 ? Values[i] : 0;
    }
#else
    // Scalar fallback
    for(i32 i = 0; i < Count; ++i)
    {
        Values[i] = Values[i] > 0 ? Values[i] : 0;
    }
#endif
}

// Matrix-vector multiply with cache tiling
internal void
MatrixVectorMultiply_Tiled(f32 *Output, f32 *Matrix, f32 *Input,
                           i32 Rows, i32 Cols)
{
    // PERFORMANCE: Cache-tiled for L1 cache
    // CACHE: Tile size chosen for 32KB L1 cache
    // Each tile processes 64 rows x 64 columns
    
    const i32 TileSize = 64;
    
    // Zero output
    for(i32 i = 0; i < Rows; ++i)
    {
        Output[i] = 0;
    }
    
    // Process tiles
    for(i32 RowTile = 0; RowTile < Rows; RowTile += TileSize)
    {
        i32 RowTileEnd = Minimum(RowTile + TileSize, Rows);
        
        for(i32 ColTile = 0; ColTile < Cols; ColTile += TileSize)
        {
            i32 ColTileEnd = Minimum(ColTile + TileSize, Cols);
            
            // Process tile with SIMD
            for(i32 Row = RowTile; Row < RowTileEnd; ++Row)
            {
#if defined(__AVX2__)
                __m256 Sum = _mm256_setzero_ps();
                i32 Col;
                
                // SIMD loop - 8 elements at a time
                for(Col = ColTile; Col < ColTileEnd - 7; Col += 8)
                {
                    __m256 MatrixVals = _mm256_loadu_ps(&Matrix[Row * Cols + Col]);
                    __m256 InputVals = _mm256_loadu_ps(&Input[Col]);
                    Sum = _mm256_fmadd_ps(MatrixVals, InputVals, Sum);
                }
                
                // Horizontal sum
                __m128 Sum128 = _mm_add_ps(_mm256_extractf128_ps(Sum, 0),
                                          _mm256_extractf128_ps(Sum, 1));
                Sum128 = _mm_hadd_ps(Sum128, Sum128);
                Sum128 = _mm_hadd_ps(Sum128, Sum128);
                Output[Row] += _mm_cvtss_f32(Sum128);
                
                // Handle remaining elements
                for(; Col < ColTileEnd; ++Col)
                {
                    Output[Row] += Matrix[Row * Cols + Col] * Input[Col];
                }
#else
                // Scalar fallback
                for(i32 Col = ColTile; Col < ColTileEnd; ++Col)
                {
                    Output[Row] += Matrix[Row * Cols + Col] * Input[Col];
                }
#endif
            }
        }
    }
}

// Forward pass through a layer
internal void
ForwardLayer(neural_layer *Layer, f32 *Input)
{
    u64 StartCycles = ReadCPUTimer();
    
    // Matrix multiply: Output = Weights * Input + Bias
    MatrixVectorMultiply_Tiled(Layer->Activations, Layer->Weights, Input,
                               Layer->OutputSize, Layer->InputSize);
    
    // Add biases with SIMD
#if defined(__AVX2__)
    for(i32 i = 0; i < Layer->OutputSize - 7; i += 8)
    {
        __m256 Act = _mm256_loadu_ps(&Layer->Activations[i]);
        __m256 Bias = _mm256_loadu_ps(&Layer->Biases[i]);
        Act = _mm256_add_ps(Act, Bias);
        _mm256_storeu_ps(&Layer->Activations[i], Act);
    }
    // Handle remainder
    for(i32 i = (Layer->OutputSize & ~7); i < Layer->OutputSize; ++i)
    {
        Layer->Activations[i] += Layer->Biases[i];
    }
#else
    for(i32 i = 0; i < Layer->OutputSize; ++i)
    {
        Layer->Activations[i] += Layer->Biases[i];
    }
#endif
    
    // Apply activation function
    ReLU_SIMD(Layer->Activations, Layer->OutputSize);
    
    u64 EndCycles = ReadCPUTimer();
    // Track cycles for performance monitoring
    (void)(EndCycles - StartCycles);
}

// Initialize neural network using the unified neural_math structures
internal neural_network *
CreateNeuralNetwork(memory_arena *Arena, i32 *LayerSizes, i32 LayerCount)
{
    neural_network *Network = PushStruct(Arena, neural_network);
    Network->NumLayers = LayerCount - 1;  // Input layer doesn't count
    Network->Layers = PushArray(Arena, Network->NumLayers, neural_layer);
    
    // Create each layer
    for(i32 i = 0; i < Network->NumLayers; ++i)
    {
        neural_layer *Layer = &Network->Layers[i];
        Layer->InputSize = LayerSizes[i];
        Layer->OutputSize = LayerSizes[i + 1];
        
        // Allocate weight matrix (cache-line aligned)
        i32 WeightCount = Layer->InputSize * Layer->OutputSize;
        Layer->Weights = (f32 *)PushSizeAligned(Arena, WeightCount * sizeof(f32), CACHE_LINE_SIZE);
        
        // Initialize weights with small random values
        for(i32 j = 0; j < WeightCount; ++j)
        {
            Layer->Weights[j] = ((f32)(j % 100) - 50.0f) * 0.01f;
        }
        
        // Allocate biases
        Layer->Biases = (f32 *)PushSizeAligned(Arena, Layer->OutputSize * sizeof(f32), CACHE_LINE_SIZE);
        ZeroSize(Layer->OutputSize * sizeof(f32), Layer->Biases);
        
        // Allocate activation buffers
        Layer->Activations = (f32 *)PushSizeAligned(Arena, Layer->OutputSize * sizeof(f32), CACHE_LINE_SIZE);
        Layer->Gradients = (f32 *)PushSizeAligned(Arena, Layer->OutputSize * sizeof(f32), CACHE_LINE_SIZE);
    }
    
    return Network;
}

// Neural network visualization
internal void
VisualizeNeuralNetwork(game_offscreen_buffer *Buffer, neural_network *Network, 
                       i32 StartX, i32 StartY, i32 NetworkWidth, i32 NetworkHeight)
{
    // PERFORMANCE: Simple visualization - O(n) in neuron count
    // MEMORY: No allocations - direct pixel manipulation
    
    if(!Network || Network->NumLayers == 0) return;
    
    i32 LayerSpacing = NetworkWidth / (Network->NumLayers + 1);
    
    // Draw each layer
    for(i32 LayerIndex = 0; LayerIndex < Network->NumLayers; ++LayerIndex)
    {
        neural_layer *Layer = &Network->Layers[LayerIndex];
        i32 LayerX = StartX + (LayerIndex + 1) * LayerSpacing;
        
        // Calculate neuron positions
        i32 NeuronCount = Layer->OutputSize;
        i32 MaxNeuronsToShow = 32;  // Limit for visualization
        i32 NeuronsToShow = (NeuronCount > MaxNeuronsToShow) ? MaxNeuronsToShow : NeuronCount;
        
        i32 NeuronSpacing = (NeuronsToShow > 1) ? NetworkHeight / (NeuronsToShow - 1) : NetworkHeight / 2;
        
        // Draw neurons as rectangles colored by activation level
        for(i32 NeuronIndex = 0; NeuronIndex < NeuronsToShow; ++NeuronIndex)
        {
            i32 ActualNeuronIndex = (NeuronIndex * NeuronCount) / NeuronsToShow;
            f32 Activation = Layer->Activations[ActualNeuronIndex];
            
            // Map activation to color intensity (0.0 = black, 1.0+ = white)
            f32 Intensity = Activation;
            if(Intensity < 0) Intensity = 0;
            if(Intensity > 1) Intensity = 1;
            
            u8 ColorValue = (u8)(255 * Intensity);
            u32 Color = RGB(ColorValue, ColorValue, ColorValue);
            
            i32 NeuronY = StartY + NeuronIndex * NeuronSpacing;
            DrawRectangle(Buffer, LayerX - 4, NeuronY - 4, 8, 8, Color);
            
            // Draw border
            if(ColorValue > 128)
            {
                DrawRectangle(Buffer, LayerX - 5, NeuronY - 5, 10, 1, COLOR_BLACK);
                DrawRectangle(Buffer, LayerX - 5, NeuronY + 4, 10, 1, COLOR_BLACK);
                DrawRectangle(Buffer, LayerX - 5, NeuronY - 4, 1, 8, COLOR_BLACK);
                DrawRectangle(Buffer, LayerX + 4, NeuronY - 4, 1, 8, COLOR_BLACK);
            }
            else
            {
                DrawRectangle(Buffer, LayerX - 5, NeuronY - 5, 10, 1, COLOR_WHITE);
                DrawRectangle(Buffer, LayerX - 5, NeuronY + 4, 10, 1, COLOR_WHITE);
                DrawRectangle(Buffer, LayerX - 5, NeuronY - 4, 1, 8, COLOR_WHITE);
                DrawRectangle(Buffer, LayerX + 4, NeuronY - 4, 1, 8, COLOR_WHITE);
            }
        }
    }
}

// Performance visualization
internal void
DrawPerformanceStats(game_offscreen_buffer *Buffer, neural_network *Network, f32 FrameTime)
{
    // Draw simple bars showing performance
    i32 BarHeight = 10;
    i32 BarWidth = 200;
    i32 StartX = 20;
    i32 StartY = Buffer->Height - 60;
    
    // Frame time bar (target: 16.67ms for 60fps)
    f32 FrameTimeRatio = FrameTime / 16.67f;
    if(FrameTimeRatio > 1.0f) FrameTimeRatio = 1.0f;
    
    u32 FrameColor = (FrameTimeRatio > 0.8f) ? COLOR_RED : COLOR_GREEN;
    i32 FrameBarWidth = (i32)(BarWidth * FrameTimeRatio);
    
    DrawRectangle(Buffer, StartX, StartY, BarWidth, BarHeight, COLOR_DARK_GRAY);
    DrawRectangle(Buffer, StartX, StartY, FrameBarWidth, BarHeight, FrameColor);
    
    // Neural inference count (if available)
    if(Network && Network->InferenceCount > 0)
    {
        // Show inference frequency as a simple indicator
        u32 InferenceColor = (Network->InferenceCount % 10 == 0) ? COLOR_YELLOW : COLOR_BLUE;
        DrawRectangle(Buffer, StartX, StartY + 15, 5, 5, InferenceColor);
    }
}

// Main game update function
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // PERFORMANCE: This is called 60 times per second
    // Must complete in < 16ms
    
    // Parameters are already defined by the macro
    
    // Set up memory arenas on first call
    local_persist b32 Initialized = 0;
    local_persist memory_arena PermanentArena;
    local_persist memory_arena TransientArena;
    local_persist neural_network *Network;
    local_persist neural_debug_state *DebugState;
    
    if(!Initialized)
    {
        InitializeArena(&PermanentArena, Memory->PermanentStorageSize, Memory->PermanentStorage);
        InitializeArena(&TransientArena, Memory->TransientStorageSize, Memory->TransientStorage);
        
        // Create a simple 3-layer network: 784 -> 128 -> 10 (MNIST-like)
        i32 LayerSizes[] = {784, 128, 10};
        Network = CreateNeuralNetwork(&PermanentArena, LayerSizes, ArrayCount(LayerSizes));
        
        // Initialize neural debug system
        DebugState = InitializeNeuralDebugSystem(&PermanentArena, 
                                               DEBUG_MAX_NEURONS, 
                                               DEBUG_HISTORY_SIZE);
        
        Initialized = 1;
    }
    
    // Example: Run inference if space is pressed
    controller_input *Keyboard = &Input->Controllers[0];
    if(Keyboard->ActionUp.EndedDown && Keyboard->ActionUp.HalfTransitionCount > 0)
    {
        // Create dummy input
        temporary_memory TempMem = BeginTemporaryMemory(&TransientArena);
        f32 *TestInput = PushArray(&TransientArena, 784, f32);
        
        // Initialize with test pattern
        for(i32 i = 0; i < 784; ++i)
        {
            TestInput[i] = (f32)(i % 10) * 0.1f;
        }
        
        // Run forward pass
        u64 StartCycles = ReadCPUTimer();
        
        f32 *CurrentInput = TestInput;
        for(i32 i = 0; i < Network->NumLayers; ++i)
        {
            ForwardLayer(&Network->Layers[i], CurrentInput);
            CurrentInput = Network->Layers[i].Activations;
        }
        
        u64 EndCycles = ReadCPUTimer();
        Network->ForwardPassCycles = EndCycles - StartCycles;
        Network->InferenceCount++;
        
        // Output would be in Network->Layers[Network->NumLayers-1].Activations
        
        EndTemporaryMemory(TempMem);
    }
    
    // UPDATE DEBUG SYSTEM: Process input and update state  
    UpdateNeuralDebug(DebugState, Input, Clock->SecondsElapsed);
    
    // RENDERING: Clear screen and draw visualization
    ClearBuffer(Buffer, COLOR_BLACK);
    
    // Neural debug visualization takes priority if enabled
    if (DebugState->DebugEnabled)
    {
        // Clear mouse hover state at start of frame
        DebugState->Mouse.IsHovering = 0;
        
        // Render appropriate debug visualization based on current mode
        switch (DebugState->CurrentMode)
        {
            case DEBUG_VIZ_NEURAL_ACTIVATIONS:
            {
                RenderNeuralActivations(DebugState, Buffer, Network);
            } break;
            
            case DEBUG_VIZ_WEIGHT_HEATMAP:
            {
                RenderWeightHeatmap(DebugState, Buffer, Network);
            } break;
            
            case DEBUG_VIZ_DNC_MEMORY:
            {
                // Note: Would need actual DNC system here
                RenderDNCMemoryMatrix(DebugState, Buffer, NULL);
            } break;
            
            case DEBUG_VIZ_LSTM_GATES:
            {
                // Note: Would need actual LSTM system here
                RenderLSTMGateStates(DebugState, Buffer, NULL);
            } break;
            
            case DEBUG_VIZ_EWC_FISHER:
            {
                RenderEWCFisherInfo(DebugState, Buffer, NULL);
            } break;
            
            case DEBUG_VIZ_NPC_BRAIN:
            {
                // Note: Would need actual NPC context here
                RenderNPCBrainActivity(DebugState, Buffer, NULL);
            } break;
            
            default:
            {
                // Default to showing basic neural network
                VisualizeNeuralNetwork(Buffer, Network, 50, 50, Buffer->Width - 100, Buffer->Height - 150);
            } break;
        }
        
        // Always render debug UI overlay
        RenderNeuralDebug(DebugState, Buffer);
    }
    else
    {
        // Standard visualization when debug is disabled
        VisualizeNeuralNetwork(Buffer, Network, 50, 50, Buffer->Width - 100, Buffer->Height - 150);
        
        // Draw performance statistics
        DrawPerformanceStats(Buffer, Network, Clock->SecondsElapsed * 1000.0f);
        
        // Draw simple animation to show the engine is running
        local_persist f32 AnimTime = 0;
        AnimTime += Clock->SecondsElapsed;
        
        i32 AnimX = (i32)(Buffer->Width / 2 + 100 * cosf(AnimTime * 2.0f));
        i32 AnimY = (i32)(Buffer->Height / 2 + 50 * sinf(AnimTime * 3.0f));
        
        DrawRectangle(Buffer, AnimX - 5, AnimY - 5, 10, 10, COLOR_CYAN);
        
        // Show basic instructions
        DrawRectangle(Buffer, 10, 10, 200, 20, COLOR_DARK_GRAY);
        DrawRectangle(Buffer, 12, 12, 196, 16, COLOR_WHITE);
        
        // Simple instruction text (normally would use proper text rendering)
        DrawRectangle(Buffer, 15, 15, 3, 10, COLOR_BLACK);  // "Press ~ for debug"
        DrawRectangle(Buffer, 25, 15, 8, 3, COLOR_BLACK);
    }
    
    // Performance statistics
    if(Network->InferenceCount > 0 && (Network->InferenceCount % 60) == 0)
    {
        f64 AvgCycles = (f64)Network->ForwardPassCycles;
        f64 AvgMS = AvgCycles / (2.4 * 1000000.0);  // Assume 2.4GHz CPU
        
        // Would normally output to debug console
        (void)AvgMS;
    }
}

// Main entry point
int main(int ArgumentCount, char **Arguments)
{
    return LinuxMain(ArgumentCount, Arguments);
}