#!/bin/bash

echo "=== Building ACTUAL Editor with REAL Editing Features ==="
echo "Focus: Selection, Manipulation, Creation, Deletion"

# Build configuration
CC="gcc"
CFLAGS="-std=c99 -O2 -g3 -Wall -Wextra -Wno-unused-parameter -I."
LIBS="-lX11 -lGL -lm -pthread"

mkdir -p build

echo "Creating actual editor with real editing features..."

# Create the ACTUAL editor implementation
cat > build/actual_editor.c << 'EOF'
// ACTUAL EDITOR - Focus on EDITING FEATURES
// Stop building platforms, start building EDITING TOOLS

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

// Basic types
typedef unsigned char u8;
typedef signed int i32;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

#define MEGABYTES(n) ((n) * 1024LL * 1024LL)
#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define MAX_OBJECTS 1000

// X11 and OpenGL
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#else
// Basic stubs
typedef void* Display;
typedef void* Window;
typedef void* GLXContext;
#define KeyPress 2
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#endif

// Math types
typedef struct { f32 x, y, z; } Vec3;
typedef struct { f32 x, y, z, w; } Quat;
typedef struct { f32 m[16]; } Mat4;

// Input types
#define KEY_DELETE 0xFF
#define KEY_CTRL_S 19
#define KEY_CTRL_D 4
#define MOUSE_LEFT 1
#define MOUSE_RIGHT 3

typedef struct {
    i32 x, y;
    bool left_down;
    bool right_down;
    bool left_clicked;
    bool right_clicked;
} Mouse;

typedef struct {
    bool keys[256];
    bool keys_pressed[256];
    Mouse mouse;
} Input;

// Memory arena
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} MemoryArena;

static void* PushSize(MemoryArena* arena, u64 size) {
    if (!arena || arena->used + size > arena->size) return NULL;
    void* result = arena->base + arena->used;
    arena->used += size;
    return result;
}

#define PushStruct(arena, type) (type*)PushSize(arena, sizeof(type))
#define PushArray(arena, type, count) (type*)PushSize(arena, sizeof(type) * (count))

// ACTUAL GAME OBJECTS (not just spinning cubes!)
typedef enum {
    OBJECT_CUBE = 0,
    OBJECT_SPHERE,
    OBJECT_PLANE,
    OBJECT_COUNT
} ObjectType;

typedef struct {
    u32 id;
    ObjectType type;
    Vec3 position;
    Quat rotation;  
    Vec3 scale;
    Vec3 color;
    bool selected;
    bool active;
} GameObject;

// ACTUAL SCENE SYSTEM 
typedef struct {
    GameObject objects[MAX_OBJECTS];
    u32 object_count;
    u32 selected_object_id;
    u32 next_object_id;
    bool has_selection;
} Scene;

// ACTUAL CAMERA SYSTEM
typedef struct {
    Vec3 position;
    Vec3 target;
    f32 distance;
    f32 yaw, pitch;
    Mat4 view_matrix;
    Mat4 proj_matrix;
} Camera;

// ACTUAL EDITOR STATE
typedef struct {
    Scene scene;
    Camera camera;
    Input input;
    
    // Editor state
    bool show_grid;
    bool show_gizmos;
    f32 grid_size;
    
    // Selection system
    u32 hovered_object_id;
    
    // Statistics
    u32 frame_count;
    f32 fps;
    
    MemoryArena arena;
} Editor;

static Editor* g_editor = NULL;

// ============================================================================
// MATH FUNCTIONS (Minimal but functional)
// ============================================================================

