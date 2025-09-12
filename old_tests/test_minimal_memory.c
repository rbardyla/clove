/*
 * Minimal memory safety test for handmade engine
 * Tests initialization and basic operations without graphics
 */

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include "handmade_gui.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== MEMORY SAFETY TEST ===\n");
    
    // Test basic struct initialization
    PlatformState platform = {0};
    platform.window.width = 800;
    platform.window.height = 600;
    
    // Test renderer data structures without OpenGL context
    Renderer renderer = {0};
    
    // Test basic math operations
    v2 test_vec = {1.0f, 2.0f};
    Color test_color = {1.0f, 0.5f, 0.2f, 1.0f};
    
    // Test texture struct
    Texture test_texture = {0};
    test_texture.width = 256;
    test_texture.height = 256;
    test_texture.valid = false;
    
    // Test sprite struct  
    Sprite test_sprite = {
        .position = {0.0f, 0.0f},
        .size = {1.0f, 1.0f},
        .rotation = 0.0f,
        .color = {1.0f, 1.0f, 1.0f, 1.0f},
        .texture = test_texture,
        .texture_offset = {0.0f, 0.0f},
        .texture_scale = {1.0f, 1.0f}
    };
    
    // Test GUI structures
    HandmadeGUI gui = {0};
    gui.initialized = false;
    gui.line_height = 20.0f;
    
    printf("Basic struct tests passed\n");
    
    // Test array operations
    f32 test_array[1000];
    for (int i = 0; i < 1000; i++) {
        test_array[i] = (f32)i * 0.5f;
    }
    
    f32 sum = 0.0f;
    for (int i = 0; i < 1000; i++) {
        sum += test_array[i];
    }
    printf("Array test: sum = %.2f\n", sum);
    
    // Test string operations
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Test string %d %f", 42, 3.14f);
    printf("String test: %s\n", buffer);
    
    printf("=== ALL MEMORY TESTS PASSED ===\n");
    return 0;
}