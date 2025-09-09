#ifndef NPC_BRAIN_H
#define NPC_BRAIN_H

#include "handmade.h"
#include "memory.h"
#include "neural_math.h"
#include "dnc.h"
#include "lstm.h"
#include "ewc.h"
#include "neural_debug.h"

/*
    NPC Brain Architecture - Complete Neural Intelligence System
    
    This is the final integration bringing together:
    1. LSTM Controller - Sequential decision making and attention control
    2. DNC Memory System - Persistent, associative long-term memory
    3. EWC Consolidation - Learning without catastrophic forgetting
    4. Sensory Processing - Vision, hearing, social context understanding
    5. Emotional System - Persistent relationships and personality
    
    Philosophy:
    - Every NPC is a complete neural agent with persistent memory
    - Sub-1ms inference for real-time gameplay at 60fps
    - Deterministic for replay systems and debugging
    - Zero allocations during gameplay - all memory pre-allocated
    - Full neural activity visualization for development
    
    Performance Targets:
    - < 1ms total update time per NPC
    - Support 10+ simultaneous NPCs 
    - < 10MB memory per NPC
    - 95%+ sparsity for energy efficiency
    - Deterministic behavior for network sync
    
    Memory Architecture:
    - Sensory buffer: 512 floats (vision, audio, social)
    - LSTM hidden state: 256 dimensions
    - DNC memory matrix: 128 locations x 64 dimensions
    - EWC Fisher info: matches network parameters
    - Emotional state: 32 persistent values
    - Working memory: 128 floats for current context
    
    NPC Capabilities:
    1. Remembers all past interactions across game sessions
    2. Forms emotional bonds that strengthen over time  
    3. Learns player preferences without forgetting old ones
    4. Develops unique personality from interaction patterns
    5. Makes contextual references to shared experiences
    6. Learns new skills while retaining old abilities
*/

// Forward declarations
typedef struct neural_debug_state neural_debug_state;

// NPC sensory input channels
typedef enum npc_sensory_channel
{
    SENSORY_VISION_START = 0,           // Visual field: 16x16 grid
    SENSORY_VISION_END = 255,
    
    SENSORY_AUDIO_START = 256,          // Audio spectrum analysis
    SENSORY_AUDIO_END = 287,
    
    SENSORY_SOCIAL_START = 288,         // Player relationship signals
    SENSORY_SOCIAL_END = 319,
    
    SENSORY_INTERNAL_START = 320,       // Hunger, energy, goals
    SENSORY_INTERNAL_END = 351,
    
    SENSORY_CONTEXT_START = 352,        // Location, time, situation
    SENSORY_CONTEXT_END = 511,
    
    SENSORY_TOTAL_CHANNELS = 512
} npc_sensory_channel;

// NPC action categories
typedef enum npc_action_type
{
    NPC_ACTION_NONE = 0,
    
    // Movement actions
    NPC_ACTION_MOVE_NORTH,
    NPC_ACTION_MOVE_SOUTH, 
    NPC_ACTION_MOVE_EAST,
    NPC_ACTION_MOVE_WEST,
    
    // Social actions
    NPC_ACTION_GREET_FRIENDLY,
    NPC_ACTION_GREET_NEUTRAL,
    NPC_ACTION_GREET_HOSTILE,
    NPC_ACTION_TELL_STORY,
    NPC_ACTION_ASK_QUESTION,
    NPC_ACTION_EXPRESS_EMOTION,
    
    // Combat actions
    NPC_ACTION_ATTACK_MELEE,
    NPC_ACTION_ATTACK_RANGED,
    NPC_ACTION_DEFEND,
    NPC_ACTION_RETREAT,
    
    // Trade actions
    NPC_ACTION_OFFER_TRADE,
    NPC_ACTION_ACCEPT_TRADE,
    NPC_ACTION_DECLINE_TRADE,
    
    // Memory actions
    NPC_ACTION_RECALL_MEMORY,
    NPC_ACTION_STORE_MEMORY,
    NPC_ACTION_SHARE_EXPERIENCE,
    
    NPC_ACTION_COUNT
} npc_action_type;

