/*
 * GPU Compute Shader for Particle System
 * Processes millions of particles in parallel
 */

#version 450

layout(local_size_x = 256) in;

// Particle data - Structure of Arrays
layout(std430, binding = 0) buffer PositionBuffer {
    vec4 positions[];  // xyz = position, w = life
};

layout(std430, binding = 1) buffer VelocityBuffer {
    vec4 velocities[]; // xyz = velocity, w = drag
};

layout(std430, binding = 2) buffer AttributeBuffer {
    vec4 attributes[]; // x = size, y = rotation, z = opacity, w = mass
};

layout(std430, binding = 3) buffer ColorBuffer {
    vec4 colors[];     // rgba
};

// Uniforms
layout(std140, binding = 4) uniform SimulationParams {
    vec4 gravity_wind;      // xyz = gravity, w = wind strength
    vec4 emitter_pos;       // xyz = position, w = emission rate
    vec4 force_field[8];    // xyz = position, w = strength
    vec2 time_delta;        // x = delta_time, y = total_time
    uint particle_count;
    uint force_field_count;
    float turbulence_scale;
    float padding;
};

// Random number generation
uint hash(uint x) {
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

float random(uint seed) {
    return float(hash(seed)) / float(0xFFFFFFFFu);
}

vec3 random_vec3(uint seed) {
    return vec3(
        random(seed),
        random(seed + 1u),
        random(seed + 2u)
    ) * 2.0 - 1.0;
}

// Turbulence noise
float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    uint n = uint(i.x) + uint(i.y) * 57u + uint(i.z) * 113u;
    float a = random(n);
    float b = random(n + 1u);
    float c = random(n + 57u);
    float d = random(n + 58u);
    float e = random(n + 113u);
    float f_val = random(n + 114u);
    float g = random(n + 170u);
    float h = random(n + 171u);
    
    float x1 = mix(a, b, f.x);
    float x2 = mix(c, d, f.x);
    float x3 = mix(e, f_val, f.x);
    float x4 = mix(g, h, f.x);
    
    float y1 = mix(x1, x2, f.y);
    float y2 = mix(x3, x4, f.y);
    
    return mix(y1, y2, f.z);
}

vec3 turbulence(vec3 pos) {
    vec3 turb = vec3(0.0);
    float scale = turbulence_scale;
    float amp = 1.0;
    
    for (int i = 0; i < 3; i++) {
        turb += random_vec3(uint(pos.x * scale) + uint(i)) * amp;
        scale *= 2.0;
        amp *= 0.5;
    }
    
    return turb;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= particle_count) {
        return;
    }
    
    // Load particle data
    vec3 pos = positions[idx].xyz;
    float life = positions[idx].w;
    vec3 vel = velocities[idx].xyz;
    float drag = velocities[idx].w;
    vec4 attr = attributes[idx];
    
    float dt = time_delta.x;
    
    // Check if particle is alive
    if (life <= 0.0) {
        return;
    }
    
    // Apply gravity
    vel += gravity_wind.xyz * dt;
    
    // Apply wind
    vel.x += gravity_wind.w * dt;
    
    // Apply drag
    vel *= (1.0 - drag * dt);
    
    // Apply force fields
    for (uint i = 0u; i < force_field_count; i++) {
        vec3 field_pos = force_field[i].xyz;
        float strength = force_field[i].w;
        
        vec3 delta = field_pos - pos;
        float dist_sq = dot(delta, delta);
        
        if (dist_sq > 0.001 && dist_sq < 100.0) {
            vec3 force = normalize(delta) * strength / dist_sq;
            vel += force * dt;
        }
    }
    
    // Apply turbulence
    if (turbulence_scale > 0.0) {
        vel += turbulence(pos * 0.1) * dt * 2.0;
    }
    
    // Update position
    pos += vel * dt;
    
    // Update life
    life -= dt;
    
    // Update opacity based on life
    float life_ratio = max(0.0, life / 2.0); // Assume 2 second lifetime
    attr.z = life_ratio;
    
    // Update rotation
    attr.y += dt * 2.0;
    
    // Store results
    positions[idx] = vec4(pos, life);
    velocities[idx] = vec4(vel, drag);
    attributes[idx] = attr;
    
    // Fade color with age
    colors[idx].a *= life_ratio;
}

/*
 * Particle Emission Kernel
 * Spawns new particles from emitters
 */
#ifdef EMISSION_KERNEL

layout(local_size_x = 64) in;

layout(std430, binding = 5) buffer EmitterBuffer {
    vec4 emitter_configs[]; // Multiple emitters
};

layout(std430, binding = 6) buffer SpawnBuffer {
    uint spawn_indices[];   // Indices of dead particles to respawn
    uint spawn_count;
};

void emit_particle(uint idx, uint emitter_id) {
    vec4 config = emitter_configs[emitter_id * 4u];
    vec3 emit_pos = config.xyz;
    float emit_radius = config.w;
    
    vec4 config2 = emitter_configs[emitter_id * 4u + 1u];
    vec3 emit_dir = config2.xyz;
    float emit_speed = config2.w;
    
    vec4 config3 = emitter_configs[emitter_id * 4u + 2u];
    vec4 emit_color = config3;
    
    vec4 config4 = emitter_configs[emitter_id * 4u + 3u];
    float lifetime = config4.x;
    float size = config4.y;
    float mass = config4.z;
    float drag_coeff = config4.w;
    
    // Random spawn position within radius
    uint seed = idx * 1000u + uint(time_delta.y * 1000.0);
    vec3 rand_offset = random_vec3(seed) * emit_radius;
    
    // Random velocity variation
    vec3 rand_vel = random_vec3(seed + 100u) * 0.2;
    
    // Initialize particle
    positions[idx] = vec4(emit_pos + rand_offset, lifetime);
    velocities[idx] = vec4(emit_dir * emit_speed + rand_vel, drag_coeff);
    attributes[idx] = vec4(size, 0.0, 1.0, mass);
    colors[idx] = emit_color;
}

void main_emission() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= spawn_count) {
        return;
    }
    
    uint particle_idx = spawn_indices[idx];
    uint emitter_id = 0u; // Could be determined by emission rates
    
    emit_particle(particle_idx, emitter_id);
}

#endif

/*
 * Particle Sorting Kernel
 * Sorts particles by distance to camera for proper blending
 */
#ifdef SORTING_KERNEL

layout(local_size_x = 256) in;

layout(std140, binding = 7) uniform CameraData {
    vec4 camera_pos;
    mat4 view_proj;
};

layout(std430, binding = 8) buffer DistanceBuffer {
    float distances[];
};

layout(std430, binding = 9) buffer IndexBuffer {
    uint indices[];
};

void main_sorting() {
    uint idx = gl_GlobalInvocationID.x;
    
    if (idx >= particle_count) {
        return;
    }
    
    vec3 pos = positions[idx].xyz;
    vec3 to_camera = camera_pos.xyz - pos;
    float dist = dot(to_camera, to_camera);
    
    distances[idx] = dist;
    indices[idx] = idx;
    
    // Bitonic sort would be implemented here
    // For now, just store distance for CPU sorting
}

#endif