/*
    Test Program for Handmade Structural Physics
    Demonstrates building construction, earthquake simulation, and progressive collapse
    
    Tests:
    1. Frame building under seismic load
    2. Suspension bridge dynamic response  
    3. Progressive collapse simulation
    4. Material failure analysis
*/

#include "handmade_structural.c"
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

// Mock geological state for seismic testing
geological_state* create_mock_geological_state(arena* arena) {
    geological_state* geo = arena_push_struct(arena, geological_state);
    
    // Create a simple 2-plate system with high stress
    geo->plate_count = 2;
    
    // Plate 0: Continental plate
    tectonic_plate* plate0 = &geo->plates[0];
    plate0->type = PLATE_CONTINENTAL;
    plate0->vertex_count = 4;
    plate0->vertices = arena_push_array(arena, tectonic_vertex, 4);
    
    // Set up high-stress vertices (simulating earthquake zone)
    for (u32 i = 0; i < 4; i++) {
        tectonic_vertex* v = &plate0->vertices[i];
        v->position = (v3){i * 100.0f, 0, i * 100.0f};
        v->stress_xx = 50e6f;  // 50 MPa stress
        v->stress_yy = 30e6f;  // 30 MPa stress  
        v->stress_xy = 20e6f;  // 20 MPa shear stress
    }
    
    // Plate 1: Oceanic plate
    tectonic_plate* plate1 = &geo->plates[1];
    plate1->type = PLATE_OCEANIC;
    plate1->vertex_count = 4;
    plate1->vertices = arena_push_array(arena, tectonic_vertex, 4);
    
    for (u32 i = 0; i < 4; i++) {
        tectonic_vertex* v = &plate1->vertices[i];
        v->position = (v3){i * 100.0f + 200.0f, 0, i * 100.0f + 200.0f};
        v->stress_xx = 30e6f;
        v->stress_yy = 40e6f;
        v->stress_xy = 15e6f;
    }
    
    return geo;
}

void test_frame_building_seismic_response() {
    printf("=== TEST: Frame Building Seismic Response ===\n");
    
    // Create arena and structural system
    arena* arena = create_test_arena(MEGABYTES(100));
    structural_system* sys = structural_system_init(arena, 200, 100, 50, 20);
    
    // Create mock geological state
    geological_state* geo = create_mock_geological_state(arena);
    
    // Configure building
    building_config config = {
        .floors = 5,
        .floor_height = 3.5f,        // 3.5m per floor
        .span_x = 20.0f,             // 20m x 15m building
        .span_z = 15.0f,
        .bays_x = 4,                 // 4 bays in X, 3 bays in Z
        .bays_z = 3,
        .column_material = &STEEL,
        .beam_material = &STEEL
    };
    
    // Construct the building
    v3 building_origin = {0, 0, 0};
    construct_frame_building(sys, &config, building_origin);
    
    printf("Building constructed:\n");
    printf("  Nodes: %u\n", sys->node_count);
    printf("  Beams: %u\n", sys->beam_count);
    printf("  Expected nodes: %u\n", (config.bays_x + 1) * (config.bays_z + 1) * (config.floors + 1));
    printf("  Expected beams: %u\n", 
           (config.bays_x + 1) * (config.bays_z + 1) * config.floors + // columns
           config.bays_x * (config.bays_z + 1) * config.floors +      // X beams
           (config.bays_x + 1) * config.bays_z * config.floors);      // Z beams
    
    // Simulate earthquake for 30 seconds
    printf("\nSimulating earthquake (30 seconds)...\n");
    f32 dt = 0.001f; // 1ms time step
    u32 steps = 30000; // 30 seconds
    
    for (u32 step = 0; step < steps; step++) {
        structural_simulate(sys, geo, dt);
        
        // Print status every 5 seconds
        if (step % 5000 == 0) {
            f32 time = step * dt;
            printf("  Time: %.1fs, Max Stress: %.1f MPa, Damaged Elements: ", 
                   time, sys->stats.max_stress / 1e6f);
            
            u32 damaged = 0;
            for (u32 i = 0; i < sys->beam_count; i++) {
                if (sys->damage_factors[i] > 0.1f) damaged++;
            }
            printf("%u/%u\n", damaged, sys->beam_count);
        }
    }
    
    // Final analysis
    printf("\nFinal Analysis:\n");
    structural_debug_draw(sys);
    
    u32 failed_elements = 0;
    f32 max_damage = 0.0f;
    for (u32 i = 0; i < sys->beam_count; i++) {
        if (sys->damage_factors[i] >= 1.0f) failed_elements++;
        if (sys->damage_factors[i] > max_damage) max_damage = sys->damage_factors[i];
    }
    
    printf("Failed elements: %u/%u\n", failed_elements, sys->beam_count);
    printf("Maximum damage: %.1f%%\n", max_damage * 100.0f);
    
    if (failed_elements == 0) {
        printf("✓ Building survived earthquake\n");
    } else {
        printf("✗ Building experienced %u element failures\n", failed_elements);
    }
    
    destroy_test_arena(arena);
    printf("=== TEST COMPLETE ===\n\n");
}

