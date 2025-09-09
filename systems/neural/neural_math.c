#include "neural_math.h"
#include <math.h>  // Only for initial random values and reference implementations
#include <string.h>  // For memset in debug builds
#include <stdio.h>  // For printf in benchmarking

// Global performance statistics
neural_perf_stats GlobalNeuralStats = {0};

/*
    =================================================================
    MEMORY ALLOCATION AND INITIALIZATION
    =================================================================
*/

neural_matrix 
AllocateMatrix(memory_arena *Arena, u32 Rows, u32 Cols)
{
    neural_matrix Result = {0};
    
    Result.Rows = Rows;
    Result.Cols = Cols;
    Result.Stride = AlignToSIMD(Cols);  // Align columns to SIMD width
    
    // PERFORMANCE: Allocate cache-aligned memory
    // CACHE: Ensure matrix starts on cache line boundary
    u32 TotalSize = Result.Rows * Result.Stride * sizeof(f32);
    Result.Data = (f32 *)PushSizeAligned(Arena, TotalSize, CACHE_LINE_SIZE);
    
    // Initialize to zero for safety
    InitializeMatrixZero(&Result);
    
    RecordAllocation(TotalSize);
    GlobalNeuralStats.MatrixMultiplies++;
    
    return Result;
}

neural_matrix 
AllocateMatrixFromPool(memory_pool *Pool, u32 Rows, u32 Cols)
{
    neural_matrix Result = {0};
    
    Result.Rows = Rows;
    Result.Cols = Cols;
    Result.Stride = AlignToSIMD(Cols);
    Result.Pool = Pool;
    
    u32 TotalSize = Result.Rows * Result.Stride * sizeof(f32);
    Assert(TotalSize <= Pool->BlockSize);  // Ensure it fits in a pool block
    
    Result.Data = (f32 *)PoolAlloc(Pool);
    InitializeMatrixZero(&Result);
    
    return Result;
}

neural_vector 
AllocateVector(memory_arena *Arena, u32 Size)
{
    neural_vector Result = {0};
    
    Result.Size = Size;
    Result.Stride = AlignToSIMD(Size);
    
    u32 TotalSize = Result.Stride * sizeof(f32);
    Result.Data = (f32 *)PushSizeAligned(Arena, TotalSize, CACHE_LINE_SIZE);
    
    InitializeVectorZero(&Result);
    
    return Result;
}

void 
InitializeMatrixRandom(neural_matrix *Matrix, f32 Scale)
{
    // Xavier/He initialization
    f32 StdDev = Scale / sqrtf((f32)Matrix->Cols);
    
    // Simple LCG for deterministic initialization
    u32 Seed = 0x12345678;
    
    for(u32 Row = 0; Row < Matrix->Rows; ++Row)
    {
        f32 *RowData = Matrix->Data + Row * Matrix->Stride;
        for(u32 Col = 0; Col < Matrix->Cols; ++Col)
        {
            // LCG: Next = (a * Seed + c) mod m
            Seed = (1664525 * Seed + 1013904223);
            f32 Random = ((Seed >> 16) & 0x7FFF) / 32768.0f;  // [0, 1)
            Random = (Random - 0.5f) * 2.0f;  // [-1, 1)
            RowData[Col] = Random * StdDev;
        }
    }
}

void 
InitializeMatrixZero(neural_matrix *Matrix)
{
    // PERFORMANCE: Use SIMD for large clears
    u32 TotalElements = Matrix->Rows * Matrix->Stride;
    
#if NEURAL_USE_AVX2
    __m256 Zero = _mm256_setzero_ps();
    u32 SimdCount = TotalElements / 8;
    
    for(u32 i = 0; i < SimdCount; ++i)
    {
        _mm256_store_ps(Matrix->Data + i * 8, Zero);
    }
    
    // Handle remainder
    for(u32 i = SimdCount * 8; i < TotalElements; ++i)
    {
        Matrix->Data[i] = 0.0f;
    }
#else
    for(u32 i = 0; i < TotalElements; ++i)
    {
        Matrix->Data[i] = 0.0f;
    }
#endif
}

void 
InitializeVectorZero(neural_vector *Vector)
{
    for(u32 i = 0; i < Vector->Stride; ++i)
    {
        Vector->Data[i] = 0.0f;
    }
}

/*
    =================================================================
    SCALAR MATRIX OPERATIONS (Reference implementations)
    =================================================================
*/

void 
MatrixMultiply_Scalar(neural_matrix *C, neural_matrix *A, neural_matrix *B)
{
    // PERFORMANCE: Naive implementation - 3 nested loops
    // CACHE: Poor locality, many cache misses
    Assert(A->Cols == B->Rows);
    Assert(C->Rows == A->Rows);
    Assert(C->Cols == B->Cols);
    
    u64 StartCycles = ReadCPUTimer();
    
    for(u32 i = 0; i < C->Rows; ++i)
    {
        for(u32 j = 0; j < C->Cols; ++j)
        {
            f32 Sum = 0.0f;
            f32 *ARow = A->Data + i * A->Stride;
            
            for(u32 k = 0; k < A->Cols; ++k)
            {
                f32 *BElement = B->Data + k * B->Stride + j;
                Sum += ARow[k] * (*BElement);
            }
            
            C->Data[i * C->Stride + j] = Sum;
        }
    }
    
    GlobalNeuralStats.ComputeCycles += ReadCPUTimer() - StartCycles;
    GlobalNeuralStats.MatrixMultiplies++;
}

void 
MatrixVectorMultiply_Scalar(neural_vector *y, neural_matrix *A, neural_vector *x)
{
    Assert(A->Cols == x->Size);
    Assert(y->Size == A->Rows);
    
    for(u32 i = 0; i < A->Rows; ++i)
    {
        f32 Sum = 0.0f;
        f32 *ARow = A->Data + i * A->Stride;
        
        for(u32 j = 0; j < A->Cols; ++j)
        {
            Sum += ARow[j] * x->Data[j];
        }
        
        y->Data[i] = Sum;
    }
    
    GlobalNeuralStats.VectorOperations++;
}

