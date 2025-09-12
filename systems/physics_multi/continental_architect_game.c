/*
    Continental Architect - Complete Game Implementation
    
    A god-game that showcases the complete MLPDD multi-scale physics system.
    Players shape continents across geological time and guide civilizations
    through the environmental challenges they create.
    
    Performance Target: 60+ FPS with full multi-scale physics simulation
    Memory Target: Arena-based, zero heap allocations in game loop
    Philosophy: Handmade, zero dependencies, SIMD optimized
*/

#include "continental_architect_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// Include OpenGL for rendering (minimal dependency)
#include <GL/gl.h>

// =============================================================================
// GAME INITIALIZATION
// =============================================================================

game_state* continental_architect_init(arena* arena) {
    game_state* game = (game_state*)arena_push_struct(arena, game_state);
    if (!game) return NULL;
    
    // Initialize memory arenas
    game->game_arena = arena;
    game->temp_arena = arena_sub_arena(arena, MEGABYTES(64));
    
    // Initialize core physics simulation
    game->physics = multi_physics_init(arena, (u32)time(NULL));
    if (!game->physics) {
        printf("Failed to initialize multi-physics system!\n");
        return NULL;
    }
    
    // Initialize camera with continental overview
    camera_state* camera = &game->camera;
    camera->position = (v3){0.0f, 50000.0f, 0.0f};  // 50km altitude
    camera->target = (v3){0.0f, 0.0f, 0.0f};
    camera->zoom_level = 1.0f;  // Continental scale
    camera->rotation_angle = 0.0f;
    
    // Default visualization settings
    camera->show_geological_layers = true;
    camera->show_water_flow = true;
    camera->show_stress_patterns = false;
    camera->show_civilization_status = true;
    camera->terrain_detail_level = 0.5f;
    camera->max_particles_visible = 50000;
    
    // Initialize player input
    player_input* input = &game->input;
    input->selected_tool = TOOL_INSPECT;
    input->tool_strength = 1.0f;
    input->tool_radius = 1000.0f;  // 1km radius
    input->time_scale_multiplier = 1.0f;
    input->pause_geological = false;
    input->pause_hydrological = false;
    input->pause_structural = false;
    
    // Start in geological mode
    game->current_mode = MODE_GEOLOGICAL;
    
    // Initialize civilizations array
    game->civilization_count = 0;
    memset(game->civilizations, 0, sizeof(game->civilizations));
    
    // Initialize scoring
    game->total_population = 0.0f;
    game->civilization_survival_time = 0.0f;
    game->disasters_survived = 0;
    game->geological_stability_score = 1.0f;
    
    // Initialize UI state
    game->show_debug_overlay = true;
    game->show_performance_stats = true;
    game->show_tutorial = true;
    game->ui_scale = 1.0f;
    
    // Initialize statistics
    game->stats.total_geological_years_simulated = 0;
    game->stats.total_civilizations_created = 0;
    game->stats.total_disasters_handled = 0;
    game->stats.total_playtime_seconds = 0.0;
    
    printf("Continental Architect initialized successfully!\n");
    printf("Multi-scale physics ready: Geological->Hydrological->Structural->Atmospheric\n");
    printf("Camera at %.0f km altitude, continental view\n", camera->position.y / 1000.0f);
    
    return game;
}

// =============================================================================
// MAIN GAME UPDATE LOOP
// =============================================================================

