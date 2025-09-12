/*
    Continental Architect ULTRA - 1000% More Bells and Whistles!
    
    Features:
    - Particle effects
    - Water simulation with rivers
    - Growing vegetation
    - Weather system with clouds and rain
    - Civilizations with buildings
    - Volcanic eruptions
    - Day/night cycle
    - Multiple biomes
    - Sound effects (simulated visually)
    - And much more!
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define GRID_SIZE 64
#define MAX_PARTICLES 1000
#define MAX_TREES 500
#define MAX_BUILDINGS 100
#define MAX_CLOUDS 20
#define MAX_RIVERS 10

// ============= TYPES =============

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life;
    float r, g, b;
    int type; // 0=dirt, 1=water, 2=lava, 3=smoke
} Particle;

typedef struct {
    float x, z;
    float height;
    float age;
    int type; // 0=pine, 1=oak, 2=palm
} Tree;

typedef struct {
    float x, z;
    float height;
    int population;
    float age;
    int type; // 0=hut, 1=house, 2=tower, 3=castle
} Building;

typedef struct {
    float x, y, z;
    float size;
    float density;
    float rain_rate;
} Cloud;

typedef struct {
    int x, z;
    float water_level;
    float flow_rate;
} River;

typedef struct {
    float terrain[GRID_SIZE][GRID_SIZE];
    float water[GRID_SIZE][GRID_SIZE];
    float temperature[GRID_SIZE][GRID_SIZE];
    float vegetation[GRID_SIZE][GRID_SIZE];
    
    Particle particles[MAX_PARTICLES];
    int particle_count;
    
    Tree trees[MAX_TREES];
    int tree_count;
    
    Building buildings[MAX_BUILDINGS];
    int building_count;
    
    Cloud clouds[MAX_CLOUDS];
    int cloud_count;
    
    River rivers[MAX_RIVERS];
    int river_count;
    
    // Camera and time
    float camera_angle;
    float camera_height;
    float camera_distance;
    float time_of_day; // 0-24
    float season; // 0-4
    float geological_time;
    int time_speed; // 1-1000x
    
    // Tools
    int current_tool; // 1=terrain, 2=water, 3=volcano, 4=city, 5=forest
    int brush_size;
    
    // Effects
    float earthquake_intensity;
    int volcano_x, volcano_z;
    float volcano_countdown;
    
    // UI
    int show_stats;
    int show_help;
    float fps;
    int frame_count;
    time_t last_fps_time;
    
    // Input
    int mouse_x, mouse_y;
    int mouse_down;
    int keys[256];
} GameState;

GameState* game;

// ============= INITIALIZATION =============

void init_terrain() {
    // Generate interesting terrain with multiple features
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float fx = (float)x / GRID_SIZE;
            float fz = (float)z / GRID_SIZE;
            
            // Continental plates
            float height = sinf(fx * 3.14159f) * cosf(fz * 3.14159f) * 0.3f;
            
            // Mountain ranges
            height += sinf(fx * 10.0f) * cosf(fz * 7.0f) * 0.15f;
            
            // Hills
            height += sinf(fx * 20.0f + fz * 15.0f) * 0.05f;
            
            // Valleys
            if (fabs(fx - 0.5f) < 0.1f) height -= 0.2f;
            
            game->terrain[z][x] = height;
            game->water[z][x] = (height < -0.1f) ? -height * 0.5f : 0;
            game->temperature[z][x] = 20.0f + height * 10.0f; // Higher = colder
            game->vegetation[z][x] = (height > 0 && height < 0.3f) ? 0.5f : 0;
        }
    }
    
    // Add some initial trees
    for (int i = 0; i < 50; i++) {
        int x = rand() % GRID_SIZE;
        int z = rand() % GRID_SIZE;
        if (game->terrain[z][x] > 0 && game->terrain[z][x] < 0.3f) {
            if (game->tree_count < MAX_TREES) {
                Tree* t = &game->trees[game->tree_count++];
                t->x = x;
                t->z = z;
                t->height = 0.05f + (rand() % 100) / 1000.0f;
                t->age = 0;
                t->type = rand() % 3;
            }
        }
    }
    
    // Add initial clouds
    for (int i = 0; i < 5; i++) {
        if (game->cloud_count < MAX_CLOUDS) {
            Cloud* c = &game->clouds[game->cloud_count++];
            c->x = (rand() % GRID_SIZE) - GRID_SIZE/2;
            c->y = 2.0f + (rand() % 100) / 100.0f;
            c->z = (rand() % GRID_SIZE) - GRID_SIZE/2;
            c->size = 0.5f + (rand() % 100) / 100.0f;
            c->density = 0.3f + (rand() % 70) / 100.0f;
            c->rain_rate = 0;
        }
    }
}

// ============= PARTICLE SYSTEM =============

void spawn_particles(float x, float y, float z, int type, int count) {
    for (int i = 0; i < count && game->particle_count < MAX_PARTICLES; i++) {
        Particle* p = &game->particles[game->particle_count++];
        p->x = x + (rand() % 100 - 50) / 100.0f;
        p->y = y;
        p->z = z + (rand() % 100 - 50) / 100.0f;
        
        p->vx = (rand() % 100 - 50) / 500.0f;
        p->vy = (rand() % 100) / 200.0f;
        p->vz = (rand() % 100 - 50) / 500.0f;
        
        p->life = 1.0f;
        p->type = type;
        
        switch(type) {
            case 0: // Dirt
                p->r = 0.5f; p->g = 0.3f; p->b = 0.1f;
                break;
            case 1: // Water
                p->r = 0.2f; p->g = 0.4f; p->b = 0.8f;
                p->vy *= 0.5f;
                break;
            case 2: // Lava
                p->r = 1.0f; p->g = 0.3f; p->b = 0.0f;
                p->vy *= 2.0f;
                break;
            case 3: // Smoke
                p->r = 0.3f; p->g = 0.3f; p->b = 0.3f;
                p->vy *= 0.3f;
                break;
        }
    }
}

void update_particles(float dt) {
    for (int i = 0; i < game->particle_count; i++) {
        Particle* p = &game->particles[i];
        
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
        
        p->vy -= 0.5f * dt; // Gravity
        p->life -= dt * 0.3f;
        
        // Remove dead particles
        if (p->life <= 0) {
            game->particles[i] = game->particles[--game->particle_count];
            i--;
        }
    }
}

// ============= SIMULATION =============

void simulate_water(float dt) {
    // Simple water flow
    float flow[GRID_SIZE][GRID_SIZE] = {0};
    
    for (int z = 1; z < GRID_SIZE-1; z++) {
        for (int x = 1; x < GRID_SIZE-1; x++) {
            if (game->water[z][x] > 0.01f) {
                float h = game->terrain[z][x] + game->water[z][x];
                
                // Flow to neighbors
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dz == 0) continue;
                        
                        float nh = game->terrain[z+dz][x+dx] + game->water[z+dz][x+dx];
                        if (nh < h) {
                            float diff = (h - nh) * 0.1f * dt;
                            flow[z+dz][x+dx] += diff;
                            flow[z][x] -= diff;
                        }
                    }
                }
            }
        }
    }
    
    // Apply flow
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            game->water[z][x] += flow[z][x];
            if (game->water[z][x] < 0) game->water[z][x] = 0;
            
            // Evaporation
            game->water[z][x] *= (1.0f - 0.001f * dt);
        }
    }
}

void simulate_vegetation(float dt) {
    // Vegetation grows near water
    for (int z = 1; z < GRID_SIZE-1; z++) {
        for (int x = 1; x < GRID_SIZE-1; x++) {
            if (game->terrain[z][x] > 0 && game->terrain[z][x] < 0.5f) {
                float water_nearby = 0;
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        water_nearby += game->water[z+dz][x+dx];
                    }
                }
                
                if (water_nearby > 0.1f) {
                    game->vegetation[z][x] += dt * 0.01f;
                    if (game->vegetation[z][x] > 1.0f) game->vegetation[z][x] = 1.0f;
                    
                    // Spawn trees randomly
                    if (game->vegetation[z][x] > 0.5f && rand() % 1000 < 1) {
                        if (game->tree_count < MAX_TREES) {
                            Tree* t = &game->trees[game->tree_count++];
                            t->x = x;
                            t->z = z;
                            t->height = 0.01f;
                            t->age = 0;
                            t->type = rand() % 3;
                        }
                    }
                }
            }
        }
    }
    
    // Grow existing trees
    for (int i = 0; i < game->tree_count; i++) {
        game->trees[i].age += dt;
        if (game->trees[i].height < 0.2f) {
            game->trees[i].height += dt * 0.01f;
        }
    }
}

void simulate_weather(float dt) {
    // Move clouds
    for (int i = 0; i < game->cloud_count; i++) {
        Cloud* c = &game->clouds[i];
        c->x += dt * 2.0f; // Wind
        
        // Wrap around
        if (c->x > GRID_SIZE) c->x = -GRID_SIZE;
        
        // Rain based on density
        if (c->density > 0.6f) {
            c->rain_rate = c->density;
            
            // Add water below cloud
            int cx = (int)(c->x + GRID_SIZE/2);
            int cz = (int)(c->z + GRID_SIZE/2);
            if (cx >= 0 && cx < GRID_SIZE && cz >= 0 && cz < GRID_SIZE) {
                game->water[cz][cx] += dt * 0.01f * c->rain_rate;
                
                // Water particles
                if (rand() % 100 < 10) {
                    spawn_particles(c->x/GRID_SIZE*3-1.5f, c->y, c->z/GRID_SIZE*3-1.5f, 1, 5);
                }
            }
        }
    }
}

void simulate_civilizations(float dt) {
    // Cities grow near water and flat land
    for (int i = 0; i < game->building_count; i++) {
        Building* b = &game->buildings[i];
        b->age += dt;
        
        int bx = (int)b->x;
        int bz = (int)b->z;
        
        // Check conditions
        float water_nearby = 0;
        float flat_land = 0;
        
        for (int dz = -2; dz <= 2; dz++) {
            for (int dx = -2; dx <= 2; dx++) {
                int x = bx + dx;
                int z = bz + dz;
                if (x >= 0 && x < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                    water_nearby += game->water[z][x];
                    float h_diff = fabs(game->terrain[z][x] - game->terrain[bz][bx]);
                    if (h_diff < 0.1f) flat_land += 1.0f;
                }
            }
        }
        
        // Grow population
        if (water_nearby > 0.5f && flat_land > 10) {
            b->population += dt * 10;
            
            // Upgrade building type
            if (b->population > 100 && b->type == 0) b->type = 1; // Hut -> House
            if (b->population > 500 && b->type == 1) b->type = 2; // House -> Tower
            if (b->population > 1000 && b->type == 2) b->type = 3; // Tower -> Castle
            
            b->height = 0.05f + b->type * 0.05f;
        }
    }
}

void trigger_volcano() {
    if (game->volcano_countdown <= 0) {
        game->volcano_x = rand() % GRID_SIZE;
        game->volcano_z = rand() % GRID_SIZE;
        game->volcano_countdown = 5.0f;
        
        // Raise terrain
        game->terrain[game->volcano_z][game->volcano_x] += 0.5f;
        
        printf("VOLCANO ERUPTING at %d,%d!\n", game->volcano_x, game->volcano_z);
    }
}

void update_volcano(float dt) {
    if (game->volcano_countdown > 0) {
        game->volcano_countdown -= dt;
        
        // Spawn lava particles
        float vx = game->volcano_x / (float)GRID_SIZE * 3.0f - 1.5f;
        float vz = game->volcano_z / (float)GRID_SIZE * 3.0f - 1.5f;
        float vy = game->terrain[game->volcano_z][game->volcano_x];
        
        spawn_particles(vx, vy, vz, 2, 10); // Lava
        spawn_particles(vx, vy + 0.2f, vz, 3, 5); // Smoke
        
        // Lava flow
        for (int dz = -2; dz <= 2; dz++) {
            for (int dx = -2; dx <= 2; dx++) {
                int x = game->volcano_x + dx;
                int z = game->volcano_z + dz;
                if (x >= 0 && x < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                    float dist = sqrtf(dx*dx + dz*dz);
                    if (dist < 3) {
                        game->temperature[z][x] += dt * 50.0f / (dist + 1);
                        
                        // Kill vegetation
                        game->vegetation[z][x] *= (1.0f - dt);
                    }
                }
            }
        }
    }
}

// ============= TOOLS =============

void apply_tool(int mx, int my) {
    // Convert mouse to world coordinates
    int gx = (mx * GRID_SIZE) / WINDOW_WIDTH;
    int gz = (my * GRID_SIZE) / WINDOW_HEIGHT;
    
    if (gx < 0 || gx >= GRID_SIZE || gz < 0 || gz >= GRID_SIZE)
        return;
    
    float wx = gx / (float)GRID_SIZE * 3.0f - 1.5f;
    float wz = gz / (float)GRID_SIZE * 3.0f - 1.5f;
    
    switch(game->current_tool) {
        case 1: // Terrain
            for (int dz = -game->brush_size; dz <= game->brush_size; dz++) {
                for (int dx = -game->brush_size; dx <= game->brush_size; dx++) {
                    int x = gx + dx;
                    int z = gz + dz;
                    if (x >= 0 && x < GRID_SIZE && z >= 0 && z < GRID_SIZE) {
                        float dist = sqrtf(dx*dx + dz*dz);
                        if (dist <= game->brush_size) {
                            float strength = (game->brush_size - dist) / game->brush_size * 0.05f;
                            if (game->keys[XK_Shift_L]) {
                                game->terrain[z][x] -= strength;
                            } else {
                                game->terrain[z][x] += strength;
                            }
                            
                            // Particles
                            spawn_particles(wx, game->terrain[gz][gx], wz, 0, 3);
                        }
                    }
                }
            }
            break;
            
        case 2: // Water
            game->water[gz][gx] += 0.1f;
            spawn_particles(wx, game->terrain[gz][gx] + game->water[gz][gx], wz, 1, 5);
            break;
            
        case 3: // Volcano
            game->volcano_x = gx;
            game->volcano_z = gz;
            game->volcano_countdown = 5.0f;
            break;
            
        case 4: // City
            if (game->building_count < MAX_BUILDINGS) {
                Building* b = &game->buildings[game->building_count++];
                b->x = gx;
                b->z = gz;
                b->height = 0.05f;
                b->population = 10;
                b->age = 0;
                b->type = 0;
                printf("Founded city at %d,%d\n", gx, gz);
            }
            break;
            
        case 5: // Forest
            for (int i = 0; i < 5 && game->tree_count < MAX_TREES; i++) {
                Tree* t = &game->trees[game->tree_count++];
                t->x = gx + (rand() % 5 - 2);
                t->z = gz + (rand() % 5 - 2);
                t->height = 0.05f + (rand() % 100) / 1000.0f;
                t->age = 0;
                t->type = rand() % 3;
            }
            break;
    }
}

// ============= RENDERING =============

void render_terrain() {
    // Calculate day/night lighting
    float sun_angle = (game->time_of_day / 24.0f) * 2 * 3.14159f;
    float sun_height = sinf(sun_angle);
    float ambient = 0.2f + sun_height * 0.3f;
    
    // Render terrain with biome colors
    for (int z = 0; z < GRID_SIZE-1; z++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = 0; x < GRID_SIZE; x++) {
            float fx = x / (float)GRID_SIZE * 3.0f - 1.5f;
            float fz0 = z / (float)GRID_SIZE * 3.0f - 1.5f;
            float fz1 = (z+1) / (float)GRID_SIZE * 3.0f - 1.5f;
            
            float h0 = game->terrain[z][x];
            float h1 = game->terrain[z+1][x];
            
            // Biome colors based on height and temperature
            float temp0 = game->temperature[z][x];
            float veg0 = game->vegetation[z][x];
            float water0 = game->water[z][x];
            
            float r, g, b;
            
            if (water0 > 0.01f) {
                // Water
                r = 0.2f; g = 0.4f; b = 0.6f + water0;
            } else if (h0 < -0.1f) {
                // Deep ocean
                r = 0.1f; g = 0.2f; b = 0.4f;
            } else if (h0 < 0) {
                // Beach
                r = 0.8f; g = 0.7f; b = 0.5f;
            } else if (temp0 > 40) {
                // Desert
                r = 0.9f; g = 0.7f; b = 0.4f;
            } else if (temp0 < 0) {
                // Snow
                r = 0.9f; g = 0.9f; b = 1.0f;
            } else if (veg0 > 0.5f) {
                // Forest
                r = 0.1f; g = 0.4f + veg0 * 0.3f; b = 0.1f;
            } else if (h0 > 0.5f) {
                // Mountain
                r = 0.5f; g = 0.4f; b = 0.3f;
            } else {
                // Grassland
                r = 0.3f; g = 0.6f; b = 0.2f;
            }
            
            // Apply lighting
            r *= ambient;
            g *= ambient;
            b *= ambient;
            
            glColor3f(r, g, b);
            glVertex3f(fx, h0, fz0);
            
            // Second vertex
            float temp1 = game->temperature[z+1][x];
            float veg1 = game->vegetation[z+1][x];
            float water1 = game->water[z+1][x];
            
            if (water1 > 0.01f) {
                r = 0.2f; g = 0.4f; b = 0.6f + water1;
            } else if (veg1 > 0.5f) {
                r = 0.1f; g = 0.4f + veg1 * 0.3f; b = 0.1f;
            } else {
                r = 0.3f; g = 0.6f; b = 0.2f;
            }
            
            r *= ambient;
            g *= ambient;
            b *= ambient;
            
            glColor3f(r, g, b);
            glVertex3f(fx, h1, fz1);
        }
        glEnd();
    }
}

void render_trees() {
    for (int i = 0; i < game->tree_count; i++) {
        Tree* t = &game->trees[i];
        float fx = t->x / (float)GRID_SIZE * 3.0f - 1.5f;
        float fz = t->z / (float)GRID_SIZE * 3.0f - 1.5f;
        int ix = (int)t->x;
        int iz = (int)t->z;
        
        if (ix >= 0 && ix < GRID_SIZE && iz >= 0 && iz < GRID_SIZE) {
            float fy = game->terrain[iz][ix];
            
            // Tree trunk
            glColor3f(0.4f, 0.2f, 0.1f);
            glBegin(GL_LINES);
            glVertex3f(fx, fy, fz);
            glVertex3f(fx, fy + t->height, fz);
            glEnd();
            
            // Leaves (triangle)
            if (t->type == 0) { // Pine
                glColor3f(0.1f, 0.4f, 0.1f);
            } else if (t->type == 1) { // Oak
                glColor3f(0.2f, 0.5f, 0.1f);
            } else { // Palm
                glColor3f(0.3f, 0.6f, 0.2f);
            }
            
            glBegin(GL_TRIANGLES);
            glVertex3f(fx - t->height/4, fy + t->height/2, fz);
            glVertex3f(fx + t->height/4, fy + t->height/2, fz);
            glVertex3f(fx, fy + t->height, fz);
            glEnd();
        }
    }
}

void render_buildings() {
    for (int i = 0; i < game->building_count; i++) {
        Building* b = &game->buildings[i];
        float fx = b->x / (float)GRID_SIZE * 3.0f - 1.5f;
        float fz = b->z / (float)GRID_SIZE * 3.0f - 1.5f;
        int ix = (int)b->x;
        int iz = (int)b->z;
        
        if (ix >= 0 && ix < GRID_SIZE && iz >= 0 && iz < GRID_SIZE) {
            float fy = game->terrain[iz][ix];
            
            // Building color based on type
            switch(b->type) {
                case 0: glColor3f(0.6f, 0.4f, 0.2f); break; // Hut
                case 1: glColor3f(0.7f, 0.7f, 0.6f); break; // House
                case 2: glColor3f(0.8f, 0.8f, 0.8f); break; // Tower
                case 3: glColor3f(0.9f, 0.9f, 1.0f); break; // Castle
            }
            
            // Draw building (box)
            float s = 0.02f + b->type * 0.01f; // Size
            glBegin(GL_QUADS);
            // Front
            glVertex3f(fx-s, fy, fz-s);
            glVertex3f(fx+s, fy, fz-s);
            glVertex3f(fx+s, fy+b->height, fz-s);
            glVertex3f(fx-s, fy+b->height, fz-s);
            // Back
            glVertex3f(fx-s, fy, fz+s);
            glVertex3f(fx+s, fy, fz+s);
            glVertex3f(fx+s, fy+b->height, fz+s);
            glVertex3f(fx-s, fy+b->height, fz+s);
            // Sides
            glVertex3f(fx-s, fy, fz-s);
            glVertex3f(fx-s, fy, fz+s);
            glVertex3f(fx-s, fy+b->height, fz+s);
            glVertex3f(fx-s, fy+b->height, fz-s);
            
            glVertex3f(fx+s, fy, fz-s);
            glVertex3f(fx+s, fy, fz+s);
            glVertex3f(fx+s, fy+b->height, fz+s);
            glVertex3f(fx+s, fy+b->height, fz-s);
            // Roof
            glVertex3f(fx-s, fy+b->height, fz-s);
            glVertex3f(fx+s, fy+b->height, fz-s);
            glVertex3f(fx+s, fy+b->height, fz+s);
            glVertex3f(fx-s, fy+b->height, fz+s);
            glEnd();
        }
    }
}

void render_particles() {
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < game->particle_count; i++) {
        Particle* p = &game->particles[i];
        glColor4f(p->r, p->g, p->b, p->life);
        glVertex3f(p->x, p->y, p->z);
    }
    glEnd();
}

void render_clouds() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    for (int i = 0; i < game->cloud_count; i++) {
        Cloud* c = &game->clouds[i];
        
        glColor4f(1.0f, 1.0f, 1.0f, c->density * 0.5f);
        
        // Draw cloud as multiple circles
        for (int j = 0; j < 5; j++) {
            float cx = c->x / (float)GRID_SIZE * 3.0f - 1.5f + (rand()%100-50)/500.0f;
            float cz = c->z / (float)GRID_SIZE * 3.0f - 1.5f + (rand()%100-50)/500.0f;
            
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(cx, c->y, cz);
            for (int a = 0; a <= 8; a++) {
                float angle = a * 3.14159f * 2 / 8;
                glVertex3f(cx + cosf(angle) * c->size/5, c->y, cz + sinf(angle) * c->size/5);
            }
            glEnd();
        }
    }
    
    glDisable(GL_BLEND);
}

void render_ui() {
    // Switch to 2D
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Tool bar
    glColor4f(0, 0, 0, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(10, 10);
    glVertex2f(500, 10);
    glVertex2f(500, 60);
    glVertex2f(10, 60);
    glEnd();
    
    // Tool selection
    const char* tools[] = {"Terrain", "Water", "Volcano", "City", "Forest"};
    for (int i = 0; i < 5; i++) {
        if (game->current_tool == i+1) {
            glColor3f(1, 1, 0);
            glBegin(GL_LINE_LOOP);
            glVertex2f(15 + i*95, 15);
            glVertex2f(105 + i*95, 15);
            glVertex2f(105 + i*95, 55);
            glVertex2f(15 + i*95, 55);
            glEnd();
        }
        
        // Tool icon placeholder
        glColor3f(1, 1, 1);
        glBegin(GL_LINES);
        glVertex2f(20 + i*95, 35);
        glVertex2f(100 + i*95, 35);
        glEnd();
    }
    
    // Stats panel
    if (game->show_stats) {
        glColor4f(0, 0, 0, 0.7f);
        glBegin(GL_QUADS);
        glVertex2f(10, 70);
        glVertex2f(250, 70);
        glVertex2f(250, 200);
        glVertex2f(10, 200);
        glEnd();
        
        // Stats text placeholder
        glColor3f(1, 1, 1);
        glBegin(GL_LINES);
        glVertex2f(20, 90); glVertex2f(240, 90);   // FPS
        glVertex2f(20, 110); glVertex2f(240, 110); // Time
        glVertex2f(20, 130); glVertex2f(240, 130); // Population
        glVertex2f(20, 150); glVertex2f(240, 150); // Trees
        glVertex2f(20, 170); glVertex2f(240, 170); // Particles
        glEnd();
    }
    
    // Time of day indicator
    float sun_x = WINDOW_WIDTH/2 + cosf((game->time_of_day/24.0f - 0.25f) * 2 * 3.14159f) * 100;
    float sun_y = 100 - sinf((game->time_of_day/24.0f - 0.25f) * 2 * 3.14159f) * 50;
    
    glColor3f(1, 1, 0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(sun_x, sun_y);
    for (int i = 0; i <= 16; i++) {
        float a = i * 3.14159f * 2 / 16;
        glVertex2f(sun_x + cosf(a) * 10, sun_y + sinf(a) * 10);
    }
    glEnd();
    
    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void render_frame() {
    // Sky color based on time of day
    float sun_height = sinf((game->time_of_day / 24.0f) * 2 * 3.14159f);
    float r = 0.1f + sun_height * 0.4f;
    float g = 0.2f + sun_height * 0.5f;
    float b = 0.4f + sun_height * 0.3f;
    glClearColor(r, g, b, 1.0f);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 3D camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)WINDOW_WIDTH / WINDOW_HEIGHT;
    float fov = 60.0f * 3.14159f / 180.0f;
    float near = 0.1f;
    float far = 100.0f;
    float top = near * tanf(fov * 0.5f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, near, far);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, -game->camera_height, -game->camera_distance);
    glRotatef(30, 1, 0, 0);
    glRotatef(game->camera_angle, 0, 1, 0);
    
    // Render everything
    render_terrain();
    render_trees();
    render_buildings();
    render_particles();
    render_clouds();
    render_ui();
}

// ============= MAIN =============

int main() {
    printf("=== CONTINENTAL ARCHITECT ULTRA ===\n");
    printf("1000%% More Bells and Whistles!\n\n");
    printf("Tools:\n");
    printf("  1-5: Select tools (Terrain/Water/Volcano/City/Forest)\n");
    printf("  Mouse: Apply tool\n");
    printf("  Shift+Mouse: Reverse tool\n\n");
    printf("Camera:\n");
    printf("  Q/E: Rotate camera\n");
    printf("  W/S: Zoom in/out\n");
    printf("  A/D: Camera height\n\n");
    printf("Simulation:\n");
    printf("  Space: Pause/Resume\n");
    printf("  +/-: Time speed\n");
    printf("  V: Trigger volcano\n");
    printf("  R: Rain\n\n");
    printf("Display:\n");
    printf("  Tab: Toggle stats\n");
    printf("  H: Help\n");
    printf("  ESC: Quit\n\n");
    
    // Initialize game state
    game = calloc(1, sizeof(GameState));
    game->camera_distance = 4.0f;
    game->camera_height = 1.0f;
    game->current_tool = 1;
    game->brush_size = 3;
    game->time_speed = 1;
    game->time_of_day = 12.0f;
    
    srand(time(NULL));
    init_terrain();
    
    // X11 and OpenGL setup
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    int scr = DefaultScreen(dpy);
    GLint att[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(dpy, scr, att);
    if (!vi) {
        fprintf(stderr, "No suitable visual\n");
        return 1;
    }
    
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask | ExposureMask;
    
    Window win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Continental Architect ULTRA");
    XFlush(dpy);
    XSync(dpy, False);
    usleep(100000);
    
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    printf("OpenGL: %s\n", glGetString(GL_VERSION));
    printf("Starting simulation...\n\n");
    
    // Main loop
    int running = 1;
    int paused = 0;
    game->last_fps_time = time(NULL);
    clock_t last_time = clock();
    
    while (running) {
        // Events
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                game->keys[key & 0xFF] = 1;
                
                switch(key) {
                    case XK_Escape: running = 0; break;
                    case XK_1: game->current_tool = 1; printf("Tool: Terrain\n"); break;
                    case XK_2: game->current_tool = 2; printf("Tool: Water\n"); break;
                    case XK_3: game->current_tool = 3; printf("Tool: Volcano\n"); break;
                    case XK_4: game->current_tool = 4; printf("Tool: City\n"); break;
                    case XK_5: game->current_tool = 5; printf("Tool: Forest\n"); break;
                    
                    case XK_q: game->camera_angle -= 5; break;
                    case XK_e: game->camera_angle += 5; break;
                    case XK_w: game->camera_distance *= 0.9f; break;
                    case XK_s: game->camera_distance *= 1.1f; break;
                    case XK_a: game->camera_height -= 0.1f; break;
                    case XK_d: game->camera_height += 0.1f; break;
                    
                    case XK_space: paused = !paused; break;
                    case XK_plus:
                    case XK_equal:
                        game->time_speed *= 2;
                        if (game->time_speed > 1000) game->time_speed = 1000;
                        printf("Time speed: %dx\n", game->time_speed);
                        break;
                    case XK_minus:
                        game->time_speed /= 2;
                        if (game->time_speed < 1) game->time_speed = 1;
                        printf("Time speed: %dx\n", game->time_speed);
                        break;
                    
                    case XK_v: trigger_volcano(); break;
                    case XK_r:
                        // Add rain clouds
                        if (game->cloud_count < MAX_CLOUDS) {
                            Cloud* c = &game->clouds[game->cloud_count++];
                            c->x = -GRID_SIZE/2;
                            c->y = 2.0f;
                            c->z = 0;
                            c->size = 2.0f;
                            c->density = 0.9f;
                            c->rain_rate = 1.0f;
                            printf("Rain storm approaching!\n");
                        }
                        break;
                    
                    case XK_Tab: game->show_stats = !game->show_stats; break;
                    case XK_h: game->show_help = !game->show_help; break;
                    
                    case XK_bracketleft:
                        game->brush_size--;
                        if (game->brush_size < 1) game->brush_size = 1;
                        printf("Brush size: %d\n", game->brush_size);
                        break;
                    case XK_bracketright:
                        game->brush_size++;
                        if (game->brush_size > 10) game->brush_size = 10;
                        printf("Brush size: %d\n", game->brush_size);
                        break;
                }
            } else if (xev.type == KeyRelease) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                game->keys[key & 0xFF] = 0;
            } else if (xev.type == ButtonPress) {
                game->mouse_down = 1;
                game->mouse_x = xev.xbutton.x;
                game->mouse_y = xev.xbutton.y;
                apply_tool(game->mouse_x, game->mouse_y);
            } else if (xev.type == ButtonRelease) {
                game->mouse_down = 0;
            } else if (xev.type == MotionNotify) {
                game->mouse_x = xev.xmotion.x;
                game->mouse_y = xev.xmotion.y;
                if (game->mouse_down) {
                    apply_tool(game->mouse_x, game->mouse_y);
                }
            }
        }
        
        // Update
        clock_t current_time = clock();
        float dt = (float)(current_time - last_time) / CLOCKS_PER_SEC;
        last_time = current_time;
        
        if (!paused) {
            dt *= game->time_speed;
            
            game->geological_time += dt * 0.001f; // Millions of years
            game->time_of_day += dt * 0.1f; // Hours
            if (game->time_of_day >= 24.0f) game->time_of_day -= 24.0f;
            
            simulate_water(dt);
            simulate_vegetation(dt);
            simulate_weather(dt);
            simulate_civilizations(dt);
            update_volcano(dt);
            update_particles(dt);
        }
        
        // Render
        render_frame();
        glXSwapBuffers(dpy, win);
        
        // FPS
        game->frame_count++;
        time_t now = time(NULL);
        if (now > game->last_fps_time) {
            game->fps = game->frame_count;
            game->frame_count = 0;
            game->last_fps_time = now;
            
            // Count totals
            int total_pop = 0;
            for (int i = 0; i < game->building_count; i++) {
                total_pop += game->buildings[i].population;
            }
            
            printf("FPS:%3.0f | Time:%02d:%02d | Pop:%d | Trees:%d | Particles:%d\n",
                   game->fps,
                   (int)game->time_of_day, (int)((game->time_of_day - (int)game->time_of_day) * 60),
                   total_pop, game->tree_count, game->particle_count);
        }
        
        usleep(16666); // 60 FPS target
    }
    
    // Cleanup
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    
    free(game);
    
    printf("\nThanks for playing Continental Architect ULTRA!\n");
    return 0;
}