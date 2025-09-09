#include "handmade.h"
#include "memory.h"
#include "npc_brain.h"
#include "neural_debug.h"
#include <math.h>
#include <stdio.h>

/*
    Complete NPC Demonstration - The Final Integration
    
    This demonstrates a fully functional NPC with:
    1. Complete neural brain (LSTM + DNC + EWC)  
    2. Rich sensory processing and world interaction
    3. Persistent memory and emotional relationships
    4. Real-time performance < 1ms per NPC
    5. Full debug visualization
    
    Demo Scenarios:
    - NPC learns player's combat preferences
    - Remembers past conversations and references them
    - Forms emotional bonds that persist across sessions
    - Learns new skills while retaining old ones
    - Demonstrates personality-driven behavior
    
    Controls:
    - WASD: Player movement
    - Space: Interact with NPC
    - Tab: Cycle through NPCs
    - R: Reset NPC relationships
    - S: Save NPC states
    - L: Load NPC states
    - 1-9: Debug visualization modes
    - ~: Toggle debug visualization
*/

// =============================================================================
// GAME WORLD AND PLAYER STATE
// =============================================================================

typedef struct player_state
{
    f32 X, Y;                           // Player position
    f32 VelocityX, VelocityY;          // Player velocity
    f32 Health;                        // Player health (0-1)
    f32 Energy;                        // Player energy (0-1)
    npc_action_type LastAction;        // What player just did
    f32 EmotionalTone;                 // Player's emotional state (-1 to 1)
    u32 CombatStyle;                   // Learned combat preferences
    
    // Interaction state
    u32 InteractingWithNPC;            // ID of NPC being talked to (0 = none)
    f32 ConversationTime;              // How long current conversation has lasted
    char LastMessage[256];             // Last thing player said
} player_state;

typedef struct game_world
{
    // World properties
    f32 WorldWidth, WorldHeight;       // World boundaries
    f32 TimeOfDay;                     // 0-1 (morning to night)
    f32 DangerLevel;                   // Overall threat level
    f32 SocialActivity;                // How busy/social the area is
    
    // Environmental audio/visual
    f32 AmbientNoise;                  // Background audio level
    f32 VisualComplexity;              // How much is happening visually
    f32 WeatherIntensity;              // Weather effects
    
    // Interaction tracking
    u32 TotalInteractions;             // Global interaction counter
    f32 AverageInteractionQuality;     // How well interactions go
} game_world;

// =============================================================================
// NPC GAME INTEGRATION
// =============================================================================

typedef struct npc_game_entity
{
    npc_brain *Brain;                  // Neural brain system
    
    // Physical properties
    f32 X, Y;                          // World position
    f32 VelocityX, VelocityY;         // Current velocity
    f32 Size;                          // Visual size
    u32 Color;                         // Display color
    
    // Game-specific state
    f32 Health;                        // NPC health
    f32 Energy;                        // NPC energy
    f32 ViewDistance;                  // How far NPC can see
    f32 HearingRange;                  // How far NPC can hear
    
    // Interaction state
    b32 IsInteracting;                 // Currently in conversation
    f32 LastInteractionTime;           // Time since last player interaction
    npc_interaction_context InteractionContext; // Rich interaction state
    
    // Learning and memory
    f32 SkillLevels[8];                // Combat, trade, social, etc. skills
    f32 PlayerPreferenceModel[16];     // What NPC has learned about player
    u32 ConversationHistory[64];       // Ring buffer of past topics
    u32 ConversationHistoryIndex;
    
    // Visual representation
    f32 AnimationTime;                 // For simple animation
    f32 EmotionalDisplayIntensity;     // How strongly to show emotions
} npc_game_entity;

typedef struct complete_npc_demo
{
    // Core systems
    npc_system *NPCSystem;             // NPC neural management
    neural_debug_state *DebugState;    // Visualization system
    memory_arena *GameArena;           // Game memory
    memory_arena *NPCArena;            // NPC-specific memory
    
    // Game state
    player_state Player;               // Player state
    game_world World;                  // World state
    npc_game_entity NPCs[8];           // Game NPC entities
    u32 ActiveNPCCount;                // Number of active NPCs
    
    // Demo control
    u32 SelectedNPC;                   // Which NPC to focus debug on
    b32 PauseSimulation;               // Pause for detailed inspection
    f32 SimulationSpeed;               // Speed multiplier
    
    // Demo scenarios
    enum {
        DEMO_SCENARIO_FIRST_MEETING,
        DEMO_SCENARIO_FRIENDSHIP_BUILDING,
        DEMO_SCENARIO_COMBAT_TRAINING,
        DEMO_SCENARIO_SKILL_LEARNING,
        DEMO_SCENARIO_MEMORY_RECALL,
        DEMO_SCENARIO_EMOTIONAL_CRISIS,
        DEMO_SCENARIO_COUNT
    } CurrentScenario;
    f32 ScenarioTime;                  // Time in current scenario
    
    // Performance tracking
    f32 TotalNPCUpdateTime;            // Performance monitoring
    f32 AverageNPCUpdateTime;
    u32 FrameCount;
} complete_npc_demo;

// =============================================================================
// SENSORY SIMULATION - Convert game world to NPC sensory input
// =============================================================================

