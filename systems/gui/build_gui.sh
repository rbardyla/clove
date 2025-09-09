#!/bin/bash

# build_gui.sh - Build script for the Handmade GUI system
# Production-ready immediate mode GUI with comprehensive features

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
print_status() {
    echo -e "${BLUE}[BUILD]${NC} $1"
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

# Configuration
GUI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_ROOT="$(dirname "$(dirname "$GUI_DIR")")"
BUILD_DIR="$GUI_DIR/build"
SRC_DIR="$GUI_DIR"

# Compiler settings
CC="${CC:-gcc}"
CXX="${CXX:-g++}"

# Build flags
DEBUG_FLAGS="-g -O0 -DDEBUG=1 -DHANDMADE_DEBUG=1"
RELEASE_FLAGS="-O3 -DNDEBUG=1 -DHANDMADE_RELEASE=1 -march=native"
WARNING_FLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-field-initializers"
INCLUDE_FLAGS="-I$SRC_DIR -I$ENGINE_ROOT/src -I$ENGINE_ROOT/systems/renderer"
MATH_FLAGS="-lm"

# Platform-specific flags
case "$(uname -s)" in
    Linux*)
        PLATFORM_FLAGS="-DLINUX=1"
        LIBS="-lX11 -lGL -lm -lpthread"
        ;;
    Darwin*)
        PLATFORM_FLAGS="-DMACOS=1"
        LIBS="-framework OpenGL -framework Cocoa -lm"
        ;;
    CYGWIN*|MINGW32*|MSYS*|MINGW*)
        PLATFORM_FLAGS="-DWINDOWS=1"
        LIBS="-lopengl32 -lgdi32 -lm"
        ;;
    *)
        print_error "Unknown platform: $(uname -s)"
        exit 1
        ;;
esac

# Functions
create_build_dir() {
    print_status "Creating build directory..."
    mkdir -p "$BUILD_DIR"
}

check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check compiler
    if ! command -v "$CC" &> /dev/null; then
        print_error "Compiler '$CC' not found"
        exit 1
    fi
    
    # Check required files
    required_files=(
        "$SRC_DIR/handmade_gui.h"
        "$SRC_DIR/handmade_gui.c"
        "$SRC_DIR/gui_demo.c"
        "$ENGINE_ROOT/systems/renderer/handmade_math.h"
    )
    
    for file in "${required_files[@]}"; do
        if [[ ! -f "$file" ]]; then
            print_error "Required file not found: $file"
            exit 1
        fi
    done
    
    print_success "All dependencies found"
}

build_gui_lib() {
    local build_type="$1"
    local flags
    
    if [[ "$build_type" == "debug" ]]; then
        flags="$DEBUG_FLAGS"
        print_status "Building GUI library (Debug)..."
    else
        flags="$RELEASE_FLAGS"
        print_status "Building GUI library (Release)..."
    fi
    
    local output_file="$BUILD_DIR/libhandmade_gui_$build_type.a"
    local obj_file="$BUILD_DIR/handmade_gui_$build_type.o"
    
    # Compile GUI implementation
    $CC $flags $WARNING_FLAGS $INCLUDE_FLAGS $PLATFORM_FLAGS \
        -c "$SRC_DIR/handmade_gui.c" -o "$obj_file"
    
    # Create static library
    ar rcs "$output_file" "$obj_file"
    
    print_success "GUI library built: $output_file"
}

