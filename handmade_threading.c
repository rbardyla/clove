#include "handmade_threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <sys/syscall.h>
#include <linux/futex.h>

// Linux futex system call wrapper
static i32 futex_syscall(i32* uaddr, i32 futex_op, i32 val,
                        const struct timespec* timeout, i32* uaddr2, i32 val3) {
    return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

// Thread-local storage for current context
__thread ThreadContext* tls_current_context = NULL;

// Forward declarations
static void* thread_worker_main(void* arg);
static void* io_thread_main(void* arg);
static Job* job_queue_pop(JobQueue* queue);
static bool job_queue_push(JobQueue* queue, Job* job);
static Job* work_stealing_deque_steal(WorkStealingDeque* deque);

// Get number of CPU cores (exported for main.c)
u32 get_cpu_count(void) {
    i32 count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) count = 4;  // Default fallback
    if (count > MAX_THREAD_COUNT) count = MAX_THREAD_COUNT;
    return (u32)count;
}

// Create thread pool
ThreadPool* thread_pool_create(u32 thread_count, MemoryArena* arena) {
    if (thread_count == 0) {
        thread_count = get_cpu_count();
    }
    if (thread_count > MAX_THREAD_COUNT) {
        thread_count = MAX_THREAD_COUNT;
    }
    
    // Allocate pool from arena
    ThreadPool* pool = (ThreadPool*)(arena->base + arena->used);
    arena->used += sizeof(ThreadPool);
    memset(pool, 0, sizeof(ThreadPool));
    
    pool->thread_count = thread_count;
    pool->persistent_arena = arena;
    pool->io_thread_count = 4;  // Fixed IO thread count
    
    // Initialize semaphore
    sem_init(&pool->wake_semaphore, 0, 0);
    
    // Initialize job pool
    atomic_store(&pool->job_pool_index, 0);
    
    // Initialize priority queues
    for (u32 i = 0; i < JOB_PRIORITY_COUNT; i++) {
        atomic_store(&pool->priority_queues[i].head, 0);
        atomic_store(&pool->priority_queues[i].tail, 0);
        atomic_store(&pool->priority_queues[i].size, 0);
    }
    
    // Initialize IO queue
    atomic_store(&pool->io_queue.head, 0);
    atomic_store(&pool->io_queue.tail, 0);
    atomic_store(&pool->io_queue.size, 0);
    
    // Initialize work-stealing deques
    for (u32 i = 0; i < thread_count; i++) {
        atomic_store(&pool->deques[i].top, 0);
        atomic_store(&pool->deques[i].bottom, 0);
    }
    
    // Create worker threads
    for (u32 i = 0; i < thread_count; i++) {
        ThreadContext* context = &pool->threads[i];
        context->thread_index = i;
        context->pool = pool;
        context->deque = &pool->deques[i];
        context->running = true;
        
        // Allocate temp arena for each thread (16MB)
        usize temp_arena_size = MEGABYTES(16);
        
        // Check if we have enough memory
        if (arena->used + sizeof(MemoryArena) + temp_arena_size > arena->size) {
            fprintf(stderr, "Not enough memory in arena for thread %u\n", i);
            pool->thread_count = i;  // Adjust actual thread count
            break;
        }
        
        context->temp_arena = (MemoryArena*)(arena->base + arena->used);
        arena->used += sizeof(MemoryArena);
        context->temp_arena->base = arena->base + arena->used;
        arena->used += temp_arena_size;
        context->temp_arena->size = temp_arena_size;
        context->temp_arena->used = 0;
        
        // Create thread
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        
        // Set stack size (2MB)
        pthread_attr_setstacksize(&attr, MEGABYTES(2));
        
        if (pthread_create(&context->handle, &attr, thread_worker_main, context) != 0) {
            fprintf(stderr, "Failed to create worker thread %u\n", i);
        }
        
        pthread_attr_destroy(&attr);
        
        // Set thread affinity to specific CPU core
        thread_set_affinity(context, i % get_cpu_count());
        
        // Set thread name for debugging
        char thread_name[16];
        snprintf(thread_name, sizeof(thread_name), "Worker_%u", i);
        pthread_setname_np(context->handle, thread_name);
    }
    
    // Create IO threads
    for (u32 i = 0; i < pool->io_thread_count; i++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, MEGABYTES(1));
        
        if (pthread_create(&pool->io_threads[i], &attr, io_thread_main, pool) != 0) {
            fprintf(stderr, "Failed to create IO thread %u\n", i);
        }
        
        pthread_attr_destroy(&attr);
        
        char thread_name[16];
        snprintf(thread_name, sizeof(thread_name), "IO_%u", i);
        pthread_setname_np(pool->io_threads[i], thread_name);
    }
    
    return pool;
}

