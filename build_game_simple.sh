#!/bin/bash

# Simple build script for standalone Crystal Dungeons

echo "Building Crystal Dungeons (Standalone)..."

# Simple compilation with X11
gcc -o crystal_dungeons_game \
    game/game_standalone.c \
    -lX11 -lm \
    -O2 -g \
    -Wall -Wno-unused-function -Wno-unused-variable \
    -I. -Igame

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Run with: ./crystal_dungeons_game"
    chmod +x crystal_dungeons_game
else
    echo "Build failed!"
    exit 1
fi