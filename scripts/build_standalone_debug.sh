#!/bin/bash

# Build the standalone neural debug system
# This version works without the neural_math.h conflicts

echo "Building Standalone Neural Debug System..."

# Clean previous build
rm -f standalone_neural_debug

# Define compiler flags
COMMON_FLAGS="-std=c99 -g -O2 -march=native"
SIMD_FLAGS="-mavx2 -mfma" 
DEFINES="-DHANDMADE_DEBUG=1"
WARNING_FLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable"
INCLUDE_FLAGS="-I."
LIBRARY_FLAGS="-lm -lX11"

# Compile standalone version
echo "Compiling standalone debug system..."
gcc $COMMON_FLAGS $SIMD_FLAGS $DEFINES $WARNING_FLAGS $INCLUDE_FLAGS \
    -o standalone_neural_debug \
    standalone_neural_debug.c \
    platform_linux.c \
    memory.c \
    $LIBRARY_FLAGS

if [ $? -eq 0 ]; then
    echo "Build successful! Standalone neural debug system compiled."
    echo ""
    echo "Usage:"
    echo "  ./standalone_neural_debug"
    echo ""
    echo "Controls:"
    echo "  1: Neural activation visualization (hot/cold)"
    echo "  2: Weight matrix heatmap"
    echo "  3: NPC brain activity (emotional radar)"
    echo "  H: Toggle help overlay"
    echo "  Mouse: Hover for value inspection"
    echo ""
    echo "Features demonstrated:"
    echo "  - Real-time neural activation hot/cold mapping"
    echo "  - Interactive weight matrix heatmaps"
    echo "  - NPC emotional state radar charts"
    echo "  - Mouse hover value inspection"
    echo "  - Casey Muratori-style immediate debug UI"
    echo "  - Performance-friendly < 1ms visualization"
    echo ""
else
    echo "Build failed!"
    exit 1
fi