// Destroy thread pool
void thread_pool_destroy(ThreadPool* pool) {
    // Signal shutdown
    atomic_store(&pool->shutdown, true);
    
    // Wake all threads
    for (u32 i = 0; i < pool->thread_count; i++) {
        sem_post(&pool->wake_semaphore);
    }
    
    // Join worker threads
    for (u32 i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i].handle, NULL);
    }
    
    // Join IO threads
    for (u32 i = 0; i < pool->io_thread_count; i++) {
        pthread_join(pool->io_threads[i], NULL);
    }
    
    // Destroy semaphore
    sem_destroy(&pool->wake_semaphore);
}

// Worker thread main function
static void* thread_worker_main(void* arg) {
    ThreadContext* context = (ThreadContext*)arg;
    ThreadPool* pool = context->pool;
    tls_current_context = context;
    
    while (context->running) {
        Job* job = NULL;
        
        // Try to pop from local deque first
        job = thread_pop_job(context);
        
        if (!job) {
            // Try priority queues
            for (i32 p = JOB_PRIORITY_CRITICAL; p >= JOB_PRIORITY_LOW && !job; p--) {
                job = job_queue_pop(&pool->priority_queues[p]);
            }
        }
        
        if (!job) {
            // Try work stealing
            job = thread_steal_job(context);
        }
        
        if (job) {
            // Execute job
            atomic_fetch_add(&context->jobs_executed, 1);
            
            job->function(job->data, context->thread_index);
            
            // Mark job as complete
            atomic_fetch_sub(&job->unfinished_jobs, 1);
            
            // Notify parent if exists
            if (job->parent) {
                atomic_fetch_sub(&job->parent->unfinished_jobs, 1);
            }
            
            atomic_fetch_add(&pool->total_jobs_completed, 1);
        } else {
            // No work available
            atomic_fetch_add(&context->idle_cycles, 1);
            
            // Check for shutdown
            if (atomic_load(&pool->shutdown)) {
                break;
            }
            
            // Wait with timeout
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_nsec += 1000000;  // 1ms timeout
            if (timeout.tv_nsec >= 1000000000) {
                timeout.tv_sec++;
                timeout.tv_nsec -= 1000000000;
            }
            
            sem_timedwait(&pool->wake_semaphore, &timeout);
        }
    }
    
    return NULL;
}

// IO thread main function
static void* io_thread_main(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    
    while (!atomic_load(&pool->shutdown)) {
        Job* job = job_queue_pop(&pool->io_queue);
        
        if (job) {
            job->function(job->data, MAX_THREAD_COUNT);  // Special index for IO threads
            
            atomic_fetch_sub(&job->unfinished_jobs, 1);
            if (job->parent) {
                atomic_fetch_sub(&job->parent->unfinished_jobs, 1);
            }
            
            atomic_fetch_add(&pool->total_jobs_completed, 1);
        } else {
            // Sleep briefly when no IO work
            usleep(1000);  // 1ms
        }
    }
    
    return NULL;
}

