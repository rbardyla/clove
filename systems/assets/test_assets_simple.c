/*
    Simplified Asset System Test
    Fixed to compile without external dependencies
*/

#include "handmade_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Mock platform and arena for testing
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} simple_arena;

void platform_log(platform_state* platform, char* format, ...) {
    (void)platform;
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

arena* arena_create(platform_state* platform, u64 size) {
    (void)platform;
    simple_arena* a = malloc(sizeof(simple_arena));
    a->base = malloc(size);
    a->size = size;
    a->used = 0;
    return (arena*)a;
}

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    simple_arena* a = (simple_arena*)arena_ptr;
    
    // Align
    u64 aligned_used = (a->used + alignment - 1) & ~(alignment - 1);
    
    if (aligned_used + size > a->size) {
        printf("Arena out of memory! Need %lu, have %lu\n", 
               aligned_used + size, a->size);
        return 0;
    }
    
    void* result = a->base + aligned_used;
    a->used = aligned_used + size;
    return result;
}

void arena_destroy(arena* arena_ptr) {
    simple_arena* a = (simple_arena*)arena_ptr;
    if (a) {
        free(a->base);
        free(a);
    }
}

work_queue* work_queue_create(platform_state* platform, u32 thread_count) {
    (void)platform; (void)thread_count;
    return (work_queue*)malloc(1);
}

void work_queue_destroy(work_queue* queue) {
    free(queue);
}

file_watcher* file_watcher_create(platform_state* platform) {
    (void)platform;
    return (file_watcher*)malloc(1);
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
    texture.format = 0;
    
    u32 pixel_count = texture.width * texture.height;
    u32 pixel_data_size = pixel_count * texture.channels;
    u8* pixels = malloc(pixel_data_size);
    
    // Generate checkerboard pattern
    for (u32 y = 0; y < texture.height; y++) {
        for (u32 x = 0; x < texture.width; x++) {
            u32 index = (y * texture.width + x) * 3;
            u8 value = ((x / 8) + (y / 8)) % 2 ? 255 : 0;
            pixels[index + 0] = value;
            pixels[index + 1] = value;
            pixels[index + 2] = value;
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
    
    // Calculate CRC32 checksum (must match asset system implementation)
    u8* temp_data = malloc(header.uncompressed_size);
    memcpy(temp_data, &texture, sizeof(texture_asset));
    memcpy(temp_data + sizeof(texture_asset), pixels, pixel_data_size);
    
    // Simple CRC32 implementation
    u32 crc = 0xFFFFFFFF;
    static u32 crc_table[256];
    static int table_computed = 0;
    
    if (!table_computed) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int j = 0; j < 8; j++) {
                if (c & 1) {
                    c = 0xEDB88320 ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            crc_table[i] = c;
        }
        table_computed = 1;
    }
    
    for (u64 i = 0; i < header.uncompressed_size; i++) {
        crc = crc_table[(crc ^ temp_data[i]) & 0xFF] ^ (crc >> 8);
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

int main() {
    printf("=== Handmade Asset System Test ===\n\n");
    
    // Create test asset file
    create_test_asset();
    
    // Initialize platform and arena
    int platform = 0;
    arena* main_arena = arena_create((platform_state*)&platform, MEGABYTES(256));
    
    // Configure asset system
    streaming_config config = {
        .max_memory_bytes = MEGABYTES(64),
        .max_concurrent_loads = 4,
        .frames_before_unload = 60,
        .load_distance = 100.0f,
        .enable_compression = 0
    };
    
    // Initialize asset system
    asset_system* assets = asset_system_init((platform_state*)&platform, 
                                             main_arena, config);
    if (!assets) {
        printf("Failed to initialize asset system\n");
        return 1;
    }
    
    printf("Asset system initialized\n");
    
    // Load test asset file
    if (!asset_load_file(assets, "test_texture.hma")) {
        printf("Failed to load test asset file\n");
        return 1;
    }
    
    printf("Loaded test asset file\n");
    
    // Test asset loading
    printf("\n=== Asset Loading Test ===\n");
    
    asset_handle handle = asset_load(assets, "test_checkerboard");
    if (!asset_is_valid(assets, handle)) {
        printf("Failed to load test asset\n");
        return 1;
    }
    
    printf("Loaded asset: %s\n", asset_get_name(assets, handle));
    printf("Asset type: %s\n", asset_type_name(asset_get_type(assets, handle)));
    printf("Asset loaded: %s\n", asset_is_loaded(assets, handle) ? "Yes" : "No");
    
    // Get asset data
    texture_asset* texture = asset_get_texture(assets, handle);
    if (texture) {
        printf("Texture info: %ux%u, %u channels\n", 
               texture->width, texture->height, texture->channels);
        
        // Verify checkerboard pattern
        u8* pixels = texture->pixels;
        u8 first_pixel = pixels[0];
        u8 corner_pixel = pixels[(8 * texture->width + 8) * 3];
        
        printf("Checkerboard verification: first=%u, corner=%u %s\n",
               first_pixel, corner_pixel, 
               (first_pixel != corner_pixel) ? "PASS" : "FAIL");
    }
    
    // Test reference counting
    printf("\n=== Reference Counting Test ===\n");
    
    asset_retain(assets, handle);
    asset_retain(assets, handle);
    printf("Added 2 references\n");
    
    asset_release(assets, handle);
    printf("Released 1 reference, loaded: %s\n", 
           asset_is_loaded(assets, handle) ? "Yes" : "No");
    
    asset_release(assets, handle);
    asset_release(assets, handle);
    printf("Released remaining references, loaded: %s\n", 
           asset_is_loaded(assets, handle) ? "Yes" : "No");
    
    // Print statistics
    printf("\n");
    asset_system_print_stats(assets);
    
    // Cleanup
    asset_system_shutdown(assets);
    arena_destroy(main_arena);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}