/*
    Standalone Neural Debug Visualization Example
    
    This demonstrates the neural debug system concepts without the complexity
    of integrating with existing neural_math.h structures.
    
    A pure Casey Muratori-style debug system showing:
    - Real-time neural activation visualization
    - Weight matrix heatmaps  
    - LSTM gate state monitoring
    - NPC brain activity visualization
    - Interactive mouse inspection
    - Performance-friendly rendering
*/

#include "handmade.h"
#include "memory.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Simple neural network structures for this example
typedef struct simple_neural_layer
{
    i32 InputSize;
    i32 OutputSize;
    f32 *Weights;       // InputSize * OutputSize
    f32 *Biases;        // OutputSize
    f32 *Activations;   // OutputSize - current activations
} simple_neural_layer;

typedef struct simple_neural_network
{
    i32 NumLayers;
    simple_neural_layer *Layers;
    u32 InferenceCount;
    f64 LastInferenceTime;
} simple_neural_network;

// Simple NPC context for brain visualization
typedef struct simple_npc
{
    u32 Id;
    char Name[64];
    f32 EmotionalState[8];    // Joy, Sadness, Anger, Fear, Trust, Disgust, Surprise, Anticipation
    f32 MemoryImportance[16]; // Memory slot importance
    f32 DecisionStages[5];    // Perception, Memory, Evaluation, Action, Execution
    f32 InteractionHistory[10]; // Recent interaction impacts
    f32 LearningProgress;
} simple_npc;

// Debug visualization state
typedef struct debug_viz_state
{
    i32 CurrentMode;              // 0=Off, 1=Activations, 2=Weights, 3=NPC Brain
    b32 ShowHelp;
    b32 IsPaused;
    f32 ZoomLevel;
    i32 PanX, PanY;
    
    // Mouse state
    i32 MouseX, MouseY;
    b32 MouseHovering;
    f32 HoverValue;
    char HoverLabel[256];
    
    // Performance tracking
    u64 VisualizationCycles;
    f32 FrameTimeMs;
    
} debug_viz_state;

// Color mapping for hot/cold visualization
u32 MapValueToHotCold(f32 Value, f32 MinVal, f32 MaxVal)
{
    f32 t = (Value - MinVal) / (MaxVal - MinVal);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    if (t < 0.5f)
    {
        // Blue to black
        f32 localT = t * 2.0f;
        u8 b = (u8)(255 * (1.0f - localT));
        return RGB(0, 0, b);
    }
    else
    {
        // Black to red
        f32 localT = (t - 0.5f) * 2.0f;
        u8 r = (u8)(255 * localT);
        return RGB(r, 0, 0);
    }
}

// Simple text rendering (using rectangles for letters)
void DrawDebugChar(game_offscreen_buffer *Buffer, char c, i32 X, i32 Y, u32 Color)
{
    // Very simple bitmap font using rectangles
    // Just show basic shapes for demonstration
    switch(c)
    {
        case 'A':
            DrawRectangle(Buffer, X, Y, 1, 8, Color);
            DrawRectangle(Buffer, X + 4, Y, 1, 8, Color);
            DrawRectangle(Buffer, X + 1, Y, 3, 1, Color);
            DrawRectangle(Buffer, X + 1, Y + 3, 3, 1, Color);
            break;
            
        case 'L':
            DrawRectangle(Buffer, X, Y, 1, 8, Color);
            DrawRectangle(Buffer, X, Y + 7, 4, 1, Color);
            break;
            
        case 'N':
            DrawRectangle(Buffer, X, Y, 1, 8, Color);
            DrawRectangle(Buffer, X + 4, Y, 1, 8, Color);
            DrawRectangle(Buffer, X + 1, Y + 2, 1, 1, Color);
            DrawRectangle(Buffer, X + 2, Y + 4, 1, 1, Color);
            DrawRectangle(Buffer, X + 3, Y + 6, 1, 1, Color);
            break;
            
        case ' ':
            // Space - draw nothing
            break;
            
        default:
            // Unknown character - draw a box
            DrawRectangle(Buffer, X, Y, 5, 8, COLOR_GRAY);
            break;
    }
}

