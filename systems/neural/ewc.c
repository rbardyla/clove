#include "ewc.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

/*
    Elastic Weight Consolidation Implementation
    
    Core principles:
    1. Fisher Information captures parameter importance
    2. Quadratic penalty preserves important weights
    3. Sparse representation minimizes memory overhead
    4. SIMD optimization for computational efficiency
    
    Performance characteristics:
    - Fisher computation: O(N * S) where N=parameters, S=samples
    - EWC penalty: O(K) where K=non-zero Fisher entries  
    - Memory usage: O(K + T*N) where T=tasks
    - Cache efficiency: Sequential access patterns
*/

// Global performance statistics
global_variable ewc_performance_stats GlobalEWCStats = {0};

// Note: Using memory functions from memory.h

// ================================================================================================
// Initialization and Memory Management
// ================================================================================================

ewc_state InitializeEWC(memory_arena *Arena, u32 TotalParameters)
{
    ewc_state EWC = {0};
    
    // Basic setup
    EWC.Arena = Arena;
    EWC.TotalParameters = TotalParameters;
    EWC.CurrentTaskID = 0;
    EWC.ActiveTaskCount = 0;
    
    // Initialize consolidation parameters
    EWC.Lambda = 400.0f;              // Standard EWC lambda value
    EWC.MinLambda = 1.0f;
    EWC.MaxLambda = 10000.0f;
    EWC.LambdaDecay = 0.99f;
    EWC.SparsityThreshold = EWC_MIN_FISHER_VALUE;
    
    // Performance settings
    EWC.UseSIMD = NEURAL_USE_AVX2;
    EWC.UseSparseFisher = EWC_USE_SPARSE_FISHER;
    
    // Allocate memory pools
    EWC.TaskPool = (memory_pool *)PushStruct(Arena, memory_pool);
    EWC.FisherPool = (memory_pool *)PushStruct(Arena, memory_pool);
    
    // Simple initialization without using InitializePool
    EWC.TaskPool->BlockSize = sizeof(ewc_task) + TotalParameters * sizeof(f32);
    EWC.TaskPool->BlockCount = EWC_MAX_TASKS;
    EWC.TaskPool->UsedCount = 0;
    EWC.TaskPool->Memory = (u8 *)PushSize(Arena, EWC.TaskPool->BlockSize * EWC.TaskPool->BlockCount);
    
    EWC.FisherPool->BlockSize = sizeof(ewc_fisher_entry);
    EWC.FisherPool->BlockCount = EWC_MAX_TASKS * TotalParameters;
    EWC.FisherPool->UsedCount = 0;
    EWC.FisherPool->Memory = (u8 *)PushSize(Arena, EWC.FisherPool->BlockSize * EWC.FisherPool->BlockCount);
    
    // Allocate temporary computation buffers
    // MEMORY: Aligned for SIMD operations
    EWC.TempGradients = (f32 *)PushArray(Arena, TotalParameters, f32);
    EWC.TempParameters = (f32 *)PushArray(Arena, TotalParameters, f32);
    EWC.TempOutput = AllocateVector(Arena, 1000); // Assume max 1000 output neurons
    
    // Initialize all tasks as inactive
    for (u32 TaskIndex = 0; TaskIndex < EWC_MAX_TASKS; ++TaskIndex) {
        EWC.Tasks[TaskIndex].IsActive = false;
        EWC.Tasks[TaskIndex].TaskID = (u32)-1;
    }
    
    ValidateEWCState(&EWC);
    return EWC;
}

void DestroyEWC(ewc_state *EWC)
{
    // Note: Memory is arena-allocated, so it will be freed when arena is destroyed
    // Just clear the structure
    memset(EWC, 0, sizeof(ewc_state));
}

void ResetEWC(ewc_state *EWC)
{
    // Reset all tasks but keep configuration
    f32 Lambda = EWC->Lambda;
    f32 MinLambda = EWC->MinLambda;
    f32 MaxLambda = EWC->MaxLambda;
    memory_arena *Arena = EWC->Arena;
    u32 TotalParameters = EWC->TotalParameters;
    
    // Clear tasks
    for (u32 TaskIndex = 0; TaskIndex < EWC_MAX_TASKS; ++TaskIndex) {
        EWC->Tasks[TaskIndex].IsActive = false;
    }
    
    EWC->ActiveTaskCount = 0;
    EWC->CurrentTaskID = 0;
    EWC->TotalPenaltyComputations = 0;
    EWC->TotalFisherComputations = 0;
    EWC->TotalCycles = 0;
    
    // Restore configuration
    EWC->Lambda = Lambda;
    EWC->MinLambda = MinLambda;
    EWC->MaxLambda = MaxLambda;
}

// ================================================================================================
// Task Management
// ================================================================================================

u32 BeginTask(ewc_state *EWC, const char *TaskName)
{
    assert(EWC->ActiveTaskCount < EWC_MAX_TASKS);
    
    u32 TaskID = EWC->CurrentTaskID++;
    ewc_task *Task = &EWC->Tasks[EWC->ActiveTaskCount++];
    
    // Initialize task
    memset(Task, 0, sizeof(ewc_task));
    strncpy(Task->Name, TaskName, sizeof(Task->Name) - 1);
    Task->TaskID = TaskID;
    Task->IsActive = true;
    Task->ParameterCount = EWC->TotalParameters;
    Task->TaskImportance = 1.0f;
    Task->CreationTimestamp = __rdtsc();
    
    // Allocate optimal weights storage
    Task->OptimalWeights = (f32 *)PushArray(EWC->Arena, EWC->TotalParameters, f32);
    
    // Initialize Fisher matrix
    ewc_fisher_matrix *Fisher = &Task->FisherMatrix;
    Fisher->TotalParameters = EWC->TotalParameters;
    Fisher->Arena = EWC->Arena;
    Fisher->MaxEntries = EWC->TotalParameters; // Start with full capacity
    Fisher->Entries = (ewc_fisher_entry *)PushArray(EWC->Arena, Fisher->MaxEntries, ewc_fisher_entry);
    Fisher->EntryCount = 0;
    
    return TaskID;
}

