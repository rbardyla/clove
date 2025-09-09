/*
    Integrated Editor - Renderer + GUI
    ===================================
    
    Complete editor combining our integrated renderer with the custom GUI system.
    Demonstrates professional editor workflow with interactive panels.
*/

#include "handmade_platform.h"
#include "editor_gui.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <GL/gl.h>

// Math types are already defined in handmade_renderer_new.h
// typedef struct { f32 x, y, z; } Vec3;
// typedef struct { f32 x, y, z, w; } Vec4;

// Complete editor state
typedef struct {
    // Core systems
    EditorGUI* gui;
    
    // Window dimensions
    i32 width, height;
    
    // 3D Scene state
    Vec3 camera_position;
    Vec3 camera_rotation;
    f32 camera_zoom;
    
    // Scene objects
    f32 cube_rotation;
    Vec3 cube_position;
    Vec4 cube_color;
    
    // Performance tracking
    f64 last_frame_time;
    f64 frame_time_accumulator;
    u32 frame_count;
    f32 fps;
    
    // Animation time
    f32 time;
    
    // Editor state
    bool show_wireframe;
    bool show_grid;
    bool auto_rotate_cube;
    f32 rotation_speed;
    
    // Material editor
    f32 material_metallic;
    f32 material_roughness;
    Vec4 material_base_color;
    
    // Initialization flag
    bool initialized;
} IntegratedEditor;

static IntegratedEditor* g_editor = NULL;

// ============================================================================
// 3D RENDERING (Similar to our previous implementation)
// ============================================================================

