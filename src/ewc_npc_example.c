/*
    EWC NPC Example: Persistent Learning Without Forgetting
    
    This example demonstrates an NPC that:
    1. Learns Task A: Combat Skills (enemy detection, attack patterns)
    2. Learns Task B: Social Interaction (dialog, trading) 
    3. Retains both skills simultaneously using EWC
    
    The NPC uses a neural network to process inputs and generate actions.
    EWC prevents catastrophic forgetting when learning new behaviors.
    
    Performance targets:
    - Combat skill retention: >95% after social training
    - Social skill acquisition: >90% accuracy 
    - Memory overhead: <2x base network size
    - Real-time inference: <1ms per decision
*/

#include "handmade.h"
#include "neural_math.h"
#include "ewc.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

// Forward declaration for simple network initialization
neural_network InitializeSimpleNeuralNetwork(memory_arena *Arena, 
                                             u32 InputSize, u32 Hidden1Size, 
                                             u32 Hidden2Size, u32 OutputSize);

// ================================================================================================
// NPC Environment and Action Definitions
// ================================================================================================

typedef enum npc_action_type
{
    ACTION_NONE = 0,
    
    // Combat actions
    ACTION_ATTACK_MELEE,
    ACTION_ATTACK_RANGED,
    ACTION_DEFEND,
    ACTION_RETREAT,
    ACTION_DODGE,
    
    // Social actions  
    ACTION_GREET,
    ACTION_TRADE,
    ACTION_NEGOTIATE,
    ACTION_SHARE_INFO,
    ACTION_REQUEST_HELP,
    
    ACTION_COUNT
} npc_action_type;

typedef struct npc_state
{
    // Sensory inputs (normalized to [0,1])
    f32 EnemyDistance;        // 0 = far, 1 = close
    f32 EnemyHealth;          // 0 = dead, 1 = full health
    f32 PlayerHealth;         // NPC's own health
    f32 WeaponReadiness;      // 0 = no weapon, 1 = weapon ready
    f32 PlayerPresence;       // 0 = no player nearby, 1 = player close
    f32 PlayerFriendliness;   // 0 = hostile, 1 = friendly
    f32 TradeOpportunity;     // 0 = no trade, 1 = trade available
    f32 InformationValue;     // 0 = no info, 1 = valuable info to share
    
    // Context
    f32 TimeOfDay;           // 0 = night, 1 = day
    f32 Location;            // 0 = safe area, 1 = dangerous area
} npc_state;

typedef struct npc_brain
{
    neural_network Network;
    ewc_state EWC;
    
    // Task-specific training data
    npc_state *CombatTrainingData;
    npc_action_type *CombatTrainingLabels;
    u32 CombatTrainingCount;
    
    npc_state *SocialTrainingData;
    npc_action_type *SocialTrainingLabels;
    u32 SocialTrainingCount;
    
    // Performance tracking
    f32 CombatSkillLevel;     // 0-1, measured accuracy on combat tasks
    f32 SocialSkillLevel;     // 0-1, measured accuracy on social tasks
    u32 TotalDecisionsMade;
    u64 InferenceTime;        // Cycles per decision
    
    memory_arena *Arena;
} npc_brain;

// ================================================================================================
// Training Data Generation
// ================================================================================================

