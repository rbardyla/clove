#!/bin/bash

# Build the neural debug system
# Simple build script that works with the existing main.c structure

echo "Building Neural Debug System..."

# Clean previous build
rm -f neural_debug_test

# Define compiler flags
COMMON_FLAGS="-std=c99 -g -O2 -march=native"
SIMD_FLAGS="-mavx2 -mfma" 
DEFINES="-DHANDMADE_DEBUG=1 -DNEURAL_USE_AVX2=1"
WARNING_FLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable"
INCLUDE_FLAGS="-I."
LIBRARY_FLAGS="-lm -lX11"

# Compile with existing main.c (which has its own neural_network structure)
echo "Compiling debug system..."
gcc $COMMON_FLAGS $SIMD_FLAGS $DEFINES $WARNING_FLAGS $INCLUDE_FLAGS \
    -o neural_debug_test \
    main.c \
    platform_linux.c \
    memory.c \
    neural_debug.c \
    $LIBRARY_FLAGS

if [ $? -eq 0 ]; then
    echo "Build successful! Neural debug system compiled."
    echo ""
    echo "Usage:"
    echo "  ./neural_debug_test"
    echo ""
    echo "Controls:"
    echo "  Keys 1-6: Switch visualization modes"
    echo "  Mouse: Hover for detailed inspection"
    echo "  Mouse wheel: Zoom in/out"
    echo "  Right drag: Pan view"
    echo "  P: Pause/resume"
    echo "  H: Toggle help"
    echo "  R: Reset debug state"
    echo ""
else
    echo "Build failed!"
    exit 1
fi