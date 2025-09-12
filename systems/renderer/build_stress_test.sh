#!/bin/bash

# Build the renderer stress test
echo "Building Renderer Stress Test..."

# Compiler flags
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
CFLAGS="$CFLAGS -I. -I../../src -I../platform -I../math"
CFLAGS="$CFLAGS -DHANDMADE_INTERNAL=1"

# Performance build by default (need accurate measurements)
BUILD_MODE=${1:-release}

if [ "$BUILD_MODE" = "debug" ]; then
    CFLAGS="$CFLAGS -g -O0 -DHANDMADE_DEBUG=1"
    echo "Building in DEBUG mode..."
else
    CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
    echo "Building in RELEASE mode (required for accurate measurements)..."
fi

# OpenGL and X11 libraries
LIBS="-lGL -lX11 -lm -lpthread -ldl"

# Source files
SOURCES="renderer_stress_test.c"
SOURCES="$SOURCES handmade_renderer.c"
SOURCES="$SOURCES handmade_opengl.c"
SOURCES="$SOURCES handmade_platform_linux.c"
SOURCES="$SOURCES handmade_math.c"

# Output
OUTPUT="renderer_stress_test"

# Clean old build
rm -f $OUTPUT

# Compile
gcc $CFLAGS $SOURCES $LIBS -o $OUTPUT

if [ $? -eq 0 ]; then
    echo "✓ Build successful: $OUTPUT"
    echo ""
    echo "Run with: ./$OUTPUT"
    echo ""
    echo "NOTE: For accurate measurements:"
    echo "  1. Close unnecessary applications"
    echo "  2. Disable compositor if possible"
    echo "  3. Ensure CPU governor is set to performance"
    echo "  4. Run in release mode (default)"
else
    echo "✗ Build failed!"
    exit 1
fi