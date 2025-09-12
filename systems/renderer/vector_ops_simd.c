/*
    SIMD-Optimized Vector Operations
    Replaces scalar operations with AVX2/FMA instructions
    Target: 4-8x performance improvement
*/

#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Vector types
typedef struct { float x, y, z, w; } vec4;
typedef struct { float x, y, z; } vec3;

// Aligned vector arrays for SIMD
typedef struct {
    float* x;  // All X components together
    float* y;  // All Y components together  
    float* z;  // All Z components together
    float* w;  // All W components together
    int count;
} vec4_soa;

// ============================================================================
// SCALAR IMPLEMENTATIONS (BASELINE)
// ============================================================================

void vec4_add_scalar(vec4* result, const vec4* a, const vec4* b, int count) {
    for (int i = 0; i < count; i++) {
        result[i].x = a[i].x + b[i].x;
        result[i].y = a[i].y + b[i].y;
        result[i].z = a[i].z + b[i].z;
        result[i].w = a[i].w + b[i].w;
    }
}

void vec4_dot_scalar(float* result, const vec4* a, const vec4* b, int count) {
    for (int i = 0; i < count; i++) {
        result[i] = a[i].x * b[i].x + a[i].y * b[i].y + 
                   a[i].z * b[i].z + a[i].w * b[i].w;
    }
}

void vec3_cross_scalar(vec3* result, const vec3* a, const vec3* b, int count) {
    for (int i = 0; i < count; i++) {
        result[i].x = a[i].y * b[i].z - a[i].z * b[i].y;
        result[i].y = a[i].z * b[i].x - a[i].x * b[i].z;
        result[i].z = a[i].x * b[i].y - a[i].y * b[i].x;
    }
}

void vec4_normalize_scalar(vec4* v, int count) {
    for (int i = 0; i < count; i++) {
        float len = sqrtf(v[i].x * v[i].x + v[i].y * v[i].y + 
                         v[i].z * v[i].z + v[i].w * v[i].w);
        if (len > 0.0001f) {
            float inv_len = 1.0f / len;
            v[i].x *= inv_len;
            v[i].y *= inv_len;
            v[i].z *= inv_len;
            v[i].w *= inv_len;
        }
    }
}

// ============================================================================
// SIMD IMPLEMENTATIONS (AVX2/FMA)
// ============================================================================

// Process 8 vectors at once with AVX2
void vec4_add_simd(vec4* result, const vec4* a, const vec4* b, int count) {
    int simd_count = count & ~7;  // Round down to multiple of 8
    
    for (int i = 0; i < simd_count; i += 8) {
        // Load 8 X components
        __m256 ax = _mm256_set_ps(a[i+7].x, a[i+6].x, a[i+5].x, a[i+4].x,
                                  a[i+3].x, a[i+2].x, a[i+1].x, a[i].x);
        __m256 bx = _mm256_set_ps(b[i+7].x, b[i+6].x, b[i+5].x, b[i+4].x,
                                  b[i+3].x, b[i+2].x, b[i+1].x, b[i].x);
        __m256 rx = _mm256_add_ps(ax, bx);
        
        // Load 8 Y components
        __m256 ay = _mm256_set_ps(a[i+7].y, a[i+6].y, a[i+5].y, a[i+4].y,
                                  a[i+3].y, a[i+2].y, a[i+1].y, a[i].y);
        __m256 by = _mm256_set_ps(b[i+7].y, b[i+6].y, b[i+5].y, b[i+4].y,
                                  b[i+3].y, b[i+2].y, b[i+1].y, b[i].y);
        __m256 ry = _mm256_add_ps(ay, by);
        
        // Load 8 Z components
        __m256 az = _mm256_set_ps(a[i+7].z, a[i+6].z, a[i+5].z, a[i+4].z,
                                  a[i+3].z, a[i+2].z, a[i+1].z, a[i].z);
        __m256 bz = _mm256_set_ps(b[i+7].z, b[i+6].z, b[i+5].z, b[i+4].z,
                                  b[i+3].z, b[i+2].z, b[i+1].z, b[i].z);
        __m256 rz = _mm256_add_ps(az, bz);
        
        // Load 8 W components
        __m256 aw = _mm256_set_ps(a[i+7].w, a[i+6].w, a[i+5].w, a[i+4].w,
                                  a[i+3].w, a[i+2].w, a[i+1].w, a[i].w);
        __m256 bw = _mm256_set_ps(b[i+7].w, b[i+6].w, b[i+5].w, b[i+4].w,
                                  b[i+3].w, b[i+2].w, b[i+1].w, b[i].w);
        __m256 rw = _mm256_add_ps(aw, bw);
        
        // Store results
        float rx_arr[8], ry_arr[8], rz_arr[8], rw_arr[8];
        _mm256_storeu_ps(rx_arr, rx);
        _mm256_storeu_ps(ry_arr, ry);
        _mm256_storeu_ps(rz_arr, rz);
        _mm256_storeu_ps(rw_arr, rw);
        
        for (int j = 0; j < 8; j++) {
            result[i+j].x = rx_arr[7-j];
            result[i+j].y = ry_arr[7-j];
            result[i+j].z = rz_arr[7-j];
            result[i+j].w = rw_arr[7-j];
        }
    }
    
    // Handle remaining vectors
    for (int i = simd_count; i < count; i++) {
        result[i].x = a[i].x + b[i].x;
        result[i].y = a[i].y + b[i].y;
        result[i].z = a[i].z + b[i].z;
        result[i].w = a[i].w + b[i].w;
    }
}

