#!/bin/bash

# Handmade World Generation - Build Script
# Builds the complete procedural world generation system and demo

echo "=== Building Handmade Procedural World Generation ==="

# Compiler settings
CC="gcc"
CFLAGS="-Wall -Wextra -O3 -march=native -ffast-math -fno-strict-aliasing"
CFLAGS_DEBUG="-Wall -Wextra -g -O0 -DDEBUG -fsanitize=address"
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
echo "Building World Generation system..."

# Build required dependencies first (achievements and settings systems)
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
cd ../world_gen

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
cd ../world_gen

# Compile core world generation system
echo ""
echo "Compiling core world generation components..."

$CC $FLAGS $INCLUDES -c handmade_world_gen.c -o build/handmade_world_gen.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile handmade_world_gen.c"
    exit 1
fi

$CC $FLAGS $INCLUDES -c world_gen_chunks.c -o build/world_gen_chunks.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile world_gen_chunks.c"
    exit 1
fi

$CC $FLAGS $INCLUDES -c world_gen_resources.c -o build/world_gen_resources.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile world_gen_resources.c"
    exit 1
fi

$CC $FLAGS $INCLUDES -c world_gen_integration.c -o build/world_gen_integration.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile world_gen_integration.c"
    exit 1
fi

echo ""
echo "Creating static library..."

# Create static library
ar rcs build/libhandmade_world_gen.a \
    build/handmade_world_gen.o \
    build/world_gen_chunks.o \
    build/world_gen_resources.o \
    build/world_gen_integration.o

if [ $? -ne 0 ]; then
    echo "Error: Failed to create static library"
    exit 1
fi

echo ""
echo "Building demo executable..."

# Build demo executable with all dependencies
$CC $FLAGS $INCLUDES \
    world_gen_demo.c \
    build/libhandmade_world_gen.a \
    ../achievements/build/libhandmade_achievements.a \
    ../settings/build/libhandmade_settings.a \
    $LIBS \
    -o build/world_gen_demo

if [ $? -ne 0 ]; then
    echo "Error: Failed to build demo"
    exit 1
fi

echo ""
echo "Building performance test..."

# Build performance test
cat > build/perf_test.c << 'EOF'
#include "../handmade_world_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== World Generation Performance Test ===\n\n");
    
    void *memory = malloc(Megabytes(16));
    world_gen_system *system = world_gen_init(memory, Megabytes(16), 123456789);
    
    if (!system) {
        printf("Failed to initialize world generation system\n");
        return 1;
    }
    
    printf("World Generation System Performance:\n");
    printf("  Initialized: %s\n", system->initialized ? "Yes" : "No");
    printf("  World seed: %llu\n", (unsigned long long)system->world_seed);
    printf("  Memory allocated: 16 MB\n");
    printf("  Registered biomes: %u\n", system->biome_count);
    
    clock_t start = clock();
    
    // Test terrain sampling speed
    printf("\nTesting terrain sampling...\n");
    start = clock();
    for (int i = 0; i < 100000; i++) {
        float x = (float)(i % 500) * 10.0f;
        float y = (float)(i / 500) * 10.0f;
        world_gen_sample_elevation(system, x, y);
    }
    clock_t end = clock();
    
    double terrain_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Terrain Sampling (100,000 samples): %.2f ms\n", terrain_time);
    printf("Rate: %.1f samples/ms (%.3f μs per sample)\n", 
           100000.0 / terrain_time, (terrain_time * 1000.0) / 100000.0);
    
    // Test chunk generation speed
    printf("\nTesting chunk generation...\n");
    start = clock();
    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            world_chunk *chunk = world_gen_generate_chunk(system, cx + 10, cy + 10);
            if (chunk) {
                // Force resource generation
                generation_context ctx;
                ctx.world_gen = system;
                ctx.chunk = chunk;
                ctx.global_x = (cx + 10) * WORLD_CHUNK_SIZE;
                ctx.global_y = (cy + 10) * WORLD_CHUNK_SIZE;
                ctx.random_seed = (unsigned int)world_gen_get_chunk_seed(system, cx + 10, cy + 10);
                world_gen_distribute_resources(&ctx);
            }
        }
    }
    end = clock();
    
    double chunk_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Chunk Generation (64 chunks): %.2f ms\n", chunk_time);
    printf("Rate: %.2f ms per chunk\n", chunk_time / 64.0);
    
    // Test biome sampling speed
    printf("\nTesting biome sampling...\n");
    start = clock();
    for (int i = 0; i < 50000; i++) {
        float x = (float)(i % 300) * 15.0f;
        float y = (float)(i / 300) * 15.0f;
        world_gen_sample_biome(system, x, y);
    }
    end = clock();
    
    double biome_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Biome Sampling (50,000 samples): %.2f ms\n", biome_time);
    printf("Rate: %.1f samples/ms\n", 50000.0 / biome_time);
    
    // Test noise generation speed
    printf("\nTesting noise generation...\n");
    start = clock();
    noise_params test_noise = {0.01f, 100.0f, 6, 2.0f, 0.5f, 0.0f, 0.0f, 12345};
    for (int i = 0; i < 200000; i++) {
        float x = (float)(i % 600) * 2.0f;
        float y = (float)(i / 600) * 2.0f;
        world_gen_noise_2d(&test_noise, x, y);
    }
    end = clock();
    
    double noise_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("Noise Generation (200,000 samples): %.2f ms\n", noise_time);
    printf("Rate: %.1f samples/ms\n", 200000.0 / noise_time);
    
    // Performance summary
    printf("\n=== Performance Summary ===\n");
    printf("Target chunk generation: <16ms (60 FPS)\n");
    printf("Achieved chunk generation: %.2f ms\n", chunk_time / 64.0);
    if ((chunk_time / 64.0) < 16.0) {
        printf("✓ Performance target achieved (%.1fx faster than required)\n", 
               16.0 / (chunk_time / 64.0));
    } else {
        printf("⚠ Performance target missed (%.1fx slower than required)\n", 
               (chunk_time / 64.0) / 16.0);
    }
    
    printf("Memory efficiency: %.1f KB per chunk\n", 
           (float)(sizeof(world_chunk)) / 1024.0f);
    printf("System memory usage: %u KB\n", sizeof(world_gen_system) / 1024);
    
    // Cleanup
    world_gen_shutdown(system);
    free(memory);
    
    printf("\nAll performance tests completed successfully!\n");
    return 0;
}
EOF