void CompleteTask(ewc_state *EWC, u32 TaskID, neural_network *Network, f32 FinalLoss)
{
    ewc_task *Task = 0;
    
    // Find the task
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        if (EWC->Tasks[TaskIndex].TaskID == TaskID) {
            Task = &EWC->Tasks[TaskIndex];
            break;
        }
    }
    
    assert(Task && "Task not found");
    assert(Task->IsActive && "Task not active");
    
    // Store optimal weights (flatten network parameters)
    u32 ParameterIndex = 0;
    
    // Layer 1: Weights and biases
    neural_matrix *W1 = &Network->Layer1.Weights;
    for (u32 Row = 0; Row < W1->Rows; ++Row) {
        for (u32 Col = 0; Col < W1->Cols; ++Col) {
            Task->OptimalWeights[ParameterIndex++] = W1->Data[Row * W1->Stride + Col];
        }
    }
    for (u32 i = 0; i < Network->Layer1.Bias.Size; ++i) {
        Task->OptimalWeights[ParameterIndex++] = Network->Layer1.Bias.Data[i];
    }
    
    // Layer 2: Weights and biases
    neural_matrix *W2 = &Network->Layer2.Weights;
    for (u32 Row = 0; Row < W2->Rows; ++Row) {
        for (u32 Col = 0; Col < W2->Cols; ++Col) {
            Task->OptimalWeights[ParameterIndex++] = W2->Data[Row * W2->Stride + Col];
        }
    }
    for (u32 i = 0; i < Network->Layer2.Bias.Size; ++i) {
        Task->OptimalWeights[ParameterIndex++] = Network->Layer2.Bias.Data[i];
    }
    
    // Layer 3: Weights and biases
    neural_matrix *W3 = &Network->Layer3.Weights;
    for (u32 Row = 0; Row < W3->Rows; ++Row) {
        for (u32 Col = 0; Col < W3->Cols; ++Col) {
            Task->OptimalWeights[ParameterIndex++] = W3->Data[Row * W3->Stride + Col];
        }
    }
    for (u32 i = 0; i < Network->Layer3.Bias.Size; ++i) {
        Task->OptimalWeights[ParameterIndex++] = Network->Layer3.Bias.Data[i];
    }
    
    assert(ParameterIndex == EWC->TotalParameters);
    
    // Store final task information
    Task->FinalLoss = FinalLoss;
    Task->LastAccessTimestamp = __rdtsc();
    
    // Compute Fisher Information Matrix for this task
    // Note: This requires access to training data, which should be provided separately
    // For now, we'll mark it as ready for Fisher computation
    
    ValidateEWCState(EWC);
}

void SetTaskImportance(ewc_state *EWC, u32 TaskID, f32 Importance)
{
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        if (EWC->Tasks[TaskIndex].TaskID == TaskID) {
            EWC->Tasks[TaskIndex].TaskImportance = Importance;
            break;
        }
    }
}

b32 HasTask(ewc_state *EWC, u32 TaskID)
{
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        if (EWC->Tasks[TaskIndex].TaskID == TaskID && EWC->Tasks[TaskIndex].IsActive) {
            return true;
        }
    }
    return false;
}

// ================================================================================================
// Fisher Information Matrix Computation
// ================================================================================================

void ComputeFisherInformation_Scalar(ewc_fisher_matrix *Fisher,
                                    neural_network *Network,
                                    neural_vector *Samples, u32 SampleCount)
{
    u64 StartCycles = __rdtsc();
    
    // Accumulate gradients over samples
    f32 *AccumulatedGradients = (f32 *)calloc(Fisher->TotalParameters, sizeof(f32));
    f32 *TempGradients = (f32 *)malloc(Fisher->TotalParameters * sizeof(f32));
    
    for (u32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex) {
        // Forward pass
        neural_vector Input = Samples[SampleIndex];
        neural_vector Output = AllocateVector(Network->Arena, Network->OutputSize);
        ForwardPass(Network, &Input, &Output);
        
        // Compute gradients (simplified - normally would use actual labels)
        // For Fisher computation, we typically use the model's own predictions
        BackwardPass(Network, &Output, 0.0f); // Learning rate 0 to just compute gradients
        
        // Extract gradients and accumulate squared values
        u32 ParameterIndex = 0;
        
        // Layer 1 gradients
        neural_matrix *W1 = &Network->Layer1.Weights;
        for (u32 Row = 0; Row < W1->Rows; ++Row) {
            for (u32 Col = 0; Col < W1->Cols; ++Col) {
                f32 Grad = W1->Data[Row * W1->Stride + Col]; // Simplified gradient access
                AccumulatedGradients[ParameterIndex] += Grad * Grad;
                ParameterIndex++;
            }
        }
        
        // Bias gradients
        for (u32 i = 0; i < Network->Layer1.Bias.Size; ++i) {
            f32 Grad = Network->Layer1.Bias.Data[i]; // Simplified
            AccumulatedGradients[ParameterIndex] += Grad * Grad;
            ParameterIndex++;
        }
        
        // Similar for Layer 2 and Layer 3...
        // (Implementation continues for all layers)
    }
    
    // Convert to Fisher entries (sparse representation)
    Fisher->EntryCount = 0;
    for (u32 ParamIndex = 0; ParamIndex < Fisher->TotalParameters; ++ParamIndex) {
        f32 FisherValue = AccumulatedGradients[ParamIndex] / SampleCount;
        
        if (FisherValue > EWC_MIN_FISHER_VALUE) {
            ewc_fisher_entry *Entry = &Fisher->Entries[Fisher->EntryCount++];
            Entry->ParameterIndex = ParamIndex;
            Entry->FisherValue = FisherValue;
        }
    }
    
    // Update statistics
    Fisher->SparsityRatio = 1.0f - ((f32)Fisher->EntryCount / Fisher->TotalParameters);
    Fisher->SampleCount = SampleCount;
    Fisher->ComputationCycles = __rdtsc() - StartCycles;
    
    // Cleanup
    free(AccumulatedGradients);
    free(TempGradients);
    
    ValidateFisherMatrix(Fisher);
}

