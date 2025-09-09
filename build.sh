#!/bin/bash

# Handmade Unity Build System
# Compiles entire editor in <1 second using single translation unit

set -e

# Configuration
BUILD_DIR="build"
OUTPUT="editor"
MODULE_OUTPUT="editor.so"
PLATFORM="linux"

# Build mode (debug/release/profile)
MODE=${1:-debug}
HOT_RELOAD=${2:-no}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Timer start
START_TIME=$(date +%s%N)

# Create build directory
mkdir -p $BUILD_DIR

# Compiler settings
CC="gcc"
CXX="g++"

# Common flags
COMMON_FLAGS="-std=c11 -march=native -fno-exceptions"
COMMON_FLAGS="$COMMON_FLAGS -Wall -Wno-unused-function -Wno-unused-variable"
COMMON_FLAGS="$COMMON_FLAGS -I. -Isrc -Isystems -Iexternal"

# OpenGL and system libraries
LIBS="-lGL -lX11 -lm -lpthread -ldl"

# Mode-specific flags
case $MODE in
    debug)
        echo -e "${YELLOW}Building DEBUG mode...${NC}"
        CFLAGS="$COMMON_FLAGS -O0 -g -DDEBUG -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1"
        ;;
    release)
        echo -e "${YELLOW}Building RELEASE mode...${NC}"
        CFLAGS="$COMMON_FLAGS -O3 -DNDEBUG -DHANDMADE_SLOW=0 -DHANDMADE_INTERNAL=0"
        CFLAGS="$CFLAGS -fomit-frame-pointer -fno-rtti"
        ;;
    profile)
        echo -e "${YELLOW}Building PROFILE mode...${NC}"
        CFLAGS="$COMMON_FLAGS -O2 -g -pg -DPROFILE -DHANDMADE_SLOW=1"
        LIBS="$LIBS -pg"
        ;;
    *)
        echo -e "${RED}Unknown build mode: $MODE${NC}"
        exit 1
        ;;
esac

# Unity build file
UNITY_FILE="$BUILD_DIR/unity_build.c"

# Generate unity build file
echo "// Auto-generated unity build file" > $UNITY_FILE
echo "// Generated at $(date)" >> $UNITY_FILE
echo "" >> $UNITY_FILE

# Platform layer
echo "#define HANDMADE_PLATFORM_IMPLEMENTATION" >> $UNITY_FILE
echo "#include \"handmade_platform_linux.c\"" >> $UNITY_FILE
echo "" >> $UNITY_FILE

# Editor implementation (if not using hot reload)
if [ "$HOT_RELOAD" != "yes" ]; then
    echo "// Editor implementation" >> $UNITY_FILE
    echo "#include \"editor_main.c\"" >> $UNITY_FILE
    
    # Include all systems
    echo "" >> $UNITY_FILE
    echo "// Core systems" >> $UNITY_FILE
    
    # Renderer (new production system)
    if [ -f "systems/renderer/handmade_renderer_opengl.c" ]; then
        echo "#include \"systems/renderer/handmade_renderer_opengl.c\"" >> $UNITY_FILE
    fi
    
    # Legacy renderer (if exists)
    if [ -f "systems/renderer/renderer.c" ]; then
        echo "#include \"systems/renderer/renderer.c\"" >> $UNITY_FILE
        echo "#include \"systems/renderer/shader.c\"" >> $UNITY_FILE
        echo "#include \"systems/renderer/mesh.c\"" >> $UNITY_FILE
        echo "#include \"systems/renderer/texture.c\"" >> $UNITY_FILE
    fi
    
    # GUI
    if [ -f "systems/gui/gui.c" ]; then
        echo "#include \"systems/gui/gui.c\"" >> $UNITY_FILE
        echo "#include \"systems/gui/widgets.c\"" >> $UNITY_FILE
        echo "#include \"systems/gui/layout.c\"" >> $UNITY_FILE
        echo "#include \"systems/gui/theme.c\"" >> $UNITY_FILE
    fi
    
    # Editor components
    if [ -f "systems/editor/editor_core.c" ]; then
        echo "#include \"systems/editor/editor_core.c\"" >> $UNITY_FILE
        echo "#include \"systems/editor/viewport.c\"" >> $UNITY_FILE
        echo "#include \"systems/editor/hierarchy.c\"" >> $UNITY_FILE
        echo "#include \"systems/editor/inspector.c\"" >> $UNITY_FILE
        echo "#include \"systems/editor/console.c\"" >> $UNITY_FILE
        echo "#include \"systems/editor/assets.c\"" >> $UNITY_FILE
        echo "#include \"systems/editor/gizmos.c\"" >> $UNITY_FILE
    fi
    
    # Scene system
    if [ -f "systems/scene/scene.c" ]; then
        echo "#include \"systems/scene/scene.c\"" >> $UNITY_FILE
        echo "#include \"systems/scene/transform.c\"" >> $UNITY_FILE
        echo "#include \"systems/scene/entity.c\"" >> $UNITY_FILE
    fi
    
    # Asset system
    if [ -f "systems/assets/assets.c" ]; then
        echo "#include \"systems/assets/assets.c\"" >> $UNITY_FILE
        echo "#include \"systems/assets/loader.c\"" >> $UNITY_FILE
    fi
    
    # Command system
    if [ -f "systems/command/command.c" ]; then
        echo "#include \"systems/command/command.c\"" >> $UNITY_FILE
        echo "#include \"systems/command/undo.c\"" >> $UNITY_FILE
    fi
