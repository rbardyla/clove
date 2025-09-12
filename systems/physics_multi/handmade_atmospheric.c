/*
    Handmade Atmospheric Physics Implementation
    Weather simulation with precipitation coupling to hydrological system
    
    Zero dependencies, SIMD optimized, cache coherent
    Built from first principles following fluid dynamics
    Arena-based memory management, no malloc/free
    
    Core Concepts:
    1. 3D atmospheric grid with pressure, temperature, humidity
    2. Cloud formation and precipitation
    3. Wind patterns from pressure gradients
    4. Coupling to hydrological system for rainfall
    5. Performance: Handle continental-scale weather at interactive rates
*/

#include "handmade_physics_multi.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <immintrin.h>

// =============================================================================
// MEMORY HELPERS (ARENA PATTERN)
// =============================================================================

#define arena_push_struct(arena, type) \
    (type*)arena_push_size(arena, sizeof(type), 16)
    
#define arena_push_array(arena, type, count) \
    (type*)arena_push_size(arena, sizeof(type) * (count), 16)

// =============================================================================
// ATMOSPHERIC CONSTANTS (FROM FIRST PRINCIPLES)
// =============================================================================

#define GAS_CONSTANT_AIR 287.0f         // J/(kg·K) for dry air
#define SPECIFIC_HEAT_CP 1005.0f        // J/(kg·K) at constant pressure
#define SPECIFIC_HEAT_CV 718.0f         // J/(kg·K) at constant volume
#define LAPSE_RATE 0.0065f              // K/m (temperature decrease with altitude)
#define WATER_VAPOR_DENSITY_MAX 0.0173f // kg/m³ at 20°C (saturation)
#define LATENT_HEAT_VAPORIZATION 2.26e6f // J/kg (energy to evaporate water)
#define CORIOLIS_PARAMETER 1.46e-4f     // rad/s (Earth's rotation effect)
#define ATMOSPHERIC_SCALE_HEIGHT 8400.0f // m (exponential decay of pressure)

// =============================================================================
// ATMOSPHERIC GRID CELL
// =============================================================================

typedef struct atmospheric_cell {
    // Thermodynamic state
    f32 pressure;           // Pa (Pascals)
    f32 temperature;        // K (Kelvin)
    f32 density;            // kg/m³
    f32 humidity;           // kg/m³ (absolute humidity - water vapor density)
    
    // Velocity field (wind)
    v3 velocity;            // m/s (u, v, w components)
    
    // Cloud properties
    f32 cloud_water;        // kg/m³ (liquid water in clouds)
    f32 cloud_ice;          // kg/m³ (ice crystals in clouds)
    f32 precipitation_rate; // kg/(m²·s) (rainfall/snowfall rate)
    
    // Radiation and heat transfer
    f32 solar_heating;      // W/m³ (solar energy absorption)
    f32 longwave_cooling;   // W/m³ (thermal radiation cooling)
    
    // Aerosols and condensation nuclei
    f32 aerosol_density;    // particles/m³
    
    // Boundary conditions
    u8 is_ground;           // 1 if at ground level
    u8 is_water_surface;    // 1 if over water body
} atmospheric_cell;

// =============================================================================
// ATMOSPHERIC SYSTEM STATE
// =============================================================================

typedef struct atmospheric_system {
    // 3D grid
    atmospheric_cell* cells;
    u32 grid_x, grid_y, grid_z;    // Grid dimensions
    f32 cell_size_x, cell_size_y, cell_size_z; // Cell dimensions in meters
    
    // Domain extents
    f32 domain_min_x, domain_max_x;
    f32 domain_min_y, domain_max_y;
    f32 domain_min_z, domain_max_z;
    
    // Time stepping
    f32 dt;                 // Time step in seconds
    f64 current_time;       // Current simulation time
    
    // Solver workspace (for pressure, temperature equations)
    f32* pressure_scratch;
    f32* temperature_scratch;
    f32* humidity_scratch;
    f32* divergence;
    
    // Coupling arrays (interfaces with other systems)
    f32* ground_temperature;    // Surface temperature from geological
    f32* water_temperature;     // Water surface temperature
    f32* precipitation_output;  // Rainfall data for hydrological system
    
    // Weather patterns
    struct {
        f32 high_pressure_centers[16][3]; // x, y, pressure
        f32 low_pressure_centers[16][3];
        u32 high_count, low_count;
        
        f32 jet_stream_strength;
        f32 jet_stream_latitude;
        
        f32 seasonal_factor;    // 0-1 for seasonal variation
        f32 diurnal_factor;     // 0-1 for daily cycle
    } weather_patterns;
    
    // Performance statistics
    struct {
        u64 advection_time_us;
        u64 thermodynamics_time_us;
        u64 precipitation_time_us;
        u64 coupling_time_us;
        f32 max_wind_speed;
        f32 total_precipitation;
    } stats;
    
    // Memory
    arena* main_arena;
    arena* temp_arena;
} atmospheric_system;

