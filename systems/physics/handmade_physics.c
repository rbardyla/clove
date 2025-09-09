/*
    Handmade Physics Engine - Core Implementation
    Zero dependencies, SIMD optimized, deterministic
    
    Memory layout philosophy:
    - Structure of Arrays for SIMD vectorization
    - Cache-coherent data access patterns
    - Arena allocation for zero fragmentation
    - Fixed memory budget - no dynamic allocation
*/

#include "handmade_physics.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

// ========================================================================
// SIMD VECTOR MATH IMPLEMENTATION
// ========================================================================

// PERFORMANCE: Hot path - used in every physics calculation
// MEMORY: Processes 16 bytes (1 SIMD register) per operation
// ========================================================================
// SIMD BATCH OPERATIONS FOR PHYSICS
// ========================================================================

// SIMD batch operations now implemented in header

// Quaternion and matrix operations now handled by math library

// Fixed-point math now implemented in header

// ========================================================================
// PHYSICS WORLD MANAGEMENT
// ========================================================================

physics_world* 
PhysicsCreateWorld(memory_index ArenaSize, void *ArenaMemory)
{
    Assert(ArenaMemory);
    Assert(ArenaSize >= Megabytes(4));  // Minimum 4MB for physics
    
    physics_world *World = (physics_world*)ArenaMemory;
    *World = (physics_world){0};  // Zero initialize
    
    World->ArenaSize = ArenaSize;
    World->ArenaBase = (u8*)ArenaMemory;
    World->ArenaUsed = sizeof(physics_world);
    
    // Allocate arrays from arena
    World->MaxBodies = PHYSICS_MAX_BODIES;
    World->Bodies = (rigid_body*)PhysicsArenaAllocate(World, World->MaxBodies * sizeof(rigid_body));
    
    World->MaxManifolds = PHYSICS_MAX_CONTACTS;
    World->Manifolds = (contact_manifold*)PhysicsArenaAllocate(World, World->MaxManifolds * sizeof(contact_manifold));
    
    World->MaxBroadPhasePairs = PHYSICS_MAX_BODIES * 10;  // Estimate
    World->BroadPhasePairs = (broad_phase_pair*)PhysicsArenaAllocate(World, World->MaxBroadPhasePairs * sizeof(broad_phase_pair));
    
    World->MaxConstraints = PHYSICS_MAX_CONSTRAINTS;
    World->Constraints = (constraint*)PhysicsArenaAllocate(World, World->MaxConstraints * sizeof(constraint));
    
    // Initialize spatial hash grid
    World->BroadPhase.CellSize = PHYSICS_BROADPHASE_CELL_SIZE;
    World->BroadPhase.Origin = V3(0.0f, 0.0f, 0.0f);
    
    for (u32 i = 0; i < SPATIAL_HASH_SIZE; ++i)
    {
        spatial_cell *Cell = &World->BroadPhase.Cells[i];
        Cell->BodyCapacity = 64;  // Initial capacity per cell
        Cell->Bodies = (u32*)PhysicsArenaAllocate(World, Cell->BodyCapacity * sizeof(u32));
        Cell->BodyCount = 0;
    }
    
    // Set default simulation parameters
    World->Gravity = V3(0.0f, PHYSICS_GRAVITY, 0.0f);
    World->TimeStep = PHYSICS_TIMESTEP;
    World->SolverIterations = PHYSICS_SOLVER_ITERATIONS;
    World->AccumulatedTime = 0.0f;
    
    return World;
}

void 
PhysicsDestroyWorld(physics_world *World)
{
    // Nothing to do - using arena allocation
    if (World)
    {
        World->IsSimulating = 0;
    }
}

void 
PhysicsResetWorld(physics_world *World)
{
    Assert(World);
    Assert(!World->IsSimulating);  // Don't reset during simulation
    
    World->BodyCount = 0;
    World->ManifoldCount = 0;
    World->BroadPhasePairCount = 0;
    World->ConstraintCount = 0;
    World->AccumulatedTime = 0.0f;
    
    // Reset spatial hash grid
    for (u32 i = 0; i < SPATIAL_HASH_SIZE; ++i)
    {
        World->BroadPhase.Cells[i].BodyCount = 0;
    }
}

// ========================================================================
// MEMORY ARENA MANAGEMENT
// ========================================================================

