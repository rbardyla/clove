/*
    Continental Architect - Rendering System
    
    Visualizes the complete multi-scale physics simulation:
    1. Geological terrain with elevation and stress patterns
    2. Hydrological systems with water flow and erosion
    3. Civilizations with population and status indicators
    4. Real-time performance overlays
    
    Optimized for 60+ FPS with millions of geological years simulated per second
    Uses immediate mode OpenGL for maximum handmade control
*/

#include "continental_architect_game.h"
#include <GL/gl.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// COLOR PALETTE FOR VISUALIZATION
// =============================================================================

typedef struct color {
    f32 r, g, b, a;
} color;

// Geological colors
const color ROCK_ANCIENT = {0.4f, 0.3f, 0.2f, 1.0f};    // Dark brown
const color ROCK_YOUNG = {0.6f, 0.5f, 0.4f, 1.0f};      // Light brown
const color MOUNTAIN_PEAK = {0.9f, 0.9f, 0.9f, 1.0f};   // White snow
const color VALLEY_DEEP = {0.2f, 0.4f, 0.2f, 1.0f};     // Dark green

// Water colors
const color WATER_SHALLOW = {0.4f, 0.7f, 1.0f, 0.7f};   // Light blue
const color WATER_DEEP = {0.1f, 0.3f, 0.8f, 0.9f};      // Dark blue
const color WATER_RAPID = {1.0f, 1.0f, 1.0f, 0.8f};     // White foam

// Civilization colors
const color CITY_THRIVING = {0.2f, 1.0f, 0.2f, 1.0f};   // Bright green
const color CITY_STRUGGLING = {1.0f, 0.8f, 0.2f, 1.0f}; // Yellow
const color CITY_DYING = {1.0f, 0.2f, 0.2f, 1.0f};      // Red
const color CITY_DEAD = {0.5f, 0.5f, 0.5f, 1.0f};       // Gray

// Stress visualization
const color STRESS_LOW = {0.0f, 1.0f, 0.0f, 0.5f};      // Green
const color STRESS_MEDIUM = {1.0f, 1.0f, 0.0f, 0.6f};   // Yellow
const color STRESS_HIGH = {1.0f, 0.0f, 0.0f, 0.8f};     // Red

// =============================================================================
// TERRAIN RENDERING
// =============================================================================

void render_geological_terrain(geological_state* geo, camera_state* camera) {
    if (!camera->show_geological_layers) return;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render each tectonic plate as a colored mesh
    for (u32 plate_idx = 0; plate_idx < geo->plate_count; plate_idx++) {
        tectonic_plate* plate = &geo->plates[plate_idx];
        
        // Choose color based on plate type and age
        color plate_color;
        if (plate->type == PLATE_CONTINENTAL) {
            f32 age_factor = plate->age / 100.0f;  // Normalize to 100 million years
            if (age_factor > 1.0f) age_factor = 1.0f;
            
            plate_color.r = ROCK_ANCIENT.r + (ROCK_YOUNG.r - ROCK_ANCIENT.r) * (1.0f - age_factor);
            plate_color.g = ROCK_ANCIENT.g + (ROCK_YOUNG.g - ROCK_ANCIENT.g) * (1.0f - age_factor);
            plate_color.b = ROCK_ANCIENT.b + (ROCK_YOUNG.b - ROCK_ANCIENT.b) * (1.0f - age_factor);
            plate_color.a = 1.0f;
        } else {
            // Oceanic plates are darker
            plate_color = (color){0.2f, 0.3f, 0.5f, 1.0f};
        }
        
        // Render plate mesh
        glBegin(GL_TRIANGLES);
        glColor4f(plate_color.r, plate_color.g, plate_color.b, plate_color.a);
        
        for (u32 tri_idx = 0; tri_idx < plate->triangle_count; tri_idx += 3) {
            // Get triangle vertices
            u32 v1_idx = plate->triangles[tri_idx];
            u32 v2_idx = plate->triangles[tri_idx + 1];
            u32 v3_idx = plate->triangles[tri_idx + 2];
            
            if (v1_idx >= plate->vertex_count || v2_idx >= plate->vertex_count || v3_idx >= plate->vertex_count) {
                continue;  // Skip invalid triangles
            }
            
            tectonic_vertex* v1 = &plate->vertices[v1_idx];
            tectonic_vertex* v2 = &plate->vertices[v2_idx];
            tectonic_vertex* v3 = &plate->vertices[v3_idx];
            
            // Color based on elevation
            f32 elevation_factor = (v1->elevation + 5000.0f) / 10000.0f;  // -5km to +5km
            if (elevation_factor < 0.0f) elevation_factor = 0.0f;
            if (elevation_factor > 1.0f) elevation_factor = 1.0f;
            
            // Mountains are lighter, valleys darker
            f32 brightness = 0.5f + elevation_factor * 0.5f;
            glColor4f(plate_color.r * brightness, plate_color.g * brightness, 
                     plate_color.b * brightness, plate_color.a);
            
            // Render triangle
            glVertex3f(v1->position.x, v1->elevation, v1->position.z);
            glVertex3f(v2->position.x, v2->elevation, v2->position.z);
            glVertex3f(v3->position.x, v3->elevation, v3->position.z);
        }
        glEnd();
    }
    
    // Render stress patterns if enabled
    if (camera->show_stress_patterns) {
        render_stress_visualization(geo);
    }
}

