# Memory Arena System Architecture

## Overview

This is a **production-grade memory arena system** that completely eliminates malloc/free from hot paths, achieving true zero-allocation frame updates. The system is designed for a handmade game editor with strict performance requirements.

## Core Design Principles

1. **No malloc/free in hot paths** - All allocations happen at startup
2. **Arena-based allocation** - Fast linear allocation with bulk deallocation
3. **Frame-based memory** - Automatic cleanup each frame
4. **Tagged allocations** - Track memory usage by subsystem
5. **Debug features** - Bounds checking, leak detection, statistics

## Memory Layout

```
Total Memory Pool (5GB)
├── Memory System Header (sizeof(MemorySystem))
├── Permanent Arena (4GB)
│   ├── Game State
│   ├── Assets
│   ├── Scene Data
│   └── Persistent Systems
├── Frame Arena (512MB)
│   └── Cleared every frame
├── Render Arena (256MB)
│   └── Circular buffer for commands
├── GUI Arena (64MB)
│   └── Immediate mode UI
├── Ring Buffer (128MB)
│   └── Multi-frame allocations
├── Stack Allocator (128MB)
│   └── Temporary allocations
└── Memory Pools (32MB each)
    ├── Small Pool (64 byte blocks)
    ├── Medium Pool (256 byte blocks)
    └── Large Pool (1024 byte blocks)
```

## Arena Types

### 1. Permanent Arena (4GB)
- **Purpose**: Long-lived allocations that persist across frames
- **Usage**: Game state, loaded assets, scene data
- **Cleared**: Only on shutdown
- **Example**:
```c
Entity* entity = PushStructTagged(g_memory_system->permanent_arena, Entity, MEMORY_TAG_SCENE);
```

### 2. Frame Arena (512MB)
- **Purpose**: Temporary per-frame allocations
- **Usage**: Calculations, temporary buffers, working memory
- **Cleared**: Start of every frame
- **Example**:
```c
Vector3* temp_positions = PushArray(g_memory_system->frame_arena, Vector3, entity_count);
// No need to free - cleared next frame!
```

### 3. Render Arena (256MB)
- **Purpose**: Render command buffers
- **Usage**: Draw calls, state changes, render data
- **Cleared**: After command execution
- **Features**: Circular buffer mode for streaming

### 4. GUI Arena (64MB)
- **Purpose**: Immediate mode GUI
- **Usage**: Widget state, layout calculations
- **Cleared**: Every frame before GUI pass

## Allocator Types

### Linear Allocator
- **Speed**: O(1) - Fastest possible
- **Deallocation**: Bulk only
- **Use case**: Frame allocations

### Stack Allocator
- **Speed**: O(1) push/pop
- **Deallocation**: LIFO order
- **Use case**: Nested temporary scopes

### Ring Buffer
- **Speed**: O(1)
- **Deallocation**: Automatic after N frames
- **Use case**: Multi-frame temporary data

### Memory Pools
- **Speed**: O(1)
- **Deallocation**: Individual blocks
- **Use case**: Fixed-size allocations (components)

## Zero-Allocation Patterns

### Pattern 1: Frame Memory for Temporaries
```c
void UpdateEntities(Scene* scene, f32 dt) {
    // Allocate working memory from frame arena
    TempMemory temp = Arena_BeginTemp(g_memory_system->frame_arena);
    
    Vector3* velocities = PushArray(g_memory_system->frame_arena, Vector3, scene->entity_count);
    Matrix4* transforms = PushArray(g_memory_system->frame_arena, Matrix4, scene->entity_count);
    
    // Do work...
    
    Arena_EndTemp(temp);  // Or just let it clear next frame
}
```

### Pattern 2: Command Buffers
```c
typedef struct {
    RenderCommand* commands;  // Pre-allocated
    u32 count;
    u32 capacity;
} RenderBuffer;

void SubmitDrawCall(RenderBuffer* buffer, DrawData* data) {
    if (buffer->count < buffer->capacity) {
        buffer->commands[buffer->count++] = *data;  // No allocation!
    }
}
```

