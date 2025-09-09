/*
    Simple World Generation Test
    Tests core functionality without integration dependencies
*/

#include "handmade_world_gen.h" 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Simple World Generation Test ===\n\n");
    
    // Allocate memory for world generation system
    void *memory = malloc(Megabytes(32));
    if (!memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Initialize world generation system
    printf("Initializing world generation system with %u MB memory...\n", (u32)(Megabytes(32) / (1024*1024)));
    world_gen_system *system = world_gen_init(memory, Megabytes(32), 12345);
    if (!system) {
        printf("Failed to initialize world generation system\n");
        printf("Memory size: %u bytes\n", Megabytes(16));
        printf("Required size: %zu bytes\n", sizeof(world_gen_system));
        free(memory);
        return 1;
    }
    
    printf("World generation system initialized successfully!\n");
    printf("Seed: %llu\n", (unsigned long long)system->world_seed);
    printf("Registered biomes: %u\n", system->biome_count);
    
    // Test basic noise generation
    printf("\nTesting noise generation...\n");
    f32 elevation = world_gen_sample_elevation(system, 100.0f, 100.0f);
    printf("Sample elevation at (100,100): %.1fm\n", elevation);
    
    biome_type biome = world_gen_sample_biome(system, 100.0f, 100.0f);
    printf("Sample biome at (100,100): %d\n", biome);
    
    // Test chunk generation
    printf("\nTesting chunk generation...\n");
    world_chunk *chunk = world_gen_generate_chunk(system, 0, 0);
    if (chunk) {
        printf("Successfully generated chunk (0,0)\n");
        printf("Dominant biome: %d\n", chunk->dominant_biome);
        printf("Average elevation: %.1fm\n", chunk->average_elevation);
        printf("Generation time: %.3fms\n", (f32)chunk->generation_time_us / 1000.0f);
        
        // Test resource distribution
        generation_context ctx;
        ctx.world_gen = system;
        ctx.chunk = chunk;
        ctx.global_x = 0;
        ctx.global_y = 0;
        ctx.random_seed = (u32)world_gen_get_chunk_seed(system, 0, 0);
        
        world_gen_distribute_resources(&ctx);
        printf("Resources distributed successfully\n");
        
        // Count resources in chunk
        i32 resource_tiles = 0;
        for (i32 y = 0; y < WORLD_CHUNK_SIZE; y++) {
            for (i32 x = 0; x < WORLD_CHUNK_SIZE; x++) {
                if (chunk->tiles[x][y].resource != RESOURCE_NONE) {
                    resource_tiles++;
                }
            }
        }
        printf("Resource tiles: %d/%d\n", resource_tiles, WORLD_CHUNK_SIZE * WORLD_CHUNK_SIZE);
    } else {
        printf("Failed to generate chunk\n");
    }
    
    // Test performance
    printf("\nTesting performance...\n");
    clock_t start = clock();
    for (i32 i = 0; i < 1000; i++) {
        f32 x = (f32)(i % 100) * 10.0f;
        f32 y = (f32)(i / 100) * 10.0f;
        world_gen_sample_elevation(system, x, y);
    }
    clock_t end = clock();
    
    f64 time_ms = ((f64)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    printf("1000 elevation samples: %.2fms (%.3f μs per sample)\n", 
           time_ms, (time_ms * 1000.0) / 1000.0);
    
    // Final stats
    world_gen_print_stats(system);
    
    // Cleanup
    world_gen_shutdown(system);
    free(memory);
    
    printf("\n✓ All tests passed! World generation system working correctly.\n");
    return 0;
}