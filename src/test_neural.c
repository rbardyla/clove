#include "handmade.h"
#include "memory.h"
#include "neural_math.h"
#include <stdio.h>
#include <stdlib.h>

// Memory statistics
memory_stats GlobalMemoryStats = {0};

/*
    Neural Math Library Test Suite
    
    Demonstrates:
    - Matrix operations with AVX2 optimization
    - Neural network forward pass
    - Memory pooling for weight allocations
    - Performance benchmarking
    - Cache-aware programming
*/

void TestMatrixOperations(memory_arena *Arena)
{
    printf("\n==================================================\n");
    printf("         MATRIX OPERATIONS TEST\n");
    printf("==================================================\n");
    
    // Test basic matrix multiply
    {
        printf("\n[TEST] Matrix Multiplication (4x3 * 3x2 = 4x2):\n");
        
        neural_matrix A = AllocateMatrix(Arena, 4, 3);
        neural_matrix B = AllocateMatrix(Arena, 3, 2);
        neural_matrix C = AllocateMatrix(Arena, 4, 2);
        
        // Initialize A
        f32 AData[] = {
            1, 2, 3,
            4, 5, 6,
            7, 8, 9,
            10, 11, 12
        };
        for(u32 i = 0; i < 4; ++i)
        {
            for(u32 j = 0; j < 3; ++j)
            {
                A.Data[i * A.Stride + j] = AData[i * 3 + j];
            }
        }
        
        // Initialize B
        f32 BData[] = {
            1, 2,
            3, 4,
            5, 6
        };
        for(u32 i = 0; i < 3; ++i)
        {
            for(u32 j = 0; j < 2; ++j)
            {
                B.Data[i * B.Stride + j] = BData[i * 2 + j];
            }
        }
        
        // Test scalar version
        u64 ScalarStart = ReadCPUTimer();
        MatrixMultiply_Scalar(&C, &A, &B);
        u64 ScalarCycles = ReadCPUTimer() - ScalarStart;
        
        printf("  Scalar result:\n");
        for(u32 i = 0; i < C.Rows; ++i)
        {
            printf("    ");
            for(u32 j = 0; j < C.Cols; ++j)
            {
                printf("%6.1f ", C.Data[i * C.Stride + j]);
            }
            printf("\n");
        }
        printf("  Scalar cycles: %llu\n", (unsigned long long)ScalarCycles);
        
#if NEURAL_USE_AVX2
        // Test AVX2 version
        InitializeMatrixZero(&C);
        u64 AVX2Start = ReadCPUTimer();
        MatrixMultiply_AVX2(&C, &A, &B);
        u64 AVX2Cycles = ReadCPUTimer() - AVX2Start;
        
        printf("\n  AVX2 result:\n");
        for(u32 i = 0; i < C.Rows; ++i)
        {
            printf("    ");
            for(u32 j = 0; j < C.Cols; ++j)
            {
                printf("%6.1f ", C.Data[i * C.Stride + j]);
            }
            printf("\n");
        }
        printf("  AVX2 cycles: %llu (%.2fx speedup)\n", 
               (unsigned long long)AVX2Cycles,
               (f64)ScalarCycles / (f64)AVX2Cycles);
#endif
    }
    
    // Test matrix-vector multiply
    {
        printf("\n[TEST] Matrix-Vector Multiplication:\n");
        
        neural_matrix A = AllocateMatrix(Arena, 3, 4);
        neural_vector x = AllocateVector(Arena, 4);
        neural_vector y = AllocateVector(Arena, 3);
        
        // Initialize matrix
        for(u32 i = 0; i < 3; ++i)
        {
            for(u32 j = 0; j < 4; ++j)
            {
                A.Data[i * A.Stride + j] = (f32)(i * 4 + j + 1);
            }
        }
        
        // Initialize vector
        for(u32 i = 0; i < 4; ++i)
        {
            x.Data[i] = (f32)(i + 1);
        }
        
        MatrixVectorMultiply(&y, &A, &x);
        
        printf("  Result: [");
        for(u32 i = 0; i < y.Size; ++i)
        {
            printf("%.1f%s", y.Data[i], (i < y.Size - 1) ? ", " : "");
        }
        printf("]\n");
    }
    
    // Test transpose
    {
        printf("\n[TEST] Matrix Transpose:\n");
        
        neural_matrix A = AllocateMatrix(Arena, 3, 4);
        neural_matrix AT = AllocateMatrix(Arena, 4, 3);
        
        // Initialize with sequential values
        for(u32 i = 0; i < 3; ++i)
        {
            for(u32 j = 0; j < 4; ++j)
            {
                A.Data[i * A.Stride + j] = (f32)(i * 4 + j);
            }
        }
        
        MatrixTranspose(&AT, &A);
        
        printf("  Original (3x4):\n");
        for(u32 i = 0; i < 3; ++i)
        {
            printf("    ");
            for(u32 j = 0; j < 4; ++j)
            {
                printf("%3.0f ", A.Data[i * A.Stride + j]);
            }
            printf("\n");
        }
        
        printf("  Transposed (4x3):\n");
        for(u32 i = 0; i < 4; ++i)
        {
            printf("    ");
            for(u32 j = 0; j < 3; ++j)
            {
                printf("%3.0f ", AT.Data[i * AT.Stride + j]);
            }
            printf("\n");
        }
    }
}

