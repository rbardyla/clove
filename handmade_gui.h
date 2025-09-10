#ifndef HANDMADE_GUI_H
#define HANDMADE_GUI_H

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include <stdbool.h>

// Immediate-mode GUI system for handmade engine
// Simple but functional GUI that builds on the working renderer

// GUI context state
typedef struct {
    bool initialized;
    
    // Renderer reference
    Renderer* renderer;
    
    // Input state (updated each frame)
    v2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_clicked;
    bool mouse_right_down;
    bool mouse_right_clicked;
    
    // Widget interaction state
    u64 hot_id;      // Widget mouse is hovering over
    u64 active_id;   // Widget being interacted with
    
    // Layout helpers
    v2 cursor;       // Current layout position
    f32 line_height; // Default line spacing
    
    // Style settings
    Color text_color;
    Color button_color;
    Color button_hover_color;
    Color button_active_color;
    Color panel_color;
    Color border_color;
    
    // Frame stats
    u32 widgets_drawn;
    
} HandmadeGUI;

// Initialization and frame management
bool HandmadeGUI_Init(HandmadeGUI* gui, Renderer* renderer);
void HandmadeGUI_Shutdown(HandmadeGUI* gui);
void HandmadeGUI_BeginFrame(HandmadeGUI* gui, PlatformState* platform);
void HandmadeGUI_EndFrame(HandmadeGUI* gui);

// Basic widgets
bool HandmadeGUI_Button(HandmadeGUI* gui, v2 position, v2 size, const char* text);
void HandmadeGUI_Text(HandmadeGUI* gui, v2 position, const char* text, f32 scale, Color color);
void HandmadeGUI_Label(HandmadeGUI* gui, v2 position, const char* text); // Uses default color/scale

// Panel system for grouping widgets
typedef struct {
    v2 position;
    v2 size;
    const char* title;
    bool* open;        // Pointer to bool that controls visibility
    bool has_close_button;
    bool is_draggable;
    v2 drag_offset;    // For dragging support
} GUIPanel;

bool HandmadeGUI_BeginPanel(HandmadeGUI* gui, GUIPanel* panel);
void HandmadeGUI_EndPanel(HandmadeGUI* gui);

// Layout helpers
void HandmadeGUI_SetCursor(HandmadeGUI* gui, v2 position);
v2 HandmadeGUI_GetCursor(HandmadeGUI* gui);
void HandmadeGUI_SameLine(HandmadeGUI* gui);
void HandmadeGUI_NewLine(HandmadeGUI* gui);
void HandmadeGUI_Indent(HandmadeGUI* gui, f32 amount);

// Simple input widgets
bool HandmadeGUI_Checkbox(HandmadeGUI* gui, v2 position, const char* label, bool* value);
bool HandmadeGUI_SliderFloat(HandmadeGUI* gui, v2 position, v2 size, const char* label, 
                             f32* value, f32 min_val, f32 max_val);

// Drawing helpers
void HandmadeGUI_DrawRect(HandmadeGUI* gui, v2 position, v2 size, Color color);
void HandmadeGUI_DrawRectOutline(HandmadeGUI* gui, v2 position, v2 size, f32 thickness, Color color);
void HandmadeGUI_Separator(HandmadeGUI* gui, v2 position, f32 width);

// Utility functions
u64 HandmadeGUI_HashString(const char* str);
u64 HandmadeGUI_HashPointer(void* ptr);
bool HandmadeGUI_IsMouseInRect(HandmadeGUI* gui, v2 position, v2 size);

// Style functions
void HandmadeGUI_SetDefaultStyle(HandmadeGUI* gui);
void HandmadeGUI_PushColor(HandmadeGUI* gui, Color color); // For temporary color changes
void HandmadeGUI_PopColor(HandmadeGUI* gui);

// Debug and info display
void HandmadeGUI_ShowDebugPanel(HandmadeGUI* gui, v2 position);

#endif // HANDMADE_GUI_H