#!/bin/bash

# Handmade Achievement System - Build Script
# Builds the achievement system and demo

echo "=== Building Handmade Achievement System ==="

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
echo "Building achievement system library..."

# Compile core achievement system
$CC $FLAGS $INCLUDES -c handmade_achievements.c -o build/handmade_achievements.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile handmade_achievements.c"
    exit 1
fi

# Compile default achievements
$CC $FLAGS $INCLUDES -c achievements_defaults.c -o build/achievements_defaults.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile achievements_defaults.c"
    exit 1
fi

# Compile file I/O
$CC $FLAGS $INCLUDES -c achievements_file.c -o build/achievements_file.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile achievements_file.c"
    exit 1
fi

echo ""
echo "Creating static library..."

# Create static library
ar rcs build/libhandmade_achievements.a \
    build/handmade_achievements.o \
    build/achievements_defaults.o \
    build/achievements_file.o

if [ $? -ne 0 ]; then
    echo "Error: Failed to create static library"
    exit 1
fi

echo ""
echo "Building demo executable..."

# Build demo executable
$CC $FLAGS $INCLUDES \
    achievements_demo.c \
    build/libhandmade_achievements.a \
    $LIBS \
    -o build/achievements_demo

if [ $? -ne 0 ]; then
    echo "Error: Failed to build demo"
    exit 1
fi

echo ""
echo "Building performance test..."

# Build performance test
cat > build/perf_test.c << 'EOF'
#include "../handmade_achievements.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Achievement System Performance Test ===\\n\\n");
    
    void *memory = malloc(1024 * 1024); // 1MB
    achievement_system *system = achievements_init(memory, 1024 * 1024);
    
    if (!system) {
        printf("Failed to initialize achievement system\\n");
        return 1;
    }
    
    // Register some achievements for testing
    achievements_register_all_defaults(system);
    
    clock_t start = clock();
    achievements_register_all_defaults(system);
    clock_t end = clock();
    
    double registration_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Achievement Registration:\\n");
    printf("  Achievements: %u\\n", system->achievement_count);
    printf("  Statistics: %u\\n", system->stat_count);
    printf("  Time: %.2f ms\\n", registration_time);
    printf("  Rate: %.1f achievements/ms\\n", system->achievement_count / registration_time);
    
    // Test achievement lookups
    start = clock();
    for (int i = 0; i < 100000; i++) {
        achievements_find(system, "first_blood");
        achievements_find(system, "slayer");
        achievements_find(system, "explorer");
    }
    end = clock();
    
    double lookup_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nAchievement Lookups (300,000 operations):\\n");
    printf("  Time: %.2f ms\\n", lookup_time);
    printf("  Rate: %.1f ops/ms\\n", 300000.0 / lookup_time);
    printf("  Per lookup: %.3f μs\\n", (lookup_time * 1000.0) / 300000.0);
    
    // Test statistic updates
    start = clock();
    for (int i = 0; i < 50000; i++) {
        achievements_add_stat_int(system, "enemies_killed", 1);
        achievements_add_stat_float(system, "distance_traveled", 1.0f);
    }
    end = clock();
    
    double stat_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nStatistic Updates (100,000 operations):\\n");
    printf("  Time: %.2f ms\\n", stat_time);
    printf("  Rate: %.1f ops/ms\\n", 100000.0 / stat_time);
    printf("  Per update: %.3f μs\\n", (stat_time * 1000.0) / 100000.0);
    
    // Test unlock performance
    start = clock();
    for (int i = 0; i < 1000; i++) {
        achievements_unlock(system, "first_blood");
        achievements_unlock(system, "slayer");
    }
    end = clock();
    
    double unlock_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nAchievement Unlocks (2,000 operations):\\n");
    printf("  Time: %.2f ms\\n", unlock_time);
    printf("  Rate: %.1f ops/ms\\n", 2000.0 / unlock_time);
    printf("  Per unlock: %.3f μs\\n", (unlock_time * 1000.0) / 2000.0);
    
    // Memory usage analysis
    printf("\\nMemory Usage:\\n");
    printf("  System size: %lu bytes\\n", sizeof(achievement_system));
    printf("  Per achievement: %lu bytes\\n", sizeof(achievement));
    printf("  Per statistic: %lu bytes\\n", sizeof(game_stat));
    printf("  Total allocated: 1024 KB\\n");
    printf("  Achievements: %u\\n", system->achievement_count);
    printf("  Statistics: %u\\n", system->stat_count);
    printf("  Memory efficiency: %.1f achievements/KB\\n", 
           system->achievement_count / 1024.0f);
    
    // Test file I/O performance
    start = clock();
    achievements_save(system);
    end = clock();
    
    double save_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("\\nFile I/O Performance:\\n");
    printf("  Save time: %.2f ms\\n", save_time);
    
    start = clock();
    achievements_load(system);
    end = clock();
    
    double load_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("  Load time: %.2f ms\\n", load_time);
    
    free(memory);
    return 0;
}
EOF

$CC $FLAGS $INCLUDES \
    build/perf_test.c \
    build/libhandmade_achievements.a \
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
ls -lh build/libhandmade_achievements.a 2>/dev/null
ls -lh build/achievements_demo 2>/dev/null
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
    ./achievements_demo
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
echo "To run the demo: ./build/achievements_demo"
echo "To run performance test: ./build/perf_test"
echo "To include in your project: link with build/libhandmade_achievements.a"