/*
 * Handmade Particle System Implementation
 * Zero-allocation, SIMD-optimized particle simulation
 */

#include "handmade_particles.h"
#include <string.h>
#include <math.h>
#include <immintrin.h>  // For SIMD
#include <stdio.h>

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

#define ALIGN_TO(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))

static void* arena_alloc(particle_system* system, u64 size) {
    size = ALIGN_TO(size, 16);  // 16-byte alignment for SIMD
    
    if (system->memory_used + size > system->memory_size) {
        printf("Particle system: Out of memory!\n");
        return NULL;
    }
    
    void* ptr = (u8*)system->memory + system->memory_used;
    system->memory_used += size;
    return ptr;
}

// ============================================================================
// SYSTEM LIFECYCLE
// ============================================================================

particle_system* particles_init(void* memory, u64 memory_size) {
    if (!memory || memory_size < sizeof(particle_system)) {
        return NULL;
    }
    
    particle_system* system = (particle_system*)memory;
    memset(system, 0, sizeof(particle_system));
    
    system->memory = memory;
    system->memory_size = memory_size;
    system->memory_used = sizeof(particle_system);
    
    // Allocate particle arrays (Structure of Arrays)
    u64 particle_array_size = PARTICLE_MAX_TOTAL * sizeof(f32);
    
    system->particles.position_x = (f32*)arena_alloc(system, particle_array_size);
    system->particles.position_y = (f32*)arena_alloc(system, particle_array_size);
    system->particles.position_z = (f32*)arena_alloc(system, particle_array_size);
    system->particles.velocity_x = (f32*)arena_alloc(system, particle_array_size);
    system->particles.velocity_y = (f32*)arena_alloc(system, particle_array_size);
    system->particles.velocity_z = (f32*)arena_alloc(system, particle_array_size);
    
    system->particles.size = (f32*)arena_alloc(system, particle_array_size);
    system->particles.rotation = (f32*)arena_alloc(system, particle_array_size);
    system->particles.opacity = (f32*)arena_alloc(system, particle_array_size);
    system->particles.color = (u32*)arena_alloc(system, PARTICLE_MAX_TOTAL * sizeof(u32));
    
    system->particles.age = (f32*)arena_alloc(system, particle_array_size);
    system->particles.max_age = (f32*)arena_alloc(system, particle_array_size);
    
    system->particles.mass = (f32*)arena_alloc(system, particle_array_size);
    system->particles.drag = (f32*)arena_alloc(system, particle_array_size);
    
    system->particles.texture_id = (u32*)arena_alloc(system, PARTICLE_MAX_TOTAL * sizeof(u32));
    system->particles.flags = (u32*)arena_alloc(system, PARTICLE_MAX_TOTAL * sizeof(u32));
    
    system->particles.capacity = PARTICLE_MAX_TOTAL;
    
    // Check allocations
    if (!system->particles.position_x || !system->particles.rotation) {
        printf("Failed to allocate particle arrays!\n");
        return NULL;
    }
    
    // Allocate emitters
    system->emitter_capacity = PARTICLE_MAX_EMITTERS;
    system->emitters = (particle_emitter*)arena_alloc(system, 
        system->emitter_capacity * sizeof(particle_emitter));
    
    // Initialize spatial hash
    system->spatial_hash.grid_size = 256;
    system->spatial_hash.cell_size = 1.0f;
    u32 hash_size = system->spatial_hash.grid_size * system->spatial_hash.grid_size;
    system->spatial_hash.cell_starts = (u32*)arena_alloc(system, hash_size * sizeof(u32));
    system->spatial_hash.cell_ends = (u32*)arena_alloc(system, hash_size * sizeof(u32));
    system->spatial_hash.particle_indices = (u32*)arena_alloc(system, PARTICLE_MAX_TOTAL * sizeof(u32));
    
    // Default configuration
    system->use_simd = true;
    system->fixed_timestep = 1.0f / 60.0f;
    
    printf("Particle system initialized: %.2f MB used\n", 
           (f32)system->memory_used / (1024.0f * 1024.0f));
    
    return system;
}

void particles_shutdown(particle_system* system) {
    if (system) {
        // Nothing to free - arena allocated
        system->memory_used = 0;
    }
}

void particles_reset(particle_system* system) {
    system->particles.count = 0;
    system->emitter_count = 0;
    system->force_field_count = 0;
    memset(&system->stats, 0, sizeof(system->stats));
}

// ============================================================================
// EMITTER MANAGEMENT
// ============================================================================

