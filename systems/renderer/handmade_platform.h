#ifndef HANDMADE_PLATFORM_H
#define HANDMADE_PLATFORM_H

/*
    Handmade Platform Layer
    Handles window creation, input, and OpenGL context
    
    Features:
    - Cross-platform window creation (Linux/X11 for now)
    - OpenGL 3.3 Core context
    - Input handling (keyboard, mouse)
    - File I/O
    - Time management
*/

#include "../../src/handmade.h"
#include "handmade_math.h"

// Platform capabilities
#define PLATFORM_MAX_KEYS 512
#define PLATFORM_MAX_MOUSE_BUTTONS 8
#define PLATFORM_MAX_GAMEPADS 4

// Key codes
typedef enum key_code {
    KEY_UNKNOWN = 0,
    
    // Letters
    KEY_A = 'A', KEY_B = 'B', KEY_C = 'C', KEY_D = 'D',
    KEY_E = 'E', KEY_F = 'F', KEY_G = 'G', KEY_H = 'H',
    KEY_I = 'I', KEY_J = 'J', KEY_K = 'K', KEY_L = 'L',
    KEY_M = 'M', KEY_N = 'N', KEY_O = 'O', KEY_P = 'P',
    KEY_Q = 'Q', KEY_R = 'R', KEY_S = 'S', KEY_T = 'T',
    KEY_U = 'U', KEY_V = 'V', KEY_W = 'W', KEY_X = 'X',
    KEY_Y = 'Y', KEY_Z = 'Z',
    
    // Numbers
    KEY_0 = '0', KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
    KEY_4 = '4', KEY_5 = '5', KEY_6 = '6', KEY_7 = '7',
    KEY_8 = '8', KEY_9 = '9',
    
    // Function keys
    KEY_F1 = 256, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    
    // Control keys
    KEY_ESCAPE, KEY_ENTER, KEY_TAB, KEY_BACKSPACE,
    KEY_INSERT, KEY_DELETE, KEY_HOME, KEY_END,
    KEY_PAGE_UP, KEY_PAGE_DOWN,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_SPACE, KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT,
    KEY_LEFT_CTRL, KEY_RIGHT_CTRL,
    KEY_LEFT_ALT, KEY_RIGHT_ALT,
    
    KEY_COUNT
} key_code;

// Mouse buttons
typedef enum mouse_button {
    MOUSE_LEFT = 0,
    MOUSE_RIGHT = 1,
    MOUSE_MIDDLE = 2,
    MOUSE_X1 = 3,
    MOUSE_X2 = 4,
    
    MOUSE_BUTTON_COUNT
} mouse_button;

// Input state
typedef struct platform_button_state {
    b32 is_down;
    b32 was_down;
    u32 transition_count;
} platform_button_state;

typedef struct mouse_state {
    i32 x, y;
    i32 dx, dy;
    i32 wheel_delta;
    platform_button_state buttons[PLATFORM_MAX_MOUSE_BUTTONS];
} mouse_state;

typedef struct keyboard_state {
    platform_button_state keys[PLATFORM_MAX_KEYS];
} keyboard_state;

typedef struct input_state {
    keyboard_state keyboard;
    mouse_state mouse;
    f64 time;
    f32 dt;
} input_state;

// Platform context
typedef struct platform_window platform_window;
typedef struct platform_opengl_context platform_opengl_context;

typedef struct platform_state {
    // Window
    platform_window *window;
    i32 window_width;
    i32 window_height;
    b32 is_running;
    b32 is_fullscreen;
    b32 vsync_enabled;
    
    // OpenGL
    platform_opengl_context *gl_context;
    i32 gl_major_version;
    i32 gl_minor_version;
    
    // Input
    input_state input;
    input_state prev_input;
    
    // Timing
    f64 start_time;
    f64 current_time;
    f64 last_frame_time;
    f32 target_fps;
    
    // Memory
    void *permanent_storage;
    u64 permanent_storage_size;
    void *transient_storage;
    u64 transient_storage_size;
    
    // File I/O
    char executable_path[1024];
    char working_directory[1024];
} platform_state;

// Window configuration
typedef struct window_config {
    char *title;
    i32 width;
    i32 height;
    b32 fullscreen;
    b32 vsync;
    b32 resizable;
    i32 samples; // MSAA samples (0 = disabled)
} window_config;

// File I/O
typedef struct platform_file {
    void *data;
    u64 size;
    b32 valid;
} platform_file;

// =============================================================================
// PLATFORM API
// =============================================================================

// Initialization
platform_state *platform_init(window_config *config, u64 permanent_storage_size, u64 transient_storage_size);
void platform_shutdown(platform_state *platform);

// Window management
void platform_poll_events(platform_state *platform);
void platform_swap_buffers(platform_state *platform);
void platform_set_window_title(platform_state *platform, char *title);
void platform_set_fullscreen(platform_state *platform, b32 fullscreen);
void platform_get_window_size(platform_state *platform, i32 *width, i32 *height);

// Input queries
b32 platform_key_down(platform_state *platform, key_code key);
b32 platform_key_pressed(platform_state *platform, key_code key);
b32 platform_key_released(platform_state *platform, key_code key);
b32 platform_mouse_down(platform_state *platform, mouse_button button);
b32 platform_mouse_pressed(platform_state *platform, mouse_button button);
b32 platform_mouse_released(platform_state *platform, mouse_button button);
void platform_get_mouse_pos(platform_state *platform, i32 *x, i32 *y);
void platform_get_mouse_delta(platform_state *platform, i32 *dx, i32 *dy);
i32 platform_get_mouse_wheel(platform_state *platform);

// Time
f64 platform_get_time(platform_state *platform);
f32 platform_get_dt(platform_state *platform);
void platform_sleep(u32 milliseconds);

// File I/O
platform_file platform_read_file(char *path);
b32 platform_write_file(char *path, void *data, u64 size);
void platform_free_file(platform_file *file);
b32 platform_file_exists(char *path);
u64 platform_get_file_time(char *path);

// Memory
void *platform_alloc(u64 size);
void platform_free(void *ptr);
void platform_zero_memory(void *ptr, u64 size);
void platform_copy_memory(void *dst, void *src, u64 size);

// Debug
void platform_log(char *format, ...);
void platform_error(char *format, ...);
void platform_assert(b32 condition, char *message);

// OpenGL loading
void *platform_gl_get_proc_address(char *name);
b32 platform_gl_load_functions(void);

#endif // HANDMADE_PLATFORM_H