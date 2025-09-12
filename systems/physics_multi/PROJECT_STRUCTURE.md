# Continental Architect Project Structure

## Overview
Continental Architect is a handmade game engine and editor built from scratch with zero external dependencies, following Casey Muratori's handmade philosophy.

## Directory Organization

### Core Systems
```
/systems/physics_multi/
├── CONTINENTAL_ARCHITECT.md     # Main project documentation
├── PROJECT_STRUCTURE.md         # This file
├── AUDIT_GUIDE.md               # Partner code audit guide
├── DEBUG_GUIDE.md               # Debugging documentation
│
├── core/                        # Core systems (to be created)
│   ├── continental_ultimate.c   # Main game engine
│   ├── continental_editor_v4.c  # Production editor
│   └── handmade_geological.c    # Geological simulation
│
├── tests/                       # Test files (to be created)
│   ├── test_geological.c        # Geological system tests
│   ├── test_mouse_calibration.c # Mouse calibration test
│   └── test_physics_multi.c     # Physics integration tests
│
├── experimental/                # Experimental versions (to be created)
│   ├── continental_editor_v1.c  # Early editor versions
│   ├── continental_editor_v2.c
│   └── continental_editor_v3.c
│
├── build/                       # Build scripts
│   ├── build_editor.sh          # Build production editor
│   ├── build_game.sh            # Build game engine
│   └── build_all.sh             # Build everything
│
└── docs/                        # Documentation
    ├── API.md                   # API documentation
    ├── PERFORMANCE.md           # Performance metrics
    └── HANDMADE_PRINCIPLES.md   # Handmade philosophy guide
```

## Key Files

### Production Ready
- **continental_editor_v4.c** - Fully featured editor with interactive UI
- **continental_ultimate.c** - High-quality terrain game engine
- **handmade_geological.c** - 1.1M years/second geological simulation

### Build System
- **build_editor.sh** - Builds the production editor
- **build_game.sh** - Builds the game engine
- **Makefile** - Main build system with debug targets

## Quick Start

### Building
```bash
# Build everything
make all

# Build with debug symbols
make debug

# Build for release
make release

# Clean build artifacts
make clean
```

### Running
```bash
# Run the editor
./binaries/continental_editor

# Run the game directly
./binaries/continental_game

# Run tests
make test
```

## Architecture

### Memory Management
- **Zero malloc/free** - All memory is statically allocated
- **Arena allocators** - Used for temporary allocations
- **Grade A compliant** - No heap allocations in hot paths

### Rendering
- **OpenGL 2.1** - Direct OpenGL usage, no libraries
- **X11** - Direct window management on Linux
- **Immediate mode** - No retained state for UI

### Physics
- **Multi-scale simulation** - Geological, hydrological, structural
- **Time scales** - From milliseconds to millions of years
- **Cache coherent** - Structure of Arrays (SoA) design

## Performance Targets
- Editor: 60+ FPS with full UI
- Game: 60+ FPS with terrain rendering
- Physics: 1M+ simulation years/second

## Debugging Features
- Visual overlays for all systems
- Real-time performance metrics
- Mouse calibration tools
- Console with command interface

## Code Standards
- C99 standard
- No external dependencies
- Everything built from scratch
- Fully debuggable
- Zero black boxes

## Contact
For code auditing or questions, refer to AUDIT_GUIDE.md