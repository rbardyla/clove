#ifndef HANDMADE_THREADING_H
#define HANDMADE_THREADING_H

#include "handmade_platform.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <immintrin.h>  // For SSE/AVX intrinsics

// Configuration
#define MAX_THREAD_COUNT 64
#define MAX_JOB_COUNT 4096
#define JOB_QUEUE_MASK (MAX_JOB_COUNT - 1)
#define CACHE_LINE_SIZE 64
#define MAX_FIBER_COUNT 256
#define FIBER_STACK_SIZE (64 * 1024)  // 64KB per fiber

// Forward declarations
typedef struct ThreadPool ThreadPool;
typedef struct JobQueue JobQueue;
typedef struct Job Job;
typedef struct ThreadContext ThreadContext;

// Job function signature
typedef void (*JobFunc)(void* data, u32 thread_index);

// Job priorities
typedef enum {
    JOB_PRIORITY_LOW = 0,
    JOB_PRIORITY_NORMAL = 1,
    JOB_PRIORITY_HIGH = 2,
    JOB_PRIORITY_CRITICAL = 3,
    JOB_PRIORITY_COUNT
} JobPriority;

// Job flags
typedef enum {
    JOB_FLAG_NONE = 0,
    JOB_FLAG_LONG_RUNNING = (1 << 0),
    JOB_FLAG_IO_BOUND = (1 << 1),
    JOB_FLAG_DETACHED = (1 << 2),
    JOB_FLAG_PARALLEL_FOR = (1 << 3),
} JobFlags;

// Aligned to cache line to prevent false sharing
typedef struct Job {
    JobFunc function;
    void* data;
    atomic_uint unfinished_jobs;  // For job dependencies
    struct Job* parent;
    JobPriority priority;
    JobFlags flags;
    char padding[CACHE_LINE_SIZE - 48];  // Ensure cache line alignment
} Job;

// Lock-free MPMC queue using ring buffer
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) JobQueue {
    atomic_uint head __attribute__((aligned(CACHE_LINE_SIZE)));
    atomic_uint tail __attribute__((aligned(CACHE_LINE_SIZE)));
    Job* jobs[MAX_JOB_COUNT] __attribute__((aligned(CACHE_LINE_SIZE)));
    atomic_uint size __attribute__((aligned(CACHE_LINE_SIZE)));
} JobQueue;

// Work-stealing deque for each thread
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) WorkStealingDeque {
    atomic_int top __attribute__((aligned(CACHE_LINE_SIZE)));
    atomic_int bottom __attribute__((aligned(CACHE_LINE_SIZE)));
    Job* jobs[MAX_JOB_COUNT] __attribute__((aligned(CACHE_LINE_SIZE)));
} WorkStealingDeque;

// Thread-local context
typedef struct ThreadContext {
    u32 thread_index;
    ThreadPool* pool;
    WorkStealingDeque* deque;
    
    // Thread-local allocator
    MemoryArena* temp_arena;
    
    // Performance counters
    atomic_uint jobs_executed;
    atomic_uint jobs_stolen;
    atomic_uint steal_attempts;
    atomic_uint idle_cycles;
    
    // Fiber support
    void* fiber_stack;
    void* current_fiber;
    
    pthread_t handle;
    bool running;
    // Padding to avoid false sharing (adjust size as needed)
    char padding[CACHE_LINE_SIZE];
} ThreadContext;

// Main thread pool structure
typedef struct ThreadPool {
    // Thread contexts
    ThreadContext threads[MAX_THREAD_COUNT];
    u32 thread_count;
    
    // Global job queues by priority
    JobQueue priority_queues[JOB_PRIORITY_COUNT];
    
    // Work-stealing deques (one per thread)
    WorkStealingDeque deques[MAX_THREAD_COUNT];
    
    // Synchronization
    sem_t wake_semaphore;
    atomic_bool shutdown;
    
    // Job pool for allocation-free job creation
    Job job_pool[MAX_JOB_COUNT];
    atomic_uint job_pool_index;
    
    // Memory management
    MemoryArena* persistent_arena;
    MemoryArena* frame_arena;
    
    // Statistics
    atomic_uint total_jobs_completed;
    atomic_uint total_jobs_submitted;
    atomic_ulong total_wait_time_ns;
    
    // IO thread pool (separate from compute threads)
    pthread_t io_threads[4];
    JobQueue io_queue;
    u32 io_thread_count;
} ThreadPool;

// Parallel for structure
typedef struct ParallelForContext {
    void (*func)(void* data, u32 index, u32 thread_index);
    void* data;
    atomic_uint next_index;
    u32 count;
    u32 batch_size;
} ParallelForContext;

// Lock-free structures
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) LockFreeStack {
    atomic_uintptr_t head __attribute__((aligned(CACHE_LINE_SIZE)));
    atomic_uint aba_counter __attribute__((aligned(CACHE_LINE_SIZE)));
} LockFreeStack;

typedef struct LockFreeNode {
    struct LockFreeNode* next;
    void* data;
} LockFreeNode;

// Hazard pointer for safe memory reclamation
typedef struct HazardPointer {
    atomic_uintptr_t pointer;
    char padding[CACHE_LINE_SIZE - sizeof(atomic_uintptr_t)];
} HazardPointer;