void render_stress_visualization(geological_state* geo) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    for (u32 plate_idx = 0; plate_idx < geo->plate_count; plate_idx++) {
        tectonic_plate* plate = &geo->plates[plate_idx];
        
        glBegin(GL_POINTS);
        glPointSize(3.0f);
        
        for (u32 vert_idx = 0; vert_idx < plate->vertex_count; vert_idx++) {
            tectonic_vertex* vertex = &plate->vertices[vert_idx];
            
            // Calculate total stress magnitude
            f32 stress_magnitude = sqrtf(vertex->stress_xx * vertex->stress_xx +
                                        vertex->stress_yy * vertex->stress_yy +
                                        vertex->stress_xy * vertex->stress_xy);
            
            // Normalize stress (adjust these values based on simulation)
            f32 stress_normalized = stress_magnitude / 1000000.0f;  // 1 MPa
            if (stress_normalized > 1.0f) stress_normalized = 1.0f;
            
            // Color based on stress level
            color stress_color;
            if (stress_normalized < 0.3f) {
                stress_color = STRESS_LOW;
            } else if (stress_normalized < 0.7f) {
                stress_color = STRESS_MEDIUM;
            } else {
                stress_color = STRESS_HIGH;
            }
            
            glColor4f(stress_color.r, stress_color.g, stress_color.b, 
                     stress_color.a * stress_normalized);
            glVertex3f(vertex->position.x, vertex->elevation + 100.0f, vertex->position.z);
        }
        glEnd();
    }
}

// =============================================================================
// WATER SYSTEM RENDERING
// =============================================================================

void render_hydrological_systems(fluid_state* fluid, camera_state* camera) {
    if (!camera->show_water_flow) return;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render water grid
    for (u32 z = 0; z < fluid->grid_z; z++) {
        for (u32 x = 0; x < fluid->grid_x; x++) {
            u32 index = z * fluid->grid_x + x;
            fluid_cell* cell = &fluid->grid[index];
            
            // Skip dry cells
            if (cell->density < 100.0f) continue;  // Less than water density
            
            // Calculate world position
            f32 world_x = (f32)x / fluid->grid_x * 10000.0f - 5000.0f;  // -5km to +5km
            f32 world_z = (f32)z / fluid->grid_z * 10000.0f - 5000.0f;
            
            // Calculate water depth (simplified)
            f32 water_depth = cell->density / WATER_DENSITY;
            if (water_depth < 0.1f) continue;  // Skip very shallow water
            
            // Color based on depth and flow speed
            f32 flow_speed = sqrtf(cell->velocity_x * cell->velocity_x +
                                  cell->velocity_y * cell->velocity_y +
                                  cell->velocity_z * cell->velocity_z);
            
            color water_color;
            if (water_depth < 1.0f) {
                water_color = WATER_SHALLOW;
            } else {
                water_color = WATER_DEEP;
            }
            
            // Add white foam for rapid flow
            if (flow_speed > 2.0f) {  // > 2 m/s
                f32 foam_factor = (flow_speed - 2.0f) / 5.0f;  // Normalize to 5 m/s max
                if (foam_factor > 1.0f) foam_factor = 1.0f;
                
                water_color.r += WATER_RAPID.r * foam_factor;
                water_color.g += WATER_RAPID.g * foam_factor;
                water_color.b += WATER_RAPID.b * foam_factor;
            }
            
            // Render water quad
            f32 cell_size = 10000.0f / fluid->grid_x;
            glBegin(GL_QUADS);
            glColor4f(water_color.r, water_color.g, water_color.b, water_color.a);
            
            glVertex3f(world_x, water_depth, world_z);
            glVertex3f(world_x + cell_size, water_depth, world_z);
            glVertex3f(world_x + cell_size, water_depth, world_z + cell_size);
            glVertex3f(world_x, water_depth, world_z + cell_size);
            glEnd();
        }
    }
    
    // Render sediment particles if zoom level is high enough
    if (camera->zoom_level < 1.0f && fluid->particle_count > 0) {
        render_sediment_particles(fluid, camera);
    }
}

