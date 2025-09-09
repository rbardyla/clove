#ifndef NEURAL_MATH_H
#define NEURAL_MATH_H

#include "handmade.h"
#include "memory.h"
#include <immintrin.h>  // For AVX/AVX2/AVX-512 intrinsics

/*
    Neural Math Library
    
    Optimized matrix operations for neural networks
    Zero-dependency, cache-aware, SIMD-accelerated
    
    Design principles:
    - Structure-of-arrays for vectorization
    - Cache-blocking for large matrices
    - Memory pooling for weight allocations
    - Explicit prefetching strategies
    - Both scalar and SIMD implementations
    
    Memory layout:
    - Row-major storage (C-style)
    - Aligned to cache lines (64 bytes)
    - Padded for SIMD width
*/

// Configuration
#define NEURAL_USE_AVX2 1
#define NEURAL_USE_AVX512 0  // Set to 1 if your CPU supports AVX-512
#define NEURAL_CACHE_BLOCK_SIZE 64  // Tuned for L1 cache
#define NEURAL_SIMD_WIDTH 8  // AVX2 processes 8 floats at once
#define NEURAL_PREFETCH_DISTANCE 8  // Cache lines ahead to prefetch

// Matrix structure with cache-aligned storage
typedef struct neural_matrix
{
    f32 *Data;           // Aligned to cache line
    u32 Rows;
    u32 Cols;
    u32 Stride;          // Padded width for SIMD alignment
    memory_pool *Pool;   // Memory pool this came from (if any)
} neural_matrix;

// Vector structure (just a matrix with 1 column)
typedef struct neural_vector
{
    f32 *Data;
    u32 Size;
    u32 Stride;
} neural_vector;

// Layer structure for neural network
typedef struct neural_layer
{
    neural_matrix Weights;      // Weight matrix
    neural_vector Bias;         // Bias vector
    neural_vector Output;       // Cached output for forward pass
    neural_vector Gradient;     // Cached gradient for backprop
    
    // Activation function pointers
    void (*Activation)(f32 *Output, u32 Size);
    void (*ActivationDerivative)(f32 *Gradient, f32 *Output, u32 Size);
} neural_layer;

// Simple 3-layer network
typedef struct neural_network
{
    neural_layer Layer1;  // Input -> Hidden1
    neural_layer Layer2;  // Hidden1 -> Hidden2  
    neural_layer Layer3;  // Hidden2 -> Output
    
    // Network configuration
    u32 InputSize;
    u32 Hidden1Size;
    u32 Hidden2Size;
    u32 OutputSize;
    
    // Memory management
    memory_arena *Arena;
    memory_pool *WeightPool;
    memory_pool *ActivationPool;
    
    // Performance counters
    u64 ForwardCycles;
    u64 BackwardCycles;
    u64 ForwardCount;
    u64 BackwardCount;
} neural_network;

// Matrix allocation and initialization
neural_matrix AllocateMatrix(memory_arena *Arena, u32 Rows, u32 Cols);
neural_matrix AllocateMatrixFromPool(memory_pool *Pool, u32 Rows, u32 Cols);
neural_vector AllocateVector(memory_arena *Arena, u32 Size);
void InitializeMatrixRandom(neural_matrix *Matrix, f32 Scale);
void InitializeMatrixZero(neural_matrix *Matrix);
void InitializeVectorZero(neural_vector *Vector);

// Basic matrix operations (scalar implementations)
void MatrixMultiply_Scalar(neural_matrix *C, neural_matrix *A, neural_matrix *B);
void MatrixVectorMultiply_Scalar(neural_vector *y, neural_matrix *A, neural_vector *x);
void MatrixTranspose_Scalar(neural_matrix *AT, neural_matrix *A);
void MatrixAdd_Scalar(neural_matrix *C, neural_matrix *A, neural_matrix *B);
void MatrixScale_Scalar(neural_matrix *A, f32 Scale);

// Optimized matrix operations (SIMD implementations)
void MatrixMultiply_AVX2(neural_matrix *C, neural_matrix *A, neural_matrix *B);
void MatrixVectorMultiply_AVX2(neural_vector *y, neural_matrix *A, neural_vector *x);
void MatrixTranspose_AVX2(neural_matrix *AT, neural_matrix *A);
void MatrixAdd_AVX2(neural_matrix *C, neural_matrix *A, neural_matrix *B);
void MatrixScale_AVX2(neural_matrix *A, f32 Scale);

#if NEURAL_USE_AVX512
void MatrixMultiply_AVX512(neural_matrix *C, neural_matrix *A, neural_matrix *B);
void MatrixVectorMultiply_AVX512(neural_vector *y, neural_matrix *A, neural_vector *x);
#endif

// Cache-blocked GEMM for large matrices
void MatrixMultiply_Blocked(neural_matrix *C, neural_matrix *A, neural_matrix *B);

// Element-wise operations
void ElementwiseMultiply(neural_matrix *C, neural_matrix *A, neural_matrix *B);
void ElementwiseAdd(neural_matrix *C, neural_matrix *A, neural_matrix *B);

