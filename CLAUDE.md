# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a complete, production-ready game engine built entirely from scratch with zero external dependencies (except OS-level APIs). The engine follows the "handmade philosophy" - everything is built from first principles for maximum performance and complete control.

## Build Commands

### Build Entire Engine
```bash
cd /home/thebackhand/Projects/handmade-engine
./BUILD_ALL.sh           # Build all systems in dependency order
./BUILD_ALL.sh release   # Build with optimizations
./BUILD_ALL.sh debug     # Build with debug symbols
```

### Build Individual Systems
```bash
# Each system has its own build script
cd systems/renderer && ./build_renderer.sh
cd systems/physics && ./build_physics.sh
cd systems/gui && ./build_gui.sh
cd systems/blueprint && ./build_blueprint.sh
cd systems/ai && ./build_neural.sh
cd systems/audio && ./build_audio.sh
cd systems/network && ./build_network.sh
cd systems/save && ./build_save.sh
cd systems/world_gen && ./build_world_gen.sh
```

### Build Integrated Demo
```bash
cd /home/thebackhand/Projects/handmade-engine
./build_demo.sh          # Build complete demo
./handmade_demo          # Run integrated demo
```

### Hot Reload Development
```bash
make                     # Quick rebuild game library
./demo_hotreload.sh      # Run with hot reload support
# Press R to reload code changes during runtime
```

## Testing Commands

### Run All Tests
```bash
./test_all.sh            # Run complete test suite
```

### Run Individual System Tests
```bash
# Test executables in binaries/
./binaries/renderer_test
./binaries/physics_test
./binaries/gui_test
./binaries/neural_test
./binaries/blueprint_test
```

### Run Performance Benchmarks
```bash
# Must use release builds for accurate metrics
./BUILD_ALL.sh release
./binaries/perf_test     # Run performance suite
```

### Run Neural Network Demos
```bash
./binaries/neural_rpg_demo       # RPG with neural NPCs
./binaries/multi_npc_social_demo # Social interaction demo
./binaries/lstm_example          # LSTM implementation
./binaries/dnc_demo              # Differentiable Neural Computer
```

## Code Architecture

### System Organization

All systems follow the same pattern and are located in `/systems/`:
- Each system is self-contained with its own build script
- Systems can be built independently or as part of the engine
- All systems follow zero-dependency philosophy

### Memory Management Pattern

```c
// All systems use arena allocators - no malloc/free in hot paths
typedef struct arena {
    u8* base;
    u64 size;
    u64 used;
} arena;

// Structure of Arrays (SoA) for cache coherency
struct physics_bodies {
    v3* positions;      // All positions together
    v3* velocities;     // All velocities together
    f32* masses;        // All masses together
    // NOT Array of Structures
};
```

### Platform Abstraction

```c
// Platform layer handles all OS interaction
// Located in handmade_platform_linux.c or handmade_platform_win32.c
platform_state platform;
platform_init(&platform);

// All systems receive platform pointer
renderer_init(&renderer, &platform);
physics_init(&physics, &platform);
```

### Performance Standards

Every system must meet these requirements:
- Zero heap allocations in hot paths
- SIMD optimizations (AVX2/FMA) where applicable
- Cache-coherent data structures (SoA)
- Predictable memory usage
- 60+ FPS performance targets

### Build Flags

```bash
# Performance builds
-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops

# Debug builds  
-g -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1

# Common flags
-Wall -Wextra -Wno-unused-parameter -std=c99
```

## System Integration Points

### Renderer Integration
```c
// Systems integrate with renderer for debug visualization
physics_debug_draw(&physics, &renderer);
gui_render(&gui, &renderer);
blueprint_editor_render(&blueprint, &renderer);
```

### GUI Integration
```c
// Systems provide GUI panels for runtime inspection
physics_show_debug_panel(&physics, &gui);
neural_show_stats_panel(&neural, &gui);
```

### Save System Integration
```c
// Systems implement serialization interface
physics_serialize(&physics, &save_context);
neural_serialize(&neural, &save_context);
```

## Key Implementation Files

### Core Engine Loop
- `main.c` - Platform initialization and main loop
- `game.c` - Game update and render functions
- `game.h` - Shared game state

### System Headers
- `systems/*/handmade_*.h` - Public interfaces
- `systems/*/handmade_*.c` - Implementations
- Always check system-specific README for details

### Platform Layer
- `handmade_platform_linux.c` - Linux/X11 implementation
- `handmade_platform_win32.c` - Windows implementation
- `handmade_platform.h` - Platform interface

## Development Workflow

### Adding New Features
1. Check if system already exists in `/systems/`
2. Follow existing system patterns for consistency
3. Use arena allocators, no malloc/free
4. Implement debug visualization
5. Add performance metrics
6. Create standalone test executable

### Debugging
```bash
# Use debug builds
./BUILD_ALL.sh debug

# Enable system debug output
export HANDMADE_DEBUG=1

# Use integrated profiler
# Press F1 in running demo for profiler overlay
```

### Performance Optimization
1. Always measure first (use release builds)
2. Check cache coherency (use perf/vtune)
3. Consider SIMD opportunities
4. Profile memory access patterns
5. Target: 60+ FPS with headroom

## Important Notes

- **NO EXTERNAL DEPENDENCIES**: Do not add any external libraries
- **HANDMADE PHILOSOPHY**: Everything from scratch, no black boxes
- **PERFORMANCE FIRST**: Every microsecond counts
- **MEASURE EVERYTHING**: Add metrics to all new systems
- **PLATFORM ABSTRACTION**: Use platform layer for OS interaction
- **ARENA ALLOCATORS**: No malloc/free in hot paths
- **STRUCTURE OF ARRAYS**: Cache-coherent data layouts
- **DEBUG VISUALIZATION**: All systems must support debug drawing