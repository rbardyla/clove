#include "handmade_steam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Memory size macros
#ifndef Megabytes
#define Kilobytes(n) ((n) * 1024)
#define Megabytes(n) (Kilobytes(n) * 1024)
#endif

int main() {
    printf("=== Steam Integration Performance Test ===\n\n");
    
    void *memory = malloc(Megabytes(8)); // 8MB
    if (!memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    steam_system *system = steam_init(memory, Megabytes(8), 480);
    
    if (!system) {
        printf("Failed to initialize Steam system\n");
        free(memory);
        return 1;
    }
    
    printf("Steam System Initialization:\n");
    printf("  Initialized: %s\n", system->initialized ? "Yes" : "No");
    printf("  App ID: %u\n", system->app_id);
    printf("  Memory allocated: 8192 KB\n");
    
    clock_t start = clock();
    
    // Test achievement operations
    start = clock();
    for (int i = 0; i < 50000; i++) {
        char ach_name[64];
        snprintf(ach_name, 63, "test_achievement_%d", i % 1000);
        steam_unlock_achievement(system, ach_name);
    }
    double ach_time = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;
    
    printf("\nAchievement Operations (50,000 ops):\n");
    printf("  Time: %.2f ms\n", ach_time);
    printf("  Rate: %.1f ops/ms\n", 50000.0 / ach_time);
    printf("  Per operation: %.3f μs\n", ach_time * 1000.0 / 50000.0);
    
    // Test stat operations
    start = clock();
    for (int i = 0; i < 100000; i++) {
        steam_set_stat_int(system, "test_int_stat", i);
        steam_set_stat_float(system, "test_float_stat", (float)i * 0.1f);
    }
    double stat_time = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;
    
    printf("\nStatistic Updates (200,000 ops):\n");
    printf("  Time: %.2f ms\n", stat_time);
    printf("  Rate: %.1f ops/ms\n", 200000.0 / stat_time);
    printf("  Per update: %.3f μs\n", stat_time * 1000.0 / 200000.0);
    
    // Test cloud file operations
    char test_data[1024];
    memset(test_data, 0, sizeof(test_data));
    
    start = clock();
    for (int i = 0; i < 1000; i++) {
        char filename[64];
        snprintf(filename, 63, "test_file_%d.dat", i % 100);
        steam_cloud_write_file(system, filename, test_data, sizeof(test_data));
    }
    double cloud_time = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;
    
    printf("\nCloud File Operations (1,000 ops):\n");
    printf("  Time: %.2f ms\n", cloud_time);
    printf("  Rate: %.1f ops/ms\n", 1000.0 / cloud_time);
    printf("  Per operation: %.3f ms\n", cloud_time / 1000.0);
    
    // Test update loop performance
    start = clock();
    for (int i = 0; i < 10000; i++) {
        steam_update(system, 0.016667f); // 60fps
    }
    double update_time = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000.0;
    
    printf("\nUpdate Loop (10,000 updates @ 60fps):\n");
    printf("  Time: %.2f ms\n", update_time);
    printf("  Rate: %.1f updates/ms\n", 10000.0 / update_time);
    printf("  Can sustain: %.1f FPS\n", 1000.0 / (update_time / 10000.0));
    
    printf("\nMemory Usage:\n");
    printf("  System size: %lu bytes\n", sizeof(steam_system));
    printf("  Achievement tracking: %u items\n", system->achievement_count);
    printf("  Statistics tracking: %u items\n", system->stat_count);
    printf("  Cloud files: %u items\n", system->cloud_file_count);
    printf("  Memory efficiency: %.1f items/KB\n", 
           (float)(system->achievement_count + system->stat_count) / 8192.0f);
    
    steam_shutdown(system);
    free(memory);
    
    printf("\nAll performance tests completed successfully!\n");
    return 0;
}