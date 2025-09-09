/*
 * Extended Particle Effect Presets
 * Additional effects for various game scenarios
 */

#include "handmade_particles.h"

// Sparks effect for metal impacts, welding, electrical
emitter_config particles_preset_sparks(v3 position, v3 direction) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_CONE;
    cfg.position = position;
    cfg.direction = direction;
    cfg.spread_angle = 0.3f;
    
    cfg.emission_rate = 50.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 8.0f;
    cfg.start_speed_variance = 2.0f;
    cfg.start_size = 0.05f;
    cfg.start_size_variance = 0.02f;
    
    cfg.start_color = (color32){255, 255, 150, 255};
    cfg.end_color = (color32){255, 100, 0, 0};
    
    cfg.particle_lifetime = 0.3f;
    cfg.lifetime_variance = 0.1f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, -15.0f, 0};
    cfg.drag_coefficient = 0.5f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    
    return cfg;
}

// Rain effect for weather systems
emitter_config particles_preset_rain(v3 area_min, v3 area_max) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_BOX;
    cfg.box_min = area_min;
    cfg.box_max = area_max;
    cfg.direction = (v3){0, -1, 0};
    
    cfg.emission_rate = 200.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 10.0f;
    cfg.start_speed_variance = 2.0f;
    cfg.start_size = 0.02f;
    cfg.start_size_variance = 0.01f;
    
    cfg.start_color = (color32){100, 120, 200, 100};
    cfg.end_color = (color32){100, 120, 200, 0};
    
    cfg.particle_lifetime = 2.0f;
    cfg.lifetime_variance = 0.5f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, -20.0f, 0};
    cfg.drag_coefficient = 0.1f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}

// Magic effect for spells and enchantments
emitter_config particles_preset_magic(v3 position, color32 color) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_SPHERE;
    cfg.position = position;
    cfg.radius = 0.5f;
    
    cfg.emission_rate = 30.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.5f;
    cfg.start_speed_variance = 0.2f;
    cfg.start_size = 0.3f;
    cfg.start_size_variance = 0.1f;
    cfg.start_rotation = 0.0f;
    cfg.start_rotation_variance = 6.28f;
    
    cfg.start_color = color;
    color32 end = color;
    end.a = 0;
    cfg.end_color = end;
    
    cfg.particle_lifetime = 1.5f;
    cfg.lifetime_variance = 0.3f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, 1.0f, 0};  // Float upward
    cfg.drag_coefficient = 1.0f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    cfg.animated_texture = true;
    cfg.animation_frames = 8;
    cfg.animation_speed = 10.0f;
    
    return cfg;
}

// Blood effect for combat damage
emitter_config particles_preset_blood(v3 position, v3 direction) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_CONE;
    cfg.position = position;
    cfg.direction = direction;
    cfg.spread_angle = 0.5f;
    
    cfg.burst_count = 50;
    cfg.continuous = false;
    
    cfg.start_speed = 5.0f;
    cfg.start_speed_variance = 2.0f;
    cfg.start_size = 0.15f;
    cfg.start_size_variance = 0.05f;
    
    cfg.start_color = (color32){150, 0, 0, 255};
    cfg.end_color = (color32){80, 0, 0, 100};
    
    cfg.particle_lifetime = 1.0f;
    cfg.lifetime_variance = 0.2f;
    cfg.emitter_lifetime = 0.1f;
    
    cfg.gravity = (v3){0, -9.8f, 0};
    cfg.drag_coefficient = 2.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    cfg.enable_collision = true;
    
    return cfg;
}

// Dust effect for environmental atmosphere
emitter_config particles_preset_dust(v3 position, f32 radius) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_SPHERE;
    cfg.position = position;
    cfg.radius = radius;
    
    cfg.emission_rate = 20.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.2f;
    cfg.start_speed_variance = 0.1f;
    cfg.start_size = 0.5f;
    cfg.start_size_variance = 0.2f;
    
    cfg.start_color = (color32){180, 160, 140, 50};
    cfg.end_color = (color32){180, 160, 140, 0};
    
    cfg.particle_lifetime = 3.0f;
    cfg.lifetime_variance = 1.0f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, -0.5f, 0};
    cfg.drag_coefficient = 3.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}

// Water splash for liquid impacts
emitter_config particles_preset_water_splash(v3 position) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_CONE;
    cfg.position = position;
    cfg.direction = (v3){0, 1, 0};
    cfg.spread_angle = 0.8f;
    
    cfg.burst_count = 100;
    cfg.continuous = false;
    
    cfg.start_speed = 6.0f;
    cfg.start_speed_variance = 2.0f;
    cfg.start_size = 0.1f;
    cfg.start_size_variance = 0.05f;
    
    cfg.start_color = (color32){200, 220, 255, 150};
    cfg.end_color = (color32){200, 220, 255, 0};
    
    cfg.particle_lifetime = 0.8f;
    cfg.lifetime_variance = 0.2f;
    cfg.emitter_lifetime = 0.1f;
    
    cfg.gravity = (v3){0, -12.0f, 0};
    cfg.drag_coefficient = 1.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}

