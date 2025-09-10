#include "handmade_platform.h"
#include "handmade_threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void simple_job(void* data, u32 thread_index) {
    int* value = (int*)data;
    printf("Thread %u: Processing value %d\n", thread_index, *value);
}

int main() {
    printf("Starting simple threading test...\n");
    
    // Allocate memory arena (need enough for thread pool and thread-local arenas)
    MemoryArena arena;
    arena.size = MEGABYTES(128);  // Increased to 128MB
    arena.base = (u8*)malloc(arena.size);
    arena.used = 0;
    
    if (!arena.base) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }
    
    printf("Memory allocated: %zu MB\n", arena.size / (1024 * 1024));
    
    // Create thread pool
    printf("Creating thread pool...\n");
    ThreadPool* pool = thread_pool_create(2, &arena);  // Just 2 threads for testing
    
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        free(arena.base);
        return 1;
    }
    
    printf("Thread pool created with %u threads\n", pool->thread_count);
    
    // Submit a simple job
    int test_value = 42;
    printf("Submitting job...\n");
    Job* job = thread_pool_submit_job(pool, simple_job, &test_value, JOB_PRIORITY_NORMAL);
    
    printf("Waiting for job...\n");
    thread_pool_wait_for_job(pool, job);
    
    printf("Job completed!\n");
    
    // Cleanup
    printf("Destroying thread pool...\n");
    thread_pool_destroy(pool);
    
    free(arena.base);
    printf("Test completed successfully!\n");
    
    return 0;
}