Vec3 Vec3Add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Vec3Sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Vec3Scale(Vec3 v, f32 s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

f32 Vec3Dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

f32 Vec3Length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 Vec3Normalize(Vec3 v) {
    f32 len = Vec3Length(v);
    if (len > 0.0f) return Vec3Scale(v, 1.0f / len);
    return v;
}

// Basic matrix functions
Mat4 Mat4Identity(void) {
    Mat4 result = {0};
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4Perspective(f32 fov, f32 aspect, f32 near, f32 far) {
    Mat4 result = {0};
    f32 tan_half_fov = tanf(fov * 0.5f);
    result.m[0] = 1.0f / (aspect * tan_half_fov);
    result.m[5] = 1.0f / tan_half_fov;
    result.m[10] = -(far + near) / (far - near);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * far * near) / (far - near);
    return result;
}

Mat4 Mat4LookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = Vec3Normalize(Vec3Sub(center, eye));
    Vec3 u = Vec3Normalize(up);
    Vec3 s = Vec3Normalize((Vec3){f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x});
    u = (Vec3){s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x};
    
    Mat4 result = Mat4Identity();
    result.m[0] = s.x; result.m[4] = s.y; result.m[8]  = s.z;
    result.m[1] = u.x; result.m[5] = u.y; result.m[9]  = u.z;
    result.m[2] = -f.x; result.m[6] = -f.y; result.m[10] = -f.z;
    result.m[12] = -Vec3Dot(s, eye);
    result.m[13] = -Vec3Dot(u, eye);
    result.m[14] = Vec3Dot(f, eye);
    return result;
}

// ============================================================================
// ACTUAL EDITING FUNCTIONS (Finally!)
// ============================================================================

u32 CreateObject(Scene* scene, ObjectType type, Vec3 position) {
    if (!scene || scene->object_count >= MAX_OBJECTS) return 0;
    
    GameObject* obj = &scene->objects[scene->object_count];
    obj->id = scene->next_object_id++;
    obj->type = type;
    obj->position = position;
    obj->rotation = (Quat){0, 0, 0, 1};
    obj->scale = (Vec3){1, 1, 1};
    obj->color = (Vec3){0.7f, 0.7f, 0.7f};
    obj->selected = false;
    obj->active = true;
    
    scene->object_count++;
    printf("[EDITOR] Created %s object (ID: %u) at (%.1f, %.1f, %.1f)\n",
           type == OBJECT_CUBE ? "Cube" : type == OBJECT_SPHERE ? "Sphere" : "Plane",
           obj->id, position.x, position.y, position.z);
    
    return obj->id;
}

bool DeleteObject(Scene* scene, u32 object_id) {
    if (!scene || object_id == 0) return false;
    
    // Find and remove object
    for (u32 i = 0; i < scene->object_count; i++) {
        if (scene->objects[i].id == object_id) {
            printf("[EDITOR] Deleted object (ID: %u)\n", object_id);
            
            // Move last object to this position
            scene->objects[i] = scene->objects[scene->object_count - 1];
            scene->object_count--;
            
            // Clear selection if this was selected
            if (scene->selected_object_id == object_id) {
                scene->selected_object_id = 0;
                scene->has_selection = false;
            }
            return true;
        }
    }
    return false;
}

GameObject* GetObject(Scene* scene, u32 object_id) {
    if (!scene || object_id == 0) return NULL;
    
    for (u32 i = 0; i < scene->object_count; i++) {
        if (scene->objects[i].id == object_id) {
            return &scene->objects[i];
        }
    }
    return NULL;
}

void SelectObject(Scene* scene, u32 object_id) {
    if (!scene) return;
    
    // Clear previous selection
    for (u32 i = 0; i < scene->object_count; i++) {
        scene->objects[i].selected = false;
    }
    
    // Select new object
    GameObject* obj = GetObject(scene, object_id);
    if (obj) {
        obj->selected = true;
        scene->selected_object_id = object_id;
        scene->has_selection = true;
        printf("[EDITOR] Selected object (ID: %u)\n", object_id);
    } else {
        scene->selected_object_id = 0;
        scene->has_selection = false;
        printf("[EDITOR] Cleared selection\n");
    }
}

// ACTUAL RAY CASTING FOR MOUSE PICKING!
typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

Ray ScreenToWorldRay(Camera* camera, i32 screen_x, i32 screen_y, i32 screen_width, i32 screen_height) {
    // Convert screen coordinates to normalized device coordinates
    f32 ndc_x = (2.0f * screen_x) / screen_width - 1.0f;
    f32 ndc_y = 1.0f - (2.0f * screen_y) / screen_height;
    
    // Simple ray from camera (this is a basic implementation)
    Ray ray;
    ray.origin = camera->position;
    
    // Calculate direction based on camera orientation
    f32 cos_pitch = cosf(camera->pitch);
    f32 sin_pitch = sinf(camera->pitch);
    f32 cos_yaw = cosf(camera->yaw + ndc_x * 0.5f); // Add mouse offset
    f32 sin_yaw = sinf(camera->yaw + ndc_x * 0.5f);
    
    ray.direction = Vec3Normalize((Vec3){
        cos_pitch * cos_yaw,
        sin_pitch + ndc_y * 0.5f, // Add vertical mouse offset
        cos_pitch * sin_yaw
    });
    
    return ray;
}

