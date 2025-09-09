// handmade_platform_linux.c - Direct X11 implementation, zero dependencies
// PERFORMANCE: All event handling and rendering optimized for <1ms overhead

#include "handmade_platform.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
    Display *display;
    Window window;
    GC gc;
    XImage *ximage;
    Atom wm_delete_window;
    
    uint32_t *pixel_buffer;
    int buffer_size;
    
    struct timespec start_time;
    struct timespec last_frame_time;
} linux_platform_data;

static linux_platform_data *g_platform_data = NULL;

// PERFORMANCE: Use CLOCK_MONOTONIC for consistent timing
double platform_get_time(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    if (g_platform_data) {
        double seconds = (now.tv_sec - g_platform_data->start_time.tv_sec);
        seconds += (now.tv_nsec - g_platform_data->start_time.tv_nsec) / 1000000000.0;
        return seconds;
    }
    
    return now.tv_sec + now.tv_nsec / 1000000000.0;
}

void platform_sleep(double seconds) {
    if (seconds > 0) {
        struct timespec req;
        req.tv_sec = (time_t)seconds;
        req.tv_nsec = (long)((seconds - req.tv_sec) * 1000000000.0);
        nanosleep(&req, NULL);
    }
}

void *platform_alloc(size_t size) {
    // MEMORY: Align to cache line for performance
    void *ptr = NULL;
    if (posix_memalign(&ptr, 64, size) == 0) {
        memset(ptr, 0, size);
        return ptr;
    }
    return NULL;
}

void platform_free(void *ptr) {
    free(ptr);
}

static platform_key translate_key(KeySym keysym) {
    // Direct mapping for common keys
    if (keysym >= XK_a && keysym <= XK_z) {
        return KEY_A + (keysym - XK_a);
    }
    if (keysym >= XK_A && keysym <= XK_Z) {
        return KEY_A + (keysym - XK_A);
    }
    if (keysym >= XK_0 && keysym <= XK_9) {
        return KEY_0 + (keysym - XK_0);
    }
    
    switch (keysym) {
        case XK_space: return KEY_SPACE;
        case XK_Return: return KEY_ENTER;
        case XK_Tab: return KEY_TAB;
        case XK_BackSpace: return KEY_BACKSPACE;
        case XK_Escape: return KEY_ESCAPE;
        case XK_Left: return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        case XK_Up: return KEY_UP;
        case XK_Down: return KEY_DOWN;
        case XK_Shift_L:
        case XK_Shift_R: return KEY_SHIFT;
        case XK_Control_L:
        case XK_Control_R: return KEY_CTRL;
        case XK_Alt_L:
        case XK_Alt_R: return KEY_ALT;
        default: return KEY_NONE;
    }
}