// Emotional state dimensions
typedef enum npc_emotion_type
{
    EMOTION_TRUST = 0,                  // Trust in player (0-1)
    EMOTION_FEAR,                       // Fear level (0-1) 
    EMOTION_ANGER,                      // Anger at player (0-1)
    EMOTION_JOY,                        // Happiness/contentment (0-1)
    EMOTION_CURIOSITY,                  // Interest in learning (0-1)
    EMOTION_RESPECT,                    // Respect for player skill (0-1)
    EMOTION_AFFECTION,                  // Personal attachment (0-1)
    EMOTION_LONELINESS,                 // Need for companionship (0-1)
    
    // Personality traits (more stable)
    PERSONALITY_EXTRAVERSION,           // Social vs solitary (-1 to 1)
    PERSONALITY_AGREEABLENESS,          // Cooperative vs competitive (-1 to 1)
    PERSONALITY_CONSCIENTIOUSNESS,      // Organized vs spontaneous (-1 to 1)
    PERSONALITY_NEUROTICISM,            // Emotional stability (-1 to 1)
    PERSONALITY_OPENNESS,               // Open to new experiences (-1 to 1)
    
    // Relationship history  
    HISTORY_POSITIVE_INTERACTIONS,      // Count of positive encounters
    HISTORY_NEGATIVE_INTERACTIONS,      // Count of negative encounters  
    HISTORY_SHARED_VICTORIES,           // Battles fought together
    HISTORY_BETRAYALS,                  // Times player broke trust
    HISTORY_GIFTS_RECEIVED,             // Gifts from player
    HISTORY_FAVORS_DONE,                // Favors done for player
    HISTORY_TIME_TOGETHER,              // Total interaction time
    
    // Current context
    CONTEXT_LOCATION_FAMILIARITY,       // How well NPC knows current area
    CONTEXT_SOCIAL_SETTING,             // Formal, casual, private, public
    CONTEXT_THREAT_LEVEL,               // Perceived danger in environment
    CONTEXT_PLAYER_MOOD,                // Perceived player emotional state
    CONTEXT_RELATIONSHIP_STATUS,        // Current relationship phase
    CONTEXT_SHARED_GOAL_PROGRESS,       // Progress on mutual objectives
    
    // Memories and associations
    MEMORY_FIRST_MEETING_QUALITY,       // How first encounter went
    MEMORY_LAST_INTERACTION_QUALITY,    // Most recent interaction quality
    MEMORY_STRONGEST_POSITIVE_MEMORY,   // Best shared experience
    MEMORY_STRONGEST_NEGATIVE_MEMORY,   // Worst shared experience
    MEMORY_PLAYER_COMBAT_STYLE,         // Learned combat patterns
    MEMORY_PLAYER_PREFERENCES,          // Learned likes/dislikes
    
    NPC_EMOTION_COUNT = 32
} npc_emotion_type;

// NPC personality archetype (affects initial emotional weights)
typedef enum npc_personality_archetype  
{
    NPC_ARCHETYPE_WARRIOR,              // Brave, direct, loyal
    NPC_ARCHETYPE_SCHOLAR,              // Curious, analytical, patient
    NPC_ARCHETYPE_MERCHANT,             // Social, opportunistic, practical
    NPC_ARCHETYPE_ROGUE,                // Independent, clever, mistrustful
    NPC_ARCHETYPE_GUARDIAN,             // Protective, dutiful, conservative
    NPC_ARCHETYPE_WANDERER,             // Adventurous, free-spirited, restless
    NPC_ARCHETYPE_MYSTIC,               // Intuitive, philosophical, mysterious
    NPC_ARCHETYPE_CRAFTSMAN,            // Methodical, perfectionist, humble
    
    NPC_ARCHETYPE_COUNT
} npc_personality_archetype;

