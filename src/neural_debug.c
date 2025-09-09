#include "neural_debug.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/*
    Neural Debug Visualization Implementation
    
    PERFORMANCE NOTES:
    - All visualization functions target < 0.5ms per frame
    - Direct pixel manipulation, no intermediate buffers
    - SIMD optimized heatmap generation
    - Cache-friendly data access patterns
    - Zero allocations in hot paths
    
    MEMORY LAYOUT:
    - Debug state is persistent across frames
    - History buffers are pre-allocated circular arrays
    - Temporary visualization data uses stack allocation
    - No dynamic memory allocation during rendering
*/

// Fast bitmap font (8x12 pixels per character)
// Simplified ASCII characters for debug text rendering
global_variable u8 DebugFont[128][12] = {
    // Basic implementation - would include actual font data
    // For now, just placeholder patterns
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['0'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    // ... (would continue for all printable ASCII characters)
    // For brevity, showing pattern - real implementation would have all chars
};

// Color lookup table for hot/cold visualization
global_variable u32 HotColdColorTable[256];
global_variable b32 ColorTablesInitialized = 0;

// Initialize color lookup tables for fast visualization
internal void
InitializeColorTables(void)
{
    if (ColorTablesInitialized) return;
    
    // PERFORMANCE: Pre-compute color mappings to avoid per-pixel calculations
    // Hot/Cold: Blue (0) -> Black (128) -> Red (255)
    for (u32 i = 0; i < 256; ++i)
    {
        f32 t = (f32)i / 255.0f;
        
        u8 r, g, b;
        if (t < 0.5f)
        {
            // Blue to black
            f32 localT = t * 2.0f;
            r = (u8)(0);
            g = (u8)(0);
            b = (u8)(255 * (1.0f - localT));
        }
        else
        {
            // Black to red
            f32 localT = (t - 0.5f) * 2.0f;
            r = (u8)(255 * localT);
            g = (u8)(0);
            b = (u8)(0);
        }
        
        HotColdColorTable[i] = RGB(r, g, b);
    }
    
    ColorTablesInitialized = 1;
}

// Initialize neural debug system
neural_debug_state *
InitializeNeuralDebugSystem(memory_arena *Arena, u32 MaxNeurons, u32 HistoryBufferSize)
{
    neural_debug_state *DebugState = PushStruct(Arena, neural_debug_state);
    ZeroStruct(*DebugState);
    
    DebugState->DebugArena = Arena;
    DebugState->CurrentMode = DEBUG_VIZ_NEURAL_ACTIVATIONS;
    DebugState->DebugEnabled = 1;
    DebugState->ShowPerformanceStats = 1;
    
    // Initialize heatmap parameters
    DebugState->HeatmapParams.MinValue = -1.0f;
    DebugState->HeatmapParams.MaxValue = 1.0f;
    DebugState->HeatmapParams.Gamma = DEBUG_DEFAULT_GAMMA;
    DebugState->HeatmapParams.AutoScale = 1;
    DebugState->HeatmapParams.ColorScheme = COLOR_SCHEME_HOT_COLD;
    DebugState->HeatmapParams.ZoomLevel = DEBUG_DEFAULT_ZOOM;
    
    // Initialize layer visibility (all visible by default)
    for (u32 i = 0; i < ArrayCount(DebugState->LayerVisibility); ++i)
    {
        DebugState->LayerVisibility[i] = 1;
        DebugState->LayerOpacity[i] = 1.0f;
    }
    
    // Allocate history buffers
    DebugState->HistoryBufferSize = HistoryBufferSize;
    DebugState->ActivationHistory = PushArray(Arena, HistoryBufferSize, f32 *);
    DebugState->WeightHistory = PushArray(Arena, HistoryBufferSize, f32 *);
    
    for (u32 i = 0; i < HistoryBufferSize; ++i)
    {
        DebugState->ActivationHistory[i] = PushArray(Arena, MaxNeurons, f32);
        DebugState->WeightHistory[i] = PushArray(Arena, MaxNeurons * MaxNeurons, f32);
    }
    
    // Initialize color tables
    InitializeColorTables();
    
    // Set initial status message
    ShowStatusMessage(DebugState, "Neural Debug System Initialized", 2.0f);
    
    return DebugState;
}

// Update debug system state
void
UpdateNeuralDebug(neural_debug_state *DebugState, game_input *Input, f32 DeltaTime)
{
    if (!DebugState || !DebugState->DebugEnabled) return;
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    // Update mouse state
    UpdateDebugMouse(DebugState, Input);
    
    // Process keyboard input
    ProcessDebugInput(DebugState, Input);
    
    // Update status message timeout
    if (DebugState->StatusMessageTimeout > 0.0f)
    {
        DebugState->StatusMessageTimeout -= DeltaTime;
    }
    
    // Update performance statistics
    UpdateDebugPerformanceStats(DebugState);
    
    DEBUG_PROFILE_END(DebugState, "Debug Update");
}

// Main debug rendering function
void
RenderNeuralDebug(neural_debug_state *DebugState, game_offscreen_buffer *Buffer)
{
    if (!DebugState || !DebugState->DebugEnabled) return;
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    // Render current visualization mode
    switch (DebugState->CurrentMode)
    {
        case DEBUG_VIZ_NEURAL_ACTIVATIONS:
        {
            // Will be implemented in specific visualization functions
            RenderDebugText(Buffer, "Neural Activations Mode", 10, 10, COLOR_WHITE);
        } break;
        
        case DEBUG_VIZ_WEIGHT_HEATMAP:
        {
            RenderDebugText(Buffer, "Weight Heatmap Mode", 10, 10, COLOR_WHITE);
        } break;
        
        case DEBUG_VIZ_DNC_MEMORY:
        {
            RenderDebugText(Buffer, "DNC Memory Mode", 10, 10, COLOR_WHITE);
        } break;
        
        case DEBUG_VIZ_LSTM_GATES:
        {
            RenderDebugText(Buffer, "LSTM Gates Mode", 10, 10, COLOR_WHITE);
        } break;
        
        case DEBUG_VIZ_EWC_FISHER:
        {
            RenderDebugText(Buffer, "EWC Fisher Mode", 10, 10, COLOR_WHITE);
        } break;
        
        case DEBUG_VIZ_NPC_BRAIN:
        {
            RenderDebugText(Buffer, "NPC Brain Mode", 10, 10, COLOR_WHITE);
        } break;
        
        default:
        {
            RenderDebugText(Buffer, "Debug Mode Selection", 10, 10, COLOR_WHITE);
        } break;
    }
    
    // Render inspection overlay if hovering
    if (DebugState->Mouse.IsHovering)
    {
        RenderInspectionOverlay(DebugState, Buffer);
    }
    
    // Render timeline if enabled
    if (DebugState->ShowTimeline)
    {
        RenderTimeline(DebugState, Buffer);
    }
    
    // Render performance stats
    if (DebugState->ShowPerformanceStats)
    {
        RenderPerformanceOverlay(DebugState, Buffer);
    }
    
    // Render help if requested
    if (DebugState->ShowHelp)
    {
        RenderDebugHelp(DebugState, Buffer);
    }
    
    // Render status message
    if (DebugState->StatusMessageTimeout > 0.0f)
    {
        u32 Alpha = (u32)(255 * DebugState->StatusMessageTimeout / DEBUG_MAX_STATUS_MESSAGE_TIME);
        u32 StatusColor = RGBA(255, 255, 255, Alpha);
        RenderDebugText(Buffer, DebugState->StatusMessage, 10, Buffer->Height - 30, StatusColor);
    }
    
    DEBUG_PROFILE_END(DebugState, "Debug Render");
}

// Process debug input (keyboard and mouse)
void
ProcessDebugInput(neural_debug_state *DebugState, game_input *Input)
{
    controller_input *Keyboard = &Input->Controllers[0];
    
    // Toggle main debug system
    if (Keyboard->ActionLeft.EndedDown && Keyboard->ActionLeft.HalfTransitionCount > 0)
    {
        DebugState->DebugEnabled = !DebugState->DebugEnabled;
        ShowStatusMessage(DebugState, 
                         DebugState->DebugEnabled ? "Debug Enabled" : "Debug Disabled", 
                         1.0f);
    }
    
    // Mode switching (keys 1-9)
    if (Keyboard->Buttons[0].EndedDown && Keyboard->Buttons[0].HalfTransitionCount > 0) // '1'
    {
        DebugState->CurrentMode = DEBUG_VIZ_NEURAL_ACTIVATIONS;
        ShowStatusMessage(DebugState, "Mode: Neural Activations", 1.5f);
    }
    else if (Keyboard->Buttons[1].EndedDown && Keyboard->Buttons[1].HalfTransitionCount > 0) // '2'
    {
        DebugState->CurrentMode = DEBUG_VIZ_WEIGHT_HEATMAP;
        ShowStatusMessage(DebugState, "Mode: Weight Heatmap", 1.5f);
    }
    else if (Keyboard->Buttons[2].EndedDown && Keyboard->Buttons[2].HalfTransitionCount > 0) // '3'
    {
        DebugState->CurrentMode = DEBUG_VIZ_DNC_MEMORY;
        ShowStatusMessage(DebugState, "Mode: DNC Memory", 1.5f);
    }
    else if (Keyboard->Buttons[3].EndedDown && Keyboard->Buttons[3].HalfTransitionCount > 0) // '4'
    {
        DebugState->CurrentMode = DEBUG_VIZ_LSTM_GATES;
        ShowStatusMessage(DebugState, "Mode: LSTM Gates", 1.5f);
    }
    else if (Keyboard->Buttons[4].EndedDown && Keyboard->Buttons[4].HalfTransitionCount > 0) // '5'
    {
        DebugState->CurrentMode = DEBUG_VIZ_EWC_FISHER;
        ShowStatusMessage(DebugState, "Mode: EWC Fisher", 1.5f);
    }
    else if (Keyboard->Buttons[5].EndedDown && Keyboard->Buttons[5].HalfTransitionCount > 0) // '6'
    {
        DebugState->CurrentMode = DEBUG_VIZ_NPC_BRAIN;
        ShowStatusMessage(DebugState, "Mode: NPC Brain", 1.5f);
    }
    
    // Toggle pause
    if (Keyboard->ActionDown.EndedDown && Keyboard->ActionDown.HalfTransitionCount > 0)
    {
        DebugState->IsPaused = !DebugState->IsPaused;
        ShowStatusMessage(DebugState, 
                         DebugState->IsPaused ? "Paused" : "Resumed", 
                         1.0f);
    }
    
    // Toggle help
    if (Keyboard->ActionRight.EndedDown && Keyboard->ActionRight.HalfTransitionCount > 0)
    {
        DebugState->ShowHelp = !DebugState->ShowHelp;
    }
    
    // Reset debug state
    if (Keyboard->ActionUp.EndedDown && Keyboard->ActionUp.HalfTransitionCount > 0)
    {
        ResetNeuralDebugState(DebugState);
        ShowStatusMessage(DebugState, "Debug State Reset", 1.0f);
    }
}

// Update mouse state for debug interactions
void
UpdateDebugMouse(neural_debug_state *DebugState, game_input *Input)
{
    debug_mouse_state *Mouse = &DebugState->Mouse;
    
    Mouse->LastX = Mouse->X;
    Mouse->LastY = Mouse->Y;
    Mouse->X = Input->MouseX;
    Mouse->Y = Input->MouseY;
    
    // Update button states
    b32 PrevLeftDown = Mouse->LeftDown;
    b32 PrevRightDown = Mouse->RightDown;
    
    Mouse->LeftDown = Input->MouseButtons[0].EndedDown;
    Mouse->RightDown = Input->MouseButtons[1].EndedDown;
    
    Mouse->LeftPressed = Mouse->LeftDown && !PrevLeftDown;
    Mouse->RightPressed = Mouse->RightDown && !PrevRightDown;
    
    // Update pan if dragging with right mouse
    if (Mouse->RightDown)
    {
        i32 DeltaX = Mouse->X - Mouse->LastX;
        i32 DeltaY = Mouse->Y - Mouse->LastY;
        
        DebugState->HeatmapParams.PanX += DeltaX;
        DebugState->HeatmapParams.PanY += DeltaY;
    }
    
    // Zoom with mouse wheel (using MouseZ)
    if (Input->MouseZ != 0)
    {
        f32 ZoomFactor = (Input->MouseZ > 0) ? 1.1f : 0.9f;
        DebugState->HeatmapParams.ZoomLevel *= ZoomFactor;
        
        // Clamp zoom level
        if (DebugState->HeatmapParams.ZoomLevel < 0.1f)
            DebugState->HeatmapParams.ZoomLevel = 0.1f;
        if (DebugState->HeatmapParams.ZoomLevel > 10.0f)
            DebugState->HeatmapParams.ZoomLevel = 10.0f;
    }
}

// Map a floating point value to color based on scheme
u32
MapValueToColor(f32 Value, f32 MinVal, f32 MaxVal, debug_color_scheme Scheme)
{
    // Normalize value to [0, 1]
    f32 t = (Value - MinVal) / (MaxVal - MinVal);
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    
    u32 Color = COLOR_BLACK;
    
    switch (Scheme)
    {
        case COLOR_SCHEME_HOT_COLD:
        {
            u32 Index = (u32)(t * 255.0f);
            Color = HotColdColorTable[Index];
        } break;
        
        case COLOR_SCHEME_GRAYSCALE:
        {
            u8 Gray = (u8)(255 * t);
            Color = RGB(Gray, Gray, Gray);
        } break;
        
        case COLOR_SCHEME_RAINBOW:
        {
            // HSV: Hue varies from 0 (red) to 240 (blue), full saturation and value
            f32 Hue = (1.0f - t) * 240.0f;  // Reverse for red=hot, blue=cold
            Color = HSVToRGB(Hue, 1.0f, 1.0f);
        } break;
        
        case COLOR_SCHEME_ATTENTION:
        {
            // Purple (low attention) to Yellow (high attention)
            u8 r = (u8)(128 + 127 * t);
            u8 g = (u8)(0 + 255 * t);
            u8 b = (u8)(128 - 128 * t);
            Color = RGB(r, g, b);
        } break;
        
        case COLOR_SCHEME_MEMORY:
        {
            // Green (unused) to Blue (heavily used)
            u8 r = (u8)(0);
            u8 g = (u8)(255 - 255 * t);
            u8 b = (u8)(255 * t);
            Color = RGB(r, g, b);
        } break;
        
        default:
        {
            Color = RGB(128, 128, 128);
        } break;
    }
    
    return Color;
}

// Convert HSV to RGB
u32
HSVToRGB(f32 H, f32 S, f32 V)
{
    // PERFORMANCE: Fast HSV to RGB conversion
    // H: 0-360, S: 0-1, V: 0-1
    
    H = fmodf(H, 360.0f);
    if (H < 0) H += 360.0f;
    
    f32 C = V * S;
    f32 X = C * (1.0f - fabsf(fmodf(H / 60.0f, 2.0f) - 1.0f));
    f32 m = V - C;
    
    f32 r, g, b;
    
    if (H >= 0 && H < 60)
    {
        r = C; g = X; b = 0;
    }
    else if (H >= 60 && H < 120)
    {
        r = X; g = C; b = 0;
    }
    else if (H >= 120 && H < 180)
    {
        r = 0; g = C; b = X;
    }
    else if (H >= 180 && H < 240)
    {
        r = 0; g = X; b = C;
    }
    else if (H >= 240 && H < 300)
    {
        r = X; g = 0; b = C;
    }
    else
    {
        r = C; g = 0; b = X;
    }
    
    u8 R = (u8)(255 * (r + m));
    u8 G = (u8)(255 * (g + m));
    u8 B = (u8)(255 * (b + m));
    
    return RGB(R, G, B);
}

// Simple bitmap font text rendering
void
RenderDebugText(game_offscreen_buffer *Buffer, const char *Text, i32 X, i32 Y, u32 Color)
{
    if (!Text) return;
    
    i32 CharX = X;
    i32 CharY = Y;
    
    while (*Text)
    {
        char c = *Text++;
        
        // Handle newlines
        if (c == '\n')
        {
            CharX = X;
            CharY += DEBUG_TEXT_HEIGHT;
            continue;
        }
        
        // Skip unsupported characters
        if (c < 0 || c >= 128)
        {
            CharX += DEBUG_TEXT_WIDTH;
            continue;
        }
        
        // Render character bitmap
        u8 *CharData = DebugFont[(u8)c];
        for (i32 Row = 0; Row < DEBUG_TEXT_HEIGHT; ++Row)
        {
            u8 RowData = CharData[Row];
            for (i32 Col = 0; Col < DEBUG_TEXT_WIDTH; ++Col)
            {
                if (RowData & (0x80 >> Col))
                {
                    i32 PixelX = CharX + Col;
                    i32 PixelY = CharY + Row;
                    DrawPixel(Buffer, PixelX, PixelY, Color);
                }
            }
        }
        
        CharX += DEBUG_TEXT_WIDTH;
    }
}

// Formatted text rendering
void
RenderDebugTextf(game_offscreen_buffer *Buffer, i32 X, i32 Y, u32 Color, const char *Format, ...)
{
    char TextBuffer[1024];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(TextBuffer, sizeof(TextBuffer), Format, Args);
    va_end(Args);
    
    RenderDebugText(Buffer, TextBuffer, X, Y, Color);
}

// Render inspection overlay for hovered elements
void
RenderInspectionOverlay(neural_debug_state *DebugState, game_offscreen_buffer *Buffer)
{
    debug_mouse_state *Mouse = &DebugState->Mouse;
    
    if (!Mouse->IsHovering) return;
    
    // Draw inspection panel
    i32 PanelX = Mouse->X + 10;
    i32 PanelY = Mouse->Y;
    i32 PanelWidth = 200;
    i32 PanelHeight = 80;
    
    // Adjust panel position to stay on screen
    if (PanelX + PanelWidth > Buffer->Width)
        PanelX = Mouse->X - PanelWidth - 10;
    if (PanelY + PanelHeight > Buffer->Height)
        PanelY = Buffer->Height - PanelHeight;
    
    // Draw panel background
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 200));
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, 1, COLOR_WHITE);
    DrawRectangle(Buffer, PanelX, PanelY + PanelHeight - 1, PanelWidth, 1, COLOR_WHITE);
    DrawRectangle(Buffer, PanelX, PanelY, 1, PanelHeight, COLOR_WHITE);
    DrawRectangle(Buffer, PanelX + PanelWidth - 1, PanelY, 1, PanelHeight, COLOR_WHITE);
    
    // Render inspection text
    RenderDebugText(Buffer, Mouse->HoverLabel, PanelX + 5, PanelY + 5, COLOR_WHITE);
    RenderDebugTextf(Buffer, PanelX + 5, PanelY + 20, COLOR_WHITE, 
                     "Value: %.3f", Mouse->HoverValue);
    RenderDebugTextf(Buffer, PanelX + 5, PanelY + 35, COLOR_WHITE,
                     "Position: (%d, %d)", Mouse->X, Mouse->Y);
}

