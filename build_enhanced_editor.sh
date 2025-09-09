#!/bin/bash

echo "Building Enhanced Editor with Integrated Renderer..."

# Use optimized build flags  
CC="gcc"
CFLAGS="-std=c11 -O2 -march=native -mavx2 -mfma -ffast-math -Wall -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -g"
INCLUDES="-I. -Isystems -Isystems/renderer"
LIBS="-lX11 -lXext -lGL -lm -pthread -ldl"

# Ensure directories exist
mkdir -p build
mkdir -p assets/shaders

# Create unity build file for simple enhanced editor
UNITY_FILE="build/simple_enhanced_editor_unity.c"
echo "// Simple Enhanced Editor - Generated at $(date)" > $UNITY_FILE
echo "" >> $UNITY_FILE
echo "#define _GNU_SOURCE" >> $UNITY_FILE
echo "#define HANDMADE_PLATFORM_IMPLEMENTATION" >> $UNITY_FILE
echo "#include \"handmade_platform_linux.c\"" >> $UNITY_FILE
echo "" >> $UNITY_FILE
echo "// Simple Enhanced Editor" >> $UNITY_FILE
echo "#include \"simple_enhanced_editor.c\"" >> $UNITY_FILE

# Build the enhanced editor
echo "Compiling simple enhanced editor with immediate mode renderer..."
$CC $CFLAGS $INCLUDES -o build/enhanced_editor $UNITY_FILE $LIBS

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