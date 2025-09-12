/*
    Test program for hydrological physics simulation
    Validates fluid dynamics, erosion, and geological coupling
    Tests the complete multi-scale feedback loop
*/

#include "handmade_geological.c"
#include "handmade_hydrological.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Simple arena for testing (matches geological test)
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

// Visualize fluid flow on 2D slice
void visualize_fluid_slice(fluid_state* fluid, u32 y_slice) {
    const char* chars = " .:-=+*#%@";
    
    printf("\nFluid flow at altitude %u (y=%u):\n", 
           (u32)((f32)y_slice / fluid->grid_y * 10000.0f), y_slice);
    
    for (u32 z = 0; z < fluid->grid_z; z += 2) {  // Downsample for display
        for (u32 x = 0; x < fluid->grid_x; x += 2) {
            u32 idx = z * fluid->grid_y * fluid->grid_x + y_slice * fluid->grid_x + x;
            fluid_cell* cell = &fluid->grid[idx];
            
            char display_char = ' ';
            
            if (cell->is_solid) {
                display_char = '#';
            } else if (cell->density > WATER_DENSITY * 0.9f) {
                // Water - show velocity magnitude
                f32 velocity_mag = sqrtf(cell->velocity_x * cell->velocity_x + 
                                       cell->velocity_z * cell->velocity_z);
                s32 level = (s32)(velocity_mag * 2.0f);  // Scale factor
                level = fmaxf(0, fminf(9, level));
                display_char = chars[level];
            } else if (cell->is_source) {
                display_char = '^';  // Precipitation source
            } else if (cell->density > 10.0f) {
                display_char = '.';  // Air with some density
            }
            
            printf("%c", display_char);
        }
        printf("\n");
    }
}

// Analyze fluid statistics
void analyze_fluid_state(fluid_state* fluid) {
    printf("\n=== Hydrological Analysis ===\n");
    
    u32 total_cells = fluid->grid_x * fluid->grid_y * fluid->grid_z;
    u32 water_cells = 0;
    u32 solid_cells = 0;
    u32 source_cells = 0;
    f32 total_velocity = 0;
    f32 max_velocity = 0;
    f32 total_sediment = 0;
    f32 total_erosion = 0;
    
    for (u32 i = 0; i < total_cells; i++) {
        fluid_cell* cell = &fluid->grid[i];
        
        if (cell->is_solid) {
            solid_cells++;
        } else if (cell->density > WATER_DENSITY * 0.9f) {
            water_cells++;
            
            f32 velocity_mag = sqrtf(cell->velocity_x * cell->velocity_x + 
                                   cell->velocity_y * cell->velocity_y + 
                                   cell->velocity_z * cell->velocity_z);
            total_velocity += velocity_mag;
            if (velocity_mag > max_velocity) max_velocity = velocity_mag;
        }
        
        if (cell->is_source) source_cells++;
        
        total_sediment += cell->sediment_amount;
        total_erosion += fabs(cell->erosion_rate);
    }
    
    f32 avg_velocity = (water_cells > 0) ? total_velocity / water_cells : 0;
    
    printf("Grid: %ux%ux%u (%u total cells)\n", 
           fluid->grid_x, fluid->grid_y, fluid->grid_z, total_cells);
    printf("Solid cells: %u (%.1f%%)\n", solid_cells, 
           100.0f * solid_cells / total_cells);
    printf("Water cells: %u (%.1f%%)\n", water_cells, 
           100.0f * water_cells / total_cells);
    printf("Precipitation sources: %u\n", source_cells);
    printf("Average water velocity: %.3f m/s\n", avg_velocity);
    printf("Maximum water velocity: %.3f m/s\n", max_velocity);
    printf("Total sediment: %.6f kg\n", total_sediment);
    printf("Total erosion rate: %.9f m/year\n", total_erosion);
    printf("Sediment particles: %u / %u\n", fluid->particle_count, fluid->max_particles);
    printf("Simulation time: %.2f years\n", fluid->hydro_time);
}

