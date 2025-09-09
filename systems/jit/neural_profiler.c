/*
 * NEURAL PROFILER IMPLEMENTATION
 * ==============================
 * 
 * Cycle-accurate profiling with minimal overhead.
 * Identifies JIT compilation opportunities automatically.
 */

#include "neural_profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* MEMORY: Allocate executable memory for profiling */
static void* prof_alloc_memory(size_t size) {
    void* mem = mmap(NULL, size, 
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL;
    return mem;
}

/* Initialize profiler with memory budget */
NeuralProfiler* prof_create(size_t memory_mb) {
    size_t total_size = memory_mb * 1024 * 1024;
    
    /* Allocate profiler structure */
    NeuralProfiler* profiler = (NeuralProfiler*)prof_alloc_memory(sizeof(NeuralProfiler));
    if (!profiler) return NULL;
    
    memset(profiler, 0, sizeof(NeuralProfiler));
    
    /* Allocate hash table for profiles */
    profiler->num_buckets = 16384;  /* 16K buckets */
    size_t bucket_size = profiler->num_buckets * sizeof(ProfileBucket);
    profiler->buckets = (ProfileBucket*)prof_alloc_memory(bucket_size);
    if (!profiler->buckets) {
        munmap(profiler, sizeof(NeuralProfiler));
        return NULL;
    }
    memset(profiler->buckets, 0, bucket_size);
    
    /* Allocate memory pool for additional data */
    profiler->memory_size = total_size - sizeof(NeuralProfiler) - bucket_size;
    profiler->memory_pool = (uint8_t*)prof_alloc_memory(profiler->memory_size);
    if (!profiler->memory_pool) {
        munmap(profiler->buckets, bucket_size);
        munmap(profiler, sizeof(NeuralProfiler));
        return NULL;
    }
    
    /* Set default JIT thresholds */
    profiler->jit_threshold_calls = 1000;      /* 1000 calls minimum */
    profiler->jit_threshold_cycles = 1000000;  /* 1M cycles minimum */
    profiler->jit_threshold_percent = 0.01f;   /* 1% of total time */
    
    /* Enable profiling by default */
    profiler->enabled = 1;
    profiler->auto_jit = 1;
    profiler->detailed_stats = 0;
    
    /* Initialize min cycles to max value */
    for (size_t i = 0; i < profiler->num_buckets; i++) {
        for (int j = 0; j < 8; j++) {
            profiler->buckets[i].entries[j].min_cycles = UINT64_MAX;
        }
    }
    
    printf("PROFILER: Initialized with %zu MB memory\n", memory_mb);
    printf("PROFILER: Hash table buckets: %zu\n", profiler->num_buckets);
    printf("PROFILER: JIT thresholds - calls: %lu, cycles: %lu, percent: %.2f%%\n",
           profiler->jit_threshold_calls, profiler->jit_threshold_cycles,
           profiler->jit_threshold_percent * 100);
    
    return profiler;
}

/* Destroy profiler and free all resources */
void prof_destroy(NeuralProfiler* profiler) {
    if (!profiler) return;
    
    /* Print final statistics */
    printf("\nPROFILER: Final Statistics\n");
    printf("=========================================\n");
    prof_print_summary(profiler);
    
    /* Free memory */
    if (profiler->memory_pool) {
        munmap(profiler->memory_pool, profiler->memory_size);
    }
    if (profiler->buckets) {
        munmap(profiler->buckets, profiler->num_buckets * sizeof(ProfileBucket));
    }
    munmap(profiler, sizeof(NeuralProfiler));
}

/* Enable/disable profiling */
void prof_enable(NeuralProfiler* profiler) {
    if (profiler) profiler->enabled = 1;
}

void prof_disable(NeuralProfiler* profiler) {
    if (profiler) profiler->enabled = 0;
}

/* Find or create profile entry */
static ProfileEntry* prof_find_or_create(NeuralProfiler* profiler, uint64_t hash,
                                        ProfileOpType op, uint32_t m, uint32_t n, 
                                        uint32_t k, uint32_t batch) {
    uint64_t bucket_idx = hash % profiler->num_buckets;
    ProfileBucket* bucket = &profiler->buckets[bucket_idx];
    
    /* Search for existing entry */
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        if (bucket->entries[i].op_hash == hash) {
            return &bucket->entries[i];
        }
    }
    
    /* Create new entry if space available */
    if (bucket->num_entries < 8) {
        ProfileEntry* entry = &bucket->entries[bucket->num_entries++];
        entry->op_hash = hash;
        entry->dims[0] = m;
        entry->dims[1] = n;
        entry->dims[2] = k;
        entry->dims[3] = batch;
        entry->min_cycles = UINT64_MAX;
        return entry;
    }
    
    /* Bucket full - evict least recently used */
    ProfileEntry* lru = &bucket->entries[0];
    for (uint32_t i = 1; i < 8; i++) {
        if (bucket->entries[i].last_cycles < lru->last_cycles) {
            lru = &bucket->entries[i];
        }
    }
    
    /* Reset and reuse LRU entry */
    memset(lru, 0, sizeof(ProfileEntry));
    lru->op_hash = hash;
    lru->dims[0] = m;
    lru->dims[1] = n;
    lru->dims[2] = k;
    lru->dims[3] = batch;
    lru->min_cycles = UINT64_MAX;
    return lru;
}

