#ifndef HANDMADE_H
#define HANDMADE_H

/*
    Handmade Neural Engine
    Core type definitions and platform layer
    
    Philosophy:
    - Zero external dependencies
    - Control every byte
    - Measure everything
    - Cache-aware data structures
*/

#include <stdint.h>
#include <stddef.h>

// Basic type definitions
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;
typedef i32      b32;  // Boolean as 32-bit for performance

// Size types
typedef size_t    memory_index;
typedef uintptr_t umm;  // size of a pointer-sized unsigned integer
typedef intptr_t  imm;  // size of a pointer-sized signed integer

// Utility macros
#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265358979323846f
#define Tau32 (2.0f * Pi32)

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// Compiler-specific directives
#ifdef __GNUC__
    #define HANDMADE_INLINE __attribute__((always_inline)) inline
    #define HANDMADE_NO_INLINE __attribute__((noinline))
    #define CACHE_LINE_SIZE 64
    #define CACHE_ALIGN __attribute__((aligned(CACHE_LINE_SIZE)))
#else
    #define HANDMADE_INLINE inline
    #define HANDMADE_NO_INLINE
    #define CACHE_LINE_SIZE 64
    #define CACHE_ALIGN
#endif

// Assertions
#if HANDMADE_DEBUG
    #define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
    #define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

// Intrinsics
#if defined(__x86_64__) || defined(_M_X64)
    #include <x86intrin.h>
    #define HANDMADE_RDTSC 1
    
    inline u64
    ReadCPUTimer(void)
    {
        return __rdtsc();
    }
#else
    #define HANDMADE_RDTSC 0
    inline u64
    ReadCPUTimer(void)
    {
        // TODO(casey): Implement for other architectures
        return 0;
    }
#endif

// Memory alignment
#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)
#define AlignCacheLine(Value) AlignPow2(Value, CACHE_LINE_SIZE)

// Min/Max without branching
#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

// Platform layer interface
typedef struct platform_memory
{
    b32 IsInitialized;
    
    u64 PermanentStorageSize;
    void *PermanentStorage;  // Required to be zeroed at startup
    
    u64 TransientStorageSize;
    void *TransientStorage;   // Required to be zeroed at startup
    
    // For hot reload
    b32 ExecutableReloaded;
} platform_memory;

typedef struct platform_file_handle
{
    b32 NoErrors;
    void *Platform;  // Platform-specific handle
} platform_file_handle;

typedef struct platform_file_result
{
    u32 ContentsSize;
    void *Contents;
} platform_file_result;

// Debug Services
#if HANDMADE_DEBUG
typedef struct debug_read_file_result
{
    u32 ContentsSize;
    void *Contents;
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(char *Filename, u32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif

// Input handling
typedef struct button_state
{
    i32 HalfTransitionCount;
    b32 EndedDown;
} button_state;

typedef struct controller_input
{
    b32 IsConnected;
    b32 IsAnalog;
    
    f32 StickAverageX;
    f32 StickAverageY;
    
    union
    {
        button_state Buttons[12];
        struct
        {
            button_state MoveUp;
            button_state MoveDown;
            button_state MoveLeft;
            button_state MoveRight;
            
            button_state ActionUp;
            button_state ActionDown;
            button_state ActionLeft;
            button_state ActionRight;
            
            button_state LeftShoulder;
            button_state RightShoulder;
            
            button_state Back;
            button_state Start;
        };
    };
} controller_input;

typedef struct game_input
{
    f32 dtForFrame;
    
    button_state MouseButtons[5];
    i32 MouseX, MouseY, MouseZ;
    
    controller_input Controllers[5];  // [0] is keyboard, [1-4] are gamepads
} game_input;

// Timing
typedef struct game_clock
{
    f32 SecondsElapsed;
} game_clock;

// Thread context for multithreading
typedef struct thread_context
{
    int Placeholder;
} thread_context;

// Graphics back buffer interface
typedef struct game_offscreen_buffer
{
    void *Memory;
    i32 Width;
    i32 Height;
    i32 Pitch;
    i32 BytesPerPixel;
} game_offscreen_buffer;

// Main update function signature
#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *Thread, platform_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer, game_clock *Clock)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// Basic drawing functions
inline void
DrawPixel(game_offscreen_buffer *Buffer, i32 X, i32 Y, u32 Color)
{
    if(X >= 0 && X < Buffer->Width && Y >= 0 && Y < Buffer->Height)
    {
        u32 *Pixel = (u32 *)((u8 *)Buffer->Memory + Y * Buffer->Pitch + X * Buffer->BytesPerPixel);
        *Pixel = Color;
    }
}

inline void
ClearBuffer(game_offscreen_buffer *Buffer, u32 Color)
{
    // PERFORMANCE: Clear entire buffer in 32-bit chunks
    u32 *Pixels = (u32 *)Buffer->Memory;
    i32 PixelCount = Buffer->Width * Buffer->Height;
    
    for(i32 i = 0; i < PixelCount; ++i)
    {
        *Pixels++ = Color;
    }
}

inline void
DrawRectangle(game_offscreen_buffer *Buffer, i32 X, i32 Y, i32 Width, i32 Height, u32 Color)
{
    // PERFORMANCE: Clamp to buffer bounds to avoid branching in inner loop
    i32 MinX = (X < 0) ? 0 : X;
    i32 MinY = (Y < 0) ? 0 : Y;
    i32 MaxX = (X + Width > Buffer->Width) ? Buffer->Width : X + Width;
    i32 MaxY = (Y + Height > Buffer->Height) ? Buffer->Height : Y + Height;
    
    for(i32 Row = MinY; Row < MaxY; ++Row)
    {
        u32 *Pixel = (u32 *)((u8 *)Buffer->Memory + Row * Buffer->Pitch + MinX * Buffer->BytesPerPixel);
        for(i32 Col = MinX; Col < MaxX; ++Col)
        {
            *Pixel++ = Color;
        }
    }
}

// Color utility functions
#define RGB(r, g, b) ((u32)(((r) << 16) | ((g) << 8) | (b)))
#define RGBA(r, g, b, a) ((u32)(((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

// Common colors
#define COLOR_BLACK     RGB(0, 0, 0)
#define COLOR_WHITE     RGB(255, 255, 255)
#define COLOR_RED       RGB(255, 0, 0)
#define COLOR_GREEN     RGB(0, 255, 0)
#define COLOR_BLUE      RGB(0, 0, 255)
#define COLOR_YELLOW    RGB(255, 255, 0)
#define COLOR_MAGENTA   RGB(255, 0, 255)
#define COLOR_CYAN      RGB(0, 255, 255)
#define COLOR_GRAY      RGB(128, 128, 128)
#define COLOR_DARK_GRAY RGB(64, 64, 64)

#endif // HANDMADE_H