void 
MatrixTranspose_Scalar(neural_matrix *AT, neural_matrix *A)
{
    Assert(AT->Rows == A->Cols);
    Assert(AT->Cols == A->Rows);
    
    for(u32 i = 0; i < A->Rows; ++i)
    {
        for(u32 j = 0; j < A->Cols; ++j)
        {
            AT->Data[j * AT->Stride + i] = A->Data[i * A->Stride + j];
        }
    }
}

void 
MatrixAdd_Scalar(neural_matrix *C, neural_matrix *A, neural_matrix *B)
{
    Assert(A->Rows == B->Rows && A->Rows == C->Rows);
    Assert(A->Cols == B->Cols && A->Cols == C->Cols);
    
    for(u32 i = 0; i < A->Rows; ++i)
    {
        f32 *ARow = A->Data + i * A->Stride;
        f32 *BRow = B->Data + i * B->Stride;
        f32 *CRow = C->Data + i * C->Stride;
        
        for(u32 j = 0; j < A->Cols; ++j)
        {
            CRow[j] = ARow[j] + BRow[j];
        }
    }
}

void 
MatrixScale_Scalar(neural_matrix *A, f32 Scale)
{
    for(u32 i = 0; i < A->Rows; ++i)
    {
        f32 *Row = A->Data + i * A->Stride;
        for(u32 j = 0; j < A->Cols; ++j)
        {
            Row[j] *= Scale;
        }
    }
}

/*
    =================================================================
    AVX2 OPTIMIZED MATRIX OPERATIONS
    =================================================================
*/

#if NEURAL_USE_AVX2

void 
MatrixMultiply_AVX2(neural_matrix *C, neural_matrix *A, neural_matrix *B)
{
    // PERFORMANCE: Hot path - optimized with AVX2 FMA instructions
    // CACHE: Tiled for L1 cache, processes 8 floats per instruction
    Assert(A->Cols == B->Rows);
    Assert(C->Rows == A->Rows);
    Assert(C->Cols == B->Cols);
    
    u64 StartCycles = ReadCPUTimer();
    
    // Initialize C to zero
    InitializeMatrixZero(C);
    
    // Process 8 columns at a time
    u32 ColChunks = C->Cols / 8;
    u32 ColRemainder = C->Cols % 8;
    
    for(u32 i = 0; i < C->Rows; ++i)
    {
        f32 *CRow = C->Data + i * C->Stride;
        f32 *ARow = A->Data + i * A->Stride;
        
        // Process 8 columns at once
        for(u32 j = 0; j < ColChunks; ++j)
        {
            __m256 Sum = _mm256_setzero_ps();
            
            // Accumulate dot products
            for(u32 k = 0; k < A->Cols; ++k)
            {
                __m256 AElement = _mm256_broadcast_ss(&ARow[k]);
                __m256 BRow = _mm256_load_ps(B->Data + k * B->Stride + j * 8);
                
                // FMA: Sum = Sum + (AElement * BRow)
                Sum = _mm256_fmadd_ps(AElement, BRow, Sum);
                
                // Prefetch next cache line
                if((k & 7) == 0 && k + 8 < A->Cols)
                {
                    PrefetchL1(B->Data + (k + 8) * B->Stride + j * 8);
                }
            }
            
            _mm256_store_ps(&CRow[j * 8], Sum);
        }
        
        // Handle remaining columns
        for(u32 j = ColChunks * 8; j < C->Cols; ++j)
        {
            f32 Sum = 0.0f;
            for(u32 k = 0; k < A->Cols; ++k)
            {
                Sum += ARow[k] * B->Data[k * B->Stride + j];
            }
            CRow[j] = Sum;
        }
    }
    
    u64 Cycles = ReadCPUTimer() - StartCycles;
    GlobalNeuralStats.ComputeCycles += Cycles;
    GlobalNeuralStats.MatrixMultiplies++;
}

void 
MatrixVectorMultiply_AVX2(neural_vector *y, neural_matrix *A, neural_vector *x)
{
    // PERFORMANCE: Optimized matrix-vector multiply using AVX2
    // CACHE: Sequential access pattern for A, broadcast for x
    Assert(A->Cols == x->Size);
    Assert(y->Size == A->Rows);
    
    u64 StartCycles = ReadCPUTimer();
    
    u32 ColChunks = A->Cols / 8;
    u32 ColRemainder = A->Cols % 8;
    
    for(u32 i = 0; i < A->Rows; ++i)
    {
        f32 *ARow = A->Data + i * A->Stride;
        __m256 Sum = _mm256_setzero_ps();
        
        // Process 8 elements at once
        for(u32 j = 0; j < ColChunks; ++j)
        {
            __m256 AElements = _mm256_load_ps(&ARow[j * 8]);
            __m256 XElements = _mm256_load_ps(&x->Data[j * 8]);
            Sum = _mm256_fmadd_ps(AElements, XElements, Sum);
        }
        
        // Horizontal sum of vector
        __m128 Sum128 = _mm256_extractf128_ps(Sum, 1);
        Sum128 = _mm_add_ps(Sum128, _mm256_castps256_ps128(Sum));
        Sum128 = _mm_hadd_ps(Sum128, Sum128);
        Sum128 = _mm_hadd_ps(Sum128, Sum128);
        f32 Result = _mm_cvtss_f32(Sum128);
        
        // Handle remainder
        for(u32 j = ColChunks * 8; j < A->Cols; ++j)
        {
            Result += ARow[j] * x->Data[j];
        }
        
        y->Data[i] = Result;
    }
    
    GlobalNeuralStats.ComputeCycles += ReadCPUTimer() - StartCycles;
    GlobalNeuralStats.VectorOperations++;
}

