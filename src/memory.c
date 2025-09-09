#include "memory.h"

// Define the global memory stats here
memory_stats GlobalMemoryStats;

/*
    Memory System Implementation
    
    This file contains implementations for more complex memory operations
    that shouldn't be inlined. Most operations are in the header for
    zero-overhead inlining.
*/

// SIMD-optimized memory clear for large blocks
internal void
ZeroSizeSIMD(memory_index Size, void *Ptr)
{
    // PERFORMANCE: AVX2 clear - 32 bytes per iteration
    // CACHE: Writes full cache lines (64 bytes every 2 iterations)
    
#if defined(__AVX2__)
    if(Size >= 32)
    {
        __m256i Zero = _mm256_setzero_si256();
        
        // Align to 32-byte boundary
        u8 *Byte = (u8 *)Ptr;
        while(((umm)Byte & 31) && Size)
        {
            *Byte++ = 0;
            --Size;
        }
        
        // Clear 32 bytes at a time
        __m256i *Dest = (__m256i *)Byte;
        while(Size >= 32)
        {
            _mm256_store_si256(Dest++, Zero);
            Size -= 32;
        }
        
        // Clear remaining bytes
        Byte = (u8 *)Dest;
        while(Size--)
        {
            *Byte++ = 0;
        }
    }
    else
#endif
    {
        // Fallback to byte-by-byte clear
        ZeroSize(Size, Ptr);
    }
}

// SIMD-optimized memory copy for large blocks
internal void
CopySIMD(memory_index Size, void *SourceInit, void *DestInit)
{
    // PERFORMANCE: AVX2 copy - 32 bytes per iteration
    // CACHE: Reads and writes full cache lines
    
#if defined(__AVX2__)
    if(Size >= 32)
    {
        u8 *Source = (u8 *)SourceInit;
        u8 *Dest = (u8 *)DestInit;
        
        // Align destination to 32-byte boundary
        while(((umm)Dest & 31) && Size)
        {
            *Dest++ = *Source++;
            --Size;
        }
        
        // Copy 32 bytes at a time
        __m256i *SourceVec = (__m256i *)Source;
        __m256i *DestVec = (__m256i *)Dest;
        while(Size >= 32)
        {
            __m256i Data = _mm256_loadu_si256(SourceVec++);
            _mm256_store_si256(DestVec++, Data);
            Size -= 32;
        }
        
        // Copy remaining bytes
        Source = (u8 *)SourceVec;
        Dest = (u8 *)DestVec;
        while(Size--)
        {
            *Dest++ = *Source++;
        }
    }
    else
#endif
    {
        // Fallback to byte-by-byte copy
        Copy(Size, SourceInit, DestInit);
    }
}

// Ring buffer for debug history
typedef struct debug_memory_block
{
    u64 Timestamp;  // CPU cycles when allocated
    memory_index Size;
    char File[64];
    i32 Line;
    void *Address;
} debug_memory_block;

#define DEBUG_MEMORY_BLOCK_COUNT 8192
global_variable debug_memory_block DebugMemoryBlocks[DEBUG_MEMORY_BLOCK_COUNT];
global_variable u32 DebugMemoryBlockIndex;

internal void
RecordDebugAllocation(void *Address, memory_index Size, char *File, i32 Line)
{
#if HANDMADE_DEBUG
    debug_memory_block *Block = &DebugMemoryBlocks[DebugMemoryBlockIndex];
    Block->Timestamp = ReadCPUTimer();
    Block->Size = Size;
    Block->Address = Address;
    Block->Line = Line;
    
    // Copy filename (truncate if needed)
    i32 i;
    for(i = 0; i < 63 && File[i]; ++i)
    {
        Block->File[i] = File[i];
    }
    Block->File[i] = 0;
    
    DebugMemoryBlockIndex = (DebugMemoryBlockIndex + 1) % DEBUG_MEMORY_BLOCK_COUNT;
#endif
}

// Memory validation for debugging
internal b32
ValidateArena(memory_arena *Arena)
{
    b32 Result = 1;
    
    if(!Arena)
    {
        Result = 0;
    }
    else if(Arena->Used > Arena->Size)
    {
        Result = 0;
    }
    else if(Arena->TempCount < 0)
    {
        Result = 0;
    }
    else if(!Arena->Base && Arena->Size > 0)
    {
        Result = 0;
    }
    
    return Result;
}

// Print memory statistics
internal void
PrintMemoryStats(memory_stats *Stats)
{
#if HANDMADE_DEBUG
    // NOTE: We avoid printf in production code, but for debug output it's acceptable
    f64 MB = 1024.0 * 1024.0;
    
    // This would normally output to a debug console or log file
    // For now, we'll just ensure the function compiles
    (void)Stats;
    (void)MB;
#endif
}

// Memory pattern fill for debugging uninitialized reads
internal void
FillMemoryPattern(void *Memory, memory_index Size, u32 Pattern)
{
#if HANDMADE_DEBUG
    u32 *Dest = (u32 *)Memory;
    memory_index Count = Size / sizeof(u32);
    while(Count--)
    {
        *Dest++ = Pattern;
    }
#endif
}

// Scratch arena for temporary allocations within a function
typedef struct scratch_memory
{
    memory_arena Arena;
    temporary_memory TempMemory;
} scratch_memory;

global_variable scratch_memory GlobalScratchMemory;

internal scratch_memory *
GetScratchMemory(memory_arena *ParentArena, memory_index Size)
{
    if(GlobalScratchMemory.Arena.Size < Size)
    {
        if(GlobalScratchMemory.Arena.Base)
        {
            EndTemporaryMemory(GlobalScratchMemory.TempMemory);
        }
        
        GlobalScratchMemory.TempMemory = BeginTemporaryMemory(ParentArena);
        SubArena(&GlobalScratchMemory.Arena, ParentArena, Size);
    }
    else
    {
        GlobalScratchMemory.Arena.Used = 0;
    }
    
    return &GlobalScratchMemory;
}

// Memory defragmentation for pools (offline operation)
internal void
DefragmentPool(memory_pool *Pool)
{
    // NOTE: This is an expensive operation, only do during loading screens
    // or other non-performance-critical times
    
    // Compact all used blocks to the beginning
    // Update free list with contiguous free blocks at the end
    // This improves cache locality for frequently used blocks
    
    // TODO: Implement when needed for long-running applications
}