void test_suspension_bridge_dynamics() {
    printf("=== TEST: Suspension Bridge Dynamics ===\n");
    
    arena* arena = create_test_arena(MEGABYTES(50));
    structural_system* sys = structural_system_init(arena, 100, 80, 20, 10);
    
    // Create suspension bridge
    v3 bridge_start = {-200.0f, 0.0f, 0.0f};
    v3 bridge_end = {200.0f, 0.0f, 0.0f};
    f32 tower_height = 80.0f;
    u32 deck_segments = 40;
    
    construct_suspension_bridge(sys, bridge_start, bridge_end, tower_height, 
                              deck_segments, &STEEL, &STEEL);
    
    printf("Bridge constructed:\n");
    printf("  Nodes: %u\n", sys->node_count);
    printf("  Beams: %u\n", sys->beam_count);
    printf("  Span: %.1fm\n", 400.0f);
    
    // Apply dynamic wind loading (simulate across bridge deck)
    for (u32 i = 0; i < sys->node_count; i++) {
        structural_node* node = &sys->nodes[i];
        
        // Apply lateral wind force to deck nodes (y ~ -20m)
        if (node->position.y < -15.0f) {
            node->applied_force.z = 1000.0f * sinf(0.5f * 3.14159f); // 1kN wind load
        }
    }
    
    // Simulate bridge dynamics for 60 seconds
    printf("\nSimulating bridge dynamics (60 seconds)...\n");
    f32 dt = 0.002f; // 2ms time step for bridge dynamics
    u32 steps = 30000; // 60 seconds
    
    f32 max_displacement = 0.0f;
    
    for (u32 step = 0; step < steps; step++) {
        structural_simulate(sys, NULL, dt); // No seismic, just wind
        
        // Track maximum displacement
        for (u32 i = 0; i < sys->node_count; i++) {
            v3 disp = sys->nodes[i].displacement;
            f32 disp_mag = sqrtf(disp.x*disp.x + disp.y*disp.y + disp.z*disp.z);
            if (disp_mag > max_displacement) {
                max_displacement = disp_mag;
            }
        }
        
        // Update wind loading (time-varying)
        if (step % 1000 == 0) {
            f32 time = step * dt;
            f32 wind_factor = sinf(time * 0.2f) + 0.5f * sinf(time * 0.7f);
            
            for (u32 i = 0; i < sys->node_count; i++) {
                structural_node* node = &sys->nodes[i];
                if (node->position.y < -15.0f) {
                    node->applied_force.z = 1000.0f * wind_factor;
                }
            }
            
            printf("  Time: %.1fs, Max Displacement: %.3fm, Wind Factor: %.2f\n", 
                   time, max_displacement, wind_factor);
        }
    }
    
    printf("\nFinal Bridge Analysis:\n");
    printf("Maximum displacement: %.3fm\n", max_displacement);
    
    // Check for cable failures
    u32 cable_failures = 0;
    for (u32 i = 0; i < sys->beam_count; i++) {
        beam_element* beam = &sys->beams[i];
        f32 damage = sys->damage_factors[i];
        
        // Cables have very small cross-sectional area
        if (beam->area < 0.01f && damage > 0.5f) {
            cable_failures++;
        }
    }
    
    if (cable_failures == 0 && max_displacement < 2.0f) {
        printf("✓ Bridge performed within acceptable limits\n");
    } else {
        printf("✗ Bridge exceeded design limits (displacement: %.3fm, cable failures: %u)\n", 
               max_displacement, cable_failures);
    }
    
    destroy_test_arena(arena);
    printf("=== TEST COMPLETE ===\n\n");
}