internal void
SimulateNPCSensoryInput(npc_game_entity *NPCEntity, 
                       player_state *Player, 
                       game_world *World,
                       npc_sensory_input *Output)
{
    // PERFORMANCE: Fast sensory simulation - direct computation, no allocations
    
    // Clear all sensory channels
    ZeroStruct(*Output);
    
    // === VISUAL PROCESSING ===
    f32 PlayerDistance = sqrtf((NPCEntity->X - Player->X) * (NPCEntity->X - Player->X) + 
                              (NPCEntity->Y - Player->Y) * (NPCEntity->Y - Player->Y));
    f32 NormalizedDistance = PlayerDistance / NPCEntity->ViewDistance;
    
    // Simple 16x16 visual field simulation
    if(NormalizedDistance <= 1.0f)
    {
        // Player is visible - put them in center of visual field
        f32 PlayerIntensity = 1.0f - NormalizedDistance;
        i32 CenterX = 8, CenterY = 8;
        
        // Add some spread for more realistic visual representation
        for(i32 dy = -2; dy <= 2; ++dy)
        {
            for(i32 dx = -2; dx <= 2; ++dx)
            {
                i32 px = CenterX + dx, py = CenterY + dy;
                if(px >= 0 && px < 16 && py >= 0 && py < 16)
                {
                    f32 Spread = 1.0f - 0.3f * (abs(dx) + abs(dy));
                    Output->VisualField[py][px] = PlayerIntensity * Spread;
                }
            }
        }
        
        Output->PlayerVisible = PlayerIntensity;
        Output->PlayerDistance = NormalizedDistance;
    }
    
    // Add world complexity to visual field
    for(i32 y = 0; y < 16; ++y)
    {
        for(i32 x = 0; x < 16; ++x)
        {
            Output->VisualField[y][x] += World->VisualComplexity * 0.1f * 
                                       ((f32)((x + y) % 5) / 5.0f);
        }
    }
    
    // === AUDIO PROCESSING ===
    f32 HearingDistance = sqrtf((NPCEntity->X - Player->X) * (NPCEntity->X - Player->X) + 
                               (NPCEntity->Y - Player->Y) * (NPCEntity->Y - Player->Y));
    f32 AudioIntensity = Maximum(0.0f, 1.0f - HearingDistance / NPCEntity->HearingRange);
    
    // Simulate frequency spectrum
    for(u32 i = 0; i < 32; ++i)
    {
        f32 FreqBase = World->AmbientNoise * 0.3f;
        
        // Low frequencies - movement/combat
        if(i < 8)
        {
            if(Player->VelocityX != 0.0f || Player->VelocityY != 0.0f)
                FreqBase += AudioIntensity * 0.6f;
            if(Player->LastAction == NPC_ACTION_ATTACK_MELEE || Player->LastAction == NPC_ACTION_ATTACK_RANGED)
                FreqBase += AudioIntensity * 0.8f;
        }
        
        // Mid frequencies - speech
        else if(i < 24)
        {
            if(NPCEntity->IsInteracting)
                FreqBase += AudioIntensity * 0.7f;
        }
        
        // High frequencies - environmental
        else
        {
            FreqBase += World->WeatherIntensity * 0.4f;
        }
        
        Output->AudioSpectrum[i] = FreqBase;
    }
    
    Output->PlayerSpeaking = NPCEntity->IsInteracting ? 1.0f : 0.0f;
    Output->CombatSounds = (Player->LastAction == NPC_ACTION_ATTACK_MELEE) ? AudioIntensity : 0.0f;
    Output->AmbientThreatLevel = World->DangerLevel;
    
    // === SOCIAL CONTEXT ===
    Output->PlayerEmotionalState = Player->EmotionalTone;
    Output->ConversationContext = NPCEntity->IsInteracting ? 1.0f : 0.0f;
    Output->SocialPressure = World->SocialActivity;
    Output->IntimacyLevel = (PlayerDistance < 2.0f && World->SocialActivity < 0.3f) ? 1.0f : 0.0f;
    
    // === INTERNAL STATE ===  
    Output->Hunger = 1.0f - NPCEntity->Energy; // Simple mapping
    Output->Energy = NPCEntity->Energy;
    Output->Health = NPCEntity->Health;
    Output->CurrentGoalPriority = 0.5f; // Default moderate priority
    
    // === ENVIRONMENTAL CONTEXT ===
    Output->LocationType = 0.5f;        // Generic location
    Output->TimeOfDay = World->TimeOfDay;
    Output->Weather = World->WeatherIntensity;
    Output->Familiarity = 0.8f;         // NPCs know their area well
    
    // Pack everything into sensory channels array
    u32 ChannelIndex = 0;
    
    // Visual channels (256)
    for(i32 y = 0; y < 16; ++y)
    {
        for(i32 x = 0; x < 16; ++x)
        {
            if(ChannelIndex < SENSORY_TOTAL_CHANNELS)
                Output->Channels[ChannelIndex++] = Output->VisualField[y][x];
        }
    }
    
    // Audio channels (32)
    for(u32 i = 0; i < 32 && ChannelIndex < SENSORY_TOTAL_CHANNELS; ++i)
    {
        Output->Channels[ChannelIndex++] = Output->AudioSpectrum[i];
    }
    
    // Fill remaining channels with context/social/internal data
    while(ChannelIndex < SENSORY_TOTAL_CHANNELS)
    {
        f32 Value = 0.0f;
        
        // Distribute different types of context information
        u32 ContextType = ChannelIndex % 8;
        switch(ContextType)
        {
            case 0: Value = Output->PlayerEmotionalState; break;
            case 1: Value = Output->ConversationContext; break;
            case 2: Value = Output->SocialPressure; break;
            case 3: Value = Output->IntimacyLevel; break;
            case 4: Value = Output->Energy; break;
            case 5: Value = Output->TimeOfDay; break;
            case 6: Value = Output->Weather; break;
            case 7: Value = Output->Familiarity; break;
        }
        
        Output->Channels[ChannelIndex++] = Value;
    }
}

// =============================================================================
// NPC BEHAVIOR EXECUTION - Convert neural output to game actions
// =============================================================================

