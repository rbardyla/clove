#!/bin/bash
# Production Build Script for Renderer Integration with Hot Reload
# Achieves <1 second incremental builds

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Build configuration
BUILD_DIR="build"
CACHE_DIR="$BUILD_DIR/cache"
SHADER_DIR="assets/shaders"
OUTPUT="renderer_demo"

# Compiler settings for maximum speed
CC="clang"
CXX="clang++"
CFLAGS="-O3 -march=native -flto=thin -fno-exceptions -fno-rtti"
CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-parameter"
CFLAGS="$CFLAGS -DNDEBUG -DPLATFORM_LINUX"  # Adjust for your platform

# Development build flags (faster compilation)
DEV_FLAGS="-O0 -g -fsanitize=address -DDEBUG -DHOT_RELOAD_ENABLED"

# Libraries
LIBS="-lGL -lGLEW -lX11 -lpthread -lm -ldl"

# Source files
SOURCES=(
    "platform_linux.c"          # Platform layer
    "memory.c"                  # Memory system
    "renderer_opengl.c"         # OpenGL backend
    "render_graph.c"            # Render graph
    "material_system.c"         # Material system
    "shader_hot_reload_example.c" # Shader manager
    "main_renderer_integration.c" # Main application
)

# Header dependencies
HEADERS=(
    "platform.h"
    "memory.h"
    "renderer.h"
    "render_graph.h"
)

# Function to get file modification time
get_mtime() {
    stat -c %Y "$1" 2>/dev/null || echo 0
}

# Function to check if rebuild is needed
needs_rebuild() {
    local source=$1
    local object=$2
    
    if [ ! -f "$object" ]; then
        return 0  # Need rebuild - object doesn't exist
    fi
    
    local src_time=$(get_mtime "$source")
    local obj_time=$(get_mtime "$object")
    
    if [ $src_time -gt $obj_time ]; then
        return 0  # Need rebuild - source is newer
    fi
    
    # Check header dependencies
    for header in "${HEADERS[@]}"; do
        local hdr_time=$(get_mtime "$header")
        if [ $hdr_time -gt $obj_time ]; then
            return 0  # Need rebuild - header is newer
        fi
    done
    
    return 1  # No rebuild needed
}

# Function to compile with progress
compile_with_progress() {
    local source=$1
    local object=$2
    local flags=$3
    local index=$4
    local total=$5
    
    printf "[%2d/%2d] " $index $total
    
    if needs_rebuild "$source" "$object"; then
        echo -e "${YELLOW}Compiling${NC} $source..."
        
        # Time the compilation
        local start_time=$(date +%s%N)
        
        if $CC -c "$source" -o "$object" $flags $CFLAGS 2>/tmp/build_error.log; then
            local end_time=$(date +%s%N)
            local elapsed=$((($end_time - $start_time) / 1000000))
            echo -e "       ${GREEN}✓${NC} Done (${elapsed}ms)"
            return 0
        else
            echo -e "       ${RED}✗ Failed${NC}"
            cat /tmp/build_error.log
            return 1
        fi
    else
        echo -e "${BLUE}Cached${NC} $source"
        return 0
    fi
}

# Function to create shader file watcher
setup_shader_watcher() {
    cat > "$BUILD_DIR/shader_watcher.sh" << 'EOF'
#!/bin/bash
SHADER_DIR="assets/shaders"
echo "Watching shaders in $SHADER_DIR for changes..."

if command -v inotifywait >/dev/null 2>&1; then
    while true; do
        inotifywait -r -e modify,create,delete,move "$SHADER_DIR" 2>/dev/null
        echo -e "\033[1;33m[HOT RELOAD]\033[0m Shader changed, will reload on next frame"
    done
else
    echo "Install inotify-tools for automatic shader watching"
    echo "Manual reload: Press F5 in the application"
fi
EOF
    chmod +x "$BUILD_DIR/shader_watcher.sh"
}

# Function to build hot-reload module
build_hot_reload_module() {
    echo -e "${YELLOW}Building hot-reload module...${NC}"
    
    # Compile renderer as shared library for hot reload
    local module_sources=(
        "renderer_opengl.c"
        "material_system.c"
        "shader_hot_reload_example.c"
    )
    
    local objects=""
    for source in "${module_sources[@]}"; do
        local object="$CACHE_DIR/${source%.c}_shared.o"
        $CC -c -fPIC "$source" -o "$object" $DEV_FLAGS
        objects="$objects $object"
    done
    
    # Link shared library
    $CC -shared $objects -o "$BUILD_DIR/renderer.so" $LIBS
    echo -e "${GREEN}Hot-reload module built: renderer.so${NC}"
}

