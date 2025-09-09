/*
    Minimal Physics Engine Test
    Step-by-step debugging of the physics engine
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

// Include all physics source files directly
#include "handmade_physics.c"
#include "physics_broadphase.c"
#include "physics_collision.c" 
#include "physics_solver.c"

int main() {
    printf("=== Minimal Physics Engine Test ===\n");
    
    // Test 1: Basic vector operations
    printf("1. Testing vector operations...\n");
    v3 a = V3(1.0f, 2.0f, 3.0f);
    v3 b = V3(4.0f, 5.0f, 6.0f);
    v3 c = V3Add(a, b);
    
    printf("   V3Add(1,2,3) + (4,5,6) = (%.1f, %.1f, %.1f)\n", c.x, c.y, c.z);
    
    if (c.x == 5.0f && c.y == 7.0f && c.z == 9.0f) {
        printf("   SUCCESS: Vector math working\n");
    } else {
        printf("   ERROR: Vector math failed\n");
        return 1;
    }
    
    // Test 2: Quaternion operations
    printf("2. Testing quaternion operations...\n");
    quaternion q = QuaternionIdentity();
    printf("   Identity quaternion: (%.1f, %.1f, %.1f, %.1f)\n", q.x, q.y, q.z, q.w);
    
    if (q.w == 1.0f && q.x == 0.0f && q.y == 0.0f && q.z == 0.0f) {
        printf("   SUCCESS: Quaternion math working\n");
    } else {
        printf("   ERROR: Quaternion math failed\n");
        return 1;
    }
    
    // Test 3: Memory allocation test
    printf("3. Testing arena allocation...\n");
    u8 test_arena[1024];
    printf("   Created 1KB test arena at address: %p\n", test_arena);
    
    // Test 4: Physics world creation (most likely place for crash)
    printf("4. Testing physics world creation...\n");
    
    const size_t ARENA_SIZE = Megabytes(1);  // Start small
    u8 *arena = malloc(ARENA_SIZE);
    if (!arena) {
        printf("   ERROR: Failed to allocate arena memory\n");
        return 1;
    }
    printf("   Allocated %zu MB arena at address: %p\n", ARENA_SIZE / (1024*1024), arena);
    
    printf("   Calling PhysicsCreateWorld...\n");
    physics_world *world = PhysicsCreateWorld(ARENA_SIZE, arena);
    
    if (world) {
        printf("   SUCCESS: Physics world created at address: %p\n", world);
        printf("   World body count: %u\n", world->BodyCount);
        printf("   World max bodies: %u\n", world->MaxBodies);
    } else {
        printf("   ERROR: Failed to create physics world\n");
        free(arena);
        return 1;
    }
    
    // Test 5: Basic body creation
    printf("5. Testing body creation...\n");
    v3 pos = V3(0, 0, 0);
    quaternion rot = QuaternionIdentity();
    
    u32 body_id = PhysicsCreateBody(world, pos, rot);
    printf("   Created body with ID: %u\n", body_id);
    printf("   World now has %u bodies\n", world->BodyCount);
    
    if (world->BodyCount == 1) {
        printf("   SUCCESS: Body creation working\n");
    } else {
        printf("   ERROR: Body creation failed\n");
        free(arena);
        return 1;
    }
    
    // Test 6: Shape creation
    printf("6. Testing shape creation...\n");
    collision_shape sphere = PhysicsCreateSphere(1.0f);
    printf("   Created sphere with radius: %.1f\n", sphere.Sphere.Radius);
    
    if (sphere.Type == SHAPE_SPHERE && sphere.Sphere.Radius == 1.0f) {
        printf("   SUCCESS: Shape creation working\n");
    } else {
        printf("   ERROR: Shape creation failed\n");
        free(arena);
        return 1;
    }
    
    printf("\n=== All Tests Passed ===\n");
    printf("Physics engine basic functionality verified!\n");
    
    free(arena);
    return 0;
}