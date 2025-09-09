#!/bin/bash

# Build script for professional game engine editor
# Zero dependencies except system libraries

set -e

echo "Building Professional Game Engine Editor..."

# Compiler settings
CC=gcc
CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=c99"
LDFLAGS="-lX11 -lGL -lm -lpthread"

# Build mode
MODE=${1:-debug}

if [ "$MODE" == "release" ]; then
    echo "Building in RELEASE mode"
    CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops -DNDEBUG"
elif [ "$MODE" == "debug" ]; then
    echo "Building in DEBUG mode"
    CFLAGS="$CFLAGS -g -O0 -DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1"
else
    echo "Unknown build mode: $MODE (use 'debug' or 'release')"
    exit 1
fi

# Platform detection
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
    LDFLAGS="$LDFLAGS -lrt"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
    LDFLAGS="-framework Cocoa -framework OpenGL"
else
    echo "Unsupported platform: $OSTYPE"
    exit 1
fi

# Create build directory
mkdir -p build

# Include paths
INCLUDES="-I. -I.. -I../gui -I../renderer -I/usr/include"

# Source files
SOURCES="editor_demo.c handmade_editor_dock.c"

# Compile
echo "Compiling editor..."
$CC $CFLAGS $INCLUDES -o build/editor_demo $SOURCES $LDFLAGS

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Run with: ./build/editor_demo"
    
    # Create run script
    cat > run_editor.sh << 'EOF'
#!/bin/bash
cd build
./editor_demo
EOF
    chmod +x run_editor.sh
    
    echo "Or use: ./run_editor.sh"
else
    echo "Build failed!"
    exit 1
fi