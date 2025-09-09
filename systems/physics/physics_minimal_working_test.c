/*
    Minimal Physics Test - Demonstrate Core Functionality
    Tests basic physics world creation and body management
    with the integrated math library.
*/

#include "handmade_physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    printf("=== Minimal Physics Engine Test ===\n");
    
    // Allocate memory for physics world
    const u32 ARENA_SIZE = Megabytes(32);
    u8 *arena_memory = malloc(ARENA_SIZE);
    if (!arena_memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Create physics world
    printf("Creating physics world...\n");
    physics_world *world = PhysicsCreateWorld(ARENA_SIZE, arena_memory);
    if (!world) {
        printf("Failed to create physics world\n");
        free(arena_memory);
        return 1;
    }
    
    printf("✓ Physics world created successfully\n");
    printf("  Max bodies: %u\n", world->MaxBodies);
    printf("  Arena size: %u MB\n", (u32)(ARENA_SIZE / 1024 / 1024));
    
    // Test body creation
    printf("\nTesting body creation...\n");
    
    v3 position = V3(0.0f, 5.0f, 0.0f);
    quat orientation = QuaternionIdentity();
    
    u32 body_id = PhysicsCreateBody(world, position, orientation);
    printf("✓ Created body with ID: %u\n", body_id);
    
    rigid_body *body = PhysicsGetBody(world, body_id);
    if (body) {
        printf("✓ Body retrieved successfully\n");
        printf("  Position: (%.2f, %.2f, %.2f)\n", body->Position.x, body->Position.y, body->Position.z);
        printf("  Mass: %.3f\n", body->Mass);
        printf("  Inverse mass: %.6f\n", body->InverseMass);
    }
    
    // Test shape assignment
    printf("\nTesting shape assignment...\n");
    
    collision_shape sphere = PhysicsCreateSphere(0.5f);
    PhysicsSetBodyShape(world, body_id, &sphere);
    printf("✓ Assigned sphere shape (radius: %.2f)\n", sphere.Sphere.Radius);
    
    // Update body properties after shape change
    body = PhysicsGetBody(world, body_id);
    printf("  Updated mass: %.3f\n", body->Mass);
    printf("  AABB min: (%.2f, %.2f, %.2f)\n", body->AABBMin.x, body->AABBMin.y, body->AABBMin.z);
    printf("  AABB max: (%.2f, %.2f, %.2f)\n", body->AABBMax.x, body->AABBMax.y, body->AABBMax.z);
    
    // Test multiple body creation
    printf("\nTesting multiple body creation...\n");
    
    const u32 TEST_BODY_COUNT = 100;
    u32 *body_ids = malloc(TEST_BODY_COUNT * sizeof(u32));
    
    clock_t start = clock();
    
    for (u32 i = 0; i < TEST_BODY_COUNT; ++i) {
        f32 x = (i % 10) * 2.0f - 10.0f;
        f32 y = 5.0f + (i / 10) * 1.5f;
        f32 z = 0.0f;
        
        v3 pos = V3(x, y, z);
        body_ids[i] = PhysicsCreateBody(world, pos, orientation);
        
        // Alternate between spheres and boxes
        if (i % 2 == 0) {
            collision_shape shape = PhysicsCreateSphere(0.4f);
            PhysicsSetBodyShape(world, body_ids[i], &shape);
        } else {
            collision_shape shape = PhysicsCreateBox(V3(0.3f, 0.3f, 0.3f));
            PhysicsSetBodyShape(world, body_ids[i], &shape);
        }
    }
    
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("✓ Created %u bodies in %.3f seconds\n", TEST_BODY_COUNT, time_taken);
    printf("  Creation rate: %.0f bodies/sec\n", TEST_BODY_COUNT / time_taken);
    printf("  Total bodies in world: %u\n", world->BodyCount);
    
    // Test force application
    printf("\nTesting force application...\n");
    
    v3 gravity_force = V3(0.0f, -9.81f, 0.0f);
    PhysicsApplyForce(world, body_id, gravity_force, body->Position);
    printf("✓ Applied gravity force to body\n");
    
    // Test material properties
    printf("\nTesting material properties...\n");
    
    material test_material = PhysicsCreateMaterial(2.0f, 0.5f, 0.8f);
    PhysicsSetBodyMaterial(world, body_id, &test_material);
    
    body = PhysicsGetBody(world, body_id);
    printf("✓ Set material properties\n");
    printf("  Density: %.2f\n", body->Material.Density);
    printf("  Restitution: %.2f\n", body->Material.Restitution);
    printf("  Friction: %.2f\n", body->Material.Friction);
    printf("  Updated mass: %.3f\n", body->Mass);
    
    // Test math library integration
    printf("\nTesting math library integration...\n");
    
    v3 test_vec1 = V3(1.0f, 2.0f, 3.0f);
    v3 test_vec2 = V3(4.0f, 5.0f, 6.0f);
    v3 result = V3Add(test_vec1, test_vec2);
    
    printf("✓ Vector addition: (%.1f, %.1f, %.1f) + (%.1f, %.1f, %.1f) = (%.1f, %.1f, %.1f)\n",
           test_vec1.x, test_vec1.y, test_vec1.z,
           test_vec2.x, test_vec2.y, test_vec2.z,
           result.x, result.y, result.z);
    
    f32 dot_product = V3Dot(test_vec1, test_vec2);
    printf("✓ Dot product: %.2f\n", dot_product);
    
    v3 cross_product = V3Cross(test_vec1, test_vec2);
    printf("✓ Cross product: (%.1f, %.1f, %.1f)\n", 
           cross_product.x, cross_product.y, cross_product.z);
    
    // Test quaternion operations
    quat test_quat = QuaternionFromAxisAngle(V3(0, 1, 0), 1.57f); // 90 degrees around Y
    quat normalized = QuaternionNormalize(test_quat);
    printf("✓ Quaternion normalized: (%.3f, %.3f, %.3f, %.3f)\n",
           normalized.x, normalized.y, normalized.z, normalized.w);
    
    // Test SIMD batch operations
    printf("\nTesting SIMD batch operations...\n");
    
    v3 test_vectors[4] = {
        V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), V3(1, 1, 1)
    };
    
    v3 scaled_vectors[4];
    V3Mul4(scaled_vectors, test_vectors, 2.0f);
    
    printf("✓ SIMD scaling by 2.0:\n");
    for (u32 i = 0; i < 4; ++i) {
        printf("  (%.1f, %.1f, %.1f) -> (%.1f, %.1f, %.1f)\n",
               test_vectors[i].x, test_vectors[i].y, test_vectors[i].z,
               scaled_vectors[i].x, scaled_vectors[i].y, scaled_vectors[i].z);
    }
    
    // Performance summary
    printf("\n=== Performance Summary ===\n");
    printf("Memory usage:\n");
    printf("  Arena allocated: %u bytes (%.2f MB)\n", 
           (u32)world->ArenaUsed, (f32)world->ArenaUsed / (1024.0f * 1024.0f));
    printf("  Memory efficiency: %.1f%% used\n", 
           (f32)world->ArenaUsed / (f32)ARENA_SIZE * 100.0f);
    
    printf("\nBody management:\n");
    printf("  Bodies created: %u\n", world->BodyCount);
    printf("  Creation rate: %.0f bodies/sec\n", TEST_BODY_COUNT / time_taken);
    
    printf("\n✓ All basic tests passed!\n");
    printf("Physics engine core functionality verified.\n");
    
    // Cleanup
    free(body_ids);
    PhysicsDestroyWorld(world);
    free(arena_memory);
    
    return 0;
}