#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include "handmade.h"
#include "memory.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
    Linux Platform Layer
    
    Responsibilities:
    - Window creation and event handling (X11)
    - Memory allocation from OS
    - File I/O
    - Timing
    - Threading (future)
    - Hot reload support
*/

// Forward declarations
GAME_UPDATE_AND_RENDER(GameUpdateAndRender);

// Software rendering back buffer
typedef struct linux_offscreen_buffer
{
    XImage *Image;
    void *Memory;
    i32 Width;
    i32 Height;
    i32 Pitch;
    i32 BytesPerPixel;
} linux_offscreen_buffer;

typedef struct linux_state
{
    Display *Display;
    Window Window;
    GC GraphicsContext;
    
    linux_offscreen_buffer BackBuffer;
    i32 WindowWidth;
    i32 WindowHeight;
    
    b32 Running;
    
    void *GameMemoryBlock;
    u64 TotalSize;
    
    // For hot reload
    void *GameCodeDLL;
    ino_t GameCodeDLLFileID;
    game_update_and_render *UpdateAndRender;
    
    // Timing
    struct timespec LastCounter;
    f32 TargetSecondsPerFrame;
} linux_state;

global_variable linux_state LinuxState;

// Get wall clock time in seconds
internal f32
LinuxGetSecondsElapsed(struct timespec Start, struct timespec End)
{
    f32 Result = ((f32)(End.tv_sec - Start.tv_sec) +
                  (f32)(End.tv_nsec - Start.tv_nsec) * 1e-9f);
    return Result;
}

internal struct timespec
LinuxGetWallClock(void)
{
    struct timespec Result;
    clock_gettime(CLOCK_MONOTONIC, &Result);
    return Result;
}