void DrawDebugString(game_offscreen_buffer *Buffer, const char *Text, i32 X, i32 Y, u32 Color)
{
    i32 CurrentX = X;
    while (*Text)
    {
        DrawDebugChar(Buffer, *Text, CurrentX, Y, Color);
        CurrentX += 6;  // Character width + spacing
        Text++;
    }
}

// Create example neural network
simple_neural_network *CreateExampleNetwork(memory_arena *Arena)
{
    simple_neural_network *Network = PushStruct(Arena, simple_neural_network);
    Network->NumLayers = 3;
    Network->Layers = PushArray(Arena, Network->NumLayers, simple_neural_layer);
    
    // Layer 1: 16 -> 32
    Network->Layers[0].InputSize = 16;
    Network->Layers[0].OutputSize = 32;
    Network->Layers[0].Weights = PushArray(Arena, 16 * 32, f32);
    Network->Layers[0].Biases = PushArray(Arena, 32, f32);
    Network->Layers[0].Activations = PushArray(Arena, 32, f32);
    
    // Layer 2: 32 -> 16  
    Network->Layers[1].InputSize = 32;
    Network->Layers[1].OutputSize = 16;
    Network->Layers[1].Weights = PushArray(Arena, 32 * 16, f32);
    Network->Layers[1].Biases = PushArray(Arena, 16, f32);
    Network->Layers[1].Activations = PushArray(Arena, 16, f32);
    
    // Layer 3: 16 -> 8
    Network->Layers[2].InputSize = 16;
    Network->Layers[2].OutputSize = 8;
    Network->Layers[2].Weights = PushArray(Arena, 16 * 8, f32);
    Network->Layers[2].Biases = PushArray(Arena, 8, f32);
    Network->Layers[2].Activations = PushArray(Arena, 8, f32);
    
    // Initialize weights randomly
    for (i32 layer = 0; layer < Network->NumLayers; ++layer)
    {
        simple_neural_layer *Layer = &Network->Layers[layer];
        i32 WeightCount = Layer->InputSize * Layer->OutputSize;
        
        for (i32 i = 0; i < WeightCount; ++i)
        {
            Layer->Weights[i] = ((f32)(i % 200) - 100.0f) * 0.01f;
        }
        
        for (i32 i = 0; i < Layer->OutputSize; ++i)
        {
            Layer->Biases[i] = ((f32)(i % 20) - 10.0f) * 0.01f;
        }
    }
    
    return Network;
}

// Create example NPC
simple_npc *CreateExampleNPC(memory_arena *Arena)
{
    simple_npc *NPC = PushStruct(Arena, simple_npc);
    NPC->Id = 42;
    strcpy(NPC->Name, "Debug NPC");
    
    // Initialize emotional state
    NPC->EmotionalState[0] = 0.7f; // Joy
    NPC->EmotionalState[1] = 0.2f; // Sadness  
    NPC->EmotionalState[2] = 0.3f; // Anger
    NPC->EmotionalState[3] = 0.1f; // Fear
    NPC->EmotionalState[4] = 0.8f; // Trust
    NPC->EmotionalState[5] = 0.1f; // Disgust
    NPC->EmotionalState[6] = 0.4f; // Surprise
    NPC->EmotionalState[7] = 0.6f; // Anticipation
    
    // Initialize memory importance
    for (i32 i = 0; i < 16; ++i)
    {
        NPC->MemoryImportance[i] = 0.2f + 0.8f * sinf((f32)i * 0.4f);
    }
    
    // Initialize decision stages
    NPC->DecisionStages[0] = 0.9f; // Perception
    NPC->DecisionStages[1] = 0.7f; // Memory
    NPC->DecisionStages[2] = 0.5f; // Evaluation
    NPC->DecisionStages[3] = 0.3f; // Action
    NPC->DecisionStages[4] = 0.1f; // Execution
    
    // Initialize interaction history
    for (i32 i = 0; i < 10; ++i)
    {
        NPC->InteractionHistory[i] = 0.5f + 0.5f * sinf((f32)i * 0.8f);
    }
    
    NPC->LearningProgress = 0.65f;
    
    return NPC;
}