void TestActivationFunctions(memory_arena *Arena)
{
    printf("\n==================================================\n");
    printf("         ACTIVATION FUNCTIONS TEST\n");
    printf("==================================================\n");
    
    u32 Size = 8;
    neural_vector Input = AllocateVector(Arena, Size);
    
    // Test ReLU
    {
        printf("\n[TEST] ReLU:\n");
        
        // Initialize with positive and negative values
        f32 TestValues[] = {-2, -1, -0.5, 0, 0.5, 1, 2, 3};
        for(u32 i = 0; i < Size; ++i)
        {
            Input.Data[i] = TestValues[i];
        }
        
        printf("  Input:  [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.2f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
        
        ReLU(Input.Data, Size);
        
        printf("  Output: [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.2f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
    }
    
    // Test Sigmoid
    {
        printf("\n[TEST] Sigmoid:\n");
        
        f32 TestValues[] = {-3, -2, -1, 0, 1, 2, 3, 4};
        for(u32 i = 0; i < Size; ++i)
        {
            Input.Data[i] = TestValues[i];
        }
        
        printf("  Input:  [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.2f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
        
        Sigmoid(Input.Data, Size);
        
        printf("  Output: [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.3f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
    }
    
    // Test Tanh
    {
        printf("\n[TEST] Tanh:\n");
        
        f32 TestValues[] = {-2, -1, -0.5, 0, 0.5, 1, 2, 3};
        for(u32 i = 0; i < Size; ++i)
        {
            Input.Data[i] = TestValues[i];
        }
        
        printf("  Input:  [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.2f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
        
        Tanh(Input.Data, Size);
        
        printf("  Output: [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.3f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
    }
    
    // Test Softmax
    {
        printf("\n[TEST] Softmax:\n");
        
        f32 TestValues[] = {1, 2, 3, 4, 5, 6, 7, 8};
        for(u32 i = 0; i < Size; ++i)
        {
            Input.Data[i] = TestValues[i];
        }
        
        printf("  Input:  [");
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.2f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
        }
        printf("]\n");
        
        Softmax(Input.Data, Size);
        
        printf("  Output: [");
        f32 Sum = 0;
        for(u32 i = 0; i < Size; ++i)
        {
            printf("%5.4f%s", Input.Data[i], (i < Size - 1) ? ", " : "");
            Sum += Input.Data[i];
        }
        printf("]\n");
        printf("  Sum: %.6f (should be 1.0)\n", Sum);
    }
}

void TestNeuralNetwork(memory_arena *Arena)
{
    printf("\n==================================================\n");
    printf("         NEURAL NETWORK TEST\n");
    printf("==================================================\n");
    
    // Create a simple network: 784 -> 256 -> 128 -> 10 (MNIST-like)
    printf("\n[TEST] Creating 3-layer network (784 -> 256 -> 128 -> 10):\n");
    
    neural_network Network = InitializeNeuralNetwork(Arena, 784, 256, 128, 10);
    
    printf("  Network initialized:\n");
    printf("    Input size:   %u\n", Network.InputSize);
    printf("    Hidden1 size: %u\n", Network.Hidden1Size);
    printf("    Hidden2 size: %u\n", Network.Hidden2Size);
    printf("    Output size:  %u\n", Network.OutputSize);
    printf("    Total parameters: %u\n",
           Network.Layer1.Weights.Rows * Network.Layer1.Weights.Cols +
           Network.Layer2.Weights.Rows * Network.Layer2.Weights.Cols +
           Network.Layer3.Weights.Rows * Network.Layer3.Weights.Cols +
           Network.Hidden1Size + Network.Hidden2Size + Network.OutputSize);
    
    // Test forward pass
    printf("\n[TEST] Forward pass:\n");
    
    neural_vector Input = AllocateVector(Arena, 784);
    neural_vector Output = AllocateVector(Arena, 10);
    
    // Initialize input with mock image data (normalized pixels)
    for(u32 i = 0; i < 784; ++i)
    {
        Input.Data[i] = (f32)(i % 256) / 255.0f;
    }
    
    // Perform forward pass
    u64 StartCycles = ReadCPUTimer();
    ForwardPass(&Network, &Input, &Output);
    u64 ForwardCycles = ReadCPUTimer() - StartCycles;
    
    printf("  Output probabilities:\n    ");
    for(u32 i = 0; i < 10; ++i)
    {
        printf("[%u]: %.4f  ", i, Output.Data[i]);
        if(i == 4) printf("\n    ");
    }
    printf("\n");
    
    // Find predicted class
    u32 PredictedClass = 0;
    f32 MaxProb = Output.Data[0];
    for(u32 i = 1; i < 10; ++i)
    {
        if(Output.Data[i] > MaxProb)
        {
            MaxProb = Output.Data[i];
            PredictedClass = i;
        }
    }
    
    printf("  Predicted class: %u (probability: %.4f)\n", PredictedClass, MaxProb);
    printf("  Forward pass cycles: %llu\n", (unsigned long long)ForwardCycles);
    
    // Test backward pass (simplified)
    printf("\n[TEST] Backward pass (gradient update):\n");
    
    neural_vector Target = AllocateVector(Arena, 10);
    InitializeVectorZero(&Target);
    Target.Data[3] = 1.0f;  // Target class is 3
    
    u64 BackwardStart = ReadCPUTimer();
    BackwardPass(&Network, &Target, 0.01f);  // Learning rate = 0.01
    u64 BackwardCycles = ReadCPUTimer() - BackwardStart;
    
    printf("  Target class: 3\n");
    printf("  Backward pass cycles: %llu\n", (unsigned long long)BackwardCycles);
    
    // Show performance statistics
    if(Network.ForwardCount > 0)
    {
        printf("\n  Performance Statistics:\n");
        printf("    Average forward cycles:  %llu\n", 
               (unsigned long long)(Network.ForwardCycles / Network.ForwardCount));
        printf("    Average backward cycles: %llu\n",
               (unsigned long long)(Network.BackwardCycles / Network.BackwardCount));
    }
}

void TestMemoryPooling(memory_arena *Arena)
{
    printf("\n==================================================\n");
    printf("         MEMORY POOLING TEST\n");
    printf("==================================================\n");
    
    printf("\n[TEST] Weight pool allocation:\n");
    
    // Create a weight pool
    memory_pool *WeightPool = PushStruct(Arena, memory_pool);
    u32 WeightBlockSize = 256 * 256;  // Max weight matrix size
    InitializePool(WeightPool, Arena, WeightBlockSize * sizeof(f32), 10);
    
    printf("  Pool created:\n");
    printf("    Block size: %u floats (%u bytes)\n", 
           WeightBlockSize, WeightBlockSize * (u32)sizeof(f32));
    printf("    Block count: %u\n", WeightPool->BlockCount);
    printf("    Total pool size: %.2f MB\n",
           (f32)(WeightPool->BlockSize * WeightPool->BlockCount) / (1024.0f * 1024.0f));
    
    // Allocate some weight matrices
    void *Weights[5];
    for(u32 i = 0; i < 5; ++i)
    {
        Weights[i] = AllocateWeights(WeightPool, 128 * 128);
        printf("  Allocated weight matrix %u\n", i);
    }
    
    printf("  Used blocks: %u/%u\n", WeightPool->UsedCount, WeightPool->BlockCount);
    
    // Free some weights
    FreeWeights(WeightPool, Weights[1]);
    FreeWeights(WeightPool, Weights[3]);
    printf("\n  Freed weight matrices 1 and 3\n");
    printf("  Used blocks: %u/%u\n", WeightPool->UsedCount, WeightPool->BlockCount);
    
    // Reallocate
    void *NewWeight = AllocateWeights(WeightPool, 64 * 64);
    printf("\n  Allocated new weight matrix\n");
    printf("  Used blocks: %u/%u\n", WeightPool->UsedCount, WeightPool->BlockCount);
}

void RunBenchmarks(memory_arena *Arena)
{
    printf("\n==================================================\n");
    printf("         PERFORMANCE BENCHMARKS\n");
    printf("==================================================\n");
    
    // Matrix multiply benchmark
    BenchmarkMatrixMultiply(Arena);
    
    // Activation function benchmark
    BenchmarkActivations(Arena);
    
    // Forward pass benchmark
    BenchmarkForwardPass(Arena);
    
    // Memory statistics
    printf("\n=== Memory Statistics ===\n");
    printf("  Total allocated: %.2f MB\n", 
           (f64)GlobalMemoryStats.TotalAllocated / (1024.0 * 1024.0));
    printf("  Peak usage: %.2f MB\n",
           (f64)GlobalMemoryStats.PeakUsed / (1024.0 * 1024.0));
    printf("  Allocation count: %u\n", GlobalMemoryStats.AllocationCount);
    
    // Neural statistics
    printf("\n=== Neural Operation Statistics ===\n");
    printf("  Matrix multiplies: %llu\n", 
           (unsigned long long)GlobalNeuralStats.MatrixMultiplies);
    printf("  Vector operations: %llu\n",
           (unsigned long long)GlobalNeuralStats.VectorOperations);
    printf("  Activation calls: %llu\n",
           (unsigned long long)GlobalNeuralStats.ActivationCalls);
    printf("  Compute cycles: %llu\n",
           (unsigned long long)GlobalNeuralStats.ComputeCycles);
}

int main(int argc, char **argv)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║      HANDMADE NEURAL MATH LIBRARY TEST SUITE    ║\n");
    printf("║                                                  ║\n");
    printf("║  Zero-dependency, SIMD-accelerated neural ops   ║\n");
    printf("║  Following the Handmade Hero philosophy         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    
    // Allocate main memory arena (64MB)
    u64 ArenaSize = Megabytes(64);
    void *ArenaMemory = malloc(ArenaSize);
    if(!ArenaMemory)
    {
        printf("ERROR: Failed to allocate memory arena\n");
        return 1;
    }
    
    memory_arena Arena = {0};
    InitializeArena(&Arena, ArenaSize, ArenaMemory);
    
    printf("\n[INIT] Memory arena: %.1f MB allocated\n", 
           (f64)ArenaSize / (1024.0 * 1024.0));
    
    // Check CPU features
    printf("\n[INIT] CPU Features:\n");
#ifdef __AVX2__
    printf("  AVX2: Available\n");
#else
    printf("  AVX2: Not available\n");
#endif
#ifdef __AVX512F__
    printf("  AVX-512: Available\n");
#else
    printf("  AVX-512: Not available\n");
#endif
#ifdef __FMA__
    printf("  FMA: Available\n");
#else
    printf("  FMA: Not available\n");
#endif
    
    printf("  SIMD width: %u floats\n", NEURAL_SIMD_WIDTH);
    printf("  Cache line size: %u bytes\n", CACHE_LINE_SIZE);
    
    // Run tests
    TestMatrixOperations(&Arena);
    TestActivationFunctions(&Arena);
    TestNeuralNetwork(&Arena);
    TestMemoryPooling(&Arena);
    
    // Run benchmarks if requested
    if(argc > 1 && argv[1][0] == 'b')
    {
        RunBenchmarks(&Arena);
    }
    else
    {
        printf("\n[INFO] Run with 'b' argument for full benchmarks\n");
    }
    
    printf("\n==================================================\n");
    printf("                 TEST COMPLETE\n");
    printf("==================================================\n\n");
    
    free(ArenaMemory);
    return 0;
}