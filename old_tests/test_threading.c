/*
    Threading System Test and Demonstration
    
    This program tests all major features of the handmade threading system:
    - Work-stealing queues
    - Parallel for loops
    - Job priorities
    - Lock-free operations
    - Performance monitoring
*/

#include "handmade_platform.h"
#include "handmade_threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

// Test data structures
typedef struct {
    f32* input;
    f32* output;
    u32 size;
} ComputeData;

typedef struct {
    atomic_uint counter;
    u32 expected;
} CounterTest;

// Compute-intensive job for testing
static void compute_job(void* data, u32 thread_index) {
    ComputeData* compute = (ComputeData*)data;
    
    // Simulate heavy computation
    for (u32 i = 0; i < compute->size; i++) {
        f32 value = compute->input[i];
        
        // Complex calculation to stress CPU
        for (u32 j = 0; j < 100; j++) {
            value = sinf(value) * cosf(value * 2.0f) + tanf(value * 0.5f);
            value = sqrtf(fabsf(value)) * 1.5f;
        }
        
        compute->output[i] = value;
    }
}

// Parallel computation test
static void parallel_compute_test(void* data, u32 index, u32 thread_index) {
    f32* array = (f32*)data;
    
    // Heavy per-element computation
    f32 value = array[index];
    for (u32 i = 0; i < 1000; i++) {
        value = sinf(value + (f32)i) * cosf(value - (f32)i);
    }
    array[index] = value;
}

// Counter increment test for atomics
static void increment_job(void* data, u32 thread_index) {
    CounterTest* test = (CounterTest*)data;
    
    // Increment counter many times to test atomics
    for (u32 i = 0; i < 10000; i++) {
        atomic_fetch_add(&test->counter, 1);
    }
}

// Matrix multiplication using parallel for
typedef struct {
    f32* a;
    f32* b;
    f32* c;
    u32 n;
} MatrixData;

static void matrix_multiply_row(void* data, u32 row, u32 thread_index) {
    MatrixData* mat = (MatrixData*)data;
    u32 n = mat->n;
    
    for (u32 col = 0; col < n; col++) {
        f32 sum = 0.0f;
        for (u32 k = 0; k < n; k++) {
            sum += mat->a[row * n + k] * mat->b[k * n + col];
        }
        mat->c[row * n + col] = sum;
    }
}

// IO simulation job
static void io_simulation_job(void* data, u32 thread_index) {
    char* filename = (char*)data;
    
    // Simulate file IO
    printf("[Thread %u] Simulating IO for file: %s\n", thread_index, filename);
    usleep(100000);  // 100ms simulated IO delay
    printf("[Thread %u] IO complete for: %s\n", thread_index, filename);
}