void continental_architect_update(game_state* game, f32 dt) {
    f64 frame_start = get_wall_clock();
    
    // Update playtime statistics
    game->stats.total_playtime_seconds += dt;
    
    // Process player input
    process_player_input(game, &game->input);
    
    // Update camera based on input
    update_camera(&game->camera, &game->input, dt);
    
    // Apply player tools to physics simulation
    apply_player_tools(game);
    
    // Update level-of-detail based on camera
    update_simulation_detail_levels(game);
    
    // Update physics simulation with time scaling
    f64 physics_start = get_wall_clock();
    
    f32 scaled_dt = dt * game->input.time_scale_multiplier;
    multi_physics_update(game->physics, scaled_dt);
    
    f64 physics_end = get_wall_clock();
    game->physics_time_ms = (physics_end - physics_start) * 1000.0;
    
    // Update geological statistics
    game->stats.total_geological_years_simulated += 
        (u64)(scaled_dt * GEOLOGICAL_TIME_SCALE);
    
    // Update civilizations based on environmental conditions
    update_civilizations(game, dt);
    
    // Handle disaster events
    handle_disasters(game, dt);
    
    // Update scoring metrics
    update_scoring(game, dt);
    
    // Calculate frame timing
    f64 frame_end = get_wall_clock();
    game->frame_time_ms = (frame_end - frame_start) * 1000.0;
    game->frames_per_second = (u32)(1.0 / dt);
    
    // Memory management - reset temp arena each frame
    arena_clear(game->temp_arena);
}

// =============================================================================
// CAMERA SYSTEM
// =============================================================================

void update_camera(camera_state* camera, player_input* input, f32 dt) {
    // Handle zoom with mouse wheel
    if (input->mouse_wheel_delta != 0.0f) {
        camera->zoom_level *= (1.0f + input->mouse_wheel_delta * 0.1f);
        
        // Clamp zoom levels
        if (camera->zoom_level < 0.001f) camera->zoom_level = 0.001f;  // City scale
        if (camera->zoom_level > 100.0f) camera->zoom_level = 100.0f;  // Global scale
        
        // Adjust altitude based on zoom
        f32 target_altitude = 1000.0f / camera->zoom_level;
        camera->position.y = lerp(camera->position.y, target_altitude, dt * 2.0f);
    }
    
    // Handle camera movement with right mouse drag
    if (input->right_mouse_down) {
        // This would be implemented with actual mouse delta input
        // For now, we'll use WASD-style movement
        f32 move_speed = 1000.0f / camera->zoom_level;  // Slower when zoomed in
        
        // Move camera target based on input
        // (This would connect to actual input system)
    }
    
    // Handle rotation
    if (input->left_mouse_down) {
        // Rotate camera around target
        // (This would connect to actual mouse delta)
    }
    
    // Smooth camera movement
    v3 desired_pos = v3_add(camera->target, 
                           v3_scale((v3){0, 1, 0}, camera->position.y));
    camera->position = v3_lerp(camera->position, desired_pos, dt * 2.0f);
    
    // Update detail levels based on zoom
    camera->terrain_detail_level = camera->zoom_level * 0.1f;
    if (camera->terrain_detail_level > 1.0f) camera->terrain_detail_level = 1.0f;
    
    // Adjust particle count based on performance
    u32 target_particles = (u32)(50000 * camera->terrain_detail_level);
    camera->max_particles_visible = target_particles;
}

// =============================================================================
// PLAYER INPUT PROCESSING
// =============================================================================

void process_player_input(game_state* game, player_input* input) {
    // Tool selection (would be connected to actual key input)
    // For now, cycle through tools based on game mode
    switch (game->current_mode) {
        case MODE_GEOLOGICAL:
            if (input->selected_tool != TOOL_TECTONIC_PUSH && 
                input->selected_tool != TOOL_TECTONIC_PULL) {
                input->selected_tool = TOOL_TECTONIC_PUSH;
            }
            break;
            
        case MODE_HYDROLOGICAL:
            if (input->selected_tool != TOOL_WATER_SOURCE) {
                input->selected_tool = TOOL_WATER_SOURCE;
            }
            break;
            
        case MODE_CIVILIZATIONS:
            if (input->selected_tool != TOOL_CIVILIZATION) {
                input->selected_tool = TOOL_CIVILIZATION;
            }
            break;
            
        case MODE_DISASTERS:
            input->selected_tool = TOOL_INSPECT;
            break;
    }
    
    // Time control adjustments
    if (game->current_mode == MODE_GEOLOGICAL) {
        input->time_scale_multiplier = 1000.0f;  // Fast geological time
    } else if (game->current_mode == MODE_HYDROLOGICAL) {
        input->time_scale_multiplier = 10.0f;    // Medium time scale
    } else {
        input->time_scale_multiplier = 1.0f;     // Real time
    }
}