void ComputeFisherInformation_AVX2(ewc_fisher_matrix *Fisher,
                                 neural_network *Network,
                                 neural_vector *Samples, u32 SampleCount)
{
    u64 StartCycles = __rdtsc();
    
    // PERFORMANCE: SIMD-optimized Fisher computation
    // CACHE: Process 8 parameters at once for cache efficiency
    
    u32 AlignedParameterCount = AlignToSIMD(Fisher->TotalParameters);
    f32 *AccumulatedGradients = (f32 *)_mm_malloc(AlignedParameterCount * sizeof(f32), 32);
    memset(AccumulatedGradients, 0, AlignedParameterCount * sizeof(f32));
    
    // Process samples in batches for better cache utilization
    const u32 BatchSize = 32; // Tuned for L1 cache
    
    for (u32 BatchStart = 0; BatchStart < SampleCount; BatchStart += BatchSize) {
        u32 BatchEnd = (BatchStart + BatchSize < SampleCount) ? BatchStart + BatchSize : SampleCount;
        
        for (u32 SampleIndex = BatchStart; SampleIndex < BatchEnd; ++SampleIndex) {
            // Forward and backward pass (same as scalar version)
            neural_vector Input = Samples[SampleIndex];
            neural_vector Output = AllocateVector(Network->Arena, Network->OutputSize);
            ForwardPass(Network, &Input, &Output);
            BackwardPass(Network, &Output, 0.0f);
            
            // SIMD accumulation of squared gradients
            // Process 8 floats at once with AVX2
            for (u32 ParamIndex = 0; ParamIndex < AlignedParameterCount; ParamIndex += 8) {
                // Load gradients (this would need proper gradient extraction)
                __m256 Gradients = _mm256_load_ps(&AccumulatedGradients[ParamIndex]); // Placeholder
                __m256 Squared = _mm256_mul_ps(Gradients, Gradients);
                __m256 Accumulated = _mm256_load_ps(&AccumulatedGradients[ParamIndex]);
                __m256 Updated = _mm256_add_ps(Accumulated, Squared);
                _mm256_store_ps(&AccumulatedGradients[ParamIndex], Updated);
            }
        }
    }
    
    // Normalize by sample count and convert to sparse representation
    __m256 SampleCountVec = _mm256_set1_ps((f32)SampleCount);
    __m256 MinFisherVec = _mm256_set1_ps(EWC_MIN_FISHER_VALUE);
    
    Fisher->EntryCount = 0;
    for (u32 ParamIndex = 0; ParamIndex < Fisher->TotalParameters; ParamIndex += 8) {
        __m256 Values = _mm256_load_ps(&AccumulatedGradients[ParamIndex]);
        __m256 Normalized = _mm256_div_ps(Values, SampleCountVec);
        
        // Check which values exceed threshold
        __m256 Mask = _mm256_cmp_ps(Normalized, MinFisherVec, _CMP_GT_OQ);
        
        // Store non-zero entries
        f32 NormalizedArray[8];
        _mm256_store_ps(NormalizedArray, Normalized);
        
        for (u32 i = 0; i < 8 && (ParamIndex + i) < Fisher->TotalParameters; ++i) {
            if (NormalizedArray[i] > EWC_MIN_FISHER_VALUE) {
                ewc_fisher_entry *Entry = &Fisher->Entries[Fisher->EntryCount++];
                Entry->ParameterIndex = ParamIndex + i;
                Entry->FisherValue = NormalizedArray[i];
            }
        }
    }
    
    Fisher->SparsityRatio = 1.0f - ((f32)Fisher->EntryCount / Fisher->TotalParameters);
    Fisher->SampleCount = SampleCount;
    Fisher->ComputationCycles = __rdtsc() - StartCycles;
    
    _mm_free(AccumulatedGradients);
    ValidateFisherMatrix(Fisher);
}

// ================================================================================================
// EWC Penalty Computation (Hot Path)
// ================================================================================================

