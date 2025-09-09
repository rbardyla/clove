#ifndef NEURAL_DEBUG_H
#define NEURAL_DEBUG_H

#include "handmade.h"
#include "memory.h"

// Forward declarations to avoid conflicts
typedef struct dnc_system dnc_system;
typedef struct lstm_network lstm_network;
typedef struct npc_memory_context npc_memory_context;

// Simple neural structures for debug compatibility
typedef struct neural_layer neural_layer;
typedef struct neural_network neural_network;

/*
    Neural Debug Visualization System
    
    Casey Muratori-style immediate mode debug system for understanding neural networks
    
    Philosophy:
    - Immediate visual feedback for neural activity
    - Zero allocations in visualization hot paths
    - Direct pixel manipulation for < 1ms overhead
    - Every neuron, weight, and memory slot visible
    - Toggle between visualization modes instantly
    
    Performance Targets:
    - < 1ms total visualization overhead per frame
    - Direct pixel writes, no GPU dependencies
    - SIMD-accelerated heatmap generation
    - Cache-friendly data access patterns
    
    Visualization Modes:
    1. Neural activations as color-coded pixels (hot/cold mapping)
    2. Weight matrices as 2D heatmaps with zoom capability
    3. DNC memory matrix with read/write head positions
    4. LSTM gate states with temporal animation
    5. EWC Fisher information importance overlay
    6. NPC decision flow with real-time updates
    
    Interactive Features:
    - Mouse hover for exact values
    - Keyboard shortcuts for mode switching
    - Zoom and pan for detailed inspection
    - Pause/resume neural inference
    - Timeline scrubbing for temporal analysis
*/

// Debug visualization modes
typedef enum debug_viz_mode
{
    DEBUG_VIZ_NONE = 0,
    DEBUG_VIZ_NEURAL_ACTIVATIONS = 1,
    DEBUG_VIZ_WEIGHT_HEATMAP = 2,
    DEBUG_VIZ_DNC_MEMORY = 3,
    DEBUG_VIZ_LSTM_GATES = 4,
    DEBUG_VIZ_EWC_FISHER = 5,
    DEBUG_VIZ_NPC_BRAIN = 6,
    DEBUG_VIZ_MEMORY_ACCESS_PATTERN = 7,
    DEBUG_VIZ_ATTENTION_WEIGHTS = 8,
    DEBUG_VIZ_TEMPORAL_LINKAGE = 9,
    
    DEBUG_VIZ_MODE_COUNT
} debug_viz_mode;

// Color schemes for different visualization types
typedef enum debug_color_scheme
{
    COLOR_SCHEME_HOT_COLD = 0,      // Blue (cold) -> Red (hot)
    COLOR_SCHEME_GRAYSCALE,         // Black -> White
    COLOR_SCHEME_RAINBOW,           // Full spectrum
    COLOR_SCHEME_ATTENTION,         // Purple -> Yellow for attention
    COLOR_SCHEME_MEMORY,            // Green -> Blue for memory
    COLOR_SCHEME_GATES              // Different colors for LSTM gates
} debug_color_scheme;

// Heatmap parameters for weight/activation visualization
typedef struct debug_heatmap_params
{
    f32 MinValue;                   // Minimum value for color mapping
    f32 MaxValue;                   // Maximum value for color mapping
    f32 Gamma;                      // Gamma correction for visualization
    b32 AutoScale;                  // Automatically adjust min/max
    debug_color_scheme ColorScheme;
    
    // Zoom and pan state
    f32 ZoomLevel;                  // 1.0 = normal, 2.0 = 2x zoom
    i32 PanX, PanY;                 // Pan offset in pixels
} debug_heatmap_params;

// Mouse interaction state
typedef struct debug_mouse_state
{
    i32 X, Y;                       // Current mouse position
    i32 LastX, LastY;               // Previous frame position
    b32 LeftDown, RightDown;        // Button states
    b32 LeftPressed, RightPressed;  // This frame button presses
    
    // Hover information
    b32 IsHovering;                 // Is mouse over a neural component?
    f32 HoverValue;                 // Value under mouse cursor
    char HoverLabel[256];           // Description of hovered element
} debug_mouse_state;

// Neural component being inspected
typedef struct debug_inspection_target
{
    enum 
    {
        INSPECT_NONE,
        INSPECT_NEURON,
        INSPECT_WEIGHT,
        INSPECT_MEMORY_SLOT,
        INSPECT_LSTM_GATE
    } Type;
    
    union
    {
        struct { u32 LayerIndex; u32 NeuronIndex; } Neuron;
        struct { u32 LayerIndex; u32 FromIndex; u32 ToIndex; } Weight;
        struct { u32 MemoryLocation; } MemorySlot;
        struct { u32 GateType; u32 CellIndex; } LSTMGate;
    };
    
    f32 CurrentValue;
    f32 MinValue, MaxValue;         // Historical min/max for this target
    f32 ValueHistory[256];          // Circular buffer of past values
    u32 HistoryIndex;
} debug_inspection_target;

