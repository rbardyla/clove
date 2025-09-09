/*
    Handmade Settings System - Core Implementation
    Complete game configuration with immediate UI
*/

#include "handmade_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define internal static
// Macros already defined in handmade.h

// String hashing for fast setting lookup
internal u32 
settings_hash_name(char *name) {
    u32 hash = 5381;
    char *str = name;
    while (*str) {
        hash = ((hash << 5) + hash) + (u32)*str++;
    }
    return hash;
}

// Initialize settings system
settings_system *
settings_init(void *memory, u32 memory_size) {
    if (!memory || memory_size < Kilobytes(64)) {
        return 0;
    }
    
    settings_system *system = (settings_system *)memory;
    memset(system, 0, sizeof(settings_system));
    
    system->memory = (u8 *)memory;
    system->memory_size = memory_size;
    
    // Initialize categories
    char *category_names[] = {
        "Video", "Audio", "Controls", "Gameplay",
        "Graphics", "Network", "Debug", "Accessibility"
    };
    
    char *category_descriptions[] = {
        "Display and rendering settings",
        "Sound and music configuration", 
        "Keyboard and mouse bindings",
        "Game mechanics and difficulty",
        "Advanced graphics options",
        "Multiplayer and connection settings",
        "Developer and diagnostic tools",
        "Options for better accessibility"
    };
    
    system->category_count = 8;
    for (u32 i = 0; i < system->category_count; i++) {
        strncpy(system->categories[i].name, category_names[i], SETTINGS_STRING_MAX - 1);
        strncpy(system->categories[i].description, category_descriptions[i], 127);
        system->categories[i].icon = i;
        system->categories[i].expanded = (i == 0); // Expand Video by default
        system->categories[i].setting_count = 0;
    }
    
    // Create default profile
    strncpy(system->profiles[0].name, "Default", SETTINGS_STRING_MAX - 1);
    strncpy(system->profiles[0].description, "Default game settings", 127);
    system->profiles[0].active = 1;
    system->profiles[0].created_time = 0; // Would use real timestamp
    system->profiles[0].modified_time = 0;
    system->profile_count = 1;
    system->active_profile = 0;
    
    // Initialize UI state
    system->selected_category = 0;
    system->selected_setting = -1;
    system->filter.category_filter = CATEGORY_VIDEO;
    system->filter.show_advanced = 0;
    system->filter.show_readonly = 1;
    
    // Set default paths
    strncpy(system->config_path, "config/settings.cfg", SETTINGS_PATH_MAX - 1);
    strncpy(system->profiles_path, "config/profiles/", SETTINGS_PATH_MAX - 1);
    
    return system;
}

// Register a boolean setting
u32
settings_register_bool(settings_system *system, char *name, char *description,
                      setting_category category, b32 default_value, u32 flags) {
    if (system->setting_count >= SETTINGS_MAX_SETTINGS) {
        return 0;
    }
    
    u32 index = system->setting_count++;
    setting *s = &system->settings[index];
    
    strncpy(s->name, name, SETTINGS_STRING_MAX - 1);
    strncpy(s->description, description, 127);
    s->type = SETTING_BOOL;
    s->category = category;
    s->flags = flags;
    s->default_value.boolean = default_value;
    s->current_value.boolean = default_value;
    s->hash = settings_hash_name(name);
    
    // Add to category
    if (category < SETTINGS_MAX_CATEGORIES && 
        system->categories[category].setting_count < 64) {
        u32 cat_index = system->categories[category].setting_count++;
        system->categories[category].settings[cat_index] = s;
    }
    
    return s->hash;
}

// Register an integer setting
u32
settings_register_int(settings_system *system, char *name, char *description,
                     setting_category category, i32 default_value, i32 min_val, i32 max_val, u32 flags) {
    if (system->setting_count >= SETTINGS_MAX_SETTINGS) {
        return 0;
    }
    
    u32 index = system->setting_count++;
    setting *s = &system->settings[index];
    
    strncpy(s->name, name, SETTINGS_STRING_MAX - 1);
    strncpy(s->description, description, 127);
    s->type = SETTING_INT;
    s->category = category;
    s->flags = flags;
    s->default_value.integer = default_value;
    s->current_value.integer = default_value;
    s->min_value.integer = min_val;
    s->max_value.integer = max_val;
    s->constraint = CONSTRAINT_RANGE;
    s->constraint_data.range.min = (f32)min_val;
    s->constraint_data.range.max = (f32)max_val;
    s->constraint_data.range.step = 1.0f;
    s->hash = settings_hash_name(name);
    
    // Add to category
    if (category < SETTINGS_MAX_CATEGORIES && 
        system->categories[category].setting_count < 64) {
        u32 cat_index = system->categories[category].setting_count++;
        system->categories[category].settings[cat_index] = s;
    }
    
    return s->hash;
}