void* 
PhysicsArenaAllocate(physics_world *World, memory_index Size)
{
    Assert(World);
    Assert(Size > 0);
    
    // Align allocation to 16-byte boundary for SIMD
    memory_index AlignedSize = Align16(Size);
    
    Assert(World->ArenaUsed + AlignedSize <= World->ArenaSize);
    
    void *Result = World->ArenaBase + World->ArenaUsed;
    World->ArenaUsed += AlignedSize;
    
    // Zero initialize allocated memory
    memset(Result, 0, AlignedSize);
    
    return Result;
}

void 
PhysicsArenaReset(physics_world *World)
{
    Assert(World);
    World->ArenaUsed = sizeof(physics_world);
}

// ========================================================================
// SHAPE UTILITIES
// ========================================================================

collision_shape 
PhysicsCreateSphere(f32 Radius)
{
    collision_shape Shape = {0};
    Shape.Type = SHAPE_SPHERE;
    Shape.Sphere.Radius = (Radius > 0.001f) ? Radius : 0.001f;  // Minimum radius
    return Shape;
}

collision_shape 
PhysicsCreateBox(v3 HalfExtents)
{
    collision_shape Shape = {0};
    Shape.Type = SHAPE_BOX;
    
    // Ensure minimum size
    Shape.Box.HalfExtents.x = (HalfExtents.x > 0.001f) ? HalfExtents.x : 0.001f;
    Shape.Box.HalfExtents.y = (HalfExtents.y > 0.001f) ? HalfExtents.y : 0.001f;
    Shape.Box.HalfExtents.z = (HalfExtents.z > 0.001f) ? HalfExtents.z : 0.001f;
    
    return Shape;
}

collision_shape 
PhysicsCreateCapsule(f32 Radius, f32 Height)
{
    collision_shape Shape = {0};
    Shape.Type = SHAPE_CAPSULE;
    Shape.Capsule.Radius = (Radius > 0.001f) ? Radius : 0.001f;
    Shape.Capsule.Height = (Height > 0.001f) ? Height : 0.001f;
    return Shape;
}

collision_shape 
PhysicsCreatePlane(v3 Normal, f32 Distance)
{
    collision_shape Shape = {0};
    Shape.Type = SHAPE_PLANE;
    Shape.Plane.Normal = V3Normalize(Normal);
    Shape.Plane.Distance = Distance;
    return Shape;
}

collision_shape 
PhysicsCreateConvexHull(v3 *Vertices, u32 VertexCount)
{
    collision_shape Shape = {0};
    Shape.Type = SHAPE_CONVEX_HULL;
    Shape.ConvexHull.Vertices = Vertices;
    Shape.ConvexHull.VertexCount = VertexCount;
    
    // Calculate centroid
    v3 Centroid = V3(0, 0, 0);
    for (u32 i = 0; i < VertexCount; ++i)
    {
        Centroid = V3Add(Centroid, Vertices[i]);
    }
    Shape.ConvexHull.LocalCentroid = V3Mul(Centroid, 1.0f / (f32)VertexCount);
    
    return Shape;
}

// ========================================================================
// MATERIAL UTILITIES
// ========================================================================

material 
PhysicsCreateMaterial(f32 Density, f32 Restitution, f32 Friction)
{
    material Mat;
    Mat.Density = (Density > 0.0f) ? Density : PHYSICS_DEFAULT_DENSITY;
    Mat.Restitution = Maximum(0.0f, Minimum(1.0f, Restitution));  // Clamp [0,1]
    Mat.Friction = Maximum(0.0f, Friction);  // No upper limit for friction
    Mat.LinearDamping = 0.01f;   // Small amount of air resistance
    Mat.AngularDamping = 0.05f;  // Rotational damping
    return Mat;
}

// ========================================================================
// MASS PROPERTY CALCULATION
// ========================================================================