void render_sediment_particles(fluid_state* fluid, camera_state* camera) {
    u32 particles_to_draw = fluid->particle_count;
    if (particles_to_draw > camera->max_particles_visible) {
        particles_to_draw = camera->max_particles_visible;
    }
    
    glBegin(GL_POINTS);
    glPointSize(1.0f);
    
    for (u32 i = 0; i < particles_to_draw; i++) {
        fluid_particle* particle = &fluid->particles[i];
        
        // Color based on sediment concentration
        f32 sediment_factor = particle->sediment_concentration;
        if (sediment_factor > 1.0f) sediment_factor = 1.0f;
        
        // Brown particles for sediment
        glColor4f(0.6f * sediment_factor, 0.4f * sediment_factor, 
                 0.2f * sediment_factor, 0.8f);
        
        glVertex3f(particle->position.x, particle->position.y, particle->position.z);
    }
    glEnd();
}

// =============================================================================
// CIVILIZATION RENDERING
// =============================================================================

void render_civilizations(game_state* game) {
    camera_state* camera = &game->camera;
    
    if (!camera->show_civilization_status) return;
    
    glDisable(GL_BLEND);
    
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        
        if (civ->population <= 0.0f) {
            // Render dead civilization marker
            glColor3f(CITY_DEAD.r, CITY_DEAD.g, CITY_DEAD.b);
            render_city_marker(civ->position, 50.0f);
            continue;
        }
        
        // Choose color based on population health
        color city_color;
        f32 population_health = civ->population / (civ->peak_population + 1.0f);
        
        if (population_health > 0.8f) {
            city_color = CITY_THRIVING;
        } else if (population_health > 0.4f) {
            city_color = CITY_STRUGGLING;
        } else {
            city_color = CITY_DYING;
        }
        
        glColor3f(city_color.r, city_color.g, city_color.b);
        
        // Size based on population
        f32 city_size = 100.0f + logf(civ->population + 1.0f) * 20.0f;
        render_city_marker(civ->position, city_size);
        
        // Render status indicators if zoomed in
        if (camera->zoom_level < 0.5f) {
            render_civilization_details(civ, camera);
        }
    }
}

void render_city_marker(v3 position, f32 size) {
    // Simple city representation as a pyramid
    glBegin(GL_TRIANGLES);
    
    // Base square (4 triangles)
    f32 half_size = size * 0.5f;
    
    // Front face
    glVertex3f(position.x - half_size, position.y, position.z - half_size);
    glVertex3f(position.x + half_size, position.y, position.z - half_size);
    glVertex3f(position.x, position.y + size, position.z);
    
    // Right face
    glVertex3f(position.x + half_size, position.y, position.z - half_size);
    glVertex3f(position.x + half_size, position.y, position.z + half_size);
    glVertex3f(position.x, position.y + size, position.z);
    
    // Back face
    glVertex3f(position.x + half_size, position.y, position.z + half_size);
    glVertex3f(position.x - half_size, position.y, position.z + half_size);
    glVertex3f(position.x, position.y + size, position.z);
    
    // Left face
    glVertex3f(position.x - half_size, position.y, position.z + half_size);
    glVertex3f(position.x - half_size, position.y, position.z - half_size);
    glVertex3f(position.x, position.y + size, position.z);
    
    glEnd();
}

