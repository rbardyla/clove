/*
 * NEURAL PROFILER - HOTSPOT DETECTION AND ANALYSIS
 * =================================================
 * 
 * Cycle-accurate profiling of neural engine components.
 * Identifies JIT compilation candidates through runtime analysis.
 * 
 * PERFORMANCE TARGETS:
 * - Profiling overhead: <2% of execution time
 * - Hotspot detection: Automatic after 1000 iterations
 * - Profile data: Cache-aligned, minimal memory overhead
 * 
 * Author: Handmade Neural Engine
 * Philosophy: Measure everything, optimize what matters
 */

#ifndef NEURAL_PROFILER_H
#define NEURAL_PROFILER_H

#include <stdint.h>
#include <stddef.h>

/* PERFORMANCE: Operation types we profile */
typedef enum {
    PROF_GEMM_F32,           /* Matrix-matrix multiply */
    PROF_GEMV_F32,           /* Matrix-vector multiply */
    PROF_LSTM_GATES,         /* LSTM gate computations */
    PROF_LSTM_STATE_UPDATE,  /* LSTM cell state update */
    PROF_DNC_CONTENT_ADDR,   /* DNC content addressing */
    PROF_DNC_MEMORY_READ,    /* DNC memory read */
    PROF_DNC_MEMORY_WRITE,   /* DNC memory write */
    PROF_DNC_TEMPORAL_LINK,  /* DNC temporal linkage update */
    PROF_EWC_PENALTY,        /* EWC penalty computation */
    PROF_ACTIVATION_TANH,    /* Tanh activation */
    PROF_ACTIVATION_SIGMOID, /* Sigmoid activation */
    PROF_VECTOR_ADD,         /* Vector addition */
    PROF_VECTOR_MUL,         /* Elementwise multiply */
    PROF_COSINE_SIMILARITY,  /* Cosine similarity */
    PROF_SOFTMAX,           /* Softmax normalization */
    PROF_OP_COUNT
} ProfileOpType;

/* CACHE: Profile entry for a single operation instance */
typedef struct __attribute__((aligned(64))) {
    uint64_t op_hash;        /* Hash of operation parameters */
    uint64_t call_count;     /* Number of executions */
    uint64_t total_cycles;   /* Total CPU cycles spent */
    uint64_t min_cycles;     /* Minimum cycles observed */
    uint64_t max_cycles;     /* Maximum cycles observed */
    uint64_t last_cycles;    /* Most recent execution */
    
    /* Operation dimensions */
    uint32_t dims[4];        /* M, N, K, batch_size */
    
    /* JIT compilation status */
    uint32_t jit_compiled : 1;
    uint32_t jit_candidate : 1;
    uint32_t reserved : 30;
    
    /* Cache behavior */
    uint64_t cache_misses;   /* L1D cache misses */
    uint64_t tlb_misses;     /* TLB misses */
    
    /* Thermal behavior */
    float avg_cycles_per_element;
    float speedup_factor;    /* After JIT compilation */
} ProfileEntry;

/* MEMORY: Profile bucket for hash collision handling */
typedef struct {
    ProfileEntry entries[8]; /* 8-way associative */
    uint32_t num_entries;
} ProfileBucket;

/* PROFILER: Main profiling context */
typedef struct __attribute__((aligned(64))) {
    /* Hash table for operation profiles */
    ProfileBucket* buckets;
    size_t num_buckets;
    
    /* Global statistics per operation type */
    struct {
        uint64_t total_calls;
        uint64_t total_cycles;
        uint64_t jit_candidates;
        uint64_t jit_compiled;
    } op_stats[PROF_OP_COUNT];
    
    /* Hotspot tracking */
    ProfileEntry* hotspots[32];  /* Top 32 hottest operations */
    uint32_t num_hotspots;
    
    /* JIT compilation thresholds */
    uint64_t jit_threshold_calls;    /* Min calls before JIT */
    uint64_t jit_threshold_cycles;   /* Min cycles before JIT */
    float jit_threshold_percent;     /* Min % of total time */
    
    /* Memory management */
    uint8_t* memory_pool;
    size_t memory_size;
    size_t memory_used;
    
    /* Hardware counters */
    uint64_t profile_overhead_cycles;
    uint64_t total_profiled_cycles;
    
    /* Configuration */
    uint32_t enabled : 1;
    uint32_t auto_jit : 1;
    uint32_t detailed_stats : 1;
    uint32_t reserved_flags : 29;
} NeuralProfiler;