f32 ComputeEWCPenalty_Scalar(ewc_state *EWC, neural_network *Network)
{
    u64 StartCycles = __rdtsc();
    
    // Extract current parameters from network
    u32 ParameterIndex = 0;
    
    // Flatten current network parameters
    // Layer 1
    neural_matrix *W1 = &Network->Layer1.Weights;
    for (u32 Row = 0; Row < W1->Rows; ++Row) {
        for (u32 Col = 0; Col < W1->Cols; ++Col) {
            EWC->TempParameters[ParameterIndex++] = W1->Data[Row * W1->Stride + Col];
        }
    }
    for (u32 i = 0; i < Network->Layer1.Bias.Size; ++i) {
        EWC->TempParameters[ParameterIndex++] = Network->Layer1.Bias.Data[i];
    }
    
    // Layer 2
    neural_matrix *W2 = &Network->Layer2.Weights;
    for (u32 Row = 0; Row < W2->Rows; ++Row) {
        for (u32 Col = 0; Col < W2->Cols; ++Col) {
            EWC->TempParameters[ParameterIndex++] = W2->Data[Row * W2->Stride + Col];
        }
    }
    for (u32 i = 0; i < Network->Layer2.Bias.Size; ++i) {
        EWC->TempParameters[ParameterIndex++] = Network->Layer2.Bias.Data[i];
    }
    
    // Layer 3
    neural_matrix *W3 = &Network->Layer3.Weights;
    for (u32 Row = 0; Row < W3->Rows; ++Row) {
        for (u32 Col = 0; Col < W3->Cols; ++Col) {
            EWC->TempParameters[ParameterIndex++] = W3->Data[Row * W3->Stride + Col];
        }
    }
    for (u32 i = 0; i < Network->Layer3.Bias.Size; ++i) {
        EWC->TempParameters[ParameterIndex++] = Network->Layer3.Bias.Data[i];
    }
    
    assert(ParameterIndex == EWC->TotalParameters);
    
    // Compute EWC penalty: λ * Σ(F_i * (θ_i - θ*_i)²)
    f32 TotalPenalty = 0.0f;
    
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        ewc_task *Task = &EWC->Tasks[TaskIndex];
        if (!Task->IsActive) continue;
        
        ewc_fisher_matrix *Fisher = &Task->FisherMatrix;
        f32 TaskPenalty = 0.0f;
        
        // Sparse penalty computation
        for (u32 EntryIndex = 0; EntryIndex < Fisher->EntryCount; ++EntryIndex) {
            ewc_fisher_entry *Entry = &Fisher->Entries[EntryIndex];
            u32 ParamIdx = Entry->ParameterIndex;
            
            f32 CurrentWeight = EWC->TempParameters[ParamIdx];
            f32 OptimalWeight = Task->OptimalWeights[ParamIdx];
            f32 WeightDiff = CurrentWeight - OptimalWeight;
            
            TaskPenalty += Entry->FisherValue * WeightDiff * WeightDiff;
        }
        
        // Weight by task importance
        TotalPenalty += Task->TaskImportance * TaskPenalty;
        Task->LastAccessTimestamp = __rdtsc();
    }
    
    f32 WeightedPenalty = EWC->Lambda * TotalPenalty;
    
    // Update statistics
    EWC->TotalPenaltyComputations++;
    EWC->TotalCycles += __rdtsc() - StartCycles;
    
    return WeightedPenalty;
}

f32 ComputeEWCPenalty_AVX2(ewc_state *EWC, neural_network *Network)
{
    u64 StartCycles = __rdtsc();
    
    // Extract parameters (same as scalar version)
    // ... parameter extraction code ...
    
    // SIMD-optimized penalty computation
    __m256 TotalPenaltyVec = _mm256_setzero_ps();
    
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        ewc_task *Task = &EWC->Tasks[TaskIndex];
        if (!Task->IsActive) continue;
        
        ewc_fisher_matrix *Fisher = &Task->FisherMatrix;
        __m256 TaskPenaltyVec = _mm256_setzero_ps();
        
        // Process Fisher entries in groups of 8
        u32 SIMDEntries = (Fisher->EntryCount / 8) * 8;
        
        for (u32 EntryIndex = 0; EntryIndex < SIMDEntries; EntryIndex += 8) {
            // Load 8 Fisher values
            __m256 FisherValues = _mm256_setzero_ps();
            __m256 WeightDiffs = _mm256_setzero_ps();
            
            // Manual loading since entries are not contiguous
            f32 FisherArray[8], DiffArray[8];
            for (u32 i = 0; i < 8; ++i) {
                ewc_fisher_entry *Entry = &Fisher->Entries[EntryIndex + i];
                u32 ParamIdx = Entry->ParameterIndex;
                
                FisherArray[i] = Entry->FisherValue;
                DiffArray[i] = EWC->TempParameters[ParamIdx] - Task->OptimalWeights[ParamIdx];
            }
            
            FisherValues = _mm256_load_ps(FisherArray);
            WeightDiffs = _mm256_load_ps(DiffArray);
            
            // Compute Fisher * (diff)²
            __m256 DiffSquared = _mm256_mul_ps(WeightDiffs, WeightDiffs);
            __m256 Penalty = _mm256_mul_ps(FisherValues, DiffSquared);
            TaskPenaltyVec = _mm256_add_ps(TaskPenaltyVec, Penalty);
        }
        
        // Handle remaining entries
        for (u32 EntryIndex = SIMDEntries; EntryIndex < Fisher->EntryCount; ++EntryIndex) {
            ewc_fisher_entry *Entry = &Fisher->Entries[EntryIndex];
            u32 ParamIdx = Entry->ParameterIndex;
            
            f32 WeightDiff = EWC->TempParameters[ParamIdx] - Task->OptimalWeights[ParamIdx];
            f32 Penalty = Entry->FisherValue * WeightDiff * WeightDiff;
            
            // Add to vector (broadcast scalar to all lanes and add to first lane)
            __m256 PenaltyVec = _mm256_set_ps(0,0,0,0,0,0,0, Penalty);
            TaskPenaltyVec = _mm256_add_ps(TaskPenaltyVec, PenaltyVec);
        }
        
        // Reduce task penalty vector to scalar
        f32 TaskPenaltyArray[8];
        _mm256_store_ps(TaskPenaltyArray, TaskPenaltyVec);
        f32 TaskPenalty = 0.0f;
        for (u32 i = 0; i < 8; ++i) {
            TaskPenalty += TaskPenaltyArray[i];
        }
        
        // Weight by task importance and add to total
        __m256 TaskImportanceVec = _mm256_set1_ps(Task->TaskImportance * TaskPenalty);
        TotalPenaltyVec = _mm256_add_ps(TotalPenaltyVec, TaskImportanceVec);
        
        Task->LastAccessTimestamp = __rdtsc();
    }
    
    // Reduce total penalty vector to scalar
    f32 TotalPenaltyArray[8];
    _mm256_store_ps(TotalPenaltyArray, TotalPenaltyVec);
    f32 TotalPenalty = TotalPenaltyArray[0]; // Only first lane has the accumulated value
    
    f32 WeightedPenalty = EWC->Lambda * TotalPenalty;
    
    EWC->TotalPenaltyComputations++;
    EWC->TotalCycles += __rdtsc() - StartCycles;
    
    return WeightedPenalty;
}