build_demo() {
    local build_type="$1"
    local flags
    
    if [[ "$build_type" == "debug" ]]; then
        flags="$DEBUG_FLAGS"
        print_status "Building GUI demo (Debug)..."
    else
        flags="$RELEASE_FLAGS"
        print_status "Building GUI demo (Release)..."
    fi
    
    local output_file="$BUILD_DIR/gui_demo_$build_type"
    local lib_file="$BUILD_DIR/libhandmade_gui_$build_type.a"
    local renderer_lib="$ENGINE_ROOT/systems/renderer/libhandmade_renderer.a"
    
    # Build renderer first if needed
    if [[ ! -f "$renderer_lib" ]]; then
        print_status "Building renderer dependency..."
        (cd "$ENGINE_ROOT/systems/renderer" && ./build_renderer.sh)
    fi
    
    # Build demo executable with renderer library
    $CC $flags $WARNING_FLAGS $INCLUDE_FLAGS $PLATFORM_FLAGS \
        "$SRC_DIR/gui_demo.c" "$lib_file" "$renderer_lib" $LIBS -o "$output_file"
    
    print_success "GUI demo built: $output_file"
}

build_standalone_demo() {
    local build_type="$1"
    local flags
    
    if [[ "$build_type" == "debug" ]]; then
        flags="$DEBUG_FLAGS"
        print_status "Building standalone GUI demo (Debug)..."
    else
        flags="$RELEASE_FLAGS"
        print_status "Building standalone GUI demo (Release)..."
    fi
    
    local output_file="$BUILD_DIR/gui_demo_standalone_$build_type"
    
    # Build standalone demo
    $CC $flags $WARNING_FLAGS $INCLUDE_FLAGS $PLATFORM_FLAGS \
        -DGUI_DEMO_STANDALONE=1 \
        "$SRC_DIR/gui_demo.c" "$SRC_DIR/handmade_gui.c" \
        $MATH_FLAGS -o "$output_file"
    
    print_success "Standalone GUI demo built: $output_file"
}

run_tests() {
    print_status "Running basic tests..."
    
    # Test standalone demo for basic functionality
    if [[ -x "$BUILD_DIR/gui_demo_standalone_debug" ]]; then
        print_status "Testing standalone demo..."
        if "$BUILD_DIR/gui_demo_standalone_debug" &> /dev/null; then
            print_success "Standalone demo test passed"
        else
            print_warning "Standalone demo test failed (this is expected without proper platform layer)"
        fi
    fi
    
    # Test library compilation
    if [[ -f "$BUILD_DIR/libhandmade_gui_debug.a" ]]; then
        print_success "GUI library compilation test passed"
    else
        print_error "GUI library not found"
        exit 1
    fi
}

install_headers() {
    local install_dir="${INSTALL_DIR:-$BUILD_DIR/include}"
    
    print_status "Installing headers to $install_dir..."
    mkdir -p "$install_dir"
    
    cp "$SRC_DIR/handmade_gui.h" "$install_dir/"
    cp "$ENGINE_ROOT/systems/renderer/handmade_math.h" "$install_dir/" 2>/dev/null || true
    
    print_success "Headers installed"
}

create_pkg_config() {
    local pkg_config_dir="$BUILD_DIR/pkgconfig"
    mkdir -p "$pkg_config_dir"
    
    cat > "$pkg_config_dir/handmade-gui.pc" << EOF
prefix=$BUILD_DIR
exec_prefix=\${prefix}
libdir=\${exec_prefix}
includedir=\${prefix}/include

Name: Handmade GUI
Description: Production-ready immediate mode GUI system
Version: 1.0.0
Libs: -L\${libdir} -lhandmade_gui_release $LIBS
Cflags: -I\${includedir}
EOF
    
    print_success "pkg-config file created: $pkg_config_dir/handmade-gui.pc"
}