// Allocate job from pool
static Job* allocate_job(ThreadPool* pool) {
    u32 index = atomic_fetch_add(&pool->job_pool_index, 1) & JOB_QUEUE_MASK;
    Job* job = &pool->job_pool[index];
    memset(job, 0, sizeof(Job));
    atomic_store(&job->unfinished_jobs, 1);
    return job;
}

// Submit job to thread pool
Job* thread_pool_submit_job(ThreadPool* pool, JobFunc func, void* data, JobPriority priority) {
    return thread_pool_submit_job_with_flags(pool, func, data, priority, JOB_FLAG_NONE);
}

// Submit job with flags
Job* thread_pool_submit_job_with_flags(ThreadPool* pool, JobFunc func, void* data,
                                       JobPriority priority, JobFlags flags) {
    Job* job = allocate_job(pool);
    job->function = func;
    job->data = data;
    job->priority = priority;
    job->flags = flags;
    
    atomic_fetch_add(&pool->total_jobs_submitted, 1);
    
    // Route to appropriate queue
    if (flags & JOB_FLAG_IO_BOUND) {
        job_queue_push(&pool->io_queue, job);
    } else if (tls_current_context && !(flags & JOB_FLAG_DETACHED)) {
        // Push to local deque if on worker thread
        thread_push_job(tls_current_context, job);
    } else {
        // Push to priority queue
        job_queue_push(&pool->priority_queues[priority], job);
    }
    
    // Wake a thread
    sem_post(&pool->wake_semaphore);
    
    return job;
}

// Wait for job completion
void thread_pool_wait_for_job(ThreadPool* pool, Job* job) {
    // Help execute jobs while waiting
    ThreadContext* context = tls_current_context;
    
    while (atomic_load(&job->unfinished_jobs) > 0) {
        Job* other_job = NULL;
        
        if (context) {
            other_job = thread_pop_job(context);
            if (!other_job) {
                other_job = thread_steal_job(context);
            }
        }
        
        if (other_job) {
            other_job->function(other_job->data, context ? context->thread_index : 0);
            atomic_fetch_sub(&other_job->unfinished_jobs, 1);
            if (other_job->parent) {
                atomic_fetch_sub(&other_job->parent->unfinished_jobs, 1);
            }
            atomic_fetch_add(&pool->total_jobs_completed, 1);
        } else {
            cpu_pause();
        }
    }
}

// Check if job is complete
bool thread_pool_is_job_complete(Job* job) {
    return atomic_load(&job->unfinished_jobs) == 0;
}

// Lock-free queue operations
static bool job_queue_push(JobQueue* queue, Job* job) {
    u32 tail = atomic_load(&queue->tail);
    u32 next_tail = (tail + 1) & JOB_QUEUE_MASK;
    u32 head = atomic_load(&queue->head);
    
    if (next_tail == head) {
        return false;  // Queue full
    }
    
    queue->jobs[tail] = job;
    atomic_store(&queue->tail, next_tail);
    atomic_fetch_add(&queue->size, 1);
    
    return true;
}

static Job* job_queue_pop(JobQueue* queue) {
    u32 head = atomic_load(&queue->head);
    u32 tail = atomic_load(&queue->tail);
    
    if (head == tail) {
        return NULL;  // Queue empty
    }
    
    Job* job = queue->jobs[head];
    atomic_store(&queue->head, (head + 1) & JOB_QUEUE_MASK);
    atomic_fetch_sub(&queue->size, 1);
    
    return job;
}

// Work-stealing deque operations
void thread_push_job(ThreadContext* context, Job* job) {
    WorkStealingDeque* deque = context->deque;
    
    i32 bottom = atomic_load(&deque->bottom);
    deque->jobs[bottom & JOB_QUEUE_MASK] = job;
    
    atomic_fence_release();
    atomic_store(&deque->bottom, bottom + 1);
}