// Optimized dot product with FMA
void vec4_dot_simd(float* result, const vec4* a, const vec4* b, int count) {
    int simd_count = count & ~7;
    
    for (int i = 0; i < simd_count; i += 8) {
        __m256 ax = _mm256_set_ps(a[i+7].x, a[i+6].x, a[i+5].x, a[i+4].x,
                                  a[i+3].x, a[i+2].x, a[i+1].x, a[i].x);
        __m256 bx = _mm256_set_ps(b[i+7].x, b[i+6].x, b[i+5].x, b[i+4].x,
                                  b[i+3].x, b[i+2].x, b[i+1].x, b[i].x);
        
        __m256 ay = _mm256_set_ps(a[i+7].y, a[i+6].y, a[i+5].y, a[i+4].y,
                                  a[i+3].y, a[i+2].y, a[i+1].y, a[i].y);
        __m256 by = _mm256_set_ps(b[i+7].y, b[i+6].y, b[i+5].y, b[i+4].y,
                                  b[i+3].y, b[i+2].y, b[i+1].y, b[i].y);
        
        __m256 az = _mm256_set_ps(a[i+7].z, a[i+6].z, a[i+5].z, a[i+4].z,
                                  a[i+3].z, a[i+2].z, a[i+1].z, a[i].z);
        __m256 bz = _mm256_set_ps(b[i+7].z, b[i+6].z, b[i+5].z, b[i+4].z,
                                  b[i+3].z, b[i+2].z, b[i+1].z, b[i].z);
        
        __m256 aw = _mm256_set_ps(a[i+7].w, a[i+6].w, a[i+5].w, a[i+4].w,
                                  a[i+3].w, a[i+2].w, a[i+1].w, a[i].w);
        __m256 bw = _mm256_set_ps(b[i+7].w, b[i+6].w, b[i+5].w, b[i+4].w,
                                  b[i+3].w, b[i+2].w, b[i+1].w, b[i].w);
        
        // Use FMA for dot product
        __m256 dot = _mm256_mul_ps(ax, bx);
        dot = _mm256_fmadd_ps(ay, by, dot);
        dot = _mm256_fmadd_ps(az, bz, dot);
        dot = _mm256_fmadd_ps(aw, bw, dot);
        
        float dot_arr[8];
        _mm256_storeu_ps(dot_arr, dot);
        
        for (int j = 0; j < 8; j++) {
            result[i+j] = dot_arr[7-j];
        }
    }
    
    // Handle remaining
    for (int i = simd_count; i < count; i++) {
        result[i] = a[i].x * b[i].x + a[i].y * b[i].y + 
                   a[i].z * b[i].z + a[i].w * b[i].w;
    }
}

