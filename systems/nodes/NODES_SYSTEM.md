# Node-Based Visual Programming System

## Overview

A complete, high-performance visual programming system built from scratch with zero external dependencies. This system allows users to create game logic, AI behaviors, shaders, and procedural content using a node-based interface similar to Unreal Blueprints or ComfyUI.

## Architecture

### Core Components

1. **handmade_nodes.h/c** - Core node system
   - Fixed memory pools (zero allocations during execution)
   - Cache-aligned node structures
   - Topological sorting for execution order
   - Connection validation with type checking

2. **nodes_renderer.c** - Visual rendering
   - Bezier curves for connections
   - Frustum culling for large graphs
   - LOD system for performance
   - Minimap navigation
   - Grid snapping

3. **nodes_executor.c** - Execution engine
   - Stack-based execution model
   - SIMD optimizations (AVX2)
   - Result caching for pure functions
   - Parallel execution support
   - Debug stepping

4. **nodes_library.c** - Built-in nodes
   - 50+ node types across 8 categories
   - Flow control (Branch, Loop, Sequence)
   - Math operations (Add, Multiply, Lerp, Trig)
   - Logic gates (AND, OR, NOT, Compare)
   - Game-specific (Spawn, Move, Play Sound)

5. **nodes_editor.c** - Editor interface
   - Context menu with search
   - Copy/paste support
   - Undo/redo system
   - Property inspector
   - Alignment tools
   - Keyboard shortcuts

6. **nodes_advanced.c** - Advanced features
   - Subgraph system
   - Bytecode compilation
   - Node templates
   - Live editing
   - Visual debugging
   - Version migration

7. **nodes_integration.c** - Engine integration
   - Script system binding
   - Hot reload support
   - Save/load system
   - C code export
   - Engine API exposure

8. **nodes_demo.c** - Demonstration
   - Game logic example
   - AI behavior trees
   - Shader graphs
   - Procedural generation
   - Performance benchmarks

## Performance Characteristics

- **Memory**: Fixed pools, zero allocations during gameplay
- **Cache**: Nodes laid out sequentially in execution order
- **SIMD**: AVX2 for vector math, matrix operations
- **Threading**: Parallel execution for independent branches
- **Targets**:
  - Render 1000+ nodes at 60 FPS
  - Execute 10,000 nodes per frame
  - <1ms overhead for simple graphs
  - <0.1ms per entity for neural inference

## Key Features

### Visual Programming
- Drag-and-drop node creation
- Type-safe connections
- Real-time value preview
- Flow visualization
- Performance overlay

### Execution Model
- Data flow with execution pins
- Deterministic evaluation
- Cached results for pure functions
- Stack-based control flow
- Breakpoint debugging

### Node Types

#### Flow Control
- Branch (if/else)
- For Loop
- While Loop
- Sequence
- Gate

#### Mathematics
- Basic operations (Add, Subtract, Multiply, Divide)
- Trigonometry (Sin, Cos, Tan)
- Interpolation (Lerp, Smoothstep)
- Vector math (Dot, Cross, Normalize)
- Random number generation

#### Logic
- Boolean operations (AND, OR, NOT, XOR)
- Comparisons (Equal, Greater, Less)
- Type conversions

#### Variables
- Local variables
- Global variables
- Get/Set operations
- Arrays

#### Events
- On Update (per frame)
- On Input (keyboard/mouse)
- Timer/Delay
- Custom events

#### Game-Specific
- Entity spawn/destroy
- Transform operations
- Physics queries
- Audio playback
- Particle effects

#### AI
- Behavior tree nodes
- State machine nodes
- Perception queries
- Navigation

#### Debug
- Print values
- Breakpoints
- Watch variables
- Performance profiling

## Usage Example

```c
// Initialize system
void *memory = malloc(Megabytes(16));
nodes_init(memory, Megabytes(16));
nodes_library_init();

// Create graph
node_graph_t *graph = node_graph_create("Player Controller");

// Create nodes
node_t *on_input = node_create(graph, node_get_type_id("On Input"), 100, 100);
node_t *get_axis = node_create(graph, node_get_type_id("Get Axis"), 300, 100);
node_t *multiply = node_create(graph, node_get_type_id("Multiply"), 500, 100);
node_t *move = node_create(graph, node_get_type_id("Move Entity"), 700, 100);

// Connect nodes
node_connect(graph, on_input->id, 0, get_axis->id, 0);
node_connect(graph, get_axis->id, 0, multiply->id, 0);
node_connect(graph, multiply->id, 0, move->id, 1);

// Execute
node_execution_context_t ctx = {0};
node_graph_execute(graph, &ctx);
```

## Building

```bash
cd /home/thebackhand/Projects/handmade-engine/systems/nodes
./build_nodes.sh
```

This creates:
- `build/libnodes.a` - Static library
- `build/nodes_demo` - Interactive demo
- `build/perf_test` - Performance benchmark

## File Format

Graphs are saved in a binary format with:
- Header with magic number and version
- Node data (position, type, custom data)
- Connection data
- View state

Files use `.nodes` extension and can be hot-reloaded.

## Export Options

1. **Binary format** - Fast loading, preserves all data
2. **C code export** - Generate compilable C code
3. **JSON export** - Human-readable, version control friendly

## Integration Points

- Script system binding for runtime node creation
- Hot reload for live editing
- Engine API exposure as nodes
- Custom node creation API
- Event system integration

## Design Philosophy

Following the handmade philosophy:
- Zero external dependencies
- Fixed memory allocation
- Measure everything
- Cache-aware data structures
- SIMD where beneficial
- Platform layer abstraction
- Deterministic execution

## Future Enhancements

- GPU execution for parallel nodes
- Network synchronization for multiplayer
- Visual scripting debugger
- Node package system
- Machine learning nodes
- Compute shader generation