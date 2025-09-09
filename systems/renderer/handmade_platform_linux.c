/*
    Handmade Platform Layer - Linux/X11 Implementation
    Provides window creation and OpenGL context for Linux
*/

#include "handmade_platform.h"
#include "handmade_opengl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdarg.h>

// X11 headers
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// OpenGL headers - prevent GL/gl.h inclusion  
#define __gl_h_
#define GL_H
#include <GL/glx.h>

// Define GL constants we need
#define GL_VERSION                        0x1F02
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01

#define internal static

// Platform window structure (Linux/X11)
typedef struct platform_window {
    Display *display;
    Window window;
    Screen *screen;
    i32 screen_id;
    Atom wm_delete_window;
    b32 should_close;
} platform_window;

// OpenGL context (Linux/GLX)
typedef struct platform_opengl_context {
    GLXContext context;
    GLXFBConfig fb_config;
    XVisualInfo *visual;
} platform_opengl_context;

// Global platform instance
internal platform_state *g_platform = 0;

// Time helpers
internal f64
get_time_in_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (f64)ts.tv_sec + (f64)ts.tv_nsec / 1000000000.0;
}

// Key translation from X11 to our key codes
internal key_code
translate_key(KeySym keysym) {
    if (keysym >= XK_A && keysym <= XK_Z) {
        return (key_code)keysym;
    }
    if (keysym >= XK_a && keysym <= XK_z) {
        return (key_code)(keysym - 32); // Convert to uppercase
    }
    if (keysym >= XK_0 && keysym <= XK_9) {
        return (key_code)keysym;
    }
    
    switch (keysym) {
        case XK_Escape: return KEY_ESCAPE;
        case XK_Return: return KEY_ENTER;
        case XK_Tab: return KEY_TAB;
        case XK_BackSpace: return KEY_BACKSPACE;
        case XK_Insert: return KEY_INSERT;
        case XK_Delete: return KEY_DELETE;
        case XK_Home: return KEY_HOME;
        case XK_End: return KEY_END;
        case XK_Page_Up: return KEY_PAGE_UP;
        case XK_Page_Down: return KEY_PAGE_DOWN;
        case XK_Left: return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        case XK_Up: return KEY_UP;
        case XK_Down: return KEY_DOWN;
        case XK_space: return KEY_SPACE;
        case XK_Shift_L: return KEY_LEFT_SHIFT;
        case XK_Shift_R: return KEY_RIGHT_SHIFT;
        case XK_Control_L: return KEY_LEFT_CTRL;
        case XK_Control_R: return KEY_RIGHT_CTRL;
        case XK_Alt_L: return KEY_LEFT_ALT;
        case XK_Alt_R: return KEY_RIGHT_ALT;
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
        default: return KEY_UNKNOWN;
    }
}

// Create OpenGL context
internal b32
create_gl_context(platform_state *platform) {
    platform_window *window = platform->window;
    platform_opengl_context *gl = (platform_opengl_context *)malloc(sizeof(platform_opengl_context));
    
    // GLX attributes for framebuffer configuration
    int visual_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        None
    };
    
    int glx_major, glx_minor;
    if (!glXQueryVersion(window->display, &glx_major, &glx_minor)) {
        platform_error("Failed to query GLX version");
        free(gl);
        return 0;
    }
    
    platform_log("GLX version: %d.%d", glx_major, glx_minor);
    
    // Get framebuffer configurations
    int fb_count;
    GLXFBConfig *fb_configs = glXChooseFBConfig(window->display, window->screen_id, 
                                                visual_attribs, &fb_count);
    if (!fb_configs || fb_count == 0) {
        platform_error("Failed to find suitable framebuffer configuration");
        free(gl);
        return 0;
    }
    
    // Pick the first configuration
    gl->fb_config = fb_configs[0];
    XFree(fb_configs);
    
    // Get visual info
    gl->visual = glXGetVisualFromFBConfig(window->display, gl->fb_config);
    if (!gl->visual) {
        platform_error("Failed to get visual info");
        free(gl);
        return 0;
    }
    
    // Create OpenGL context
    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");
    
    if (glXCreateContextAttribsARB) {
        // Request OpenGL 3.3 Core Profile
        int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };
        
        gl->context = glXCreateContextAttribsARB(window->display, gl->fb_config, 
                                                 0, True, context_attribs);
    } else {
        // Fallback to legacy context
        gl->context = glXCreateNewContext(window->display, gl->fb_config, 
                                         GLX_RGBA_TYPE, 0, True);
    }
    
    if (!gl->context) {
        platform_error("Failed to create OpenGL context");
        XFree(gl->visual);
        free(gl);
        return 0;
    }
    
    // Make context current
    if (!glXMakeCurrent(window->display, window->window, gl->context)) {
        platform_error("Failed to make OpenGL context current");
        glXDestroyContext(window->display, gl->context);
        XFree(gl->visual);
        free(gl);
        return 0;
    }
    
    // Load OpenGL functions
    if (!gl_load_functions((void *(*)(char *))glXGetProcAddressARB)) {
        platform_error("Failed to load OpenGL functions");
        glXDestroyContext(window->display, gl->context);
        XFree(gl->visual);
        free(gl);
        return 0;
    }
    
    platform->gl_context = gl;
    platform->gl_major_version = 3;
    platform->gl_minor_version = 3;
    
    // Get OpenGL version
    const char *version = (const char *)glGetString(GL_VERSION);
    platform_log("OpenGL version: %s", version);
    platform_log("OpenGL vendor: %s", glGetString(GL_VENDOR));
    platform_log("OpenGL renderer: %s", glGetString(GL_RENDERER));
    
    return 1;
}

