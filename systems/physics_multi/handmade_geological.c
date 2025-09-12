/*
    Handmade Geological Physics Implementation
    Tectonic plate simulation from first principles
    SIMD optimized, zero allocations in hot path
*/

#include "handmade_physics_multi.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// MEMORY HELPERS
// =============================================================================

#define arena_push_struct(arena, type) \
    (type*)arena_push_size(arena, sizeof(type), 16)
    
#define arena_push_array(arena, type, count) \
    (type*)arena_push_size(arena, sizeof(type) * (count), 16)

// =============================================================================
// GEOLOGICAL INITIALIZATION
// =============================================================================

internal void init_plate_mesh(tectonic_plate* plate, arena* arena, 
                             f32 center_lat, f32 center_lon, f32 radius) {
    // Create hexagonal mesh on sphere surface
    const u32 RINGS = 16;
    const u32 SEGMENTS = 32;
    
    plate->vertex_count = RINGS * SEGMENTS;
    plate->vertices = arena_push_array(arena, tectonic_vertex, plate->vertex_count);
    
    u32 v_idx = 0;
    for (u32 ring = 0; ring < RINGS; ring++) {
        f32 ring_radius = radius * sinf((f32)ring / RINGS * 3.14159f * 0.5f);
        f32 ring_height = radius * cosf((f32)ring / RINGS * 3.14159f * 0.5f);
        
        for (u32 seg = 0; seg < SEGMENTS; seg++) {
            f32 angle = (f32)seg / SEGMENTS * 2.0f * 3.14159f;
            
            tectonic_vertex* v = &plate->vertices[v_idx++];
            
            // Position on sphere
            v->position.x = ring_radius * cosf(angle + center_lon);
            v->position.y = ring_height + center_lat * radius;
            v->position.z = ring_radius * sinf(angle + center_lon);
            
            // Normalize to sphere surface
            f32 len = sqrtf(v->position.x * v->position.x + 
                          v->position.y * v->position.y + 
                          v->position.z * v->position.z);
            v->position.x *= EARTH_RADIUS_KM / len;
            v->position.y *= EARTH_RADIUS_KM / len;
            v->position.z *= EARTH_RADIUS_KM / len;
            
            // Initial properties
            v->velocity = (v3){0, 0, 0};
            v->elevation = (plate->type == PLATE_CONTINENTAL) ? 100.0f : -4000.0f;
            v->thickness = (plate->type == PLATE_CONTINENTAL) ? 35.0f : 7.0f;
            v->temperature = 300.0f;  // Kelvin
            v->pressure = 101325.0f;  // Pa (1 atm)
            v->stress_xx = 0;
            v->stress_yy = 0;
            v->stress_xy = 0;
        }
    }
    
    // Generate triangle indices
    plate->triangle_count = (RINGS - 1) * SEGMENTS * 2 * 3;
    plate->triangles = arena_push_array(arena, u32, plate->triangle_count);
    
    u32 tri_idx = 0;
    for (u32 ring = 0; ring < RINGS - 1; ring++) {
        for (u32 seg = 0; seg < SEGMENTS; seg++) {
            u32 current = ring * SEGMENTS + seg;
            u32 next = ring * SEGMENTS + ((seg + 1) % SEGMENTS);
            u32 below = (ring + 1) * SEGMENTS + seg;
            u32 below_next = (ring + 1) * SEGMENTS + ((seg + 1) % SEGMENTS);
            
            // First triangle
            plate->triangles[tri_idx++] = current;
            plate->triangles[tri_idx++] = below;
            plate->triangles[tri_idx++] = next;
            
            // Second triangle
            plate->triangles[tri_idx++] = next;
            plate->triangles[tri_idx++] = below;
            plate->triangles[tri_idx++] = below_next;
        }
    }
}

