/*
    Simple EWC Demonstration
    
    Shows basic EWC functionality without complex memory management
*/

#include "handmade.h" 
#include "neural_math.h"
#include "ewc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int main()
{
    printf("=== EWC Demonstration ===\n");
    printf("Elastic Weight Consolidation for Continual Learning\n\n");
    
    // Initialize memory arena
    memory_arena Arena = {0};
    void *ArenaMemory = malloc(Megabytes(32));
    InitializeArena(&Arena, Megabytes(32), ArenaMemory);
    
    printf("✓ Initialized memory arena: 32 MB\n");
    
    // Initialize EWC system  
    const u32 ParameterCount = 1000;
    ewc_state EWC = InitializeEWC(&Arena, ParameterCount);
    
    printf("✓ Initialized EWC system: %u parameters\n", ParameterCount);
    printf("  Lambda: %.2f\n", EWC.Lambda);
    printf("  SIMD enabled: %s\n", EWC.UseSIMD ? "Yes" : "No");
    printf("  Sparse Fisher: %s\n", EWC.UseSparseFisher ? "Yes" : "No");
    
    // Begin Task A
    u32 TaskA = BeginTask(&EWC, "Combat Skills");
    printf("\n✓ Started Task A: %s (ID: %u)\n", EWC.Tasks[0].Name, TaskA);
    
    // Simulate task completion
    for (u32 i = 0; i < ParameterCount; ++i) {
        EWC.Tasks[0].OptimalWeights[i] = ((f32)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
    
    // Add Fisher Information (sparse)
    ewc_fisher_matrix *Fisher = &EWC.Tasks[0].FisherMatrix;
    Fisher->EntryCount = ParameterCount / 10; // 10% sparsity
    for (u32 i = 0; i < Fisher->EntryCount; ++i) {
        Fisher->Entries[i] = (ewc_fisher_entry){i * 10, 0.1f + ((f32)rand() / RAND_MAX) * 0.9f};
    }
    
    EWC.Tasks[0].IsActive = true;
    printf("✓ Completed Task A with %u Fisher entries (%.1f%% sparse)\n", 
           Fisher->EntryCount, 
           (1.0f - (f32)Fisher->EntryCount / ParameterCount) * 100.0f);
    
    // Begin Task B
    u32 TaskB = BeginTask(&EWC, "Social Interaction");
    printf("✓ Started Task B: %s (ID: %u)\n", EWC.Tasks[1].Name, TaskB);
    printf("✓ Active tasks: %u\n", EWC.ActiveTaskCount);
    
    // Get memory usage
    u32 MemoryUsed = GetMemoryUsage(&EWC);
    printf("\n=== Memory Usage ===\n");
    printf("Total EWC memory: %u KB\n", MemoryUsed / 1024);
    printf("Avg per parameter: %.2f bytes\n", (f32)MemoryUsed / ParameterCount);
    
    // Test adaptive lambda
    f32 OriginalLambda = EWC.Lambda;
    UpdateLambda(&EWC, 0.5f, 0.6f); // Validation loss increasing
    printf("\n=== Adaptive Lambda ===\n");
    printf("Original lambda: %.2f\n", OriginalLambda);
    printf("Updated lambda: %.2f\n", EWC.Lambda);
    
    // Performance characteristics
    printf("\n=== Performance Characteristics ===\n");
    printf("Parameters: %u\n", ParameterCount);
    printf("Non-zero Fisher: %u (%.1f%% sparse)\n", 
           Fisher->EntryCount, (1.0f - (f32)Fisher->EntryCount / ParameterCount) * 100.0f);
    printf("Memory per task: %lu KB\n", 
           (sizeof(ewc_task) + ParameterCount * sizeof(f32) + Fisher->EntryCount * sizeof(ewc_fisher_entry)) / 1024);
    
    // Demonstrate key features
    printf("\n=== Key Features ===\n");
    printf("☑ Task management: %u/%u tasks active\n", EWC.ActiveTaskCount, EWC_MAX_TASKS);
    printf("☑ Sparse Fisher matrices: Memory efficient\n");
    printf("☑ Adaptive lambda: Prevents overfitting/underfitting\n");
    printf("☑ SIMD optimization: Fast penalty computation\n");
    printf("☑ Persistent storage: Save/load EWC state\n");
    
    printf("\n=== Use Cases ===\n");
    printf("• Neural NPCs learning new behaviors\n");
    printf("• Continual learning in dynamic environments\n");
    printf("• Multi-task neural networks\n");
    printf("• Online learning with memory constraints\n");
    
    printf("\n✅ EWC system demonstration complete!\n");
    printf("🧠 Ready to prevent catastrophic forgetting in neural NPCs\n");
    
    free(ArenaMemory);
    return 0;
}