/*
 * GPU Particle System Implementation
 * Compute shader acceleration for millions of particles
 */

#include "handmade_particles.h"
#include <stdio.h>
#include <string.h>

#ifdef PARTICLE_GPU_SUPPORT

#include <GL/gl.h>
#include <GL/glext.h>

// GPU particle buffers
typedef struct gpu_particle_buffers {
    GLuint position_buffer;
    GLuint velocity_buffer;
    GLuint attribute_buffer;
    GLuint color_buffer;
    GLuint distance_buffer;
    GLuint index_buffer;
    GLuint spawn_buffer;
    GLuint emitter_buffer;
    
    GLuint compute_program;
    GLuint emission_program;
    GLuint sorting_program;
    
    GLuint simulation_ubo;
    GLuint camera_ubo;
} gpu_particle_buffers;

// ============================================================================
// SHADER COMPILATION
// ============================================================================

static GLuint compile_compute_shader(const char* source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("Compute shader compilation failed: %s\n", info_log);
        return 0;
    }
    
    return shader;
}

static GLuint create_compute_program(const char* source) {
    GLuint shader = compile_compute_shader(source);
    if (!shader) return 0;
    
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("Compute program linking failed: %s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    
    glDeleteShader(shader);
    return program;
}

// ============================================================================
// GPU INITIALIZATION
// ============================================================================

static b32 particles_init_gpu_buffers(particle_system* system) {
    gpu_particle_buffers* gpu = (gpu_particle_buffers*)arena_alloc(system, 
        sizeof(gpu_particle_buffers));
    
    if (!gpu) {
        printf("Failed to allocate GPU buffers struct\n");
        return false;
    }
    
    system->gpu.particle_buffer = gpu;
    
    // Create position buffer
    glGenBuffers(1, &gpu->position_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->position_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
        PARTICLE_MAX_TOTAL * sizeof(v4), NULL, GL_DYNAMIC_DRAW);
    
    // Create velocity buffer
    glGenBuffers(1, &gpu->velocity_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->velocity_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
        PARTICLE_MAX_TOTAL * sizeof(v4), NULL, GL_DYNAMIC_DRAW);
    
    // Create attribute buffer
    glGenBuffers(1, &gpu->attribute_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->attribute_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
        PARTICLE_MAX_TOTAL * sizeof(v4), NULL, GL_DYNAMIC_DRAW);
    
    // Create color buffer
    glGenBuffers(1, &gpu->color_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->color_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
        PARTICLE_MAX_TOTAL * sizeof(v4), NULL, GL_DYNAMIC_DRAW);
    
    // Create uniform buffer for simulation params
    glGenBuffers(1, &gpu->simulation_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, gpu->simulation_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 256, NULL, GL_DYNAMIC_DRAW);
    
    printf("GPU particle buffers initialized\n");
    return true;
}

// ============================================================================
// GPU DATA TRANSFER
// ============================================================================

static void particles_upload_to_gpu(particle_system* system) {
    gpu_particle_buffers* gpu = (gpu_particle_buffers*)system->gpu.particle_buffer;
    u32 count = system->particles.count;
    
    if (count == 0) return;
    
    // Convert SoA to AoS for GPU (vec4 format)
    v4* positions = (v4*)alloca(count * sizeof(v4));
    v4* velocities = (v4*)alloca(count * sizeof(v4));
    v4* attributes = (v4*)alloca(count * sizeof(v4));
    v4* colors = (v4*)alloca(count * sizeof(v4));
    
    for (u32 i = 0; i < count; i++) {
        // Position + life
        positions[i] = (v4){
            system->particles.position_x[i],
            system->particles.position_y[i],
            system->particles.position_z[i],
            system->particles.max_age[i] - system->particles.age[i]
        };
        
        // Velocity + drag
        velocities[i] = (v4){
            system->particles.velocity_x[i],
            system->particles.velocity_y[i],
            system->particles.velocity_z[i],
            system->particles.drag[i]
        };
        
        // Size, rotation, opacity, mass
        attributes[i] = (v4){
            system->particles.size[i],
            system->particles.rotation[i],
            system->particles.opacity[i],
            system->particles.mass[i]
        };
        
        // Color
        u32 c = system->particles.color[i];
        colors[i] = (v4){
            ((c >> 24) & 0xFF) / 255.0f,
            ((c >> 16) & 0xFF) / 255.0f,
            ((c >> 8) & 0xFF) / 255.0f,
            (c & 0xFF) / 255.0f
        };
    }
    
    // Upload to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->position_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), positions);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->velocity_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), velocities);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->attribute_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), attributes);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->color_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), colors);
}

static void particles_download_from_gpu(particle_system* system) {
    gpu_particle_buffers* gpu = (gpu_particle_buffers*)system->gpu.particle_buffer;
    u32 count = system->particles.count;
    
    if (count == 0) return;
    
    // Download from GPU
    v4* positions = (v4*)alloca(count * sizeof(v4));
    v4* velocities = (v4*)alloca(count * sizeof(v4));
    v4* attributes = (v4*)alloca(count * sizeof(v4));
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->position_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), positions);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->velocity_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), velocities);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->attribute_buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(v4), attributes);
    
    // Convert back to SoA
    for (u32 i = 0; i < count; i++) {
        system->particles.position_x[i] = positions[i].x;
        system->particles.position_y[i] = positions[i].y;
        system->particles.position_z[i] = positions[i].z;
        
        system->particles.velocity_x[i] = velocities[i].x;
        system->particles.velocity_y[i] = velocities[i].y;
        system->particles.velocity_z[i] = velocities[i].z;
        system->particles.drag[i] = velocities[i].w;
        
        system->particles.size[i] = attributes[i].x;
        system->particles.rotation[i] = attributes[i].y;
        system->particles.opacity[i] = attributes[i].z;
        system->particles.mass[i] = attributes[i].w;
        
        // Update age from life
        system->particles.age[i] = system->particles.max_age[i] - positions[i].w;
    }
}