geological_state* geological_init(arena* arena, u32 seed) {
    geological_state* geo = arena_push_struct(arena, geological_state);
    
    // Initialize random seed for plate generation
    srand(seed);
    
    // Create major tectonic plates (simplified Earth-like configuration)
    geo->plate_count = 3;  // Start with just 3 plates for stability
    
    // Initialize all plates to zero first
    memset(geo->plates, 0, sizeof(geo->plates));
    
    // Pacific Plate (oceanic)
    geo->plates[0].type = PLATE_OCEANIC;
    geo->plates[0].density = 3000.0f;  // kg/m³
    geo->plates[0].age = 180.0f;       // Million years
    geo->plates[0].rotation_axis = (v3){0, 1, 0};
    geo->plates[0].angular_velocity = 0.0001f;
    init_plate_mesh(&geo->plates[0], arena, 0, 0, 3000);
    
    // North American Plate (continental)
    geo->plates[1].type = PLATE_CONTINENTAL;
    geo->plates[1].density = 2700.0f;
    geo->plates[1].age = 250.0f;
    geo->plates[1].rotation_axis = (v3){0.1f, 0.9f, 0.1f};
    geo->plates[1].angular_velocity = -0.00005f;
    init_plate_mesh(&geo->plates[1], arena, 0.5f, -1.5f, 2500);
    
    // Eurasian Plate (continental)
    geo->plates[2].type = PLATE_CONTINENTAL;
    geo->plates[2].density = 2700.0f;
    geo->plates[2].age = 300.0f;
    geo->plates[2].rotation_axis = (v3){-0.1f, 0.95f, 0.05f};
    geo->plates[2].angular_velocity = 0.00003f;
    init_plate_mesh(&geo->plates[2], arena, 0.7f, 0.5f, 2800);
    
    // Initialize mantle convection
    geo->mantle = arena_push_struct(arena, mantle_convection);
    geo->mantle->grid_size = 32;  // Reduced for testing
    
    u32 grid_total = geo->mantle->grid_size * geo->mantle->grid_size * geo->mantle->grid_size;
    geo->mantle->velocity_field = arena_push_array(arena, v3, grid_total);
    geo->mantle->temperature_field = arena_push_array(arena, f32, grid_total);
    geo->mantle->density_field = arena_push_array(arena, f32, grid_total);
    
    // Initialize mantle with convection cells
    for (u32 z = 0; z < geo->mantle->grid_size; z++) {
        for (u32 y = 0; y < geo->mantle->grid_size; y++) {
            for (u32 x = 0; x < geo->mantle->grid_size; x++) {
                u32 idx = z * geo->mantle->grid_size * geo->mantle->grid_size + 
                         y * geo->mantle->grid_size + x;
                
                // Temperature increases with depth
                f32 depth = (f32)y / geo->mantle->grid_size;
                geo->mantle->temperature_field[idx] = 300.0f + depth * 3000.0f;
                
                // Density varies with temperature (thermal expansion)
                geo->mantle->density_field[idx] = 3300.0f - depth * 50.0f;
                
                // Initial convection pattern (Rayleigh-Benard)
                f32 fx = (f32)x / geo->mantle->grid_size * 2.0f * 3.14159f;
                f32 fz = (f32)z / geo->mantle->grid_size * 2.0f * 3.14159f;
                geo->mantle->velocity_field[idx].x = sinf(fx) * cosf(fz) * 0.01f;
                geo->mantle->velocity_field[idx].y = cosf(fx) * cosf(fz) * 0.02f;
                geo->mantle->velocity_field[idx].z = sinf(fz) * 0.01f;
            }
        }
    }
    
    // Physics parameters
    geo->mantle->rayleigh_number = 1e6;  // Vigorous convection
    geo->mantle->prandtl_number = 1e23;  // Mantle rock
    geo->mantle->thermal_diffusivity = 1e-6;  // m²/s
    
    // Initial conditions
    geo->geological_time = 0.0;
    geo->dt = 0.001;  // 1000 year time step
    geo->sea_level = 0.0f;
    geo->global_temperature = 288.0f;  // 15°C
    
    printf("Geological simulation initialized with %u plates\n", geo->plate_count);
    return geo;
}

// =============================================================================
// MANTLE CONVECTION (DRIVES PLATE MOTION)
// =============================================================================

