#!/bin/bash

# Build script for integrated editor + game demo
# Builds with proper dependency order and includes

echo "Building integrated editor + game demo..."

# Set build flags
CC="gcc"
CFLAGS="-O2 -march=native -mavx2 -mfma -ffast-math -Wall -Wno-unused-parameter -Wno-unused-function"
INCLUDES="-I. -Isystems -Igame"
LIBS="-lX11 -lXext -lm -pthread"

# Create build directory
mkdir -p build

# Build the integrated demo as a single unit
echo "Compiling editor demo..."
$CC $CFLAGS $INCLUDES -o build/editor_demo \
    editor_game_demo.c \
    game/crystal_dungeons.c \
    game/sprite_assets.c \
    game/neural_enemies.c \
    game/game_audio.c \
    systems/ai/handmade_neural.c \
    $LIBS 2>&1 | tee build/editor_build.log

if [ $? -eq 0 ]; then
    echo "Build successful! Run with: ./build/editor_demo"
else
    echo "Build failed. Check build/editor_build.log for errors"
    exit 1
fi