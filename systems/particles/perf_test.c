#include "handmade_particles.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Particle System Performance Test ===\n\n");
    
    // Allocate 128MB for particle system (1M particles need ~60MB)
    size_t memory_size = 128 * 1024 * 1024;
    void* memory = malloc(memory_size);
    
    particle_system* system = particles_init(memory, memory_size);
    
    // Create multiple emitters
    v3 positions[] = {
        {0, 0, 0}, {5, 0, 0}, {-5, 0, 0},
        {0, 5, 0}, {0, -5, 0}
    };
    
    emitter_id emitters[5];
    for (int i = 0; i < 5; i++) {
        emitter_config cfg = particles_preset_fire(positions[i]);
        cfg.emission_rate = 1000.0f;  // High emission rate
        emitters[i] = particles_create_emitter(system, &cfg);
    }
    
    // Benchmark update performance
    printf("Spawning particles...\n");
    for (int i = 0; i < 100; i++) {
        particles_update(system, 0.016f);
    }
    
    printf("Particles active: %u\n", system->particles.count);
    
    // Benchmark different particle counts
    u32 test_counts[] = {1000, 10000, 50000, 100000};
    
    for (int t = 0; t < 4; t++) {
        // Reset and spawn exact number
        particles_reset(system);
        
        u32 target = test_counts[t];
        emitter_config burst_cfg = particles_preset_explosion((v3){0,0,0}, 1.0f);
        burst_cfg.particle_lifetime = 10.0f;  // Long lifetime
        emitter_id burst = particles_create_emitter(system, &burst_cfg);
        
        while (system->particles.count < target) {
            u32 to_spawn = target - system->particles.count;
            if (to_spawn > 1000) to_spawn = 1000;
            particles_burst_emitter(system, burst, to_spawn);
        }
        
        // Benchmark update
        clock_t start = clock();
        int iterations = 1000;
        
        for (int i = 0; i < iterations; i++) {
            particles_update(system, 0.016f);
        }
        
        clock_t end = clock();
        double time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
        double per_frame = time_ms / iterations;
        double particles_per_ms = target / per_frame;
        
        printf("\n%u particles:\n", target);
        printf("  Total time: %.2f ms\n", time_ms);
        printf("  Per frame: %.3f ms\n", per_frame);
        printf("  Throughput: %.0f particles/ms\n", particles_per_ms);
        printf("  Can sustain: %.0f FPS\n", 1000.0 / per_frame);
    }
    
    // Test memory usage
    printf("\nMemory usage:\n");
    printf("  Total allocated: %.2f MB\n", (f32)memory_size / (1024.0f * 1024.0f));
    printf("  Used: %.2f MB\n", (f32)system->memory_used / (1024.0f * 1024.0f));
    printf("  Efficiency: %.1f%%\n", (f32)system->memory_used * 100.0f / memory_size);
    
    particles_shutdown(system);
    free(memory);
    
    printf("\nAll tests completed!\n");
    return 0;
}
