/*
    Asset System Test Program
    Tests asset loading, memory management, and performance
*/

#include "handmade_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

// Mock platform functions for testing
typedef struct {
    int dummy;
} mock_platform;

void platform_log(platform_state* platform, char* format, ...) {
    (void)platform;
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// Mock arena implementation
typedef struct mock_arena {
    u8* base;
    u64 size;
    u64 used;
} mock_arena;

arena* arena_create(platform_state* platform, u64 size) {
    (void)platform;
    mock_arena* a = malloc(sizeof(mock_arena));
    a->base = malloc(size);
    a->size = size;
    a->used = 0;
    return (arena*)a;
}

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    mock_arena* a = (mock_arena*)arena_ptr;
    
    // Align
    u64 aligned_used = (a->used + alignment - 1) & ~(alignment - 1);
    
    if (aligned_used + size > a->size) {
        printf("Arena out of memory! Need %llu, have %llu\n", 
               aligned_used + size, a->size);
        return 0;
    }
    
    void* result = a->base + aligned_used;
    a->used = aligned_used + size;
    return result;
}

void arena_destroy(arena* arena_ptr) {
    mock_arena* a = (mock_arena*)arena_ptr;
    if (a) {
        free(a->base);
        free(a);
    }
}

// Mock work queue
work_queue* work_queue_create(platform_state* platform, u32 thread_count) {
    (void)platform; (void)thread_count;
    return (work_queue*)malloc(1);  // Dummy pointer
}

void work_queue_destroy(work_queue* queue) {
    free(queue);
}

// Mock file watcher
file_watcher* file_watcher_create(platform_state* platform) {
    (void)platform;
    return (file_watcher*)malloc(1);  // Dummy pointer
}

void file_watcher_destroy(file_watcher* watcher) {
    free(watcher);
}