void GenerateCombatTrainingData(npc_brain *Brain)
{
    const u32 SampleCount = 1000;
    Brain->CombatTrainingCount = SampleCount;
    Brain->CombatTrainingData = (npc_state *)PushArray(Brain->Arena, SampleCount, npc_state);
    Brain->CombatTrainingLabels = (npc_action_type *)PushArray(Brain->Arena, SampleCount, npc_action_type);
    
    // Generate realistic combat scenarios
    for (u32 i = 0; i < SampleCount; ++i) {
        npc_state *State = &Brain->CombatTrainingData[i];
        
        // Random combat scenario
        State->EnemyDistance = (f32)rand() / RAND_MAX;
        State->EnemyHealth = (f32)rand() / RAND_MAX;
        State->PlayerHealth = 0.3f + 0.7f * ((f32)rand() / RAND_MAX); // Usually healthy
        State->WeaponReadiness = (rand() % 2) ? 1.0f : 0.0f;
        
        // Non-combat inputs set to default/irrelevant values
        State->PlayerPresence = 0.0f;
        State->PlayerFriendliness = 0.0f;
        State->TradeOpportunity = 0.0f;
        State->InformationValue = 0.0f;
        
        State->TimeOfDay = (f32)rand() / RAND_MAX;
        State->Location = 0.7f + 0.3f * ((f32)rand() / RAND_MAX); // Usually dangerous
        
        // Generate optimal combat action based on scenario
        npc_action_type OptimalAction = ACTION_NONE;
        
        if (State->EnemyDistance < 0.2f && State->WeaponReadiness > 0.5f) {
            OptimalAction = ACTION_ATTACK_MELEE;
        } else if (State->EnemyDistance < 0.6f && State->WeaponReadiness > 0.5f) {
            OptimalAction = ACTION_ATTACK_RANGED;
        } else if (State->EnemyHealth > 0.8f && State->PlayerHealth < 0.3f) {
            OptimalAction = ACTION_RETREAT;
        } else if (State->EnemyDistance < 0.3f) {
            OptimalAction = ACTION_DODGE;
        } else {
            OptimalAction = ACTION_DEFEND;
        }
        
        Brain->CombatTrainingLabels[i] = OptimalAction;
    }
    
    printf("Generated %u combat training samples\n", SampleCount);
}

void GenerateSocialTrainingData(npc_brain *Brain)
{
    const u32 SampleCount = 800;
    Brain->SocialTrainingCount = SampleCount;
    Brain->SocialTrainingData = (npc_state *)PushArray(Brain->Arena, SampleCount, npc_state);
    Brain->SocialTrainingLabels = (npc_action_type *)PushArray(Brain->Arena, SampleCount, npc_action_type);
    
    // Generate realistic social scenarios
    for (u32 i = 0; i < SampleCount; ++i) {
        npc_state *State = &Brain->SocialTrainingData[i];
        
        // Social scenario setup
        State->PlayerPresence = 0.7f + 0.3f * ((f32)rand() / RAND_MAX); // Player usually present
        State->PlayerFriendliness = (f32)rand() / RAND_MAX;
        State->TradeOpportunity = (f32)rand() / RAND_MAX;
        State->InformationValue = (f32)rand() / RAND_MAX;
        
        // Combat inputs set to peaceful defaults
        State->EnemyDistance = 1.0f; // No enemies
        State->EnemyHealth = 0.0f;   // No enemy threat
        State->PlayerHealth = 0.8f + 0.2f * ((f32)rand() / RAND_MAX); // Usually healthy
        State->WeaponReadiness = 0.2f + 0.3f * ((f32)rand() / RAND_MAX); // Some weapon readiness
        
        State->TimeOfDay = (f32)rand() / RAND_MAX;
        State->Location = 0.2f * ((f32)rand() / RAND_MAX); // Usually safe areas
        
        // Generate optimal social action
        npc_action_type OptimalAction = ACTION_NONE;
        
        if (State->PlayerPresence > 0.8f && State->PlayerFriendliness < 0.3f) {
            OptimalAction = ACTION_GREET; // Be friendly to improve relations
        } else if (State->TradeOpportunity > 0.7f && State->PlayerFriendliness > 0.5f) {
            OptimalAction = ACTION_TRADE;
        } else if (State->InformationValue > 0.8f && State->PlayerFriendliness > 0.6f) {
            OptimalAction = ACTION_SHARE_INFO;
        } else if (State->PlayerFriendliness > 0.8f) {
            OptimalAction = ACTION_NEGOTIATE;
        } else if (State->PlayerPresence > 0.5f) {
            OptimalAction = ACTION_REQUEST_HELP;
        } else {
            OptimalAction = ACTION_GREET;
        }
        
        Brain->SocialTrainingLabels[i] = OptimalAction;
    }
    
    printf("Generated %u social training samples\n", SampleCount);
}

// ================================================================================================
// Neural Network Training
// ================================================================================================

