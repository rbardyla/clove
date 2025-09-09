#!/bin/bash

echo "================================"
echo "Building Professional Game Editor"
echo "================================"

# Create build directory
mkdir -p build

# Common flags
COMMON_FLAGS="-Wall -Wextra -Wno-unused-parameter -std=c99"
DEBUG_FLAGS="-g -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
RELEASE_FLAGS="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
LINK_FLAGS="-lGL -lX11 -lXi -lm -lpthread"

# Choose build type
BUILD_TYPE="${1:-debug}"
if [ "$BUILD_TYPE" = "release" ]; then
    FLAGS="$COMMON_FLAGS $RELEASE_FLAGS"
    echo "Building RELEASE version..."
else
    FLAGS="$COMMON_FLAGS $DEBUG_FLAGS"
    echo "Building DEBUG version..."
fi

# Build the professional editor
echo "Compiling professional editor..."
gcc $FLAGS \
    -I. \
    -Isystems/renderer \
    -Isystems/gui \
    -Isystems/physics \
    -Isystems/ai \
    -Isystems/editor \
    professional_editor_demo.c \
    systems/editor/handmade_main_editor.c \
    systems/renderer/handmade_renderer.c \
    systems/gui/handmade_gui.c \
    systems/physics/handmade_physics.c \
    src/platform_linux.c \
    $LINK_FLAGS \
    -o build/professional_editor

if [ $? -eq 0 ]; then
    echo "================================"
    echo "Build successful!"
    echo "Run with: ./build/professional_editor"
    echo "================================"
else
    echo "================================"
    echo "Build failed!"
    echo "================================"
    exit 1
fi

# Make executable
chmod +x build/professional_editor