// Platform initialization
platform_state *
platform_init(window_config *config, u64 permanent_storage_size, u64 transient_storage_size) {
    // Allocate platform state
    platform_state *platform = (platform_state *)calloc(1, sizeof(platform_state));
    if (!platform) {
        platform_error("Failed to allocate platform state");
        return 0;
    }
    
    g_platform = platform;
    
    // Allocate memory pools
    platform->permanent_storage_size = permanent_storage_size;
    platform->permanent_storage = mmap(0, permanent_storage_size, 
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (platform->permanent_storage == MAP_FAILED) {
        platform_error("Failed to allocate permanent storage");
        free(platform);
        return 0;
    }
    
    platform->transient_storage_size = transient_storage_size;
    platform->transient_storage = mmap(0, transient_storage_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (platform->transient_storage == MAP_FAILED) {
        platform_error("Failed to allocate transient storage");
        munmap(platform->permanent_storage, permanent_storage_size);
        free(platform);
        return 0;
    }
    
    // Initialize window
    platform_window *window = (platform_window *)calloc(1, sizeof(platform_window));
    platform->window = window;
    
    // Open X11 display
    window->display = XOpenDisplay(NULL);
    if (!window->display) {
        platform_error("Failed to open X11 display");
        munmap(platform->permanent_storage, permanent_storage_size);
        munmap(platform->transient_storage, transient_storage_size);
        free(window);
        free(platform);
        return 0;
    }
    
    window->screen_id = DefaultScreen(window->display);
    window->screen = DefaultScreenOfDisplay(window->display);
    
    // Create window
    Window root = RootWindow(window->display, window->screen_id);
    
    XSetWindowAttributes window_attribs = {0};
    window_attribs.colormap = XCreateColormap(window->display, root,
                                             DefaultVisual(window->display, window->screen_id),
                                             AllocNone);
    window_attribs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                                ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                                StructureNotifyMask | FocusChangeMask;
    
    window->window = XCreateWindow(
        window->display, root,
        0, 0, config->width, config->height,
        0, DefaultDepth(window->display, window->screen_id),
        InputOutput, DefaultVisual(window->display, window->screen_id),
        CWColormap | CWEventMask, &window_attribs
    );
    
    if (!window->window) {
        platform_error("Failed to create X11 window");
        XCloseDisplay(window->display);
        munmap(platform->permanent_storage, permanent_storage_size);
        munmap(platform->transient_storage, transient_storage_size);
        free(window);
        free(platform);
        return 0;
    }
    
    // Set window title
    XStoreName(window->display, window->window, config->title);
    
    // Setup window close event
    window->wm_delete_window = XInternAtom(window->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window->display, window->window, &window->wm_delete_window, 1);
    
    // Show window
    XMapWindow(window->display, window->window);
    XFlush(window->display);
    
    platform->window_width = config->width;
    platform->window_height = config->height;
    platform->is_fullscreen = config->fullscreen;
    platform->vsync_enabled = config->vsync;
    platform->is_running = 1;
    
    // Create OpenGL context
    if (!create_gl_context(platform)) {
        XDestroyWindow(window->display, window->window);
        XCloseDisplay(window->display);
        munmap(platform->permanent_storage, permanent_storage_size);
        munmap(platform->transient_storage, transient_storage_size);
        free(window);
        free(platform);
        return 0;
    }
    
    // Set vsync
    if (config->vsync) {
        typedef void (*glXSwapIntervalEXTProc)(Display*, GLXDrawable, int);
        glXSwapIntervalEXTProc glXSwapIntervalEXT = 
            (glXSwapIntervalEXTProc)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT) {
            glXSwapIntervalEXT(window->display, window->window, 1);
        }
    }
    
    // Initialize timing
    platform->start_time = get_time_in_seconds();
    platform->current_time = platform->start_time;
    platform->last_frame_time = platform->start_time;
    platform->target_fps = 60.0f;
    
    // Get executable path
    readlink("/proc/self/exe", platform->executable_path, sizeof(platform->executable_path) - 1);
    getcwd(platform->working_directory, sizeof(platform->working_directory) - 1);
    
    platform_log("Platform initialized successfully");
    platform_log("Window: %dx%d", config->width, config->height);
    platform_log("Memory: %llu MB permanent, %llu MB transient", 
                permanent_storage_size / (1024*1024), transient_storage_size / (1024*1024));
    
    return platform;
}

void
platform_shutdown(platform_state *platform) {
    if (!platform) return;
    
    if (platform->gl_context) {
        platform_window *window = platform->window;
        platform_opengl_context *gl = platform->gl_context;
        
        glXMakeCurrent(window->display, None, NULL);
        glXDestroyContext(window->display, gl->context);
        XFree(gl->visual);
        free(gl);
    }
    
    if (platform->window) {
        platform_window *window = platform->window;
        XDestroyWindow(window->display, window->window);
        XCloseDisplay(window->display);
        free(window);
    }
    
    if (platform->permanent_storage) {
        munmap(platform->permanent_storage, platform->permanent_storage_size);
    }
    
    if (platform->transient_storage) {
        munmap(platform->transient_storage, platform->transient_storage_size);
    }
    
    free(platform);
    g_platform = 0;
}

void
platform_poll_events(platform_state *platform) {
    platform_window *window = platform->window;
    
    // Save previous input state
    platform->prev_input = platform->input;
    
    // Update time
    f64 current_time = get_time_in_seconds();
    platform->input.dt = (f32)(current_time - platform->last_frame_time);
    platform->input.time = current_time - platform->start_time;
    platform->last_frame_time = current_time;
    platform->current_time = current_time;
    
    // Reset mouse delta
    platform->input.mouse.dx = 0;
    platform->input.mouse.dy = 0;
    platform->input.mouse.wheel_delta = 0;
    
    // Process X11 events
    XEvent event;
    while (XPending(window->display)) {
        XNextEvent(window->display, &event);
        
        switch (event.type) {
            case ClientMessage: {
                if ((Atom)event.xclient.data.l[0] == window->wm_delete_window) {
                    platform->is_running = 0;
                }
            } break;
            
            case ConfigureNotify: {
                XConfigureEvent *xce = &event.xconfigure;
                if (xce->width != platform->window_width || xce->height != platform->window_height) {
                    platform->window_width = xce->width;
                    platform->window_height = xce->height;
                    glViewport(0, 0, xce->width, xce->height);
                }
            } break;
            
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                key_code key = translate_key(keysym);
                if (key != KEY_UNKNOWN) {
                    platform->input.keyboard.keys[key].is_down = 1;
                    platform->input.keyboard.keys[key].transition_count++;
                }
            } break;
            
            case KeyRelease: {
                // Check for key repeat
                if (XEventsQueued(window->display, QueuedAfterReading)) {
                    XEvent nev;
                    XPeekEvent(window->display, &nev);
                    if (nev.type == KeyPress && nev.xkey.time == event.xkey.time &&
                        nev.xkey.keycode == event.xkey.keycode) {
                        // Key repeat, ignore release
                        XNextEvent(window->display, &nev);
                        break;
                    }
                }
                
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                key_code key = translate_key(keysym);
                if (key != KEY_UNKNOWN) {
                    platform->input.keyboard.keys[key].is_down = 0;
                    platform->input.keyboard.keys[key].transition_count++;
                }
            } break;
            
            case ButtonPress: {
                if (event.xbutton.button >= 1 && event.xbutton.button <= 5) {
                    if (event.xbutton.button == 4) {
                        platform->input.mouse.wheel_delta = 1;
                    } else if (event.xbutton.button == 5) {
                        platform->input.mouse.wheel_delta = -1;
                    } else {
                        i32 button = event.xbutton.button - 1;
                        platform->input.mouse.buttons[button].is_down = 1;
                        platform->input.mouse.buttons[button].transition_count++;
                    }
                }
            } break;
            
            case ButtonRelease: {
                if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
                    i32 button = event.xbutton.button - 1;
                    platform->input.mouse.buttons[button].is_down = 0;
                    platform->input.mouse.buttons[button].transition_count++;
                }
            } break;
            
            case MotionNotify: {
                i32 new_x = event.xmotion.x;
                i32 new_y = event.xmotion.y;
                platform->input.mouse.dx = new_x - platform->input.mouse.x;
                platform->input.mouse.dy = new_y - platform->input.mouse.y;
                platform->input.mouse.x = new_x;
                platform->input.mouse.y = new_y;
            } break;
        }
    }
    
    // Update was_down states
    for (i32 i = 0; i < PLATFORM_MAX_KEYS; i++) {
        platform->input.keyboard.keys[i].was_down = platform->prev_input.keyboard.keys[i].is_down;
    }
    for (i32 i = 0; i < PLATFORM_MAX_MOUSE_BUTTONS; i++) {
        platform->input.mouse.buttons[i].was_down = platform->prev_input.mouse.buttons[i].is_down;
    }
}

