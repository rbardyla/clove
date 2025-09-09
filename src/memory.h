#ifndef HANDMADE_MEMORY_H
#define HANDMADE_MEMORY_H

#include "handmade.h"

/*
    Memory Arena System
    
    Zero-allocation philosophy:
    - Pre-allocate all memory at startup
    - Use arena/stack allocators for transient allocations
    - Fixed-size pools for persistent allocations
    - No malloc/free in hot paths
    
    Cache-aware design:
    - Align allocations to cache lines
    - Keep hot data together
    - Minimize pointer chasing
*/

// Memory arena for linear/stack allocation
typedef struct memory_arena
{
    memory_index Size;
    u8 *Base;
    memory_index Used;
    i32 TempCount;
} memory_arena;

typedef struct temporary_memory
{
    memory_arena *Arena;
    memory_index Used;
} temporary_memory;

// Initialize arena from a memory block
inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (u8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

// Push allocation with alignment
#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)

inline void *
PushSize_(memory_arena *Arena, memory_index SizeInit)
{
    // PERFORMANCE: Hot path - called for every allocation
    // CACHE: Sequential access pattern, good for prefetching
    memory_index Size = AlignPow2(SizeInit, 8);  // 8-byte alignment minimum
    
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    
    return Result;
}

// Forward declaration
inline memory_index GetAlignmentOffset(memory_arena *Arena, memory_index Alignment);

// Aligned push for SIMD/cache optimization
inline void *
PushSizeAligned(memory_arena *Arena, memory_index Size, u32 Alignment)
{
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    Size += AlignmentOffset;
    
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;
    
    Assert(((umm)Result & (Alignment - 1)) == 0);
    return Result;
}

inline memory_index
GetAlignmentOffset(memory_arena *Arena, memory_index Alignment)
{
    memory_index AlignmentOffset = 0;
    
    memory_index ResultPointer = (memory_index)(Arena->Base + Arena->Used);
    memory_index AlignmentMask = Alignment - 1;
    if(ResultPointer & AlignmentMask)
    {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }
    
    return AlignmentOffset;
}

// Temporary memory for scoped allocations
inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;
    
    Result.Arena = Arena;
    Result.Used = Arena->Used;
    
    ++Arena->TempCount;
    
    return Result;
}

inline void
EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

inline void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

// Zero memory utilities
inline void
ZeroSize(memory_index Size, void *Ptr)
{
    // PERFORMANCE: Use SIMD for large clears
    // TODO: Implement SIMD version for sizes > 256 bytes
    u8 *Byte = (u8 *)Ptr;
    while(Size--)
    {
        *Byte++ = 0;
    }
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Pointer) ZeroSize((Count)*sizeof((Pointer)[0]), Pointer)

// Copy memory
inline void
Copy(memory_index Size, void *SourceInit, void *DestInit)
{
    // PERFORMANCE: Use SIMD for large copies
    // TODO: Implement SIMD version for sizes > 256 bytes
    u8 *Source = (u8 *)SourceInit;
    u8 *Dest = (u8 *)DestInit;
    while(Size--) {*Dest++ = *Source++;}
}

// Sub-arena creation
inline void
SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size)
{
    Result->Size = Size;
    Result->Base = (u8 *)PushSize_(Arena, Size);
    Result->Used = 0;
    Result->TempCount = 0;
}

// Bootstrap allocation for initial setup
inline memory_index
GetArenaSize(memory_arena *Arena)
{
    memory_index Result = Arena->Used;
    return Result;
}

// Memory statistics for profiling
typedef struct memory_stats
{
    u64 TotalAllocated;
    u64 TotalFreed;
    u64 CurrentUsed;
    u64 PeakUsed;
    u32 AllocationCount;
    u32 FreeCount;
    
    // Cache statistics
    u64 CacheLineAlignedAllocs;
    u64 UnalignedAllocs;
} memory_stats;

extern memory_stats GlobalMemoryStats;

inline void
RecordAllocation(memory_index Size)
{
    GlobalMemoryStats.TotalAllocated += Size;
    GlobalMemoryStats.CurrentUsed += Size;
    if(GlobalMemoryStats.CurrentUsed > GlobalMemoryStats.PeakUsed)
    {
        GlobalMemoryStats.PeakUsed = GlobalMemoryStats.CurrentUsed;
    }
    GlobalMemoryStats.AllocationCount++;
}


// Fixed-size pool allocator for neural network weights
typedef struct memory_pool
{
    memory_index BlockSize;
    u32 BlockCount;
    u32 UsedCount;
    u8 *Memory;
    u32 *FreeList;  // Stack of free indices
    u32 FreeCount;
} memory_pool;

inline void
InitializePool(memory_pool *Pool, memory_arena *Arena, memory_index BlockSize, u32 BlockCount)
{
    Pool->BlockSize = AlignPow2(BlockSize, CACHE_LINE_SIZE);  // Cache-align blocks
    Pool->BlockCount = BlockCount;
    Pool->UsedCount = 0;
    Pool->Memory = (u8 *)PushSizeAligned(Arena, Pool->BlockSize * BlockCount, CACHE_LINE_SIZE);
    Pool->FreeList = (u32 *)PushArray(Arena, BlockCount, u32);
    Pool->FreeCount = 0;
    
    // Initialize free list
    for(u32 i = 0; i < BlockCount; ++i)
    {
        Pool->FreeList[i] = i;
    }
    Pool->FreeCount = BlockCount;
}

inline void *
PoolAlloc(memory_pool *Pool)
{
    // PERFORMANCE: O(1) allocation
    void *Result = 0;
    
    if(Pool->FreeCount > 0)
    {
        u32 BlockIndex = Pool->FreeList[--Pool->FreeCount];
        Result = Pool->Memory + (BlockIndex * Pool->BlockSize);
        Pool->UsedCount++;
        
        RecordAllocation(Pool->BlockSize);
    }
    
    return Result;
}

inline void
PoolFree(memory_pool *Pool, void *Block)
{
    // PERFORMANCE: O(1) deallocation
    if(Block)
    {
        umm BlockOffset = (u8 *)Block - Pool->Memory;
        Assert(BlockOffset % Pool->BlockSize == 0);
        u32 BlockIndex = (u32)(BlockOffset / Pool->BlockSize);
        Assert(BlockIndex < Pool->BlockCount);
        
        Pool->FreeList[Pool->FreeCount++] = BlockIndex;
        Pool->UsedCount--;
        
        GlobalMemoryStats.CurrentUsed -= Pool->BlockSize;
        GlobalMemoryStats.TotalFreed += Pool->BlockSize;
        GlobalMemoryStats.FreeCount++;
    }
}

inline void
ResetMemoryPool(memory_pool *Pool)
{
    // Reset pool to initial state - all blocks become free
    Pool->FreeCount = Pool->BlockCount;
    Pool->UsedCount = 0;
    
    // Rebuild free list
    for(u32 i = 0; i < Pool->BlockCount; ++i)
    {
        Pool->FreeList[i] = i;
    }
    
    GlobalMemoryStats.CurrentUsed -= Pool->UsedCount * Pool->BlockSize;
}

inline memory_arena *
PushSubArena(memory_arena *Arena, memory_index Size)
{
    memory_arena *Result = PushStruct(Arena, memory_arena);
    SubArena(Result, Arena, Size);
    return Result;
}

#endif // HANDMADE_MEMORY_H