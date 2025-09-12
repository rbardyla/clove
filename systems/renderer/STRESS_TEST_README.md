# 3D Renderer Stress Test Documentation

## Overview

The renderer stress test (`renderer_stress_test.c`) is a comprehensive performance measurement tool that evaluates the actual capabilities of the handmade OpenGL 3D renderer. It provides concrete metrics to help make informed decisions about scene complexity, LOD systems, and optimization strategies.

## Building the Tests

```bash
# Build the full stress test (takes ~5 seconds per test scenario)
./build_stress_test.sh release

# Build the quick test (takes ~1 second total)
./build_quick_test.sh
```

**Important**: Always use release builds for accurate performance measurements.

## Running the Tests

### Quick Test (Verification)
```bash
./test_quick
```
This runs a brief test to verify the system works and gives you a rough idea of performance.

### Full Stress Test
```bash
# Basic run
./renderer_stress_test

# With environment optimization (recommended)
./run_stress_test.sh
```

The full stress test takes approximately 20-30 seconds and tests multiple scenarios.

## Test Scenarios

### 1. Many Simple Objects (Draw Call Overhead)
- **What it tests**: Maximum number of simple objects (cubes with 12 triangles each)
- **Bottleneck identified**: Draw call overhead
- **Use case**: Helps determine if you need batching or instancing
- **Typical results**: 1000-5000 objects at 60 FPS

### 2. Few Complex Objects (Vertex Processing)
- **What it tests**: High-poly spheres with varying complexity
- **Bottleneck identified**: Vertex shader performance
- **Use case**: Determines when to use LOD systems
- **Typical results**: 50-200 high-poly objects at 60 FPS

### 3. Batch Rendering
- **What it tests**: Large numbers of identical objects
- **Bottleneck identified**: Draw call submission vs vertex processing
- **Use case**: Evaluates benefit of instancing implementation
- **Note**: Currently uses multiple draw calls (no hardware instancing)

### 4. State Changes
- **What it tests**: Cost of switching shaders and textures
- **Bottleneck identified**: Pipeline state changes
- **Use case**: Determines if you need to sort by material
- **Typical results**: 200-800 state changes per frame at 60 FPS

## Interpreting Results

### Key Metrics

1. **FPS Metrics**
   - `Average FPS`: Overall performance
   - `1% Low FPS`: Worst-case performance (99th percentile of frame times)
   - `60 FPS Target`: Whether the test maintains 60 FPS consistently

2. **Geometry Metrics**
   - `Triangles/Second`: Raw triangle throughput
   - `Vertices/Second`: Vertex processing capability
   - `Draw Calls/Second`: Draw call submission rate

3. **Bottleneck Indicators**
   - **Draw call limited**: < 1000 draw calls/frame
   - **Vertex limited**: < 1M vertices/frame
   - **Fill rate limited**: < 100K triangles/frame with large triangles

### Example Output Interpretation

```
MAXIMUM PERFORMANCE AT 60 FPS:
  Max Triangles/Frame:    48000
  Max Draw Calls/Frame:   2500
  Max Vertices/Frame:     144000
```

This tells you:
- Budget ~24000 triangles for gameplay (50% of max)
- Keep draw calls under 1250 for safety margin
- Complex meshes over 1000 vertices need LOD

## Performance Guidelines

Based on typical stress test results:

### Scene Complexity Budget (at 60 FPS)
- **Simple scenes**: 5000+ objects, 60K triangles
- **Medium scenes**: 1000 objects, 100K triangles  
- **Complex scenes**: 200 objects, 200K triangles

### Optimization Priorities
1. **If draw call limited** (< 1000/frame)
   - Implement batching
   - Add hardware instancing support
   - Merge static geometry

2. **If vertex limited** (< 1M/frame)
   - Implement LOD system
   - Use simpler meshes for distant objects
   - Consider geometry shaders for grass/particles

3. **If fill rate limited** (large triangles)
   - Implement early Z-rejection
   - Use depth pre-pass
   - Reduce overdraw

## Customizing Tests

To add new test scenarios, follow this pattern:

```c
static test_result test_new_scenario(renderer_state *renderer, platform_state *platform) {
    print_test_header("New Test Scenario");
    
    test_result result = {0};
    strcpy(result.test_name, "New Scenario");
    
    // Your test implementation
    // Measure frame times, collect stats
    // Calculate statistics
    
    return result;
}
```

Add the test to the main function:
```c
if (platform->is_running) {
    results[test_count++] = test_new_scenario(renderer, platform);
}
```

## System Requirements

- OpenGL 3.3+ Core Profile
- X11 windowing system (Linux)
- 512MB+ RAM for stress testing
- Release build of the renderer

## Troubleshooting

### Test crashes immediately
- Check OpenGL version: `glxinfo | grep "OpenGL version"`
- Verify X11 is running: `echo $DISPLAY`
- Try the quick test first: `./test_quick`

### Results show 0 FPS
- Disable VSync in window config
- Check for GPU driver issues
- Ensure release build is used

### Inconsistent results
- Close other applications
- Disable compositor
- Set CPU governor to performance
- Use the run script: `./run_stress_test.sh`

## Next Steps

Based on stress test results:

1. **Implement instancing** if draw call limited
2. **Add LOD system** if vertex limited  
3. **Optimize shaders** if consistently below targets
4. **Profile with tools** like RenderDoc for deeper analysis

The stress test provides baseline metrics. Real game performance depends on:
- AI and physics overhead
- Memory bandwidth usage
- CPU-GPU synchronization
- Asset streaming

Always leave 30-50% performance headroom for gameplay systems!