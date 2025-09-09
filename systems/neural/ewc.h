#ifndef EWC_H
#define EWC_H

#include "handmade.h"
#include "neural_math.h"
#include "memory.h"
#include <immintrin.h>

/*
    Elastic Weight Consolidation (EWC) Implementation
    
    Prevents catastrophic forgetting in neural networks by:
    1. Computing Fisher Information Matrix for parameter importance
    2. Storing optimal weights from previous tasks
    3. Adding quadratic penalty to prevent important weight changes
    4. Supporting continual learning across multiple tasks
    
    Mathematical Foundation:
    
    EWC Loss = Standard_Loss + λ * Σ(F_i * (θ_i - θ*_i)²)
    
    Where:
    - F_i: Fisher Information for parameter i
    - θ_i: Current parameter value
    - θ*_i: Optimal parameter from previous task
    - λ: Consolidation strength (adaptive)
    
    Memory Layout:
    - Fisher Information stored as sparse matrices
    - Previous task weights cached for penalty computation
    - Efficient diagonal approximation for large networks
    - SIMD-optimized penalty calculations
    
    Performance Targets:
    - Fisher computation: <5ms for 100k parameters
    - EWC penalty: <1ms per forward pass
    - Memory overhead: <2x parameter count
    - SIMD utilization: >80% for large networks
*/

// Configuration constants
#define EWC_USE_SPARSE_FISHER 1         // Use sparse representation for Fisher Information
#define EWC_USE_DIAGONAL_APPROXIMATION 1 // Use diagonal Fisher approximation
#define EWC_FISHER_SAMPLES 1000         // Number of samples for Fisher computation
#define EWC_MIN_FISHER_VALUE 1e-8f      // Minimum Fisher value (numerical stability)
#define EWC_MAX_TASKS 16                // Maximum concurrent tasks to remember
#define EWC_ADAPTIVE_LAMBDA 1           // Enable adaptive lambda adjustment

// Forward declarations
typedef struct ewc_state ewc_state;
typedef struct ewc_task ewc_task;
typedef struct ewc_fisher_matrix ewc_fisher_matrix;

// Sparse Fisher Information Matrix
// MEMORY: Stores only non-zero entries to reduce memory usage
// CACHE: Optimized for sequential access during penalty computation
typedef struct ewc_fisher_entry
{
    u32 ParameterIndex;    // Which parameter this entry corresponds to
    f32 FisherValue;       // Fisher Information value
} ewc_fisher_entry;

typedef struct ewc_fisher_matrix
{
    ewc_fisher_entry *Entries;    // Sparse entries array
    u32 EntryCount;               // Number of non-zero entries
    u32 MaxEntries;               // Allocated capacity
    u32 TotalParameters;          // Total parameter count in network
    f32 SparsityRatio;            // Fraction of parameters with non-zero Fisher
    
    // Performance counters
    u64 ComputationCycles;        // Cycles spent computing this matrix
    u64 SampleCount;              // Number of samples used
    
    memory_arena *Arena;          // Memory arena for allocations
} ewc_fisher_matrix;

// Task-specific EWC data
// MEMORY: Stores weights and Fisher info for one completed task
typedef struct ewc_task
{
    char Name[64];                     // Human-readable task name
    u32 TaskID;                        // Unique task identifier
    b32 IsActive;                      // Whether this task data is valid
    
    // Optimal weights from this task (θ*)
    f32 *OptimalWeights;               // Flattened parameter vector
    u32 ParameterCount;                // Total parameters
    
    // Fisher Information Matrix
    ewc_fisher_matrix FisherMatrix;
    
    // Task-specific statistics
    f32 FinalLoss;                     // Loss when task was completed
    f32 TaskImportance;                // Relative importance of this task
    u32 TrainingEpochs;                // Epochs spent on this task
    
    // Timing information
    u64 CreationTimestamp;             // When task was consolidated
    u64 LastAccessTimestamp;           // Last time penalty was computed
} ewc_task;