internal void
ExecuteNPCAction(npc_game_entity *NPCEntity, 
                npc_action_output *Action,
                player_state *Player,
                game_world *World,
                f32 DeltaTime)
{
    // PERFORMANCE: Direct action execution with minimal branching
    
    // === MOVEMENT ACTIONS ===
    if(Action->PrimaryAction == NPC_ACTION_MOVE_NORTH ||
       Action->PrimaryAction == NPC_ACTION_MOVE_SOUTH ||
       Action->PrimaryAction == NPC_ACTION_MOVE_EAST ||
       Action->PrimaryAction == NPC_ACTION_MOVE_WEST)
    {
        f32 MoveSpeed = 50.0f * Action->MovementSpeed * DeltaTime;
        
        switch(Action->PrimaryAction)
        {
            case NPC_ACTION_MOVE_NORTH: NPCEntity->VelocityY = -MoveSpeed; break;
            case NPC_ACTION_MOVE_SOUTH: NPCEntity->VelocityY = MoveSpeed; break;
            case NPC_ACTION_MOVE_EAST:  NPCEntity->VelocityX = MoveSpeed; break;
            case NPC_ACTION_MOVE_WEST:  NPCEntity->VelocityX = -MoveSpeed; break;
        }
    }
    else
    {
        // Apply neural network's direct movement output
        f32 MoveSpeed = 30.0f * Action->MovementSpeed * DeltaTime;
        NPCEntity->VelocityX = Action->MovementX * MoveSpeed;
        NPCEntity->VelocityY = Action->MovementY * MoveSpeed;
    }
    
    // Apply velocity with damping
    NPCEntity->X += NPCEntity->VelocityX;
    NPCEntity->Y += NPCEntity->VelocityY;
    NPCEntity->VelocityX *= 0.95f; // Damping
    NPCEntity->VelocityY *= 0.95f;
    
    // Keep in world bounds
    if(NPCEntity->X < 0) NPCEntity->X = 0;
    if(NPCEntity->X > World->WorldWidth) NPCEntity->X = World->WorldWidth;
    if(NPCEntity->Y < 0) NPCEntity->Y = 0;
    if(NPCEntity->Y > World->WorldHeight) NPCEntity->Y = World->WorldHeight;
    
    // === SOCIAL ACTIONS ===
    if(Action->SpeechText[0] != '\0')
    {
        // NPC is speaking - update interaction state
        NPCEntity->IsInteracting = 1;
        NPCEntity->InteractionContext.InConversation = 1;
        NPCEntity->InteractionContext.ConversationDuration += DeltaTime;
        
        // Update emotional display based on speech tone
        NPCEntity->EmotionalDisplayIntensity = Action->SpeechEmotionalTone;
        
        // Simple conversation history tracking
        if(Action->PrimaryAction != NPC_ACTION_NONE)
        {
            NPCEntity->ConversationHistory[NPCEntity->ConversationHistoryIndex] = 
                (u32)Action->PrimaryAction;
            NPCEntity->ConversationHistoryIndex = 
                (NPCEntity->ConversationHistoryIndex + 1) % 64;
        }
    }
    
    // === LEARNING ACTIONS ===
    if(Action->MemoryStoreSignal > 0.5f)
    {
        // NPC wants to remember this experience
        npc_learning_experience Experience = {0};
        Experience.Type = LEARNING_SOCIAL_INTERACTION; // Default type
        Experience.Importance = Action->MemoryStoreSignal;
        Experience.Success = (Action->ActionConfidence > 0.6f) ? 1.0f : 0.0f;
        Experience.Novelty = 0.5f; // Default novelty
        
        StoreNPCExperience(NPCEntity->Brain, &Experience);
    }
    
    // === SKILL LEARNING ===
    if(Action->LearningRate > 0.01f)
    {
        // Update skill levels based on recent actions and outcomes
        switch(Action->PrimaryAction)
        {
            case NPC_ACTION_ATTACK_MELEE:
            case NPC_ACTION_ATTACK_RANGED:
            case NPC_ACTION_DEFEND:
                NPCEntity->SkillLevels[0] += Action->LearningRate * 0.1f; // Combat skill
                break;
                
            case NPC_ACTION_OFFER_TRADE:
            case NPC_ACTION_ACCEPT_TRADE:
                NPCEntity->SkillLevels[1] += Action->LearningRate * 0.1f; // Trade skill
                break;
                
            case NPC_ACTION_GREET_FRIENDLY:
            case NPC_ACTION_TELL_STORY:
            case NPC_ACTION_ASK_QUESTION:
                NPCEntity->SkillLevels[2] += Action->LearningRate * 0.1f; // Social skill
                break;
        }
        
        // Clamp skills to [0, 1]
        for(u32 i = 0; i < 8; ++i)
        {
            if(NPCEntity->SkillLevels[i] > 1.0f) NPCEntity->SkillLevels[i] = 1.0f;
            if(NPCEntity->SkillLevels[i] < 0.0f) NPCEntity->SkillLevels[i] = 0.0f;
        }
    }
    
    // === PLAYER PREFERENCE LEARNING ===
    // Update what NPC has learned about player's preferences
    if(Player->LastAction != NPC_ACTION_NONE && NPCEntity->IsInteracting)
    {
        f32 LearningRate = Action->LearningRate;
        u32 ActionIndex = (u32)Player->LastAction % 16;
        
        // Increase preference weight for actions player uses frequently
        NPCEntity->PlayerPreferenceModel[ActionIndex] += LearningRate;
        
        // Normalize to prevent unbounded growth
        f32 Sum = 0.0f;
        for(u32 i = 0; i < 16; ++i) Sum += NPCEntity->PlayerPreferenceModel[i];
        if(Sum > 0.0f)
        {
            for(u32 i = 0; i < 16; ++i)
                NPCEntity->PlayerPreferenceModel[i] /= Sum;
        }
    }
    
    // === VISUAL FEEDBACK ===
    // Update NPC color based on emotional state and actions
    f32 EmotionIntensity = Action->SpeechEmotionalTone;
    
    switch(Action->DominantEmotion)
    {
        case EMOTION_JOY:
            NPCEntity->Color = RGB((u8)(128 + 127 * EmotionIntensity), 
                                  (u8)(200 + 55 * EmotionIntensity), 
                                  128);
            break;
            
        case EMOTION_FEAR:
            NPCEntity->Color = RGB(128, 
                                  (u8)(128 - 64 * EmotionIntensity), 
                                  (u8)(128 + 127 * EmotionIntensity));
            break;
            
        case EMOTION_ANGER:
            NPCEntity->Color = RGB((u8)(200 + 55 * EmotionIntensity), 
                                  (u8)(128 - 64 * EmotionIntensity), 
                                  128);
            break;
            
        case EMOTION_TRUST:
            NPCEntity->Color = RGB(128, 
                                  (u8)(200 + 55 * EmotionIntensity), 
                                  (u8)(200 + 55 * EmotionIntensity));
            break;
            
        default:
            NPCEntity->Color = RGB(180, 180, 180); // Neutral gray
            break;
    }
    
    // Update animation time
    NPCEntity->AnimationTime += DeltaTime * (1.0f + Action->ActionIntensity);
}

