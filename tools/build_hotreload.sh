#!/bin/bash

# PERFORMANCE: Build script optimized for fast compilation and hot reload
# Separate platform and game builds for minimal rebuild time

set -e  # Exit on error

echo "[BUILD] Handmade Engine Hot Reload Build"
echo "========================================"

# Compiler flags
CC="gcc"
COMMON_FLAGS="-Wall -Wextra -std=c99 -march=native"
DEBUG_FLAGS="-g -O0 -DDEBUG"
RELEASE_FLAGS="-O3 -ffast-math -funroll-loops -DNDEBUG"
PROFILE_FLAGS="-O2 -g -pg"

# Select build mode
BUILD_MODE="${1:-release}"

case "$BUILD_MODE" in
    debug)
        CFLAGS="$COMMON_FLAGS $DEBUG_FLAGS"
        echo "[BUILD] Debug mode"
        ;;
    release)
        CFLAGS="$COMMON_FLAGS $RELEASE_FLAGS"
        echo "[BUILD] Release mode"
        ;;
    profile)
        CFLAGS="$COMMON_FLAGS $PROFILE_FLAGS"
        echo "[BUILD] Profile mode"
        ;;
    *)
        echo "Usage: $0 [debug|release|profile]"
        exit 1
        ;;
esac

# Platform executable flags (simplified - no OpenGL)
PLATFORM_LIBS="-lX11 -ldl -lm -lpthread"

# Game shared library flags  
# -fPIC: Position Independent Code (required for shared libraries)
# -shared: Create shared library
# -fvisibility=hidden: Hide all symbols by default (we mark exports explicitly)
GAME_FLAGS="-fPIC -shared -fvisibility=hidden"

# Build hot reload system
echo "[BUILD] Compiling hot reload system..."
$CC $CFLAGS -c handmade_hotreload.c -o handmade_hotreload.o

# Build platform executable (using simplified version)
echo "[BUILD] Compiling platform layer..."
START_TIME=$(date +%s%N)
$CC $CFLAGS handmade_hotreload.o platform_simple.c -o platform $PLATFORM_LIBS
END_TIME=$(date +%s%N)
PLATFORM_TIME=$((($END_TIME - $START_TIME) / 1000000))
echo "  Platform built in ${PLATFORM_TIME}ms"

# Build game shared library
echo "[BUILD] Compiling game library..."
START_TIME=$(date +%s%N)
$CC $CFLAGS $GAME_FLAGS game_main.c -o game.so -lm
END_TIME=$(date +%s%N)
GAME_TIME=$((($END_TIME - $START_TIME) / 1000000))
echo "  Game library built in ${GAME_TIME}ms"

# Check file sizes
PLATFORM_SIZE=$(stat -c%s platform)
GAME_SIZE=$(stat -c%s game.so)

echo ""
echo "[BUILD] Build complete!"
echo "  Platform executable: $(($PLATFORM_SIZE / 1024))KB"
echo "  Game library:        $(($GAME_SIZE / 1024))KB"
echo "  Total build time:    $(($PLATFORM_TIME + $GAME_TIME))ms"
echo ""
echo "[BUILD] To run: ./platform"
echo "[BUILD] To hot reload: Edit game_main.c and run './build_hotreload.sh' or just 'make game'"
echo ""

# Create a simple Makefile for quick game rebuilds
cat > Makefile << 'EOF'
# Quick rebuild for hot reload testing
.PHONY: all game platform clean watch

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -march=native -O3 -ffast-math
GAME_FLAGS = -fPIC -shared -fvisibility=hidden
PLATFORM_LIBS = -lX11 -lGL -ldl -lm -lpthread

all: platform game.so

platform: platform_main.c handmade_hotreload.o
	$(CC) $(CFLAGS) $^ -o $@ $(PLATFORM_LIBS)

handmade_hotreload.o: handmade_hotreload.c handmade_hotreload.h
	$(CC) $(CFLAGS) -c $< -o $@

game.so: game_main.c handmade_hotreload.h
	@echo "[HOT RELOAD] Rebuilding game..."
	@$(CC) $(CFLAGS) $(GAME_FLAGS) $< -o $@ -lm
	@echo "[HOT RELOAD] Game rebuilt - will auto-reload!"

# Quick game rebuild (most common during development)
game: game.so

# Watch for changes and auto-rebuild
watch:
	@echo "Watching for changes... (Ctrl+C to stop)"
	@while true; do \
		inotifywait -q -e modify game_main.c; \
		make -s game; \
	done

clean:
	rm -f platform game.so handmade_hotreload.o

run: all
	./platform

# Performance build with profiling
profile: CFLAGS += -g -pg
profile: all

# Debug build with symbols
debug: CFLAGS = -Wall -Wextra -std=c99 -g -O0 -DDEBUG
debug: all
EOF

echo "[BUILD] Makefile created for quick rebuilds:"
echo "  make         - Build everything"
echo "  make game    - Rebuild only game.so (for hot reload)"
echo "  make watch   - Auto-rebuild on file changes"
echo "  make run     - Build and run"
echo "  make clean   - Clean build files"

# Make the script executable
chmod +x build_hotreload.sh