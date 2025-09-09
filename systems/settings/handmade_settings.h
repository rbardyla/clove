#ifndef HANDMADE_SETTINGS_H
#define HANDMADE_SETTINGS_H

/*
    Handmade Settings System
    Complete game configuration management with immediate UI
    
    Features:
    - Hierarchical settings categories
    - Real-time value changes
    - Validation and constraints
    - Profile management
    - Hotkeys and bindings
    
    PERFORMANCE: Targets
    - Settings access: <1μs
    - Menu rendering: <0.1ms
    - Save time: <10ms
    - Memory usage: <64KB
*/

#include "../../src/handmade.h"

// Settings system constants
#define SETTINGS_MAGIC_NUMBER 0x53475448  // "HTGS" in hex
#define SETTINGS_VERSION 1
#define SETTINGS_MAX_PROFILES 8
#define SETTINGS_MAX_CATEGORIES 16
#define SETTINGS_MAX_SETTINGS 256
#define SETTINGS_MAX_HOTKEYS 128
#define SETTINGS_STRING_MAX 64
#define SETTINGS_PATH_MAX 256

// Forward declarations
typedef struct memory_arena memory_arena;
typedef struct gui_context gui_context;
typedef struct input_state input_state;
typedef struct render_state render_state;

// Setting data types
typedef enum setting_type {
    SETTING_BOOL = 0,
    SETTING_INT = 1,
    SETTING_FLOAT = 2,
    SETTING_STRING = 3,
    SETTING_ENUM = 4,
    SETTING_KEY = 5,
    SETTING_COLOR = 6,
    SETTING_VECTOR2 = 7,
    SETTING_VECTOR3 = 8,
} setting_type;

// Setting constraint types
typedef enum constraint_type {
    CONSTRAINT_NONE = 0,
    CONSTRAINT_RANGE = 1,    // Min/max values
    CONSTRAINT_LIST = 2,     // Predefined options
    CONSTRAINT_REGEX = 3,    // String pattern
} constraint_type;

// Setting categories
typedef enum setting_category {
    CATEGORY_VIDEO = 0,
    CATEGORY_AUDIO = 1,
    CATEGORY_CONTROLS = 2,
    CATEGORY_GAMEPLAY = 3,
    CATEGORY_GRAPHICS = 4,
    CATEGORY_NETWORK = 5,
    CATEGORY_DEBUG = 6,
    CATEGORY_ACCESSIBILITY = 7,
    CATEGORY_USER = 8,
} setting_category;

// Setting flags
typedef enum setting_flags {
    SETTING_HIDDEN = (1 << 0),      // Don't show in UI
    SETTING_READONLY = (1 << 1),    // Can't be modified
    SETTING_RESTART_REQUIRED = (1 << 2),
    SETTING_ADVANCED = (1 << 3),    // Show in advanced mode
    SETTING_PROFILE_SPECIFIC = (1 << 4),
    SETTING_HOTKEY = (1 << 5),
    SETTING_AUTOSAVE = (1 << 6),
} setting_flags;

// Setting value union
typedef union setting_value {
    b32 boolean;
    i32 integer;
    f32 floating;
    char string[SETTINGS_STRING_MAX];
    i32 enumeration;
    u32 keycode;
    u32 color;          // RGBA packed
    f32 vector2[2];
    f32 vector3[3];
} setting_value;

// Setting constraint data
typedef union constraint_data {
    struct {
        f32 min;
        f32 max;
        f32 step;
    } range;
    
    struct {
        char options[16][SETTINGS_STRING_MAX];
        i32 count;
    } list;
    
    struct {
        char pattern[128];
    } regex;
} constraint_data;

// Individual setting definition
typedef struct setting {
    char name[SETTINGS_STRING_MAX];
    char description[128];
    char tooltip[256];
    
    setting_type type;
    setting_category category;
    u32 flags;
    
    setting_value default_value;
    setting_value current_value;
    setting_value min_value;
    setting_value max_value;
    
    constraint_type constraint;
    constraint_data constraint_data;
    
    // Callback for value changes
    void (*on_change)(struct setting *setting, setting_value old_value, setting_value new_value);
    
    // Hot-reload support
    void *target_variable;  // Direct memory address
    u32 target_offset;      // Offset in structure
    
    u32 hash;              // For fast lookup
} setting;

// Settings category info
typedef struct settings_category {
    char name[SETTINGS_STRING_MAX];
    char description[128];
    u32 icon;              // Icon ID for UI
    b32 expanded;          // UI state
    u32 setting_count;
    setting *settings[64];
} settings_category;

// Settings profile
typedef struct settings_profile {
    char name[SETTINGS_STRING_MAX];
    char description[128];
    b32 active;
    u64 created_time;
    u64 modified_time;
    
    // Profile-specific values override defaults
    setting_value overrides[SETTINGS_MAX_SETTINGS];
    b32 has_override[SETTINGS_MAX_SETTINGS];
} settings_profile;

// Hotkey binding
typedef struct hotkey_binding {
    char name[SETTINGS_STRING_MAX];
    char command[128];
    u32 primary_key;
    u32 modifier_keys;     // Shift, Ctrl, Alt flags
    b32 enabled;
} hotkey_binding;