// Main EWC state management
// PERFORMANCE: Hot data first, cold data at end
typedef struct ewc_state
{
    // Active tasks (most frequently accessed)
    ewc_task Tasks[EWC_MAX_TASKS];
    u32 ActiveTaskCount;
    u32 CurrentTaskID;
    
    // Global consolidation parameters
    f32 Lambda;                        // Consolidation strength
    f32 MinLambda;                     // Minimum lambda value
    f32 MaxLambda;                     // Maximum lambda value
    f32 LambdaDecay;                   // Lambda decay rate over time
    
    // Network parameter management
    f32 *CurrentWeights;               // Current network weights (reference)
    u32 TotalParameters;               // Total parameter count
    
    // Temporary computation buffers
    f32 *TempGradients;                // Buffer for Fisher computation
    f32 *TempParameters;               // Buffer for parameter backup
    neural_vector TempOutput;          // Buffer for forward pass outputs
    
    // Performance optimization
    b32 UseSIMD;                       // Enable SIMD optimizations
    b32 UseSparseFisher;               // Enable sparse Fisher matrices
    f32 SparsityThreshold;             // Threshold for Fisher sparsity
    
    // Statistics and monitoring
    u64 TotalPenaltyComputations;      // Number of penalty computations
    u64 TotalFisherComputations;       // Number of Fisher computations
    u64 TotalCycles;                   // Total cycles spent in EWC
    f64 AverageSparsity;               // Average Fisher matrix sparsity
    
    // Memory management
    memory_arena *Arena;               // Main memory arena
    memory_pool *TaskPool;             // Pool for task allocations
    memory_pool *FisherPool;           // Pool for Fisher matrix allocations
} ewc_state;

// Initialization and cleanup
ewc_state InitializeEWC(memory_arena *Arena, u32 TotalParameters);
void DestroyEWC(ewc_state *EWC);
void ResetEWC(ewc_state *EWC);

// Task management
u32 BeginTask(ewc_state *EWC, const char *TaskName);
void CompleteTask(ewc_state *EWC, u32 TaskID, neural_network *Network, f32 FinalLoss);
void SetTaskImportance(ewc_state *EWC, u32 TaskID, f32 Importance);
b32 HasTask(ewc_state *EWC, u32 TaskID);

// Fisher Information Matrix computation
void ComputeFisherInformation_Scalar(ewc_fisher_matrix *Fisher, 
                                   neural_network *Network,
                                   neural_vector *Samples, u32 SampleCount);
void ComputeFisherInformation_AVX2(ewc_fisher_matrix *Fisher,
                                 neural_network *Network, 
                                 neural_vector *Samples, u32 SampleCount);

// PERFORMANCE: Hot path - called every training step
// CACHE: Optimized for sequential parameter access
// SIMD: Vectorized penalty computation
f32 ComputeEWCPenalty_Scalar(ewc_state *EWC, neural_network *Network);
f32 ComputeEWCPenalty_AVX2(ewc_state *EWC, neural_network *Network);

// Parameter update with EWC regularization
void UpdateParametersWithEWC(ewc_state *EWC, neural_network *Network,
                            neural_vector *Gradients, f32 LearningRate);

// Adaptive lambda management
void UpdateLambda(ewc_state *EWC, f32 CurrentLoss, f32 ValidationLoss);
void SetLambdaRange(ewc_state *EWC, f32 MinLambda, f32 MaxLambda);
f32 GetRecommendedLambda(ewc_state *EWC, neural_network *Network);

// Memory optimization
void CompressFisherMatrix(ewc_fisher_matrix *Fisher, f32 SparsityThreshold);
void PruneInactiveTasks(ewc_state *EWC, u32 MaxTasks);
u32 GetMemoryUsage(ewc_state *EWC);

// Persistence and serialization
typedef struct ewc_save_data
{
    u32 Version;
    u32 TaskCount;
    u32 ParameterCount;
    f32 Lambda;
    // Followed by task data...
} ewc_save_data;

