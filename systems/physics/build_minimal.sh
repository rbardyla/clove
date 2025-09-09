#!/bin/bash

# Minimal Physics Engine Build - Core Functionality Only
# Builds and tests the basic physics engine without problematic modules

set -e

echo "Building Minimal Handmade Physics Engine..."

# Create build directory
mkdir -p build
cd build

# Compiler settings
CC="gcc"
CFLAGS=(
    "-O2"                    # Moderate optimization
    "-march=native" 
    "-Wall"
    "-Wno-unused-parameter"
    "-Wno-unused-variable" 
    "-Wno-missing-braces"    # Suppress math library warnings
    "-std=c99"
    "-g"                     # Debug symbols
    
    # Include paths
    "-I."
    "-I../.."
    "-I../../src"
    "-I../renderer"
)

LDFLAGS=(
    "-lm"           # Math library
)

DEFINES=(
    "-DHANDMADE_DEBUG=1"      # Enable debug mode
    "-DHANDMADE_RDTSC=1"      # Enable CPU timing
)

echo "Compiling core physics engine..."

# Compile only the core physics module (avoid the problematic collision/solver modules for now)
echo "  Compiling handmade_physics.c..."
$CC "${CFLAGS[@]}" "${DEFINES[@]}" -c "../handmade_physics.c" -o "handmade_physics.o"

echo "Creating minimal physics library..."
ar rcs libphysics_minimal.a handmade_physics.o

echo "Compiling minimal test..."
$CC "${CFLAGS[@]}" "${DEFINES[@]}" "../physics_minimal_working_test.c" -L. -lphysics_minimal "${LDFLAGS[@]}" -o physics_minimal_test

echo ""
echo "Running minimal physics test..."
echo "================================"
./physics_minimal_test

echo ""
echo "Minimal build complete! Generated files:"
echo "  - libphysics_minimal.a: Core physics library"
echo "  - physics_minimal_test: Basic functionality test"

cd ..