void 
MatrixTranspose_AVX2(neural_matrix *AT, neural_matrix *A)
{
    // PERFORMANCE: Cache-blocked transpose with AVX2
    // CACHE: Process 8x8 blocks for cache efficiency
    Assert(AT->Rows == A->Cols);
    Assert(AT->Cols == A->Rows);
    
    const u32 BlockSize = 8;
    u32 RowBlocks = A->Rows / BlockSize;
    u32 ColBlocks = A->Cols / BlockSize;
    
    // Process 8x8 blocks
    for(u32 i = 0; i < RowBlocks; ++i)
    {
        for(u32 j = 0; j < ColBlocks; ++j)
        {
            // Load 8x8 block
            __m256 Row0 = _mm256_load_ps(A->Data + (i*8+0) * A->Stride + j*8);
            __m256 Row1 = _mm256_load_ps(A->Data + (i*8+1) * A->Stride + j*8);
            __m256 Row2 = _mm256_load_ps(A->Data + (i*8+2) * A->Stride + j*8);
            __m256 Row3 = _mm256_load_ps(A->Data + (i*8+3) * A->Stride + j*8);
            __m256 Row4 = _mm256_load_ps(A->Data + (i*8+4) * A->Stride + j*8);
            __m256 Row5 = _mm256_load_ps(A->Data + (i*8+5) * A->Stride + j*8);
            __m256 Row6 = _mm256_load_ps(A->Data + (i*8+6) * A->Stride + j*8);
            __m256 Row7 = _mm256_load_ps(A->Data + (i*8+7) * A->Stride + j*8);
            
            // Transpose 8x8 block using shuffle operations
            __m256 T0 = _mm256_unpacklo_ps(Row0, Row1);
            __m256 T1 = _mm256_unpackhi_ps(Row0, Row1);
            __m256 T2 = _mm256_unpacklo_ps(Row2, Row3);
            __m256 T3 = _mm256_unpackhi_ps(Row2, Row3);
            __m256 T4 = _mm256_unpacklo_ps(Row4, Row5);
            __m256 T5 = _mm256_unpackhi_ps(Row4, Row5);
            __m256 T6 = _mm256_unpacklo_ps(Row6, Row7);
            __m256 T7 = _mm256_unpackhi_ps(Row6, Row7);
            
            Row0 = _mm256_shuffle_ps(T0, T2, 0x44);
            Row1 = _mm256_shuffle_ps(T0, T2, 0xEE);
            Row2 = _mm256_shuffle_ps(T1, T3, 0x44);
            Row3 = _mm256_shuffle_ps(T1, T3, 0xEE);
            Row4 = _mm256_shuffle_ps(T4, T6, 0x44);
            Row5 = _mm256_shuffle_ps(T4, T6, 0xEE);
            Row6 = _mm256_shuffle_ps(T5, T7, 0x44);
            Row7 = _mm256_shuffle_ps(T5, T7, 0xEE);
            
            // Store transposed block
            _mm256_store_ps(AT->Data + (j*8+0) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row0, Row4, 0x20));
            _mm256_store_ps(AT->Data + (j*8+1) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row1, Row5, 0x20));
            _mm256_store_ps(AT->Data + (j*8+2) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row2, Row6, 0x20));
            _mm256_store_ps(AT->Data + (j*8+3) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row3, Row7, 0x20));
            _mm256_store_ps(AT->Data + (j*8+4) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row0, Row4, 0x31));
            _mm256_store_ps(AT->Data + (j*8+5) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row1, Row5, 0x31));
            _mm256_store_ps(AT->Data + (j*8+6) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row2, Row6, 0x31));
            _mm256_store_ps(AT->Data + (j*8+7) * AT->Stride + i*8, 
                           _mm256_permute2f128_ps(Row3, Row7, 0x31));
        }
    }
    
    // Handle remainder with scalar code
    for(u32 i = 0; i < A->Rows; ++i)
    {
        for(u32 j = ColBlocks * BlockSize; j < A->Cols; ++j)
        {
            AT->Data[j * AT->Stride + i] = A->Data[i * A->Stride + j];
        }
    }
    
    for(u32 i = RowBlocks * BlockSize; i < A->Rows; ++i)
    {
        for(u32 j = 0; j < A->Cols; ++j)
        {
            AT->Data[j * AT->Stride + i] = A->Data[i * A->Stride + j];
        }
    }
}

void 
MatrixAdd_AVX2(neural_matrix *C, neural_matrix *A, neural_matrix *B)
{
    // PERFORMANCE: Vectorized addition, 8 floats per cycle
    Assert(A->Rows == B->Rows && A->Rows == C->Rows);
    Assert(A->Cols == B->Cols && A->Cols == C->Cols);
    
    u32 ColChunks = A->Cols / 8;
    
    for(u32 i = 0; i < A->Rows; ++i)
    {
        f32 *ARow = A->Data + i * A->Stride;
        f32 *BRow = B->Data + i * B->Stride;
        f32 *CRow = C->Data + i * C->Stride;
        
        for(u32 j = 0; j < ColChunks; ++j)
        {
            __m256 AVec = _mm256_load_ps(&ARow[j * 8]);
            __m256 BVec = _mm256_load_ps(&BRow[j * 8]);
            __m256 CVec = _mm256_add_ps(AVec, BVec);
            _mm256_store_ps(&CRow[j * 8], CVec);
        }
        
        // Handle remainder
        for(u32 j = ColChunks * 8; j < A->Cols; ++j)
        {
            CRow[j] = ARow[j] + BRow[j];
        }
    }
}

void 
MatrixScale_AVX2(neural_matrix *A, f32 Scale)
{
    __m256 ScaleVec = _mm256_set1_ps(Scale);
    u32 TotalElements = A->Rows * A->Stride;
    u32 Chunks = TotalElements / 8;
    
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 Values = _mm256_load_ps(&A->Data[i * 8]);
        Values = _mm256_mul_ps(Values, ScaleVec);
        _mm256_store_ps(&A->Data[i * 8], Values);
    }
    
    // Handle remainder
    for(u32 i = Chunks * 8; i < TotalElements; ++i)
    {
        A->Data[i] *= Scale;
    }
}

#endif // NEURAL_USE_AVX2

/*
    =================================================================
    CACHE-BLOCKED MATRIX MULTIPLICATION
    =================================================================
*/