// Activation functions (scalar)
void ReLU_Scalar(f32 *Output, u32 Size);
void ReLU_Derivative_Scalar(f32 *Gradient, f32 *Output, u32 Size);
void Sigmoid_Scalar(f32 *Output, u32 Size);
void Sigmoid_Derivative_Scalar(f32 *Gradient, f32 *Output, u32 Size);
void Tanh_Scalar(f32 *Output, u32 Size);
void Tanh_Derivative_Scalar(f32 *Gradient, f32 *Output, u32 Size);
void Softmax_Scalar(f32 *Output, u32 Size);

// Activation functions (SIMD)
void ReLU_AVX2(f32 *Output, u32 Size);
void ReLU_Derivative_AVX2(f32 *Gradient, f32 *Output, u32 Size);
void Sigmoid_AVX2(f32 *Output, u32 Size);
void Tanh_AVX2(f32 *Output, u32 Size);
void Softmax_AVX2(f32 *Output, u32 Size);

// Neural network operations
neural_network InitializeNeuralNetwork(memory_arena *Arena, 
                                       u32 InputSize, u32 Hidden1Size, 
                                       u32 Hidden2Size, u32 OutputSize);
void ForwardPass(neural_network *Network, neural_vector *Input, neural_vector *Output);
void BackwardPass(neural_network *Network, neural_vector *Target, f32 LearningRate);

// Memory pooling for weight allocations
memory_pool CreateWeightPool(memory_arena *Arena, u32 MaxWeights);
void *AllocateWeights(memory_pool *Pool, u32 Count);
void FreeWeights(memory_pool *Pool, void *Weights);

// Benchmarking utilities
typedef struct benchmark_result
{
    char Name[64];
    u64 Cycles;
    u64 BytesProcessed;
    f64 GBPerSecond;
    f64 GFLOPS;
} benchmark_result;

void BenchmarkMatrixMultiply(memory_arena *Arena);
void BenchmarkActivations(memory_arena *Arena);
void BenchmarkForwardPass(memory_arena *Arena);
void PrintBenchmarkResult(benchmark_result *Result);

// Performance monitoring
typedef struct neural_perf_stats
{
    // Cache statistics
    u64 L1CacheMisses;
    u64 L2CacheMisses;
    u64 TLBMisses;
    
    // Operation counts
    u64 MatrixMultiplies;
    u64 VectorOperations;
    u64 ActivationCalls;
    
    // Timing
    u64 TotalCycles;
    u64 ComputeCycles;
    u64 MemoryCycles;
} neural_perf_stats;

extern neural_perf_stats GlobalNeuralStats;

// Utility functions
inline u32 AlignToSIMD(u32 Size)
{
    return AlignPow2(Size, NEURAL_SIMD_WIDTH);
}

inline b32 IsAligned(void *Ptr, u32 Alignment)
{
    return ((umm)Ptr & (Alignment - 1)) == 0;
}

// Fast math approximations for activation functions
inline f32 FastExp(f32 x)
{
    // PERFORMANCE: Fast approximation, ~2x faster than expf()
    // Accuracy: ~0.001 relative error for x in [-10, 10]
    union { f32 f; i32 i; } v;
    v.i = (i32)(12102203.0f * x + 1064866805.0f);
    return v.f;
}

inline f32 FastTanh(f32 x)
{
    // PERFORMANCE: Rational approximation, ~3x faster than tanhf()
    f32 x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Prefetching hints
inline void PrefetchL1(void *Addr)
{
    _mm_prefetch((char *)Addr, _MM_HINT_T0);
}

inline void PrefetchL2(void *Addr)
{
    _mm_prefetch((char *)Addr, _MM_HINT_T1);
}

inline void PrefetchNTA(void *Addr)
{
    _mm_prefetch((char *)Addr, _MM_HINT_NTA);
}

// Debug utilities
#if HANDMADE_DEBUG
void ValidateMatrix(neural_matrix *Matrix);
void PrintMatrix(neural_matrix *Matrix, const char *Name);
void CheckNaN(neural_matrix *Matrix);
void CheckGradients(neural_network *Network, neural_vector *Input, neural_vector *Target);
#else
#define ValidateMatrix(Matrix)
#define PrintMatrix(Matrix, Name)
#define CheckNaN(Matrix)
#define CheckGradients(Network, Input, Target)
#endif

// Architecture selection macros
#if NEURAL_USE_AVX2
    #define MatrixMultiply MatrixMultiply_AVX2
    #define MatrixVectorMultiply MatrixVectorMultiply_AVX2
    #define MatrixTranspose MatrixTranspose_AVX2
    #define MatrixAdd MatrixAdd_AVX2
    #define MatrixScale MatrixScale_AVX2
    #define ReLU ReLU_AVX2
    #define Sigmoid Sigmoid_AVX2
    #define Tanh Tanh_AVX2
    #define Softmax Softmax_AVX2
#else
    #define MatrixMultiply MatrixMultiply_Scalar
    #define MatrixVectorMultiply MatrixVectorMultiply_Scalar
    #define MatrixTranspose MatrixTranspose_Scalar
    #define MatrixAdd MatrixAdd_Scalar
    #define MatrixScale MatrixScale_Scalar
    #define ReLU ReLU_Scalar
    #define Sigmoid Sigmoid_Scalar
    #define Tanh Tanh_Scalar
    #define Softmax Softmax_Scalar
#endif

#endif // NEURAL_MATH_H