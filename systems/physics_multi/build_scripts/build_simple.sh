#!/bin/bash

#
# Continental Architect - Simple Build Script
# Builds the standalone simplified demo
#

set -e

echo "Building Continental Architect - Simplified Demo"
echo "==============================================="

# Build configuration
BUILD_MODE=${1:-release}
OUTPUT_DIR="../../binaries"
BINARY_NAME="continental_architect_simple"

# Compiler and flags
CC="gcc"
COMMON_FLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter"
LINK_FLAGS="-lX11 -lGL -lGLX -lm"

# Mode-specific flags
if [ "$BUILD_MODE" = "debug" ]; then
    echo "Building in DEBUG mode"
    BUILD_FLAGS="-g -O0"
elif [ "$BUILD_MODE" = "release" ]; then
    echo "Building in RELEASE mode"
    BUILD_FLAGS="-O3 -march=native -ffast-math -DNDEBUG"
else
    echo "Unknown build mode: $BUILD_MODE"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "Compiler: $CC"
echo "Flags: $COMMON_FLAGS $BUILD_FLAGS"
echo "Output: $OUTPUT_DIR/$BINARY_NAME"
echo ""

# Compile the game
echo "Compiling simplified Continental Architect demo..."

$CC $COMMON_FLAGS $BUILD_FLAGS \
    continental_architect_simple.c \
    $LINK_FLAGS \
    -o "$OUTPUT_DIR/$BINARY_NAME"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo "Executable: $OUTPUT_DIR/$BINARY_NAME"
    
    ls -lh "$OUTPUT_DIR/$BINARY_NAME"
    
    echo ""
    echo "To run: $OUTPUT_DIR/$BINARY_NAME"
    echo ""
    echo "Controls:"
    echo "  1-4: Select tools (Push, Pull, Water, Civilization)"  
    echo "  Mouse: Click and drag to apply tools"
    echo "  Space: Toggle time acceleration"
    echo "  +/-: Zoom in/out"
    echo "  ESC: Exit"
    
else
    echo "✗ Build failed!"
    exit 1
fi

echo ""
echo "Ready to simulate continental evolution and civilization management!"