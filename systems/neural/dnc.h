#ifndef DNC_H
#define DNC_H

#include "handmade.h"
#include "memory.h"
#include "neural_math.h"
#include "lstm.h"

/*
    Differentiable Neural Computer (DNC)
    
    Revolutionary memory-augmented neural architecture for NPCs
    Based on DeepMind's "Hybrid computing using a neural network with dynamic external memory"
    Nature 538, 471-476 (2016)
    
    Key Innovation: NPCs with TRUE PERSISTENT MEMORY
    - Content-based addressing (find memories by similarity)
    - Sequential memory linkage (temporal relationships)
    - Dynamic memory allocation and deallocation
    - Multiple read heads for parallel memory access
    
    Architecture Components:
    1. Controller: LSTM that generates interface vectors
    2. Memory Matrix: N×M external memory (N locations, M-dimensional vectors)
    3. Read Heads: R heads that retrieve information from memory
    4. Write Head: Single head that writes to memory
    5. Temporal Linkage: Tracks order of memory writes
    6. Usage Tracking: Monitors which memory slots are in use
    
    Memory Access Mechanisms:
    - Content-based: Find memories similar to a query (cosine similarity)
    - Location-based: Access specific memory addresses
    - Temporal: Follow links between sequentially written memories
    
    Performance Optimizations:
    - SIMD cosine similarity computation (AVX2)
    - Sparse temporal link matrix operations
    - Cache-friendly memory layout
    - Memory pooling for multiple NPCs
    
    NPC Applications:
    - Long-term relationship tracking
    - Quest state persistence
    - Learning from past interactions
    - Emotional memory formation
    - Social network understanding
*/

// Configuration
#define DNC_MAX_MEMORY_LOCATIONS 256    // N: number of memory slots
#define DNC_MEMORY_VECTOR_SIZE 64        // M: size of each memory vector
#define DNC_MAX_READ_HEADS 4             // R: number of read heads
#define DNC_CONTROLLER_OUTPUT_SIZE 256   // LSTM controller output size

// Memory access modes
typedef enum dnc_addressing_mode
{
    DNC_CONTENT_ADDRESSING,    // Find by similarity
    DNC_LOCATION_ADDRESSING,    // Direct address access
    DNC_TEMPORAL_ADDRESSING     // Follow temporal links
} dnc_addressing_mode;

// Memory matrix: N × M
typedef struct dnc_memory
{
    // MEMORY LAYOUT: Row-major, cache-aligned
    // Each row is a memory vector, padded to cache line
    f32 *Matrix;                         // N × M memory matrix
    u32 NumLocations;                    // N
    u32 VectorSize;                      // M
    u32 Stride;                          // Padded width for SIMD
    
    // Memory statistics
    u32 TotalWrites;
    u32 TotalReads;
    u64 AccessCycles;
} dnc_memory;

// Usage tracking: monitors which memory slots are free/used
typedef struct dnc_usage
{
    f32 *UsageVector;                    // N-dimensional [0,1]
    f32 *RetentionVector;                // N-dimensional, how much to retain
    u32 *FreeList;                       // Sorted indices of free locations
    u32 NumFree;
    
    // PERFORMANCE: Cache frequently accessed slots
    u32 MostUsedSlots[16];
    u32 LeastUsedSlots[16];
} dnc_usage;

// Temporal linkage: tracks sequential write order
typedef struct dnc_temporal_linkage
{
    f32 *LinkMatrix;                     // N × N sparse matrix
    f32 *PrecedenceWeighting;            // N-dimensional, last write location
    u32 *WriteOrder;                     // Circular buffer of write history
    u32 WriteIndex;
    u32 MaxHistory;
    
    // PERFORMANCE: Sparse matrix optimization
    u32 *NonZeroIndices;                // Indices of non-zero entries
    u32 NumNonZero;
} dnc_temporal_linkage;

// Read head: retrieves information from memory
typedef struct dnc_read_head
{
    // Attention weights
    f32 *ContentWeighting;               // N-dimensional, content-based attention
    f32 *LocationWeighting;              // N-dimensional, final read weights
    
    // Read vectors
    f32 *ReadVector;                     // M-dimensional, retrieved content
    
    // Addressing parameters (from controller)
    f32 *Key;                           // M-dimensional, what to look for
    f32 Beta;                           // Scalar, focus sharpness
    f32 *Gate;                          // 3-dimensional, addressing mode blend
    f32 *Shift;                         // 3-dimensional, shift weights
    
    // Head index
    u32 HeadIndex;
    
    // Performance counters
    u64 SimilarityCycles;
    u64 ReadCycles;
} dnc_read_head;