// Create test asset file
void create_test_asset() {
    printf("Creating test asset file...\n");
    
    FILE* file = fopen("test_texture.hma", "wb");
    if (!file) {
        printf("Failed to create test asset file\n");
        return;
    }
    
    // Create a simple texture asset
    texture_asset texture;
    texture.width = 64;
    texture.height = 64;
    texture.channels = 3;
    texture.format = 0;  // RGB
    
    u32 pixel_count = texture.width * texture.height;
    u32 pixel_data_size = pixel_count * texture.channels;
    u8* pixels = malloc(pixel_data_size);
    
    // Generate checkerboard pattern
    for (u32 y = 0; y < texture.height; y++) {
        for (u32 x = 0; x < texture.width; x++) {
            u32 index = (y * texture.width + x) * 3;
            u8 value = ((x / 8) + (y / 8)) % 2 ? 255 : 0;
            pixels[index + 0] = value;     // R
            pixels[index + 1] = value;     // G
            pixels[index + 2] = value;     // B
        }
    }
    
    texture.pixels = pixels;
    
    // Create asset header
    asset_header header = {0};
    header.magic = ASSET_MAGIC;
    header.version = ASSET_VERSION;
    header.type = ASSET_TYPE_TEXTURE;
    header.compression = ASSET_COMPRESSION_NONE;
    header.uncompressed_size = sizeof(texture_asset) + pixel_data_size;
    header.compressed_size = header.uncompressed_size;
    header.data_offset = 0;
    strcpy(header.name, "test_checkerboard");
    
    // Calculate checksum
    u8* temp_data = malloc(header.uncompressed_size);
    memcpy(temp_data, &texture, sizeof(texture_asset));
    memcpy(temp_data + sizeof(texture_asset), pixels, pixel_data_size);
    
    u32 crc = 0xFFFFFFFF;
    for (u64 i = 0; i < header.uncompressed_size; i++) {
        crc ^= temp_data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    header.checksum = crc ^ 0xFFFFFFFF;
    
    // Write to file
    fwrite(&header, sizeof(asset_header), 1, file);
    fwrite(temp_data, header.uncompressed_size, 1, file);
    
    fclose(file);
    free(pixels);
    free(temp_data);
    
    printf("Created test asset: 64x64 checkerboard texture\n");
}

// Performance test
void performance_test(asset_system* assets) {
    printf("\n=== Performance Test ===\n");
    
    const int ITERATIONS = 1000;
    double start_time, end_time;
    
    // Test asset lookup performance
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start_time = ts.tv_sec + ts.tv_nsec / 1e9;
    
    for (int i = 0; i < ITERATIONS; i++) {
        asset_handle handle = asset_load(assets, "test_checkerboard");
        texture_asset* tex = asset_get_texture(assets, handle);
        if (tex) {
            // Access data to prevent optimization
            volatile u32 width = tex->width;
            (void)width;
        }
        asset_unload(assets, handle);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &ts);
    end_time = ts.tv_sec + ts.tv_nsec / 1e9;
    
    double total_time = (end_time - start_time) * 1000.0; // ms
    double time_per_load = total_time / ITERATIONS;
    
    printf("Asset load/unload performance:\n");
    printf("  %d iterations in %.2f ms\n", ITERATIONS, total_time);
    printf("  %.3f ms per load/unload\n", time_per_load);
    printf("  %.0f loads per second\n", 1000.0 / time_per_load);
}\n\nint main() {\n    printf(\"=== Handmade Asset System Test ===\\n\\n\");\n    \n    // Create test asset file\n    create_test_asset();\n    \n    // Initialize platform and arena\n    mock_platform platform = {0};\n    arena* main_arena = arena_create((platform_state*)&platform, MEGABYTES(256));\n    \n    // Configure asset system\n    streaming_config config = {\n        .max_memory_bytes = MEGABYTES(64),\n        .max_concurrent_loads = 4,\n        .frames_before_unload = 60,\n        .load_distance = 100.0f,\n        .enable_compression = 0\n    };\n    \n    // Initialize asset system\n    asset_system* assets = asset_system_init((platform_state*)&platform, \n                                             main_arena, config);\n    if (!assets) {\n        printf(\"Failed to initialize asset system\\n\");\n        return 1;\n    }\n    \n    printf(\"Asset system initialized\\n\");\n    \n    // Load test asset file\n    if (!asset_load_file(assets, \"test_texture.hma\")) {\n        printf(\"Failed to load test asset file\\n\");\n        return 1;\n    }\n    \n    printf(\"Loaded test asset file\\n\");\n    \n    // Test asset loading\n    printf(\"\\n=== Asset Loading Test ===\\n\");\n    \n    asset_handle handle = asset_load(assets, \"test_checkerboard\");\n    if (!asset_is_valid(assets, handle)) {\n        printf(\"Failed to load test asset\\n\");\n        return 1;\n    }\n    \n    printf(\"Loaded asset: %s\\n\", asset_get_name(assets, handle));\n    printf(\"Asset type: %s\\n\", asset_type_name(asset_get_type(assets, handle)));\n    printf(\"Asset loaded: %s\\n\", asset_is_loaded(assets, handle) ? \"Yes\" : \"No\");\n    \n    // Get asset data\n    texture_asset* texture = asset_get_texture(assets, handle);\n    if (texture) {\n        printf(\"Texture info: %ux%u, %u channels\\n\", \n               texture->width, texture->height, texture->channels);\n        \n        // Verify checkerboard pattern\n        u8* pixels = texture->pixels;\n        u8 first_pixel = pixels[0];\n        u8 corner_pixel = pixels[(8 * texture->width + 8) * 3];\n        \n        printf(\"Checkerboard verification: first=%u, corner=%u %s\\n\",\n               first_pixel, corner_pixel, \n               (first_pixel != corner_pixel) ? \"PASS\" : \"FAIL\");\n    }\n    \n    // Test reference counting\n    printf(\"\\n=== Reference Counting Test ===\\n\");\n    \n    asset_retain(assets, handle);\n    asset_retain(assets, handle);\n    printf(\"Added 2 references\\n\");\n    \n    asset_release(assets, handle);\n    printf(\"Released 1 reference, loaded: %s\\n\", \n           asset_is_loaded(assets, handle) ? \"Yes\" : \"No\");\n    \n    asset_release(assets, handle);\n    asset_release(assets, handle);\n    printf(\"Released remaining references, loaded: %s\\n\", \n           asset_is_loaded(assets, handle) ? \"Yes\" : \"No\");\n    \n    // Performance test\n    performance_test(assets);\n    \n    // Print statistics\n    printf(\"\\n\");\n    asset_system_print_stats(assets);\n    \n    // Cleanup\n    asset_system_shutdown(assets);\n    arena_destroy(main_arena);\n    \n    printf(\"\\n=== Test Complete ===\\n\");\n    return 0;\n}\n"}