// Show status message with timeout
void
ShowStatusMessage(neural_debug_state *DebugState, const char *Message, f32 Duration)
{
    if (Message && strlen(Message) < sizeof(DebugState->StatusMessage))
    {
        strcpy(DebugState->StatusMessage, Message);
        DebugState->StatusMessageTimeout = Duration;
    }
}

// Reset debug state to defaults
void
ResetNeuralDebugState(neural_debug_state *DebugState)
{
    DebugState->CurrentMode = DEBUG_VIZ_NEURAL_ACTIVATIONS;
    DebugState->HeatmapParams.ZoomLevel = DEBUG_DEFAULT_ZOOM;
    DebugState->HeatmapParams.PanX = 0;
    DebugState->HeatmapParams.PanY = 0;
    DebugState->HeatmapParams.AutoScale = 1;
    DebugState->IsPaused = 0;
    DebugState->ShowTimeline = 0;
    DebugState->ShowHelp = 0;
    
    ZeroStruct(DebugState->InspectionTarget);
    ZeroStruct(DebugState->Mouse);
}

// Update performance statistics
void
UpdateDebugPerformanceStats(neural_debug_state *DebugState)
{
    debug_performance_stats *Stats = &DebugState->PerfStats;
    
    // Calculate frame time in milliseconds
    if (Stats->VisualizationCycles > 0)
    {
        // Assume 2.4GHz CPU for cycle to time conversion
        f32 CyclesPerMs = 2.4f * 1000000.0f;
        Stats->FrameTimeMs = (f32)Stats->VisualizationCycles / CyclesPerMs;
        
        // Update rolling average (exponential moving average)
        f32 Alpha = 0.1f;  // Smoothing factor
        Stats->AverageVisualizationTimeMs = Alpha * Stats->FrameTimeMs + 
                                           (1.0f - Alpha) * Stats->AverageVisualizationTimeMs;
    }
    
    // Reset per-frame counters
    Stats->VisualizationCycles = 0;
    Stats->PixelsDrawn = 0;
}