void 
MatrixMultiply_Blocked(neural_matrix *C, neural_matrix *A, neural_matrix *B)
{
    // PERFORMANCE: Cache-blocked GEMM for large matrices
    // CACHE: Tiled to fit in L1/L2 cache
    Assert(A->Cols == B->Rows);
    Assert(C->Rows == A->Rows);
    Assert(C->Cols == B->Cols);
    
    const u32 BlockSize = NEURAL_CACHE_BLOCK_SIZE;
    
    // Initialize C to zero
    InitializeMatrixZero(C);
    
    // Tile loops for cache blocking
    for(u32 ii = 0; ii < C->Rows; ii += BlockSize)
    {
        u32 iMax = Minimum(ii + BlockSize, C->Rows);
        
        for(u32 kk = 0; kk < A->Cols; kk += BlockSize)
        {
            u32 kMax = Minimum(kk + BlockSize, A->Cols);
            
            for(u32 jj = 0; jj < C->Cols; jj += BlockSize)
            {
                u32 jMax = Minimum(jj + BlockSize, C->Cols);
                
                // Multiply block
                for(u32 i = ii; i < iMax; ++i)
                {
                    f32 *CRow = C->Data + i * C->Stride;
                    f32 *ARow = A->Data + i * A->Stride;
                    
                    for(u32 k = kk; k < kMax; ++k)
                    {
                        f32 AElement = ARow[k];
                        f32 *BRow = B->Data + k * B->Stride;
                        
#if NEURAL_USE_AVX2
                        // Process 8 columns at once within block
                        u32 j = jj;
                        for(; j + 8 <= jMax; j += 8)
                        {
                            __m256 CVec = _mm256_load_ps(&CRow[j]);
                            __m256 BVec = _mm256_load_ps(&BRow[j]);
                            __m256 AVec = _mm256_set1_ps(AElement);
                            CVec = _mm256_fmadd_ps(AVec, BVec, CVec);
                            _mm256_store_ps(&CRow[j], CVec);
                        }
                        
                        // Handle remainder
                        for(; j < jMax; ++j)
                        {
                            CRow[j] += AElement * BRow[j];
                        }
#else
                        for(u32 j = jj; j < jMax; ++j)
                        {
                            CRow[j] += AElement * BRow[j];
                        }
#endif
                    }
                }
            }
        }
    }
    
    GlobalNeuralStats.MatrixMultiplies++;
}

/*
    =================================================================
    ACTIVATION FUNCTIONS - SCALAR
    =================================================================
*/

void 
ReLU_Scalar(f32 *Output, u32 Size)
{
    // PERFORMANCE: Simple branch-free ReLU
    for(u32 i = 0; i < Size; ++i)
    {
        Output[i] = (Output[i] > 0.0f) ? Output[i] : 0.0f;
    }
    GlobalNeuralStats.ActivationCalls++;
}

void 
ReLU_Derivative_Scalar(f32 *Gradient, f32 *Output, u32 Size)
{
    for(u32 i = 0; i < Size; ++i)
    {
        Gradient[i] *= (Output[i] > 0.0f) ? 1.0f : 0.0f;
    }
}

void 
Sigmoid_Scalar(f32 *Output, u32 Size)
{
    for(u32 i = 0; i < Size; ++i)
    {
        Output[i] = 1.0f / (1.0f + FastExp(-Output[i]));
    }
    GlobalNeuralStats.ActivationCalls++;
}

void 
Sigmoid_Derivative_Scalar(f32 *Gradient, f32 *Output, u32 Size)
{
    for(u32 i = 0; i < Size; ++i)
    {
        f32 Sig = Output[i];
        Gradient[i] *= Sig * (1.0f - Sig);
    }
}

void 
Tanh_Scalar(f32 *Output, u32 Size)
{
    for(u32 i = 0; i < Size; ++i)
    {
        Output[i] = FastTanh(Output[i]);
    }
    GlobalNeuralStats.ActivationCalls++;
}

void 
Tanh_Derivative_Scalar(f32 *Gradient, f32 *Output, u32 Size)
{
    for(u32 i = 0; i < Size; ++i)
    {
        f32 T = Output[i];
        Gradient[i] *= (1.0f - T * T);
    }
}

void 
Softmax_Scalar(f32 *Output, u32 Size)
{
    // Find max for numerical stability
    f32 Max = Output[0];
    for(u32 i = 1; i < Size; ++i)
    {
        if(Output[i] > Max) Max = Output[i];
    }
    
    // Compute exp(x - max) and sum
    f32 Sum = 0.0f;
    for(u32 i = 0; i < Size; ++i)
    {
        Output[i] = FastExp(Output[i] - Max);
        Sum += Output[i];
    }
    
    // Normalize
    f32 InvSum = 1.0f / Sum;
    for(u32 i = 0; i < Size; ++i)
    {
        Output[i] *= InvSum;
    }
    
    GlobalNeuralStats.ActivationCalls++;
}

/*
    =================================================================
    ACTIVATION FUNCTIONS - AVX2
    =================================================================
*/

#if NEURAL_USE_AVX2

void 
ReLU_AVX2(f32 *Output, u32 Size)
{
    // PERFORMANCE: Vectorized ReLU, 8 floats per cycle
    __m256 Zero = _mm256_setzero_ps();
    u32 Chunks = Size / 8;
    
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 Values = _mm256_load_ps(&Output[i * 8]);
        Values = _mm256_max_ps(Values, Zero);
        _mm256_store_ps(&Output[i * 8], Values);
    }
    
    // Handle remainder
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        Output[i] = (Output[i] > 0.0f) ? Output[i] : 0.0f;
    }
    
    GlobalNeuralStats.ActivationCalls++;
}

void 
ReLU_Derivative_AVX2(f32 *Gradient, f32 *Output, u32 Size)
{
    __m256 Zero = _mm256_setzero_ps();
    __m256 One = _mm256_set1_ps(1.0f);
    u32 Chunks = Size / 8;
    
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 Out = _mm256_load_ps(&Output[i * 8]);
        __m256 Grad = _mm256_load_ps(&Gradient[i * 8]);
        __m256 Mask = _mm256_cmp_ps(Out, Zero, _CMP_GT_OS);
        __m256 Derivative = _mm256_blendv_ps(Zero, One, Mask);
        Grad = _mm256_mul_ps(Grad, Derivative);
        _mm256_store_ps(&Gradient[i * 8], Grad);
    }
    
    // Handle remainder
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        Gradient[i] *= (Output[i] > 0.0f) ? 1.0f : 0.0f;
    }
}