emitter_id particles_create_emitter(particle_system* system, const emitter_config* config) {
    if (system->emitter_count >= system->emitter_capacity) {
        printf("Particle system: Max emitters reached!\n");
        return 0;
    }
    
    emitter_id id = system->emitter_count + 1;  // IDs start at 1
    particle_emitter* emitter = &system->emitters[system->emitter_count++];
    
    memset(emitter, 0, sizeof(particle_emitter));
    emitter->id = id;
    emitter->config = *config;
    emitter->world_position = config->position;
    emitter->is_active = true;
    
    // Allocate particle pool space
    emitter->particle_capacity = PARTICLE_MAX_PER_EMITTER;
    emitter->particle_start = system->particles.count;
    
    return id;
}

void particles_destroy_emitter(particle_system* system, emitter_id id) {
    for (u32 i = 0; i < system->emitter_count; i++) {
        if (system->emitters[i].id == id) {
            // Kill all particles from this emitter
            u32 start = system->emitters[i].particle_start;
            u32 count = system->emitters[i].particle_count;
            
            // Shift particles down
            if (start + count < system->particles.count) {
                u32 move_count = system->particles.count - (start + count);
                memmove(&system->particles.position_x[start], 
                       &system->particles.position_x[start + count],
                       move_count * sizeof(f32));
                // ... repeat for all arrays
            }
            
            system->particles.count -= count;
            
            // Remove emitter
            if (i < system->emitter_count - 1) {
                memmove(&system->emitters[i], &system->emitters[i + 1],
                       (system->emitter_count - i - 1) * sizeof(particle_emitter));
            }
            system->emitter_count--;
            break;
        }
    }
}

particle_emitter* particles_get_emitter(particle_system* system, emitter_id id) {
    for (u32 i = 0; i < system->emitter_count; i++) {
        if (system->emitters[i].id == id) {
            return &system->emitters[i];
        }
    }
    return NULL;
}

// ============================================================================
// EMITTER CONTROL
// ============================================================================

void particles_play_emitter(particle_system* system, emitter_id id) {
    particle_emitter* emitter = particles_get_emitter(system, id);
    if (emitter) {
        emitter->is_active = true;
        emitter->is_paused = false;
    }
}

void particles_stop_emitter(particle_system* system, emitter_id id) {
    particle_emitter* emitter = particles_get_emitter(system, id);
    if (emitter) {
        emitter->is_active = false;
    }
}

void particles_burst_emitter(particle_system* system, emitter_id id, u32 count) {
    particle_emitter* emitter = particles_get_emitter(system, id);
    if (!emitter) return;
    
    // Spawn burst of particles
    for (u32 i = 0; i < count && system->particles.count < system->particles.capacity; i++) {
        u32 idx = system->particles.count++;
        
        // Initialize particle based on emitter config
        emitter_config* cfg = &emitter->config;
        
        // Position
        switch (cfg->shape) {
            case EMISSION_POINT:
                system->particles.position_x[idx] = cfg->position.x;
                system->particles.position_y[idx] = cfg->position.y;
                system->particles.position_z[idx] = cfg->position.z;
                break;
                
            case EMISSION_SPHERE: {
                // Random point in sphere
                f32 theta = ((f32)rand() / RAND_MAX) * 2.0f * 3.14159f;
                f32 phi = ((f32)rand() / RAND_MAX) * 3.14159f;
                f32 r = ((f32)rand() / RAND_MAX) * cfg->radius;
                
                system->particles.position_x[idx] = cfg->position.x + r * sinf(phi) * cosf(theta);
                system->particles.position_y[idx] = cfg->position.y + r * sinf(phi) * sinf(theta);
                system->particles.position_z[idx] = cfg->position.z + r * cosf(phi);
                break;
            }
                
            case EMISSION_BOX: {
                // Random point in box
                system->particles.position_x[idx] = cfg->box_min.x + 
                    ((f32)rand() / RAND_MAX) * (cfg->box_max.x - cfg->box_min.x);
                system->particles.position_y[idx] = cfg->box_min.y + 
                    ((f32)rand() / RAND_MAX) * (cfg->box_max.y - cfg->box_min.y);
                system->particles.position_z[idx] = cfg->box_min.z + 
                    ((f32)rand() / RAND_MAX) * (cfg->box_max.z - cfg->box_min.z);
                break;
            }
            
            default:
                system->particles.position_x[idx] = cfg->position.x;
                system->particles.position_y[idx] = cfg->position.y;
                system->particles.position_z[idx] = cfg->position.z;
                break;
        }
        
        // Velocity
        f32 speed = cfg->start_speed + 
            ((f32)rand() / RAND_MAX - 0.5f) * cfg->start_speed_variance;
        
        system->particles.velocity_x[idx] = cfg->direction.x * speed;
        system->particles.velocity_y[idx] = cfg->direction.y * speed;
        system->particles.velocity_z[idx] = cfg->direction.z * speed;
        
        // Add spread
        if (cfg->spread_angle > 0) {
            f32 angle = ((f32)rand() / RAND_MAX - 0.5f) * cfg->spread_angle;
            // Apply rotation to velocity vector (simplified)
            f32 vx = system->particles.velocity_x[idx];
            f32 vy = system->particles.velocity_y[idx];
            system->particles.velocity_x[idx] = vx * cosf(angle) - vy * sinf(angle);
            system->particles.velocity_y[idx] = vx * sinf(angle) + vy * cosf(angle);
        }
        
        // Properties
        system->particles.size[idx] = cfg->start_size + 
            ((f32)rand() / RAND_MAX - 0.5f) * cfg->start_size_variance;
        system->particles.rotation[idx] = cfg->start_rotation + 
            ((f32)rand() / RAND_MAX - 0.5f) * cfg->start_rotation_variance;
        system->particles.opacity[idx] = 1.0f;
        
        // Color
        system->particles.color[idx] = (cfg->start_color.r << 24) | 
                                       (cfg->start_color.g << 16) |
                                       (cfg->start_color.b << 8) | 
                                       cfg->start_color.a;
        
        // Lifetime
        system->particles.age[idx] = 0.0f;
        system->particles.max_age[idx] = cfg->particle_lifetime + 
            ((f32)rand() / RAND_MAX - 0.5f) * cfg->lifetime_variance;
        
        // Physics
        system->particles.mass[idx] = 1.0f;
        system->particles.drag[idx] = cfg->drag_coefficient;
        
        // Metadata
        system->particles.texture_id[idx] = cfg->texture_id;
        system->particles.flags[idx] = 0;
        
        emitter->particle_count++;
        system->stats.particles_spawned++;
    }
}