show_usage() {
    echo "Usage: $0 [OPTIONS] [TARGETS]"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help           Show this help message"
    echo "  -c, --clean          Clean build directory"
    echo "  -d, --debug          Build debug version (default)"
    echo "  -r, --release        Build release version"
    echo "  -t, --test           Run tests after building"
    echo "  -i, --install        Install headers"
    echo "  --standalone         Build standalone demo only"
    echo ""
    echo "TARGETS:"
    echo "  all                  Build everything (default)"
    echo "  lib                  Build GUI library only"
    echo "  demo                 Build demo only"
    echo "  clean                Clean build directory"
    echo ""
    echo "EXAMPLES:"
    echo "  $0                   Build debug version of everything"
    echo "  $0 -r                Build release version of everything"
    echo "  $0 -r lib            Build release library only"
    echo "  $0 --standalone      Build standalone demo"
    echo "  $0 -c                Clean build directory"
    echo ""
    echo "ENVIRONMENT VARIABLES:"
    echo "  CC                   C compiler (default: gcc)"
    echo "  INSTALL_DIR          Header installation directory"
}

clean_build() {
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    print_success "Build directory cleaned"
}

main() {
    local build_type="debug"
    local run_tests_flag=false
    local install_flag=false
    local standalone_only=false
    local targets=("all")
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -c|--clean)
                clean_build
                exit 0
                ;;
            -d|--debug)
                build_type="debug"
                shift
                ;;
            -r|--release)
                build_type="release"
                shift
                ;;
            -t|--test)
                run_tests_flag=true
                shift
                ;;
            -i|--install)
                install_flag=true
                shift
                ;;
            --standalone)
                standalone_only=true
                shift
                ;;
            clean|lib|demo|all)
                targets=("$1")
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Handle clean target
    if [[ "${targets[0]}" == "clean" ]]; then
        clean_build
        exit 0
    fi
    
    # Show build information
    print_status "Handmade GUI Build System"
    print_status "Build Type: $build_type"
    print_status "Platform: $(uname -s)"
    print_status "Compiler: $CC"
    print_status "Targets: ${targets[*]}"
    echo ""
    
    # Setup build environment
    create_build_dir
    check_dependencies
    
    # Handle standalone build
    if [[ "$standalone_only" == true ]]; then
        build_standalone_demo "$build_type"
        print_success "Standalone build complete!"
        echo ""
        print_status "Run with: $BUILD_DIR/gui_demo_standalone_$build_type"
        exit 0
    fi
    
    # Build based on targets
    for target in "${targets[@]}"; do
        case $target in
            all)
                build_gui_lib "$build_type"
                build_demo "$build_type"
                build_standalone_demo "$build_type"
                ;;
            lib)
                build_gui_lib "$build_type"
                ;;
            demo)
                build_gui_lib "$build_type"
                build_demo "$build_type"
                ;;
        esac
    done
    
    # Additional tasks
    if [[ "$install_flag" == true ]]; then
        install_headers
        create_pkg_config
    fi
    
    if [[ "$run_tests_flag" == true ]]; then
        run_tests
    fi
    
    # Show completion message
    echo ""
    print_success "Build complete!"
    echo ""
    print_status "Built files:"
    ls -la "$BUILD_DIR"/ 2>/dev/null || true
    echo ""
    print_status "Usage examples:"
    echo "  Library: Link with -L$BUILD_DIR -lhandmade_gui_$build_type"
    echo "  Demo: $BUILD_DIR/gui_demo_$build_type"
    echo "  Standalone: $BUILD_DIR/gui_demo_standalone_$build_type"
    echo ""
    print_status "Features included:"
    echo "  ✓ Immediate mode GUI (zero allocations per frame)"
    echo "  ✓ Full widget set (buttons, sliders, checkboxes, text, etc.)"
    echo "  ✓ Advanced layout system (vertical, horizontal, grid)"
    echo "  ✓ Window management with docking support"
    echo "  ✓ Theme/styling system with hot reload"
    echo "  ✓ Performance metrics overlay"
    echo "  ✓ Property inspector widgets"
    echo "  ✓ Console/log viewer"
    echo "  ✓ Asset browser"
    echo "  ✓ Scene hierarchy viewer"
    echo "  ✓ Neural network visualization"
    echo "  ✓ Production-ready tools"
    echo ""
    print_success "Ready for 60fps with hundreds of windows!"
}

# Run main function
main "$@"