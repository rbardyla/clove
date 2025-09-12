/*
    Editor GUI System - Integration Layer
    =====================================
    
    Bridges our integrated renderer with a simplified immediate-mode GUI
    for the editor. Focuses on the essential panels needed for development.
*/

#ifndef EDITOR_GUI_H
#define EDITOR_GUI_H

#include "handmade_platform.h"
#include "systems/renderer/handmade_renderer_new.h"

// Forward declarations
typedef struct EditorGUI EditorGUI;
typedef struct EditorPanel EditorPanel;

// Panel types
typedef enum {
    PANEL_HIERARCHY,
    PANEL_INSPECTOR, 
    PANEL_CONSOLE,
    PANEL_PERFORMANCE,
    PANEL_MATERIAL_EDITOR,
    PANEL_ASSET_BROWSER,
    PANEL_COUNT
} PanelType;

// Widget state
typedef enum {
    WIDGET_IDLE,
    WIDGET_HOT,
    WIDGET_ACTIVE
} WidgetState;

// Color scheme
typedef struct {
    Vec4 background;
    Vec4 panel_bg;
    Vec4 header_bg;
    Vec4 button_bg;
    Vec4 button_hot;
    Vec4 button_active;
    Vec4 text_normal;
    Vec4 text_highlight;
    Vec4 border;
    Vec4 selection;
} EditorTheme;

// Panel configuration
typedef struct {
    bool visible;
    f32 x, y;
    f32 width, height;
    f32 min_width, min_height;
    bool resizable;
    bool movable;
    char title[64];
} PanelConfig;

// Console log entry
typedef struct {
    char message[512];
    Vec4 color;
    f64 timestamp;
    i32 level; // 0=info, 1=warning, 2=error
} LogEntry;

// Property for inspector
typedef struct {
    char name[64];
    i32 type; // 0=float, 1=int, 2=bool, 3=vec3, 4=color
    void* data;
    f32 min_val, max_val;
} Property;

// Editor GUI state
struct EditorGUI {
    // Renderer integration
    Renderer* renderer;
    ShaderHandle gui_shader;
    MaterialHandle gui_material;
    MeshHandle quad_mesh;
    
    // Theme and styling
    EditorTheme theme;
    
    // Panel management
    PanelConfig panels[PANEL_COUNT];
    bool panel_order[PANEL_COUNT]; // Z-order for rendering
    
    // Input state
    Vec2 mouse_pos;
    Vec2 last_mouse_pos;
    bool mouse_down[3];
    bool mouse_clicked[3];
    i32 hot_widget_id;
    i32 active_widget_id;
    
    // Console system
    LogEntry console_logs[256];
    i32 console_log_count;
    i32 console_log_head;
    bool console_auto_scroll;
    
    // Performance tracking
    f32 frame_times[120];
    i32 frame_time_index;
    f32 current_fps;
    f32 avg_frame_time;
    
    // Material editor
    MaterialHandle selected_material;
    Property material_properties[32];
    i32 material_property_count;
    
    // Hierarchy
    void* selected_object;
    char object_names[64][32];
    i32 object_count;
    
    // Asset browser
    char current_directory[256];
    char asset_filter[64];
    
    // Memory arena for temporary allocations
    MemoryArena* temp_arena;
};

// ============================================================================
// CORE SYSTEM
// ============================================================================

EditorGUI* EditorGUI_Create(PlatformState* platform, Renderer* renderer);
void EditorGUI_Destroy(EditorGUI* gui);
void EditorGUI_BeginFrame(EditorGUI* gui, PlatformInput* input);
void EditorGUI_EndFrame(EditorGUI* gui);

// ============================================================================
// PANEL SYSTEM
// ============================================================================

void EditorGUI_ShowPanel(EditorGUI* gui, PanelType type, bool show);
bool EditorGUI_IsPanelVisible(EditorGUI* gui, PanelType type);
void EditorGUI_SetPanelConfig(EditorGUI* gui, PanelType type, PanelConfig config);

// Individual panel rendering
void EditorGUI_DrawHierarchyPanel(EditorGUI* gui);
void EditorGUI_DrawInspectorPanel(EditorGUI* gui);
void EditorGUI_DrawConsolePanel(EditorGUI* gui);
void EditorGUI_DrawPerformancePanel(EditorGUI* gui);
void EditorGUI_DrawMaterialEditorPanel(EditorGUI* gui);
void EditorGUI_DrawAssetBrowserPanel(EditorGUI* gui);

// ============================================================================
// WIDGET SYSTEM
// ============================================================================

bool EditorGUI_Button(EditorGUI* gui, const char* label, f32 x, f32 y, f32 w, f32 h);
bool EditorGUI_Checkbox(EditorGUI* gui, const char* label, bool* value, f32 x, f32 y);
bool EditorGUI_SliderFloat(EditorGUI* gui, const char* label, f32* value, f32 min, f32 max, f32 x, f32 y, f32 w);
bool EditorGUI_InputText(EditorGUI* gui, const char* label, char* buffer, i32 buffer_size, f32 x, f32 y, f32 w);
void EditorGUI_Text(EditorGUI* gui, const char* text, f32 x, f32 y, Vec4 color);
bool EditorGUI_TreeNode(EditorGUI* gui, const char* label, bool* open, f32 x, f32 y);

// ============================================================================
// RENDERING PRIMITIVES
// ============================================================================

void EditorGUI_DrawRect(EditorGUI* gui, f32 x, f32 y, f32 w, f32 h, Vec4 color);
void EditorGUI_DrawRectOutline(EditorGUI* gui, f32 x, f32 y, f32 w, f32 h, Vec4 color, f32 thickness);
void EditorGUI_DrawLine(EditorGUI* gui, f32 x1, f32 y1, f32 x2, f32 y2, Vec4 color, f32 thickness);
void EditorGUI_DrawText(EditorGUI* gui, const char* text, f32 x, f32 y, Vec4 color);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void EditorGUI_Log(EditorGUI* gui, const char* message);
void EditorGUI_LogWarning(EditorGUI* gui, const char* message);
void EditorGUI_LogError(EditorGUI* gui, const char* message);
void EditorGUI_ClearLog(EditorGUI* gui);

void EditorGUI_UpdatePerformanceStats(EditorGUI* gui, f32 frame_time, f32 fps);

EditorTheme EditorGUI_DefaultTheme(void);
EditorTheme EditorGUI_DarkTheme(void);

#endif // EDITOR_GUI_H