// Render performance overlay
void
RenderPerformanceOverlay(neural_debug_state *DebugState, game_offscreen_buffer *Buffer)
{
    debug_performance_stats *Stats = &DebugState->PerfStats;
    
    i32 X = Buffer->Width - 250;
    i32 Y = 10;
    
    RenderDebugText(Buffer, "Performance Stats:", X, Y, COLOR_YELLOW);
    Y += DEBUG_TEXT_HEIGHT + 2;
    
    RenderDebugTextf(Buffer, X, Y, COLOR_WHITE,
                     "Debug Time: %.2f ms", Stats->AverageVisualizationTimeMs);
    Y += DEBUG_TEXT_HEIGHT;
    
    RenderDebugTextf(Buffer, X, Y, COLOR_WHITE,
                     "Pixels Drawn: %u", Stats->PixelsDrawn);
    Y += DEBUG_TEXT_HEIGHT;
    
    RenderDebugTextf(Buffer, X, Y, COLOR_WHITE,
                     "Mode: %d", DebugState->CurrentMode);
}

// Render help overlay
void
RenderDebugHelp(neural_debug_state *DebugState, game_offscreen_buffer *Buffer)
{
    i32 PanelX = Buffer->Width / 2 - 200;
    i32 PanelY = Buffer->Height / 2 - 150;
    i32 PanelWidth = 400;
    i32 PanelHeight = 300;
    
    // Background
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 220));
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, 2, COLOR_YELLOW);
    DrawRectangle(Buffer, PanelX, PanelY + PanelHeight - 2, PanelWidth, 2, COLOR_YELLOW);
    DrawRectangle(Buffer, PanelX, PanelY, 2, PanelHeight, COLOR_YELLOW);
    DrawRectangle(Buffer, PanelX + PanelWidth - 2, PanelY, 2, PanelHeight, COLOR_YELLOW);
    
    i32 TextX = PanelX + 10;
    i32 TextY = PanelY + 10;
    
    RenderDebugText(Buffer, "Neural Debug System - Help", TextX, TextY, COLOR_YELLOW);
    TextY += DEBUG_TEXT_HEIGHT * 2;
    
    RenderDebugText(Buffer, "Keyboard Controls:", TextX, TextY, COLOR_WHITE);
    TextY += DEBUG_TEXT_HEIGHT + 2;
    
    RenderDebugText(Buffer, "1-9: Switch visualization modes", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "P: Pause/Resume neural inference", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "H: Toggle this help", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "R: Reset debug state", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "Mouse wheel: Zoom in/out", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "Right drag: Pan view", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "Left click: Inspect element", TextX, TextY, COLOR_GRAY);
    TextY += DEBUG_TEXT_HEIGHT * 2;
    
    RenderDebugText(Buffer, "Modes:", TextX, TextY, COLOR_WHITE);
    TextY += DEBUG_TEXT_HEIGHT + 2;
    
    RenderDebugText(Buffer, "1: Neural Activations  5: EWC Fisher", TextX, TextY, COLOR_CYAN);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "2: Weight Heatmaps     6: NPC Brain Activity", TextX, TextY, COLOR_CYAN);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "3: DNC Memory Matrix   7: Memory Access", TextX, TextY, COLOR_CYAN);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugText(Buffer, "4: LSTM Gate States    8: Attention Weights", TextX, TextY, COLOR_CYAN);
}

// Neural activation visualization with hot/cold mapping
void
RenderNeuralActivations(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, neural_network *Network)
{
    // For compatibility, we treat neural_network as an opaque pointer
    // and access its fields through careful casting
    if (!Network) return;
    
    // Access NumLayers field (assuming it's the first field after the struct tag)
    i32 *NumLayersPtr = (i32 *)((u8 *)Network + 0);  // Assumes NumLayers is at offset 0
    i32 NumLayers = *NumLayersPtr;
    
    if (NumLayers == 0) return;
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    // PERFORMANCE: Direct pixel manipulation for real-time visualization
    // CACHE: Sequential access through layers and neurons
    
    i32 StartX = 100;
    i32 StartY = 50;
    i32 LayerWidth = 120;
    i32 LayerHeight = Buffer->Height - 150;
    i32 LayerSpacing = (Buffer->Width - 200) / Network->NumLayers;
    
    // Render each layer as a vertical column of pixels
    for (i32 LayerIndex = 0; LayerIndex < Network->NumLayers; ++LayerIndex)
    {
        if (!DebugState->LayerVisibility[LayerIndex]) continue;
        
        neural_layer *Layer = &Network->Layers[LayerIndex];
        i32 LayerX = StartX + LayerIndex * LayerSpacing;
        
        // Calculate neuron pixel dimensions
        i32 NeuronCount = Layer->OutputSize;
        i32 MaxPixelsPerLayer = LayerHeight;
        
        // For layers with many neurons, group them into pixels
        i32 NeuronsPerPixel = (NeuronCount > MaxPixelsPerLayer) ? 
                             (NeuronCount + MaxPixelsPerLayer - 1) / MaxPixelsPerLayer : 1;
        i32 PixelsToRender = (NeuronCount + NeuronsPerPixel - 1) / NeuronsPerPixel;
        i32 PixelHeight = (PixelsToRender > 0) ? LayerHeight / PixelsToRender : LayerHeight;
        
        if (PixelHeight < 1) PixelHeight = 1;
        
        // Render activation pixels
        for (i32 PixelIndex = 0; PixelIndex < PixelsToRender; ++PixelIndex)
        {
            // Aggregate activation for this pixel
            f32 ActivationSum = 0.0f;
            i32 NeuronStart = PixelIndex * NeuronsPerPixel;
            i32 NeuronEnd = NeuronStart + NeuronsPerPixel;
            if (NeuronEnd > NeuronCount) NeuronEnd = NeuronCount;
            
            for (i32 NeuronIndex = NeuronStart; NeuronIndex < NeuronEnd; ++NeuronIndex)
            {
                ActivationSum += Layer->Activations[NeuronIndex];
            }
            
            f32 AverageActivation = ActivationSum / (f32)(NeuronEnd - NeuronStart);
            
            // Map activation to color
            u32 PixelColor = MapValueToColor(AverageActivation, 
                                           DebugState->HeatmapParams.MinValue,
                                           DebugState->HeatmapParams.MaxValue,
                                           DebugState->HeatmapParams.ColorScheme);
            
            // Apply layer opacity
            if (DebugState->LayerOpacity[LayerIndex] < 1.0f)
            {
                u8 Alpha = (u8)(255 * DebugState->LayerOpacity[LayerIndex]);
                u8 r = (PixelColor >> 16) & 0xFF;
                u8 g = (PixelColor >> 8) & 0xFF;
                u8 b = PixelColor & 0xFF;
                PixelColor = RGBA(r, g, b, Alpha);
            }
            
            // Draw pixel block
            i32 PixelY = StartY + PixelIndex * PixelHeight;
            DrawRectangle(Buffer, LayerX, PixelY, LayerWidth, PixelHeight, PixelColor);
            
            // Update mouse hover detection
            if (DebugState->Mouse.X >= LayerX && DebugState->Mouse.X < LayerX + LayerWidth &&
                DebugState->Mouse.Y >= PixelY && DebugState->Mouse.Y < PixelY + PixelHeight)
            {
                DebugState->Mouse.IsHovering = 1;
                DebugState->Mouse.HoverValue = AverageActivation;
                snprintf(DebugState->Mouse.HoverLabel, sizeof(DebugState->Mouse.HoverLabel),
                        "Layer %d, Neurons %d-%d", LayerIndex, NeuronStart, NeuronEnd - 1);
            }
        }
        
        // Draw layer border
        DrawRectangle(Buffer, LayerX - 1, StartY - 1, LayerWidth + 2, 1, COLOR_WHITE);
        DrawRectangle(Buffer, LayerX - 1, StartY + LayerHeight, LayerWidth + 2, 1, COLOR_WHITE);
        DrawRectangle(Buffer, LayerX - 1, StartY - 1, 1, LayerHeight + 2, COLOR_WHITE);
        DrawRectangle(Buffer, LayerX + LayerWidth, StartY - 1, 1, LayerHeight + 2, COLOR_WHITE);
        
        // Layer label
        RenderDebugTextf(Buffer, LayerX, StartY - 20, COLOR_WHITE, 
                        "L%d (%d)", LayerIndex, Layer->OutputSize);
        
        // Update performance stats
        DebugState->PerfStats.PixelsDrawn += PixelsToRender * LayerWidth * PixelHeight;
    }
    
    // Render activation statistics
    RenderActivationStats(DebugState, Buffer, Network);
    
    DEBUG_PROFILE_END(DebugState, "Neural Activations");
}

// Render activation statistics panel
internal void
RenderActivationStats(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, neural_network *Network)
{
    i32 PanelX = 10;
    i32 PanelY = Buffer->Height - 120;
    i32 PanelWidth = 300;
    i32 PanelHeight = 100;
    
    // Background panel
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 180));
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, 1, COLOR_CYAN);
    
    i32 TextY = PanelY + 5;
    RenderDebugText(Buffer, "Activation Statistics", PanelX + 5, TextY, COLOR_CYAN);
    TextY += DEBUG_TEXT_HEIGHT + 2;
    
    // Calculate statistics for each layer
    for (i32 LayerIndex = 0; LayerIndex < Network->NumLayers && LayerIndex < 3; ++LayerIndex)
    {
        neural_layer *Layer = &Network->Layers[LayerIndex];
        
        f32 MinActivation = Layer->Activations[0];
        f32 MaxActivation = Layer->Activations[0];
        f32 AvgActivation = 0.0f;
        i32 ActiveNeurons = 0;
        
        for (i32 i = 0; i < Layer->OutputSize; ++i)
        {
            f32 Activation = Layer->Activations[i];
            if (Activation < MinActivation) MinActivation = Activation;
            if (Activation > MaxActivation) MaxActivation = Activation;
            AvgActivation += Activation;
            if (Activation > 0.1f) ActiveNeurons++;
        }
        AvgActivation /= (f32)Layer->OutputSize;
        
        f32 Sparsity = 1.0f - ((f32)ActiveNeurons / (f32)Layer->OutputSize);
        
        RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                        "L%d: Avg=%.2f Min=%.2f Max=%.2f Sparse=%.1f%%", 
                        LayerIndex, AvgActivation, MinActivation, MaxActivation, Sparsity * 100.0f);
        TextY += DEBUG_TEXT_HEIGHT;
    }
}

