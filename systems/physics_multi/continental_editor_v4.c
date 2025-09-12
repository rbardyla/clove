// Continental Architect Editor V4 - Improved Text Rendering
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
#define MAX_TEXT_BUFFER 65536
#define MAX_FILENAME 256

typedef enum {
    WINDOW_CONSOLE,
    WINDOW_FILES,
    WINDOW_TOOLBAR,
    WINDOW_SCENE,
    WINDOW_PROPERTIES,
    WINDOW_CODE
} WindowType;

typedef struct {
    char* text;
    int size;
    int used;
    int cursor_pos;
    int selection_start;
    int selection_end;
    char filename[MAX_FILENAME];
    int dirty;
    float scroll_x, scroll_y;
} TextBuffer;

typedef enum {
    WIN_STATE_NORMAL,
    WIN_STATE_MINIMIZED,
    WIN_STATE_MAXIMIZED
} WindowState;

typedef struct {
    char title[64];
    float x, y, width, height;
    float saved_x, saved_y, saved_width, saved_height;
    WindowType type;
    int visible;
    int focused;
    int moving;
    int resizing;
    int resize_edge;
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
    int selected_file;
    float scroll_y;
} FileBrowser;

typedef struct {
    char input_buffer[256];
    int input_cursor;
    int input_active;
} ConsoleInput;

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
    ConsoleInput console_input;
    FileBrowser files;
    EngineState engine;
    TextBuffer code_buffer;
    int mouse_x, mouse_y;
    int debug_mouse;
    int shift_held;
    int ctrl_held;
    unsigned char font_data[256][8];  // Extended to full ASCII
} Editor;

// Static allocation - no malloc needed
static Editor editor_instance;
static Editor* editor = &editor_instance;

// Static text buffer allocation
static char global_text_buffer[MAX_TEXT_BUFFER];

// ============= FORWARD DECLARATIONS =============
void console_add(const char* text);
void compile_engine();
void start_engine();
void stop_engine();
void refresh_files();
void init_text_buffer(TextBuffer* buffer);
void load_file(TextBuffer* buffer, const char* filename);
void save_file(TextBuffer* buffer, const char* filename);
void console_execute_command(const char* command);
void render_console(EditorWindow* win);
void render_file_browser(EditorWindow* win);
void render_toolbar(EditorWindow* win);
void render_scene(EditorWindow* win);
void render_properties(EditorWindow* win);
void render_code_editor(EditorWindow* win);

// ============= COMPLETE BITMAP FONT =============

