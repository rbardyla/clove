#define _GNU_SOURCE  // For clock_gettime, usleep
#include "handmade_hotreload.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// PERFORMANCE: Simplified platform layer for hot reload demo
// Uses X11 directly without OpenGL

#define PERMANENT_STORAGE_SIZE (256 * 1024 * 1024)  // 256MB
#define TRANSIENT_STORAGE_SIZE (128 * 1024 * 1024)  // 128MB
#define DEBUG_STORAGE_SIZE     (16 * 1024 * 1024)   // 16MB

typedef struct {
    Display* display;
    Window window;
    GC gc;
    Pixmap backbuffer;
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
    if (data) {
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

static void platform_queue_work(void (*work_proc)(void*), void* data) {
    work_proc(data);
}

static void platform_complete_all_work(void) {
    // No-op for single-threaded version
}

static bool init_x11(int width, int height) {
    g_platform.display = XOpenDisplay(NULL);
    if (!g_platform.display) {
        printf("[PLATFORM] Failed to open X display\n");
        return false;
    }
    
    int screen = DefaultScreen(g_platform.display);
    Window root = RootWindow(g_platform.display, screen);
    
    // Create window
    g_platform.window = XCreateSimpleWindow(
        g_platform.display, root,
        0, 0, width, height,
        1, BlackPixel(g_platform.display, screen),
        WhitePixel(g_platform.display, screen)
    );
    
    // Setup events
    XSelectInput(g_platform.display, g_platform.window,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 StructureNotifyMask);
    
    XMapWindow(g_platform.display, g_platform.window);
    XStoreName(g_platform.display, g_platform.window, "Handmade Engine - Hot Reload Demo");
    
    // Setup close button
    g_platform.wm_delete_window = XInternAtom(g_platform.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_platform.display, g_platform.window, &g_platform.wm_delete_window, 1);
    
    // Create graphics context
    g_platform.gc = XCreateGC(g_platform.display, g_platform.window, 0, NULL);
    
    // Create backbuffer
    g_platform.backbuffer = XCreatePixmap(g_platform.display, g_platform.window,
                                          width, height,
                                          DefaultDepth(g_platform.display, screen));
    
    g_platform.window_width = width;
    g_platform.window_height = height;
    
    printf("[PLATFORM] X11 initialized (%dx%d)\n", width, height);
    return true;
}

static void process_x11_events(GameInput* input) {
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
                if (event.xconfigure.width != g_platform.window_width ||
                    event.xconfigure.height != g_platform.window_height) {
                    g_platform.window_width = event.xconfigure.width;
                    g_platform.window_height = event.xconfigure.height;
                    
                    // Recreate backbuffer
                    XFreePixmap(g_platform.display, g_platform.backbuffer);
                    g_platform.backbuffer = XCreatePixmap(
                        g_platform.display, g_platform.window,
                        g_platform.window_width, g_platform.window_height,
                        DefaultDepth(g_platform.display, DefaultScreen(g_platform.display))
                    );
                }
                break;
                
            case KeyPress: {
                KeySym key = XkbKeycodeToKeysym(g_platform.display, event.xkey.keycode, 0, 0);
                // Convert to ASCII for simple keys
                if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';  // Lowercase
                // Use lower 6 bits of ASCII value
                uint32_t bit_index = key & 63;
                input->keys_down |= (1ULL << bit_index);
                input->keys_pressed |= (1ULL << bit_index);
                break;
            }
                
            case KeyRelease: {
                KeySym key = XkbKeycodeToKeysym(g_platform.display, event.xkey.keycode, 0, 0);
                if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';  // Lowercase
                uint32_t bit_index = key & 63;
                input->keys_down &= ~(1ULL << bit_index);
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
    // Clear backbuffer
    XSetForeground(g_platform.display, g_platform.gc, 0x1A1A1F);
    XFillRectangle(g_platform.display, g_platform.backbuffer, g_platform.gc,
                   0, 0, g_platform.window_width, g_platform.window_height);
    
    // Render commands to backbuffer
    for (uint32_t i = 0; i < commands->command_count; i++) {
        Vec2 pos = commands->positions[i];
        Vec2 size = commands->sizes[i];
        Color col = commands->colors[i];
        
        // Convert color to X11 format
        unsigned long color = ((int)(col.r * 255) << 16) |
                             ((int)(col.g * 255) << 8) |
                             ((int)(col.b * 255));
        
        XSetForeground(g_platform.display, g_platform.gc, color);
        XFillRectangle(g_platform.display, g_platform.backbuffer, g_platform.gc,
                      (int)pos.x, (int)pos.y, (int)size.x, (int)size.y);
    }
    
    // Copy backbuffer to window
    XCopyArea(g_platform.display, g_platform.backbuffer, g_platform.window, g_platform.gc,
              0, 0, g_platform.window_width, g_platform.window_height, 0, 0);
    
    XFlush(g_platform.display);
}

int main(int argc, char** argv) {
    printf("[PLATFORM] Handmade Engine Platform Layer\n");
    printf("[PLATFORM] Hot reload enabled - modify game.so while running!\n");
    printf("[PLATFORM] Controls: WASD to move, Tab for debug info\n");
    
    // Initialize platform
    if (!init_x11(1280, 720)) {
        return 1;
    }
    
    // MEMORY: Allocate fixed memory blocks
    GameMemory game_memory = {0};
    game_memory.permanent_size = PERMANENT_STORAGE_SIZE;
    game_memory.transient_size = TRANSIENT_STORAGE_SIZE;
    game_memory.debug_size = DEBUG_STORAGE_SIZE;
    
    game_memory.permanent_storage = mmap(NULL, 
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
    commands.text_buffer = (char*)malloc(1024 * 1024);
    
    // Initialize timing
    clock_gettime(CLOCK_MONOTONIC, &g_platform.start_time);
    g_platform.last_frame_time = g_platform.start_time;
    
    printf("[PLATFORM] Entering main loop...\n");
    
    while (!g_platform.should_quit) {
        uint64_t frame_start = read_cpu_timer();
        
        // Calculate timing
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        float dt = (current_time.tv_sec - g_platform.last_frame_time.tv_sec) +
                  (current_time.tv_nsec - g_platform.last_frame_time.tv_nsec) * 1e-9f;
        
        // Cap dt to prevent huge jumps
        if (dt > 0.1f) dt = 0.016f;
        
        float total_time = (current_time.tv_sec - g_platform.start_time.tv_sec) +
                          (current_time.tv_nsec - g_platform.start_time.tv_nsec) * 1e-9f;
        
        g_platform.last_frame_time = current_time;
        
        // Update input
        input.keys_pressed = 0;
        input.mouse_delta.x = 0;
        input.mouse_delta.y = 0;
        input.dt = dt;
        input.time = total_time;
        
        process_x11_events(&input);
        
        // Check for hot reload
        if (hotreload_check_and_reload(hotreload)) {
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
        
        // Simple frame rate limiting
        if (g_platform.frame_ms < 16.0f) {
            usleep((16.0f - g_platform.frame_ms) * 1000);
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
    
    // Cleanup X11
    XFreePixmap(g_platform.display, g_platform.backbuffer);
    XFreeGC(g_platform.display, g_platform.gc);
    XDestroyWindow(g_platform.display, g_platform.window);
    XCloseDisplay(g_platform.display);
    
    return 0;
}