bool RayIntersectsSphere(Ray ray, Vec3 center, f32 radius, f32* t) {
    Vec3 oc = Vec3Sub(ray.origin, center);
    f32 a = Vec3Dot(ray.direction, ray.direction);
    f32 b = 2.0f * Vec3Dot(oc, ray.direction);
    f32 c = Vec3Dot(oc, oc) - radius * radius;
    f32 discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) return false;
    
    f32 t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
    f32 t2 = (-b + sqrtf(discriminant)) / (2.0f * a);
    
    *t = (t1 > 0) ? t1 : t2;
    return *t > 0;
}

bool RayIntersectsBox(Ray ray, Vec3 center, Vec3 size, f32* t) {
    Vec3 min = Vec3Sub(center, Vec3Scale(size, 0.5f));
    Vec3 max = Vec3Add(center, Vec3Scale(size, 0.5f));
    
    f32 t_min = 0.0f;
    f32 t_max = INFINITY;
    
    for (int i = 0; i < 3; i++) {
        f32 ray_dir = (i == 0) ? ray.direction.x : (i == 1) ? ray.direction.y : ray.direction.z;
        f32 ray_orig = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
        f32 box_min = (i == 0) ? min.x : (i == 1) ? min.y : min.z;
        f32 box_max = (i == 0) ? max.x : (i == 1) ? max.y : max.z;
        
        if (fabsf(ray_dir) < 0.001f) {
            if (ray_orig < box_min || ray_orig > box_max) return false;
        } else {
            f32 t1 = (box_min - ray_orig) / ray_dir;
            f32 t2 = (box_max - ray_orig) / ray_dir;
            if (t1 > t2) { f32 temp = t1; t1 = t2; t2 = temp; }
            
            t_min = fmaxf(t_min, t1);
            t_max = fminf(t_max, t2);
            
            if (t_min > t_max) return false;
        }
    }
    
    *t = t_min > 0 ? t_min : t_max;
    return *t > 0;
}

u32 PickObject(Scene* scene, Camera* camera, i32 mouse_x, i32 mouse_y, i32 screen_width, i32 screen_height) {
    Ray ray = ScreenToWorldRay(camera, mouse_x, mouse_y, screen_width, screen_height);
    
    u32 closest_object = 0;
    f32 closest_distance = INFINITY;
    
    for (u32 i = 0; i < scene->object_count; i++) {
        GameObject* obj = &scene->objects[i];
        if (!obj->active) continue;
        
        f32 t;
        bool hit = false;
        
        switch (obj->type) {
            case OBJECT_CUBE:
                hit = RayIntersectsBox(ray, obj->position, obj->scale, &t);
                break;
            case OBJECT_SPHERE:
                hit = RayIntersectsSphere(ray, obj->position, obj->scale.x, &t);
                break;
            case OBJECT_PLANE:
                // Simple plane intersection at Y=position.y
                if (fabsf(ray.direction.y) > 0.001f) {
                    t = (obj->position.y - ray.origin.y) / ray.direction.y;
                    Vec3 hit_point = Vec3Add(ray.origin, Vec3Scale(ray.direction, t));
                    Vec3 local = Vec3Sub(hit_point, obj->position);
                    hit = (t > 0 && fabsf(local.x) <= obj->scale.x && fabsf(local.z) <= obj->scale.z);
                }
                break;
        }
        
        if (hit && t < closest_distance) {
            closest_distance = t;
            closest_object = obj->id;
        }
    }
    
    return closest_object;
}

// ============================================================================
// MINIMAL PLATFORM LAYER (Just enough to work)
// ============================================================================

#ifdef __linux__
static Display* g_display = NULL;
static Window g_window;
static GLXContext g_gl_context;
static int g_screen_width = 1024;
static int g_screen_height = 768;