// ================================================================================================
// Parameter Updates with EWC
// ================================================================================================

void UpdateParametersWithEWC(ewc_state *EWC, neural_network *Network,
                            neural_vector *Gradients, f32 LearningRate)
{
    // Standard gradient update with EWC penalty gradient added
    // ∂EWC/∂θ_i = 2 * λ * Σ(F_i * (θ_i - θ*_i))
    
    // First, compute EWC penalty gradients
    f32 *EWCGradients = EWC->TempGradients;
    memset(EWCGradients, 0, EWC->TotalParameters * sizeof(f32));
    
    // Extract current parameters
    u32 ParameterIndex = 0;
    // ... (same parameter extraction as in penalty computation)
    
    // Compute EWC gradients
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        ewc_task *Task = &EWC->Tasks[TaskIndex];
        if (!Task->IsActive) continue;
        
        ewc_fisher_matrix *Fisher = &Task->FisherMatrix;
        
        for (u32 EntryIndex = 0; EntryIndex < Fisher->EntryCount; ++EntryIndex) {
            ewc_fisher_entry *Entry = &Fisher->Entries[EntryIndex];
            u32 ParamIdx = Entry->ParameterIndex;
            
            f32 WeightDiff = EWC->TempParameters[ParamIdx] - Task->OptimalWeights[ParamIdx];
            f32 EWCGrad = 2.0f * EWC->Lambda * Task->TaskImportance * Entry->FisherValue * WeightDiff;
            
            EWCGradients[ParamIdx] += EWCGrad;
        }
    }
    
    // Apply combined gradients to network parameters
    ParameterIndex = 0;
    
    // Layer 1 weights
    neural_matrix *W1 = &Network->Layer1.Weights;
    for (u32 Row = 0; Row < W1->Rows; ++Row) {
        for (u32 Col = 0; Col < W1->Cols; ++Col) {
            f32 StandardGrad = Gradients->Data[ParameterIndex]; // Simplified access
            f32 CombinedGrad = StandardGrad + EWCGradients[ParameterIndex];
            W1->Data[Row * W1->Stride + Col] -= LearningRate * CombinedGrad;
            ParameterIndex++;
        }
    }
    
    // Similar updates for other layers...
    // (Complete implementation would update all layers)
}

// ================================================================================================
// Adaptive Lambda Management
// ================================================================================================

void UpdateLambda(ewc_state *EWC, f32 CurrentLoss, f32 ValidationLoss)
{
    // Adaptive lambda based on performance
    // Increase lambda if validation loss is rising (indicating forgetting)
    // Decrease lambda if learning is too constrained
    
    local_persist f32 PreviousValidationLoss = 0.0f;
    
    if (PreviousValidationLoss > 0.0f) {
        f32 LossChange = ValidationLoss - PreviousValidationLoss;
        
        if (LossChange > 0.01f) {
            // Validation loss increasing - more consolidation needed
            EWC->Lambda = fminf(EWC->Lambda * 1.1f, EWC->MaxLambda);
        } else if (LossChange < -0.01f && CurrentLoss > ValidationLoss * 1.5f) {
            // Learning is too constrained - reduce consolidation
            EWC->Lambda = fmaxf(EWC->Lambda * 0.9f, EWC->MinLambda);
        }
    }
    
    PreviousValidationLoss = ValidationLoss;
    
    // Apply decay over time
    EWC->Lambda *= EWC->LambdaDecay;
    EWC->Lambda = fmaxf(EWC->Lambda, EWC->MinLambda);
}

void SetLambdaRange(ewc_state *EWC, f32 MinLambda, f32 MaxLambda)
{
    EWC->MinLambda = MinLambda;
    EWC->MaxLambda = MaxLambda;
    EWC->Lambda = fmaxf(fminf(EWC->Lambda, MaxLambda), MinLambda);
}

f32 GetRecommendedLambda(ewc_state *EWC, neural_network *Network)
{
    // Heuristic based on network size and number of tasks
    f32 BaseLog = logf((f32)EWC->TotalParameters);
    f32 TaskMultiplier = 1.0f + 0.5f * EWC->ActiveTaskCount;
    return 100.0f * BaseLog * TaskMultiplier;
}

// ================================================================================================
// Performance Monitoring and Statistics
// ================================================================================================

