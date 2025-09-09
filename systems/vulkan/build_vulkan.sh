#!/bin/bash

# Handmade Vulkan Renderer Build Script
# Zero dependencies except Vulkan SDK
# Optimized for maximum performance

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Handmade Vulkan Renderer Build${NC}"
echo -e "${GREEN}========================================${NC}"

# Detect platform
PLATFORM=""
PLATFORM_DEFINE=""
PLATFORM_LIBS=""

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
    PLATFORM_DEFINE="-DPLATFORM_LINUX"
    PLATFORM_LIBS="-lxcb -lX11 -lX11-xcb -ldl -lpthread -lm"
    echo -e "${YELLOW}Platform: Linux${NC}"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
    PLATFORM_DEFINE="-DPLATFORM_MACOS"
    PLATFORM_LIBS="-framework Cocoa -framework Metal -framework QuartzCore"
    echo -e "${YELLOW}Platform: macOS${NC}"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="windows"
    PLATFORM_DEFINE="-DPLATFORM_WINDOWS"
    PLATFORM_LIBS="-luser32 -lgdi32 -lkernel32"
    echo -e "${YELLOW}Platform: Windows${NC}"
else
    echo -e "${RED}Unsupported platform: $OSTYPE${NC}"
    exit 1
fi

# Find Vulkan SDK
VULKAN_SDK_PATH=""
if [ -n "$VULKAN_SDK" ]; then
    VULKAN_SDK_PATH="$VULKAN_SDK"
elif [ -d "/usr/include/vulkan" ]; then
    VULKAN_SDK_PATH="/usr"