void 
Sigmoid_AVX2(f32 *Output, u32 Size)
{
    // PERFORMANCE: Vectorized sigmoid with fast exp approximation
    __m256 One = _mm256_set1_ps(1.0f);
    __m256 NegOne = _mm256_set1_ps(-1.0f);
    u32 Chunks = Size / 8;
    
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 X = _mm256_load_ps(&Output[i * 8]);
        X = _mm256_mul_ps(X, NegOne);  // -x
        
        // Fast exp approximation for AVX2
        // exp(x) ≈ 2^(x * 1.442695)
        __m256 Scale = _mm256_set1_ps(1.442695f);
        X = _mm256_mul_ps(X, Scale);
        
        // Extract integer and fractional parts
        __m256 IntPart = _mm256_floor_ps(X);
        __m256 FracPart = _mm256_sub_ps(X, IntPart);
        
        // Polynomial approximation of 2^frac
        __m256 P0 = _mm256_set1_ps(1.0f);
        __m256 P1 = _mm256_set1_ps(0.6931472f);
        __m256 P2 = _mm256_set1_ps(0.2402265f);
        __m256 P3 = _mm256_set1_ps(0.0554906f);
        
        __m256 Exp2Frac = _mm256_fmadd_ps(FracPart, P3, P2);
        Exp2Frac = _mm256_fmadd_ps(FracPart, Exp2Frac, P1);
        Exp2Frac = _mm256_fmadd_ps(FracPart, Exp2Frac, P0);
        
        // Scale by 2^int using bit manipulation
        __m256i IntPartI = _mm256_cvtps_epi32(IntPart);
        IntPartI = _mm256_add_epi32(IntPartI, _mm256_set1_epi32(127));
        IntPartI = _mm256_slli_epi32(IntPartI, 23);
        __m256 Exp2Int = _mm256_castsi256_ps(IntPartI);
        
        __m256 ExpX = _mm256_mul_ps(Exp2Frac, Exp2Int);
        
        // sigmoid = 1 / (1 + exp(-x))
        ExpX = _mm256_add_ps(One, ExpX);
        __m256 Result = _mm256_div_ps(One, ExpX);
        
        _mm256_store_ps(&Output[i * 8], Result);
    }
    
    // Handle remainder with scalar code
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        Output[i] = 1.0f / (1.0f + FastExp(-Output[i]));
    }
    
    GlobalNeuralStats.ActivationCalls++;
}

void 
Tanh_AVX2(f32 *Output, u32 Size)
{
    // PERFORMANCE: Vectorized tanh using rational approximation
    u32 Chunks = Size / 8;
    
    __m256 C27 = _mm256_set1_ps(27.0f);
    __m256 C9 = _mm256_set1_ps(9.0f);
    
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 X = _mm256_load_ps(&Output[i * 8]);
        __m256 X2 = _mm256_mul_ps(X, X);
        
        // tanh(x) ≈ x * (27 + x²) / (27 + 9*x²)
        __m256 Numerator = _mm256_add_ps(C27, X2);
        Numerator = _mm256_mul_ps(X, Numerator);
        
        __m256 Denominator = _mm256_mul_ps(C9, X2);
        Denominator = _mm256_add_ps(C27, Denominator);
        
        __m256 Result = _mm256_div_ps(Numerator, Denominator);
        _mm256_store_ps(&Output[i * 8], Result);
    }
    
    // Handle remainder
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        Output[i] = FastTanh(Output[i]);
    }
    
    GlobalNeuralStats.ActivationCalls++;
}

void 
Softmax_AVX2(f32 *Output, u32 Size)
{
    // Find max for numerical stability
    __m256 MaxVec = _mm256_load_ps(&Output[0]);
    u32 Chunks = Size / 8;
    
    for(u32 i = 1; i < Chunks; ++i)
    {
        __m256 Values = _mm256_load_ps(&Output[i * 8]);
        MaxVec = _mm256_max_ps(MaxVec, Values);
    }
    
    // Horizontal max
    __m128 Max128 = _mm_max_ps(_mm256_extractf128_ps(MaxVec, 1), 
                                _mm256_castps256_ps128(MaxVec));
    Max128 = _mm_max_ps(Max128, _mm_shuffle_ps(Max128, Max128, 0x0E));
    Max128 = _mm_max_ps(Max128, _mm_shuffle_ps(Max128, Max128, 0x01));
    f32 Max = _mm_cvtss_f32(Max128);
    
    // Check remainder for max
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        if(Output[i] > Max) Max = Output[i];
    }
    
    // Compute exp(x - max) and sum
    __m256 MaxBroadcast = _mm256_set1_ps(Max);
    __m256 SumVec = _mm256_setzero_ps();
    
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 Values = _mm256_load_ps(&Output[i * 8]);
        Values = _mm256_sub_ps(Values, MaxBroadcast);
        
        // Fast exp (simplified for softmax range)
        __m256 Scale = _mm256_set1_ps(1.442695f);
        Values = _mm256_mul_ps(Values, Scale);
        
        // Clamp to prevent overflow
        __m256 MinVal = _mm256_set1_ps(-87.0f);
        __m256 MaxVal = _mm256_set1_ps(87.0f);
        Values = _mm256_max_ps(Values, MinVal);
        Values = _mm256_min_ps(Values, MaxVal);
        
        // Convert to 2^x
        __m256 IntPart = _mm256_floor_ps(Values);
        __m256 FracPart = _mm256_sub_ps(Values, IntPart);
        
        // Polynomial approximation
        __m256 Exp2Frac = _mm256_set1_ps(1.0f);
        Exp2Frac = _mm256_fmadd_ps(FracPart, _mm256_set1_ps(0.693147f), Exp2Frac);
        
        // Scale by 2^int
        __m256i IntPartI = _mm256_cvtps_epi32(IntPart);
        IntPartI = _mm256_add_epi32(IntPartI, _mm256_set1_epi32(127));
        IntPartI = _mm256_slli_epi32(IntPartI, 23);
        __m256 ExpValues = _mm256_mul_ps(Exp2Frac, _mm256_castsi256_ps(IntPartI));
        
        _mm256_store_ps(&Output[i * 8], ExpValues);
        SumVec = _mm256_add_ps(SumVec, ExpValues);
    }
    
    // Horizontal sum
    __m128 Sum128 = _mm_add_ps(_mm256_extractf128_ps(SumVec, 1), 
                                _mm256_castps256_ps128(SumVec));
    Sum128 = _mm_hadd_ps(Sum128, Sum128);
    Sum128 = _mm_hadd_ps(Sum128, Sum128);
    f32 Sum = _mm_cvtss_f32(Sum128);
    
    // Handle remainder
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        Output[i] = FastExp(Output[i] - Max);
        Sum += Output[i];
    }
    
    // Normalize
    __m256 InvSum = _mm256_set1_ps(1.0f / Sum);
    for(u32 i = 0; i < Chunks; ++i)
    {
        __m256 Values = _mm256_load_ps(&Output[i * 8]);
        Values = _mm256_mul_ps(Values, InvSum);
        _mm256_store_ps(&Output[i * 8], Values);
    }
    
    for(u32 i = Chunks * 8; i < Size; ++i)
    {
        Output[i] /= Sum;
    }
    
    GlobalNeuralStats.ActivationCalls++;
}