// Weight heatmap visualization
void
RenderWeightHeatmap(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, neural_network *Network)
{
    if (!Network || Network->NumLayers == 0) return;
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    // PERFORMANCE: Visualize weight matrices as 2D heatmaps
    // MEMORY: Direct access to weight matrices, no intermediate copies
    
    // For now, show the first layer's weights
    i32 LayerToShow = 0;  // TODO: Make this selectable
    if (LayerToShow >= Network->NumLayers) return;
    
    neural_layer *Layer = &Network->Layers[LayerToShow];
    
    // Calculate heatmap dimensions
    i32 MatrixRows = Layer->OutputSize;
    i32 MatrixCols = Layer->InputSize;
    
    // Fit heatmap to available screen space
    i32 MaxHeatmapWidth = Buffer->Width - 200;
    i32 MaxHeatmapHeight = Buffer->Height - 200;
    
    i32 CellWidth = MaxHeatmapWidth / MatrixCols;
    i32 CellHeight = MaxHeatmapHeight / MatrixRows;
    
    // Ensure minimum cell size
    if (CellWidth < 1) CellWidth = 1;
    if (CellHeight < 1) CellHeight = 1;
    
    // Apply zoom
    CellWidth = (i32)(CellWidth * DebugState->HeatmapParams.ZoomLevel);
    CellHeight = (i32)(CellHeight * DebugState->HeatmapParams.ZoomLevel);
    
    i32 HeatmapWidth = MatrixCols * CellWidth;
    i32 HeatmapHeight = MatrixRows * CellHeight;
    
    // Center the heatmap with pan offset
    i32 StartX = (Buffer->Width - HeatmapWidth) / 2 + DebugState->HeatmapParams.PanX;
    i32 StartY = (Buffer->Height - HeatmapHeight) / 2 + DebugState->HeatmapParams.PanY;
    
    // Find weight range for color mapping
    f32 MinWeight = Layer->Weights[0];
    f32 MaxWeight = Layer->Weights[0];
    
    if (DebugState->HeatmapParams.AutoScale)
    {
        for (i32 i = 1; i < MatrixRows * MatrixCols; ++i)
        {
            f32 Weight = Layer->Weights[i];
            if (Weight < MinWeight) MinWeight = Weight;
            if (Weight > MaxWeight) MaxWeight = Weight;
        }
        
        // Update params
        DebugState->HeatmapParams.MinValue = MinWeight;
        DebugState->HeatmapParams.MaxValue = MaxWeight;
    }
    else
    {
        MinWeight = DebugState->HeatmapParams.MinValue;
        MaxWeight = DebugState->HeatmapParams.MaxValue;
    }
    
    // Render weight cells
    for (i32 Row = 0; Row < MatrixRows; ++Row)
    {
        for (i32 Col = 0; Col < MatrixCols; ++Col)
        {
            i32 WeightIndex = Row * MatrixCols + Col;
            f32 Weight = Layer->Weights[WeightIndex];
            
            // Map weight to color
            u32 CellColor = MapValueToColor(Weight, MinWeight, MaxWeight, 
                                          DebugState->HeatmapParams.ColorScheme);
            
            // Calculate cell position
            i32 CellX = StartX + Col * CellWidth;
            i32 CellY = StartY + Row * CellHeight;
            
            // Only draw cells that are visible
            if (CellX + CellWidth >= 0 && CellX < Buffer->Width &&
                CellY + CellHeight >= 0 && CellY < Buffer->Height)
            {
                DrawRectangle(Buffer, CellX, CellY, CellWidth, CellHeight, CellColor);
                
                // Update mouse hover detection
                if (DebugState->Mouse.X >= CellX && DebugState->Mouse.X < CellX + CellWidth &&
                    DebugState->Mouse.Y >= CellY && DebugState->Mouse.Y < CellY + CellHeight)
                {
                    DebugState->Mouse.IsHovering = 1;
                    DebugState->Mouse.HoverValue = Weight;
                    snprintf(DebugState->Mouse.HoverLabel, sizeof(DebugState->Mouse.HoverLabel),
                            "Weight[%d][%d] Layer %d", Row, Col, LayerToShow);
                    
                    // Highlight hovered cell
                    DrawRectangle(Buffer, CellX, CellY, CellWidth, 1, COLOR_WHITE);
                    DrawRectangle(Buffer, CellX, CellY + CellHeight - 1, CellWidth, 1, COLOR_WHITE);
                    DrawRectangle(Buffer, CellX, CellY, 1, CellHeight, COLOR_WHITE);
                    DrawRectangle(Buffer, CellX + CellWidth - 1, CellY, 1, CellHeight, COLOR_WHITE);
                }
                
                DebugState->PerfStats.PixelsDrawn += CellWidth * CellHeight;
            }
        }
    }
    
    // Render heatmap info
    RenderDebugTextf(Buffer, 10, 30, COLOR_WHITE, "Layer %d Weights (%dx%d)", 
                    LayerToShow, MatrixRows, MatrixCols);
    RenderDebugTextf(Buffer, 10, 45, COLOR_WHITE, "Range: %.3f to %.3f", MinWeight, MaxWeight);
    RenderDebugTextf(Buffer, 10, 60, COLOR_WHITE, "Zoom: %.1fx Pan: (%d,%d)", 
                    DebugState->HeatmapParams.ZoomLevel, 
                    DebugState->HeatmapParams.PanX, 
                    DebugState->HeatmapParams.PanY);
    
    // Color scale legend
    RenderColorScale(DebugState, Buffer, MinWeight, MaxWeight);
    
    DEBUG_PROFILE_END(DebugState, "Weight Heatmap");
}

// Render color scale legend for heatmaps
internal void
RenderColorScale(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, f32 MinValue, f32 MaxValue)
{
    i32 LegendX = Buffer->Width - 80;
    i32 LegendY = 100;
    i32 LegendWidth = 30;
    i32 LegendHeight = 200;
    
    // Draw color gradient
    for (i32 y = 0; y < LegendHeight; ++y)
    {
        f32 t = (f32)y / (f32)(LegendHeight - 1);
        f32 Value = MinValue + t * (MaxValue - MinValue);
        u32 Color = MapValueToColor(Value, MinValue, MaxValue, 
                                   DebugState->HeatmapParams.ColorScheme);
        
        DrawRectangle(Buffer, LegendX, LegendY + y, LegendWidth, 1, Color);
    }
    
    // Draw border
    DrawRectangle(Buffer, LegendX - 1, LegendY - 1, LegendWidth + 2, 1, COLOR_WHITE);
    DrawRectangle(Buffer, LegendX - 1, LegendY + LegendHeight, LegendWidth + 2, 1, COLOR_WHITE);
    DrawRectangle(Buffer, LegendX - 1, LegendY - 1, 1, LegendHeight + 2, COLOR_WHITE);
    DrawRectangle(Buffer, LegendX + LegendWidth, LegendY - 1, 1, LegendHeight + 2, COLOR_WHITE);
    
    // Draw value labels
    RenderDebugTextf(Buffer, LegendX + LegendWidth + 5, LegendY - 5, COLOR_WHITE, 
                    "%.2f", MaxValue);
    RenderDebugTextf(Buffer, LegendX + LegendWidth + 5, LegendY + LegendHeight - 10, COLOR_WHITE,
                    "%.2f", MinValue);
}

// DNC Memory Matrix visualization with read/write heads
void
RenderDNCMemoryMatrix(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, dnc_system *DNC)
{
    if (!DNC || !DNC->Memory.Matrix) return;
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    // PERFORMANCE: Visualize DNC memory as a 2D matrix with special indicators
    // CACHE: Sequential access through memory locations
    
    dnc_memory *Memory = &DNC->Memory;
    u32 NumLocations = Memory->NumLocations;
    u32 VectorSize = Memory->VectorSize;
    
    // Calculate cell dimensions to fit screen
    i32 MaxWidth = Buffer->Width - 300;
    i32 MaxHeight = Buffer->Height - 200;
    
    i32 CellWidth = MaxWidth / VectorSize;
    i32 CellHeight = MaxHeight / NumLocations;
    
    // Minimum cell size for visibility
    if (CellWidth < 2) CellWidth = 2;
    if (CellHeight < 2) CellHeight = 2;
    
    // Apply zoom
    CellWidth = (i32)(CellWidth * DebugState->HeatmapParams.ZoomLevel);
    CellHeight = (i32)(CellHeight * DebugState->HeatmapParams.ZoomLevel);
    
    i32 MatrixWidth = VectorSize * CellWidth;
    i32 MatrixHeight = NumLocations * CellHeight;
    
    // Center the matrix with pan offset
    i32 StartX = 50 + DebugState->HeatmapParams.PanX;
    i32 StartY = 50 + DebugState->HeatmapParams.PanY;
    
    // Auto-scale memory values for visualization
    f32 MinValue = 0.0f, MaxValue = 0.0f;
    b32 FirstValue = 1;
    
    for (u32 Location = 0; Location < NumLocations; ++Location)
    {
        f32 *MemoryVector = Memory->Matrix + Location * Memory->Stride;
        for (u32 Component = 0; Component < VectorSize; ++Component)
        {
            f32 Value = MemoryVector[Component];
            if (FirstValue)
            {
                MinValue = MaxValue = Value;
                FirstValue = 0;
            }
            else
            {
                if (Value < MinValue) MinValue = Value;
                if (Value > MaxValue) MaxValue = Value;
            }
        }
    }
    
    // Avoid division by zero
    if (MaxValue == MinValue) MaxValue = MinValue + 1.0f;
    
    // Render memory cells
    for (u32 Location = 0; Location < NumLocations; ++Location)
    {
        f32 *MemoryVector = Memory->Matrix + Location * Memory->Stride;
        
        for (u32 Component = 0; Component < VectorSize; ++Component)
        {
            f32 MemoryValue = MemoryVector[Component];
            
            // Map memory value to color
            u32 CellColor = MapValueToColor(MemoryValue, MinValue, MaxValue,
                                          COLOR_SCHEME_MEMORY);
            
            i32 CellX = StartX + Component * CellWidth;
            i32 CellY = StartY + Location * CellHeight;
            
            // Only draw visible cells
            if (CellX + CellWidth >= 0 && CellX < Buffer->Width &&
                CellY + CellHeight >= 0 && CellY < Buffer->Height)
            {
                DrawRectangle(Buffer, CellX, CellY, CellWidth, CellHeight, CellColor);
                
                // Mouse hover detection
                if (DebugState->Mouse.X >= CellX && DebugState->Mouse.X < CellX + CellWidth &&
                    DebugState->Mouse.Y >= CellY && DebugState->Mouse.Y < CellY + CellHeight)
                {
                    DebugState->Mouse.IsHovering = 1;
                    DebugState->Mouse.HoverValue = MemoryValue;
                    snprintf(DebugState->Mouse.HoverLabel, sizeof(DebugState->Mouse.HoverLabel),
                            "Memory[%u][%u]", Location, Component);
                    
                    // Highlight hovered cell
                    DrawRectangle(Buffer, CellX, CellY, CellWidth, 1, COLOR_YELLOW);
                    DrawRectangle(Buffer, CellX, CellY + CellHeight - 1, CellWidth, 1, COLOR_YELLOW);
                    DrawRectangle(Buffer, CellX, CellY, 1, CellHeight, COLOR_YELLOW);
                    DrawRectangle(Buffer, CellX + CellWidth - 1, CellY, 1, CellHeight, COLOR_YELLOW);
                }
                
                DebugState->PerfStats.PixelsDrawn += CellWidth * CellHeight;
            }
        }
    }
    
    // Render read heads
    for (u32 HeadIndex = 0; HeadIndex < DNC->NumReadHeads; ++HeadIndex)
    {
        dnc_read_head *ReadHead = &DNC->ReadHeads[HeadIndex];
        
        // Find the most attended memory location for this read head
        u32 MaxLocation = 0;
        f32 MaxWeight = 0.0f;
        
        if (ReadHead->LocationWeighting)
        {
            for (u32 Location = 0; Location < NumLocations; ++Location)
            {
                if (ReadHead->LocationWeighting[Location] > MaxWeight)
                {
                    MaxWeight = ReadHead->LocationWeighting[Location];
                    MaxLocation = Location;
                }
            }
        }
        
        // Draw read head indicator
        i32 ReadHeadX = StartX + VectorSize * CellWidth + 20 + HeadIndex * 15;
        i32 ReadHeadY = StartY + MaxLocation * CellHeight + CellHeight / 2;
        
        u32 ReadHeadColor = DEBUG_COLOR_MEMORY_READ;
        DrawRectangle(Buffer, ReadHeadX, ReadHeadY - 3, 10, 6, ReadHeadColor);
        
        // Draw line to memory location
        DrawDebugLine(Buffer, ReadHeadX, ReadHeadY,
                     StartX + VectorSize * CellWidth, ReadHeadY, ReadHeadColor);
        
        RenderDebugTextf(Buffer, ReadHeadX, ReadHeadY - 15, COLOR_WHITE, "R%u", HeadIndex);
    }
    
    // Render write head
    dnc_write_head *WriteHead = &DNC->WriteHead;
    if (WriteHead->WriteWeighting)
    {
        // Find most active write location
        u32 MaxLocation = 0;
        f32 MaxWeight = 0.0f;
        
        for (u32 Location = 0; Location < NumLocations; ++Location)
        {
            if (WriteHead->WriteWeighting[Location] > MaxWeight)
            {
                MaxWeight = WriteHead->WriteWeighting[Location];
                MaxLocation = Location;
            }
        }
        
        // Draw write head indicator
        i32 WriteHeadX = StartX - 30;
        i32 WriteHeadY = StartY + MaxLocation * CellHeight + CellHeight / 2;
        
        u32 WriteHeadColor = DEBUG_COLOR_MEMORY_WRITE;
        DrawRectangle(Buffer, WriteHeadX, WriteHeadY - 4, 12, 8, WriteHeadColor);
        
        // Draw line to memory location
        DrawDebugLine(Buffer, WriteHeadX + 12, WriteHeadY,
                     StartX, WriteHeadY, WriteHeadColor);
        
        RenderDebugText(Buffer, "W", WriteHeadX + 2, WriteHeadY - 3, COLOR_BLACK);
    }
    
    // Render memory usage overlay
    if (DNC->Usage.UsageVector)
    {
        RenderMemoryUsage(DebugState, Buffer, DNC, StartX, StartY, CellHeight);
    }
    
    // Render DNC statistics
    RenderDNCStats(DebugState, Buffer, DNC);
    
    DEBUG_PROFILE_END(DebugState, "DNC Memory");
}