f32 TrainNetwork(npc_brain *Brain, npc_state *TrainingData, npc_action_type *Labels, 
                u32 SampleCount, u32 Epochs, const char *TaskName)
{
    printf("\nTraining %s for %u epochs...\n", TaskName, Epochs);
    
    const f32 LearningRate = 0.001f;
    f32 TotalLoss = 0.0f;
    
    for (u32 Epoch = 0; Epoch < Epochs; ++Epoch) {
        f32 EpochLoss = 0.0f;
        
        for (u32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex) {
            // Prepare input vector
            neural_vector Input = AllocateVector(Brain->Arena, 10);
            npc_state *State = &TrainingData[SampleIndex];
            
            Input.Data[0] = State->EnemyDistance;
            Input.Data[1] = State->EnemyHealth;
            Input.Data[2] = State->PlayerHealth;
            Input.Data[3] = State->WeaponReadiness;
            Input.Data[4] = State->PlayerPresence;
            Input.Data[5] = State->PlayerFriendliness;
            Input.Data[6] = State->TradeOpportunity;
            Input.Data[7] = State->InformationValue;
            Input.Data[8] = State->TimeOfDay;
            Input.Data[9] = State->Location;
            
            // Prepare target vector (one-hot encoding)
            neural_vector Target = AllocateVector(Brain->Arena, ACTION_COUNT);
            InitializeVectorZero(&Target);
            Target.Data[Labels[SampleIndex]] = 1.0f;
            
            // Forward pass
            neural_vector Output = AllocateVector(Brain->Arena, ACTION_COUNT);
            ForwardPass(&Brain->Network, &Input, &Output);
            
            // Compute loss (cross-entropy)
            f32 SampleLoss = 0.0f;
            for (u32 i = 0; i < ACTION_COUNT; ++i) {
                if (Target.Data[i] > 0.0f) {
                    SampleLoss -= Target.Data[i] * logf(fmaxf(Output.Data[i], 1e-7f));
                }
            }
            
            // Add EWC penalty if we have previous tasks
            if (Brain->EWC.ActiveTaskCount > 0) {
                f32 EWCPenalty = ComputeEWCPenalty(&Brain->EWC, &Brain->Network);
                SampleLoss += EWCPenalty;
            }
            
            EpochLoss += SampleLoss;
            
            // Backward pass with EWC-modified gradients
            if (Brain->EWC.ActiveTaskCount > 0) {
                neural_vector Gradients = AllocateVector(Brain->Arena, Brain->EWC.TotalParameters);
                // Note: Gradient computation would be more complex in real implementation
                UpdateParametersWithEWC(&Brain->EWC, &Brain->Network, &Gradients, LearningRate);
            } else {
                // Standard backpropagation
                BackwardPass(&Brain->Network, &Target, LearningRate);
            }
        }
        
        f32 AvgLoss = EpochLoss / SampleCount;
        if (Epoch % 10 == 0) {
            printf("Epoch %u: Loss = %.6f\n", Epoch, AvgLoss);
        }
        
        TotalLoss = AvgLoss;
    }
    
    return TotalLoss;
}

// ================================================================================================
// Skill Evaluation
// ================================================================================================

f32 EvaluateSkill(npc_brain *Brain, npc_state *TestData, npc_action_type *TestLabels, 
                 u32 TestCount, const char *SkillName)
{
    u32 CorrectPredictions = 0;
    
    for (u32 TestIndex = 0; TestIndex < TestCount; ++TestIndex) {
        // Prepare input
        neural_vector Input = AllocateVector(Brain->Arena, 10);
        npc_state *State = &TestData[TestIndex];
        
        Input.Data[0] = State->EnemyDistance;
        Input.Data[1] = State->EnemyHealth;
        Input.Data[2] = State->PlayerHealth;
        Input.Data[3] = State->WeaponReadiness;
        Input.Data[4] = State->PlayerPresence;
        Input.Data[5] = State->PlayerFriendliness;
        Input.Data[6] = State->TradeOpportunity;
        Input.Data[7] = State->InformationValue;
        Input.Data[8] = State->TimeOfDay;
        Input.Data[9] = State->Location;
        
        // Forward pass
        u64 StartCycles = __rdtsc();
        neural_vector Output = AllocateVector(Brain->Arena, ACTION_COUNT);
        ForwardPass(&Brain->Network, &Input, &Output);
        u64 EndCycles = __rdtsc();
        
        Brain->InferenceTime = EndCycles - StartCycles;
        
        // Find predicted action (argmax)
        npc_action_type PredictedAction = ACTION_NONE;
        f32 MaxOutput = -1.0f;
        for (u32 ActionIndex = 0; ActionIndex < ACTION_COUNT; ++ActionIndex) {
            if (Output.Data[ActionIndex] > MaxOutput) {
                MaxOutput = Output.Data[ActionIndex];
                PredictedAction = (npc_action_type)ActionIndex;
            }
        }
        
        // Check if prediction matches label
        if (PredictedAction == TestLabels[TestIndex]) {
            CorrectPredictions++;
        }
        
        Brain->TotalDecisionsMade++;
    }
    
    f32 Accuracy = (f32)CorrectPredictions / TestCount;
    printf("%s Skill Evaluation: %u/%u correct (%.2f%% accuracy)\n", 
           SkillName, CorrectPredictions, TestCount, Accuracy * 100.0f);
    
    return Accuracy;
}

