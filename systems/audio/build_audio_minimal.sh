#!/bin/bash

echo "====================================="
echo "Building Minimal Handmade Audio System"
echo "100% Arena Allocation - Zero malloc/free"
echo "====================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Compiler settings - handmade compliant
CC=gcc
CFLAGS="-O3 -march=native -mtune=native -Wall -Wextra -std=c99"
CFLAGS="$CFLAGS -mavx2 -msse4.2 -mfma -ffast-math -funroll-loops -fprefetch-loop-arrays"

# Libraries - minimal dependencies
LIBS="-lasound -lpthread -lm -lrt"

# Directories
BUILD_DIR="build"
BIN_DIR="../../binaries"

# Create directories
mkdir -p $BUILD_DIR
mkdir -p $BIN_DIR

echo "Compiler flags: $CFLAGS"
echo "Libraries: $LIBS"
echo ""

# Build minimal audio library
echo "Building minimal audio library..."
$CC -c handmade_audio_minimal.c -o $BUILD_DIR/handmade_audio_minimal.o $CFLAGS
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to compile audio library${NC}"
    exit 1
fi

# Create static library
ar rcs $BUILD_DIR/libhandmade_audio_minimal.a $BUILD_DIR/handmade_audio_minimal.o
echo -e "${GREEN}Created: $BUILD_DIR/libhandmade_audio_minimal.a${NC}"

# Build minimal demo
echo ""
echo "Building minimal audio demo..."
$CC audio_demo_minimal.c $BUILD_DIR/libhandmade_audio_minimal.a -o $BIN_DIR/audio_demo_minimal $CFLAGS $LIBS
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build audio demo${NC}"
    exit 1
fi
echo -e "${GREEN}Created: $BIN_DIR/audio_demo_minimal${NC}"

# Check for malloc/free symbols
echo ""
echo "Checking for malloc/free symbols..."
if nm $BUILD_DIR/libhandmade_audio_minimal.a | grep -E "U (malloc|free|calloc|realloc)" > /dev/null; then
    echo -e "${RED}WARNING: Found malloc/free symbols in library!${NC}"
    nm $BUILD_DIR/libhandmade_audio_minimal.a | grep -E "U (malloc|free|calloc|realloc)"
else
    echo -e "${GREEN}✓ No malloc/free symbols found - 100% handmade compliant!${NC}"
fi

# Check demo for malloc/free
if nm $BIN_DIR/audio_demo_minimal | grep -E "U (malloc|free|calloc|realloc)" > /dev/null; then
    echo -e "${YELLOW}Note: Demo may link to system libraries that use malloc/free${NC}"
else
    echo -e "${GREEN}✓ Demo is malloc/free clean${NC}"
fi

echo ""
echo "====================================="
echo -e "${GREEN}Build complete!${NC}"
echo "Run demo: $BIN_DIR/audio_demo_minimal"
echo "====================================="