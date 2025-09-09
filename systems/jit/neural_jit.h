/*
 * NEURAL JIT COMPILER - HANDMADE FROM SCRATCH
 * ============================================
 * 
 * Zero-dependency JIT compilation for neural network operations.
 * Generates optimized x86-64 machine code at runtime.
 * 
 * PERFORMANCE TARGETS:
 * - Matrix multiplication: >90% of theoretical FLOPS
 * - Activation functions: <3 cycles per element with SIMD
 * - Code generation: <1ms for typical kernels
 * - Memory overhead: <1MB for code cache
 * 
 * Author: Handmade Neural Engine
 * Philosophy: Every byte understood, every cycle counted
 */

#ifndef NEURAL_JIT_H
#define NEURAL_JIT_H

#include <stdint.h>
#include <stddef.h>

/* PERFORMANCE: CPU feature detection for optimal code generation */
typedef struct {
    uint32_t has_sse2    : 1;
    uint32_t has_sse3    : 1;
    uint32_t has_ssse3   : 1;
    uint32_t has_sse41   : 1;
    uint32_t has_sse42   : 1;
    uint32_t has_avx     : 1;
    uint32_t has_avx2    : 1;
    uint32_t has_fma3    : 1;
    uint32_t has_avx512f : 1;
    uint32_t reserved    : 23;
} CPUFeatures;

/* MEMORY: Executable code block with metadata */
typedef struct {
    uint8_t*  code;           /* Executable memory */
    size_t    code_size;      /* Current code size */
    size_t    code_capacity;  /* Allocated capacity */
    uint64_t  exec_count;     /* Execution counter for profiling */
    uint64_t  total_cycles;   /* Total CPU cycles consumed */
} CodeBlock;

/* CACHE: Compiled kernel cache entry */
typedef struct {
    uint64_t  hash;           /* Operation hash for lookup */
    CodeBlock block;          /* Compiled code */
    uint64_t  last_used;      /* LRU timestamp */
    uint32_t  m, n, k;        /* Matrix dimensions for GEMM */
    uint32_t  op_type;        /* Operation type enum */
} CachedKernel;

/* PROFILING: Hot path detection */
typedef struct {
    uint64_t  call_count;     /* Number of calls */
    uint64_t  total_cycles;   /* Total cycles spent */
    uint64_t  input_sizes[3]; /* Typical input dimensions */
    uint32_t  should_compile; /* JIT compilation threshold reached */
} ProfileData;

/* JIT: Main compiler context */
typedef struct {
    /* CPU capabilities */
    CPUFeatures cpu;
    
    /* Code cache - PERFORMANCE: Direct-mapped for O(1) lookup */
    CachedKernel* cache;
    size_t        cache_size;
    size_t        cache_capacity;
    
    /* Profiling data */
    ProfileData* profiles;
    size_t       profile_count;
    
    /* Memory management */
    uint8_t*     exec_memory;      /* Executable memory pool */
    size_t       exec_memory_size;
    size_t       exec_memory_used;
    
    /* Statistics */
    uint64_t     compilations;
    uint64_t     cache_hits;
    uint64_t     cache_misses;
    uint64_t     total_jit_cycles;
} NeuralJIT;

/* Operation types for JIT compilation */
typedef enum {
    OP_GEMM_F32,        /* General matrix multiply float32 */
    OP_GEMV_F32,        /* Matrix-vector multiply float32 */
    OP_TANH_F32,        /* Tanh activation */
    OP_SIGMOID_F32,     /* Sigmoid activation */
    OP_RELU_F32,        /* ReLU activation */
    OP_SOFTMAX_F32,     /* Softmax activation */
    OP_ADAM_UPDATE_F32, /* Adam optimizer update */
    OP_SGD_UPDATE_F32,  /* SGD optimizer update */
    OP_COSINE_SIMILARITY, /* Cosine similarity */
} OpType;

/* Function signatures for generated code */
typedef void (*gemm_f32_fn)(const float* a, const float* b, float* c,
                            uint32_t m, uint32_t n, uint32_t k,
                            float alpha, float beta);
typedef void (*gemv_f32_fn)(const float* a, const float* x, float* y,
                            uint32_t m, uint32_t n, float alpha, float beta);
typedef void (*activation_f32_fn)(const float* input, float* output, uint32_t count);

/* PUBLIC API - Zero dependencies, full control */

/* Initialize JIT compiler with memory budget */
NeuralJIT* njit_create(size_t exec_memory_mb, size_t cache_entries);

/* Destroy JIT compiler and free all resources */
void njit_destroy(NeuralJIT* jit);

/* Detect CPU features for optimal code generation */
void njit_detect_cpu_features(CPUFeatures* features);

/* Profile an operation for potential JIT compilation */
void njit_profile_operation(NeuralJIT* jit, OpType op, 
                           uint32_t m, uint32_t n, uint32_t k,
                           uint64_t cycles);

/* JIT compile a specific operation if profitable */
CodeBlock* njit_compile_operation(NeuralJIT* jit, OpType op,
                                 uint32_t m, uint32_t n, uint32_t k);

/* Execute or JIT-compile matrix multiplication */
void njit_gemm_f32(NeuralJIT* jit,
                  const float* a, const float* b, float* c,
                  uint32_t m, uint32_t n, uint32_t k,
                  float alpha, float beta);

/* Execute or JIT-compile matrix-vector multiplication */
void njit_gemv_f32(NeuralJIT* jit,
                  const float* a, const float* x, float* y,
                  uint32_t m, uint32_t n,
                  float alpha, float beta);

/* Execute or JIT-compile activation functions */
void njit_tanh_f32(NeuralJIT* jit, const float* input, float* output, uint32_t count);
void njit_sigmoid_f32(NeuralJIT* jit, const float* input, float* output, uint32_t count);
void njit_relu_f32(NeuralJIT* jit, const float* input, float* output, uint32_t count);

/* Cache management */
void njit_clear_cache(NeuralJIT* jit);
size_t njit_get_cache_size_bytes(NeuralJIT* jit);

/* Statistics and debugging */
void njit_print_stats(NeuralJIT* jit);
void njit_dump_assembly(CodeBlock* block, const char* filename);

/* INTERNAL: x86-64 instruction encoding */

/* Register encoding (x86-64) */
#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7
#define REG_R8  8
#define REG_R9  9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15

/* XMM/YMM register encoding */
#define XMM0  0
#define XMM1  1
#define XMM2  2
#define XMM3  3
#define XMM4  4
#define XMM5  5
#define XMM6  6
#define XMM7  7
#define XMM8  8
#define XMM9  9
#define XMM10 10
#define XMM11 11
#define XMM12 12
#define XMM13 13
#define XMM14 14
#define XMM15 15

/* PERFORMANCE: Inline cycle counter for profiling */
static inline uint64_t njit_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* PERFORMANCE: Memory prefetch hints */
static inline void njit_prefetch_t0(const void* addr) {
    __asm__ __volatile__ ("prefetcht0 %0" : : "m"(*(const char*)addr));
}

static inline void njit_prefetch_t1(const void* addr) {
    __asm__ __volatile__ ("prefetcht1 %0" : : "m"(*(const char*)addr));
}

static inline void njit_prefetch_nta(const void* addr) {
    __asm__ __volatile__ ("prefetchnta %0" : : "m"(*(const char*)addr));
}

#endif /* NEURAL_JIT_H */