// Memory consolidation importance for EWC
typedef enum npc_memory_importance
{
    MEMORY_IMPORTANCE_CRITICAL = 0,     // Core personality, never forget
    MEMORY_IMPORTANCE_HIGH,             // Important relationships/events  
    MEMORY_IMPORTANCE_MEDIUM,           // Useful skills and knowledge
    MEMORY_IMPORTANCE_LOW,              // Recent experiences, can fade
    
    MEMORY_IMPORTANCE_COUNT
} npc_memory_importance;

// Complete sensory input state
typedef struct npc_sensory_input
{
    // Raw sensory channels
    f32 Channels[SENSORY_TOTAL_CHANNELS];
    
    // Vision processing (16x16 visual field)
    f32 VisualField[16][16];            // Simplified visual representation
    f32 PlayerVisible;                   // Is player in visual field (0-1)
    f32 PlayerDistance;                  // Distance to player (normalized)
    f32 PlayerFacing;                    // Angle player is facing toward NPC
    
    // Audio processing
    f32 AudioSpectrum[32];              // Frequency analysis of environment
    f32 PlayerSpeaking;                 // Is player currently speaking
    f32 CombatSounds;                   // Combat audio in environment
    f32 AmbientThreatLevel;             // Perceived danger from audio
    
    // Social context
    f32 PlayerEmotionalState;           // Perceived player emotion
    f32 ConversationContext;            // Type of current interaction
    f32 SocialPressure;                 // Presence of other NPCs/players
    f32 IntimacyLevel;                  // Privacy of current setting
    
    // Internal state
    f32 Hunger;                         // Physical needs
    f32 Energy;                         // Tiredness level
    f32 Health;                         // Physical condition
    f32 CurrentGoalPriority;            // Importance of active goal
    
    // Environmental context
    f32 LocationType;                   // Inn, wilderness, dungeon, etc
    f32 TimeOfDay;                      // Morning, noon, evening, night
    f32 Weather;                        // Environmental conditions
    f32 Familiarity;                    // How well NPC knows this place
} npc_sensory_input;

// NPC decision output
typedef struct npc_action_output
{
    // Primary action selection
    npc_action_type PrimaryAction;
    f32 ActionConfidence;               // Certainty in this choice (0-1)
    f32 ActionIntensity;                // How strongly to perform action (0-1)
    
    // Movement vector
    f32 MovementX, MovementY;           // Desired movement direction
    f32 MovementSpeed;                  // Movement speed multiplier
    
    // Communication output
    char SpeechText[256];               // What NPC says (if speaking)
    f32 SpeechEmotionalTone;            // Emotional coloring of speech
    npc_emotion_type DominantEmotion;   // Primary emotion being expressed
    
    // Attention and memory control
    f32 AttentionWeights[SENSORY_TOTAL_CHANNELS];  // What to focus on
    f32 MemoryStoreSignal;              // Should current experience be stored
    f32 MemoryRecallQuery[DNC_MEMORY_VECTOR_SIZE]; // What to recall from memory
    
    // Internal state changes
    f32 EmotionalStateChanges[NPC_EMOTION_COUNT]; // Updates to emotional state
    f32 LearningRate;                   // How fast to learn from this experience
    npc_memory_importance MemoryImportance; // How important is this moment
} npc_action_output;