// ============================================================================
// PARTICLE UPDATE - SCALAR VERSION
// ============================================================================

static void update_particle_scalar(particle_system* system, u32 idx, f32 dt) {
    // Age particle
    system->particles.age[idx] += dt;
    
    // Kill if too old
    if (system->particles.age[idx] >= system->particles.max_age[idx]) {
        system->particles.flags[idx] |= 0x1;  // Mark as dead
        system->stats.particles_killed++;
        return;
    }
    
    // Apply forces
    f32 vx = system->particles.velocity_x[idx];
    f32 vy = system->particles.velocity_y[idx];
    f32 vz = system->particles.velocity_z[idx];
    
    // Gravity (simplified - should come from emitter config)
    vy -= 9.8f * dt;
    
    // Drag
    f32 drag = system->particles.drag[idx];
    vx *= (1.0f - drag * dt);
    vy *= (1.0f - drag * dt);
    vz *= (1.0f - drag * dt);
    
    // Update velocity
    system->particles.velocity_x[idx] = vx;
    system->particles.velocity_y[idx] = vy;
    system->particles.velocity_z[idx] = vz;
    
    // Update position
    system->particles.position_x[idx] += vx * dt;
    system->particles.position_y[idx] += vy * dt;
    system->particles.position_z[idx] += vz * dt;
    
    // Update visual properties
    f32 life_ratio = system->particles.age[idx] / system->particles.max_age[idx];
    
    // Fade out
    system->particles.opacity[idx] = 1.0f - life_ratio;
    
    // Rotate
    system->particles.rotation[idx] += 90.0f * dt;  // degrees per second
}

// ============================================================================
// PARTICLE UPDATE - SIMD VERSION
// ============================================================================

