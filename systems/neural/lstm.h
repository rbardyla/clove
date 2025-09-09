#ifndef LSTM_H
#define LSTM_H

#include "handmade.h"
#include "memory.h"
#include "neural_math.h"

/*
    LSTM (Long Short-Term Memory) Implementation
    
    High-performance temporal processing for NPCs
    Zero-dependency, cache-aware, SIMD-accelerated
    
    Architecture:
    - Forget gate: controls what to forget from previous cell state
    - Input gate: controls what new information to store
    - Candidate values: new candidate values for cell state
    - Output gate: controls what parts of cell state to output
    - Cell state: long-term memory
    - Hidden state: short-term memory/output
    
    Memory layout optimized for:
    - Sequential access patterns
    - SIMD vectorization (AVX2)
    - Cache line alignment
    - Minimal pointer chasing
    
    NPC Usage:
    - Each NPC has its own LSTM state
    - States are pooled for efficient memory usage
    - Support for stacked layers for complex behaviors
*/

// LSTM gate operations use tanh and sigmoid
// We pre-compute lookup tables for speed
#define LSTM_ACTIVATION_TABLE_SIZE 4096
#define LSTM_MAX_SEQUENCE_LENGTH 256
#define LSTM_MAX_LAYERS 4

// Forward declarations
typedef struct lstm_cell lstm_cell;
typedef struct lstm_layer lstm_layer;
typedef struct lstm_network lstm_network;
typedef struct lstm_state lstm_state;
typedef struct npc_memory_context npc_memory_context;

// LSTM cell parameters (shared across all instances)
typedef struct lstm_cell
{
    // Weight matrices for gates (all concatenated for cache efficiency)
    // Layout: [Wf, Wi, Wc, Wo] - forget, input, candidate, output
    // Each is (input_size + hidden_size) x hidden_size
    neural_matrix WeightsConcatenated;  // 4 * hidden_size x (input_size + hidden_size)
    
    // Bias vectors for gates
    neural_vector BiasForget;
    neural_vector BiasInput;
    neural_vector BiasCandidate;
    neural_vector BiasOutput;
    
    // Dimensions
    u32 InputSize;
    u32 HiddenSize;
    u32 ConcatSize;  // InputSize + HiddenSize
    
    // Performance counters
    u64 ForwardCycles;
    u64 GateComputeCycles;
    u64 StateUpdateCycles;
} lstm_cell;

// LSTM state for a single timestep (per NPC instance)
typedef struct lstm_state
{
    // Current states
    neural_vector CellState;      // Long-term memory
    neural_vector HiddenState;     // Short-term memory/output
    
    // Gate values (cached for backprop if needed)
    neural_vector ForgetGate;
    neural_vector InputGate;
    neural_vector CandidateValues;
    neural_vector OutputGate;
    
    // Concatenated input buffer for efficiency
    f32 *ConcatenatedInput;  // [input; hidden_prev]
    
    // Timestamp for temporal tracking
    u32 TimeStep;
    
    // NPC association
    u32 NPCId;
    
    // Memory pool reference
    memory_pool *StatePool;
} lstm_state;

// LSTM layer (can stack multiple)
typedef struct lstm_layer
{
    lstm_cell Cell;
    
    // State management for multiple NPCs
    lstm_state *States;        // Array of states
    u32 MaxNPCs;
    u32 ActiveNPCs;
    
    // Sequence buffer for batch processing
    f32 *SequenceBuffer;       // For processing sequences
    u32 MaxSequenceLength;
    
    // Layer configuration
    u32 LayerIndex;
    b32 ReturnSequences;       // Return full sequence or just last output
    f32 DropoutRate;           // For regularization
    
    // Memory pools
    memory_pool *WeightPool;
    memory_pool *StatePool;
    memory_pool *BufferPool;
} lstm_layer;

// Stacked LSTM network
typedef struct lstm_network
{
    lstm_layer Layers[LSTM_MAX_LAYERS];
    u32 NumLayers;
    
    // Network configuration
    u32 InputSize;
    u32 OutputSize;
    u32 *HiddenSizes;  // Size for each layer
    
    // Shared memory arena
    memory_arena *Arena;
    
    // Global performance stats
    u64 TotalForwardPasses;
    u64 TotalCycles;
    f64 AverageLatencyMs;
} lstm_network;

// NPC memory context for game integration
typedef struct npc_memory_context
{
    u32 NPCId;
    char Name[64];
    
    // LSTM states for this NPC
    lstm_state *CurrentState;
    lstm_state *StateHistory;  // Circular buffer of past states
    u32 HistorySize;
    u32 HistoryIndex;
    
    // Interaction memory
    f32 *LastInteractionEmbedding;
    u32 InteractionCount;
    f64 LastInteractionTime;
    
    // Emotional state (output from LSTM)
    f32 EmotionalVector[8];  // Joy, sadness, anger, fear, etc.
    
    // Behavioral modifiers
    f32 Personality[16];     // Static personality traits
    f32 Mood[8];            // Dynamic mood state
    
    // Memory importance scores (for forgetting)
    f32 *ImportanceScores;
    u32 MemoryCapacity;
} npc_memory_context;

// Initialization functions
lstm_cell CreateLSTMCell(memory_arena *Arena, u32 InputSize, u32 HiddenSize);
lstm_layer CreateLSTMLayer(memory_arena *Arena, u32 InputSize, u32 HiddenSize, u32 MaxNPCs);
lstm_network CreateLSTMNetwork(memory_arena *Arena, u32 InputSize, u32 *HiddenSizes, 
                               u32 NumLayers, u32 OutputSize);