void test_progressive_collapse() {
    printf("=== TEST: Progressive Collapse Simulation ===\n");
    
    arena* arena = create_test_arena(MEGABYTES(50));
    structural_system* sys = structural_system_init(arena, 100, 60, 20, 10);
    
    // Create smaller building for collapse test
    building_config config = {
        .floors = 3,
        .floor_height = 3.0f,
        .span_x = 12.0f,
        .span_z = 12.0f,
        .bays_x = 2,
        .bays_z = 2,
        .column_material = &CONCRETE, // Use concrete (more brittle)
        .beam_material = &CONCRETE
    };
    
    construct_frame_building(sys, &config, (v3){0, 0, 0});
    
    printf("Building for collapse test:\n");
    printf("  Nodes: %u\n", sys->node_count);
    printf("  Beams: %u (concrete)\n", sys->beam_count);
    
    // Artificially damage critical column (simulate blast or extreme load)
    if (sys->beam_count > 0) {
        u32 critical_column = 0; // First column
        sys->damage_factors[critical_column] = 0.95f; // 95% damaged
        printf("Artificially damaged column %u to 95%%\n", critical_column);
    }
    
    // Apply extreme loading
    geological_state* severe_geo = create_mock_geological_state(arena);
    
    // Increase geological stress by 10x to trigger collapse
    for (u32 p = 0; p < severe_geo->plate_count; p++) {
        tectonic_plate* plate = &severe_geo->plates[p];
        for (u32 v = 0; v < plate->vertex_count; v++) {
            tectonic_vertex* vertex = &plate->vertices[v];
            vertex->stress_xx *= 10.0f;
            vertex->stress_yy *= 10.0f;
            vertex->stress_xy *= 10.0f;
        }
    }
    
    // Simulate collapse scenario
    printf("\nSimulating progressive collapse...\n");
    f32 dt = 0.001f;
    u32 steps = 10000; // 10 seconds
    
    u32 collapse_initiated = 0;
    
    for (u32 step = 0; step < steps; step++) {
        structural_simulate(sys, severe_geo, dt);
        simulate_progressive_collapse(sys);
        
        // Check for collapse initiation
        u32 failed_elements = 0;
        for (u32 i = 0; i < sys->beam_count; i++) {
            if (sys->damage_factors[i] >= 1.0f) failed_elements++;
        }
        
        if (failed_elements > 0 && !collapse_initiated) {
            collapse_initiated = 1;
            printf("  Collapse initiated at t=%.3fs with %u failed elements\n", 
                   step * dt, failed_elements);
        }
        
        // Print progress every second
        if (step % 1000 == 0) {
            f32 time = step * dt;
            printf("  Time: %.1fs, Failed: %u/%u, Max Stress: %.1f MPa\n", 
                   time, failed_elements, sys->beam_count, sys->stats.max_stress / 1e6f);
        }
        
        // Stop if complete collapse
        if (failed_elements > sys->beam_count / 2) {
            printf("  Complete collapse detected at t=%.3fs\n", step * dt);
            break;
        }
    }
    
    // Final collapse analysis
    printf("\nCollapse Analysis:\n");
    u32 final_failed = 0;
    u32 severely_damaged = 0;
    
    for (u32 i = 0; i < sys->beam_count; i++) {
        f32 damage = sys->damage_factors[i];
        if (damage >= 1.0f) final_failed++;
        else if (damage >= 0.5f) severely_damaged++;
    }
    
    printf("Final failed elements: %u/%u (%.1f%%)\n", 
           final_failed, sys->beam_count, 100.0f * final_failed / sys->beam_count);
    printf("Severely damaged elements: %u/%u (%.1f%%)\n", 
           severely_damaged, sys->beam_count, 100.0f * severely_damaged / sys->beam_count);
    
    if (collapse_initiated) {
        printf("✓ Progressive collapse successfully simulated\n");
    } else {
        printf("✗ Collapse not initiated (loads may be insufficient)\n");
    }
    
    destroy_test_arena(arena);
    printf("=== TEST COMPLETE ===\n\n");
}

int main() {
    printf("Handmade Structural Physics Test Suite\n");
    printf("=====================================\n\n");
    
    // Run all tests
    test_frame_building_seismic_response();
    test_suspension_bridge_dynamics();
    test_progressive_collapse();
    
    printf("=====================================\n");
    printf("All structural physics tests completed!\n");
    
    return 0;
}