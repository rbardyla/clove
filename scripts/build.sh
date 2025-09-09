#!/bin/bash

# Handmade Neural Engine Build Script
# Compile with full optimization and debug symbols for profiling

set -e  # Exit on any error

# Build flags for performance and debugging
CFLAGS="-O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-braces"
CFLAGS="$CFLAGS -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
CFLAGS="$CFLAGS -march=native -mfpmath=sse -mavx2"  # Enable SIMD
CFLAGS="$CFLAGS -std=c99"

# Link flags
LDFLAGS="-lX11 -lm -lpthread"

# Source files
SOURCES="main.c memory.c platform_linux.c"

# Output binary
OUTPUT="handmade_engine"

echo "Building Handmade Neural Engine..."
echo "Source files: $SOURCES"
echo "Output: $OUTPUT"
echo ""

# Compile
gcc $CFLAGS $SOURCES -o $OUTPUT $LDFLAGS

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Run with: ./$OUTPUT"
    echo ""
    echo "Controls:"
    echo "  Arrow Up: Run neural inference"
    echo "  WASD: Movement input"
    echo "  ESC: Exit"
else
    echo "Build failed!"
    exit 1
fi