#!/bin/bash

# Build asset system and tools

echo "Building handmade asset system..."

# Create required missing headers
echo "// Mock handmade.h for asset system" > ../../src/handmade.h
cat << 'EOF' >> ../../src/handmade.h
#include <stdint.h>
#include <stdarg.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef int32_t b32;

#define internal static
#define MEGABYTES(x) ((x) * 1024 * 1024)

typedef struct arena arena;
typedef struct platform_state platform_state;
typedef struct work_queue work_queue;
typedef struct file_watcher file_watcher;

void platform_log(platform_state* platform, char* format, ...);
arena* arena_create(platform_state* platform, u64 size);
void* arena_push_size(arena* arena, u64 size, u32 alignment);
void arena_destroy(arena* arena);
work_queue* work_queue_create(platform_state* platform, u32 thread_count);
void work_queue_destroy(work_queue* queue);
file_watcher* file_watcher_create(platform_state* platform);
void file_watcher_destroy(file_watcher* watcher);

#define arena_push_struct(arena, type) (type*)arena_push_size(arena, sizeof(type), 16)
EOF

# Build test program
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -g \
    test_assets_simple.c handmade_assets.c \
    -o test_assets \
    -lm

if [ $? -eq 0 ]; then
    echo "✓ Asset system test built successfully"
else
    echo "✗ Asset system test build failed"
    exit 1
fi

# Build asset compiler
cd ../../tools
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -g \
    asset_compiler.c \
    -o asset_compiler \
    -lm

if [ $? -eq 0 ]; then
    echo "✓ Asset compiler built successfully"
else
    echo "✗ Asset compiler build failed"
    exit 1
fi

echo "Asset system build complete!"
echo ""
echo "Usage:"
echo "  ./systems/assets/test_assets     # Run asset system tests"
echo "  ./tools/asset_compiler --help    # Asset compilation help"