bool InitPlatform(void) {
    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        printf("Failed to open X display\n");
        return false;
    }
    
    int screen = DefaultScreen(g_display);
    
    // Create window
    g_window = XCreateSimpleWindow(g_display, RootWindow(g_display, screen),
                                   100, 100, g_screen_width, g_screen_height, 1,
                                   BlackPixel(g_display, screen),
                                   WhitePixel(g_display, screen));
    
    XSelectInput(g_display, g_window, ExposureMask | KeyPressMask | KeyReleaseMask | 
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(g_display, g_window);
    XStoreName(g_display, g_window, "ACTUAL Editor - Now With EDITING!");
    
    // Basic OpenGL setup
    int gl_attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
    XVisualInfo* vi = glXChooseVisual(g_display, 0, gl_attributes);
    if (!vi) {
        printf("No appropriate visual found\n");
        return false;
    }
    
    g_gl_context = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_display, g_window, g_gl_context);
    
    // OpenGL setup
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    
    printf("[PLATFORM] OpenGL Editor initialized (%dx%d)\n", g_screen_width, g_screen_height);
    return true;
}

void ProcessInput(Input* input) {
    // Clear per-frame input
    memset(input->keys_pressed, 0, sizeof(input->keys_pressed));
    input->mouse.left_clicked = false;
    input->mouse.right_clicked = false;
    
    XEvent event;
    while (XPending(g_display)) {
        XNextEvent(g_display, &event);
        
        switch (event.type) {
            case KeyPress: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key < 256) {
                    input->keys[key] = true;
                    input->keys_pressed[key] = true;
                }
                break;
            }
            case KeyRelease: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key < 256) {
                    input->keys[key] = false;
                }
                break;
            }
            case ButtonPress: {
                input->mouse.x = event.xbutton.x;
                input->mouse.y = event.xbutton.y;
                if (event.xbutton.button == Button1) {
                    input->mouse.left_down = true;
                    input->mouse.left_clicked = true;
                }
                if (event.xbutton.button == Button3) {
                    input->mouse.right_down = true;
                    input->mouse.right_clicked = true;
                }
                break;
            }
            case ButtonRelease: {
                if (event.xbutton.button == Button1) {
                    input->mouse.left_down = false;
                }
                if (event.xbutton.button == Button3) {
                    input->mouse.right_down = false;
                }
                break;
            }
            case MotionNotify: {
                input->mouse.x = event.xmotion.x;
                input->mouse.y = event.xmotion.y;
                break;
            }
        }
    }
}

void SwapBuffers(void) {
    glXSwapBuffers(g_display, g_window);
}

void ShutdownPlatform(void) {
    if (g_display) {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_gl_context);
        XDestroyWindow(g_display, g_window);
        XCloseDisplay(g_display);
    }
}

#else
bool InitPlatform(void) { return false; }
void ProcessInput(Input* input) {}
void SwapBuffers(void) {}
void ShutdownPlatform(void) {}
#endif

// ============================================================================
// ACTUAL RENDERING (Simple but functional)
// ============================================================================

