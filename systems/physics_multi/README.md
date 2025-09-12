# Multi-Level Physics-Driven Detail (MLPDD) System

A deep, multi-timescale physics simulation where terrain emerges from actual physical forces rather than mathematical noise functions. Following Casey Muratori's handmade philosophy of "building up, not out" - going deeper into simulation rather than broader.

## Architecture

The system simulates physics at multiple timescales that interact:

### Timescales

1. **Geological (1M years/sec)** - Tectonic plates, mountain formation
2. **Hydrological (100 years/sec)** - Rivers, erosion, sedimentation [TODO]
3. **Structural (1 day/sec)** - Building settling, weathering [TODO]  
4. **Realtime (1:1)** - Player interaction, immediate physics

### Current Implementation

#### Geological Simulation (`handmade_geological.c`)
- Tectonic plate movement driven by mantle convection
- Continental collision creating mountain ranges
- Rayleigh-Benard convection in the mantle
- Stress tensor calculations for crustal deformation
- Zero heap allocations - fully arena-based

## Building

```bash
./build_physics_multi.sh         # Release build (optimized)
./build_physics_multi.sh debug   # Debug build
```

## Running

```bash
./test_geological   # Run geological simulation test
```

## Performance

- Simulates 1 million years per second on modern hardware
- 3 tectonic plates with 10,000 vertices each
- Mountain formation through realistic collision physics
- Peak memory: ~1.24 MB (arena allocated)
- Grade A compliant: zero malloc/free

## Integration Points

The MLPDD system is designed to integrate with:

### Renderer
```c
// Export heightmap for terrain rendering
f32* heightmap = arena_push_array(arena, f32, 512 * 256);
geological_export_heightmap(geo_state, heightmap, 512, 256, temp_arena);
renderer_update_terrain(renderer, heightmap, 512, 256);
```

### Game Logic
```c
// Query elevation at world position
f32 elevation = geological_query_elevation(geo_state, world_x, world_z);

// Detect mountain ranges for gameplay
mountain_range* ranges = geological_find_mountains(geo_state, arena);
```

### Save System
```c
// Serialize geological state
geological_serialize(geo_state, save_context);

// Deserialize on load
geo_state = geological_deserialize(save_context, arena);
```

## Technical Details

### Mantle Convection
- 32x32 grid of convection cells
- Temperature-driven buoyancy forces
- Rayleigh number: 10^6 (vigorous convection)

### Plate Tectonics
- Continental plates: 35km thick, 2700 kg/m³ density
- Oceanic plates: 7km thick, 2900 kg/m³ density  
- Collision detection using spatial hashing
- Stress accumulation and mountain uplift

### Memory Management
- All allocations through arena system
- Temporary allocations use stack-restored arena state
- Zero dynamic allocations during simulation

## Future Work

### Hydrological System
- River formation from precipitation
- Erosion and sediment transport
- Lake and ocean formation

### Structural Physics
- Building foundation settling
- Material weathering over time
- Earthquake damage propagation

### Time Integration
- Smooth transitions between timescales
- Player ability to control time flow
- Persistence across all scales

## Code Style

Following handmade philosophy:
- No external dependencies
- Structure of Arrays for cache coherency
- SIMD optimization where beneficial
- Predictable memory usage
- Debug visualization support

## Files

- `handmade_physics_multi.h` - Main system architecture
- `handmade_geological.c` - Geological simulation implementation
- `handmade_math_types.h` - Basic math types (v2, v3, v4)
- `test_geological.c` - Validation and benchmarking
- `build_physics_multi.sh` - Build script

## References

- Rayleigh-Benard convection physics
- Plate tectonics and continental drift
- Stress-strain relationships in rock mechanics
- Casey Muratori's Handmade Hero principles