// State management
lstm_state *AllocateLSTMState(memory_pool *Pool, u32 HiddenSize);
void InitializeLSTMState(lstm_state *State, u32 HiddenSize);
void ResetLSTMState(lstm_state *State);
void FreeLSTMState(memory_pool *Pool, lstm_state *State);

// Forward pass operations (scalar)
void LSTMCellForward_Scalar(lstm_cell *Cell, lstm_state *State, 
                            f32 *Input, f32 *Output);
void LSTMLayerForward_Scalar(lstm_layer *Layer, u32 NPCId,
                             f32 *Input, u32 SequenceLength, f32 *Output);

// Forward pass operations (SIMD optimized)
void LSTMCellForward_AVX2(lstm_cell *Cell, lstm_state *State,
                          f32 *Input, f32 *Output);
void LSTMLayerForward_AVX2(lstm_layer *Layer, u32 NPCId,
                           f32 *Input, u32 SequenceLength, f32 *Output);

// Gate computations (SIMD optimized)
void ComputeGates_AVX2(lstm_cell *Cell, f32 *ConcatenatedInput,
                       f32 *ForgetGate, f32 *InputGate, 
                       f32 *CandidateValues, f32 *OutputGate);

// State updates (SIMD optimized)
void UpdateCellState_AVX2(f32 *CellState, f32 *PrevCellState,
                         f32 *ForgetGate, f32 *InputGate,
                         f32 *CandidateValues, u32 HiddenSize);

// Network operations
void LSTMNetworkForward(lstm_network *Network, u32 NPCId,
                        f32 *Input, u32 SequenceLength, f32 *Output);
void ProcessNPCBatch(lstm_network *Network, u32 *NPCIds, u32 BatchSize,
                     f32 **Inputs, u32 *SequenceLengths, f32 **Outputs);

// NPC memory management
npc_memory_context *CreateNPCMemory(memory_arena *Arena, u32 NPCId, const char *Name);
void UpdateNPCMemory(npc_memory_context *NPC, lstm_network *Network,
                    f32 *InteractionData, u32 DataLength);
void GetNPCEmotionalState(npc_memory_context *NPC, f32 *EmotionalOutput);
void SaveNPCMemory(npc_memory_context *NPC, const char *Filepath);
void LoadNPCMemory(npc_memory_context *NPC, const char *Filepath);

// Memory pooling for multiple NPCs
typedef struct npc_memory_pool
{
    npc_memory_context *NPCs;
    u32 MaxNPCs;
    u32 ActiveNPCs;
    
    // Shared LSTM network
    lstm_network *Network;
    
    // Memory statistics
    memory_index TotalMemoryUsed;
    memory_index MemoryPerNPC;
} npc_memory_pool;

npc_memory_pool *CreateNPCMemoryPool(memory_arena *Arena, u32 MaxNPCs,
                                     lstm_network *Network);
npc_memory_context *AllocateNPC(npc_memory_pool *Pool, const char *Name);
void FreeNPC(npc_memory_pool *Pool, u32 NPCId);

// Utility functions
void PrintLSTMStats(lstm_network *Network);
void BenchmarkLSTM(memory_arena *Arena);
void ValidateLSTMGradients(lstm_cell *Cell, f32 *Input, f32 *Target);

// Helper function for absolute value
inline f32 LSTMAbs(f32 x)
{
    return x < 0 ? -x : x;
}

// Fast activation functions optimized for LSTM
inline f32 FastSigmoid(f32 x)
{
    // PERFORMANCE: Rational approximation, ~4x faster than 1/(1+exp(-x))
    // Good accuracy for x in [-5, 5]
    f32 abs_x = LSTMAbs(x);
    if(abs_x < 0.0001f) return 0.5f;  // Avoid division by near-zero
    return 0.5f + 0.5f * x / (1.0f + abs_x);
}

inline void FastSigmoid_AVX2(f32 *Output, f32 *Input, u32 Size)
{
    // PERFORMANCE: Vectorized sigmoid for gate computations
    // Processes 8 floats per iteration
    __m256 half = _mm256_set1_ps(0.5f);
    __m256 one = _mm256_set1_ps(1.0f);
    
    u32 i = 0;
    for(; i + 7 < Size; i += 8)
    {
        __m256 x = _mm256_load_ps(Input + i);
        __m256 abs_x = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), x);
        __m256 denom = _mm256_add_ps(one, abs_x);
        __m256 ratio = _mm256_div_ps(x, denom);
        __m256 result = _mm256_fmadd_ps(half, ratio, half);
        _mm256_store_ps(Output + i, result);
    }
    
    // Handle remaining elements
    for(; i < Size; ++i)
    {
        Output[i] = FastSigmoid(Input[i]);
    }
}

// Architecture selection
#if NEURAL_USE_AVX2
    #define LSTMCellForward LSTMCellForward_AVX2
    #define LSTMLayerForward LSTMLayerForward_AVX2
#else
    #define LSTMCellForward LSTMCellForward_Scalar
    #define LSTMLayerForward LSTMLayerForward_Scalar
#endif

// Performance macros
#define LSTM_PROFILE_START() u64 StartCycles = ReadCPUTimer()
#define LSTM_PROFILE_END(Counter) Counter += ReadCPUTimer() - StartCycles

// Debug utilities
#if HANDMADE_DEBUG
void ValidateLSTMState(lstm_state *State);
void PrintLSTMGates(lstm_state *State);
void CheckLSTMNaN(lstm_cell *Cell, lstm_state *State);
#else
#define ValidateLSTMState(State)
#define PrintLSTMGates(State)
#define CheckLSTMNaN(Cell, State)
#endif

#endif // LSTM_H