/*
    Editor with Hot Reload Support
    ===============================
    
    This version of the editor supports state preservation across reloads.
*/

#define HOT_RELOAD_MODULE
#include "handmade_platform.h"
#include "handmade_hotreload.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <GL/gl.h>

// Basic math types
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;

// Editor state (preserved across reloads)
typedef struct {
    // Camera (preserved)
    v3 camera_position;
    v3 camera_rotation;
    f32 camera_zoom;
    
    // Viewport control (preserved)
    bool camera_rotating;
    bool camera_panning;
    f32 last_mouse_x;
    f32 last_mouse_y;
    
    // Editor panels (preserved)
    bool show_hierarchy;
    bool show_inspector;
    bool show_console;
    bool show_assets;
    f32 hierarchy_width;
    f32 inspector_width;
    f32 console_height;
    
    // Performance (preserved)
    f64 last_frame_time;
    f64 frame_time_accumulator;
    u32 frame_count;
    f32 fps;
    
    // Grid (preserved)
    bool show_grid;
    bool show_wireframe;
    bool show_stats;
    
    // Scene (preserved)
    u32 selected_object;
    u32 object_count;
    
    // Tools (preserved)
    enum { TOOL_SELECT, TOOL_MOVE, TOOL_ROTATE, TOOL_SCALE } current_tool;
    
    // Hot reload info
    u32 reload_count;
    f64 last_reload_time;
    
    // Initialized flag
    bool initialized;
} EditorState;

// Global editor state pointer (preserved in platform memory)
static EditorState* g_editor = NULL;

// Serialize editor state
GAME_MODULE_EXPORT void GameSerializeState(void* buffer, usize* size) {
    if (!g_editor) {
        *size = 0;
        return;
    }
    
    // Calculate size needed
    usize needed = sizeof(HotReloadStateHeader) + sizeof(EditorState);
    if (*size == 0) {
        *size = needed;
        return;
    }
    
    // Write header
    u8* write_ptr = (u8*)buffer;
    write_ptr += sizeof(HotReloadStateHeader); // Skip header (filled by system)
    
    // Write editor state
    memcpy(write_ptr, g_editor, sizeof(EditorState));
    write_ptr += sizeof(EditorState);
    
    *size = write_ptr - (u8*)buffer;
    
    printf("[Editor] Serialized %zu bytes of state\n", *size);
}

// Deserialize editor state
GAME_MODULE_EXPORT void GameDeserializeState(void* buffer, usize size) {
    if (!g_editor || size < sizeof(HotReloadStateHeader) + sizeof(EditorState)) {
        return;
    }
    
    // Skip header
    u8* read_ptr = (u8*)buffer + sizeof(HotReloadStateHeader);
    
    // Preserve the pointer but restore the data
    EditorState temp;
    memcpy(&temp, read_ptr, sizeof(EditorState));
    
    // Restore everything except the initialized flag
    bool was_initialized = g_editor->initialized;
    *g_editor = temp;
    g_editor->initialized = was_initialized;
    
    printf("[Editor] Deserialized %zu bytes of state\n", size);
}

// Called when module is about to be unloaded
GAME_MODULE_EXPORT void GameOnUnload(PlatformState* platform) {
    printf("[Editor] Module unloading (reload #%d)...\n", g_editor ? g_editor->reload_count : 0);
}

// Called after module is reloaded
GAME_MODULE_EXPORT void GameOnReload(PlatformState* platform) {
    if (g_editor) {
        g_editor->reload_count++;
        g_editor->last_reload_time = PlatformGetTime();
        printf("[Editor] Module reloaded! (reload #%d)\n", g_editor->reload_count);
        printf("[Editor] State preserved: camera=(%.1f,%.1f,%.1f) zoom=%.1f tool=%d\n",
               g_editor->camera_position.x, g_editor->camera_position.y, 
               g_editor->camera_position.z, g_editor->camera_zoom,
               g_editor->current_tool);
    }
}

// Get module version
GAME_MODULE_EXPORT u32 GameGetVersion(void) {
    return HOTRELOAD_MODULE_VERSION;
}

// Get build info
GAME_MODULE_EXPORT const char* GameGetBuildInfo(void) {
    return __DATE__ " " __TIME__;
}

// Drawing helpers (same as before)
static void DrawRect(f32 x, f32 y, f32 w, f32 h, v4 color) {
    glBegin(GL_QUADS);
    glColor4f(color.x, color.y, color.z, color.w);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void DrawText(f32 x, f32 y, const char* text, v4 color) {
    glColor4f(color.x, color.y, color.z, color.w);
    // Text rendering would go here
}

// Initialize editor
GAME_MODULE_EXPORT void GameInit(PlatformState* platform) {
    // Check if we already have state (from reload)
    if (!g_editor) {
        // First time init - allocate from permanent memory
        g_editor = PushStruct(&platform->permanent_arena, EditorState);
        
        // Initialize default state
        g_editor->camera_position = (v3){0, 0, 0};
        g_editor->camera_rotation = (v3){-30, 45, 0};
        g_editor->camera_zoom = 10.0f;
        
        g_editor->show_hierarchy = true;
        g_editor->show_inspector = true;
        g_editor->show_console = true;
        g_editor->show_grid = true;
        g_editor->show_stats = true;
        
        g_editor->hierarchy_width = 250;
        g_editor->inspector_width = 300;
        g_editor->console_height = 200;
        
        g_editor->current_tool = TOOL_SELECT;
        g_editor->reload_count = 0;
        
        printf("[Editor] First time initialization\n");
    } else {
        printf("[Editor] Reinitializing after reload\n");
    }
    
    // Setup OpenGL state (needs to be done every time)
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    g_editor->initialized = true;
}

// Update continues to work with preserved state...
GAME_MODULE_EXPORT void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_editor || !g_editor->initialized) return;
    
    PlatformInput* input = &platform->input;
    
    // All the update logic remains the same
    // The state is preserved across reloads!
    
    // Camera controls...
    // Tool selection...
    // FPS counter...
    
    // Show hot reload indicator
    if (g_editor->reload_count > 0 && g_editor->last_reload_time > 0) {
        f64 time_since_reload = PlatformGetTime() - g_editor->last_reload_time;
        if (time_since_reload < 2.0) { // Show for 2 seconds
            // Flash indicator showing successful reload
        }
    }
}

// Render (implementation same as before)
GAME_MODULE_EXPORT void GameRender(PlatformState* platform) {
    // Rendering code unchanged - uses preserved state
}

// Shutdown
GAME_MODULE_EXPORT void GameShutdown(PlatformState* platform) {
    printf("[Editor] Shutting down\n");
    g_editor = NULL;
}

// Export the module API
GAME_MODULE_EXPORT GameModuleAPI* GetGameModuleAPI(void) {
    static GameModuleAPI api = {
        .GameInit = GameInit,
        .GameUpdate = GameUpdate,
        .GameRender = GameRender,
        .GameShutdown = GameShutdown,
        .GameSerializeState = GameSerializeState,
        .GameDeserializeState = GameDeserializeState,
        .GameOnReload = GameOnReload,
        .GameOnUnload = GameOnUnload,
        .GameGetVersion = GameGetVersion,
        .GameGetBuildInfo = GameGetBuildInfo,
    };
    return &api;
}