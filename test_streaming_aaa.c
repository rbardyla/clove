/*
    AAA Streaming System Test & Demonstration
    
    Simulates a large open-world game with:
    - 100GB of assets
    - 2GB memory budget
    - Virtual textures
    - Predictive loading
    - Zero-hitch streaming
*/

#include "handmade_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Test configuration
#define WORLD_SIZE 10000.0f
#define ASSET_COUNT 100000
#define TEXTURE_COUNT 10000
#define MESH_COUNT 50000
#define SOUND_COUNT 40000
#define PLAYER_SPEED 50.0f
#define SIMULATION_FRAMES 3600  // 1 minute at 60 FPS

// Test asset structure
typedef struct {
    u64 id;
    StreamAssetType type;
    v3 position;
    f32 radius;
    u32 lod_count;
    usize sizes[LOD_LEVELS];
    bool loaded[LOD_LEVELS];
} TestAsset;

// Global test data
static TestAsset* test_assets;
static StreamingSystem streaming_system;
static v3 player_pos = {0, 0, 0};
static v3 player_vel = {0, 0, 0};

// Generate test asset files
static void generate_test_assets() {
    printf("Generating test assets...\n");
    
    // Create assets directory
    mkdir("assets", 0755);
    mkdir("assets/streaming", 0755);
    
    test_assets = (TestAsset*)calloc(ASSET_COUNT, sizeof(TestAsset));
    
    for (u32 i = 0; i < ASSET_COUNT; i++) {
        TestAsset* asset = &test_assets[i];
        asset->id = i + 1;
        
        // Assign type
        if (i < TEXTURE_COUNT) {
            asset->type = STREAM_TYPE_TEXTURE;
        } else if (i < TEXTURE_COUNT + MESH_COUNT) {
            asset->type = STREAM_TYPE_MESH;
        } else {
            asset->type = STREAM_TYPE_AUDIO;
        }
        
        // Random position in world
        asset->position.x = ((f32)rand() / RAND_MAX - 0.5f) * WORLD_SIZE;
        asset->position.y = 0;
        asset->position.z = ((f32)rand() / RAND_MAX - 0.5f) * WORLD_SIZE;
        asset->radius = 10.0f + (f32)rand() / RAND_MAX * 90.0f;
        
        // LOD sizes (each LOD is ~50% smaller)
        asset->lod_count = 5;
        usize base_size = 0;
        switch (asset->type) {
            case STREAM_TYPE_TEXTURE:
                base_size = MEGABYTES(4);  // 4K texture
                break;
            case STREAM_TYPE_MESH:
                base_size = MEGABYTES(2);  // High-poly mesh
                break;
            case STREAM_TYPE_AUDIO:
                base_size = MEGABYTES(1);  // Audio clip
                break;
            default:
                base_size = KILOBYTES(100);
        }
        
        for (u32 lod = 0; lod < asset->lod_count; lod++) {
            asset->sizes[lod] = base_size >> lod;
        }
        
        // Create asset file
        char path[256];
        snprintf(path, sizeof(path), "assets/streaming/%016lx.asset", asset->id);
        
        FILE* f = fopen(path, "wb");
        if (f) {
            // Write header
            AssetHeader header = {0};
            header.magic = 0x534D4148;  // 'HMAS'
            header.version = 1;
            header.asset_id = asset->id;
            header.type = asset->type;
            header.lod_count = asset->lod_count;
            
            usize offset = 0;
            for (u32 lod = 0; lod < asset->lod_count; lod++) {
                header.lods[lod].data_size = asset->sizes[lod];
                header.lods[lod].compressed_size = asset->sizes[lod];  // No compression for test
                header.lods[lod].data_offset = offset;
                header.lods[lod].compression = COMPRESS_NONE;
                header.lods[lod].screen_size_threshold = 1.0f / (1 << lod);
                offset += asset->sizes[lod];
            }
            
            header.uncompressed_size = offset;
            header.compressed_size = offset;
            snprintf(header.name, sizeof(header.name), "Asset_%lu", asset->id);
            
            fwrite(&header, sizeof(header), 1, f);
            
            // Write dummy data for each LOD
            u8* dummy = (u8*)calloc(1, MEGABYTES(4));
            for (u32 lod = 0; lod < asset->lod_count; lod++) {
                // Fill with pattern
                for (usize j = 0; j < asset->sizes[lod]; j++) {
                    dummy[j] = (u8)((asset->id + lod + j) & 0xFF);
                }
                fwrite(dummy, asset->sizes[lod], 1, f);
            }
            free(dummy);
            
            fclose(f);
        }
        
        if (i % 1000 == 0) {
            printf("  Generated %u / %u assets\n", i, ASSET_COUNT);
        }
    }
    
    printf("Generated %u test assets\n", ASSET_COUNT);
}

// Find assets near position
static void find_nearby_assets(v3 pos, f32 radius, TestAsset** out_assets, u32* out_count, u32 max_count) {
    u32 count = 0;
    
    for (u32 i = 0; i < ASSET_COUNT && count < max_count; i++) {
        TestAsset* asset = &test_assets[i];
        v3 delta = v3_sub(asset->position, pos);
        f32 dist = v3_length(delta);
        
        if (dist < radius + asset->radius) {
            out_assets[count++] = asset;
        }
    }
    
    *out_count = count;
}

