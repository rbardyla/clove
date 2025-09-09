#include "handmade_hotreload.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// PERFORMANCE: Platform layer - zero allocations after init
// MEMORY: All memory pre-allocated at startup

#define PERMANENT_STORAGE_SIZE (256 * 1024 * 1024)  // 256MB
#define TRANSIENT_STORAGE_SIZE (128 * 1024 * 1024)  // 128MB
#define DEBUG_STORAGE_SIZE     (16 * 1024 * 1024)   // 16MB

typedef struct {
    Display* display;
    Window window;
    GLXContext gl_context;
    Atom wm_delete_window;
    
    int window_width;
    int window_height;
    bool should_quit;
    
    // Timing
    struct timespec start_time;
    struct timespec last_frame_time;
    uint64_t frame_count;
    
    // Performance counters
    float frame_ms;
    float update_ms;
    float render_ms;
} PlatformState;

// MEMORY: Global platform state
static PlatformState g_platform = {0};

// Platform API implementations
static void* platform_load_file(const char* path, uint64_t* size) {
    // PERFORMANCE: Memory-mapped file I/O
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }
    
    void* data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (data == MAP_FAILED) {
        return NULL;
    }
    
    if (size) {
        *size = st.st_size;
    }
    
    return data;
}

static void platform_free_file(void* data) {
    // Unmap the file
    if (data) {
        // We don't know the size here, but munmap will unmap the whole mapping
        munmap(data, 0);
    }
}

static void platform_debug_print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

static uint64_t platform_get_cycles(void) {
    return read_cpu_timer();
}

static uint64_t platform_get_wall_clock(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Simple work queue (single-threaded for now)
static void platform_queue_work(void (*work_proc)(void*), void* data) {
    // PERFORMANCE: For now, just execute immediately
    // TODO: Implement thread pool
    work_proc(data);
}

static void platform_complete_all_work(void) {
    // No-op for single-threaded version
}

static bool init_x11_and_opengl(int width, int height) {
    // PERFORMANCE: Direct X11/GLX initialization
    g_platform.display = XOpenDisplay(NULL);
    if (!g_platform.display) {
        printf("[PLATFORM] Failed to open X display\n");
        return false;
    }
    
    int screen = DefaultScreen(g_platform.display);
    Window root = RootWindow(g_platform.display, screen);
    
    // Setup OpenGL attributes
    GLint glx_attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_SAMPLE_BUFFERS, 0,
        GLX_SAMPLES, 0,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(g_platform.display, screen, glx_attribs);
    if (!vi) {
        printf("[PLATFORM] No suitable visual found\n");
        return false;
    }
    
    Colormap cmap = XCreateColormap(g_platform.display, root, vi->visual, AllocNone);
    
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask;
    
    g_platform.window = XCreateWindow(
        g_platform.display, root,
        0, 0, width, height,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );
    
    XMapWindow(g_platform.display, g_platform.window);
    XStoreName(g_platform.display, g_platform.window, "Handmade Engine - Hot Reload");
    
    // Setup close button
    g_platform.wm_delete_window = XInternAtom(g_platform.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_platform.display, g_platform.window, &g_platform.wm_delete_window, 1);
    
    // Create OpenGL context
    g_platform.gl_context = glXCreateContext(g_platform.display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_platform.display, g_platform.window, g_platform.gl_context);
    
    // Enable VSync
    typedef int (*PFNGLXSWAPINTERVALMESAPROC)(unsigned int);
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = 
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");
    if (glXSwapIntervalMESA) {
        glXSwapIntervalMESA(1);
    }
    
    g_platform.window_width = width;
    g_platform.window_height = height;
    
    // Setup OpenGL state
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    
    // Simple 2D projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    XFree(vi);
    
    printf("[PLATFORM] X11/OpenGL initialized (%dx%d)\n", width, height);
    return true;
}

static void process_x11_events(GameInput* input) {
    // PERFORMANCE: Process all pending events
    while (XPending(g_platform.display)) {
        XEvent event;
        XNextEvent(g_platform.display, &event);
        
        switch (event.type) {
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == g_platform.wm_delete_window) {
                    g_platform.should_quit = true;
                }
                break;
                
            case ConfigureNotify:
                g_platform.window_width = event.xconfigure.width;
                g_platform.window_height = event.xconfigure.height;
                glViewport(0, 0, g_platform.window_width, g_platform.window_height);
                break;
                
            case KeyPress: {
                KeySym key = XkbKeycodeToKeysym(g_platform.display, event.xkey.keycode, 0, 0);
                if (key < 64) {
                    input->keys_down |= (1ULL << key);
                    input->keys_pressed |= (1ULL << key);
                }
                break;
            }
                
            case KeyRelease: {
                KeySym key = XkbKeycodeToKeysym(g_platform.display, event.xkey.keycode, 0, 0);
                if (key < 64) {
                    input->keys_down &= ~(1ULL << key);
                }
                break;
            }
                
            case ButtonPress:
                input->mouse_buttons |= (1 << (event.xbutton.button - 1));
                break;
                
            case ButtonRelease:
                input->mouse_buttons &= ~(1 << (event.xbutton.button - 1));
                break;
                
            case MotionNotify:
                input->mouse_delta.x = event.xmotion.x - input->mouse_pos.x;
                input->mouse_delta.y = event.xmotion.y - input->mouse_pos.y;
                input->mouse_pos.x = event.xmotion.x;
                input->mouse_pos.y = event.xmotion.y;
                break;
        }
    }
}