/* Start profiling for an operation */
ProfileContext prof_begin(NeuralProfiler* profiler, ProfileOpType op,
                         uint32_t m, uint32_t n, uint32_t k, uint32_t batch) {
    ProfileContext ctx = {0};
    
    if (!profiler || !profiler->enabled) {
        return ctx;
    }
    
    ctx.op_type = op;
    ctx.dims[0] = m;
    ctx.dims[1] = n;
    ctx.dims[2] = k;
    ctx.dims[3] = batch;
    
    /* PERFORMANCE: Use RDTSCP for serializing instruction */
    uint32_t aux;
    __asm__ __volatile__ ("rdtscp" : "=a"(ctx.start_cycles), "=d"(aux) : : "ecx");
    ctx.start_cycles |= ((uint64_t)aux << 32);
    
    return ctx;
}

/* End profiling for an operation */
void prof_end(NeuralProfiler* profiler, ProfileContext* ctx) {
    if (!profiler || !profiler->enabled || !ctx->start_cycles) {
        return;
    }
    
    /* PERFORMANCE: Use RDTSCP for serializing instruction */
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi) : : "ecx");
    uint64_t end_cycles = ((uint64_t)hi << 32) | lo;
    
    uint64_t elapsed = end_cycles - ctx->start_cycles;
    
    /* Subtract profiling overhead (approximately 100 cycles) */
    if (elapsed > 100) elapsed -= 100;
    
    /* Update profile entry */
    uint64_t hash = prof_hash_op(ctx->op_type, ctx->dims[0], ctx->dims[1], 
                                ctx->dims[2], ctx->dims[3]);
    ProfileEntry* entry = prof_find_or_create(profiler, hash, ctx->op_type,
                                             ctx->dims[0], ctx->dims[1],
                                             ctx->dims[2], ctx->dims[3]);
    if (entry) {
        entry->call_count++;
        entry->total_cycles += elapsed;
        entry->last_cycles = elapsed;
        
        if (elapsed < entry->min_cycles) entry->min_cycles = elapsed;
        if (elapsed > entry->max_cycles) entry->max_cycles = elapsed;
        
        /* Calculate average cycles per element */
        uint64_t elements = ctx->dims[0] * ctx->dims[1];
        if (ctx->dims[2] > 0) elements *= ctx->dims[2];
        if (elements > 0) {
            entry->avg_cycles_per_element = 
                (float)entry->total_cycles / (float)(entry->call_count * elements);
        }
        
        /* Check if this should be a JIT candidate */
        if (!entry->jit_compiled && !entry->jit_candidate) {
            if (entry->call_count >= profiler->jit_threshold_calls &&
                entry->total_cycles >= profiler->jit_threshold_cycles) {
                entry->jit_candidate = 1;
                profiler->op_stats[ctx->op_type].jit_candidates++;
            }
        }
    }
    
    /* Update global statistics */
    profiler->op_stats[ctx->op_type].total_calls++;
    profiler->op_stats[ctx->op_type].total_cycles += elapsed;
    profiler->total_profiled_cycles += elapsed;
}

/* Mark operation as JIT compiled */
void prof_mark_jit_compiled(NeuralProfiler* profiler, uint64_t op_hash, float speedup) {
    if (!profiler) return;
    
    uint64_t bucket_idx = op_hash % profiler->num_buckets;
    ProfileBucket* bucket = &profiler->buckets[bucket_idx];
    
    for (uint32_t i = 0; i < bucket->num_entries; i++) {
        if (bucket->entries[i].op_hash == op_hash) {
            bucket->entries[i].jit_compiled = 1;
            bucket->entries[i].jit_candidate = 0;
            bucket->entries[i].speedup_factor = speedup;
            
            /* Update statistics */
            for (int op = 0; op < PROF_OP_COUNT; op++) {
                if (bucket->entries[i].dims[0] > 0) {  /* Valid entry */
                    profiler->op_stats[op].jit_compiled++;
                    break;
                }
            }
            return;
        }
    }
}