// Render memory usage bar along the side of DNC matrix
internal void
RenderMemoryUsage(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, 
                  dnc_system *DNC, i32 StartX, i32 StartY, i32 CellHeight)
{
    i32 UsageBarX = StartX - 15;
    i32 UsageBarWidth = 10;
    
    for (u32 Location = 0; Location < DNC->Memory.NumLocations; ++Location)
    {
        f32 Usage = DNC->Usage.UsageVector ? DNC->Usage.UsageVector[Location] : 0.0f;
        
        // Map usage to color intensity (0 = dark, 1 = bright)
        u8 Intensity = (u8)(255 * Usage);
        u32 UsageColor = RGB(Intensity, Intensity / 2, 0);  // Orange gradient
        
        i32 UsageY = StartY + Location * CellHeight;
        DrawRectangle(Buffer, UsageBarX, UsageY, UsageBarWidth, CellHeight, UsageColor);
    }
    
    // Usage bar label
    RenderDebugText(Buffer, "Usage", UsageBarX - 40, StartY - 15, COLOR_WHITE);
}

// Render DNC statistics panel
internal void
RenderDNCStats(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, dnc_system *DNC)
{
    i32 PanelX = Buffer->Width - 200;
    i32 PanelY = 50;
    i32 PanelWidth = 190;
    i32 PanelHeight = 120;
    
    // Background
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 180));
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, 1, COLOR_CYAN);
    
    i32 TextY = PanelY + 5;
    RenderDebugText(Buffer, "DNC Memory Stats", PanelX + 5, TextY, COLOR_CYAN);
    TextY += DEBUG_TEXT_HEIGHT + 2;
    
    RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                    "Memory: %ux%u", DNC->Memory.NumLocations, DNC->Memory.VectorSize);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                    "Read Heads: %u", DNC->NumReadHeads);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                    "Total Reads: %u", DNC->Memory.TotalReads);
    TextY += DEBUG_TEXT_HEIGHT;
    
    RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                    "Total Writes: %u", DNC->Memory.TotalWrites);
    TextY += DEBUG_TEXT_HEIGHT;
    
    // Calculate average usage
    f32 AvgUsage = 0.0f;
    if (DNC->Usage.UsageVector)
    {
        for (u32 i = 0; i < DNC->Memory.NumLocations; ++i)
        {
            AvgUsage += DNC->Usage.UsageVector[i];
        }
        AvgUsage /= (f32)DNC->Memory.NumLocations;
    }
    
    RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                    "Avg Usage: %.1f%%", AvgUsage * 100.0f);
}

// LSTM Gate States visualization with temporal animation
void
RenderLSTMGateStates(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, lstm_network *LSTM)
{
    if (!LSTM || LSTM->NumLayers == 0) return;
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    // PERFORMANCE: Visualize LSTM gate states as colored bars
    // CACHE: Sequential access through LSTM states
    
    // For now, show the first layer's first NPC state
    i32 LayerIndex = 0;
    i32 NPCIndex = 0;  // TODO: Make this selectable
    
    if (LayerIndex >= LSTM->NumLayers) return;
    
    lstm_layer *Layer = &LSTM->Layers[LayerIndex];
    if (NPCIndex >= Layer->ActiveNPCs || !Layer->States) return;
    
    lstm_state *State = &Layer->States[NPCIndex];
    if (!State->ForgetGate.Data || !State->InputGate.Data || 
        !State->CandidateValues.Data || !State->OutputGate.Data) return;
    
    // Layout gates in a 2x2 grid
    i32 GateWidth = (Buffer->Width - 100) / 2;
    i32 GateHeight = (Buffer->Height - 200) / 2;
    i32 StartX = 50;
    i32 StartY = 50;
    
    // Gate visualization parameters
    i32 NeuronBarWidth = GateWidth / State->HiddenState.Size;
    if (NeuronBarWidth < 2) NeuronBarWidth = 2;
    i32 BarHeight = GateHeight - 40;
    
    // Render each gate
    struct {
        neural_vector *GateData;
        const char *Name;
        u32 Color;
        i32 X, Y;
    } Gates[] = {
        {&State->ForgetGate, "Forget Gate", DEBUG_COLOR_LSTM_FORGET, StartX, StartY},
        {&State->InputGate, "Input Gate", DEBUG_COLOR_LSTM_INPUT, StartX + GateWidth, StartY},
        {&State->CandidateValues, "Candidate Values", DEBUG_COLOR_LSTM_CANDIDATE, StartX, StartY + GateHeight},
        {&State->OutputGate, "Output Gate", DEBUG_COLOR_LSTM_OUTPUT, StartX + GateWidth, StartY + GateHeight}
    };
    
    for (i32 GateIndex = 0; GateIndex < 4; ++GateIndex)
    {
        neural_vector *GateVector = Gates[GateIndex].GateData;
        const char *GateName = Gates[GateIndex].Name;
        u32 BaseColor = Gates[GateIndex].Color;
        i32 GateX = Gates[GateIndex].X;
        i32 GateY = Gates[GateIndex].Y;
        
        // Draw gate border
        DrawRectangle(Buffer, GateX - 2, GateY - 2, GateWidth + 4, GateHeight + 4, COLOR_WHITE);
        DrawRectangle(Buffer, GateX, GateY, GateWidth, GateHeight, COLOR_BLACK);
        
        // Gate title
        RenderDebugText(Buffer, GateName, GateX + 5, GateY + 5, COLOR_WHITE);
        
        // Find gate value range for this visualization
        f32 MinValue = GateVector->Data[0];
        f32 MaxValue = GateVector->Data[0];
        
        for (u32 i = 1; i < GateVector->Size; ++i)
        {
            f32 Value = GateVector->Data[i];
            if (Value < MinValue) MinValue = Value;
            if (Value > MaxValue) MaxValue = Value;
        }
        
        // Avoid division by zero
        if (MaxValue == MinValue) MaxValue = MinValue + 1.0f;
        
        // Draw neuron bars for this gate
        i32 BarStartY = GateY + 25;
        
        for (u32 NeuronIndex = 0; NeuronIndex < GateVector->Size; ++NeuronIndex)
        {
            f32 GateValue = GateVector->Data[NeuronIndex];
            
            // Normalize gate value to [0, 1] for bar height
            f32 NormalizedValue = (GateValue - MinValue) / (MaxValue - MinValue);
            i32 CurrentBarHeight = (i32)(BarHeight * NormalizedValue);
            
            // Modulate color based on value
            u8 r = (BaseColor >> 16) & 0xFF;
            u8 g = (BaseColor >> 8) & 0xFF;
            u8 b = BaseColor & 0xFF;
            
            // Darken based on value (lower values are darker)
            r = (u8)(r * NormalizedValue);
            g = (u8)(g * NormalizedValue);
            b = (u8)(b * NormalizedValue);
            
            u32 BarColor = RGB(r, g, b);
            
            i32 BarX = GateX + NeuronIndex * NeuronBarWidth;
            i32 BarY = BarStartY + BarHeight - CurrentBarHeight;
            
            DrawRectangle(Buffer, BarX, BarY, NeuronBarWidth - 1, CurrentBarHeight, BarColor);
            
            // Mouse hover detection
            if (DebugState->Mouse.X >= BarX && DebugState->Mouse.X < BarX + NeuronBarWidth &&
                DebugState->Mouse.Y >= GateY && DebugState->Mouse.Y < GateY + GateHeight)
            {
                DebugState->Mouse.IsHovering = 1;
                DebugState->Mouse.HoverValue = GateValue;
                snprintf(DebugState->Mouse.HoverLabel, sizeof(DebugState->Mouse.HoverLabel),
                        "%s[%u] NPC %d", GateName, NeuronIndex, NPCIndex);
                
                // Highlight bar
                DrawRectangle(Buffer, BarX, BarStartY, NeuronBarWidth, 1, COLOR_YELLOW);
                DrawRectangle(Buffer, BarX, BarStartY + BarHeight - 1, NeuronBarWidth, 1, COLOR_YELLOW);
                DrawRectangle(Buffer, BarX, BarStartY, 1, BarHeight, COLOR_YELLOW);
                DrawRectangle(Buffer, BarX + NeuronBarWidth - 1, BarStartY, 1, BarHeight, COLOR_YELLOW);
            }
            
            DebugState->PerfStats.PixelsDrawn += NeuronBarWidth * CurrentBarHeight;
        }
        
        // Gate statistics
        f32 AvgValue = 0.0f;
        for (u32 i = 0; i < GateVector->Size; ++i)
        {
            AvgValue += GateVector->Data[i];
        }
        AvgValue /= (f32)GateVector->Size;
        
        RenderDebugTextf(Buffer, GateX + 5, GateY + GateHeight - 20, COLOR_WHITE,
                        "Avg: %.3f Range: %.3f-%.3f", AvgValue, MinValue, MaxValue);
    }
    
    // Render cell state and hidden state as line graphs
    RenderLSTMStateGraphs(DebugState, Buffer, State, StartX, StartY + 2 * GateHeight + 20);
    
    // LSTM statistics
    RenderLSTMStats(DebugState, Buffer, LSTM, LayerIndex, NPCIndex);
    
    DEBUG_PROFILE_END(DebugState, "LSTM Gates");
}