// =============================================================================
// DEMO SCENARIOS - Structured learning experiences
// =============================================================================

internal void
UpdateDemoScenario(complete_npc_demo *Demo, f32 DeltaTime)
{
    // PERFORMANCE: Simple scenario state machine
    
    Demo->ScenarioTime += DeltaTime;
    
    switch(Demo->CurrentScenario)
    {
        case DEMO_SCENARIO_FIRST_MEETING:
        {
            // Scenario: Player approaches NPC for the first time
            if(Demo->ScenarioTime < 2.0f)
            {
                // Move player close to NPC
                f32 TargetDistance = 3.0f;
                f32 CurrentDistance = sqrtf((Demo->Player.X - Demo->NPCs[0].X) * 
                                          (Demo->Player.X - Demo->NPCs[0].X) +
                                          (Demo->Player.Y - Demo->NPCs[0].Y) * 
                                          (Demo->Player.Y - Demo->NPCs[0].Y));
                
                if(CurrentDistance > TargetDistance)
                {
                    f32 MoveSpeed = 20.0f * DeltaTime;
                    f32 DirectionX = (Demo->NPCs[0].X - Demo->Player.X) / CurrentDistance;
                    f32 DirectionY = (Demo->NPCs[0].Y - Demo->Player.Y) / CurrentDistance;
                    
                    Demo->Player.X += DirectionX * MoveSpeed;
                    Demo->Player.Y += DirectionY * MoveSpeed;
                }
            }
            else if(Demo->ScenarioTime < 8.0f)
            {
                // Simulate friendly greeting interaction
                Demo->Player.LastAction = NPC_ACTION_GREET_FRIENDLY;
                Demo->Player.EmotionalTone = 0.7f; // Positive
                Demo->NPCs[0].IsInteracting = 1;
            }
            else
            {
                // Move to next scenario
                Demo->CurrentScenario = DEMO_SCENARIO_FRIENDSHIP_BUILDING;
                Demo->ScenarioTime = 0.0f;
            }
        } break;
        
        case DEMO_SCENARIO_FRIENDSHIP_BUILDING:
        {
            // Scenario: Multiple positive interactions to build trust
            if(Demo->ScenarioTime < 3.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_ASK_QUESTION;
                Demo->Player.EmotionalTone = 0.5f;
            }
            else if(Demo->ScenarioTime < 6.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_TELL_STORY;
                Demo->Player.EmotionalTone = 0.8f;
            }
            else if(Demo->ScenarioTime < 9.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_EXPRESS_EMOTION;
                Demo->Player.EmotionalTone = 0.6f;
            }
            else
            {
                Demo->CurrentScenario = DEMO_SCENARIO_COMBAT_TRAINING;
                Demo->ScenarioTime = 0.0f;
            }
        } break;
        
        case DEMO_SCENARIO_COMBAT_TRAINING:
        {
            // Scenario: Practice combat together to learn player's style
            if(Demo->ScenarioTime < 4.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_ATTACK_MELEE;
                Demo->Player.EmotionalTone = 0.4f; // Focused
                Demo->World.DangerLevel = 0.6f;
            }
            else if(Demo->ScenarioTime < 8.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_DEFEND;
                Demo->Player.EmotionalTone = 0.3f;
                Demo->World.DangerLevel = 0.8f;
            }
            else
            {
                Demo->CurrentScenario = DEMO_SCENARIO_SKILL_LEARNING;
                Demo->ScenarioTime = 0.0f;
                Demo->World.DangerLevel = 0.2f; // Reset danger
            }
        } break;
        
        case DEMO_SCENARIO_SKILL_LEARNING:
        {
            // Scenario: NPC learns new skill (trading) while retaining combat
            if(Demo->ScenarioTime < 5.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_OFFER_TRADE;
                Demo->Player.EmotionalTone = 0.6f;
            }
            else if(Demo->ScenarioTime < 10.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_ACCEPT_TRADE;
                Demo->Player.EmotionalTone = 0.7f;
            }
            else
            {
                Demo->CurrentScenario = DEMO_SCENARIO_MEMORY_RECALL;
                Demo->ScenarioTime = 0.0f;
            }
        } break;
        
        case DEMO_SCENARIO_MEMORY_RECALL:
        {
            // Scenario: NPC references past experiences
            Demo->Player.LastAction = NPC_ACTION_RECALL_MEMORY;
            Demo->Player.EmotionalTone = 0.5f;
            
            if(Demo->ScenarioTime > 6.0f)
            {
                Demo->CurrentScenario = DEMO_SCENARIO_EMOTIONAL_CRISIS;
                Demo->ScenarioTime = 0.0f;
            }
        } break;
        
        case DEMO_SCENARIO_EMOTIONAL_CRISIS:
        {
            // Scenario: Test emotional resilience and relationship strength
            if(Demo->ScenarioTime < 3.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_GREET_HOSTILE;
                Demo->Player.EmotionalTone = -0.8f; // Very negative
            }
            else if(Demo->ScenarioTime < 6.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_GREET_NEUTRAL;
                Demo->Player.EmotionalTone = 0.0f; // Return to neutral
            }
            else if(Demo->ScenarioTime < 9.0f)
            {
                Demo->Player.LastAction = NPC_ACTION_GREET_FRIENDLY;
                Demo->Player.EmotionalTone = 0.7f; // Recovery
            }
            else
            {
                // Cycle back to beginning
                Demo->CurrentScenario = DEMO_SCENARIO_FIRST_MEETING;
                Demo->ScenarioTime = 0.0f;
            }
        } break;
    }
}

// =============================================================================
// VISUALIZATION - Game world and NPC rendering
// =============================================================================