void
platform_swap_buffers(platform_state *platform) {
    if (platform && platform->window) {
        glXSwapBuffers(platform->window->display, platform->window->window);
    }
}

// Input queries
b32 platform_key_down(platform_state *platform, key_code key) {
    return platform->input.keyboard.keys[key].is_down;
}

b32 platform_key_pressed(platform_state *platform, key_code key) {
    platform_button_state *state = &platform->input.keyboard.keys[key];
    return state->is_down && !state->was_down;
}

b32 platform_key_released(platform_state *platform, key_code key) {
    platform_button_state *state = &platform->input.keyboard.keys[key];
    return !state->is_down && state->was_down;
}

b32 platform_mouse_down(platform_state *platform, mouse_button button) {
    return platform->input.mouse.buttons[button].is_down;
}

b32 platform_mouse_pressed(platform_state *platform, mouse_button button) {
    platform_button_state *state = &platform->input.mouse.buttons[button];
    return state->is_down && !state->was_down;
}

b32 platform_mouse_released(platform_state *platform, mouse_button button) {
    platform_button_state *state = &platform->input.mouse.buttons[button];
    return !state->is_down && state->was_down;
}

void platform_get_mouse_pos(platform_state *platform, i32 *x, i32 *y) {
    *x = platform->input.mouse.x;
    *y = platform->input.mouse.y;
}