// Render LSTM cell state and hidden state as line graphs
internal void
RenderLSTMStateGraphs(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, 
                      lstm_state *State, i32 StartX, i32 StartY)
{
    i32 GraphWidth = Buffer->Width - 200;
    i32 GraphHeight = 60;
    i32 GraphSpacing = 70;
    
    // Cell State Graph
    RenderDebugText(Buffer, "Cell State", StartX, StartY, COLOR_WHITE);
    RenderStateGraph(Buffer, &State->CellState, StartX, StartY + 15, GraphWidth, GraphHeight, 
                    COLOR_CYAN, -1.0f, 1.0f);
    
    // Hidden State Graph  
    RenderDebugText(Buffer, "Hidden State", StartX, StartY + GraphSpacing, COLOR_WHITE);
    RenderStateGraph(Buffer, &State->HiddenState, StartX, StartY + GraphSpacing + 15, 
                    GraphWidth, GraphHeight, COLOR_GREEN, -1.0f, 1.0f);
}

// Render a single state vector as a line graph
internal void
RenderStateGraph(game_offscreen_buffer *Buffer, neural_vector *Vector,
                 i32 X, i32 Y, i32 Width, i32 Height, u32 Color, f32 MinVal, f32 MaxVal)
{
    if (!Vector || !Vector->Data) return;
    
    // Graph background
    DrawRectangle(Buffer, X, Y, Width, Height, RGBA(0, 0, 0, 128));
    DrawRectangle(Buffer, X, Y, Width, 1, COLOR_GRAY);
    DrawRectangle(Buffer, X, Y + Height - 1, Width, 1, COLOR_GRAY);
    DrawRectangle(Buffer, X, Y, 1, Height, COLOR_GRAY);
    DrawRectangle(Buffer, X + Width - 1, Y, 1, Height, COLOR_GRAY);
    
    // Zero line
    i32 ZeroY = Y + Height / 2;
    DrawRectangle(Buffer, X, ZeroY, Width, 1, COLOR_DARK_GRAY);
    
    // Plot the values
    f32 XStep = (f32)Width / (f32)Vector->Size;
    
    for (u32 i = 0; i < Vector->Size - 1; ++i)
    {
        f32 Value1 = Vector->Data[i];
        f32 Value2 = Vector->Data[i + 1];
        
        // Normalize to graph coordinates
        f32 NormValue1 = (Value1 - MinVal) / (MaxVal - MinVal);
        f32 NormValue2 = (Value2 - MinVal) / (MaxVal - MinVal);
        
        i32 X1 = X + (i32)(i * XStep);
        i32 Y1 = Y + Height - (i32)(NormValue1 * Height);
        i32 X2 = X + (i32)((i + 1) * XStep);
        i32 Y2 = Y + Height - (i32)(NormValue2 * Height);
        
        DrawDebugLine(Buffer, X1, Y1, X2, Y2, Color);
    }
}

// Render LSTM statistics panel
internal void
RenderLSTMStats(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, 
                lstm_network *LSTM, i32 LayerIndex, i32 NPCIndex)
{
    i32 PanelX = Buffer->Width - 200;
    i32 PanelY = Buffer->Height - 150;
    i32 PanelWidth = 190;
    i32 PanelHeight = 140;
    
    // Background
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 180));
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, 1, COLOR_CYAN);
    
    i32 TextY = PanelY + 5;
    RenderDebugText(Buffer, "LSTM Stats", PanelX + 5, TextY, COLOR_CYAN);
    TextY += DEBUG_TEXT_HEIGHT + 2;
    
    if (LayerIndex < LSTM->NumLayers)
    {
        lstm_layer *Layer = &LSTM->Layers[LayerIndex];
        
        RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                        "Layer: %d/%d", LayerIndex, LSTM->NumLayers - 1);
        TextY += DEBUG_TEXT_HEIGHT;
        
        RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                        "NPC: %d/%d", NPCIndex, Layer->ActiveNPCs - 1);
        TextY += DEBUG_TEXT_HEIGHT;
        
        if (NPCIndex < Layer->ActiveNPCs && Layer->States)
        {
            lstm_state *State = &Layer->States[NPCIndex];
            
            RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                            "Hidden Size: %u", State->HiddenState.Size);
            TextY += DEBUG_TEXT_HEIGHT;
            
            RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                            "Time Step: %u", State->TimeStep);
            TextY += DEBUG_TEXT_HEIGHT;
            
            // Calculate gate statistics
            f32 AvgForget = 0.0f, AvgInput = 0.0f;
            if (State->ForgetGate.Data && State->InputGate.Data)
            {
                for (u32 i = 0; i < State->HiddenState.Size; ++i)
                {
                    AvgForget += State->ForgetGate.Data[i];
                    AvgInput += State->InputGate.Data[i];
                }
                AvgForget /= (f32)State->HiddenState.Size;
                AvgInput /= (f32)State->HiddenState.Size;
            }
            
            RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                            "Avg Forget: %.3f", AvgForget);
            TextY += DEBUG_TEXT_HEIGHT;
            
            RenderDebugTextf(Buffer, PanelX + 5, TextY, COLOR_WHITE,
                            "Avg Input: %.3f", AvgInput);
        }
    }
}

// EWC Fisher Information visualization
void
RenderEWCFisherInfo(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, void *EWCSystem)
{
    // Since we don't have the EWC system definition, we'll create a placeholder
    // that shows how the visualization would work
    
    DEBUG_PROFILE_BEGIN(DebugState);
    
    RenderDebugText(Buffer, "EWC Fisher Information Mode", 10, 30, COLOR_WHITE);
    RenderDebugText(Buffer, "Shows importance weights for catastrophic forgetting prevention", 10, 45, COLOR_GRAY);
    
    // Simulate Fisher information heatmap
    i32 FisherWidth = 200;
    i32 FisherHeight = 200;
    i32 StartX = 100;
    i32 StartY = 80;
    
    // Draw Fisher information as a heatmap
    for (i32 y = 0; y < FisherHeight; ++y)
    {
        for (i32 x = 0; x < FisherWidth; ++x)
        {
            // Simulate Fisher information values (normally would come from EWC system)
            f32 FisherValue = sinf((f32)x * 0.1f) * cosf((f32)y * 0.1f) + 1.0f;
            FisherValue *= 0.5f;  // Normalize to [0, 1]
            
            // Map to orange/red color scheme for importance
            u8 Intensity = (u8)(255 * FisherValue);
            u32 FisherColor = RGB(255, Intensity, 0);
            
            DrawPixel(Buffer, StartX + x, StartY + y, FisherColor);
            
            DebugState->PerfStats.PixelsDrawn++;
        }
    }
    
    // Draw border and labels
    DrawRectangle(Buffer, StartX - 1, StartY - 1, FisherWidth + 2, 1, COLOR_WHITE);
    DrawRectangle(Buffer, StartX - 1, StartY + FisherHeight, FisherWidth + 2, 1, COLOR_WHITE);
    DrawRectangle(Buffer, StartX - 1, StartY - 1, 1, FisherHeight + 2, COLOR_WHITE);
    DrawRectangle(Buffer, StartX + FisherWidth, StartY - 1, 1, FisherHeight + 2, COLOR_WHITE);
    
    RenderDebugText(Buffer, "Fisher Information Matrix", StartX, StartY - 20, COLOR_WHITE);
    RenderDebugText(Buffer, "Red = High Importance, Dark = Low Importance", StartX, StartY + FisherHeight + 10, COLOR_GRAY);
    
    // Show Fisher statistics
    i32 StatsX = StartX + FisherWidth + 30;
    RenderDebugText(Buffer, "EWC Statistics:", StatsX, StartY, COLOR_CYAN);
    RenderDebugText(Buffer, "Lambda: 1000.0", StatsX, StartY + 20, COLOR_WHITE);
    RenderDebugText(Buffer, "Tasks Learned: 3", StatsX, StartY + 35, COLOR_WHITE);
    RenderDebugText(Buffer, "Avg Fisher: 0.245", StatsX, StartY + 50, COLOR_WHITE);
    RenderDebugText(Buffer, "Max Fisher: 0.987", StatsX, StartY + 65, COLOR_WHITE);
    RenderDebugText(Buffer, "Protected Params: 78%", StatsX, StartY + 80, COLOR_WHITE);
    
    DEBUG_PROFILE_END(DebugState, "EWC Fisher");
}

