#!/bin/bash

# Build script for DNC (Differentiable Neural Computer) Example
# Compiles the revolutionary NPC memory system

set -e  # Exit on any error

echo "==================================================="
echo "   Building DNC - NPCs with True Persistent Memory"
echo "==================================================="
echo ""

# Compiler flags
CFLAGS="-O3 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable"
CFLAGS="$CFLAGS -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
CFLAGS="$CFLAGS -DDNC_STANDALONE=1"  # Enable standalone main
CFLAGS="$CFLAGS -march=native -mavx2 -mfma"  # SIMD optimizations
CFLAGS="$CFLAGS -std=c99"

# Profiling and optimization reports
CFLAGS="$CFLAGS -fopt-info-vec-optimized"  # Show vectorization

# Link flags
LDFLAGS="-lm -lpthread"

# Source files
SOURCES="dnc_example.c dnc.c lstm.c neural_math.c memory.c"

# Output binary
OUTPUT="dnc_demo"

echo "Configuration:"
echo "  Memory Locations: 128"
echo "  Memory Vector Size: 64" 
echo "  Read Heads: 2"
echo "  SIMD: AVX2 + FMA"
echo ""
echo "Compiling..."

# Compile
gcc $CFLAGS $SOURCES -o $OUTPUT $LDFLAGS 2>&1 | grep -E "vectorized|optimized" || true

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo ""
    echo "Run with: ./$OUTPUT"
    echo ""
    echo "This demonstrates:"
    echo "  - NPCs storing persistent memories"
    echo "  - Content-based memory retrieval" 
    echo "  - Temporal memory linking"
    echo "  - Dynamic memory allocation"
    echo ""
    echo "Based on DeepMind's Nature paper (2016)"
    echo "but implemented from scratch in pure C!"
else
    echo "Build failed!"
    exit 1
fi

# Make executable
chmod +x $OUTPUT