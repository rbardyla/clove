#!/bin/bash

# Build script for LSTM neural network module
# Zero dependencies, maximum performance

echo "Building LSTM Neural Network Module..."
echo "======================================"

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -mtune=native"
CFLAGS="$CFLAGS -mavx2 -mfma"                    # Enable AVX2 and FMA instructions
CFLAGS="$CFLAGS -ffast-math"                     # Fast math optimizations
CFLAGS="$CFLAGS -funroll-loops"                  # Loop unrolling
CFLAGS="$CFLAGS -finline-functions"              # Inline functions
CFLAGS="$CFLAGS -fomit-frame-pointer"            # Optimize stack frames
CFLAGS="$CFLAGS -Wall -Wextra"                   # Warnings

# Debug build flags (uncomment for debug)
# CFLAGS="-g -O0 -DHANDMADE_DEBUG=1 -Wall -Wextra -fsanitize=address"

# Include paths
INCLUDES="-I../src"

# Source files for LSTM
LSTM_SOURCES="../src/lstm.c ../src/neural_math.c ../src/memory.c"

# Build LSTM library object files
echo "Compiling LSTM module..."
$CC $CFLAGS $INCLUDES -c ../src/lstm.c -o lstm.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile lstm.c"
    exit 1
fi

# Build LSTM example
echo "Building LSTM example..."
$CC $CFLAGS $INCLUDES \
    ../src/lstm_example.c \
    lstm.o \
    ../src/neural_math.c \
    ../src/memory.c \
    -o ../lstm_example \
    -lm \
    -pthread

if [ $? -ne 0 ]; then
    echo "Error: Failed to build LSTM example"
    exit 1
fi

echo "Build successful!"
echo ""
echo "Executables created:"
echo "  ../lstm_example       - NPC memory demonstration"
echo ""
echo "Usage:"
echo "  ../lstm_example       - Run NPC interaction demo (default)"
echo "  ../lstm_example b     - Run benchmarks"
echo "  ../lstm_example m     - Test memory persistence"
echo ""
echo "Performance notes:"
echo "  - AVX2 SIMD optimizations enabled"
echo "  - Cache-aligned memory layout"
echo "  - Zero heap allocations in hot path"
echo "  - Optimized for modern x86-64 CPUs"

# Make executable
chmod +x ../lstm_example

# Show binary info
echo ""
echo "Binary information:"
ls -lh ../lstm_example
file ../lstm_example

# Check for AVX2 support
echo ""
echo "CPU capabilities:"
grep -o 'avx2\|fma' /proc/cpuinfo | head -1 | xargs echo "  Detected:"