# Main build function
build() {
    local build_type=${1:-"release"}
    
    echo -e "${BLUE}════════════════════════════════════════${NC}"
    echo -e "${BLUE}   Renderer Build System v1.0${NC}"
    echo -e "${BLUE}   Build Type: $build_type${NC}"
    echo -e "${BLUE}════════════════════════════════════════${NC}"
    
    # Create directories
    mkdir -p "$BUILD_DIR"
    mkdir -p "$CACHE_DIR"
    mkdir -p "$SHADER_DIR"
    
    # Record start time
    local start_time=$(date +%s%N)
    
    # Determine flags
    local flags=$CFLAGS
    if [ "$build_type" = "dev" ]; then
        flags=$DEV_FLAGS
    fi
    
    # Compile sources
    local objects=""
    local index=1
    local total=${#SOURCES[@]}
    local failed=0
    
    for source in "${SOURCES[@]}"; do
        local object="$CACHE_DIR/${source%.c}.o"
        
        if compile_with_progress "$source" "$object" "$flags" $index $total; then
            objects="$objects $object"
        else
            failed=1
            break
        fi
        
        index=$((index + 1))
    done
    
    if [ $failed -eq 1 ]; then
        echo -e "${RED}Build failed!${NC}"
        exit 1
    fi
    
    # Link
    echo -e "${YELLOW}Linking...${NC}"
    if $CC $objects -o "$BUILD_DIR/$OUTPUT" $flags $LIBS; then
        echo -e "${GREEN}✓ Link successful${NC}"
    else
        echo -e "${RED}✗ Link failed${NC}"
        exit 1
    fi
    
    # Build hot-reload module if in dev mode
    if [ "$build_type" = "dev" ]; then
        build_hot_reload_module
        setup_shader_watcher
    fi
    
    # Calculate total time
    local end_time=$(date +%s%N)
    local elapsed=$((($end_time - $start_time) / 1000000))
    
    echo -e "${BLUE}════════════════════════════════════════${NC}"
    echo -e "${GREEN}Build completed in ${elapsed}ms${NC}"
    echo -e "Executable: ${BUILD_DIR}/${OUTPUT}"
    
    if [ "$build_type" = "dev" ]; then
        echo -e "Hot-reload module: ${BUILD_DIR}/renderer.so"
        echo -e "Shader watcher: ${BUILD_DIR}/shader_watcher.sh"
        echo -e ""
        echo -e "${YELLOW}To enable live shader editing:${NC}"
        echo -e "  1. Run: ${BUILD_DIR}/shader_watcher.sh &"
        echo -e "  2. Edit shaders in: ${SHADER_DIR}/"
        echo -e "  3. Changes appear instantly!"
    fi
    
    # Show size
    local size=$(du -h "$BUILD_DIR/$OUTPUT" | cut -f1)
    echo -e "Size: $size"
    echo -e "${BLUE}════════════════════════════════════════${NC}"
}

# Clean build
clean() {
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}Clean complete${NC}"
}

# Create example shaders
create_shaders() {
    echo -e "${YELLOW}Creating example shaders...${NC}"
    mkdir -p "$SHADER_DIR"
    
    # Standard vertex shader
    cat > "$SHADER_DIR/standard.vert" << 'EOF'
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_MVP;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

out vec3 v_Normal;
out vec2 v_TexCoord;
out vec3 v_WorldPos;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    v_WorldPos = (u_Model * vec4(a_Position, 1.0)).xyz;
    v_Normal = u_NormalMatrix * a_Normal;
    v_TexCoord = a_TexCoord;
}
EOF

    # Standard fragment shader
    cat > "$SHADER_DIR/standard.frag" << 'EOF'
#version 330 core
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec3 v_WorldPos;

uniform vec4 u_BaseColor;
uniform vec3 u_LightDir;
uniform vec3 u_CameraPos;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightDir);
    
    // Simple diffuse lighting
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = u_BaseColor.rgb * NdotL;
    
    // Add ambient
    vec3 ambient = u_BaseColor.rgb * 0.2;
    
    FragColor = vec4(ambient + diffuse, u_BaseColor.a);
}
EOF

    echo -e "${GREEN}Shaders created in $SHADER_DIR${NC}"
}

# Run the application
run() {
    if [ ! -f "$BUILD_DIR/$OUTPUT" ]; then
        echo -e "${RED}Build the project first!${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Starting renderer...${NC}"
    
    # Start shader watcher in background
    if [ -f "$BUILD_DIR/shader_watcher.sh" ]; then
        "$BUILD_DIR/shader_watcher.sh" &
        WATCHER_PID=$!
        trap "kill $WATCHER_PID 2>/dev/null" EXIT
    fi
    
    # Run the renderer
    cd "$BUILD_DIR" && ./"$OUTPUT"
}

# Benchmark build
benchmark() {
    echo -e "${YELLOW}Running build benchmark...${NC}"
    
    # Clean first
    clean > /dev/null 2>&1
    
    # Full build
    echo "Full build:"
    build release
    
    # Touch a source file
    touch "renderer_opengl.c"
    
    # Incremental build
    echo -e "\nIncremental build (one file changed):"
    build release
    
    # No changes build
    echo -e "\nNo-op build (no changes):"
    build release
}

# Print help
help() {
    echo "Renderer Build System"
    echo ""
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  build [release|dev]  Build the renderer (default: release)"
    echo "  clean               Clean build directory"
    echo "  run                 Build and run the renderer"
    echo "  shaders             Create example shaders"
    echo "  benchmark           Run build benchmark"
    echo "  help                Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 build           # Release build"
    echo "  $0 build dev       # Development build with hot reload"
    echo "  $0 run             # Build and run"
}

# Main script
case ${1:-build} in
    build)
        build ${2:-release}
        ;;
    clean)
        clean
        ;;
    run)
        build dev
        run
        ;;
    shaders)
        create_shaders
        ;;
    benchmark)
        benchmark
        ;;
    help)
        help
        ;;
    *)
        echo -e "${RED}Unknown command: $1${NC}"
        help
        exit 1
        ;;
esac