#define _GNU_SOURCE
#include "handmade_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// Linux platform specific data
typedef struct {
    Display* display;
    Window window;
    GLXContext gl_context;
    Atom wm_delete_window;
    Atom clipboard;
    Atom targets;
    Atom utf8_string;
    XIM input_method;
    XIC input_context;
    Cursor hidden_cursor;
    
    int inotify_fd;
    int watch_fd;
    
    bool running;
    bool cursor_visible;
    bool fullscreen;
    
    struct timespec start_time;
} LinuxPlatformData;

// Work queue implementation
typedef struct {
    PlatformWork* entries;
    volatile u32 read_index;
    volatile u32 write_index;
    volatile u32 completion_count;
    volatile u32 completion_goal;
    pthread_t* threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    u32 thread_count;
    bool running;
} LinuxWorkQueue;

// Global state
static PlatformState g_platform = {0};
static PlatformAPI g_platform_api = {0};
static LinuxPlatformData g_linux_data = {0};

PlatformAPI* Platform = &g_platform_api;
PlatformState* GlobalPlatform = &g_platform;

// Forward declarations
static void InitializePlatformAPI(void);
static bool InitializeWindow(u32 width, u32 height, const char* title);
static bool InitializeOpenGL(void);
static void ProcessEvents(void);
static HandmadeKeyCode TranslateKeycode(KeySym keysym);
static void UpdateInput(void);
static void* WorkerThreadProc(void* param);

// Editor functions (defined in editor_main.c)
void GameInit(PlatformState* platform);
void GameUpdate(PlatformState* platform, f32 dt);
void GameRender(PlatformState* platform);
void GameShutdown(PlatformState* platform);
void GameOnReload(PlatformState* platform);

// Memory functions
static void* LinuxAllocateMemory(usize size) {
    void* result = mmap(0, size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (result == MAP_FAILED) ? NULL : result;
}

static void LinuxFreeMemory(void* memory) {
    if (memory) {
        // Need to know the size for munmap, should track this
        // For now, we'll use the arena system primarily
    }
}

static void LinuxCopyMemory(void* dest, const void* src, usize size) {
    memcpy(dest, src, size);
}

static void LinuxZeroMemory(void* memory, usize size) {
    memset(memory, 0, size);
}

// File I/O functions
static PlatformFile LinuxReadFile(const char* path, MemoryArena* arena) {
    PlatformFile result = {0};
    
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        struct stat file_stat;
        if (fstat(fd, &file_stat) == 0) {
            result.size = file_stat.st_size;
            result.data = PushSize(arena, result.size + 1);
            
            if (result.data) {
                ssize_t bytes_read = read(fd, result.data, result.size);
                if (bytes_read == (ssize_t)result.size) {
                    ((char*)result.data)[result.size] = 0;
                } else {
                    result.data = NULL;
                    result.size = 0;
                }
            }
        }
        close(fd);
    }
    
    return result;
}

static bool LinuxWriteFile(const char* path, void* data, usize size) {
    bool result = false;
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t bytes_written = write(fd, data, size);
        result = (bytes_written == (ssize_t)size);
        close(fd);
    }
    
    return result;
}

static bool LinuxFileExists(const char* path) {
    struct stat file_stat;
    return (stat(path, &file_stat) == 0);
}

static u64 LinuxGetFileTime(const char* path) {
    struct stat file_stat;
    if (stat(path, &file_stat) == 0) {
        return file_stat.st_mtime;
    }
    return 0;
}

// Timing functions
static u64 LinuxGetPerformanceCounter(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static f64 LinuxGetSecondsElapsed(u64 start, u64 end) {
    return (f64)(end - start) / 1000000000.0;
}

static void LinuxSleep(u32 milliseconds) {
    usleep(milliseconds * 1000);
}

// Debug functions
static void LinuxDebugPrint(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}

static void LinuxDebugBreak(void) {
    __builtin_trap();
}

// Dialog functions (using zenity if available)
static char* LinuxOpenFileDialog(const char* filter, MemoryArena* arena) {
    char command[1024];
    snprintf(command, sizeof(command), "zenity --file-selection 2>/dev/null");
    
    FILE* pipe = popen(command, "r");
    if (pipe) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') {
                buffer[len-1] = 0;
            }
            char* result = (char*)PushSize(arena, len);
            strcpy(result, buffer);
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return NULL;
}

