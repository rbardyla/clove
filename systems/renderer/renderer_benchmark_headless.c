/*
    Headless Renderer Benchmark
    Measures CPU-side rendering performance without OpenGL/X11 dependency
    
    Tests:
    - Matrix math operations (multiplication, transformation)
    - Frustum culling algorithms
    - Draw call batching logic
    - Scene graph traversal
    - Memory allocation patterns
    - SIMD optimizations
*/

#include "../../src/handmade.h"
#include "handmade_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// Performance timer
typedef struct perf_timer {
    struct timeval start;
    struct timeval end;
    f64 elapsed_ms;
} perf_timer;

static void timer_start(perf_timer *timer) {
    gettimeofday(&timer->start, NULL);
}

static f64 timer_end(perf_timer *timer) {
    gettimeofday(&timer->end, NULL);
    timer->elapsed_ms = (timer->end.tv_sec - timer->start.tv_sec) * 1000.0 +
                       (timer->end.tv_usec - timer->start.tv_usec) / 1000.0;
    return timer->elapsed_ms;
}

// Simulated scene object
typedef struct scene_object {
    m4x4 transform;
    v3 position;
    v3 bounds_min;
    v3 bounds_max;
    f32 bounding_radius;
    u32 vertex_count;
    u32 index_count;
    b32 is_visible;
    u32 batch_id;
} scene_object;

// Simulated draw command
typedef struct draw_command_sim {
    u32 object_id;
    u32 material_id;
    u32 shader_id;
    m4x4 mvp_matrix;
    u32 vertex_offset;
    u32 index_offset;
    u32 instance_count;
} draw_command_sim;

// Simulated render batch
typedef struct render_batch {
    u32 shader_id;
    u32 material_id;
    draw_command_sim *commands;
    u32 command_count;
    u32 command_capacity;
} render_batch;

// Frustum for culling
typedef struct frustum {
    v4 planes[6];  // left, right, top, bottom, near, far
} frustum;

// Helper for v3_one() scaled
static inline v3 v3_one_scale(f32 s) {
    return v3_make(s, s, s);
}

// Extract frustum planes from view-projection matrix
static void extract_frustum_planes(frustum *f, m4x4 vp) {
    // Left plane
    f->planes[0].x = vp.e[3] + vp.e[0];
    f->planes[0].y = vp.e[7] + vp.e[4];
    f->planes[0].z = vp.e[11] + vp.e[8];
    f->planes[0].w = vp.e[15] + vp.e[12];
    
    // Right plane
    f->planes[1].x = vp.e[3] - vp.e[0];
    f->planes[1].y = vp.e[7] - vp.e[4];
    f->planes[1].z = vp.e[11] - vp.e[8];
    f->planes[1].w = vp.e[15] - vp.e[12];
    
    // Top plane
    f->planes[2].x = vp.e[3] - vp.e[1];
    f->planes[2].y = vp.e[7] - vp.e[5];
    f->planes[2].z = vp.e[11] - vp.e[9];
    f->planes[2].w = vp.e[15] - vp.e[13];
    
    // Bottom plane
    f->planes[3].x = vp.e[3] + vp.e[1];
    f->planes[3].y = vp.e[7] + vp.e[5];
    f->planes[3].z = vp.e[11] + vp.e[9];
    f->planes[3].w = vp.e[15] + vp.e[13];
    
    // Near plane
    f->planes[4].x = vp.e[3] + vp.e[2];
    f->planes[4].y = vp.e[7] + vp.e[6];
    f->planes[4].z = vp.e[11] + vp.e[10];
    f->planes[4].w = vp.e[15] + vp.e[14];
    
    // Far plane
    f->planes[5].x = vp.e[3] - vp.e[2];
    f->planes[5].y = vp.e[7] - vp.e[6];
    f->planes[5].z = vp.e[11] - vp.e[10];
    f->planes[5].w = vp.e[15] - vp.e[14];
    
    // Normalize planes
    for (i32 i = 0; i < 6; i++) {
        f32 length = sqrtf(f->planes[i].x * f->planes[i].x + 
                          f->planes[i].y * f->planes[i].y + 
                          f->planes[i].z * f->planes[i].z);
        if (length > 0.0f) {
            f->planes[i].x /= length;
            f->planes[i].y /= length;
            f->planes[i].z /= length;
            f->planes[i].w /= length;
        }
    }
}