// Complete NPC neural brain
typedef struct npc_brain
{
    // Neural architecture components
    lstm_network *Controller;           // Main decision-making LSTM
    dnc_system *Memory;                 // Long-term associative memory
    ewc_state *Consolidation;           // Learning without forgetting
    
    // Current state
    npc_sensory_input CurrentInput;     // Current sensory state
    npc_action_output CurrentOutput;    // Current action decision
    
    // Emotional and personality state
    f32 EmotionalState[NPC_EMOTION_COUNT];      // Current emotional values
    f32 BasePersonality[NPC_EMOTION_COUNT];     // Stable personality traits
    npc_personality_archetype Archetype;        // Personality template
    
    // Working memory and context
    f32 WorkingMemory[128];             // Short-term context buffer
    f32 AttentionState[SENSORY_TOTAL_CHANNELS]; // Current attention weights
    f32 LongTermContext[DNC_MEMORY_VECTOR_SIZE]; // Context from long-term memory
    
    // Learning and adaptation
    f32 LearningHistory[1024];          // Recent learning experiences
    u32 LearningHistoryIndex;           // Circular buffer index
    f32 AdaptationRate;                 // How quickly NPC adapts to new experiences
    u32 TotalInteractions;              // Lifetime interaction count
    
    // Identity and persistence
    u32 NPCID;                          // Unique identifier for this NPC
    char Name[64];                      // NPC's name
    char Background[256];               // Brief personality description
    u64 CreationTime;                   // When this NPC was first created
    u64 LastInteractionTime;            // Last player interaction timestamp
    
    // Performance and debugging
    f32 LastUpdateTimeMs;               // Performance monitoring
    u32 DebugVisualizationEnabled;      // Show neural activity
    f32 InferenceStats[16];             // Performance counters
    
    // Memory arena for NPC-specific allocations
    memory_arena *NPCArena;             // Persistent NPC memory
    memory_pool *TempPool;              // Per-frame temporary allocations
} npc_brain;

// NPC interaction context for player communication
typedef struct npc_interaction_context
{
    // Conversation state
    b32 InConversation;                 // Currently talking to player
    f32 ConversationDuration;           // How long have they been talking
    npc_action_type LastPlayerAction;   // What did player just do
    f32 PlayerEmotionalTone;            // Perceived player emotion
    
    // Shared history
    u32 PreviousConversations;          // Number of past conversations
    f32 RelationshipProgression;        // How relationship has evolved
    f32 TrustLevel;                     // Current trust in player
    f32 IntimacyLevel;                  // How close they feel to player
    
    // Current situation
    f32 ThreatLevel;                    // Perceived danger in environment
    b32 PrivateSetting;                 // Are they alone with player
    f32 UrgencyLevel;                   // How urgent is current situation
    f32 SharedGoalAlignment;            // Do they share player's current goal
    
    // Memory recall context  
    f32 RelevantMemories[8];            // Most relevant past experiences
    char RecentSharedExperience[256];   // Most recent thing they did together
    f32 MemoryEmotionalColoring;        // Emotional tone of recalled memories
} npc_interaction_context;

// NPC learning experience for EWC consolidation
typedef struct npc_learning_experience
{
    // Experience classification
    enum {
        LEARNING_SOCIAL_INTERACTION,
        LEARNING_COMBAT_ENCOUNTER, 
        LEARNING_TRADE_NEGOTIATION,
        LEARNING_EXPLORATION_DISCOVERY,
        LEARNING_SKILL_PRACTICE,
        LEARNING_EMOTIONAL_EVENT
    } Type;
    
    // Experience data
    f32 InputState[SENSORY_TOTAL_CHANNELS];     // What was happening
    f32 ActionTaken[NPC_ACTION_COUNT];          // What NPC chose to do
    f32 Outcome[32];                            // How it turned out
    f32 EmotionalImpact[NPC_EMOTION_COUNT];     // Emotional consequences
    
    // Learning metadata
    f32 Importance;                     // How important was this experience (0-1)
    f32 Novelty;                        // How new/surprising was this (0-1)
    f32 Success;                        // How well did NPC's actions work (0-1)
    u64 Timestamp;                      // When did this happen
    
    // EWC consolidation data
    f32 ParameterSnapshot[LSTM_MAX_PARAMETERS]; // Network state before learning
    f32 FisherInformation[LSTM_MAX_PARAMETERS]; // Parameter importance
    f32 ConsolidationWeight;            // How strongly to protect this memory
} npc_learning_experience;

