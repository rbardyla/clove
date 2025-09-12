/*
    Continental Architect - Simplified Standalone Demo
    
    A complete playable demonstration of multi-scale physics concepts.
    This simplified version focuses on gameplay and visualization rather
    than the complete MLPDD implementation.
    
    Demonstrates:
    - Multi-scale time control (geological to real-time)
    - Interactive terrain shaping tools
    - Civilization placement and survival simulation
    - Disaster events and environmental challenges
    - Performance-oriented 60+ FPS gameplay
    
    Philosophy: Handmade, zero external dependencies, maximum performance
*/

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// TYPES AND CONSTANTS
// =============================================================================

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef float f32;
typedef double f64;
typedef int32_t b32;

typedef struct v2 { f32 x, y; } v2;
typedef struct v3 { f32 x, y, z; } v3;

#define WORLD_SIZE 20000.0f  // 20km world
#define TERRAIN_GRID_SIZE 256
#define MAX_CIVILIZATIONS 32
#define GEOLOGICAL_TIME_SCALE 1000000.0f

// =============================================================================
// GAME STATE
// =============================================================================

typedef enum tool_type {
    TOOL_TECTONIC_PUSH = 0,
    TOOL_TECTONIC_PULL,
    TOOL_WATER_SOURCE, 
    TOOL_CIVILIZATION,
    TOOL_INSPECT
} tool_type;

typedef struct terrain_cell {
    f32 height;          // Elevation in meters
    f32 water_depth;     // Water depth
    f32 stress_level;    // Geological stress
    f32 flow_rate;       // Water flow speed
    f32 sediment;        // Sediment concentration
} terrain_cell;

typedef struct civilization {
    v2 position;
    f32 population;
    f32 age_years;
    f32 stability_rating;
    f32 water_access;
    f32 disaster_resistance;
    b32 alive;
} civilization;

typedef struct game_state {
    // Terrain simulation
    terrain_cell terrain[TERRAIN_GRID_SIZE][TERRAIN_GRID_SIZE];
    
    // Civilizations
    civilization civilizations[MAX_CIVILIZATIONS];
    u32 civilization_count;
    
    // Player interaction
    tool_type selected_tool;
    f32 tool_strength;
    f32 tool_radius;
    v2 mouse_world_pos;
    b32 mouse_down;
    
    // Camera
    v3 camera_pos;
    f32 zoom_level;
    
    // Time control
    f32 time_scale;
    f64 geological_time;
    f64 total_time;
    
    // Statistics
    f32 total_population;
    u32 disasters_survived;
    f64 frame_time_ms;
    u32 fps;
    
    // Platform
    b32 running;
} game_state;

// =============================================================================
// TERRAIN SIMULATION
// =============================================================================

f32 noise(f32 x, f32 y) {
    // Simple hash-based noise
    i32 ix = (i32)floorf(x);
    i32 iy = (i32)floorf(y);
    
    f32 fx = x - ix;
    f32 fy = y - iy;
    
    // Hash coordinates
    u32 h = (u32)(ix * 73856093) ^ (u32)(iy * 19349663);
    h = h ^ (h >> 16);
    h = h * 0x85ebca6b;
    h = h ^ (h >> 13);
    h = h * 0xc2b2ae35;
    h = h ^ (h >> 16);
    
    return (f32)(h % 10000) / 10000.0f;
}