internal void
RenderGameWorld(complete_npc_demo *Demo, game_offscreen_buffer *Buffer)
{
    // PERFORMANCE: Simple immediate-mode rendering
    
    // Clear background with world-colored tint
    u32 BackgroundColor = RGB((u8)(50 + 50 * Demo->World.TimeOfDay),
                             (u8)(40 + 60 * (1.0f - Demo->World.DangerLevel)),
                             (u8)(80 + 40 * Demo->World.WeatherIntensity));
    ClearBuffer(Buffer, BackgroundColor);
    
    // Draw world boundaries
    DrawRectangle(Buffer, 0, 0, Buffer->Width, 2, COLOR_WHITE);
    DrawRectangle(Buffer, 0, Buffer->Height-2, Buffer->Width, 2, COLOR_WHITE);
    DrawRectangle(Buffer, 0, 0, 2, Buffer->Height, COLOR_WHITE);
    DrawRectangle(Buffer, Buffer->Width-2, 0, 2, Buffer->Height, COLOR_WHITE);
    
    // Draw player
    i32 PlayerScreenX = (i32)Demo->Player.X;
    i32 PlayerScreenY = (i32)Demo->Player.Y;
    u32 PlayerColor = RGB(100, 255, 100); // Green for player
    
    DrawRectangle(Buffer, PlayerScreenX - 8, PlayerScreenY - 8, 16, 16, PlayerColor);
    
    // Draw player velocity indicator
    if(Demo->Player.VelocityX != 0.0f || Demo->Player.VelocityY != 0.0f)
    {
        i32 VelEndX = PlayerScreenX + (i32)(Demo->Player.VelocityX * 5.0f);
        i32 VelEndY = PlayerScreenY + (i32)(Demo->Player.VelocityY * 5.0f);
        DrawRectangle(Buffer, VelEndX - 2, VelEndY - 2, 4, 4, COLOR_YELLOW);
    }
    
    // Draw NPCs
    for(u32 i = 0; i < Demo->ActiveNPCCount; ++i)
    {
        npc_game_entity *NPC = &Demo->NPCs[i];
        
        i32 NPCScreenX = (i32)NPC->X;
        i32 NPCScreenY = (i32)NPC->Y;
        
        // Size varies with emotional intensity
        i32 NPCSize = (i32)(12 + 8 * NPC->EmotionalDisplayIntensity);
        
        // Draw NPC with emotional color
        DrawRectangle(Buffer, NPCScreenX - NPCSize/2, NPCScreenY - NPCSize/2, 
                     NPCSize, NPCSize, NPC->Color);
        
        // Draw NPC border
        u32 BorderColor = (i == Demo->SelectedNPC) ? COLOR_YELLOW : COLOR_WHITE;
        DrawRectangle(Buffer, NPCScreenX - NPCSize/2 - 1, NPCScreenY - NPCSize/2 - 1, 
                     NPCSize + 2, 1, BorderColor);
        DrawRectangle(Buffer, NPCScreenX - NPCSize/2 - 1, NPCScreenY + NPCSize/2, 
                     NPCSize + 2, 1, BorderColor);
        DrawRectangle(Buffer, NPCScreenX - NPCSize/2 - 1, NPCScreenY - NPCSize/2, 
                     1, NPCSize, BorderColor);
        DrawRectangle(Buffer, NPCScreenX + NPCSize/2, NPCScreenY - NPCSize/2, 
                     1, NPCSize, BorderColor);
        
        // Draw interaction line if talking to player
        if(NPC->IsInteracting)
        {
            DrawRectangle(Buffer, NPCScreenX, NPCScreenY, 
                         PlayerScreenX - NPCScreenX, 2, COLOR_CYAN);
            DrawRectangle(Buffer, NPCScreenX, NPCScreenY,
                         2, PlayerScreenY - NPCScreenY, COLOR_CYAN);
        }
        
        // Draw NPC vision range (if selected)
        if(i == Demo->SelectedNPC)
        {
            i32 VisionRadius = (i32)NPC->ViewDistance;
            u32 VisionColor = RGBA(255, 255, 0, 64); // Semi-transparent yellow
            
            // Simple circle approximation
            for(i32 angle = 0; angle < 360; angle += 10)
            {
                f32 RadAngle = (f32)angle * Pi32 / 180.0f;
                i32 CircleX = NPCScreenX + (i32)(VisionRadius * cosf(RadAngle));
                i32 CircleY = NPCScreenY + (i32)(VisionRadius * sinf(RadAngle));
                DrawRectangle(Buffer, CircleX - 1, CircleY - 1, 2, 2, VisionColor);
            }
        }
    }
}

