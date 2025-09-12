/*
    Test program for AAA Asset Streaming System
    
    Demonstrates:
    - Virtual texture streaming
    - LOD management
    - Memory pool with LRU eviction
    - Async I/O with multiple threads
    - Spatial indexing and predictive loading
    - Defragmentation
*/

#include "handmade_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

// Simple timer
static double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Create test asset files
static void create_test_assets() {
    printf("Creating test assets...\n");
    
    // Create assets directory
    system("mkdir -p assets/streaming");
    
    // Create 100 test assets with different LODs
    for (u32 i = 0; i < 100; i++) {
        char path[256];
        snprintf(path, sizeof(path), "assets/streaming/%016lx.asset", (u64)i);
        
        FILE* f = fopen(path, "wb");
        if (!f) continue;
        
        // Write asset header
        AssetHeader header = {0};
        header.magic = 0x534D4148;  // 'HMAS'
        header.version = 1;
        header.asset_id = i;
        header.type = STREAM_TYPE_TEXTURE;
        header.lod_count = 3;
        
        // Configure LODs
        u32 sizes[] = {MEGABYTES(4), MEGABYTES(2), MEGABYTES(1)};
        for (u32 lod = 0; lod < 3; lod++) {
            header.lods[lod].data_size = sizes[lod];
            header.lods[lod].compressed_size = sizes[lod];  // No compression for test
            header.lods[lod].data_offset = sizeof(AssetHeader);
            for (u32 j = 0; j < lod; j++) {
                header.lods[lod].data_offset += sizes[j];
            }
            header.lods[lod].compression = COMPRESS_NONE;
            header.lods[lod].screen_size_threshold = 0.5f / (lod + 1);
        }
        
        snprintf(header.name, sizeof(header.name), "TestAsset_%03u", i);
        
        fwrite(&header, sizeof(header), 1, f);
        
        // Write dummy data for each LOD
        for (u32 lod = 0; lod < 3; lod++) {
            u8* data = (u8*)malloc(sizes[lod]);
            memset(data, 0xAA + lod, sizes[lod]);
            fwrite(data, sizes[lod], 1, f);
            free(data);
        }
        
        fclose(f);
    }
    
    printf("Created 100 test assets\n");
}

// Simulate camera movement
static void simulate_camera(v3* pos, v3* vel, f32 dt) {
    static f32 angle = 0;
    angle += dt * 0.5f;
    
    // Circular motion with some vertical movement
    pos->x = cosf(angle) * 200.0f;
    pos->z = sinf(angle) * 200.0f;
    pos->y = 50.0f + sinf(angle * 2) * 20.0f;
    
    // Velocity (derivative of position)
    vel->x = -sinf(angle) * 200.0f * 0.5f;
    vel->z = cosf(angle) * 200.0f * 0.5f;
    vel->y = cosf(angle * 2) * 20.0f * 1.0f;
}

// Test virtual texture system
static void test_virtual_textures(StreamingSystem* system) {
    printf("\n=== Testing Virtual Texture System ===\n");
    
    // Create a 16K x 16K virtual texture
    VirtualTexture* vt = streaming_create_virtual_texture(system, 16384, 16384, 0);
    printf("Created 16K x 16K virtual texture\n");
    printf("  Page count: %u x %u\n", vt->page_count_x, vt->page_count_y);
    printf("  Mip levels: %u\n", vt->mip_count);
    
    // Request some pages
    for (u32 y = 0; y < 4; y++) {
        for (u32 x = 0; x < 4; x++) {
            streaming_request_vt_page(system, vt, x, y, 0);
        }
    }
    
    printf("Requested 16 pages (4x4 grid)\n");
    
    // Wait for loading
    usleep(100000);  // 100ms
    
    // Update indirection texture
    streaming_update_vt_indirection(system, vt);
    
    printf("Pages requested: %u\n", atomic_load(&vt->pages_requested));
    printf("Pages resident: %u\n", atomic_load(&vt->pages_resident));
}

