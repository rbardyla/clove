// handmade_command_palette.h - VSCode/Sublime-style command palette
// PERFORMANCE: <0.5ms fuzzy search for 1000 commands, SIMD string matching
// TARGET: Instant response, predictive suggestions, muscle memory support

#ifndef HANDMADE_COMMAND_PALETTE_H
#define HANDMADE_COMMAND_PALETTE_H

#include <stdint.h>
#include <stdbool.h>
#include "../renderer/handmade_math.h"

#define MAX_COMMANDS 2048
#define MAX_COMMAND_NAME 128
#define MAX_COMMAND_DESC 256
#define MAX_SEARCH_RESULTS 50
#define MAX_RECENT_COMMANDS 20
#define MAX_KEYBIND_LENGTH 32

// ============================================================================
// COMMAND SYSTEM
// ============================================================================

typedef enum {
    CMD_CATEGORY_FILE,
    CMD_CATEGORY_EDIT,
    CMD_CATEGORY_VIEW,
    CMD_CATEGORY_SEARCH,
    CMD_CATEGORY_BUILD,
    CMD_CATEGORY_DEBUG,
    CMD_CATEGORY_TOOLS,
    CMD_CATEGORY_WINDOW,
    CMD_CATEGORY_HELP,
    CMD_CATEGORY_CUSTOM,
} command_category;

typedef enum {
    CMD_FLAG_NONE = 0,
    CMD_FLAG_HIDDEN = (1 << 0),
    CMD_FLAG_DEVELOPER = (1 << 1),
    CMD_FLAG_DANGEROUS = (1 << 2),
    CMD_FLAG_REQUIRES_SELECTION = (1 << 3),
    CMD_FLAG_REQUIRES_PROJECT = (1 << 4),
    CMD_FLAG_TOGGLE = (1 << 5),
    CMD_FLAG_RECENT = (1 << 6),
} command_flags;

// Command execution callback
typedef void (*command_execute_func)(void* context, void* args);
typedef bool (*command_enabled_func)(void* context);

typedef struct command_keybind {
    uint32_t modifiers;  // Ctrl, Shift, Alt flags
    uint32_t key;        // Key code
    char display[MAX_KEYBIND_LENGTH];  // "Ctrl+Shift+P"
} command_keybind;

typedef struct command_definition {
    uint32_t id;
    char name[MAX_COMMAND_NAME];
    char display_name[MAX_COMMAND_NAME];
    char description[MAX_COMMAND_DESC];
    command_category category;
    command_flags flags;
    
    // Keybinding
    command_keybind keybind;
    command_keybind alternate_keybind;
    
    // Icon (optional)
    uint32_t icon_id;
    
    // Execution
    command_execute_func execute;
    command_enabled_func is_enabled;
    void* user_data;
    
    // Usage tracking
    uint32_t execution_count;
    uint64_t last_execution_time;
    f32 frequency_score;  // For smart sorting
} command_definition;

// ============================================================================
// FUZZY SEARCH
// ============================================================================

typedef struct search_match {
    uint32_t command_index;
    f32 score;
    
    // Match positions for highlighting
    uint8_t match_positions[MAX_COMMAND_NAME];
    uint32_t match_count;
    
    // Score components
    f32 exact_match_score;
    f32 prefix_score;
    f32 acronym_score;
    f32 fuzzy_score;
    f32 frequency_bonus;
    f32 recency_bonus;
} search_match;

typedef struct search_context {
    char query[MAX_COMMAND_NAME];
    uint32_t query_length;
    
    // SIMD-optimized query data
    __m256i query_chars_lower[4];  // Lowercase for case-insensitive
    __m256i query_chars_upper[4];  // Uppercase variants
    uint32_t simd_blocks;
    
    // Results
    search_match matches[MAX_SEARCH_RESULTS];
    uint32_t match_count;
    
    // Filtering
    command_category category_filter;
    command_flags flag_filter;
    bool show_hidden;
} search_context;

// ============================================================================
// COMMAND HISTORY
// ============================================================================

typedef struct command_history {
    uint32_t recent_commands[MAX_RECENT_COMMANDS];
    uint32_t recent_count;
    uint32_t recent_head;
    
    // Frequency tracking
    struct {
        uint32_t command_id;
        uint32_t count;
    } frequency[MAX_COMMANDS];
    uint32_t frequency_count;
    
    // Session history for undo
    uint32_t* session_history;
    uint32_t session_count;
    uint32_t session_capacity;
} command_history;

// ============================================================================
// UI STATE
// ============================================================================

typedef struct palette_ui_state {
    bool is_open;
    bool just_opened;
    
    // Layout
    v2 position;
    v2 size;
    f32 max_height;
    
    // Animation
    f32 open_animation;
    f32 scroll_y;
    f32 scroll_target;
    
    // Selection
    int32_t selected_index;
    int32_t hover_index;
    
    // Input
    char input_buffer[MAX_COMMAND_NAME];
    uint32_t cursor_position;
    uint32_t selection_start;
    uint32_t selection_end;
    
    // Visual settings
    f32 item_height;
    f32 padding;
    color32 background_color;
    color32 selection_color;
    color32 text_color;
    color32 match_highlight_color;
    color32 shortcut_color;
    color32 category_color;
} palette_ui_state;

// ============================================================================
// COMMAND PALETTE
// ============================================================================

