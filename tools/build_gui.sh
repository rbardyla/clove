#!/bin/bash

# build_gui.sh - Build handmade GUI framework with zero dependencies

set -e  # Exit on error

echo "Building Handmade GUI Framework..."

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -Wall -Wextra -Wno-unused-parameter"
CFLAGS="$CFLAGS -ffast-math -funroll-loops"

# Enable SIMD optimizations
CFLAGS="$CFLAGS -msse2 -msse3 -mssse3 -msse4.1 -msse4.2"

# Check for AVX2 support
if grep -q avx2 /proc/cpuinfo 2>/dev/null; then
    echo "AVX2 support detected, enabling AVX2 optimizations"
    CFLAGS="$CFLAGS -mavx -mavx2"
fi

# X11 flags
X11_LIBS="-lX11"

# Debug build option
if [ "$1" = "debug" ]; then
    echo "Building debug version..."
    CFLAGS="-g -O0 -DDEBUG -Wall -Wextra"
fi

# Create build directory
mkdir -p build

# Build the GUI demo
echo "Compiling GUI demo..."
$CC $CFLAGS \
    gui_demo.c \
    handmade_gui.c \
    handmade_renderer.c \
    handmade_platform_linux.c \
    $X11_LIBS \
    -lm \
    -o handmade_gui_demo

echo "Build complete!"
echo ""
echo "Run with: ./handmade_gui_demo"
echo ""
echo "Features:"
echo "  - Pure software rendering (no OpenGL/Vulkan)"
echo "  - Direct X11 integration (no SDL/GLFW)"
echo "  - SIMD optimized rendering (SSE2/AVX2)"
echo "  - Immediate mode GUI"
echo "  - Zero heap allocations per frame"
echo "  - Neural network visualization"
echo "  - NPC simulation with emotions"
echo "  - Real-time performance graphs"
echo ""

# Check binary size
SIZE=$(stat -c%s handmade_gui_demo)
echo "Binary size: $(($SIZE / 1024)) KB"

# Show detected CPU features
echo ""
echo "CPU Features:"
if grep -q sse2 /proc/cpuinfo 2>/dev/null; then echo "  ✓ SSE2"; fi
if grep -q sse3 /proc/cpuinfo 2>/dev/null; then echo "  ✓ SSE3"; fi
if grep -q ssse3 /proc/cpuinfo 2>/dev/null; then echo "  ✓ SSSE3"; fi
if grep -q sse4_1 /proc/cpuinfo 2>/dev/null; then echo "  ✓ SSE4.1"; fi
if grep -q sse4_2 /proc/cpuinfo 2>/dev/null; then echo "  ✓ SSE4.2"; fi
if grep -q avx /proc/cpuinfo 2>/dev/null; then echo "  ✓ AVX"; fi
if grep -q avx2 /proc/cpuinfo 2>/dev/null; then echo "  ✓ AVX2"; fi