b32 SaveEWCState(ewc_state *EWC, const char *Filename);
b32 LoadEWCState(ewc_state *EWC, const char *Filename);
void SerializeTask(ewc_task *Task, u8 *Buffer, u32 *Offset);
void DeserializeTask(ewc_task *Task, u8 *Buffer, u32 *Offset, memory_arena *Arena);

// Integration with neural architectures
void IntegrateWithNetwork(ewc_state *EWC, neural_network *Network);
// TODO: Enable when LSTM/DNC structures are available
// void IntegrateWithLSTM(ewc_state *EWC, struct lstm_layer *LSTM);
// void IntegrateWithDNC(ewc_state *EWC, struct dnc_controller *DNC);

// Performance monitoring and debugging
typedef struct ewc_performance_stats
{
    // Timing statistics
    u64 FisherComputationCycles;
    u64 PenaltyComputationCycles;
    u64 UpdateCycles;
    
    // Memory statistics  
    u32 TotalMemoryUsed;
    u32 FisherMemoryUsed;
    u32 TaskMemoryUsed;
    f32 AverageSparsity;
    
    // Computational statistics
    u32 NonZeroFisherEntries;
    u32 TotalFisherEntries;
    f32 GFLOPS;
    f32 MemoryBandwidth;
} ewc_performance_stats;

void GetEWCStats(ewc_state *EWC, ewc_performance_stats *Stats);
void PrintEWCStats(ewc_performance_stats *Stats);
void BenchmarkEWC(memory_arena *Arena);

// Utility functions
inline f32 ComputeFisherDiagonal(f32 *Gradients, u32 Count)
{
    // PERFORMANCE: Diagonal Fisher approximation
    // F_ii ≈ E[∂log P(y|x,θ)/∂θ_i]²
    f32 Sum = 0.0f;
    for (u32 i = 0; i < Count; ++i) {
        Sum += Gradients[i] * Gradients[i];
    }
    return Sum / Count;
}

inline b32 ShouldComputeFisher(ewc_state *EWC, u32 TaskID)
{
    // Heuristic: Compute Fisher when task loss has stabilized
    ewc_task *Task = &EWC->Tasks[TaskID];
    return (Task->TrainingEpochs > 10) && (Task->FinalLoss > 0.0f);
}

inline f32 AdaptiveLambda(f32 TaskImportance, f32 ForgettingRate)
{
    // Adaptive lambda based on task importance and forgetting
    return TaskImportance * (1.0f + ForgettingRate);
}

// SIMD utility functions
inline __m256 LoadAligned8(f32 *Ptr)
{
    return _mm256_load_ps(Ptr);
}

inline void StoreAligned8(f32 *Ptr, __m256 Value)
{
    _mm256_store_ps(Ptr, Value);
}

inline __m256 FusedMultiplyAdd(__m256 A, __m256 B, __m256 C)
{
    return _mm256_fmadd_ps(A, B, C);
}

// Debug utilities
#if HANDMADE_DEBUG
void ValidateEWCState(ewc_state *EWC);
void ValidateFisherMatrix(ewc_fisher_matrix *Fisher);
void PrintTaskInfo(ewc_task *Task);
void CheckFisherNaN(ewc_fisher_matrix *Fisher);
void VerifyEWCGradients(ewc_state *EWC, neural_network *Network);
#else
#define ValidateEWCState(EWC)
#define ValidateFisherMatrix(Fisher)
#define PrintTaskInfo(Task)
#define CheckFisherNaN(Fisher)
#define VerifyEWCGradients(EWC, Network)
#endif

// Architecture-specific function selection
#if NEURAL_USE_AVX2
    #define ComputeFisherInformation ComputeFisherInformation_AVX2
    #define ComputeEWCPenalty ComputeEWCPenalty_AVX2
#else
    #define ComputeFisherInformation ComputeFisherInformation_Scalar
    #define ComputeEWCPenalty ComputeEWCPenalty_Scalar
#endif

#endif // EWC_H