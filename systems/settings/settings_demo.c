/*
    Settings System Demo
    Shows complete settings management functionality
*/

#include "handmade_settings.h"
#include <stdio.h>
#include <stdlib.h>

// Stub types for demo
typedef struct input_state {
    b32 keys[256];
    i32 mouse_x, mouse_y;
    b32 mouse_buttons[3];
} input_state;

// Demo main function
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    printf("=== Handmade Settings System Demo ===\n\n");
    
    // Allocate memory for settings system
    u32 memory_size = 1024 * 1024; // 1MB
    void *memory = malloc(memory_size);
    if (!memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Initialize settings system
    settings_system *settings = settings_init(memory, memory_size);
    if (!settings) {
        printf("Failed to initialize settings system\n");
        free(memory);
        return 1;
    }
    
    printf("Settings system initialized\n");
    printf("Memory allocated: %u KB\n", memory_size / 1024);
    
    // Register all default settings
    printf("\nRegistering default settings...\n");
    settings_register_all_defaults(settings);
    
    printf("Registered %u settings in %u categories\n", 
           settings->setting_count, settings->category_count);
    
    // Show initial state
    printf("\n=== Initial Settings State ===\n");
    settings_dump_all(settings);
    
    // Demonstrate setting values
    printf("\n=== Testing Setting Values ===\n");
    
    // Test boolean settings
    printf("Original fullscreen: %s\n", 
           settings_get_bool(settings, "fullscreen") ? "true" : "false");
    settings_set_bool(settings, "fullscreen", 1);
    printf("After setting fullscreen: %s\n", 
           settings_get_bool(settings, "fullscreen") ? "true" : "false");
    
    // Test integer settings
    printf("Original FOV: %d\n", settings_get_int(settings, "fov"));
    settings_set_int(settings, "fov", 110);
    printf("After setting FOV: %d\n", settings_get_int(settings, "fov"));
    
    // Test float settings
    printf("Original mouse sensitivity: %.2f\n", 
           settings_get_float(settings, "mouse_sensitivity"));
    settings_set_float(settings, "mouse_sensitivity", 1.5f);
    printf("After setting mouse sensitivity: %.2f\n", 
           settings_get_float(settings, "mouse_sensitivity"));
    
    // Show modified settings count
    printf("\nModified settings: %u\n", settings_get_modified_count(settings));
    
    // Test validation
    printf("\n=== Testing Validation ===\n");
    settings_set_int(settings, "fov", 200); // Should be clamped
    printf("FOV after setting to 200: %d (should be clamped to 120)\n", 
           settings_get_int(settings, "fov"));
    
    // Test profiles
    printf("\n=== Testing Profiles ===\n");
    i32 gaming_profile = settings_create_profile(settings, "Gaming", 
                                                "High performance gaming settings");
    i32 casual_profile = settings_create_profile(settings, "Casual", 
                                                "Balanced settings for casual play");
    
    if (gaming_profile >= 0) {
        printf("Created gaming profile (index %d)\n", gaming_profile);
        
        // Switch to gaming profile and change some settings
        settings_activate_profile(settings, gaming_profile);
        settings_set_int(settings, "fov", 120);
        settings_set_bool(settings, "reduce_input_lag", 1);
        settings_set_float(settings, "mouse_sensitivity", 2.0f);
        
        printf("Gaming profile settings applied\n");
        printf("  FOV: %d\n", settings_get_int(settings, "fov"));
        printf("  Mouse sensitivity: %.2f\n", 
               settings_get_float(settings, "mouse_sensitivity"));
    }
    
    if (casual_profile >= 0) {
        printf("Created casual profile (index %d)\n", casual_profile);
        
        // Switch to casual profile
        settings_activate_profile(settings, casual_profile);
        settings_set_int(settings, "fov", 90);
        settings_set_bool(settings, "motion_blur", 1);
        settings_set_float(settings, "mouse_sensitivity", 1.0f);
        
        printf("Casual profile settings applied\n");
        printf("  FOV: %d\n", settings_get_int(settings, "fov"));
        printf("  Mouse sensitivity: %.2f\n", 
               settings_get_float(settings, "mouse_sensitivity"));
    }
    
    // Switch back to gaming profile to show persistence
    if (gaming_profile >= 0) {
        settings_activate_profile(settings, gaming_profile);
        printf("Switched back to gaming profile\n");
        printf("  FOV: %d (should be 120)\n", settings_get_int(settings, "fov"));
        printf("  Mouse sensitivity: %.2f (should be 2.0)\n", 
               settings_get_float(settings, "mouse_sensitivity"));
    }
    
    // Test file I/O
    printf("\n=== Testing File I/O ===\n");
    
    // Save current settings
    if (settings_save_to_file(settings, "test_settings.cfg")) {
        printf("Settings saved successfully\n");
        
        // Modify some settings
        settings_set_bool(settings, "fullscreen", 0);
        settings_set_int(settings, "fov", 75);
        
        printf("Modified settings after save:\n");
        printf("  Fullscreen: %s\n", 
               settings_get_bool(settings, "fullscreen") ? "true" : "false");
        printf("  FOV: %d\n", settings_get_int(settings, "fov"));
        
        // Load settings back
        if (settings_load_from_file(settings, "test_settings.cfg")) {
            printf("Settings loaded successfully\n");
            printf("Restored settings:\n");
            printf("  Fullscreen: %s (should be true)\n", 
                   settings_get_bool(settings, "fullscreen") ? "true" : "false");
            printf("  FOV: %d (should be 120)\n", settings_get_int(settings, "fov"));
        }
    }
    
    // Test reset functionality
    printf("\n=== Testing Reset Functionality ===\n");
    printf("Modified settings before reset: %u\n", 
           settings_get_modified_count(settings));
    
    settings_reset_to_defaults(settings);
    printf("Modified settings after reset: %u\n", 
           settings_get_modified_count(settings));
    
    // Show final state
    printf("\n=== Final Settings Summary ===\n");
    for (u32 i = 0; i < settings->category_count; i++) {
        settings_category *cat = &settings->categories[i];
        printf("Category '%s': %u settings\n", cat->name, cat->setting_count);
    }
    
    printf("\nTotal settings: %u\n", settings->setting_count);
    printf("Total profiles: %u\n", settings->profile_count);
    printf("Active profile: %s\n", 
           settings->profiles[settings->active_profile].name);
    
    // Test UI rendering (just prints to console)
    printf("\n=== UI Demo ===\n");
    settings_show_menu(settings);
    
    // Create stub GUI context
    typedef struct gui_context {
        i32 x, y, width, height;
        u32 hot_id, active_id;
        b32 mouse_down;
        i32 mouse_x, mouse_y;
    } gui_context;
    
    gui_context gui = {0};
    gui.width = 800;
    gui.height = 600;
    gui.mouse_x = 400;
    gui.mouse_y = 300;
    
    settings_render_ui(settings, &gui);
    
    // Cleanup
    printf("\n=== Cleanup ===\n");
    settings_shutdown(settings);
    free(memory);
    
    printf("Settings system demo completed successfully!\n");
    
    return 0;
}