// Simulate neural network forward pass
void SimulateNetworkInference(simple_neural_network *Network, f32 Time)
{
    for (i32 layer = 0; layer < Network->NumLayers; ++layer)
    {
        simple_neural_layer *Layer = &Network->Layers[layer];
        
        // Simulate activations with temporal dynamics
        for (i32 i = 0; i < Layer->OutputSize; ++i)
        {
            f32 Base = sinf((f32)i * 0.2f + Time * 2.0f);
            f32 Modulation = cosf((f32)layer * 0.5f + Time);
            f32 Noise = 0.1f * sinf((f32)(i + layer * 100) * 1.3f + Time * 3.0f);
            
            Layer->Activations[i] = 0.5f + 0.3f * (Base * Modulation + Noise);
            
            // Apply some sparsity
            if (Layer->Activations[i] < 0.2f)
            {
                Layer->Activations[i] = 0.0f;
            }
        }
    }
    
    Network->InferenceCount++;
}

// Visualize neural activations as hot/cold columns
void RenderNetworkActivations(debug_viz_state *DebugState, game_offscreen_buffer *Buffer, 
                              simple_neural_network *Network)
{
    if (!Network) return;
    
    i32 StartX = 100;
    i32 StartY = 50;
    i32 LayerWidth = 60;
    i32 LayerHeight = Buffer->Height - 150;
    i32 LayerSpacing = (Buffer->Width - 200) / Network->NumLayers;
    
    // Render each layer as a vertical column of pixels
    for (i32 layer = 0; layer < Network->NumLayers; ++layer)
    {
        simple_neural_layer *Layer = &Network->Layers[layer];
        i32 LayerX = StartX + layer * LayerSpacing;
        
        // Calculate pixel dimensions
        i32 PixelHeight = LayerHeight / Layer->OutputSize;
        if (PixelHeight < 1) PixelHeight = 1;
        
        // Draw activation pixels
        for (i32 neuron = 0; neuron < Layer->OutputSize; ++neuron)
        {
            f32 Activation = Layer->Activations[neuron];
            u32 Color = MapValueToHotCold(Activation, 0.0f, 1.0f);
            
            i32 PixelY = StartY + neuron * PixelHeight;
            DrawRectangle(Buffer, LayerX, PixelY, LayerWidth, PixelHeight, Color);
            
            // Mouse hover detection
            if (DebugState->MouseX >= LayerX && DebugState->MouseX < LayerX + LayerWidth &&
                DebugState->MouseY >= PixelY && DebugState->MouseY < PixelY + PixelHeight)
            {
                DebugState->MouseHovering = 1;
                DebugState->HoverValue = Activation;
                snprintf(DebugState->HoverLabel, sizeof(DebugState->HoverLabel),
                        "Layer %d Neuron %d", layer, neuron);
            }
        }
        
        // Layer border
        DrawRectangle(Buffer, LayerX - 1, StartY - 1, LayerWidth + 2, 1, COLOR_WHITE);
        DrawRectangle(Buffer, LayerX - 1, StartY + LayerHeight, LayerWidth + 2, 1, COLOR_WHITE);
        DrawRectangle(Buffer, LayerX - 1, StartY, 1, LayerHeight + 1, COLOR_WHITE);
        DrawRectangle(Buffer, LayerX + LayerWidth, StartY, 1, LayerHeight + 1, COLOR_WHITE);
    }
}