void 
PhysicsCalculateMassProperties(rigid_body *Body)
{
    Assert(Body);
    
    if (Body->Flags & RIGID_BODY_STATIC)
    {
        Body->Mass = 0.0f;
        Body->InverseMass = 0.0f;
        Body->InertiaTensor = V3(0, 0, 0);
        Body->InverseInertiaTensor = V3(0, 0, 0);
        return;
    }
    
    f32 Density = Body->Material.Density;
    f32 Volume = 0.0f;
    v3 Inertia = V3(0, 0, 0);
    
    switch (Body->Shape.Type)
    {
        case SHAPE_SPHERE:
        {
            f32 Radius = Body->Shape.Sphere.Radius;
            Volume = (4.0f/3.0f) * Pi32 * Radius * Radius * Radius;
            
            // Sphere inertia: (2/5) * m * r²
            f32 I = (2.0f/5.0f) * Radius * Radius;
            Inertia = V3(I, I, I);
        } break;
        
        case SHAPE_BOX:
        {
            v3 Size = V3Mul(Body->Shape.Box.HalfExtents, 2.0f);
            Volume = Size.x * Size.y * Size.z;
            
            // Box inertia: (1/12) * m * (h² + w²) for each axis
            f32 Ixx = (1.0f/12.0f) * (Size.y*Size.y + Size.z*Size.z);
            f32 Iyy = (1.0f/12.0f) * (Size.x*Size.x + Size.z*Size.z);
            f32 Izz = (1.0f/12.0f) * (Size.x*Size.x + Size.y*Size.y);
            Inertia = V3(Ixx, Iyy, Izz);
        } break;
        
        case SHAPE_CAPSULE:
        {
            f32 Radius = Body->Shape.Capsule.Radius;
            f32 Height = Body->Shape.Capsule.Height;
            
            // Capsule = cylinder + 2 hemispheres
            f32 CylVolume = Pi32 * Radius * Radius * Height;
            f32 SphVolume = (4.0f/3.0f) * Pi32 * Radius * Radius * Radius;
            Volume = CylVolume + SphVolume;
            
            // Simplified inertia approximation
            f32 I = 0.4f * Radius * Radius + 0.25f * Height * Height;
            Inertia = V3(I, I, 0.5f * Radius * Radius);
        } break;
        
        default:
        {
            // Default to unit sphere
            Volume = (4.0f/3.0f) * Pi32;
            Inertia = V3(0.4f, 0.4f, 0.4f);
        } break;
    }
    
    Body->Mass = Density * Volume;
    Body->InverseMass = (Body->Mass > 0.0f) ? 1.0f / Body->Mass : 0.0f;
    
    // Scale inertia by mass
    Body->InertiaTensor = V3Mul(Inertia, Body->Mass);
    
    // Inverse inertia tensor (diagonal)
    Body->InverseInertiaTensor.x = (Body->InertiaTensor.x > 0.0f) ? 1.0f / Body->InertiaTensor.x : 0.0f;
    Body->InverseInertiaTensor.y = (Body->InertiaTensor.y > 0.0f) ? 1.0f / Body->InertiaTensor.y : 0.0f;
    Body->InverseInertiaTensor.z = (Body->InertiaTensor.z > 0.0f) ? 1.0f / Body->InertiaTensor.z : 0.0f;
}

// ========================================================================
// AABB CALCULATION
// ========================================================================