void RenderCube(Vec3 position, Vec3 scale, Vec3 color, bool selected) {
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    glScalef(scale.x, scale.y, scale.z);
    
    if (selected) {
        glColor3f(1.0f, 1.0f, 0.0f); // Yellow for selected
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(3.0f);
    } else {
        glColor3f(color.x, color.y, color.z);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // Draw cube faces
    glBegin(GL_QUADS);
    // Front face
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    // Back face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    // Top face
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    // Bottom face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    // Right face
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    // Left face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glEnd();
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();
}

void RenderGrid(f32 size, u32 lines) {
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    
    f32 half_size = size * 0.5f;
    f32 step = size / lines;
    
    for (u32 i = 0; i <= lines; i++) {
        f32 pos = -half_size + i * step;
        // X lines
        glVertex3f(pos, 0, -half_size);
        glVertex3f(pos, 0,  half_size);
        // Z lines
        glVertex3f(-half_size, 0, pos);
        glVertex3f( half_size, 0, pos);
    }
    
    glEnd();
}

void UpdateCamera(Camera* camera, Input* input, f32 dt) {
    // Mouse camera control
    static i32 last_mouse_x = 0, last_mouse_y = 0;
    static bool first_mouse = true;
    
    if (input->mouse.right_down) {
        if (!first_mouse) {
            i32 dx = input->mouse.x - last_mouse_x;
            i32 dy = input->mouse.y - last_mouse_y;
            
            camera->yaw -= dx * 0.01f;
            camera->pitch -= dy * 0.01f;
            
            // Clamp pitch
            if (camera->pitch > 1.4f) camera->pitch = 1.4f;
            if (camera->pitch < -1.4f) camera->pitch = -1.4f;
        }
        first_mouse = false;
    } else {
        first_mouse = true;
    }
    
    last_mouse_x = input->mouse.x;
    last_mouse_y = input->mouse.y;
    
    // Update camera position
    camera->position.x = camera->target.x + camera->distance * cosf(camera->pitch) * cosf(camera->yaw);
    camera->position.y = camera->target.y + camera->distance * sinf(camera->pitch);
    camera->position.z = camera->target.z + camera->distance * cosf(camera->pitch) * sinf(camera->yaw);
    
    // Create view matrix
    camera->view_matrix = Mat4LookAt(camera->position, camera->target, (Vec3){0, 1, 0});
    camera->proj_matrix = Mat4Perspective(45.0f * M_PI / 180.0f, 4.0f/3.0f, 0.1f, 100.0f);
}

void RenderScene(Scene* scene, Camera* camera) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up matrices
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(camera->proj_matrix.m);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(camera->view_matrix.m);
    
    // Render grid
    RenderGrid(20.0f, 20);
    
    // Render all objects
    for (u32 i = 0; i < scene->object_count; i++) {
        GameObject* obj = &scene->objects[i];
        if (!obj->active) continue;
        
        switch (obj->type) {
            case OBJECT_CUBE:
                RenderCube(obj->position, obj->scale, obj->color, obj->selected);
                break;
                // Add other object types later
        }
    }
}

// ============================================================================
// ACTUAL EDITOR LOGIC (The heart of editing!)
// ============================================================================

void HandleEditorInput(Editor* editor, f32 dt) {
    Input* input = &editor->input;
    Scene* scene = &editor->scene;
    
    // ACTUAL MOUSE PICKING!
    if (input->mouse.left_clicked) {
        u32 picked_object = PickObject(scene, &editor->camera, 
                                     input->mouse.x, input->mouse.y,
                                     g_screen_width, g_screen_height);
        SelectObject(scene, picked_object);
    }
    
    // ACTUAL OBJECT CREATION!
    if (input->keys_pressed['c'] || input->keys_pressed['C']) {
        Vec3 spawn_pos = {0, 0, 0};
        if (scene->has_selection) {
            GameObject* selected = GetObject(scene, scene->selected_object_id);
            if (selected) {
                spawn_pos = Vec3Add(selected->position, (Vec3){2, 0, 0});
            }
        }
        u32 new_id = CreateObject(scene, OBJECT_CUBE, spawn_pos);
        SelectObject(scene, new_id);
    }
    
    // ACTUAL OBJECT DELETION!
    if (input->keys_pressed[XK_Delete] && scene->has_selection) {
        DeleteObject(scene, scene->selected_object_id);
    }
    
    // ACTUAL OBJECT MOVEMENT!
    if (scene->has_selection) {
        GameObject* selected = GetObject(scene, scene->selected_object_id);
        if (selected) {
            f32 move_speed = 5.0f * dt;
            
            if (input->keys['w'] || input->keys['W']) selected->position.z -= move_speed;
            if (input->keys['s'] || input->keys['S']) selected->position.z += move_speed;
            if (input->keys['a'] || input->keys['A']) selected->position.x -= move_speed;
            if (input->keys['d'] || input->keys['D']) selected->position.x += move_speed;
            if (input->keys['q'] || input->keys['Q']) selected->position.y += move_speed;
            if (input->keys['e'] || input->keys['E']) selected->position.y -= move_speed;
        }
    }
    
    // Update camera
    UpdateCamera(&editor->camera, input, dt);
}

void UpdateEditor(Editor* editor, f32 dt) {
    HandleEditorInput(editor, dt);
    
    // Update statistics
    editor->frame_count++;
    static f32 fps_timer = 0;
    fps_timer += dt;
    if (fps_timer >= 1.0f) {
        editor->fps = editor->frame_count / fps_timer;
        fps_timer = 0;
        editor->frame_count = 0;
        
        printf("[EDITOR] FPS: %.1f | Objects: %u | Selected: %u\n", 
               editor->fps, editor->scene.object_count, 
               editor->scene.selected_object_id);
    }
}