typedef struct HazardPointerDomain {
    HazardPointer pointers[MAX_THREAD_COUNT * 2];
    atomic_uint retired_count;
    void* retired_list[1024];
} HazardPointerDomain;

// Read-Copy-Update (RCU) for lock-free reads
typedef struct RCUContext {
    atomic_uint global_counter;
    atomic_uint thread_counters[MAX_THREAD_COUNT];
    void* (*updater)(void* old_data);
} RCUContext;

// Futex wrapper for efficient waiting
typedef struct Futex {
    atomic_int value;
} Futex;

// Main API functions
u32 get_cpu_count(void);
ThreadPool* thread_pool_create(u32 thread_count, MemoryArena* arena);
void thread_pool_destroy(ThreadPool* pool);

// Job submission
Job* thread_pool_submit_job(ThreadPool* pool, JobFunc func, void* data, JobPriority priority);
Job* thread_pool_submit_job_with_flags(ThreadPool* pool, JobFunc func, void* data, 
                                       JobPriority priority, JobFlags flags);
void thread_pool_wait_for_job(ThreadPool* pool, Job* job);
bool thread_pool_is_job_complete(Job* job);

// Parallel for loops
void thread_pool_parallel_for(ThreadPool* pool, u32 count, u32 batch_size,
                              void (*func)(void* data, u32 index, u32 thread_index),
                              void* data);

// Advanced parallel patterns
void thread_pool_parallel_reduce(ThreadPool* pool, u32 count,
                                 void* (*map_func)(void* data, u32 index),
                                 void* (*reduce_func)(void* a, void* b),
                                 void* data, void* result);

void thread_pool_parallel_scan(ThreadPool* pool, u32 count,
                               void* (*scan_func)(void* acc, void* element),
                               void* data, void* result);

// Work stealing
Job* thread_steal_job(ThreadContext* context);
void thread_push_job(ThreadContext* context, Job* job);
Job* thread_pop_job(ThreadContext* context);

// Memory management
void* thread_pool_alloc_temp(ThreadContext* context, usize size);
void thread_pool_reset_temp(ThreadContext* context);

// Synchronization primitives
void thread_pool_barrier(ThreadPool* pool);
void thread_pool_fence(void);

// Lock-free data structures
void lock_free_stack_push(LockFreeStack* stack, LockFreeNode* node);
LockFreeNode* lock_free_stack_pop(LockFreeStack* stack);

// Hazard pointers
HazardPointer* hazard_pointer_acquire(HazardPointerDomain* domain, u32 thread_index);
void hazard_pointer_release(HazardPointer* hp);
void hazard_pointer_retire(HazardPointerDomain* domain, void* ptr);

// RCU operations
void rcu_read_lock(RCUContext* rcu, u32 thread_index);
void rcu_read_unlock(RCUContext* rcu, u32 thread_index);
void rcu_synchronize(RCUContext* rcu);
void* rcu_update(RCUContext* rcu, void* old_data);

// Futex operations
void futex_wait(Futex* futex, i32 expected_value);
void futex_wake(Futex* futex, i32 wake_count);
void futex_wake_all(Futex* futex);

// Thread-local storage
void* thread_get_local(ThreadContext* context, u32 key);
void thread_set_local(ThreadContext* context, u32 key, void* value);

// Performance monitoring
typedef struct ThreadPoolStats {
    u64 total_jobs_completed;
    u64 total_jobs_submitted;
    u64 average_wait_time_ns;
    u64 jobs_per_thread[MAX_THREAD_COUNT];
    u64 steal_count_per_thread[MAX_THREAD_COUNT];
    f32 thread_utilization[MAX_THREAD_COUNT];
    u32 active_thread_count;
} ThreadPoolStats;

void thread_pool_get_stats(ThreadPool* pool, ThreadPoolStats* stats);
void thread_pool_reset_stats(ThreadPool* pool);

// Atomic operations wrappers
static inline u32 atomic_inc_u32(atomic_uint* value) {
    return atomic_fetch_add(value, 1);
}

static inline u32 atomic_dec_u32(atomic_uint* value) {
    return atomic_fetch_sub(value, 1);
}

static inline bool atomic_cas_u32(atomic_uint* ptr, u32* expected, u32 desired) {
    return atomic_compare_exchange_weak(ptr, expected, desired);
}

static inline void atomic_fence_acquire(void) {
    atomic_thread_fence(memory_order_acquire);
}

static inline void atomic_fence_release(void) {
    atomic_thread_fence(memory_order_release);
}

static inline void atomic_fence_seq_cst(void) {
    atomic_thread_fence(memory_order_seq_cst);
}

// CPU pause for spin-wait loops
static inline void cpu_pause(void) {
    _mm_pause();
}

// Get current thread index
u32 thread_get_current_index(ThreadPool* pool);

// Yield to scheduler
void thread_yield(void);

// Set thread affinity
void thread_set_affinity(ThreadContext* context, u32 core_index);

// Priority management
void thread_set_priority(ThreadContext* context, i32 priority);

// Debugging support
void thread_pool_dump_state(ThreadPool* pool);
void thread_pool_validate(ThreadPool* pool);

#ifdef HANDMADE_DEBUG
    #define THREAD_ASSERT(expr) do { if (!(expr)) { thread_pool_dump_state(0); __builtin_trap(); } } while(0)
#else
    #define THREAD_ASSERT(expr) ((void)0)
#endif

#endif // HANDMADE_THREADING_H