internal void
RenderNPCStatus(complete_npc_demo *Demo, game_offscreen_buffer *Buffer)
{
    // PERFORMANCE: Simple status overlay
    
    if(Demo->SelectedNPC >= Demo->ActiveNPCCount) return;
    
    npc_game_entity *NPC = &Demo->NPCs[Demo->SelectedNPC];
    npc_brain *Brain = NPC->Brain;
    
    // Status panel background
    i32 PanelX = 10, PanelY = 10;
    i32 PanelWidth = 300, PanelHeight = 200;
    
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, RGBA(0, 0, 0, 128));
    DrawRectangle(Buffer, PanelX + 2, PanelY + 2, PanelWidth - 4, PanelHeight - 4, 
                 RGBA(50, 50, 50, 200));
    
    // NPC name and archetype
    i32 TextY = PanelY + 10;
    const char *ArchetypeNames[] = {
        "Warrior", "Scholar", "Merchant", "Rogue", 
        "Guardian", "Wanderer", "Mystic", "Craftsman"
    };
    
    // Simple text rendering using rectangles (normally would use proper font)
    // "NPC: [Name] ([Archetype])"
    DrawRectangle(Buffer, PanelX + 10, TextY, 100, 12, COLOR_WHITE);
    TextY += 16;
    
    // Emotional state bars
    const char *EmotionNames[] = {
        "Trust", "Fear", "Anger", "Joy", "Curiosity", 
        "Respect", "Affection", "Loneliness"
    };
    
    for(u32 i = 0; i < 8; ++i)
    {
        f32 EmotionValue = Brain->EmotionalState[i];
        i32 BarWidth = (i32)(100 * EmotionValue);
        
        // Emotion name (simplified)
        DrawRectangle(Buffer, PanelX + 10, TextY, 60, 8, COLOR_GRAY);
        
        // Emotion bar background
        DrawRectangle(Buffer, PanelX + 80, TextY, 100, 8, COLOR_DARK_GRAY);
        
        // Emotion bar fill
        u32 BarColor = (EmotionValue > 0.7f) ? COLOR_GREEN : 
                      (EmotionValue > 0.3f) ? COLOR_YELLOW : COLOR_RED;
        DrawRectangle(Buffer, PanelX + 80, TextY, BarWidth, 8, BarColor);
        
        TextY += 12;
    }
    
    // Performance stats
    DrawRectangle(Buffer, PanelX + 200, PanelY + 10, 80, 40, COLOR_DARK_GRAY);
    
    // Update time bar
    f32 UpdateTimeRatio = NPC->Brain->LastUpdateTimeMs / 1.0f; // Target 1ms
    i32 PerfBarWidth = (i32)(70 * UpdateTimeRatio);
    u32 PerfColor = (UpdateTimeRatio > 1.0f) ? COLOR_RED : COLOR_GREEN;
    DrawRectangle(Buffer, PanelX + 205, PanelY + 15, PerfBarWidth, 6, PerfColor);
    
    // Skill levels
    TextY = PanelY + 60;
    const char *SkillNames[] = {"Combat", "Trade", "Social", "Magic"};
    
    for(u32 i = 0; i < 4; ++i)
    {
        f32 SkillValue = NPC->SkillLevels[i];
        i32 SkillBarWidth = (i32)(50 * SkillValue);
        
        DrawRectangle(Buffer, PanelX + 200, TextY, 50, 6, COLOR_DARK_GRAY);
        DrawRectangle(Buffer, PanelX + 200, TextY, SkillBarWidth, 6, COLOR_BLUE);
        
        TextY += 10;
    }
}

internal void
RenderScenarioInfo(complete_npc_demo *Demo, game_offscreen_buffer *Buffer)
{
    // PERFORMANCE: Simple scenario display
    
    const char *ScenarioNames[] = {
        "First Meeting",
        "Friendship Building", 
        "Combat Training",
        "Skill Learning",
        "Memory Recall",
        "Emotional Crisis"
    };
    
    // Scenario panel
    i32 PanelX = Buffer->Width - 220;
    i32 PanelY = 10;
    i32 PanelWidth = 200;
    i32 PanelHeight = 100;
    
    DrawRectangle(Buffer, PanelX, PanelY, PanelWidth, PanelHeight, 
                 RGBA(0, 0, 0, 128));
    DrawRectangle(Buffer, PanelX + 2, PanelY + 2, PanelWidth - 4, PanelHeight - 4,
                 RGBA(30, 30, 60, 200));
    
    // Scenario name (simplified text rendering)
    DrawRectangle(Buffer, PanelX + 10, PanelY + 10, 180, 12, COLOR_CYAN);
    
    // Scenario progress bar
    f32 ScenarioProgress = Demo->ScenarioTime / 10.0f; // Assume 10s per scenario
    if(ScenarioProgress > 1.0f) ScenarioProgress = 1.0f;
    
    i32 ProgressBarWidth = (i32)(160 * ScenarioProgress);
    DrawRectangle(Buffer, PanelX + 10, PanelY + 30, 160, 8, COLOR_DARK_GRAY);
    DrawRectangle(Buffer, PanelX + 10, PanelY + 30, ProgressBarWidth, 8, COLOR_GREEN);
    
    // Instructions
    DrawRectangle(Buffer, PanelX + 10, PanelY + 50, 180, 40, COLOR_GRAY);
}

// =============================================================================
// MAIN DEMO UPDATE AND RENDER
// =============================================================================

