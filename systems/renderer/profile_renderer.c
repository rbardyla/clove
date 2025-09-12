#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <immintrin.h>  // AVX2 intrinsics

// Profile specific renderer bottlenecks with SIMD comparison

typedef struct {
    float x, y, z, w;
} vec4;

typedef struct {
    float m[16];
} mat4;

// Scalar version - current implementation
static void transform_points_scalar(vec4* points, int count, mat4* matrix) {
    for (int i = 0; i < count; i++) {
        vec4 p = points[i];
        vec4 result;
        result.x = matrix->m[0] * p.x + matrix->m[4] * p.y + matrix->m[8]  * p.z + matrix->m[12] * p.w;
        result.y = matrix->m[1] * p.x + matrix->m[5] * p.y + matrix->m[9]  * p.z + matrix->m[13] * p.w;
        result.z = matrix->m[2] * p.x + matrix->m[6] * p.y + matrix->m[10] * p.z + matrix->m[14] * p.w;
        result.w = matrix->m[3] * p.x + matrix->m[7] * p.y + matrix->m[11] * p.z + matrix->m[15] * p.w;
        points[i] = result;
    }
}

// AVX2 version - optimized correctly
static void transform_points_avx2(vec4* points, int count, mat4* matrix) {
    // Load matrix columns for SIMD processing
    __m128 col0 = _mm_load_ps(&matrix->m[0]);
    __m128 col1 = _mm_load_ps(&matrix->m[4]);
    __m128 col2 = _mm_load_ps(&matrix->m[8]);
    __m128 col3 = _mm_load_ps(&matrix->m[12]);
    
    // Process one vec4 at a time with SSE (more efficient for this workload)
    for (int i = 0; i < count; i++) {
        __m128 p = _mm_load_ps((float*)&points[i]);
        
        __m128 x = _mm_shuffle_ps(p, p, 0x00);
        __m128 y = _mm_shuffle_ps(p, p, 0x55);
        __m128 z = _mm_shuffle_ps(p, p, 0xAA);
        __m128 w = _mm_shuffle_ps(p, p, 0xFF);
        
        __m128 result = _mm_mul_ps(col0, x);
        result = _mm_fmadd_ps(col1, y, result);
        result = _mm_fmadd_ps(col2, z, result);
        result = _mm_fmadd_ps(col3, w, result);
        
        _mm_store_ps((float*)&points[i], result);
    }
}

// Profile scene graph with better cache layout
typedef struct {
    mat4* world_matrices;     // Hot data together
    mat4* local_matrices;     // Hot data together
    int* parent_indices;      // Cold data separate
    int* dirty_flags;         // Hot for updates
    int count;
} scene_graph_soa;

static void update_scene_graph_soa(scene_graph_soa* sg) {
    // Process dirty nodes first (better branch prediction)
    for (int i = 0; i < sg->count; i++) {
        if (sg->dirty_flags[i]) {
            int parent = sg->parent_indices[i];
            if (parent >= 0) {
                // Matrix multiply with hot data layout
                // Implementation details omitted for brevity
            }
            sg->dirty_flags[i] = 0;
        }
    }
}

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    printf("========================================\n");
    printf("    RENDERER OPTIMIZATION PROFILE\n");
    printf("========================================\n\n");
    
    const int POINT_COUNT = 1000000;
    const int ITERATIONS = 100;
    
    // Allocate aligned memory for SIMD
    vec4* points_scalar = aligned_alloc(32, sizeof(vec4) * POINT_COUNT);
    vec4* points_simd = aligned_alloc(32, sizeof(vec4) * POINT_COUNT);
    mat4 matrix = {{1,0,0,0, 0,1,0,0, 0,0,1,0, 1,2,3,1}};
    
    // Initialize test data
    for (int i = 0; i < POINT_COUNT; i++) {
        points_scalar[i] = (vec4){i * 0.1f, i * 0.2f, i * 0.3f, 1.0f};
        points_simd[i] = points_scalar[i];
    }
    
    // Warm up caches
    transform_points_scalar(points_scalar, 1000, &matrix);
    transform_points_avx2(points_simd, 1000, &matrix);
    
    // Profile scalar version
    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        transform_points_scalar(points_scalar, POINT_COUNT, &matrix);
    }
    double scalar_time = get_time_ms() - start;
    
    // Profile SIMD version
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        transform_points_avx2(points_simd, POINT_COUNT, &matrix);
    }
    double simd_time = get_time_ms() - start;
    
    printf("Transform Performance (1M points, 100 iterations):\n");
    printf("  Scalar: %.2f ms (%.2f Mtransforms/s)\n", 
           scalar_time, (POINT_COUNT * ITERATIONS) / (scalar_time * 1000));
    printf("  AVX2:   %.2f ms (%.2f Mtransforms/s)\n",
           simd_time, (POINT_COUNT * ITERATIONS) / (simd_time * 1000));
    printf("  Speedup: %.2fx\n\n", scalar_time / simd_time);
    
    // Profile memory patterns
    printf("Cache Analysis:\n");
    printf("  vec4 size: %zu bytes\n", sizeof(vec4));
    printf("  Cache line points: %zu\n", 64 / sizeof(vec4));
    printf("  Total memory: %.2f MB\n", (sizeof(vec4) * POINT_COUNT) / (1024.0 * 1024.0));
    printf("  Bandwidth (scalar): %.2f GB/s\n", 
           (sizeof(vec4) * POINT_COUNT * ITERATIONS * 2) / (scalar_time * 1000000));
    printf("  Bandwidth (AVX2): %.2f GB/s\n",
           (sizeof(vec4) * POINT_COUNT * ITERATIONS * 2) / (simd_time * 1000000));
    
    free(points_scalar);
    free(points_simd);
    
    printf("\n========================================\n");
    printf("         OPTIMIZATION COMPLETE\n");
    printf("========================================\n");
    
    return 0;
}