// Lightning/electricity effect
emitter_config particles_preset_lightning(v3 start, v3 end) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_LINE;
    cfg.position = start;
    cfg.box_max = end;  // Reuse box_max for line endpoint
    
    cfg.emission_rate = 200.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.1f;
    cfg.start_speed_variance = 0.05f;
    cfg.start_size = 0.2f;
    cfg.start_size_variance = 0.1f;
    
    cfg.start_color = (color32){200, 200, 255, 255};
    cfg.end_color = (color32){150, 150, 255, 0};
    
    cfg.particle_lifetime = 0.2f;
    cfg.lifetime_variance = 0.05f;
    cfg.emitter_lifetime = 0.5f;
    
    cfg.gravity = (v3){0, 0, 0};
    cfg.drag_coefficient = 0.0f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    
    return cfg;
}

// Portal/teleport effect
emitter_config particles_preset_portal(v3 position, color32 color) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_RING;
    cfg.position = position;
    cfg.radius = 1.0f;
    cfg.direction = (v3){0, 0, 1};
    
    cfg.emission_rate = 100.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.5f;
    cfg.start_speed_variance = 0.1f;
    cfg.start_size = 0.2f;
    cfg.start_size_variance = 0.05f;
    cfg.start_rotation = 0.0f;
    cfg.start_rotation_variance = 6.28f;
    
    cfg.start_color = color;
    color32 end = color;
    end.a = 0;
    cfg.end_color = end;
    
    cfg.particle_lifetime = 1.0f;
    cfg.lifetime_variance = 0.2f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, 0, 0};
    cfg.drag_coefficient = 0.5f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    cfg.animated_texture = true;
    cfg.animation_frames = 16;
    cfg.animation_speed = 20.0f;
    
    return cfg;
}

// Healing/buff effect
emitter_config particles_preset_healing(v3 position) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_SPHERE;
    cfg.position = position;
    cfg.radius = 0.3f;
    
    cfg.emission_rate = 25.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 1.0f;
    cfg.start_speed_variance = 0.2f;
    cfg.start_size = 0.15f;
    cfg.start_size_variance = 0.05f;
    
    cfg.start_color = (color32){100, 255, 100, 200};
    cfg.end_color = (color32){200, 255, 200, 0};
    
    cfg.particle_lifetime = 1.5f;
    cfg.lifetime_variance = 0.3f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, 2.0f, 0};  // Float upward
    cfg.drag_coefficient = 1.0f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    
    return cfg;
}

// Projectile trail effect
emitter_config particles_preset_projectile_trail(v3 position, color32 color) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_POINT;
    cfg.position = position;
    
    cfg.emission_rate = 100.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.5f;
    cfg.start_speed_variance = 0.2f;
    cfg.start_size = 0.1f;
    cfg.start_size_variance = 0.02f;
    
    cfg.start_color = color;
    color32 end = color;
    end.a = 0;
    cfg.end_color = end;
    
    cfg.particle_lifetime = 0.5f;
    cfg.lifetime_variance = 0.1f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, 0, 0};
    cfg.drag_coefficient = 2.0f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    cfg.world_space = false;  // Stay relative to emitter
    
    return cfg;
}

// Confetti celebration effect
emitter_config particles_preset_confetti(v3 position) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_CONE;
    cfg.position = position;
    cfg.direction = (v3){0, 1, 0};
    cfg.spread_angle = 0.5f;
    
    cfg.burst_count = 200;
    cfg.continuous = false;
    
    cfg.start_speed = 8.0f;
    cfg.start_speed_variance = 2.0f;
    cfg.start_size = 0.1f;
    cfg.start_size_variance = 0.05f;
    cfg.start_rotation = 0.0f;
    cfg.start_rotation_variance = 6.28f;
    
    // Random colors - would need to randomize per particle
    cfg.start_color = (color32){255, 200, 100, 255};
    cfg.end_color = (color32){255, 200, 100, 200};
    
    cfg.particle_lifetime = 3.0f;
    cfg.lifetime_variance = 0.5f;
    cfg.emitter_lifetime = 0.1f;
    
    cfg.gravity = (v3){0, -5.0f, 0};
    cfg.drag_coefficient = 2.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}

// Poison/toxic cloud effect
emitter_config particles_preset_poison_cloud(v3 position, f32 radius) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_SPHERE;
    cfg.position = position;
    cfg.radius = radius;
    
    cfg.emission_rate = 40.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.3f;
    cfg.start_speed_variance = 0.1f;
    cfg.start_size = 0.8f;
    cfg.start_size_variance = 0.2f;
    
    cfg.start_color = (color32){50, 150, 50, 100};
    cfg.end_color = (color32){30, 100, 30, 0};
    
    cfg.particle_lifetime = 2.0f;
    cfg.lifetime_variance = 0.5f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, 0.5f, 0};  // Slight rise
    cfg.drag_coefficient = 2.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}