/*
    Test program for terrain collision and raycasting
*/

#include "handmade_noise.c"
#include "handmade_terrain.c"
#include "handmade_terrain_collision.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>

// Simple arena
typedef struct {
    u8* base;
    u64 size;
    u64 used;
} test_arena;

void* arena_push_size(arena* arena_ptr, u64 size, u32 alignment) {
    test_arena* a = (test_arena*)arena_ptr;
    u64 aligned = (a->used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > a->size) {
        printf("Arena out of memory!\n");
        return 0;
    }
    void* result = a->base + aligned;
    a->used = aligned + size;
    return result;
}

// Stub dependencies
struct renderer_state { int dummy; };
struct asset_system { int dummy; };

// Test helpers
void print_v3(const char* label, v3 v) {
    printf("%s: (%.2f, %.2f, %.2f)\n", label, v.x, v.y, v.z);
}

// =============================================================================
// COLLISION TESTS
// =============================================================================

void test_height_queries(terrain_system* terrain) {
    printf("\n=== Height Query Tests ===\n");
    
    // Test basic height query
    f32 h1 = terrain_get_height(terrain, 0, 0);
    f32 h2 = terrain_get_height_interpolated(terrain, 0, 0);
    printf("Height at (0,0): %.2f (interpolated: %.2f)\n", h1, h2);
    
    // Test interpolation between points
    f32 h_mid = terrain_get_height_interpolated(terrain, 0.5f, 0.5f);
    printf("Height at (0.5,0.5): %.2f\n", h_mid);
    
    // Test normal calculation
    v3 normal = terrain_get_normal(terrain, 0, 0);
    print_v3("Normal at (0,0)", normal);
    
    // Verify normal is normalized
    f32 normal_len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    printf("Normal length: %.4f (should be ~1.0)\n", normal_len);
    assert(fabsf(normal_len - 1.0f) < 0.01f);
    
    // Test slope
    f32 slope = terrain_get_slope(terrain, 0, 0);
    printf("Slope at (0,0): %.2f degrees\n", slope);
    
    // Test walkability
    b32 walkable = terrain_is_walkable(terrain, 0, 0, 45.0f);
    printf("Is walkable (max 45°): %s\n", walkable ? "Yes" : "No");
}

void test_sphere_collision(terrain_system* terrain) {
    printf("\n=== Sphere Collision Tests ===\n");
    
    f32 terrain_height = terrain_get_height_interpolated(terrain, 10, 10);
    printf("Terrain height at (10,10): %.2f\n", terrain_height);
    
    // Test sphere above terrain
    v3 sphere_pos = {10, terrain_height + 10, 10};
    f32 radius = 2.0f;
    v3 out_pos, out_normal;
    
    b32 collision = terrain_sphere_collision(terrain, sphere_pos, radius, &out_pos, &out_normal);
    printf("Sphere above terrain: %s\n", collision ? "Collision" : "No collision");
    
    // Test sphere intersecting terrain
    sphere_pos.y = terrain_height + 1.0f;
    collision = terrain_sphere_collision(terrain, sphere_pos, radius, &out_pos, &out_normal);
    printf("Sphere intersecting: %s\n", collision ? "Collision" : "No collision");
    if (collision) {
        print_v3("  Resolved position", out_pos);
        print_v3("  Contact normal", out_normal);
    }
    
    // Test sphere below terrain
    sphere_pos.y = terrain_height - 5.0f;
    collision = terrain_sphere_collision(terrain, sphere_pos, radius, &out_pos, &out_normal);
    printf("Sphere below terrain: %s\n", collision ? "Collision" : "No collision");
    if (collision) {
        printf("  Resolved Y: %.2f (terrain: %.2f)\n", out_pos.y, terrain_height);
    }
}