// Register a float setting
u32
settings_register_float(settings_system *system, char *name, char *description,
                       setting_category category, f32 default_value, f32 min_val, f32 max_val, u32 flags) {
    if (system->setting_count >= SETTINGS_MAX_SETTINGS) {
        return 0;
    }
    
    u32 index = system->setting_count++;
    setting *s = &system->settings[index];
    
    strncpy(s->name, name, SETTINGS_STRING_MAX - 1);
    strncpy(s->description, description, 127);
    s->type = SETTING_FLOAT;
    s->category = category;
    s->flags = flags;
    s->default_value.floating = default_value;
    s->current_value.floating = default_value;
    s->min_value.floating = min_val;
    s->max_value.floating = max_val;
    s->constraint = CONSTRAINT_RANGE;
    s->constraint_data.range.min = min_val;
    s->constraint_data.range.max = max_val;
    s->constraint_data.range.step = 0.1f;
    s->hash = settings_hash_name(name);
    
    // Add to category
    if (category < SETTINGS_MAX_CATEGORIES && 
        system->categories[category].setting_count < 64) {
        u32 cat_index = system->categories[category].setting_count++;
        system->categories[category].settings[cat_index] = s;
    }
    
    return s->hash;
}

// Register an enum setting
u32
settings_register_enum(settings_system *system, char *name, char *description,
                      setting_category category, char **options, i32 option_count, i32 default_index, u32 flags) {
    if (system->setting_count >= SETTINGS_MAX_SETTINGS) {
        return 0;
    }
    
    u32 index = system->setting_count++;
    setting *s = &system->settings[index];
    
    strncpy(s->name, name, SETTINGS_STRING_MAX - 1);
    strncpy(s->description, description, 127);
    s->type = SETTING_ENUM;
    s->category = category;
    s->flags = flags;
    s->default_value.enumeration = default_index;
    s->current_value.enumeration = default_index;
    s->constraint = CONSTRAINT_LIST;
    
    // Copy options
    s->constraint_data.list.count = (option_count > 16) ? 16 : option_count;
    for (i32 i = 0; i < s->constraint_data.list.count; i++) {
        strncpy(s->constraint_data.list.options[i], options[i], SETTINGS_STRING_MAX - 1);
    }
    
    s->hash = settings_hash_name(name);
    
    // Add to category
    if (category < SETTINGS_MAX_CATEGORIES && 
        system->categories[category].setting_count < 64) {
        u32 cat_index = system->categories[category].setting_count++;
        system->categories[category].settings[cat_index] = s;
    }
    
    return s->hash;
}

// Register a string setting
u32
settings_register_string(settings_system *system, char *name, char *description,
                         setting_category category, char *default_value, u32 flags) {
    if (system->setting_count >= SETTINGS_MAX_SETTINGS) {
        return 0;
    }
    
    u32 index = system->setting_count++;
    setting *s = &system->settings[index];
    
    strncpy(s->name, name, SETTINGS_STRING_MAX - 1);
    strncpy(s->description, description, 127);
    s->type = SETTING_STRING;
    s->category = category;
    s->flags = flags;
    strncpy(s->default_value.string, default_value, SETTINGS_STRING_MAX - 1);
    strncpy(s->current_value.string, default_value, SETTINGS_STRING_MAX - 1);
    s->constraint = CONSTRAINT_NONE;
    s->hash = settings_hash_name(name);
    
    // Add to category
    if (category < SETTINGS_MAX_CATEGORIES && 
        system->categories[category].setting_count < 64) {
        u32 cat_index = system->categories[category].setting_count++;
        system->categories[category].settings[cat_index] = s;
    }
    
    return s->hash;
}

