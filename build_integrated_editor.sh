#!/bin/bash

echo "Building Integrated Editor (GUI + Renderer)..."

# Use optimized build flags  
CC="gcc"
CFLAGS="-std=c11 -O2 -march=native -Wall -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -g"
INCLUDES="-I. -Isystems -Isystems/renderer"
LIBS="-lX11 -lXext -lGL -lm -pthread -ldl"

# Ensure directories exist
mkdir -p build

# Create unity build file for integrated editor
UNITY_FILE="build/integrated_editor_unity.c"
echo "// Integrated Editor (GUI + Renderer) - Generated at $(date)" > $UNITY_FILE
echo "" >> $UNITY_FILE
echo "#define _GNU_SOURCE" >> $UNITY_FILE
echo "#define HANDMADE_PLATFORM_IMPLEMENTATION" >> $UNITY_FILE
echo "#include \"handmade_platform_linux.c\"" >> $UNITY_FILE
echo "" >> $UNITY_FILE
echo "// Include GL headers" >> $UNITY_FILE
echo "#include <GL/gl.h>" >> $UNITY_FILE
echo "" >> $UNITY_FILE
echo "// Editor GUI System" >> $UNITY_FILE
echo "#include \"editor_gui.c\"" >> $UNITY_FILE
echo "" >> $UNITY_FILE
echo "// Integrated Editor" >> $UNITY_FILE
echo "#include \"integrated_editor.c\"" >> $UNITY_FILE

# Build the integrated editor
echo "Compiling integrated editor with GUI and renderer..."
$CC $CFLAGS $INCLUDES -o build/integrated_editor $UNITY_FILE $LIBS

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "INTEGRATED EDITOR FEATURES:"
    echo "  ✓ 3D Scene Viewport - Interactive camera controls"
    echo "  ✓ Scene Hierarchy Panel - Expandable object tree"
    echo "  ✓ Property Inspector - Live parameter editing"
    echo "  ✓ Console Panel - Real-time logging system"
    echo "  ✓ Performance Panel - FPS/frame time graphs"
    echo "  ✓ Material Editor - Live color/property tweaking"
    echo "  ✓ Professional Layout - Resizable, dockable panels"
    echo ""
    echo "CONTROLS:"
    echo "  F1-F4: Toggle panels (Hierarchy, Inspector, Console, Performance)"
    echo "  WASD: Camera rotation"
    echo "  Mouse: Interactive GUI controls"
    echo "  ESC: Exit editor"
    echo ""
    echo "Run with: ./build/integrated_editor"
    echo ""
    echo "🎯 Professional Editor Ready!"
else
    echo "✗ Build failed"
    exit 1
fi