// ============================================================================
// GPU UPDATE
// ============================================================================

void particles_update_gpu(particle_system* system, f32 delta_time) {
    if (!system->gpu.enabled || !system->gpu.compute_shader) {
        // Fall back to CPU
        particles_update(system, delta_time);
        return;
    }
    
    gpu_particle_buffers* gpu = (gpu_particle_buffers*)system->gpu.particle_buffer;
    u32 count = system->particles.count;
    
    if (count == 0) return;
    
    // Upload current state to GPU
    particles_upload_to_gpu(system);
    
    // Update simulation parameters
    struct {
        v4 gravity_wind;
        v4 emitter_pos;
        v4 force_fields[8];
        v2 time_delta;
        u32 particle_count;
        u32 force_field_count;
        f32 turbulence_scale;
        f32 padding;
    } sim_params;
    
    // Set gravity and wind
    sim_params.gravity_wind = (v4){0, -9.8f, 0, 0.5f};
    sim_params.time_delta = (v2){delta_time, system->stats.particles_spawned * 0.001f};
    sim_params.particle_count = count;
    sim_params.force_field_count = system->force_field_count;
    sim_params.turbulence_scale = 0.1f;
    
    // Copy force fields
    for (u32 i = 0; i < system->force_field_count && i < 8; i++) {
        force_field* field = &system->force_fields[i];
        sim_params.force_fields[i] = (v4){
            field->position.x,
            field->position.y,
            field->position.z,
            field->strength
        };
    }
    
    // Upload uniform buffer
    glBindBuffer(GL_UNIFORM_BUFFER, gpu->simulation_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(sim_params), &sim_params);
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpu->position_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gpu->velocity_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpu->attribute_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpu->color_buffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 4, gpu->simulation_ubo);
    
    // Run compute shader
    glUseProgram(gpu->compute_program);
    
    // Dispatch compute threads (256 threads per group)
    u32 num_groups = (count + 255) / 256;
    glDispatchCompute(num_groups, 1, 1);
    
    // Memory barrier to ensure writes complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Download results back to CPU
    particles_download_from_gpu(system);
    
    // Update stats
    system->stats.update_time_ms = delta_time * 1000.0f;
}

// ============================================================================
// GPU CLEANUP
// ============================================================================

static void particles_cleanup_gpu(particle_system* system) {
    if (!system->gpu.particle_buffer) return;
    
    gpu_particle_buffers* gpu = (gpu_particle_buffers*)system->gpu.particle_buffer;
    
    // Delete buffers
    glDeleteBuffers(1, &gpu->position_buffer);
    glDeleteBuffers(1, &gpu->velocity_buffer);
    glDeleteBuffers(1, &gpu->attribute_buffer);
    glDeleteBuffers(1, &gpu->color_buffer);
    glDeleteBuffers(1, &gpu->distance_buffer);
    glDeleteBuffers(1, &gpu->index_buffer);
    glDeleteBuffers(1, &gpu->spawn_buffer);
    glDeleteBuffers(1, &gpu->emitter_buffer);
    glDeleteBuffers(1, &gpu->simulation_ubo);
    glDeleteBuffers(1, &gpu->camera_ubo);
    
    // Delete programs
    if (gpu->compute_program) glDeleteProgram(gpu->compute_program);
    if (gpu->emission_program) glDeleteProgram(gpu->emission_program);
    if (gpu->sorting_program) glDeleteProgram(gpu->sorting_program);
    
    system->gpu.enabled = false;
    system->gpu.particle_buffer = NULL;
}

// ============================================================================
// PUBLIC API
// ============================================================================

b32 particles_enable_gpu(particle_system* system) {
    // Check for compute shader support
    GLint max_compute_work_group_count[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_compute_work_group_count[0]);
    
    if (max_compute_work_group_count[0] < 65535) {
        printf("GPU compute shaders not supported\n");
        return false;
    }
    
    // Initialize GPU buffers
    if (!particles_init_gpu_buffers(system)) {
        return false;
    }
    
    // Load compute shader
    // In production, this would load from file
    extern const char* particle_compute_shader_source;
    
    gpu_particle_buffers* gpu = (gpu_particle_buffers*)system->gpu.particle_buffer;
    gpu->compute_program = create_compute_program(particle_compute_shader_source);
    
    if (!gpu->compute_program) {
        particles_cleanup_gpu(system);
        return false;
    }
    
    system->gpu.enabled = true;
    system->gpu.compute_shader = (void*)gpu->compute_program;
    
    printf("GPU particle acceleration enabled\n");
    printf("Max particles (GPU): %d million\n", PARTICLE_MAX_TOTAL / 1000000);
    
    return true;
}

void particles_disable_gpu(particle_system* system) {
    particles_cleanup_gpu(system);
    system->use_gpu = false;
    printf("GPU particle acceleration disabled\n");
}

#else // !PARTICLE_GPU_SUPPORT

// Stub implementations when GPU support is not compiled in
b32 particles_enable_gpu(particle_system* system) {
    (void)system;
    printf("Particle system compiled without GPU support\n");
    return false;
}

void particles_disable_gpu(particle_system* system) {
    (void)system;
}

void particles_update_gpu(particle_system* system, f32 delta_time) {
    // Fall back to CPU implementation
    particles_update(system, delta_time);
}

#endif // PARTICLE_GPU_SUPPORT