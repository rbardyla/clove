#!/bin/bash

# Minimal build script for the foundation
# Compiles only the platform layer + our minimal main.c

echo "Building Handmade Engine Foundation..."

# Compiler flags
CC=gcc
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter"
CFLAGS="$CFLAGS -O2 -march=native"  # Release optimizations
CFLAGS="$CFLAGS -g"                 # Debug info
CFLAGS="$CFLAGS -DGUI_DEMO_STANDALONE"  # Enable GUI standalone mode
LIBS="-lX11 -lGL -lGLX -lm -lpthread -ldl"

# Output
OUTPUT="handmade_foundation"

# Build command - include GUI system, minimal renderer, and asset system
SOURCES="main.c handmade_platform_linux.c minimal_renderer.c simple_gui.c handmade_assets.c"
echo "Compiling: $CC $CFLAGS $SOURCES $LIBS -o $OUTPUT"

$CC $CFLAGS $SOURCES $LIBS -o $OUTPUT

if [ $? -eq 0 ]; then
    echo "Build successful! Run with: ./$OUTPUT"
    echo ""
    echo "Features:"
    echo "  - Complete editor interface with panels"
    echo "  - Asset Browser with filesystem scanning"
    echo "  - Texture loading (BMP format)"
    echo "  - Model loading (OBJ format)"
    echo "  - Sound loading (WAV format)"
    echo "  - Thumbnail generation for assets"
    echo "  - Scene Hierarchy and Property Inspector"
    echo "  - Performance overlay"
    echo ""
    echo "Controls:"
    echo "  ESC   - Quit"  
    echo "  F1-F4 - Toggle editor panels"
    echo "  1-4   - Select tools"
    echo "  Arrow Keys - Navigate asset browser"
    echo "  Enter - Open folder/Load asset"
    echo ""
    echo "You should see:"
    echo "  - Animated background with colorful triangle"
    echo "  - GUI windows with buttons, sliders, and text"
    echo "  - Performance metrics overlay"
else
    echo "Build failed!"
    exit 1
fi