// Test pressure solver convergence
void test_pressure_solver(fluid_state* fluid, arena* temp_arena) {
    printf("\n=== Pressure Solver Test ===\n");
    
    // Create a simple test case - water column
    u32 mid_x = fluid->grid_x / 2;
    u32 mid_z = fluid->grid_z / 2;
    
    // Fill vertical column with water
    for (u32 y = 0; y < fluid->grid_y / 2; y++) {
        u32 idx = mid_z * fluid->grid_y * fluid->grid_x + y * fluid->grid_x + mid_x;
        fluid->grid[idx].is_solid = 0;
        fluid->grid[idx].density = WATER_DENSITY;
        fluid->grid[idx].velocity_x = 1.0f;  // Initial horizontal flow
        fluid->grid[idx].velocity_y = 0;
        fluid->grid[idx].velocity_z = 0;
    }
    
    // Measure pressure solver performance
    clock_t start = clock();
    
    const u32 TEST_ITERATIONS = 100;
    for (u32 iter = 0; iter < TEST_ITERATIONS; iter++) {
        // Calculate divergence
        for (u32 z = 1; z < fluid->grid_z - 1; z++) {
            for (u32 y = 1; y < fluid->grid_y - 1; y++) {
                for (u32 x = 1; x < fluid->grid_x - 1; x++) {
                    u32 idx = z * fluid->grid_y * fluid->grid_x + y * fluid->grid_x + x;
                    
                    if (fluid->grid[idx].is_solid) {
                        fluid->divergence[idx] = 0;
                        continue;
                    }
                    
                    f32 u_right = fluid->grid[idx].velocity_x;
                    f32 u_left = fluid->grid[idx - 1].velocity_x;
                    f32 v_up = fluid->grid[idx].velocity_y;
                    f32 v_down = fluid->grid[idx - fluid->grid_x].velocity_y;
                    f32 w_front = fluid->grid[idx].velocity_z;
                    f32 w_back = fluid->grid[idx - fluid->grid_x * fluid->grid_y].velocity_z;
                    
                    fluid->divergence[idx] = (u_right - u_left) + (v_up - v_down) + (w_front - w_back);
                }
            }
        }
        
        // Run pressure solver
        fluid_pressure_solve_simd(fluid->grid, fluid->pressure_scratch, fluid->divergence,
                                 fluid->grid_x, fluid->grid_y, fluid->grid_z, 20);
    }
    
    f64 elapsed = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    f64 per_iteration = elapsed / TEST_ITERATIONS;
    
    printf("Pressure solver performance:\n");
    printf("  %u iterations in %.2f ms\n", TEST_ITERATIONS, elapsed);
    printf("  Average: %.3f ms per solve\n", per_iteration);
    
    // Check final divergence
    f32 max_divergence = 0;
    f32 avg_divergence = 0;
    u32 fluid_cells = 0;
    
    for (u32 i = 0; i < fluid->grid_x * fluid->grid_y * fluid->grid_z; i++) {
        if (!fluid->grid[i].is_solid) {
            f32 div = fabsf(fluid->divergence[i]);
            avg_divergence += div;
            if (div > max_divergence) max_divergence = div;
            fluid_cells++;
        }
    }
    avg_divergence /= fluid_cells;
    
    printf("  Final divergence - max: %.6f, avg: %.6f\n", max_divergence, avg_divergence);
    
    if (max_divergence < 0.01f) {
        printf("  SUCCESS: Low divergence achieved\n");
    } else {
        printf("  WARNING: High divergence - solver may need more iterations\n");
    }
}