### Pattern 3: String Building
```c
StringBuilder* sb = StringBuilder_Create(g_memory_system->frame_arena, 4096);
StringBuilder_Append(sb, "FPS: ");
StringBuilder_AppendFormat(sb, "%.1f", fps);
// String freed automatically next frame
```

## Performance Characteristics

| Operation | Time Complexity | Cache Behavior |
|-----------|----------------|----------------|
| Arena Push | O(1) | Sequential, cache-friendly |
| Arena Clear | O(1) | Bulk operation |
| Pool Allocate | O(1) | May fragment cache |
| Pool Free | O(1) | Immediate reuse |
| Stack Push | O(1) | Sequential |
| Stack Pop | O(1) | LIFO cache-friendly |
| Ring Push | O(1) | Circular, predictable |

## Memory Budgets

| Subsystem | Budget | Typical Usage |
|-----------|--------|---------------|
| Renderer | 256MB | Draw calls, vertex data |
| GUI | 64MB | Widgets, layout |
| Physics | 128MB | Collision data, contacts |
| Audio | 32MB | Sound buffers, mixing |
| Animation | 64MB | Bone transforms, clips |
| Particles | 32MB | Particle systems |
| AI | 32MB | Pathfinding, behavior trees |

## Debug Features

### Memory Tracking
```c
Memory_EnableTracking(true);  // Track all allocations
Memory_PrintStats();           // Show usage statistics
Memory_GetUsedByTag(MEMORY_TAG_RENDERER);  // Per-system tracking
```

### Bounds Checking
```c
// Automatically enabled in debug builds
// Adds guard values before/after each allocation
Memory_CheckIntegrity(arena);  // Verify guards intact
```

### Leak Detection
```c
// At shutdown
Memory_CheckLeaks();     // Report any unfreed memory
Memory_DumpAllocations(); // List all active allocations
```

## Best Practices

### DO:
- ✅ Pre-allocate at startup
- ✅ Use frame arena for temporaries
- ✅ Clear arenas in bulk
- ✅ Tag allocations for tracking
- ✅ Use pools for fixed-size objects
- ✅ Profile memory usage regularly

### DON'T:
- ❌ Use malloc/free in update loops
- ❌ Allocate per-entity per-frame
- ❌ Mix arena and malloc memory
- ❌ Forget to clear frame arena
- ❌ Allocate strings repeatedly
- ❌ Use dynamic containers (vector, etc)

## Integration Example

```c
// In main()
void* base_memory = mmap(NULL, GIGABYTES(5), ...);
Memory_Initialize(base_memory, GIGABYTES(5));

// In game loop
while (running) {
    Arena_Clear(g_memory_system->frame_arena);
    Ring_BeginFrame(g_memory_system->frame_ring);
    
    Update(dt);  // Zero allocations!
    Render();    // Zero allocations!
    
    Ring_EndFrame(g_memory_system->frame_ring);
}

// At shutdown
Memory_PrintStats();
Memory_Shutdown();
```

## Performance Metrics

With this system, you should achieve:
- **0 allocations** per frame in steady state
- **<0.01ms** for all frame allocations
- **<1ms** for arena clear
- **Predictable** memory usage
- **No fragmentation**
- **Cache-friendly** access patterns

## Compile Flags

```bash
# Debug build with full tracking
gcc -DDEBUG -DMEMORY_TRACKING -DMEMORY_BOUNDS_CHECK

# Release build optimized
gcc -O3 -DNDEBUG

# Profile build
gcc -O2 -pg -DMEMORY_TRACKING
```

## Memory Profiling

Use the built-in profiler:
```c
// Every second
if (frame_count % 60 == 0) {
    Memory_PrintStats();
}

// On demand
if (key_pressed(KEY_F9)) {
    Memory_PrintArenaInfo(g_memory_system->permanent_arena);
}
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Out of frame memory | Increase frame arena size or use temp memory |
| Memory leak | Enable tracking, check Memory_CheckLeaks() |
| Corruption | Enable bounds checking, use Memory_CheckIntegrity() |
| Fragmentation | Use pools for fixed-size allocations |
| Slow allocation | Ensure using arenas not malloc |

## Conclusion

This memory system provides **deterministic, fast, and debuggable** memory management suitable for production game development. By eliminating dynamic allocation from hot paths, we achieve consistent frame times and predictable performance.