// Write head: stores information in memory
typedef struct dnc_write_head
{
    // Write weights
    f32 *ContentWeighting;               // N-dimensional, content-based
    f32 *AllocationWeighting;            // N-dimensional, free slot selection
    f32 *WriteWeighting;                 // N-dimensional, final write weights
    
    // Write vectors
    f32 *WriteVector;                    // M-dimensional, what to write
    f32 *EraseVector;                    // M-dimensional, what to erase [0,1]
    
    // Addressing parameters (from controller)
    f32 *Key;                           // M-dimensional, similarity key
    f32 Beta;                           // Scalar, focus sharpness
    f32 *Gate;                          // 2-dimensional, content vs allocation
    f32 WriteStrength;                  // Scalar, write gate
    
    // Performance counters
    u64 AllocationCycles;
    u64 WriteCycles;
} dnc_write_head;

// Interface between controller and memory
typedef struct dnc_interface
{
    // Read interface (per read head)
    f32 **ReadKeys;                     // R × M
    f32 *ReadStrengths;                 // R scalars
    f32 **ReadGates;                    // R × 3 (content, forward, backward)
    f32 **ReadShifts;                   // R × 3 (backward, none, forward)
    
    // Write interface
    f32 *WriteKey;                      // M-dimensional
    f32 WriteStrength;                  // Scalar
    f32 *WriteGate;                     // 2-dimensional (content, allocation)
    f32 *WriteVector;                   // M-dimensional
    f32 *EraseVector;                   // M-dimensional
    
    // Memory management
    f32 *FreeGates;                     // R-dimensional, read memory freeing
    
    // Parsed from controller output
    u32 TotalParameters;
} dnc_interface;

// Complete DNC system
typedef struct dnc_system
{
    // Core components
    dnc_memory Memory;
    dnc_usage Usage;
    dnc_temporal_linkage Linkage;
    dnc_read_head ReadHeads[DNC_MAX_READ_HEADS];
    dnc_write_head WriteHead;
    dnc_interface Interface;
    
    // Controller (LSTM)
    lstm_cell *Controller;
    lstm_state *ControllerState;
    
    // Configuration
    u32 NumReadHeads;
    u32 MemoryLocations;
    u32 MemoryVectorSize;
    u32 ControllerHiddenSize;
    
    // Combined output
    f32 *Output;                        // Controller output + read vectors
    u32 OutputSize;
    
    // Memory arena
    memory_arena *Arena;
    memory_pool *WorkingPool;
    
    // Performance statistics
    u64 TotalCycles;
    u64 ControllerCycles;
    u64 MemoryAccessCycles;
    u32 StepCount;
} dnc_system;

// NPC-specific DNC context
typedef struct npc_dnc_context
{
    dnc_system *DNC;
    u32 NPCId;
    char Name[64];
    
    // Persistent state across interactions
    dnc_memory SavedMemory;             // Snapshot of memory
    dnc_usage SavedUsage;
    dnc_temporal_linkage SavedLinkage;
    
    // Interaction history
    f32 **InteractionEmbeddings;        // Past interaction vectors
    u32 NumInteractions;
    u32 MaxInteractions;
    
    // Memory importance scoring
    f32 *ImportanceScores;              // Per memory slot
    f32 ImportanceThreshold;            // For garbage collection
    
    // Emotional associations
    f32 *EmotionalMemoryTags;           // N × 8 (emotion per memory)
    
    // Performance tracking
    f64 AverageResponseTime;
    u32 MemoryAccessCount;
} npc_dnc_context;

// Initialization
dnc_system *CreateDNCSystem(memory_arena *Arena, 
                           u32 InputSize,
                           u32 ControllerHiddenSize,
                           u32 NumReadHeads,
                           u32 MemoryLocations,
                           u32 MemoryVectorSize);
void InitializeDNCMemory(dnc_memory *Memory, memory_arena *Arena,
                        u32 NumLocations, u32 VectorSize);
void ResetDNCSystem(dnc_system *DNC);

// Forward pass
void DNCForward(dnc_system *DNC, f32 *Input, f32 *Output);
void DNCStep(dnc_system *DNC, f32 *Input);

// Memory operations
void ContentAddressing(f32 *Weights, dnc_memory *Memory, 
                      f32 *Key, f32 Beta, u32 NumLocations);
void AllocateMemory(f32 *AllocationWeights, dnc_usage *Usage, 
                   u32 NumLocations);
void WriteToMemory(dnc_memory *Memory, dnc_write_head *WriteHead,
                  f32 *WriteWeights);