void test_raycast(terrain_system* terrain) {
    printf("\n=== Raycast Tests ===\n");
    
    // Test 1: Ray from above pointing down
    v3 origin = {50, 200, 50};
    v3 direction = {0, -1, 0};
    v3 hit_point, hit_normal;
    
    clock_t start = clock();
    b32 hit = terrain_raycast(terrain, origin, direction, 500.0f, &hit_point, &hit_normal);
    f64 time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("Downward ray: %s (%.3f ms)\n", hit ? "Hit" : "Miss", time_ms);
    if (hit) {
        print_v3("  Hit point", hit_point);
        print_v3("  Hit normal", hit_normal);
        
        // Verify hit point is on terrain
        f32 terrain_h = terrain_get_height_interpolated(terrain, hit_point.x, hit_point.z);
        printf("  Hit accuracy: %.4f (terrain height: %.2f)\n", 
               fabsf(hit_point.y - terrain_h), terrain_h);
    }
    
    // Test 2: Diagonal ray
    origin = (v3){0, 150, 0};
    direction = (v3){1, -0.5f, 1};
    
    start = clock();
    hit = terrain_raycast(terrain, origin, direction, 500.0f, &hit_point, &hit_normal);
    time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("Diagonal ray: %s (%.3f ms)\n", hit ? "Hit" : "Miss", time_ms);
    if (hit) {
        print_v3("  Hit point", hit_point);
    }
    
    // Test 3: Ray going up (should miss)
    origin = (v3){0, 150, 0};
    direction = (v3){0, 1, 0};
    hit = terrain_raycast(terrain, origin, direction, 100.0f, &hit_point, &hit_normal);
    printf("Upward ray: %s\n", hit ? "Hit" : "Miss");
    
    // Test 4: Horizontal ray close to terrain
    f32 terrain_h = terrain_get_height_interpolated(terrain, 100, 100);
    origin = (v3){100, terrain_h + 5, 100};
    direction = (v3){1, 0, 0};
    
    start = clock();
    hit = terrain_raycast(terrain, origin, direction, 100.0f, &hit_point, &hit_normal);
    time_ms = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("Horizontal ray: %s (%.3f ms)\n", hit ? "Hit" : "Miss", time_ms);
}

void test_capsule_collision(terrain_system* terrain) {
    printf("\n=== Capsule Collision Tests ===\n");
    
    f32 terrain_height = terrain_get_height_interpolated(terrain, 20, 20);
    
    // Standing capsule
    v3 bottom = {20, terrain_height - 1, 20};
    v3 top = {20, terrain_height + 3, 20};
    f32 radius = 0.5f;
    v3 out_pos, out_normal;
    
    b32 collision = terrain_capsule_collision(terrain, bottom, top, radius, &out_pos, &out_normal);
    printf("Standing capsule: %s\n", collision ? "Collision" : "No collision");
    if (collision) {
        print_v3("  Resolved position", out_pos);
        printf("  Penetration resolved: %.2f\n", out_pos.y - bottom.y);
    }
    
    // Tilted capsule
    bottom = (v3){30, terrain_height + 1, 30};
    top = (v3){35, terrain_height + 2, 35};
    collision = terrain_capsule_collision(terrain, bottom, top, radius, &out_pos, &out_normal);
    printf("Tilted capsule: %s\n", collision ? "Collision" : "No collision");
}

void test_box_collision(terrain_system* terrain) {
    printf("\n=== Box Collision Tests ===\n");
    
    f32 terrain_height = terrain_get_height_interpolated(terrain, 40, 40);
    
    // Box partially underground
    v3 box_min = {40, terrain_height - 2, 40};
    v3 box_max = {45, terrain_height + 2, 45};
    v3 out_pos, out_normal;
    
    b32 collision = terrain_box_collision(terrain, box_min, box_max, &out_pos, &out_normal);
    printf("Box collision: %s\n", collision ? "Collision" : "No collision");
    if (collision) {
        print_v3("  Resolved position", out_pos);
        printf("  Box lifted by: %.2f\n", out_pos.y - box_min.y);
    }
}

