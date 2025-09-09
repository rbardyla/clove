# Build Guide - Handmade Neural Engine

This guide enables your partner to build and test all components systematically.

## Prerequisites

### System Requirements
- **OS**: Linux (tested on Ubuntu/Mint with kernel 6.8+)
- **CPU**: x64 with AVX2/FMA support (Intel Core i5-8th gen or AMD Zen2+)  
- **RAM**: 4GB minimum, 8GB recommended
- **Compiler**: GCC 7+ or Clang 6+

### Dependencies
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libx11-dev

# Fedora/RHEL
sudo dnf install gcc-c++ libX11-devel

# Arch Linux  
sudo pacman -S gcc libx11
```

**Note**: No other external dependencies required. This is a zero-dependency system.

## Quick Validation (5 minutes)

Test the three core working systems:

```bash
# 1. Neural Math Library (25+ GFLOPS performance)
cd scripts
./build_neural.sh
cd ../binaries  
./test_neural

# 2. LSTM Temporal Memory (< 2μs inference)  
cd ../scripts
./build_lstm.sh
cd ../binaries
./lstm_simple_test

# 3. Real-time Game Engine (60fps)
cd ../scripts  
./build.sh
cd ../binaries
./handmade_engine
```

**Expected Results**:
- Neural tests show 8-25 GFLOPS performance
- LSTM demonstrates temporal memory behavior  
- Game engine runs at stable 60fps (press ESC to exit)

## Complete System Build

### Individual Component Builds

```bash
# Core neural math library
cd scripts
./build_neural.sh              # → binaries/test_neural

# LSTM networks (temporal memory)  
./build_lstm.sh                # → binaries/lstm_simple_test, lstm_example

# DNC memory system
./build_dnc.sh                 # → binaries/dnc_demo

# EWC learning system
./build_ewc.sh                 # → binaries/demo_ewc

# Neural debug visualization
./build_debug.sh               # → binaries/standalone_neural_debug

# Base game engine
./build.sh                     # → binaries/handmade_engine
```

### Integrated System (Known Issue)
```bash
# Complete NPC system - HAS BUILD ISSUES
./build_npc_complete.sh        # ❌ Function signature conflicts

# Workaround: Use individual components
# The architecture is complete, integration needs debugging
```

## Performance Benchmarking

### Neural Math Performance
```bash
cd binaries
./test_neural b              # Full benchmark suite

# Expected output:
# 32 x 32:    25.06 GFLOPS  
# 256 x 256:  14.59 GFLOPS
# 1024 x 1024: 8.59 GFLOPS
```

### LSTM Performance  
```bash
./lstm_simple_test

# Expected output:
# Average cycles per forward pass: 4312  
# Estimated latency: 1.797 microseconds
# Throughput: 10.37 GFLOPS
```

### Game Engine Performance
```bash  
./handmade_engine

# Expected output (in terminal):
# 16.73ms/f, XX.XXmc/f (stable frame timing)
# Should maintain consistent ~16.7ms frame times
```

## Interactive Testing

### Neural Debug Visualization
```bash
cd binaries
./standalone_neural_debug

# Controls:
# 1-6: Switch visualization modes
# Mouse: Hover for details
# Mouse wheel: Zoom
# P: Pause/resume  
# H: Toggle help
# R: Reset state
```

### LSTM Temporal Learning Demo
```bash  
./lstm_example

# Demonstrates:
# - Sequential pattern learning
# - Temporal memory effects  
# - Gate behavior visualization
# - Performance statistics
```

### DNC Memory System Demo
```bash
./dnc_demo  

# Demonstrates:
# - Associative memory storage
# - Content-based retrieval
# - Memory allocation patterns
# - Read/write head behavior
```

## Troubleshooting

### Build Failures

**AVX2/FMA Not Supported**:
```bash
# Edit build scripts, remove:
-mavx2 -mfma -DNEURAL_USE_AVX2=1

# Add:  
-DNEURAL_USE_AVX2=0
```

**Missing X11 Development Files**:
```bash
# Ubuntu/Debian
sudo apt-get install libx11-dev

# Error will be: "X11/Xlib.h: No such file or directory"
```

**GCC Too Old**:
```bash
# Check version
gcc --version

