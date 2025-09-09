#!/bin/bash

# Handmade Steam Integration - Build Script
# Builds the Steam integration system and demo

echo "=== Building Handmade Steam Integration ==="

# Compiler settings
CC="gcc"
CFLAGS="-Wall -Wextra -O3 -march=native -ffast-math"
CFLAGS_DEBUG="-Wall -Wextra -g -O0 -DDEBUG"
INCLUDES="-I../../src -I../achievements -I../settings"
LIBS="-lm"

# Build mode (release by default)
BUILD_MODE="release"
if [ "$1" == "debug" ]; then
    BUILD_MODE="debug"
    FLAGS="$CFLAGS_DEBUG"
else
    FLAGS="$CFLAGS"
fi

echo "Build mode: $BUILD_MODE"
echo "Compiler flags: $FLAGS"

# Create build directory
mkdir -p build

echo ""
echo "Building Steam integration library..."

# Build required dependencies first (achievements system)
echo "Building achievements dependency..."
cd ../achievements
if [ ! -f build/libhandmade_achievements.a ]; then
    chmod +x build_achievements.sh
    ./build_achievements.sh
    if [ $? -ne 0 ]; then
        echo "Error: Failed to build achievements dependency"
        exit 1
    fi
fi
cd ../steam

# Build settings dependency
echo "Building settings dependency..."  
cd ../settings
if [ ! -f build/libhandmade_settings.a ]; then
    chmod +x build_settings.sh
    ./build_settings.sh
    if [ $? -ne 0 ]; then
        echo "Error: Failed to build settings dependency"
        exit 1
    fi
fi
cd ../steam

# Compile core Steam system
$CC $FLAGS $INCLUDES -c handmade_steam.c -o build/handmade_steam.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile handmade_steam.c"
    exit 1
fi

# Compile Steam integration helpers
$CC $FLAGS $INCLUDES -c steam_integration.c -o build/steam_integration.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile steam_integration.c"
    exit 1
fi

echo ""
echo "Creating static library..."

# Create static library
ar rcs build/libhandmade_steam.a \
    build/handmade_steam.o \
    build/steam_integration.o

if [ $? -ne 0 ]; then
    echo "Error: Failed to create static library"
    exit 1
fi

echo ""
echo "Building demo executable..."

# Build demo executable with all dependencies
$CC $FLAGS $INCLUDES \
    steam_demo.c \
    build/libhandmade_steam.a \
    ../achievements/build/libhandmade_achievements.a \
    ../settings/build/libhandmade_settings.a \
    $LIBS \
    -o build/steam_demo

if [ $? -ne 0 ]; then
    echo "Error: Failed to build demo"
    exit 1
fi

echo ""
echo "Building performance test..."

# Build performance test
cat > build/perf_test.c << 'EOF'
#include "../handmade_steam.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Steam Integration Performance Test ===\\n\\n");
    
    void *memory = malloc(1024 * 1024); // 1MB
    steam_system *system = steam_init(memory, 1024 * 1024, 480);
    
    if (!system) {
        printf("Failed to initialize Steam system\\n");
        return 1;
    }
    
    printf("Steam System Initialization:\\n");
    printf("  Initialized: %s\\n", system->initialized ? "Yes" : "No");
    printf("  App ID: %u\\n", system->app_id);
    printf("  Memory allocated: 1024 KB\\n");
    
    clock_t start = clock();
    
    // Test achievement operations
    start = clock();
    for (int i = 0; i < 50000; i++) {
        char ach_name[64];
        snprintf(ach_name, 63, "test_achievement_%d", i % 1000);
        steam_unlock_achievement(system, ach_name);
    }
    clock_t end = clock();
    
    double ach_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nAchievement Operations (50,000 ops):\\n");
    printf("  Time: %.2f ms\\n", ach_time);
    printf("  Rate: %.1f ops/ms\\n", 50000.0 / ach_time);
    printf("  Per operation: %.3f μs\\n", (ach_time * 1000.0) / 50000.0);
    
    // Test stat updates
    start = clock();
    for (int i = 0; i < 100000; i++) {
        steam_set_stat_int(system, "test_int_stat", i);
        steam_set_stat_float(system, "test_float_stat", (float)i * 0.1f);
    }
    end = clock();
    
    double stat_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nStatistic Updates (200,000 ops):\\n");
    printf("  Time: %.2f ms\\n", stat_time);
    printf("  Rate: %.1f ops/ms\\n", 200000.0 / stat_time);
    printf("  Per update: %.3f μs\\n", (stat_time * 1000.0) / 200000.0);
    
    // Test cloud file operations
    start = clock();
    char test_data[1024];
    memset(test_data, 0, sizeof(test_data));
    for (int i = 0; i < 1000; i++) {
        char filename[64];
        snprintf(filename, 63, "test_file_%d.dat", i);
        steam_cloud_write_file(system, filename, test_data, sizeof(test_data));
    }
    end = clock();
    
    double cloud_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nCloud File Operations (1,000 ops):\\n");
    printf("  Time: %.2f ms\\n", cloud_time);
    printf("  Rate: %.1f ops/ms\\n", 1000.0 / cloud_time);
    printf("  Per operation: %.3f ms\\n", cloud_time / 1000.0);
    
    // Test update loop performance
    start = clock();
    for (int i = 0; i < 10000; i++) {
        steam_update(system, 0.016f); // 60 FPS
    }
    end = clock();
    
    double update_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nUpdate Loop (10,000 updates @ 60fps):\\n");
    printf("  Time: %.2f ms\\n", update_time);
    printf("  Per update: %.3f μs\\n", (update_time * 1000.0) / 10000.0);
    printf("  Can sustain: %.1f FPS\\n", 1000.0 / (update_time / 10000.0));
    
    // Memory usage analysis  
    printf("\\nMemory Usage:\\n");
    printf("  System size: %lu bytes\\n", sizeof(steam_system));
    printf("  Achievement tracking: %u items\\n", system->achievement_count);
    printf("  Statistics tracking: %u items\\n", system->stat_count);
    printf("  Cloud files: %u items\\n", system->cloud_file_count);
    printf("  Memory efficiency: %.1f items/KB\\n",
           (system->achievement_count + system->stat_count) / 1024.0f);
    
    printf("\\nAll performance tests completed successfully!\\n");
    
    steam_shutdown(system);
    free(memory);
    return 0;
}
EOF

$CC $FLAGS $INCLUDES \
    build/perf_test.c \
    build/libhandmade_steam.a \
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
ls -lh build/libhandmade_steam.a 2>/dev/null
ls -lh build/steam_demo 2>/dev/null
if [ -f build/perf_test ]; then
    ls -lh build/perf_test
fi

echo ""
echo "Object file sizes:"
size build/*.o 2>/dev/null | tail -n +2 | sort -k1 -n -r

echo ""
echo "Dependencies:"
echo "  libhandmade_achievements.a: $(ls -lh ../achievements/build/libhandmade_achievements.a 2>/dev/null | awk '{print $5}')"
echo "  libhandmade_settings.a: $(ls -lh ../settings/build/libhandmade_settings.a 2>/dev/null | awk '{print $5}')"

# Run demo if requested
if [ "$2" == "run" ]; then
    echo ""
    echo "=== Running Steam Integration Demo ==="
    cd build
    ./steam_demo
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
echo "To run the demo: ./build/steam_demo"
echo "To run performance test: ./build/perf_test"  
echo "To include in your project: link with build/libhandmade_steam.a"
echo ""
echo "Note: This demo uses Steam API stubs for testing."
echo "For real Steam integration, link with steam_api.dll/libsteam_api.so"