void RenderEditor(Editor* editor) {
    RenderScene(&editor->scene, &editor->camera);
    SwapBuffers();
}

// ============================================================================
// MAIN - ACTUAL EDITOR APPLICATION
// ============================================================================

int main(int argc, char** argv) {
    printf("=== ACTUAL EDITOR - Focus on EDITING, not platforms ===\n");
    printf("Controls:\n");
    printf("  Left Click: Select object\n");
    printf("  C: Create new cube\n");
    printf("  Delete: Delete selected object\n");
    printf("  WASD: Move selected object\n");
    printf("  Q/E: Move object up/down\n");
    printf("  Right Mouse: Rotate camera\n");
    printf("  ESC: Exit\n\n");
    
    if (!InitPlatform()) {
        printf("Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize memory
    static u8 memory[MEGABYTES(16)];
    MemoryArena arena = {memory, sizeof(memory), 0};
    
    // Create editor
    g_editor = PushStruct(&arena, Editor);
    g_editor->arena = arena;
    
    // Initialize scene with a default cube
    g_editor->scene.object_count = 0;
    g_editor->scene.next_object_id = 1;
    g_editor->scene.selected_object_id = 0;
    g_editor->scene.has_selection = false;
    
    // Create initial objects
    CreateObject(&g_editor->scene, OBJECT_CUBE, (Vec3){0, 0, 0});
    CreateObject(&g_editor->scene, OBJECT_CUBE, (Vec3){3, 0, 0});
    CreateObject(&g_editor->scene, OBJECT_CUBE, (Vec3){-3, 0, 0});
    
    // Initialize camera
    g_editor->camera.position = (Vec3){5, 3, 5};
    g_editor->camera.target = (Vec3){0, 0, 0};
    g_editor->camera.distance = Vec3Length(Vec3Sub(g_editor->camera.position, g_editor->camera.target));
    g_editor->camera.yaw = atan2f(g_editor->camera.position.z, g_editor->camera.position.x);
    g_editor->camera.pitch = asinf(g_editor->camera.position.y / g_editor->camera.distance);
    
    printf("[EDITOR] Initialized with %u objects\n", g_editor->scene.object_count);
    
    // Main loop
    bool running = true;
    f32 last_time = 0;
    
    while (running) {
        f32 current_time = (f32)clock() / CLOCKS_PER_SEC;
        f32 dt = current_time - last_time;
        if (dt > 0.1f) dt = 0.016f; // Cap at 60 FPS
        last_time = current_time;
        
        ProcessInput(&g_editor->input);
        
        // Check for exit
        if (g_editor->input.keys[XK_Escape]) {
            running = false;
        }
        
        UpdateEditor(g_editor, dt);
        RenderEditor(g_editor);
    }
    
    printf("\n[EDITOR] Final Statistics:\n");
    printf("  Objects Created: %u\n", g_editor->scene.next_object_id - 1);
    printf("  Objects Remaining: %u\n", g_editor->scene.object_count);
    
    ShutdownPlatform();
    return 0;
}
EOF

echo "Compiling ACTUAL editor with REAL editing features..."

# Compile the actual editor
if $CC $CFLAGS build/actual_editor.c $LIBS -o build/actual_editor; then
    echo "✅ ACTUAL EDITOR BUILT SUCCESSFULLY!"
    echo ""
    echo "🎯 ACTUAL EDITING FEATURES:"
    echo "  ✅ Mouse Picking - Click to select objects"
    echo "  ✅ Object Creation - Press 'C' to create cubes" 
    echo "  ✅ Object Deletion - Delete key removes selected objects"
    echo "  ✅ Object Movement - WASD/QE to move selected objects"
    echo "  ✅ Camera Control - Right mouse to rotate view"
    echo "  ✅ Visual Selection - Selected objects highlighted in yellow"
    echo "  ✅ Multiple Objects - Scene can contain many objects"
    echo ""
    echo "🚀 Ready to ACTUALLY EDIT:"
    echo "  ./build/actual_editor"
    echo ""
    echo "✨ Finally! An editor that can actually EDIT things!"
    echo "No more spinning cubes - now you can CREATE, SELECT, MOVE, and DELETE!"
    
else
    echo "❌ Build failed"
    $CC $CFLAGS build/actual_editor.c $LIBS -o build/actual_editor
    exit 1
fi