f32 fractal_noise(f32 x, f32 y) {
    f32 result = 0.0f;
    f32 amplitude = 1.0f;
    f32 frequency = 0.001f;
    
    for (int i = 0; i < 4; i++) {
        result += amplitude * noise(x * frequency, y * frequency);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return result;
}

void initialize_terrain(game_state* game) {
    for (u32 y = 0; y < TERRAIN_GRID_SIZE; y++) {
        for (u32 x = 0; x < TERRAIN_GRID_SIZE; x++) {
            f32 world_x = (f32)x / TERRAIN_GRID_SIZE * WORLD_SIZE - WORLD_SIZE * 0.5f;
            f32 world_y = (f32)y / TERRAIN_GRID_SIZE * WORLD_SIZE - WORLD_SIZE * 0.5f;
            
            terrain_cell* cell = &game->terrain[y][x];
            
            // Generate initial terrain with fractal noise
            cell->height = fractal_noise(world_x, world_y) * 2000.0f - 500.0f;
            cell->water_depth = fmaxf(0.0f, -cell->height * 0.1f);
            cell->stress_level = fabsf(cell->height) * 0.001f;
            cell->flow_rate = 0.0f;
            cell->sediment = 0.0f;
        }
    }
    
    printf("Terrain initialized: %dx%d grid covering %.0f km²\n", 
           TERRAIN_GRID_SIZE, TERRAIN_GRID_SIZE, WORLD_SIZE * WORLD_SIZE / 1000000.0f);
}

void apply_tectonic_tool(game_state* game, v2 pos, f32 strength, f32 radius, b32 push) {
    f32 grid_scale = TERRAIN_GRID_SIZE / WORLD_SIZE;
    i32 center_x = (i32)((pos.x + WORLD_SIZE * 0.5f) * grid_scale);
    i32 center_y = (i32)((pos.y + WORLD_SIZE * 0.5f) * grid_scale);
    i32 grid_radius = (i32)(radius * grid_scale);
    
    for (i32 dy = -grid_radius; dy <= grid_radius; dy++) {
        for (i32 dx = -grid_radius; dx <= grid_radius; dx++) {
            i32 x = center_x + dx;
            i32 y = center_y + dy;
            
            if (x >= 0 && x < TERRAIN_GRID_SIZE && y >= 0 && y < TERRAIN_GRID_SIZE) {
                f32 distance = sqrtf(dx*dx + dy*dy) / grid_radius;
                if (distance <= 1.0f) {
                    f32 force = strength * (1.0f - distance);
                    
                    terrain_cell* cell = &game->terrain[y][x];
                    
                    if (push) {
                        cell->height += force * 10.0f;  // Push up
                    } else {
                        cell->height -= force * 10.0f;  // Pull down
                    }
                    
                    // Increase geological stress
                    cell->stress_level += force * 0.1f;
                }
            }
        }
    }
}

void apply_water_tool(game_state* game, v2 pos, f32 strength) {
    f32 grid_scale = TERRAIN_GRID_SIZE / WORLD_SIZE;
    i32 x = (i32)((pos.x + WORLD_SIZE * 0.5f) * grid_scale);
    i32 y = (i32)((pos.y + WORLD_SIZE * 0.5f) * grid_scale);
    
    if (x >= 0 && x < TERRAIN_GRID_SIZE && y >= 0 && y < TERRAIN_GRID_SIZE) {
        terrain_cell* cell = &game->terrain[y][x];
        cell->water_depth += strength * 0.1f;
        cell->flow_rate = strength;
    }
}

void update_terrain_simulation(game_state* game, f32 dt) {
    f32 scaled_dt = dt * game->time_scale;
    
    // Simple erosion simulation
    for (u32 y = 1; y < TERRAIN_GRID_SIZE - 1; y++) {
        for (u32 x = 1; x < TERRAIN_GRID_SIZE - 1; x++) {
            terrain_cell* cell = &game->terrain[y][x];
            
            if (cell->water_depth > 0.1f) {
                // Calculate erosion based on water flow
                f32 erosion_rate = cell->flow_rate * 0.01f * scaled_dt;
                cell->height -= erosion_rate;
                cell->sediment += erosion_rate;
                
                // Simple water flow to lower neighbors
                terrain_cell* neighbors[4] = {
                    &game->terrain[y-1][x], &game->terrain[y+1][x],
                    &game->terrain[y][x-1], &game->terrain[y][x+1]
                };
                
                f32 total_flow = 0.0f;
                for (int i = 0; i < 4; i++) {
                    f32 height_diff = cell->height - neighbors[i]->height;
                    if (height_diff > 0.0f) {
                        f32 flow = height_diff * 0.001f * scaled_dt;
                        total_flow += flow;
                        neighbors[i]->water_depth += flow;
                        neighbors[i]->flow_rate += flow * 0.1f;
                    }
                }
                
                cell->water_depth -= total_flow;
                if (cell->water_depth < 0.0f) cell->water_depth = 0.0f;
            }
            
            // Geological stress evolution
            if (cell->stress_level > 1000.0f && (rand() % 10000) < 10) {
                // Stress release event (earthquake)
                cell->stress_level *= 0.5f;
                printf("Earthquake at grid (%u, %u)!\n", x, y);
            }
        }
    }
}

// =============================================================================
// CIVILIZATION SIMULATION
// =============================================================================

void place_civilization(game_state* game, v2 pos) {
    if (game->civilization_count >= MAX_CIVILIZATIONS) return;
    
    civilization* civ = &game->civilizations[game->civilization_count++];
    civ->position = pos;
    civ->population = 1000.0f;
    civ->age_years = 0.0f;
    civ->alive = true;
    
    // Calculate starting conditions based on terrain
    f32 grid_scale = TERRAIN_GRID_SIZE / WORLD_SIZE;
    i32 x = (i32)((pos.x + WORLD_SIZE * 0.5f) * grid_scale);
    i32 y = (i32)((pos.y + WORLD_SIZE * 0.5f) * grid_scale);
    
    if (x >= 0 && x < TERRAIN_GRID_SIZE && y >= 0 && y < TERRAIN_GRID_SIZE) {
        terrain_cell* cell = &game->terrain[y][x];
        civ->water_access = fminf(1.0f, cell->water_depth / 5.0f + 0.5f);
        civ->stability_rating = fmaxf(0.1f, 1.0f - cell->stress_level / 1000.0f);
        civ->disaster_resistance = civ->stability_rating * 0.5f;
    }
    
    printf("Civilization founded at (%.0f, %.0f) - Pop: %.0f, Water: %.2f, Stability: %.2f\n",
           pos.x, pos.y, civ->population, civ->water_access, civ->stability_rating);
}

void update_civilizations(game_state* game, f32 dt) {
    game->total_population = 0.0f;
    
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        if (!civ->alive) continue;
        
        civ->age_years += dt;
        
        // Population growth based on conditions
        f32 growth_rate = 0.02f;  // 2% base growth
        growth_rate *= civ->water_access;
        growth_rate *= civ->stability_rating;
        
        civ->population *= (1.0f + growth_rate * dt);
        
        // Technology improves disaster resistance
        f32 tech_level = fminf(2.0f, civ->age_years / 100.0f);  // Mature after 100 years
        civ->disaster_resistance = civ->stability_rating * 0.5f + tech_level * 0.3f;
        
        // Check for disasters
        f32 grid_scale = TERRAIN_GRID_SIZE / WORLD_SIZE;
        i32 x = (i32)((civ->position.x + WORLD_SIZE * 0.5f) * grid_scale);
        i32 y = (i32)((civ->position.y + WORLD_SIZE * 0.5f) * grid_scale);
        
        if (x >= 0 && x < TERRAIN_GRID_SIZE && y >= 0 && y < TERRAIN_GRID_SIZE) {
            terrain_cell* cell = &game->terrain[y][x];
            
            // Earthquake damage
            if (cell->stress_level > 800.0f) {
                f32 damage = (cell->stress_level - 800.0f) / 1000.0f;
                damage *= (1.0f - civ->disaster_resistance);
                civ->population *= (1.0f - damage * 0.5f);
                
                if (civ->population > 100.0f) {
                    game->disasters_survived++;
                }
            }
            
            // Flood damage
            if (cell->water_depth > 10.0f) {
                f32 flood_damage = (cell->water_depth - 10.0f) / 20.0f;
                flood_damage *= (1.0f - civ->disaster_resistance);
                civ->population *= (1.0f - flood_damage * 0.3f);
            }
        }
        
        // Death threshold
        if (civ->population < 50.0f) {
            civ->alive = false;
            printf("Civilization at (%.0f, %.0f) has perished after %.1f years\n",
                   civ->position.x, civ->position.y, civ->age_years);
        }
        
        game->total_population += civ->population;
    }
}