static void LinuxShowErrorBox(const char* title, const char* message) {
    char command[2048];
    snprintf(command, sizeof(command), 
             "zenity --error --title='%s' --text='%s' 2>/dev/null",
             title, message);
    system(command);
}

// Key translation  
static HandmadeKeyCode TranslateKeycode(KeySym keysym) {
    switch(keysym) {
        case XK_a: case XK_A: return KEY_A;
        case XK_b: case XK_B: return KEY_B;
        case XK_c: case XK_C: return KEY_C;
        case XK_d: case XK_D: return KEY_D;
        case XK_e: case XK_E: return KEY_E;
        case XK_f: case XK_F: return KEY_F;
        case XK_g: case XK_G: return KEY_G;
        case XK_h: case XK_H: return KEY_H;
        case XK_i: case XK_I: return KEY_I;
        case XK_j: case XK_J: return KEY_J;
        case XK_k: case XK_K: return KEY_K;
        case XK_l: case XK_L: return KEY_L;
        case XK_m: case XK_M: return KEY_M;
        case XK_n: case XK_N: return KEY_N;
        case XK_o: case XK_O: return KEY_O;
        case XK_p: case XK_P: return KEY_P;
        case XK_q: case XK_Q: return KEY_Q;
        case XK_r: case XK_R: return KEY_R;
        case XK_s: case XK_S: return KEY_S;
        case XK_t: case XK_T: return KEY_T;
        case XK_u: case XK_U: return KEY_U;
        case XK_v: case XK_V: return KEY_V;
        case XK_w: case XK_W: return KEY_W;
        case XK_x: case XK_X: return KEY_X;
        case XK_y: case XK_Y: return KEY_Y;
        case XK_z: case XK_Z: return KEY_Z;
        case XK_0: return KEY_0;
        case XK_1: return KEY_1;
        case XK_2: return KEY_2;
        case XK_3: return KEY_3;
        case XK_4: return KEY_4;
        case XK_5: return KEY_5;
        case XK_6: return KEY_6;
        case XK_7: return KEY_7;
        case XK_8: return KEY_8;
        case XK_9: return KEY_9;
        case XK_space: return KEY_SPACE;
        case XK_Return: return KEY_ENTER;
        case XK_Tab: return KEY_TAB;
        case XK_Escape: return KEY_ESCAPE;
        case XK_BackSpace: return KEY_BACKSPACE;
        case XK_Delete: return KEY_DELETE;
        case XK_Up: return KEY_UP;
        case XK_Down: return KEY_DOWN;
        case XK_Left: return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        case XK_F1: return KEY_F1;
        case XK_F2: return KEY_F2;
        case XK_F3: return KEY_F3;
        case XK_F4: return KEY_F4;
        case XK_F5: return KEY_F5;
        case XK_F6: return KEY_F6;
        case XK_F7: return KEY_F7;
        case XK_F8: return KEY_F8;
        case XK_F9: return KEY_F9;
        case XK_F10: return KEY_F10;
        case XK_F11: return KEY_F11;
        case XK_F12: return KEY_F12;
        case XK_Shift_L: case XK_Shift_R: return KEY_SHIFT;
        case XK_Control_L: case XK_Control_R: return KEY_CTRL;
        case XK_Alt_L: case XK_Alt_R: return KEY_ALT;
        case XK_Super_L: case XK_Super_R: return KEY_SUPER;
        default: return KEY_UNKNOWN;
    }
}