bool platform_init(platform_state *state, const char *title, int width, int height) {
    memset(state, 0, sizeof(platform_state));
    
    linux_platform_data *data = (linux_platform_data *)platform_alloc(sizeof(linux_platform_data));
    if (!data) return false;
    
    g_platform_data = data;
    clock_gettime(CLOCK_MONOTONIC, &data->start_time);
    data->last_frame_time = data->start_time;
    
    // Connect to X server
    data->display = XOpenDisplay(NULL);
    if (!data->display) {
        platform_free(data);
        return false;
    }
    
    int screen = DefaultScreen(data->display);
    Window root = RootWindow(data->display, screen);
    
    // Create window with backing store for better performance
    XSetWindowAttributes attrs = {0};
    attrs.backing_store = WhenMapped;
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       StructureNotifyMask | FocusChangeMask;
    
    data->window = XCreateWindow(
        data->display, root,
        0, 0, width, height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackingStore | CWEventMask, &attrs
    );
    
    // Set window title
    XStoreName(data->display, data->window, title);
    
    // Handle window close
    data->wm_delete_window = XInternAtom(data->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(data->display, data->window, &data->wm_delete_window, 1);
    
    // Create GC
    data->gc = XCreateGC(data->display, data->window, 0, NULL);
    
    // Create pixel buffer
    data->buffer_size = width * height * 4;
    data->pixel_buffer = (uint32_t *)platform_alloc(data->buffer_size);
    
    // Create XImage for blitting
    data->ximage = XCreateImage(
        data->display,
        DefaultVisual(data->display, screen),
        DefaultDepth(data->display, screen),
        ZPixmap, 0,
        (char *)data->pixel_buffer,
        width, height, 32, 0
    );
    
    // Initialize state
    state->framebuffer.pixels = data->pixel_buffer;
    state->framebuffer.width = width;
    state->framebuffer.height = height;
    state->framebuffer.pitch = width * 4;
    state->window_width = width;
    state->window_height = height;
    
    // Show window
    XMapWindow(data->display, data->window);
    XFlush(data->display);
    
    return true;
}

void platform_shutdown(platform_state *state) {
    if (!g_platform_data) return;
    
    linux_platform_data *data = g_platform_data;
    
    // Note: XDestroyImage will free the pixel buffer
    if (data->ximage) {
        data->ximage->data = NULL;  // Prevent double free
        XDestroyImage(data->ximage);
    }
    
    if (data->pixel_buffer) {
        platform_free(data->pixel_buffer);
    }
    
    if (data->gc) {
        XFreeGC(data->display, data->gc);
    }
    
    if (data->window) {
        XDestroyWindow(data->display, data->window);
    }
    
    if (data->display) {
        XCloseDisplay(data->display);
    }
    
    platform_free(data);
    g_platform_data = NULL;
}

void platform_pump_events(platform_state *state) {
    if (!g_platform_data) return;
    
    linux_platform_data *data = g_platform_data;
    
    // Clear text input
    state->keyboard.text_input_length = 0;
    state->mouse.wheel_delta = 0;
    
    // Update timing
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    state->delta_time = (now.tv_sec - data->last_frame_time.tv_sec) +
                       (now.tv_nsec - data->last_frame_time.tv_nsec) / 1000000000.0;
    data->last_frame_time = now;
    state->total_time = platform_get_time();
    state->frame_count++;
    
    // Process all pending events
    XEvent event;
    while (XPending(data->display) > 0) {
        XNextEvent(data->display, &event);
        
        switch (event.type) {
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == data->wm_delete_window) {
                    state->should_quit = true;
                }
                break;
                
            case ConfigureNotify:
                state->window_width = event.xconfigure.width;
                state->window_height = event.xconfigure.height;
                break;
                
            case FocusIn:
                state->window_active = true;
                break;
                
            case FocusOut:
                state->window_active = false;
                break;
                
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                platform_key key = translate_key(keysym);
                if (key != KEY_NONE) {
                    state->keyboard.keys[key] = true;
                    
                    // Update modifier states
                    if (key == KEY_SHIFT) state->keyboard.shift_down = true;
                    if (key == KEY_CTRL) state->keyboard.ctrl_down = true;
                    if (key == KEY_ALT) state->keyboard.alt_down = true;
                }
                
                // Handle text input
                char buffer[32];
                int len = XLookupString(&event.xkey, buffer, sizeof(buffer), NULL, NULL);
                if (len > 0 && state->keyboard.text_input_length < 31) {
                    memcpy(state->keyboard.text_input + state->keyboard.text_input_length, 
                           buffer, len);
                    state->keyboard.text_input_length += len;
                    state->keyboard.text_input[state->keyboard.text_input_length] = 0;
                }
                break;
            }
            
            case KeyRelease: {
                // Check for auto-repeat
                if (XEventsQueued(data->display, QueuedAfterReading)) {
                    XEvent next;
                    XPeekEvent(data->display, &next);
                    if (next.type == KeyPress && next.xkey.time == event.xkey.time &&
                        next.xkey.keycode == event.xkey.keycode) {
                        // Auto-repeat detected, ignore release
                        break;
                    }
                }
                
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                platform_key key = translate_key(keysym);
                if (key != KEY_NONE) {
                    state->keyboard.keys[key] = false;
                    
                    // Update modifier states
                    if (key == KEY_SHIFT) state->keyboard.shift_down = false;
                    if (key == KEY_CTRL) state->keyboard.ctrl_down = false;
                    if (key == KEY_ALT) state->keyboard.alt_down = false;
                }
                break;
            }
            
            case ButtonPress:
                switch (event.xbutton.button) {
                    case Button1: state->mouse.left_down = true; break;
                    case Button2: state->mouse.middle_down = true; break;
                    case Button3: state->mouse.right_down = true; break;
                    case Button4: state->mouse.wheel_delta = 1; break;  // Wheel up
                    case Button5: state->mouse.wheel_delta = -1; break; // Wheel down
                }
                break;
                
            case ButtonRelease:
                switch (event.xbutton.button) {
                    case Button1: state->mouse.left_down = false; break;
                    case Button2: state->mouse.middle_down = false; break;
                    case Button3: state->mouse.right_down = false; break;
                }
                break;
                
            case MotionNotify:
                state->mouse.x = event.xmotion.x;
                state->mouse.y = event.xmotion.y;
                break;
        }
    }
}

void platform_present_framebuffer(platform_state *state) {
    if (!g_platform_data) return;
    
    linux_platform_data *data = g_platform_data;
    
    // Blit the image to the window
    XPutImage(
        data->display, data->window, data->gc, data->ximage,
        0, 0, 0, 0,
        state->framebuffer.width, state->framebuffer.height
    );
    
    XFlush(data->display);
}