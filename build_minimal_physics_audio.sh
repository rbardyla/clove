#!/bin/bash

# Build script for Minimal Engine with 2D Physics and Audio
# Creates a single integrated binary with renderer, GUI, physics, and audio

echo "Building Minimal Engine with Physics and Audio..."

# Compiler settings
CC=gcc
CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=c99"
CFLAGS="$CFLAGS -I. -Isystems/audio"  # Include audio headers
LIBS="-lX11 -lGL -lm -lpthread -lasound"  # Added ALSA for audio

# Build mode (debug or release)
BUILD_MODE=${1:-release}

if [ "$BUILD_MODE" = "debug" ]; then
    echo "Building in DEBUG mode"
    CFLAGS="$CFLAGS -g -O0 -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
    OUTPUT="minimal_engine_physics_audio_debug"
else
    echo "Building in RELEASE mode" 
    CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
    OUTPUT="minimal_engine_physics_audio"
fi

# First, build the audio library if needed
echo "Building audio library..."
cd systems/audio
if [ ! -f build/libhandmade_audio.a ] || [ handmade_audio.c -nt build/libhandmade_audio.a ]; then
    ./build_audio.sh $BUILD_MODE
    if [ $? -ne 0 ]; then
        echo "Failed to build audio library!"
        exit 1
    fi
fi
cd ../..

# Source files
SOURCES="main_minimal_physics_audio.c"
SOURCES="$SOURCES handmade_platform_linux.c"
SOURCES="$SOURCES handmade_renderer.c"
SOURCES="$SOURCES handmade_gui.c"
SOURCES="$SOURCES handmade_physics_2d.c"

# Link with audio library
AUDIO_LIB="systems/audio/build/libhandmade_audio.a"

# Clean old build
rm -f $OUTPUT

# Build
echo "Compiling integrated engine..."
$CC $CFLAGS $SOURCES $AUDIO_LIB -o $OUTPUT $LIBS

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
    echo "  - Audio system with collision sounds"
    echo "  - Interactive physics demo with sound effects"
    echo ""
    echo "Audio Features:"
    echo "  - Procedural collision sounds"
    echo "  - Impact strength detection"
    echo "  - Volume controls"
    echo "  - ALSA backend for low-latency audio"
else
    echo "Build failed!"
    exit 1
fi