/* Analyze and identify hotspots */
void prof_analyze_hotspots(NeuralProfiler* profiler) {
    if (!profiler) return;
    
    /* Clear existing hotspots */
    profiler->num_hotspots = 0;
    memset(profiler->hotspots, 0, sizeof(profiler->hotspots));
    
    /* Find top 32 operations by total cycles */
    for (size_t b = 0; b < profiler->num_buckets; b++) {
        ProfileBucket* bucket = &profiler->buckets[b];
        
        for (uint32_t e = 0; e < bucket->num_entries; e++) {
            ProfileEntry* entry = &bucket->entries[e];
            if (entry->call_count == 0) continue;
            
            /* Check if this entry should be in hotspots */
            uint32_t insert_pos = profiler->num_hotspots;
            for (uint32_t h = 0; h < profiler->num_hotspots; h++) {
                if (entry->total_cycles > profiler->hotspots[h]->total_cycles) {
                    insert_pos = h;
                    break;
                }
            }
            
            if (insert_pos < 32) {
                /* Shift entries down */
                for (uint32_t h = 31; h > insert_pos; h--) {
                    profiler->hotspots[h] = profiler->hotspots[h-1];
                }
                profiler->hotspots[insert_pos] = entry;
                if (profiler->num_hotspots < 32) {
                    profiler->num_hotspots++;
                }
            }
        }
    }
}

/* Get JIT compilation candidates */
ProfileEntry** prof_get_jit_candidates(NeuralProfiler* profiler, uint32_t* count) {
    if (!profiler || !count) return NULL;
    
    static ProfileEntry* candidates[256];
    uint32_t num_candidates = 0;
    
    for (size_t b = 0; b < profiler->num_buckets && num_candidates < 256; b++) {
        ProfileBucket* bucket = &profiler->buckets[b];
        
        for (uint32_t e = 0; e < bucket->num_entries && num_candidates < 256; e++) {
            ProfileEntry* entry = &bucket->entries[e];
            if (entry->jit_candidate && !entry->jit_compiled) {
                candidates[num_candidates++] = entry;
            }
        }
    }
    
    *count = num_candidates;
    return candidates;
}

/* Print profiling summary */
void prof_print_summary(NeuralProfiler* profiler) {
    if (!profiler) return;
    
    printf("\nPROFILING SUMMARY\n");
    printf("=================\n");
    printf("Total profiled cycles: %lu\n", profiler->total_profiled_cycles);
    printf("\nOperation Statistics:\n");
    printf("%-20s %10s %15s %10s %10s\n", 
           "Operation", "Calls", "Total Cycles", "JIT Cand.", "JIT Comp.");
    printf("%-20s %10s %15s %10s %10s\n",
           "--------------------", "----------", "---------------", 
           "----------", "----------");
    
    for (int op = 0; op < PROF_OP_COUNT; op++) {
        if (profiler->op_stats[op].total_calls > 0) {
            printf("%-20s %10lu %15lu %10lu %10lu\n",
                   prof_op_names[op],
                   profiler->op_stats[op].total_calls,
                   profiler->op_stats[op].total_cycles,
                   profiler->op_stats[op].jit_candidates,
                   profiler->op_stats[op].jit_compiled);
        }
    }
}

