#!/bin/bash

echo "Building Enhanced Handmade Game Editor..."

# Use the same build flags as the working renderer
CC="gcc"
CFLAGS="-O2 -march=native -mavx2 -mfma -ffast-math -Wall -Wno-unused-parameter -Wno-unused-function -g"
INCLUDES="-I. -Isystems -Isystems/renderer"
LIBS="-lX11 -lXext -lGL -lm -pthread"

# Build the enhanced editor
echo "Compiling enhanced editor with real panels..."
$CC $CFLAGS $INCLUDES -o enhanced_editor \
    enhanced_editor.c \
    systems/renderer/handmade_math.c \
    systems/renderer/handmade_platform_linux.c \
    systems/renderer/handmade_renderer.c \
    systems/renderer/handmade_opengl.c \
    $LIBS

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Run with: ./enhanced_editor"
    echo ""
    echo "FEATURES:"
    echo "  • Scene Hierarchy Panel - View and select game objects"
    echo "  • Property Inspector - Edit object properties"  
    echo "  • Console Output - See editor messages"
    echo "  • Toolbar - File/Edit menus and play controls"
    echo "  • 3D Viewport - View your scene"
    echo ""
    echo "CONTROLS:"
    echo "  • Click objects in hierarchy to select"
    echo "  • Use toolbar buttons to change tools"
    echo "  • Press Play button to start game simulation"
    echo "  • DELETE key - Delete selected object"
    echo "  • 1-4 keys - Quick tool selection"
    echo "  • ESC - Exit"
else
    echo "✗ Build failed"
    exit 1
fi