// Initialize OpenGL
static bool InitializeOpenGL(void) {
    GLint attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 4,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(g_linux_data.display, 
                                      DefaultScreen(g_linux_data.display), 
                                      attribs);
    if (!vi) {
        printf("Failed to choose visual\n");
        return false;
    }
    
    g_linux_data.gl_context = glXCreateContext(g_linux_data.display, vi, NULL, GL_TRUE);
    if (!g_linux_data.gl_context) {
        printf("Failed to create OpenGL context\n");
        XFree(vi);
        return false;
    }
    
    glXMakeCurrent(g_linux_data.display, g_linux_data.window, g_linux_data.gl_context);
    
    // OpenGL callbacks will be set up later
    
    XFree(vi);
    return true;
}

// Initialize window
static bool InitializeWindow(u32 width, u32 height, const char* title) {
    g_linux_data.display = XOpenDisplay(NULL);
    if (!g_linux_data.display) {
        printf("Failed to open X display\n");
        return false;
    }
    
    int screen = DefaultScreen(g_linux_data.display);
    Window root = RootWindow(g_linux_data.display, screen);
    
    GLint attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(g_linux_data.display, screen, attribs);
    if (!vi) {
        printf("No appropriate visual found\n");
        return false;
    }
    
    Colormap cmap = XCreateColormap(g_linux_data.display, root, vi->visual, AllocNone);
    
    XSetWindowAttributes swa = {0};
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask | FocusChangeMask;
    
    g_linux_data.window = XCreateWindow(g_linux_data.display, root,
                                        0, 0, width, height, 0,
                                        vi->depth, InputOutput, vi->visual,
                                        CWColormap | CWEventMask, &swa);
    
    XMapWindow(g_linux_data.display, g_linux_data.window);
    XStoreName(g_linux_data.display, g_linux_data.window, title);
    
    // Setup window close event
    g_linux_data.wm_delete_window = XInternAtom(g_linux_data.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_linux_data.display, g_linux_data.window, &g_linux_data.wm_delete_window, 1);
    
    // Initialize input method
    g_linux_data.input_method = XOpenIM(g_linux_data.display, NULL, NULL, NULL);
    if (g_linux_data.input_method) {
        g_linux_data.input_context = XCreateIC(g_linux_data.input_method,
                                              XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                              XNClientWindow, g_linux_data.window,
                                              NULL);
    }
    
    g_platform.window.width = width;
    g_platform.window.height = height;
    g_platform.window.dpi_scale = 1.0f;
    
    XFree(vi);
    return true;
}

// Process events
static void ProcessEvents(void) {
    XEvent event;
    
    // Clear frame input states
    for (int i = 0; i < KEY_COUNT; i++) {
        g_platform.input.keys[i].pressed = false;
        g_platform.input.keys[i].released = false;
    }
    for (int i = 0; i < MOUSE_COUNT; i++) {
        g_platform.input.mouse[i].pressed = false;
        g_platform.input.mouse[i].released = false;
    }
    g_platform.input.mouse_wheel = 0;
    g_platform.input.text_length = 0;
    
    while (XPending(g_linux_data.display) > 0) {
        XNextEvent(g_linux_data.display, &event);
        
        switch (event.type) {
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == g_linux_data.wm_delete_window) {
                    g_platform.window.should_close = true;
                }
                break;
                
            case ConfigureNotify:
                if (event.xconfigure.width != (int)g_platform.window.width ||
                    event.xconfigure.height != (int)g_platform.window.height) {
                    g_platform.window.width = event.xconfigure.width;
                    g_platform.window.height = event.xconfigure.height;
                    g_platform.window.resized = true;
                }
                break;
                
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                HandmadeKeyCode key = TranslateKeycode(keysym);
                if (key != KEY_UNKNOWN) {
                    g_platform.input.keys[key].down = true;
                    g_platform.input.keys[key].pressed = true;
                }
                
                // Text input
                if (g_linux_data.input_context) {
                    char buffer[32];
                    Status status;
                    int len = Xutf8LookupString(g_linux_data.input_context, &event.xkey,
                                               buffer, sizeof(buffer)-1, NULL, &status);
                    if (len > 0 && g_platform.input.text_length < 31) {
                        memcpy(g_platform.input.text_input + g_platform.input.text_length,
                               buffer, len);
                        g_platform.input.text_length += len;
                        g_platform.input.text_input[g_platform.input.text_length] = 0;
                    }
                }
                break;
            }
                
            case KeyRelease: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                HandmadeKeyCode key = TranslateKeycode(keysym);
                if (key != KEY_UNKNOWN) {
                    g_platform.input.keys[key].down = false;
                    g_platform.input.keys[key].released = true;
                }
                break;
            }
                
            case ButtonPress:
                if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
                    int button = event.xbutton.button - 1;
                    g_platform.input.mouse[button].down = true;
                    g_platform.input.mouse[button].pressed = true;
                } else if (event.xbutton.button == 4) {
                    g_platform.input.mouse_wheel = 1.0f;
                } else if (event.xbutton.button == 5) {
                    g_platform.input.mouse_wheel = -1.0f;
                }
                break;
                
            case ButtonRelease:
                if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
                    int button = event.xbutton.button - 1;
                    g_platform.input.mouse[button].down = false;
                    g_platform.input.mouse[button].released = true;
                }
                break;
                
            case MotionNotify: {
                f32 new_x = event.xmotion.x;
                f32 new_y = event.xmotion.y;
                g_platform.input.mouse_dx = new_x - g_platform.input.mouse_x;
                g_platform.input.mouse_dy = new_y - g_platform.input.mouse_y;
                g_platform.input.mouse_x = new_x;
                g_platform.input.mouse_y = new_y;
                break;
            }
                
            case FocusIn:
                g_platform.window.focused = true;
                break;
                
            case FocusOut:
                g_platform.window.focused = false;
                break;
        }
    }
}