/* Print top N hotspots */
void prof_print_hotspots(NeuralProfiler* profiler, uint32_t top_n) {
    if (!profiler) return;
    
    prof_analyze_hotspots(profiler);
    
    printf("\nTOP %u HOTSPOTS\n", top_n);
    printf("===============\n");
    printf("%3s %-15s %8s %12s %12s %12s %8s %6s %10s\n",
           "#", "Operation", "Dims", "Calls", "Total Cyc", "Avg Cyc", 
           "Cyc/Elem", "JIT", "Speedup");
    printf("%3s %-15s %8s %12s %12s %12s %8s %6s %10s\n",
           "---", "---------------", "--------", "------------", "------------",
           "------------", "--------", "------", "----------");
    
    for (uint32_t i = 0; i < top_n && i < profiler->num_hotspots; i++) {
        ProfileEntry* entry = profiler->hotspots[i];
        if (!entry || entry->call_count == 0) continue;
        
        /* Determine operation type from hash and dims */
        const char* op_name = "UNKNOWN";
        for (int op = 0; op < PROF_OP_COUNT; op++) {
            uint64_t test_hash = prof_hash_op(op, entry->dims[0], entry->dims[1],
                                             entry->dims[2], entry->dims[3]);
            if (test_hash == entry->op_hash) {
                op_name = prof_op_names[op];
                break;
            }
        }
        
        char dims_str[32];
        if (entry->dims[2] > 0) {
            snprintf(dims_str, sizeof(dims_str), "%ux%ux%u", 
                    entry->dims[0], entry->dims[1], entry->dims[2]);
        } else {
            snprintf(dims_str, sizeof(dims_str), "%ux%u",
                    entry->dims[0], entry->dims[1]);
        }
        
        uint64_t avg_cycles = entry->total_cycles / entry->call_count;
        
        printf("%3u %-15s %8s %12lu %12lu %12lu %8.2f %6s %10.2fx\n",
               i + 1,
               op_name,
               dims_str,
               entry->call_count,
               entry->total_cycles,
               avg_cycles,
               entry->avg_cycles_per_element,
               entry->jit_compiled ? "YES" : (entry->jit_candidate ? "CAND" : "NO"),
               entry->speedup_factor > 0 ? entry->speedup_factor : 1.0f);
    }
    
    /* Calculate percentage of time in top hotspots */
    uint64_t hotspot_cycles = 0;
    for (uint32_t i = 0; i < top_n && i < profiler->num_hotspots; i++) {
        if (profiler->hotspots[i]) {
            hotspot_cycles += profiler->hotspots[i]->total_cycles;
        }
    }
    
    if (profiler->total_profiled_cycles > 0) {
        float percent = (float)hotspot_cycles * 100.0f / (float)profiler->total_profiled_cycles;
        printf("\nTop %u hotspots account for %.2f%% of total execution time\n",
               top_n, percent);
    }
}

/* Export profiling data to file */
void prof_export_data(NeuralProfiler* profiler, const char* filename) {
    if (!profiler || !filename) return;
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("ERROR: Could not open file %s for writing\n", filename);
        return;
    }
    
    fprintf(file, "# Neural Profiler Data Export\n");
    fprintf(file, "# Total Cycles: %lu\n", profiler->total_profiled_cycles);
    fprintf(file, "# Format: OpHash,OpType,M,N,K,Batch,Calls,TotalCycles,MinCycles,"
                  "MaxCycles,AvgCycles,CyclesPerElem,JIT,Speedup\n");
    
    for (size_t b = 0; b < profiler->num_buckets; b++) {
        ProfileBucket* bucket = &profiler->buckets[b];
        
        for (uint32_t e = 0; e < bucket->num_entries; e++) {
            ProfileEntry* entry = &bucket->entries[e];
            if (entry->call_count == 0) continue;
            
            /* Determine operation type */
            int op_type = -1;
            for (int op = 0; op < PROF_OP_COUNT; op++) {
                uint64_t test_hash = prof_hash_op(op, entry->dims[0], entry->dims[1],
                                                 entry->dims[2], entry->dims[3]);
                if (test_hash == entry->op_hash) {
                    op_type = op;
                    break;
                }
            }
            
            uint64_t avg_cycles = entry->total_cycles / entry->call_count;
            
            fprintf(file, "0x%016lx,%d,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%.2f,%d,%.2f\n",
                   entry->op_hash,
                   op_type,
                   entry->dims[0], entry->dims[1], entry->dims[2], entry->dims[3],
                   entry->call_count,
                   entry->total_cycles,
                   entry->min_cycles,
                   entry->max_cycles,
                   avg_cycles,
                   entry->avg_cycles_per_element,
                   entry->jit_compiled,
                   entry->speedup_factor);
        }
    }
    
    fclose(file);
    printf("PROFILER: Exported data to %s\n", filename);
}

/* Reset all profiling data */
void prof_reset(NeuralProfiler* profiler) {
    if (!profiler) return;
    
    /* Clear buckets */
    memset(profiler->buckets, 0, profiler->num_buckets * sizeof(ProfileBucket));
    
    /* Reset statistics */
    memset(profiler->op_stats, 0, sizeof(profiler->op_stats));
    
    /* Clear hotspots */
    profiler->num_hotspots = 0;
    memset(profiler->hotspots, 0, sizeof(profiler->hotspots));
    
    /* Reset counters */
    profiler->total_profiled_cycles = 0;
    profiler->profile_overhead_cycles = 0;
    
    printf("PROFILER: Reset all profiling data\n");
}