void GetEWCStats(ewc_state *EWC, ewc_performance_stats *Stats)
{
    memset(Stats, 0, sizeof(ewc_performance_stats));
    
    // Timing statistics
    Stats->PenaltyComputationCycles = EWC->TotalCycles;
    Stats->FisherComputationCycles = 0;
    Stats->UpdateCycles = 0;
    
    // Memory usage
    Stats->TaskMemoryUsed = EWC->ActiveTaskCount * (sizeof(ewc_task) + 
                                                   EWC->TotalParameters * sizeof(f32));
    Stats->FisherMemoryUsed = 0;
    Stats->TotalMemoryUsed = Stats->TaskMemoryUsed + Stats->FisherMemoryUsed;
    
    // Fisher matrix statistics
    u32 TotalFisherEntries = 0;
    u32 NonZeroEntries = 0;
    f32 TotalSparsity = 0.0f;
    
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        ewc_task *Task = &EWC->Tasks[TaskIndex];
        if (Task->IsActive) {
            Stats->FisherComputationCycles += Task->FisherMatrix.ComputationCycles;
            Stats->FisherMemoryUsed += Task->FisherMatrix.EntryCount * sizeof(ewc_fisher_entry);
            TotalFisherEntries += Task->FisherMatrix.TotalParameters;
            NonZeroEntries += Task->FisherMatrix.EntryCount;
            TotalSparsity += Task->FisherMatrix.SparsityRatio;
        }
    }
    
    Stats->TotalFisherEntries = TotalFisherEntries;
    Stats->NonZeroFisherEntries = NonZeroEntries;
    Stats->AverageSparsity = (EWC->ActiveTaskCount > 0) ? 
                            TotalSparsity / EWC->ActiveTaskCount : 0.0f;
    
    // Performance estimates
    if (Stats->PenaltyComputationCycles > 0 && EWC->TotalPenaltyComputations > 0) {
        f64 CyclesPerComputation = (f64)Stats->PenaltyComputationCycles / EWC->TotalPenaltyComputations;
        f64 ComputationsPerSecond = 2.5e9 / CyclesPerComputation; // Assume 2.5GHz CPU
        f64 FLOPsPerComputation = NonZeroEntries * 4.0; // 4 FLOPs per Fisher entry
        Stats->GFLOPS = (f32)(ComputationsPerSecond * FLOPsPerComputation / 1e9);
    }
    
    Stats->TotalMemoryUsed = Stats->TaskMemoryUsed + Stats->FisherMemoryUsed;
}

void PrintEWCStats(ewc_performance_stats *Stats)
{
    printf("\n=== EWC Performance Statistics ===\n");
    printf("Memory Usage:\n");
    printf("  Total: %u KB\n", Stats->TotalMemoryUsed / 1024);
    printf("  Tasks: %u KB\n", Stats->TaskMemoryUsed / 1024);
    printf("  Fisher: %u KB\n", Stats->FisherMemoryUsed / 1024);
    printf("\nFisher Matrix Stats:\n");
    printf("  Non-zero entries: %u / %u (%.2f%% sparse)\n",
           Stats->NonZeroFisherEntries, Stats->TotalFisherEntries,
           Stats->AverageSparsity * 100.0f);
    printf("\nPerformance:\n");
    printf("  Penalty computation: %.2f GFLOPS\n", Stats->GFLOPS);
    printf("  Fisher cycles: %llu\n", (unsigned long long)Stats->FisherComputationCycles);
    printf("  Penalty cycles: %llu\n", (unsigned long long)Stats->PenaltyComputationCycles);
}

// ================================================================================================
// Debug and Validation
// ================================================================================================

#if HANDMADE_DEBUG

void ValidateEWCState(ewc_state *EWC)
{
    assert(EWC->Arena != 0);
    assert(EWC->TotalParameters > 0);
    assert(EWC->ActiveTaskCount <= EWC_MAX_TASKS);
    assert(EWC->Lambda >= EWC->MinLambda && EWC->Lambda <= EWC->MaxLambda);
    
    for (u32 TaskIndex = 0; TaskIndex < EWC->ActiveTaskCount; ++TaskIndex) {
        ewc_task *Task = &EWC->Tasks[TaskIndex];
        if (Task->IsActive) {
            assert(Task->OptimalWeights != 0);
            assert(Task->ParameterCount == EWC->TotalParameters);
            assert(Task->TaskImportance >= 0.0f);
            ValidateFisherMatrix(&Task->FisherMatrix);
        }
    }
}

void ValidateFisherMatrix(ewc_fisher_matrix *Fisher)
{
    assert(Fisher->TotalParameters > 0);
    assert(Fisher->EntryCount <= Fisher->MaxEntries);
    assert(Fisher->SparsityRatio >= 0.0f && Fisher->SparsityRatio <= 1.0f);
    
    for (u32 i = 0; i < Fisher->EntryCount; ++i) {
        ewc_fisher_entry *Entry = &Fisher->Entries[i];
        assert(Entry->ParameterIndex < Fisher->TotalParameters);
        assert(Entry->FisherValue >= EWC_MIN_FISHER_VALUE);
        assert(isfinite(Entry->FisherValue));
    }
}

void PrintTaskInfo(ewc_task *Task)
{
    printf("Task %u (%s):\n", Task->TaskID, Task->Name);
    printf("  Parameters: %u\n", Task->ParameterCount);
    printf("  Fisher entries: %u (%.2f%% sparse)\n",
           Task->FisherMatrix.EntryCount,
           Task->FisherMatrix.SparsityRatio * 100.0f);
    printf("  Final loss: %.6f\n", Task->FinalLoss);
    printf("  Importance: %.3f\n", Task->TaskImportance);
    printf("  Training epochs: %u\n", Task->TrainingEpochs);
}

void CheckFisherNaN(ewc_fisher_matrix *Fisher)
{
    for (u32 i = 0; i < Fisher->EntryCount; ++i) {
        assert(isfinite(Fisher->Entries[i].FisherValue));
        assert(!isnan(Fisher->Entries[i].FisherValue));
    }
}

#endif // HANDMADE_DEBUG

// ================================================================================================
// Missing Implementation Functions
// ================================================================================================

void CompressFisherMatrix(ewc_fisher_matrix *Fisher, f32 SparsityThreshold)
{
    // Remove entries below threshold
    u32 WriteIndex = 0;
    for (u32 ReadIndex = 0; ReadIndex < Fisher->EntryCount; ++ReadIndex) {
        if (Fisher->Entries[ReadIndex].FisherValue >= SparsityThreshold) {
            if (WriteIndex != ReadIndex) {
                Fisher->Entries[WriteIndex] = Fisher->Entries[ReadIndex];
            }
            WriteIndex++;
        }
    }
    
    Fisher->EntryCount = WriteIndex;
    Fisher->SparsityRatio = 1.0f - ((f32)WriteIndex / Fisher->TotalParameters);
}

