#!/bin/bash

# Build script for handmade threading system
# Compiles with maximum optimization and threading support

echo "=== Building Handmade Threading System ==="
echo

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops"
CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-parameter -std=c11"
CFLAGS="$CFLAGS -D_GNU_SOURCE -pthread"
LDFLAGS="-pthread -lm -lrt"

# Debug build option
if [ "$1" == "debug" ]; then
    echo "Building DEBUG version..."
    CFLAGS="-g -O0 -DHANDMADE_DEBUG=1 -Wall -Wextra -std=c11 -D_GNU_SOURCE -pthread"
elif [ "$1" == "profile" ]; then
    echo "Building PROFILE version..."
    CFLAGS="$CFLAGS -pg"
    LDFLAGS="$LDFLAGS -pg"
else
    echo "Building RELEASE version..."
fi

# Build threading library
echo "Compiling threading system..."
$CC $CFLAGS -c handmade_threading.c -o handmade_threading.o
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile handmade_threading.c"
    exit 1
fi

# Build test program
echo "Building test program..."
$CC $CFLAGS test_threading.c handmade_threading.o $LDFLAGS -o test_threading
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build test program"
    exit 1
fi

echo
echo "Build successful!"
echo "Run ./test_threading to test the threading system"
echo

# Optional: Run the test immediately
if [ "$2" == "run" ]; then
    echo "Running threading test..."
    echo "========================="
    ./test_threading
fi