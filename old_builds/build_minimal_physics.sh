#!/bin/bash

# Build script for Minimal Engine with 2D Physics
# Creates a single integrated binary with renderer, GUI, and physics

echo "Building Minimal Engine with 2D Physics..."

# Compiler settings
CC=gcc
CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=c99"
LIBS="-lX11 -lGL -lm -lpthread"

# Build mode (debug or release)
BUILD_MODE=${1:-release}

if [ "$BUILD_MODE" = "debug" ]; then
    echo "Building in DEBUG mode"
    CFLAGS="$CFLAGS -g -O0 -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
    OUTPUT="minimal_engine_physics_debug"
else
    echo "Building in RELEASE mode" 
    CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
    OUTPUT="minimal_engine_physics"
fi

# Source files
SOURCES="main_minimal_physics.c"
SOURCES="$SOURCES handmade_platform_linux.c"
SOURCES="$SOURCES handmade_renderer.c"
SOURCES="$SOURCES handmade_gui.c"
SOURCES="$SOURCES handmade_physics_2d.c"

# Clean old build
rm -f $OUTPUT

# Build
echo "Compiling..."
$CC $CFLAGS $SOURCES -o $OUTPUT $LIBS

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Binary: $OUTPUT"
    echo "Size: $(du -h $OUTPUT | cut -f1)"
    echo ""
    echo "Run with: ./$OUTPUT"
    echo ""
    echo "This integrated version includes:"
    echo "  - 2D Renderer with shapes and sprites"
    echo "  - Immediate-mode GUI system"
    echo "  - 2D Physics with collision detection"
    echo "  - Interactive physics demo"
else
    echo "Build failed!"
    exit 1
fi