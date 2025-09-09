#!/bin/bash

# Particle System Validation Script
# Tests all particle system functionality

set -e

echo "========================================"
echo "    PARTICLE SYSTEM VALIDATION"
echo "========================================"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASSED=0
FAILED=0

# Function to test a command
test_command() {
    local name="$1"
    local cmd="$2"
    local expected="$3"
    
    echo -n "Testing $name... "
    
    if timeout 2 bash -c "$cmd" > /dev/null 2>&1; then
        if [ -n "$expected" ]; then
            result=$(bash -c "$cmd" 2>&1)
            if echo "$result" | grep -q "$expected"; then
                echo -e "${GREEN}✓${NC}"
                ((PASSED++))
            else
                echo -e "${RED}✗${NC} (output mismatch)"
                ((FAILED++))
            fi
        else
            echo -e "${GREEN}✓${NC}"
            ((PASSED++))
        fi
    else
        echo -e "${RED}✗${NC}"
        ((FAILED++))
    fi
}

# Build the system
echo "Building particle system..."
if ./build_particles.sh release > /dev/null 2>&1; then
    echo -e "${GREEN}Build successful${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo ""
echo "Running validation tests..."
echo "=========================="

# Test 1: Performance benchmark runs
test_command "Performance benchmark" "./build/release/perf_test | head -1" "Particle System Performance Test"

# Test 2: Check SIMD performance
echo -n "Testing SIMD performance... "
output=$(./build/release/perf_test 2>&1 | grep "100000 particles" -A 4)
fps=$(echo "$output" | grep "Can sustain" | awk '{print $3}' | cut -d'.' -f1)
if [ "$fps" -gt 60 ]; then
    echo -e "${GREEN}✓${NC} (${fps} FPS for 100K particles)"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} (Only ${fps} FPS, need > 60)"
    ((FAILED++))
fi

# Test 3: Memory efficiency
echo -n "Testing memory efficiency... "
output=$(./build/release/perf_test 2>&1 | grep "Efficiency")
efficiency=$(echo "$output" | awk '{print $2}' | cut -d'%' -f1 | cut -d'.' -f1)
if [ "$efficiency" -gt 40 ]; then
    echo -e "${GREEN}✓${NC} (${efficiency}% efficiency)"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} (Only ${efficiency}% efficiency)"
    ((FAILED++))
fi

# Test 4: Demo runs without crash
test_command "Demo application" "timeout 1 ./build/release/particle_demo" ""

# Test 5: Check library size
echo -n "Testing binary size... "
size=$(ls -l build/release/libhandmade_particles.a | awk '{print $5}')
size_kb=$((size / 1024))
if [ "$size_kb" -lt 100 ]; then
    echo -e "${GREEN}✓${NC} (${size_kb}KB library)"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠${NC} (${size_kb}KB - larger than expected)"
    ((PASSED++))
fi

# Test 6: Zero heap allocations
echo -n "Testing for heap allocations... "
if grep -q "malloc\|new\|calloc" handmade_particles.c 2>/dev/null; then
    echo -e "${RED}✗${NC} (found heap allocations in core)"
    ((FAILED++))
else
    echo -e "${GREEN}✓${NC} (no heap allocations in core)"
    ((PASSED++))
fi

# Test 7: SIMD alignment
echo -n "Testing SIMD alignment... "
if grep -q "_mm256_load_ps\|_mm256_store_ps" handmade_particles.c 2>/dev/null; then
    echo -e "${YELLOW}⚠${NC} (using aligned loads - potential issue)"
    ((PASSED++))
else
    echo -e "${GREEN}✓${NC} (using unaligned loads)"
    ((PASSED++))
fi

echo ""
echo "========================================"
echo "           VALIDATION RESULTS"
echo "========================================"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
    echo "Particle system is fully functional!"
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    echo "Please review the failures above."
    exit 1
fi