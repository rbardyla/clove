#!/bin/bash
# build_blueprint.sh - Build script for the Blueprint Visual Scripting System
# PERFORMANCE: Optimized compilation flags for maximum performance
# FEATURES: Debug builds, release builds, tests, examples

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_NAME="Blueprint System"
BUILD_DIR="build"
SRC_DIR="."
ENGINE_ROOT="../.."

# Compiler settings
CC=${CC:-gcc}
CXX=${CXX:-g++}

# Common flags
COMMON_FLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable"
INCLUDE_FLAGS="-I${SRC_DIR} -I${ENGINE_ROOT}/src -I../gui -I../renderer"
SYSTEM_LIBS="-lm -lpthread -lrt"

# OpenGL/Graphics libraries (adjust based on system)
if command -v pkg-config &> /dev/null; then
    if pkg-config --exists gl; then
        GRAPHICS_LIBS="$(pkg-config --libs gl)"
    else
        GRAPHICS_LIBS="-lGL"
    fi
    
    if pkg-config --exists glfw3; then
        GRAPHICS_LIBS="${GRAPHICS_LIBS} $(pkg-config --libs glfw3)"
    else
        GRAPHICS_LIBS="${GRAPHICS_LIBS} -lglfw"
    fi
else
    GRAPHICS_LIBS="-lGL -lglfw"
fi

# Build configurations
DEBUG_FLAGS="${COMMON_FLAGS} -g -O0 -DDEBUG -DBLUEPRINT_DEBUG"
RELEASE_FLAGS="${COMMON_FLAGS} -O3 -DNDEBUG -march=native -flto"
PROFILE_FLAGS="${COMMON_FLAGS} -g -O2 -pg -DPROFILE"

# Source files
CORE_SOURCES="handmade_blueprint.c blueprint_compiler.c blueprint_vm.c blueprint_nodes.c blueprint_editor.c"
TEST_SOURCES="blueprint_test.c"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check prerequisites
check_prerequisites() {
    print_status "Checking prerequisites..."
    
    # Check compiler
    if ! command -v ${CC} &> /dev/null; then
        print_error "Compiler ${CC} not found"
        exit 1
    fi
    
    # Check required headers (basic check)
    if [ ! -f "${ENGINE_ROOT}/src/handmade.h" ]; then
        print_warning "Engine header not found at ${ENGINE_ROOT}/src/handmade.h"
        print_warning "Some features may not work correctly"
    fi
    
    # Check GUI system
    if [ ! -f "../gui/handmade_gui.h" ]; then
        print_warning "GUI system not found - editor may not work"
    fi
    
    # Check renderer system
    if [ ! -f "../renderer/handmade_renderer.h" ]; then
        print_warning "Renderer system not found - visual features may not work"
    fi
    
    print_success "Prerequisites check completed"
}

# Function to create build directory
setup_build_dir() {
    print_status "Setting up build directory..."
    
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
    fi
    
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}/debug"
    mkdir -p "${BUILD_DIR}/release"
    mkdir -p "${BUILD_DIR}/profile"
    
    print_success "Build directory created"
}

# Function to build the core library
build_core_library() {
    local config=$1
    local flags=$2
    local output_dir="${BUILD_DIR}/${config}"
    
    print_status "Building core library (${config})..."
    
    # Compile individual object files
    for src in ${CORE_SOURCES}; do
        if [ -f "${src}" ]; then
            print_status "Compiling ${src}..."
            ${CC} ${flags} ${INCLUDE_FLAGS} -c "${src}" -o "${output_dir}/$(basename ${src} .c).o"
        else
            print_warning "Source file ${src} not found, skipping..."
        fi
    done
    
    # Create static library
    local lib_file="${output_dir}/libblueprint.a"
    print_status "Creating static library ${lib_file}..."
    
    local object_files=""
    for src in ${CORE_SOURCES}; do
        local obj_file="${output_dir}/$(basename ${src} .c).o"
        if [ -f "${obj_file}" ]; then
            object_files="${object_files} ${obj_file}"
        fi
    done
    
    if [ -n "${object_files}" ]; then
        ar rcs "${lib_file}" ${object_files}
        print_success "Core library built successfully"
    else
        print_error "No object files found for library creation"
        exit 1
    fi
}