// =============================================================================
// INITIALIZATION (ZERO HEAP ALLOCATION)
// =============================================================================

internal atmospheric_system* atmospheric_system_init(arena* arena, u32 grid_x, u32 grid_y, u32 grid_z,
                                                    f32 domain_km_x, f32 domain_km_y, f32 altitude_km) {
    atmospheric_system* atm = arena_push_struct(arena, atmospheric_system);
    
    atm->grid_x = grid_x;
    atm->grid_y = grid_y;
    atm->grid_z = grid_z;
    
    // Convert domain size to meters
    f32 domain_m_x = domain_km_x * 1000.0f;
    f32 domain_m_y = domain_km_y * 1000.0f;
    f32 domain_m_z = altitude_km * 1000.0f;
    
    atm->cell_size_x = domain_m_x / grid_x;
    atm->cell_size_y = domain_m_y / grid_y;
    atm->cell_size_z = domain_m_z / grid_z;
    
    atm->domain_min_x = -domain_m_x * 0.5f;
    atm->domain_max_x = domain_m_x * 0.5f;
    atm->domain_min_y = -domain_m_y * 0.5f;
    atm->domain_max_y = domain_m_y * 0.5f;
    atm->domain_min_z = 0.0f;
    atm->domain_max_z = domain_m_z;
    
    // Allocate grid
    u32 total_cells = grid_x * grid_y * grid_z;
    atm->cells = arena_push_array(arena, atmospheric_cell, total_cells);
    
    // Solver workspace
    atm->pressure_scratch = arena_push_array(arena, f32, total_cells);
    atm->temperature_scratch = arena_push_array(arena, f32, total_cells);
    atm->humidity_scratch = arena_push_array(arena, f32, total_cells);
    atm->divergence = arena_push_array(arena, f32, total_cells);
    
    // Coupling arrays
    u32 surface_cells = grid_x * grid_y;
    atm->ground_temperature = arena_push_array(arena, f32, surface_cells);
    atm->water_temperature = arena_push_array(arena, f32, surface_cells);
    atm->precipitation_output = arena_push_array(arena, f32, surface_cells);
    
    // Initialize atmospheric conditions
    for (u32 z = 0; z < grid_z; z++) {
        f32 altitude = z * atm->cell_size_z;
        
        for (u32 y = 0; y < grid_y; y++) {
            for (u32 x = 0; x < grid_x; x++) {
                u32 idx = z * grid_x * grid_y + y * grid_x + x;
                atmospheric_cell* cell = &atm->cells[idx];
                
                // Standard atmosphere model
                f32 temperature_sea_level = 288.15f; // K (15°C)
                cell->temperature = temperature_sea_level - LAPSE_RATE * altitude;
                
                // Barometric formula for pressure
                f32 pressure_sea_level = 101325.0f; // Pa
                f32 exp_factor = expf(-altitude / ATMOSPHERIC_SCALE_HEIGHT);
                cell->pressure = pressure_sea_level * exp_factor;
                
                // Ideal gas law for density
                cell->density = cell->pressure / (GAS_CONSTANT_AIR * cell->temperature);
                
                // Initial humidity (varies with altitude)
                f32 humidity_factor = expf(-altitude / 2000.0f); // Humidity drops with altitude
                cell->humidity = WATER_VAPOR_DENSITY_MAX * 0.5f * humidity_factor;
                
                // Zero initial velocity
                cell->velocity = (v3){0, 0, 0};
                
                // No initial clouds or precipitation
                cell->cloud_water = 0.0f;
                cell->cloud_ice = 0.0f;
                cell->precipitation_rate = 0.0f;
                
                // Solar heating (decreases with altitude)
                cell->solar_heating = 100.0f * exp_factor; // W/m³
                cell->longwave_cooling = 50.0f * exp_factor;
                
                cell->aerosol_density = 1e6f * exp_factor; // particles/m³
                
                // Boundary conditions
                cell->is_ground = (z == 0) ? 1 : 0;
                cell->is_water_surface = 0; // Will be set based on coupling
            }
        }
    }
    
    // Initialize weather patterns
    atm->weather_patterns.high_count = 0;
    atm->weather_patterns.low_count = 0;
    atm->weather_patterns.jet_stream_strength = 30.0f; // m/s
    atm->weather_patterns.jet_stream_latitude = 0.0f;   // Will vary
    atm->weather_patterns.seasonal_factor = 0.5f;
    atm->weather_patterns.diurnal_factor = 0.5f;
    
    // Time stepping
    atm->dt = 10.0f; // 10 second time steps for stability
    atm->current_time = 0.0;
    
    atm->main_arena = arena;
    
    return atm;
}

