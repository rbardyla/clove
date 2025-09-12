# Headless Renderer Benchmark

## Overview

A comprehensive CPU-side renderer performance benchmark that requires **NO display, NO OpenGL, and NO X11**. This tool measures the computational performance of rendering algorithms without any GPU or display dependencies.

## Features

### 1. Matrix Operations
- **Matrix Multiplication**: Tests 4x4 matrix multiplication performance
- **Transform Operations**: Benchmarks point and vector transformations
- **Performance**: ~46M matrix multiplications/sec on Intel i7-10750H

### 2. Frustum Culling
- **Sphere Culling**: Tests bounding sphere against frustum planes
- **AABB Culling**: Tests axis-aligned bounding boxes
- **Performance**: ~46K culling tests/ms with 13-14% visibility ratio

### 3. Draw Call Batching
- **State Sorting**: Groups draw calls by shader and material
- **Batch Optimization**: Measures state change reduction
- **Performance**: 100-200:1 batching ratio achieved

### 4. Memory Allocation Patterns
- **Small Allocations**: Vertex data (48 bytes)
- **Medium Allocations**: Mesh data (4KB)
- **Large Allocations**: Texture data (1MB)
- **Arena Allocations**: Single-allocation pool strategy
- **Result**: Arena allocations 1.4x faster than individual mallocs

### 5. Scene Graph Traversal
- **Hierarchical Updates**: Transform propagation through tree
- **Dirty Flagging**: Optimized partial updates
- **Performance**: 1.4x speedup with partial updates

### 6. Vector Operations
- **Basic Operations**: Add, dot, cross, normalize
- **SIMD Potential**: Operations that could benefit from vectorization
- **Performance**: 0.25-0.66 Gops/s depending on operation

## Building

```bash
# Make build script executable
chmod +x build_benchmark_headless.sh

# Build release version (optimized)
./build_benchmark_headless.sh release

# Build debug version (with symbols)
./build_benchmark_headless.sh debug
```

## Running

```bash
./renderer_benchmark_headless
```

No display required! Works in SSH sessions, containers, and headless servers.

## Performance Results (Intel i7-10750H @ 2.60GHz)

| Operation | Performance | Units |
|-----------|------------|-------|
| Matrix Multiply | 46.4M | ops/sec |
| Transform Point | 122.4M | ops/sec |
| Frustum Culling (Sphere) | 46.3K | tests/ms |
| Frustum Culling (AABB) | 48.2K | tests/ms |
| Draw Call Batching | 25.8K | objects/ms |
| Scene Graph Update | 80.6K | nodes/ms |
| Vector Addition | 0.25 | Gops/s |
| Vector Normalization | 0.66 | Gops/s |

## Use Cases

1. **Performance Regression Testing**: Track renderer performance across commits
2. **Hardware Comparison**: Compare CPU performance across different systems
3. **Algorithm Optimization**: Measure impact of optimizations
4. **CI/CD Integration**: Run in automated testing pipelines
5. **Profiling Baseline**: Establish performance baselines for new features

## Implementation Details

### No Dependencies
- Pure C implementation
- No OpenGL/DirectX/Vulkan
- No X11/Wayland/Win32
- Only standard C library and math

### Compiler Optimizations
- `-O3`: Maximum optimization
- `-march=native`: CPU-specific instructions
- `-mavx2 -mfma`: SIMD instructions
- `-ffast-math`: Aggressive float optimizations
- `-funroll-loops`: Loop unrolling

### Memory Strategy
- Arena allocators for bulk operations
- Structure of Arrays (SoA) for cache coherency
- Aligned allocations for SIMD
- Minimal heap fragmentation

## Extending the Benchmark

To add new benchmarks:

1. Create a new benchmark function:
```c
static void benchmark_your_feature(u32 iterations) {
    printf("\n=== Your Feature Benchmark ===\n");
    
    perf_timer timer;
    // Setup code
    
    timer_start(&timer);
    // Benchmark code
    f64 elapsed = timer_end(&timer);
    
    printf("  Time: %.3f ms\n", elapsed);
    // Print results
}
```

2. Call it from main():
```c
benchmark_your_feature(100000);
```

3. Rebuild and run

## Comparison with GPU Benchmarks

This benchmark measures CPU-side work only:
- **Measured**: Transform calculations, culling, batching, scene management
- **Not Measured**: Rasterization, shading, texture sampling, framebuffer ops

For complete renderer performance, combine with GPU profiling tools.

## Troubleshooting

### Build Errors
- Ensure GCC is installed: `sudo apt install build-essential`
- Check math library: `-lm` flag is required

### Performance Issues
- Ensure release build: Use `release` argument
- Check CPU throttling: Monitor with `watch -n 1 "grep MHz /proc/cpuinfo"`
- Close other applications for consistent results

### Inconsistent Results
- Run multiple times and average
- Ensure CPU governor is set to performance
- Consider process affinity: `taskset -c 0 ./renderer_benchmark_headless`

## Future Enhancements

- [ ] SIMD implementations for vector operations
- [ ] Multi-threaded scene traversal
- [ ] Cache miss analysis
- [ ] Memory bandwidth measurements
- [ ] Comparison with GPU timings
- [ ] JSON output for automated analysis
- [ ] Configurable workload sizes

## License

Part of the Handmade Engine project. Zero dependencies, maximum control.