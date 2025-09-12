#ifndef HANDMADE_PHYSICS_MULTI_H
#define HANDMADE_PHYSICS_MULTI_H

/*
    Handmade Multi-Scale Physics System
    Deep physics simulation from geological to quantum timescales
    Zero dependencies, SIMD optimized, cache coherent
    
    Timescales:
    - Geological: Millions of years (tectonic plates, mountain formation)
    - Hydrological: Years to centuries (erosion, river formation)
    - Structural: Days to years (buildings, bridges settling)
    - Realtime: Microseconds (particles, collisions)
    
    All scales influence each other through unified physics laws
*/

#include "../../src/handmade.h"
#include "handmade_math_types.h"
#include <immintrin.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#define MAX_TECTONIC_PLATES 32
#define MAX_PLATE_VERTICES 1024
#define MANTLE_GRID_SIZE 256
#define FLUID_GRID_SIZE 512
#define MAX_FLUID_PARTICLES 1000000
#define STRUCTURAL_GRID_SIZE 128
#define MAX_STRUCTURAL_ELEMENTS 65536

// Time scaling factors (how much simulation time passes per real second)
#define GEOLOGICAL_TIME_SCALE (1000000.0)  // 1 million years per second
#define HYDROLOGICAL_TIME_SCALE (100.0)    // 100 years per second  
#define STRUCTURAL_TIME_SCALE (1.0)        // 1 day per second
#define REALTIME_TIME_SCALE (1.0)          // 1:1 with real time

// Physics constants
#define EARTH_RADIUS_KM 6371.0f
#define GRAVITY 9.81f
#define MANTLE_VISCOSITY 1e21f  // Pa·s
#define ROCK_DENSITY 2700.0f    // kg/m³
#define WATER_DENSITY 1000.0f   // kg/m³

// =============================================================================
// GEOLOGICAL PHYSICS (Tectonic Simulation)
// =============================================================================

typedef enum plate_type {
    PLATE_CONTINENTAL = 0,
    PLATE_OCEANIC = 1
} plate_type;

typedef struct tectonic_vertex {
    v3 position;        // Position on sphere
    v3 velocity;        // Movement velocity
    f32 elevation;      // Height above sea level
    f32 thickness;      // Crust thickness
    f32 temperature;    // Temperature in Kelvin
    f32 pressure;       // Pressure in Pa
    f32 stress_xx, stress_yy, stress_xy;  // Stress tensor components
} tectonic_vertex;

typedef struct tectonic_plate {
    // Plate properties
    plate_type type;
    f32 density;
    f32 age;  // Million years
    
    // Mesh representation (SoA for SIMD)
    tectonic_vertex* vertices;
    u32 vertex_count;
    u32* triangles;
    u32 triangle_count;
    
    // Movement
    v3 rotation_axis;
    f32 angular_velocity;  // rad/million years
    v3 center_of_mass;
    
    // Forces
    v3 mantle_force;
    v3 collision_force;
    v3 subduction_force;
    
    // Boundaries
    u32 neighboring_plates[8];
    u32 neighbor_count;
    
    // Statistics
    f32 total_mass;
    f32 total_area;
    f32 average_elevation;
} tectonic_plate;

typedef struct mantle_convection {
    // 3D grid of mantle flow
    v3* velocity_field;      // Mantle flow velocities
    f32* temperature_field;  // Temperature distribution
    f32* density_field;      // Density variations
    u32 grid_size;
    
    // Rayleigh-Benard convection parameters
    f32 rayleigh_number;
    f32 prandtl_number;
    f32 thermal_diffusivity;
} mantle_convection;

typedef struct geological_state {
    tectonic_plate plates[MAX_TECTONIC_PLATES];
    u32 plate_count;
    
    mantle_convection* mantle;
    
    // Time tracking
    f64 geological_time;  // Million years since simulation start
    f64 dt;               // Time step in million years
    
    // Global properties
    f32 sea_level;
    f32 global_temperature;
    
    // Collision detection acceleration
    void* collision_grid;  // Spatial hash for plate boundaries
} geological_state;

// =============================================================================
// HYDROLOGICAL PHYSICS (Fluid Dynamics)
// =============================================================================

typedef struct fluid_particle {
    v3 position;
    v3 velocity;
    f32 pressure;
    f32 density;
    f32 temperature;
    f32 sediment_concentration;
} fluid_particle;

typedef struct fluid_cell {
    // MAC grid (staggered grid for stability)
    f32 velocity_x;  // Face-centered
    f32 velocity_y;
    f32 velocity_z;
    f32 pressure;    // Cell-centered
    f32 density;
    f32 temperature;
    
    // Erosion/deposition
    f32 sediment_capacity;
    f32 sediment_amount;
    f32 erosion_rate;
    
    // Precipitation
    f32 precipitation_rate;  // m/s
    
    // Boundary conditions
    u8 is_solid;
    u8 is_source;
    u8 is_sink;
} fluid_cell;

typedef struct fluid_state {
    // Eulerian grid (for incompressible flow)
    fluid_cell* grid;
    u32 grid_x, grid_y, grid_z;
    
    // Lagrangian particles (for sediment transport)
    fluid_particle* particles;
    u32 particle_count;
    u32 max_particles;
    
    // Solver state
    f32* pressure_scratch;
    f32* divergence;
    
    // Physical parameters
    f32 viscosity;
    f32 surface_tension;
    f32 evaporation_rate;
    f32 precipitation_rate;
    
    // Time
    f64 hydro_time;  // Years since start
    f32 dt;          // Time step in years
} fluid_state;