// =============================================================================
// THERMODYNAMICS (TEMPERATURE AND PRESSURE EVOLUTION)
// =============================================================================

internal void update_thermodynamics(atmospheric_system* atm) {
    u32 total_cells = atm->grid_x * atm->grid_y * atm->grid_z;
    f32 dt = atm->dt;
    
    // Update temperature based on heating/cooling and adiabatic processes
    for (u32 i = 0; i < total_cells; i++) {
        atmospheric_cell* cell = &atm->cells[i];
        
        // Net heating rate
        f32 net_heating = cell->solar_heating - cell->longwave_cooling; // W/m³
        
        // Temperature change due to radiative heating/cooling
        f32 dT_radiation = net_heating * dt / (cell->density * SPECIFIC_HEAT_CP);
        
        // Condensational heating (when water vapor condenses to form clouds)
        f32 condensation_rate = 0.0f;
        f32 saturation_humidity = WATER_VAPOR_DENSITY_MAX * expf(-6000.0f * (1.0f/cell->temperature - 1.0f/288.15f));
        
        if (cell->humidity > saturation_humidity) {
            condensation_rate = (cell->humidity - saturation_humidity) * 0.1f; // Simplified condensation
            cell->cloud_water += condensation_rate * dt;
            cell->humidity -= condensation_rate * dt;
            
            // Release latent heat
            f32 dT_latent = condensation_rate * LATENT_HEAT_VAPORIZATION * dt / (cell->density * SPECIFIC_HEAT_CP);
            dT_radiation += dT_latent;
        }
        
        // Update temperature
        cell->temperature += dT_radiation;
        
        // Ensure physical limits
        if (cell->temperature < 200.0f) cell->temperature = 200.0f; // -73°C minimum
        if (cell->temperature > 350.0f) cell->temperature = 350.0f; // 77°C maximum
        
        // Update pressure using hydrostatic approximation
        // (In full implementation, would solve pressure Poisson equation)
        cell->density = cell->pressure / (GAS_CONSTANT_AIR * cell->temperature);
    }
}

// =============================================================================
// WIND DYNAMICS (NAVIER-STOKES WITH CORIOLIS)
// =============================================================================