void ReadFromMemory(f32 *ReadVector, dnc_memory *Memory,
                   f32 *ReadWeights, u32 VectorSize);
void UpdateUsage(dnc_usage *Usage, f32 *WriteWeights, 
                f32 *FreeGates, u32 NumLocations);
void UpdateTemporalLinkage(dnc_temporal_linkage *Linkage,
                          f32 *WriteWeights, u32 NumLocations);

// Interface parsing
void ParseInterface(dnc_interface *Interface, f32 *ControllerOutput,
                   u32 NumReadHeads, u32 MemoryVectorSize);

// Similarity computation (SIMD optimized)
f32 CosineSimilarity_Scalar(f32 *A, f32 *B, u32 Size);
f32 CosineSimilarity_AVX2(f32 *A, f32 *B, u32 Size);
void CosineSimilarityBatch_AVX2(f32 *Similarities, dnc_memory *Memory,
                                f32 *Key, u32 NumLocations);

// Addressing mechanisms
void ComputeContentWeights(f32 *ContentWeights, dnc_memory *Memory,
                          f32 *Key, f32 Beta);
void ComputeAllocationWeights(f32 *AllocationWeights, dnc_usage *Usage);
void ComputeTemporalWeights(f32 *TemporalWeights, 
                           dnc_temporal_linkage *Linkage,
                           f32 *PrevWeights, f32 *Direction);

// NPC integration
npc_dnc_context *CreateNPCWithDNC(memory_arena *Arena, const char *Name,
                                  dnc_system *SharedDNC);
void ProcessNPCInteraction(npc_dnc_context *NPC, f32 *Input, f32 *Response);
void SaveNPCMemoryState(npc_dnc_context *NPC, const char *Filepath);
void LoadNPCMemoryState(npc_dnc_context *NPC, const char *Filepath);
void ConsolidateNPCMemories(npc_dnc_context *NPC);

// Memory analysis
typedef struct memory_analysis
{
    f32 AverageUsage;
    f32 FragmentationScore;
    u32 MostAccessedSlot;
    u32 OldestMemorySlot;
    f32 TemporalCoherence;
    f32 ContentDiversity;
} memory_analysis;

memory_analysis AnalyzeMemory(dnc_system *DNC);
void VisualizeMemoryMatrix(dnc_memory *Memory, const char *Title);
void PrintMemorySlot(dnc_memory *Memory, u32 Slot);

// Utilities
void SoftmaxWeights(f32 *Weights, u32 Size);
void SharpnWeights(f32 *Weights, f32 Beta, u32 Size);
void NormalizeWeights(f32 *Weights, u32 Size);
f32 ComputeEntropy(f32 *Weights, u32 Size);

// Benchmarking
void BenchmarkDNC(memory_arena *Arena);
void BenchmarkCosineSimilarity(memory_arena *Arena);
void ProfileMemoryAccess(dnc_system *DNC, u32 NumSteps);

// Debugging
#if HANDMADE_DEBUG
void ValidateDNCState(dnc_system *DNC);
void CheckDNCGradients(dnc_system *DNC, f32 *Input, f32 *Target);
void PrintDNCStats(dnc_system *DNC);
void DumpMemoryContents(dnc_memory *Memory, const char *Filename);
#else
#define ValidateDNCState(DNC)
#define CheckDNCGradients(DNC, Input, Target)
#define PrintDNCStats(DNC)
#define DumpMemoryContents(Memory, Filename)
#endif

// Performance macros
#define DNC_PROFILE_START() u64 _DNCStart = ReadCPUTimer()
#define DNC_PROFILE_END(Counter) Counter += ReadCPUTimer() - _DNCStart

// Architecture selection
#if NEURAL_USE_AVX2
    #define CosineSimilarity CosineSimilarity_AVX2
#else
    #define CosineSimilarity CosineSimilarity_Scalar
#endif

// Memory layout helpers
inline u32 DNCMemoryIndex(u32 Location, u32 Component, u32 Stride)
{
    return Location * Stride + Component;
}

inline f32 *DNCMemoryVector(dnc_memory *Memory, u32 Location)
{
    return Memory->Matrix + Location * Memory->Stride;
}

// Fast operations
inline f32 DNCClamp(f32 Value, f32 Min, f32 Max)
{
    if(Value < Min) return Min;
    if(Value > Max) return Max;
    return Value;
}

inline f32 DNCSigmoid(f32 x)
{
    // Fast sigmoid approximation
    return 0.5f + 0.5f * x / (1.0f + (x < 0 ? -x : x));
}

inline f32 DNCOneMinusX(f32 x)
{
    return 1.0f - x;
}

#endif // DNC_H