elif [ -d "$HOME/VulkanSDK" ]; then
    # Find latest version in home directory
    VULKAN_SDK_PATH=$(ls -d $HOME/VulkanSDK/*/x86_64 2>/dev/null | head -n1)
fi

if [ -z "$VULKAN_SDK_PATH" ]; then
    echo -e "${RED}Error: Vulkan SDK not found!${NC}"
    echo "Please install Vulkan SDK or set VULKAN_SDK environment variable"
    exit 1
fi

echo -e "${YELLOW}Vulkan SDK: $VULKAN_SDK_PATH${NC}"

# Compiler settings
CC="gcc"
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
INCLUDES="-I. -I$VULKAN_SDK_PATH/include"
LIBS="-L$VULKAN_SDK_PATH/lib -lvulkan $PLATFORM_LIBS"

# Build configurations
BUILD_TYPE="${1:-release}"

if [ "$BUILD_TYPE" = "debug" ]; then
    echo -e "${YELLOW}Build type: Debug${NC}"
    CFLAGS="$CFLAGS -g -O0 -DDEBUG"
    OUTPUT_DIR="build/debug"
elif [ "$BUILD_TYPE" = "release" ]; then
    echo -e "${YELLOW}Build type: Release${NC}"
    # PERFORMANCE: Aggressive optimizations
    CFLAGS="$CFLAGS -O3 -march=native -mtune=native -ffast-math"
    CFLAGS="$CFLAGS -funroll-loops -fomit-frame-pointer"
    CFLAGS="$CFLAGS -DNDEBUG"
    OUTPUT_DIR="build/release"
elif [ "$BUILD_TYPE" = "profile" ]; then
    echo -e "${YELLOW}Build type: Profile${NC}"
    CFLAGS="$CFLAGS -O2 -g -pg -DPROFILE"
    OUTPUT_DIR="build/profile"
else
    echo -e "${RED}Unknown build type: $BUILD_TYPE${NC}"
    echo "Usage: $0 [debug|release|profile]"
    exit 1
fi

# Create output directory
mkdir -p $OUTPUT_DIR

# Source files
VULKAN_SOURCES=(
    "handmade_vulkan.c"
    "vulkan_pipeline.c"
    "vulkan_resources.c"
    "vulkan_techniques.c"
    "vulkan_raymarch.c"
)

DEMO_SOURCES=(
    "vulkan_demo.c"
)

# Platform layer (assumed to exist)
PLATFORM_SOURCES=(
    "platform.c"
)

# Shader compilation (if GLSL files exist)
SHADER_DIR="shaders"
if [ -d "$SHADER_DIR" ]; then
    echo -e "${GREEN}Compiling shaders...${NC}"
    
    # Find glslangValidator
    GLSLANG=""
    if [ -x "$VULKAN_SDK_PATH/bin/glslangValidator" ]; then
        GLSLANG="$VULKAN_SDK_PATH/bin/glslangValidator"
    elif command -v glslangValidator &> /dev/null; then
        GLSLANG="glslangValidator"
    fi
    
    if [ -n "$GLSLANG" ]; then
        for shader in $SHADER_DIR/*.vert $SHADER_DIR/*.frag $SHADER_DIR/*.comp; do
            if [ -f "$shader" ]; then
                output="$OUTPUT_DIR/$(basename ${shader}).spv"
                echo "  Compiling $(basename $shader)..."
                $GLSLANG -V "$shader" -o "$output"
            fi
        done
    else
        echo -e "${YELLOW}Warning: glslangValidator not found, skipping shader compilation${NC}"
    fi
fi

# Compile object files
echo -e "${GREEN}Compiling Vulkan renderer...${NC}"

OBJECTS=""

# Compile Vulkan sources
for source in "${VULKAN_SOURCES[@]}"; do
    if [ -f "$source" ]; then
        object="$OUTPUT_DIR/$(basename ${source%.c}.o)"
        echo "  Compiling $source..."
        $CC $CFLAGS $PLATFORM_DEFINE $INCLUDES -c "$source" -o "$object"
        OBJECTS="$OBJECTS $object"
    fi
done

# Compile platform sources
for source in "${PLATFORM_SOURCES[@]}"; do
    if [ -f "$source" ]; then
        object="$OUTPUT_DIR/$(basename ${source%.c}.o)"
        echo "  Compiling $source..."
        $CC $CFLAGS $PLATFORM_DEFINE $INCLUDES -c "$source" -o "$object"
        OBJECTS="$OBJECTS $object"
    fi
done

# Build static library
LIBRARY="$OUTPUT_DIR/libvulkan_renderer.a"
echo -e "${GREEN}Creating static library...${NC}"
ar rcs "$LIBRARY" $OBJECTS
echo "  Created $LIBRARY"

# Build demo executable
echo -e "${GREEN}Building demo application...${NC}"
DEMO_OBJECTS=""
for source in "${DEMO_SOURCES[@]}"; do
    if [ -f "$source" ]; then
        object="$OUTPUT_DIR/$(basename ${source%.c}.o)"
        echo "  Compiling $source..."
        $CC $CFLAGS $PLATFORM_DEFINE $INCLUDES -c "$source" -o "$object"
        DEMO_OBJECTS="$DEMO_OBJECTS $object"
    fi
done

EXECUTABLE="$OUTPUT_DIR/vulkan_demo"
echo "  Linking $EXECUTABLE..."
$CC $CFLAGS $DEMO_OBJECTS "$LIBRARY" $LIBS -o "$EXECUTABLE"

# Generate run script
RUN_SCRIPT="$OUTPUT_DIR/run.sh"
cat > "$RUN_SCRIPT" << 'EOF'
#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Set Vulkan validation layers for debug builds
if [[ "$DIR" == *"debug"* ]]; then
    export VK_LAYER_PATH="$VULKAN_SDK/etc/vulkan/explicit_layer.d"
    export VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"
    echo "Vulkan validation layers enabled"
fi

# Run the demo
cd "$DIR"
./vulkan_demo "$@"
EOF

chmod +x "$RUN_SCRIPT"

# Performance test script for release builds
if [ "$BUILD_TYPE" = "release" ]; then
    PERF_SCRIPT="$OUTPUT_DIR/perf_test.sh"
    cat > "$PERF_SCRIPT" << 'EOF'
#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

echo "Running performance test..."
echo "=========================="

# CPU affinity for consistent results
if command -v taskset &> /dev/null; then
    # Pin to first CPU core
    taskset -c 0 ./vulkan_demo --benchmark
else
    ./vulkan_demo --benchmark
fi

# Collect performance counters if available
if command -v perf &> /dev/null; then
    echo ""
    echo "Collecting performance counters..."
    perf stat -e cycles,instructions,cache-misses,branch-misses ./vulkan_demo --benchmark-short
fi
EOF
    chmod +x "$PERF_SCRIPT"
    echo -e "${GREEN}Created performance test script: $PERF_SCRIPT${NC}"
fi

# Print summary
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "Library:    ${YELLOW}$LIBRARY${NC}"
echo -e "Executable: ${YELLOW}$EXECUTABLE${NC}"
echo -e "Run script: ${YELLOW}$RUN_SCRIPT${NC}"
echo ""
echo -e "To run the demo:"
echo -e "  ${YELLOW}$RUN_SCRIPT${NC}"
echo ""

# Print size information
echo "Binary sizes:"
ls -lh "$LIBRARY" "$EXECUTABLE" | awk '{print "  " $9 ": " $5}'

# Print performance hints for release build
if [ "$BUILD_TYPE" = "release" ]; then
    echo ""
    echo -e "${GREEN}Performance hints:${NC}"
    echo "  - Disable compositor for best performance"
    echo "  - Use exclusive fullscreen mode"
    echo "  - Set CPU governor to performance:"
    echo "    sudo cpupower frequency-set -g performance"
fi