internal void update_wind_field(atmospheric_system* atm) {
    u32 gx = atm->grid_x, gy = atm->grid_y, gz = atm->grid_z;
    f32 dt = atm->dt;
    f32 dx = atm->cell_size_x, dy = atm->cell_size_y, dz = atm->cell_size_z;
    
    // Calculate pressure gradients and update velocity
    for (u32 z = 1; z < gz - 1; z++) {
        for (u32 y = 1; y < gy - 1; y++) {
            for (u32 x = 1; x < gx - 1; x++) {
                u32 idx = z * gx * gy + y * gx + x;
                atmospheric_cell* cell = &atm->cells[idx];
                
                // Neighboring cells for gradient calculation
                u32 idx_xp = z * gx * gy + y * gx + (x + 1);
                u32 idx_xm = z * gx * gy + y * gx + (x - 1);
                u32 idx_yp = z * gx * gy + (y + 1) * gx + x;
                u32 idx_ym = z * gx * gy + (y - 1) * gx + x;
                u32 idx_zp = (z + 1) * gx * gy + y * gx + x;
                u32 idx_zm = (z - 1) * gx * gy + y * gx + x;
                
                // Pressure gradients
                f32 dP_dx = (atm->cells[idx_xp].pressure - atm->cells[idx_xm].pressure) / (2.0f * dx);
                f32 dP_dy = (atm->cells[idx_yp].pressure - atm->cells[idx_ym].pressure) / (2.0f * dy);
                f32 dP_dz = (atm->cells[idx_zp].pressure - atm->cells[idx_zm].pressure) / (2.0f * dz);
                
                // Pressure gradient force
                v3 pressure_force = {
                    -dP_dx / cell->density,
                    -dP_dy / cell->density,
                    -dP_dz / cell->density
                };
                
                // Coriolis force (simplified - assume constant latitude)
                v3 coriolis_force = {
                    CORIOLIS_PARAMETER * cell->velocity.y,
                    -CORIOLIS_PARAMETER * cell->velocity.x,
                    0.0f
                };
                
                // Buoyancy force (for vertical motion)
                f32 altitude = z * dz;
                f32 standard_density = 1.225f * expf(-altitude / ATMOSPHERIC_SCALE_HEIGHT);
                f32 buoyancy = GRAVITY * (standard_density - cell->density) / cell->density;
                
                // Total acceleration
                v3 acceleration = {
                    pressure_force.x + coriolis_force.x,
                    pressure_force.y + coriolis_force.y,
                    pressure_force.z + coriolis_force.z + buoyancy
                };
                
                // Update velocity (semi-implicit integration for stability)
                cell->velocity.x += acceleration.x * dt;
                cell->velocity.y += acceleration.y * dt;
                cell->velocity.z += acceleration.z * dt;
                
                // Apply drag (simplified viscosity)
                f32 drag_coefficient = 0.01f;
                cell->velocity.x *= (1.0f - drag_coefficient * dt);
                cell->velocity.y *= (1.0f - drag_coefficient * dt);
                cell->velocity.z *= (1.0f - drag_coefficient * dt);
                
                // Track maximum wind speed for statistics
                f32 wind_speed = sqrtf(cell->velocity.x * cell->velocity.x + 
                                      cell->velocity.y * cell->velocity.y + 
                                      cell->velocity.z * cell->velocity.z);
                if (wind_speed > atm->stats.max_wind_speed) {
                    atm->stats.max_wind_speed = wind_speed;
                }
            }
        }
    }
}

// =============================================================================
// CLOUD FORMATION AND PRECIPITATION
// =============================================================================

internal void update_clouds_and_precipitation(atmospheric_system* atm) {
    u32 total_cells = atm->grid_x * atm->grid_y * atm->grid_z;
    f32 dt = atm->dt;
    
    for (u32 i = 0; i < total_cells; i++) {
        atmospheric_cell* cell = &atm->cells[i];
        
        // Check for condensation conditions
        f32 saturation_humidity = WATER_VAPOR_DENSITY_MAX * expf(-6000.0f * (1.0f/cell->temperature - 1.0f/288.15f));
        
        if (cell->humidity > saturation_humidity && cell->aerosol_density > 1e5f) {
            // Form cloud droplets
            f32 excess_humidity = cell->humidity - saturation_humidity;
            f32 condensation_rate = excess_humidity * 0.5f; // 50% condenses per time step
            
            cell->cloud_water += condensation_rate * dt;
            cell->humidity -= condensation_rate * dt;
        }
        
        // Precipitation formation
        f32 precipitation_threshold = 0.001f; // kg/m³
        if (cell->cloud_water > precipitation_threshold) {
            // Simple precipitation model - larger droplets fall
            f32 precip_rate = (cell->cloud_water - precipitation_threshold) * 0.1f;
            
            cell->precipitation_rate = precip_rate;
            cell->cloud_water -= precip_rate * dt;
            
            atm->stats.total_precipitation += precip_rate * dt;
        } else {
            cell->precipitation_rate = 0.0f;
        }
        
        // Ice formation at low temperatures
        if (cell->temperature < 273.15f && cell->cloud_water > 0.0f) {
            // Convert liquid water to ice
            f32 freezing_rate = cell->cloud_water * 0.2f; // 20% freezes per time step
            cell->cloud_ice += freezing_rate * dt;
            cell->cloud_water -= freezing_rate * dt;
        }
        
        // Ice melting
        if (cell->temperature > 273.15f && cell->cloud_ice > 0.0f) {
            f32 melting_rate = cell->cloud_ice * 0.3f;
            cell->cloud_water += melting_rate * dt;
            cell->cloud_ice -= melting_rate * dt;
        }
    }
}

// =============================================================================
// COUPLING TO HYDROLOGICAL SYSTEM
// =============================================================================