void particles_update_simd(particle_system* system, f32 delta_time) {
    u32 count = system->particles.count;
    if (count == 0) return;
    
    // Process 8 particles at a time using AVX2
    u32 simd_count = (count / 8) * 8;
    
    __m256 dt = _mm256_set1_ps(delta_time);
    __m256 gravity = _mm256_set1_ps(-9.8f * delta_time);
    __m256 one = _mm256_set1_ps(1.0f);
    
    for (u32 i = 0; i < simd_count; i += 8) {
        // Load particle data (use unaligned loads for safety)
        __m256 px = _mm256_loadu_ps(&system->particles.position_x[i]);
        __m256 py = _mm256_loadu_ps(&system->particles.position_y[i]);
        __m256 pz = _mm256_loadu_ps(&system->particles.position_z[i]);
        
        __m256 vx = _mm256_loadu_ps(&system->particles.velocity_x[i]);
        __m256 vy = _mm256_loadu_ps(&system->particles.velocity_y[i]);
        __m256 vz = _mm256_loadu_ps(&system->particles.velocity_z[i]);
        
        __m256 age = _mm256_loadu_ps(&system->particles.age[i]);
        __m256 max_age = _mm256_loadu_ps(&system->particles.max_age[i]);
        __m256 drag = _mm256_loadu_ps(&system->particles.drag[i]);
        
        // Update age
        age = _mm256_add_ps(age, dt);
        
        // Apply gravity
        vy = _mm256_add_ps(vy, gravity);
        
        // Apply drag
        __m256 drag_factor = _mm256_sub_ps(one, _mm256_mul_ps(drag, dt));
        vx = _mm256_mul_ps(vx, drag_factor);
        vy = _mm256_mul_ps(vy, drag_factor);
        vz = _mm256_mul_ps(vz, drag_factor);
        
        // Update position
        px = _mm256_add_ps(px, _mm256_mul_ps(vx, dt));
        py = _mm256_add_ps(py, _mm256_mul_ps(vy, dt));
        pz = _mm256_add_ps(pz, _mm256_mul_ps(vz, dt));
        
        // Store back (use unaligned store)
        _mm256_storeu_ps(&system->particles.position_x[i], px);
        _mm256_storeu_ps(&system->particles.position_y[i], py);
        _mm256_storeu_ps(&system->particles.position_z[i], pz);
        
        _mm256_storeu_ps(&system->particles.velocity_x[i], vx);
        _mm256_storeu_ps(&system->particles.velocity_y[i], vy);
        _mm256_storeu_ps(&system->particles.velocity_z[i], vz);
        
        _mm256_storeu_ps(&system->particles.age[i], age);
        
        // Update opacity based on age
        __m256 life_ratio = _mm256_div_ps(age, max_age);
        __m256 opacity = _mm256_sub_ps(one, life_ratio);
        _mm256_storeu_ps(&system->particles.opacity[i], opacity);
    }
    
    // Handle remaining particles
    for (u32 i = simd_count; i < count; i++) {
        update_particle_scalar(system, i, delta_time);
    }
    
    // Remove dead particles
    u32 write_idx = 0;
    for (u32 read_idx = 0; read_idx < count; read_idx++) {
        if (!(system->particles.flags[read_idx] & 0x1)) {  // Not dead
            if (write_idx != read_idx) {
                // Copy particle data
                system->particles.position_x[write_idx] = system->particles.position_x[read_idx];
                system->particles.position_y[write_idx] = system->particles.position_y[read_idx];
                system->particles.position_z[write_idx] = system->particles.position_z[read_idx];
                // ... copy all other properties
            }
            write_idx++;
        }
    }
    system->particles.count = write_idx;
}

// ============================================================================
// MAIN UPDATE FUNCTION
// ============================================================================

void particles_update(particle_system* system, f32 delta_time) {
    // Update emitters
    for (u32 i = 0; i < system->emitter_count; i++) {
        particle_emitter* emitter = &system->emitters[i];
        
        if (!emitter->is_active || emitter->is_paused) continue;
        
        // Update emitter lifetime
        emitter->time_alive += delta_time;
        if (emitter->config.emitter_lifetime > 0 && 
            emitter->time_alive >= emitter->config.emitter_lifetime) {
            emitter->is_active = false;
            continue;
        }
        
        // Spawn new particles
        if (emitter->config.continuous) {
            emitter->emission_accumulator += emitter->config.emission_rate * delta_time;
            
            while (emitter->emission_accumulator >= 1.0f) {
                particles_burst_emitter(system, emitter->id, 1);
                emitter->emission_accumulator -= 1.0f;
            }
        }
    }
    
    // Update particles
    if (system->use_simd) {
        particles_update_simd(system, delta_time);
    } else {
        for (u32 i = 0; i < system->particles.count; i++) {
            update_particle_scalar(system, i, delta_time);
        }
    }
    
    // Apply force fields
    for (u32 f = 0; f < system->force_field_count; f++) {
        force_field* field = &system->force_fields[f];
        if (!field->is_active) continue;
        
        for (u32 i = 0; i < system->particles.count; i++) {
            f32 dx = system->particles.position_x[i] - field->position.x;
            f32 dy = system->particles.position_y[i] - field->position.y;
            f32 dz = system->particles.position_z[i] - field->position.z;
            
            f32 dist_sq = dx*dx + dy*dy + dz*dz;
            if (dist_sq < field->radius * field->radius && dist_sq > 0.001f) {
                f32 dist = sqrtf(dist_sq);
                f32 force = field->strength / dist_sq;
                
                switch (field->type) {
                    case FORCE_ATTRACT:
                        system->particles.velocity_x[i] -= (dx / dist) * force * delta_time;
                        system->particles.velocity_y[i] -= (dy / dist) * force * delta_time;
                        system->particles.velocity_z[i] -= (dz / dist) * force * delta_time;
                        break;
                        
                    case FORCE_REPEL:
                        system->particles.velocity_x[i] += (dx / dist) * force * delta_time;
                        system->particles.velocity_y[i] += (dy / dist) * force * delta_time;
                        system->particles.velocity_z[i] += (dz / dist) * force * delta_time;
                        break;
                        
                    case FORCE_VORTEX:
                        // Rotate around field center
                        system->particles.velocity_x[i] += (-dy / dist) * force * delta_time;
                        system->particles.velocity_y[i] += (dx / dist) * force * delta_time;
                        break;
                }
            }
        }
    }
}