// Initialize platform API
static void InitializePlatformAPI(void) {
    g_platform_api.AllocateMemory = LinuxAllocateMemory;
    g_platform_api.FreeMemory = LinuxFreeMemory;
    g_platform_api.CopyMemory = LinuxCopyMemory;
    g_platform_api.ZeroMemory = LinuxZeroMemory;
    
    g_platform_api.ReadFile = LinuxReadFile;
    g_platform_api.WriteFile = LinuxWriteFile;
    g_platform_api.FileExists = LinuxFileExists;
    g_platform_api.GetFileTime = LinuxGetFileTime;
    
    g_platform_api.GetPerformanceCounter = LinuxGetPerformanceCounter;
    g_platform_api.GetSecondsElapsed = LinuxGetSecondsElapsed;
    g_platform_api.Sleep = LinuxSleep;
    
    g_platform_api.OpenFileDialog = LinuxOpenFileDialog;
    g_platform_api.ShowErrorBox = LinuxShowErrorBox;
    
    g_platform_api.DebugPrint = LinuxDebugPrint;
    g_platform_api.DebugBreak = LinuxDebugBreak;
}

// Platform main functions
bool PlatformInit(PlatformState* platform, const char* title, u32 width, u32 height) {
    *platform = g_platform;
    platform->platform_data = &g_linux_data;
    
    InitializePlatformAPI();
    
    if (!InitializeWindow(width, height, title)) {
        return false;
    }
    
    if (!InitializeOpenGL()) {
        return false;
    }
    
    // Initialize memory arenas according to the architecture doc
    // Total: ~5GB as specified
    
    // Permanent Arena: 4GB for persistent data
    platform->permanent_memory.size = GIGABYTES(4);
    platform->permanent_memory.memory = LinuxAllocateMemory(platform->permanent_memory.size);
    platform->permanent_memory.initialized = true;
    
    // Frame Arena: 512MB for per-frame allocations
    platform->transient_memory.size = MEGABYTES(512);
    platform->transient_memory.memory = LinuxAllocateMemory(platform->transient_memory.size);
    platform->transient_memory.initialized = true;
    
    platform->permanent_arena.base = (u8*)platform->permanent_memory.memory;
    platform->permanent_arena.size = platform->permanent_memory.size;
    platform->permanent_arena.used = 0;
    platform->permanent_arena.id = 1; // ARENA_PERMANENT
    
    platform->frame_arena.base = (u8*)platform->transient_memory.memory;
    platform->frame_arena.size = platform->transient_memory.size;
    platform->frame_arena.used = 0;
    platform->frame_arena.id = 2; // ARENA_FRAME
    
    // Initialize timing
    clock_gettime(CLOCK_MONOTONIC, &g_linux_data.start_time);
    
    g_linux_data.running = true;
    g_platform = *platform;
    
    return true;
}

