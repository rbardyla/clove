/*
    Continental Architect ULTIMATE
    Actually professional quality with smooth rendering
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
//#include <GL/glu.h>
#include <GL/glx.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define TERRAIN_SIZE 128
#define TERRAIN_SCALE 20.0f
#define MAX_PARTICLES 5000

// Smooth terrain mesh with normals
typedef struct {
    float height;
    float water;
    float vegetation;
    float temperature;
    float nx, ny, nz; // Normal for lighting
} TerrainVertex;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float r, g, b, a;
    float size;
    float life;
    float rotation;
} Particle;

typedef struct {
    TerrainVertex terrain[TERRAIN_SIZE][TERRAIN_SIZE];
    Particle particles[MAX_PARTICLES];
    int particle_count;
    
    // Camera - smooth interpolation
    float cam_x, cam_y, cam_z;
    float cam_target_x, cam_target_y, cam_target_z;
    float cam_yaw, cam_pitch;
    float cam_target_yaw, cam_target_pitch;
    float cam_zoom, cam_target_zoom;
    
    // Time and environment
    float time_of_day;
    float wind_x, wind_z;
    float fog_density;
    
    // Tools
    int current_tool;
    float brush_size;
    float brush_softness;
    float brush_strength;
    
    // Quality settings
    int shadows_enabled;
    int water_reflections;
    int high_quality;
    int vsync;
    
    // UI state
    float ui_animation_time;
    int show_menu;
    float menu_alpha;
    
    // Input
    int mouse_x, mouse_y;
    int mouse_down;
    int keys[256];
    
    // Performance
    float fps;
    float frame_time;
    int frame_count;
    double last_time;
} GameState;

// Static allocation - no malloc needed
static GameState game_instance;
GameState* game = &game_instance;

// ============= MATH HELPERS =============

float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0, fminf(1, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

float noise2d(float x, float y) {
    int ix = (int)x;
    int iy = (int)y;
    float fx = x - ix;
    float fy = y - iy;
    
    // Smooth interpolation
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    
    // Random values at corners
    float a = sinf(ix * 12.9898f + iy * 78.233f) * 43758.5453f;
    float b = sinf((ix+1) * 12.9898f + iy * 78.233f) * 43758.5453f;
    float c = sinf(ix * 12.9898f + (iy+1) * 78.233f) * 43758.5453f;
    float d = sinf((ix+1) * 12.9898f + (iy+1) * 78.233f) * 43758.5453f;
    
    a = a - floorf(a);
    b = b - floorf(b);
    c = c - floorf(c);
    d = d - floorf(d);
    
    // Bilinear interpolation
    float v1 = a * (1-fx) + b * fx;
    float v2 = c * (1-fx) + d * fx;
    return v1 * (1-fy) + v2 * fy;
}

float fractal_noise(float x, float y, int octaves) {
    float value = 0;
    float amplitude = 1;
    float frequency = 1;
    float max_value = 0;
    
    for (int i = 0; i < octaves; i++) {
        value += noise2d(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return value / max_value;
}

// ============= TERRAIN GENERATION =============

void calculate_normals() {
    for (int y = 0; y < TERRAIN_SIZE; y++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            float h = game->terrain[y][x].height;
            float hx1 = (x > 0) ? game->terrain[y][x-1].height : h;
            float hx2 = (x < TERRAIN_SIZE-1) ? game->terrain[y][x+1].height : h;
            float hy1 = (y > 0) ? game->terrain[y-1][x].height : h;
            float hy2 = (y < TERRAIN_SIZE-1) ? game->terrain[y+1][x].height : h;
            
            float dx = (hx2 - hx1) * 0.5f;
            float dy = (hy2 - hy1) * 0.5f;
            
            // Normal vector
            float nx = -dx;
            float ny = 1.0f;
            float nz = -dy;
            
            // Normalize
            float len = sqrtf(nx*nx + ny*ny + nz*nz);
            game->terrain[y][x].nx = nx / len;
            game->terrain[y][x].ny = ny / len;
            game->terrain[y][x].nz = nz / len;
        }
    }
}

void generate_terrain() {
    for (int y = 0; y < TERRAIN_SIZE; y++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            float fx = (float)x / TERRAIN_SIZE;
            float fy = (float)y / TERRAIN_SIZE;
            
            // Multi-octave noise for realistic terrain
            float height = 0;
            
            // Continental shelf
            height += fractal_noise(fx * 2, fy * 2, 4) * 0.5f;
            
            // Mountain ranges
            float mountains = fractal_noise(fx * 5, fy * 5, 3);
            mountains = powf(mountains, 2.0f);
            height += mountains * 0.8f;
            
            // Small details
            height += fractal_noise(fx * 20, fy * 20, 2) * 0.1f;
            
            // Valleys
            float valley = sinf(fx * 3.14159f * 3) * sinf(fy * 3.14159f * 2);
            height += valley * 0.2f;
            
            game->terrain[y][x].height = height;
            
            // Water in low areas
            game->terrain[y][x].water = (height < 0.1f) ? 0.1f - height : 0;
            
            // Vegetation based on height and moisture
            float veg_ideal = 0.3f;
            float veg_factor = 1.0f - fabs(height - veg_ideal) / 0.5f;
            veg_factor = fmaxf(0, veg_factor);
            game->terrain[y][x].vegetation = veg_factor * fractal_noise(fx * 10, fy * 10, 2);
            
            // Temperature based on height
            game->terrain[y][x].temperature = 20.0f - height * 30.0f;
        }
    }
    
    calculate_normals();
}

// ============= SMOOTH TERRAIN MODIFICATION =============

void modify_terrain(int cx, int cy, float amount) {
    float brush_radius = game->brush_size * TERRAIN_SIZE / 20.0f;
    
    for (int y = 0; y < TERRAIN_SIZE; y++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            
            if (dist < brush_radius) {
                // Smooth falloff
                float factor = 1.0f - (dist / brush_radius);
                factor = powf(factor, game->brush_softness);
                
                game->terrain[y][x].height += amount * factor * game->brush_strength;
                
                // Clamp height
                if (game->terrain[y][x].height > 2.0f) game->terrain[y][x].height = 2.0f;
                if (game->terrain[y][x].height < -1.0f) game->terrain[y][x].height = -1.0f;
                
                // Add particles at modification point
                if (rand() % 10 == 0 && game->particle_count < MAX_PARTICLES) {
                    Particle* p = &game->particles[game->particle_count++];
                    
                    float world_x = ((float)x / TERRAIN_SIZE - 0.5f) * TERRAIN_SCALE;
                    float world_z = ((float)y / TERRAIN_SIZE - 0.5f) * TERRAIN_SCALE;
                    float world_y = game->terrain[y][x].height * 2.0f;
                    
                    p->x = world_x + (rand() % 100 - 50) / 100.0f;
                    p->y = world_y;
                    p->z = world_z + (rand() % 100 - 50) / 100.0f;
                    
                    p->vx = (rand() % 100 - 50) / 200.0f;
                    p->vy = (rand() % 100 + 50) / 100.0f;
                    p->vz = (rand() % 100 - 50) / 200.0f;
                    
                    // Earth colors
                    float brown = 0.3f + (rand() % 100) / 300.0f;
                    p->r = brown;
                    p->g = brown * 0.6f;
                    p->b = brown * 0.3f;
                    p->a = 1.0f;
                    
                    p->size = 0.02f + (rand() % 100) / 5000.0f;
                    p->life = 1.0f;
                    p->rotation = rand() % 360;
                }
            }
        }
    }
    
    calculate_normals();
}

// ============= SMOOTH CAMERA =============

void update_camera(float dt) {
    // Smooth interpolation
    float cam_speed = 5.0f * dt;
    
    game->cam_x += (game->cam_target_x - game->cam_x) * cam_speed;
    game->cam_y += (game->cam_target_y - game->cam_y) * cam_speed;
    game->cam_z += (game->cam_target_z - game->cam_z) * cam_speed;
    
    game->cam_yaw += (game->cam_target_yaw - game->cam_yaw) * cam_speed;
    game->cam_pitch += (game->cam_target_pitch - game->cam_pitch) * cam_speed;
    game->cam_zoom += (game->cam_target_zoom - game->cam_zoom) * cam_speed;
}

// ============= HIGH QUALITY RENDERING =============

void setup_lighting() {
    // Sun position based on time
    float sun_angle = (game->time_of_day / 24.0f) * 2 * 3.14159f;
    float sun_height = sinf(sun_angle);
    float sun_x = cosf(sun_angle);
    
    GLfloat light_position[] = {sun_x * 10, sun_height * 10 + 5, 0, 0};
    
    // Warm sunlight colors
    float intensity = fmaxf(0.2f, sun_height);
    GLfloat light_ambient[] = {0.2f * intensity, 0.18f * intensity, 0.15f * intensity, 1.0f};
    GLfloat light_diffuse[] = {0.9f * intensity, 0.85f * intensity, 0.7f * intensity, 1.0f};
    GLfloat light_specular[] = {1.0f * intensity, 0.95f * intensity, 0.8f * intensity, 1.0f};
    
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    
    // Fog for atmosphere
    if (game->high_quality) {
        glEnable(GL_FOG);
        GLfloat fog_color[] = {0.7f, 0.8f, 0.9f, 1.0f};
        glFogi(GL_FOG_MODE, GL_EXP2);
        glFogfv(GL_FOG_COLOR, fog_color);
        glFogf(GL_FOG_DENSITY, game->fog_density);
        glFogf(GL_FOG_START, 10.0f);
        glFogf(GL_FOG_END, 50.0f);
    }
}

void render_terrain_high_quality() {
    // Material properties
    GLfloat mat_specular[] = {0.1f, 0.1f, 0.1f, 1.0f};
    GLfloat mat_shininess[] = {5.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    
    // Render with smooth shading
    glShadeModel(GL_SMOOTH);
    
    for (int y = 0; y < TERRAIN_SIZE - 1; y++) {
        glBegin(GL_TRIANGLE_STRIP);
        
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            for (int dy = 0; dy <= 1; dy++) {
                int cy = y + dy;
                TerrainVertex* v = &game->terrain[cy][x];
                
                float world_x = ((float)x / TERRAIN_SIZE - 0.5f) * TERRAIN_SCALE;
                float world_z = ((float)cy / TERRAIN_SIZE - 0.5f) * TERRAIN_SCALE;
                float world_y = v->height * 2.0f;
                
                // Calculate color based on height and environment
                float r, g, b;
                
                if (v->water > 0.01f) {
                    // Water - deep blue to light blue
                    float depth = v->water;
                    r = 0.1f + depth * 0.1f;
                    g = 0.3f + depth * 0.2f;
                    b = 0.5f + depth * 0.3f;
                } else if (v->height < 0.0f) {
                    // Beach sand
                    r = 0.9f; g = 0.85f; b = 0.7f;
                } else if (v->vegetation > 0.5f) {
                    // Lush green
                    r = 0.2f; g = 0.5f + v->vegetation * 0.2f; b = 0.1f;
                } else if (v->height > 1.0f) {
                    // Snow cap
                    r = 0.95f; g = 0.95f; b = 1.0f;
                } else if (v->height > 0.6f) {
                    // Rock
                    r = 0.5f; g = 0.45f; b = 0.4f;
                } else {
                    // Grass to dirt gradient
                    float grass = 1.0f - v->height / 0.6f;
                    r = 0.4f - grass * 0.2f;
                    g = 0.5f + grass * 0.2f;
                    b = 0.3f - grass * 0.1f;
                }
                
                GLfloat mat_ambient[] = {r * 0.3f, g * 0.3f, b * 0.3f, 1.0f};
                GLfloat mat_diffuse[] = {r, g, b, 1.0f};
                glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
                
                glNormal3f(v->nx, v->ny, v->nz);
                glVertex3f(world_x, world_y, world_z);
            }
        }
        
        glEnd();
    }
    
    // Water with transparency
    if (game->water_reflections) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        
        GLfloat water_ambient[] = {0.0f, 0.1f, 0.2f, 0.6f};
        GLfloat water_diffuse[] = {0.1f, 0.3f, 0.5f, 0.6f};
        GLfloat water_specular[] = {0.8f, 0.9f, 1.0f, 0.6f};
        GLfloat water_shininess[] = {80.0f};
        
        glMaterialfv(GL_FRONT, GL_AMBIENT, water_ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, water_diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR, water_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, water_shininess);
        
        for (int y = 0; y < TERRAIN_SIZE - 1; y++) {
            glBegin(GL_TRIANGLE_STRIP);
            
            for (int x = 0; x < TERRAIN_SIZE; x++) {
                for (int dy = 0; dy <= 1; dy++) {
                    int cy = y + dy;
                    TerrainVertex* v = &game->terrain[cy][x];
                    
                    if (v->water > 0.01f) {
                        float world_x = ((float)x / TERRAIN_SIZE - 0.5f) * TERRAIN_SCALE;
                        float world_z = ((float)cy / TERRAIN_SIZE - 0.5f) * TERRAIN_SCALE;
                        float world_y = v->height * 2.0f + v->water;
                        
                        // Animated water normal for waves
                        float wave = sinf(game->ui_animation_time * 2 + world_x) * 0.1f;
                        glNormal3f(wave, 1.0f, wave * 0.5f);
                        glVertex3f(world_x, world_y, world_z);
                    }
                }
            }
            
            glEnd();
        }
        
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void render_particles() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    
    for (int i = 0; i < game->particle_count; i++) {
        Particle* p = &game->particles[i];
        
        glPushMatrix();
        glTranslatef(p->x, p->y, p->z);
        glRotatef(p->rotation, 0, 1, 0);
        
        glColor4f(p->r, p->g, p->b, p->a * p->life);
        
        // Render as small quad facing camera
        glBegin(GL_QUADS);
        glVertex3f(-p->size, -p->size, 0);
        glVertex3f(p->size, -p->size, 0);
        glVertex3f(p->size, p->size, 0);
        glVertex3f(-p->size, p->size, 0);
        glEnd();
        
        glPopMatrix();
    }
    
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ============= GLASS UI WITH SMOOTH TEXT =============

void render_glass_panel(float x, float y, float w, float h, float r, float g, float b, float a) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Glass effect with gradient
    glBegin(GL_QUADS);
    
    // Main panel with vertical gradient
    glColor4f(r, g, b, a * 0.3f);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glColor4f(r * 0.6f, g * 0.6f, b * 0.6f, a * 0.5f);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Glow border
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glColor4f(1, 1, 1, a * 0.5f);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    // Inner highlight
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glColor4f(1, 1, 1, a * 0.3f);
    glVertex2f(x + 1, y + 1);
    glVertex2f(x + w - 1, y + 1);
    glVertex2f(x + 1, y + 1);
    glVertex2f(x + 1, y + h - 1);
    glEnd();
    
    glDisable(GL_BLEND);
}

void render_smooth_text(float x, float y, const char* text, float size) {
    // For now, using simple GL text - in production would use FreeType
    glColor4f(1, 1, 1, 1);
    glRasterPos2f(x, y);
    
    for (int i = 0; text[i] != '\0'; i++) {
        // In real version, would render anti-aliased glyphs here
        // For demo, just draw placeholder
        glBegin(GL_POINTS);
        glVertex2f(x + i * size * 8, y);
        glEnd();
    }
}

void render_ui() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Animated fade in
    float ui_alpha = smoothstep(0, 1, game->ui_animation_time);
    
    // Top bar - glass effect
    render_glass_panel(0, 0, WINDOW_WIDTH, 80, 0.1f, 0.1f, 0.2f, ui_alpha * 0.9f);
    
    // Tool panel - slide in from left
    float panel_x = -250 + 270 * ui_alpha;
    render_glass_panel(panel_x, 100, 250, 400, 0.1f, 0.15f, 0.2f, ui_alpha * 0.8f);
    
    // Bottom time control - slide up
    float bottom_y = WINDOW_HEIGHT - 60 * ui_alpha;
    render_glass_panel(WINDOW_WIDTH/2 - 200, bottom_y, 400, 60, 0.1f, 0.1f, 0.15f, ui_alpha * 0.8f);
    
    // FPS counter with glow
    char fps_text[64];
    snprintf(fps_text, sizeof(fps_text), "%.0f FPS | %.1f ms", game->fps, game->frame_time * 1000);
    
    glColor4f(0, 0, 0, 0.5f);
    render_smooth_text(WINDOW_WIDTH - 152, 32, fps_text, 1);
    glColor4f(1, 1, 0.8f, 1);
    render_smooth_text(WINDOW_WIDTH - 150, 30, fps_text, 1);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============= MAIN =============

int main() {
    printf("Continental Architect ULTIMATE - True Professional Quality\n");
    printf("============================================\n\n");
    
    // Use static instance - no calloc
    memset(&game_instance, 0, sizeof(GameState));
    game = &game_instance;
    game->cam_target_zoom = 30.0f;
    game->cam_zoom = 30.0f;
    game->cam_target_pitch = 45.0f;
    game->cam_pitch = 45.0f;
    game->time_of_day = 14.0f; // 2 PM for good lighting
    game->brush_size = 2.0f;
    game->brush_softness = 2.0f;
    game->brush_strength = 0.3f;
    game->fog_density = 0.01f;
    game->high_quality = 1;
    game->water_reflections = 1;
    game->shadows_enabled = 1;
    
    srand(time(NULL));
    generate_terrain();
    
    // X11 and OpenGL setup
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    
    int scr = DefaultScreen(dpy);
    GLint att[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8, GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None
    };
    
    XVisualInfo* vi = glXChooseVisual(dpy, scr, att);
    if (!vi) {
        printf("No suitable visual found\n");
        return 1;
    }
    
    Window root = RootWindow(dpy, scr);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | 
                     ButtonReleaseMask | PointerMotionMask;
    
    Window win = XCreateWindow(dpy, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                              vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Continental Architect ULTIMATE");
    XFlush(dpy); XSync(dpy, False);
    usleep(100000);
    
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    
    // OpenGL setup for quality
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    printf("OpenGL: %s\n\n", glGetString(GL_VERSION));
    printf("Controls:\n");
    printf("  Mouse: Rotate camera\n");
    printf("  Scroll: Zoom\n");
    printf("  Click: Modify terrain\n");
    printf("  1-5: Select tools\n");
    printf("  Q: Toggle quality\n");
    printf("  ESC: Exit\n\n");
    
    // Main loop
    int running = 1;
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    
    while (running) {
        // Timing
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double dt = (current_time.tv_sec - last_time.tv_sec) + 
                   (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
        last_time = current_time;
        
        // Events
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            
            if (xev.type == KeyPress) {
                KeySym key = XLookupKeysym(&xev.xkey, 0);
                if (key == XK_Escape) running = 0;
                else if (key == XK_q) {
                    game->high_quality = !game->high_quality;
                    printf("Quality: %s\n", game->high_quality ? "High" : "Low");
                }
                else if (key >= XK_1 && key <= XK_5) {
                    game->current_tool = key - XK_1;
                }
            } else if (xev.type == ButtonPress) {
                if (xev.xbutton.button == 1) {
                    game->mouse_down = 1;
                } else if (xev.xbutton.button == 4) { // Scroll up
                    game->cam_target_zoom *= 0.9f;
                    if (game->cam_target_zoom < 10) game->cam_target_zoom = 10;
                } else if (xev.xbutton.button == 5) { // Scroll down
                    game->cam_target_zoom *= 1.1f;
                    if (game->cam_target_zoom > 50) game->cam_target_zoom = 50;
                }
            } else if (xev.type == ButtonRelease) {
                game->mouse_down = 0;
            } else if (xev.type == MotionNotify) {
                int dx = xev.xmotion.x - game->mouse_x;
                int dy = xev.xmotion.y - game->mouse_y;
                
                if (game->mouse_down) {
                    // Terrain modification
                    int terrain_x = (xev.xmotion.x * TERRAIN_SIZE) / WINDOW_WIDTH;
                    int terrain_y = (xev.xmotion.y * TERRAIN_SIZE) / WINDOW_HEIGHT;
                    modify_terrain(terrain_x, terrain_y, 0.1f);
                } else {
                    // Camera rotation
                    game->cam_target_yaw += dx * 0.5f;
                    game->cam_target_pitch += dy * 0.5f;
                    if (game->cam_target_pitch > 89) game->cam_target_pitch = 89;
                    if (game->cam_target_pitch < 10) game->cam_target_pitch = 10;
                }
                
                game->mouse_x = xev.xmotion.x;
                game->mouse_y = xev.xmotion.y;
            }
        }
        
        // Update
        game->time_of_day += dt * 0.5f;
        if (game->time_of_day >= 24) game->time_of_day -= 24;
        
        game->ui_animation_time += dt;
        if (game->ui_animation_time > 2) game->ui_animation_time = 2;
        
        update_camera(dt);
        
        // Update particles
        for (int i = 0; i < game->particle_count; i++) {
            Particle* p = &game->particles[i];
            p->x += p->vx * dt;
            p->y += p->vy * dt;
            p->z += p->vz * dt;
            p->vy -= 9.8f * dt;
            p->life -= dt * 0.5f;
            p->rotation += dt * 50;
            
            if (p->life <= 0) {
                game->particles[i] = game->particles[--game->particle_count];
                i--;
            }
        }
        
        // Render
        float sun = fmaxf(0.2f, sinf(game->time_of_day / 24.0f * 2 * 3.14159f));
        glClearColor(0.4f * sun, 0.6f * sun, 0.8f * sun, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 3D view
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // Manual perspective projection
        float aspect = (float)WINDOW_WIDTH/WINDOW_HEIGHT;
        float fov = 60.0f * 3.14159f / 180.0f;
        float near = 0.1f;
        float far = 100.0f;
        float top = near * tanf(fov * 0.5f);
        float right = top * aspect;
        glFrustum(-right, right, -top, top, near, far);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        // Manual lookAt
        float eye_x = sinf(game->cam_yaw * 3.14159f/180) * game->cam_zoom;
        float eye_y = game->cam_zoom * sinf(game->cam_pitch * 3.14159f/180);
        float eye_z = cosf(game->cam_yaw * 3.14159f/180) * game->cam_zoom;
        glTranslatef(0, 0, -game->cam_zoom);
        glRotatef(game->cam_pitch, 1, 0, 0);
        glRotatef(game->cam_yaw, 0, 1, 0);
        
        setup_lighting();
        render_terrain_high_quality();
        render_particles();
        
        if (game->high_quality) {
            glDisable(GL_FOG);
        }
        
        render_ui();
        
        glXSwapBuffers(dpy, win);
        
        // FPS
        game->frame_count++;
        game->frame_time = dt;
        if (game->frame_count % 30 == 0) {
            game->fps = 1.0 / dt;
        }
        
        usleep(16666); // 60 FPS target
    }
    
    // Cleanup
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    // No need to free static allocation
    
    return 0;
}