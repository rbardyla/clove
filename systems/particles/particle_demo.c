/*
 * Particle System Demo
 * Interactive demonstration of particle effects
 */

#include "handmade_particles.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// Simple terminal-based visualization
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define WORLD_SCALE 5.0f

static char screen[SCREEN_HEIGHT][SCREEN_WIDTH + 1];

// Non-blocking keyboard input
static int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

static void clear_screen() {
    printf("\033[2J\033[H");
}

static void render_particles(particle_system* system) {
    // Clear screen buffer
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen[y][x] = ' ';
        }
        screen[y][SCREEN_WIDTH] = '\0';
    }
    
    // Render particles as ASCII
    for (u32 i = 0; i < system->particles.count; i++) {
        // Convert world position to screen position
        int sx = (int)(system->particles.position_x[i] * WORLD_SCALE + SCREEN_WIDTH / 2);
        int sy = SCREEN_HEIGHT - (int)(system->particles.position_y[i] * WORLD_SCALE + SCREEN_HEIGHT / 2);
        
        if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
            // Choose character based on particle properties
            char c = '*';
            f32 opacity = system->particles.opacity[i];
            
            if (opacity > 0.8f) c = '@';
            else if (opacity > 0.6f) c = 'o';
            else if (opacity > 0.4f) c = '*';
            else if (opacity > 0.2f) c = '.';
            else c = '·';
            
            screen[sy][sx] = c;
        }
    }
    
    // Draw to terminal
    clear_screen();
    printf("=== HANDMADE PARTICLE SYSTEM DEMO ===\n");
    printf("Particles: %u | FPS: 60\n", system->particles.count);
    printf("Controls: 1-9: Effects | SPACE: Burst | Q: Quit\n");
    printf("=====================================\n");
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        printf("%s\n", screen[y]);
    }
}