Job* thread_pop_job(ThreadContext* context) {
    WorkStealingDeque* deque = context->deque;
    
    i32 bottom = atomic_load(&deque->bottom) - 1;
    atomic_store(&deque->bottom, bottom);
    
    atomic_fence_seq_cst();
    
    i32 top = atomic_load(&deque->top);
    
    if (top <= bottom) {
        Job* job = deque->jobs[bottom & JOB_QUEUE_MASK];
        
        if (top == bottom) {
            // Last job in deque, need CAS
            i32 expected = top;
            if (!atomic_compare_exchange_strong(&deque->top, &expected, top + 1)) {
                job = NULL;
            }
            atomic_store(&deque->bottom, top + 1);
        }
        return job;
    } else {
        // Deque empty
        atomic_store(&deque->bottom, top);
        return NULL;
    }
}

static Job* work_stealing_deque_steal(WorkStealingDeque* deque) {
    i32 top = atomic_load(&deque->top);
    
    atomic_fence_acquire();
    
    i32 bottom = atomic_load(&deque->bottom);
    
    if (top < bottom) {
        Job* job = deque->jobs[top & JOB_QUEUE_MASK];
        
        i32 expected = top;
        if (atomic_compare_exchange_strong(&deque->top, &expected, top + 1)) {
            return job;
        }
    }
    
    return NULL;
}

// Steal job from other threads
Job* thread_steal_job(ThreadContext* context) {
    ThreadPool* pool = context->pool;
    u32 thread_count = pool->thread_count;
    u32 current = context->thread_index;
    
    atomic_fetch_add(&context->steal_attempts, 1);
    
    // Try to steal from random thread
    u32 victim = (current + 1 + (rand() % (thread_count - 1))) % thread_count;
    
    for (u32 i = 0; i < thread_count - 1; i++) {
        if (victim != current) {
            Job* job = work_stealing_deque_steal(&pool->deques[victim]);
            if (job) {
                atomic_fetch_add(&context->jobs_stolen, 1);
                return job;
            }
        }
        victim = (victim + 1) % thread_count;
    }
    
    return NULL;
}

// Parallel for implementation
typedef struct ParallelForJob {
    ParallelForContext* context;
    ThreadPool* pool;
} ParallelForJob;

static void parallel_for_worker(void* data, u32 thread_index) {
    ParallelForJob* job_data = (ParallelForJob*)data;
    ParallelForContext* ctx = job_data->context;
    
    while (true) {
        u32 index = atomic_fetch_add(&ctx->next_index, ctx->batch_size);
        if (index >= ctx->count) break;
        
        u32 end = index + ctx->batch_size;
        if (end > ctx->count) end = ctx->count;
        
        for (u32 i = index; i < end; i++) {
            ctx->func(ctx->data, i, thread_index);
        }
    }
}

void thread_pool_parallel_for(ThreadPool* pool, u32 count, u32 batch_size,
                              void (*func)(void* data, u32 index, u32 thread_index),
                              void* data) {
    if (count == 0) return;
    
    // Determine optimal batch size if not specified
    if (batch_size == 0) {
        batch_size = (count + pool->thread_count * 4 - 1) / (pool->thread_count * 4);
        if (batch_size < 1) batch_size = 1;
        if (batch_size > 64) batch_size = 64;
    }
    
    ParallelForContext context = {
        .func = func,
        .data = data,
        .count = count,
        .batch_size = batch_size
    };
    atomic_store(&context.next_index, 0);
    
    ParallelForJob job_data = {
        .context = &context,
        .pool = pool
    };
    
    // Create parent job to track completion
    Job* parent = allocate_job(pool);
    u32 job_count = (count + batch_size - 1) / batch_size;
    atomic_store(&parent->unfinished_jobs, job_count + 1);
    
    // Submit worker jobs
    for (u32 i = 0; i < job_count && i < pool->thread_count * 2; i++) {
        Job* job = thread_pool_submit_job_with_flags(pool, parallel_for_worker, &job_data,
                                                     JOB_PRIORITY_HIGH, JOB_FLAG_PARALLEL_FOR);
        job->parent = parent;
    }
    
    // Help with work
    parallel_for_worker(&job_data, tls_current_context ? tls_current_context->thread_index : 0);
    
    // Mark parent as done with our part
    atomic_fetch_sub(&parent->unfinished_jobs, 1);
    
    // Wait for completion
    thread_pool_wait_for_job(pool, parent);
}

