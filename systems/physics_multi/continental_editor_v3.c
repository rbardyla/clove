// Continental Architect Editor V3 - Fixed Mouse Calibration
// Zero dependencies, handmade from scratch

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <math.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define TITLE_BAR_HEIGHT 30
#define BUTTON_SIZE 20
#define MAX_WINDOWS 10
#define MAX_CONSOLE_LINES 100
#define MAX_FILES 100

typedef enum {
    WINDOW_CONSOLE,
    WINDOW_FILES,
    WINDOW_TOOLBAR,
    WINDOW_SCENE,
    WINDOW_PROPERTIES,
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
    WindowType type;
    int visible;
    int focused;
    int moving;
    int resizing;
    int resize_edge; // 0=none, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW
    float move_offset_x, move_offset_y;
    float content_scroll_y;
    float content_height;
    WindowState state;
} EditorWindow;

typedef struct {
    char lines[MAX_CONSOLE_LINES][256];
    int line_count;
} Console;

typedef struct {
    char names[MAX_FILES][256];
    int is_dir[MAX_FILES];
    int count;
} FileBrowser;

typedef struct {
    pid_t engine_pid;
    int is_running;
} EngineState;

typedef struct {
    Display* display;
    Window window;
    GLXContext context;
    EditorWindow windows[MAX_WINDOWS];
    int window_count;
    Console console;
    FileBrowser files;
    EngineState engine;
    int mouse_x, mouse_y;
    int debug_mouse;  // Debug mode for mouse calibration
    unsigned char font_data[128][8];
} Editor;

static Editor* editor = NULL;

// ============= FONT =============

