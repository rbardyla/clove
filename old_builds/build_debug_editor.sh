#!/bin/bash

echo "Building Debug Editor with Full Validation..."

# Debug build flags with all validation
CC="gcc"
DEBUG_FLAGS="-g3 -O0 -DDEBUG=1 -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
VALIDATION_FLAGS="-fsanitize=address -fsanitize=undefined -fstack-protector-strong"
WARNING_FLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable"
INCLUDES="-I. -Isystems -Isystems/renderer"
LIBS="-lX11 -lXext -lGL -lm -pthread -ldl -lasan -lubsan"

# Ensure directories exist
mkdir -p build

echo "Compiling with full debugging and sanitizers..."

# Create minimal test editor first
cat > build/test_editor.c << 'EOF'
#define _GNU_SOURCE
#define HANDMADE_PLATFORM_IMPLEMENTATION
#include "handmade_platform_linux.c"
#include <stdio.h>
#include <GL/gl.h>

void GameInit(PlatformState* platform) {
    printf("[DEBUG] GameInit called - platform=%p\n", platform);
    if (!platform) {
        printf("[ERROR] Platform is NULL!\n");
        return;
    }
    printf("[DEBUG] Window: %dx%d\n", platform->window.width, platform->window.height);
    printf("[DEBUG] Permanent arena: %p (size: %zu)\n", platform->permanent_arena.base, platform->permanent_arena.size);
}

void GameUpdate(PlatformState* platform, f32 dt) {
    static int frame_count = 0;
    frame_count++;
    
    if (frame_count % 60 == 0) {
        printf("[DEBUG] Frame %d - dt=%.3f\n", frame_count, dt);
    }
    
    // Exit after 300 frames (5 seconds at 60fps)
    if (frame_count > 300) {
        platform->window.should_close = true;
    }
}

void GameRender(PlatformState* platform) {
    // Clear screen to green to verify rendering works
    glClearColor(0.0f, 0.5f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GameShutdown(PlatformState* platform) {
    printf("[DEBUG] GameShutdown called\n");
}
EOF

# Build the test editor
$CC $DEBUG_FLAGS $VALIDATION_FLAGS $WARNING_FLAGS $INCLUDES \
    build/test_editor.c $LIBS -o build/debug_test_editor

if [ $? -eq 0 ]; then
    echo "✓ Debug test editor built successfully!"
    echo ""
    echo "DEBUGGING FEATURES:"
    echo "  ✓ Address Sanitizer - Catches memory corruption"
    echo "  ✓ Undefined Behavior Sanitizer - Catches UB"
    echo "  ✓ Stack protector - Prevents buffer overflows"
    echo "  ✓ Full debug symbols - Complete stack traces"
    echo "  ✓ Verbose logging - All operations logged"
    echo ""
    echo "Run with: ./build/debug_test_editor"
    echo "Expected: Green screen for 5 seconds, then clean exit"
else
    echo "✗ Debug build failed"
    exit 1
fi