// Draw spinning cube using legacy OpenGL
static void DrawAnimatedCube(f32 time, Vec3 position, Vec4 color, bool wireframe) {
    glPushMatrix();
    
    glTranslatef(position.x, position.y, position.z - 5.0f);
    glRotatef(time * 30.0f, 1.0f, 1.0f, 1.0f);
    
    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    glBegin(GL_QUADS);
    
    // Front face
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    
    // Back face
    glColor4f(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    
    // Top face
    glColor4f(color.x * 0.9f, color.y * 0.9f, color.z * 0.9f, color.w);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    
    // Bottom face
    glColor4f(color.x * 0.7f, color.y * 0.7f, color.z * 0.7f, color.w);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    
    // Right face
    glColor4f(color.x * 0.85f, color.y * 0.85f, color.z * 0.85f, color.w);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    
    // Left face
    glColor4f(color.x * 0.75f, color.y * 0.75f, color.z * 0.75f, color.w);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    
    glEnd();
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();
}

// Draw grid
static void DrawGrid(f32 size, f32 spacing) {
    glBegin(GL_LINES);
    glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
    
    for (f32 i = -size; i <= size; i += spacing) {
        // Vertical lines
        glVertex3f(i, 0, -size);
        glVertex3f(i, 0, size);
        
        // Horizontal lines
        glVertex3f(-size, 0, i);
        glVertex3f(size, 0, i);
    }
    
    glEnd();
}

// Setup 3D viewport
static void Setup3DViewport(IntegratedEditor* editor) {
    // Calculate viewport accounting for GUI panels
    f32 viewport_x = 250; // Hierarchy panel width
    f32 viewport_y = 150; // Console panel height
    f32 viewport_width = editor->width - 250 - 300; // Minus hierarchy and inspector
    f32 viewport_height = editor->height - 150 - 90; // Minus console and toolbar
    
    glViewport((GLint)viewport_x, (GLint)viewport_y, (GLsizei)viewport_width, (GLsizei)viewport_height);
    
    // Setup 3D projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    f32 aspect = viewport_width / viewport_height;
    
    f32 fov = 45.0f;
    f32 near_plane = 0.1f;
    f32 far_plane = 1000.0f;
    f32 f = 1.0f / tanf(fov * M_PI / 360.0f);
    
    f32 projection[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (far_plane + near_plane)/(near_plane - far_plane), -1,
        0, 0, (2*far_plane*near_plane)/(near_plane - far_plane), 0
    };
    
    glLoadMatrixf(projection);
    
    // Setup modelview
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Camera
    glTranslatef(editor->camera_position.x, editor->camera_position.y, editor->camera_position.z);
    glRotatef(editor->camera_rotation.x, 1, 0, 0);
    glRotatef(editor->camera_rotation.y, 0, 1, 0);
    glRotatef(editor->camera_rotation.z, 0, 0, 1);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

// Setup 2D viewport for GUI
static void Setup2DViewport(IntegratedEditor* editor) {
    glViewport(0, 0, editor->width, editor->height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, editor->width, editor->height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ============================================================================
// CUSTOM EDITOR PANELS
// ============================================================================

// Override hierarchy panel to show our scene
void CustomDrawHierarchyPanel(IntegratedEditor* editor) {
    EditorGUI* gui = editor->gui;
    PanelConfig* panel = &gui->panels[PANEL_HIERARCHY];
    
    // Draw panel background
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.panel_bg);
    EditorGUI_DrawRectOutline(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.border, 1.0f);
    
    // Draw header
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, 25, gui->theme.header_bg);
    EditorGUI_DrawText(gui, "Scene Hierarchy", panel->x + 5, panel->y + 5, gui->theme.text_normal);
    
    // Scene items
    f32 item_y = panel->y + 35;
    f32 item_height = 22;
    
    static bool scene_root_open = true;
    if (EditorGUI_TreeNode(gui, "Scene Root", &scene_root_open, panel->x + 5, item_y)) {
        item_y += item_height;
        
        // Animated cube
        static bool cube_selected = false;
        if (EditorGUI_Button(gui, "  Animated Cube", panel->x + 15, item_y, 150, 18)) {
            cube_selected = !cube_selected;
            EditorGUI_Log(gui, "Selected: Animated Cube");
        }
        
        item_y += item_height;
        
        // Lighting
        static bool lighting_open = false;
        if (EditorGUI_TreeNode(gui, "  Lighting", &lighting_open, panel->x + 15, item_y)) {
            item_y += item_height;
            EditorGUI_Text(gui, "    - Main Light", panel->x + 25, item_y, gui->theme.text_normal);
            
            item_y += item_height;
            EditorGUI_Text(gui, "    - Ambient", panel->x + 25, item_y, gui->theme.text_normal);
        }
        
        item_y += item_height;
        
        // Camera
        static bool camera_selected = false;
        if (EditorGUI_Button(gui, "  Camera", panel->x + 15, item_y, 100, 18)) {
            camera_selected = !camera_selected;
            EditorGUI_Log(gui, "Selected: Camera");
        }
    }
}

// Override inspector to show cube properties
void CustomDrawInspectorPanel(IntegratedEditor* editor) {
    EditorGUI* gui = editor->gui;
    PanelConfig* panel = &gui->panels[PANEL_INSPECTOR];
    
    // Draw panel background
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.panel_bg);
    EditorGUI_DrawRectOutline(gui, panel->x, panel->y, panel->width, panel->height, gui->theme.border, 1.0f);
    
    // Draw header
    EditorGUI_DrawRect(gui, panel->x, panel->y, panel->width, 25, gui->theme.header_bg);
    EditorGUI_DrawText(gui, "Inspector", panel->x + 5, panel->y + 5, gui->theme.text_normal);
    
    f32 prop_y = panel->y + 35;
    f32 prop_height = 28;
    
    // Transform section
    EditorGUI_DrawText(gui, "Transform:", panel->x + 5, prop_y, gui->theme.text_highlight);
    prop_y += prop_height;
    
    EditorGUI_SliderFloat(gui, "Pos X", &editor->cube_position.x, -5.0f, 5.0f, panel->x + 5, prop_y, panel->width - 10);
    prop_y += prop_height;
    EditorGUI_SliderFloat(gui, "Pos Y", &editor->cube_position.y, -5.0f, 5.0f, panel->x + 5, prop_y, panel->width - 10);
    prop_y += prop_height;
    EditorGUI_SliderFloat(gui, "Pos Z", &editor->cube_position.z, -5.0f, 5.0f, panel->x + 5, prop_y, panel->width - 10);
    
    prop_y += prop_height + 10;
    
    // Animation section
    EditorGUI_DrawText(gui, "Animation:", panel->x + 5, prop_y, gui->theme.text_highlight);
    prop_y += prop_height;
    
    EditorGUI_Checkbox(gui, "Auto Rotate", &editor->auto_rotate_cube, panel->x + 5, prop_y);
    prop_y += prop_height;
    EditorGUI_SliderFloat(gui, "Speed", &editor->rotation_speed, 0.0f, 3.0f, panel->x + 5, prop_y, panel->width - 10);
    
    prop_y += prop_height + 10;
    
    // Rendering section
    EditorGUI_DrawText(gui, "Rendering:", panel->x + 5, prop_y, gui->theme.text_highlight);
    prop_y += prop_height;
    
    EditorGUI_Checkbox(gui, "Wireframe", &editor->show_wireframe, panel->x + 5, prop_y);
    prop_y += prop_height;
    EditorGUI_Checkbox(gui, "Show Grid", &editor->show_grid, panel->x + 5, prop_y);
    
    prop_y += prop_height + 10;
    
    // Material section
    EditorGUI_DrawText(gui, "Material:", panel->x + 5, prop_y, gui->theme.text_highlight);
    prop_y += prop_height;
    
    bool material_changed = false;
    material_changed |= EditorGUI_SliderFloat(gui, "Red", &editor->material_base_color.x, 0.0f, 1.0f, panel->x + 5, prop_y, panel->width - 10);
    prop_y += prop_height;
    material_changed |= EditorGUI_SliderFloat(gui, "Green", &editor->material_base_color.y, 0.0f, 1.0f, panel->x + 5, prop_y, panel->width - 10);
    prop_y += prop_height;
    material_changed |= EditorGUI_SliderFloat(gui, "Blue", &editor->material_base_color.z, 0.0f, 1.0f, panel->x + 5, prop_y, panel->width - 10);
    
    if (material_changed) {
        editor->cube_color = editor->material_base_color;
        EditorGUI_Log(gui, "Material color changed");
    }
}

// ============================================================================
// MAIN EDITOR FUNCTIONS
// ============================================================================

void GameInit(PlatformState* platform) {
    if (!g_editor) {
        g_editor = PushStruct(&platform->permanent_arena, IntegratedEditor);
        
        g_editor->width = platform->window.width;
        g_editor->height = platform->window.height;
        
        // Initialize 3D scene
        g_editor->camera_position = (Vec3){0, 0, 0};
        g_editor->camera_rotation = (Vec3){-20, 0, 0};
        g_editor->camera_zoom = 10.0f;
        
        g_editor->cube_position = (Vec3){0, 0, 0};
        g_editor->cube_color = (Vec4){0.5f, 0.3f, 0.7f, 1.0f};
        g_editor->material_base_color = g_editor->cube_color;
        
        // Initialize editor settings
        g_editor->show_wireframe = false;
        g_editor->show_grid = true;
        g_editor->auto_rotate_cube = true;
        g_editor->rotation_speed = 1.0f;
        
        // Create GUI system
        g_editor->gui = EditorGUI_Create(platform, NULL); // No renderer for now
        
        EditorGUI_Log(g_editor->gui, "Integrated Editor initialized");
        EditorGUI_Log(g_editor->gui, "Features: 3D Viewport + Interactive GUI");
        EditorGUI_Log(g_editor->gui, "Press F1-F4 to toggle panels, WASD for camera");
        
        printf("[Integrated Editor] Initialized with GUI + Renderer\\n");
        printf("[Integrated Editor] Features: Professional editor workflow\\n");
    }
    
    g_editor->initialized = true;
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_editor || !g_editor->initialized) return;
    
    PlatformInput* input = &platform->input;
    
    // Update dimensions
    g_editor->width = platform->window.width;
    g_editor->height = platform->window.height;
    
    // Update time and animation
    g_editor->time += dt * g_editor->rotation_speed;
    if (g_editor->auto_rotate_cube) {
        g_editor->cube_rotation += dt * g_editor->rotation_speed * 50.0f;
    }
    
    // Camera controls
    f32 rotate_speed = 50.0f;
    f32 move_speed = 5.0f;
    
    if (input->keys[KEY_A].down) g_editor->camera_rotation.y -= rotate_speed * dt;
    if (input->keys[KEY_D].down) g_editor->camera_rotation.y += rotate_speed * dt;
    if (input->keys[KEY_W].down) g_editor->camera_rotation.x -= rotate_speed * dt;
    if (input->keys[KEY_S].down) g_editor->camera_rotation.x += rotate_speed * dt;
    
    // Panel toggles
    if (input->keys[KEY_F1].pressed) EditorGUI_ShowPanel(g_editor->gui, PANEL_HIERARCHY, 
                                     !EditorGUI_IsPanelVisible(g_editor->gui, PANEL_HIERARCHY));
    if (input->keys[KEY_F2].pressed) EditorGUI_ShowPanel(g_editor->gui, PANEL_INSPECTOR,
                                     !EditorGUI_IsPanelVisible(g_editor->gui, PANEL_INSPECTOR));
    if (input->keys[KEY_F3].pressed) EditorGUI_ShowPanel(g_editor->gui, PANEL_CONSOLE,
                                     !EditorGUI_IsPanelVisible(g_editor->gui, PANEL_CONSOLE));
    if (input->keys[KEY_F4].pressed) EditorGUI_ShowPanel(g_editor->gui, PANEL_PERFORMANCE,
                                     !EditorGUI_IsPanelVisible(g_editor->gui, PANEL_PERFORMANCE));
    
    // FPS calculation
    g_editor->frame_count++;
    g_editor->frame_time_accumulator += dt;
    if (g_editor->frame_time_accumulator >= 1.0) {
        g_editor->fps = g_editor->frame_count / g_editor->frame_time_accumulator;
        g_editor->frame_count = 0;
        g_editor->frame_time_accumulator = 0;
    }
    g_editor->last_frame_time = dt;
    
    // Update GUI performance stats
    EditorGUI_UpdatePerformanceStats(g_editor->gui, dt, g_editor->fps);
    
    // Begin GUI frame
    EditorGUI_BeginFrame(g_editor->gui, input);
}

