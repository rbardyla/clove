#!/bin/bash

# Build script for Neural Editor System
# Zero dependencies, maximum performance

echo "========================================="
echo "Building Neural Editor System"
echo "========================================="

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-parameter -std=c99"
CFLAGS="$CFLAGS -DHANDMADE_NEURAL=1"

# Debug build option
if [ "$1" = "debug" ]; then
    echo "Building DEBUG version..."
    CFLAGS="-g -O0 -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
    CFLAGS="$CFLAGS -Wall -Wextra -std=c99"
    CFLAGS="$CFLAGS -fsanitize=address -fsanitize=undefined"
fi

# Performance build option
if [ "$1" = "perf" ]; then
    echo "Building PERFORMANCE version..."
    CFLAGS="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
    CFLAGS="$CFLAGS -fprofile-generate -flto"
fi

# Create build directory
mkdir -p ../../build

echo "Compiling neural editor system..."
$CC $CFLAGS -c handmade_editor_neural.c -o ../../build/editor_neural.o

echo "Compiling integration example..."
$CC $CFLAGS neural_editor_integration.c ../../build/editor_neural.o \
    -o ../../build/neural_editor_demo -lm

# Build test suite
echo "Building test suite..."
$CC $CFLAGS test_neural_editor.c ../../build/editor_neural.o \
    -o ../../build/test_neural_editor -lm

echo ""
echo "Build complete!"
echo "Outputs:"
echo "  - ../../build/editor_neural.o (object file)"
echo "  - ../../build/neural_editor_demo (integration demo)"
echo "  - ../../build/test_neural_editor (test suite)"

# Check binary size
echo ""
echo "Binary sizes:"
ls -lh ../../build/editor_neural.o
ls -lh ../../build/neural_editor_demo
ls -lh ../../build/test_neural_editor 2>/dev/null || true

echo ""
echo "========================================="
echo "Performance targets:"
echo "  - Inference: < 0.1ms per prediction"
echo "  - Memory: < 2MB footprint"
echo "  - Cache misses: < 5% in hot paths"
echo "  - 60+ FPS with continuous predictions"
echo "========================================="