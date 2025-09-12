#!/bin/bash

echo "Building Quick Renderer Test..."

# Simple build for quick test
gcc -O2 -std=c99 -I. -I../../src \
    test_quick.c \
    handmade_renderer.c \
    handmade_opengl.c \
    handmade_platform_linux.c \
    handmade_math.c \
    -lGL -lX11 -lm -lpthread -ldl \
    -o test_quick

if [ $? -eq 0 ]; then
    echo "✓ Build successful"
    echo "Run with: ./test_quick"
else
    echo "✗ Build failed"
    exit 1
fi