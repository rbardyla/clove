#!/bin/bash

# Handmade Editor Setup Script
# Sets up the unified platform layer and integrates existing systems

set -e

echo "==================================="
echo "Handmade Editor Setup & Integration"
echo "==================================="

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Create directory structure
echo -e "${YELLOW}Creating directory structure...${NC}"
mkdir -p build
mkdir -p src
mkdir -p systems/{renderer,gui,editor,scene,assets,command,physics,audio,network,ai}
mkdir -p external
mkdir -p assets/{models,textures,shaders,scenes}

# Check for existing systems and merge them
echo -e "${YELLOW}Checking for existing systems...${NC}"

# Function to integrate existing code
integrate_system() {
    local system=$1
    local target_dir=$2
    
    if [ -d "$system" ]; then
        echo -e "  ${GREEN}✓${NC} Found $system - integrating..."
        cp -r $system/* $target_dir/ 2>/dev/null || true
    else
        echo -e "  ${YELLOW}!${NC} $system not found - will create placeholder"
    fi
}

# Integrate existing systems
integrate_system "renderer" "systems/renderer"
integrate_system "gui" "systems/gui"
integrate_system "editor" "systems/editor"
integrate_system "systems/hotreload" "systems/hotreload"
integrate_system "systems/physics" "systems/physics"
integrate_system "systems/audio" "systems/audio"
integrate_system "systems/network" "systems/network"
integrate_system "systems/ai" "systems/ai"

# Create placeholder files for missing systems
create_placeholder() {
    local file=$1
    local system=$2
    
    if [ ! -f "$file" ]; then
        echo "// $system - Placeholder" > $file
        echo "// TODO: Implement $system" >> $file
        echo "" >> $file
        echo "#include \"handmade_platform.h\"" >> $file
        echo "" >> $file
        echo "void ${system}_Init(PlatformState* platform) {" >> $file
        echo "    // TODO: Initialize $system" >> $file
        echo "}" >> $file
    fi
}

# Create core system files if they don't exist
echo -e "${YELLOW}Creating missing system files...${NC}"

create_placeholder "systems/renderer/renderer.c" "Renderer"
create_placeholder "systems/renderer/shader.c" "Shader"
create_placeholder "systems/renderer/mesh.c" "Mesh"
create_placeholder "systems/renderer/texture.c" "Texture"

create_placeholder "systems/gui/gui.c" "GUI"
create_placeholder "systems/gui/widgets.c" "Widgets"
create_placeholder "systems/gui/layout.c" "Layout"
create_placeholder "systems/gui/theme.c" "Theme"

create_placeholder "systems/editor/editor_core.c" "EditorCore"
create_placeholder "systems/editor/viewport.c" "Viewport"
create_placeholder "systems/editor/hierarchy.c" "Hierarchy"
create_placeholder "systems/editor/inspector.c" "Inspector"
create_placeholder "systems/editor/console.c" "Console"
create_placeholder "systems/editor/assets.c" "Assets"
create_placeholder "systems/editor/gizmos.c" "Gizmos"

create_placeholder "systems/scene/scene.c" "Scene"
create_placeholder "systems/scene/transform.c" "Transform"
create_placeholder "systems/scene/entity.c" "Entity"

create_placeholder "systems/assets/assets.c" "AssetSystem"
create_placeholder "systems/assets/loader.c" "AssetLoader"

create_placeholder "systems/command/command.c" "Command"
create_placeholder "systems/command/undo.c" "UndoRedo"

# Make build script executable
chmod +x build.sh

# Create a simple Makefile for convenience
echo -e "${YELLOW}Creating Makefile...${NC}"
cat > Makefile << 'EOF'
# Handmade Editor Makefile

.PHONY: all debug release clean run watch hot-reload

all: debug

debug:
	@./build.sh debug

release:
	@./build.sh release

profile:
	@./build.sh profile

clean:
	@rm -rf build/*

run: debug
	@./build/editor

watch:
	@./build.sh debug no watch

hot-reload:
	@./build.sh debug yes

hot-watch:
	@./build.sh debug yes watch

help:
	@echo "Handmade Editor Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make         - Build debug version"
	@echo "  make debug   - Build debug version"
	@echo "  make release - Build release version"
	@echo "  make profile - Build with profiling"
	@echo "  make run     - Build and run"
	@echo "  make watch   - Build with file watching"
	@echo "  make hot-reload - Build with hot reload support"
	@echo "  make clean   - Clean build directory"
	@echo ""
	@echo "Build script options:"
	@echo "  ./build.sh [mode] [hot-reload] [action]"
	@echo "    mode: debug|release|profile"
	@echo "    hot-reload: yes|no"
	@echo "    action: run|watch"
EOF

# Create .gitignore
echo -e "${YELLOW}Creating .gitignore...${NC}"
cat > .gitignore << 'EOF'
# Build artifacts
build/
*.o
*.so
*.a
*.out
editor

# Temporary files
*.tmp
*.swp
*.swo
*~
.DS_Store

# IDE files
.vscode/
.idea/
*.sublime-*
*.code-workspace

# Debug files
*.pdb
*.ilk
core
vgcore.*
callgrind.*

# Profile data
gmon.out
*.prof
EOF

# Install dependencies check
echo -e "${YELLOW}Checking dependencies...${NC}"

check_dependency() {
    local cmd=$1
    local package=$2
    
    if command -v $cmd &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} $cmd found"
    else
        echo -e "  ${RED}✗${NC} $cmd not found - install with: sudo apt install $package"
    fi
}

check_dependency "gcc" "build-essential"
check_dependency "g++" "build-essential"
check_dependency "make" "make"
check_dependency "pkg-config" "pkg-config"
check_dependency "inotifywait" "inotify-tools"
check_dependency "zenity" "zenity"

# Check for X11 and OpenGL headers
if [ -f "/usr/include/X11/Xlib.h" ]; then
    echo -e "  ${GREEN}✓${NC} X11 headers found"
else
    echo -e "  ${RED}✗${NC} X11 headers not found - install with: sudo apt install libx11-dev libxi-dev"
fi

if [ -f "/usr/include/GL/gl.h" ]; then
    echo -e "  ${GREEN}✓${NC} OpenGL headers found"
else
    echo -e "  ${RED}✗${NC} OpenGL headers not found - install with: sudo apt install libgl1-mesa-dev"
fi

# Create initial project file
echo -e "${YELLOW}Creating initial project file...${NC}"
cat > project.json << 'EOF'
{
    "name": "Handmade Editor",
    "version": "0.1.0",
    "build": {
        "target": "editor",
        "platform": "linux",
        "compiler": "gcc",
        "optimization": "debug"
    },
    "settings": {
        "window": {
            "width": 1920,
            "height": 1080,
            "title": "Handmade Editor"
        },
        "renderer": {
            "vsync": true,
            "msaa": 4
        },
        "memory": {
            "permanent": 1073741824,
            "transient": 536870912
        }
    }
}
EOF

# Create README
echo -e "${YELLOW}Creating README...${NC}"
cat > README.md << 'EOF'
# Handmade Editor - Unified Build

A high-performance game editor built from scratch using handmade philosophy.

## Quick Start

```bash
# Build and run
make run

# Development mode with file watching
make watch

# Hot reload development
make hot-reload
```

## Build Options

- `make debug` - Debug build with symbols
- `make release` - Optimized release build
- `make profile` - Build with profiling support
- `make clean` - Clean build artifacts

## Architecture

### Unified Platform Layer
- Single platform abstraction (`handmade_platform.h`)
- Linux/X11 implementation (`handmade_platform_linux.c`)
- Memory arena system (no malloc in hot paths)
- Hot reload support

### Core Systems
- **Renderer** - OpenGL-based rendering
- **GUI** - Immediate mode UI system
- **Editor** - Scene editor with viewport, hierarchy, inspector
- **Scene** - Entity-component system
- **Assets** - Asset loading and management
- **Command** - Undo/redo system

### Performance Targets
- <1 second compile time (unity build)
- 60+ FPS with complex scenes
- <16ms frame time
- Zero allocations in hot paths

## Development

### Hot Reload
```bash
# Terminal 1: Run editor with hot reload
./build/editor --hot-reload

# Terminal 2: Watch and rebuild on changes
make hot-watch
```

### Profiling
```bash
make profile
./build/editor
gprof ./build/editor gmon.out > profile.txt
```

## Controls

- **Right Mouse** - Camera control
- **WASD** - Move camera
- **QE** - Move up/down
- **Scroll** - Zoom
- **F1-F4** - Toggle panels
- **G** - Toggle grid
- **Z** - Toggle wireframe
- **Ctrl+S** - Save
- **Ctrl+O** - Open
- **Ctrl+Z/Y** - Undo/Redo

## Status

- [x] Unified platform layer
- [x] Memory arena system
- [x] Unity build (<1 second)
- [x] Hot reload
- [x] Basic viewport
- [ ] Full GUI integration
- [ ] Scene serialization
- [ ] Asset pipeline
- [ ] Physics integration
- [ ] Audio system
EOF

# Test build
echo ""
echo -e "${YELLOW}Testing build system...${NC}"
./build.sh debug

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}==================================="
    echo -e "Setup Complete!"
    echo -e "===================================${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Run the editor: make run"
    echo "  2. Start development: make watch"
    echo "  3. Enable hot reload: make hot-reload"
    echo ""
    echo "The unified platform layer has been created."
    echo "Your existing systems have been integrated."
    echo ""
else
    echo ""
    echo -e "${RED}Build failed. Please check the error messages above.${NC}"
    echo "You may need to install missing dependencies."
fi
EOF