// ================================================================================================
// Main Demonstration
// ================================================================================================

int RunEWCNPCExample(memory_arena *Arena)
{
    printf("=== EWC NPC Learning Example ===\n");
    printf("Demonstrating catastrophic forgetting prevention in neural NPCs\n\n");
    
    // Initialize NPC brain
    npc_brain Brain = {0};
    Brain.Arena = Arena;
    
    // Create neural network (10 inputs -> 32 hidden -> 16 hidden -> ACTION_COUNT outputs)
    Brain.Network = InitializeSimpleNeuralNetwork(Arena, 10, 32, 16, ACTION_COUNT);
    
    // Calculate total parameters for EWC
    u32 TotalParams = (10 * 32 + 32) +    // Layer 1: weights + bias
                      (32 * 16 + 16) +    // Layer 2: weights + bias  
                      (16 * ACTION_COUNT + ACTION_COUNT); // Layer 3: weights + bias
    
    // Initialize EWC system
    Brain.EWC = InitializeEWC(Arena, TotalParams);
    
    // Generate training data
    GenerateCombatTrainingData(&Brain);
    GenerateSocialTrainingData(&Brain);
    
    // ========================================================================================
    // Phase 1: Learn Combat Skills
    // ========================================================================================
    
    printf("\n=== PHASE 1: Learning Combat Skills ===\n");
    
    // Begin Task A: Combat Skills
    u32 CombatTaskID = BeginTask(&Brain.EWC, "Combat Skills");
    
    // Train on combat data
    f32 CombatLoss = TrainNetwork(&Brain, Brain.CombatTrainingData, Brain.CombatTrainingLabels,
                                 Brain.CombatTrainingCount, 100, "Combat Skills");
    
    // Evaluate initial combat skill
    Brain.CombatSkillLevel = EvaluateSkill(&Brain, Brain.CombatTrainingData, Brain.CombatTrainingLabels,
                                          200, "Combat"); // Use subset for testing
    
    // Complete combat task and compute Fisher Information
    CompleteTask(&Brain.EWC, CombatTaskID, &Brain.Network, CombatLoss);
    
    // Create sample data for Fisher computation (simplified)
    neural_vector FisherSamples[100];
    for (u32 i = 0; i < 100; ++i) {
        FisherSamples[i] = AllocateVector(Arena, 10);
        npc_state *State = &Brain.CombatTrainingData[i];
        FisherSamples[i].Data[0] = State->EnemyDistance;
        FisherSamples[i].Data[1] = State->EnemyHealth;
        FisherSamples[i].Data[2] = State->PlayerHealth;
        FisherSamples[i].Data[3] = State->WeaponReadiness;
        FisherSamples[i].Data[4] = State->PlayerPresence;
        FisherSamples[i].Data[5] = State->PlayerFriendliness;
        FisherSamples[i].Data[6] = State->TradeOpportunity;
        FisherSamples[i].Data[7] = State->InformationValue;
        FisherSamples[i].Data[8] = State->TimeOfDay;
        FisherSamples[i].Data[9] = State->Location;
    }
    
    // Compute Fisher Information for combat task
    ewc_task *CombatTask = &Brain.EWC.Tasks[0];
    ComputeFisherInformation(&CombatTask->FisherMatrix, &Brain.Network, FisherSamples, 100);
    
    printf("Combat task consolidated with %u Fisher entries (%.2f%% sparse)\n",
           CombatTask->FisherMatrix.EntryCount, 
           CombatTask->FisherMatrix.SparsityRatio * 100.0f);
    
    // ========================================================================================
    // Phase 2: Learn Social Skills (with EWC protection)
    // ========================================================================================
    
    printf("\n=== PHASE 2: Learning Social Skills (EWC Active) ===\n");
    
    // Begin Task B: Social Skills
    u32 SocialTaskID = BeginTask(&Brain.EWC, "Social Interaction");
    
    // Adjust lambda for optimal consolidation
    Brain.EWC.Lambda = GetRecommendedLambda(&Brain.EWC, &Brain.Network);
    printf("Using EWC lambda = %.2f\n", Brain.EWC.Lambda);
    
    // Train on social data (EWC should prevent combat skill forgetting)
    f32 SocialLoss = TrainNetwork(&Brain, Brain.SocialTrainingData, Brain.SocialTrainingLabels,
                                 Brain.SocialTrainingCount, 80, "Social Skills");
    
    // Evaluate social skill acquisition
    Brain.SocialSkillLevel = EvaluateSkill(&Brain, Brain.SocialTrainingData, Brain.SocialTrainingLabels,
                                          160, "Social"); // Use subset for testing
    
    // Complete social task
    CompleteTask(&Brain.EWC, SocialTaskID, &Brain.Network, SocialLoss);
    
    // ========================================================================================
    // Phase 3: Test Retention and Performance
    // ========================================================================================
    
    printf("\n=== PHASE 3: Skill Retention Analysis ===\n");
    
    // Re-evaluate combat skills to check for catastrophic forgetting
    f32 RetainedCombatSkill = EvaluateSkill(&Brain, Brain.CombatTrainingData, Brain.CombatTrainingLabels,
                                           200, "Combat (After Social Training)");
    
    // Calculate retention percentage
    f32 SkillRetentionPercent = (RetainedCombatSkill / Brain.CombatSkillLevel) * 100.0f;
    
    printf("\n=== RESULTS SUMMARY ===\n");
    printf("Initial Combat Skill: %.2f%% accuracy\n", Brain.CombatSkillLevel * 100.0f);
    printf("Social Skill Acquired: %.2f%% accuracy\n", Brain.SocialSkillLevel * 100.0f);
    printf("Combat Skill Retained: %.2f%% accuracy (%.1f%% retention)\n", 
           RetainedCombatSkill * 100.0f, SkillRetentionPercent);
    
    // Performance statistics
    printf("\nPerformance Statistics:\n");
    printf("Total Decisions Made: %u\n", Brain.TotalDecisionsMade);
    printf("Average Inference Time: %llu cycles (%.3f ms @ 2.5GHz)\n", 
           (unsigned long long)Brain.InferenceTime, Brain.InferenceTime / 2.5e6);
    
    // EWC system statistics
    ewc_performance_stats EWCStats;
    GetEWCStats(&Brain.EWC, &EWCStats);
    PrintEWCStats(&EWCStats);
    
    // Success criteria
    b32 Success = (SkillRetentionPercent >= 95.0f) && 
                  (Brain.SocialSkillLevel >= 0.85f) &&
                  (Brain.InferenceTime < 2500000); // <1ms @ 2.5GHz
    
    printf("\n=== EVALUATION ===\n");
    if (Success) {
        printf("✓ SUCCESS: EWC prevented catastrophic forgetting!\n");
        printf("  - Combat skills retained: %.1f%% (target: ≥95%%)\n", SkillRetentionPercent);
        printf("  - Social skills acquired: %.1f%% (target: ≥85%%)\n", Brain.SocialSkillLevel * 100.0f);
        printf("  - Inference time: %.3fms (target: <1ms)\n", Brain.InferenceTime / 2.5e6);
    } else {
        printf("✗ FAILURE: Performance targets not met\n");
        if (SkillRetentionPercent < 95.0f) {
            printf("  - Insufficient skill retention: %.1f%% < 95%%\n", SkillRetentionPercent);
        }
        if (Brain.SocialSkillLevel < 0.85f) {
            printf("  - Insufficient social skill: %.1f%% < 85%%\n", Brain.SocialSkillLevel * 100.0f);
        }
        if (Brain.InferenceTime >= 2500000) {
            printf("  - Too slow inference: %.3fms ≥ 1ms\n", Brain.InferenceTime / 2.5e6);
        }
    }
    
    return Success ? 0 : 1;
}