void render_civilization_details(civilization* civ, camera_state* camera) {
    // Render resistance indicators as colored spheres around the city
    
    f32 indicator_radius = 20.0f;
    f32 sphere_size = 10.0f;
    
    // Earthquake resistance (red sphere)
    v3 eq_pos = {
        civ->position.x + indicator_radius,
        civ->position.y + 20.0f,
        civ->position.z
    };
    glColor3f(1.0f - civ->earthquake_resistance, civ->earthquake_resistance, 0.0f);
    render_sphere(eq_pos, sphere_size);
    
    // Flood resistance (blue sphere)
    v3 flood_pos = {
        civ->position.x - indicator_radius,
        civ->position.y + 20.0f,
        civ->position.z
    };
    glColor3f(1.0f - civ->flood_resistance, 1.0f - civ->flood_resistance, 
             0.5f + civ->flood_resistance * 0.5f);
    render_sphere(flood_pos, sphere_size);
    
    // Drought resistance (green sphere)
    v3 drought_pos = {
        civ->position.x,
        civ->position.y + 20.0f,
        civ->position.z + indicator_radius
    };
    glColor3f(1.0f - civ->drought_resistance, civ->drought_resistance, 
             1.0f - civ->drought_resistance);
    render_sphere(drought_pos, sphere_size);
}

void render_sphere(v3 center, f32 radius) {
    // Simple sphere approximation using octahedron subdivision
    const u32 subdivisions = 2;  // Keep it simple for performance
    
    glBegin(GL_TRIANGLES);
    
    // Just render as a simple octahedron for now
    v3 top = {center.x, center.y + radius, center.z};
    v3 bottom = {center.x, center.y - radius, center.z};
    v3 front = {center.x, center.y, center.z + radius};
    v3 back = {center.x, center.y, center.z - radius};
    v3 left = {center.x - radius, center.y, center.z};
    v3 right = {center.x + radius, center.y, center.z};
    
    // Top faces
    glVertex3f(top.x, top.y, top.z);
    glVertex3f(front.x, front.y, front.z);
    glVertex3f(right.x, right.y, right.z);
    
    glVertex3f(top.x, top.y, top.z);
    glVertex3f(right.x, right.y, right.z);
    glVertex3f(back.x, back.y, back.z);
    
    glVertex3f(top.x, top.y, top.z);
    glVertex3f(back.x, back.y, back.z);
    glVertex3f(left.x, left.y, left.z);
    
    glVertex3f(top.x, top.y, top.z);
    glVertex3f(left.x, left.y, left.z);
    glVertex3f(front.x, front.y, front.z);
    
    // Bottom faces
    glVertex3f(bottom.x, bottom.y, bottom.z);
    glVertex3f(right.x, right.y, right.z);
    glVertex3f(front.x, front.y, front.z);
    
    glVertex3f(bottom.x, bottom.y, bottom.z);
    glVertex3f(back.x, back.y, back.z);
    glVertex3f(right.x, right.y, right.z);
    
    glVertex3f(bottom.x, bottom.y, bottom.z);
    glVertex3f(left.x, left.y, left.z);
    glVertex3f(back.x, back.y, back.z);
    
    glVertex3f(bottom.x, bottom.y, bottom.z);
    glVertex3f(front.x, front.y, front.z);
    glVertex3f(left.x, left.y, left.z);
    
    glEnd();
}

// =============================================================================
// UI AND OVERLAY RENDERING
// =============================================================================