# Function to build test program
build_test_program() {
    local config=$1
    local flags=$2
    local output_dir="${BUILD_DIR}/${config}"
    
    print_status "Building test program (${config})..."
    
    if [ ! -f "blueprint_test.c" ]; then
        print_warning "Test source not found, skipping test build"
        return
    fi
    
    local exe_file="${output_dir}/blueprint_test"
    ${CC} ${flags} ${INCLUDE_FLAGS} blueprint_test.c -o "${exe_file}" \
          -L"${output_dir}" -lblueprint ${SYSTEM_LIBS} ${GRAPHICS_LIBS}
    
    if [ -f "${exe_file}" ]; then
        print_success "Test program built successfully"
        
        # Make executable
        chmod +x "${exe_file}"
    else
        print_error "Failed to build test program"
        exit 1
    fi
}

# Function to build standalone example
build_standalone_example() {
    local config=$1
    local flags=$2
    local output_dir="${BUILD_DIR}/${config}"
    
    print_status "Building standalone example (${config})..."
    
    # Create a simple standalone example
    cat > "${output_dir}/blueprint_example.c" << 'EOF'
#include "handmade_blueprint.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Blueprint System Standalone Example\n");
    printf("===================================\n");
    
    // Minimal setup
    platform_state platform = {0};
    renderer r = {0};
    gui_context gui = {0};
    
    blueprint_context ctx;
    blueprint_init(&ctx, &gui, &r, &platform);
    
    // Create a simple graph
    blueprint_graph* graph = blueprint_create_graph(&ctx, "Example Graph");
    if (graph) {
        printf("Created graph: %s\n", graph->name);
        
        // Test basic functionality
        blueprint_node* node = blueprint_create_node(graph, NODE_TYPE_ADD, (v2){0, 0});
        if (node) {
            printf("Created node: %s\n", node->name);
        }
        
        printf("Graph has %u nodes\n", graph->node_count);
        
        // Compile and execute
        blueprint_compile_graph(&ctx, graph);
        printf("Graph compiled, bytecode size: %u bytes\n", graph->bytecode_size);
    }
    
    blueprint_shutdown(&ctx);
    
    printf("Example completed successfully!\n");
    return 0;
}
EOF
    
    local exe_file="${output_dir}/blueprint_example"
    ${CC} ${flags} ${INCLUDE_FLAGS} "${output_dir}/blueprint_example.c" -o "${exe_file}" \
          -L"${output_dir}" -lblueprint ${SYSTEM_LIBS}
    
    if [ -f "${exe_file}" ]; then
        print_success "Standalone example built successfully"
        chmod +x "${exe_file}"
    else
        print_warning "Failed to build standalone example"
    fi
}

# Function to run tests
run_tests() {
    print_status "Running tests..."
    
    local test_exe="${BUILD_DIR}/debug/blueprint_test"
    if [ -f "${test_exe}" ]; then
        print_status "Running debug tests..."
        "${test_exe}"
        
        if [ $? -eq 0 ]; then
            print_success "All tests passed!"
        else
            print_error "Some tests failed!"
            exit 1
        fi
    else
        print_warning "Test executable not found, skipping tests"
    fi
    
    # Run standalone example
    local example_exe="${BUILD_DIR}/debug/blueprint_example"
    if [ -f "${example_exe}" ]; then
        print_status "Running standalone example..."
        "${example_exe}"
    fi
}

