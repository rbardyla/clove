#!/bin/bash

# Build script for Multi-Level Physics-Driven Detail system
# Following handmade philosophy: zero dependencies, arena allocation, SIMD optimization

echo "=================================="
echo "Building Multi-Level Physics System"
echo "=================================="

# Compiler settings
CC=gcc
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter"
OPTFLAGS="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
DEBUGFLAGS="-g -DHANDMADE_DEBUG=1"
LIBS="-lm"

# Determine build mode
BUILD_MODE=${1:-release}

if [ "$BUILD_MODE" = "debug" ]; then
    echo "Building in DEBUG mode..."
    FLAGS="$CFLAGS $DEBUGFLAGS"
else
    echo "Building in RELEASE mode..."
    FLAGS="$CFLAGS $OPTFLAGS"
fi

# Build geological simulation test
echo ""
echo "Building geological simulation test..."
$CC $FLAGS -o test_geological test_geological.c $LIBS

if [ $? -eq 0 ]; then
    echo "✓ test_geological built successfully"
else
    echo "✗ test_geological build failed"
    exit 1
fi

# Build any additional tests here
# echo "Building hydrological simulation test..."
# $CC $FLAGS -o test_hydro test_hydro.c $LIBS

echo ""
echo "=================================="
echo "Build Complete!"
echo "=================================="
echo ""
echo "Run tests with:"
echo "  ./test_geological"
echo ""
echo "Performance notes:"
echo "  - Geological timescale: 1M years/sec"
echo "  - Tectonic plate simulation with 3 plates"
echo "  - Mountain formation through collision"
echo "  - Zero heap allocations (Grade A compliant)"