// =============================================================================
// RENDERING
// =============================================================================

void render_terrain(game_state* game) {
    glBegin(GL_TRIANGLES);
    
    f32 world_scale = WORLD_SIZE / TERRAIN_GRID_SIZE;
    
    for (u32 y = 0; y < TERRAIN_GRID_SIZE - 1; y++) {
        for (u32 x = 0; x < TERRAIN_GRID_SIZE - 1; x++) {
            // Get height values for quad vertices
            f32 h00 = game->terrain[y][x].height;
            f32 h10 = game->terrain[y][x+1].height;
            f32 h01 = game->terrain[y+1][x].height;
            f32 h11 = game->terrain[y+1][x+1].height;
            
            // World coordinates
            f32 wx0 = x * world_scale - WORLD_SIZE * 0.5f;
            f32 wx1 = (x+1) * world_scale - WORLD_SIZE * 0.5f;
            f32 wy0 = y * world_scale - WORLD_SIZE * 0.5f;
            f32 wy1 = (y+1) * world_scale - WORLD_SIZE * 0.5f;
            
            // Color based on height
            f32 color = (h00 + 1000.0f) / 2000.0f;  // Normalize -1000 to +1000m
            if (color < 0.0f) color = 0.0f;
            if (color > 1.0f) color = 1.0f;
            
            // Water areas are blue
            if (game->terrain[y][x].water_depth > 0.1f) {
                glColor3f(0.2f, 0.4f, 0.8f);  // Blue water
            } else {
                // Land colors from green (low) to white (high)
                if (color < 0.3f) {
                    glColor3f(0.2f, 0.8f, 0.2f);  // Green lowlands
                } else if (color < 0.7f) {
                    glColor3f(0.6f, 0.6f, 0.3f);  // Brown hills
                } else {
                    glColor3f(0.9f, 0.9f, 0.9f);  // White mountains
                }
            }
            
            // Render two triangles for each cell
            glVertex3f(wx0, h00, wy0);
            glVertex3f(wx1, h10, wy0);
            glVertex3f(wx0, h01, wy1);
            
            glVertex3f(wx1, h10, wy0);
            glVertex3f(wx1, h11, wy1);
            glVertex3f(wx0, h01, wy1);
        }
    }
    
    glEnd();
}

