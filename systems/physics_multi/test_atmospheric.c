/*
    Test Program for Handmade Atmospheric Physics
    Demonstrates weather simulation, cloud formation, and precipitation
    
    Tests:
    1. Continental-scale weather simulation
    2. Cloud formation and precipitation
    3. Wind patterns and pressure systems
    4. Coupling to hydrological system
*/

#include "handmade_atmospheric.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Mock arena implementation for testing
typedef struct test_arena {
    u8* memory;
    u64 size;
    u64 used;
} test_arena;

void* arena_push_size(arena* arena_void, u64 size, u32 alignment) {
    test_arena* arena = (test_arena*)arena_void;
    
    // Align the current position
    u64 aligned_used = (arena->used + alignment - 1) & ~(alignment - 1);
    
    if (aligned_used + size > arena->size) {
        printf("Arena out of memory! Requested: %lu, Available: %lu\n", 
               (unsigned long)size, (unsigned long)(arena->size - aligned_used));
        return NULL;
    }
    
    void* result = arena->memory + aligned_used;
    arena->used = aligned_used + size;
    return result;
}

arena* create_test_arena(u64 size) {
    test_arena* test_arena_ptr = malloc(sizeof(test_arena));
    test_arena_ptr->memory = malloc(size);
    test_arena_ptr->size = size;
    test_arena_ptr->used = 0;
    return (arena*)test_arena_ptr;
}

void destroy_test_arena(arena* arena_void) {
    test_arena* arena = (test_arena*)arena_void;
    free(arena->memory);
    free(arena);
}

void test_continental_weather_simulation() {
    printf("=== TEST: Continental Weather Simulation ===\n");
    
    // Create large-scale atmospheric simulation
    arena* arena = create_test_arena(MEGABYTES(500));
    
    // Continental domain: 2000km x 2000km x 20km altitude
    atmospheric_system* atm = atmospheric_system_init(arena, 64, 64, 32, 
                                                     2000.0f, 2000.0f, 20.0f);
    
    printf("Continental atmosphere initialized:\n");
    printf("  Domain: %.0fkm x %.0fkm x %.0fkm\n", 
           (atm->domain_max_x - atm->domain_min_x) / 1000.0f,
           (atm->domain_max_y - atm->domain_min_y) / 1000.0f,
           (atm->domain_max_z - atm->domain_min_z) / 1000.0f);
    printf("  Grid: %ux%ux%u cells\n", atm->grid_x, atm->grid_y, atm->grid_z);
    printf("  Cell size: %.1fkm x %.1fkm x %.1fkm\n", 
           atm->cell_size_x / 1000.0f, atm->cell_size_y / 1000.0f, atm->cell_size_z / 1000.0f);
    
    // Create initial pressure disturbances to drive weather
    u32 center_x = atm->grid_x / 2;
    u32 center_y = atm->grid_y / 2;
    
    // High pressure system (anticyclone)
    for (u32 z = 0; z < 8; z++) { // Lower atmosphere only
        for (u32 dy = 0; dy < 8; dy++) {
            for (u32 dx = 0; dx < 8; dx++) {
                u32 x = center_x + dx - 4;
                u32 y = center_y + dy - 4;
                if (x < atm->grid_x && y < atm->grid_y) {
                    u32 idx = z * atm->grid_x * atm->grid_y + y * atm->grid_x + x;
                    atm->cells[idx].pressure += 2000.0f; // +20 hPa high pressure
                }
            }
        }
    }
    
    // Low pressure system (cyclone) - offset location
    u32 low_x = center_x + 16;
    u32 low_y = center_y - 16;
    for (u32 z = 0; z < 12; z++) {
        for (u32 dy = 0; dy < 10; dy++) {
            for (u32 dx = 0; dx < 10; dx++) {
                u32 x = low_x + dx - 5;
                u32 y = low_y + dy - 5;
                if (x < atm->grid_x && y < atm->grid_y) {
                    u32 idx = z * atm->grid_x * atm->grid_y + y * atm->grid_x + x;
                    atm->cells[idx].pressure -= 3000.0f; // -30 hPa low pressure
                    
                    // Add moisture to low pressure system
                    atm->cells[idx].humidity *= 2.0f; // Double humidity
                }
            }
        }
    }
    
    printf("Initial pressure systems created\n");
    
    // Simulate weather evolution for 48 hours
    printf("\nSimulating 48 hours of weather...\n");
    f32 dt = 60.0f; // 1 minute time steps
    u32 steps = 48 * 60; // 48 hours
    
    f32 max_recorded_wind = 0.0f;
    f32 total_rain = 0.0f;
    
    for (u32 step = 0; step < steps; step++) {
        atmospheric_simulate(atm, dt);
        
        if (atm->stats.max_wind_speed > max_recorded_wind) {
            max_recorded_wind = atm->stats.max_wind_speed;
        }
        
        total_rain += atm->stats.total_precipitation;
        
        // Print status every 4 hours
        if (step % (4 * 60) == 0) {
            f32 hours = step / 60.0f;
            printf("  Hour %.0f: Max Wind %.1fm/s, Precip %.1fmm, Total Rain %.1fmm\n",
                   hours, atm->stats.max_wind_speed, 
                   atm->stats.total_precipitation * 1000.0f, // Convert to mm
                   total_rain * 1000.0f);
        }
    }
    
    printf("\n48-hour simulation complete\n");
    atmospheric_debug_draw(atm);
    
    printf("Weather Statistics:\n");
    printf("  Maximum wind speed: %.1f m/s\n", max_recorded_wind);
    printf("  Total precipitation: %.2f mm\n", total_rain * 1000.0f);
    
    if (max_recorded_wind > 10.0f && total_rain > 0.001f) {
        printf("✓ Realistic weather patterns generated\n");
    } else {
        printf("✗ Weather simulation may need adjustment\n");
    }
    
    destroy_test_arena(arena);
    printf("=== TEST COMPLETE ===\n\n");
}

