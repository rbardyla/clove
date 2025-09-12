/*
    Test program for geological physics simulation
    Validates tectonic plate movement and mountain formation
*/

#include "handmade_geological.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Simple arena
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} test_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    test_arena* a = (test_arena*)arena_ptr;
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

// ASCII visualization of heightmap
void visualize_heightmap(f32* heightmap, u32 width, u32 height) {
    const char* chars = " .-:=+*#%@";
    
    // Find min/max for normalization
    f32 min_h = 1e9f, max_h = -1e9f;
    for (u32 i = 0; i < width * height; i++) {
        if (heightmap[i] < min_h) min_h = heightmap[i];
        if (heightmap[i] > max_h) max_h = heightmap[i];
    }
    
    printf("\nHeightmap (min: %.1f, max: %.1f meters):\n", min_h, max_h);
    
    // Downsample for display
    const u32 DISPLAY_WIDTH = 80;
    const u32 DISPLAY_HEIGHT = 40;
    u32 step_x = width / DISPLAY_WIDTH;
    u32 step_y = height / DISPLAY_HEIGHT;
    
    for (u32 y = 0; y < DISPLAY_HEIGHT; y++) {
        for (u32 x = 0; x < DISPLAY_WIDTH; x++) {
            u32 sample_x = x * step_x;
            u32 sample_y = y * step_y;
            f32 h = heightmap[sample_y * width + sample_x];
            
            // Normalize to 0-9
            f32 normalized = (h - min_h) / (max_h - min_h + 0.001f);
            s32 level = (s32)(normalized * 9);
            if (level < 0) level = 0;
            if (level > 9) level = 9;
            
            printf("%c", chars[level]);
        }
        printf("\n");
    }
}

// Analyze plate statistics
void analyze_plates(geological_state* geo) {
    printf("\n=== Tectonic Plate Analysis ===\n");
    
    for (u32 i = 0; i < geo->plate_count; i++) {
        tectonic_plate* plate = &geo->plates[i];
        
        // Find elevation range
        f32 min_elev = 1e9f, max_elev = -1e9f;
        f32 avg_stress = 0;
        f32 max_thickness = 0;
        
        for (u32 v = 0; v < plate->vertex_count; v++) {
            f32 elev = plate->vertices[v].elevation;
            if (elev < min_elev) min_elev = elev;
            if (elev > max_elev) max_elev = elev;
            
            avg_stress += plate->vertices[v].stress_xx;
            
            if (plate->vertices[v].thickness > max_thickness) {
                max_thickness = plate->vertices[v].thickness;
            }
        }
        avg_stress /= plate->vertex_count;
        
        printf("Plate %u (%s):\n", i, 
               plate->type == PLATE_CONTINENTAL ? "Continental" : "Oceanic");
        printf("  Elevation: %.1f to %.1f m (avg: %.1f m)\n", 
               min_elev, max_elev, plate->average_elevation);
        printf("  Max thickness: %.1f km\n", max_thickness);
        printf("  Avg stress: %.1f Pa\n", avg_stress);
        printf("  Angular velocity: %.6f rad/My\n", plate->angular_velocity);
        printf("  Center: (%.1f, %.1f, %.1f) km\n",
               plate->center_of_mass.x, plate->center_of_mass.y, plate->center_of_mass.z);
    }
}

// Test collision detection
void test_collisions(geological_state* geo) {
    printf("\n=== Collision Test ===\n");
    
    // Move plates closer to force collision
    geo->plates[1].center_of_mass.x = 1000;
    geo->plates[2].center_of_mass.x = 1500;
    
    printf("Forcing plates 1 and 2 to collide...\n");
    
    // Run simulation for 10 million years
    for (u32 i = 0; i < 10; i++) {
        geological_simulate(geo, 1.0);
        printf("  Time: %.1f My - Plate 1 max elevation: %.1f m\n",
               geo->geological_time, geo->plates[1].average_elevation);
    }
    
    // Check for mountain formation
    f32 max_elevation = 0;
    for (u32 v = 0; v < geo->plates[1].vertex_count; v++) {
        if (geo->plates[1].vertices[v].elevation > max_elevation) {
            max_elevation = geo->plates[1].vertices[v].elevation;
        }
    }
    
    printf("Maximum elevation after collision: %.1f m\n", max_elevation);
    if (max_elevation > 5000) {
        printf("SUCCESS: Mountain range formed!\n");
    } else {
        printf("WARNING: Expected higher mountains from collision\n");
    }
}