internal void update_mantle_convection(mantle_convection* mantle, f32 dt) {
    // Simplified Rayleigh-Benard convection using finite differences
    // This drives the plate tectonics from below
    
    u32 size = mantle->grid_size;
    f32 dx = 1.0f / size;
    
    // Update temperature field (heat equation with buoyancy)
    for (u32 z = 1; z < size - 1; z++) {
        for (u32 y = 1; y < size - 1; y++) {
            for (u32 x = 1; x < size - 1; x++) {
                u32 idx = z * size * size + y * size + x;
                
                // Laplacian of temperature (thermal diffusion)
                u32 xm = idx - 1;
                u32 xp = idx + 1;
                u32 ym = idx - size;
                u32 yp = idx + size;
                u32 zm = idx - size * size;
                u32 zp = idx + size * size;
                
                f32 laplacian = (mantle->temperature_field[xm] + 
                                mantle->temperature_field[xp] +
                                mantle->temperature_field[ym] + 
                                mantle->temperature_field[yp] +
                                mantle->temperature_field[zm] + 
                                mantle->temperature_field[zp] -
                                6.0f * mantle->temperature_field[idx]) / (dx * dx);
                
                // Update temperature
                mantle->temperature_field[idx] += dt * mantle->thermal_diffusivity * laplacian;
                
                // Update density based on temperature (thermal expansion)
                f32 T = mantle->temperature_field[idx];
                mantle->density_field[idx] = 3300.0f * (1.0f - 3e-5f * (T - 1600.0f));
            }
        }
    }
    
    // Update velocity field (Navier-Stokes with buoyancy)
    f32 Ra = mantle->rayleigh_number;
    f32 Pr = mantle->prandtl_number;
    
    for (u32 z = 1; z < size - 1; z++) {
        for (u32 y = 1; y < size - 1; y++) {
            for (u32 x = 1; x < size - 1; x++) {
                u32 idx = z * size * size + y * size + x;
                
                // Buoyancy force (vertical)
                f32 density_diff = mantle->density_field[idx] - 3300.0f;
                f32 buoyancy = -GRAVITY * density_diff / 3300.0f;
                
                // Update vertical velocity
                mantle->velocity_field[idx].y += dt * buoyancy * Ra * Pr;
                
                // Simple damping to maintain stability
                mantle->velocity_field[idx].x *= 0.99f;
                mantle->velocity_field[idx].y *= 0.99f;
                mantle->velocity_field[idx].z *= 0.99f;
            }
        }
    }
}

// =============================================================================
// PLATE TECTONICS PHYSICS
// =============================================================================

internal void calculate_plate_forces(tectonic_plate* plate, mantle_convection* mantle) {
    // Calculate forces on plate from mantle convection
    plate->mantle_force = (v3){0, 0, 0};
    
    // Sample mantle velocity beneath plate
    for (u32 i = 0; i < plate->vertex_count; i++) {
        v3 pos = plate->vertices[i].position;
        
        // Map position to mantle grid
        u32 mx = (u32)((pos.x / EARTH_RADIUS_KM + 1.0f) * 0.5f * mantle->grid_size);
        u32 my = 0;  // Surface of mantle
        u32 mz = (u32)((pos.z / EARTH_RADIUS_KM + 1.0f) * 0.5f * mantle->grid_size);
        
        if (mx < mantle->grid_size && mz < mantle->grid_size) {
            u32 idx = mz * mantle->grid_size * mantle->grid_size + 
                     my * mantle->grid_size + mx;
            
            // Drag force from mantle
            v3 mantle_vel = mantle->velocity_field[idx];
            v3 relative_vel = {
                mantle_vel.x - plate->vertices[i].velocity.x,
                0,  // Ignore vertical
                mantle_vel.z - plate->vertices[i].velocity.z
            };
            
            // Accumulate drag force
            f32 drag_coeff = 0.01f;
            plate->mantle_force.x += drag_coeff * relative_vel.x;
            plate->mantle_force.z += drag_coeff * relative_vel.z;
        }
    }
    
    // Average the force
    plate->mantle_force.x /= plate->vertex_count;
    plate->mantle_force.z /= plate->vertex_count;
}

