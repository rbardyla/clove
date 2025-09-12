#!/bin/bash

# Build script for Handmade World Generation System
# Zero dependencies, SIMD optimized

echo "=== Building Handmade World Generation System ==="

# Compiler flags
CC="gcc"
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter"
OPTIMIZE="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
DEBUG="-g -DHANDMADE_DEBUG=1"
LIBS="-lm"

# Default to release build
BUILD_TYPE=${1:-release}

if [ "$BUILD_TYPE" = "debug" ]; then
    echo "Building DEBUG version..."
    FLAGS="$CFLAGS $DEBUG -mavx2 -mfma"
elif [ "$BUILD_TYPE" = "release" ]; then
    echo "Building RELEASE version..."
    FLAGS="$CFLAGS $OPTIMIZE"
else
    echo "Unknown build type: $BUILD_TYPE"
    echo "Usage: $0 [debug|release]"
    exit 1
fi

# Create output directory
mkdir -p ../../binaries

# Build test programs
echo "Compiling noise test..."
$CC $FLAGS -o ../../binaries/test_noise test_noise.c $LIBS

echo "Compiling simple noise test..."
$CC $FLAGS -o ../../binaries/simple_noise_test simple_noise_test.c $LIBS

echo "Compiling terrain test..."
$CC $FLAGS -o ../../binaries/test_terrain test_terrain.c $LIBS

echo "Compiling collision test..."
$CC $FLAGS -o ../../binaries/test_collision test_collision.c $LIBS

echo "Compiling validation suite..."
$CC $FLAGS -o ../../binaries/validate_world_gen validate_world_gen.c $LIBS

# Build as static library for integration
echo "Building static library..."
$CC $FLAGS -c handmade_noise.c -o handmade_noise.o
$CC $FLAGS -c handmade_terrain.c -o handmade_terrain.o
$CC $FLAGS -c handmade_terrain_collision.c -o handmade_terrain_collision.o

ar rcs ../../binaries/libhandmade_worldgen.a handmade_noise.o handmade_terrain.o handmade_terrain_collision.o

# Clean up object files
rm -f *.o

echo ""
echo "=== Build Complete ==="
echo "Binaries in: binaries/"
echo "Library: binaries/libhandmade_worldgen.a"
echo ""
echo "Run validation: ./binaries/validate_world_gen"
echo "Run tests: ./binaries/test_terrain"