void render_civilizations(game_state* game) {
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    
    for (u32 i = 0; i < game->civilization_count; i++) {
        civilization* civ = &game->civilizations[i];
        if (!civ->alive) {
            glColor3f(0.5f, 0.5f, 0.5f);  // Gray for dead
        } else if (civ->population > 5000.0f) {
            glColor3f(0.2f, 1.0f, 0.2f);  // Bright green for thriving
        } else if (civ->population > 1000.0f) {
            glColor3f(1.0f, 1.0f, 0.2f);  // Yellow for stable
        } else {
            glColor3f(1.0f, 0.2f, 0.2f);  // Red for struggling
        }
        
        // Get terrain height at civilization position
        f32 grid_scale = TERRAIN_GRID_SIZE / WORLD_SIZE;
        i32 x = (i32)((civ->position.x + WORLD_SIZE * 0.5f) * grid_scale);
        i32 y = (i32)((civ->position.y + WORLD_SIZE * 0.5f) * grid_scale);
        
        f32 height = 0.0f;
        if (x >= 0 && x < TERRAIN_GRID_SIZE && y >= 0 && y < TERRAIN_GRID_SIZE) {
            height = game->terrain[y][x].height;
        }
        
        glVertex3f(civ->position.x, height + 50.0f, civ->position.y);
    }
    
    glEnd();
}

void render_ui(game_state* game) {
    // Switch to 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 800, 600, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    
    // Simple text rendering using colored rectangles
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Tool indicator
    glBegin(GL_QUADS);
    glVertex2f(10, 10);
    glVertex2f(200, 10);
    glVertex2f(200, 30);
    glVertex2f(10, 30);
    glEnd();
    
    // Statistics
    glBegin(GL_QUADS);
    glVertex2f(10, 40);
    glVertex2f(300, 40);
    glVertex2f(300, 120);
    glVertex2f(10, 120);
    glEnd();
    
    // Performance stats
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(10, 560);
    glVertex2f(200, 560);
    glVertex2f(200, 590);
    glVertex2f(10, 590);
    glEnd();
    
    // Restore 3D rendering
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void render_game(game_state* game) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up 3D camera
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Simple camera looking down at the world
    f32 eye_height = 1000.0f + game->zoom_level * 5000.0f;
    glTranslatef(-game->camera_pos.x, -eye_height, -game->camera_pos.z);
    
    // Render terrain
    render_terrain(game);
    
    // Render civilizations
    render_civilizations(game);
    
    // Render UI overlay
    render_ui(game);
}

