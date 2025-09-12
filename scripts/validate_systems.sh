#!/bin/bash
# validate_systems.sh - Comprehensive system validation for Handmade Engine
# Tests all 16+ systems and reports their status

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Root directory
ENGINE_ROOT="/home/thebackhand/Projects/handmade-engine"
cd "$ENGINE_ROOT"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   HANDMADE ENGINE SYSTEM VALIDATION   ${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Track results
TOTAL_SYSTEMS=0
PASSED_SYSTEMS=0
FAILED_SYSTEMS=0
RESULTS=""

# Function to test a system
test_system() {
    local system_name=$1
    local build_script=$2
    local test_executable=$3
    
    TOTAL_SYSTEMS=$((TOTAL_SYSTEMS + 1))
    
    echo -e "${BLUE}[TEST]${NC} Testing $system_name..."
    
    # Try to build
    if [ -f "$build_script" ]; then
        if $build_script debug > /dev/null 2>&1; then
            echo -e "${GREEN}  ✓${NC} Build successful"
            
            # Try to run test if executable exists
            if [ -n "$test_executable" ] && [ -f "$test_executable" ]; then
                if timeout 2s $test_executable > /dev/null 2>&1; then
                    echo -e "${GREEN}  ✓${NC} Test passed"
                    PASSED_SYSTEMS=$((PASSED_SYSTEMS + 1))
                    RESULTS="${RESULTS}${GREEN}✓ $system_name${NC}\n"
                else
                    echo -e "${YELLOW}  !${NC} Test execution failed or timed out"
                    FAILED_SYSTEMS=$((FAILED_SYSTEMS + 1))
                    RESULTS="${RESULTS}${YELLOW}! $system_name (test failed)${NC}\n"
                fi
            else
                echo -e "${GREEN}  ✓${NC} No test executable, build successful"
                PASSED_SYSTEMS=$((PASSED_SYSTEMS + 1))
                RESULTS="${RESULTS}${GREEN}✓ $system_name${NC}\n"
            fi
        else
            echo -e "${RED}  ✗${NC} Build failed"
            FAILED_SYSTEMS=$((FAILED_SYSTEMS + 1))
            RESULTS="${RESULTS}${RED}✗ $system_name (build failed)${NC}\n"
        fi
    else
        echo -e "${YELLOW}  ?${NC} Build script not found"
        FAILED_SYSTEMS=$((FAILED_SYSTEMS + 1))
        RESULTS="${RESULTS}${YELLOW}? $system_name (no build script)${NC}\n"
    fi
    echo ""
}

# Test Core Systems
echo -e "${BLUE}=== CORE SYSTEMS ===${NC}"
echo ""

test_system "Renderer (OpenGL)" \
    "systems/renderer/build_renderer.sh" \
    "systems/renderer/build/renderer_test"

test_system "Physics Engine" \
    "systems/physics/build_physics.sh" \
    "systems/physics/build/physics_test"

test_system "GUI (Immediate Mode)" \
    "systems/gui/build_gui.sh" \
    "systems/gui/build/gui_demo"

test_system "Audio System" \
    "systems/audio/build_audio.sh" \
    "systems/audio/audio_test"

test_system "Networking" \
    "systems/network/build_network.sh" \
    "systems/network/network_test"

test_system "Save/Load System" \
    "systems/save/build_save.sh" \
    "systems/save/save_demo"

# Test Advanced Systems  
echo -e "${BLUE}=== ADVANCED SYSTEMS ===${NC}"
echo ""

test_system "Blueprint Visual Scripting" \
    "systems/blueprint/build_blueprint.sh" \
    "systems/blueprint/build/debug/blueprint_test"

test_system "Neural AI (DNC/EWC)" \
    "systems/ai/build_neural.sh" \
    "binaries/neural_test"

test_system "World Generation" \
    "systems/world_gen/build_world_gen.sh" \
    "systems/world_gen/world_gen_test"

test_system "Hot Reload" \
    "systems/hotreload/build_hotreload.sh" \
    ""

test_system "JIT Compiler" \
    "systems/jit/build_jit.sh" \
    "systems/jit/jit_test"

# Test Platform Systems
echo -e "${BLUE}=== PLATFORM SYSTEMS ===${NC}"
echo ""

test_system "Steam Integration" \
    "systems/steam/build_steam.sh" \
    ""

test_system "Vulkan Renderer" \
    "systems/vulkan/build_vulkan.sh" \
    ""

test_system "Settings System" \
    "systems/settings/build_settings.sh" \
    "systems/settings/perf_test"

test_system "Achievements" \
    "systems/achievements/build_achievements.sh" \
    ""

test_system "Lua Scripting" \
    "systems/script/build_script.sh" \
    ""

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}           VALIDATION SUMMARY           ${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Total Systems: $TOTAL_SYSTEMS"
echo -e "Passed: ${GREEN}$PASSED_SYSTEMS${NC}"
echo -e "Failed: ${RED}$FAILED_SYSTEMS${NC}"
echo ""
echo -e "Results by System:"
echo -e "$RESULTS"

# Overall status
if [ $FAILED_SYSTEMS -eq 0 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   ALL SYSTEMS VALIDATED SUCCESSFULLY!  ${NC}"
    echo -e "${GREEN}========================================${NC}"
    exit 0
else
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}   SOME SYSTEMS NEED ATTENTION         ${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    # Provide fix suggestions
    echo ""
    echo -e "${BLUE}Suggested Fixes:${NC}"
    echo -e "1. GUI System: Link with renderer library"
    echo -e "2. Missing build scripts: Create for systems without them"
    echo -e "3. Failed tests: Check individual system logs"
    echo ""
    echo -e "Run individual builds with:"
    echo -e "  cd systems/<system> && ./build_<system>.sh debug"
    
    exit 1
fi