// =============================================================================
// TOOL APPLICATION
// =============================================================================

void apply_player_tools(game_state* game) {
    player_input* input = &game->input;
    
    if (!input->left_mouse_down) return;
    
    switch (input->selected_tool) {
        case TOOL_TECTONIC_PUSH:
            apply_tectonic_push_tool(game->physics->geological, 
                                   input->mouse_world_pos,
                                   input->tool_strength,
                                   input->tool_radius);
            break;
            
        case TOOL_TECTONIC_PULL:
            apply_tectonic_pull_tool(game->physics->geological,
                                   input->mouse_world_pos,
                                   input->tool_strength,
                                   input->tool_radius);
            break;
            
        case TOOL_WATER_SOURCE:
            apply_water_source_tool(game->physics->fluid,
                                  input->mouse_world_pos,
                                  input->tool_strength);
            break;
            
        case TOOL_CIVILIZATION:
            place_civilization_tool(game, input->mouse_world_pos);
            break;
            
        case TOOL_INSPECT:
            // Show detailed information about the selected area
            break;
    }
}

void apply_tectonic_push_tool(geological_state* geo, v2 position, f32 strength, f32 radius) {
    // Apply upward force to tectonic plates in the area
    for (u32 plate_idx = 0; plate_idx < geo->plate_count; plate_idx++) {
        tectonic_plate* plate = &geo->plates[plate_idx];
        
        for (u32 vert_idx = 0; vert_idx < plate->vertex_count; vert_idx++) {
            tectonic_vertex* vertex = &plate->vertices[vert_idx];
            
            // Calculate distance from tool position
            f32 dx = vertex->position.x - position.x;
            f32 dz = vertex->position.z - position.y;
            f32 distance = sqrtf(dx*dx + dz*dz);
            
            if (distance < radius) {
                // Apply force based on distance (closer = stronger)
                f32 force_factor = (radius - distance) / radius;
                f32 force = strength * force_factor * 1000.0f;  // Scale to geological forces
                
                // Increase elevation and apply stress
                vertex->elevation += force * 0.001f;  // Convert to meters
                vertex->stress_xx += force * 0.1f;
                vertex->stress_yy += force * 0.1f;
                vertex->temperature += force * 0.01f;  // Heating from compression
            }
        }
    }
}

void apply_tectonic_pull_tool(geological_state* geo, v2 position, f32 strength, f32 radius) {
    // Apply downward/rifting force to create valleys
    apply_tectonic_push_tool(geo, position, -strength, radius);
}

void apply_water_source_tool(fluid_state* fluid, v2 position, f32 flow_rate) {
    // Add water source at the specified position
    u32 grid_x = (u32)((position.x + 5000.0f) / 10000.0f * fluid->grid_x);
    u32 grid_z = (u32)((position.y + 5000.0f) / 10000.0f * fluid->grid_z);
    
    if (grid_x < fluid->grid_x && grid_z < fluid->grid_z) {
        u32 index = grid_z * fluid->grid_x + grid_x;
        fluid_cell* cell = &fluid->grid[index];
        
        cell->is_source = 1;
        cell->precipitation_rate += flow_rate * 0.001f;  // mm/s
    }
}