// =============================================================================
// INPUT AND MAIN LOOP
// =============================================================================

void process_input(game_state* game, XEvent* event) {
    switch (event->type) {
        case KeyPress: {
            KeySym key = XLookupKeysym(&event->xkey, 0);
            
            switch (key) {
                case XK_Escape:
                    game->running = false;
                    break;
                case XK_1:
                    game->selected_tool = TOOL_TECTONIC_PUSH;
                    printf("Tool: Tectonic Push\n");
                    break;
                case XK_2:
                    game->selected_tool = TOOL_TECTONIC_PULL;
                    printf("Tool: Tectonic Pull\n");
                    break;
                case XK_3:
                    game->selected_tool = TOOL_WATER_SOURCE;
                    printf("Tool: Water Source\n");
                    break;
                case XK_4:
                    game->selected_tool = TOOL_CIVILIZATION;
                    printf("Tool: Place Civilization\n");
                    break;
                case XK_space:
                    game->time_scale = (game->time_scale > 1.0f) ? 1.0f : 100.0f;
                    printf("Time scale: %.0fx\n", game->time_scale);
                    break;
                case XK_plus:
                case XK_equal:
                    game->zoom_level *= 0.8f;
                    if (game->zoom_level < 0.1f) game->zoom_level = 0.1f;
                    break;
                case XK_minus:
                    game->zoom_level *= 1.25f;
                    if (game->zoom_level > 10.0f) game->zoom_level = 10.0f;
                    break;
            }
            break;
        }
        
        case ButtonPress: {
            if (event->xbutton.button == Button1) {
                game->mouse_down = true;
            }
            break;
        }
        
        case ButtonRelease: {
            if (event->xbutton.button == Button1) {
                game->mouse_down = false;
            }
            break;
        }
        
        case MotionNotify: {
            // Convert screen coordinates to world coordinates (simplified)
            f32 screen_x = (f32)event->xmotion.x / 800.0f - 0.5f;  // Normalize to -0.5 to 0.5
            f32 screen_y = (f32)event->xmotion.y / 600.0f - 0.5f;
            
            game->mouse_world_pos.x = screen_x * WORLD_SIZE * 0.5f * game->zoom_level;
            game->mouse_world_pos.y = screen_y * WORLD_SIZE * 0.5f * game->zoom_level;
            break;
        }
    }
}

