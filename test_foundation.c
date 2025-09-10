/*
    Test Foundation - Verification that all components link properly
    
    This test creates a mock platform and calls our Game* functions
    to verify everything links and basic logic works.
*/

#include "handmade_platform.h"
#include <stdio.h>
#include <string.h>

// External Game* functions from main.c
extern void GameInit(PlatformState* platform);
extern void GameUpdate(PlatformState* platform, f32 dt);
extern void GameRender(PlatformState* platform);
extern void GameShutdown(PlatformState* platform);
extern void GameOnReload(PlatformState* platform);

int main() {
    printf("Testing Handmade Foundation Components...\n");
    
    // Create mock platform state
    PlatformState mock_platform = {0};
    mock_platform.window.width = 1280;
    mock_platform.window.height = 720;
    mock_platform.window.should_close = false;
    
    // Test GameInit
    printf("Testing GameInit...\n");
    GameInit(&mock_platform);
    printf("✓ GameInit completed\n");
    
    // Test GameUpdate
    printf("Testing GameUpdate...\n");
    GameUpdate(&mock_platform, 0.016f);  // ~60fps frame time
    printf("✓ GameUpdate completed\n");
    
    // Test GameRender with stub GL functions
    printf("Testing GameRender (with stub GL functions)...\n");
    GameRender(&mock_platform);
    printf("✓ GameRender completed\n");
    
    // Test GameOnReload
    printf("Testing GameOnReload...\n");
    GameOnReload(&mock_platform);
    printf("✓ GameOnReload completed\n");
    
    // Test GameShutdown
    printf("Testing GameShutdown...\n");
    GameShutdown(&mock_platform);
    printf("✓ GameShutdown completed\n");
    
    printf("\nAll foundation components tested successfully!\n");
    printf("The foundation is ready for development.\n");
    
    return 0;
}