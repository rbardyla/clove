/*
    Memory Arena Demo
    Demonstrates zero-allocation patterns in the handmade editor
*/

#include "handmade_platform.h"
#include <stdio.h>

// Example: Using frame memory for temporary calculations
void ProcessEntitiesWithFrameMemory(PlatformState* platform, int entity_count) {
    // Allocate temporary arrays from frame arena
    // These will be automatically cleared next frame - no free() needed!
    
    f32* distances = PushArray(&platform->frame_arena, f32, entity_count);
    i32* sorted_indices = PushArray(&platform->frame_arena, i32, entity_count);
    
    // Use the memory
    for (int i = 0; i < entity_count; i++) {
        distances[i] = i * 1.5f;
        sorted_indices[i] = i;
    }
    
    printf("Frame arena used: %zu bytes (%.2f MB)\n", 
           platform->frame_arena.used,
           platform->frame_arena.used / (1024.0 * 1024.0));
}

// Example: Using permanent memory for persistent data
typedef struct {
    char name[64];
    f32 x, y, z;
    i32 id;
} Entity;

Entity* CreateEntitySystem(PlatformState* platform, int max_entities) {
    // Allocate from permanent arena - lives for entire program lifetime
    Entity* entities = PushArray(&platform->permanent_arena, Entity, max_entities);
    
    printf("Permanent arena used: %zu bytes (%.2f MB)\n",
           platform->permanent_arena.used,
           platform->permanent_arena.used / (1024.0 * 1024.0));
    
    return entities;
}

// Example: Temporary memory scope
void RenderFrameWithTempMemory(PlatformState* platform) {
    // Save current frame arena state
    TempMemory temp = BeginTempMemory(&platform->frame_arena);
    
    // Allocate a bunch of temporary data
    int vertex_count = 10000;
    f32* vertices = PushArray(&platform->frame_arena, f32, vertex_count * 3);
    f32* normals = PushArray(&platform->frame_arena, f32, vertex_count * 3);
    u32* indices = PushArray(&platform->frame_arena, u32, vertex_count);
    
    // Use the memory for rendering...
    printf("Temp allocation: %zu bytes\n", 
           platform->frame_arena.used - temp.used);
    
    // Restore frame arena to saved state - frees all temp allocations
    EndTempMemory(temp);
}

// Demo function to show memory patterns
void DemoMemorySystem(PlatformState* platform) {
    printf("\n=== Memory Arena Demo ===\n");
    printf("Permanent arena: %.2f GB allocated\n", 
           platform->permanent_arena.size / (1024.0 * 1024.0 * 1024.0));
    printf("Frame arena: %.2f MB allocated\n",
           platform->frame_arena.size / (1024.0 * 1024.0));
    
    // 1. Create persistent entity system
    printf("\n1. Creating entity system (permanent memory):\n");
    Entity* entities = CreateEntitySystem(platform, 1000);
    
    // 2. Process entities with frame memory
    printf("\n2. Processing entities (frame memory):\n");
    ProcessEntitiesWithFrameMemory(platform, 1000);
    
    // 3. Render with temporary scope
    printf("\n3. Rendering with temp memory:\n");
    RenderFrameWithTempMemory(platform);
    
    // Show final usage
    printf("\n=== Final Memory Usage ===\n");
    printf("Permanent: %zu / %zu bytes (%.1f%%)\n",
           platform->permanent_arena.used,
           platform->permanent_arena.size,
           100.0 * platform->permanent_arena.used / platform->permanent_arena.size);
    printf("Frame: %zu / %zu bytes (%.1f%%)\n",
           platform->frame_arena.used,
           platform->frame_arena.size,
           100.0 * platform->frame_arena.used / platform->frame_arena.size);
    
    printf("\n✓ No malloc/free calls in hot paths!\n");
    printf("✓ Frame memory auto-clears every frame!\n");
    printf("✓ O(1) allocation performance!\n");
}