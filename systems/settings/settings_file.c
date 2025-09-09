/*
    Settings File I/O Implementation
    Binary format with validation and profiles
*/

#include "handmade_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal static

// Settings file header
typedef struct settings_file_header {
    u32 magic;           // SETTINGS_MAGIC_NUMBER
    u32 version;         // SETTINGS_VERSION
    u32 setting_count;   // Number of settings
    u32 checksum;        // CRC32 of data
    u64 timestamp;       // Creation time
    char profile_name[64];
} settings_file_header;

// Simple CRC32 implementation
internal u32
settings_crc32(u8 *data, u32 size) {
    static u32 crc_table[256] = {0}; // Would be initialized
    u32 crc = 0xFFFFFFFF;
    
    // Simplified CRC calculation
    for (u32 i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    
    return crc ^ 0xFFFFFFFF;
}

// Save settings to file
b32
settings_save_to_file(settings_system *system, char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return 0;
    }
    
    // Write header
    settings_file_header header = {0};
    header.magic = SETTINGS_MAGIC_NUMBER;
    header.version = SETTINGS_VERSION;
    header.setting_count = system->setting_count;
    header.timestamp = 0; // Would use real timestamp
    
    if (system->active_profile >= 0 && system->active_profile < system->profile_count) {
        strncpy(header.profile_name, system->profiles[system->active_profile].name, 63);
    }
    
    fwrite(&header, sizeof(header), 1, file);
    
    // Write settings data
    for (u32 i = 0; i < system->setting_count; i++) {
        setting *s = &system->settings[i];
        
        // Write setting info
        fwrite(s->name, SETTINGS_STRING_MAX, 1, file);
        fwrite(&s->type, sizeof(s->type), 1, file);
        fwrite(&s->current_value, sizeof(s->current_value), 1, file);
    }
    
    fclose(file);
    
    printf("Settings saved to: %s (%u settings)\n", path, system->setting_count);
    return 1;
}

// Load settings from file
b32
settings_load_from_file(settings_system *system, char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("Settings file not found: %s\n", path);
        return 0;
    }
    
    // Read header
    settings_file_header header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return 0;
    }
    
    // Validate header
    if (header.magic != SETTINGS_MAGIC_NUMBER) {
        printf("Invalid settings file magic number\n");
        fclose(file);
        return 0;
    }
    
    if (header.version != SETTINGS_VERSION) {
        printf("Settings file version mismatch: %u (expected %u)\n", 
               header.version, SETTINGS_VERSION);
        fclose(file);
        return 0;
    }
    
    printf("Loading settings from: %s (%u settings)\n", path, header.setting_count);
    
    // Read settings data
    for (u32 i = 0; i < header.setting_count; i++) {
        char name[SETTINGS_STRING_MAX];
        setting_type type;
        setting_value value;
        
        fread(name, SETTINGS_STRING_MAX, 1, file);
        fread(&type, sizeof(type), 1, file);
        fread(&value, sizeof(value), 1, file);
        
        // Find matching setting in current system
        setting *s = settings_find_by_name(system, name);
        if (s && s->type == type) {
            // Validate value before setting
            switch (type) {
                case SETTING_INT:
                    if (value.integer >= s->min_value.integer && 
                        value.integer <= s->max_value.integer) {
                        s->current_value = value;
                    }
                    break;
                    
                case SETTING_FLOAT:
                    if (value.floating >= s->min_value.floating && 
                        value.floating <= s->max_value.floating) {
                        s->current_value = value;
                    }
                    break;
                    
                case SETTING_ENUM:
                    if (value.enumeration >= 0 && 
                        value.enumeration < s->constraint_data.list.count) {
                        s->current_value = value;
                    }
                    break;
                    
                default:
                    s->current_value = value;
                    break;
            }
        } else if (s) {
            printf("Warning: Setting '%s' type mismatch, skipping\n", name);
        } else {
            printf("Warning: Unknown setting '%s', skipping\n", name);
        }
    }
    
    fclose(file);
    
    // Validate all settings after loading
    if (!settings_validate_all(system)) {
        printf("Warning: Some settings had invalid values and were reset\n");
    }
    
    return 1;
}

// Auto-save settings
b32
settings_auto_save(settings_system *system) {
    return settings_save_to_file(system, system->config_path);
}

// Auto-load settings
b32
settings_auto_load(settings_system *system) {
    return settings_load_from_file(system, system->config_path);
}

// Export profile to file
b32
settings_export_profile(settings_system *system, i32 profile_index, char *path) {
    if (profile_index < 0 || profile_index >= system->profile_count) {
        return 0;
    }
    
    // Temporarily switch to this profile for export
    i32 old_active = system->active_profile;
    settings_activate_profile(system, profile_index);
    
    b32 result = settings_save_to_file(system, path);
    
    // Restore original profile
    settings_activate_profile(system, old_active);
    
    return result;
}

