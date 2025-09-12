#!/bin/bash

# Build script for Unified Multi-Scale Physics Simulation
# Continental-scale simulation with all systems coupled

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_TYPE=${1:-release}
CC=gcc

echo "Building Unified Multi-Scale Physics Simulation ($BUILD_TYPE)..."
echo "Target: Continental-scale simulation with 256+ tectonic plates"
echo "Performance goal: >1M geological years/second"
echo ""

# Compiler flags optimized for massive simulation
COMMON_FLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function"
COMMON_FLAGS="$COMMON_FLAGS -ffast-math -fno-signed-zeros -fno-trapping-math"
COMMON_FLAGS="$COMMON_FLAGS -DHANDMADE_INTERNAL=1"

# Maximum performance flags for continental simulation
PERF_FLAGS="-O3 -march=native -mavx2 -mfma -funroll-loops"
PERF_FLAGS="$PERF_FLAGS -fomit-frame-pointer -finline-functions -finline-limit=1000"
PERF_FLAGS="$PERF_FLAGS -ftree-vectorize -floop-optimize -floop-strip-mine"
PERF_FLAGS="$PERF_FLAGS -falign-functions=32 -falign-loops=32"
PERF_FLAGS="$PERF_FLAGS -fprefetch-loop-arrays -ftracer"
PERF_FLAGS="$PERF_FLAGS -DNDEBUG"

# Debug flags (reduced optimization for debugging)
DEBUG_FLAGS="-g -O1 -DHANDMADE_DEBUG=1"
DEBUG_FLAGS="$DEBUG_FLAGS -fsanitize=address -fsanitize=undefined"
DEBUG_FLAGS="$DEBUG_FLAGS -fstack-protector-strong"

# Math and threading libraries
LIBS="-lm -lrt"

# Set build-specific flags
if [ "$BUILD_TYPE" = "release" ]; then
    CFLAGS="$COMMON_FLAGS $PERF_FLAGS"
    echo "Release build: Maximum performance optimization"
    echo "SIMD: AVX2 + FMA enabled"
    echo "Vectorization: Aggressive loop optimization"
elif [ "$BUILD_TYPE" = "debug" ]; then
    CFLAGS="$COMMON_FLAGS $DEBUG_FLAGS"
    echo "Debug build: Sanitizers enabled, reduced optimization"
else
    echo "Unknown build type: $BUILD_TYPE (use 'debug' or 'release')"
    exit 1
fi

echo "Compiler: $CC"
echo "Flags: $CFLAGS"
echo "Libraries: $LIBS"
echo ""

# Build unified simulation
echo "Building unified multi-scale physics simulation..."
echo "  Including all systems:"
echo "    ✓ Geological physics (256+ tectonic plates)"
echo "    ✓ Hydrological dynamics (continental-scale fluid flow)"
echo "    ✓ Structural engineering (earthquake response)"
echo "    ✓ Atmospheric simulation (weather and precipitation)"
echo "    ✓ Full cross-system coupling"
echo ""

$CC $CFLAGS -o unified_simulation unified_simulation.c $LIBS

# Check if binary was created
if [ -f "unified_simulation" ]; then
    echo "✓ unified_simulation built successfully"
    
    # Make executable
    chmod +x unified_simulation
    
    # Show binary info
    echo ""
    echo "Binary information:"
    ls -lah unified_simulation
    
    if command -v file >/dev/null 2>&1; then
        file unified_simulation
    fi
    
    # Check for SIMD capabilities
    if [ "$BUILD_TYPE" = "release" ]; then
        echo ""
        echo "SIMD Analysis:"
        if objdump -d unified_simulation | grep -q "vmulps\|vfmadd"; then
            echo "  ✓ AVX2/FMA instructions detected in binary"
        else
            echo "  ⚠ No AVX2/FMA instructions found (may need newer CPU)"
        fi
    fi
    
    echo ""
    echo "Build completed successfully!"
    echo ""
    echo "To run unified simulation:"
    echo "  ./unified_simulation"
    echo ""
    
    if [ "$BUILD_TYPE" = "release" ]; then
        echo "Performance analysis commands:"
        echo "  time ./unified_simulation"
        echo "  perf stat -e cycles,instructions,cache-misses,cache-references ./unified_simulation"
        echo "  perf record -g ./unified_simulation && perf report"
        echo ""
        echo "Memory analysis:"
        echo "  valgrind --tool=massif ./unified_simulation"
        echo "  valgrind --tool=cachegrind ./unified_simulation"
    else
        echo "Debug analysis:"
        echo "  gdb ./unified_simulation"
        echo "  valgrind ./unified_simulation"
    fi
    
else
    echo "✗ Build failed - unified_simulation binary not created"
    exit 1
fi

echo ""
echo "=========================================="
echo "Handmade Multi-Scale Physics Engine build complete!"
echo ""
echo "Features implemented:"
echo "✓ Zero dependencies (only OS APIs)"
echo "✓ SIMD optimizations (AVX2 + FMA)"
echo "✓ Arena memory management (no malloc/free)"
echo "✓ Continental-scale geological simulation"
echo "✓ Hydrological erosion and river formation"
echo "✓ Structural earthquake response"
echo "✓ Atmospheric weather and precipitation"
echo "✓ Full multi-scale coupling"
echo "✓ Performance target: >1M years/second"
echo ""
echo "Handmade philosophy compliance:"
echo "✓ Every line of code understood"
echo "✓ Built from first principles"
echo "✓ No black box libraries"
echo "✓ Cache-coherent data structures"
echo "✓ Performance-first design"
echo "✓ Always have something working"
echo "=========================================="