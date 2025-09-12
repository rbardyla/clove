#!/bin/bash

# Build script for 2D Physics Demo
# Integrates with minimal engine

echo "Building 2D Physics Demo..."

# Compiler settings
CC=gcc
CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=c99"
LIBS="-lX11 -lGL -lm -lpthread"

# Build mode (debug or release)
BUILD_MODE=${1:-release}

if [ "$BUILD_MODE" = "debug" ]; then
    echo "Building in DEBUG mode"
    CFLAGS="$CFLAGS -g -O0 -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
else
    echo "Building in RELEASE mode"
    CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
fi

# Source files
SOURCES="physics_2d_demo.c"
SOURCES="$SOURCES handmade_platform_linux.c"
SOURCES="$SOURCES handmade_renderer.c"
SOURCES="$SOURCES handmade_gui.c"
SOURCES="$SOURCES handmade_physics_2d.c"

# Output binary
OUTPUT="physics_2d_demo"

# Clean old build
rm -f $OUTPUT

# Build
echo "Compiling..."
$CC $CFLAGS $SOURCES -o $OUTPUT $LIBS

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Binary size: $(du -h $OUTPUT | cut -f1)"
    echo ""
    echo "Run with: ./$OUTPUT"
else
    echo "Build failed!"
    exit 1
fi