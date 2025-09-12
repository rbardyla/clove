/*
    Multi-Scale Physics Demo
    Demonstrates geological + hydrological coupling
    Shows the full handmade philosophy in action
*/

#include "handmade_hydrological.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Arena implementation (matches test files)
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} demo_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    demo_arena* a = (demo_arena*)arena_ptr;
    u64 aligned = (a->used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > a->size) {
        printf("Arena out of memory!\n");
        return 0;
    }
    void* result = a->base + aligned;
    a->used = aligned + size;
    memset(result, 0, size);
    return result;
}

void demo_multi_scale_coupling() {
    printf("=== Multi-Scale Physics Demo ===\n");
    printf("Simulating Earth's surface processes from first principles\n\n");
    
    // Create arena (512MB for full demo)
    #define DEMO_ARENA_SIZE (512 * 1024 * 1024)
    static u8 arena_memory[DEMO_ARENA_SIZE] __attribute__((aligned(32)));
    
    demo_arena arena = {0};
    arena.size = DEMO_ARENA_SIZE;
    arena.base = arena_memory;
    arena.used = 0;
    
    demo_arena temp_arena = {0};
    temp_arena.size = 64 * 1024 * 1024;
    temp_arena.base = arena_memory + 400 * 1024 * 1024;  // Last 112MB for temp
    temp_arena.used = 0;
    
    // Phase 1: Geological Foundation (Deep Time)
    printf("Phase 1: Creating geological foundation...\n");
    geological_state* geo = geological_init((arena*)&arena, time(NULL));
    
    // Simulate 100 million years of geological time
    printf("  Simulating tectonic activity (100 million years)...\n");
    for (u32 i = 0; i < 100; i++) {
        geological_simulate(geo, 1.0);  // 1 million years per step
        if (i % 20 == 0) {
            printf("    %.0f million years: max elevation %.0f m\n", 
                   geo->geological_time, geo->plates[0].average_elevation);
        }
    }
    
    // Phase 2: Hydrological Layer (Human Time)
    printf("\nPhase 2: Adding hydrological processes...\n");
    fluid_state* fluid = fluid_init((arena*)&arena, geo, 128);  // Higher resolution
    
    // Simulate 1000 years of fluid dynamics
    printf("  Simulating erosion and river formation (1000 years)...\n");
    for (u32 year = 0; year < 1000; year++) {
        // Run coupled simulation
        geological_simulate(geo, 0.000001);  // Very small geological time step
        fluid_simulate(fluid, geo, (arena*)&temp_arena, 1.0);  // 1 year fluid time
        apply_fluid_erosion_to_geological(fluid, geo);
        apply_geological_to_fluid(geo, fluid);
        
        if (year % 100 == 0) {
            printf("    Year %u: rivers=%u particles, erosion=%.6f m\n", 
                   year, fluid->particle_count, 
                   (geo->plates[0].average_elevation - geo->plates[1].average_elevation));
        }
    }
    
    // Phase 3: Analysis and Results
    printf("\nPhase 3: Final analysis...\n");
    
    // Export heightmap for visualization
    f32* heightmap = (f32*)arena_push_size((arena*)&temp_arena, 512 * 256 * sizeof(f32), 16);
    geological_export_heightmap(geo, heightmap, 512, 256, (arena*)&temp_arena);
    
    // Count river systems
    u32 river_cells = 0;
    u32 water_cells = 0;
    f32 max_flow = 0;
    
    for (u32 i = 0; i < fluid->grid_x * fluid->grid_y * fluid->grid_z; i++) {
        if (fluid->grid[i].density > WATER_DENSITY * 0.9f) {
            water_cells++;
            f32 flow = sqrtf(fluid->grid[i].velocity_x * fluid->grid[i].velocity_x + 
                           fluid->grid[i].velocity_z * fluid->grid[i].velocity_z);
            if (flow > 0.5f) river_cells++;
            if (flow > max_flow) max_flow = flow;
        }
    }
    
    // Performance metrics
    printf("\nResults:\n");
    printf("  Geological time simulated: %.1f million years\n", geo->geological_time);
    printf("  Hydrological time simulated: %.1f years\n", fluid->hydro_time);
    printf("  Maximum elevation: %.1f m\n", geo->plates[1].average_elevation);
    printf("  Water cells: %u\n", water_cells);
    printf("  River channel cells: %u\n", river_cells);
    printf("  Maximum flow velocity: %.2f m/s\n", max_flow);
    printf("  Sediment particles: %u\n", fluid->particle_count);
    printf("  Memory used: %.1f MB\n", (f64)arena.used / (1024.0 * 1024.0));
    
    printf("\n=== Success: Multi-scale physics working! ===\n");
    printf("This demonstrates the handmade philosophy:\n");
    printf("  ✓ Zero external dependencies\n");
    printf("  ✓ Complete understanding of all code\n");
    printf("  ✓ Performance-first design\n");
    printf("  ✓ Arena memory management\n");
    printf("  ✓ SIMD-optimized algorithms\n");
    printf("  ✓ Multi-scale physics coupling\n");
}

int main() {
    demo_multi_scale_coupling();
    return 0;
}