// Performance tracking for debug system itself
typedef struct debug_performance_stats
{
    u64 VisualizationCycles;        // Cycles spent in visualization
    u64 HeatmapGenerationCycles;    // Cycles for heatmap generation
    u64 TextRenderingCycles;        // Cycles for text rendering
    u32 PixelsDrawn;                // Number of pixels updated
    f32 FrameTimeMs;                // Total frame time including debug
    
    // Rolling averages
    f32 AverageVisualizationTimeMs;
    f32 AveragePixelThroughput;     // Millions of pixels per second
} debug_performance_stats;

// Complete debug visualization state
typedef struct neural_debug_state
{
    // Current visualization mode
    debug_viz_mode CurrentMode;
    b32 DebugEnabled;               // Master debug toggle
    b32 ShowPerformanceStats;       // Show debug performance overlay
    
    // Mode-specific parameters
    debug_heatmap_params HeatmapParams;
    debug_mouse_state Mouse;
    debug_inspection_target InspectionTarget;
    debug_performance_stats PerfStats;
    
    // Temporal visualization state
    f32 TimelinePosition;           // 0.0 to 1.0 through recorded timeline
    b32 IsPaused;                   // Pause neural inference for inspection
    b32 ShowTimeline;               // Show timeline scrubber
    
    // Data recording for temporal visualization
    f32 **ActivationHistory;        // [timestep][neuron] circular buffer
    f32 **WeightHistory;            // [timestep][weight] for weight change vis
    u32 HistoryBufferSize;          // Number of timesteps recorded
    u32 CurrentHistoryIndex;        // Current write position in circular buffer
    
    // Layer-specific visibility toggles
    b32 LayerVisibility[32];        // Show/hide individual layers
    f32 LayerOpacity[32];           // Opacity for each layer overlay
    
    // UI interaction state  
    b32 ShowHelp;                   // Show keyboard shortcuts
    char StatusMessage[512];        // Current status/info message
    f32 StatusMessageTimeout;       // Seconds until message fades
    
    // Memory for debug system itself
    memory_arena *DebugArena;       // Persistent debug memory
    memory_pool *TempDebugPool;     // Per-frame debug allocations
} neural_debug_state;

// Initialization and cleanup
neural_debug_state *InitializeNeuralDebugSystem(memory_arena *Arena, 
                                                u32 MaxNeurons,
                                                u32 HistoryBufferSize);
void ResetNeuralDebugState(neural_debug_state *DebugState);
void ShutdownNeuralDebugSystem(neural_debug_state *DebugState);

// Main update and render functions
void UpdateNeuralDebug(neural_debug_state *DebugState, 
                      game_input *Input,
                      f32 DeltaTime);
void RenderNeuralDebug(neural_debug_state *DebugState,
                      game_offscreen_buffer *Buffer);

// Input handling
void ProcessDebugInput(neural_debug_state *DebugState, game_input *Input);
void UpdateDebugMouse(neural_debug_state *DebugState, game_input *Input);

// Visualization mode implementations
void RenderNeuralActivations(neural_debug_state *DebugState,
                           game_offscreen_buffer *Buffer,
                           neural_network *Network);
void RenderWeightHeatmap(neural_debug_state *DebugState,
                        game_offscreen_buffer *Buffer,
                        neural_network *Network);
void RenderDNCMemoryMatrix(neural_debug_state *DebugState,
                          game_offscreen_buffer *Buffer,
                          dnc_system *DNC);
void RenderLSTMGateStates(neural_debug_state *DebugState,
                         game_offscreen_buffer *Buffer,
                         lstm_network *LSTM);
void RenderEWCFisherInfo(neural_debug_state *DebugState,
                        game_offscreen_buffer *Buffer,
                        void *EWCSystem);
void RenderNPCBrainActivity(neural_debug_state *DebugState,
                           game_offscreen_buffer *Buffer,
                           npc_memory_context *NPC);

// Heatmap generation (SIMD optimized)
void GenerateHeatmap_Scalar(u32 *Pixels, f32 *Values, 
                           u32 Width, u32 Height,
                           debug_heatmap_params *Params);
void GenerateHeatmap_AVX2(u32 *Pixels, f32 *Values,
                         u32 Width, u32 Height, 
                         debug_heatmap_params *Params);

// Color utilities
u32 MapValueToColor(f32 Value, f32 MinVal, f32 MaxVal, 
                    debug_color_scheme Scheme);
u32 HSVToRGB(f32 H, f32 S, f32 V);
u32 InterpolateColors(u32 ColorA, u32 ColorB, f32 t);

// Text rendering for debug overlays (bitmap font)
void RenderDebugText(game_offscreen_buffer *Buffer,
                    const char *Text, i32 X, i32 Y, u32 Color);
void RenderDebugTextf(game_offscreen_buffer *Buffer,
                     i32 X, i32 Y, u32 Color,
                     const char *Format, ...);