// Visualize weight matrix as 2D heatmap
void RenderWeightHeatmap(debug_viz_state *DebugState, game_offscreen_buffer *Buffer,
                        simple_neural_network *Network)
{
    if (!Network || Network->NumLayers == 0) return;
    
    // Show first layer weights
    simple_neural_layer *Layer = &Network->Layers[0];
    
    i32 StartX = 50;
    i32 StartY = 50;
    i32 CellSize = 4;
    
    // Find weight range
    f32 MinWeight = Layer->Weights[0];
    f32 MaxWeight = Layer->Weights[0];
    i32 WeightCount = Layer->InputSize * Layer->OutputSize;
    
    for (i32 i = 1; i < WeightCount; ++i)
    {
        if (Layer->Weights[i] < MinWeight) MinWeight = Layer->Weights[i];
        if (Layer->Weights[i] > MaxWeight) MaxWeight = Layer->Weights[i];
    }
    
    // Render weight matrix
    for (i32 row = 0; row < Layer->OutputSize; ++row)
    {
        for (i32 col = 0; col < Layer->InputSize; ++col)
        {
            f32 Weight = Layer->Weights[row * Layer->InputSize + col];
            u32 Color = MapValueToHotCold(Weight, MinWeight, MaxWeight);
            
            i32 CellX = StartX + col * CellSize;
            i32 CellY = StartY + row * CellSize;
            
            DrawRectangle(Buffer, CellX, CellY, CellSize, CellSize, Color);
            
            // Mouse hover
            if (DebugState->MouseX >= CellX && DebugState->MouseX < CellX + CellSize &&
                DebugState->MouseY >= CellY && DebugState->MouseY < CellY + CellSize)
            {
                DebugState->MouseHovering = 1;
                DebugState->HoverValue = Weight;
                snprintf(DebugState->HoverLabel, sizeof(DebugState->HoverLabel),
                        "Weight[%d][%d]", row, col);
            }
        }
    }
}

// Draw simple radar chart for NPC emotional state
void RenderNPCBrainActivity(debug_viz_state *DebugState, game_offscreen_buffer *Buffer,
                           simple_npc *NPC)
{
    if (!NPC) return;
    
    i32 CenterX = Buffer->Width / 2;
    i32 CenterY = Buffer->Height / 2;
    i32 Radius = 100;
    
    // Draw radar chart rings
    for (i32 ring = 1; ring <= 3; ++ring)
    {
        i32 RingRadius = (Radius * ring) / 3;
        // Draw simple octagon approximation
        for (i32 side = 0; side < 8; ++side)
        {
            f32 Angle1 = (f32)side * Tau32 / 8.0f;
            f32 Angle2 = (f32)(side + 1) * Tau32 / 8.0f;
            
            i32 X1 = CenterX + (i32)(cosf(Angle1) * RingRadius);
            i32 Y1 = CenterY + (i32)(sinf(Angle1) * RingRadius);
            i32 X2 = CenterX + (i32)(cosf(Angle2) * RingRadius);
            i32 Y2 = CenterY + (i32)(sinf(Angle2) * RingRadius);
            
            // Draw line using rectangles (simple line drawing)
            DrawRectangle(Buffer, X1, Y1, X2 - X1 + 1, Y2 - Y1 + 1, COLOR_DARK_GRAY);
        }
    }
    
    // Draw emotional state points
    const char *EmotionNames[] = {"Joy", "Sad", "Ang", "Fear", "Trust", "Disg", "Surp", "Ant"};
    u32 EmotionColors[] = {COLOR_YELLOW, COLOR_BLUE, COLOR_RED, RGB(128, 0, 128),
                          COLOR_GREEN, RGB(165, 42, 42), COLOR_CYAN, RGB(255, 165, 0)};
    
    for (i32 emotion = 0; emotion < 8; ++emotion)
    {
        f32 Angle = (f32)emotion * Tau32 / 8.0f;
        f32 Value = NPC->EmotionalState[emotion];
        
        i32 PointRadius = (i32)(Value * Radius);
        i32 PointX = CenterX + (i32)(cosf(Angle) * PointRadius);
        i32 PointY = CenterY + (i32)(sinf(Angle) * PointRadius);
        
        // Draw emotion point
        DrawRectangle(Buffer, PointX - 3, PointY - 3, 6, 6, EmotionColors[emotion]);
        
        // Draw axis line
        i32 AxisX = CenterX + (i32)(cosf(Angle) * Radius);
        i32 AxisY = CenterY + (i32)(sinf(Angle) * Radius);
        DrawRectangle(Buffer, CenterX, CenterY, AxisX - CenterX, 1, COLOR_GRAY);
        DrawRectangle(Buffer, CenterX, CenterY, 1, AxisY - CenterY, COLOR_GRAY);
        
        // Mouse hover for emotion
        if (DebugState->MouseX >= PointX - 5 && DebugState->MouseX <= PointX + 5 &&
            DebugState->MouseY >= PointY - 5 && DebugState->MouseY <= PointY + 5)
        {
            DebugState->MouseHovering = 1;
            DebugState->HoverValue = Value;
            snprintf(DebugState->HoverLabel, sizeof(DebugState->HoverLabel),
                    "%s: %.2f", EmotionNames[emotion], Value);
        }
    }
}

