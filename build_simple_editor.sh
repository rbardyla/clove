#!/bin/bash

echo "Building Simple Handmade Game Editor..."
echo "Using the proven working renderer as foundation"

# Use the same build flags as the working renderer
CC="gcc"
CFLAGS="-O2 -march=native -mavx2 -mfma -ffast-math -Wall -Wno-unused-parameter -Wno-unused-function -g"
INCLUDES="-I. -Isystems -Isystems/renderer"
LIBS="-lX11 -lXext -lGL -lm -pthread"

# Build using the same pattern as renderer_test
echo "Compiling simple editor..."
$CC $CFLAGS $INCLUDES -o simple_editor \
    simple_editor.c \
    systems/renderer/handmade_math.c \
    systems/renderer/handmade_platform_linux.c \
    systems/renderer/handmade_renderer.c \
    systems/renderer/handmade_opengl.c \
    $LIBS

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "Run with: ./simple_editor"
    echo ""
    echo "Controls:"
    echo "  F1: Toggle menu"
    echo "  SPACE: Start/Stop game"
    echo "  ESC: Quit"
else
    echo "✗ Build failed"
    exit 1
fi