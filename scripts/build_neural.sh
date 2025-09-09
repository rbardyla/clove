#!/bin/bash

# Build script for neural math library test
# Zero dependencies except standard C library for math functions

echo "========================================="
echo "Building Handmade Neural Math Library"
echo "========================================="

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -Wall -Wextra"
CFLAGS="$CFLAGS -mavx2 -mfma"  # Enable AVX2 and FMA instructions
CFLAGS="$CFLAGS -DHANDMADE_DEBUG=1"  # Enable debug features
CFLAGS="$CFLAGS -ffast-math"  # Fast math optimizations
CFLAGS="$CFLAGS -funroll-loops"  # Loop unrolling
CFLAGS="$CFLAGS -ftree-vectorize"  # Auto-vectorization

# Source files
SOURCES="test_neural.c neural_math.c"

# Output binary
OUTPUT="test_neural"

echo ""
echo "Compiler: $CC"
echo "Flags: $CFLAGS"
echo ""

# Clean old binary
if [ -f "$OUTPUT" ]; then
    echo "Removing old binary..."
    rm "$OUTPUT"
fi

# Compile
echo "Compiling..."
$CC $CFLAGS $SOURCES -o $OUTPUT -lm

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Binary: ./$OUTPUT"
    echo ""
    echo "Run './$OUTPUT' for tests"
    echo "Run './$OUTPUT b' for full benchmarks"
    echo ""
    
    # Check CPU features
    echo "Checking CPU features..."
    lscpu | grep -E "avx2|avx512|fma" || echo "  (AVX2/AVX512/FMA detection requires lscpu)"
    
else
    echo ""
    echo "Build failed!"
    exit 1
fi