static void render_commands(RenderCommands* commands) {
    // PERFORMANCE: Batch rendering with minimal state changes
    // CACHE: Process commands sequentially for cache efficiency
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (commands->command_count > 0) {
        // SIMD: Could process 8 vertices at once with AVX2
        // For now, simple immediate mode rendering
        
        glBegin(GL_QUADS);
        for (uint32_t i = 0; i < commands->command_count; i++) {
            Vec2 pos = commands->positions[i];
            Vec2 size = commands->sizes[i];
            Color col = commands->colors[i];
            
            glColor4f(col.r, col.g, col.b, col.a);
            
            glVertex2f(pos.x, pos.y);
            glVertex2f(pos.x + size.x, pos.y);
            glVertex2f(pos.x + size.x, pos.y + size.y);
            glVertex2f(pos.x, pos.y + size.y);
        }
        glEnd();
    }
    
    glXSwapBuffers(g_platform.display, g_platform.window);
}

int main(int argc, char** argv) {
    printf("[PLATFORM] Handmade Engine Platform Layer\n");
    printf("[PLATFORM] Hot reload enabled - modify game.so while running!\n");
    
    // Initialize platform
    if (!init_x11_and_opengl(1280, 720)) {
        return 1;
    }
    
    // MEMORY: Allocate fixed memory blocks
    void* base_address = (void*)0x100000000000;  // Fixed base for stability
    
    GameMemory game_memory = {0};
    game_memory.permanent_size = PERMANENT_STORAGE_SIZE;
    game_memory.transient_size = TRANSIENT_STORAGE_SIZE;
    game_memory.debug_size = DEBUG_STORAGE_SIZE;
    
    // Try to get fixed address, fall back if needed
    game_memory.permanent_storage = mmap(base_address, 
                                         PERMANENT_STORAGE_SIZE,
                                         PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS,
                                         -1, 0);
    
    if (game_memory.permanent_storage == MAP_FAILED) {
        printf("[PLATFORM] Failed to allocate permanent storage\n");
        return 1;
    }
    
    game_memory.transient_storage = mmap(NULL,
                                         TRANSIENT_STORAGE_SIZE,
                                         PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS,
                                         -1, 0);
    
    if (game_memory.transient_storage == MAP_FAILED) {
        printf("[PLATFORM] Failed to allocate transient storage\n");
        return 1;
    }
    
    game_memory.debug_storage = mmap(NULL,
                                     DEBUG_STORAGE_SIZE,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS,
                                     -1, 0);
    
    if (game_memory.debug_storage == MAP_FAILED) {
        printf("[PLATFORM] Failed to allocate debug storage\n");
        return 1;
    }
    
    printf("[PLATFORM] Memory allocated:\n");
    printf("  Permanent: %p (%lu MB)\n", game_memory.permanent_storage, game_memory.permanent_size / (1024*1024));
    printf("  Transient: %p (%lu MB)\n", game_memory.transient_storage, game_memory.transient_size / (1024*1024));
    printf("  Debug:     %p (%lu MB)\n", game_memory.debug_storage, game_memory.debug_size / (1024*1024));
    
    // Setup platform API
    PlatformAPI platform_api = {
        .load_file = platform_load_file,
        .free_file = platform_free_file,
        .debug_print = platform_debug_print,
        .get_cycles = platform_get_cycles,
        .get_wall_clock = platform_get_wall_clock,
        .queue_work = platform_queue_work,
        .complete_all_work = platform_complete_all_work
    };
    
    // Initialize hot reload
    const char* game_dll_path = "./game.so";
    HotReloadState* hotreload = hotreload_init(game_dll_path);
    if (!hotreload) {
        printf("[PLATFORM] Failed to initialize hot reload\n");
        return 1;
    }
    
    // Get initial game code
    GameCode* game = &hotreload->code_buffer[hotreload->current_buffer];
    
    // Initialize game
    if (game->Initialize) {
        game->Initialize(&game_memory, &platform_api);
    }
    
    // Setup input
    GameInput input = {0};
    
    // Allocate render commands
    RenderCommands commands = {0};
    commands.positions = (Vec2*)malloc(sizeof(Vec2) * MAX_RENDER_COMMANDS);
    commands.sizes = (Vec2*)malloc(sizeof(Vec2) * MAX_RENDER_COMMANDS);
    commands.colors = (Color*)malloc(sizeof(Color) * MAX_RENDER_COMMANDS);
    commands.texture_ids = (uint32_t*)malloc(sizeof(uint32_t) * MAX_RENDER_COMMANDS);
    commands.tex_coords = (Vec2*)malloc(sizeof(Vec2) * MAX_RENDER_COMMANDS * 2);
    commands.text_buffer = (char*)malloc(1024 * 1024);  // 1MB text buffer
    
    // Initialize timing
    clock_gettime(CLOCK_MONOTONIC, &g_platform.start_time);
    g_platform.last_frame_time = g_platform.start_time;
    
    // PERFORMANCE: Main loop - maintain 60 FPS
    printf("[PLATFORM] Entering main loop...\n");
    
    while (!g_platform.should_quit) {
        uint64_t frame_start = read_cpu_timer();
        
        // Calculate timing
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        float dt = (current_time.tv_sec - g_platform.last_frame_time.tv_sec) +
                  (current_time.tv_nsec - g_platform.last_frame_time.tv_nsec) * 1e-9f;
        
        float total_time = (current_time.tv_sec - g_platform.start_time.tv_sec) +
                          (current_time.tv_nsec - g_platform.start_time.tv_nsec) * 1e-9f;
        
        g_platform.last_frame_time = current_time;
        
        // Update input
        input.keys_pressed = 0;  // Clear frame events
        input.mouse_delta.x = 0;
        input.mouse_delta.y = 0;
        input.dt = dt;
        input.time = total_time;
        
        process_x11_events(&input);
        
        // Check for hot reload
        if (hotreload_check_and_reload(hotreload)) {
            // Get new game code
            game = &hotreload->code_buffer[hotreload->current_buffer];
            printf("[PLATFORM] Game code reloaded!\n");
        }
        
        // Clear transient memory each frame
        game_memory.transient_used = 0;
        
        // Clear render commands
        commands.command_count = 0;
        commands.vertex_count = 0;
        commands.text_offset = 0;
        
        // Update and render game
        uint64_t update_start = read_cpu_timer();
        
        if (game->UpdateAndRender) {
            game->UpdateAndRender(&game_memory, &input, &commands);
        }
        
        uint64_t render_start = read_cpu_timer();
        
        // Render
        render_commands(&commands);
        
        uint64_t frame_end = read_cpu_timer();
        
        // Calculate performance metrics
        float cpu_freq = 3000.0f;  // 3GHz assumed
        g_platform.update_ms = (render_start - update_start) / (cpu_freq * 1000.0f);
        g_platform.render_ms = (frame_end - render_start) / (cpu_freq * 1000.0f);
        g_platform.frame_ms = (frame_end - frame_start) / (cpu_freq * 1000.0f);
        
        g_platform.frame_count++;
        
        // Print performance every second
        if (g_platform.frame_count % 60 == 0) {
            printf("[PERF] Frame: %.2fms | Update: %.2fms | Render: %.2fms | FPS: %.1f\n",
                   g_platform.frame_ms, g_platform.update_ms, g_platform.render_ms,
                   1000.0f / g_platform.frame_ms);
        }
    }
    
    printf("[PLATFORM] Shutting down...\n");
    
    // Cleanup
    hotreload_shutdown(hotreload);
    
    // Free render commands
    free(commands.positions);
    free(commands.sizes);
    free(commands.colors);
    free(commands.texture_ids);
    free(commands.tex_coords);
    free(commands.text_buffer);
    
    // Unmap memory
    munmap(game_memory.permanent_storage, game_memory.permanent_size);
    munmap(game_memory.transient_storage, game_memory.transient_size);
    munmap(game_memory.debug_storage, game_memory.debug_size);
    
    // Cleanup X11/OpenGL
    glXMakeCurrent(g_platform.display, None, NULL);
    glXDestroyContext(g_platform.display, g_platform.gl_context);
    XDestroyWindow(g_platform.display, g_platform.window);
    XCloseDisplay(g_platform.display);
    
    return 0;
}