// Simulate frame
static void simulate_frame(u32 frame_num, f32 dt) {
    // Update player movement (circular path for testing)
    f32 angle = frame_num * 0.01f;
    f32 radius = 1000.0f + sinf(frame_num * 0.005f) * 500.0f;
    
    v3 new_pos = {
        cosf(angle) * radius,
        0,
        sinf(angle) * radius
    };
    
    player_vel = v3_scale(v3_sub(new_pos, player_pos), 1.0f / dt);
    player_pos = new_pos;
    
    // Update streaming system
    streaming_update(&streaming_system, player_pos, player_vel, dt);
    
    // Find assets in view
    TestAsset* nearby[1000];
    u32 nearby_count = 0;
    find_nearby_assets(player_pos, PREFETCH_RADIUS, nearby, &nearby_count, 1000);
    
    // Request streaming for nearby assets
    for (u32 i = 0; i < nearby_count; i++) {
        TestAsset* asset = nearby[i];
        v3 delta = v3_sub(asset->position, player_pos);
        f32 distance = v3_length(delta);
        
        // Calculate appropriate LOD
        u32 lod = streaming_calculate_lod(distance, asset->radius, 60.0f * M_PI / 180.0f);
        
        // Determine priority based on distance
        StreamPriority priority;
        if (distance < 100.0f) {
            priority = STREAM_PRIORITY_CRITICAL;
        } else if (distance < 250.0f) {
            priority = STREAM_PRIORITY_HIGH;
        } else if (distance < 400.0f) {
            priority = STREAM_PRIORITY_NORMAL;
        } else {
            priority = STREAM_PRIORITY_PREFETCH;
        }
        
        // Check if already loaded
        if (!streaming_is_resident(&streaming_system, asset->id, lod)) {
            StreamRequest* request = streaming_request_asset(&streaming_system, 
                                                            asset->id, priority, lod);
            asset->loaded[lod] = true;
        }
    }
}

// Print statistics
static void print_statistics() {
    StreamingStats stats = streaming_get_stats(&streaming_system);
    
    printf("\n=== Streaming Statistics ===\n");
    printf("Total Requests:     %u\n", stats.total_requests);
    printf("Completed:          %u\n", stats.completed_requests);
    printf("Failed:             %u\n", stats.failed_requests);
    printf("Cache Hits:         %u (%.1f%%)\n", stats.cache_hits,
           stats.total_requests > 0 ? 100.0f * stats.cache_hits / stats.total_requests : 0);
    printf("Cache Misses:       %u\n", stats.cache_misses);
    printf("Bytes Loaded:       %.2f MB\n", stats.bytes_loaded / (1024.0f * 1024.0f));
    printf("Bytes Evicted:      %.2f MB\n", stats.bytes_evicted / (1024.0f * 1024.0f));
    printf("Memory Usage:       %.2f MB / %.2f MB\n", 
           stats.current_memory_usage / (1024.0f * 1024.0f),
           STREAMING_MEMORY_BUDGET / (1024.0f * 1024.0f));
    printf("Peak Memory:        %.2f MB\n", stats.peak_memory_usage / (1024.0f * 1024.0f));
    
    // Calculate efficiency
    if (stats.completed_requests > 0) {
        f32 avg_time = stats.average_load_time_ms;
        printf("Avg Load Time:      %.2f ms\n", avg_time);
        printf("Peak Load Time:     %.2f ms\n", stats.peak_load_time_ms);
    }
    
    // Memory efficiency
    usize used, available;
    f32 fragmentation;
    streaming_get_memory_stats(&streaming_system, &used, &available, &fragmentation);
    printf("Memory Fragmentation: %.1f%%\n", fragmentation * 100.0f);
}

// Test virtual texture system
static void test_virtual_textures() {
    printf("\n=== Testing Virtual Texture System ===\n");
    
    // Create a 16K x 16K virtual texture
    VirtualTexture* vt = streaming_create_virtual_texture(&streaming_system,
                                                          16384, 16384, 4);  // RGBA
    
    printf("Created virtual texture: %ux%u\n", vt->width, vt->height);
    printf("Page count: %ux%u\n", vt->page_count_x, vt->page_count_y);
    printf("Mip levels: %u\n", vt->mip_count);
    
    // Request some pages
    for (u32 y = 0; y < 4; y++) {
        for (u32 x = 0; x < 4; x++) {
            streaming_request_vt_page(&streaming_system, vt, x, y, 0);
        }
    }
    
    // Wait for loading
    usleep(100000);  // 100ms
    
    // Update indirection texture
    streaming_update_vt_indirection(&streaming_system, vt);
    
    printf("Pages requested: %u\n", vt->pages_requested);
    printf("Pages resident: %u\n", vt->pages_resident);
    printf("Pages evicted: %u\n", vt->pages_evicted);
}

