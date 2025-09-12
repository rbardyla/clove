#!/bin/bash

echo "================================"
echo "Building Professional Game Editor"
echo "================================"

# Create build directory
mkdir -p build

# Build the editor using existing systems
echo "Compiling editor..."
gcc -g -O2 -Wall -Wextra -std=c99 \
    -I. \
    -Isrc \
    -Isystems/renderer \
    -Isystems/gui \
    simple_editor.c \
    -lGL -lX11 -lm -lpthread \
    -o build/game_editor

if [ $? -eq 0 ]; then
    echo "================================"
    echo "Build successful!"
    echo "Run with: ./build/game_editor"
    echo "================================"
else
    echo "================================"
    echo "Build failed!"
    echo "================================"
    exit 1
fi

# Make executable
chmod +x build/game_editor