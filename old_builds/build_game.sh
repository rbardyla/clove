#!/bin/bash

# Build script for Crystal Dungeons game

echo "Building Crystal Dungeons..."

# Compiler flags
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
CFLAGS="$CFLAGS -I. -Isrc -Isystems -Igame"
CFLAGS="$CFLAGS -DHANDMADE_INTERNAL=1"

# Platform-specific flags
PLATFORM_FLAGS="-lX11 -lXext -lm -lpthread -ldl"

# Optimization flags
if [ "$1" == "debug" ]; then
    echo "Building DEBUG configuration..."
    CFLAGS="$CFLAGS -g -O0 -DHANDMADE_DEBUG=1"
elif [ "$1" == "release" ]; then
    echo "Building RELEASE configuration..."
    CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
else
    echo "Building DEFAULT configuration..."
    CFLAGS="$CFLAGS -O2 -g"
fi

# Create build directory
mkdir -p build

# Build the game
echo "Compiling game..."
gcc $CFLAGS \
    game/main_game.c \
    game/crystal_dungeons.c \
    game/sprite_assets.c \
    game/neural_enemies.c \
    game/game_audio.c \
    src/platform_linux.c \
    systems/renderer/handmade_renderer.c \
    systems/physics/handmade_physics.c \
    systems/ai/handmade_dnc_enhanced.c \
    systems/audio/handmade_audio.c \
    systems/gui/handmade_gui.c \
    $PLATFORM_FLAGS \
    -o crystal_dungeons

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Run with: ./crystal_dungeons"
    
    # Make executable
    chmod +x crystal_dungeons
else
    echo "Build failed!"
    exit 1
fi