int main(int argc, char* argv[]) {
    printf("Initializing particle system...\n");
    
    // Allocate 128MB for particle system (1M particles need ~60MB)
    size_t memory_size = 128 * 1024 * 1024;
    void* memory = malloc(memory_size);
    
    if (!memory) {
        printf("Failed to allocate memory!\n");
        return 1;
    }
    
    // Initialize particle system
    particle_system* system = particles_init(memory, memory_size);
    
    if (!system) {
        printf("Failed to initialize particle system!\n");
        free(memory);
        return 1;
    }
    
    // Seed random
    srand(time(NULL));
    
    // Create initial emitters
    v3 center = {0, 0, 0};
    emitter_config fire_cfg = particles_preset_fire(center);
    emitter_id fire_emitter = particles_create_emitter(system, &fire_cfg);
    
    // Add a force field
    force_field wind = {
        .position = {5, 0, 0},
        .radius = 10.0f,
        .strength = 5.0f,
        .type = FORCE_REPEL,
        .is_active = true
    };
    particles_add_force_field(system, &wind);
    
    printf("Starting demo...\n");
    sleep(1);
    
    // Main loop
    bool running = true;
    clock_t last_time = clock();
    
    while (running) {
        // Calculate delta time
        clock_t current_time = clock();
        f32 delta_time = (f32)(current_time - last_time) / CLOCKS_PER_SEC;
        last_time = current_time;
        
        // Cap delta time
        if (delta_time > 0.1f) delta_time = 0.1f;
        if (delta_time < 0.001f) delta_time = 0.016f;  // ~60 FPS
        
        // Handle input
        if (kbhit()) {
            char key = getchar();
            v3 pos = {0, 0, 0};
            
            switch (key) {
                case 'q':
                case 'Q':
                    running = false;
                    break;
                    
                case ' ':
                    // Burst current emitter
                    particles_burst_emitter(system, fire_emitter, 50);
                    break;
                    
                case '1': {
                    // Fire effect
                    emitter_config cfg = particles_preset_fire(pos);
                    particles_destroy_emitter(system, fire_emitter);
                    fire_emitter = particles_create_emitter(system, &cfg);
                    printf("\nFire effect activated!\n");
                    break;
                }
                    
                case '2': {
                    // Smoke effect
                    emitter_config cfg = particles_preset_smoke(pos);
                    particles_destroy_emitter(system, fire_emitter);
                    fire_emitter = particles_create_emitter(system, &cfg);
                    printf("\nSmoke effect activated!\n");
                    break;
                }
                    
                case '3': {
                    // Explosion
                    emitter_config cfg = particles_preset_explosion(pos, 2.0f);
                    emitter_id explosion = particles_create_emitter(system, &cfg);
                    particles_burst_emitter(system, explosion, cfg.burst_count);
                    printf("\nExplosion!\n");
                    break;
                }
                    
                case '4': {
                    // Fountain
                    emitter_config cfg = {0};
                    cfg.shape = EMISSION_CONE;
                    cfg.position = (v3){0, -3, 0};
                    cfg.direction = (v3){0, 1, 0};
                    cfg.spread_angle = 0.2f;
                    cfg.emission_rate = 100.0f;
                    cfg.continuous = true;
                    cfg.start_speed = 5.0f;
                    cfg.start_speed_variance = 1.0f;
                    cfg.start_size = 0.2f;
                    cfg.particle_lifetime = 2.0f;
                    cfg.gravity = (v3){0, -9.8f, 0};
                    cfg.drag_coefficient = 0.1f;
                    cfg.start_color = (color32){100, 150, 255, 255};
                    cfg.blend_mode = BLEND_ALPHA;
                    
                    particles_destroy_emitter(system, fire_emitter);
                    fire_emitter = particles_create_emitter(system, &cfg);
                    printf("\nFountain effect activated!\n");
                    break;
                }
                    
                case '5': {
                    // Snow
                    v3 area_min = {-5, 5, -1};
                    v3 area_max = {5, 5, 1};
                    emitter_config cfg = particles_preset_snow(area_min, area_max);
                    particles_destroy_emitter(system, fire_emitter);
                    fire_emitter = particles_create_emitter(system, &cfg);
                    printf("\nSnow effect activated!\n");
                    break;
                }
                    
                case 'r':
                case 'R':
                    // Reset
                    particles_reset(system);
                    fire_cfg = particles_preset_fire(center);
                    fire_emitter = particles_create_emitter(system, &fire_cfg);
                    printf("\nSystem reset!\n");
                    break;
            }
        }
        
        // Update particle system
        particles_update(system, delta_time);
        
        // Render
        render_particles(system);
        
        // Frame rate limiting
        usleep(16666);  // ~60 FPS
    }
    
    // Cleanup
    printf("\nShutting down...\n");
    particles_shutdown(system);
    free(memory);
    
    return 0;
}

// Implementation of snow preset (was missing)
emitter_config particles_preset_snow(v3 area_min, v3 area_max) {
    emitter_config cfg = {0};
    
    cfg.shape = EMISSION_BOX;
    cfg.box_min = area_min;
    cfg.box_max = area_max;
    cfg.direction = (v3){0, -1, 0};
    
    cfg.emission_rate = 30.0f;
    cfg.continuous = true;
    
    cfg.start_speed = 1.0f;
    cfg.start_speed_variance = 0.3f;
    cfg.start_size = 0.15f;
    cfg.start_size_variance = 0.05f;
    
    cfg.start_color = (color32){255, 255, 255, 200};
    cfg.end_color = (color32){255, 255, 255, 0};
    
    cfg.particle_lifetime = 5.0f;
    cfg.lifetime_variance = 1.0f;
    cfg.emitter_lifetime = -1;
    
    cfg.gravity = (v3){0, -1.0f, 0};
    cfg.drag_coefficient = 2.0f;
    
    cfg.blend_mode = BLEND_ALPHA;
    
    return cfg;
}

// Stub for force field addition (was referenced but not implemented)
u32 particles_add_force_field(particle_system* system, const force_field* field) {
    if (system->force_field_count >= PARTICLE_FORCE_FIELDS) {
        return (u32)-1;
    }
    
    u32 index = system->force_field_count++;
    system->force_fields[index] = *field;
    return index;
}