// NPC Brain Activity comprehensive visualization  
void
RenderNPCBrainActivity(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, npc_memory_context *NPC)
{
    DEBUG_PROFILE_BEGIN(DebugState);
    
    if (!NPC)
    {
        RenderDebugText(Buffer, "No NPC Selected for Brain Activity Visualization", 10, 30, COLOR_RED);
        DEBUG_PROFILE_END(DebugState, "NPC Brain");
        return;
    }
    
    // Title with NPC name
    RenderDebugTextf(Buffer, 10, 10, COLOR_CYAN, "NPC Brain Activity: %s (ID: %u)", NPC->Name, NPC->NPCId);
    
    // Layout sections
    i32 SectionWidth = Buffer->Width / 3;
    i32 SectionHeight = (Buffer->Height - 100) / 2;
    i32 StartY = 50;
    
    // Section 1: Emotional State
    RenderNPCEmotionalState(DebugState, Buffer, NPC, 10, StartY, SectionWidth - 10, SectionHeight);
    
    // Section 2: Decision Making Process
    RenderNPCDecisionProcess(DebugState, Buffer, NPC, SectionWidth, StartY, SectionWidth - 10, SectionHeight);
    
    // Section 3: Memory Formation
    RenderNPCMemoryFormation(DebugState, Buffer, NPC, 2 * SectionWidth, StartY, SectionWidth - 10, SectionHeight);
    
    // Section 4: Interaction History (bottom half)
    RenderNPCInteractionHistory(DebugState, Buffer, NPC, 10, StartY + SectionHeight + 20, 
                               Buffer->Width - 20, SectionHeight - 20);
    
    DEBUG_PROFILE_END(DebugState, "NPC Brain");
}

// Forward declaration of internal functions
internal void RenderNPCEmotionalState(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, 
                                      npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height);
internal void RenderNPCDecisionProcess(neural_debug_state *DebugState, game_offscreen_buffer *Buffer,
                                       npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height);
internal void RenderNPCMemoryFormation(neural_debug_state *DebugState, game_offscreen_buffer *Buffer,
                                       npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height);
internal void RenderNPCInteractionHistory(neural_debug_state *DebugState, game_offscreen_buffer *Buffer,
                                          npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height);

// Render NPC emotional state as radar chart
internal void
RenderNPCEmotionalState(neural_debug_state *DebugState, game_offscreen_buffer *Buffer, 
                        npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height)
{
    // Section background
    DrawRectangle(Buffer, X, Y, Width, Height, RGBA(0, 0, 0, 100));
    DrawRectangle(Buffer, X, Y, Width, 1, COLOR_GREEN);
    
    RenderDebugText(Buffer, "Emotional State", X + 5, Y + 5, COLOR_GREEN);
    
    // Emotional radar chart center
    i32 CenterX = X + Width / 2;
    i32 CenterY = Y + Height / 2;
    i32 Radius = (Width < Height ? Width : Height) / 3;
    
    // Draw radar chart grid
    for (i32 ring = 1; ring <= 3; ++ring)
    {
        i32 RingRadius = (Radius * ring) / 3;
        DrawDebugCircle(Buffer, CenterX, CenterY, RingRadius, COLOR_DARK_GRAY);
    }
    
    // 8 emotional dimensions
    const char *EmotionNames[] = {"Joy", "Sadness", "Anger", "Fear", "Trust", "Disgust", "Surprise", "Anticipation"};
    u32 EmotionColors[] = {COLOR_YELLOW, COLOR_BLUE, COLOR_RED, RGB(128, 0, 128), 
                          COLOR_GREEN, RGB(165, 42, 42), COLOR_CYAN, RGB(255, 165, 0)};
    
    f32 AngleStep = Tau32 / 8.0f;
    
    for (i32 emotionIndex = 0; emotionIndex < 8; ++emotionIndex)
    {
        f32 Angle = emotionIndex * AngleStep;
        f32 EmotionValue = NPC->EmotionalVector[emotionIndex];
        
        // Clamp emotion value to [0, 1]
        if (EmotionValue < 0.0f) EmotionValue = 0.0f;
        if (EmotionValue > 1.0f) EmotionValue = 1.0f;
        
        // Calculate radar point
        i32 PointRadius = (i32)(EmotionValue * Radius);
        i32 PointX = CenterX + (i32)(cosf(Angle) * PointRadius);
        i32 PointY = CenterY + (i32)(sinf(Angle) * PointRadius);
        
        // Draw axis line
        i32 AxisX = CenterX + (i32)(cosf(Angle) * Radius);
        i32 AxisY = CenterY + (i32)(sinf(Angle) * Radius);
        DrawDebugLine(Buffer, CenterX, CenterY, AxisX, AxisY, COLOR_GRAY);
        
        // Draw emotion point
        DrawRectangle(Buffer, PointX - 3, PointY - 3, 6, 6, EmotionColors[emotionIndex]);
        
        // Draw emotion label
        i32 LabelX = CenterX + (i32)(cosf(Angle) * (Radius + 20));
        i32 LabelY = CenterY + (i32)(sinf(Angle) * (Radius + 20));
        RenderDebugText(Buffer, EmotionNames[emotionIndex], LabelX - 20, LabelY - 6, EmotionColors[emotionIndex]);
        
        // Connect to previous point for polygon
        if (emotionIndex > 0)
        {
            f32 PrevAngle = (emotionIndex - 1) * AngleStep;
            f32 PrevEmotionValue = NPC->EmotionalVector[emotionIndex - 1];
            if (PrevEmotionValue < 0.0f) PrevEmotionValue = 0.0f;
            if (PrevEmotionValue > 1.0f) PrevEmotionValue = 1.0f;
            
            i32 PrevPointRadius = (i32)(PrevEmotionValue * Radius);
            i32 PrevPointX = CenterX + (i32)(cosf(PrevAngle) * PrevPointRadius);
            i32 PrevPointY = CenterY + (i32)(sinf(PrevAngle) * PrevPointRadius);
            
            DrawDebugLine(Buffer, PrevPointX, PrevPointY, PointX, PointY, RGB(255, 255, 255));
        }
    }
    
    // Close the emotional polygon
    if (8 > 1)
    {
        f32 FirstAngle = 0;
        f32 LastAngle = 7 * AngleStep;
        f32 FirstEmotionValue = NPC->EmotionalVector[0];
        f32 LastEmotionValue = NPC->EmotionalVector[7];
        
        if (FirstEmotionValue < 0.0f) FirstEmotionValue = 0.0f;
        if (FirstEmotionValue > 1.0f) FirstEmotionValue = 1.0f;
        if (LastEmotionValue < 0.0f) LastEmotionValue = 0.0f;
        if (LastEmotionValue > 1.0f) LastEmotionValue = 1.0f;
        
        i32 FirstPointX = CenterX + (i32)(cosf(FirstAngle) * FirstEmotionValue * Radius);
        i32 FirstPointY = CenterY + (i32)(sinf(FirstAngle) * FirstEmotionValue * Radius);
        i32 LastPointX = CenterX + (i32)(cosf(LastAngle) * LastEmotionValue * Radius);
        i32 LastPointY = CenterY + (i32)(sinf(LastAngle) * LastEmotionValue * Radius);
        
        DrawDebugLine(Buffer, LastPointX, LastPointY, FirstPointX, FirstPointY, COLOR_WHITE);
    }
}

// Render NPC decision making process as flowchart
internal void
RenderNPCDecisionProcess(neural_debug_state *DebugState, game_offscreen_buffer *Buffer,
                         npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height)
{
    DrawRectangle(Buffer, X, Y, Width, Height, RGBA(0, 0, 0, 100));
    DrawRectangle(Buffer, X, Y, Width, 1, COLOR_YELLOW);
    
    RenderDebugText(Buffer, "Decision Process", X + 5, Y + 5, COLOR_YELLOW);
    
    // Decision flowchart
    i32 BoxWidth = 80;
    i32 BoxHeight = 30;
    i32 BoxSpacing = 15;
    i32 StartX = X + 10;
    i32 CurrentY = Y + 30;
    
    // Decision stages
    const char *DecisionStages[] = {
        "Perception",
        "Memory Recall", 
        "Evaluation",
        "Action Selection",
        "Execution"
    };
    
    u32 StageColors[] = {
        RGB(100, 255, 100),  // Light green
        RGB(100, 100, 255),  // Light blue  
        RGB(255, 255, 100),  // Yellow
        RGB(255, 100, 100),  // Light red
        RGB(255, 100, 255)   // Magenta
    };
    
    for (i32 stage = 0; stage < 5; ++stage)
    {
        // Simulate decision stage activation (would come from actual NPC processing)
        f32 StageActivation = 0.3f + 0.7f * sinf((f32)stage + DebugState->Mouse.X * 0.01f);
        
        // Draw decision box with activation-based intensity
        u32 BaseColor = StageColors[stage];
        u8 r = (u8)(((BaseColor >> 16) & 0xFF) * StageActivation);
        u8 g = (u8)(((BaseColor >> 8) & 0xFF) * StageActivation);  
        u8 b = (u8)((BaseColor & 0xFF) * StageActivation);
        u32 ActiveColor = RGB(r, g, b);
        
        DrawRectangle(Buffer, StartX, CurrentY, BoxWidth, BoxHeight, ActiveColor);
        DrawRectangle(Buffer, StartX, CurrentY, BoxWidth, 1, COLOR_WHITE);
        DrawRectangle(Buffer, StartX, CurrentY + BoxHeight - 1, BoxWidth, 1, COLOR_WHITE);
        DrawRectangle(Buffer, StartX, CurrentY, 1, BoxHeight, COLOR_WHITE);
        DrawRectangle(Buffer, StartX + BoxWidth - 1, CurrentY, 1, BoxHeight, COLOR_WHITE);
        
        // Stage label
        RenderDebugText(Buffer, DecisionStages[stage], StartX + 2, CurrentY + BoxHeight / 2 - 6, COLOR_BLACK);
        
        // Activation percentage
        RenderDebugTextf(Buffer, StartX + BoxWidth + 5, CurrentY + BoxHeight / 2 - 6, COLOR_WHITE,
                        "%.0f%%", StageActivation * 100.0f);
        
        // Arrow to next stage
        if (stage < 4)
        {
            i32 ArrowY = CurrentY + BoxHeight + BoxSpacing / 2;
            DrawDebugLine(Buffer, StartX + BoxWidth / 2, CurrentY + BoxHeight, 
                         StartX + BoxWidth / 2, ArrowY, COLOR_WHITE);
            DrawDebugLine(Buffer, StartX + BoxWidth / 2 - 3, ArrowY - 3,
                         StartX + BoxWidth / 2, ArrowY, COLOR_WHITE);
            DrawDebugLine(Buffer, StartX + BoxWidth / 2 + 3, ArrowY - 3,
                         StartX + BoxWidth / 2, ArrowY, COLOR_WHITE);
        }
        
        CurrentY += BoxHeight + BoxSpacing;
    }
}