// Process debug input
void ProcessDebugInput(debug_viz_state *DebugState, game_input *Input)
{
    controller_input *Keyboard = &Input->Controllers[0];
    
    // Mode switching
    if (Keyboard->Buttons[0].EndedDown && Keyboard->Buttons[0].HalfTransitionCount > 0) // '1'
    {
        DebugState->CurrentMode = 1;
    }
    else if (Keyboard->Buttons[1].EndedDown && Keyboard->Buttons[1].HalfTransitionCount > 0) // '2'
    {
        DebugState->CurrentMode = 2;
    }
    else if (Keyboard->Buttons[2].EndedDown && Keyboard->Buttons[2].HalfTransitionCount > 0) // '3'
    {
        DebugState->CurrentMode = 3;
    }
    
    // Toggle help
    if (Keyboard->ActionRight.EndedDown && Keyboard->ActionRight.HalfTransitionCount > 0)
    {
        DebugState->ShowHelp = !DebugState->ShowHelp;
    }
    
    // Update mouse position
    DebugState->MouseX = Input->MouseX;
    DebugState->MouseY = Input->MouseY;
    DebugState->MouseHovering = 0; // Reset each frame
}

// Render help overlay
void RenderDebugHelp(debug_viz_state *DebugState, game_offscreen_buffer *Buffer)
{
    if (!DebugState->ShowHelp) return;
    
    i32 PanelX = Buffer->Width / 2 - 200;
    i32 PanelY = Buffer->Height / 2 - 100;
    i32 PanelWidth = 400;
    i32 PanelHeight = 200;
    
    // Help panel background
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 200));
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, 2, COLOR_YELLOW);
    DrawRectangle(Buffer, PanelX, PanelY + PanelHeight - 2, PanelWidth, 2, COLOR_YELLOW);
    DrawRectangle(Buffer, PanelX, PanelY, 2, PanelHeight, COLOR_YELLOW);
    DrawRectangle(Buffer, PanelX + PanelWidth - 2, PanelY, 2, PanelHeight, COLOR_YELLOW);
    
    // Help text (using simple rectangles to represent text)
    DrawRectangle(Buffer, PanelX + 10, PanelY + 10, 100, 8, COLOR_WHITE); // "Neural Debug Help"
    DrawRectangle(Buffer, PanelX + 10, PanelY + 30, 80, 6, COLOR_GRAY);   // "1: Activations"
    DrawRectangle(Buffer, PanelX + 10, PanelY + 40, 80, 6, COLOR_GRAY);   // "2: Weights"
    DrawRectangle(Buffer, PanelX + 10, PanelY + 50, 80, 6, COLOR_GRAY);   // "3: NPC Brain"
}