$CC $FLAGS $INCLUDES \
    build/perf_test.c \
    build/libhandmade_world_gen.a \
    $LIBS \
    -o build/perf_test

if [ $? -ne 0 ]; then
    echo "Warning: Failed to build performance test"
fi

# Build heightmap export utility
echo ""
echo "Building heightmap export utility..."

cat > build/export_heightmap.c << 'EOF'
#include "../handmade_world_gen.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int width = 512, height = 512;
    unsigned long long seed = 424242;
    
    if (argc > 1) width = atoi(argv[1]);
    if (argc > 2) height = atoi(argv[2]);
    if (argc > 3) seed = strtoull(argv[3], NULL, 10);
    
    printf("Exporting %dx%d heightmap with seed %llu\n", width, height, seed);
    
    void *memory = malloc(Megabytes(16));
    world_gen_system *system = world_gen_init(memory, Megabytes(16), seed);
    
    if (!system) {
        printf("Failed to initialize world generation\n");
        return 1;
    }
    
    world_gen_export_heightmap(system, "heightmap.pgm", width, height);
    world_gen_export_biome_map(system, "biomemap.txt", width, height);
    
    printf("Files exported: heightmap.pgm, biomemap.txt\n");
    
    world_gen_shutdown(system);
    free(memory);
    return 0;
}
EOF

$CC $FLAGS $INCLUDES \
    build/export_heightmap.c \
    build/libhandmade_world_gen.a \
    $LIBS \
    -o build/export_heightmap

if [ $? -ne 0 ]; then
    echo "Warning: Failed to build heightmap export utility"
fi

# Generate size report
echo ""
echo "=== Build Complete ==="
echo ""
echo "Size report:"
ls -lh build/libhandmade_world_gen.a 2>/dev/null
ls -lh build/world_gen_demo 2>/dev/null
if [ -f build/perf_test ]; then
    ls -lh build/perf_test
fi
if [ -f build/export_heightmap ]; then
    ls -lh build/export_heightmap
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
    echo "=== Running World Generation Demo ==="
    cd build
    ./world_gen_demo
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

# Export heightmap if requested
if [ "$2" == "export" ]; then
    echo ""
    echo "=== Exporting Heightmap ==="
    cd build
    ./export_heightmap 256 256
    echo "Generated: heightmap.pgm, biomemap.txt"
    cd ..
fi

echo ""
echo "Build successful!"
echo ""
echo "Available executables:"
echo "  ./build/world_gen_demo     - Complete world generation demonstration"
echo "  ./build/perf_test         - Performance benchmarks and analysis"  
echo "  ./build/export_heightmap  - Export heightmaps and biome maps"
echo ""
echo "To include in your project: link with build/libhandmade_world_gen.a"
echo ""
echo "Usage examples:"
echo "  ./build_world_gen.sh run         # Build and run demo"
echo "  ./build_world_gen.sh perf        # Build and run performance test"
echo "  ./build_world_gen.sh export      # Build and export heightmaps"
echo "  ./build_world_gen.sh debug run   # Build debug version and run"