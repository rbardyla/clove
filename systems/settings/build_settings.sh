#!/bin/bash

# Handmade Settings System - Build Script
# Builds the settings system and demo

echo "=== Building Handmade Settings System ==="

# Compiler settings
CC="gcc"
CFLAGS="-Wall -Wextra -O3 -march=native -ffast-math"
CFLAGS_DEBUG="-Wall -Wextra -g -O0 -DDEBUG"
INCLUDES="-I../../src"
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
echo "Building settings system library..."

# Compile core settings system
$CC $FLAGS $INCLUDES -c handmade_settings.c -o build/handmade_settings.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile handmade_settings.c"
    exit 1
fi

# Compile default settings
$CC $FLAGS $INCLUDES -c settings_defaults.c -o build/settings_defaults.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile settings_defaults.c"
    exit 1
fi

# Compile UI system
$CC $FLAGS $INCLUDES -c settings_ui.c -o build/settings_ui.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile settings_ui.c"
    exit 1
fi

# Compile file I/O
$CC $FLAGS $INCLUDES -c settings_file.c -o build/settings_file.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile settings_file.c"
    exit 1
fi

echo ""
echo "Creating static library..."

# Create static library
ar rcs build/libhandmade_settings.a \
    build/handmade_settings.o \
    build/settings_defaults.o \
    build/settings_ui.o \
    build/settings_file.o

if [ $? -ne 0 ]; then
    echo "Error: Failed to create static library"
    exit 1
fi

echo ""
echo "Building demo executable..."

# Build demo executable
$CC $FLAGS $INCLUDES \
    settings_demo.c \
    build/libhandmade_settings.a \
    $LIBS \
    -o build/settings_demo

if [ $? -ne 0 ]; then
    echo "Error: Failed to build demo"
    exit 1
fi

echo ""
echo "Building performance test..."

# Build performance test
cat > build/perf_test.c << 'EOF'
#include "../handmade_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Settings System Performance Test ===\n\n");
    
    void *memory = malloc(1024 * 1024);
    settings_system *system = settings_init(memory, 1024 * 1024);
    
    if (!system) {
        printf("Failed to initialize settings system\n");
        return 1;
    }
    
    // Register settings
    clock_t start = clock();
    settings_register_all_defaults(system);
    clock_t end = clock();
    
    double registration_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Settings Registration:\n");
    printf("  Settings: %u\n", system->setting_count);
    printf("  Time: %.2f ms\n", registration_time);
    printf("  Rate: %.1f settings/ms\n", system->setting_count / registration_time);
    
    // Test setting access performance
    start = clock();
    for (int i = 0; i < 10000; i++) {
        settings_get_bool(system, "fullscreen");
        settings_get_int(system, "fov");
        settings_get_float(system, "mouse_sensitivity");
    }
    end = clock();
    
    double access_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\nSettings Access (30,000 operations):\n");
    printf("  Time: %.2f ms\n", access_time);
    printf("  Rate: %.1f ops/ms\n", 30000.0 / access_time);
    printf("  Per access: %.3f μs\n", (access_time * 1000.0) / 30000.0);
    
    // Test setting modification performance
    start = clock();
    for (int i = 0; i < 1000; i++) {
        settings_set_bool(system, "fullscreen", i % 2);
        settings_set_int(system, "fov", 90 + (i % 30));
        settings_set_float(system, "mouse_sensitivity", 1.0f + (i % 10) * 0.1f);
    }
    end = clock();
    
    double modify_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\nSettings Modification (3,000 operations):\n");
    printf("  Time: %.2f ms\n", modify_time);
    printf("  Rate: %.1f ops/ms\n", 3000.0 / modify_time);
    printf("  Per modification: %.3f μs\n", (modify_time * 1000.0) / 3000.0);
    
    // Test profile switching
    int profile1 = settings_create_profile(system, "Profile1", "Test profile 1");
    int profile2 = settings_create_profile(system, "Profile2", "Test profile 2");
    
    start = clock();
    for (int i = 0; i < 100; i++) {
        settings_activate_profile(system, profile1);
        settings_activate_profile(system, profile2);
    }
    end = clock();
    
    double profile_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\nProfile Switching (200 operations):\n");
    printf("  Time: %.2f ms\n", profile_time);
    printf("  Rate: %.1f switches/ms\n", 200.0 / profile_time);
    printf("  Per switch: %.3f μs\n", (profile_time * 1000.0) / 200.0);
    
    // Memory usage
    printf("\nMemory Usage:\n");
    printf("  System size: %lu bytes\n", sizeof(settings_system));
    printf("  Per setting: %lu bytes\n", sizeof(setting));
    printf("  Total allocated: 1024 KB\n");
    printf("  Settings registered: %u\n", system->setting_count);
    printf("  Memory efficiency: %.1f settings/KB\n", 
           system->setting_count / 1024.0f);
    
    free(memory);
    return 0;
}
EOF

$CC $FLAGS $INCLUDES \
    build/perf_test.c \
    build/libhandmade_settings.a \
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
ls -lh build/libhandmade_settings.a 2>/dev/null
ls -lh build/settings_demo 2>/dev/null
if [ -f build/perf_test ]; then
    ls -lh build/perf_test
fi

echo ""
echo "Object file sizes:"
size build/*.o 2>/dev/null | tail -n +2 | sort -k1 -n -r

# Run demo if requested
if [ "$2" == "run" ]; then
    echo ""
    echo "=== Running Demo ==="
    cd build
    ./settings_demo
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
echo "To run the demo: ./build/settings_demo"
echo "To run performance test: ./build/perf_test"
echo "To include in your project: link with build/libhandmade_settings.a"