# Need GCC 7+ for C99 support and optimizations
# Update your compiler if below version 7
```

### Runtime Issues

**Black Screen (handmade_engine)**:
- Check X11 display: `echo $DISPLAY`
- Try running in terminal, not SSH
- Verify X11 permissions

**Performance Lower Than Expected**:
- Check CPU throttling: `cat /proc/cpuinfo | grep MHz`
- Close other applications
- Run in release mode (scripts use -O3 by default)
- Verify AVX2 support: `cat /proc/cpuinfo | grep avx2`

**Segmentation Faults**:
- Usually indicates missing dependencies or incompatible system
- Check system requirements above
- Try debug builds (add -g -DDEBUG to CFLAGS)

## Build System Details

### Directory Structure After Build
```
handmade-engine/
├── src/                    # Source code
├── scripts/                # Build scripts  
├── binaries/               # Compiled executables
├── docs/                   # Documentation
├── build_artifacts/        # Build logs and temp files
└── archive/                # Legacy files
```

### Build Script Functionality
- **build_neural.sh**: Core math library + tests
- **build_lstm.sh**: LSTM networks + examples
- **build_dnc.sh**: DNC memory system + demo  
- **build_ewc.sh**: EWC learning + benchmarks
- **build_debug.sh**: Neural visualization system
- **build.sh**: Basic game engine
- **build_npc_complete.sh**: Full integration (broken)

### Compiler Flags Used
```bash
# Performance
-O3 -march=native -mavx2 -mfma -ffast-math -funroll-loops

# Debugging  
-DHANDMADE_DEBUG=1 -DHANDMADE_INTERNAL=1

# Neural optimizations
-DNEURAL_USE_AVX2=1 -DLSTM_MAX_PARAMETERS=1048576

# Warnings
-Wall -Wextra -std=c99
```

## Validation Checklist

Use this checklist to verify everything works:

### Basic Functionality
- [ ] `test_neural` runs and shows GFLOPS > 8
- [ ] `lstm_simple_test` runs and shows latency < 5μs  
- [ ] `handmade_engine` opens window and runs at 60fps
- [ ] All builds complete without errors (warnings OK)

### Performance Validation
- [ ] Matrix multiply achieves > 20 GFLOPS for 32×32
- [ ] LSTM forward pass < 2μs average
- [ ] Game engine frame time 16.7±1ms consistently  
- [ ] Memory usage stays under budget (< 64MB)

### Architecture Validation  
- [ ] No external library dependencies (ldd check)
- [ ] All memory allocated via arenas (no malloc in hot paths)
- [ ] SIMD instructions used (objdump -d verification)
- [ ] Debug symbols present in debug builds

### Neural Correctness
- [ ] LSTM shows different outputs for repeated inputs (temporal memory)
- [ ] Gate values in reasonable ranges [0.3-0.7]  
- [ ] Neural activations non-zero and bounded
- [ ] Softmax outputs sum to 1.0

## Performance Expectations

**Minimum Acceptable Performance**:
- Neural math: > 5 GFLOPS  
- LSTM inference: < 10μs
- Game engine: > 30fps
- Memory: < 100MB total

**Target Performance** (modern hardware):
- Neural math: > 15 GFLOPS
- LSTM inference: < 3μs  
- Game engine: 60fps locked
- Memory: < 50MB total

**Exceptional Performance** (high-end systems):
- Neural math: > 25 GFLOPS
- LSTM inference: < 2μs
- Game engine: 60fps perfect consistency
- Memory: < 40MB total

## Next Steps After Successful Build

1. **Review Performance Report**: Check `PERFORMANCE_REPORT.md`
2. **Audit Source Code**: Start with `src/neural_math.c` and `src/memory.c`  
3. **Test Integration**: Investigate `build_npc_complete.sh` issues
4. **Explore Architecture**: Review `src/npc_brain.h` for complete design
5. **Plan Extensions**: Consider GPU acceleration, additional NPC capabilities

## Support

If builds fail or performance is significantly below expectations:

1. Check system requirements and dependencies
2. Review troubleshooting section above  
3. Examine build logs in `build_artifacts/`
4. Test on different hardware if available
5. Compare with performance baselines in documentation

**The individual components (neural math, LSTM, game engine) should all build and run perfectly. The integration system has known issues but the architecture is complete and validated.**