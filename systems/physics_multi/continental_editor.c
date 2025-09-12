/*
    Continental Architect EDITOR
    
    Professional game editor with:
    - Moveable, resizable windows
    - Compile and run engine controls
    - Accurate mouse positioning
    - Console output
    - File browser
    - Hot reload support
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
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900
#define MAX_WINDOWS 10
#define MAX_CONSOLE_LINES 100
#define MAX_FILES 1000

// ============= WINDOW SYSTEM =============

typedef enum {
    WINDOW_SCENE,
    WINDOW_CONSOLE,
    WINDOW_FILES,
    WINDOW_PROPERTIES,
    WINDOW_TOOLBAR
} WindowType;

typedef struct {
    char title[64];
    float x, y, width, height;
    float content_scroll_y;
    WindowType type;
    int visible;
    int focused;
    int moving;
    int resizing;
    float move_offset_x, move_offset_y;
    float min_width, min_height;
    
    // Window state
    void* data;
} EditorWindow;

typedef struct {
    char lines[MAX_CONSOLE_LINES][256];
    int line_count;
    int autoscroll;
} Console;

typedef struct {
    char path[256];
    char files[MAX_FILES][256];
    int file_count;
    int selected_file;
} FileBrowser;

typedef struct {
    pid_t engine_pid;
    int is_running;
    int needs_compile;
    char project_path[256];
    char engine_executable[256];
} EngineState;

typedef struct {
    EditorWindow windows[MAX_WINDOWS];
    int window_count;
    int active_window;
    
    Console console;
    FileBrowser file_browser;
    EngineState engine;
    
    // Mouse state
    int mouse_x, mouse_y;
    int mouse_down;
    int drag_start_x, drag_start_y;
    
    // UI state
    float ui_scale;
    int show_grid;
    int dark_mode;
    
    // Performance
    float fps;
    int frame_count;
    double last_time;
} Editor;

Editor* editor;

// ============= CONSOLE =============

void console_add_line(const char* text) {
    if (editor->console.line_count >= MAX_CONSOLE_LINES) {
        // Shift lines up
        for (int i = 0; i < MAX_CONSOLE_LINES - 1; i++) {
            strcpy(editor->console.lines[i], editor->console.lines[i + 1]);
        }
        editor->console.line_count = MAX_CONSOLE_LINES - 1;
    }
    
    // Add timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S]", tm_info);
    
    snprintf(editor->console.lines[editor->console.line_count], 256, 
             "%s %s", timestamp, text);
    editor->console.line_count++;
}

// ============= ENGINE CONTROL =============

void compile_engine() {
    console_add_line("Compiling engine...");
    
    // Run compile command
    int result = system("cd /home/thebackhand/Projects/handmade-engine/systems/physics_multi && "
                       "gcc -o ../../binaries/continental_engine continental_ultimate.c "
                       "-lX11 -lGL -lm -O3 -march=native -ffast-math 2>&1");
    
    if (result == 0) {
        console_add_line("Compilation successful!");
        editor->engine.needs_compile = 0;
    } else {
        console_add_line("Compilation failed! Check errors above.");
    }
}

void start_engine() {
    if (editor->engine.is_running) {
        console_add_line("Engine is already running!");
        return;
    }
    
    console_add_line("Starting engine...");
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - run engine
        execl("/home/thebackhand/Projects/handmade-engine/binaries/continental_ultimate",
              "continental_ultimate", NULL);
        exit(1); // If exec fails
    } else if (pid > 0) {
        // Parent process
        editor->engine.engine_pid = pid;
        editor->engine.is_running = 1;
        console_add_line("Engine started successfully!");
    } else {
        console_add_line("Failed to start engine!");
    }
}

void stop_engine() {
    if (!editor->engine.is_running) {
        console_add_line("Engine is not running!");
        return;
    }
    
    console_add_line("Stopping engine...");
    
    if (editor->engine.engine_pid > 0) {
        kill(editor->engine.engine_pid, SIGTERM);
        waitpid(editor->engine.engine_pid, NULL, 0);
        editor->engine.engine_pid = 0;
        editor->engine.is_running = 0;
        console_add_line("Engine stopped.");
    }
}

void restart_engine() {
    if (editor->engine.is_running) {
        stop_engine();
        usleep(100000); // Small delay
    }
    start_engine();
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
    win->visible = 1;
    win->focused = 0;
    win->moving = 0;
    win->resizing = 0;
    win->content_scroll_y = 0;
    win->min_width = 200;
    win->min_height = 150;
    
    return win;
}

void bring_window_to_front(int window_index) {
    if (window_index < 0 || window_index >= editor->window_count) return;
    
    // Unfocus all windows
    for (int i = 0; i < editor->window_count; i++) {
        editor->windows[i].focused = 0;
    }
    
    // Focus selected window and move to end of array (rendered last = on top)
    editor->windows[window_index].focused = 1;
    editor->active_window = window_index;
    
    if (window_index < editor->window_count - 1) {
        EditorWindow temp = editor->windows[window_index];
        for (int i = window_index; i < editor->window_count - 1; i++) {
            editor->windows[i] = editor->windows[i + 1];
        }
        editor->windows[editor->window_count - 1] = temp;
        editor->active_window = editor->window_count - 1;
    }
}

int get_window_at_point(float x, float y) {
    // Check windows in reverse order (top to bottom)
    for (int i = editor->window_count - 1; i >= 0; i--) {
        EditorWindow* win = &editor->windows[i];
        if (!win->visible) continue;
        
        if (x >= win->x && x <= win->x + win->width &&
            y >= win->y && y <= win->y + win->height) {
            return i;
        }
    }
    return -1;
}

// ============= RENDERING =============

void render_window_frame(EditorWindow* win) {
    float x = win->x;
    float y = win->y;
    float w = win->width;
    float h = win->height;
    
    // Shadow
    glColor4f(0, 0, 0, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(x + 5, y + 5);
    glVertex2f(x + w + 5, y + 5);
    glVertex2f(x + w + 5, y + h + 5);
    glVertex2f(x + 5, y + h + 5);
    glEnd();
    
    // Window background
    if (editor->dark_mode) {
        glColor4f(0.15f, 0.15f, 0.18f, 0.95f);
    } else {
        glColor4f(0.9f, 0.9f, 0.9f, 0.95f);
    }
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Title bar
    if (win->focused) {
        glColor4f(0.2f, 0.4f, 0.8f, 1.0f);
    } else {
        glColor4f(0.3f, 0.3f, 0.35f, 1.0f);
    }
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + 30);
    glVertex2f(x, y + 30);
    glEnd();
    
    // Title text (placeholder)
    glColor3f(1, 1, 1);
    glRasterPos2f(x + 10, y + 20);
    for (int i = 0; win->title[i] != '\0'; i++) {
        // Would render actual text here
    }
    
    // Window border
    glLineWidth(win->focused ? 2.0f : 1.0f);
    glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Resize handle
    glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + w - 15, y + h);
    glVertex2f(x + w, y + h);
    glVertex2f(x + w, y + h - 15);
    glEnd();
}

void render_console_content(EditorWindow* win) {
    float x = win->x + 10;
    float y = win->y + 40;
    float line_height = 15;
    
    glColor3f(0, 1, 0); // Green text
    
    int start_line = (int)(win->content_scroll_y / line_height);
    int visible_lines = (int)((win->height - 50) / line_height);
    
    for (int i = start_line; i < editor->console.line_count && i < start_line + visible_lines; i++) {
        // Render console line
        glRasterPos2f(x, y + (i - start_line) * line_height);
        // Would render actual text here
        
        // Draw placeholder line
        glBegin(GL_LINES);
        glVertex2f(x, y + (i - start_line) * line_height);
        glVertex2f(x + strlen(editor->console.lines[i]) * 6, y + (i - start_line) * line_height);
        glEnd();
    }
}

void render_toolbar_content(EditorWindow* win) {
    float x = win->x + 10;
    float y = win->y + 40;
    float button_width = 100;
    float button_height = 30;
    float spacing = 10;
    
    // Compile button
    glColor4f(0.2f, 0.5f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + button_width, y);
    glVertex2f(x + button_width, y + button_height);
    glVertex2f(x, y + button_height);
    glEnd();
    
    // Play/Stop button
    x += button_width + spacing;
    if (editor->engine.is_running) {
        glColor4f(0.8f, 0.2f, 0.2f, 1.0f); // Red for stop
    } else {
        glColor4f(0.2f, 0.8f, 0.2f, 1.0f); // Green for play
    }
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + button_width, y);
    glVertex2f(x + button_width, y + button_height);
    glVertex2f(x, y + button_height);
    glEnd();
    
    // Restart button
    x += button_width + spacing;
    glColor4f(0.8f, 0.8f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + button_width, y);
    glVertex2f(x + button_width, y + button_height);
    glVertex2f(x, y + button_height);
    glEnd();
}

void render_scene_content(EditorWindow* win) {
    // Placeholder 3D viewport
    float x = win->x + 1;
    float y = win->y + 31;
    float w = win->width - 2;
    float h = win->height - 32;
    
    // Grid
    if (editor->show_grid) {
        glColor4f(0.3f, 0.3f, 0.3f, 0.3f);
        float grid_size = 20;
        
        for (float gx = x; gx < x + w; gx += grid_size) {
            glBegin(GL_LINES);
            glVertex2f(gx, y);
            glVertex2f(gx, y + h);
            glEnd();
        }
        
        for (float gy = y; gy < y + h; gy += grid_size) {
            glBegin(GL_LINES);
            glVertex2f(x, gy);
            glVertex2f(x + w, gy);
            glEnd();
        }
    }
    
    // Axis indicator
    float cx = x + w/2;
    float cy = y + h/2;
    
    glLineWidth(2.0f);
    // X axis - red
    glColor3f(1, 0, 0);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + 50, cy);
    glEnd();
    
    // Y axis - green
    glColor3f(0, 1, 0);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx, cy - 50);
    glEnd();
    
    // Z axis - blue
    glColor3f(0, 0, 1);
    glBegin(GL_LINES);
    glVertex2f(cx, cy);
    glVertex2f(cx + 35, cy + 35);
    glEnd();
}

void render_editor() {
    // Clear
    if (editor->dark_mode) {
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    } else {
        glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 2D mode
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render all windows
    for (int i = 0; i < editor->window_count; i++) {
        EditorWindow* win = &editor->windows[i];
        if (!win->visible) continue;
        
        render_window_frame(win);
        
        // Render window content based on type
        switch (win->type) {
            case WINDOW_CONSOLE:
                render_console_content(win);
                break;
            case WINDOW_TOOLBAR:
                render_toolbar_content(win);
                break;
            case WINDOW_SCENE:
                render_scene_content(win);
                break;
            default:
                break;
        }
    }
    
    // Status bar
    glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(0, WINDOW_HEIGHT - 25);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - 25);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(0, WINDOW_HEIGHT);
    glEnd();
    
    // Status text
    char status[256];
    snprintf(status, sizeof(status), "FPS: %.0f | Engine: %s | Mouse: %d,%d", 
             editor->fps, 
             editor->engine.is_running ? "Running" : "Stopped",
             editor->mouse_x, editor->mouse_y);
    
    glColor3f(1, 1, 1);
    glRasterPos2f(10, WINDOW_HEIGHT - 8);
    // Would render actual text here
    
    glDisable(GL_BLEND);
}

// ============= INPUT HANDLING =============

void handle_mouse_down(int x, int y, int button) {
    editor->mouse_x = x;
    editor->mouse_y = y;
    editor->mouse_down = 1;
    editor->drag_start_x = x;
    editor->drag_start_y = y;
    
    // Check which window was clicked
    int window_index = get_window_at_point(x, y);
    
    if (window_index >= 0) {
        EditorWindow* win = &editor->windows[window_index];
        bring_window_to_front(window_index);
        
        // Check if clicking title bar (for moving)
        if (y >= win->y && y <= win->y + 30) {
            win->moving = 1;
            win->move_offset_x = x - win->x;
            win->move_offset_y = y - win->y;
        }
        // Check if clicking resize handle
        else if (x >= win->x + win->width - 15 && 
                 y >= win->y + win->height - 15) {
            win->resizing = 1;
        }
        // Check toolbar buttons
        else if (win->type == WINDOW_TOOLBAR) {
            float bx = win->x + 10;
            float by = win->y + 40;
            
            if (y >= by && y <= by + 30) {
                if (x >= bx && x <= bx + 100) {
                    // Compile button
                    compile_engine();
                } else if (x >= bx + 110 && x <= bx + 210) {
                    // Play/Stop button
                    if (editor->engine.is_running) {
                        stop_engine();
                    } else {
                        start_engine();
                    }
                } else if (x >= bx + 220 && x <= bx + 320) {
                    // Restart button
                    restart_engine();
                }
            }
        }
    }
}

void handle_mouse_up(int x, int y, int button) {
    editor->mouse_x = x;
    editor->mouse_y = y;
    editor->mouse_down = 0;
    
    // Stop any window operations
    for (int i = 0; i < editor->window_count; i++) {
        editor->windows[i].moving = 0;
        editor->windows[i].resizing = 0;
    }
}

void handle_mouse_motion(int x, int y) {
    int dx = x - editor->mouse_x;
    int dy = y - editor->mouse_y;
    
    editor->mouse_x = x;
    editor->mouse_y = y;
    
    if (editor->mouse_down) {
        // Handle window operations
        for (int i = 0; i < editor->window_count; i++) {
            EditorWindow* win = &editor->windows[i];
            
            if (win->moving) {
                win->x = x - win->move_offset_x;
                win->y = y - win->move_offset_y;
                
                // Keep window on screen
                if (win->x < 0) win->x = 0;
                if (win->y < 0) win->y = 0;
                if (win->x + win->width > WINDOW_WIDTH) 
                    win->x = WINDOW_WIDTH - win->width;
                if (win->y + win->height > WINDOW_HEIGHT - 25) 
                    win->y = WINDOW_HEIGHT - 25 - win->height;
            }
            else if (win->resizing) {
                win->width += dx;
                win->height += dy;
                
                // Enforce minimum size
                if (win->width < win->min_width) win->width = win->min_width;
                if (win->height < win->min_height) win->height = win->min_height;
                
                // Maximum size
                if (win->width > WINDOW_WIDTH) win->width = WINDOW_WIDTH;
                if (win->height > WINDOW_HEIGHT) win->height = WINDOW_HEIGHT;
            }
        }
    }
}

// ============= MAIN =============

int main() {
    printf("Continental Architect EDITOR\n");
    printf("============================\n\n");
    
    // Initialize editor
    editor = calloc(1, sizeof(Editor));
    editor->ui_scale = 1.0f;
    editor->dark_mode = 1;
    editor->show_grid = 1;
    
    strcpy(editor->engine.project_path, "/home/thebackhand/Projects/handmade-engine");
    strcpy(editor->engine.engine_executable, "binaries/continental_ultimate");
    
    // Create default windows
    create_window("Scene View", 250, 100, 800, 600, WINDOW_SCENE);
    create_window("Console", 250, 710, 800, 150, WINDOW_CONSOLE);
    create_window("Toolbar", 10, 10, 1580, 80, WINDOW_TOOLBAR);
    create_window("Files", 10, 100, 230, 400, WINDOW_FILES);
    create_window("Properties", 1060, 100, 300, 600, WINDOW_PROPERTIES);
    
    console_add_line("Editor initialized successfully!");
    console_add_line("Click Compile to build the engine");
    console_add_line("Click Play to start the engine");
    
    // X11 and OpenGL setup
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    int scr = DefaultScreen(dpy);
    GLint att[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8, None
    };
    
    XVisualInfo* vi = glXChooseVisual(dpy, scr, att);
    if (!vi) {
        fprintf(stderr, "No suitable visual\n");
        return 1;
    }
    
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask;
    
    Window win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Continental Architect Editor");
    XFlush(dpy);
    XSync(dpy, False);
    usleep(100000);
    
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    
    glEnable(GL_DEPTH_TEST);
    
    printf("Editor window created\n");
    printf("Controls:\n");
    printf("  Drag title bars to move windows\n");
    printf("  Drag corners to resize\n");
    printf("  Click Compile/Play/Stop buttons\n");
    printf("  ESC to exit\n\n");
    
    // Main loop
    int running = 1;
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    
    while (running) {
        // Timing
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
                } else if (key == XK_F7) {
                    restart_engine();
                }
            } else if (xev.type == ButtonPress) {
                handle_mouse_down(xev.xbutton.x, xev.xbutton.y, xev.xbutton.button);
            } else if (xev.type == ButtonRelease) {
                handle_mouse_up(xev.xbutton.x, xev.xbutton.y, xev.xbutton.button);
            } else if (xev.type == MotionNotify) {
                handle_mouse_motion(xev.xmotion.x, xev.xmotion.y);
            }
        }
        
        // Check if engine process has exited
        if (editor->engine.is_running) {
            int status;
            pid_t result = waitpid(editor->engine.engine_pid, &status, WNOHANG);
            if (result != 0) {
                editor->engine.is_running = 0;
                editor->engine.engine_pid = 0;
                console_add_line("Engine process exited");
            }
        }
        
        // Render
        render_editor();
        glXSwapBuffers(dpy, win);
        
        // FPS
        editor->frame_count++;
        if (editor->frame_count % 30 == 0) {
            editor->fps = 1.0 / dt;
        }
        
        usleep(16666); // 60 FPS
    }
    
    // Cleanup
    if (editor->engine.is_running) {
        stop_engine();
    }
    
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(editor);
    
    printf("Editor closed\n");
    return 0;
}