// Test LOD switching
static void test_lod_system(StreamingSystem* system) {
    printf("\n=== Testing LOD System ===\n");
    
    // Test LOD calculation
    f32 distances[] = {10, 50, 100, 200, 500, 1000};
    f32 object_radius = 5.0f;
    f32 fov = 1.57f;  // 90 degrees
    
    for (u32 i = 0; i < 6; i++) {
        u32 lod = streaming_calculate_lod(distances[i], object_radius, fov);
        printf("Distance %.0f -> LOD %u\n", distances[i], lod);
    }
    
    // Request asset at different LODs
    u64 asset_id = 42;
    
    streaming_request_asset(system, asset_id, STREAM_PRIORITY_HIGH, 0);
    usleep(50000);
    
    if (streaming_is_resident(system, asset_id, 0)) {
        printf("Asset %lu loaded at LOD 0\n", asset_id);
        
        // Switch to lower LOD
        streaming_switch_lod(system, asset_id, 2);
        printf("Switched to LOD 2\n");
        
        u32 current_lod = streaming_get_current_lod(system, asset_id);
        printf("Current LOD: %u\n", current_lod);
    }
}

// Test memory management
static void test_memory_management(StreamingSystem* system) {
    printf("\n=== Testing Memory Management ===\n");
    
    usize used, available;
    f32 fragmentation;
    
    streaming_get_memory_stats(system, &used, &available, &fragmentation);
    printf("Initial state:\n");
    printf("  Used: %.2f MB\n", used / 1024.0f / 1024.0f);
    printf("  Available: %.2f MB\n", available / 1024.0f / 1024.0f);
    printf("  Fragmentation: %.1f%%\n", fragmentation * 100.0f);
    
    // Load many assets to trigger eviction
    printf("\nLoading 50 assets...\n");
    for (u32 i = 0; i < 50; i++) {
        streaming_request_asset(system, i, STREAM_PRIORITY_NORMAL, 0);
    }
    
    // Wait for loading
    sleep(1);
    
    streaming_get_memory_stats(system, &used, &available, &fragmentation);
    printf("After loading:\n");
    printf("  Used: %.2f MB\n", used / 1024.0f / 1024.0f);
    printf("  Available: %.2f MB\n", available / 1024.0f / 1024.0f);
    printf("  Fragmentation: %.1f%%\n", fragmentation * 100.0f);
    
    // Test defragmentation
    if (fragmentation > 0.1f) {
        printf("\nDefragmenting memory...\n");
        streaming_defragment(system);
        
        streaming_get_memory_stats(system, &used, &available, &fragmentation);
        printf("After defragmentation:\n");
        printf("  Used: %.2f MB\n", used / 1024.0f / 1024.0f);
        printf("  Available: %.2f MB\n", available / 1024.0f / 1024.0f);
        printf("  Fragmentation: %.1f%%\n", fragmentation * 100.0f);
    }
}

// Test compression
static void test_compression() {
    printf("\n=== Testing Compression ===\n");
    
    // Create test data with patterns
    usize src_size = KILOBYTES(64);
    u8* src = (u8*)malloc(src_size);
    u8* dst = (u8*)malloc(src_size * 2);
    u8* verify = (u8*)malloc(src_size);
    
    // Fill with pattern (some repetition for compression)
    for (usize i = 0; i < src_size; i++) {
        if (i % 256 < 128) {
            src[i] = i % 256;
        } else {
            src[i] = 0xAA;  // Repeated value
        }
    }
    
    // Test each compression type
    CompressionType types[] = {COMPRESS_NONE, COMPRESS_LZ4};
    const char* names[] = {"None", "LZ4"};
    
    for (u32 t = 0; t < 2; t++) {
        double start = get_time();
        usize compressed_size = streaming_compress(src, src_size, dst, src_size * 2, types[t]);
        double compress_time = get_time() - start;
        
        if (compressed_size > 0) {
            f32 ratio = (f32)compressed_size / (f32)src_size;
            
            start = get_time();
            usize decompressed_size = streaming_decompress(dst, compressed_size, 
                                                           verify, src_size, types[t]);
            double decompress_time = get_time() - start;
            
            bool valid = (decompressed_size == src_size) && 
                        (memcmp(src, verify, src_size) == 0);
            
            printf("%s: ratio=%.2f, compress=%.3fms, decompress=%.3fms, valid=%s\n",
                   names[t], ratio, compress_time * 1000, decompress_time * 1000,
                   valid ? "yes" : "NO!");
        }
    }
    
    free(src);
    free(dst);
    free(verify);
}

