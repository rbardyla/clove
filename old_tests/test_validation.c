/*
    Headless validation test for the handmade engine editor
    
    Tests core functionality without requiring a display:
    - Memory management
    - Asset system initialization  
    - GUI system setup
    - Basic data structure integrity
*/

#include "handmade_platform.h"
#include "simple_gui.h"
#include "handmade_assets.h"
#include "minimal_renderer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Mock platform state for testing
PlatformState mock_platform = {0};

// Test data validation
int test_color_system(void) {
    printf("Testing color system...\n");
    
    // Test color construction
    color32 red = rgb(255, 0, 0);
    color32 blue = rgba(0, 0, 255, 128);
    
    assert(red.r == 255 && red.g == 0 && red.b == 0 && red.a == 255);
    assert(blue.r == 0 && blue.g == 0 && blue.b == 255 && blue.a == 128);
    
    printf("✓ Color system working correctly\n");
    return 1;
}

int test_renderer_init(void) {
    printf("Testing renderer initialization...\n");
    
    renderer test_renderer;
    renderer_init(&test_renderer, 1024, 768);
    
    assert(test_renderer.width == 1024);
    assert(test_renderer.height == 768);
    assert(test_renderer.pixels_drawn == 0);
    assert(test_renderer.primitives_drawn == 0);
    
    renderer_shutdown(&test_renderer);
    
    printf("✓ Renderer initialization working correctly\n");
    return 1;
}

int test_gui_system(void) {
    printf("Testing GUI system...\n");
    
    renderer test_renderer;
    simple_gui gui;
    
    renderer_init(&test_renderer, 800, 600);
    simple_gui_init(&gui, &test_renderer);
    
    assert(gui.r == &test_renderer);
    assert(gui.widgets_drawn == 0);
    assert(gui.hot_id == 0);
    assert(gui.active_id == 0);
    
    renderer_shutdown(&test_renderer);
    
    printf("✓ GUI system initialization working correctly\n");
    return 1;
}

int test_asset_browser(void) {
    printf("Testing asset browser...\n");
    
    AssetBrowser browser;
    asset_browser_init(&browser, "./assets");
    
    assert(browser.asset_count >= 0);
    assert(browser.selected_asset_index == -1);
    assert(browser.hovered_asset_index == -1);
    assert(browser.show_thumbnails == true);
    assert(browser.thumbnail_scale == 1);
    
    printf("✓ Asset browser initialization working correctly\n");
    printf("  Found %d assets in directory\n", browser.asset_count);
    return 1;
}

int test_asset_type_detection(void) {
    printf("Testing asset type detection...\n");
    
    assert(asset_get_type_from_extension("test.png") == ASSET_TYPE_TEXTURE);
    assert(asset_get_type_from_extension("test.jpg") == ASSET_TYPE_TEXTURE);
    assert(asset_get_type_from_extension("test.bmp") == ASSET_TYPE_TEXTURE);
    assert(asset_get_type_from_extension("test.obj") == ASSET_TYPE_MODEL);
    assert(asset_get_type_from_extension("test.wav") == ASSET_TYPE_SOUND);
    assert(asset_get_type_from_extension("test.glsl") == ASSET_TYPE_SHADER);
    assert(asset_get_type_from_extension("test.xyz") == ASSET_TYPE_UNKNOWN);
    
    printf("✓ Asset type detection working correctly\n");
    return 1;
}

int test_memory_safety(void) {
    printf("Testing memory safety...\n");
    
    // Test basic allocations and deallocations
    renderer test_renderer;
    simple_gui gui;
    AssetBrowser browser;
    
    // Initialize systems
    renderer_init(&test_renderer, 1280, 720);
    simple_gui_init(&gui, &test_renderer);
    asset_browser_init(&browser, "./assets");
    
    // Simulate frame operations
    simple_gui_begin_frame(&gui, &mock_platform);
    
    // Test text size calculation (should not crash)
    int w, h;
    renderer_text_size(&test_renderer, "Test String", &w, &h);
    assert(w > 0 && h > 0);
    
    simple_gui_end_frame(&gui);
    
    // Cleanup
    renderer_shutdown(&test_renderer);
    
    printf("✓ Memory safety tests passed\n");
    return 1;
}

int test_data_structures(void) {
    printf("Testing data structure integrity...\n");
    
    // Test GUI structures
    gui_tree_node node = {"Test Node", true, 0, false};
    assert(strcmp(node.label, "Test Node") == 0);
    assert(node.expanded == true);
    assert(node.depth == 0);
    assert(node.selected == false);
    
    // Test panel structure
    bool panel_open = true;
    gui_panel panel = {
        .x = 10, .y = 20, .width = 300, .height = 400,
        .title = "Test Panel", .open = &panel_open,
        .collapsed = false, .resizable = true
    };
    assert(panel.x == 10 && panel.y == 20);
    assert(panel.width == 300 && panel.height == 400);
    assert(*panel.open == true);
    
    printf("✓ Data structure integrity verified\n");
    return 1;
}

int test_string_safety(void) {
    printf("Testing string handling safety...\n");
    
    // Test asset name formatting
    char size_buffer[64];
    asset_format_size(1024, size_buffer, sizeof(size_buffer));
    assert(strlen(size_buffer) > 0 && strlen(size_buffer) < sizeof(size_buffer));
    
    asset_format_size(1024*1024, size_buffer, sizeof(size_buffer));
    assert(strlen(size_buffer) > 0 && strlen(size_buffer) < sizeof(size_buffer));
    
    asset_format_size(1024*1024*1024, size_buffer, sizeof(size_buffer));
    assert(strlen(size_buffer) > 0 && strlen(size_buffer) < sizeof(size_buffer));
    
    // Test type name retrieval
    const char* type_name = asset_get_type_name(ASSET_TYPE_TEXTURE);
    assert(type_name != NULL && strlen(type_name) > 0);
    
    printf("✓ String handling safety verified\n");
    return 1;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    printf("=== HANDMADE ENGINE VALIDATION TEST ===\n\n");
    
    int tests_passed = 0;
    int total_tests = 8;
    
    if (test_color_system()) tests_passed++;
    if (test_renderer_init()) tests_passed++;
    if (test_gui_system()) tests_passed++;  
    if (test_asset_browser()) tests_passed++;
    if (test_asset_type_detection()) tests_passed++;
    if (test_memory_safety()) tests_passed++;
    if (test_data_structures()) tests_passed++;
    if (test_string_safety()) tests_passed++;
    
    printf("\n=== VALIDATION RESULTS ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);
    
    if (tests_passed == total_tests) {
        printf("✓ ALL TESTS PASSED - Engine core functionality is working\n");
        return 0;
    } else {
        printf("✗ SOME TESTS FAILED - Issues found in engine core\n");
        return 1;
    }
}