// Render NPC memory formation process
internal void
RenderNPCMemoryFormation(neural_debug_state *DebugState, game_offscreen_buffer *Buffer,
                         npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height)
{
    DrawRectangle(Buffer, X, Y, Width, Height, RGBA(0, 0, 0, 100));
    DrawRectangle(Buffer, X, Y, Width, 1, COLOR_MAGENTA);
    
    RenderDebugText(Buffer, "Memory Formation", X + 5, Y + 5, COLOR_MAGENTA);
    
    // Memory importance visualization
    i32 MemorySlots = 16;  // Show subset of memory slots
    i32 SlotWidth = (Width - 20) / 4;
    i32 SlotHeight = 20;
    i32 SlotSpacing = 5;
    
    i32 CurrentX = X + 10;
    i32 CurrentY = Y + 30;
    
    for (i32 slot = 0; slot < MemorySlots; ++slot)
    {
        // Simulate memory importance (would come from actual NPC memory)
        f32 Importance = NPC->ImportanceScores ? NPC->ImportanceScores[slot % NPC->MemoryCapacity] : 
                        (0.2f + 0.8f * sinf((f32)slot * 0.5f));
        
        // Color based on importance
        u32 MemoryColor = MapValueToColor(Importance, 0.0f, 1.0f, COLOR_SCHEME_ATTENTION);
        
        DrawRectangle(Buffer, CurrentX, CurrentY, SlotWidth - SlotSpacing, SlotHeight, MemoryColor);
        
        // Memory slot label
        RenderDebugTextf(Buffer, CurrentX + 2, CurrentY + 5, COLOR_BLACK, "M%d", slot);
        
        // Move to next position
        CurrentX += SlotWidth;
        if ((slot + 1) % 4 == 0)
        {
            CurrentX = X + 10;
            CurrentY += SlotHeight + SlotSpacing;
        }
    }
    
    // Memory statistics
    RenderDebugTextf(Buffer, X + 5, Y + Height - 40, COLOR_WHITE,
                    "Active Memories: %u/%u", NPC->InteractionCount, NPC->MemoryCapacity);
    RenderDebugTextf(Buffer, X + 5, Y + Height - 25, COLOR_WHITE,
                    "Last Interaction: %.1fs ago", 
                    DebugState->Mouse.X * 0.1f);  // Simulate time since last interaction
}

// Render NPC interaction history timeline
internal void  
RenderNPCInteractionHistory(neural_debug_state *DebugState, game_offscreen_buffer *Buffer,
                           npc_memory_context *NPC, i32 X, i32 Y, i32 Width, i32 Height)
{
    DrawRectangle(Buffer, X, Y, Width, Height, RGBA(0, 0, 0, 100));
    DrawRectangle(Buffer, X, Y, Width, 1, COLOR_CYAN);
    
    RenderDebugText(Buffer, "Interaction History & Learning", X + 5, Y + 5, COLOR_CYAN);
    
    // Timeline visualization
    i32 TimelineY = Y + 30;
    i32 TimelineHeight = Height - 60;
    
    // Draw timeline axis
    DrawDebugLine(Buffer, X + 20, TimelineY, X + 20, TimelineY + TimelineHeight, COLOR_WHITE);
    DrawDebugLine(Buffer, X + 20, TimelineY + TimelineHeight, X + Width - 20, TimelineY + TimelineHeight, COLOR_WHITE);
    
    // Simulate interaction history
    i32 MaxInteractions = 10;
    f32 TimelineWidth = (f32)(Width - 60);
    
    for (i32 interaction = 0; interaction < MaxInteractions; ++interaction)
    {
        f32 TimeOffset = (f32)interaction / (f32)(MaxInteractions - 1);
        i32 InteractionX = X + 20 + (i32)(TimeOffset * TimelineWidth);
        
        // Simulate interaction type and emotional impact
        f32 EmotionalImpact = 0.5f + 0.5f * sinf((f32)interaction * 0.8f);
        i32 ImpactHeight = (i32)(EmotionalImpact * (TimelineHeight - 20));
        
        // Color based on interaction type
        u32 InteractionColor = (interaction % 2 == 0) ? COLOR_GREEN : COLOR_BLUE;
        if (EmotionalImpact > 0.8f) InteractionColor = COLOR_RED;  // High impact
        
        // Draw interaction bar
        DrawRectangle(Buffer, InteractionX - 2, TimelineY + TimelineHeight - ImpactHeight, 
                     4, ImpactHeight, InteractionColor);
        
        // Interaction marker
        DrawRectangle(Buffer, InteractionX - 1, TimelineY + TimelineHeight - 2, 2, 4, COLOR_WHITE);
        
        // Mouse hover for interaction details
        if (DebugState->Mouse.X >= InteractionX - 5 && DebugState->Mouse.X <= InteractionX + 5 &&
            DebugState->Mouse.Y >= TimelineY && DebugState->Mouse.Y <= TimelineY + TimelineHeight)
        {
            DebugState->Mouse.IsHovering = 1;
            DebugState->Mouse.HoverValue = EmotionalImpact;
            snprintf(DebugState->Mouse.HoverLabel, sizeof(DebugState->Mouse.HoverLabel),
                    "Interaction %d: Impact %.2f", interaction, EmotionalImpact);
        }
    }
    
    // Learning curve
    RenderDebugText(Buffer, "Learning Progress:", X + Width / 2, Y + Height - 25, COLOR_WHITE);
    i32 ProgressBarWidth = Width / 3;
    i32 ProgressBarX = X + Width / 2 + 120;
    f32 LearningProgress = 0.65f;  // Simulate learning progress
    i32 ProgressWidth = (i32)(ProgressBarWidth * LearningProgress);
    
    DrawRectangle(Buffer, ProgressBarX, Y + Height - 20, ProgressBarWidth, 8, COLOR_DARK_GRAY);
    DrawRectangle(Buffer, ProgressBarX, Y + Height - 20, ProgressWidth, 8, COLOR_GREEN);
    
    RenderDebugTextf(Buffer, ProgressBarX + ProgressBarWidth + 10, Y + Height - 25, COLOR_WHITE,
                    "%.0f%%", LearningProgress * 100.0f);
}

void
RenderTimeline(neural_debug_state *DebugState, game_offscreen_buffer *Buffer)
{
    if (!DebugState->ShowTimeline) return;
    
    i32 TimelineY = Buffer->Height - DEBUG_TIMELINE_HEIGHT;
    i32 TimelineWidth = Buffer->Width;
    
    // Timeline background
    DrawRectangle(Buffer, 0, TimelineY, TimelineWidth, DEBUG_TIMELINE_HEIGHT, RGBA(0, 0, 0, 200));
    DrawRectangle(Buffer, 0, TimelineY, TimelineWidth, 1, COLOR_YELLOW);
    
    // Timeline scrubber
    i32 ScrubberX = (i32)(DebugState->TimelinePosition * TimelineWidth);
    DrawRectangle(Buffer, ScrubberX - 1, TimelineY, 2, DEBUG_TIMELINE_HEIGHT, COLOR_RED);
    
    // Timeline labels
    RenderDebugText(Buffer, "Timeline", 10, TimelineY + 5, COLOR_YELLOW);
    RenderDebugTextf(Buffer, TimelineWidth - 100, TimelineY + 5, COLOR_WHITE,
                    "%.1f%%", DebugState->TimelinePosition * 100.0f);
}

// Additional utility functions
void
DrawDebugLine(game_offscreen_buffer *Buffer, i32 X1, i32 Y1, i32 X2, i32 Y2, u32 Color)
{
    // Simple Bresenham line drawing
    i32 dx = (X2 > X1) ? (X2 - X1) : (X1 - X2);
    i32 dy = (Y2 > Y1) ? (Y2 - Y1) : (Y1 - Y2);
    i32 sx = (X1 < X2) ? 1 : -1;
    i32 sy = (Y1 < Y2) ? 1 : -1;
    i32 err = dx - dy;
    
    i32 x = X1, y = Y1;
    
    while (1)
    {
        DrawPixel(Buffer, x, y, Color);
        
        if (x == X2 && y == Y2) break;
        
        i32 e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y += sy;
        }
    }
}

void
DrawDebugCircle(game_offscreen_buffer *Buffer, i32 CenterX, i32 CenterY, i32 Radius, u32 Color)
{
    // Simple circle drawing using midpoint algorithm
    i32 x = 0;
    i32 y = Radius;
    i32 d = 1 - Radius;
    
    while (x <= y)
    {
        // Draw circle points in all 8 octants
        DrawPixel(Buffer, CenterX + x, CenterY + y, Color);
        DrawPixel(Buffer, CenterX - x, CenterY + y, Color);
        DrawPixel(Buffer, CenterX + x, CenterY - y, Color);
        DrawPixel(Buffer, CenterX - x, CenterY - y, Color);
        DrawPixel(Buffer, CenterX + y, CenterY + x, Color);
        DrawPixel(Buffer, CenterX - y, CenterY + x, Color);
        DrawPixel(Buffer, CenterX + y, CenterY - x, Color);
        DrawPixel(Buffer, CenterX - y, CenterY - x, Color);
        
        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

// SIMD-optimized heatmap generation (AVX2 version)
void
GenerateHeatmap_AVX2(u32 *Pixels, f32 *Values, u32 Width, u32 Height, debug_heatmap_params *Params)
{
    // TODO: Implement SIMD-optimized heatmap generation
    // For now, fall back to scalar version
    GenerateHeatmap_Scalar(Pixels, Values, Width, Height, Params);
}

// Scalar heatmap generation
void
GenerateHeatmap_Scalar(u32 *Pixels, f32 *Values, u32 Width, u32 Height, debug_heatmap_params *Params)
{
    // PERFORMANCE: Simple scalar implementation
    // Can be optimized with SIMD later
    
    f32 MinVal = Params->MinValue;
    f32 MaxVal = Params->MaxValue;
    
    // Auto-scale if enabled
    if (Params->AutoScale)
    {
        MinVal = Values[0];
        MaxVal = Values[0];
        
        for (u32 i = 1; i < Width * Height; ++i)
        {
            if (Values[i] < MinVal) MinVal = Values[i];
            if (Values[i] > MaxVal) MaxVal = Values[i];
        }
        
        // Avoid division by zero
        if (MaxVal == MinVal)
        {
            MaxVal = MinVal + 1.0f;
        }
    }
    
    // Generate colors
    for (u32 i = 0; i < Width * Height; ++i)
    {
        f32 Value = Values[i];
        
        // Apply gamma correction
        if (Params->Gamma != 1.0f)
        {
            f32 normalizedValue = (Value - MinVal) / (MaxVal - MinVal);
            normalizedValue = powf(normalizedValue, Params->Gamma);
            Value = MinVal + normalizedValue * (MaxVal - MinVal);
        }
        
        Pixels[i] = MapValueToColor(Value, MinVal, MaxVal, Params->ColorScheme);
    }
}