void 
PhysicsUpdateAABB(rigid_body *Body)
{
    Assert(Body);
    
    v3 Center = Body->Position;
    v3 HalfExtent = V3(0, 0, 0);
    
    switch (Body->Shape.Type)
    {
        case SHAPE_SPHERE:
        {
            f32 Radius = Body->Shape.Sphere.Radius;
            HalfExtent = V3(Radius, Radius, Radius);
        } break;
        
        case SHAPE_BOX:
        {
            // Rotate box extents to world space
            v3 LocalExtent = Body->Shape.Box.HalfExtents;
            
            // Transform each extent axis by rotation matrix
            m4x4 Transform = quat_to_m4x4(Body->Orientation);
            
            // Calculate maximum extent in each world axis
            f32 XX = fabsf(Transform.m[0][0] * LocalExtent.x);
            f32 XY = fabsf(Transform.m[0][1] * LocalExtent.y);
            f32 XZ = fabsf(Transform.m[0][2] * LocalExtent.z);
            
            f32 YX = fabsf(Transform.m[1][0] * LocalExtent.x);
            f32 YY = fabsf(Transform.m[1][1] * LocalExtent.y);
            f32 YZ = fabsf(Transform.m[1][2] * LocalExtent.z);
            
            f32 ZX = fabsf(Transform.m[2][0] * LocalExtent.x);
            f32 ZY = fabsf(Transform.m[2][1] * LocalExtent.y);
            f32 ZZ = fabsf(Transform.m[2][2] * LocalExtent.z);
            
            HalfExtent.x = XX + XY + XZ;
            HalfExtent.y = YX + YY + YZ;
            HalfExtent.z = ZX + ZY + ZZ;
        } break;
        
        case SHAPE_CAPSULE:
        {
            // Capsule AABB depends on orientation
            f32 Radius = Body->Shape.Capsule.Radius;
            f32 HalfHeight = Body->Shape.Capsule.Height * 0.5f;
            
            // Transform capsule axis (assume Y-axis)
            v3 Axis = quat_rotate_v3(Body->Orientation, V3(0, 1, 0));
            v3 AbsAxis = V3(fabsf(Axis.x), fabsf(Axis.y), fabsf(Axis.z));
            
            HalfExtent = V3Add(V3(Radius, Radius, Radius), V3Mul(AbsAxis, HalfHeight));
        } break;
        
        case SHAPE_CONVEX_HULL:
        {
            // Calculate AABB of transformed vertices
            if (Body->Shape.ConvexHull.VertexCount > 0)
            {
                m4x4 Transform = quat_to_m4x4(Body->Orientation);
                
                v3 Min = V3(1e30f, 1e30f, 1e30f);
                v3 Max = V3(-1e30f, -1e30f, -1e30f);
                
                for (u32 i = 0; i < Body->Shape.ConvexHull.VertexCount; ++i)
                {
                    v3 WorldVert = V3Add(m4x4_mul_v3_point(Transform, Body->Shape.ConvexHull.Vertices[i]), Center);
                    
                    Min.x = Minimum(Min.x, WorldVert.x);
                    Min.y = Minimum(Min.y, WorldVert.y);
                    Min.z = Minimum(Min.z, WorldVert.z);
                    
                    Max.x = Maximum(Max.x, WorldVert.x);
                    Max.y = Maximum(Max.y, WorldVert.y);
                    Max.z = Maximum(Max.z, WorldVert.z);
                }
                
                HalfExtent = V3Mul(V3Sub(Max, Min), 0.5f);
                Center = V3Mul(V3Add(Min, Max), 0.5f);
            }
            else
            {
                HalfExtent = V3(0.1f, 0.1f, 0.1f);  // Fallback
            }
        } break;
        
        case SHAPE_PLANE:
        {
            // Infinite plane - use large AABB
            HalfExtent = V3(1e6f, 1e6f, 1e6f);
        } break;
        
        case SHAPE_COUNT:
        default:
        {
            HalfExtent = V3(0.1f, 0.1f, 0.1f);
        } break;
    }
    
    Body->AABBMin = V3Sub(Center, HalfExtent);
    Body->AABBMax = V3Add(Center, HalfExtent);
}

// ========================================================================
// BODY MANAGEMENT
// ========================================================================

u32 
PhysicsCreateBody(physics_world *World, v3 Position, quat Orientation)
{
    Assert(World);
    Assert(World->BodyCount < World->MaxBodies);
    
    u32 BodyID = World->BodyCount++;
    rigid_body *Body = &World->Bodies[BodyID];
    
    // Initialize body
    *Body = (rigid_body){0};
    Body->Position = Position;
    Body->Orientation = quat_normalize(Orientation);
    Body->LinearVelocity = V3(0, 0, 0);
    Body->AngularVelocity = V3(0, 0, 0);
    Body->Force = V3(0, 0, 0);
    Body->Torque = V3(0, 0, 0);
    
    // Default to dynamic body
    Body->Flags = RIGID_BODY_ACTIVE;
    
    // Default material
    Body->Material = PhysicsCreateMaterial(PHYSICS_DEFAULT_DENSITY, 
                                          PHYSICS_DEFAULT_RESTITUTION, 
                                          PHYSICS_DEFAULT_FRICTION);
    
    // Default shape (unit sphere)
    Body->Shape = PhysicsCreateSphere(0.5f);
    
    // Calculate mass and AABB
    PhysicsCalculateMassProperties(Body);
    PhysicsUpdateAABB(Body);
    
    // Sleep parameters
    Body->SleepTimer = 0.0f;
    Body->MotionThreshold = 0.1f;
    
    return BodyID;
}

void 
PhysicsDestroyBody(physics_world *World, u32 BodyID)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    
    // Simple approach: mark as destroyed and compact later
    // For now, just set to static with zero mass
    rigid_body *Body = &World->Bodies[BodyID];
    Body->Flags |= RIGID_BODY_STATIC;
    Body->Mass = 0.0f;
    Body->InverseMass = 0.0f;
}

void 
PhysicsSetBodyShape(physics_world *World, u32 BodyID, collision_shape *Shape)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    Assert(Shape);
    
    rigid_body *Body = &World->Bodies[BodyID];
    Body->Shape = *Shape;
    
    // Recalculate mass properties and AABB
    PhysicsCalculateMassProperties(Body);
    PhysicsUpdateAABB(Body);
}

