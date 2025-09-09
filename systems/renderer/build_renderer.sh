#!/bin/bash

echo "Building Handmade 3D Renderer..."

# Compiler flags
CC=gcc
CFLAGS="-O2 -Wall -Wextra -march=native -ffast-math"
INCLUDES="-I. -I../../src"
LIBS="-lX11 -lXext -lGL -lm -lpthread"

# Build math library
echo "Compiling math library..."
$CC $CFLAGS $INCLUDES -c handmade_math.c -o handmade_math.o

# Build OpenGL loader
echo "Compiling OpenGL loader..."
$CC $CFLAGS $INCLUDES -c handmade_opengl.c -o handmade_opengl.o

# Build platform layer
echo "Compiling platform layer..."
$CC $CFLAGS $INCLUDES -c handmade_platform_linux.c -o handmade_platform_linux.o

# Build renderer
echo "Compiling renderer..."
$CC $CFLAGS $INCLUDES -c handmade_renderer.c -o handmade_renderer.o

# Build test program
echo "Compiling test program..."
$CC $CFLAGS $INCLUDES -c renderer_test.c -o renderer_test.o

# Link
echo "Linking..."
$CC renderer_test.o handmade_renderer.o handmade_platform_linux.o handmade_opengl.o handmade_math.o \
    $LIBS -o renderer_test

# Create library archive for integration
echo "Creating renderer library..."
ar rcs libhandmade_renderer.a handmade_renderer.o handmade_opengl.o

# Keep object files for debugging
# rm -f *.o

echo "✓ Build complete!"
echo "Run ./renderer_test to see the 3D renderer in action"