// Memory management
void* thread_pool_alloc_temp(ThreadContext* context, usize size) {
    if (!context || !context->temp_arena) return NULL;
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    if (context->temp_arena->used + size > context->temp_arena->size) {
        return NULL;  // Out of memory
    }
    
    void* result = context->temp_arena->base + context->temp_arena->used;
    context->temp_arena->used += size;
    
    return result;
}

void thread_pool_reset_temp(ThreadContext* context) {
    if (context && context->temp_arena) {
        context->temp_arena->used = 0;
    }
}

// Synchronization
void thread_pool_barrier(ThreadPool* pool) {
    // Simple barrier - wait for all jobs to complete
    while (atomic_load(&pool->total_jobs_submitted) != 
           atomic_load(&pool->total_jobs_completed)) {
        ThreadContext* context = tls_current_context;
        if (context) {
            Job* job = thread_pop_job(context);
            if (!job) job = thread_steal_job(context);
            
            if (job) {
                job->function(job->data, context->thread_index);
                atomic_fetch_sub(&job->unfinished_jobs, 1);
                if (job->parent) {
                    atomic_fetch_sub(&job->parent->unfinished_jobs, 1);
                }
                atomic_fetch_add(&pool->total_jobs_completed, 1);
            }
        }
        cpu_pause();
    }
}

void thread_pool_fence(void) {
    atomic_fence_seq_cst();
}

// Lock-free stack operations
void lock_free_stack_push(LockFreeStack* stack, LockFreeNode* node) {
    while (true) {
        uintptr_t head = atomic_load(&stack->head);
        node->next = (LockFreeNode*)head;
        
        if (atomic_compare_exchange_weak(&stack->head, &head, (uintptr_t)node)) {
            atomic_fetch_add(&stack->aba_counter, 1);
            break;
        }
        cpu_pause();
    }
}

LockFreeNode* lock_free_stack_pop(LockFreeStack* stack) {
    while (true) {
        uintptr_t head = atomic_load(&stack->head);
        if (head == 0) return NULL;
        
        LockFreeNode* node = (LockFreeNode*)head;
        uintptr_t next = (uintptr_t)node->next;
        
        if (atomic_compare_exchange_weak(&stack->head, &head, next)) {
            atomic_fetch_add(&stack->aba_counter, 1);
            return node;
        }
        cpu_pause();
    }
}

// Futex operations
void futex_wait(Futex* futex, i32 expected_value) {
    futex_syscall((i32*)&futex->value, FUTEX_WAIT_PRIVATE, expected_value, NULL, NULL, 0);
}

void futex_wake(Futex* futex, i32 wake_count) {
    futex_syscall((i32*)&futex->value, FUTEX_WAKE_PRIVATE, wake_count, NULL, NULL, 0);
}

void futex_wake_all(Futex* futex) {
    futex_wake(futex, INT32_MAX);
}

// Thread utilities
u32 thread_get_current_index(ThreadPool* pool) {
    ThreadContext* context = tls_current_context;
    return context ? context->thread_index : UINT32_MAX;
}

void thread_yield(void) {
    sched_yield();
}

void thread_set_affinity(ThreadContext* context, u32 core_index) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_index, &cpuset);
    
    pthread_setaffinity_np(context->handle, sizeof(cpu_set_t), &cpuset);
}

void thread_set_priority(ThreadContext* context, i32 priority) {
    struct sched_param param;
    param.sched_priority = priority;
    
    pthread_setschedparam(context->handle, SCHED_FIFO, &param);
}

