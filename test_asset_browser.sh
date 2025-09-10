#!/bin/bash

echo "============================================"
echo " Handmade Engine Asset Browser Test"
echo "============================================"
echo ""
echo "This test verifies the asset browser functionality:"
echo ""

# Check if executable exists
if [ ! -f "./handmade_foundation" ]; then
    echo "ERROR: handmade_foundation not found. Run ./build_minimal.sh first."
    exit 1
fi

# Check if assets directory exists
if [ ! -d "./assets" ]; then
    echo "ERROR: assets directory not found."
    echo "Creating sample assets..."
    if [ -f "./create_test_assets" ]; then
        ./create_test_assets
    else
        echo "ERROR: create_test_assets not found. Compile it first."
        exit 1
    fi
fi

echo "✓ Executable found: handmade_foundation"
echo "✓ Assets directory found with:"
echo "  - $(ls assets/textures/*.bmp 2>/dev/null | wc -l) textures"
echo "  - $(ls assets/models/*.obj 2>/dev/null | wc -l) models"
echo "  - $(ls assets/sounds/*.wav 2>/dev/null | wc -l) sounds"
echo "  - $(ls assets/shaders/*.* 2>/dev/null | wc -l) shaders"
echo ""
echo "CONTROLS:"
echo "  F1    - Toggle Scene Hierarchy"
echo "  F2    - Toggle Property Inspector"
echo "  F3    - Toggle Asset Browser"
echo "  F4    - Toggle Performance Stats"
echo "  1-4   - Select different tools"
echo "  ESC   - Quit"
echo ""
echo "IN ASSET BROWSER:"
echo "  Click on folders to navigate"
echo "  Double-click assets to load them"
echo "  View thumbnails for textures"
echo "  Switch between thumbnail and list view"
echo ""
echo "Starting editor..."
echo "============================================"
echo ""

# Run the editor
./handmade_foundation