void test_storm_development() {
    printf("=== TEST: Storm Development and Precipitation ===\n");
    
    arena* arena = create_test_arena(MEGABYTES(200));
    
    // Smaller domain for detailed storm simulation: 500km x 500km x 15km
    atmospheric_system* atm = atmospheric_system_init(arena, 50, 50, 20,
                                                     500.0f, 500.0f, 15.0f);
    
    printf("Storm simulation domain: 500km x 500km x 15km\n");
    printf("Grid resolution: %.0fm x %.0fm x %.0fm\n", 
           atm->cell_size_x, atm->cell_size_y, atm->cell_size_z);
    
    // Create strong low pressure center with high humidity
    u32 center_x = atm->grid_x / 2;
    u32 center_y = atm->grid_y / 2;
    
    for (u32 z = 0; z < atm->grid_z; z++) {
        for (u32 y = 0; y < atm->grid_y; y++) {
            for (u32 x = 0; x < atm->grid_x; x++) {
                u32 idx = z * atm->grid_x * atm->grid_y + y * atm->grid_x + x;
                
                // Distance from storm center
                f32 dx = (f32)(x - center_x) * atm->cell_size_x;
                f32 dy = (f32)(y - center_y) * atm->cell_size_y;
                f32 distance = sqrtf(dx*dx + dy*dy);
                
                // Create circular low pressure system
                if (distance < 100000.0f) { // 100km radius
                    f32 pressure_drop = 5000.0f * expf(-distance / 50000.0f); // Gaussian drop
                    atm->cells[idx].pressure -= pressure_drop;
                    
                    // Add moisture
                    f32 humidity_boost = 0.01f * expf(-distance / 30000.0f);
                    atm->cells[idx].humidity += humidity_boost;
                    
                    // Add temperature contrast (cooler at center)
                    f32 temp_drop = 5.0f * expf(-distance / 40000.0f);
                    atm->cells[idx].temperature -= temp_drop;
                }
            }
        }
    }
    
    // Simulate storm for 12 hours with high resolution time steps
    printf("\nSimulating storm development (12 hours)...\n");
    f32 dt = 30.0f; // 30 second time steps for storm dynamics
    u32 steps = 12 * 60 * 2; // 12 hours
    
    f32 peak_precipitation = 0.0f;
    f32 storm_intensity = 0.0f;
    
    for (u32 step = 0; step < steps; step++) {
        atmospheric_simulate(atm, dt);
        
        // Track storm intensity metrics
        if (atm->stats.total_precipitation > peak_precipitation) {
            peak_precipitation = atm->stats.total_precipitation;
        }
        
        f32 current_intensity = atm->stats.max_wind_speed + atm->stats.total_precipitation * 10000.0f;
        if (current_intensity > storm_intensity) {
            storm_intensity = current_intensity;
        }
        
        // Print status every 2 hours
        if (step % (2 * 60 * 2) == 0) {
            f32 hours = step / (60.0f * 2.0f);
            
            // Find maximum precipitation rate in current time step
            f32 max_precip_rate = 0.0f;
            for (u32 i = 0; i < atm->grid_x * atm->grid_y * atm->grid_z; i++) {
                if (atm->cells[i].precipitation_rate > max_precip_rate) {
                    max_precip_rate = atm->cells[i].precipitation_rate;
                }
            }
            
            printf("  Hour %.1f: Wind %.1fm/s, Max Precip Rate %.1fmm/h, Storm Intensity %.1f\n",
                   hours, atm->stats.max_wind_speed, 
                   max_precip_rate * 3600.0f * 1000.0f, // Convert to mm/h
                   current_intensity);
        }
    }
    
    printf("\nStorm simulation complete\n");
    atmospheric_debug_draw(atm);
    
    // Analyze storm characteristics
    u32 cloudy_cells = 0;
    u32 precipitating_cells = 0;
    f32 total_cloud_water = 0.0f;
    
    for (u32 i = 0; i < atm->grid_x * atm->grid_y * atm->grid_z; i++) {
        if (atm->cells[i].cloud_water > 0.0001f) cloudy_cells++;
        if (atm->cells[i].precipitation_rate > 0.0001f) precipitating_cells++;
        total_cloud_water += atm->cells[i].cloud_water;
    }
    
    printf("Storm Analysis:\n");
    printf("  Peak precipitation rate: %.1f mm/h\n", peak_precipitation * 3600.0f * 1000.0f);
    printf("  Storm intensity index: %.1f\n", storm_intensity);
    printf("  Cloudy cells: %u/%u (%.1f%%)\n", cloudy_cells, 
           atm->grid_x * atm->grid_y * atm->grid_z,
           100.0f * cloudy_cells / (atm->grid_x * atm->grid_y * atm->grid_z));
    printf("  Precipitating cells: %u\n", precipitating_cells);
    printf("  Total cloud water: %.1f kg\n", total_cloud_water);
    
    if (precipitating_cells > 50 && storm_intensity > 100.0f) {
        printf("✓ Storm developed successfully with precipitation\n");
    } else {
        printf("✗ Storm development insufficient\n");
    }
    
    destroy_test_arena(arena);
    printf("=== TEST COMPLETE ===\n\n");
}