void PruneInactiveTasks(ewc_state *EWC, u32 MaxTasks)
{
    if (EWC->ActiveTaskCount <= MaxTasks) return;
    
    // Simple pruning: remove oldest tasks
    // In a more sophisticated implementation, could use LRU or importance-based pruning
    u32 TasksToRemove = EWC->ActiveTaskCount - MaxTasks;
    
    for (u32 i = 0; i < TasksToRemove; ++i) {
        EWC->Tasks[i].IsActive = false;
    }
    
    // Shift remaining tasks down
    for (u32 i = TasksToRemove; i < EWC->ActiveTaskCount; ++i) {
        EWC->Tasks[i - TasksToRemove] = EWC->Tasks[i];
    }
    
    EWC->ActiveTaskCount = MaxTasks;
}

u32 GetMemoryUsage(ewc_state *EWC)
{
    u32 TotalMemory = 0;
    
    // Base EWC structure
    TotalMemory += sizeof(ewc_state);
    
    // Task memory
    for (u32 i = 0; i < EWC->ActiveTaskCount; ++i) {
        if (EWC->Tasks[i].IsActive) {
            TotalMemory += sizeof(ewc_task);
            TotalMemory += EWC->TotalParameters * sizeof(f32); // Optimal weights
            TotalMemory += EWC->Tasks[i].FisherMatrix.EntryCount * sizeof(ewc_fisher_entry);
        }
    }
    
    // Temporary buffers
    TotalMemory += EWC->TotalParameters * sizeof(f32) * 2; // TempGradients + TempParameters
    
    return TotalMemory;
}

void IntegrateWithNetwork(ewc_state *EWC, neural_network *Network)
{
    // Store reference to current network weights
    // In a full implementation, this would extract parameter pointers from the network
    EWC->CurrentWeights = EWC->TempParameters; // Simplified for now
    
    // Validate parameter count matches
    u32 NetworkParams = Network->Layer1.Weights.Rows * Network->Layer1.Weights.Cols +
                        Network->Layer1.Bias.Size +
                        Network->Layer2.Weights.Rows * Network->Layer2.Weights.Cols +
                        Network->Layer2.Bias.Size +
                        Network->Layer3.Weights.Rows * Network->Layer3.Weights.Cols +
                        Network->Layer3.Bias.Size;
    
    Assert(NetworkParams <= EWC->TotalParameters);
}

// TODO: Implement when LSTM/DNC structures are available
/*
void IntegrateWithLSTM(ewc_state *EWC, struct lstm_layer *LSTM)
{
    // Placeholder implementation
    // In reality, would integrate with LSTM parameter structure
    (void)EWC; (void)LSTM;
}

void IntegrateWithDNC(ewc_state *EWC, struct dnc_controller *DNC)
{
    // Placeholder implementation  
    // In reality, would integrate with DNC parameter structure
    (void)EWC; (void)DNC;
}
*/

b32 SaveEWCState(ewc_state *EWC, const char *Filename)
{
    FILE *File = fopen(Filename, "wb");
    if (!File) return false;
    
    // Write header
    ewc_save_data Header;
    Header.Version = 1;
    Header.TaskCount = EWC->ActiveTaskCount;
    Header.ParameterCount = EWC->TotalParameters;
    Header.Lambda = EWC->Lambda;
    
    fwrite(&Header, sizeof(Header), 1, File);
    
    // Write task data
    for (u32 i = 0; i < EWC->ActiveTaskCount; ++i) {
        if (EWC->Tasks[i].IsActive) {
            fwrite(&EWC->Tasks[i], sizeof(ewc_task), 1, File);
            fwrite(EWC->Tasks[i].OptimalWeights, sizeof(f32), EWC->TotalParameters, File);
            fwrite(EWC->Tasks[i].FisherMatrix.Entries, 
                   sizeof(ewc_fisher_entry), 
                   EWC->Tasks[i].FisherMatrix.EntryCount, File);
        }
    }
    
    fclose(File);
    return true;
}

b32 LoadEWCState(ewc_state *EWC, const char *Filename)
{
    FILE *File = fopen(Filename, "rb");
    if (!File) return false;
    
    // Read header
    ewc_save_data Header;
    if (fread(&Header, sizeof(Header), 1, File) != 1) {
        fclose(File);
        return false;
    }
    
    // Validate compatibility
    if (Header.Version != 1 || Header.ParameterCount != EWC->TotalParameters) {
        fclose(File);
        return false;
    }
    
    EWC->Lambda = Header.Lambda;
    EWC->ActiveTaskCount = Header.TaskCount;
    
    // Read task data (simplified - full implementation would properly allocate memory)
    for (u32 i = 0; i < Header.TaskCount; ++i) {
        fread(&EWC->Tasks[i], sizeof(ewc_task), 1, File);
        fread(EWC->Tasks[i].OptimalWeights, sizeof(f32), Header.ParameterCount, File);
        fread(EWC->Tasks[i].FisherMatrix.Entries,
              sizeof(ewc_fisher_entry),
              EWC->Tasks[i].FisherMatrix.EntryCount, File);
    }
    
    fclose(File);
    return true;
}

void SerializeTask(ewc_task *Task, u8 *Buffer, u32 *Offset)
{
    // Copy task structure
    memcpy(Buffer + *Offset, Task, sizeof(ewc_task));
    *Offset += sizeof(ewc_task);
    
    // Copy optimal weights
    memcpy(Buffer + *Offset, Task->OptimalWeights, Task->ParameterCount * sizeof(f32));
    *Offset += Task->ParameterCount * sizeof(f32);
    
    // Copy Fisher entries
    u32 FisherSize = Task->FisherMatrix.EntryCount * sizeof(ewc_fisher_entry);
    memcpy(Buffer + *Offset, Task->FisherMatrix.Entries, FisherSize);
    *Offset += FisherSize;
}