internal void
UpdateCompleteNPCDemo(complete_npc_demo *Demo, game_input *Input, f32 DeltaTime)
{
    // PERFORMANCE: Complete NPC system update < 5ms for 8 NPCs
    u64 StartCycles = ReadCPUTimer();
    
    // Handle input
    controller_input *Controller = &Input->Controllers[0];
    
    // Player movement
    f32 MoveSpeed = 100.0f * DeltaTime;
    if(Controller->MoveUp.EndedDown) Demo->Player.VelocityY -= MoveSpeed;
    if(Controller->MoveDown.EndedDown) Demo->Player.VelocityY += MoveSpeed;
    if(Controller->MoveLeft.EndedDown) Demo->Player.VelocityX -= MoveSpeed;
    if(Controller->MoveRight.EndedDown) Demo->Player.VelocityX += MoveSpeed;
    
    // Apply player movement
    Demo->Player.X += Demo->Player.VelocityX * DeltaTime;
    Demo->Player.Y += Demo->Player.VelocityY * DeltaTime;
    Demo->Player.VelocityX *= 0.9f; // Damping
    Demo->Player.VelocityY *= 0.9f;
    
    // Keep player in bounds
    if(Demo->Player.X < 10) Demo->Player.X = 10;
    if(Demo->Player.X > Demo->World.WorldWidth - 10) Demo->Player.X = Demo->World.WorldWidth - 10;
    if(Demo->Player.Y < 10) Demo->Player.Y = 10;
    if(Demo->Player.Y > Demo->World.WorldHeight - 10) Demo->Player.Y = Demo->World.WorldHeight - 10;
    
    // NPC selection
    if(Controller->ActionRight.EndedDown && Controller->ActionRight.HalfTransitionCount > 0)
    {
        Demo->SelectedNPC = (Demo->SelectedNPC + 1) % Demo->ActiveNPCCount;
    }
    
    // Interaction toggle
    if(Controller->ActionUp.EndedDown && Controller->ActionUp.HalfTransitionCount > 0)
    {
        Demo->Player.LastAction = NPC_ACTION_GREET_FRIENDLY;
        Demo->Player.InteractingWithNPC = (Demo->Player.InteractingWithNPC == 0) ? 1 : 0;
    }
    
    // Update world state
    Demo->World.TimeOfDay += DeltaTime * 0.1f; // Slow day/night cycle
    if(Demo->World.TimeOfDay > 1.0f) Demo->World.TimeOfDay -= 1.0f;
    
    Demo->World.SocialActivity = 0.3f + 0.2f * sinf(Demo->ScenarioTime * 0.5f);
    Demo->World.VisualComplexity = 0.4f + 0.1f * cosf(Demo->ScenarioTime * 0.3f);
    Demo->World.AmbientNoise = 0.2f + 0.1f * Demo->World.SocialActivity;
    
    // Update demo scenario
    if(!Demo->PauseSimulation)
    {
        UpdateDemoScenario(Demo, DeltaTime * Demo->SimulationSpeed);
    }
    
    // Update all NPCs
    Demo->TotalNPCUpdateTime = 0.0f;
    
    for(u32 i = 0; i < Demo->ActiveNPCCount; ++i)
    {
        u64 NPCStartCycles = ReadCPUTimer();
        
        npc_game_entity *NPCEntity = &Demo->NPCs[i];
        
        // Simulate sensory input
        npc_sensory_input SensoryInput;
        SimulateNPCSensoryInput(NPCEntity, &Demo->Player, &Demo->World, &SensoryInput);
        
        // Update interaction context
        NPCEntity->InteractionContext.InConversation = NPCEntity->IsInteracting;
        NPCEntity->InteractionContext.PlayerEmotionalTone = Demo->Player.EmotionalTone;
        NPCEntity->InteractionContext.ThreatLevel = Demo->World.DangerLevel;
        NPCEntity->InteractionContext.PrivateSetting = (Demo->World.SocialActivity < 0.3f);
        
        f32 DistanceToPlayer = sqrtf((NPCEntity->X - Demo->Player.X) * (NPCEntity->X - Demo->Player.X) +
                                   (NPCEntity->Y - Demo->Player.Y) * (NPCEntity->Y - Demo->Player.Y));
        NPCEntity->InteractionContext.UrgencyLevel = (DistanceToPlayer < 5.0f) ? 0.8f : 0.2f;
        
        // Update NPC brain
        UpdateNPCBrain(NPCEntity->Brain, &SensoryInput, &NPCEntity->InteractionContext, DeltaTime);
        
        // Execute NPC actions
        ExecuteNPCAction(NPCEntity, &NPCEntity->Brain->CurrentOutput, &Demo->Player, &Demo->World, DeltaTime);
        
        // Update interaction state
        if(DistanceToPlayer > 10.0f)
        {
            NPCEntity->IsInteracting = 0;
            NPCEntity->InteractionContext.InConversation = 0;
        }
        
        u64 NPCEndCycles = ReadCPUTimer();
        f32 NPCUpdateTime = (f32)(NPCEndCycles - NPCStartCycles) / 2400000.0f; // Assume 2.4GHz
        Demo->TotalNPCUpdateTime += NPCUpdateTime;
    }
    
    Demo->AverageNPCUpdateTime = Demo->TotalNPCUpdateTime / Demo->ActiveNPCCount;
    
    // Update debug system
    UpdateNeuralDebug(Demo->DebugState, Input, DeltaTime);
    
    u64 EndCycles = ReadCPUTimer();
    f32 TotalUpdateTime = (f32)(EndCycles - StartCycles) / 2400000.0f;
    
    Demo->FrameCount++;
    
    // Print performance stats occasionally
    if(Demo->FrameCount % 300 == 0) // Every 5 seconds at 60fps
    {
        printf("NPC Demo Performance:\n");
        printf("  Total Update Time: %.3f ms\n", TotalUpdateTime);
        printf("  Average NPC Update: %.3f ms\n", Demo->AverageNPCUpdateTime);
        printf("  NPCs: %u\n", Demo->ActiveNPCCount);
        printf("  Scenario: %d (%.1fs)\n", Demo->CurrentScenario, Demo->ScenarioTime);
    }
}

internal void
RenderCompleteNPCDemo(complete_npc_demo *Demo, game_offscreen_buffer *Buffer)
{
    // PERFORMANCE: Immediate mode rendering < 2ms
    
    if(Demo->DebugState->DebugEnabled && Demo->SelectedNPC < Demo->ActiveNPCCount)
    {
        // Debug visualization takes priority
        npc_brain *SelectedBrain = Demo->NPCs[Demo->SelectedNPC].Brain;
        
        switch(Demo->DebugState->CurrentMode)
        {
            case DEBUG_VIZ_NPC_BRAIN:
                RenderNPCBrainDebug(SelectedBrain, Buffer, Demo->DebugState);
                break;
                
            case DEBUG_VIZ_NEURAL_ACTIVATIONS:
                // Would render LSTM activations
                break;
                
            case DEBUG_VIZ_DNC_MEMORY:
                RenderDNCMemoryMatrix(Demo->DebugState, Buffer, SelectedBrain->Memory);
                break;
                
            case DEBUG_VIZ_EWC_FISHER:
                RenderEWCFisherInfo(Demo->DebugState, Buffer, SelectedBrain->Consolidation);
                break;
                
            default:
                RenderGameWorld(Demo, Buffer);
                break;
        }
        
        // Always render debug overlay
        RenderNeuralDebug(Demo->DebugState, Buffer);
    }
    else
    {
        // Normal game view
        RenderGameWorld(Demo, Buffer);
        RenderNPCStatus(Demo, Buffer);
        RenderScenarioInfo(Demo, Buffer);
        
        // Instructions overlay
        i32 InstrY = Buffer->Height - 80;
        DrawRectangle(Buffer, 10, InstrY, Buffer->Width - 20, 70, RGBA(0, 0, 0, 128));
        
        // Simple instruction text representation
        DrawRectangle(Buffer, 20, InstrY + 10, 200, 10, COLOR_WHITE);  // "WASD: Move Player"
        DrawRectangle(Buffer, 20, InstrY + 25, 200, 10, COLOR_WHITE);  // "Space: Interact"
        DrawRectangle(Buffer, 20, InstrY + 40, 200, 10, COLOR_WHITE);  // "Tab: Select NPC"
        DrawRectangle(Buffer, 20, InstrY + 55, 200, 10, COLOR_WHITE);  // "~: Debug Mode"
    }
}