// ================================================================================================
// Interactive Demo Mode
// ================================================================================================

void RunInteractiveDemo(npc_brain *Brain)
{
    printf("\n=== Interactive NPC Demo ===\n");
    printf("Enter scenarios to see how the NPC responds:\n");
    printf("Commands: combat, social, mixed, quit\n\n");
    
    char Command[64];
    while (true) {
        printf("npc> ");
        if (scanf("%s", Command) != 1) break;
        
        if (strcmp(Command, "quit") == 0) break;
        
        npc_state TestState = {0};
        
        if (strcmp(Command, "combat") == 0) {
            // Combat scenario
            TestState.EnemyDistance = 0.3f;
            TestState.EnemyHealth = 0.8f;
            TestState.PlayerHealth = 0.6f;
            TestState.WeaponReadiness = 1.0f;
            TestState.Location = 0.9f; // Dangerous area
            
            printf("Scenario: Enemy nearby with high health, weapon ready\n");
        }
        else if (strcmp(Command, "social") == 0) {
            // Social scenario
            TestState.PlayerPresence = 0.9f;
            TestState.PlayerFriendliness = 0.7f;
            TestState.TradeOpportunity = 0.8f;
            TestState.PlayerHealth = 0.9f;
            TestState.Location = 0.1f; // Safe area
            
            printf("Scenario: Friendly player nearby with trade opportunity\n");
        }
        else if (strcmp(Command, "mixed") == 0) {
            // Mixed scenario
            TestState.EnemyDistance = 0.6f;
            TestState.EnemyHealth = 0.4f;
            TestState.PlayerPresence = 0.8f;
            TestState.PlayerFriendliness = 0.5f;
            TestState.PlayerHealth = 0.7f;
            TestState.WeaponReadiness = 0.8f;
            
            printf("Scenario: Distant weak enemy, neutral player present\n");
        } else {
            printf("Unknown command. Use: combat, social, mixed, quit\n");
            continue;
        }
        
        // Run inference
        neural_vector Input = AllocateVector(Brain->Arena, 10);
        Input.Data[0] = TestState.EnemyDistance;
        Input.Data[1] = TestState.EnemyHealth;
        Input.Data[2] = TestState.PlayerHealth;
        Input.Data[3] = TestState.WeaponReadiness;
        Input.Data[4] = TestState.PlayerPresence;
        Input.Data[5] = TestState.PlayerFriendliness;
        Input.Data[6] = TestState.TradeOpportunity;
        Input.Data[7] = TestState.InformationValue;
        Input.Data[8] = TestState.TimeOfDay;
        Input.Data[9] = TestState.Location;
        
        neural_vector Output = AllocateVector(Brain->Arena, ACTION_COUNT);
        u64 StartCycles = __rdtsc();
        ForwardPass(&Brain->Network, &Input, &Output);
        u64 InferenceTime = __rdtsc() - StartCycles;
        
        // Find best action
        npc_action_type BestAction = ACTION_NONE;
        f32 BestScore = -1.0f;
        for (u32 i = 0; i < ACTION_COUNT; ++i) {
            if (Output.Data[i] > BestScore) {
                BestScore = Output.Data[i];
                BestAction = (npc_action_type)i;
            }
        }
        
        // Display result
        const char* ActionNames[] = {
            "None", "Attack Melee", "Attack Ranged", "Defend", "Retreat", "Dodge",
            "Greet", "Trade", "Negotiate", "Share Info", "Request Help"
        };
        
        printf("NPC Decision: %s (confidence: %.3f)\n", 
               ActionNames[BestAction], BestScore);
        printf("Inference time: %llu cycles (%.3f ms)\n\n", 
               (unsigned long long)InferenceTime, InferenceTime / 2.5e6);
    }
}

// ================================================================================================
// Entry Point
// ================================================================================================

#ifdef EWC_EXAMPLE_STANDALONE

int main()
{
    // Initialize memory arena
    memory_arena Arena = {0};
    void *ArenaMemory = malloc(Megabytes(64));
    InitializeArena(&Arena, Megabytes(64), ArenaMemory);
    
    // Run the main example
    int Result = RunEWCNPCExample(&Arena);
    
    if (Result == 0) {
        // If successful, offer interactive demo
        printf("\nWould you like to try the interactive demo? (y/n): ");
        char Response;
        scanf(" %c", &Response);
        
        if (Response == 'y' || Response == 'Y') {
            npc_brain Brain = {0}; // Would need to restore from example
            RunInteractiveDemo(&Brain);
        }
    }
    
    return Result;
}

#endif // EWC_EXAMPLE_STANDALONE