// Complete NPC system for multiple NPCs
typedef struct npc_system  
{
    // Active NPCs
    npc_brain NPCs[16];                 // Maximum simultaneous NPCs
    u32 ActiveNPCCount;                 // Number of currently active NPCs
    b32 NPCActive[16];                  // Which NPC slots are in use
    
    // Shared neural processing resources
    memory_arena *SystemArena;          // Shared NPC system memory
    neural_debug_state *DebugState;     // Visualization system
    
    // Global NPC settings
    f32 GlobalLearningRate;             // System-wide learning rate
    f32 GlobalAttentionDecay;           // How fast attention fades
    f32 GlobalMemoryThreshold;          // Minimum importance for memory storage
    b32 DeterministicMode;              // Consistent behavior for replay
    
    // Performance monitoring
    f32 TotalNPCUpdateTime;             // Time spent updating all NPCs
    f32 AverageNPCUpdateTime;           // Average per-NPC update time
    u32 NPCUpdatesThisFrame;            // Number of NPCs updated this frame
    f32 SystemCPUUsagePercent;          // CPU percentage used by NPC system
    
    // Save/load support
    char SaveFilePath[256];             // Where to save NPC states
    b32 AutoSave;                       // Automatically save NPC states
    f32 SaveInterval;                   // Time between auto-saves
    f32 TimeSinceLastSave;              // Seconds since last save
} npc_system;

// =============================================================================
// NPC BRAIN CORE FUNCTIONS
// =============================================================================

// Initialization and lifecycle
npc_brain *CreateNPCBrain(memory_arena *Arena, 
                         npc_personality_archetype Archetype,
                         const char *Name,
                         const char *Background);
void InitializeNPCBrain(npc_brain *NPC, 
                       npc_personality_archetype Archetype);
void ShutdownNPCBrain(npc_brain *NPC);

// Main update cycle (must be < 1ms)
void UpdateNPCBrain(npc_brain *NPC, 
                   npc_sensory_input *Input,
                   npc_interaction_context *Context,
                   f32 DeltaTime);

// Sensory processing
void ProcessNPCSensoryInput(npc_brain *NPC, 
                           npc_sensory_input *Input);
void UpdateNPCAttention(npc_brain *NPC, 
                       npc_sensory_input *Input);
void ExtractNPCContext(npc_brain *NPC, 
                      npc_sensory_input *Input);

// Decision making and action selection
void ComputeNPCDecision(npc_brain *NPC, 
                       npc_action_output *Output);
npc_action_type SelectNPCAction(npc_brain *NPC, 
                               f32 *ActionProbabilities);
void GenerateNPCSpeech(npc_brain *NPC, 
                      npc_action_output *Output,
                      npc_interaction_context *Context);

// Memory and learning
void StoreNPCExperience(npc_brain *NPC, 
                       npc_learning_experience *Experience);
void RecallNPCMemories(npc_brain *NPC, 
                      f32 *QueryVector,
                      f32 *RecalledMemories);
void ConsolidateNPCLearning(npc_brain *NPC, 
                           npc_learning_experience *Experience);

// Emotional state management
void UpdateNPCEmotions(npc_brain *NPC, 
                      npc_interaction_context *Context,
                      f32 DeltaTime);
f32 ComputeNPCEmotionalResponse(npc_brain *NPC, 
                               npc_emotion_type Emotion,
                               f32 Stimulus);
void ApplyNPCPersonalityBias(npc_brain *NPC);

// =============================================================================
// NPC SYSTEM MANAGEMENT
// =============================================================================

// System initialization
npc_system *InitializeNPCSystem(memory_arena *Arena,
                               neural_debug_state *DebugState);
void ShutdownNPCSystem(npc_system *System);

// NPC lifecycle in system
u32 SpawnNPC(npc_system *System,
            npc_personality_archetype Archetype, 
            const char *Name,
            const char *Background,
            f32 X, f32 Y);
void RemoveNPC(npc_system *System, u32 NPCID);
npc_brain *GetNPCByID(npc_system *System, u32 NPCID);