#endif // NEURAL_USE_AVX2

/*
    =================================================================
    NEURAL NETWORK OPERATIONS
    =================================================================
*/

neural_network 
InitializeNeuralNetwork(memory_arena *Arena, u32 InputSize, u32 Hidden1Size, 
                        u32 Hidden2Size, u32 OutputSize)
{
    neural_network Network = {0};
    
    Network.InputSize = InputSize;
    Network.Hidden1Size = Hidden1Size;
    Network.Hidden2Size = Hidden2Size;
    Network.OutputSize = OutputSize;
    Network.Arena = Arena;
    
    // Create memory pools for weights and activations
    u32 MaxWeightSize = Maximum(Maximum(InputSize * Hidden1Size, 
                                        Hidden1Size * Hidden2Size),
                                Hidden2Size * OutputSize);
    MaxWeightSize = AlignToSIMD(MaxWeightSize) * sizeof(f32);
    
    Network.WeightPool = PushStruct(Arena, memory_pool);
    InitializePool(Network.WeightPool, Arena, MaxWeightSize, 6);  // 6 weight matrices
    
    u32 MaxActivationSize = Maximum(Maximum(Hidden1Size, Hidden2Size), OutputSize);
    MaxActivationSize = AlignToSIMD(MaxActivationSize) * sizeof(f32);
    
    Network.ActivationPool = PushStruct(Arena, memory_pool);
    InitializePool(Network.ActivationPool, Arena, MaxActivationSize, 12);  // Activations and gradients
    
    // Initialize Layer 1 (Input -> Hidden1)
    Network.Layer1.Weights = AllocateMatrix(Arena, Hidden1Size, InputSize);
    InitializeMatrixRandom(&Network.Layer1.Weights, 2.0f);  // He initialization
    
    Network.Layer1.Bias = AllocateVector(Arena, Hidden1Size);
    InitializeVectorZero(&Network.Layer1.Bias);
    
    Network.Layer1.Output = AllocateVector(Arena, Hidden1Size);
    Network.Layer1.Gradient = AllocateVector(Arena, Hidden1Size);
    
    Network.Layer1.Activation = ReLU;
    Network.Layer1.ActivationDerivative = ReLU_Derivative_AVX2;
    
    // Initialize Layer 2 (Hidden1 -> Hidden2)
    Network.Layer2.Weights = AllocateMatrix(Arena, Hidden2Size, Hidden1Size);
    InitializeMatrixRandom(&Network.Layer2.Weights, 2.0f);
    
    Network.Layer2.Bias = AllocateVector(Arena, Hidden2Size);
    InitializeVectorZero(&Network.Layer2.Bias);
    
    Network.Layer2.Output = AllocateVector(Arena, Hidden2Size);
    Network.Layer2.Gradient = AllocateVector(Arena, Hidden2Size);
    
    Network.Layer2.Activation = ReLU;
    Network.Layer2.ActivationDerivative = ReLU_Derivative_AVX2;
    
    // Initialize Layer 3 (Hidden2 -> Output)
    Network.Layer3.Weights = AllocateMatrix(Arena, OutputSize, Hidden2Size);
    InitializeMatrixRandom(&Network.Layer3.Weights, 1.0f);  // Xavier initialization for output
    
    Network.Layer3.Bias = AllocateVector(Arena, OutputSize);
    InitializeVectorZero(&Network.Layer3.Bias);
    
    Network.Layer3.Output = AllocateVector(Arena, OutputSize);
    Network.Layer3.Gradient = AllocateVector(Arena, OutputSize);
    
    Network.Layer3.Activation = Softmax;
    Network.Layer3.ActivationDerivative = 0;  // Softmax derivative handled separately
    
    return Network;
}

void 
ForwardPass(neural_network *Network, neural_vector *Input, neural_vector *Output)
{
    // PERFORMANCE: Forward propagation through 3-layer network
    u64 StartCycles = ReadCPUTimer();
    
    // Layer 1: Input -> Hidden1
    MatrixVectorMultiply(&Network->Layer1.Output, &Network->Layer1.Weights, Input);
    
    // Add bias
    for(u32 i = 0; i < Network->Layer1.Output.Size; ++i)
    {
        Network->Layer1.Output.Data[i] += Network->Layer1.Bias.Data[i];
    }
    
    // Apply activation
    Network->Layer1.Activation(Network->Layer1.Output.Data, Network->Layer1.Output.Size);
    
    // Layer 2: Hidden1 -> Hidden2
    MatrixVectorMultiply(&Network->Layer2.Output, &Network->Layer2.Weights, &Network->Layer1.Output);
    
    for(u32 i = 0; i < Network->Layer2.Output.Size; ++i)
    {
        Network->Layer2.Output.Data[i] += Network->Layer2.Bias.Data[i];
    }
    
    Network->Layer2.Activation(Network->Layer2.Output.Data, Network->Layer2.Output.Size);
    
    // Layer 3: Hidden2 -> Output
    MatrixVectorMultiply(&Network->Layer3.Output, &Network->Layer3.Weights, &Network->Layer2.Output);
    
    for(u32 i = 0; i < Network->Layer3.Output.Size; ++i)
    {
        Network->Layer3.Output.Data[i] += Network->Layer3.Bias.Data[i];
    }
    
    Network->Layer3.Activation(Network->Layer3.Output.Data, Network->Layer3.Output.Size);
    
    // Copy to output
    for(u32 i = 0; i < Output->Size; ++i)
    {
        Output->Data[i] = Network->Layer3.Output.Data[i];
    }
    
    u64 Cycles = ReadCPUTimer() - StartCycles;
    Network->ForwardCycles += Cycles;
    Network->ForwardCount++;
}