void render_game_ui(game_state* game) {
    // Set up 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1920, 1080, 0, -1, 1);  // Assuming 1920x1080 screen
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render tool selection UI
    render_tool_selection_ui(game);
    
    // Render time controls
    render_time_controls_ui(game);
    
    // Render civilization statistics
    render_civilization_stats_ui(game);
    
    // Render mode selection
    render_mode_selection_ui(game);
    
    if (game->show_performance_stats) {
        render_performance_overlay(game);
    }
    
    if (game->show_debug_overlay) {
        render_debug_overlays(game);
    }
    
    if (game->show_tutorial) {
        render_tutorial_overlay(game);
    }
    
    // Restore 3D rendering
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void render_tool_selection_ui(game_state* game) {
    // Tool selection panel in top-left corner
    f32 panel_x = 20.0f;
    f32 panel_y = 20.0f;
    f32 panel_width = 200.0f;
    f32 panel_height = 150.0f;
    
    // Panel background
    glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
    render_rect(panel_x, panel_y, panel_width, panel_height);
    
    // Tool buttons (simplified text representation)
    glColor3f(1.0f, 1.0f, 1.0f);
    
    const char* tool_names[] = {
        "Tectonic Push",
        "Tectonic Pull", 
        "Water Source",
        "Civilization",
        "Inspect",
        "Time Control"
    };
    
    for (u32 i = 0; i < 6; i++) {
        f32 button_y = panel_y + 20.0f + i * 20.0f;
        
        // Highlight selected tool
        if ((u32)game->input.selected_tool == i) {
            glColor3f(1.0f, 1.0f, 0.0f);  // Yellow highlight
        } else {
            glColor3f(1.0f, 1.0f, 1.0f);  // White
        }
        
        // Simplified text rendering (would use actual font rendering)
        render_text(panel_x + 10.0f, button_y, tool_names[i]);
    }
}

void render_time_controls_ui(game_state* game) {
    f32 panel_x = 20.0f;
    f32 panel_y = 200.0f;
    f32 panel_width = 200.0f;
    f32 panel_height = 100.0f;
    
    // Panel background
    glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
    render_rect(panel_x, panel_y, panel_width, panel_height);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Time scale display
    char time_scale_text[64];
    snprintf(time_scale_text, sizeof(time_scale_text), 
             "Time Scale: %.1fx", game->input.time_scale_multiplier);
    render_text(panel_x + 10.0f, panel_y + 20.0f, time_scale_text);
    
    // Geological time display
    char geo_time_text[64];
    snprintf(geo_time_text, sizeof(geo_time_text),
             "Geo Time: %llu My", 
             (unsigned long long)game->stats.total_geological_years_simulated);
    render_text(panel_x + 10.0f, panel_y + 40.0f, geo_time_text);
    
    // Game mode display
    const char* mode_names[] = {"Geological", "Hydrological", "Civilizations", "Disasters"};
    char mode_text[64];
    snprintf(mode_text, sizeof(mode_text), "Mode: %s", mode_names[game->current_mode]);
    render_text(panel_x + 10.0f, panel_y + 60.0f, mode_text);
}

void render_civilization_stats_ui(game_state* game) {
    f32 panel_x = 1920.0f - 220.0f;  // Right side of screen
    f32 panel_y = 20.0f;
    f32 panel_width = 200.0f;
    f32 panel_height = 200.0f;
    
    // Panel background
    glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
    render_rect(panel_x, panel_y, panel_width, panel_height);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Statistics
    char stats_text[256];
    
    snprintf(stats_text, sizeof(stats_text), "Civilizations: %u", game->civilization_count);
    render_text(panel_x + 10.0f, panel_y + 20.0f, stats_text);
    
    snprintf(stats_text, sizeof(stats_text), "Population: %.0f", game->total_population);
    render_text(panel_x + 10.0f, panel_y + 40.0f, stats_text);
    
    snprintf(stats_text, sizeof(stats_text), "Disasters: %u", game->disasters_survived);
    render_text(panel_x + 10.0f, panel_y + 60.0f, stats_text);
    
    snprintf(stats_text, sizeof(stats_text), "Stability: %.2f", game->geological_stability_score);
    render_text(panel_x + 10.0f, panel_y + 80.0f, stats_text);
    
    snprintf(stats_text, sizeof(stats_text), "Survival: %.1f years", game->civilization_survival_time);
    render_text(panel_x + 10.0f, panel_y + 100.0f, stats_text);
    
    snprintf(stats_text, sizeof(stats_text), "Created: %llu", 
             (unsigned long long)game->stats.total_civilizations_created);
    render_text(panel_x + 10.0f, panel_y + 120.0f, stats_text);
}

void render_mode_selection_ui(game_state* game) {
    f32 panel_x = (1920.0f - 400.0f) * 0.5f;  // Center of screen
    f32 panel_y = 20.0f;
    f32 panel_width = 400.0f;
    f32 panel_height = 60.0f;
    
    // Panel background
    glColor4f(0.2f, 0.2f, 0.2f, 0.8f);
    render_rect(panel_x, panel_y, panel_width, panel_height);
    
    // Mode buttons
    const char* mode_names[] = {"Geological", "Hydrological", "Civilizations", "Disasters"};
    f32 button_width = panel_width / 4.0f;
    
    for (u32 i = 0; i < 4; i++) {
        f32 button_x = panel_x + i * button_width;
        
        if ((u32)game->current_mode == i) {
            glColor4f(0.5f, 0.5f, 1.0f, 0.8f);  // Blue highlight
        } else {
            glColor4f(0.3f, 0.3f, 0.3f, 0.8f);  // Dark gray
        }
        
        render_rect(button_x + 2.0f, panel_y + 2.0f, button_width - 4.0f, panel_height - 4.0f);
        
        glColor3f(1.0f, 1.0f, 1.0f);
        render_text(button_x + 10.0f, panel_y + 30.0f, mode_names[i]);
    }
}

void render_performance_overlay(game_state* game) {
    f32 panel_x = 20.0f;
    f32 panel_y = 1080.0f - 160.0f;  // Bottom left
    f32 panel_width = 300.0f;
    f32 panel_height = 140.0f;
    
    // Panel background
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
    render_rect(panel_x, panel_y, panel_width, panel_height);
    
    glColor3f(0.0f, 1.0f, 0.0f);  // Green text
    
    char perf_text[128];
    
    snprintf(perf_text, sizeof(perf_text), "FPS: %u", game->frames_per_second);
    render_text(panel_x + 10.0f, panel_y + 20.0f, perf_text);
    
    snprintf(perf_text, sizeof(perf_text), "Frame: %.2f ms", game->frame_time_ms);
    render_text(panel_x + 10.0f, panel_y + 40.0f, perf_text);
    
    snprintf(perf_text, sizeof(perf_text), "Physics: %.2f ms", game->physics_time_ms);
    render_text(panel_x + 10.0f, panel_y + 60.0f, perf_text);
    
    snprintf(perf_text, sizeof(perf_text), "Render: %.2f ms", game->render_time_ms);
    render_text(panel_x + 10.0f, panel_y + 80.0f, perf_text);
    
    snprintf(perf_text, sizeof(perf_text), "Playtime: %.1f s", game->stats.total_playtime_seconds);
    render_text(panel_x + 10.0f, panel_y + 100.0f, perf_text);
}

void render_debug_overlays(game_state* game) {
    // Additional debug information would go here
    // For now, just show that debug mode is active
    glColor3f(1.0f, 0.0f, 0.0f);
    render_text(1920.0f - 200.0f, 1080.0f - 40.0f, "DEBUG MODE");
}

void render_tutorial_overlay(game_state* game) {
    f32 panel_x = (1920.0f - 600.0f) * 0.5f;  // Center
    f32 panel_y = 100.0f;
    f32 panel_width = 600.0f;
    f32 panel_height = 300.0f;
    
    // Semi-transparent background
    glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
    render_rect(panel_x, panel_y, panel_width, panel_height);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Tutorial text
    render_text(panel_x + 20.0f, panel_y + 30.0f, "CONTINENTAL ARCHITECT");
    render_text(panel_x + 20.0f, panel_y + 60.0f, "Shape continents across geological time");
    render_text(panel_x + 20.0f, panel_y + 90.0f, "Guide civilizations through disasters");
    render_text(panel_x + 20.0f, panel_y + 120.0f, "");
    render_text(panel_x + 20.0f, panel_y + 150.0f, "Mouse Wheel: Zoom in/out");
    render_text(panel_x + 20.0f, panel_y + 170.0f, "Left Click: Apply selected tool");
    render_text(panel_x + 20.0f, panel_y + 190.0f, "Right Click: Move camera");
    render_text(panel_x + 20.0f, panel_y + 210.0f, "1-4: Switch game modes");
    render_text(panel_x + 20.0f, panel_y + 230.0f, "");
    render_text(panel_x + 20.0f, panel_y + 250.0f, "Press T to close tutorial");
}

// =============================================================================
// UTILITY RENDERING FUNCTIONS
// =============================================================================

void render_rect(f32 x, f32 y, f32 width, f32 height) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void render_text(f32 x, f32 y, const char* text) {
    // This is a placeholder for text rendering
    // In a real implementation, you'd use a font rendering library
    // or implement bitmap font rendering
    
    // For now, just draw a small rectangle for each character
    f32 char_width = 8.0f;
    f32 char_height = 12.0f;
    
    glColor4f(1.0f, 1.0f, 1.0f, 0.3f);  // Very faint rectangles
    
    const char* c = text;
    f32 current_x = x;
    
    while (*c) {
        if (*c != ' ') {  // Don't draw spaces
            glBegin(GL_QUADS);
            glVertex2f(current_x, y);
            glVertex2f(current_x + char_width, y);
            glVertex2f(current_x + char_width, y + char_height);
            glVertex2f(current_x, y + char_height);
            glEnd();
        }
        
        current_x += char_width;
        c++;
    }
}