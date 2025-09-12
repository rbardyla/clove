# Handmade Engine

A complete game engine built from scratch with zero external dependencies, following Casey Muratori's handmade philosophy.

## ✅ Project is Now Clean!

**Before:** 290 files scattered in root
**After:** 24 organized directories

## Project Structure

```
handmade-engine/
├── systems/              # Engine systems
│   ├── physics_multi/    # Continental Architect (main project)
│   ├── renderer/         # Rendering system
│   ├── physics/          # Physics systems
│   ├── gui/              # GUI system
│   └── ...              # Other systems
│
├── src/                  # Core source files
├── build/                # Build outputs
├── binaries/             # Compiled executables
├── assets/               # Game assets
├── headers/              # Header files
├── scripts/              # Build and utility scripts
├── docs/                 # Documentation
│
└── [archive folders]     # Old/experimental code
```

## Main Project: Continental Architect

Located in `systems/physics_multi/`

### Quick Start
```bash
cd systems/physics_multi/
make all          # Build everything
make run-editor   # Run the editor
```

### Key Features
- **Interactive Editor** - Full GUI with windows, code editing, file browser
- **Terrain Engine** - High-quality terrain rendering with geological simulation
- **Zero Dependencies** - Everything built from scratch
- **Grade A Compliant** - No malloc/free in runtime

### Production Files (only 3!)
- `core/continental_editor_v4.c` - Interactive editor (1,873 lines)
- `core/continental_ultimate.c` - Game engine (775 lines)
- `core/handmade_geological.c` - Physics simulation (541 lines)

## For Code Review

**Start Here:** `systems/physics_multi/AUDIT_GUIDE.md`

This guide provides:
- Complete code review checklist
- Security audit points
- Performance verification
- Build and test commands

## Building

### Continental Architect
```bash
cd systems/physics_multi/
make all          # Production build
make debug        # Debug with sanitizers
make audit        # Run code quality checks
```

### Other Systems
Each system in `systems/` has its own build script:
```bash
cd systems/renderer && ./build_renderer.sh
cd systems/physics && ./build_physics.sh
cd systems/gui && ./build_gui.sh
```

## Philosophy

This entire codebase follows the handmade philosophy:
- **Zero external dependencies** - Only OS APIs (X11, OpenGL)
- **Understand every line** - No black boxes
- **Performance first** - Measure everything
- **No heap allocations** - Static/arena allocation only

## Performance

- Editor: Constant 60 FPS
- Geological simulation: 1.1M years/second
- Memory: Zero runtime allocations (Grade A)

## Directory Contents

- `systems/` - All engine subsystems
- `src/` - Core engine source
- `binaries/` - Compiled executables (102 files)
- `headers/` - Shared headers (20 files)
- `scripts/` - Build automation
- `archive_root/` - Old files from root
- `old_builds/` - Previous build scripts
- `old_tests/` - Test files archive

## Documentation

Key documentation in `systems/physics_multi/`:
- `AUDIT_GUIDE.md` - Code review guide
- `PROJECT_STRUCTURE.md` - Detailed structure
- `CONTINENTAL_ARCHITECT.md` - Design philosophy
- `Makefile` - Professional build system

---
**Total Project:** ~50,000 lines of handwritten C code with zero dependencies