void 
BackwardPass(neural_network *Network, neural_vector *Target, f32 LearningRate)
{
    // PERFORMANCE: Simplified backpropagation (full implementation would be more complex)
    u64 StartCycles = ReadCPUTimer();
    
    // Compute output layer gradient (cross-entropy with softmax)
    for(u32 i = 0; i < Network->OutputSize; ++i)
    {
        Network->Layer3.Gradient.Data[i] = 
            Network->Layer3.Output.Data[i] - Target->Data[i];
    }
    
    // Update Layer 3 weights (simplified - would need full gradient computation)
    for(u32 i = 0; i < Network->Layer3.Weights.Rows; ++i)
    {
        for(u32 j = 0; j < Network->Layer3.Weights.Cols; ++j)
        {
            f32 Gradient = Network->Layer3.Gradient.Data[i] * 
                          Network->Layer2.Output.Data[j];
            Network->Layer3.Weights.Data[i * Network->Layer3.Weights.Stride + j] -= 
                LearningRate * Gradient;
        }
    }
    
    // Update biases
    for(u32 i = 0; i < Network->OutputSize; ++i)
    {
        Network->Layer3.Bias.Data[i] -= LearningRate * Network->Layer3.Gradient.Data[i];
    }
    
    // Note: Full backpropagation would continue through all layers
    // This is a simplified version for demonstration
    
    u64 Cycles = ReadCPUTimer() - StartCycles;
    Network->BackwardCycles += Cycles;
    Network->BackwardCount++;
}

/*
    =================================================================
    MEMORY POOLING
    =================================================================
*/

memory_pool 
CreateWeightPool(memory_arena *Arena, u32 MaxWeights)
{
    memory_pool Pool = {0};
    u32 BlockSize = AlignToSIMD(MaxWeights) * sizeof(f32);
    InitializePool(&Pool, Arena, BlockSize, 32);  // 32 weight blocks
    return Pool;
}

void *
AllocateWeights(memory_pool *Pool, u32 Count)
{
    u32 RequiredSize = AlignToSIMD(Count) * sizeof(f32);
    Assert(RequiredSize <= Pool->BlockSize);
    return PoolAlloc(Pool);
}

void 
FreeWeights(memory_pool *Pool, void *Weights)
{
    PoolFree(Pool, Weights);
}

/*
    =================================================================
    BENCHMARKING
    =================================================================
*/

void 
BenchmarkMatrixMultiply(memory_arena *Arena)
{
    printf("\n=== Matrix Multiply Benchmark ===\n");
    
    u32 Sizes[] = {32, 64, 128, 256, 512, 1024};
    u32 SizeCount = ArrayCount(Sizes);
    
    for(u32 i = 0; i < SizeCount; ++i)
    {
        u32 N = Sizes[i];
        
        neural_matrix A = AllocateMatrix(Arena, N, N);
        neural_matrix B = AllocateMatrix(Arena, N, N);
        neural_matrix C = AllocateMatrix(Arena, N, N);
        
        InitializeMatrixRandom(&A, 1.0f);
        InitializeMatrixRandom(&B, 1.0f);
        
        // Warmup
        MatrixMultiply(&C, &A, &B);
        
        // Benchmark
        u32 Iterations = (N <= 128) ? 100 : 10;
        u64 StartCycles = ReadCPUTimer();
        
        for(u32 j = 0; j < Iterations; ++j)
        {
            MatrixMultiply(&C, &A, &B);
        }
        
        u64 TotalCycles = ReadCPUTimer() - StartCycles;
        u64 CyclesPerOp = TotalCycles / Iterations;
        
        // Calculate GFLOPS
        u64 Operations = 2ULL * N * N * N;  // 2N³ operations for matrix multiply
        f64 GFLOPS = (f64)Operations / (f64)CyclesPerOp * 3.0;  // Assume 3GHz CPU
        
        printf("  %4u x %4u: %12llu cycles, %.2f GFLOPS\n", 
               N, N, (unsigned long long)CyclesPerOp, GFLOPS);
    }
}

void 
BenchmarkActivations(memory_arena *Arena)
{
    printf("\n=== Activation Function Benchmark ===\n");
    
    u32 Size = 1024 * 1024;  // 1M elements
    neural_vector Vector = AllocateVector(Arena, Size);
    
    // Initialize with random values
    for(u32 i = 0; i < Size; ++i)
    {
        Vector.Data[i] = (f32)(i % 100 - 50) / 10.0f;  // [-5, 5]
    }
    
    u32 Iterations = 100;
    
    // Benchmark ReLU
    {
        u64 StartCycles = ReadCPUTimer();
        for(u32 i = 0; i < Iterations; ++i)
        {
            ReLU(Vector.Data, Size);
        }
        u64 Cycles = (ReadCPUTimer() - StartCycles) / Iterations;
        f64 ElementsPerCycle = (f64)Size / (f64)Cycles;
        printf("  ReLU:     %12llu cycles, %.2f elements/cycle\n", 
               (unsigned long long)Cycles, ElementsPerCycle);
    }
    
    // Reset vector
    for(u32 i = 0; i < Size; ++i)
    {
        Vector.Data[i] = (f32)(i % 100 - 50) / 10.0f;
    }
    
    // Benchmark Sigmoid
    {
        u64 StartCycles = ReadCPUTimer();
        for(u32 i = 0; i < Iterations; ++i)
        {
            Sigmoid(Vector.Data, Size);
        }
        u64 Cycles = (ReadCPUTimer() - StartCycles) / Iterations;
        f64 ElementsPerCycle = (f64)Size / (f64)Cycles;
        printf("  Sigmoid:  %12llu cycles, %.2f elements/cycle\n", 
               (unsigned long long)Cycles, ElementsPerCycle);
    }
    
    // Reset vector
    for(u32 i = 0; i < Size; ++i)
    {
        Vector.Data[i] = (f32)(i % 100 - 50) / 10.0f;
    }
    
    // Benchmark Tanh
    {
        u64 StartCycles = ReadCPUTimer();
        for(u32 i = 0; i < Iterations; ++i)
        {
            Tanh(Vector.Data, Size);
        }
        u64 Cycles = (ReadCPUTimer() - StartCycles) / Iterations;
        f64 ElementsPerCycle = (f64)Size / (f64)Cycles;
        printf("  Tanh:     %12llu cycles, %.2f elements/cycle\n", 
               (unsigned long long)Cycles, ElementsPerCycle);
    }
}