// Import profile from file
i32
settings_import_profile(settings_system *system, char *path) {
    // Create new profile
    if (system->profile_count >= SETTINGS_MAX_PROFILES) {
        return -1;
    }
    
    // Load into temporary system to get profile name
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    
    settings_file_header header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);
    
    // Create new profile
    i32 profile_index = system->profile_count++;
    settings_profile *profile = &system->profiles[profile_index];
    
    strncpy(profile->name, header.profile_name, SETTINGS_STRING_MAX - 1);
    if (strlen(profile->name) == 0) {
        snprintf(profile->name, SETTINGS_STRING_MAX, "Imported Profile %d", profile_index);
    }
    
    strncpy(profile->description, "Imported settings profile", 127);
    profile->created_time = header.timestamp;
    profile->modified_time = header.timestamp;
    
    // Load settings into this profile
    i32 old_active = system->active_profile;
    settings_activate_profile(system, profile_index);
    
    b32 load_success = settings_load_from_file(system, path);
    
    if (!load_success) {
        // Remove failed profile
        system->profile_count--;
        settings_activate_profile(system, old_active);
        return -1;
    }
    
    return profile_index;
}

// Create new profile
i32
settings_create_profile(settings_system *system, char *name, char *description) {
    if (system->profile_count >= SETTINGS_MAX_PROFILES) {
        return -1;
    }
    
    i32 profile_index = system->profile_count++;
    settings_profile *profile = &system->profiles[profile_index];
    
    strncpy(profile->name, name, SETTINGS_STRING_MAX - 1);
    strncpy(profile->description, description, 127);
    profile->created_time = 0; // Would use real timestamp
    profile->modified_time = 0;
    profile->active = 0;
    
    // Initialize with current settings as defaults
    for (u32 i = 0; i < system->setting_count; i++) {
        profile->overrides[i] = system->settings[i].current_value;
        profile->has_override[i] = 0; // Start with no overrides
    }
    
    printf("Created profile '%s' (index %d)\n", name, profile_index);
    return profile_index;
}

// Activate profile
b32
settings_activate_profile(settings_system *system, i32 profile_index) {
    if (profile_index < 0 || profile_index >= system->profile_count) {
        return 0;
    }
    
    // Save current profile overrides
    if (system->active_profile >= 0) {
        settings_profile *old_profile = &system->profiles[system->active_profile];
        for (u32 i = 0; i < system->setting_count; i++) {
            setting *s = &system->settings[i];
            // Check if value differs from default
            b32 is_different = 0;
            
            switch (s->type) {
                case SETTING_BOOL:
                    is_different = (s->current_value.boolean != s->default_value.boolean);
                    break;
                case SETTING_INT:
                    is_different = (s->current_value.integer != s->default_value.integer);
                    break;
                case SETTING_FLOAT:
                    is_different = (s->current_value.floating != s->default_value.floating);
                    break;
                case SETTING_STRING:
                    is_different = (strcmp(s->current_value.string, s->default_value.string) != 0);
                    break;
                case SETTING_ENUM:
                    is_different = (s->current_value.enumeration != s->default_value.enumeration);
                    break;
            }
            
            if (is_different) {
                old_profile->overrides[i] = s->current_value;
                old_profile->has_override[i] = 1;
            }
        }
        old_profile->active = 0;
        old_profile->modified_time = 0; // Would use real timestamp
    }
    
    // Load new profile
    settings_profile *new_profile = &system->profiles[profile_index];
    for (u32 i = 0; i < system->setting_count; i++) {
        if (new_profile->has_override[i]) {
            system->settings[i].current_value = new_profile->overrides[i];
        } else {
            system->settings[i].current_value = system->settings[i].default_value;
        }
    }
    
    new_profile->active = 1;
    system->active_profile = profile_index;
    
    printf("Activated profile '%s'\n", new_profile->name);
    return 1;
}

// Delete profile
b32
settings_delete_profile(settings_system *system, i32 profile_index) {
    if (profile_index < 0 || profile_index >= system->profile_count) {
        return 0;
    }
    
    if (profile_index == 0) {
        printf("Cannot delete default profile\n");
        return 0;
    }
    
    if (profile_index == system->active_profile) {
        // Switch to default profile first
        settings_activate_profile(system, 0);
    }
    
    // Remove profile by shifting array
    for (i32 i = profile_index; i < system->profile_count - 1; i++) {
        system->profiles[i] = system->profiles[i + 1];
    }
    system->profile_count--;
    
    // Update active profile index if needed
    if (system->active_profile > profile_index) {
        system->active_profile--;
    }
    
    printf("Deleted profile (index %d)\n", profile_index);
    return 1;
}