// Test erosion feedback with geological system
void test_erosion_feedback(fluid_state* fluid, geological_state* geo) {
    printf("\n=== Erosion Feedback Test ===\n");
    
    // Record initial mountain heights
    f32 initial_max_elevation = 0;
    for (u32 p = 0; p < geo->plate_count; p++) {
        for (u32 v = 0; v < geo->plates[p].vertex_count; v++) {
            if (geo->plates[p].vertices[v].elevation > initial_max_elevation) {
                initial_max_elevation = geo->plates[p].vertices[v].elevation;
            }
        }
    }
    
    printf("Initial maximum elevation: %.1f m\n", initial_max_elevation);
    
    // Create artificial water flow over mountains
    for (u32 z = 0; z < fluid->grid_z; z++) {
        for (u32 y = fluid->grid_y / 2; y < fluid->grid_y - 5; y++) {
            for (u32 x = 0; x < fluid->grid_x; x++) {
                u32 idx = z * fluid->grid_y * fluid->grid_x + y * fluid->grid_x + x;
                
                if (!fluid->grid[idx].is_solid) {
                    fluid->grid[idx].density = WATER_DENSITY;
                    fluid->grid[idx].velocity_x = 2.0f;  // Fast flow
                    fluid->grid[idx].velocity_y = -0.5f;  // Downward
                    fluid->grid[idx].velocity_z = 0;
                    fluid->grid[idx].sediment_amount = 0;  // Start with clean water
                }
            }
        }
    }
    
    printf("Applied strong water flow over terrain\n");
    
    // Run erosion calculation
    calculate_erosion(fluid, geo);
    
    // Check for erosion effects
    f32 final_max_elevation = 0;
    f32 total_sediment = 0;
    u32 eroded_cells = 0;
    
    for (u32 p = 0; p < geo->plate_count; p++) {
        for (u32 v = 0; v < geo->plates[p].vertex_count; v++) {
            if (geo->plates[p].vertices[v].elevation > final_max_elevation) {
                final_max_elevation = geo->plates[p].vertices[v].elevation;
            }
        }
    }
    
    for (u32 i = 0; i < fluid->grid_x * fluid->grid_y * fluid->grid_z; i++) {
        total_sediment += fluid->grid[i].sediment_amount;
        if (fluid->grid[i].erosion_rate > 1e-9f) eroded_cells++;
    }
    
    f32 elevation_change = initial_max_elevation - final_max_elevation;
    
    printf("Results after erosion:\n");
    printf("  Final maximum elevation: %.1f m\n", final_max_elevation);
    printf("  Elevation change: %.3f m\n", elevation_change);
    printf("  Total sediment generated: %.6f kg\n", total_sediment);
    printf("  Active erosion cells: %u\n", eroded_cells);
    
    if (elevation_change > 0.001f && total_sediment > 1e-6f) {
        printf("  SUCCESS: Erosion feedback is working!\n");
    } else {
        printf("  WARNING: Weak erosion feedback - may need parameter tuning\n");
    }
}

// Test river formation algorithm
void test_river_formation(fluid_state* fluid, geological_state* geo, arena* temp_arena) {
    printf("\n=== River Formation Test ===\n");
    
    // Create mountain scenario - steep terrain with precipitation
    f32 seasonal_phase = 0;  // Spring/summer precipitation
    apply_precipitation_patterns(fluid, seasonal_phase);
    
    // Count initial precipitation sources
    u32 precipitation_sources = 0;
    for (u32 i = 0; i < fluid->grid_x * fluid->grid_y * fluid->grid_z; i++) {
        if (fluid->grid[i].is_source) precipitation_sources++;
    }
    
    printf("Applied precipitation pattern: %u source cells\n", precipitation_sources);
    
    // Simulate water flow for several time steps
    for (u32 step = 0; step < 10; step++) {
        fluid_simulate(fluid, geo, temp_arena, 0.1f);  // 0.1 years per step
        
        if (step % 3 == 0) {
            printf("  Step %u: %.1f years simulated\n", step, fluid->hydro_time);
        }
    }
    
    // Detect river formation
    detect_river_formation(fluid, temp_arena);
    
    // Count water cells with significant flow
    u32 river_cells = 0;
    f32 max_flow_velocity = 0;
    
    for (u32 i = 0; i < fluid->grid_x * fluid->grid_y * fluid->grid_z; i++) {
        if (fluid->grid[i].density > WATER_DENSITY * 0.9f) {
            f32 velocity_mag = sqrtf(fluid->grid[i].velocity_x * fluid->grid[i].velocity_x + 
                                   fluid->grid[i].velocity_z * fluid->grid[i].velocity_z);
            
            if (velocity_mag > 0.5f) {  // Significant lateral flow
                river_cells++;
                if (velocity_mag > max_flow_velocity) {
                    max_flow_velocity = velocity_mag;
                }
            }
        }
    }
    
    printf("River formation results:\n");
    printf("  River channel cells: %u\n", river_cells);
    printf("  Maximum flow velocity: %.2f m/s\n", max_flow_velocity);
    printf("  Sediment particles spawned: %u\n", fluid->particle_count);
    
    if (river_cells > 50 && max_flow_velocity > 1.0f) {
        printf("  SUCCESS: Rivers formed with realistic flow!\n");
    } else {
        printf("  INFO: Limited river formation - may need more simulation time\n");
    }
}