void init_font() {
    // Simple 8x8 bitmap font - numbers and letters
    unsigned char font_bitmaps[][8] = {
        // '0'
        {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
        // '1'
        {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
        // Space ' '
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ... more characters would go here
    };
    
    // Initialize with basic ASCII set
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 8; j++) {
            if (i >= '0' && i <= '9') {
                editor->font_data[i][j] = font_bitmaps[i - '0'][j];
            } else if (i == ' ') {
                editor->font_data[i][j] = 0x00;
            } else {
                // Default pattern for undefined chars
                editor->font_data[i][j] = 0xFF;
            }
        }
    }
    
    // Add letters A-Z
    unsigned char letters[][8] = {
        // A
        {0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        // B
        {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
        // C
        {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
        // D
        {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
        // E
        {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00},
        // F
        {0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00},
        // G
        {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
        // H
        {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        // I
        {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00},
        // More letters...
    };
    
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 8; j++) {
            editor->font_data['A' + i][j] = letters[i][j];
            editor->font_data['a' + i][j] = letters[i][j];
        }
    }
    
    // Special characters
    editor->font_data[':'][0] = 0x00;
    editor->font_data[':'][1] = 0x18;
    editor->font_data[':'][2] = 0x18;
    editor->font_data[':'][3] = 0x00;
    editor->font_data[':'][4] = 0x00;
    editor->font_data[':'][5] = 0x18;
    editor->font_data[':'][6] = 0x18;
    editor->font_data[':'][7] = 0x00;
    
    editor->font_data['/'][0] = 0x06;
    editor->font_data['/'][1] = 0x0C;
    editor->font_data['/'][2] = 0x18;
    editor->font_data['/'][3] = 0x30;
    editor->font_data['/'][4] = 0x60;
    editor->font_data['/'][5] = 0xC0;
    editor->font_data['/'][6] = 0x80;
    editor->font_data['/'][7] = 0x00;
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
    editor->files.count = 0;
    
    DIR* dir = opendir(".");
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && editor->files.count < MAX_FILES) {
        if (entry->d_name[0] == '.') continue;
        
        strcpy(editor->files.names[editor->files.count], entry->d_name);
        editor->files.is_dir[editor->files.count] = (entry->d_type == DT_DIR);
        editor->files.count++;
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
        // Child process
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
    kill(editor->engine.engine_pid, SIGTERM);
    waitpid(editor->engine.engine_pid, NULL, 0);
    editor->engine.is_running = 0;
    console_add("Engine stopped");
}

// ============= WINDOW MANAGEMENT =============

EditorWindow* create_window(const char* title, float x, float y, float width, float height, WindowType type) {
    if (editor->window_count >= MAX_WINDOWS) return NULL;
    
    EditorWindow* win = &editor->windows[editor->window_count++];
    strcpy(win->title, title);
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->type = type;
    win->visible = 1;
    win->focused = 0;
    win->moving = 0;
    win->resizing = 0;
    win->resize_edge = 0;
    win->content_scroll_y = 0;
    win->content_height = 500;
    win->state = WIN_STATE_NORMAL;
    
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
    int edge_size = 8;  // Increased from 5 for better grab accuracy
    int on_left = (x >= win->x - edge_size && x <= win->x + edge_size);
    int on_right = (x >= win->x + win->width - edge_size && x <= win->x + win->width + edge_size);
    int on_top = (y >= win->y - edge_size && y <= win->y + edge_size);
    int on_bottom = (y >= win->y + win->height - edge_size && y <= win->y + win->height + edge_size);
    
    if (on_top && on_left) return 8;  // NW
    if (on_top && on_right) return 2;  // NE
    if (on_bottom && on_left) return 6;  // SW
    if (on_bottom && on_right) return 4;  // SE
    if (on_top) return 1;  // N
    if (on_right) return 3;  // E
    if (on_bottom) return 5;  // S
    if (on_left) return 7;  // W
    
    return 0;
}

void render_window(EditorWindow* win) {
    if (!win->visible) return;
    
    float x = win->x;
    float y = win->y;
    float w = win->width;
    float h = win->height;
    
    // Title bar
    if (win->focused) {
        glColor4f(0.2f, 0.3f, 0.5f, 1.0f);
    } else {
        glColor4f(0.15f, 0.15f, 0.2f, 1.0f);
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
    
    // Window control buttons
    float btn_x = x + w - BUTTON_SIZE - 5;
    float btn_y = y + 5;
    
    // Close button
    glColor4f(0.8f, 0.2f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(btn_x, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y);
    glVertex2f(btn_x + BUTTON_SIZE, btn_y + BUTTON_SIZE);
    glVertex2f(btn_x, btn_y + BUTTON_SIZE);
    glEnd();
    
    // X for close
    glColor3f(1, 1, 1);
    glLineWidth(2);
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
    
    float y = 10;
    int start = editor->console.line_count - 20;
    if (start < 0) start = 0;
    
    for (int i = start; i < editor->console.line_count; i++) {
        draw_text(10, y, editor->console.lines[i], 1.2f);
        y += 15;
    }
    
    win->content_height = editor->console.line_count * 15 + 20;
}

void render_file_browser(EditorWindow* win) {
    glColor3f(0.8f, 0.8f, 0.8f);
    draw_text(10, 10, "Files:", 1.5f);
    
    float y = 35;
    for (int i = 0; i < editor->files.count && i < 20; i++) {
        if (editor->files.is_dir[i]) {
            glColor3f(0.5f, 0.5f, 1.0f);
            draw_text(10, y, "[DIR]", 1.2f);
            draw_text(60, y, editor->files.names[i], 1.2f);
        } else {
            glColor3f(0.8f, 0.8f, 0.8f);
            draw_text(10, y, editor->files.names[i], 1.2f);
        }
        y += 18;
    }
    
    win->content_height = editor->files.count * 18 + 40;
}

void render_toolbar(EditorWindow* win) {
    float x = 10;
    float y = 10;
    
    // Compile button
    glColor4f(0.2f, 0.4f, 0.6f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 80, y);
    glVertex2f(x + 80, y + 30);
    glVertex2f(x, y + 30);
    glEnd();
    
    glColor3f(1, 1, 1);
    draw_text(x + 10, y + 10, "COMPILE", 1.2f);
    
    x += 90;
    
    // Play/Stop button
    if (editor->engine.is_running) {
        glColor4f(0.6f, 0.2f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + 60, y);
        glVertex2f(x + 60, y + 30);
        glVertex2f(x, y + 30);
        glEnd();
        
        glColor3f(1, 1, 1);
        draw_text(x + 15, y + 10, "STOP", 1.2f);
    } else {
        glColor4f(0.2f, 0.6f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + 60, y);
        glVertex2f(x + 60, y + 30);
        glVertex2f(x, y + 30);
        glEnd();
        
        glColor3f(1, 1, 1);
        draw_text(x + 15, y + 10, "PLAY", 1.2f);
    }
    
    x += 70;
    
    // Restart button
    glColor4f(0.6f, 0.6f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 80, y);
    glVertex2f(x + 80, y + 30);
    glVertex2f(x, y + 30);
    glEnd();
    
    glColor3f(1, 1, 1);
    draw_text(x + 10, y + 10, "RESTART", 1.2f);
}

void render_scene(EditorWindow* win) {
    glColor3f(0.5f, 0.5f, 0.5f);
    
    // Draw grid
    float grid_size = 20;
    glBegin(GL_LINES);
    for (float x = 0; x < win->width; x += grid_size) {
        glVertex2f(x, 0);
        glVertex2f(x, win->height - TITLE_BAR_HEIGHT);
    }
    for (float y = 0; y < win->height - TITLE_BAR_HEIGHT; y += grid_size) {
        glVertex2f(0, y);
        glVertex2f(win->width, y);
    }
    glEnd();
    
    // Scene label
    glColor3f(1, 1, 1);
    draw_text(10, 10, "3D Scene View", 1.5f);
    draw_text(10, 30, "Camera: Perspective", 1.2f);
}

void render_properties(EditorWindow* win) {
    glColor3f(0.8f, 0.8f, 0.8f);
    draw_text(10, 10, "Properties", 1.5f);
    
    glColor3f(0.6f, 0.6f, 0.6f);
    draw_text(10, 35, "Object: Terrain", 1.2f);
    draw_text(10, 55, "Size: 128x128", 1.2f);
    draw_text(10, 75, "Height: 2.0", 1.2f);
    draw_text(10, 95, "Material: Grass", 1.2f);
}

void render_code_editor(EditorWindow* win) {
    glColor3f(0.2f, 0.8f, 0.2f);
    draw_text(10, 10, "Code Editor", 1.5f);
    
    glColor3f(0.5f, 0.5f, 0.5f);
    draw_text(10, 35, "continental_ultimate.c", 1.2f);
    
    glColor3f(0.7f, 0.7f, 0.7f);
    draw_text(10, 60, "void generate_terrain() {", 1.2f);
    draw_text(10, 80, "    for (int y = 0; y < SIZE; y++) {", 1.2f);
    draw_text(10, 100, "        // Generate height", 1.2f);
    draw_text(10, 120, "    }", 1.2f);
    draw_text(10, 140, "}", 1.2f);
}

// ============= INPUT =============

void handle_mouse_down(int x, int y, int button) {
    // Debug output mouse coordinates
    if (editor->debug_mouse) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Mouse down at: %d, %d (button %d)", x, y, button);
        console_add(msg);
    }
    
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
            
            // Start window drag
            win->moving = 1;
            win->move_offset_x = x - win->x;
            win->move_offset_y = y - win->y;
            
            // Focus this window
            for (int j = 0; j < editor->window_count; j++) {
                editor->windows[j].focused = 0;
            }
            win->focused = 1;
            
            // Move to front
            if (i < editor->window_count - 1) {
                EditorWindow temp = *win;
                memmove(&editor->windows[i], &editor->windows[i + 1],
                       (editor->window_count - i - 1) * sizeof(EditorWindow));
                editor->windows[editor->window_count - 1] = temp;
            }
            return;
        }
        
        // Check for resize
        int edge = get_resize_edge(win, x, y);
        if (edge != 0) {
            win->resizing = 1;
            win->resize_edge = edge;
            win->move_offset_x = x;
            win->move_offset_y = y;
            
            // Focus window
            for (int j = 0; j < editor->window_count; j++) {
                editor->windows[j].focused = 0;
            }
            win->focused = 1;
            return;
        }
        
        // Check toolbar buttons
        if (win->type == WINDOW_TOOLBAR && 
            x >= win->x && x <= win->x + win->width &&
            y >= win->y + TITLE_BAR_HEIGHT && y <= win->y + win->height) {
            
            float rel_x = x - win->x - 10;
            float rel_y = y - win->y - TITLE_BAR_HEIGHT - 10;
            
            if (rel_y >= 0 && rel_y <= 30) {
                if (rel_x >= 0 && rel_x <= 80) {
                    compile_engine();
                } else if (rel_x >= 90 && rel_x <= 150) {
                    if (editor->engine.is_running) {
                        stop_engine();
                    } else {
                        start_engine();
                    }
                } else if (rel_x >= 160 && rel_x <= 240) {
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
        
        if (win->resizing) {
            float dx = x - win->move_offset_x;
            float dy = y - win->move_offset_y;
            
            switch (win->resize_edge) {
                case 1: // N
                    win->y += dy;
                    win->height -= dy;
                    break;
                case 2: // NE
                    win->y += dy;
                    win->height -= dy;
                    win->width += dx;
                    break;
                case 3: // E
                    win->width += dx;
                    break;
                case 4: // SE
                    win->width += dx;
                    win->height += dy;
                    break;
                case 5: // S
                    win->height += dy;
                    break;
                case 6: // SW
                    win->x += dx;
                    win->width -= dx;
                    win->height += dy;
                    break;
                case 7: // W
                    win->x += dx;
                    win->width -= dx;
                    break;
                case 8: // NW
                    win->x += dx;
                    win->width -= dx;
                    win->y += dy;
                    win->height -= dy;
                    break;
            }
            
            // Minimum size
            if (win->width < 150) win->width = 150;
            if (win->height < 100) win->height = 100;
            
            win->move_offset_x = x;
            win->move_offset_y = y;
        }
    }
}

void handle_scroll(int x, int y, int direction) {
    for (int i = editor->window_count - 1; i >= 0; i--) {
        EditorWindow* win = &editor->windows[i];
        if (!win->visible) continue;
        
        if (x >= win->x && x <= win->x + win->width &&
            y >= win->y && y <= win->y + win->height) {
            
            win->content_scroll_y += direction * 20;
            if (win->content_scroll_y < 0) win->content_scroll_y = 0;
            
            float max_scroll = win->content_height - (win->height - TITLE_BAR_HEIGHT);
            if (max_scroll < 0) max_scroll = 0;
            if (win->content_scroll_y > max_scroll) win->content_scroll_y = max_scroll;
            break;
        }
    }
}

// ============= MAIN =============

int main() {
    editor = calloc(1, sizeof(Editor));
    
    // Init X11 and OpenGL
    editor->display = XOpenDisplay(NULL);
    if (!editor->display) {
        printf("Cannot open display\n");
        return 1;
    }
    
    int screen = DefaultScreen(editor->display);
    Window root = RootWindow(editor->display, screen);
    
    GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
    XVisualInfo* vi = glXChooseVisual(editor->display, 0, att);
    
    if (!vi) {
        printf("No appropriate visual found\n");
        return 1;
    }
    
    Colormap cmap = XCreateColormap(editor->display, root, vi->visual, AllocNone);
    
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask;
    
    editor->window = XCreateWindow(editor->display, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                                  0, vi->depth, InputOutput, vi->visual,
                                  CWColormap | CWEventMask, &swa);
    
    XMapWindow(editor->display, editor->window);
    XStoreName(editor->display, editor->window, "Continental Architect Editor V3 - Fixed Mouse");
    
    editor->context = glXCreateContext(editor->display, vi, NULL, GL_TRUE);
    glXMakeCurrent(editor->display, editor->window, editor->context);
    
    // Initialize
    init_font();
    refresh_files();
    
    // Create default windows
    create_window("Console", 10, 450, 400, 300, WINDOW_CONSOLE);
    create_window("Files", 420, 450, 350, 300, WINDOW_FILES);
    create_window("Toolbar", 10, 10, 350, 60, WINDOW_TOOLBAR);
    create_window("Scene", 370, 10, 500, 430, WINDOW_SCENE);
    create_window("Properties", 880, 10, 300, 250, WINDOW_PROPERTIES);
    create_window("Code", 780, 270, 400, 480, WINDOW_CODE);
    
    console_add("Editor initialized");
    console_add("Continental Architect Editor V3");
    console_add("===============================");
    console_add("Fixed mouse calibration");
    console_add("Press D to toggle debug mode");
    console_add("Click window buttons to min/max/close");
    console_add("Drag edges to resize");
    console_add("F5: Compile, F6: Play/Stop");
    
    // Main loop
    int running = 1;
    while (running) {
        XEvent xev;
        
        while (XPending(editor->display)) {
            XNextEvent(editor->display, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                if (key == XK_Escape) {
                    running = 0;
                } else if (key == XK_F5) {
                    compile_engine();
                } else if (key == XK_F6) {
                    if (editor->engine.is_running) {
                        stop_engine();
                    } else {
                        start_engine();
                    }
                } else if (key == XK_d || key == XK_D) {
                    editor->debug_mouse = !editor->debug_mouse;
                    console_add(editor->debug_mouse ? "Debug mode ON" : "Debug mode OFF");
                }
            } else if (xev.type == ButtonPress) {
                if (xev.xbutton.button == 4 || xev.xbutton.button == 5) {
                    handle_scroll(xev.xbutton.x, xev.xbutton.y, 
                                xev.xbutton.button == 4 ? -1 : 1);
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
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
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
        glColor4f(0.1f, 0.1f, 0.15f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0, WINDOW_HEIGHT - 25);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - 25);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
        glVertex2f(0, WINDOW_HEIGHT);
        glEnd();
        
        glColor3f(0.8f, 0.8f, 0.8f);
        char status[256];
        snprintf(status, sizeof(status), "FPS: %.0f  Engine: %s  Mouse: %d,%d", 
                60.0f, editor->engine.is_running ? "Running" : "Stopped",
                editor->mouse_x, editor->mouse_y);
        draw_text(10, WINDOW_HEIGHT - 17, status, 1.2f);
        
        // Draw mouse crosshair in debug mode
        if (editor->debug_mouse) {
            glColor3f(1, 0, 0);
            glLineWidth(1);
            glBegin(GL_LINES);
            // Horizontal line
            glVertex2f(editor->mouse_x - 10, editor->mouse_y);
            glVertex2f(editor->mouse_x + 10, editor->mouse_y);
            // Vertical line
            glVertex2f(editor->mouse_x, editor->mouse_y - 10);
            glVertex2f(editor->mouse_x, editor->mouse_y + 10);
            glEnd();
        }
        
        glXSwapBuffers(editor->display, editor->window);
        usleep(16666); // ~60 FPS
    }
    
    // Cleanup
    if (editor->engine.is_running) {
        stop_engine();
    }
    
    glXMakeCurrent(editor->display, None, NULL);
    glXDestroyContext(editor->display, editor->context);
    XDestroyWindow(editor->display, editor->window);
    XCloseDisplay(editor->display);
    
    free(editor);
    return 0;
}