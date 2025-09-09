/*
 * JIT COMPILATION CONCEPT DEMONSTRATION
 * =====================================
 * 
 * Shows the core concepts of JIT compilation without complex x86 assembly.
 * Demonstrates profile-guided optimization and performance improvements.
 * 
 * Author: Handmade Neural Engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

/* ============================================================================
 * PERFORMANCE MEASUREMENT
 * ============================================================================
 */

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* ============================================================================
 * NEURAL OPERATION IMPLEMENTATIONS
 * ============================================================================
 */

/* Level 0: Naive implementation - simple and correct */
static void gemm_naive(const float* a, const float* b, float* c,
                       int m, int n, int k) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int kk = 0; kk < k; kk++) {
                sum += a[i * k + kk] * b[kk * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

/* Level 1: Cache-friendly loop ordering */
static void gemm_cached(const float* a, const float* b, float* c,
                       int m, int n, int k) {
    /* Clear output matrix */
    memset(c, 0, m * n * sizeof(float));
    
    /* Use i-k-j ordering for better cache utilization */
    for (int i = 0; i < m; i++) {
        for (int kk = 0; kk < k; kk++) {
            float a_ik = a[i * k + kk];
            for (int j = 0; j < n; j++) {
                c[i * n + j] += a_ik * b[kk * n + j];
            }
        }
    }
}

/* Level 2: Loop unrolling for instruction-level parallelism */
static void gemm_unrolled(const float* a, const float* b, float* c,
                         int m, int n, int k) {
    memset(c, 0, m * n * sizeof(float));
    
    for (int i = 0; i < m; i++) {
        for (int kk = 0; kk < k; kk++) {
            float a_ik = a[i * k + kk];
            int j = 0;
            
            /* Unroll by 4 */
            for (; j < n - 3; j += 4) {
                c[i * n + j + 0] += a_ik * b[kk * n + j + 0];
                c[i * n + j + 1] += a_ik * b[kk * n + j + 1];
                c[i * n + j + 2] += a_ik * b[kk * n + j + 2];
                c[i * n + j + 3] += a_ik * b[kk * n + j + 3];
            }
            
            /* Handle remainder */
            for (; j < n; j++) {
                c[i * n + j] += a_ik * b[kk * n + j];
            }
        }
    }
}

/* Level 3: Cache blocking for large matrices */
static void gemm_blocked(const float* a, const float* b, float* c,
                        int m, int n, int k) {
    const int BLOCK = 32;  /* Fits in L1 cache */
    memset(c, 0, m * n * sizeof(float));
    
    /* Block loops */
    for (int ii = 0; ii < m; ii += BLOCK) {
        for (int kk = 0; kk < k; kk += BLOCK) {
            for (int jj = 0; jj < n; jj += BLOCK) {
                /* Process block */
                int i_end = ii + BLOCK < m ? ii + BLOCK : m;
                int k_end = kk + BLOCK < k ? kk + BLOCK : k;
                int j_end = jj + BLOCK < n ? jj + BLOCK : n;
                
                for (int i = ii; i < i_end; i++) {
                    for (int kv = kk; kv < k_end; kv++) {
                        float a_ik = a[i * k + kv];
                        for (int j = jj; j < j_end; j++) {
                            c[i * n + j] += a_ik * b[kv * n + j];
                        }
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * JIT COMPILATION SIMULATION
 * ============================================================================
 */

typedef enum {
    OPT_LEVEL_0,  /* Naive */
    OPT_LEVEL_1,  /* Cache-friendly */
    OPT_LEVEL_2,  /* Unrolled */
    OPT_LEVEL_3   /* Blocked */
} OptimizationLevel;

typedef struct {
    uint64_t call_count;
    uint64_t total_cycles;
    OptimizationLevel opt_level;
    int m, n, k;  /* Matrix dimensions */
} KernelProfile;

typedef struct {
    KernelProfile profiles[100];
    int profile_count;
    uint64_t compilations;
} JITCompiler;

static void jit_init(JITCompiler* jit) {
    memset(jit, 0, sizeof(JITCompiler));
    printf("JIT Compiler Initialized\n");
    printf("  Optimization levels: 0 (naive) -> 3 (fully optimized)\n");
    printf("  Compilation thresholds:\n");
    printf("    Level 1: 10 calls\n");
    printf("    Level 2: 50 calls\n");
    printf("    Level 3: 100 calls\n\n");
}

static KernelProfile* find_profile(JITCompiler* jit, int m, int n, int k) {
    for (int i = 0; i < jit->profile_count; i++) {
        KernelProfile* p = &jit->profiles[i];
        if (p->m == m && p->n == n && p->k == k) {
            return p;
        }
    }
    
    /* Create new profile */
    if (jit->profile_count < 100) {
        KernelProfile* p = &jit->profiles[jit->profile_count++];
        p->m = m;
        p->n = n;
        p->k = k;
        p->opt_level = OPT_LEVEL_0;
        return p;
    }
    
    return NULL;
}

static void jit_gemm(JITCompiler* jit, const float* a, const float* b, float* c,
                    int m, int n, int k) {
    uint64_t start = rdtsc();
    
    /* Find or create profile */
    KernelProfile* prof = find_profile(jit, m, n, k);
    if (!prof) {
        gemm_naive(a, b, c, m, n, k);
        return;
    }
    
    /* Update profile */
    prof->call_count++;
    
    /* Check for optimization upgrades */
    OptimizationLevel old_level = prof->opt_level;
    if (prof->call_count == 10 && prof->opt_level < OPT_LEVEL_1) {
        prof->opt_level = OPT_LEVEL_1;
        jit->compilations++;
        printf("[JIT] Optimizing %dx%dx%d to level 1 (cache-friendly)\n", m, n, k);
    } else if (prof->call_count == 50 && prof->opt_level < OPT_LEVEL_2) {
        prof->opt_level = OPT_LEVEL_2;
        jit->compilations++;
        printf("[JIT] Optimizing %dx%dx%d to level 2 (unrolled)\n", m, n, k);
    } else if (prof->call_count == 100 && prof->opt_level < OPT_LEVEL_3) {
        prof->opt_level = OPT_LEVEL_3;
        jit->compilations++;
        printf("[JIT] Optimizing %dx%dx%d to level 3 (blocked)\n", m, n, k);
    }
    
    /* Execute appropriate version */
    switch (prof->opt_level) {
        case OPT_LEVEL_0:
            gemm_naive(a, b, c, m, n, k);
            break;
        case OPT_LEVEL_1:
            gemm_cached(a, b, c, m, n, k);
            break;
        case OPT_LEVEL_2:
            gemm_unrolled(a, b, c, m, n, k);
            break;
        case OPT_LEVEL_3:
            gemm_blocked(a, b, c, m, n, k);
            break;
    }
    
    prof->total_cycles += rdtsc() - start;
}

/* ============================================================================
 * DEMONSTRATIONS
 * ============================================================================
 */

static void demo_optimization_progression(JITCompiler* jit) {
    printf("=== Optimization Progression Demo ===\n\n");
    
    const int SIZE = 64;
    float* a = malloc(SIZE * SIZE * sizeof(float));
    float* b = malloc(SIZE * SIZE * sizeof(float));
    float* c = malloc(SIZE * SIZE * sizeof(float));
    
    /* Initialize matrices */
    for (int i = 0; i < SIZE * SIZE; i++) {
        a[i] = (float)rand() / RAND_MAX;
        b[i] = (float)rand() / RAND_MAX;
    }
    
    printf("Matrix size: %dx%d\n\n", SIZE, SIZE);
    
    /* Phase 1: Initial calls (naive) */
    printf("Calls 1-9: Using naive implementation\n");
    double t1 = get_time_ms();
    for (int i = 0; i < 9; i++) {
        jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    }
    t1 = get_time_ms() - t1;
    printf("  Time: %.3f ms/op\n\n", t1 / 9);
    
    /* Phase 2: Level 1 optimization */
    printf("Call 10: Triggering optimization\n");
    jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    
    printf("Calls 11-49: Using cache-friendly implementation\n");
    double t2 = get_time_ms();
    for (int i = 0; i < 39; i++) {
        jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    }
    t2 = get_time_ms() - t2;
    printf("  Time: %.3f ms/op (%.1fx speedup)\n\n", t2 / 39, (t1/9) / (t2/39));
    
    /* Phase 3: Level 2 optimization */
    printf("Call 50: Triggering level 2 optimization\n");
    jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    
    printf("Calls 51-99: Using unrolled implementation\n");
    double t3 = get_time_ms();
    for (int i = 0; i < 49; i++) {
        jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    }
    t3 = get_time_ms() - t3;
    printf("  Time: %.3f ms/op (%.1fx speedup from naive)\n\n", 
           t3 / 49, (t1/9) / (t3/49));
    
    /* Phase 4: Level 3 optimization */
    printf("Call 100: Triggering level 3 optimization\n");
    jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    
    printf("Calls 101-150: Using blocked implementation\n");
    double t4 = get_time_ms();
    for (int i = 0; i < 50; i++) {
        jit_gemm(jit, a, b, c, SIZE, SIZE, SIZE);
    }
    t4 = get_time_ms() - t4;
    printf("  Time: %.3f ms/op (%.1fx speedup from naive)\n\n", 
           t4 / 50, (t1/9) / (t4/50));
    
    free(a);
    free(b);
    free(c);
}

static void benchmark_optimization_levels(void) {
    printf("=== Optimization Level Benchmarks ===\n\n");
    
    const int SIZES[] = {32, 64, 128, 256};
    const int NUM_SIZES = 4;
    const int ITERATIONS = 100;
    
    printf("%-10s | %-12s | %-12s | %-12s | %-12s\n",
           "Size", "Naive", "Cached", "Unrolled", "Blocked");
    printf("-----------|--------------|--------------|--------------|-------------\n");
    
    for (int s = 0; s < NUM_SIZES; s++) {
        int size = SIZES[s];
        float* a = malloc(size * size * sizeof(float));
        float* b = malloc(size * size * sizeof(float));
        float* c = malloc(size * size * sizeof(float));
        
        /* Initialize */
        for (int i = 0; i < size * size; i++) {
            a[i] = (float)rand() / RAND_MAX;
            b[i] = (float)rand() / RAND_MAX;
        }
        
        /* Benchmark each level */
        double times[4];
        
        /* Level 0: Naive */
        double start = get_time_ms();
        for (int i = 0; i < ITERATIONS; i++) {
            gemm_naive(a, b, c, size, size, size);
        }
        times[0] = (get_time_ms() - start) / ITERATIONS;
        
        /* Level 1: Cached */
        start = get_time_ms();
        for (int i = 0; i < ITERATIONS; i++) {
            gemm_cached(a, b, c, size, size, size);
        }
        times[1] = (get_time_ms() - start) / ITERATIONS;
        
        /* Level 2: Unrolled */
        start = get_time_ms();
        for (int i = 0; i < ITERATIONS; i++) {
            gemm_unrolled(a, b, c, size, size, size);
        }
        times[2] = (get_time_ms() - start) / ITERATIONS;
        
        /* Level 3: Blocked */
        start = get_time_ms();
        for (int i = 0; i < ITERATIONS; i++) {
            gemm_blocked(a, b, c, size, size, size);
        }
        times[3] = (get_time_ms() - start) / ITERATIONS;
        
        /* Print results */
        printf("%-10d | %9.3f ms | %9.3f ms | %9.3f ms | %9.3f ms\n",
               size, times[0], times[1], times[2], times[3]);
        printf("           | %12s | %9.1fx | %9.1fx | %9.1fx\n",
               "baseline", times[0]/times[1], times[0]/times[2], times[0]/times[3]);
        
        free(a);
        free(b);
        free(c);
    }
    printf("\n");
}

static void demo_compilation_cost(JITCompiler* jit) {
    printf("=== Compilation Cost Analysis ===\n\n");
    
    printf("Simulating compilation overhead...\n\n");
    
    /* Small workload */
    const int SIZE = 32;
    float* a = malloc(SIZE * SIZE * sizeof(float));
    float* b = malloc(SIZE * SIZE * sizeof(float));
    float* c = malloc(SIZE * SIZE * sizeof(float));
    
    for (int i = 0; i < SIZE * SIZE; i++) {
        a[i] = (float)rand() / RAND_MAX;
        b[i] = (float)rand() / RAND_MAX;
    }
    
    /* Measure single operation cost */
    uint64_t single_start = rdtsc();
    gemm_naive(a, b, c, SIZE, SIZE, SIZE);
    uint64_t single_cycles = rdtsc() - single_start;
    
    printf("Single operation cost: %lu cycles\n", single_cycles);
    printf("Compilation threshold: 10 operations\n");
    printf("Break-even point: When optimized version is %.1fx faster\n\n",
           10.0 / 9.0);
    
    /* Measure actual speedup */
    uint64_t naive_start = rdtsc();
    for (int i = 0; i < 100; i++) {
        gemm_naive(a, b, c, SIZE, SIZE, SIZE);
    }
    uint64_t naive_cycles = rdtsc() - naive_start;
    
    uint64_t opt_start = rdtsc();
    for (int i = 0; i < 100; i++) {
        gemm_cached(a, b, c, SIZE, SIZE, SIZE);
    }
    uint64_t opt_cycles = rdtsc() - opt_start;
    
    printf("100 operations:\n");
    printf("  Naive: %lu cycles\n", naive_cycles);
    printf("  Optimized: %lu cycles\n", opt_cycles);
    printf("  Speedup: %.2fx\n", (double)naive_cycles / opt_cycles);
    printf("  Cycles saved: %lu\n", naive_cycles - opt_cycles);
    printf("  ROI: Compilation pays off after ~%d operations\n\n",
           (int)(10 * opt_cycles / (naive_cycles - opt_cycles)));
    
    free(a);
    free(b);
    free(c);
}

/* ============================================================================
 * MAIN DEMONSTRATION
 * ============================================================================
 */

int main(void) {
    printf("==========================================\n");
    printf(" JIT COMPILATION CONCEPT DEMONSTRATION\n");
    printf(" Profile-Guided Optimization\n");
    printf("==========================================\n\n");
    
    /* Initialize JIT compiler */
    JITCompiler jit;
    jit_init(&jit);
    
    /* Run demonstrations */
    demo_optimization_progression(&jit);
    benchmark_optimization_levels();
    demo_compilation_cost(&jit);
    
    /* Print statistics */
    printf("=== JIT Statistics ===\n\n");
    printf("Total compilations: %lu\n", jit.compilations);
    printf("Kernels profiled: %d\n", jit.profile_count);
    
    if (jit.profile_count > 0) {
        printf("\nKernel profiles:\n");
        for (int i = 0; i < jit.profile_count; i++) {
            KernelProfile* p = &jit.profiles[i];
            printf("  %dx%dx%d: %lu calls, level %d, avg %lu cycles\n",
                   p->m, p->n, p->k, p->call_count, p->opt_level,
                   p->call_count > 0 ? p->total_cycles / p->call_count : 0);
        }
    }
    
    printf("\n=== Key Insights ===\n\n");
    printf("1. Profile-guided optimization minimizes overhead\n");
    printf("2. Progressive optimization matches workload importance\n");
    printf("3. Cache-friendly access patterns provide major speedups\n");
    printf("4. Loop unrolling enables instruction-level parallelism\n");
    printf("5. Blocking keeps working set in cache\n\n");
    
    printf("In a real JIT compiler:\n");
    printf("- We would generate actual machine code\n");
    printf("- Use SIMD instructions (AVX2, FMA)\n");
    printf("- Adapt to specific CPU microarchitecture\n");
    printf("- Fuse operations to reduce memory traffic\n\n");
    
    printf("This demonstration shows the concepts without the complexity\n");
    printf("of actual x86-64 code generation.\n\n");
    
    printf("Performance is earned through understanding.\n");
    printf("Every optimization deliberate, every trade-off measured.\n");
    
    return 0;
}