// ============================================================================
// RENDERING
// ============================================================================

particle_render_data particles_get_render_data(particle_system* system) {
    particle_render_data data = {0};
    
    // For now, return direct pointers
    // In production, would copy to render-specific format
    data.count = system->particles.count;
    
    // Note: positions need to be interleaved for rendering
    // This is just a placeholder
    data.positions = system->particles.position_x;
    data.sizes = system->particles.size;
    data.colors = system->particles.color;
    data.rotations = system->particles.rotation;
    data.texture_ids = system->particles.texture_id;
    
    return data;
}

// ============================================================================
// PRESET EFFECTS
// ============================================================================

emitter_config particles_preset_fire(v3 position) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_CONE;
    cfg.position = position;
    cfg.direction = (v3){0, 1, 0};  // Upward
    cfg.spread_angle = 0.3f;  // radians
    
    cfg.emission_rate = 50.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 2.0f;
    cfg.start_speed_variance = 0.5f;
    cfg.start_size = 0.3f;
    cfg.start_size_variance = 0.1f;
    
    cfg.start_color = (color32){255, 200, 50, 255};  // Orange
    cfg.end_color = (color32){255, 50, 0, 0};        // Red, fading
    
    cfg.particle_lifetime = 1.5f;
    cfg.lifetime_variance = 0.3f;
    cfg.emitter_lifetime = -1;  // Infinite
    
    cfg.gravity = (v3){0, -2.0f, 0};  // Slight upward buoyancy
    cfg.drag_coefficient = 0.5f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    
    return cfg;
}

emitter_config particles_preset_smoke(v3 position) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_SPHERE;
    cfg.position = position;
    cfg.radius = 0.2f;
    cfg.direction = (v3){0, 1, 0};
    
    cfg.emission_rate = 20.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 0.5f;
    cfg.start_speed_variance = 0.2f;
    cfg.start_size = 0.5f;
    cfg.start_size_variance = 0.2f;
    
    cfg.start_color = (color32){100, 100, 100, 200};
    cfg.end_color = (color32){50, 50, 50, 0};
    
    cfg.particle_lifetime = 3.0f;
    cfg.lifetime_variance = 0.5f;
    
    cfg.gravity = (v3){0, 0.5f, 0};  // Rise slowly
    cfg.drag_coefficient = 1.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}

emitter_config particles_preset_explosion(v3 position, f32 radius) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_SPHERE;
    cfg.position = position;
    cfg.radius = radius * 0.1f;
    
    cfg.burst_count = 200;
    cfg.continuous = false;
    
    cfg.start_speed = radius * 10.0f;
    cfg.start_speed_variance = radius * 2.0f;
    cfg.start_size = 0.4f;
    cfg.start_size_variance = 0.2f;
    
    cfg.start_color = (color32){255, 255, 100, 255};
    cfg.end_color = (color32){255, 50, 0, 0};
    
    cfg.particle_lifetime = 0.5f;
    cfg.lifetime_variance = 0.1f;
    cfg.emitter_lifetime = 0.1f;  // One burst
    
    cfg.gravity = (v3){0, -9.8f, 0};
    cfg.drag_coefficient = 2.0f;
    
    cfg.blend_mode = BLEND_ADDITIVE;
    
    return cfg;
}