void platform_get_mouse_delta(platform_state *platform, i32 *dx, i32 *dy) {
    *dx = platform->input.mouse.dx;
    *dy = platform->input.mouse.dy;
}

i32 platform_get_mouse_wheel(platform_state *platform) {
    return platform->input.mouse.wheel_delta;
}

// Time
f64 platform_get_time(platform_state *platform) {
    return platform->current_time - platform->start_time;
}

f32 platform_get_dt(platform_state *platform) {
    return platform->input.dt;
}

void platform_sleep(u32 milliseconds) {
    usleep(milliseconds * 1000);
}

// File I/O
platform_file platform_read_file(char *path) {
    platform_file result = {0};
    
    FILE *file = fopen(path, "rb");
    if (!file) {
        return result;
    }
    
    fseek(file, 0, SEEK_END);
    result.size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    result.data = malloc(result.size + 1);
    if (!result.data) {
        fclose(file);
        return result;
    }
    
    size_t bytes_read = fread(result.data, 1, result.size, file);
    if (bytes_read != result.size) {
        free(result.data);
        result.data = 0;
        result.size = 0;
        fclose(file);
        return result;
    }
    
    ((char *)result.data)[result.size] = 0; // Null terminate
    result.valid = 1;
    
    fclose(file);
    return result;
}

b32 platform_write_file(char *path, void *data, u64 size) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return 0;
    }
    
    size_t bytes_written = fwrite(data, 1, size, file);
    fclose(file);
    
    return bytes_written == size;
}

void platform_free_file(platform_file *file) {
    if (file && file->data) {
        free(file->data);
        file->data = 0;
        file->size = 0;
        file->valid = 0;
    }
}

// Debug
void platform_log(char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[PLATFORM] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void platform_error(char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void platform_assert(b32 condition, char *message) {
    if (!condition) {
        platform_error("Assertion failed: %s", message);
        abort();
    }
}

// OpenGL function loading
void *platform_gl_get_proc_address(char *name) {
    return (void *)glXGetProcAddressARB((const GLubyte *)name);
}

// Window management
void platform_set_window_title(platform_state *platform, char *title) {
    if (platform && platform->window) {
        XStoreName(platform->window->display, platform->window->window, title);
    }
}

void platform_get_window_size(platform_state *platform, i32 *width, i32 *height) {
    *width = platform->window_width;
    *height = platform->window_height;
}

// Memory
void *platform_alloc(u64 size) {
    return malloc(size);
}

void platform_free(void *ptr) {
    free(ptr);
}

void platform_zero_memory(void *ptr, u64 size) {
    memset(ptr, 0, size);
}

void platform_copy_memory(void *dst, void *src, u64 size) {
    memcpy(dst, src, size);
}