typedef struct command_palette {
    // Registered commands
    command_definition commands[MAX_COMMANDS];
    uint32_t command_count;
    
    // Fast lookup
    struct {
        uint32_t* name_hashes;
        uint32_t* indices;
        uint32_t count;
    } command_map;
    
    // Categories
    struct {
        const char* name;
        uint32_t icon_id;
        color32 color;
    } categories[16];
    
    // Search
    search_context search;
    
    // History
    command_history history;
    
    // UI
    palette_ui_state ui;
    
    // Context for command execution
    void* execution_context;
    
    // Performance stats
    struct {
        uint64_t search_time_us;
        uint64_t render_time_us;
        uint32_t commands_searched;
    } stats;
} command_palette;

// ============================================================================
// CORE API
// ============================================================================

// Initialize/shutdown
void command_palette_init(command_palette* palette);
void command_palette_shutdown(command_palette* palette);

// Command registration
uint32_t command_register(command_palette* palette, const command_definition* def);
void command_unregister(command_palette* palette, uint32_t command_id);
command_definition* command_get(command_palette* palette, uint32_t command_id);
command_definition* command_find(command_palette* palette, const char* name);

// Keybinding
void command_set_keybind(command_palette* palette, uint32_t command_id,
                        const command_keybind* keybind);
bool command_handle_keypress(command_palette* palette, uint32_t modifiers, 
                            uint32_t key);

// Execution
void command_execute(command_palette* palette, uint32_t command_id, void* args);
void command_execute_by_name(command_palette* palette, const char* name, void* args);
bool command_can_execute(command_palette* palette, uint32_t command_id);

// ============================================================================
// UI INTERFACE
// ============================================================================

// Show/hide
void command_palette_show(command_palette* palette);
void command_palette_hide(command_palette* palette);
void command_palette_toggle(command_palette* palette);
bool command_palette_is_open(command_palette* palette);

// Rendering
void command_palette_update(command_palette* palette, f32 dt);
void command_palette_render(command_palette* palette);

// Input handling
bool command_palette_handle_input(command_palette* palette, 
                                 const input_event* event);
void command_palette_handle_text_input(command_palette* palette, const char* text);

// ============================================================================
// SEARCH
// ============================================================================

// Fuzzy search
void command_search(command_palette* palette, const char* query);
void command_search_clear(command_palette* palette);
uint32_t command_get_search_results(command_palette* palette, 
                                   search_match* out_matches, uint32_t max_results);

// Filtering
void command_set_category_filter(command_palette* palette, command_category cat);
void command_set_flag_filter(command_palette* palette, command_flags flags);
void command_clear_filters(command_palette* palette);

// ============================================================================
// HISTORY & SUGGESTIONS
// ============================================================================

// Recent commands
void command_add_to_history(command_palette* palette, uint32_t command_id);
void command_get_recent(command_palette* palette, uint32_t* out_ids, 
                       uint32_t* out_count);
void command_clear_history(command_palette* palette);

// Smart suggestions
void command_get_suggestions(command_palette* palette, uint32_t* out_ids,
                            uint32_t* out_count, uint32_t max_suggestions);

// Frequency analysis
f32 command_get_frequency_score(command_palette* palette, uint32_t command_id);
void command_update_frequency_scores(command_palette* palette);

// ============================================================================
// UTILITIES
// ============================================================================

// String matching algorithms
f32 fuzzy_match_score(const char* pattern, const char* str);
f32 fuzzy_match_score_simd(const search_context* ctx, const char* str);
bool fuzzy_match_positions(const char* pattern, const char* str,
                          uint8_t* out_positions, uint32_t* out_count);

// Keybind formatting
void keybind_to_string(const command_keybind* keybind, char* out_str);
bool keybind_from_string(const char* str, command_keybind* out_keybind);

// Category helpers
const char* command_category_to_string(command_category cat);
command_category command_category_from_string(const char* str);

// ============================================================================
// BUILT-IN COMMANDS
// ============================================================================

// Register standard editor commands
void command_register_file_commands(command_palette* palette);
void command_register_edit_commands(command_palette* palette);
void command_register_view_commands(command_palette* palette);
void command_register_search_commands(command_palette* palette);
void command_register_window_commands(command_palette* palette);
void command_register_debug_commands(command_palette* palette);

// Common command IDs
enum {
    CMD_FILE_NEW = 1000,
    CMD_FILE_OPEN,
    CMD_FILE_SAVE,
    CMD_FILE_SAVE_AS,
    CMD_FILE_CLOSE,
    
    CMD_EDIT_UNDO = 2000,
    CMD_EDIT_REDO,
    CMD_EDIT_CUT,
    CMD_EDIT_COPY,
    CMD_EDIT_PASTE,
    CMD_EDIT_SELECT_ALL,
    
    CMD_VIEW_ZOOM_IN = 3000,
    CMD_VIEW_ZOOM_OUT,
    CMD_VIEW_ZOOM_RESET,
    CMD_VIEW_FULLSCREEN,
    
    CMD_SEARCH_FIND = 4000,
    CMD_SEARCH_REPLACE,
    CMD_SEARCH_FIND_IN_FILES,
    CMD_SEARCH_GOTO_LINE,
    
    CMD_WINDOW_NEW = 5000,
    CMD_WINDOW_CLOSE,
    CMD_WINDOW_NEXT,
    CMD_WINDOW_PREV,
    
    CMD_DEBUG_TOGGLE_BREAKPOINT = 6000,
    CMD_DEBUG_START,
    CMD_DEBUG_STOP,
    CMD_DEBUG_STEP_OVER,
    CMD_DEBUG_STEP_INTO,
};

#endif // HANDMADE_COMMAND_PALETTE_H