void test_atmospheric_coupling() {
    printf("=== TEST: Atmospheric-Hydrological Coupling ===\n");
    
    arena* arena = create_test_arena(MEGABYTES(100));
    
    // Regional domain for coupling test: 200km x 200km x 10km
    atmospheric_system* atm = atmospheric_system_init(arena, 32, 32, 16,
                                                     200.0f, 200.0f, 10.0f);
    
    printf("Coupling test domain: 200km x 200km x 10km\n");
    printf("Surface cells for precipitation output: %u\n", atm->grid_x * atm->grid_y);
    
    // Create scattered precipitation events
    for (u32 z = 8; z < atm->grid_z; z++) { // Upper levels
        for (u32 y = 0; y < atm->grid_y; y++) {
            for (u32 x = 0; x < atm->grid_x; x++) {
                u32 idx = z * atm->grid_x * atm->grid_y + y * atm->grid_x + x;
                
                // Create patchy high humidity regions
                if ((x + y + z) % 7 == 0) { // Pseudo-random pattern
                    atm->cells[idx].humidity *= 3.0f; // Triple humidity
                    atm->cells[idx].cloud_water = 0.002f; // Pre-existing clouds
                    atm->cells[idx].aerosol_density *= 5.0f; // More condensation nuclei
                }
            }
        }
    }
    
    // Simulate for 6 hours to develop precipitation
    printf("\nSimulating precipitation development (6 hours)...\n");
    f32 dt = 60.0f; // 1 minute time steps
    u32 steps = 6 * 60;
    
    f32 cumulative_precip[32*32] = {0}; // Track surface precipitation
    
    for (u32 step = 0; step < steps; step++) {
        atmospheric_simulate(atm, dt);
        
        // Accumulate precipitation at surface
        for (u32 i = 0; i < atm->grid_x * atm->grid_y; i++) {
            cumulative_precip[i] += atm->precipitation_output[i] * dt;
        }
        
        if (step % 60 == 0) { // Every hour
            f32 hours = step / 60.0f;
            
            // Calculate average precipitation rate over domain
            f32 avg_precip = 0.0f;
            f32 max_precip = 0.0f;
            for (u32 i = 0; i < atm->grid_x * atm->grid_y; i++) {
                avg_precip += atm->precipitation_output[i];
                if (atm->precipitation_output[i] > max_precip) {
                    max_precip = atm->precipitation_output[i];
                }
            }
            avg_precip /= (atm->grid_x * atm->grid_y);
            
            printf("  Hour %.0f: Avg Precip %.2f mm/h, Max Precip %.2f mm/h\n",
                   hours, avg_precip * 3600.0f * 1000.0f, max_precip * 3600.0f * 1000.0f);
        }
    }
    
    // Analyze precipitation distribution for hydrological coupling
    printf("\nPrecipitation Analysis for Hydrological Coupling:\n");
    
    f32 total_domain_precip = 0.0f;
    f32 max_cell_precip = 0.0f;
    u32 wet_cells = 0;
    
    for (u32 i = 0; i < atm->grid_x * atm->grid_y; i++) {
        total_domain_precip += cumulative_precip[i];
        if (cumulative_precip[i] > max_cell_precip) {
            max_cell_precip = cumulative_precip[i];
        }
        if (cumulative_precip[i] > 0.001f) { // >1mm
            wet_cells++;
        }
    }
    
    f32 avg_precip = total_domain_precip / (atm->grid_x * atm->grid_y);
    
    printf("  Total domain precipitation: %.1f mm\n", total_domain_precip * 1000.0f);
    printf("  Average precipitation: %.2f mm\n", avg_precip * 1000.0f);
    printf("  Maximum cell precipitation: %.1f mm\n", max_cell_precip * 1000.0f);
    printf("  Wet cells (>1mm): %u/%u (%.1f%%)\n", wet_cells, 
           atm->grid_x * atm->grid_y, 100.0f * wet_cells / (atm->grid_x * atm->grid_y));
    
    // Verify coupling data format
    printf("\nCoupling Data Verification:\n");
    printf("  Precipitation output array size: %u cells\n", atm->grid_x * atm->grid_y);
    printf("  Sample precipitation values (mm):\n");
    for (u32 i = 0; i < 10 && i < atm->grid_x * atm->grid_y; i++) {
        printf("    Cell[%u]: %.3f mm\n", i, cumulative_precip[i] * 1000.0f);
    }
    
    atmospheric_debug_draw(atm);
    
    if (wet_cells > 10 && avg_precip > 0.001f) {
        printf("✓ Atmospheric-hydrological coupling data generated successfully\n");
    } else {
        printf("✗ Insufficient precipitation for hydrological coupling\n");
    }
    
    destroy_test_arena(arena);
    printf("=== TEST COMPLETE ===\n\n");
}

int main() {
    printf("Handmade Atmospheric Physics Test Suite\n");
    printf("=======================================\n\n");
    
    // Run all tests
    test_continental_weather_simulation();
    test_storm_development();
    test_atmospheric_coupling();
    
    printf("=======================================\n");
    printf("All atmospheric physics tests completed!\n");
    
    return 0;
}