// System update
void UpdateNPCSystem(npc_system *System,
                    game_input *Input,
                    f32 DeltaTime);
void UpdateAllNPCs(npc_system *System, f32 DeltaTime);

// Save/load system
void SaveNPCStates(npc_system *System, const char *FilePath);
void LoadNPCStates(npc_system *System, const char *FilePath);
void AutoSaveNPCs(npc_system *System, f32 DeltaTime);

// =============================================================================
// PLAYER INTERACTION INTERFACE
// =============================================================================

// Player-NPC interaction
void BeginNPCConversation(npc_brain *NPC, npc_interaction_context *Context);
void ProcessPlayerInput(npc_brain *NPC, 
                       npc_interaction_context *Context,
                       const char *PlayerMessage);
void EndNPCConversation(npc_brain *NPC, npc_interaction_context *Context);

// Contextual memory queries
void AskNPCAboutTopic(npc_brain *NPC, const char *Topic, char *Response);
void AskNPCAboutPastEvent(npc_brain *NPC, const char *Event, char *Response);
void GetNPCOpinionOfPlayer(npc_brain *NPC, char *Opinion);

// Relationship management
f32 GetNPCRelationshipLevel(npc_brain *NPC);
void ModifyNPCRelationship(npc_brain *NPC, f32 Change);
b32 DoesNPCTrustPlayer(npc_brain *NPC);
b32 DoesNPCLikePlayer(npc_brain *NPC);

// =============================================================================
// VISUALIZATION AND DEBUGGING
// =============================================================================

// Debug visualization
void RenderNPCBrainDebug(npc_brain *NPC, 
                        game_offscreen_buffer *Buffer,
                        neural_debug_state *DebugState);
void RenderNPCEmotionalState(npc_brain *NPC,
                           game_offscreen_buffer *Buffer,
                           i32 X, i32 Y);
void RenderNPCMemoryMatrix(npc_brain *NPC,
                          game_offscreen_buffer *Buffer,
                          i32 X, i32 Y);
void RenderNPCDecisionTree(npc_brain *NPC,
                          game_offscreen_buffer *Buffer,
                          i32 X, i32 Y);

// Performance monitoring
void ProfileNPCUpdate(npc_brain *NPC, f32 UpdateTimeMs);
void GetNPCPerformanceStats(npc_brain *NPC, f32 *Stats);
void PrintNPCDiagnostics(npc_brain *NPC);

// Development tools
void SimulateNPCScenario(npc_brain *NPC, const char *ScenarioName);
void RunNPCPersonalityTest(npc_brain *NPC);
void ValidateNPCNeuralState(npc_brain *NPC);

// =============================================================================
// PERFORMANCE OPTIMIZATION SETTINGS
// =============================================================================

// Compile-time optimization flags
#define NPC_USE_SIMD_PROCESSING 1       // Use SIMD for neural computation
#define NPC_ENABLE_MEMORY_POOLING 1     // Use memory pools for allocations
#define NPC_ENABLE_DETERMINISTIC_MODE 1 // Support deterministic execution
#define NPC_ENABLE_DEBUG_VISUALIZATION 1 // Include visualization code

// Runtime performance tuning
#define NPC_MAX_UPDATE_TIME_MS 0.8f     // Maximum time budget per NPC update
#define NPC_TARGET_SPARSITY 0.95f       // Target neural activation sparsity
#define NPC_MEMORY_CONSOLIDATION_BATCH_SIZE 8 // How many memories to consolidate per frame

// Memory usage limits
#define NPC_MAX_MEMORY_PER_NPC Megabytes(10)  // Memory budget per NPC
#define NPC_MAX_TOTAL_NPC_MEMORY Megabytes(128) // Total system NPC memory

// Default archetype personality templates
extern f32 NPCPersonalityTemplates[NPC_ARCHETYPE_COUNT][NPC_EMOTION_COUNT];

#endif // NPC_BRAIN_H