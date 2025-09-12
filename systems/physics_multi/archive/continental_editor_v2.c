/*
    Continental Architect EDITOR V2
    
    Improved editor with:
    - Working window resize
    - Min/Max/Close buttons
    - Actual window content
    - Better text rendering
    - Scrollbars
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900
#define MAX_WINDOWS 20
#define MAX_CONSOLE_LINES 500
#define MAX_FILES 1000
#define TITLE_BAR_HEIGHT 30
#define BUTTON_SIZE 20

// ============= WINDOW SYSTEM =============

typedef enum {
    WINDOW_SCENE,
    WINDOW_CONSOLE,
    WINDOW_FILES,
    WINDOW_PROPERTIES,
    WINDOW_TOOLBAR,
    WINDOW_CODE
} WindowType;

typedef enum {
    WIN_STATE_NORMAL,
    WIN_STATE_MINIMIZED,
    WIN_STATE_MAXIMIZED
} WindowState;

typedef struct {
    char title[64];
    float x, y, width, height;
    float saved_x, saved_y, saved_width, saved_height; // For restore
    float content_scroll_x, content_scroll_y;
    float content_width, content_height;
    WindowType type;
    WindowState state;
    int visible;
    int focused;
    int moving;
    int resizing;
    int resize_edge; // 0=none, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW
    float move_offset_x, move_offset_y;
    float min_width, min_height;
    void* data;
} EditorWindow;

typedef struct {
    char lines[MAX_CONSOLE_LINES][256];
    int line_count;
    int autoscroll;
} Console;

typedef struct {
    char current_path[512];
    char files[MAX_FILES][256];
    int file_count;
    int selected_file;
    int is_dir[MAX_FILES];
} FileBrowser;

typedef struct {
    pid_t engine_pid;
    int is_running;
    int needs_compile;
    char project_path[256];
} EngineState;

typedef struct {
    EditorWindow windows[MAX_WINDOWS];
    int window_count;
    int active_window;
    
    Console console;
    FileBrowser file_browser;
    EngineState engine;
    
    // Mouse
    int mouse_x, mouse_y;
    int mouse_down;
    int hover_window;
    int hover_button; // 0=none, 1=close, 2=max, 3=min
    
    // UI
    float ui_scale;
    int show_grid;
    int dark_mode;
    
    // Font (simple bitmap)
    unsigned char font_data[128][8];
    
    // Performance
    float fps;
    double last_time;
} Editor;

Editor* editor;

// ============= SIMPLE FONT =============

void init_font() {
    // Basic ASCII characters as 8x8 bitmaps
    unsigned char chars[128][8] = {
        ['A'] = {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
        ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
        ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
        ['E'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00},
        ['F'] = {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00},
        ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
        ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
        ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
        ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
        ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
        ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
        ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
        ['S'] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
        ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
        ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
        ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
        ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
        ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
        ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
        ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
        ['a'] = {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
        ['b'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
        ['c'] = {0x00, 0x00, 0x3C, 0x60, 0x60, 0x60, 0x3C, 0x00},
        ['d'] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
        ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
        ['f'] = {0x0E, 0x18, 0x3E, 0x18, 0x18, 0x18, 0x18, 0x00},
        ['g'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x7C},
        ['h'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
        ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
        ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        ['m'] = {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
        ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
        ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
        ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
        ['r'] = {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
        ['s'] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
        ['t'] = {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00},
        ['u'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
        ['v'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
        ['w'] = {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00},
        ['x'] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
        ['y'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x0C, 0x78},
        ['z'] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
        ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
        ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
        ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
        ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
        ['4'] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
        ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
        ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
        ['7'] = {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
        ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
        ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
        [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
        [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
        [':'] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
        ['/'] = {0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00, 0x00},
        ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
        ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00},
        ['|'] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
        ['['] = {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
        [']'] = {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
        ['('] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
        [')'] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    };
    
    memcpy(editor->font_data, chars, sizeof(chars));
}

void draw_char(float x, float y, char c, float scale) {
    if (c < 0 || c >= 128) return;
    
    unsigned char* bitmap = editor->font_data[(int)c];
    
    glBegin(GL_POINTS);
    for (int row = 0; row < 8; row++) {
        unsigned char line = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        glVertex2f(x + col * scale + sx, y + row * scale + sy);
                    }
                }
            }
        }
    }
    glEnd();
}

void draw_text(float x, float y, const char* text, float scale) {
    float cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(cursor_x, y, text[i], scale);
        cursor_x += 8 * scale;
    }
}

// ============= CONSOLE =============

void console_add(const char* text) {
    if (editor->console.line_count >= MAX_CONSOLE_LINES) {
        for (int i = 0; i < MAX_CONSOLE_LINES - 1; i++) {
            strcpy(editor->console.lines[i], editor->console.lines[i + 1]);
        }
        editor->console.line_count = MAX_CONSOLE_LINES - 1;
    }
    
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S]", tm);
    
    snprintf(editor->console.lines[editor->console.line_count], 256, "%s %s", timestamp, text);
    editor->console.line_count++;
}

// ============= FILE BROWSER =============

void refresh_files() {
    DIR* dir = opendir(editor->file_browser.current_path);
    if (!dir) return;
    
    editor->file_browser.file_count = 0;
    struct dirent* entry;
    
    // Add parent directory
    strcpy(editor->file_browser.files[editor->file_browser.file_count], "..");
    editor->file_browser.is_dir[editor->file_browser.file_count] = 1;
    editor->file_browser.file_count++;
    
    while ((entry = readdir(dir)) != NULL && editor->file_browser.file_count < MAX_FILES) {
        if (entry->d_name[0] == '.' && strcmp(entry->d_name, "..") != 0) continue;
        
        strcpy(editor->file_browser.files[editor->file_browser.file_count], entry->d_name);
        editor->file_browser.is_dir[editor->file_browser.file_count] = (entry->d_type == DT_DIR);
        editor->file_browser.file_count++;
    }
    
    closedir(dir);
}

// ============= ENGINE CONTROL =============

void compile_engine() {
    console_add("Compiling engine...");
    
    int result = system("cd /home/thebackhand/Projects/handmade-engine/systems/physics_multi && "
                       "gcc -o ../../binaries/continental_engine continental_ultimate.c "
                       "-lX11 -lGL -lm -O3 -march=native -ffast-math 2>&1");
    
    if (result == 0) {
        console_add("SUCCESS: Compilation complete!");
    } else {
        console_add("ERROR: Compilation failed!");
    }
}

void start_engine() {
    if (editor->engine.is_running) {
        console_add("Engine already running");
        return;
    }
    
    console_add("Starting engine...");
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("/home/thebackhand/Projects/handmade-engine/binaries/continental_ultimate",
              "continental_ultimate", NULL);
        exit(1);
    } else if (pid > 0) {
        editor->engine.engine_pid = pid;
        editor->engine.is_running = 1;
        console_add("Engine started");
    } else {
        console_add("Failed to start engine");
    }
}

void stop_engine() {
    if (!editor->engine.is_running) return;
    
    console_add("Stopping engine...");
    
    if (editor->engine.engine_pid > 0) {
        kill(editor->engine.engine_pid, SIGTERM);
        waitpid(editor->engine.engine_pid, NULL, 0);
        editor->engine.engine_pid = 0;
        editor->engine.is_running = 0;
        console_add("Engine stopped");
    }
}

// ============= WINDOW MANAGEMENT =============

EditorWindow* create_window(const char* title, float x, float y, float w, float h, WindowType type) {
    if (editor->window_count >= MAX_WINDOWS) return NULL;
    
    EditorWindow* win = &editor->windows[editor->window_count++];
    strcpy(win->title, title);
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->type = type;
    win->state = WIN_STATE_NORMAL;
    win->visible = 1;
    win->focused = 0;
    win->moving = 0;
    win->resizing = 0;
    win->resize_edge = 0;
    win->content_scroll_x = 0;
    win->content_scroll_y = 0;
    win->min_width = 150;
    win->min_height = 100;
    
    return win;
}

// Forward declarations for window content renderers
void render_console(EditorWindow* win);
void render_file_browser(EditorWindow* win);
void render_toolbar(EditorWindow* win);
void render_scene(EditorWindow* win);
void render_properties(EditorWindow* win);
void render_code_editor(EditorWindow* win);

void minimize_window(EditorWindow* win) {
    if (win->state == WIN_STATE_MINIMIZED) {
        // Restore
        win->state = WIN_STATE_NORMAL;
        win->x = win->saved_x;
        win->y = win->saved_y;
        win->width = win->saved_width;
        win->height = win->saved_height;
        win->visible = 1;
    } else {
        // Minimize
        win->saved_x = win->x;
        win->saved_y = win->y;
        win->saved_width = win->width;
        win->saved_height = win->height;
        win->state = WIN_STATE_MINIMIZED;
        win->visible = 0;
    }
}

void maximize_window(EditorWindow* win) {
    if (win->state == WIN_STATE_MAXIMIZED) {
        // Restore
        win->state = WIN_STATE_NORMAL;
        win->x = win->saved_x;
        win->y = win->saved_y;
        win->width = win->saved_width;
        win->height = win->saved_height;
    } else {
        // Maximize
        win->saved_x = win->x;
        win->saved_y = win->y;
        win->saved_width = win->width;
        win->saved_height = win->height;
        win->state = WIN_STATE_MAXIMIZED;
        win->x = 0;
        win->y = 0;
        win->width = WINDOW_WIDTH;
        win->height = WINDOW_HEIGHT - 25;
    }
}

void close_window(EditorWindow* win) {
    win->visible = 0;
    // In real app would remove from array
}

int get_resize_edge(EditorWindow* win, int x, int y) {
    int edge_size = 5;
    int on_left = (x >= win->x && x <= win->x + edge_size);
    int on_right = (x >= win->x + win->width - edge_size && x <= win->x + win->width);
    int on_top = (y >= win->y && y <= win->y + edge_size);
    int on_bottom = (y >= win->y + win->height - edge_size && y <= win->y + win->height);
    
    if (on_top && on_left) return 8; // NW
    if (on_top && on_right) return 2; // NE
    if (on_bottom && on_left) return 6; // SW
    if (on_bottom && on_right) return 4; // SE
    if (on_top) return 1; // N
    if (on_bottom) return 5; // S
    if (on_left) return 7; // W
    if (on_right) return 3; // E
    
    return 0;
}

// ============= RENDERING =============

void render_window(EditorWindow* win) {
    if (!win->visible) return;
    
    float x = win->x;
    float y = win->y;
    float w = win->width;
    float h = win->height;
    
    // Shadow
    glColor4f(0, 0, 0, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(x + 4, y + 4);
    glVertex2f(x + w + 4, y + 4);
    glVertex2f(x + w + 4, y + h + 4);
    glVertex2f(x + 4, y + h + 4);
    glEnd();
    
    // Window background
    glColor4f(0.15f, 0.15f, 0.18f, 0.98f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Title bar
    if (win->focused) {
        glColor4f(0.2f, 0.4f, 0.7f, 1.0f);
    } else {
        glColor4f(0.25f, 0.25f, 0.3f, 1.0f);
    }
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + TITLE_BAR_HEIGHT);
    glVertex2f(x, y + TITLE_BAR_HEIGHT);
    glEnd();
    
    // Title text
    glColor3f(1, 1, 1);
    draw_text(x + 10, y + 8, win->title, 1.5f);
    
    // Window buttons (close, max, min)
    float btn_x = x + w - BUTTON_SIZE - 5;
    float btn_y = y + 5;
    
    // Close button (X)
    glColor4f(0.8f, 0.2f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(btn_x, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y + BUTTON_SIZE);
    glVertex2f(btn_x, btn_y + BUTTON_SIZE);
    glEnd();
    
    glColor3f(1, 1, 1);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(btn_x + 5, btn_y + 5);
    glVertex2f(btn_x + 15, btn_y + 15);
    glVertex2f(btn_x + 15, btn_y + 5);
    glVertex2f(btn_x + 5, btn_y + 15);
    glEnd();
    
    // Maximize button
    btn_x -= BUTTON_SIZE + 5;
    glColor4f(0.2f, 0.6f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(btn_x, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y + BUTTON_SIZE);
    glVertex2f(btn_x, btn_y + BUTTON_SIZE);
    glEnd();
    
    glColor3f(1, 1, 1);
    glBegin(GL_LINE_LOOP);
    glVertex2f(btn_x + 5, btn_y + 5);
    glVertex2f(btn_x + 15, btn_y + 5);
    glVertex2f(btn_x + 15, btn_y + 15);
    glVertex2f(btn_x + 5, btn_y + 15);
    glEnd();
    
    // Minimize button
    btn_x -= BUTTON_SIZE + 5;
    glColor4f(0.6f, 0.6f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(btn_x, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y + BUTTON_SIZE);
    glVertex2f(btn_x, btn_y + BUTTON_SIZE);
    glEnd();
    
    glColor3f(1, 1, 1);
    glBegin(GL_LINES);
    glVertex2f(btn_x + 5, btn_y + 15);
    glVertex2f(btn_x + 15, btn_y + 15);
    glEnd();
    
    // Content area background
    glColor4f(0.1f, 0.1f, 0.12f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x + 1, y + TITLE_BAR_HEIGHT);
    glVertex2f(x + w - 1, y + TITLE_BAR_HEIGHT);
    glVertex2f(x + w - 1, y + h - 1);
    glVertex2f(x + 1, y + h - 1);
    glEnd();
    
    // Window border
    glLineWidth(win->focused ? 2.0f : 1.0f);
    glColor4f(0.3f, 0.3f, 0.35f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Render content based on type
    glPushMatrix();
    glTranslatef(x, y + TITLE_BAR_HEIGHT, 0);
    
    // Clip to content area
    glEnable(GL_SCISSOR_TEST);
    glScissor(x + 1, WINDOW_HEIGHT - (y + h), w - 2, h - TITLE_BAR_HEIGHT - 1);
    
    switch (win->type) {
        case WINDOW_CONSOLE:
            render_console(win);
            break;
        case WINDOW_FILES:
            render_file_browser(win);
            break;
        case WINDOW_TOOLBAR:
            render_toolbar(win);
            break;
        case WINDOW_SCENE:
            render_scene(win);
            break;
        case WINDOW_PROPERTIES:
            render_properties(win);
            break;
        case WINDOW_CODE:
            render_code_editor(win);
            break;
    }
    
    glDisable(GL_SCISSOR_TEST);
    glPopMatrix();
    
    // Scrollbars if needed
    if (win->content_height > win->height - TITLE_BAR_HEIGHT) {
        // Vertical scrollbar
        float scrollbar_x = x + w - 15;
        float scrollbar_y = y + TITLE_BAR_HEIGHT;
        float scrollbar_height = h - TITLE_BAR_HEIGHT;
        
        glColor4f(0.2f, 0.2f, 0.2f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(scrollbar_x, scrollbar_y);
        glVertex2f(scrollbar_x + 12, scrollbar_y);
        glVertex2f(scrollbar_x + 12, scrollbar_y + scrollbar_height);
        glVertex2f(scrollbar_x, scrollbar_y + scrollbar_height);
        glEnd();
        
        // Scroll thumb
        float thumb_height = (scrollbar_height / win->content_height) * scrollbar_height;
        float thumb_y = scrollbar_y + (win->content_scroll_y / win->content_height) * scrollbar_height;
        
        glColor4f(0.5f, 0.5f, 0.5f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(scrollbar_x + 2, thumb_y);
        glVertex2f(scrollbar_x + 10, thumb_y);
        glVertex2f(scrollbar_x + 10, thumb_y + thumb_height);
        glVertex2f(scrollbar_x + 2, thumb_y + thumb_height);
        glEnd();
    }
}

void render_console(EditorWindow* win) {
    glColor3f(0, 1, 0);
    
    float line_height = 14;
    float y = 10 - win->content_scroll_y;
    
    for (int i = 0; i < editor->console.line_count; i++) {
        if (y > -line_height && y < win->height - TITLE_BAR_HEIGHT) {
            draw_text(10, y, editor->console.lines[i], 1.2f);
        }
        y += line_height;
    }
    
    win->content_height = editor->console.line_count * line_height + 20;
}

void render_file_browser(EditorWindow* win) {
    glColor3f(0.8f, 0.8f, 0.8f);
    draw_text(10, 10, "Files:", 1.5f);
    draw_text(10, 30, editor->file_browser.current_path, 1.0f);
    
    float y = 50 - win->content_scroll_y;
    float line_height = 16;
    
    for (int i = 0; i < editor->file_browser.file_count; i++) {
        if (y > 0 && y < win->height - TITLE_BAR_HEIGHT) {
            if (i == editor->file_browser.selected_file) {
                glColor4f(0.3f, 0.3f, 0.5f, 0.5f);
                glBegin(GL_QUADS);
                glVertex2f(5, y - 2);
                glVertex2f(win->width - 20, y - 2);
                glVertex2f(win->width - 20, y + line_height - 2);
                glVertex2f(5, y + line_height - 2);
                glEnd();
            }
            
            if (editor->file_browser.is_dir[i]) {
                glColor3f(0.6f, 0.8f, 1.0f);
                draw_text(10, y, "[DIR]", 1.0f);
                draw_text(50, y, editor->file_browser.files[i], 1.0f);
            } else {
                glColor3f(0.9f, 0.9f, 0.9f);
                draw_text(10, y, editor->file_browser.files[i], 1.0f);
            }
        }
        y += line_height;
    }
    
    win->content_height = editor->file_browser.file_count * line_height + 60;
}

void render_toolbar(EditorWindow* win) {
    float btn_width = 100;
    float btn_height = 40;
    float x = 10;
    float y = 10;
    
    // Compile button
    glColor4f(0.2f, 0.5f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + btn_width, y);
    glVertex2f(x + btn_width, y + btn_height);
    glVertex2f(x, y + btn_height);
    glEnd();
    glColor3f(1, 1, 1);
    draw_text(x + 20, y + 15, "COMPILE", 1.5f);
    
    x += btn_width + 10;
    
    // Play/Stop button
    if (editor->engine.is_running) {
        glColor4f(0.8f, 0.2f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + btn_width, y);
        glVertex2f(x + btn_width, y + btn_height);
        glVertex2f(x, y + btn_height);
        glEnd();
        glColor3f(1, 1, 1);
        draw_text(x + 30, y + 15, "STOP", 1.5f);
    } else {
        glColor4f(0.2f, 0.8f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + btn_width, y);
        glVertex2f(x + btn_width, y + btn_height);
        glVertex2f(x, y + btn_height);
        glEnd();
        glColor3f(1, 1, 1);
        draw_text(x + 30, y + 15, "PLAY", 1.5f);
    }
    
    x += btn_width + 10;
    
    // Restart button
    glColor4f(0.8f, 0.8f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + btn_width, y);
    glVertex2f(x + btn_width, y + btn_height);
    glVertex2f(x, y + btn_height);
    glEnd();
    glColor3f(1, 1, 1);
    draw_text(x + 20, y + 15, "RESTART", 1.5f);
}

void render_scene(EditorWindow* win) {
    // Grid
    if (editor->show_grid) {
        glColor4f(0.2f, 0.2f, 0.2f, 0.5f);
        float grid_size = 30;
        
        for (float gx = 0; gx < win->width; gx += grid_size) {
            glBegin(GL_LINES);
            glVertex2f(gx, 0);
            glVertex2f(gx, win->height - TITLE_BAR_HEIGHT);
            glEnd();
        }
        
        for (float gy = 0; gy < win->height - TITLE_BAR_HEIGHT; gy += grid_size) {
            glBegin(GL_LINES);
            glVertex2f(0, gy);
            glVertex2f(win->width, gy);
            glEnd();
        }
    }
    
    // Axis
    float cx = win->width / 2;
    float cy = (win->height - TITLE_BAR_HEIGHT) / 2;
    
    glLineWidth(2.0f);
    glColor3f(1, 0, 0);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + 60, cy);
    glEnd();
    
    glColor3f(0, 1, 0);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx, cy - 60);
    glEnd();
    
    glColor3f(0, 0, 1);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + 42, cy + 42);
    glEnd();
    
    glLineWidth(1.0f);
    
    // Label
    glColor3f(1, 1, 1);
    draw_text(10, 10, "3D Scene View", 1.5f);
    draw_text(10, 30, "Camera: Perspective", 1.2f);
}

void render_properties(EditorWindow* win) {
    glColor3f(0.9f, 0.9f, 0.9f);
    draw_text(10, 10, "Properties", 1.5f);
    
    float y = 40;
    glColor3f(0.7f, 0.7f, 0.7f);
    draw_text(10, y, "Object: Terrain", 1.2f); y += 20;
    draw_text(10, y, "Size: 128x128", 1.2f); y += 20;
    draw_text(10, y, "Height: 2.0", 1.2f); y += 20;
    draw_text(10, y, "Material: Grass", 1.2f); y += 20;
}

void render_code_editor(EditorWindow* win) {
    glColor3f(0.9f, 0.9f, 0.9f);
    draw_text(10, 10, "Code Editor", 1.5f);
    
    glColor3f(0.6f, 0.6f, 0.6f);
    draw_text(10, 30, "continental_ultimate.c", 1.2f);
    
    // Sample code
    glColor3f(0.7f, 0.7f, 0.9f);
    draw_text(10, 60, "void generate_terrain() {", 1.0f);
    draw_text(10, 75, "    for (int y = 0; y < SIZE; y++) {", 1.0f);
    draw_text(10, 90, "        // Generate height", 1.0f);
    draw_text(10, 105, "    }", 1.0f);
    draw_text(10, 120, "}", 1.0f);
}

// ============= INPUT =============

void handle_mouse_down(int x, int y, int button) {
    // Check window buttons first
    for (int i = editor->window_count - 1; i >= 0; i--) {
        EditorWindow* win = &editor->windows[i];
        if (!win->visible) continue;
        
        if (x >= win->x && x <= win->x + win->width &&
            y >= win->y && y <= win->y + TITLE_BAR_HEIGHT) {
            
            // Check window buttons
            float btn_x = win->x + win->width - BUTTON_SIZE - 5;
            float btn_y = win->y + 5;
            
            // Close button
            if (x >= btn_x && x <= btn_x + BUTTON_SIZE &&
                y >= btn_y && y <= btn_y + BUTTON_SIZE) {
                close_window(win);
                return;
            }
            
            // Maximize button
            btn_x -= BUTTON_SIZE + 5;
            if (x >= btn_x && x <= btn_x + BUTTON_SIZE &&
                y >= btn_y && y <= btn_y + BUTTON_SIZE) {
                maximize_window(win);
                return;
            }
            
            // Minimize button
            btn_x -= BUTTON_SIZE + 5;
            if (x >= btn_x && x <= btn_x + BUTTON_SIZE &&
                y >= btn_y && y <= btn_y + BUTTON_SIZE) {
                minimize_window(win);
                return;
            }
            
            // Start moving
            win->moving = 1;
            win->move_offset_x = x - win->x;
            win->move_offset_y = y - win->y;
            
            // Bring to front
            if (i < editor->window_count - 1) {
                EditorWindow temp = *win;
                for (int j = i; j < editor->window_count - 1; j++) {
                    editor->windows[j] = editor->windows[j + 1];
                }
                editor->windows[editor->window_count - 1] = temp;
            }
            
            // Focus
            for (int j = 0; j < editor->window_count; j++) {
                editor->windows[j].focused = (j == editor->window_count - 1);
            }
            
            return;
        }
        
        // Check resize edges
        int edge = get_resize_edge(win, x, y);
        if (edge != 0) {
            win->resizing = 1;
            win->resize_edge = edge;
            win->move_offset_x = x;
            win->move_offset_y = y;
            return;
        }
        
        // Check toolbar buttons
        if (win->type == WINDOW_TOOLBAR && 
            x >= win->x && x <= win->x + win->width &&
            y >= win->y + TITLE_BAR_HEIGHT && y <= win->y + win->height) {
            
            float btn_x = win->x + 10;
            float btn_y = win->y + TITLE_BAR_HEIGHT + 10;
            
            if (y >= btn_y && y <= btn_y + 40) {
                if (x >= btn_x && x <= btn_x + 100) {
                    compile_engine();
                } else if (x >= btn_x + 110 && x <= btn_x + 210) {
                    if (editor->engine.is_running) {
                        stop_engine();
                    } else {
                        start_engine();
                    }
                } else if (x >= btn_x + 220 && x <= btn_x + 320) {
                    stop_engine();
                    start_engine();
                }
            }
        }
    }
}

void handle_mouse_up(int x, int y, int button) {
    for (int i = 0; i < editor->window_count; i++) {
        editor->windows[i].moving = 0;
        editor->windows[i].resizing = 0;
        editor->windows[i].resize_edge = 0;
    }
}

void handle_mouse_motion(int x, int y) {
    editor->mouse_x = x;
    editor->mouse_y = y;
    
    for (int i = 0; i < editor->window_count; i++) {
        EditorWindow* win = &editor->windows[i];
        
        if (win->moving) {
            win->x = x - win->move_offset_x;
            win->y = y - win->move_offset_y;
            
            // Keep on screen
            if (win->x < 0) win->x = 0;
            if (win->y < 0) win->y = 0;
            if (win->x + win->width > WINDOW_WIDTH) win->x = WINDOW_WIDTH - win->width;
            if (win->y + win->height > WINDOW_HEIGHT - 25) win->y = WINDOW_HEIGHT - 25 - win->height;
        }
        else if (win->resizing) {
            float dx = x - win->move_offset_x;
            float dy = y - win->move_offset_y;
            
            switch (win->resize_edge) {
                case 3: // E
                    win->width += dx;
                    break;
                case 5: // S
                    win->height += dy;
                    break;
                case 4: // SE
                    win->width += dx;
                    win->height += dy;
                    break;
                case 7: // W
                    win->x += dx;
                    win->width -= dx;
                    break;
                case 1: // N
                    win->y += dy;
                    win->height -= dy;
                    break;
                case 8: // NW
                    win->x += dx;
                    win->y += dy;
                    win->width -= dx;
                    win->height -= dy;
                    break;
                case 2: // NE
                    win->y += dy;
                    win->width += dx;
                    win->height -= dy;
                    break;
                case 6: // SW
                    win->x += dx;
                    win->width -= dx;
                    win->height += dy;
                    break;
            }
            
            // Enforce minimum size
            if (win->width < win->min_width) win->width = win->min_width;
            if (win->height < win->min_height) win->height = win->min_height;
            
            win->move_offset_x = x;
            win->move_offset_y = y;
        }
    }
}

void handle_scroll(EditorWindow* win, float delta) {
    win->content_scroll_y -= delta * 20;
    if (win->content_scroll_y < 0) win->content_scroll_y = 0;
    if (win->content_scroll_y > win->content_height - win->height + TITLE_BAR_HEIGHT) {
        win->content_scroll_y = win->content_height - win->height + TITLE_BAR_HEIGHT;
        if (win->content_scroll_y < 0) win->content_scroll_y = 0;
    }
}

// ============= MAIN =============

int main() {
    printf("Continental Architect Editor V2\n");
    printf("===============================\n\n");
    
    editor = calloc(1, sizeof(Editor));
    editor->dark_mode = 1;
    editor->show_grid = 1;
    
    init_font();
    
    strcpy(editor->file_browser.current_path, "/home/thebackhand/Projects/handmade-engine");
    refresh_files();
    
    // Create windows
    create_window("Toolbar", 10, 10, 1580, 80, WINDOW_TOOLBAR);
    create_window("Scene View", 250, 100, 800, 500, WINDOW_SCENE);
    create_window("Console", 250, 610, 800, 200, WINDOW_CONSOLE);
    create_window("Files", 10, 100, 230, 400, WINDOW_FILES);
    create_window("Properties", 1060, 100, 300, 400, WINDOW_PROPERTIES);
    create_window("Code", 1060, 510, 530, 300, WINDOW_CODE);
    
    console_add("Editor initialized");
    console_add("Ready to compile and run engine");
    
    // X11/OpenGL setup
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    
    int scr = DefaultScreen(dpy);
    GLint att[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8, None
    };
    
    XVisualInfo* vi = glXChooseVisual(dpy, scr, att);
    if (!vi) return 1;
    
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask;
    
    Window win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Continental Architect Editor V2");
    XFlush(dpy);
    XSync(dpy, False);
    usleep(100000);
    
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    
    glEnable(GL_DEPTH_TEST);
    
    printf("Editor ready\n");
    printf("Click window buttons to min/max/close\n");
    printf("Drag edges to resize\n");
    printf("F5: Compile, F6: Play/Stop\n\n");
    
    // Main loop
    int running = 1;
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double dt = (current_time.tv_sec - last_time.tv_sec) + 
                   (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
        last_time = current_time;
        
        // Events
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                if (key == XK_Escape) running = 0;
                else if (key == XK_F5) compile_engine();
                else if (key == XK_F6) {
                    if (editor->engine.is_running) stop_engine();
                    else start_engine();
                }
            } else if (xev.type == ButtonPress) {
                if (xev.xbutton.button == 4) { // Scroll up
                    for (int i = 0; i < editor->window_count; i++) {
                        EditorWindow* win = &editor->windows[i];
                        if (xev.xbutton.x >= win->x && xev.xbutton.x <= win->x + win->width &&
                            xev.xbutton.y >= win->y && xev.xbutton.y <= win->y + win->height) {
                            handle_scroll(win, 1);
                            break;
                        }
                    }
                } else if (xev.xbutton.button == 5) { // Scroll down
                    for (int i = 0; i < editor->window_count; i++) {
                        EditorWindow* win = &editor->windows[i];
                        if (xev.xbutton.x >= win->x && xev.xbutton.x <= win->x + win->width &&
                            xev.xbutton.y >= win->y && xev.xbutton.y <= win->y + win->height) {
                            handle_scroll(win, -1);
                            break;
                        }
                    }
                } else {
                    handle_mouse_down(xev.xbutton.x, xev.xbutton.y, xev.xbutton.button);
                }
            } else if (xev.type == ButtonRelease) {
                handle_mouse_up(xev.xbutton.x, xev.xbutton.y, xev.xbutton.button);
            } else if (xev.type == MotionNotify) {
                handle_mouse_motion(xev.xmotion.x, xev.xmotion.y);
            }
        }
        
        // Check engine status
        if (editor->engine.is_running) {
            int status;
            pid_t result = waitpid(editor->engine.engine_pid, &status, WNOHANG);
            if (result != 0) {
                editor->engine.is_running = 0;
                console_add("Engine stopped");
            }
        }
        
        // Render
        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Render windows
        for (int i = 0; i < editor->window_count; i++) {
            render_window(&editor->windows[i]);
        }
        
        // Status bar
        glColor4f(0.15f, 0.15f, 0.18f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0, WINDOW_HEIGHT - 25);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - 25);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
        glVertex2f(0, WINDOW_HEIGHT);
        glEnd();
        
        glColor3f(1, 1, 1);
        char status[256];
        snprintf(status, sizeof(status), "FPS: %.0f  Engine: %s  Mouse: %d,%d", 
                1.0/dt, editor->engine.is_running ? "Running" : "Stopped",
                editor->mouse_x, editor->mouse_y);
        draw_text(10, WINDOW_HEIGHT - 17, status, 1.2f);
        
        glDisable(GL_BLEND);
        glXSwapBuffers(dpy, win);
        
        usleep(16666);
    }
    
    if (editor->engine.is_running) stop_engine();
    
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(editor);
    
    return 0;
}