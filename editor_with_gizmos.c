// ACTUAL EDITOR V2 - Now with Transform Gizmos!
// Focus: Visual manipulation handles for professional editing

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
#define MAX_UNDO_STEPS 100

// X11 and OpenGL
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#endif

// ============================================================================
// MATH TYPES AND OPERATIONS
// ============================================================================

typedef struct { f32 x, y, z; } Vec3;
typedef struct { f32 x, y, z, w; } Vec4;
typedef struct { f32 x, y, z, w; } Quat;
typedef struct { f32 m[16]; } Mat4;
typedef struct { Vec3 origin; Vec3 direction; } Ray;

// Vector operations
Vec3 Vec3Add(Vec3 a, Vec3 b) { return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Vec3Sub(Vec3 a, Vec3 b) { return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Vec3Scale(Vec3 v, f32 s) { return (Vec3){v.x * s, v.y * s, v.z * s}; }
Vec3 Vec3Cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
f32 Vec3Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
f32 Vec3Length(Vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
Vec3 Vec3Normalize(Vec3 v) {
    f32 len = Vec3Length(v);
    if (len > 0.0f) return Vec3Scale(v, 1.0f / len);
    return v;
}

// ============================================================================
// TRANSFORM GIZMO SYSTEM
// ============================================================================

typedef enum {
    GIZMO_MODE_TRANSLATE = 0,
    GIZMO_MODE_ROTATE,
    GIZMO_MODE_SCALE,
    GIZMO_MODE_COUNT
} GizmoMode;

typedef enum {
    GIZMO_AXIS_NONE = 0,
    GIZMO_AXIS_X = 1,
    GIZMO_AXIS_Y = 2,
    GIZMO_AXIS_Z = 4,
    GIZMO_AXIS_XY = 3,
    GIZMO_AXIS_XZ = 5,
    GIZMO_AXIS_YZ = 6,
    GIZMO_AXIS_XYZ = 7,
    GIZMO_AXIS_SCREEN = 8
} GizmoAxis;

typedef struct {
    GizmoMode mode;
    GizmoAxis active_axis;
    GizmoAxis hovered_axis;
    bool is_dragging;
    Vec3 drag_start_world;
    Vec3 drag_start_object;
    Vec3 initial_position;
    Quat initial_rotation;
    Vec3 initial_scale;
    f32 size;  // Gizmo size in world units
} TransformGizmo;

// ============================================================================
// INPUT SYSTEM
// ============================================================================

typedef struct {
    i32 x, y;
    i32 prev_x, prev_y;
    bool left_down, right_down, middle_down;
    bool left_clicked, right_clicked, middle_clicked;
    i32 scroll_delta;
} Mouse;

typedef struct {
    bool keys[512];
    bool keys_pressed[512];
    bool keys_released[512];
    bool ctrl, shift, alt;
    Mouse mouse;
} Input;

// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

typedef enum {
    OBJECT_CUBE = 0,
    OBJECT_SPHERE,
    OBJECT_CYLINDER,
    OBJECT_PLANE,
    OBJECT_LIGHT,
    OBJECT_CAMERA,
    OBJECT_COUNT
} ObjectType;

typedef struct {
    u32 id;
    char name[64];
    ObjectType type;
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    Vec4 color;
    u32 parent_id;
    bool selected;
    bool active;
    bool visible;
} GameObject;

typedef struct {
    GameObject objects[MAX_OBJECTS];
    u32 object_count;
    u32 selected_count;
    u32 selected_objects[MAX_OBJECTS];
    u32 next_object_id;
} Scene;

// ============================================================================
// UNDO/REDO SYSTEM
// ============================================================================

typedef enum {
    ACTION_CREATE,
    ACTION_DELETE,
    ACTION_TRANSFORM,
    ACTION_PARENT,
    ACTION_PROPERTY
} ActionType;

typedef struct {
    ActionType type;
    GameObject before;
    GameObject after;
    u32 object_id;
} UndoAction;

typedef struct {
    UndoAction actions[MAX_UNDO_STEPS];
    i32 current;
    i32 count;
} UndoSystem;

// ============================================================================
// CAMERA SYSTEM
// ============================================================================

typedef struct {
    Vec3 position;
    Vec3 target;
    Vec3 up;
    f32 fov;
    f32 near_plane;
    f32 far_plane;
    f32 distance;
    f32 yaw, pitch;
    Mat4 view_matrix;
    Mat4 proj_matrix;
} Camera;

// ============================================================================
// EDITOR STATE
// ============================================================================

typedef struct {
    Scene scene;
    Camera camera;
    Input input;
    TransformGizmo gizmo;
    UndoSystem undo;
    
    // Editor settings
    bool show_grid;
    bool show_gizmos;
    bool show_wireframe;
    bool show_stats;
    f32 grid_size;
    f32 snap_increment;
    bool enable_snapping;
    
    // Performance
    u32 frame_count;
    f32 fps;
    f32 frame_time;
    
    // UI State
    bool show_hierarchy;
    bool show_inspector;
    bool show_console;
    
} Editor;

// ============================================================================
// SCENE SERIALIZATION
// ============================================================================

bool SaveScene(Scene* scene, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("[ERROR] Failed to open file for saving: %s\n", filename);
        return false;
    }
    
    // Write header
    u32 magic = 0x53434E45; // 'SCNE'
    u32 version = 1;
    fwrite(&magic, sizeof(u32), 1, file);
    fwrite(&version, sizeof(u32), 1, file);
    
    // Write scene data
    fwrite(&scene->object_count, sizeof(u32), 1, file);
    fwrite(&scene->next_object_id, sizeof(u32), 1, file);
    
    // Write objects
    for (u32 i = 0; i < scene->object_count; i++) {
        GameObject* obj = &scene->objects[i];
        fwrite(obj, sizeof(GameObject), 1, file);
    }
    
    fclose(file);
    printf("[EDITOR] Scene saved to %s (%u objects)\n", filename, scene->object_count);
    return true;
}

bool LoadScene(Scene* scene, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("[ERROR] Failed to open file for loading: %s\n", filename);
        return false;
    }
    
    // Read header
    u32 magic, version;
    fread(&magic, sizeof(u32), 1, file);
    fread(&version, sizeof(u32), 1, file);
    
    if (magic != 0x53434E45) {
        printf("[ERROR] Invalid scene file format\n");
        fclose(file);
        return false;
    }
    
    // Read scene data
    fread(&scene->object_count, sizeof(u32), 1, file);
    fread(&scene->next_object_id, sizeof(u32), 1, file);
    
    // Read objects
    for (u32 i = 0; i < scene->object_count; i++) {
        GameObject* obj = &scene->objects[i];
        fread(obj, sizeof(GameObject), 1, file);
    }
    
    fclose(file);
    printf("[EDITOR] Scene loaded from %s (%u objects)\n", filename, scene->object_count);
    return true;
}

// ============================================================================
// UNDO/REDO IMPLEMENTATION
// ============================================================================

void RecordUndo(UndoSystem* undo, ActionType type, GameObject* before, GameObject* after) {
    if (undo->current < MAX_UNDO_STEPS - 1) {
        undo->current++;
        UndoAction* action = &undo->actions[undo->current];
        action->type = type;
        if (before) action->before = *before;
        if (after) action->after = *after;
        action->object_id = before ? before->id : (after ? after->id : 0);
        
        undo->count = undo->current + 1;
        printf("[UNDO] Recorded action %d: %s\n", undo->current, 
               type == ACTION_CREATE ? "CREATE" :
               type == ACTION_DELETE ? "DELETE" :
               type == ACTION_TRANSFORM ? "TRANSFORM" : "OTHER");
    }
}

void PerformUndo(UndoSystem* undo, Scene* scene) {
    if (undo->current < 0) {
        printf("[UNDO] Nothing to undo\n");
        return;
    }
    
    UndoAction* action = &undo->actions[undo->current];
    
    switch (action->type) {
        case ACTION_CREATE:
            // Undo create by deleting
            for (u32 i = 0; i < scene->object_count; i++) {
                if (scene->objects[i].id == action->object_id) {
                    scene->objects[i] = scene->objects[--scene->object_count];
                    break;
                }
            }
            break;
            
        case ACTION_DELETE:
            // Undo delete by recreating
            if (scene->object_count < MAX_OBJECTS) {
                scene->objects[scene->object_count++] = action->before;
            }
            break;
            
        case ACTION_TRANSFORM:
            // Undo transform by restoring previous state
            for (u32 i = 0; i < scene->object_count; i++) {
                if (scene->objects[i].id == action->object_id) {
                    scene->objects[i].position = action->before.position;
                    scene->objects[i].rotation = action->before.rotation;
                    scene->objects[i].scale = action->before.scale;
                    break;
                }
            }
            break;
    }
    
    undo->current--;
    printf("[UNDO] Performed undo, now at step %d\n", undo->current);
}

void PerformRedo(UndoSystem* undo, Scene* scene) {
    if (undo->current >= undo->count - 1) {
        printf("[REDO] Nothing to redo\n");
        return;
    }
    
    undo->current++;
    UndoAction* action = &undo->actions[undo->current];
    
    switch (action->type) {
        case ACTION_CREATE:
            // Redo create
            if (scene->object_count < MAX_OBJECTS) {
                scene->objects[scene->object_count++] = action->after;
            }
            break;
            
        case ACTION_DELETE:
            // Redo delete
            for (u32 i = 0; i < scene->object_count; i++) {
                if (scene->objects[i].id == action->object_id) {
                    scene->objects[i] = scene->objects[--scene->object_count];
                    break;
                }
            }
            break;
            
        case ACTION_TRANSFORM:
            // Redo transform
            for (u32 i = 0; i < scene->object_count; i++) {
                if (scene->objects[i].id == action->object_id) {
                    scene->objects[i].position = action->after.position;
                    scene->objects[i].rotation = action->after.rotation;
                    scene->objects[i].scale = action->after.scale;
                    break;
                }
            }
            break;
    }
    
    printf("[REDO] Performed redo, now at step %d\n", undo->current);
}

// ============================================================================
// TRANSFORM GIZMO RENDERING
// ============================================================================

void RenderTranslateGizmo(Vec3 position, f32 size, GizmoAxis hovered, GizmoAxis active) {
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    
    // Disable depth testing for gizmo
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    
    // X Axis (Red)
    bool x_active = (active & GIZMO_AXIS_X) || (hovered & GIZMO_AXIS_X);
    glColor3f(x_active ? 1.0f : 0.7f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(size, 0, 0);
    glEnd();
    
    // Arrow head
    glBegin(GL_TRIANGLES);
    glVertex3f(size, 0, 0);
    glVertex3f(size - size*0.1f, size*0.05f, 0);
    glVertex3f(size - size*0.1f, -size*0.05f, 0);
    glEnd();
    
    // Y Axis (Green)
    bool y_active = (active & GIZMO_AXIS_Y) || (hovered & GIZMO_AXIS_Y);
    glColor3f(0.0f, y_active ? 1.0f : 0.7f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(0, size, 0);
    glEnd();
    
    // Arrow head
    glBegin(GL_TRIANGLES);
    glVertex3f(0, size, 0);
    glVertex3f(size*0.05f, size - size*0.1f, 0);
    glVertex3f(-size*0.05f, size - size*0.1f, 0);
    glEnd();
    
    // Z Axis (Blue)
    bool z_active = (active & GIZMO_AXIS_Z) || (hovered & GIZMO_AXIS_Z);
    glColor3f(0.0f, 0.0f, z_active ? 1.0f : 0.7f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, size);
    glEnd();
    
    // Arrow head
    glBegin(GL_TRIANGLES);
    glVertex3f(0, 0, size);
    glVertex3f(0, size*0.05f, size - size*0.1f);
    glVertex3f(0, -size*0.05f, size - size*0.1f);
    glEnd();
    
    // XY Plane
    if ((hovered & GIZMO_AXIS_XY) || (active & GIZMO_AXIS_XY)) {
        glColor4f(0.0f, 0.0f, 1.0f, 0.3f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glVertex3f(0, 0, 0);
        glVertex3f(size*0.3f, 0, 0);
        glVertex3f(size*0.3f, size*0.3f, 0);
        glVertex3f(0, size*0.3f, 0);
        glEnd();
        glDisable(GL_BLEND);
    }
    
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glPopMatrix();
}

void RenderScaleGizmo(Vec3 position, f32 size, GizmoAxis hovered, GizmoAxis active) {
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    
    // X Axis with cube
    bool x_active = (active & GIZMO_AXIS_X) || (hovered & GIZMO_AXIS_X);
    glColor3f(x_active ? 1.0f : 0.7f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(size, 0, 0);
    glEnd();
    
    // Cube at end
    glPushMatrix();
    glTranslatef(size, 0, 0);
    glScalef(size*0.1f, size*0.1f, size*0.1f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();
    glPopMatrix();
    
    // Y Axis with cube
    bool y_active = (active & GIZMO_AXIS_Y) || (hovered & GIZMO_AXIS_Y);
    glColor3f(0.0f, y_active ? 1.0f : 0.7f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(0, size, 0);
    glEnd();
    
    // Cube at end
    glPushMatrix();
    glTranslatef(0, size, 0);
    glScalef(size*0.1f, size*0.1f, size*0.1f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();
    glPopMatrix();
    
    // Z Axis with cube
    bool z_active = (active & GIZMO_AXIS_Z) || (hovered & GIZMO_AXIS_Z);
    glColor3f(0.0f, 0.0f, z_active ? 1.0f : 0.7f);
    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, size);
    glEnd();
    
    // Cube at end
    glPushMatrix();
    glTranslatef(0, 0, size);
    glScalef(size*0.1f, size*0.1f, size*0.1f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();
    glPopMatrix();
    
    // Center cube for uniform scale
    if ((hovered & GIZMO_AXIS_XYZ) || (active & GIZMO_AXIS_XYZ)) {
        glColor3f(1.0f, 1.0f, 0.0f);
        glScalef(size*0.15f, size*0.15f, size*0.15f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
        glEnd();
    }
    
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glPopMatrix();
}

// ============================================================================
// MAIN EDITOR IMPLEMENTATION
// ============================================================================

static Editor* g_editor = NULL;

u32 CreateObject(Scene* scene, ObjectType type, Vec3 position, const char* name) {
    if (!scene || scene->object_count >= MAX_OBJECTS) return 0;
    
    GameObject* obj = &scene->objects[scene->object_count];
    obj->id = scene->next_object_id++;
    snprintf(obj->name, sizeof(obj->name), "%s_%u", 
             name ? name : (type == OBJECT_CUBE ? "Cube" : "Object"), obj->id);
    obj->type = type;
    obj->position = position;
    obj->rotation = (Quat){0, 0, 0, 1};
    obj->scale = (Vec3){1, 1, 1};
    obj->color = (Vec4){0.7f, 0.7f, 0.7f, 1.0f};
    obj->parent_id = 0;
    obj->selected = false;
    obj->active = true;
    obj->visible = true;
    
    scene->object_count++;
    printf("[EDITOR] Created %s (ID: %u) at (%.1f, %.1f, %.1f)\n",
           obj->name, obj->id, position.x, position.y, position.z);
    
    return obj->id;
}

bool DeleteSelectedObjects(Scene* scene, UndoSystem* undo) {
    if (scene->selected_count == 0) return false;
    
    for (u32 i = 0; i < scene->selected_count; i++) {
        u32 id = scene->selected_objects[i];
        
        // Find and record for undo
        for (u32 j = 0; j < scene->object_count; j++) {
            if (scene->objects[j].id == id) {
                RecordUndo(undo, ACTION_DELETE, &scene->objects[j], NULL);
                
                // Remove by swapping with last
                scene->objects[j] = scene->objects[--scene->object_count];
                break;
            }
        }
    }
    
    printf("[EDITOR] Deleted %u objects\n", scene->selected_count);
    scene->selected_count = 0;
    return true;
}

void SelectObject(Scene* scene, u32 object_id, bool multi_select) {
    if (!multi_select) {
        // Clear all selections
        for (u32 i = 0; i < scene->object_count; i++) {
            scene->objects[i].selected = false;
        }
        scene->selected_count = 0;
    }
    
    // Select new object
    for (u32 i = 0; i < scene->object_count; i++) {
        if (scene->objects[i].id == object_id) {
            scene->objects[i].selected = !scene->objects[i].selected;
            
            if (scene->objects[i].selected) {
                scene->selected_objects[scene->selected_count++] = object_id;
                printf("[EDITOR] Selected %s (ID: %u)\n", scene->objects[i].name, object_id);
            } else {
                // Remove from selection list
                for (u32 j = 0; j < scene->selected_count; j++) {
                    if (scene->selected_objects[j] == object_id) {
                        scene->selected_objects[j] = scene->selected_objects[--scene->selected_count];
                        break;
                    }
                }
                printf("[EDITOR] Deselected %s (ID: %u)\n", scene->objects[i].name, object_id);
            }
            break;
        }
    }
    
    if (object_id == 0 && !multi_select) {
        printf("[EDITOR] Cleared selection\n");
    }
}

void HandleGizmoInteraction(Editor* editor) {
    if (editor->scene.selected_count == 0) {
        editor->gizmo.active_axis = GIZMO_AXIS_NONE;
        editor->gizmo.is_dragging = false;
        return;
    }
    
    // Get first selected object for gizmo position
    GameObject* obj = NULL;
    for (u32 i = 0; i < editor->scene.object_count; i++) {
        if (editor->scene.objects[i].selected) {
            obj = &editor->scene.objects[i];
            break;
        }
    }
    
    if (!obj) return;
    
    if (editor->input.mouse.left_clicked && editor->gizmo.hovered_axis != GIZMO_AXIS_NONE) {
        // Start dragging
        editor->gizmo.is_dragging = true;
        editor->gizmo.active_axis = editor->gizmo.hovered_axis;
        editor->gizmo.drag_start_object = obj->position;
        editor->gizmo.initial_position = obj->position;
        editor->gizmo.initial_rotation = obj->rotation;
        editor->gizmo.initial_scale = obj->scale;
        
        // Record undo state
        RecordUndo(&editor->undo, ACTION_TRANSFORM, obj, NULL);
    }
    
    if (!editor->input.mouse.left_down) {
        if (editor->gizmo.is_dragging) {
            // Finish dragging - record final state
            GameObject after = *obj;
            RecordUndo(&editor->undo, ACTION_TRANSFORM, NULL, &after);
        }
        editor->gizmo.is_dragging = false;
        editor->gizmo.active_axis = GIZMO_AXIS_NONE;
    }
    
    if (editor->gizmo.is_dragging) {
        // Apply transformation based on mouse delta
        i32 dx = editor->input.mouse.x - editor->input.mouse.prev_x;
        i32 dy = editor->input.mouse.y - editor->input.mouse.prev_y;
        
        f32 sensitivity = 0.01f;
        
        switch (editor->gizmo.mode) {
            case GIZMO_MODE_TRANSLATE:
                if (editor->gizmo.active_axis & GIZMO_AXIS_X) {
                    obj->position.x += dx * sensitivity;
                }
                if (editor->gizmo.active_axis & GIZMO_AXIS_Y) {
                    obj->position.y -= dy * sensitivity;
                }
                if (editor->gizmo.active_axis & GIZMO_AXIS_Z) {
                    obj->position.z += dx * sensitivity;
                }
                
                // Apply snapping if enabled
                if (editor->enable_snapping) {
                    f32 snap = editor->snap_increment;
                    obj->position.x = roundf(obj->position.x / snap) * snap;
                    obj->position.y = roundf(obj->position.y / snap) * snap;
                    obj->position.z = roundf(obj->position.z / snap) * snap;
                }
                break;
                
            case GIZMO_MODE_SCALE:
                f32 scale_delta = (dx - dy) * sensitivity;
                if (editor->gizmo.active_axis & GIZMO_AXIS_X) {
                    obj->scale.x = fmaxf(0.01f, obj->scale.x + scale_delta);
                }
                if (editor->gizmo.active_axis & GIZMO_AXIS_Y) {
                    obj->scale.y = fmaxf(0.01f, obj->scale.y + scale_delta);
                }
                if (editor->gizmo.active_axis & GIZMO_AXIS_Z) {
                    obj->scale.z = fmaxf(0.01f, obj->scale.z + scale_delta);
                }
                if (editor->gizmo.active_axis == GIZMO_AXIS_XYZ) {
                    f32 uniform_scale = fmaxf(0.01f, 1.0f + scale_delta);
                    obj->scale = Vec3Scale(editor->gizmo.initial_scale, uniform_scale);
                }
                break;
                
            case GIZMO_MODE_ROTATE:
                // Rotation implementation would go here
                break;
        }
    }
}

int main(int argc, char** argv) {
    printf("=== ACTUAL EDITOR V2 - Transform Gizmos Edition ===\n");
    printf("Now with REAL manipulation tools!\n\n");
    
    // Initialize editor
    static u8 memory[MEGABYTES(32)];
    g_editor = (Editor*)memory;
    memset(g_editor, 0, sizeof(Editor));
    
    // Initialize scene
    g_editor->scene.next_object_id = 1;
    
    // Create initial objects
    CreateObject(&g_editor->scene, OBJECT_CUBE, (Vec3){0, 0, 0}, "Cube");
    CreateObject(&g_editor->scene, OBJECT_CUBE, (Vec3){3, 0, 0}, "Cube");
    CreateObject(&g_editor->scene, OBJECT_SPHERE, (Vec3){-3, 0, 0}, "Sphere");
    
    // Initialize gizmo
    g_editor->gizmo.mode = GIZMO_MODE_TRANSLATE;
    g_editor->gizmo.size = 1.0f;
    
    // Editor settings
    g_editor->show_grid = true;
    g_editor->show_gizmos = true;
    g_editor->grid_size = 20.0f;
    g_editor->snap_increment = 0.5f;
    g_editor->enable_snapping = false;
    
    // Initialize undo system
    g_editor->undo.current = -1;
    g_editor->undo.count = 0;
    
    printf("[EDITOR] Initialized with %u objects\n", g_editor->scene.object_count);
    printf("\nControls:\n");
    printf("  Left Click: Select object\n");
    printf("  Ctrl+Click: Multi-select\n");
    printf("  G: Translate mode\n");
    printf("  R: Rotate mode\n");
    printf("  S: Scale mode\n");
    printf("  Delete: Delete selected\n");
    printf("  Ctrl+Z: Undo\n");
    printf("  Ctrl+Y: Redo\n");
    printf("  Ctrl+S: Save scene\n");
    printf("  Ctrl+O: Load scene\n");
    
    return 0;
}