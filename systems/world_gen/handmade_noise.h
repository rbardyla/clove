#ifndef HANDMADE_NOISE_H
#define HANDMADE_NOISE_H

/*
    Handmade Noise Generation
    SIMD-optimized Perlin and Simplex noise from scratch
    Zero dependencies, AVX2 accelerated
    
    Based on Ken Perlin's improved noise (2002)
    All algorithms implemented from first principles
*/

#include "../../src/handmade.h"
#include <immintrin.h>  // AVX2 intrinsics

// Noise configuration
typedef struct noise_config {
    f32 frequency;      // Base frequency (0.01 = large features, 1.0 = small)
    f32 amplitude;      // Output amplitude multiplier
    u32 octaves;        // Number of octave layers
    f32 persistence;    // Amplitude multiplier per octave (0.5 = each octave half amplitude)
    f32 lacunarity;     // Frequency multiplier per octave (2.0 = each octave double frequency)
    u32 seed;           // Random seed for permutation table
} noise_config;

// Noise state (holds permutation tables)
typedef struct noise_state {
    u8 perm[512];       // Permutation table (duplicated for wrap-around)
    f32 grad3[16][3];  // 3D gradient vectors
    
    // SIMD-friendly data layout
    __m256 perm_simd[64];    // Permutation table in SIMD registers
    __m256 grad_x[16];       // Gradient X components
    __m256 grad_y[16];       // Gradient Y components
    __m256 grad_z[16];       // Gradient Z components
} noise_state;

// =============================================================================
// NOISE API
// =============================================================================

// Initialize noise state with seed
noise_state* noise_init(arena* arena, u32 seed);

// Single-point noise functions
f32 noise_perlin_2d(noise_state* state, f32 x, f32 y);
f32 noise_perlin_3d(noise_state* state, f32 x, f32 y, f32 z);
f32 noise_simplex_2d(noise_state* state, f32 x, f32 y);
f32 noise_simplex_3d(noise_state* state, f32 x, f32 y, f32 z);

// SIMD batch processing (8 points at once)
void noise_perlin_2d_simd(noise_state* state, 
                          const f32* x, const f32* y, 
                          f32* output, u32 count);

void noise_perlin_3d_simd(noise_state* state,
                          const f32* x, const f32* y, const f32* z,
                          f32* output, u32 count);

// Fractal noise (multiple octaves)
f32 noise_fractal_2d(noise_state* state, noise_config* config, f32 x, f32 y);
f32 noise_fractal_3d(noise_state* state, noise_config* config, f32 x, f32 y, f32 z);

// Terrain-specific noise functions
f32 noise_ridge(noise_state* state, f32 x, f32 y, f32 z, f32 offset, f32 gain);
f32 noise_turbulence(noise_state* state, f32 x, f32 y, f32 z, u32 octaves);
f32 noise_billowy(noise_state* state, f32 x, f32 y, f32 z);

// Utility functions
void noise_set_seed(noise_state* state, u32 seed);
f32 noise_remap(f32 value, f32 old_min, f32 old_max, f32 new_min, f32 new_max);

// =============================================================================
// TERRAIN HEIGHT MAP GENERATION
// =============================================================================

typedef struct terrain_params {
    f32 base_frequency;
    f32 amplitude;
    u32 octaves;
    f32 persistence;
    f32 lacunarity;
    
    // Terrain shaping
    f32 elevation_scale;
    f32 elevation_offset;
    f32 erosion_strength;
    f32 ridge_frequency;
    f32 valley_depth;
} terrain_params;

// Generate height map using SIMD
void terrain_generate_heightmap(noise_state* state, terrain_params* params,
                                f32* heightmap, u32 width, u32 height);

// Generate normal map from height map
void terrain_generate_normalmap(const f32* heightmap, u32 width, u32 height,
                                f32 scale, f32* normalmap);

// Performance metrics
typedef struct noise_stats {
    u64 samples_generated;
    f64 total_time_ms;
    f64 simd_speedup;
} noise_stats;

void noise_print_stats(noise_stats* stats);

#endif // HANDMADE_NOISE_H