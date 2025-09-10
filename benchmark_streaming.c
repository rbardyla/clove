/*
    Streaming System Performance Benchmark
*/

#include "handmade_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static void benchmark_memory_pool() {
    printf("\n=== Memory Pool Benchmark ===\n");
    
    StreamingMemoryPool pool;
    pool.base = (u8*)malloc(GIGABYTES(1));
    pool.size = GIGABYTES(1);
    pool.used = 0;
    pool.free_list = NULL;
    
    clock_t start = clock();
    
    // Allocate and free many blocks
    void* blocks[10000];
    usize sizes[10000];
    
    for (u32 i = 0; i < 10000; i++) {
        sizes[i] = 1024 + (rand() % (1024 * 1024));
        blocks[i] = pool_alloc(&pool, sizes[i]);
    }
    
    // Free half randomly
    for (u32 i = 0; i < 5000; i++) {
        u32 idx = rand() % 10000;
        if (blocks[idx]) {
            pool_free(&pool, blocks[idx], sizes[idx]);
            blocks[idx] = NULL;
        }
    }
    
    // Allocate more
    for (u32 i = 0; i < 10000; i++) {
        if (!blocks[i]) {
            blocks[i] = pool_alloc(&pool, sizes[i]);
        }
    }
    
    clock_t end = clock();
    f64 elapsed = (f64)(end - start) / CLOCKS_PER_SEC;
    
    printf("20000 allocations in %.3f seconds\n", elapsed);
    printf("%.0f allocations/second\n", 20000.0 / elapsed);
    printf("Pool usage: %.1f MB / %.1f MB\n",
           pool.used / (1024.0f * 1024.0f),
           pool.size / (1024.0f * 1024.0f));
    
    free(pool.base);
}

static void benchmark_compression() {
    printf("\n=== Compression Benchmark ===\n");
    
    usize test_size = MEGABYTES(10);
    u8* src = (u8*)malloc(test_size);
    u8* dst = (u8*)malloc(test_size * 2);
    
    // Generate test data with varying entropy
    for (usize i = 0; i < test_size; i++) {
        if (i % 100 < 50) {
            src[i] = 0;  // Runs of zeros
        } else {
            src[i] = rand() & 0xFF;  // Random data
        }
    }
    
    clock_t start = clock();
    usize compressed = streaming_compress(src, test_size, dst, test_size * 2, COMPRESS_LZ4);
    clock_t end = clock();
    
    f64 compress_time = (f64)(end - start) / CLOCKS_PER_SEC;
    f32 ratio = (f32)compressed / test_size;
    
    printf("Compressed %.1f MB -> %.1f MB (ratio: %.1f%%)\n",
           test_size / (1024.0f * 1024.0f),
           compressed / (1024.0f * 1024.0f),
           ratio * 100.0f);
    printf("Compression speed: %.1f MB/s\n",
           (test_size / (1024.0f * 1024.0f)) / compress_time);
    
    // Decompress
    u8* decompressed = (u8*)malloc(test_size);
    start = clock();
    streaming_decompress(dst, compressed, decompressed, test_size, COMPRESS_LZ4);
    end = clock();
    
    f64 decompress_time = (f64)(end - start) / CLOCKS_PER_SEC;
    printf("Decompression speed: %.1f MB/s\n",
           (test_size / (1024.0f * 1024.0f)) / decompress_time);
    
    free(src);
    free(dst);
    free(decompressed);
}

static void benchmark_virtual_textures() {
    printf("\n=== Virtual Texture Benchmark ===\n");
    
    StreamingSystem system;
    streaming_init(&system, GIGABYTES(1));
    
    // Create large virtual texture
    VirtualTexture* vt = streaming_create_virtual_texture(&system, 32768, 32768, 4);
    
    printf("Virtual texture: %ux%u (%u pages)\n",
           vt->width, vt->height, vt->page_count_x * vt->page_count_y);
    
    clock_t start = clock();
    
    // Request many pages
    for (u32 i = 0; i < 1000; i++) {
        u32 x = rand() % vt->page_count_x;
        u32 y = rand() % vt->page_count_y;
        streaming_request_vt_page(&system, vt, x, y, 0);
    }
    
    // Update indirection texture
    streaming_update_vt_indirection(&system, vt);
    
    clock_t end = clock();
    f64 elapsed = (f64)(end - start) / CLOCKS_PER_SEC;
    
    printf("1000 page requests in %.3f seconds\n", elapsed);
    printf("%.0f requests/second\n", 1000.0 / elapsed);
    
    streaming_shutdown(&system);
}

int main() {
    printf("=== AAA Streaming System Benchmarks ===\n");
    
    benchmark_memory_pool();
    benchmark_compression();
    benchmark_virtual_textures();
    
    printf("\nBenchmarks complete!\n");
    return 0;
}
