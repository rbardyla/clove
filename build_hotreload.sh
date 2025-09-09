#!/bin/bash

# Hot Reload Build Script
# Builds platform executable and hot-reloadable game module

set -e

# Configuration
BUILD_DIR="build"
PLATFORM_EXE="editor_hot"
MODULE_SO="editor.so"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Build mode
MODE=${1:-debug}

echo -e "${CYAN}==================================${NC}"
echo -e "${CYAN}   Hot Reload Build System${NC}"
echo -e "${CYAN}==================================${NC}"
echo ""

# Compiler settings
CC="gcc"
COMMON_FLAGS="-std=c11 -march=native -Wall -Wno-unused-function"
INCLUDES="-I."
LIBS="-lGL -lX11 -lm -lpthread -ldl"

# Mode-specific flags
case $MODE in
    debug)
        echo -e "${YELLOW}Building DEBUG mode with hot reload...${NC}"
        CFLAGS="$COMMON_FLAGS -O0 -g -DDEBUG -DHOT_RELOAD_ENABLED"
        ;;
    release)
        echo -e "${YELLOW}Building RELEASE mode with hot reload...${NC}"
        CFLAGS="$COMMON_FLAGS -O3 -DNDEBUG -DHOT_RELOAD_ENABLED"
        ;;
    *)
        echo -e "${RED}Unknown mode: $MODE${NC}"
        exit 1
        ;;
esac

mkdir -p $BUILD_DIR

# Step 1: Build hot reload system
echo -e "${GREEN}[1/3] Building hot reload system...${NC}"
$CC $CFLAGS $INCLUDES -c handmade_hotreload.c -o $BUILD_DIR/hotreload.o

# Step 2: Build platform executable (loads the module)
echo -e "${GREEN}[2/3] Building platform executable...${NC}"
cat > $BUILD_DIR/platform_main.c << 'EOF'
#include "handmade_platform.h"
#include "handmade_hotreload.h"
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    PlatformState platform = {0};
    HotReloadState hotreload = {0};
    
    // Initialize platform
    if (!PlatformInit(&platform, "Handmade Editor [Hot Reload]", 1920, 1080)) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize hot reload
    if (!HotReload->InitHotReload(&hotreload, "./build/editor.so")) {
        fprintf(stderr, "Failed to initialize hot reload\n");
        return 1;
    }
    
    // Get game API
    GameModuleAPI* game = GetCurrentGameAPI();
    
    // Initialize game
    if (game->GameInit) {
        game->GameInit(&platform);
    }
    
    printf("[Platform] Hot reload enabled - modify editor_hotreload.c and rebuild!\n");
    printf("[Platform] Watching for changes to ./build/editor.so\n");
    
    // Main loop
    f64 last_time = PlatformGetTime();
    f64 reload_check_time = 0;
    
    while (PlatformProcessEvents(&platform)) {
        // Clear frame memory
        platform.frame_arena.used = 0;
        
        // Check for hot reload every 0.5 seconds
        f64 current_time = PlatformGetTime();
        if (current_time - reload_check_time > 0.5) {
            if (HotReload->CheckForReload(&hotreload)) {
                printf("[Platform] Module change detected, reloading...\n");
                
                if (HotReload->PerformReload(&hotreload, &platform)) {
                    // Update game API pointer
                    game = GetCurrentGameAPI();
                    printf("[Platform] Reload successful!\n");
                } else {
                    fprintf(stderr, "[Platform] Reload failed!\n");
                }
            }
            reload_check_time = current_time;
        }
        
        // Calculate delta time
        f32 dt = (f32)(current_time - last_time);
        last_time = current_time;
        
        // Update and render
        if (game->GameUpdate) {
            game->GameUpdate(&platform, dt);
        }
        
        if (game->GameRender) {
            game->GameRender(&platform);
        }
        
        PlatformSwapBuffers(&platform);
    }
    
    // Shutdown
    if (game->GameShutdown) {
        game->GameShutdown(&platform);
    }
    
    HotReload->DumpHotReloadStats(&hotreload);
    PlatformShutdown(&platform);
    
    return 0;
}
EOF

# Include platform implementation inline
cat handmade_platform_linux.c | sed '/int main/,/^}/d' | sed '/GameInit/,/GameShutdown/d' > $BUILD_DIR/platform_impl.c

$CC $CFLAGS $INCLUDES -DHOT_RELOAD_PLATFORM \
    $BUILD_DIR/platform_main.c \
    $BUILD_DIR/platform_impl.c \
    $BUILD_DIR/hotreload.o \
    -o $BUILD_DIR/$PLATFORM_EXE $LIBS

# Step 3: Build game module (hot-reloadable)
echo -e "${GREEN}[3/3] Building hot-reloadable editor module...${NC}"
$CC $CFLAGS $INCLUDES -DHOT_RELOAD_MODULE -fPIC -shared \
    editor_hotreload.c \
    -o $BUILD_DIR/$MODULE_SO \
    -lGL -lm

echo ""
echo -e "${GREEN}==================================${NC}"
echo -e "${GREEN}✓ Hot Reload Build Complete!${NC}"
echo -e "${GREEN}==================================${NC}"
echo ""
echo -e "Platform executable: ${CYAN}$BUILD_DIR/$PLATFORM_EXE${NC}"
echo -e "Editor module:       ${CYAN}$BUILD_DIR/$MODULE_SO${NC}"
echo ""
echo -e "${YELLOW}Usage:${NC}"
echo -e "  1. Run the platform: ${CYAN}./$BUILD_DIR/$PLATFORM_EXE${NC}"
echo -e "  2. Edit ${CYAN}editor_hotreload.c${NC}"
echo -e "  3. Rebuild module:   ${CYAN}./build_hotreload.sh module${NC}"
echo -e "  4. Changes appear instantly without restart!"
echo ""

# Module-only rebuild (for hot reload)
if [ "$2" = "module" ]; then
    echo -e "${YELLOW}Rebuilding module only...${NC}"
    $CC $CFLAGS $INCLUDES -DHOT_RELOAD_MODULE -fPIC -shared \
        editor_hotreload.c \
        -o $BUILD_DIR/${MODULE_SO}.tmp \
        -lGL -lm
    
    # Atomic replace
    mv $BUILD_DIR/${MODULE_SO}.tmp $BUILD_DIR/$MODULE_SO
    echo -e "${GREEN}✓ Module rebuilt - will hot reload!${NC}"
fi