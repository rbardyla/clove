#!/bin/bash

# Handmade Engine Build Script
# Builds the engine with basic renderer capabilities

set -e

echo "=== BUILDING HANDMADE ENGINE WITH RENDERER ==="
echo ""

# Compiler flags following handmade philosophy
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter"
LIBS="-lGL -lX11 -lpthread -lm -ldl"

# Build mode
BUILD_MODE=${1:-debug}

if [ "$BUILD_MODE" = "release" ]; then
    CFLAGS="$CFLAGS -O3 -DNDEBUG -march=native"
    echo "Building RELEASE version (optimized)"
else
    CFLAGS="$CFLAGS -g -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
    echo "Building DEBUG version"
fi

echo "Compiler flags: $CFLAGS"
echo ""

# Clean previous build
rm -f minimal_engine

# Compile
echo "Compiling engine with renderer and GUI..."
gcc $CFLAGS main_minimal.c handmade_renderer.c handmade_gui.c handmade_platform_linux.c -o minimal_engine $LIBS

if [ $? -eq 0 ]; then
    echo ""
    echo "=== BUILD SUCCESSFUL ==="
    echo ""
    echo "Executable: ./minimal_engine"
    echo "Size: $(ls -lh minimal_engine | awk '{print $5}')"
    echo ""
    echo "Controls:"
    echo "  ESC - Quit"
    echo "  SPACE - Print debug info"
    echo "  WASD - Move camera"
    echo "  Q/E - Zoom camera"
    echo "  G - Toggle GUI debug panel"
    echo "  H - Toggle GUI demo panel"
    echo ""
    echo "Renderer features:"
    echo "  ✓ Basic shapes (triangles, quads, circles, lines)"
    echo "  ✓ 2D sprite rendering with textures"
    echo "  ✓ BMP texture loading support"
    echo "  ✓ Basic 2D camera with zoom and pan"
    echo "  ✓ Immediate mode rendering (no batching yet)"
    echo "  ✓ Text rendering with bitmap fonts"
    echo ""
    echo "GUI features:"
    echo "  ✓ Immediate-mode GUI system"
    echo "  ✓ Button and checkbox widgets"
    echo "  ✓ Panel system with dragging"
    echo "  ✓ Text labels and debug info"
    echo "  ✓ Built on working renderer system"
    echo ""
    echo "Following handmade philosophy:"
    echo "  ✓ Always have something working"
    echo "  ✓ Understand every line of code" 
    echo "  ✓ No external dependencies (except OS/OpenGL)"
    echo "  ✓ Performance first approach"
    echo ""
else
    echo "BUILD FAILED"
    exit 1
fi