void place_civilization_tool(game_state* game, v2 position) {
    if (game->civilization_count >= 64) return;  // Max civilizations
    
    civilization* civ = &game->civilizations[game->civilization_count++];
    
    // Initialize civilization at the specified position
    civ->position = (v3){position.x, 0.0f, position.y};
    civ->population = 1000.0f;  // Starting population
    civ->technology_level = 1.0f;
    civ->age_years = 0.0f;
    civ->peak_population = civ->population;
    civ->has_survived_disaster = false;
    
    // Calculate initial environmental factors
    civ->geological_stability = calculate_geological_stability(game->physics, position);
    civ->water_access = calculate_water_access(game->physics, position);
    civ->resource_access = calculate_resource_access(game->physics, position);
    
    // Set resistance based on starting conditions
    civ->earthquake_resistance = civ->geological_stability * 0.5f;
    civ->flood_resistance = (1.0f - civ->water_access) * 0.5f + 0.5f;
    civ->drought_resistance = civ->water_access * 0.8f;
    
    game->stats.total_civilizations_created++;
    
    printf("Civilization founded at (%.0f, %.0f) with %d population\n",
           position.x, position.y, (int)civ->population);
}

// =============================================================================
// CIVILIZATION MANAGEMENT
// =============================================================================

void update_civilizations(game_state* game, f32 dt) {
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        
        if (civ->population <= 0.0f) continue;  // Dead civilization
        
        // Age the civilization
        civ->age_years += dt;
        
        // Update environmental adaptation
        v2 civ_pos = {civ->position.x, civ->position.z};
        civilization_adapt_to_environment(civ, game->physics);
        
        // Population growth based on environmental factors
        f32 growth_rate = 0.02f;  // Base 2% annual growth
        growth_rate *= civ->geological_stability;
        growth_rate *= civ->water_access;
        growth_rate *= civ->resource_access;
        
        // Technology advancement
        civ->technology_level += dt * 0.1f * (civ->population / 10000.0f);
        
        // Apply growth
        civ->population *= (1.0f + growth_rate * dt);
        
        // Track peak population
        if (civ->population > civ->peak_population) {
            civ->peak_population = civ->population;
        }
        
        // Update total population
        game->total_population += civ->population;
    }
}

void civilization_adapt_to_environment(civilization* civ, multi_physics_state* physics) {
    v2 position = {civ->position.x, civ->position.z};
    
    // Recalculate environmental factors
    civ->geological_stability = calculate_geological_stability(physics, position);
    civ->water_access = calculate_water_access(physics, position);
    civ->resource_access = calculate_resource_access(physics, position);
    
    // Improve resistances based on technology level
    f32 tech_bonus = civ->technology_level * 0.1f;
    civ->earthquake_resistance = civ->geological_stability * 0.5f + tech_bonus;
    civ->flood_resistance = (1.0f - civ->water_access) * 0.5f + 0.5f + tech_bonus;
    civ->drought_resistance = civ->water_access * 0.8f + tech_bonus;
    
    // Clamp resistance values
    if (civ->earthquake_resistance > 1.0f) civ->earthquake_resistance = 1.0f;
    if (civ->flood_resistance > 1.0f) civ->flood_resistance = 1.0f;
    if (civ->drought_resistance > 1.0f) civ->drought_resistance = 1.0f;
}

f32 calculate_geological_stability(multi_physics_state* physics, v2 position) {
    // Sample stress levels from geological simulation
    f32 stress = multi_physics_get_rock_stress(physics, position.x, position.y);
    
    // Convert stress to stability (inverse relationship)
    f32 stability = 1.0f / (1.0f + stress * 0.0001f);
    
    return stability;
}

f32 calculate_water_access(multi_physics_state* physics, v2 position) {
    f32 water_depth = multi_physics_get_water_depth(physics, position.x, position.y);
    
    // Ideal water access is shallow water (not too deep, not dry)
    f32 ideal_depth = 2.0f;  // 2 meters
    f32 access = 1.0f - fabsf(water_depth - ideal_depth) / 10.0f;
    
    if (access < 0.0f) access = 0.0f;
    if (access > 1.0f) access = 1.0f;
    
    return access;
}

f32 calculate_resource_access(multi_physics_state* physics, v2 position) {
    f32 height = multi_physics_get_height(physics, position.x, position.y);
    
    // Resource access based on terrain variation (mountains have minerals)
    f32 terrain_variation = fabsf(height - 100.0f) / 1000.0f;  // Normalize to sea level
    f32 resource_access = terrain_variation * 0.5f + 0.5f;
    
    if (resource_access > 1.0f) resource_access = 1.0f;
    
    return resource_access;
}