void init_font() {
    // Clear all font data first
    memset(editor->font_data, 0, sizeof(editor->font_data));
    
    // Complete 8x8 bitmap font for ASCII 32-126
    unsigned char complete_font[95][8] = {
        // Space (32)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ! (33)
        {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
        // " (34)
        {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
        // # (35)
        {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},
        // $ (36)
        {0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00},
        // % (37)
        {0x60, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x06, 0x00},
        // & (38)
        {0x38, 0x6C, 0x38, 0x70, 0xDE, 0xCC, 0x76, 0x00},
        // ' (39)
        {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ( (40)
        {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
        // ) (41)
        {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
        // * (42)
        {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
        // + (43)
        {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
        // , (44)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
        // - (45)
        {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
        // . (46)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
        // / (47)
        {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00},
        // 0 (48)
        {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
        // 1 (49)
        {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
        // 2 (50)
        {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
        // 3 (51)
        {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
        // 4 (52)
        {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
        // 5 (53)
        {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
        // 6 (54)
        {0x3C, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
        // 7 (55)
        {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
        // 8 (56)
        {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
        // 9 (57)
        {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x3C, 0x00},
        // : (58)
        {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
        // ; (59)
        {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30},
        // < (60)
        {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
        // = (61)
        {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
        // > (62)
        {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00},
        // ? (63)
        {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
        // @ (64)
        {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x66, 0x3C, 0x00},
        // A (65)
        {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
        // B (66)
        {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
        // C (67)
        {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
        // D (68)
        {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
        // E (69)
        {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
        // F (70)
        {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
        // G (71)
        {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00},
        // H (72)
        {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        // I (73)
        {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
        // J (74)
        {0x3E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
        // K (75)
        {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
        // L (76)
        {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
        // M (77)
        {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
        // N (78)
        {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
        // O (79)
        {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
        // P (80)
        {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
        // Q (81)
        {0x3C, 0x66, 0x66, 0x66, 0x66, 0x6C, 0x36, 0x00},
        // R (82)
        {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00},
        // S (83)
        {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
        // T (84)
        {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
        // U (85)
        {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
        // V (86)
        {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
        // W (87)
        {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
        // X (88)
        {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
        // Y (89)
        {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
        // Z (90)
        {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
        // [ (91)
        {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
        // \ (92)
        {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00},
        // ] (93)
        {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
        // ^ (94)
        {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},
        // _ (95)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00},
        // ` (96)
        {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00},
        // a (97)
        {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
        // b (98)
        {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
        // c (99)
        {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00},
        // d (100)
        {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
        // e (101)
        {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
        // f (102)
        {0x1C, 0x36, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x00},
        // g (103)
        {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C},
        // h (104)
        {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
        // i (105)
        {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
        // j (106)
        {0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00},
        // k (107)
        {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
        // l (108)
        {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        // m (109)
        {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
        // n (110)
        {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
        // o (111)
        {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
        // p (112)
        {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
        // q (113)
        {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06},
        // r (114)
        {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
        // s (115)
        {0x00, 0x00, 0x3C, 0x60, 0x3C, 0x06, 0x7C, 0x00},
        // t (116)
        {0x30, 0x30, 0x7C, 0x30, 0x30, 0x36, 0x1C, 0x00},
        // u (117)
        {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
        // v (118)
        {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
        // w (119)
        {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00},
        // x (120)
        {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
        // y (121)
        {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C},
        // z (122)
        {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
        // { (123)
        {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00},
        // | (124)
        {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
        // } (125)
        {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00},
        // ~ (126)
        {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    };
    
    // Copy font data to editor structure
    for (int i = 0; i < 95; i++) {
        for (int j = 0; j < 8; j++) {
            editor->font_data[32 + i][j] = complete_font[i][j];
        }
    }
}

void draw_char(float x, float y, char c, float scale) {
    if (c < 32 || c > 126) return;  // Only printable ASCII
    
    unsigned char* bitmap = editor->font_data[(unsigned char)c];
    
    // Use quads instead of points for better quality
    glBegin(GL_QUADS);
    for (int row = 0; row < 8; row++) {
        unsigned char line = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if (line & (1 << (7 - col))) {
                float px = x + col * scale;
                float py = y + row * scale;
                glVertex2f(px, py);
                glVertex2f(px + scale, py);
                glVertex2f(px + scale, py + scale);
                glVertex2f(px, py + scale);
            }
        }
    }
    glEnd();
}

void draw_text(float x, float y, const char* text, float scale) {
    float cursor_x = x;
    float char_width = 8 * scale;
    
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(cursor_x, y, text[i], scale);
        cursor_x += char_width;
    }
}

// ============= TEXT BUFFER MANAGEMENT =============

void init_text_buffer(TextBuffer* buffer) {
    // Use static global buffer - no malloc
    buffer->text = global_text_buffer;
    buffer->size = MAX_TEXT_BUFFER;
    buffer->used = 0;
    buffer->cursor_pos = 0;
    buffer->selection_start = -1;
    buffer->selection_end = -1;
    buffer->filename[0] = '\0';
    buffer->dirty = 0;
    buffer->scroll_x = 0;
    buffer->scroll_y = 0;
    buffer->text[0] = '\0';
}

void text_buffer_insert(TextBuffer* buffer, char c) {
    if (buffer->used >= buffer->size - 1) return;
    
    // Clear selection if exists
    if (buffer->selection_start >= 0) {
        int sel_start = buffer->selection_start < buffer->selection_end ? 
                       buffer->selection_start : buffer->selection_end;
        int sel_end = buffer->selection_start < buffer->selection_end ? 
                     buffer->selection_end : buffer->selection_start;
        
        int sel_len = sel_end - sel_start;
        memmove(buffer->text + sel_start, buffer->text + sel_end, 
                buffer->used - sel_end + 1);
        buffer->used -= sel_len;
        buffer->cursor_pos = sel_start;
        buffer->selection_start = buffer->selection_end = -1;
    }
    
    // Insert character at cursor
    memmove(buffer->text + buffer->cursor_pos + 1, 
            buffer->text + buffer->cursor_pos, 
            buffer->used - buffer->cursor_pos + 1);
    buffer->text[buffer->cursor_pos] = c;
    buffer->used++;
    buffer->cursor_pos++;
    buffer->dirty = 1;
}

void text_buffer_backspace(TextBuffer* buffer) {
    if (buffer->selection_start >= 0) {
        text_buffer_insert(buffer, 0);  // Delete selection
        return;
    }
    
    if (buffer->cursor_pos > 0) {
        memmove(buffer->text + buffer->cursor_pos - 1,
                buffer->text + buffer->cursor_pos,
                buffer->used - buffer->cursor_pos + 1);
        buffer->used--;
        buffer->cursor_pos--;
        buffer->dirty = 1;
    }
}

void text_buffer_move_cursor(TextBuffer* buffer, int delta, int select) {
    int new_pos = buffer->cursor_pos + delta;
    if (new_pos < 0) new_pos = 0;
    if (new_pos > buffer->used) new_pos = buffer->used;
    
    if (select) {
        if (buffer->selection_start < 0) {
            buffer->selection_start = buffer->cursor_pos;
        }
        buffer->selection_end = new_pos;
    } else {
        buffer->selection_start = buffer->selection_end = -1;
    }
    
    buffer->cursor_pos = new_pos;
}

void text_buffer_load_file(TextBuffer* buffer, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to open file: %s", filename);
        console_add(msg);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size >= buffer->size) {
        console_add("File too large for buffer");
        fclose(file);
        return;
    }
    
    buffer->used = fread(buffer->text, 1, file_size, file);
    buffer->text[buffer->used] = '\0';
    buffer->cursor_pos = 0;
    buffer->selection_start = buffer->selection_end = -1;
    strncpy(buffer->filename, filename, MAX_FILENAME - 1);
    buffer->filename[MAX_FILENAME - 1] = '\0';
    buffer->dirty = 0;
    
    fclose(file);
    
    char msg[512];
    snprintf(msg, sizeof(msg), "Loaded file: %s (%d bytes)", filename, buffer->used);
    console_add(msg);
}

void text_buffer_save_file(TextBuffer* buffer) {
    if (buffer->filename[0] == '\0') {
        console_add("No filename specified");
        return;
    }
    
    FILE* file = fopen(buffer->filename, "wb");
    if (!file) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to save file: %s", buffer->filename);
        console_add(msg);
        return;
    }
    
    fwrite(buffer->text, 1, buffer->used, file);
    fclose(file);
    buffer->dirty = 0;
    
    char msg[512];
    snprintf(msg, sizeof(msg), "Saved file: %s (%d bytes)", buffer->filename, buffer->used);
    console_add(msg);
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

void console_execute_command(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    char msg[512];
    snprintf(msg, sizeof(msg), "> %s", cmd);
    console_add(msg);
    
    if (strcmp(cmd, "help") == 0) {
        console_add("Available commands:");
        console_add("  help - Show this help");
        console_add("  clear - Clear console");
        console_add("  compile - Compile engine");
        console_add("  run - Start engine");
        console_add("  stop - Stop engine");
        console_add("  files - Refresh file list");
    } else if (strcmp(cmd, "clear") == 0) {
        editor->console.line_count = 0;
    } else if (strcmp(cmd, "compile") == 0) {
        compile_engine();
    } else if (strcmp(cmd, "run") == 0) {
        start_engine();
    } else if (strcmp(cmd, "stop") == 0) {
        stop_engine();
    } else if (strcmp(cmd, "files") == 0) {
        refresh_files();
        console_add("File list refreshed");
    } else {
        console_add("Unknown command. Type 'help' for available commands.");
    }
}

// ============= FILE BROWSER =============

void refresh_files() {
    editor->files.count = 0;
    editor->files.selected_file = -1;
    
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

void file_browser_select(int index) {
    if (index < 0 || index >= editor->files.count) return;
    
    editor->files.selected_file = index;
    
    if (!editor->files.is_dir[index]) {
        // Load file into code editor
        char* filename = editor->files.names[index];
        text_buffer_load_file(&editor->code_buffer, filename);
        
        // Focus code window
        for (int i = 0; i < editor->window_count; i++) {
            if (editor->windows[i].type == WINDOW_CODE) {
                for (int j = 0; j < editor->window_count; j++) {
                    editor->windows[j].focused = 0;
                }
                editor->windows[i].focused = 1;
                editor->windows[i].visible = 1;
                break;
            }
        }
    }
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

// Forward declarations
void render_console(EditorWindow* win);
void render_file_browser(EditorWindow* win);
void render_toolbar(EditorWindow* win);
void render_scene(EditorWindow* win);
void render_properties(EditorWindow* win);
void render_code_editor(EditorWindow* win);

void minimize_window(EditorWindow* win) {
    if (win->state == WIN_STATE_MINIMIZED) {
        // Restore window
        win->state = WIN_STATE_NORMAL;
        win->x = win->saved_x;
        win->y = win->saved_y;
        win->width = win->saved_width;
        win->height = win->saved_height;
        win->visible = 1;
        
        // Auto-resize content on restore
        switch (win->type) {
            case WINDOW_CODE:
                // Maintain cursor visibility when restoring code editor
                if (editor->code_buffer.text) {
                    // Calculate line of cursor and ensure it's visible
                    int line = 0;
                    for (int i = 0; i < editor->code_buffer.cursor_pos && i < editor->code_buffer.used; i++) {
                        if (editor->code_buffer.text[i] == '\n') line++;
                    }
                    
                    float line_height = 16;
                    float cursor_y = line * line_height + 55;
                    float visible_height = win->height - TITLE_BAR_HEIGHT;
                    
                    if (cursor_y < win->content_scroll_y) {
                        win->content_scroll_y = cursor_y - line_height;
                    } else if (cursor_y > win->content_scroll_y + visible_height - line_height) {
                        win->content_scroll_y = cursor_y - visible_height + line_height * 2;
                    }
                    
                    if (win->content_scroll_y < 0) win->content_scroll_y = 0;
                }
                break;
                
            case WINDOW_CONSOLE:
                // Keep console scrolled to recent messages
                if (editor->console.line_count > 0) {
                    float max_scroll = editor->console.line_count * 16 - win->height + TITLE_BAR_HEIGHT + 40;
                    if (max_scroll > 0 && win->content_scroll_y < max_scroll * 0.8f) {
                        win->content_scroll_y = max_scroll;
                    }
                }
                break;
                
            default:
                break;
        }
    } else {
        // Minimize window
        win->saved_x = win->x;
        win->saved_y = win->y;
        win->saved_width = win->width;
        win->saved_height = win->height;
        win->state = WIN_STATE_MINIMIZED;
        win->visible = 0;
        
        // Deactivate console input if console is minimized
        if (win->type == WINDOW_CONSOLE) {
            editor->console_input.input_active = 0;
        }
    }
}

void maximize_window(EditorWindow* win) {
    if (win->state == WIN_STATE_MAXIMIZED) {
        win->state = WIN_STATE_NORMAL;
        win->x = win->saved_x;
        win->y = win->saved_y;
        win->width = win->saved_width;
        win->height = win->saved_height;
    } else {
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
    
    // Auto-resize content based on window type
    switch (win->type) {
        case WINDOW_CODE:
            // Reset scroll position for maximized code editor
            if (win->state == WIN_STATE_MAXIMIZED) {
                win->content_scroll_y = 0;
                editor->code_buffer.scroll_x = 0;
                editor->code_buffer.scroll_y = 0;
            }
            break;
            
        case WINDOW_CONSOLE:
            // Scroll to bottom for console
            if (win->state == WIN_STATE_MAXIMIZED) {
                float max_scroll = editor->console.line_count * 16 - win->height + TITLE_BAR_HEIGHT + 40;
                if (max_scroll > 0) {
                    win->content_scroll_y = max_scroll;
                }
            }
            break;
            
        case WINDOW_FILES:
            // Reset file browser scroll
            if (win->state == WIN_STATE_MAXIMIZED) {
                win->content_scroll_y = 0;
                editor->files.scroll_y = 0;
            }
            break;
            
        default:
            break;
    }
}

void close_window(EditorWindow* win) {
    win->visible = 0;
}

int get_resize_edge(EditorWindow* win, int x, int y) {
    int edge_size = 10;  // Increased for better grab accuracy with calibration
    int on_left = (x >= win->x - edge_size && x <= win->x + edge_size);
    int on_right = (x >= win->x + win->width - edge_size && x <= win->x + win->width + edge_size);
    int on_top = (y >= win->y - edge_size && y <= win->y + edge_size);
    int on_bottom = (y >= win->y + win->height - edge_size && y <= win->y + win->height + edge_size);
    
    if (on_top && on_left) return 8;
    if (on_top && on_right) return 2;
    if (on_bottom && on_left) return 6;
    if (on_bottom && on_right) return 4;
    if (on_top) return 1;
    if (on_right) return 3;
    if (on_bottom) return 5;
    if (on_left) return 7;
    
    return 0;
}

void render_window(EditorWindow* win) {
    if (!win->visible) return;
    
    float x = win->x;
    float y = win->y;
    float w = win->width;
    float h = win->height;
    
    // Title bar gradient
    glBegin(GL_QUADS);
    if (win->focused) {
        glColor4f(0.25f, 0.35f, 0.55f, 1.0f);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glColor4f(0.15f, 0.25f, 0.45f, 1.0f);
        glVertex2f(x + w, y + TITLE_BAR_HEIGHT);
        glVertex2f(x, y + TITLE_BAR_HEIGHT);
    } else {
        glColor4f(0.15f, 0.15f, 0.2f, 1.0f);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glColor4f(0.1f, 0.1f, 0.15f, 1.0f);
        glVertex2f(x + w, y + TITLE_BAR_HEIGHT);
        glVertex2f(x, y + TITLE_BAR_HEIGHT);
    }
    glEnd();
    
    // Title text with shadow
    glColor3f(0, 0, 0);
    draw_text(x + 11, y + 9, win->title, 1.5f);
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
    
    // Content area
    glColor4f(0.08f, 0.08f, 0.1f, 1.0f);
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
    
    // Render content
    glPushMatrix();
    glTranslatef(x, y + TITLE_BAR_HEIGHT, 0);
    
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
    
    // Scrollbar if needed
    if (win->content_height > win->height - TITLE_BAR_HEIGHT) {
        float scrollbar_x = x + w - 15;
        float scrollbar_y = y + TITLE_BAR_HEIGHT;
        float scrollbar_height = h - TITLE_BAR_HEIGHT;
        
        glColor4f(0.15f, 0.15f, 0.15f, 0.7f);
        glBegin(GL_QUADS);
        glVertex2f(scrollbar_x, scrollbar_y);
        glVertex2f(scrollbar_x + 12, scrollbar_y);
        glVertex2f(scrollbar_x + 12, scrollbar_y + scrollbar_height);
        glVertex2f(scrollbar_x, scrollbar_y + scrollbar_height);
        glEnd();
        
        float thumb_height = (scrollbar_height / win->content_height) * scrollbar_height;
        float thumb_y = scrollbar_y + (win->content_scroll_y / win->content_height) * scrollbar_height;
        
        glColor4f(0.4f, 0.4f, 0.45f, 0.9f);
        glBegin(GL_QUADS);
        glVertex2f(scrollbar_x + 2, thumb_y);
        glVertex2f(scrollbar_x + 10, thumb_y);
        glVertex2f(scrollbar_x + 10, thumb_y + thumb_height);
        glVertex2f(scrollbar_x + 2, thumb_y + thumb_height);
        glEnd();
    }
}

void render_console(EditorWindow* win) {
    glColor3f(0.0f, 1.0f, 0.0f);
    
    float y = 10 - win->content_scroll_y;
    int start = 0;
    
    for (int i = start; i < editor->console.line_count; i++) {
        if (y > -16 && y < win->height - TITLE_BAR_HEIGHT) {
            draw_text(10, y, editor->console.lines[i], 1.3f);
        }
        y += 16;
    }
    
    // Input line at bottom
    float input_y = win->height - TITLE_BAR_HEIGHT - 25;
    
    // Input background
    glColor4f(0.1f, 0.1f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(5, input_y - 2);
    glVertex2f(win->width - 5, input_y - 2);
    glVertex2f(win->width - 5, input_y + 18);
    glVertex2f(5, input_y + 18);
    glEnd();
    
    // Prompt
    glColor3f(0.0f, 1.0f, 0.0f);
    draw_text(10, input_y, "> ", 1.3f);
    
    // Input text
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(30, input_y, editor->console_input.input_buffer, 1.3f);
    
    // Cursor
    if (editor->console_input.input_active) {
        float cursor_x = 30 + editor->console_input.input_cursor * 8 * 1.3f;
        glColor3f(1.0f, 1.0f, 0.0f);
        glBegin(GL_LINES);
        glVertex2f(cursor_x, input_y);
        glVertex2f(cursor_x, input_y + 16);
        glEnd();
    }
    
    win->content_height = editor->console.line_count * 16 + 40;
}

void render_file_browser(EditorWindow* win) {
    glColor3f(0.9f, 0.9f, 0.9f);
    draw_text(10, 10 - win->content_scroll_y, "PROJECT FILES:", 1.4f);
    
    float y = 35 - win->content_scroll_y;
    for (int i = 0; i < editor->files.count; i++) {
        if (y > -18 && y < win->height - TITLE_BAR_HEIGHT) {
            // Highlight selected file
            if (i == editor->files.selected_file) {
                glColor4f(0.3f, 0.3f, 0.5f, 0.7f);
                glBegin(GL_QUADS);
                glVertex2f(5, y - 2);
                glVertex2f(win->width - 15, y - 2);
                glVertex2f(win->width - 15, y + 16);
                glVertex2f(5, y + 16);
                glEnd();
            }
            
            if (editor->files.is_dir[i]) {
                glColor3f(0.4f, 0.6f, 1.0f);
                draw_text(10, y, "[DIR] ", 1.2f);
                draw_text(60, y, editor->files.names[i], 1.2f);
            } else {
                // Color code by file extension
                char* ext = strrchr(editor->files.names[i], '.');
                if (ext && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)) {
                    glColor3f(0.9f, 0.7f, 0.3f);  // C files in orange
                } else if (ext && strcmp(ext, ".txt") == 0) {
                    glColor3f(0.7f, 0.9f, 0.7f);  // Text files in green
                } else {
                    glColor3f(0.8f, 0.8f, 0.8f);  // Default files in white
                }
                draw_text(10, y, "      ", 1.2f);
                draw_text(60, y, editor->files.names[i], 1.2f);
            }
        }
        y += 18;
    }
    
    win->content_height = editor->files.count * 18 + 40;
}

void render_toolbar(EditorWindow* win) {
    float x = 10;
    float y = 10;
    
    // Compile button
    glColor4f(0.2f, 0.4f, 0.7f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 90, y);
    glVertex2f(x + 90, y + 35);
    glVertex2f(x, y + 35);
    glEnd();
    
    glColor3f(1, 1, 1);
    draw_text(x + 12, y + 12, "COMPILE", 1.3f);
    
    x += 100;
    
    // Play/Stop button
    if (editor->engine.is_running) {
        glColor4f(0.7f, 0.2f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + 70, y);
        glVertex2f(x + 70, y + 35);
        glVertex2f(x, y + 35);
        glEnd();
        
        glColor3f(1, 1, 1);
        draw_text(x + 18, y + 12, "STOP", 1.3f);
    } else {
        glColor4f(0.2f, 0.7f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + 70, y);
        glVertex2f(x + 70, y + 35);
        glVertex2f(x, y + 35);
        glEnd();
        
        glColor3f(1, 1, 1);
        draw_text(x + 18, y + 12, "PLAY", 1.3f);
    }
    
    x += 80;
    
    // Restart button
    glColor4f(0.7f, 0.7f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 90, y);
    glVertex2f(x + 90, y + 35);
    glVertex2f(x, y + 35);
    glEnd();
    
    glColor3f(1, 1, 1);
    draw_text(x + 12, y + 12, "RESTART", 1.3f);
}

void render_scene(EditorWindow* win) {
    glColor3f(0.3f, 0.3f, 0.3f);
    
    // Grid
    float grid_size = 25;
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
    
    // Labels
    glColor3f(1, 1, 1);
    draw_text(10, 10, "3D SCENE VIEWPORT", 1.4f);
    glColor3f(0.7f, 0.7f, 0.7f);
    draw_text(10, 30, "Camera: Perspective", 1.2f);
    draw_text(10, 48, "Grid: 25 units", 1.2f);
}

void render_properties(EditorWindow* win) {
    glColor3f(0.9f, 0.9f, 0.9f);
    draw_text(10, 10, "PROPERTIES", 1.4f);
    
    glColor3f(0.6f, 0.6f, 0.6f);
    draw_text(10, 35, "Object: Terrain", 1.2f);
    draw_text(10, 55, "Type: Mesh", 1.2f);
    draw_text(10, 75, "Vertices: 16384", 1.2f);
    draw_text(10, 95, "Material: Grass", 1.2f);
    draw_text(10, 115, "LOD: Automatic", 1.2f);
}

void render_code_editor(EditorWindow* win) {
    TextBuffer* buffer = &editor->code_buffer;
    
    // Header
    glColor3f(0.2f, 0.9f, 0.2f);
    draw_text(10, 10 - win->content_scroll_y, "CODE EDITOR", 1.4f);
    
    // Filename
    glColor3f(0.5f, 0.5f, 0.5f);
    char title[512];
    if (buffer->filename[0] != '\0') {
        snprintf(title, sizeof(title), "%s%s", buffer->filename, buffer->dirty ? " *" : "");
    } else {
        strcpy(title, "[New File]");
    }
    draw_text(10, 30 - win->content_scroll_y, title, 1.2f);
    
    // Text content
    float char_width = 8 * 1.1f;
    float line_height = 16;
    float start_x = 40;  // Leave space for line numbers
    float start_y = 55;
    
    if (buffer->text) {
        // Calculate visible range
        int first_visible_line = (int)(win->content_scroll_y / line_height);
        int last_visible_line = first_visible_line + (int)((win->height - TITLE_BAR_HEIGHT) / line_height) + 2;
        
        // Count lines and find line starts
        int line_count = 1;
        int line_starts[1000] = {0};  // Support up to 1000 lines for demo
        int line_idx = 1;
        
        for (int i = 0; i < buffer->used && line_idx < 999; i++) {
            if (buffer->text[i] == '\n') {
                line_starts[line_idx++] = i + 1;
                line_count++;
            }
        }
        line_starts[line_idx] = buffer->used;
        
        // Render visible lines
        for (int line = first_visible_line; line < line_count && line <= last_visible_line; line++) {
            if (line < 0) continue;
            
            float y = start_y + line * line_height - win->content_scroll_y;
            
            // Line number
            glColor3f(0.4f, 0.4f, 0.4f);
            char line_num[16];
            snprintf(line_num, sizeof(line_num), "%3d", line + 1);
            draw_text(10, y, line_num, 1.1f);
            
            // Line content
            int line_start = line_starts[line];
            int line_end = (line + 1 < line_count) ? line_starts[line + 1] - 1 : buffer->used;
            
            // Selection highlighting
            if (buffer->selection_start >= 0 && buffer->selection_end >= 0) {
                int sel_start = buffer->selection_start < buffer->selection_end ? 
                               buffer->selection_start : buffer->selection_end;
                int sel_end = buffer->selection_start < buffer->selection_end ? 
                             buffer->selection_end : buffer->selection_start;
                
                if (sel_start < line_end && sel_end > line_start) {
                    int hl_start = sel_start > line_start ? sel_start - line_start : 0;
                    int hl_end = sel_end < line_end ? sel_end - line_start : line_end - line_start;
                    
                    glColor4f(0.3f, 0.3f, 0.6f, 0.5f);
                    glBegin(GL_QUADS);
                    glVertex2f(start_x + hl_start * char_width, y - 2);
                    glVertex2f(start_x + hl_end * char_width, y - 2);
                    glVertex2f(start_x + hl_end * char_width, y + line_height - 2);
                    glVertex2f(start_x + hl_start * char_width, y + line_height - 2);
                    glEnd();
                }
            }
            
            // Text rendering
            glColor3f(0.9f, 0.9f, 0.9f);
            for (int i = line_start; i < line_end && i < buffer->used; i++) {
                if (buffer->text[i] == '\n') break;
                if (buffer->text[i] >= 32 && buffer->text[i] <= 126) {
                    draw_char(start_x + (i - line_start) * char_width, y, buffer->text[i], 1.1f);
                }
            }
            
            // Cursor
            if (buffer->cursor_pos >= line_start && buffer->cursor_pos <= line_end) {
                float cursor_x = start_x + (buffer->cursor_pos - line_start) * char_width;
                glColor3f(1.0f, 1.0f, 0.0f);
                glBegin(GL_LINES);
                glVertex2f(cursor_x, y);
                glVertex2f(cursor_x, y + line_height - 2);
                glEnd();
            }
        }
        
        win->content_height = line_count * line_height + 70;
    } else {
        glColor3f(0.7f, 0.3f, 0.3f);
        draw_text(10, start_y - win->content_scroll_y, "Buffer not initialized", 1.2f);
        win->content_height = 100;
    }
}

// ============= INPUT =============

void handle_mouse_down(int x, int y, int button) {
    // Apply small calibration offset for window borders
    x = x - 2;  // Compensate for typical X11 window border
    y = y - 2;  // Compensate for typical X11 window border
    
    if (editor->debug_mouse) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Mouse down: %d, %d (btn %d)", x, y, button);
        console_add(msg);
    }
    
    for (int i = editor->window_count - 1; i >= 0; i--) {
        EditorWindow* win = &editor->windows[i];
        if (!win->visible) continue;
        
        if (x >= win->x && x <= win->x + win->width &&
            y >= win->y && y <= win->y + TITLE_BAR_HEIGHT) {
            
            float btn_x = win->x + win->width - BUTTON_SIZE - 5;
            float btn_y = win->y + 5;
            
            if (x >= btn_x && x <= btn_x + BUTTON_SIZE &&
                y >= btn_y && y <= btn_y + BUTTON_SIZE) {
                close_window(win);
                return;
            }
            
            btn_x -= BUTTON_SIZE + 5;
            if (x >= btn_x && x <= btn_x + BUTTON_SIZE &&
                y >= btn_y && y <= btn_y + BUTTON_SIZE) {
                maximize_window(win);
                return;
            }
            
            btn_x -= BUTTON_SIZE + 5;
            if (x >= btn_x && x <= btn_x + BUTTON_SIZE &&
                y >= btn_y && y <= btn_y + BUTTON_SIZE) {
                minimize_window(win);
                return;
            }
            
            win->moving = 1;
            win->move_offset_x = x - win->x;
            win->move_offset_y = y - win->y;
            
            for (int j = 0; j < editor->window_count; j++) {
                editor->windows[j].focused = 0;
            }
            win->focused = 1;
            
            if (i < editor->window_count - 1) {
                EditorWindow temp = *win;
                memmove(&editor->windows[i], &editor->windows[i + 1],
                       (editor->window_count - i - 1) * sizeof(EditorWindow));
                editor->windows[editor->window_count - 1] = temp;
            }
            return;
        }
        
        int edge = get_resize_edge(win, x, y);
        if (edge != 0) {
            win->resizing = 1;
            win->resize_edge = edge;
            win->move_offset_x = x;
            win->move_offset_y = y;
            
            for (int j = 0; j < editor->window_count; j++) {
                editor->windows[j].focused = 0;
            }
            win->focused = 1;
            return;
        }
        
        // Handle content clicks for different window types
        if (x >= win->x && x <= win->x + win->width &&
            y >= win->y + TITLE_BAR_HEIGHT && y <= win->y + win->height) {
            
            float rel_x = x - win->x;
            float rel_y = y - win->y - TITLE_BAR_HEIGHT;
            
            switch (win->type) {
                case WINDOW_TOOLBAR: {
                    rel_x -= 10;
                    rel_y -= 10;
                    if (rel_y >= 0 && rel_y <= 35) {
                        if (rel_x >= 0 && rel_x <= 90) {
                            compile_engine();
                        } else if (rel_x >= 100 && rel_x <= 170) {
                            if (editor->engine.is_running) {
                                stop_engine();
                            } else {
                                start_engine();
                            }
                        } else if (rel_x >= 180 && rel_x <= 270) {
                            stop_engine();
                            start_engine();
                        }
                    }
                    break;
                }
                
                case WINDOW_FILES: {
                    // File browser clicking
                    float adjusted_y = rel_y + win->content_scroll_y;
                    if (adjusted_y >= 35) {
                        int file_index = (int)((adjusted_y - 35) / 18);
                        if (file_index >= 0 && file_index < editor->files.count) {
                            file_browser_select(file_index);
                        }
                    }
                    break;
                }
                
                case WINDOW_CONSOLE: {
                    // Activate console input
                    float input_y = win->height - TITLE_BAR_HEIGHT - 25;
                    if (rel_y >= input_y && rel_y <= input_y + 20) {
                        editor->console_input.input_active = 1;
                        // Set cursor position based on click position
                        float click_x = rel_x - 30;
                        int char_pos = (int)(click_x / (8 * 1.3f));
                        if (char_pos < 0) char_pos = 0;
                        if (char_pos > strlen(editor->console_input.input_buffer)) {
                            char_pos = strlen(editor->console_input.input_buffer);
                        }
                        editor->console_input.input_cursor = char_pos;
                    } else {
                        editor->console_input.input_active = 0;
                    }
                    break;
                }
                
                case WINDOW_CODE: {
                    // Code editor text clicking for cursor placement
                    TextBuffer* buffer = &editor->code_buffer;
                    if (buffer->text) {
                        float char_width = 8 * 1.1f;
                        float line_height = 16;
                        float start_x = 40;
                        float start_y = 55;
                        
                        float adjusted_y = rel_y + win->content_scroll_y;
                        int line = (int)((adjusted_y - start_y) / line_height);
                        
                        // Find line start positions
                        int line_starts[1000] = {0};
                        int line_idx = 1;
                        int line_count = 1;
                        
                        for (int i = 0; i < buffer->used && line_idx < 999; i++) {
                            if (buffer->text[i] == '\n') {
                                line_starts[line_idx++] = i + 1;
                                line_count++;
                            }
                        }
                        
                        if (line >= 0 && line < line_count) {
                            int line_start = line_starts[line];
                            int line_end = (line + 1 < line_count) ? line_starts[line + 1] - 1 : buffer->used;
                            
                            int char_in_line = (int)((rel_x - start_x) / char_width);
                            if (char_in_line < 0) char_in_line = 0;
                            
                            int new_cursor = line_start + char_in_line;
                            if (new_cursor > line_end) new_cursor = line_end;
                            if (new_cursor > buffer->used) new_cursor = buffer->used;
                            
                            buffer->cursor_pos = new_cursor;
                            buffer->selection_start = buffer->selection_end = -1;
                        }
                    }
                    break;
                }
                
                default:
                    break;
            }
            
            // Focus the clicked window
            for (int j = 0; j < editor->window_count; j++) {
                editor->windows[j].focused = 0;
            }
            win->focused = 1;
            return;
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
    // Apply small calibration offset for window borders
    x = x - 2;  // Compensate for typical X11 window border
    y = y - 2;  // Compensate for typical X11 window border
    
    editor->mouse_x = x;
    editor->mouse_y = y;
    
    for (int i = 0; i < editor->window_count; i++) {
        EditorWindow* win = &editor->windows[i];
        
        if (win->moving) {
            win->x = x - win->move_offset_x;
            win->y = y - win->move_offset_y;
            
            if (win->x < 0) win->x = 0;
            if (win->y < 0) win->y = 0;
            if (win->x + win->width > WINDOW_WIDTH) win->x = WINDOW_WIDTH - win->width;
            if (win->y + win->height > WINDOW_HEIGHT - 25) win->y = WINDOW_HEIGHT - 25 - win->height;
        }
        
        if (win->resizing) {
            float dx = x - win->move_offset_x;
            float dy = y - win->move_offset_y;
            
            switch (win->resize_edge) {
                case 1: win->y += dy; win->height -= dy; break;
                case 2: win->y += dy; win->height -= dy; win->width += dx; break;
                case 3: win->width += dx; break;
                case 4: win->width += dx; win->height += dy; break;
                case 5: win->height += dy; break;
                case 6: win->x += dx; win->width -= dx; win->height += dy; break;
                case 7: win->x += dx; win->width -= dx; break;
                case 8: win->x += dx; win->width -= dx; win->y += dy; win->height -= dy; break;
            }
            
            if (win->width < 150) win->width = 150;
            if (win->height < 100) win->height = 100;
            
            win->move_offset_x = x;
            win->move_offset_y = y;
        }
    }
}

void handle_scroll(int x, int y, int direction) {
    // Apply small calibration offset for window borders
    x = x - 2;  // Compensate for typical X11 window border
    y = y - 2;  // Compensate for typical X11 window border
    
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
    // Use static instance - no calloc
    memset(&editor_instance, 0, sizeof(Editor));
    editor = &editor_instance;
    
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
    XStoreName(editor->display, editor->window, "Continental Architect Editor V4 - Crystal Clear Text");
    
    editor->context = glXCreateContext(editor->display, vi, NULL, GL_TRUE);
    glXMakeCurrent(editor->display, editor->window, editor->context);
    
    // Initialize
    init_font();
    refresh_files();
    init_text_buffer(&editor->code_buffer);
    editor->console_input.input_buffer[0] = '\0';
    editor->console_input.input_cursor = 0;
    editor->console_input.input_active = 0;
    editor->files.selected_file = -1;
    editor->files.scroll_y = 0;
    
    // Create default windows
    create_window("Console", 10, 450, 400, 300, WINDOW_CONSOLE);
    create_window("Files", 420, 450, 350, 300, WINDOW_FILES);
    create_window("Toolbar", 10, 10, 380, 70, WINDOW_TOOLBAR);
    create_window("Scene", 400, 10, 500, 430, WINDOW_SCENE);
    create_window("Properties", 910, 10, 280, 250, WINDOW_PROPERTIES);
    create_window("Code", 780, 270, 410, 480, WINDOW_CODE);
    
    console_add("Editor V4 initialized");
    console_add("Continental Architect Editor");
    console_add("=============================");
    console_add("Complete font rendering system");
    console_add("All ASCII characters supported");
    console_add("Press D for debug mode");
    console_add("F5: Compile | F6: Play/Stop");
    
    // Main loop
    int running = 1;
    while (running) {
        XEvent xev;
        
        while (XPending(editor->display)) {
            XNextEvent(editor->display, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                
                // Check modifier keys
                editor->shift_held = (xev.xkey.state & ShiftMask) != 0;
                editor->ctrl_held = (xev.xkey.state & ControlMask) != 0;
                
                // Global keys
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
                    if (!editor->console_input.input_active) {
                        editor->debug_mouse = !editor->debug_mouse;
                        console_add(editor->debug_mouse ? "Debug mode ON" : "Debug mode OFF");
                    }
                }
                
                // Handle input based on active window/component
                EditorWindow* focused_win = NULL;
                for (int i = 0; i < editor->window_count; i++) {
                    if (editor->windows[i].focused) {
                        focused_win = &editor->windows[i];
                        break;
                    }
                }
                
                // Console input handling
                if (editor->console_input.input_active) {
                    if (key == XK_Return) {
                        console_execute_command(editor->console_input.input_buffer);
                        editor->console_input.input_buffer[0] = '\0';
                        editor->console_input.input_cursor = 0;
                    } else if (key == XK_BackSpace) {
                        if (editor->console_input.input_cursor > 0) {
                            memmove(&editor->console_input.input_buffer[editor->console_input.input_cursor - 1],
                                   &editor->console_input.input_buffer[editor->console_input.input_cursor],
                                   strlen(&editor->console_input.input_buffer[editor->console_input.input_cursor]) + 1);
                            editor->console_input.input_cursor--;
                        }
                    } else if (key == XK_Left) {
                        if (editor->console_input.input_cursor > 0) {
                            editor->console_input.input_cursor--;
                        }
                    } else if (key == XK_Right) {
                        if (editor->console_input.input_cursor < strlen(editor->console_input.input_buffer)) {
                            editor->console_input.input_cursor++;
                        }
                    } else {
                        // Regular character input
                        char buffer[2];
                        int count = XLookupString(&xev.xkey, buffer, 1, NULL, NULL);
                        if (count == 1 && buffer[0] >= 32 && buffer[0] <= 126 && 
                            strlen(editor->console_input.input_buffer) < 255) {
                            // Insert character at cursor
                            int len = strlen(editor->console_input.input_buffer);
                            memmove(&editor->console_input.input_buffer[editor->console_input.input_cursor + 1],
                                   &editor->console_input.input_buffer[editor->console_input.input_cursor],
                                   len - editor->console_input.input_cursor + 1);
                            editor->console_input.input_buffer[editor->console_input.input_cursor] = buffer[0];
                            editor->console_input.input_cursor++;
                        }
                    }
                }
                // Code editor input handling
                else if (focused_win && focused_win->type == WINDOW_CODE && editor->code_buffer.text) {
                    TextBuffer* buffer = &editor->code_buffer;
                    
                    if (editor->ctrl_held) {
                        // Ctrl key combinations
                        if (key == XK_s) {
                            text_buffer_save_file(buffer);
                        } else if (key == XK_a) {
                            buffer->selection_start = 0;
                            buffer->selection_end = buffer->used;
                        } else if (key == XK_c && buffer->selection_start >= 0) {
                            // Copy to system clipboard would go here
                            console_add("Copy: Clipboard support not implemented");
                        } else if (key == XK_v) {
                            // Paste from system clipboard would go here
                            console_add("Paste: Clipboard support not implemented");
                        }
                    } else {
                        // Regular text editing
                        if (key == XK_Return) {
                            text_buffer_insert(buffer, '\n');
                        } else if (key == XK_BackSpace) {
                            text_buffer_backspace(buffer);
                        } else if (key == XK_Delete) {
                            if (buffer->cursor_pos < buffer->used) {
                                text_buffer_move_cursor(buffer, 1, 0);
                                text_buffer_backspace(buffer);
                            }
                        } else if (key == XK_Left) {
                            text_buffer_move_cursor(buffer, -1, editor->shift_held);
                        } else if (key == XK_Right) {
                            text_buffer_move_cursor(buffer, 1, editor->shift_held);
                        } else if (key == XK_Up) {
                            // Move to previous line - simplified for demo
                            int steps = 0;
                            int pos = buffer->cursor_pos;
                            while (pos > 0 && buffer->text[pos - 1] != '\n') {
                                pos--; steps++;
                            }
                            if (pos > 0) {
                                pos--; // Skip the newline
                                int line_start = pos;
                                while (line_start > 0 && buffer->text[line_start - 1] != '\n') {
                                    line_start--;
                                }
                                int target = line_start + steps;
                                if (target > pos) target = pos;
                                text_buffer_move_cursor(buffer, target - buffer->cursor_pos, editor->shift_held);
                            }
                        } else if (key == XK_Down) {
                            // Move to next line - simplified for demo
                            int steps = 0;
                            int pos = buffer->cursor_pos;
                            int line_start = pos;
                            while (line_start > 0 && buffer->text[line_start - 1] != '\n') {
                                line_start--; steps++;
                            }
                            
                            while (pos < buffer->used && buffer->text[pos] != '\n') {
                                pos++;
                            }
                            if (pos < buffer->used) {
                                pos++; // Skip the newline
                                int target = pos + steps;
                                int line_end = pos;
                                while (line_end < buffer->used && buffer->text[line_end] != '\n') {
                                    line_end++;
                                }
                                if (target > line_end) target = line_end;
                                text_buffer_move_cursor(buffer, target - buffer->cursor_pos, editor->shift_held);
                            }
                        } else if (key == XK_Home) {
                            // Move to beginning of line
                            int pos = buffer->cursor_pos;
                            while (pos > 0 && buffer->text[pos - 1] != '\n') {
                                pos--;
                            }
                            text_buffer_move_cursor(buffer, pos - buffer->cursor_pos, editor->shift_held);
                        } else if (key == XK_End) {
                            // Move to end of line
                            int pos = buffer->cursor_pos;
                            while (pos < buffer->used && buffer->text[pos] != '\n') {
                                pos++;
                            }
                            text_buffer_move_cursor(buffer, pos - buffer->cursor_pos, editor->shift_held);
                        } else {
                            // Regular character input
                            char text_buffer[2];
                            int count = XLookupString(&xev.xkey, text_buffer, 1, NULL, NULL);
                            if (count == 1 && text_buffer[0] >= 32 && text_buffer[0] <= 126) {
                                text_buffer_insert(buffer, text_buffer[0]);
                            }
                        }
                    }
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
        
        if (editor->engine.is_running) {
            int status;
            pid_t result = waitpid(editor->engine.engine_pid, &status, WNOHANG);
            if (result != 0) {
                editor->engine.is_running = 0;
                console_add("Engine stopped");
            }
        }
        
        // Render
        glClearColor(0.03f, 0.03f, 0.05f, 1.0f);
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
        glColor4f(0.08f, 0.08f, 0.12f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0, WINDOW_HEIGHT - 25);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - 25);
        glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
        glVertex2f(0, WINDOW_HEIGHT);
        glEnd();
        
        glColor3f(0.9f, 0.9f, 0.9f);
        char status[256];
        snprintf(status, sizeof(status), "FPS: 60 | Engine: %s | Mouse: (%d, %d)", 
                editor->engine.is_running ? "Running" : "Stopped",
                editor->mouse_x, editor->mouse_y);
        draw_text(10, WINDOW_HEIGHT - 18, status, 1.3f);
        
        if (editor->debug_mouse) {
            glColor3f(1, 0, 0);
            glLineWidth(1);
            glBegin(GL_LINES);
            glVertex2f(editor->mouse_x - 10, editor->mouse_y);
            glVertex2f(editor->mouse_x + 10, editor->mouse_y);
            glVertex2f(editor->mouse_x, editor->mouse_y - 10);
            glVertex2f(editor->mouse_x, editor->mouse_y + 10);
            glEnd();
        }
        
        glXSwapBuffers(editor->display, editor->window);
        usleep(16666);
    }
    
    if (editor->engine.is_running) {
        stop_engine();
    }
    
    // Cleanup
    // No need to free static allocations
    
    glXMakeCurrent(editor->display, None, NULL);
    glXDestroyContext(editor->display, editor->context);
    XDestroyWindow(editor->display, editor->window);
    XCloseDisplay(editor->display);
    return 0;
}