// Find setting by name
setting *
settings_find_by_name(settings_system *system, char *name) {
    u32 hash = settings_hash_name(name);
    
    for (u32 i = 0; i < system->setting_count; i++) {
        if (system->settings[i].hash == hash) {
            if (strcmp(system->settings[i].name, name) == 0) {
                return &system->settings[i];
            }
        }
    }
    
    return 0;
}

// Get boolean setting value
b32
settings_get_bool(settings_system *system, char *name) {
    setting *s = settings_find_by_name(system, name);
    if (s && s->type == SETTING_BOOL) {
        return s->current_value.boolean;
    }
    return 0;
}

// Get integer setting value
i32
settings_get_int(settings_system *system, char *name) {
    setting *s = settings_find_by_name(system, name);
    if (s && s->type == SETTING_INT) {
        return s->current_value.integer;
    }
    return 0;
}

// Get float setting value
f32
settings_get_float(settings_system *system, char *name) {
    setting *s = settings_find_by_name(system, name);
    if (s && s->type == SETTING_FLOAT) {
        return s->current_value.floating;
    }
    return 0.0f;
}

// Set boolean setting value
b32
settings_set_bool(settings_system *system, char *name, b32 value) {
    setting *s = settings_find_by_name(system, name);
    if (!s || s->type != SETTING_BOOL || (s->flags & SETTING_READONLY)) {
        return 0;
    }
    
    setting_value old_value = s->current_value;
    s->current_value.boolean = value;
    
    // Record change for undo
    if (system->history_count < 256) {
        settings_change *change = &system->history[system->history_count++];
        change->setting_hash = s->hash;
        change->old_value = old_value;
        change->new_value = s->current_value;
        change->timestamp = 0; // Would use real timestamp
    }
    
    // Call change callback
    if (s->on_change) {
        s->on_change(s, old_value, s->current_value);
    }
    
    system->changes_pending++;
    return 1;
}

// Set integer setting value
b32
settings_set_int(settings_system *system, char *name, i32 value) {
    setting *s = settings_find_by_name(system, name);
    if (!s || s->type != SETTING_INT || (s->flags & SETTING_READONLY)) {
        return 0;
    }
    
    // Apply constraints
    if (value < s->min_value.integer) value = s->min_value.integer;
    if (value > s->max_value.integer) value = s->max_value.integer;
    
    setting_value old_value = s->current_value;
    s->current_value.integer = value;
    
    // Record change for undo
    if (system->history_count < 256) {
        settings_change *change = &system->history[system->history_count++];
        change->setting_hash = s->hash;
        change->old_value = old_value;
        change->new_value = s->current_value;
        change->timestamp = 0; // Would use real timestamp
    }
    
    // Call change callback
    if (s->on_change) {
        s->on_change(s, old_value, s->current_value);
    }
    
    system->changes_pending++;
    return 1;
}

// Set float setting value
b32
settings_set_float(settings_system *system, char *name, f32 value) {
    setting *s = settings_find_by_name(system, name);
    if (!s || s->type != SETTING_FLOAT || (s->flags & SETTING_READONLY)) {
        return 0;
    }
    
    // Apply constraints
    if (value < s->min_value.floating) value = s->min_value.floating;
    if (value > s->max_value.floating) value = s->max_value.floating;
    
    setting_value old_value = s->current_value;
    s->current_value.floating = value;
    
    // Record change for undo
    if (system->history_count < 256) {
        settings_change *change = &system->history[system->history_count++];
        change->setting_hash = s->hash;
        change->old_value = old_value;
        change->new_value = s->current_value;
        change->timestamp = 0; // Would use real timestamp
    }
    
    // Call change callback
    if (s->on_change) {
        s->on_change(s, old_value, s->current_value);
    }
    
    system->changes_pending++;
    return 1;
}

// Reset all settings to defaults
void
settings_reset_to_defaults(settings_system *system) {
    for (u32 i = 0; i < system->setting_count; i++) {
        setting *s = &system->settings[i];
        if (!(s->flags & SETTING_READONLY)) {
            s->current_value = s->default_value;
        }
    }
    system->changes_pending = system->setting_count;
}