void 
PhysicsSetBodyMaterial(physics_world *World, u32 BodyID, material *Material)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    Assert(Material);
    
    rigid_body *Body = &World->Bodies[BodyID];
    Body->Material = *Material;
    
    // Recalculate mass properties
    PhysicsCalculateMassProperties(Body);
}

void 
PhysicsSetBodyTransform(physics_world *World, u32 BodyID, v3 Position, quat Orientation)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    
    rigid_body *Body = &World->Bodies[BodyID];
    Body->Position = Position;
    Body->Orientation = quat_normalize(Orientation);
    
    // Update AABB
    PhysicsUpdateAABB(Body);
    
    // Wake up body if it was sleeping
    Body->Flags |= RIGID_BODY_ACTIVE;
    Body->Flags &= ~RIGID_BODY_SLEEPING;
    Body->SleepTimer = 0.0f;
}

void 
PhysicsSetBodyVelocity(physics_world *World, u32 BodyID, v3 Linear, v3 Angular)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    
    rigid_body *Body = &World->Bodies[BodyID];
    Body->LinearVelocity = Linear;
    Body->AngularVelocity = Angular;
    
    // Wake up body
    Body->Flags |= RIGID_BODY_ACTIVE;
    Body->Flags &= ~RIGID_BODY_SLEEPING;
    Body->SleepTimer = 0.0f;
}

rigid_body* 
PhysicsGetBody(physics_world *World, u32 BodyID)
{
    Assert(World);
    if (BodyID < World->BodyCount)
    {
        return &World->Bodies[BodyID];
    }
    return 0;
}

b32 
PhysicsIsBodyStatic(physics_world *World, u32 BodyID)
{
    rigid_body *Body = PhysicsGetBody(World, BodyID);
    return Body ? (Body->Flags & RIGID_BODY_STATIC) != 0 : 0;
}

b32 
PhysicsIsBodySleeping(physics_world *World, u32 BodyID)
{
    rigid_body *Body = PhysicsGetBody(World, BodyID);
    return Body ? (Body->Flags & RIGID_BODY_SLEEPING) != 0 : 0;
}

// ========================================================================
// FORCE APPLICATION
// ========================================================================

void 
PhysicsApplyForce(physics_world *World, u32 BodyID, v3 Force, v3 Point)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    
    rigid_body *Body = &World->Bodies[BodyID];
    
    if (Body->InverseMass == 0.0f) return;  // Static body
    
    // Accumulate linear force
    Body->Force = V3Add(Body->Force, Force);
    
    // Calculate torque: τ = r × F
    v3 RelativePoint = V3Sub(Point, Body->Position);
    v3 Torque = V3Cross(RelativePoint, Force);
    Body->Torque = V3Add(Body->Torque, Torque);
    
    // Wake up body
    Body->Flags |= RIGID_BODY_ACTIVE;
    Body->Flags &= ~RIGID_BODY_SLEEPING;
    Body->SleepTimer = 0.0f;
}

void 
PhysicsApplyImpulse(physics_world *World, u32 BodyID, v3 Impulse, v3 Point)
{
    Assert(World);
    Assert(BodyID < World->BodyCount);
    
    rigid_body *Body = &World->Bodies[BodyID];
    
    if (Body->InverseMass == 0.0f) return;  // Static body
    
    // Apply linear impulse: Δv = J / m
    Body->LinearVelocity = V3Add(Body->LinearVelocity, V3Mul(Impulse, Body->InverseMass));
    
    // Apply angular impulse: Δω = I⁻¹ * (r × J)
    v3 RelativePoint = V3Sub(Point, Body->Position);
    v3 AngularImpulse = V3Cross(RelativePoint, Impulse);
    
    v3 DeltaAngularVel;
    DeltaAngularVel.x = AngularImpulse.x * Body->InverseInertiaTensor.x;
    DeltaAngularVel.y = AngularImpulse.y * Body->InverseInertiaTensor.y;
    DeltaAngularVel.z = AngularImpulse.z * Body->InverseInertiaTensor.z;
    
    Body->AngularVelocity = V3Add(Body->AngularVelocity, DeltaAngularVel);
    
    // Wake up body
    Body->Flags |= RIGID_BODY_ACTIVE;
    Body->Flags &= ~RIGID_BODY_SLEEPING;
    Body->SleepTimer = 0.0f;
}

void 
PhysicsSetGravity(physics_world *World, v3 Gravity)
{
    Assert(World);
    World->Gravity = Gravity;
}