// Statistics
void thread_pool_get_stats(ThreadPool* pool, ThreadPoolStats* stats) {
    stats->total_jobs_completed = atomic_load(&pool->total_jobs_completed);
    stats->total_jobs_submitted = atomic_load(&pool->total_jobs_submitted);
    stats->active_thread_count = 0;
    
    for (u32 i = 0; i < pool->thread_count; i++) {
        ThreadContext* ctx = &pool->threads[i];
        stats->jobs_per_thread[i] = atomic_load(&ctx->jobs_executed);
        stats->steal_count_per_thread[i] = atomic_load(&ctx->jobs_stolen);
        
        u64 idle = atomic_load(&ctx->idle_cycles);
        u64 executed = stats->jobs_per_thread[i];
        if (executed > 0) {
            stats->thread_utilization[i] = (f32)executed / (f32)(executed + idle);
            stats->active_thread_count++;
        } else {
            stats->thread_utilization[i] = 0.0f;
        }
    }
    
    u64 total_wait = atomic_load(&pool->total_wait_time_ns);
    if (stats->total_jobs_completed > 0) {
        stats->average_wait_time_ns = total_wait / stats->total_jobs_completed;
    } else {
        stats->average_wait_time_ns = 0;
    }
}

void thread_pool_reset_stats(ThreadPool* pool) {
    atomic_store(&pool->total_jobs_completed, 0);
    atomic_store(&pool->total_jobs_submitted, 0);
    atomic_store(&pool->total_wait_time_ns, 0);
    
    for (u32 i = 0; i < pool->thread_count; i++) {
        ThreadContext* ctx = &pool->threads[i];
        atomic_store(&ctx->jobs_executed, 0);
        atomic_store(&ctx->jobs_stolen, 0);
        atomic_store(&ctx->steal_attempts, 0);
        atomic_store(&ctx->idle_cycles, 0);
    }
}

// Debugging
void thread_pool_dump_state(ThreadPool* pool) {
    if (!pool) return;
    
    printf("\n=== Thread Pool State ===\n");
    printf("Threads: %u\n", pool->thread_count);
    printf("Jobs submitted: %lu\n", (unsigned long)atomic_load(&pool->total_jobs_submitted));
    printf("Jobs completed: %lu\n", (unsigned long)atomic_load(&pool->total_jobs_completed));
    
    for (u32 i = 0; i < JOB_PRIORITY_COUNT; i++) {
        printf("Priority Queue %u size: %u\n", i, atomic_load(&pool->priority_queues[i].size));
    }
    
    printf("\nPer-thread stats:\n");
    for (u32 i = 0; i < pool->thread_count; i++) {
        ThreadContext* ctx = &pool->threads[i];
        printf("  Thread %u: executed=%u stolen=%u steal_attempts=%u idle=%u\n",
               i,
               atomic_load(&ctx->jobs_executed),
               atomic_load(&ctx->jobs_stolen),
               atomic_load(&ctx->steal_attempts),
               atomic_load(&ctx->idle_cycles));
    }
    printf("========================\n\n");
}

void thread_pool_validate(ThreadPool* pool) {
    // Validate all data structures
    THREAD_ASSERT(pool != NULL);
    THREAD_ASSERT(pool->thread_count > 0 && pool->thread_count <= MAX_THREAD_COUNT);
    
    // Check job counts
    u64 submitted = atomic_load(&pool->total_jobs_submitted);
    u64 completed = atomic_load(&pool->total_jobs_completed);
    THREAD_ASSERT(completed <= submitted);
    
    // Validate queues
    for (u32 i = 0; i < JOB_PRIORITY_COUNT; i++) {
        JobQueue* queue = &pool->priority_queues[i];
        u32 head = atomic_load(&queue->head);
        u32 tail = atomic_load(&queue->tail);
        u32 size = atomic_load(&queue->size);
        
        u32 actual_size = (tail - head) & JOB_QUEUE_MASK;
        THREAD_ASSERT(size == actual_size);
    }
    
    (void)submitted;  // Suppress unused warnings
    (void)completed;
}