void 
BenchmarkForwardPass(memory_arena *Arena)
{
    printf("\n=== Forward Pass Benchmark ===\n");
    
    u32 InputSizes[] = {784, 1024, 2048};  // MNIST, larger inputs
    u32 Hidden1 = 256;
    u32 Hidden2 = 128;
    u32 Output = 10;
    
    for(u32 i = 0; i < ArrayCount(InputSizes); ++i)
    {
        u32 InputSize = InputSizes[i];
        
        neural_network Network = InitializeNeuralNetwork(Arena, InputSize, 
                                                         Hidden1, Hidden2, Output);
        
        neural_vector Input = AllocateVector(Arena, InputSize);
        neural_vector OutputVec = AllocateVector(Arena, Output);
        
        // Initialize input
        for(u32 j = 0; j < InputSize; ++j)
        {
            Input.Data[j] = (f32)(j % 256) / 255.0f;
        }
        
        // Warmup
        ForwardPass(&Network, &Input, &OutputVec);
        
        // Benchmark
        u32 Iterations = 1000;
        u64 StartCycles = ReadCPUTimer();
        
        for(u32 j = 0; j < Iterations; ++j)
        {
            ForwardPass(&Network, &Input, &OutputVec);
        }
        
        u64 TotalCycles = ReadCPUTimer() - StartCycles;
        u64 CyclesPerPass = TotalCycles / Iterations;
        
        // Calculate operations
        u64 Ops = (u64)InputSize * Hidden1 * 2 +     // Layer 1
                  (u64)Hidden1 * Hidden2 * 2 +        // Layer 2
                  (u64)Hidden2 * Output * 2 +         // Layer 3
                  (u64)(Hidden1 + Hidden2 + Output);  // Activations
        
        f64 GFLOPS = (f64)Ops / (f64)CyclesPerPass * 3.0;  // Assume 3GHz
        
        printf("  Input %4u: %12llu cycles, %.3f GFLOPS\n",
               InputSize, (unsigned long long)CyclesPerPass, GFLOPS);
    }
}

void 
PrintBenchmarkResult(benchmark_result *Result)
{
    printf("%-32s: %12llu cycles", Result->Name, (unsigned long long)Result->Cycles);
    
    if(Result->GBPerSecond > 0)
    {
        printf(", %.2f GB/s", Result->GBPerSecond);
    }
    
    if(Result->GFLOPS > 0)
    {
        printf(", %.2f GFLOPS", Result->GFLOPS);
    }
    
    printf("\n");
}

/*
    =================================================================
    DEBUG UTILITIES
    =================================================================
*/

#if HANDMADE_DEBUG

void 
ValidateMatrix(neural_matrix *Matrix)
{
    Assert(Matrix->Data != 0);
    Assert(Matrix->Rows > 0);
    Assert(Matrix->Cols > 0);
    Assert(Matrix->Stride >= Matrix->Cols);
    Assert(IsAligned(Matrix->Data, CACHE_LINE_SIZE));
}

void 
PrintMatrix(neural_matrix *Matrix, const char *Name)
{
    printf("\n%s (%u x %u):\n", Name, Matrix->Rows, Matrix->Cols);
    
    for(u32 i = 0; i < Minimum(Matrix->Rows, 5); ++i)
    {
        printf("  ");
        for(u32 j = 0; j < Minimum(Matrix->Cols, 5); ++j)
        {
            printf("%8.4f ", Matrix->Data[i * Matrix->Stride + j]);
        }
        if(Matrix->Cols > 5) printf("...");
        printf("\n");
    }
    if(Matrix->Rows > 5) printf("  ...\n");
}

void 
CheckNaN(neural_matrix *Matrix)
{
    for(u32 i = 0; i < Matrix->Rows; ++i)
    {
        for(u32 j = 0; j < Matrix->Cols; ++j)
        {
            f32 Value = Matrix->Data[i * Matrix->Stride + j];
            Assert(Value == Value);  // NaN != NaN
        }
    }
}

void 
CheckGradients(neural_network *Network, neural_vector *Input, neural_vector *Target)
{
    // Numerical gradient checking
    f32 Epsilon = 1e-4f;
    
    printf("\nGradient Check:\n");
    
    // Check a few random weights
    for(u32 Sample = 0; Sample < 5; ++Sample)
    {
        // Pick random weight
        u32 Row = Sample % Network->Layer1.Weights.Rows;
        u32 Col = Sample % Network->Layer1.Weights.Cols;
        
        f32 *Weight = &Network->Layer1.Weights.Data[Row * Network->Layer1.Weights.Stride + Col];
        f32 OriginalWeight = *Weight;
        
        // Forward pass with weight + epsilon
        *Weight = OriginalWeight + Epsilon;
        neural_vector Output1 = AllocateVector(Network->Arena, Network->OutputSize);
        ForwardPass(Network, Input, &Output1);
        
        // Compute loss (simplified - cross entropy)
        f32 Loss1 = 0.0f;
        for(u32 i = 0; i < Network->OutputSize; ++i)
        {
            if(Target->Data[i] > 0.5f)
            {
                Loss1 -= logf(Output1.Data[i] + 1e-10f);
            }
        }
        
        // Forward pass with weight - epsilon
        *Weight = OriginalWeight - Epsilon;
        neural_vector Output2 = AllocateVector(Network->Arena, Network->OutputSize);
        ForwardPass(Network, Input, &Output2);
        
        f32 Loss2 = 0.0f;
        for(u32 i = 0; i < Network->OutputSize; ++i)
        {
            if(Target->Data[i] > 0.5f)
            {
                Loss2 -= logf(Output2.Data[i] + 1e-10f);
            }
        }
        
        // Numerical gradient
        f32 NumericalGradient = (Loss1 - Loss2) / (2.0f * Epsilon);
        
        // Restore weight
        *Weight = OriginalWeight;
        
        printf("  Weight[%u][%u]: Numerical gradient = %.6f\n", 
               Row, Col, NumericalGradient);
    }
}

#endif // HANDMADE_DEBUG