// Memory allocation from OS
internal void *
LinuxAllocateMemory(memory_index Size)
{
    // PERFORMANCE: Use huge pages for large allocations
    void *Result = mmap(0, Size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
    
    if(Result == MAP_FAILED)
    {
        Result = 0;
    }
    
    return Result;
}

internal void
LinuxFreeMemory(void *Memory, memory_index Size)
{
    if(Memory)
    {
        munmap(Memory, Size);
    }
}

// File I/O
#if HANDMADE_DEBUG
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(Memory)
    {
        free(Memory);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};
    
    int FileHandle = open(Filename, O_RDONLY);
    if(FileHandle >= 0)
    {
        struct stat FileStat;
        if(fstat(FileHandle, &FileStat) == 0)
        {
            Result.ContentsSize = FileStat.st_size;
            Result.Contents = malloc(Result.ContentsSize);
            
            if(Result.Contents)
            {
                memory_index BytesRead = read(FileHandle, Result.Contents, Result.ContentsSize);
                if(BytesRead != Result.ContentsSize)
                {
                    free(Result.Contents);
                    Result.Contents = 0;
                }
            }
        }
        close(FileHandle);
    }
    
    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    b32 Result = 0;
    
    int FileHandle = open(Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(FileHandle >= 0)
    {
        memory_index BytesWritten = write(FileHandle, Memory, MemorySize);
        Result = (BytesWritten == MemorySize);
        close(FileHandle);
    }
    
    return Result;
}
#endif

// Hot reload support
internal void
LinuxLoadGameCode(char *SourceDLLName)
{
    struct stat FileStat;
    LinuxState.GameCodeDLLFileID = 0;
    
    if(stat(SourceDLLName, &FileStat) == 0)
    {
        LinuxState.GameCodeDLLFileID = FileStat.st_ino;
    }
    
    if(LinuxState.GameCodeDLL)
    {
        dlclose(LinuxState.GameCodeDLL);
        LinuxState.GameCodeDLL = 0;
    }
    
    LinuxState.GameCodeDLL = dlopen(SourceDLLName, RTLD_NOW);
    if(LinuxState.GameCodeDLL)
    {
        LinuxState.UpdateAndRender = (game_update_and_render *)
            dlsym(LinuxState.GameCodeDLL, "GameUpdateAndRender");
    }
    
    if(!LinuxState.UpdateAndRender)
    {
        LinuxState.UpdateAndRender = 0;
    }
}

internal b32
LinuxShouldReloadGameCode(char *SourceDLLName)
{
    struct stat FileStat;
    b32 Result = 0;
    
    if(stat(SourceDLLName, &FileStat) == 0)
    {
        if(FileStat.st_ino != LinuxState.GameCodeDLLFileID)
        {
            Result = 1;
        }
    }
    
    return Result;
}

// Input processing
internal void
LinuxProcessKeyPress(KeySym Key, b32 IsDown, game_input *Input)
{
    controller_input *KeyboardController = &Input->Controllers[0];
    
    if(IsDown != KeyboardController->MoveUp.EndedDown)
    {
        if(Key == XK_w || Key == XK_W)
        {
            KeyboardController->MoveUp.EndedDown = IsDown;
            KeyboardController->MoveUp.HalfTransitionCount++;
        }
    }
    
    if(IsDown != KeyboardController->MoveDown.EndedDown)
    {
        if(Key == XK_s || Key == XK_S)
        {
            KeyboardController->MoveDown.EndedDown = IsDown;
            KeyboardController->MoveDown.HalfTransitionCount++;
        }
    }
    
    if(IsDown != KeyboardController->MoveLeft.EndedDown)
    {
        if(Key == XK_a || Key == XK_A)
        {
            KeyboardController->MoveLeft.EndedDown = IsDown;
            KeyboardController->MoveLeft.HalfTransitionCount++;
        }
    }
    
    if(IsDown != KeyboardController->MoveRight.EndedDown)
    {
        if(Key == XK_d || Key == XK_D)
        {
            KeyboardController->MoveRight.EndedDown = IsDown;
            KeyboardController->MoveRight.HalfTransitionCount++;
        }
    }
    
    // Action buttons
    if(IsDown != KeyboardController->ActionUp.EndedDown)
    {
        if(Key == XK_Up)
        {
            KeyboardController->ActionUp.EndedDown = IsDown;
            KeyboardController->ActionUp.HalfTransitionCount++;
        }
    }
    
    if(IsDown != KeyboardController->ActionDown.EndedDown)
    {
        if(Key == XK_Down)
        {
            KeyboardController->ActionDown.EndedDown = IsDown;
            KeyboardController->ActionDown.HalfTransitionCount++;
        }
    }
    
    if(IsDown != KeyboardController->ActionLeft.EndedDown)
    {
        if(Key == XK_Left)
        {
            KeyboardController->ActionLeft.EndedDown = IsDown;
            KeyboardController->ActionLeft.HalfTransitionCount++;
        }
    }
    
    if(IsDown != KeyboardController->ActionRight.EndedDown)
    {
        if(Key == XK_Right)
        {
            KeyboardController->ActionRight.EndedDown = IsDown;
            KeyboardController->ActionRight.HalfTransitionCount++;
        }
    }
    
    if(Key == XK_Escape && IsDown)
    {
        LinuxState.Running = 0;
    }
}

// Software rendering functions
internal void
LinuxResizeOffscreenBuffer(linux_offscreen_buffer *Buffer, i32 Width, i32 Height)
{
    // PERFORMANCE: Pixel format is 32-bit ARGB for fastest blitting
    // MEMORY: Buffer is allocated once and reused
    
    if(Buffer->Memory)
    {
        XDestroyImage(Buffer->Image);
        // XDestroyImage frees the memory we allocated
    }
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
    
    // Allocate pixel buffer
    i32 BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
    Buffer->Memory = malloc(BitmapMemorySize);
    
    // Create XImage
    Buffer->Image = XCreateImage(LinuxState.Display,
                                DefaultVisual(LinuxState.Display, DefaultScreen(LinuxState.Display)),
                                24,  // depth
                                ZPixmap,  // format
                                0,  // offset
                                (char *)Buffer->Memory,  // data
                                Buffer->Width,
                                Buffer->Height,
                                32,  // bitmap_pad
                                Buffer->Pitch);  // bytes_per_line
}

internal void
LinuxDisplayBufferInWindow(linux_offscreen_buffer *Buffer)
{
    // PERFORMANCE: Direct memory copy to X server - no additional processing
    XPutImage(LinuxState.Display, LinuxState.Window, LinuxState.GraphicsContext,
              Buffer->Image,
              0, 0,  // src x, y
              0, 0,  // dest x, y
              Buffer->Width, Buffer->Height);
    XFlush(LinuxState.Display);
}

// X11 window creation
internal void
LinuxCreateWindow(i32 Width, i32 Height)
{
    LinuxState.Display = XOpenDisplay(NULL);
    if(!LinuxState.Display)
    {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }
    
    int Screen = DefaultScreen(LinuxState.Display);
    Window RootWindow = RootWindow(LinuxState.Display, Screen);
    
    XSetWindowAttributes WindowAttributes = {};
    WindowAttributes.background_pixel = BlackPixel(LinuxState.Display, Screen);
    WindowAttributes.border_pixel = BlackPixel(LinuxState.Display, Screen);
    WindowAttributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                                  ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                                  StructureNotifyMask;
    
    LinuxState.Window = XCreateWindow(LinuxState.Display, RootWindow,
                                      0, 0, Width, Height, 0,
                                      CopyFromParent, InputOutput,
                                      CopyFromParent,
                                      CWBackPixel | CWBorderPixel | CWEventMask,
                                      &WindowAttributes);
    
    XStoreName(LinuxState.Display, LinuxState.Window, "Handmade Neural Engine");
    
    // Create graphics context
    LinuxState.GraphicsContext = XCreateGC(LinuxState.Display, LinuxState.Window, 0, 0);
    
    // Store window dimensions
    LinuxState.WindowWidth = Width;
    LinuxState.WindowHeight = Height;
    
    // Create back buffer
    LinuxResizeOffscreenBuffer(&LinuxState.BackBuffer, Width, Height);
    
    // Set WM_DELETE_WINDOW protocol
    Atom WMDeleteWindow = XInternAtom(LinuxState.Display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(LinuxState.Display, LinuxState.Window, &WMDeleteWindow, 1);
    
    XMapWindow(LinuxState.Display, LinuxState.Window);
    XFlush(LinuxState.Display);
}

// Main platform entry point
int
LinuxMain(int ArgumentCount, char **Arguments)
{
    // Window dimensions
    i32 WindowWidth = 1280;
    i32 WindowHeight = 720;
    
    LinuxCreateWindow(WindowWidth, WindowHeight);
    
    // Allocate game memory
    platform_memory Memory = {};
    Memory.PermanentStorageSize = Megabytes(256);
    Memory.TransientStorageSize = Gigabytes(1);
    
    LinuxState.TotalSize = Memory.PermanentStorageSize + Memory.TransientStorageSize;
    LinuxState.GameMemoryBlock = LinuxAllocateMemory(LinuxState.TotalSize);
    
    if(!LinuxState.GameMemoryBlock)
    {
        fprintf(stderr, "Failed to allocate game memory\n");
        return 1;
    }
    
    Memory.PermanentStorage = LinuxState.GameMemoryBlock;
    Memory.TransientStorage = ((u8 *)Memory.PermanentStorage + Memory.PermanentStorageSize);
    
    // Initialize memory to zero
    memset(Memory.PermanentStorage, 0, Memory.PermanentStorageSize);
    memset(Memory.TransientStorage, 0, Memory.TransientStorageSize);
    
    // Target frame rate
    i32 GameUpdateHz = 60;
    LinuxState.TargetSecondsPerFrame = 1.0f / (f32)GameUpdateHz;
    
    // Input state
    game_input Input[2] = {};
    game_input *NewInput = &Input[0];
    game_input *OldInput = &Input[1];
    
    // Enable keyboard controller
    NewInput->Controllers[0].IsConnected = 1;
    
    // Timing
    struct timespec LastCounter = LinuxGetWallClock();
    u64 LastCycleCount = ReadCPUTimer();
    
    LinuxState.Running = 1;
    
    while(LinuxState.Running)
    {
        // Process X11 events
        while(XPending(LinuxState.Display))
        {
            XEvent Event;
            XNextEvent(LinuxState.Display, &Event);
            
            switch(Event.type)
            {
                case KeyPress:
                case KeyRelease:
                {
                    KeySym Key = XLookupKeysym(&Event.xkey, 0);
                    b32 IsDown = (Event.type == KeyPress);
                    LinuxProcessKeyPress(Key, IsDown, NewInput);
                } break;
                
                case ButtonPress:
                case ButtonRelease:
                {
                    b32 IsDown = (Event.type == ButtonPress);
                    if(Event.xbutton.button == Button1)
                    {
                        NewInput->MouseButtons[0].EndedDown = IsDown;
                        NewInput->MouseButtons[0].HalfTransitionCount++;
                    }
                    else if(Event.xbutton.button == Button2)
                    {
                        NewInput->MouseButtons[2].EndedDown = IsDown;
                        NewInput->MouseButtons[2].HalfTransitionCount++;
                    }
                    else if(Event.xbutton.button == Button3)
                    {
                        NewInput->MouseButtons[1].EndedDown = IsDown;
                        NewInput->MouseButtons[1].HalfTransitionCount++;
                    }
                } break;
                
                case MotionNotify:
                {
                    NewInput->MouseX = Event.xmotion.x;
                    NewInput->MouseY = Event.xmotion.y;
                } break;
                
                case ClientMessage:
                {
                    Atom WMDeleteWindow = XInternAtom(LinuxState.Display, "WM_DELETE_WINDOW", False);
                    if((Atom)Event.xclient.data.l[0] == WMDeleteWindow)
                    {
                        LinuxState.Running = 0;
                    }
                } break;
            }
        }
        
        // Calculate frame timing
        struct timespec WorkCounter = LinuxGetWallClock();
        f32 WorkSecondsElapsed = LinuxGetSecondsElapsed(LastCounter, WorkCounter);
        
        f32 SecondsElapsedForFrame = WorkSecondsElapsed;
        if(SecondsElapsedForFrame < LinuxState.TargetSecondsPerFrame)
        {
            useconds_t SleepMS = (useconds_t)(1000000.0f * (LinuxState.TargetSecondsPerFrame - SecondsElapsedForFrame));
            if(SleepMS > 0)
            {
                usleep(SleepMS);
            }
            
            SecondsElapsedForFrame = LinuxGetSecondsElapsed(LastCounter, LinuxGetWallClock());
            
            while(SecondsElapsedForFrame < LinuxState.TargetSecondsPerFrame)
            {
                SecondsElapsedForFrame = LinuxGetSecondsElapsed(LastCounter, LinuxGetWallClock());
            }
        }
        
        struct timespec EndCounter = LinuxGetWallClock();
        f32 MSPerFrame = 1000.0f * LinuxGetSecondsElapsed(LastCounter, EndCounter);
        LastCounter = EndCounter;
        
        // Update input timing
        NewInput->dtForFrame = LinuxState.TargetSecondsPerFrame;
        
        // Game update and render
        thread_context Thread = {};
        game_clock Clock = {};
        Clock.SecondsElapsed = SecondsElapsedForFrame;
        
        // Prepare offscreen buffer for game
        game_offscreen_buffer GameBuffer = {};
        GameBuffer.Memory = LinuxState.BackBuffer.Memory;
        GameBuffer.Width = LinuxState.BackBuffer.Width;
        GameBuffer.Height = LinuxState.BackBuffer.Height;
        GameBuffer.Pitch = LinuxState.BackBuffer.Pitch;
        GameBuffer.BytesPerPixel = LinuxState.BackBuffer.BytesPerPixel;
        
        // Call game update and render
        GameUpdateAndRender(&Thread, &Memory, NewInput, &GameBuffer, &Clock);
        
        // Display the back buffer
        LinuxDisplayBufferInWindow(&LinuxState.BackBuffer);
        
        // Performance counters
        u64 EndCycleCount = ReadCPUTimer();
        u64 CyclesElapsed = EndCycleCount - LastCycleCount;
        LastCycleCount = EndCycleCount;
        
        f64 MCPF = ((f64)CyclesElapsed / (1000.0f * 1000.0f));
        
#if HANDMADE_DEBUG
        printf("%.02fms/f, %.02fmc/f\n", MSPerFrame, MCPF);
#endif
        
        // Swap input buffers
        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
    }
    
    // Cleanup
    if(LinuxState.BackBuffer.Memory)
    {
        XDestroyImage(LinuxState.BackBuffer.Image);
    }
    XDestroyWindow(LinuxState.Display, LinuxState.Window);
    XCloseDisplay(LinuxState.Display);
    
    LinuxFreeMemory(LinuxState.GameMemoryBlock, LinuxState.TotalSize);
    
    return 0;
}

int
main(int argc, char **argv)
{
    return LinuxMain(argc, argv);
}