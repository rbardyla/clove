#!/bin/bash

# Build script for headless renderer benchmark
# No OpenGL/X11 dependencies - pure CPU benchmarking

set -e

echo "Building headless renderer benchmark..."

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
DEBUG_FLAGS="-g -DHANDMADE_DEBUG=1"
WARNINGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
INCLUDES="-I. -I../../src"

# Choose build type
BUILD_TYPE=${1:-release}

if [ "$BUILD_TYPE" = "debug" ]; then
    echo "Building debug version..."
    FLAGS="$DEBUG_FLAGS $WARNINGS $INCLUDES"
else
    echo "Building release version..."
    FLAGS="$CFLAGS $WARNINGS $INCLUDES"
fi

# Build the benchmark (no OpenGL/X11 libs needed!)
$CC $FLAGS \
    renderer_benchmark_headless.c \
    -o renderer_benchmark_headless \
    -lm \
    -lpthread

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo ""
    echo "Run with: ./renderer_benchmark_headless"
    echo ""
    echo "This benchmark measures:"
    echo "  - Matrix multiplication performance"
    echo "  - Transform operations"
    echo "  - Frustum culling algorithms"
    echo "  - Draw call batching logic"
    echo "  - Memory allocation patterns"
    echo "  - Scene graph traversal"
    echo "  - Vector operations (SIMD potential)"
    echo ""
    echo "No display required - pure CPU benchmarking!"
else
    echo "Build failed!"
    exit 1
fi