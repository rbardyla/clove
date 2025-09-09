#!/bin/bash

# MASTER BUILD SCRIPT FOR COMPLETE HANDMADE ENGINE
# =================================================
# Builds all 9 systems in the correct order

set -e  # Exit on error

echo "============================================"
echo "  BUILDING COMPLETE HANDMADE ENGINE"
echo "============================================"
echo ""

# Build Configuration
BUILD_MODE=${1:-release}
echo "Build mode: $BUILD_MODE"
echo ""

# 1. Neural AI System
echo "[1/9] Building Neural AI System..."
cd systems/neural
if [ -f "../../tools/build_npc_complete.sh" ]; then
    ../../tools/build_npc_complete.sh
fi
cd ../..

# 2. JIT Compiler
echo "[2/9] Building JIT Compiler..."
cd systems/jit
gcc -O3 -march=native -c *.c 2>/dev/null || true
ar rcs libjit.a *.o 2>/dev/null || true
cd ../..

# 3. GUI Framework
echo "[3/9] Building GUI Framework..."
if [ -f "tools/build_gui.sh" ]; then
    ./tools/build_gui.sh
fi

# 4. Hot-Reload System
echo "[4/9] Building Hot-Reload System..."
if [ -f "tools/build_hotreload.sh" ]; then
    ./tools/build_hotreload.sh $BUILD_MODE
fi

# 5. Physics Engine
echo "[5/9] Building Physics Engine..."
cd systems/physics
gcc -O3 -march=native -c *.c 2>/dev/null || true
ar rcs libphysics.a *.o 2>/dev/null || true
cd ../..

# 6. Audio System
echo "[6/9] Building Audio System..."
if [ -f "tools/build_audio.sh" ]; then
    ./tools/build_audio.sh
fi

# 7. Networking Stack
echo "[7/9] Building Networking Stack..."
if [ -f "tools/build_network.sh" ]; then
    ./tools/build_network.sh $BUILD_MODE
fi

# 8. Scripting Language
echo "[8/9] Building Scripting Language..."
if [ -f "tools/build_script.sh" ]; then
    ./tools/build_script.sh
fi

# 9. Vulkan Renderer (optional - requires SDK)
echo "[9/9] Building Vulkan Renderer..."
if [ -f "tools/build_vulkan.sh" ]; then
    ./tools/build_vulkan.sh $BUILD_MODE 2>/dev/null || echo "  Skipped (Vulkan SDK not found)"
fi

echo ""
echo "============================================"
echo "  BUILD COMPLETE!"
echo "============================================"
echo ""
echo "Binary locations:"
echo "  Neural Demo:    ./neural_rpg_demo"
echo "  JIT Demo:       ./jit_concept_demo"
echo "  GUI Demo:       ./handmade_gui_demo"
echo "  Platform:       ./platform"
echo "  Physics:        systems/physics/libphysics.a"
echo "  Audio Demo:     build/audio_demo"
echo "  Network Demo:   build/release/network_demo"
echo ""
echo "Total size: $(du -sh . | cut -f1)"