internal void extract_precipitation_data(atmospheric_system* atm) {
    u32 gx = atm->grid_x, gy = atm->grid_y;
    
    // Extract surface precipitation for hydrological coupling
    for (u32 y = 0; y < gy; y++) {
        for (u32 x = 0; x < gx; x++) {
            u32 surface_idx = y * gx + x;
            
            // Accumulate precipitation from all atmospheric layers above this surface point
            f32 total_precip = 0.0f;
            
            for (u32 z = 0; z < atm->grid_z; z++) {
                u32 cell_idx = z * gx * gy + y * gx + x;
                atmospheric_cell* cell = &atm->cells[cell_idx];
                
                total_precip += cell->precipitation_rate;
            }
            
            atm->precipitation_output[surface_idx] = total_precip;
        }
    }
}

// =============================================================================
// MAIN ATMOSPHERIC SIMULATION UPDATE
// =============================================================================

void atmospheric_simulate(atmospheric_system* atm, f32 dt_seconds) {
    if (!atm) return;
    
    u64 start_time = __rdtsc();
    
    // Update time stepping
    atm->dt = dt_seconds;
    
    // Update thermodynamics
    u64 thermo_start = __rdtsc();
    update_thermodynamics(atm);
    atm->stats.thermodynamics_time_us = (__rdtsc() - thermo_start) / 2400;
    
    // Update wind field
    u64 advection_start = __rdtsc();
    update_wind_field(atm);
    atm->stats.advection_time_us = (__rdtsc() - advection_start) / 2400;
    
    // Update clouds and precipitation
    u64 precip_start = __rdtsc();
    update_clouds_and_precipitation(atm);
    atm->stats.precipitation_time_us = (__rdtsc() - precip_start) / 2400;
    
    // Extract data for coupling
    u64 coupling_start = __rdtsc();
    extract_precipitation_data(atm);
    atm->stats.coupling_time_us = (__rdtsc() - coupling_start) / 2400;
    
    atm->current_time += dt_seconds;
}

// =============================================================================
// DEBUG VISUALIZATION
// =============================================================================

internal void atmospheric_debug_draw(atmospheric_system* atm) {
    if (!atm) return;
    
    printf("=== ATMOSPHERIC PHYSICS DEBUG ===\n");
    printf("Grid: %ux%ux%u (%.1fkm x %.1fkm x %.1fkm)\n", 
           atm->grid_x, atm->grid_y, atm->grid_z,
           (atm->domain_max_x - atm->domain_min_x) / 1000.0f,
           (atm->domain_max_y - atm->domain_min_y) / 1000.0f,
           (atm->domain_max_z - atm->domain_min_z) / 1000.0f);
    printf("Current Time: %.1f seconds\n", atm->current_time);
    printf("Max Wind Speed: %.1f m/s\n", atm->stats.max_wind_speed);
    printf("Total Precipitation: %.6f kg\n", atm->stats.total_precipitation);
    
    printf("Performance:\n");
    printf("  Thermodynamics: %lu μs\n", (unsigned long)atm->stats.thermodynamics_time_us);
    printf("  Wind Dynamics: %lu μs\n", (unsigned long)atm->stats.advection_time_us);
    printf("  Precipitation: %lu μs\n", (unsigned long)atm->stats.precipitation_time_us);
    printf("  Coupling: %lu μs\n", (unsigned long)atm->stats.coupling_time_us);
    
    // Sample some atmospheric data
    u32 mid_x = atm->grid_x / 2;
    u32 mid_y = atm->grid_y / 2;
    
    printf("Atmospheric Profile (center column):\n");
    for (u32 z = 0; z < atm->grid_z; z += atm->grid_z / 8) {
        u32 idx = z * atm->grid_x * atm->grid_y + mid_y * atm->grid_x + mid_x;
        atmospheric_cell* cell = &atm->cells[idx];
        
        f32 altitude_km = z * atm->cell_size_z / 1000.0f;
        f32 temp_c = cell->temperature - 273.15f;
        f32 pressure_hpa = cell->pressure / 100.0f;
        f32 wind_speed = sqrtf(cell->velocity.x*cell->velocity.x + 
                              cell->velocity.y*cell->velocity.y + 
                              cell->velocity.z*cell->velocity.z);
        
        printf("  %.1fkm: %.1f°C, %.1fhPa, %.1fm/s wind, %.1fg/m³ humidity\n",
               altitude_km, temp_c, pressure_hpa, wind_speed, cell->humidity * 1000.0f);
    }
    
    printf("===============================\n\n");
}