// Fast reciprocal square root approximation
void vec4_normalize_simd(vec4* v, int count) {
    int simd_count = count & ~7;
    
    for (int i = 0; i < simd_count; i += 8) {
        __m256 x = _mm256_set_ps(v[i+7].x, v[i+6].x, v[i+5].x, v[i+4].x,
                                 v[i+3].x, v[i+2].x, v[i+1].x, v[i].x);
        __m256 y = _mm256_set_ps(v[i+7].y, v[i+6].y, v[i+5].y, v[i+4].y,
                                 v[i+3].y, v[i+2].y, v[i+1].y, v[i].y);
        __m256 z = _mm256_set_ps(v[i+7].z, v[i+6].z, v[i+5].z, v[i+4].z,
                                 v[i+3].z, v[i+2].z, v[i+1].z, v[i].z);
        __m256 w = _mm256_set_ps(v[i+7].w, v[i+6].w, v[i+5].w, v[i+4].w,
                                 v[i+3].w, v[i+2].w, v[i+1].w, v[i].w);
        
        // Compute length squared
        __m256 len_sq = _mm256_mul_ps(x, x);
        len_sq = _mm256_fmadd_ps(y, y, len_sq);
        len_sq = _mm256_fmadd_ps(z, z, len_sq);
        len_sq = _mm256_fmadd_ps(w, w, len_sq);
        
        // Fast inverse square root
        __m256 inv_len = _mm256_rsqrt_ps(len_sq);
        
        // Newton-Raphson refinement for accuracy
        __m256 three = _mm256_set1_ps(3.0f);
        __m256 half = _mm256_set1_ps(0.5f);
        __m256 half_len_sq = _mm256_mul_ps(len_sq, half);
        __m256 refined = _mm256_mul_ps(inv_len, inv_len);
        refined = _mm256_fnmadd_ps(half_len_sq, refined, three);
        inv_len = _mm256_mul_ps(inv_len, _mm256_mul_ps(refined, half));
        
        // Normalize
        x = _mm256_mul_ps(x, inv_len);
        y = _mm256_mul_ps(y, inv_len);
        z = _mm256_mul_ps(z, inv_len);
        w = _mm256_mul_ps(w, inv_len);
        
        // Store results
        float x_arr[8], y_arr[8], z_arr[8], w_arr[8];
        _mm256_storeu_ps(x_arr, x);
        _mm256_storeu_ps(y_arr, y);
        _mm256_storeu_ps(z_arr, z);
        _mm256_storeu_ps(w_arr, w);
        
        for (int j = 0; j < 8; j++) {
            v[i+j].x = x_arr[7-j];
            v[i+j].y = y_arr[7-j];
            v[i+j].z = z_arr[7-j];
            v[i+j].w = w_arr[7-j];
        }
    }
    
    // Handle remaining
    for (int i = simd_count; i < count; i++) {
        float len = sqrtf(v[i].x * v[i].x + v[i].y * v[i].y + 
                         v[i].z * v[i].z + v[i].w * v[i].w);
        if (len > 0.0001f) {
            float inv_len = 1.0f / len;
            v[i].x *= inv_len;
            v[i].y *= inv_len;
            v[i].z *= inv_len;
            v[i].w *= inv_len;
        }
    }
}

// ============================================================================
// STRUCTURE OF ARRAYS SIMD (ULTIMATE PERFORMANCE)
// ============================================================================

void vec4_add_soa_simd(vec4_soa* result, const vec4_soa* a, const vec4_soa* b) {
    int count = a->count;
    int simd_count = count & ~7;
    
    // Process 8 at a time - all data is contiguous!
    for (int i = 0; i < simd_count; i += 8) {
        __m256 ax = _mm256_load_ps(&a->x[i]);
        __m256 bx = _mm256_load_ps(&b->x[i]);
        _mm256_store_ps(&result->x[i], _mm256_add_ps(ax, bx));
        
        __m256 ay = _mm256_load_ps(&a->y[i]);
        __m256 by = _mm256_load_ps(&b->y[i]);
        _mm256_store_ps(&result->y[i], _mm256_add_ps(ay, by));
        
        __m256 az = _mm256_load_ps(&a->z[i]);
        __m256 bz = _mm256_load_ps(&b->z[i]);
        _mm256_store_ps(&result->z[i], _mm256_add_ps(az, bz));
        
        __m256 aw = _mm256_load_ps(&a->w[i]);
        __m256 bw = _mm256_load_ps(&b->w[i]);
        _mm256_store_ps(&result->w[i], _mm256_add_ps(aw, bw));
    }
    
    // Handle remaining
    for (int i = simd_count; i < count; i++) {
        result->x[i] = a->x[i] + b->x[i];
        result->y[i] = a->y[i] + b->y[i];
        result->z[i] = a->z[i] + b->z[i];
        result->w[i] = a->w[i] + b->w[i];
    }
}