void DeserializeTask(ewc_task *Task, u8 *Buffer, u32 *Offset, memory_arena *Arena)
{
    // Copy task structure
    memcpy(Task, Buffer + *Offset, sizeof(ewc_task));
    *Offset += sizeof(ewc_task);
    
    // Allocate and copy optimal weights
    Task->OptimalWeights = (f32 *)PushArray(Arena, Task->ParameterCount, f32);
    memcpy(Task->OptimalWeights, Buffer + *Offset, Task->ParameterCount * sizeof(f32));
    *Offset += Task->ParameterCount * sizeof(f32);
    
    // Allocate and copy Fisher entries
    Task->FisherMatrix.Entries = (ewc_fisher_entry *)PushArray(Arena, Task->FisherMatrix.EntryCount, ewc_fisher_entry);
    u32 FisherSize = Task->FisherMatrix.EntryCount * sizeof(ewc_fisher_entry);
    memcpy(Task->FisherMatrix.Entries, Buffer + *Offset, FisherSize);
    *Offset += FisherSize;
}

// Simple neural network for EWC testing (bypasses InitializePool issues)
neural_network InitializeSimpleNeuralNetwork(memory_arena *Arena, 
                                             u32 InputSize, u32 Hidden1Size, 
                                             u32 Hidden2Size, u32 OutputSize)
{
    neural_network Network = {0};
    Network.InputSize = InputSize;
    Network.Hidden1Size = Hidden1Size;
    Network.Hidden2Size = Hidden2Size;
    Network.OutputSize = OutputSize;
    Network.Arena = Arena;
    
    // Allocate Layer 1 (Input -> Hidden1)
    Network.Layer1.Weights = AllocateMatrix(Arena, Hidden1Size, InputSize);
    Network.Layer1.Bias = AllocateVector(Arena, Hidden1Size);
    Network.Layer1.Output = AllocateVector(Arena, Hidden1Size);
    Network.Layer1.Gradient = AllocateVector(Arena, Hidden1Size);
    
    // Allocate Layer 2 (Hidden1 -> Hidden2)
    Network.Layer2.Weights = AllocateMatrix(Arena, Hidden2Size, Hidden1Size);
    Network.Layer2.Bias = AllocateVector(Arena, Hidden2Size);
    Network.Layer2.Output = AllocateVector(Arena, Hidden2Size);
    Network.Layer2.Gradient = AllocateVector(Arena, Hidden2Size);
    
    // Allocate Layer 3 (Hidden2 -> Output)
    Network.Layer3.Weights = AllocateMatrix(Arena, OutputSize, Hidden2Size);
    Network.Layer3.Bias = AllocateVector(Arena, OutputSize);
    Network.Layer3.Output = AllocateVector(Arena, OutputSize);
    Network.Layer3.Gradient = AllocateVector(Arena, OutputSize);
    
    // Initialize weights randomly
    InitializeMatrixRandom(&Network.Layer1.Weights, 0.1f);
    InitializeMatrixRandom(&Network.Layer2.Weights, 0.1f);
    InitializeMatrixRandom(&Network.Layer3.Weights, 0.1f);
    
    // Initialize biases to zero
    InitializeVectorZero(&Network.Layer1.Bias);
    InitializeVectorZero(&Network.Layer2.Bias);
    InitializeVectorZero(&Network.Layer3.Bias);
    
    return Network;
}

void BenchmarkEWC(memory_arena *Arena)
{
    printf("=== EWC Benchmark Suite ===\n\n");
    
    u32 ParamCounts[] = {1000, 10000};  // Reduced for simpler testing
    u32 NumSizes = sizeof(ParamCounts) / sizeof(ParamCounts[0]);
    
    printf("Parameter Count | EWC Penalty (μs) | Memory (KB)\n");
    printf("----------------|------------------|------------\n");
    
    for (u32 i = 0; i < NumSizes; ++i) {
        u32 ParamCount = ParamCounts[i];
        
        ewc_state EWC = InitializeEWC(Arena, ParamCount);
        neural_network Network = InitializeSimpleNeuralNetwork(Arena, 100, ParamCount/200, 100, 10);
        
        // Add a task
        u32 TaskID = BeginTask(&EWC, "Benchmark Task");
        ewc_task *Task = &EWC.Tasks[0];
        Task->IsActive = true;
        
        // Set up Fisher matrix (10% sparsity)
        u32 NonZeroCount = ParamCount / 10;
        Task->FisherMatrix.EntryCount = NonZeroCount;
        for (u32 j = 0; j < NonZeroCount; ++j) {
            Task->FisherMatrix.Entries[j] = (ewc_fisher_entry){j * 10, 0.5f};
            Task->OptimalWeights[j * 10] = 1.0f;
        }
        
        // Benchmark penalty computation
        const u32 Iterations = 100;  // Reduced for quick testing
        u64 StartCycles = __rdtsc();
        for (u32 iter = 0; iter < Iterations; ++iter) {
            f32 Penalty = ComputeEWCPenalty(&EWC, &Network);
            (void)Penalty;
        }
        u64 TotalCycles = __rdtsc() - StartCycles;
        
        f64 AvgTimeMicros = (TotalCycles / (f64)Iterations) / 2500.0; // Assume 2.5GHz
        u32 MemoryKB = GetMemoryUsage(&EWC) / 1024;
        
        printf("%15u | %16.1f | %10u\n", ParamCount, AvgTimeMicros, MemoryKB);
    }
    
    printf("\n");
}