// Interactive inspection
void UpdateInspectionTarget(neural_debug_state *DebugState,
                           i32 MouseX, i32 MouseY);
void RenderInspectionOverlay(neural_debug_state *DebugState,
                            game_offscreen_buffer *Buffer);
void RecordValueHistory(debug_inspection_target *Target, f32 NewValue);

// Temporal visualization
void UpdateTemporalVisualization(neural_debug_state *DebugState,
                                neural_network *Network,
                                dnc_system *DNC,
                                lstm_network *LSTM);
void RenderTimeline(neural_debug_state *DebugState,
                   game_offscreen_buffer *Buffer);
void ScrubTimeline(neural_debug_state *DebugState, f32 Position);

// Performance monitoring
void BeginDebugProfiling(neural_debug_state *DebugState);
void EndDebugProfiling(neural_debug_state *DebugState, 
                      const char *SectionName);
void UpdateDebugPerformanceStats(neural_debug_state *DebugState);
void RenderPerformanceOverlay(neural_debug_state *DebugState,
                             game_offscreen_buffer *Buffer);

// Utility functions
void DrawDebugRectangle(game_offscreen_buffer *Buffer,
                       i32 X, i32 Y, i32 Width, i32 Height,
                       u32 Color, f32 Alpha);
void DrawDebugLine(game_offscreen_buffer *Buffer,
                  i32 X1, i32 Y1, i32 X2, i32 Y2,
                  u32 Color);
void DrawDebugCircle(game_offscreen_buffer *Buffer,
                    i32 CenterX, i32 CenterY, i32 Radius,
                    u32 Color);

// Help and UI
void RenderDebugHelp(neural_debug_state *DebugState,
                    game_offscreen_buffer *Buffer);
void ShowStatusMessage(neural_debug_state *DebugState,
                      const char *Message, f32 Duration);

// Architecture-specific optimizations
#if NEURAL_USE_AVX2
    #define GenerateHeatmap GenerateHeatmap_AVX2
#else
    #define GenerateHeatmap GenerateHeatmap_Scalar
#endif

// Performance profiling macros
#define DEBUG_PROFILE_BEGIN(DebugState) \
    u64 _DebugProfileStart = ReadCPUTimer(); \
    (void)_DebugProfileStart

#define DEBUG_PROFILE_END(DebugState, SectionName) \
    do { \
        u64 _DebugProfileEnd = ReadCPUTimer(); \
        (DebugState)->PerfStats.VisualizationCycles += _DebugProfileEnd - _DebugProfileStart; \
    } while(0)

// Common debug visualization constants
#define DEBUG_TEXT_HEIGHT 12
#define DEBUG_TEXT_WIDTH 8
#define DEBUG_TIMELINE_HEIGHT 40
#define DEBUG_INSPECTION_PANEL_WIDTH 300
#define DEBUG_MAX_STATUS_MESSAGE_TIME 3.0f

// Keyboard shortcuts
#define DEBUG_KEY_TOGGLE_MAIN '`'
#define DEBUG_KEY_MODE_1 '1'
#define DEBUG_KEY_MODE_2 '2'
#define DEBUG_KEY_MODE_3 '3'
#define DEBUG_KEY_MODE_4 '4'
#define DEBUG_KEY_MODE_5 '5'
#define DEBUG_KEY_MODE_6 '6'
#define DEBUG_KEY_MODE_7 '7'
#define DEBUG_KEY_MODE_8 '8'
#define DEBUG_KEY_MODE_9 '9'
#define DEBUG_KEY_PAUSE_TOGGLE 'P'
#define DEBUG_KEY_HELP 'H'
#define DEBUG_KEY_RESET 'R'

// Default parameters
#define DEBUG_DEFAULT_ZOOM 1.0f
#define DEBUG_DEFAULT_GAMMA 1.0f
#define DEBUG_HISTORY_SIZE 1024
#define DEBUG_MAX_NEURONS 65536

// Color constants for different neural components
#define DEBUG_COLOR_ACTIVE_NEURON    RGB(255, 100, 100)
#define DEBUG_COLOR_INACTIVE_NEURON  RGB(100, 100, 255)
#define DEBUG_COLOR_POSITIVE_WEIGHT  RGB(255, 255, 100)
#define DEBUG_COLOR_NEGATIVE_WEIGHT  RGB(100, 255, 255)
#define DEBUG_COLOR_MEMORY_READ      RGB(100, 255, 100)
#define DEBUG_COLOR_MEMORY_WRITE     RGB(255, 100, 255)
#define DEBUG_COLOR_LSTM_FORGET      RGB(255, 200, 200)
#define DEBUG_COLOR_LSTM_INPUT       RGB(200, 255, 200)
#define DEBUG_COLOR_LSTM_OUTPUT      RGB(200, 200, 255)
#define DEBUG_COLOR_LSTM_CANDIDATE   RGB(255, 255, 200)

#endif // NEURAL_DEBUG_H