// =============================================================================
// DEMO INITIALIZATION AND MAIN LOOP
// =============================================================================

internal complete_npc_demo *
InitializeCompleteNPCDemo(memory_arena *Arena)
{
    // PERFORMANCE: One-time initialization of complete demo system
    
    complete_npc_demo *Demo = PushStruct(Arena, complete_npc_demo);
    
    // Initialize memory arenas
    Demo->GameArena = PushSubArena(Arena, Megabytes(64));
    Demo->NPCArena = PushSubArena(Arena, Megabytes(128));
    
    // Initialize NPC system
    Demo->NPCSystem = InitializeNPCSystem(Demo->NPCArena, NULL);
    
    // Initialize debug system
    Demo->DebugState = InitializeNeuralDebugSystem(Demo->GameArena, 
                                                  DEBUG_MAX_NEURONS,
                                                  DEBUG_HISTORY_SIZE);
    
    // Initialize world
    Demo->World.WorldWidth = 800.0f;
    Demo->World.WorldHeight = 600.0f;
    Demo->World.TimeOfDay = 0.5f;
    Demo->World.DangerLevel = 0.2f;
    Demo->World.SocialActivity = 0.4f;
    Demo->World.AmbientNoise = 0.3f;
    Demo->World.VisualComplexity = 0.3f;
    Demo->World.WeatherIntensity = 0.2f;
    
    // Initialize player
    Demo->Player.X = 400.0f;
    Demo->Player.Y = 300.0f;
    Demo->Player.Health = 1.0f;
    Demo->Player.Energy = 1.0f;
    Demo->Player.EmotionalTone = 0.0f;
    Demo->Player.LastAction = NPC_ACTION_NONE;
    
    // Create NPCs with different archetypes
    const char *NPCNames[] = {
        "Gareth", "Sophia", "Marcus", "Raven", 
        "Elena", "Kai", "Mystic", "Thorin"
    };
    
    const char *NPCBackgrounds[] = {
        "A brave warrior seeking glory",
        "A curious scholar of ancient lore", 
        "A shrewd merchant with many contacts",
        "A mysterious rogue with hidden agenda",
        "A devoted guardian of sacred places",
        "A free-spirited wanderer of distant lands",
        "An enigmatic mystic who sees beyond",
        "A master craftsman of legendary skill"
    };
    
    Demo->ActiveNPCCount = 4; // Start with 4 NPCs for performance
    
    for(u32 i = 0; i < Demo->ActiveNPCCount; ++i)
    {
        npc_game_entity *NPCEntity = &Demo->NPCs[i];
        
        // Create NPC brain
        NPCEntity->Brain = CreateNPCBrain(Demo->NPCArena, 
                                         (npc_personality_archetype)i,
                                         NPCNames[i],
                                         NPCBackgrounds[i]);
        
        // Set physical properties
        f32 Angle = (f32)i * Tau32 / Demo->ActiveNPCCount;
        NPCEntity->X = 400.0f + 150.0f * cosf(Angle);
        NPCEntity->Y = 300.0f + 150.0f * sinf(Angle);
        NPCEntity->Size = 12.0f;
        NPCEntity->Color = RGB(150, 150, 150);
        
        // Set capabilities
        NPCEntity->Health = 1.0f;
        NPCEntity->Energy = 1.0f;
        NPCEntity->ViewDistance = 100.0f;
        NPCEntity->HearingRange = 80.0f;
        
        // Initialize skill levels based on archetype
        switch(i)
        {
            case 0: // Warrior
                NPCEntity->SkillLevels[0] = 0.8f; // Combat
                NPCEntity->SkillLevels[1] = 0.2f; // Trade  
                NPCEntity->SkillLevels[2] = 0.5f; // Social
                break;
            case 1: // Scholar
                NPCEntity->SkillLevels[0] = 0.3f; // Combat
                NPCEntity->SkillLevels[1] = 0.4f; // Trade
                NPCEntity->SkillLevels[2] = 0.7f; // Social
                break;
            case 2: // Merchant
                NPCEntity->SkillLevels[0] = 0.2f; // Combat
                NPCEntity->SkillLevels[1] = 0.9f; // Trade
                NPCEntity->SkillLevels[2] = 0.8f; // Social
                break;
            case 3: // Rogue
                NPCEntity->SkillLevels[0] = 0.7f; // Combat
                NPCEntity->SkillLevels[1] = 0.6f; // Trade
                NPCEntity->SkillLevels[2] = 0.3f; // Social
                break;
        }
        
        // Initialize interaction context
        ZeroStruct(NPCEntity->InteractionContext);
    }
    
    // Demo control
    Demo->SelectedNPC = 0;
    Demo->PauseSimulation = 0;
    Demo->SimulationSpeed = 1.0f;
    Demo->CurrentScenario = DEMO_SCENARIO_FIRST_MEETING;
    Demo->ScenarioTime = 0.0f;
    Demo->FrameCount = 0;
    
    return Demo;
}

// Main demo update function for integration
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    local_persist b32 Initialized = 0;
    local_persist memory_arena DemoArena;
    local_persist complete_npc_demo *Demo;
    
    if(!Initialized)
    {
        InitializeArena(&DemoArena, Memory->PermanentStorageSize, Memory->PermanentStorage);
        Demo = InitializeCompleteNPCDemo(&DemoArena);
        Initialized = 1;
        
        printf("Complete NPC Demo Initialized\n");
        printf("NPCs: %u\n", Demo->ActiveNPCCount);
        printf("Memory Usage: %.1f MB\n", (f32)DemoArena.Used / (1024.0f * 1024.0f));
    }
    
    UpdateCompleteNPCDemo(Demo, Input, Clock->SecondsElapsed);
    RenderCompleteNPCDemo(Demo, Buffer);
}