// Benchmark performance
void benchmark_simulation(geological_state* geo) {
    printf("\n=== Performance Benchmark ===\n");
    
    const u32 ITERATIONS = 100;
    const f64 DT = 0.1;  // 100,000 years per step
    
    clock_t start = clock();
    
    for (u32 i = 0; i < ITERATIONS; i++) {
        geological_simulate(geo, DT);
    }
    
    f64 elapsed = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    f64 per_step = elapsed / ITERATIONS;
    f64 million_years_per_second = (ITERATIONS * DT) / (elapsed / 1000.0);
    
    printf("Simulated %.1f million years in %.2f ms\n", ITERATIONS * DT, elapsed);
    printf("Performance: %.2f ms per step\n", per_step);
    printf("Speed: %.1f million years per second\n", million_years_per_second);
    
    // Check memory usage
    test_arena* a = (test_arena*)geo;
    printf("Memory used: %.2f MB\n", (f64)a->used / (1024.0 * 1024.0));
}

// Long-term evolution test
void test_long_term_evolution(geological_state* geo, arena* arena) {
    printf("\n=== Long-term Evolution Test ===\n");
    printf("Simulating 200 million years of plate tectonics...\n");
    
    f32* heightmap = (f32*)arena_push_size(arena, 512 * 256 * sizeof(f32), 16);
    
    // Initial state
    geological_export_heightmap(geo, heightmap, 512, 256, arena);
    printf("\nInitial Earth:\n");
    visualize_heightmap(heightmap, 512, 256);
    
    // Simulate 50 million years
    printf("\nSimulating...\n");
    for (u32 i = 0; i < 50; i++) {
        geological_simulate(geo, 1.0);  // 1 million years per step
        if (i % 10 == 0) {
            printf("  %.0f million years elapsed\n", geo->geological_time);
        }
    }
    
    // After 50 My
    geological_export_heightmap(geo, heightmap, 512, 256, arena);
    printf("\nAfter 50 million years:\n");
    visualize_heightmap(heightmap, 512, 256);
    analyze_plates(geo);
    
    // Continue to 200 My
    for (u32 i = 0; i < 150; i++) {
        geological_simulate(geo, 1.0);
    }
    
    // Final state
    geological_export_heightmap(geo, heightmap, 512, 256, arena);
    printf("\nAfter 200 million years:\n");
    visualize_heightmap(heightmap, 512, 256);
    analyze_plates(geo);
    
    // No need to free - using arena allocator
}

int main() {
    printf("=== Geological Physics Simulation Test ===\n");
    printf("Simulating tectonic plates and mountain formation\n\n");
    
    // Create test arena - stack allocated for handmade philosophy
    #define ARENA_SIZE (256 * 1024 * 1024)
    static u8 arena_memory[ARENA_SIZE] __attribute__((aligned(32)));
    
    test_arena arena = {0};
    arena.size = ARENA_SIZE;
    arena.base = arena_memory;
    arena.used = 0;
    
    // Initialize geological simulation
    geological_state* geo = geological_init((struct arena*)&arena, 42);
    
    // Run tests
    analyze_plates(geo);
    test_collisions(geo);
    benchmark_simulation(geo);
    test_long_term_evolution(geo, (struct arena*)&arena);
    
    printf("\n=== Test Complete ===\n");
    printf("Peak memory usage: %.2f MB\n", (f64)arena.used / (1024.0 * 1024.0));
    
    // No need to free - stack allocated
    return 0;
}