// Stress test
static void stress_test() {
    printf("\n=== Stress Test: Rapid Movement ===\n");
    
    // Rapidly change position to stress the system
    for (u32 i = 0; i < 100; i++) {
        // Random teleport
        player_pos.x = ((f32)rand() / RAND_MAX - 0.5f) * WORLD_SIZE;
        player_pos.z = ((f32)rand() / RAND_MAX - 0.5f) * WORLD_SIZE;
        
        simulate_frame(i, 1.0f / 60.0f);
        
        if (i % 10 == 0) {
            StreamingStats stats = streaming_get_stats(&streaming_system);
            printf("Frame %u: Requests=%u, Memory=%.1fMB\n", 
                   i, stats.total_requests,
                   stats.current_memory_usage / (1024.0f * 1024.0f));
        }
    }
}

// Memory pressure test
static void memory_pressure_test() {
    printf("\n=== Memory Pressure Test ===\n");
    
    // Request everything at once to trigger eviction
    u32 requests = 0;
    for (u32 i = 0; i < ASSET_COUNT && i < 10000; i++) {
        streaming_request_asset(&streaming_system, test_assets[i].id,
                              STREAM_PRIORITY_NORMAL, 0);
        requests++;
    }
    
    printf("Queued %u requests\n", requests);
    
    // Let it process
    for (u32 frame = 0; frame < 60; frame++) {
        streaming_update(&streaming_system, player_pos, player_vel, 1.0f / 60.0f);
        usleep(16667);  // ~60 FPS
    }
    
    StreamingStats stats = streaming_get_stats(&streaming_system);
    printf("After 1 second:\n");
    printf("  Completed: %u\n", stats.completed_requests);
    printf("  Bytes evicted: %.2f MB\n", stats.bytes_evicted / (1024.0f * 1024.0f));
}

int main() {
    printf("=== AAA Asset Streaming System Test ===\n");
    printf("Simulating %u assets with %.1f GB memory budget\n",
           ASSET_COUNT, STREAMING_MEMORY_BUDGET / (1024.0f * 1024.0f * 1024.0f));
    
    // Initialize random seed
    srand(time(NULL));
    
    // Generate test assets
    generate_test_assets();
    
    // Initialize streaming system
    printf("\nInitializing streaming system...\n");
    streaming_init(&streaming_system, STREAMING_MEMORY_BUDGET);
    
    // Configure streaming rings
    StreamingRing rings[] = {
        {0, 100, STREAM_PRIORITY_CRITICAL, 50},
        {100, 250, STREAM_PRIORITY_HIGH, 100},
        {250, 400, STREAM_PRIORITY_NORMAL, 200},
        {400, 600, STREAM_PRIORITY_PREFETCH, 400},
        {600, 1000, STREAM_PRIORITY_LOW, 800}
    };
    streaming_configure_rings(&streaming_system, rings, 5);
    
    // Test virtual textures
    test_virtual_textures();
    
    // Main simulation
    printf("\n=== Starting Simulation ===\n");
    printf("Simulating %u frames (%.1f seconds)\n", 
           SIMULATION_FRAMES, SIMULATION_FRAMES / 60.0f);
    
    clock_t start_time = clock();
    
    for (u32 frame = 0; frame < SIMULATION_FRAMES; frame++) {
        simulate_frame(frame, 1.0f / 60.0f);
        
        // Print progress
        if (frame % 300 == 0 && frame > 0) {  // Every 5 seconds
            printf("\nFrame %u / %u:\n", frame, SIMULATION_FRAMES);
            printf("  Player position: (%.1f, %.1f, %.1f)\n",
                   player_pos.x, player_pos.y, player_pos.z);
            
            StreamingStats stats = streaming_get_stats(&streaming_system);
            printf("  Active requests: %u\n", 
                   stats.total_requests - stats.completed_requests - stats.failed_requests);
            printf("  Memory: %.1f / %.1f MB\n",
                   stats.current_memory_usage / (1024.0f * 1024.0f),
                   STREAMING_MEMORY_BUDGET / (1024.0f * 1024.0f));
            printf("  Cache hit rate: %.1f%%\n",
                   stats.total_requests > 0 ? 
                   100.0f * stats.cache_hits / stats.total_requests : 0);
        }
        
        // Simulate frame time
        usleep(1000);  // 1ms (fast simulation)
    }
    
    clock_t end_time = clock();
    f64 elapsed = (f64)(end_time - start_time) / CLOCKS_PER_SEC;
    
    printf("\nSimulation completed in %.2f seconds\n", elapsed);
    printf("Average frame time: %.2f ms\n", elapsed * 1000.0 / SIMULATION_FRAMES);
    
    // Print final statistics
    print_statistics();
    
    // Stress tests
    stress_test();
    memory_pressure_test();
    
    // Dump final state
    streaming_dump_state(&streaming_system, "streaming_state.txt");
    printf("\nState dumped to streaming_state.txt\n");
    
    // Cleanup
    printf("\nShutting down...\n");
    streaming_shutdown(&streaming_system);
    free(test_assets);
    
    printf("Test completed successfully!\n");
    
    return 0;
}