# Function to create package
create_package() {
    print_status "Creating distribution package..."
    
    local package_dir="${BUILD_DIR}/package"
    mkdir -p "${package_dir}"
    
    # Copy headers
    mkdir -p "${package_dir}/include"
    cp handmade_blueprint.h "${package_dir}/include/"
    
    # Copy release library
    mkdir -p "${package_dir}/lib"
    if [ -f "${BUILD_DIR}/release/libblueprint.a" ]; then
        cp "${BUILD_DIR}/release/libblueprint.a" "${package_dir}/lib/"
    fi
    
    # Copy examples
    mkdir -p "${package_dir}/examples"
    if [ -f "${BUILD_DIR}/release/blueprint_example" ]; then
        cp "${BUILD_DIR}/release/blueprint_example" "${package_dir}/examples/"
    fi
    
    # Create documentation
    cat > "${package_dir}/README.md" << 'EOF'
# Blueprint Visual Scripting System

A high-performance visual scripting system for game engines.

## Features

- Node-based visual scripting
- Bytecode compilation for performance
- Virtual machine execution
- Type-safe connections
- Debug support with breakpoints
- Hot reload capabilities

## Files

- `include/handmade_blueprint.h` - Main header file
- `lib/libblueprint.a` - Static library
- `examples/blueprint_example` - Example program

## Integration

Include the header and link with the library:

```c
#include "handmade_blueprint.h"

// Initialize system
blueprint_context ctx;
blueprint_init(&ctx, gui, renderer, platform);

// Create and use graphs
blueprint_graph* graph = blueprint_create_graph(&ctx, "My Graph");
```

## Performance Targets

- Execute 10,000+ nodes per frame at 60fps
- Compile large graphs in <10ms
- Zero allocations in hot paths
- Cache-coherent data structures
EOF
    
    print_success "Package created in ${package_dir}"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  debug      Build debug version"
    echo "  release    Build optimized release version"
    echo "  profile    Build with profiling enabled"
    echo "  test       Build and run tests"
    echo "  all        Build all configurations"
    echo "  clean      Clean build directory"
    echo "  package    Create distribution package"
    echo "  help       Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  CC         C compiler (default: gcc)"
    echo "  CXX        C++ compiler (default: g++)"
    echo ""
    echo "Examples:"
    echo "  $0 debug          # Build debug version"
    echo "  $0 release        # Build release version"
    echo "  $0 test           # Build and run tests"
    echo "  CC=clang $0 all   # Build all with clang"
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."
    
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        print_success "Build directory cleaned"
    else
        print_status "Build directory already clean"
    fi
}

# Function to show build info
show_build_info() {
    print_status "${PROJECT_NAME} Build Information"
    echo "=================================="
    echo "Compiler: ${CC}"
    echo "Build Directory: ${BUILD_DIR}"
    echo "Source Directory: ${SRC_DIR}"
    echo "Engine Root: ${ENGINE_ROOT}"
    echo ""
    echo "Available configurations:"
    echo "  debug   - Unoptimized build with debug symbols"
    echo "  release - Optimized build for production"
    echo "  profile - Build with profiling instrumentation"
    echo ""
}

# Main build function
build_configuration() {
    local config=$1
    
    case ${config} in
        debug)
            build_core_library "debug" "${DEBUG_FLAGS}"
            build_test_program "debug" "${DEBUG_FLAGS}"
            build_standalone_example "debug" "${DEBUG_FLAGS}"
            ;;
        release)
            build_core_library "release" "${RELEASE_FLAGS}"
            build_test_program "release" "${RELEASE_FLAGS}"
            build_standalone_example "release" "${RELEASE_FLAGS}"
            ;;
        profile)
            build_core_library "profile" "${PROFILE_FLAGS}"
            build_test_program "profile" "${PROFILE_FLAGS}"
            build_standalone_example "profile" "${PROFILE_FLAGS}"
            ;;
        *)
            print_error "Unknown configuration: ${config}"
            exit 1
            ;;
    esac
}

# Main script logic
main() {
    local command=${1:-help}
    
    case ${command} in
        debug|release|profile)
            show_build_info
            check_prerequisites
            setup_build_dir
            build_configuration ${command}
            print_success "Build completed successfully!"
            ;;
        test)
            show_build_info
            check_prerequisites
            setup_build_dir
            build_configuration "debug"
            run_tests
            ;;
        all)
            show_build_info
            check_prerequisites
            setup_build_dir
            build_configuration "debug"
            build_configuration "release"
            build_configuration "profile"
            print_success "All configurations built successfully!"
            ;;
        clean)
            clean_build
            ;;
        package)
            check_prerequisites
            setup_build_dir
            build_configuration "release"
            create_package
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            print_error "Unknown command: ${command}"
            show_usage
            exit 1
            ;;
    esac
}

# Script entry point
print_status "Starting ${PROJECT_NAME} build..."
main "$@"
print_success "Build script completed!"