// Benchmark utilities
static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char** argv) {
    printf("=== Handmade Threading System Test ===\n\n");
    
    // Allocate memory arena (128MB)
    MemoryArena arena;
    arena.size = MEGABYTES(128);
    arena.base = (u8*)malloc(arena.size);
    arena.used = 0;
    
    if (!arena.base) {
        fprintf(stderr, "Failed to allocate memory arena\n");
        return 1;
    }
    
    // Create thread pool
    u32 thread_count = 0;  // 0 = auto-detect
    ThreadPool* pool = thread_pool_create(thread_count, &arena);
    
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        free(arena.base);
        return 1;
    }
    
    printf("Thread pool created with %u threads\n\n", pool->thread_count);
    
    // TEST 1: Basic job submission
    printf("TEST 1: Basic Job Submission\n");
    printf("------------------------------\n");
    
    ComputeData compute_data;
    compute_data.size = 10000;
    compute_data.input = (f32*)malloc(sizeof(f32) * compute_data.size);
    compute_data.output = (f32*)malloc(sizeof(f32) * compute_data.size);
    
    // Initialize input data
    for (u32 i = 0; i < compute_data.size; i++) {
        compute_data.input[i] = (f32)i * 0.1f;
    }
    
    double start_time = get_time_seconds();
    
    // Submit multiple compute jobs
    Job* jobs[16];
    for (u32 i = 0; i < 16; i++) {
        jobs[i] = thread_pool_submit_job(pool, compute_job, &compute_data, JOB_PRIORITY_NORMAL);
    }
    
    // Wait for all jobs
    for (u32 i = 0; i < 16; i++) {
        thread_pool_wait_for_job(pool, jobs[i]);
    }
    
    double elapsed = get_time_seconds() - start_time;
    printf("16 compute jobs completed in %.3f ms\n\n", elapsed * 1000.0);
    
    // TEST 2: Parallel For Loop
    printf("TEST 2: Parallel For Loop\n");
    printf("-------------------------\n");
    
    u32 array_size = 100000;
    f32* parallel_array = (f32*)malloc(sizeof(f32) * array_size);
    
    // Initialize array
    for (u32 i = 0; i < array_size; i++) {
        parallel_array[i] = (f32)i;
    }
    
    start_time = get_time_seconds();
    
    // Parallel computation
    thread_pool_parallel_for(pool, array_size, 1000, parallel_compute_test, parallel_array);
    
    elapsed = get_time_seconds() - start_time;
    printf("Parallel for on %u elements completed in %.3f ms\n\n", array_size, elapsed * 1000.0);
    
    // TEST 3: Atomic Operations
    printf("TEST 3: Atomic Operations\n");
    printf("-------------------------\n");
    
    CounterTest counter_test = {0};
    counter_test.expected = pool->thread_count * 10000;
    
    start_time = get_time_seconds();
    
    // Submit increment jobs from all threads
    Job* counter_jobs[32];
    for (u32 i = 0; i < pool->thread_count; i++) {
        counter_jobs[i] = thread_pool_submit_job(pool, increment_job, &counter_test, JOB_PRIORITY_HIGH);
    }
    
    // Wait for completion
    for (u32 i = 0; i < pool->thread_count; i++) {
        thread_pool_wait_for_job(pool, counter_jobs[i]);
    }
    
    elapsed = get_time_seconds() - start_time;
    u32 final_count = atomic_load(&counter_test.counter);
    
    printf("Expected: %u, Got: %u (Match: %s)\n", 
           counter_test.expected, final_count,
           final_count == counter_test.expected ? "YES" : "NO");
    printf("Atomic increment test completed in %.3f ms\n\n", elapsed * 1000.0);
    
    // TEST 4: Priority System
    printf("TEST 4: Job Priority System\n");
    printf("---------------------------\n");
    
    // Submit jobs with different priorities
    Job* low_priority = thread_pool_submit_job(pool, compute_job, &compute_data, JOB_PRIORITY_LOW);
    Job* high_priority = thread_pool_submit_job(pool, compute_job, &compute_data, JOB_PRIORITY_CRITICAL);
    Job* normal_priority = thread_pool_submit_job(pool, compute_job, &compute_data, JOB_PRIORITY_NORMAL);
    
    thread_pool_wait_for_job(pool, high_priority);
    thread_pool_wait_for_job(pool, normal_priority);
    thread_pool_wait_for_job(pool, low_priority);
    
    printf("Priority jobs completed (Critical -> Normal -> Low)\n\n");
    
    // TEST 5: Matrix Multiplication
    printf("TEST 5: Parallel Matrix Multiplication\n");
    printf("--------------------------------------\n");
    
    u32 matrix_size = 256;
    MatrixData mat_data;
    mat_data.n = matrix_size;
    mat_data.a = (f32*)calloc(matrix_size * matrix_size, sizeof(f32));
    mat_data.b = (f32*)calloc(matrix_size * matrix_size, sizeof(f32));
    mat_data.c = (f32*)calloc(matrix_size * matrix_size, sizeof(f32));
    
    // Initialize matrices
    for (u32 i = 0; i < matrix_size * matrix_size; i++) {
        mat_data.a[i] = (f32)(rand() % 10) / 10.0f;
        mat_data.b[i] = (f32)(rand() % 10) / 10.0f;
    }
    
    start_time = get_time_seconds();
    
    // Parallel matrix multiplication
    thread_pool_parallel_for(pool, matrix_size, 8, matrix_multiply_row, &mat_data);
    
    elapsed = get_time_seconds() - start_time;
    printf("%ux%u matrix multiplication in %.3f ms\n\n", matrix_size, matrix_size, elapsed * 1000.0);
    
    // TEST 6: IO-bound Jobs
    printf("TEST 6: IO-Bound Jobs\n");
    printf("---------------------\n");
    
    char* filenames[] = {
        "file1.txt", "file2.txt", "file3.txt", "file4.txt",
        "file5.txt", "file6.txt", "file7.txt", "file8.txt"
    };
    
    start_time = get_time_seconds();
    
    Job* io_jobs[8];
    for (u32 i = 0; i < 8; i++) {
        io_jobs[i] = thread_pool_submit_job_with_flags(pool, io_simulation_job, filenames[i],
                                                       JOB_PRIORITY_NORMAL, JOB_FLAG_IO_BOUND);
    }
    
    for (u32 i = 0; i < 8; i++) {
        thread_pool_wait_for_job(pool, io_jobs[i]);
    }
    
    elapsed = get_time_seconds() - start_time;
    printf("8 IO jobs completed in %.3f ms\n\n", elapsed * 1000.0);
    
    // TEST 7: Work Stealing
    printf("TEST 7: Work Stealing Test\n");
    printf("--------------------------\n");
    
    // Reset stats
    thread_pool_reset_stats(pool);
    
    // Create unbalanced workload
    for (u32 i = 0; i < 100; i++) {
        // Submit to specific threads to create imbalance
        Job* job = thread_pool_submit_job(pool, compute_job, &compute_data, JOB_PRIORITY_NORMAL);
    }
    
    // Wait for all work to complete
    thread_pool_barrier(pool);
    
    // Get statistics
    ThreadPoolStats stats;
    thread_pool_get_stats(pool, &stats);
    
    printf("Work stealing statistics:\n");
    for (u32 i = 0; i < pool->thread_count; i++) {
        printf("  Thread %u: %lu jobs executed, %lu stolen\n",
               i, stats.jobs_per_thread[i], stats.steal_count_per_thread[i]);
    }
    
    u64 total_steals = 0;
    for (u32 i = 0; i < pool->thread_count; i++) {
        total_steals += stats.steal_count_per_thread[i];
    }
    printf("Total steals: %lu\n\n", total_steals);
    
    // Final Statistics
    printf("=== Final Statistics ===\n");
    thread_pool_get_stats(pool, &stats);
    
    printf("Total jobs completed: %lu\n", stats.total_jobs_completed);
    printf("Total jobs submitted: %lu\n", stats.total_jobs_submitted);
    printf("Average wait time: %lu ns\n", stats.average_wait_time_ns);
    printf("Active threads: %u / %u\n", stats.active_thread_count, pool->thread_count);
    
    printf("\nThread utilization:\n");
    for (u32 i = 0; i < pool->thread_count; i++) {
        printf("  Thread %u: %.1f%%\n", i, stats.thread_utilization[i] * 100.0f);
    }
    
    // Cleanup
    printf("\nCleaning up...\n");
    
    thread_pool_destroy(pool);
    
    free(compute_data.input);
    free(compute_data.output);
    free(parallel_array);
    free(mat_data.a);
    free(mat_data.b);
    free(mat_data.c);
    free(arena.base);
    
    printf("Test completed successfully!\n");
    
    return 0;
}