void PlatformShutdown(PlatformState* platform) {
    if (g_linux_data.gl_context) {
        glXMakeCurrent(g_linux_data.display, None, NULL);
        glXDestroyContext(g_linux_data.display, g_linux_data.gl_context);
    }
    
    if (g_linux_data.input_context) {
        XDestroyIC(g_linux_data.input_context);
    }
    
    if (g_linux_data.input_method) {
        XCloseIM(g_linux_data.input_method);
    }
    
    if (g_linux_data.window) {
        XDestroyWindow(g_linux_data.display, g_linux_data.window);
    }
    
    if (g_linux_data.display) {
        XCloseDisplay(g_linux_data.display);
    }
}

bool PlatformProcessEvents(PlatformState* platform) {
    g_platform.window.resized = false;
    ProcessEvents();
    *platform = g_platform;
    return !g_platform.window.should_close;
}

void PlatformSwapBuffers(PlatformState* platform) {
    glXSwapBuffers(g_linux_data.display, g_linux_data.window);
}

f64 PlatformGetTime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// Hot reload functions
bool PlatformLoadGameModule(PlatformState* platform, const char* path) {
    if (platform->game_module.handle) {
        dlclose(platform->game_module.handle);
    }
    
    platform->game_module.handle = dlopen(path, RTLD_NOW);
    if (!platform->game_module.handle) {
        printf("Failed to load module: %s\n", dlerror());
        return false;
    }
    
    platform->game_module.Init = (void(*)(PlatformState*))dlsym(platform->game_module.handle, "GameInit");
    platform->game_module.Update = (void(*)(PlatformState*, f32))dlsym(platform->game_module.handle, "GameUpdate");
    platform->game_module.Render = (void(*)(PlatformState*))dlsym(platform->game_module.handle, "GameRender");
    platform->game_module.Shutdown = (void(*)(PlatformState*))dlsym(platform->game_module.handle, "GameShutdown");
    platform->game_module.OnReload = (void(*)(PlatformState*))dlsym(platform->game_module.handle, "GameOnReload");
    
    platform->game_module.valid = true;
    platform->game_module.last_write_time = LinuxGetFileTime(path);
    
    if (platform->game_module.OnReload) {
        platform->game_module.OnReload(platform);
    }
    
    return true;
}

void PlatformUnloadGameModule(PlatformState* platform) {
    if (platform->game_module.handle) {
        if (platform->game_module.Shutdown) {
            platform->game_module.Shutdown(platform);
        }
        dlclose(platform->game_module.handle);
        memset(&platform->game_module, 0, sizeof(platform->game_module));
    }
}

bool PlatformCheckModuleReload(PlatformState* platform, const char* path) {
    u64 write_time = LinuxGetFileTime(path);
    if (write_time > platform->game_module.last_write_time) {
        return PlatformLoadGameModule(platform, path);
    }
    return false;
}

// Main entry point
int main(int argc, char** argv) {
    PlatformState platform = {0};
    
    // Initialize platform
    if (!PlatformInit(&platform, "Handmade Editor", 1920, 1080)) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize editor (direct call since not using hot reload by default)
    GameInit(&platform);
    
    // Main loop
    f64 last_time = PlatformGetTime();
    
    while (PlatformProcessEvents(&platform)) {
        // Clear frame memory at start of frame
        platform.frame_arena.used = 0;
        
        // Calculate delta time
        f64 current_time = PlatformGetTime();
        f32 dt = (f32)(current_time - last_time);
        last_time = current_time;
        
        // Update
        GameUpdate(&platform, dt);
        
        // Render
        GameRender(&platform);
        
        // Swap buffers
        PlatformSwapBuffers(&platform);
    }
    
    // Shutdown
    GameShutdown(&platform);
    PlatformShutdown(&platform);
    
    return 0;
}