/* SAMPLING: Lightweight profiling context for hot paths */
typedef struct {
    uint64_t start_cycles;
    uint64_t start_cache_refs;
    ProfileOpType op_type;
    uint32_t dims[4];
} ProfileContext;

/* PUBLIC API */

/* Initialize profiler with memory budget */
NeuralProfiler* prof_create(size_t memory_mb);
void prof_destroy(NeuralProfiler* profiler);

/* Enable/disable profiling */
void prof_enable(NeuralProfiler* profiler);
void prof_disable(NeuralProfiler* profiler);

/* Start/end profiling for an operation */
ProfileContext prof_begin(NeuralProfiler* profiler, ProfileOpType op,
                         uint32_t m, uint32_t n, uint32_t k, uint32_t batch);
void prof_end(NeuralProfiler* profiler, ProfileContext* ctx);

/* Mark operation as JIT candidate */
void prof_mark_jit_candidate(NeuralProfiler* profiler, uint64_t op_hash);
void prof_mark_jit_compiled(NeuralProfiler* profiler, uint64_t op_hash, float speedup);

/* Analyze and identify hotspots */
void prof_analyze_hotspots(NeuralProfiler* profiler);
ProfileEntry** prof_get_hotspots(NeuralProfiler* profiler, uint32_t* count);

/* Get JIT compilation candidates */
ProfileEntry** prof_get_jit_candidates(NeuralProfiler* profiler, uint32_t* count);

/* Statistics and reporting */
void prof_print_summary(NeuralProfiler* profiler);
void prof_print_hotspots(NeuralProfiler* profiler, uint32_t top_n);
void prof_export_data(NeuralProfiler* profiler, const char* filename);

/* Reset profiling data */
void prof_reset(NeuralProfiler* profiler);
void prof_reset_op_type(NeuralProfiler* profiler, ProfileOpType op);

/* INLINE HELPERS */

/* PERFORMANCE: Fast hash for operation parameters */
static inline uint64_t prof_hash_op(ProfileOpType op, uint32_t m, uint32_t n, 
                                   uint32_t k, uint32_t batch) {
    uint64_t hash = op;
    hash = (hash << 16) | (m & 0xFFFF);
    hash = (hash << 16) | (n & 0xFFFF);
    hash = (hash << 8)  | (k & 0xFF);
    hash = (hash << 8)  | (batch & 0xFF);
    
    /* Simple mixing */
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    return hash;
}

/* PERFORMANCE: Read CPU timestamp counter */
static inline uint64_t prof_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* PERFORMANCE: Read performance monitoring counter */
static inline uint64_t prof_rdpmc(uint32_t counter) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdpmc" : "=a"(lo), "=d"(hi) : "c"(counter));
    return ((uint64_t)hi << 32) | lo;
}

/* MACROS: Convenience profiling macros */

#define PROF_START(profiler, op, m, n, k) \
    ProfileContext _prof_ctx = prof_begin(profiler, op, m, n, k, 1)

#define PROF_END(profiler) \
    prof_end(profiler, &_prof_ctx)

#define PROF_SCOPE(profiler, op, m, n, k) \
    for(ProfileContext _prof_ctx = prof_begin(profiler, op, m, n, k, 1); \
        _prof_ctx.start_cycles; \
        prof_end(profiler, &_prof_ctx), _prof_ctx.start_cycles = 0)

/* Operation name lookup */
static const char* prof_op_names[PROF_OP_COUNT] = {
    "GEMM_F32",
    "GEMV_F32", 
    "LSTM_GATES",
    "LSTM_STATE_UPDATE",
    "DNC_CONTENT_ADDR",
    "DNC_MEMORY_READ",
    "DNC_MEMORY_WRITE",
    "DNC_TEMPORAL_LINK",
    "EWC_PENALTY",
    "ACTIVATION_TANH",
    "ACTIVATION_SIGMOID",
    "VECTOR_ADD",
    "VECTOR_MUL",
    "COSINE_SIMILARITY",
    "SOFTMAX"
};

#endif /* NEURAL_PROFILER_H */