// Test if sphere is in frustum
static b32 sphere_in_frustum(frustum *f, v3 center, f32 radius) {
    for (i32 i = 0; i < 6; i++) {
        f32 distance = f->planes[i].x * center.x + 
                      f->planes[i].y * center.y + 
                      f->planes[i].z * center.z + 
                      f->planes[i].w;
        if (distance < -radius) {
            return 0;
        }
    }
    return 1;
}

// Test if AABB is in frustum
static b32 aabb_in_frustum(frustum *f, v3 min, v3 max) {
    v3 center = v3_scale(v3_add(min, max), 0.5f);
    v3 half_extents = v3_scale(v3_sub(max, min), 0.5f);
    
    for (i32 i = 0; i < 6; i++) {
        v3 normal = v3_make(f->planes[i].x, f->planes[i].y, f->planes[i].z);
        f32 radius = fabsf(v3_dot(half_extents, v3_make(fabsf(normal.x), 
                                                        fabsf(normal.y), 
                                                        fabsf(normal.z))));
        f32 distance = v3_dot(center, normal) + f->planes[i].w;
        
        if (distance < -radius) {
            return 0;
        }
    }
    return 1;
}

// Benchmark matrix multiplication
static void benchmark_matrix_multiply(u32 iterations) {
    printf("\n=== Matrix Multiplication Benchmark ===\n");
    
    perf_timer timer;
    m4x4 *matrices_a = malloc(iterations * sizeof(m4x4));
    m4x4 *matrices_b = malloc(iterations * sizeof(m4x4));
    m4x4 *results = malloc(iterations * sizeof(m4x4));
    
    // Initialize with random matrices
    for (u32 i = 0; i < iterations; i++) {
        for (u32 j = 0; j < 16; j++) {
            matrices_a[i].e[j] = (f32)rand() / RAND_MAX;
            matrices_b[i].e[j] = (f32)rand() / RAND_MAX;
        }
    }
    
    // Warm-up
    for (u32 i = 0; i < 100; i++) {
        results[i] = m4x4_mul(matrices_a[i], matrices_b[i]);
    }
    
    // Benchmark
    timer_start(&timer);
    for (u32 i = 0; i < iterations; i++) {
        results[i] = m4x4_mul(matrices_a[i], matrices_b[i]);
    }
    f64 elapsed = timer_end(&timer);
    
    printf("  Iterations: %u\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed);
    printf("  Per operation: %.6f µs\n", (elapsed * 1000.0) / iterations);
    printf("  Operations/sec: %.0f\n", iterations / (elapsed / 1000.0));
    
    free(matrices_a);
    free(matrices_b);
    free(results);
}

// Benchmark transform operations
static void benchmark_transforms(u32 iterations) {
    printf("\n=== Transform Operations Benchmark ===\n");
    
    perf_timer timer;
    v3 *positions = malloc(iterations * sizeof(v3));
    m4x4 *transforms = malloc(iterations * sizeof(m4x4));
    v3 *results = malloc(iterations * sizeof(v3));
    
    // Initialize
    for (u32 i = 0; i < iterations; i++) {
        positions[i] = v3_make((f32)rand() / RAND_MAX * 100.0f,
                               (f32)rand() / RAND_MAX * 100.0f,
                               (f32)rand() / RAND_MAX * 100.0f);
        
        // Create composite transform
        m4x4 translation = m4x4_translate(rand() % 100, rand() % 100, rand() % 100);
        m4x4 rotation = m4x4_rotate_y((f32)rand() / RAND_MAX * 6.28f);
        m4x4 scale = m4x4_scale_uniform(1.0f + (f32)rand() / RAND_MAX * 2.0f);
        transforms[i] = m4x4_mul(m4x4_mul(translation, rotation), scale);
    }
    
    // Benchmark point transformation
    timer_start(&timer);
    for (u32 i = 0; i < iterations; i++) {
        results[i] = m4x4_mul_v3_point(transforms[i], positions[i]);
    }
    f64 elapsed = timer_end(&timer);
    
    printf("  Point transforms: %u\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed);
    printf("  Per transform: %.6f µs\n", (elapsed * 1000.0) / iterations);
    printf("  Transforms/sec: %.0f\n", iterations / (elapsed / 1000.0));
    
    free(positions);
    free(transforms);
    free(results);
}

// Benchmark frustum culling
static void benchmark_frustum_culling(u32 object_count) {
    printf("\n=== Frustum Culling Benchmark ===\n");
    
    perf_timer timer;
    scene_object *objects = malloc(object_count * sizeof(scene_object));
    
    // Create view and projection matrices
    v3 camera_pos = v3_make(0, 10, -20);
    v3 camera_target = v3_make(0, 0, 0);
    v3 camera_up = v3_make(0, 1, 0);
    m4x4 view = m4x4_look_at(camera_pos, camera_target, camera_up);
    m4x4 projection = m4x4_perspective(hm_radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f);
    m4x4 view_projection = m4x4_mul(projection, view);
    
    // Extract frustum
    frustum f;
    extract_frustum_planes(&f, view_projection);
    
    // Initialize objects scattered in world
    for (u32 i = 0; i < object_count; i++) {
        objects[i].position = v3_make(
            (f32)(rand() % 200 - 100),
            (f32)(rand() % 50),
            (f32)(rand() % 200 - 100)
        );
        objects[i].bounding_radius = 1.0f + (f32)rand() / RAND_MAX * 5.0f;
        
        // Calculate AABB
        objects[i].bounds_min = v3_sub(objects[i].position, 
                                       v3_one_scale(objects[i].bounding_radius));
        objects[i].bounds_max = v3_add(objects[i].position,
                                       v3_one_scale(objects[i].bounding_radius));
        objects[i].is_visible = 0;
    }
    
    // Benchmark sphere culling
    timer_start(&timer);
    u32 visible_sphere = 0;
    for (u32 i = 0; i < object_count; i++) {
        objects[i].is_visible = sphere_in_frustum(&f, objects[i].position, 
                                                  objects[i].bounding_radius);
        if (objects[i].is_visible) visible_sphere++;
    }
    f64 sphere_time = timer_end(&timer);
    
    // Benchmark AABB culling
    timer_start(&timer);
    u32 visible_aabb = 0;
    for (u32 i = 0; i < object_count; i++) {
        objects[i].is_visible = aabb_in_frustum(&f, objects[i].bounds_min, 
                                                objects[i].bounds_max);
        if (objects[i].is_visible) visible_aabb++;
    }
    f64 aabb_time = timer_end(&timer);
    
    printf("  Objects tested: %u\n", object_count);
    printf("  Sphere culling:\n");
    printf("    Time: %.3f ms\n", sphere_time);
    printf("    Visible: %u (%.1f%%)\n", visible_sphere, 
           (visible_sphere * 100.0f) / object_count);
    printf("    Per test: %.6f µs\n", (sphere_time * 1000.0) / object_count);
    printf("  AABB culling:\n");
    printf("    Time: %.3f ms\n", aabb_time);
    printf("    Visible: %u (%.1f%%)\n", visible_aabb,
           (visible_aabb * 100.0f) / object_count);
    printf("    Per test: %.6f µs\n", (aabb_time * 1000.0) / object_count);
    
    free(objects);
}

// Benchmark draw call batching
static void benchmark_draw_batching(u32 object_count, u32 material_count, u32 shader_count) {
    printf("\n=== Draw Call Batching Benchmark ===\n");
    
    perf_timer timer;
    scene_object *objects = malloc(object_count * sizeof(scene_object));
    draw_command_sim *commands = malloc(object_count * sizeof(draw_command_sim));
    render_batch *batches = malloc(material_count * shader_count * sizeof(render_batch));
    
    // Initialize objects with random materials and shaders
    for (u32 i = 0; i < object_count; i++) {
        objects[i].position = v3_make(
            (f32)(rand() % 100),
            (f32)(rand() % 100),
            (f32)(rand() % 100)
        );
        objects[i].transform = m4x4_translate_v3(objects[i].position);
        objects[i].vertex_count = 100 + rand() % 1000;
        objects[i].index_count = objects[i].vertex_count * 3;
        objects[i].is_visible = 1;
        
        commands[i].object_id = i;
        commands[i].material_id = rand() % material_count;
        commands[i].shader_id = rand() % shader_count;
        commands[i].vertex_offset = i * 1000;
        commands[i].index_offset = i * 3000;
        commands[i].instance_count = 1;
    }
    
    // Initialize batches
    u32 batch_count = 0;
    for (u32 s = 0; s < shader_count; s++) {
        for (u32 m = 0; m < material_count; m++) {
            batches[batch_count].shader_id = s;
            batches[batch_count].material_id = m;
            batches[batch_count].commands = malloc(object_count * sizeof(draw_command_sim));
            batches[batch_count].command_count = 0;
            batches[batch_count].command_capacity = object_count;
            batch_count++;
        }
    }
    
    // Benchmark batching process
    timer_start(&timer);
    
    // Sort commands into batches
    for (u32 i = 0; i < object_count; i++) {
        if (objects[i].is_visible) {
            u32 batch_index = commands[i].shader_id * material_count + commands[i].material_id;
            u32 cmd_index = batches[batch_index].command_count++;
            batches[batch_index].commands[cmd_index] = commands[i];
        }
    }
    
    // Count actual draw calls
    u32 draw_calls = 0;
    u32 state_changes = 0;
    for (u32 b = 0; b < batch_count; b++) {
        if (batches[b].command_count > 0) {
            draw_calls += batches[b].command_count;
            state_changes++; // One state change per batch
        }
    }
    
    f64 elapsed = timer_end(&timer);
    
    printf("  Objects: %u\n", object_count);
    printf("  Materials: %u\n", material_count);
    printf("  Shaders: %u\n", shader_count);
    printf("  Batching time: %.3f ms\n", elapsed);
    printf("  Draw calls: %u\n", draw_calls);
    printf("  State changes: %u\n", state_changes);
    printf("  Batching ratio: %.2f:1\n", (f32)draw_calls / state_changes);
    
    // Cleanup
    for (u32 b = 0; b < batch_count; b++) {
        free(batches[b].commands);
    }
    free(objects);
    free(commands);
    free(batches);
}

// Benchmark memory allocation patterns
static void benchmark_memory_patterns(u32 allocation_count) {
    printf("\n=== Memory Allocation Patterns Benchmark ===\n");
    
    perf_timer timer;
    void **pointers = malloc(allocation_count * sizeof(void*));
    
    // Benchmark small allocations (vertex data simulation)
    timer_start(&timer);
    for (u32 i = 0; i < allocation_count; i++) {
        pointers[i] = malloc(sizeof(f32) * 12);  // 3 floats x 4 vertices
    }
    for (u32 i = 0; i < allocation_count; i++) {
        free(pointers[i]);
    }
    f64 small_time = timer_end(&timer);
    
    // Benchmark medium allocations (mesh data)
    timer_start(&timer);
    for (u32 i = 0; i < allocation_count / 10; i++) {
        pointers[i] = malloc(sizeof(f32) * 1000);  // ~1000 vertices
    }
    for (u32 i = 0; i < allocation_count / 10; i++) {
        free(pointers[i]);
    }
    f64 medium_time = timer_end(&timer);
    
    // Benchmark large allocations (texture data)
    timer_start(&timer);
    for (u32 i = 0; i < allocation_count / 100; i++) {
        pointers[i] = malloc(1024 * 1024);  // 1MB texture
    }
    for (u32 i = 0; i < allocation_count / 100; i++) {
        free(pointers[i]);
    }
    f64 large_time = timer_end(&timer);
    
    // Benchmark arena allocation pattern
    timer_start(&timer);
    size_t arena_size = Megabytes(16);
    void *arena_memory = malloc(arena_size);
    size_t arena_used = 0;
    
    // Simulate arena allocations
    for (u32 i = 0; i < allocation_count; i++) {
        size_t alloc_size = 100 + (rand() % 1000);
        if (arena_used + alloc_size <= arena_size) {
            pointers[i] = (u8*)arena_memory + arena_used;
            arena_used += alloc_size;
        }
    }
    free(arena_memory);  // Single free for entire arena
    f64 arena_time = timer_end(&timer);
    
    printf("  Small allocations (%u x 48 bytes):\n", allocation_count);
    printf("    Time: %.3f ms\n", small_time);
    printf("    Per alloc: %.6f µs\n", (small_time * 1000.0) / (allocation_count * 2));
    
    printf("  Medium allocations (%u x 4KB):\n", allocation_count / 10);
    printf("    Time: %.3f ms\n", medium_time);
    printf("    Per alloc: %.6f µs\n", (medium_time * 1000.0) / (allocation_count / 10 * 2));
    
    printf("  Large allocations (%u x 1MB):\n", allocation_count / 100);
    printf("    Time: %.3f ms\n", large_time);
    printf("    Per alloc: %.6f µs\n", (large_time * 1000.0) / (allocation_count / 100 * 2));
    
    printf("  Arena allocations (%u ops):\n", allocation_count);
    printf("    Time: %.3f ms\n", arena_time);
    printf("    Speedup vs small: %.2fx\n", small_time / arena_time);
    
    free(pointers);
}

// Benchmark scene graph traversal
static void benchmark_scene_traversal(u32 node_count, u32 depth) {
    printf("\n=== Scene Graph Traversal Benchmark ===\n");
    
    typedef struct scene_node {
        m4x4 local_transform;
        m4x4 world_transform;
        struct scene_node *parent;
        struct scene_node **children;
        u32 child_count;
        b32 dirty;
    } scene_node;
    
    perf_timer timer;
    scene_node *nodes = malloc(node_count * sizeof(scene_node));
    
    // Build a tree structure
    for (u32 i = 0; i < node_count; i++) {
        nodes[i].local_transform = m4x4_translate(
            (f32)(rand() % 10),
            (f32)(rand() % 10),
            (f32)(rand() % 10)
        );
        nodes[i].world_transform = m4x4_identity();
        nodes[i].parent = NULL;
        nodes[i].children = NULL;
        nodes[i].child_count = 0;
        nodes[i].dirty = 1;
    }
    
    // Create hierarchy (simple: each non-leaf has 4 children)
    u32 current_index = 1;
    for (u32 i = 0; i < node_count && current_index < node_count; i++) {
        u32 children_to_add = (rand() % 4) + 1;
        if (current_index + children_to_add > node_count) {
            children_to_add = node_count - current_index;
        }
        
        if (children_to_add > 0) {
            nodes[i].children = malloc(children_to_add * sizeof(scene_node*));
            nodes[i].child_count = children_to_add;
            
            for (u32 c = 0; c < children_to_add; c++) {
                nodes[i].children[c] = &nodes[current_index];
                nodes[current_index].parent = &nodes[i];
                current_index++;
            }
        }
    }
    
    // Function to update transforms recursively
    void update_transforms(scene_node *node) {
        if (node->dirty) {
            if (node->parent) {
                node->world_transform = m4x4_mul(node->parent->world_transform,
                                                 node->local_transform);
            } else {
                node->world_transform = node->local_transform;
            }
            node->dirty = 0;
        }
        
        for (u32 i = 0; i < node->child_count; i++) {
            update_transforms(node->children[i]);
        }
    }
    
    // Benchmark full traversal
    timer_start(&timer);
    update_transforms(&nodes[0]);
    f64 full_traversal = timer_end(&timer);
    
    // Mark some nodes dirty and re-traverse
    for (u32 i = 0; i < node_count / 10; i++) {
        u32 index = rand() % node_count;
        nodes[index].dirty = 1;
        nodes[index].local_transform = m4x4_rotate_y((f32)rand() / RAND_MAX * 6.28f);
    }
    
    timer_start(&timer);
    update_transforms(&nodes[0]);
    f64 partial_traversal = timer_end(&timer);
    
    printf("  Node count: %u\n", node_count);
    printf("  Full traversal: %.3f ms\n", full_traversal);
    printf("  Per node: %.6f µs\n", (full_traversal * 1000.0) / node_count);
    printf("  Partial update (10%% dirty): %.3f ms\n", partial_traversal);
    printf("  Speedup: %.2fx\n", full_traversal / partial_traversal);
    
    // Cleanup
    for (u32 i = 0; i < node_count; i++) {
        if (nodes[i].children) {
            free(nodes[i].children);
        }
    }
    free(nodes);
}

// Benchmark vector operations with SIMD potential
static void benchmark_vector_ops(u32 vector_count) {
    printf("\n=== Vector Operations Benchmark ===\n");
    
    perf_timer timer;
    v3 *vectors_a = malloc(vector_count * sizeof(v3));
    v3 *vectors_b = malloc(vector_count * sizeof(v3));
    v3 *results = malloc(vector_count * sizeof(v3));
    f32 *dot_results = malloc(vector_count * sizeof(f32));
    
    // Initialize vectors
    for (u32 i = 0; i < vector_count; i++) {
        vectors_a[i] = v3_make((f32)rand() / RAND_MAX,
                               (f32)rand() / RAND_MAX,
                               (f32)rand() / RAND_MAX);
        vectors_b[i] = v3_make((f32)rand() / RAND_MAX,
                               (f32)rand() / RAND_MAX,
                               (f32)rand() / RAND_MAX);
    }
    
    // Benchmark vector addition
    timer_start(&timer);
    for (u32 i = 0; i < vector_count; i++) {
        results[i] = v3_add(vectors_a[i], vectors_b[i]);
    }
    f64 add_time = timer_end(&timer);
    
    // Benchmark dot product
    timer_start(&timer);
    for (u32 i = 0; i < vector_count; i++) {
        dot_results[i] = v3_dot(vectors_a[i], vectors_b[i]);
    }
    f64 dot_time = timer_end(&timer);
    
    // Benchmark cross product
    timer_start(&timer);
    for (u32 i = 0; i < vector_count; i++) {
        results[i] = v3_cross(vectors_a[i], vectors_b[i]);
    }
    f64 cross_time = timer_end(&timer);
    
    // Benchmark normalization
    timer_start(&timer);
    for (u32 i = 0; i < vector_count; i++) {
        results[i] = v3_normalize(vectors_a[i]);
    }
    f64 normalize_time = timer_end(&timer);
    
    printf("  Vector count: %u\n", vector_count);
    printf("  Addition: %.3f ms (%.2f Gops/s)\n", add_time,
           (vector_count / 1e9) / (add_time / 1000.0));
    printf("  Dot product: %.3f ms (%.2f Gops/s)\n", dot_time,
           (vector_count / 1e9) / (dot_time / 1000.0));
    printf("  Cross product: %.3f ms (%.2f Gops/s)\n", cross_time,
           (vector_count / 1e9) / (cross_time / 1000.0));
    printf("  Normalization: %.3f ms (%.2f Gops/s)\n", normalize_time,
           (vector_count / 1e9) / (normalize_time / 1000.0));
    
    free(vectors_a);
    free(vectors_b);
    free(results);
    free(dot_results);
}

// Main benchmark runner
int main(int argc, char **argv) {
    printf("========================================\n");
    printf("    HEADLESS RENDERER BENCHMARK\n");
    printf("========================================\n");
    printf("CPU-side rendering performance analysis\n");
    printf("No OpenGL/X11 dependencies required\n");
    
    // Get CPU info
    printf("\nSystem Information:\n");
    system("lscpu | grep 'Model name' | head -1");
    system("lscpu | grep 'CPU MHz' | head -1");
    system("lscpu | grep 'L3 cache' | head -1");
    
    // Seed random for consistent results
    srand(12345);
    
    // Run benchmarks with increasing complexity
    printf("\n----------------------------------------\n");
    
    // Matrix operations
    benchmark_matrix_multiply(100000);
    benchmark_matrix_multiply(1000000);
    
    // Transform operations
    benchmark_transforms(100000);
    benchmark_transforms(1000000);
    
    // Frustum culling
    benchmark_frustum_culling(10000);
    benchmark_frustum_culling(100000);
    
    // Draw call batching
    benchmark_draw_batching(10000, 10, 5);
    benchmark_draw_batching(50000, 50, 10);
    
    // Memory patterns
    benchmark_memory_patterns(10000);
    
    // Scene graph
    benchmark_scene_traversal(10000, 5);
    benchmark_scene_traversal(50000, 8);
    
    // Vector operations
    benchmark_vector_ops(1000000);
    benchmark_vector_ops(10000000);
    
    printf("\n========================================\n");
    printf("         BENCHMARK COMPLETE\n");
    printf("========================================\n");
    
    return 0;
}