// Main test
int main(int argc, char** argv) {
    printf("=== Handmade AAA Asset Streaming System Test ===\n");
    printf("Production-quality streaming for open-world games\n\n");
    
    // Create test assets if needed
    if (access("assets/streaming", F_OK) != 0) {
        create_test_assets();
    }
    
    // Initialize streaming system with 2GB budget
    StreamingSystem* system = (StreamingSystem*)calloc(1, sizeof(StreamingSystem));
    streaming_init(system, GIGABYTES(2));
    
    printf("Initialized streaming system:\n");
    printf("  Memory budget: 2 GB\n");
    printf("  Worker threads: %u\n", STREAMING_THREAD_COUNT);
    printf("  Max requests: %u\n", MAX_STREAMING_REQUESTS);
    printf("  Virtual texture cache: %.0f MB\n", 
           VIRTUAL_TEXTURE_CACHE_SIZE / 1024.0f / 1024.0f);
    
    // Run tests
    test_compression();
    test_virtual_textures(system);
    test_lod_system(system);
    test_memory_management(system);
    
    // Simulate runtime streaming
    printf("\n=== Simulating Runtime Streaming ===\n");
    printf("Simulating camera movement and streaming...\n");
    
    v3 camera_pos = {0, 50, 0};
    v3 camera_vel = {0, 0, 0};
    
    // Add assets to spatial index
    printf("Adding assets to spatial index...\n");
    SpatialNode* spatial_root = (SpatialNode*)system->spatial_root;
    for (u32 i = 0; i < 100; i++) {
        // Place assets in a grid
        v3 asset_pos = {
            (i % 10) * 100.0f - 450.0f,
            0,
            (i / 10) * 100.0f - 450.0f
        };
        spatial_node_insert(spatial_root, i, asset_pos, 50.0f);
    }
    
    double start_time = get_time();
    u32 frame_count = 0;
    
    while (frame_count < 300) {  // 5 seconds at 60 FPS
        f32 dt = 1.0f / 60.0f;
        
        // Update camera
        simulate_camera(&camera_pos, &camera_vel, dt);
        
        // Update streaming
        streaming_update(system, camera_pos, camera_vel, dt);
        
        // Prefetch in radius
        if (frame_count % 30 == 0) {  // Every 0.5 seconds
            streaming_prefetch_radius(system, camera_pos, 300.0f);
        }
        
        // Print stats every second
        if (frame_count % 60 == 0) {
            StreamingStats stats = streaming_get_stats(system);
            printf("Frame %u: Requests=%u, Completed=%u, CacheHits=%u, Loaded=%.1fMB\n",
                   frame_count,
                   atomic_load(&stats.total_requests),
                   atomic_load(&stats.completed_requests),
                   atomic_load(&stats.cache_hits),
                   atomic_load(&stats.bytes_loaded) / 1024.0f / 1024.0f);
        }
        
        usleep(16666);  // ~60 FPS
        frame_count++;
    }
    
    double elapsed = get_time() - start_time;
    printf("\nSimulation complete in %.2f seconds\n", elapsed);
    
    // Final statistics
    printf("\n=== Final Statistics ===\n");
    StreamingStats stats = streaming_get_stats(system);
    printf("Total requests: %u\n", atomic_load(&stats.total_requests));
    printf("Completed: %u\n", atomic_load(&stats.completed_requests));
    printf("Failed: %u\n", atomic_load(&stats.failed_requests));
    printf("Cache hits: %u\n", atomic_load(&stats.cache_hits));
    printf("Cache misses: %u\n", atomic_load(&stats.cache_misses));
    printf("Bytes loaded: %.2f MB\n", atomic_load(&stats.bytes_loaded) / 1024.0f / 1024.0f);
    printf("Bytes evicted: %.2f MB\n", atomic_load(&stats.bytes_evicted) / 1024.0f / 1024.0f);
    printf("Current memory: %.2f MB\n", stats.current_memory_usage / 1024.0f / 1024.0f);
    printf("Peak memory: %.2f MB\n", stats.peak_memory_usage / 1024.0f / 1024.0f);
    
    // Dump state for debugging
    streaming_dump_state(system, "streaming_state.txt");
    printf("\nState dumped to streaming_state.txt\n");
    
    // Shutdown
    printf("\nShutting down...\n");
    streaming_shutdown(system);
    free(system);
    
    printf("Test complete!\n");
    
    return 0;
}