#!/bin/bash

# Neural Kingdom - Build Script
# Building the future of gaming, one neural network at a time!

echo "🚀 Building Neural Kingdom - The AI Revolution!"

# Build configuration
CC="gcc"
CFLAGS="-O3 -march=native -mavx2 -mfma -ffast-math"
CFLAGS="$CFLAGS -Wall -Wno-unused-parameter -Wno-unused-function -D_POSIX_C_SOURCE=199309L"
INCLUDES="-I../../ -I../../systems -I../../game -Isrc"
LIBS="-lX11 -lXext -lm -pthread"

# Create build directory
mkdir -p build

# Source files
SOURCES=(
    "src/neural_kingdom.c"
    "../../systems/ai/handmade_neural.c"
)

echo "Compiling Neural Kingdom..."
echo "Target: 144 FPS | Memory: <100MB | NPCs: 100+"

# Build the game
$CC $CFLAGS $INCLUDES -o build/neural_kingdom "${SOURCES[@]}" $LIBS

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "🎮 Run with: ./build/neural_kingdom"
    echo ""
    echo "Remember the mission:"
    echo "• Beat Unity/Unreal performance"
    echo "• Create impossible AI"  
    echo "• Zero dependencies"
    echo "• Perfect game feel"
    echo ""
    echo "🔥 LET'S SHATTER THE INDUSTRY! 🔥"
else
    echo "❌ Build failed!"
    echo "Check the errors above and fix them."
    exit 1
fi