internal void detect_plate_collisions(geological_state* geo) {
    // Check for collisions between plate boundaries
    for (u32 i = 0; i < geo->plate_count; i++) {
        for (u32 j = i + 1; j < geo->plate_count; j++) {
            tectonic_plate* plate_a = &geo->plates[i];
            tectonic_plate* plate_b = &geo->plates[j];
            
            // Simple distance check between plate centers
            f32 dx = plate_a->center_of_mass.x - plate_b->center_of_mass.x;
            f32 dy = plate_a->center_of_mass.y - plate_b->center_of_mass.y;
            f32 dz = plate_a->center_of_mass.z - plate_b->center_of_mass.z;
            f32 dist_sq = dx * dx + dy * dy + dz * dz;
            
            f32 collision_dist = 5000.0f;  // km
            if (dist_sq < collision_dist * collision_dist) {
                // Plates are colliding!
                
                // Determine collision type based on plate types
                if (plate_a->type == PLATE_OCEANIC && plate_b->type == PLATE_CONTINENTAL) {
                    // Oceanic subducts under continental
                    plate_a->subduction_force.y = -100.0f;  // Push down
                    
                    // Create volcanic activity along boundary
                    for (u32 v = 0; v < plate_b->vertex_count; v += 10) {
                        f32 vdx = plate_b->vertices[v].position.x - plate_a->center_of_mass.x;
                        f32 vdz = plate_b->vertices[v].position.z - plate_a->center_of_mass.z;
                        f32 vdist = sqrtf(vdx * vdx + vdz * vdz);
                        
                        if (vdist < 1000.0f) {  // Near collision zone
                            // Volcanic uplift
                            plate_b->vertices[v].elevation += 10.0f;
                            plate_b->vertices[v].temperature += 100.0f;
                        }
                    }
                } else if (plate_a->type == PLATE_CONTINENTAL && 
                          plate_b->type == PLATE_CONTINENTAL) {
                    // Continental collision - mountain building!
                    
                    // Calculate collision normal
                    v3 collision_normal = {dx / sqrtf(dist_sq), 0, dz / sqrtf(dist_sq)};
                    
                    // Apply compression to boundary vertices
                    for (u32 v = 0; v < plate_a->vertex_count; v++) {
                        f32 vdx = plate_a->vertices[v].position.x - plate_b->center_of_mass.x;
                        f32 vdz = plate_a->vertices[v].position.z - plate_b->center_of_mass.z;
                        f32 vdist = sqrtf(vdx * vdx + vdz * vdz);
                        
                        if (vdist < 1500.0f) {  // In collision zone
                            // Mountain building - controlled uplift with limits
                            f32 uplift = (1500.0f - vdist) / 1500.0f * 5.0f;  // Reduced from 50
                            
                            // Limit maximum elevation to prevent runaway growth
                            if (plate_a->vertices[v].elevation < 8000.0f) {
                                plate_a->vertices[v].elevation += uplift;
                            }
                            
                            // Increase crustal thickness with limits
                            if (plate_a->vertices[v].thickness < 70.0f) {
                                plate_a->vertices[v].thickness += uplift * 0.1f;  // Reduced from 0.5
                            }
                            
                            // Build up stress
                            plate_a->vertices[v].stress_xx += 1000.0f;
                            plate_a->vertices[v].stress_yy += 1000.0f;
                        }
                    }
                    
                    // Same for plate B
                    for (u32 v = 0; v < plate_b->vertex_count; v++) {
                        f32 vdx = plate_b->vertices[v].position.x - plate_a->center_of_mass.x;
                        f32 vdz = plate_b->vertices[v].position.z - plate_a->center_of_mass.z;
                        f32 vdist = sqrtf(vdx * vdx + vdz * vdz);
                        
                        if (vdist < 1500.0f) {
                            f32 uplift = (1500.0f - vdist) / 1500.0f * 5.0f;  // Reduced
                            
                            // Limit maximum elevation
                            if (plate_b->vertices[v].elevation < 8000.0f) {
                                plate_b->vertices[v].elevation += uplift;
                            }
                            
                            // Limit crustal thickness
                            if (plate_b->vertices[v].thickness < 70.0f) {
                                plate_b->vertices[v].thickness += uplift * 0.1f;
                            }
                            plate_b->vertices[v].stress_xx += 1000.0f;
                            plate_b->vertices[v].stress_yy += 1000.0f;
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// PLATE MOTION INTEGRATION
// =============================================================================

internal void update_plate_motion(tectonic_plate* plate, f32 dt) {
    // Update plate rotation based on forces
    f32 torque = plate->mantle_force.x * 0.001f;  // Simplified torque
    plate->angular_velocity += torque * dt;
    
    // Apply rotation to all vertices
    f32 angle = plate->angular_velocity * dt;
    f32 cos_a = cosf(angle);
    f32 sin_a = sinf(angle);
    
    // Update center of mass
    plate->center_of_mass = (v3){0, 0, 0};
    
    for (u32 i = 0; i < plate->vertex_count; i++) {
        tectonic_vertex* v = &plate->vertices[i];
        
        // Rotate around Y axis (simplified)
        f32 new_x = v->position.x * cos_a - v->position.z * sin_a;
        f32 new_z = v->position.x * sin_a + v->position.z * cos_a;
        
        v->position.x = new_x;
        v->position.z = new_z;
        
        // Update velocity based on rotation
        v->velocity.x = -plate->angular_velocity * v->position.z;
        v->velocity.z = plate->angular_velocity * v->position.x;
        
        // Accumulate for center of mass
        plate->center_of_mass.x += v->position.x;
        plate->center_of_mass.y += v->position.y;
        plate->center_of_mass.z += v->position.z;
    }
    
    // Finish center of mass calculation
    plate->center_of_mass.x /= plate->vertex_count;
    plate->center_of_mass.y /= plate->vertex_count;
    plate->center_of_mass.z /= plate->vertex_count;
    
    // Calculate statistics
    plate->average_elevation = 0;
    for (u32 i = 0; i < plate->vertex_count; i++) {
        plate->average_elevation += plate->vertices[i].elevation;
    }
    plate->average_elevation /= plate->vertex_count;
}

// =============================================================================
// MAIN GEOLOGICAL SIMULATION STEP
// =============================================================================

void geological_simulate(geological_state* geo, f64 dt_million_years) {
    // Convert to smaller time steps for stability
    const u32 SUBSTEPS = 100;
    f32 dt = (f32)(dt_million_years / SUBSTEPS);
    
    for (u32 substep = 0; substep < SUBSTEPS; substep++) {
        // Step 1: Update mantle convection
        update_mantle_convection(geo->mantle, dt);
        
        // Step 2: Calculate forces on each plate
        for (u32 i = 0; i < geo->plate_count; i++) {
            calculate_plate_forces(&geo->plates[i], geo->mantle);
        }
        
        // Step 3: Detect and resolve collisions
        detect_plate_collisions(geo);
        
        // Step 4: Update plate motion
        for (u32 i = 0; i < geo->plate_count; i++) {
            update_plate_motion(&geo->plates[i], dt);
        }
        
        // Step 5: Apply isostasy (buoyancy equilibrium)
        for (u32 i = 0; i < geo->plate_count; i++) {
            tectonic_plate* plate = &geo->plates[i];
            
            for (u32 v = 0; v < plate->vertex_count; v++) {
                tectonic_vertex* vertex = &plate->vertices[v];
                
                // Isostatic adjustment based on crustal thickness
                f32 excess_thickness = vertex->thickness - 35.0f;  // Continental average
                f32 isostatic_adjustment = excess_thickness * 0.1f;  // Simplified
                vertex->elevation -= isostatic_adjustment * dt;
                
                // Erosion (very simplified)
                if (vertex->elevation > 0) {
                    vertex->elevation -= 0.001f * dt * vertex->elevation;  // Higher = more erosion
                }
                
                // Stress relaxation
                vertex->stress_xx *= (1.0f - 0.01f * dt);
                vertex->stress_yy *= (1.0f - 0.01f * dt);
                vertex->stress_xy *= (1.0f - 0.01f * dt);
            }
        }
    }
    
    // Update geological time
    geo->geological_time += dt_million_years;
}

// =============================================================================
// HEIGHT FIELD EXPORT
// =============================================================================

void geological_export_heightmap(geological_state* geo, f32* heightmap, 
                                u32 width, u32 height, arena* temp_arena) {
    // Clear heightmap
    memset(heightmap, 0, width * height * sizeof(f32));
    
    // Project all plate vertices onto heightmap
    for (u32 p = 0; p < geo->plate_count; p++) {
        tectonic_plate* plate = &geo->plates[p];
        
        for (u32 v = 0; v < plate->vertex_count; v++) {
            tectonic_vertex* vertex = &plate->vertices[v];
            
            // Project sphere position to 2D map (equirectangular)
            f32 lon = atan2f(vertex->position.z, vertex->position.x);
            f32 lat = asinf(vertex->position.y / EARTH_RADIUS_KM);
            
            // Map to heightmap coordinates
            u32 x = (u32)((lon / 3.14159f + 1.0f) * 0.5f * width);
            u32 y = (u32)((lat / 1.5708f + 1.0f) * 0.5f * height);
            
            if (x < width && y < height) {
                u32 idx = y * width + x;
                // Take maximum elevation (handles overlapping plates)
                if (vertex->elevation > heightmap[idx]) {
                    heightmap[idx] = vertex->elevation;
                }
            }
        }
    }
    
    // Smooth the heightmap with a simple box filter
    // Save arena state to restore after temp allocation
    u64 saved_used = ((struct {u8* base; u64 size; u64 used;}*)temp_arena)->used;
    f32* temp = (f32*)arena_push_size(temp_arena, width * height * sizeof(f32), 16);
    memcpy(temp, heightmap, width * height * sizeof(f32));
    
    for (u32 y = 1; y < height - 1; y++) {
        for (u32 x = 1; x < width - 1; x++) {
            f32 sum = 0;
            for (s32 dy = -1; dy <= 1; dy++) {
                for (s32 dx = -1; dx <= 1; dx++) {
                    sum += temp[(y + dy) * width + (x + dx)];
                }
            }
            heightmap[y * width + x] = sum / 9.0f;
        }
    }
    
    // Restore arena state (pop temp allocation)
    ((struct {u8* base; u64 size; u64 used;}*)temp_arena)->used = saved_used;
}