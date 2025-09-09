// handmade_platform.h - Platform abstraction layer
// Zero dependencies, direct OS API calls only

#ifndef HANDMADE_PLATFORM_H
#define HANDMADE_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t *pixels;  // ARGB format
    int width;
    int height;
    int pitch;         // bytes per row
} platform_framebuffer;

typedef enum {
    KEY_NONE = 0,
    KEY_A = 'A', KEY_B = 'B', KEY_C = 'C', KEY_D = 'D',
    KEY_E = 'E', KEY_F = 'F', KEY_G = 'G', KEY_H = 'H',
    KEY_I = 'I', KEY_J = 'J', KEY_K = 'K', KEY_L = 'L',
    KEY_M = 'M', KEY_N = 'N', KEY_O = 'O', KEY_P = 'P',
    KEY_Q = 'Q', KEY_R = 'R', KEY_S = 'S', KEY_T = 'T',
    KEY_U = 'U', KEY_V = 'V', KEY_W = 'W', KEY_X = 'X',
    KEY_Y = 'Y', KEY_Z = 'Z',
    KEY_0 = '0', KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
    KEY_4 = '4', KEY_5 = '5', KEY_6 = '6', KEY_7 = '7',
    KEY_8 = '8', KEY_9 = '9',
    KEY_SPACE = ' ',
    KEY_ENTER = '\n',
    KEY_TAB = '\t',
    KEY_BACKSPACE = 0x08,
    KEY_ESCAPE = 0x1B,
    KEY_LEFT = 0x100,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_SHIFT = 0x200,
    KEY_CTRL,
    KEY_ALT
} platform_key;

typedef struct {
    int x, y;
    bool left_down;
    bool right_down;
    bool middle_down;
    int wheel_delta;
} platform_mouse_state;

typedef struct {
    bool keys[512];
    bool shift_down;
    bool ctrl_down;
    bool alt_down;
    char text_input[32];  // UTF-8 text input this frame
    int text_input_length;
} platform_keyboard_state;

typedef struct {
    platform_framebuffer framebuffer;
    platform_mouse_state mouse;
    platform_keyboard_state keyboard;
    
    double delta_time;  // seconds since last frame
    double total_time;  // seconds since start
    uint64_t frame_count;
    
    bool should_quit;
    bool window_active;
    int window_width;
    int window_height;
} platform_state;

// Platform interface - implemented per OS
bool platform_init(platform_state *state, const char *title, int width, int height);
void platform_shutdown(platform_state *state);
void platform_pump_events(platform_state *state);
void platform_present_framebuffer(platform_state *state);
double platform_get_time(void);  // High precision timer in seconds
void platform_sleep(double seconds);

// Memory functions
void *platform_alloc(size_t size);
void platform_free(void *ptr);

#endif // HANDMADE_PLATFORM_H