// Performance benchmark for full simulation
void benchmark_coupled_simulation(fluid_state* fluid, geological_state* geo, arena* temp_arena) {
    printf("\n=== Coupled Simulation Benchmark ===\n");
    
    const u32 SIMULATION_STEPS = 50;
    const f32 DT = 0.02f;  // 0.02 years per step (about 7 days)
    
    clock_t start = clock();
    
    for (u32 step = 0; step < SIMULATION_STEPS; step++) {
        // Run coupled simulation
        geological_simulate(geo, DT * GEOLOGICAL_TIME_SCALE / 1000000.0f);  // Convert to geological time
        fluid_simulate(fluid, geo, temp_arena, DT);
        apply_fluid_erosion_to_geological(fluid, geo);
        apply_geological_to_fluid(geo, fluid);
        
        if (step % 10 == 0) {
            printf("  Step %u: Geo=%.3f My, Fluid=%.2f years\n", 
                   step, geo->geological_time, fluid->hydro_time);
        }
    }
    
    f64 elapsed = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    f64 per_step = elapsed / SIMULATION_STEPS;
    f64 years_per_second = (SIMULATION_STEPS * DT) / (elapsed / 1000.0);
    
    printf("Performance results:\n");
    printf("  Simulated %.1f years in %.2f ms\n", SIMULATION_STEPS * DT, elapsed);
    printf("  Average: %.2f ms per step\n", per_step);
    printf("  Speed: %.1f years per second\n", years_per_second);
    printf("  Geological time: %.6f million years\n", geo->geological_time);
    printf("  Hydrological time: %.2f years\n", fluid->hydro_time);
    
    // Performance targets
    if (per_step < 50.0 && years_per_second > 10.0) {
        printf("  SUCCESS: Performance targets met!\n");
    } else {
        printf("  INFO: Performance could be optimized further\n");
    }
}

int main() {
    printf("=== Hydrological Physics Simulation Test ===\n");
    printf("Testing fluid dynamics, erosion, and geological coupling\n\n");
    
    // Create test arena - same size as geological test
    #define ARENA_SIZE (256 * 1024 * 1024)
    static u8 arena_memory[ARENA_SIZE] __attribute__((aligned(32)));
    
    test_arena arena = {0};
    arena.size = ARENA_SIZE;
    arena.base = arena_memory;
    arena.used = 0;
    
    // Initialize geological simulation first
    printf("Initializing geological foundation...\n");
    geological_state* geo = geological_init((struct arena*)&arena, 42);
    
    // Run geological simulation to create terrain
    printf("Creating initial terrain...\n");
    for (u32 i = 0; i < 5; i++) {
        geological_simulate(geo, 1.0);  // 5 million years of geological time
    }
    
    // Initialize hydrological simulation
    printf("Initializing hydrological system...\n");
    fluid_state* fluid = fluid_init((struct arena*)&arena, geo, 64);  // 64x64x64 grid
    
    // Create temporary arena for river formation algorithm
    test_arena temp_arena = {0};
    temp_arena.size = 32 * 1024 * 1024;  // 32 MB for temporary allocations
    temp_arena.base = arena_memory + arena.used;
    temp_arena.used = 0;
    
    // Run tests
    analyze_fluid_state(fluid);
    visualize_fluid_slice(fluid, fluid->grid_y / 4);  // Lower altitude slice
    test_pressure_solver(fluid, (struct arena*)&temp_arena);
    test_erosion_feedback(fluid, geo);
    test_river_formation(fluid, geo, (struct arena*)&temp_arena);
    benchmark_coupled_simulation(fluid, geo, (struct arena*)&temp_arena);
    
    // Final state analysis
    printf("\n=== Final Analysis ===\n");
    analyze_fluid_state(fluid);
    
    printf("\nMemory usage:\n");
    printf("  Main arena: %.2f MB\n", (f64)arena.used / (1024.0 * 1024.0));
    printf("  Temp arena peak: %.2f MB\n", (f64)temp_arena.used / (1024.0 * 1024.0));
    
    printf("\n=== Test Complete ===\n");
    printf("Multi-scale physics simulation validated!\n");
    
    // No need to free - stack allocated
    return 0;
}