void apply_tools(game_state* game) {
    if (!game->mouse_down) return;
    
    switch (game->selected_tool) {
        case TOOL_TECTONIC_PUSH:
            apply_tectonic_tool(game, game->mouse_world_pos, game->tool_strength, game->tool_radius, true);
            break;
        case TOOL_TECTONIC_PULL:
            apply_tectonic_tool(game, game->mouse_world_pos, game->tool_strength, game->tool_radius, false);
            break;
        case TOOL_WATER_SOURCE:
            apply_water_tool(game, game->mouse_world_pos, game->tool_strength);
            break;
        case TOOL_CIVILIZATION:
            if ((rand() % 30) == 0) {  // Don't place too rapidly
                place_civilization(game, game->mouse_world_pos);
            }
            break;
        case TOOL_INSPECT:
            // Show information about the area under cursor
            break;
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    printf("Continental Architect - Simplified Multi-Scale Physics Demo\n");
    printf("==========================================================\n");
    printf("Experience geological time and civilization management!\n\n");
    
    // Initialize X11 and OpenGL
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        printf("Cannot open X11 display\n");
        return 1;
    }
    
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    
    XVisualInfo* visual_info;
    GLint visual_attributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    
    visual_info = glXChooseVisual(display, screen, visual_attributes);
    if (!visual_info) {
        printf("Cannot find suitable OpenGL visual\n");
        return 1;
    }
    
    Colormap colormap = XCreateColormap(display, root, visual_info->visual, AllocNone);
    
    XSetWindowAttributes window_attributes;
    window_attributes.colormap = colormap;
    window_attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | 
                                  ButtonReleaseMask | PointerMotionMask;
    
    Window window = XCreateWindow(display, root, 100, 100, 800, 600, 0,
                                 visual_info->depth, InputOutput, visual_info->visual,
                                 CWColormap | CWEventMask, &window_attributes);
    
    XMapWindow(display, window);
    XStoreName(display, window, "Continental Architect - Multi-Scale Physics");
    
    GLXContext gl_context = glXCreateContext(display, visual_info, NULL, GL_TRUE);
    glXMakeCurrent(display, window, gl_context);
    
    // Initialize OpenGL
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    f32 aspect = 800.0f / 600.0f;
    f32 fov = 60.0f * M_PI / 180.0f;
    f32 f = 1.0f / tanf(fov * 0.5f);
    
    // Simple perspective projection
    f32 proj_matrix[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, -1.002f, -2.002f,
        0, 0, -1, 0
    };
    glMultMatrixf(proj_matrix);
    
    glMatrixMode(GL_MODELVIEW);
    
    // Initialize game state
    game_state* game = calloc(1, sizeof(game_state));
    game->running = true;
    game->selected_tool = TOOL_TECTONIC_PUSH;
    game->tool_strength = 1.0f;
    game->tool_radius = 1000.0f;
    game->zoom_level = 1.0f;
    game->time_scale = 1.0f;
    
    // Initialize simulation
    srand((u32)time(NULL));
    initialize_terrain(game);
    
    printf("Controls:\n");
    printf("  1-4: Select tools (Push, Pull, Water, Civilization)\n");
    printf("  Mouse: Click and drag to apply tools\n");
    printf("  Space: Toggle time acceleration\n");
    printf("  +/-: Zoom in/out\n");
    printf("  ESC: Exit\n\n");
    printf("Starting simulation...\n\n");
    
    // Main game loop
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    
    u32 frame_count = 0;
    
    while (game->running) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        f64 dt = (current_time.tv_sec - last_time.tv_sec) + 
                (current_time.tv_nsec - last_time.tv_nsec) / 1000000000.0;
        last_time = current_time;
        
        // Process events
        XEvent event;
        while (XPending(display)) {
            XNextEvent(display, &event);
            process_input(game, &event);
        }
        
        // Apply tools
        apply_tools(game);
        
        // Update simulation
        update_terrain_simulation(game, dt);
        update_civilizations(game, dt);
        
        game->geological_time += dt * game->time_scale * GEOLOGICAL_TIME_SCALE;
        game->total_time += dt;
        
        // Render
        f64 render_start = current_time.tv_sec + current_time.tv_nsec / 1000000000.0;
        render_game(game);
        glXSwapBuffers(display, window);
        
        struct timespec render_end_time;
        clock_gettime(CLOCK_MONOTONIC, &render_end_time);
        f64 render_end = render_end_time.tv_sec + render_end_time.tv_nsec / 1000000000.0;
        game->frame_time_ms = (render_end - render_start) * 1000.0;
        
        // Performance stats
        frame_count++;
        if (frame_count % 60 == 0) {
            game->fps = (u32)(1.0 / dt);
            printf("FPS: %u, Frame: %.2f ms, Pop: %.0f, Disasters: %u, Time: %.1f My\n",
                   game->fps, game->frame_time_ms, game->total_population,
                   game->disasters_survived, game->geological_time / 1000000.0f);
        }
        
        // Cap frame rate
        if (dt < 1.0/60.0) {
            usleep((unsigned int)((1.0/60.0 - dt) * 1000000));
        }
    }
    
    // Cleanup
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    
    printf("\nGame complete! Final stats:\n");
    printf("  Total time: %.1f seconds\n", game->total_time);
    printf("  Geological time: %.2f million years\n", game->geological_time / 1000000.0f);
    printf("  Civilizations placed: %u\n", game->civilization_count);
    printf("  Final population: %.0f\n", game->total_population);
    printf("  Disasters survived: %u\n", game->disasters_survived);
    
    free(game);
    return 0;
}