// Settings search/filter
typedef struct settings_filter {
    char search_text[128];
    setting_category category_filter;
    b32 show_advanced;
    b32 show_readonly;
    b32 modified_only;
} settings_filter;

// Settings change history (for undo/redo)
typedef struct settings_change {
    u32 setting_hash;
    setting_value old_value;
    setting_value new_value;
    u64 timestamp;
} settings_change;

// Main settings system
typedef struct settings_system {
    // Memory management
    // memory_arena arena; // TODO: Use arena when integrated with main engine
    u8 *memory;
    u32 memory_size;
    
    // All settings storage
    setting settings[SETTINGS_MAX_SETTINGS];
    u32 setting_count;
    
    // Category organization
    settings_category categories[SETTINGS_MAX_CATEGORIES];
    u32 category_count;
    
    // Profile management
    settings_profile profiles[SETTINGS_MAX_PROFILES];
    u32 profile_count;
    i32 active_profile;
    
    // Hotkey system
    hotkey_binding hotkeys[SETTINGS_MAX_HOTKEYS];
    u32 hotkey_count;
    
    // UI state
    settings_filter filter;
    i32 selected_category;
    i32 selected_setting;
    b32 ui_visible;
    b32 search_focused;
    
    // Change tracking
    settings_change history[256];
    u32 history_count;
    u32 history_index;
    
    // Performance metrics
    f32 last_save_time;
    f32 last_load_time;
    u32 changes_pending;
    
    // File paths
    char config_path[SETTINGS_PATH_MAX];
    char profiles_path[SETTINGS_PATH_MAX];
} settings_system;

// Core system functions
settings_system *settings_init(void *memory, u32 memory_size);
void settings_shutdown(settings_system *system);
void settings_update(settings_system *system, input_state *input, f32 dt);

// Setting registration and management
u32 settings_register_bool(settings_system *system, char *name, char *description, 
                          setting_category category, b32 default_value, u32 flags);
u32 settings_register_int(settings_system *system, char *name, char *description,
                         setting_category category, i32 default_value, i32 min_val, i32 max_val, u32 flags);
u32 settings_register_float(settings_system *system, char *name, char *description,
                           setting_category category, f32 default_value, f32 min_val, f32 max_val, u32 flags);
u32 settings_register_string(settings_system *system, char *name, char *description,
                            setting_category category, char *default_value, u32 flags);
u32 settings_register_enum(settings_system *system, char *name, char *description,
                          setting_category category, char **options, i32 option_count, i32 default_index, u32 flags);

// Value access
b32 settings_get_bool(settings_system *system, char *name);
i32 settings_get_int(settings_system *system, char *name);
f32 settings_get_float(settings_system *system, char *name);
char *settings_get_string(settings_system *system, char *name);
i32 settings_get_enum(settings_system *system, char *name);

// Value modification
b32 settings_set_bool(settings_system *system, char *name, b32 value);
b32 settings_set_int(settings_system *system, char *name, i32 value);
b32 settings_set_float(settings_system *system, char *name, f32 value);
b32 settings_set_string(settings_system *system, char *name, char *value);
b32 settings_set_enum(settings_system *system, char *name, i32 value);

// Profile management
i32 settings_create_profile(settings_system *system, char *name, char *description);
b32 settings_activate_profile(settings_system *system, i32 profile_index);
b32 settings_delete_profile(settings_system *system, i32 profile_index);
b32 settings_export_profile(settings_system *system, i32 profile_index, char *path);
i32 settings_import_profile(settings_system *system, char *path);

// Hotkey system
u32 settings_register_hotkey(settings_system *system, char *name, char *command, u32 default_key, u32 modifiers);
b32 settings_check_hotkey(settings_system *system, char *name, input_state *input);
b32 settings_process_hotkeys(settings_system *system, input_state *input);

// File I/O
b32 settings_save_to_file(settings_system *system, char *path);
b32 settings_load_from_file(settings_system *system, char *path);
b32 settings_auto_save(settings_system *system);
b32 settings_auto_load(settings_system *system);

// UI system
void settings_render_ui(settings_system *system, gui_context *gui);
void settings_show_menu(settings_system *system);
void settings_hide_menu(settings_system *system);
b32 settings_handle_input(settings_system *system, input_state *input);

// Utility functions
void settings_reset_to_defaults(settings_system *system);
void settings_reset_category(settings_system *system, setting_category category);
b32 settings_validate_all(settings_system *system);
u32 settings_get_modified_count(settings_system *system);

// Search and filtering
void settings_set_filter(settings_system *system, char *search_text, setting_category category, b32 show_advanced);
setting **settings_get_filtered(settings_system *system, u32 *count);

// Change tracking and undo/redo
b32 settings_undo(settings_system *system);
b32 settings_redo(settings_system *system);
void settings_clear_history(settings_system *system);

// Debug and introspection
void settings_dump_all(settings_system *system);
void settings_print_category(settings_system *system, setting_category category);
setting *settings_find_by_name(settings_system *system, char *name);

// Default settings registration (called by engine)
void settings_register_video_settings(settings_system *system);
void settings_register_audio_settings(settings_system *system);
void settings_register_control_settings(settings_system *system);
void settings_register_gameplay_settings(settings_system *system);
void settings_register_all_defaults(settings_system *system);

#endif // HANDMADE_SETTINGS_H