// =============================================================================
// DISASTER SYSTEM
// =============================================================================

void handle_disasters(game_state* game, f32 dt) {
    // Check for earthquake threats
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        if (civ->population <= 0.0f) continue;
        
        v2 civ_pos = {civ->position.x, civ->position.z};
        
        // Earthquake detection
        if (detect_earthquake_threat(game->physics->geological, civ_pos, 5000.0f)) {
            f32 magnitude = 6.0f + (rand() % 3);  // 6.0 to 8.9 magnitude
            trigger_earthquake_event(game, civ_pos, magnitude);
        }
        
        // Flood detection
        if (detect_flood_threat(game->physics->fluid, civ_pos, 2000.0f)) {
            f32 intensity = 0.5f + (rand() % 100) / 100.0f;  // 0.5 to 1.5 intensity
            trigger_flood_event(game, civ_pos, intensity);
        }
    }
}

b32 detect_earthquake_threat(geological_state* geo, v2 position, f32 radius) {
    // Sample stress levels in the area
    f32 total_stress = 0.0f;
    u32 sample_count = 0;
    
    for (u32 plate_idx = 0; plate_idx < geo->plate_count; plate_idx++) {
        tectonic_plate* plate = &geo->plates[plate_idx];
        
        for (u32 vert_idx = 0; vert_idx < plate->vertex_count; vert_idx++) {
            tectonic_vertex* vertex = &plate->vertices[vert_idx];
            
            f32 dx = vertex->position.x - position.x;
            f32 dz = vertex->position.z - position.y;
            f32 distance = sqrtf(dx*dx + dz*dz);
            
            if (distance < radius) {
                f32 vertex_stress = sqrtf(vertex->stress_xx * vertex->stress_xx +
                                        vertex->stress_yy * vertex->stress_yy +
                                        vertex->stress_xy * vertex->stress_xy);
                total_stress += vertex_stress;
                sample_count++;
            }
        }
    }
    
    if (sample_count == 0) return false;
    
    f32 average_stress = total_stress / sample_count;
    f32 earthquake_threshold = 1000000.0f;  // Pa
    
    return average_stress > earthquake_threshold;
}

b32 detect_flood_threat(fluid_state* fluid, v2 position, f32 radius) {
    // Check for rapid water level changes in the area
    u32 grid_x = (u32)((position.x + 5000.0f) / 10000.0f * fluid->grid_x);
    u32 grid_z = (u32)((position.y + 5000.0f) / 10000.0f * fluid->grid_z);
    
    u32 check_radius = (u32)(radius / 10000.0f * fluid->grid_x);
    
    f32 total_precipitation = 0.0f;
    u32 sample_count = 0;
    
    for (u32 z = grid_z - check_radius; z <= grid_z + check_radius; z++) {
        for (u32 x = grid_x - check_radius; x <= grid_x + check_radius; x++) {
            if (x < fluid->grid_x && z < fluid->grid_z) {
                u32 index = z * fluid->grid_x + x;
                total_precipitation += fluid->grid[index].precipitation_rate;
                sample_count++;
            }
        }
    }
    
    if (sample_count == 0) return false;
    
    f32 average_precipitation = total_precipitation / sample_count;
    f32 flood_threshold = 0.01f;  // 10mm/s
    
    return average_precipitation > flood_threshold;
}