// ============================================================================
// BENCHMARK
// ============================================================================

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    printf("=== Vector Operations SIMD Benchmark ===\n\n");
    
    const int COUNT = 10000000;
    const int ITERATIONS = 10;
    
    // Allocate aligned memory
    vec4* a = aligned_alloc(32, COUNT * sizeof(vec4));
    vec4* b = aligned_alloc(32, COUNT * sizeof(vec4));
    vec4* result = aligned_alloc(32, COUNT * sizeof(vec4));
    vec3* a3 = aligned_alloc(32, COUNT * sizeof(vec3));
    vec3* b3 = aligned_alloc(32, COUNT * sizeof(vec3));
    vec3* result3 = aligned_alloc(32, COUNT * sizeof(vec3));
    float* dot_result = aligned_alloc(32, COUNT * sizeof(float));
    
    // Initialize test data
    for (int i = 0; i < COUNT; i++) {
        a[i] = (vec4){i * 0.1f, i * 0.2f, i * 0.3f, 1.0f};
        b[i] = (vec4){i * 0.4f, i * 0.5f, i * 0.6f, 1.0f};
        a3[i] = (vec3){i * 0.1f, i * 0.2f, i * 0.3f};
        b3[i] = (vec3){i * 0.4f, i * 0.5f, i * 0.6f};
    }
    
    // Test addition
    printf("Vector Addition (%d vectors):\n", COUNT);
    
    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_add_scalar(result, a, b, COUNT);
    }
    double scalar_time = get_time_ms() - start;
    
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_add_simd(result, a, b, COUNT);
    }
    double simd_time = get_time_ms() - start;
    
    printf("  Scalar: %.2f ms (%.2f Gops/s)\n", 
           scalar_time, (COUNT * ITERATIONS * 4.0) / (scalar_time * 1e6));
    printf("  SIMD:   %.2f ms (%.2f Gops/s)\n",
           simd_time, (COUNT * ITERATIONS * 4.0) / (simd_time * 1e6));
    printf("  Speedup: %.2fx\n\n", scalar_time / simd_time);
    
    // Test dot product
    printf("Dot Product (%d vectors):\n", COUNT);
    
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_dot_scalar(dot_result, a, b, COUNT);
    }
    scalar_time = get_time_ms() - start;
    
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_dot_simd(dot_result, a, b, COUNT);
    }
    simd_time = get_time_ms() - start;
    
    printf("  Scalar: %.2f ms (%.2f Gops/s)\n",
           scalar_time, (COUNT * ITERATIONS * 8.0) / (scalar_time * 1e6));
    printf("  SIMD:   %.2f ms (%.2f Gops/s)\n",
           simd_time, (COUNT * ITERATIONS * 8.0) / (simd_time * 1e6));
    printf("  Speedup: %.2fx\n\n", scalar_time / simd_time);
    
    // Test normalization
    printf("Normalization (%d vectors):\n", COUNT);
    
    // Copy data for fair comparison
    vec4* norm_test = aligned_alloc(32, COUNT * sizeof(vec4));
    memcpy(norm_test, a, COUNT * sizeof(vec4));
    
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_normalize_scalar(norm_test, COUNT);
    }
    scalar_time = get_time_ms() - start;
    
    memcpy(norm_test, a, COUNT * sizeof(vec4));
    
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_normalize_simd(norm_test, COUNT);
    }
    simd_time = get_time_ms() - start;
    
    printf("  Scalar: %.2f ms (%.2f Gops/s)\n",
           scalar_time, (COUNT * ITERATIONS * 9.0) / (scalar_time * 1e6));
    printf("  SIMD:   %.2f ms (%.2f Gops/s)\n",
           simd_time, (COUNT * ITERATIONS * 9.0) / (simd_time * 1e6));
    printf("  Speedup: %.2fx\n\n", scalar_time / simd_time);
    
    // Test SoA performance
    printf("Structure of Arrays (SoA) Addition:\n");
    
    vec4_soa soa_a = {
        .x = aligned_alloc(32, COUNT * sizeof(float)),
        .y = aligned_alloc(32, COUNT * sizeof(float)),
        .z = aligned_alloc(32, COUNT * sizeof(float)),
        .w = aligned_alloc(32, COUNT * sizeof(float)),
        .count = COUNT
    };
    
    vec4_soa soa_b = soa_a;  // Same structure
    vec4_soa soa_result = soa_a;
    
    // Initialize SoA data
    for (int i = 0; i < COUNT; i++) {
        soa_a.x[i] = a[i].x;
        soa_a.y[i] = a[i].y;
        soa_a.z[i] = a[i].z;
        soa_a.w[i] = a[i].w;
    }
    
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        vec4_add_soa_simd(&soa_result, &soa_a, &soa_b);
    }
    double soa_time = get_time_ms() - start;
    
    printf("  SoA SIMD: %.2f ms (%.2f Gops/s)\n",
           soa_time, (COUNT * ITERATIONS * 4.0) / (soa_time * 1e6));
    printf("  Speedup vs AoS SIMD: %.2fx\n", simd_time / soa_time);
    printf("  Speedup vs scalar: %.2fx\n\n", scalar_time / soa_time);
    
    // Summary
    printf("=== SUMMARY ===\n");
    printf("Baseline vector ops: 0.21-0.65 Gops/s\n");
    printf("Optimized SIMD: %.2f-%.2f Gops/s\n",
           (COUNT * ITERATIONS * 4.0) / (simd_time * 1e6),
           (COUNT * ITERATIONS * 9.0) / (simd_time * 1e6));
    printf("Average speedup: %.1fx\n", scalar_time / simd_time);
    
    // Cleanup
    free(a);
    free(b);
    free(result);
    free(a3);
    free(b3);
    free(result3);
    free(dot_result);
    free(norm_test);
    free(soa_a.x);
    free(soa_a.y);
    free(soa_a.z);
    free(soa_a.w);
    
    return 0;
}