void test_line_of_sight(terrain_system* terrain) {
    printf("\n=== Line of Sight Tests ===\n");
    
    // Test clear line of sight (both points high above terrain)
    v3 from = {0, 200, 0};
    v3 to = {100, 200, 100};
    
    b32 clear = terrain_line_of_sight(terrain, from, to);
    printf("High altitude LOS: %s\n", clear ? "Clear" : "Blocked");
    
    // Test blocked line of sight (through terrain)
    f32 h1 = terrain_get_height_interpolated(terrain, 0, 0);
    f32 h2 = terrain_get_height_interpolated(terrain, 100, 0);
    from = (v3){0, h1 + 2, 0};
    to = (v3){100, h2 + 2, 0};
    
    clear = terrain_line_of_sight(terrain, from, to);
    printf("Low altitude LOS: %s\n", clear ? "Clear" : "Blocked");
}

void benchmark_collision(terrain_system* terrain) {
    printf("\n=== Collision Performance ===\n");
    
    const u32 ITERATIONS = 10000;
    
    // Benchmark height queries
    clock_t start = clock();
    f32 sum = 0;
    for (u32 i = 0; i < ITERATIONS; i++) {
        f32 x = ((f32)rand() / RAND_MAX) * 1000.0f;
        f32 z = ((f32)rand() / RAND_MAX) * 1000.0f;
        sum += terrain_get_height_interpolated(terrain, x, z);
    }
    f64 height_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark sphere collision
    start = clock();
    u32 collision_count = 0;
    for (u32 i = 0; i < ITERATIONS; i++) {
        f32 x = ((f32)rand() / RAND_MAX) * 1000.0f;
        f32 z = ((f32)rand() / RAND_MAX) * 1000.0f;
        f32 y = ((f32)rand() / RAND_MAX) * 200.0f;
        v3 pos = {x, y, z};
        v3 out_pos, out_normal;
        if (terrain_sphere_collision(terrain, pos, 2.0f, &out_pos, &out_normal)) {
            collision_count++;
        }
    }
    f64 sphere_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark raycasts
    start = clock();
    u32 hit_count = 0;
    for (u32 i = 0; i < ITERATIONS / 10; i++) {  // Fewer iterations as raycast is slower
        v3 origin = {
            ((f32)rand() / RAND_MAX) * 1000.0f,
            200.0f,
            ((f32)rand() / RAND_MAX) * 1000.0f
        };
        v3 direction = {
            ((f32)rand() / RAND_MAX) * 2.0f - 1.0f,
            -1.0f,
            ((f32)rand() / RAND_MAX) * 2.0f - 1.0f
        };
        v3 hit_point, hit_normal;
        if (terrain_raycast(terrain, origin, direction, 500.0f, &hit_point, &hit_normal)) {
            hit_count++;
        }
    }
    f64 raycast_time = (double)(clock() - start) / CLOCKS_PER_SEC * 1000.0;
    
    printf("Height queries: %.2f ms for %u queries (%.0f/ms)\n",
           height_time, ITERATIONS, ITERATIONS / height_time);
    printf("Sphere checks:  %.2f ms for %u checks (%.0f/ms, %u collisions)\n",
           sphere_time, ITERATIONS, ITERATIONS / sphere_time, collision_count);
    printf("Raycasts:       %.2f ms for %u rays (%.0f/ms, %u hits)\n",
           raycast_time, ITERATIONS/10, (ITERATIONS/10) / raycast_time, hit_count);
}

int main() {
    printf("=== Terrain Collision System Test ===\n");
    
    // Create arena
    test_arena arena = {0};
    arena.size = 128 * 1024 * 1024;  // 128MB
    arena.base = aligned_alloc(32, arena.size);
    
    // Initialize systems
    struct renderer_state renderer = {0};
    struct asset_system assets = {0};
    
    noise_state* noise = noise_init((struct arena*)&arena, 12345);
    terrain_system* terrain = terrain_init((struct arena*)&arena, &renderer, &assets, 12345);
    
    // Generate a test chunk
    terrain_chunk* chunk = &terrain->chunks[0];
    terrain_generate_chunk(terrain, chunk, 0, 0, 0);
    
    // Run tests
    test_height_queries(terrain);
    test_sphere_collision(terrain);
    test_raycast(terrain);
    test_capsule_collision(terrain);
    test_box_collision(terrain);
    test_line_of_sight(terrain);
    
    // Benchmark
    benchmark_collision(terrain);
    
    printf("\n=== Test Complete ===\n");
    
    free(arena.base);
    return 0;
}