fi

# Compile
echo -e "${GREEN}Compiling...${NC}"

if [ "$HOT_RELOAD" = "yes" ]; then
    # Build platform executable
    echo -e "  Platform layer..."
    $CC $CFLAGS -DHOT_RELOAD -c handmade_platform_linux.c -o $BUILD_DIR/platform.o
    $CC $BUILD_DIR/platform.o $LIBS -o $BUILD_DIR/$OUTPUT
    
    # Build editor module
    echo -e "  Editor module..."
    EDITOR_UNITY="$BUILD_DIR/editor_unity.c"
    echo "// Editor module unity build" > $EDITOR_UNITY
    echo "#include \"editor_main.c\"" >> $EDITOR_UNITY
    # Add other includes as above...
    
    $CC $CFLAGS -shared -fPIC -c $EDITOR_UNITY -o $BUILD_DIR/editor.o
    $CC -shared $BUILD_DIR/editor.o $LIBS -o $BUILD_DIR/$MODULE_OUTPUT
    
    echo -e "${GREEN}Hot reload enabled. Run with: ./$BUILD_DIR/$OUTPUT --hot-reload${NC}"
else
    # Single executable
    $CC $CFLAGS $UNITY_FILE $LIBS -o $BUILD_DIR/$OUTPUT
fi

# Calculate compile time
END_TIME=$(date +%s%N)
ELAPSED_TIME=$(echo "scale=3; ($END_TIME - $START_TIME) / 1000000000" | bc)

# Output results
echo -e "${GREEN}✓ Build complete!${NC}"
echo -e "  Time: ${YELLOW}${ELAPSED_TIME}s${NC}"
echo -e "  Mode: $MODE"
echo -e "  Output: $BUILD_DIR/$OUTPUT"

# Get executable size
SIZE=$(du -h $BUILD_DIR/$OUTPUT | cut -f1)
echo -e "  Size: $SIZE"

# Optional: Run immediately
if [ "$3" = "run" ]; then
    echo -e "${YELLOW}Starting editor...${NC}"
    ./$BUILD_DIR/$OUTPUT
fi

# Watch mode for development
if [ "$3" = "watch" ]; then
    echo -e "${YELLOW}Watch mode enabled. Rebuilding on changes...${NC}"
    
    # Use inotifywait if available
    if command -v inotifywait &> /dev/null; then
        while true; do
            inotifywait -r -e modify,create,delete \
                --exclude '\.(o|so|swp|tmp)$' \
                src/ systems/ handmade_platform.h handmade_platform_linux.c editor_main.c \
                2>/dev/null
            
            clear
            echo -e "${YELLOW}Changes detected. Rebuilding...${NC}"
            ./$0 $MODE $HOT_RELOAD
            
            if [ "$HOT_RELOAD" = "yes" ]; then
                echo -e "${GREEN}Module reloaded. Editor will hot-reload automatically.${NC}"
            fi
        done
    else
        echo -e "${RED}inotifywait not found. Install inotify-tools for watch mode.${NC}"
        exit 1
    fi
fi