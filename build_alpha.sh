#!/bin/bash

echo "========================================"
echo "   NEURAL VILLAGE ALPHA BUILD SYSTEM"
echo "========================================"
echo "Building Alpha v0.001..."
echo

# Compile the alpha build with optimizations
gcc -o neural_village_alpha neural_village_alpha.c -lX11 -lm -O2 -march=native -DNDEBUG

if [ $? -eq 0 ]; then
    echo "✓ Alpha v0.001 compiled successfully!"
    echo
    
    # Display build information
    echo "BUILD INFORMATION:"
    echo "  Version: 0.001"
    echo "  Build Date: $(date)"
    echo "  File size: $(ls -lh neural_village_alpha | awk '{print $5}')"
    echo "  Architecture: $(uname -m)"
    echo "  Compiler: $(gcc --version | head -1)"
    echo
    
    echo "FEATURES INCLUDED:"
    echo "  ✓ Advanced Neural NPC AI"
    echo "  ✓ Behavioral Trees"
    echo "  ✓ 5-Trait Personality System"
    echo "  ✓ Dynamic Emotion Simulation"
    echo "  ✓ Social Relationship Networks"
    echo "  ✓ Memory Systems"
    echo "  ✓ Emergent Quest Generation"
    echo "  ✓ Village Economy Simulation"
    echo "  ✓ Performance Monitoring"
    echo "  ✓ Real-time AI Debug Interface"
    echo
    
    echo "PERFORMANCE OPTIMIZATIONS:"
    echo "  ✓ -O2 compiler optimizations"
    echo "  ✓ Native CPU instruction targeting"
    echo "  ✓ Frustum culling for tile rendering"
    echo "  ✓ Efficient memory layouts"
    echo "  ✓ Fixed timestep game loop"
    echo
    
    echo "ALPHA BUILD READY FOR TESTING!"
    echo "Run with: ./neural_village_alpha"
    echo
    
else
    echo "✗ Build failed!"
    exit 1
fi