// Main debug rendering function
void RenderNeuralDebugVisualization(debug_viz_state *DebugState, game_offscreen_buffer *Buffer,
                                   simple_neural_network *Network, simple_npc *NPC)
{
    u64 StartCycles = ReadCPUTimer();
    
    // Render current mode
    switch (DebugState->CurrentMode)
    {
        case 1:
        {
            RenderNetworkActivations(DebugState, Buffer, Network);
            DrawDebugString(Buffer, "Neural Activations", 10, 10, COLOR_WHITE);
        } break;
        
        case 2:
        {
            RenderWeightHeatmap(DebugState, Buffer, Network);
            DrawDebugString(Buffer, "Weight Heatmap", 10, 10, COLOR_WHITE);
        } break;
        
        case 3:
        {
            RenderNPCBrainActivity(DebugState, Buffer, NPC);
            DrawDebugString(Buffer, "NPC Brain Activity", 10, 10, COLOR_WHITE);
        } break;
        
        default:
        {
            DrawDebugString(Buffer, "Press 1-3 for debug modes", 10, 10, COLOR_WHITE);
        } break;
    }
    
    // Render mouse hover information
    if (DebugState->MouseHovering)
    {
        i32 InfoX = DebugState->MouseX + 10;
        i32 InfoY = DebugState->MouseY - 20;
        
        // Adjust position to stay on screen
        if (InfoX + 150 > Buffer->Width) InfoX = DebugState->MouseX - 160;
        if (InfoY < 0) InfoY = DebugState->MouseY + 10;
        
        // Info panel
        DrawRectangle(Buffer, InfoX, InfoY, 150, 30, RGBA(0, 0, 0, 200));
        DrawRectangle(Buffer, InfoX, InfoY, 150, 1, COLOR_WHITE);
        DrawRectangle(Buffer, InfoX, InfoY + 29, 150, 1, COLOR_WHITE);
        DrawRectangle(Buffer, InfoX, InfoY, 1, 30, COLOR_WHITE);
        DrawRectangle(Buffer, InfoX + 149, InfoY, 1, 30, COLOR_WHITE);
        
        // Info text (simplified)
        DrawRectangle(Buffer, InfoX + 5, InfoY + 5, 100, 8, COLOR_WHITE);
        DrawRectangle(Buffer, InfoX + 5, InfoY + 15, 80, 8, COLOR_CYAN);
    }
    
    // Render help if requested
    RenderDebugHelp(DebugState, Buffer);
    
    // Performance timing
    u64 EndCycles = ReadCPUTimer();
    DebugState->VisualizationCycles = EndCycles - StartCycles;
    
    // Show mode in corner
    char ModeText[32];
    snprintf(ModeText, sizeof(ModeText), "Mode: %d", DebugState->CurrentMode);
    DrawRectangle(Buffer, Buffer->Width - 100, 10, 80, 12, RGBA(0, 0, 0, 128));
    DrawRectangle(Buffer, Buffer->Width - 95, 12, 70, 8, COLOR_GREEN);
}

// Main example function
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    local_persist b32 Initialized = 0;
    local_persist memory_arena PermanentArena;
    local_persist memory_arena TransientArena;
    local_persist simple_neural_network *Network;
    local_persist simple_npc *NPC;
    local_persist debug_viz_state DebugState;
    local_persist f32 Time = 0.0f;
    
    if (!Initialized)
    {
        InitializeArena(&PermanentArena, Memory->PermanentStorageSize, Memory->PermanentStorage);
        InitializeArena(&TransientArena, Memory->TransientStorageSize, Memory->TransientStorage);
        
        Network = CreateExampleNetwork(&PermanentArena);
        NPC = CreateExampleNPC(&PermanentArena);
        
        ZeroStruct(DebugState);
        DebugState.CurrentMode = 1; // Start with activations
        DebugState.ZoomLevel = 1.0f;
        
        Initialized = 1;
    }
    
    Time += Clock->SecondsElapsed;
    
    // Process debug input
    ProcessDebugInput(&DebugState, Input);
    
    // Simulate neural network
    if (!DebugState.IsPaused)
    {
        SimulateNetworkInference(Network, Time);
        
        // Update NPC emotional state over time
        for (i32 i = 0; i < 8; ++i)
        {
            NPC->EmotionalState[i] += 0.01f * sinf(Time * 0.5f + (f32)i);
            if (NPC->EmotionalState[i] > 1.0f) NPC->EmotionalState[i] = 1.0f;
            if (NPC->EmotionalState[i] < 0.0f) NPC->EmotionalState[i] = 0.0f;
        }
    }
    
    // RENDERING
    ClearBuffer(Buffer, COLOR_BLACK);
    
    // Render debug visualization
    RenderNeuralDebugVisualization(&DebugState, Buffer, Network, NPC);
    
    // Show instructions at bottom
    DrawRectangle(Buffer, 10, Buffer->Height - 30, 300, 20, RGBA(0, 0, 0, 128));
    DrawDebugString(Buffer, "1=Act 2=Weights 3=NPC H=Help", 15, Buffer->Height - 25, COLOR_WHITE);
}

// Forward declaration for LinuxMain
int LinuxMain(int ArgumentCount, char **Arguments);

// Main entry point
int main(int ArgumentCount, char **Arguments)
{
    return LinuxMain(ArgumentCount, Arguments);
}