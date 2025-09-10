#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint32_t u32;
typedef size_t   usize;

#define MEGABYTES(n) ((n) * 1024LL * 1024LL)

typedef struct {
    u8* base;
    usize size;
    usize used;
    usize temp_count;
    u32 id;
} MemoryArena;

// Just test the basic allocation
int main() {
    printf("Test 1: Memory allocation\n");
    
    MemoryArena arena;
    arena.size = MEGABYTES(16);
    arena.base = (u8*)malloc(arena.size);
    arena.used = 0;
    
    if (!arena.base) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }
    
    printf("Memory allocated successfully: %zu bytes at %p\n", arena.size, arena.base);
    
    printf("Test 2: Simulating ThreadPool allocation\n");
    
    // Simulate what thread_pool_create does
    size_t pool_size = 1024 * 1024;  // Approximate size
    void* pool = arena.base + arena.used;
    arena.used += pool_size;
    
    printf("Pool would be at: %p\n", pool);
    
    free(arena.base);
    printf("All tests passed!\n");
    
    return 0;
}