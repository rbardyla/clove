#!/bin/bash

# Handmade Save System - Build Script
# Builds the save system and demo with optimizations

echo "=== Building Handmade Save System ==="

# Compiler flags
CC="gcc"
CFLAGS="-Wall -Wextra -O3 -march=native -ffast-math"
CFLAGS_DEBUG="-Wall -Wextra -g -O0 -DDEBUG"
INCLUDES="-I../../src -I../neural -I../physics -I../audio -I../script -I../nodes"
LIBS="-lm -lpthread"

# Platform-specific flags
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM_FLAGS="-D_GNU_SOURCE"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM_FLAGS="-framework Foundation"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    PLATFORM_FLAGS="-D_WIN32"
    LIBS="$LIBS -lws2_32"
fi

# Build mode (release by default)
BUILD_MODE="release"
if [ "$1" == "debug" ]; then
    BUILD_MODE="debug"
    CFLAGS="$CFLAGS_DEBUG"
fi

echo "Build mode: $BUILD_MODE"
echo "Compiler flags: $CFLAGS"

# Create build directory
mkdir -p build

# Build save system library
echo ""
echo "Building save system library..."

# Compile core serialization
$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES -c handmade_save.c -o build/handmade_save.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile handmade_save.c"
    exit 1
fi

# Compile compression
$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES -c save_compression.c -o build/save_compression.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile save_compression.c"
    exit 1
fi

# Compile game state serialization
$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES -c save_gamestate.c -o build/save_gamestate.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile save_gamestate.c"
    exit 1
fi

# Compile migration
$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES -c save_migration.c -o build/save_migration.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile save_migration.c"
    exit 1
fi

# Compile slot management
$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES -c save_slots.c -o build/save_slots.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile save_slots.c"
    exit 1
fi

# Compile integration
$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES -c save_integration.c -o build/save_integration.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile save_integration.c"
    exit 1
fi

# Create static library
echo ""
echo "Creating static library..."
ar rcs build/libhandmade_save.a \
    build/handmade_save.o \
    build/save_compression.o \
    build/save_gamestate.o \
    build/save_migration.o \
    build/save_slots.o \
    build/save_integration.o

if [ $? -ne 0 ]; then
    echo "Error: Failed to create static library"
    exit 1
fi

# Build demo executable
echo ""
echo "Building demo executable..."

$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES \
    save_demo.c platform_save_stub.c \
    build/libhandmade_save.a \
    $LIBS \
    -o build/save_demo

if [ $? -ne 0 ]; then
    echo "Error: Failed to build demo"
    exit 1
fi

# Build performance test
echo ""
echo "Building performance test..."

cat > build/perf_test.c << 'EOF'
#include "../handmade_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Save System Performance Test ===\n\n");
    
    // Allocate 8MB for save system
    void *memory = malloc(8 * 1024 * 1024);
    save_system *system = save_system_init(memory, 8 * 1024 * 1024);
    
    // Test compression speed
    u8 *test_data = malloc(1024 * 1024);
    u8 *compressed = malloc(2 * 1024 * 1024);
    
    // Fill with pattern
    for (int i = 0; i < 1024 * 1024; i++) {
        test_data[i] = (i >> 8) & 0xFF;
    }
    
    // Benchmark compression
    clock_t start = clock();
    u32 size = save_compress_lz4(test_data, 1024 * 1024, compressed, 2 * 1024 * 1024);
    clock_t end = clock();
    
    double time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    double throughput = (1.0 / (time_ms / 1000.0));
    
    printf("LZ4 Compression:\n");
    printf("  Input: 1MB\n");
    printf("  Output: %u bytes (%.1f%% ratio)\n", size, (100.0 * size) / (1024 * 1024));
    printf("  Time: %.2fms\n", time_ms);
    printf("  Throughput: %.1f MB/s\n\n", throughput);
    
    // Test CRC32 speed
    start = clock();
    for (int i = 0; i < 100; i++) {
        save_crc32(test_data, 1024 * 1024);
    }
    end = clock();
    
    time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0 / 100.0;
    throughput = (1.0 / (time_ms / 1000.0));
    
    printf("CRC32 Checksum:\n");
    printf("  Data size: 1MB\n");
    printf("  Time: %.2fms\n", time_ms);
    printf("  Throughput: %.1f MB/s\n", throughput);
    
    free(test_data);
    free(compressed);
    free(memory);
    
    return 0;
}
EOF

$CC $CFLAGS $PLATFORM_FLAGS $INCLUDES \
    build/perf_test.c \
    build/libhandmade_save.a \
    $LIBS \
    -o build/perf_test

if [ $? -ne 0 ]; then
    echo "Warning: Failed to build performance test"
fi

# Generate size report
echo ""
echo "=== Build Complete ==="
echo ""
echo "Size report:"
ls -lh build/libhandmade_save.a
ls -lh build/save_demo
if [ -f build/perf_test ]; then
    ls -lh build/perf_test
fi

echo ""
echo "Object file sizes:"
size build/*.o | tail -n +2 | sort -k1 -n -r

# Run demo if requested
if [ "$2" == "run" ]; then
    echo ""
    echo "=== Running Demo ==="
    cd build
    ./save_demo
    cd ..
fi

# Run performance test if requested
if [ "$2" == "perf" ]; then
    echo ""
    echo "=== Running Performance Test ==="
    cd build
    ./perf_test
    cd ..
fi

echo ""
echo "Build successful!"
echo ""
echo "To run the demo: ./build/save_demo"
echo "To run performance test: ./build/perf_test"
echo "To include in your project: link with build/libhandmade_save.a"