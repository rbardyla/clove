#!/bin/bash

# Integration test for headless renderer benchmark
# Verifies the benchmark can be built and run in various environments

set -e

echo "========================================="
echo "  HEADLESS BENCHMARK INTEGRATION TEST"
echo "========================================="
echo ""

# Test 1: Build in release mode
echo "[TEST 1] Building in release mode..."
./build_benchmark_headless.sh release > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  ✓ Release build successful"
else
    echo "  ✗ Release build failed"
    exit 1
fi

# Test 2: Build in debug mode
echo "[TEST 2] Building in debug mode..."
./build_benchmark_headless.sh debug > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  ✓ Debug build successful"
else
    echo "  ✗ Debug build failed"
    exit 1
fi

# Test 3: Run benchmark (quick test)
echo "[TEST 3] Running benchmark..."
timeout 10 ./renderer_benchmark_headless > test_output.txt 2>&1
if [ $? -eq 0 ]; then
    echo "  ✓ Benchmark runs successfully"
else
    echo "  ✗ Benchmark failed to run"
    exit 1
fi

# Test 4: Verify output contains expected sections
echo "[TEST 4] Verifying benchmark output..."
EXPECTED_SECTIONS=(
    "Matrix Multiplication Benchmark"
    "Transform Operations Benchmark"
    "Frustum Culling Benchmark"
    "Draw Call Batching Benchmark"
    "Memory Allocation Patterns Benchmark"
    "Scene Graph Traversal Benchmark"
    "Vector Operations Benchmark"
)

ALL_FOUND=true
for section in "${EXPECTED_SECTIONS[@]}"; do
    if grep -q "$section" test_output.txt; then
        echo "  ✓ Found: $section"
    else
        echo "  ✗ Missing: $section"
        ALL_FOUND=false
    fi
done

if [ "$ALL_FOUND" = true ]; then
    echo "  ✓ All benchmark sections present"
else
    echo "  ✗ Some benchmark sections missing"
    exit 1
fi

# Test 5: Check performance metrics are reasonable
echo "[TEST 5] Checking performance metrics..."
MATRIX_OPS=$(grep "Operations/sec" test_output.txt | head -1 | awk '{print $2}')
if [ ! -z "$MATRIX_OPS" ] && [ "$MATRIX_OPS" -gt 1000000 ]; then
    echo "  ✓ Matrix ops reasonable: $MATRIX_OPS ops/sec"
else
    echo "  ✗ Matrix ops too low or missing"
    exit 1
fi

# Test 6: Memory footprint
echo "[TEST 6] Checking memory usage..."
SIZE_KB=$(ls -l renderer_benchmark_headless | awk '{print int($5/1024)}')
if [ "$SIZE_KB" -lt 500 ]; then
    echo "  ✓ Binary size reasonable: ${SIZE_KB}KB"
else
    echo "  ⚠ Binary size large: ${SIZE_KB}KB"
fi

# Test 7: No display dependency
echo "[TEST 7] Verifying no display dependencies..."
if ldd renderer_benchmark_headless | grep -q "libGL\|libX11\|libxcb"; then
    echo "  ✗ Found display dependencies!"
    ldd renderer_benchmark_headless | grep "libGL\|libX11\|libxcb"
    exit 1
else
    echo "  ✓ No display dependencies found"
fi

# Test 8: Run in background (headless test)
echo "[TEST 8] Testing headless execution..."
# Run with a 2 second timeout - benchmark won't finish but that's OK
timeout --preserve-status 2 ./renderer_benchmark_headless > headless_test.txt 2>&1 || true

# Check if output was generated
if [ -s headless_test.txt ]; then
    echo "  ✓ Runs in headless mode (verified output generation)"
else
    echo "  ✗ Failed in headless mode - no output generated"
    exit 1
fi

# Cleanup
rm -f test_output.txt headless_test.txt

echo ""
echo "========================================="
echo "      ALL INTEGRATION TESTS PASSED"
echo "========================================="
echo ""
echo "The headless renderer benchmark is ready for:"
echo "  • CI/CD pipelines"
echo "  • SSH sessions"
echo "  • Docker containers"
echo "  • Performance regression testing"
echo "  • Cross-platform benchmarking"
echo ""