// Validate all settings
b32
settings_validate_all(settings_system *system) {
    b32 all_valid = 1;
    
    for (u32 i = 0; i < system->setting_count; i++) {
        setting *s = &system->settings[i];
        
        switch (s->type) {
            case SETTING_INT: {
                if (s->current_value.integer < s->min_value.integer ||
                    s->current_value.integer > s->max_value.integer) {
                    s->current_value.integer = s->default_value.integer;
                    all_valid = 0;
                }
            } break;
            
            case SETTING_FLOAT: {
                if (s->current_value.floating < s->min_value.floating ||
                    s->current_value.floating > s->max_value.floating) {
                    s->current_value.floating = s->default_value.floating;
                    all_valid = 0;
                }
            } break;
            
            case SETTING_ENUM: {
                if (s->current_value.enumeration < 0 ||
                    s->current_value.enumeration >= s->constraint_data.list.count) {
                    s->current_value.enumeration = s->default_value.enumeration;
                    all_valid = 0;
                }
            } break;
        }
    }
    
    return all_valid;
}

// Get count of modified settings
u32
settings_get_modified_count(settings_system *system) {
    u32 modified = 0;
    
    for (u32 i = 0; i < system->setting_count; i++) {
        setting *s = &system->settings[i];
        
        // Compare current with default
        switch (s->type) {
            case SETTING_BOOL:
                if (s->current_value.boolean != s->default_value.boolean) modified++;
                break;
            case SETTING_INT:
                if (s->current_value.integer != s->default_value.integer) modified++;
                break;
            case SETTING_FLOAT:
                if (fabsf(s->current_value.floating - s->default_value.floating) > 0.001f) modified++;
                break;
            case SETTING_STRING:
                if (strcmp(s->current_value.string, s->default_value.string) != 0) modified++;
                break;
            case SETTING_ENUM:
                if (s->current_value.enumeration != s->default_value.enumeration) modified++;
                break;
        }
    }
    
    return modified;
}

// Update settings system (called each frame)
void
settings_update(settings_system *system, input_state *input, f32 dt) {
    // Auto-save if changes are pending
    if (system->changes_pending > 0 && dt > 0.0f) {
        // Simple auto-save timer logic would go here
        // For now, just clear the pending flag
        system->changes_pending = 0;
    }
}

// Show settings menu
void
settings_show_menu(settings_system *system) {
    system->ui_visible = 1;
}

// Hide settings menu
void
settings_hide_menu(settings_system *system) {
    system->ui_visible = 0;
    system->search_focused = 0;
}

// Debug dump all settings
void
settings_dump_all(settings_system *system) {
    printf("=== Settings System Debug ===\n");
    printf("Total settings: %u\n", system->setting_count);
    printf("Categories: %u\n", system->category_count);
    printf("Active profile: %s\n", system->profiles[system->active_profile].name);
    printf("Changes pending: %u\n", system->changes_pending);
    printf("Modified settings: %u\n", settings_get_modified_count(system));
    
    printf("\nSettings by category:\n");
    for (u32 cat = 0; cat < system->category_count; cat++) {
        settings_category *category = &system->categories[cat];
        printf("  %s: %u settings\n", category->name, category->setting_count);
        
        for (u32 i = 0; i < category->setting_count; i++) {
            setting *s = category->settings[i];
            printf("    %s = ", s->name);
            
            switch (s->type) {
                case SETTING_BOOL:
                    printf("%s\n", s->current_value.boolean ? "true" : "false");
                    break;
                case SETTING_INT:
                    printf("%d\n", s->current_value.integer);
                    break;
                case SETTING_FLOAT:
                    printf("%.3f\n", s->current_value.floating);
                    break;
                case SETTING_STRING:
                    printf("\"%s\"\n", s->current_value.string);
                    break;
                case SETTING_ENUM:
                    if (s->current_value.enumeration < s->constraint_data.list.count) {
                        printf("%s\n", s->constraint_data.list.options[s->current_value.enumeration]);
                    } else {
                        printf("invalid(%d)\n", s->current_value.enumeration);
                    }
                    break;
            }
        }
    }
}

// Shutdown settings system
void
settings_shutdown(settings_system *system) {
    if (system) {
        // Auto-save before shutdown
        settings_auto_save(system);
        
        // Clear memory
        memset(system, 0, sizeof(settings_system));
    }
}