// =============================================================================
// STRUCTURAL PHYSICS (Buildings, Bridges)
// =============================================================================

typedef enum element_type {
    ELEMENT_BEAM,
    ELEMENT_COLUMN,
    ELEMENT_SLAB,
    ELEMENT_WALL,
    ELEMENT_FOUNDATION
} element_type;

typedef struct structural_element {
    element_type type;
    
    // Geometry
    v3 start, end;  // For beams/columns
    v3 corners[8];  // For slabs/walls
    
    // Material properties
    f32 youngs_modulus;
    f32 poisson_ratio;
    f32 yield_strength;
    f32 density;
    
    // Current state
    v3 displacement;
    v3 rotation;
    f32 stress[6];  // σxx, σyy, σzz, τxy, τyz, τxz
    f32 strain[6];
    
    // Damage
    f32 damage;  // 0 = pristine, 1 = failed
    f32 fatigue_cycles;
    
    // Connections
    u32 connected_elements[16];
    u32 connection_count;
} structural_element;

typedef struct structural_state {
    structural_element* elements;
    u32 element_count;
    u32 max_elements;
    
    // Global stiffness matrix (sparse)
    void* stiffness_matrix;
    
    // Load vectors
    v3* nodal_forces;
    v3* nodal_displacements;
    
    // Environmental loads
    v3 wind_load;
    v3 seismic_acceleration;
    f32 temperature_change;
    
    // Time
    f64 structural_time;  // Days since start
    f32 dt;               // Time step in days
} structural_state;

// =============================================================================
// UNIFIED MULTI-SCALE PHYSICS
// =============================================================================

typedef struct physics_interaction {
    // How geological affects fluid
    f32 terrain_to_fluid_coupling;
    
    // How fluid affects geological (erosion)
    f32 fluid_to_terrain_coupling;
    
    // How geological affects structural (earthquakes)
    f32 geological_to_structural_coupling;
    
    // How structural affects fluid (dams, channels)
    f32 structural_to_fluid_coupling;
} physics_interaction;

typedef struct multi_physics_state {
    // All simulation layers
    geological_state* geological;
    fluid_state* fluid;
    structural_state* structural;
    
    // Time management
    f64 current_time;
    f64 time_scales[4];
    
    // Cross-scale interactions
    physics_interaction interactions;
    
    // Unified height field (result of all simulations)
    f32* unified_heightmap;
    u32 heightmap_resolution;
    
    // Statistics and debugging
    struct {
        u64 geological_steps;
        u64 fluid_steps;
        u64 structural_steps;
        f64 total_compute_time_ms;
        f64 geological_time_ms;
        f64 fluid_time_ms;
        f64 structural_time_ms;
    } stats;
    
    // Memory
    arena* main_arena;
    arena* temp_arena;
} multi_physics_state;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct structural_system structural_system;
typedef struct atmospheric_system atmospheric_system;

// =============================================================================
// API FUNCTIONS
// =============================================================================

// Initialize multi-scale physics system
multi_physics_state* multi_physics_init(arena* arena, u32 seed);

// Step simulations forward
void multi_physics_update(multi_physics_state* state, f32 real_dt);

// Individual scale updates
void geological_simulate(geological_state* geo, f64 dt_million_years);
void fluid_simulate(fluid_state* fluid, geological_state* geo, arena* temp_arena, f32 dt_years);
void structural_simulate(structural_system* sys, geological_state* geo, f32 dt_seconds);
void atmospheric_simulate(atmospheric_system* atm, f32 dt_seconds);

// Cross-scale coupling
void apply_geological_to_fluid(geological_state* geo, fluid_state* fluid);
void apply_fluid_erosion_to_geological(fluid_state* fluid, geological_state* geo);
void apply_seismic_to_structural(geological_state* geo, structural_state* structural);

// Query functions
f32 multi_physics_get_height(multi_physics_state* state, f32 x, f32 z);
f32 multi_physics_get_water_depth(multi_physics_state* state, f32 x, f32 z);
f32 multi_physics_get_rock_stress(multi_physics_state* state, f32 x, f32 z);

// Visualization
void multi_physics_debug_draw(multi_physics_state* state);
void geological_debug_draw(geological_state* geo);
void fluid_debug_draw(fluid_state* fluid);

// =============================================================================
// SIMD OPTIMIZED OPERATIONS
// =============================================================================

// Tectonic plate collision detection (AVX2)
void plate_collision_simd(tectonic_plate* plate_a, tectonic_plate* plate_b,
                          f32* collision_forces, u32 count);

// Fluid pressure solve (AVX2 Gauss-Seidel)
void fluid_pressure_solve_simd(fluid_cell* grid, f32* pressure,
                               f32* divergence, u32 grid_x, u32 grid_y, 
                               u32 grid_z, u32 iterations);

// Structural stiffness matrix multiply (AVX2)
void stiffness_matrix_multiply_simd(f32* K, f32* u, f32* f, u32 size);

// Erosion calculation (AVX2)
void calculate_erosion_simd(f32* heights, f32* water_flow, f32* erosion,
                           u32 width, u32 height);

#endif // HANDMADE_PHYSICS_MULTI_H