void GameRender(PlatformState* platform) {
    if (!g_editor || !g_editor->initialized) return;
    
    // Clear screen
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // ========== 3D SCENE RENDERING ==========
    Setup3DViewport(g_editor);
    
    // Draw grid if enabled
    if (g_editor->show_grid) {
        DrawGrid(10.0f, 1.0f);
    }
    
    // Draw animated cube
    DrawAnimatedCube(g_editor->time, g_editor->cube_position, g_editor->cube_color, g_editor->show_wireframe);
    
    // ========== GUI RENDERING ==========
    Setup2DViewport(g_editor);
    
    // Draw custom panels
    if (EditorGUI_IsPanelVisible(g_editor->gui, PANEL_HIERARCHY)) {
        CustomDrawHierarchyPanel(g_editor);
    }
    
    if (EditorGUI_IsPanelVisible(g_editor->gui, PANEL_INSPECTOR)) {
        CustomDrawInspectorPanel(g_editor);
    }
    
    // Draw standard GUI panels
    EditorGUI_EndFrame(g_editor->gui);
}

void GameShutdown(PlatformState* platform) {
    if (g_editor && g_editor->gui) {
        EditorGUI_Log(g_editor->gui, "Integrated Editor shutting down");
        EditorGUI_Destroy(g_editor->gui);
        printf("\\n[Integrated Editor] Shutdown complete\\n");
    }
    g_editor = NULL;
}