void trigger_earthquake_event(game_state* game, v2 epicenter, f32 magnitude) {
    printf("EARTHQUAKE! Magnitude %.1f at (%.0f, %.0f)\n", 
           magnitude, epicenter.x, epicenter.y);
    
    f32 damage_radius = magnitude * 1000.0f;  // km
    f32 max_damage = (magnitude - 5.0f) / 4.0f;  // Scale 5.0-9.0 to 0.0-1.0
    
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        if (civ->population <= 0.0f) continue;
        
        v2 civ_pos = {civ->position.x, civ->position.z};
        f32 dx = civ_pos.x - epicenter.x;
        f32 dy = civ_pos.y - epicenter.y;
        f32 distance = sqrtf(dx*dx + dy*dy);
        
        if (distance < damage_radius) {
            f32 damage_factor = (damage_radius - distance) / damage_radius;
            f32 damage = max_damage * damage_factor;
            
            // Apply damage based on earthquake resistance
            f32 actual_damage = damage * (1.0f - civ->earthquake_resistance);
            civ->population *= (1.0f - actual_damage);
            
            if (civ->population > 0) {
                civ->has_survived_disaster = true;
                game->disasters_survived++;
            }
            
            printf("  Civilization at (%.0f, %.0f): %.1f%% damage, %.0f survivors\n",
                   civ_pos.x, civ_pos.y, actual_damage * 100.0f, civ->population);
        }
    }
    
    game->stats.total_disasters_handled++;
}

void trigger_flood_event(game_state* game, v2 origin, f32 intensity) {
    printf("FLOOD! Intensity %.1f at (%.0f, %.0f)\n", 
           intensity, origin.x, origin.y);
    
    // Similar to earthquake but with different damage pattern
    f32 flood_radius = intensity * 2000.0f;  // Floods spread further
    
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        if (civ->population <= 0.0f) continue;
        
        v2 civ_pos = {civ->position.x, civ->position.z};
        f32 dx = civ_pos.x - origin.x;
        f32 dy = civ_pos.y - origin.y;
        f32 distance = sqrtf(dx*dx + dy*dy);
        
        if (distance < flood_radius) {
            f32 damage_factor = (flood_radius - distance) / flood_radius;
            f32 damage = intensity * 0.3f * damage_factor;  // Floods less deadly than earthquakes
            
            f32 actual_damage = damage * (1.0f - civ->flood_resistance);
            civ->population *= (1.0f - actual_damage);
            
            if (civ->population > 0) {
                civ->has_survived_disaster = true;
            }
        }
    }
    
    game->stats.total_disasters_handled++;
}

// =============================================================================
// SCORING SYSTEM
// =============================================================================

void update_scoring(game_state* game, f32 dt) {
    // Update total population
    game->total_population = 0.0f;
    f32 total_survival_time = 0.0f;
    
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        game->total_population += civ->population;
        total_survival_time += civ->age_years;
    }
    
    game->civilization_survival_time = total_survival_time;
    
    // Calculate geological stability score (average across all plates)
    if (game->physics->geological->plate_count > 0) {
        f32 total_stability = 0.0f;
        for (u32 i = 0; i < game->physics->geological->plate_count; i++) {
            // Sample stability across the continent
            v2 sample_pos = {0.0f, 0.0f};  // Would sample multiple points
            total_stability += calculate_geological_stability(game->physics, sample_pos);
        }
        game->geological_stability_score = total_stability / game->physics->geological->plate_count;
    }
}

// =============================================================================
// LEVEL OF DETAIL OPTIMIZATION
// =============================================================================

void update_simulation_detail_levels(game_state* game) {
    camera_state* camera = &game->camera;
    
    // Adjust physics simulation detail based on zoom level
    if (camera->zoom_level > 10.0f) {
        // Continental view - reduce structural detail
        optimize_physics_regions(game->physics, camera);
    } else if (camera->zoom_level > 1.0f) {
        // Regional view - balance all systems
        // Keep moderate detail
    } else {
        // Local view - maximize structural detail
        // Enable all systems at full detail
    }
}

void optimize_physics_regions(multi_physics_state* physics, camera_state* camera) {
    // This would implement spatial LOD optimization
    // For now, we'll just document the approach:
    
    // 1. Divide world into regions based on camera frustum
    // 2. Update only regions visible to camera at full detail
    // 3. Update distant regions at reduced frequency
    // 4. Use simplified physics for very distant regions
    
    // Performance target: Maintain 60+ FPS regardless of zoom level
}

// Utility functions are implemented in the demo file