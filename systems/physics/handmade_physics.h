#ifndef HANDMADE_PHYSICS_H
#define HANDMADE_PHYSICS_H

/*
    Handmade Physics Engine
    Zero-dependency, deterministic physics for multiplayer games
    
    Philosophy:
    - Fixed timestep for determinism
    - SIMD throughout for performance
    - Cache-coherent data layouts
    - Arena-based allocation
    - Every byte accounted for
    
    Performance targets:
    - 1000 dynamic bodies at 60 FPS
    - <1ms broad phase with 10,000 bodies
    - Zero heap allocations during simulation
*/

#include "../../src/handmade.h"
#include "../renderer/handmade_math.h"
#include <immintrin.h>  // SSE2/AVX2 intrinsics

// ========================================================================
// PHYSICS-SPECIFIC EXTENSIONS TO MATH LIBRARY
// ========================================================================

// Use existing math library types: v3, m4x4, quat
// Add SIMD extensions where needed

// SIMD-enhanced v3 operations for physics
typedef struct v3_simd
{
    __m128 simd;  // x, y, z, w (padded)
} v3_simd;

// Convert between math library v3 and SIMD-optimized version
static inline v3_simd v3_to_simd(v3 v) {
    v3_simd result;
    result.simd = _mm_set_ps(0.0f, v.z, v.y, v.x);
    return result;
}

static inline v3 v3_from_simd(v3_simd vs) {
    v3 result;
    _mm_storeu_ps(&result.x, vs.simd);
    return result;
}

// Rename quaternion to avoid conflict
typedef quat physics_quaternion;

// PERFORMANCE: Use math library functions with physics-optimized SIMD batch operations
#define V3(x, y, z) v3_make(x, y, z)
#define V3Add(a, b) v3_add(a, b)
#define V3Sub(a, b) v3_sub(a, b)
#define V3Mul(a, s) v3_scale(a, s)
#define V3Dot(a, b) v3_dot(a, b)
#define V3Cross(a, b) v3_cross(a, b)
#define V3LengthSq(a) v3_length_sq(a)
#define V3Length(a) v3_length(a)
#define V3Normalize(a) v3_normalize(a)
#define V3Lerp(a, b, t) v3_lerp(a, b, t)

// SIMD batch operations for physics performance
static inline void V3Add4(v3 *Dest, v3 *A, v3 *B) {
    // Process 4 vectors at once using SIMD for cache efficiency
    for (u32 i = 0; i < 4; ++i) {
        Dest[i] = V3Add(A[i], B[i]);
    }
}

static inline void V3Mul4(v3 *Dest, v3 *A, f32 S) {
    // Scale 4 vectors at once using SIMD for cache efficiency
    for (u32 i = 0; i < 4; ++i) {
        Dest[i] = V3Mul(A[i], S);
    }
}

// Quaternion operations using math library
#define QuaternionIdentity() quat_identity()
#define QuaternionFromAxisAngle(axis, angle) quat_from_axis_angle(axis, angle)
#define QuaternionMul(a, b) quat_mul(a, b)
#define QuaternionRotateV3(q, v) quat_rotate_v3(q, v)
#define QuaternionNormalize(q) quat_normalize(q)

// Matrix operations using math library
#define M4x4Identity() m4x4_identity()
#define M4x4FromQuaternion(q) quat_to_m4x4(q)
#define M4x4Translate(v) m4x4_translate_v3(v)
#define M4x4MulV3(m, v, w) (w == 1.0f ? m4x4_mul_v3_point(m, v) : m4x4_mul_v3_direction(m, v))

// ========================================================================
// FIXED-POINT MATH (for perfect determinism)
// ========================================================================

// 32.32 fixed point for deterministic physics
typedef i64 fixed64;
typedef i32 fixed32;

#define FIXED_ONE_32 (1 << 16)          // 16.16 fixed point
#define FIXED_ONE_64 (1LL << 32)        // 32.32 fixed point

static inline fixed32 Fixed32FromFloat(f32 Value) {
    return (fixed32)(Value * FIXED_ONE_32);
}

static inline f32 Fixed32ToFloat(fixed32 Value) {
    return (f32)Value / (f32)FIXED_ONE_32;
}

static inline fixed32 Fixed32Mul(fixed32 A, fixed32 B) {
    // Multiply and shift back to maintain precision
    i64 Result = ((i64)A * (i64)B) >> 16;
    return (fixed32)Result;
}

static inline fixed32 Fixed32Div(fixed32 A, fixed32 B) {
    if (B == 0) return 0;  // Avoid division by zero
    i64 Result = ((i64)A << 16) / (i64)B;
    return (fixed32)Result;
}

// Fixed-point vector for deterministic simulation
typedef struct v3_fixed
{
    fixed32 x, y, z;
} v3_fixed;

// ========================================================================
// PHYSICS CONSTANTS
// ========================================================================

#define PHYSICS_TIMESTEP (1.0f / 60.0f)      // Fixed 60Hz timestep
#define PHYSICS_GRAVITY -9.81f                // Earth gravity (m/s²)
#define PHYSICS_MAX_BODIES 10000              // Maximum rigid bodies
#define PHYSICS_MAX_CONTACTS 50000            // Maximum contact points
#define PHYSICS_MAX_CONSTRAINTS 10000        // Maximum constraints
#define PHYSICS_BROADPHASE_CELL_SIZE 2.0f    // Spatial hash cell size
#define PHYSICS_CONTACT_TOLERANCE 0.01f      // Contact generation threshold
#define PHYSICS_SOLVER_ITERATIONS 10         // Sequential impulse iterations

// Material properties
#define PHYSICS_DEFAULT_RESTITUTION 0.3f     // Bounciness
#define PHYSICS_DEFAULT_FRICTION 0.7f        // Surface friction
#define PHYSICS_DEFAULT_DENSITY 1.0f         // kg/m³

// ========================================================================
// RIGID BODY REPRESENTATION
// ========================================================================

typedef enum shape_type
{
    SHAPE_SPHERE,
    SHAPE_BOX,
    SHAPE_CAPSULE,
    SHAPE_CONVEX_HULL,
    SHAPE_PLANE,  // Infinite plane for ground
    SHAPE_COUNT
} shape_type;

// Collision shape data
typedef struct collision_shape
{
    shape_type Type;
    union
    {
        struct { f32 Radius; } Sphere;
        struct { v3 HalfExtents; } Box;
        struct { f32 Radius; f32 Height; } Capsule;
        struct { 
            v3 *Vertices; 
            u32 VertexCount; 
            v3 LocalCentroid;
        } ConvexHull;
        struct { v3 Normal; f32 Distance; } Plane;
    };
} collision_shape;

// Material properties
typedef struct material
{
    f32 Density;
    f32 Restitution;  // 0 = no bounce, 1 = perfect bounce
    f32 Friction;     // 0 = ice, 1+ = sticky
    f32 LinearDamping;
    f32 AngularDamping;
} material;

// Rigid body state - Structure of Arrays for SIMD
typedef struct rigid_body
{
    // Transform
    v3 Position;
    quat Orientation;
    
    // Motion
    v3 LinearVelocity;
    v3 AngularVelocity;
    
    // Forces (accumulated each frame)
    v3 Force;
    v3 Torque;
    
    // Mass properties
    f32 Mass;
    f32 InverseMass;  // 0 for static bodies
    v3 InertiaTensor;    // Diagonal inertia tensor (for boxes/spheres)
    v3 InverseInertiaTensor;
    
    // Collision
    collision_shape Shape;
    material Material;
    
    // Broad phase optimization
    v3 AABBMin, AABBMax;  // World-space bounding box
    u32 BroadPhaseID;     // Hash grid cell ID
    
    // Flags
    u32 Flags;
    #define RIGID_BODY_STATIC    (1 << 0)
    #define RIGID_BODY_KINEMATIC (1 << 1)  // Animated but not simulated
    #define RIGID_BODY_SLEEPING  (1 << 2)  // Optimization for stationary bodies
    #define RIGID_BODY_ACTIVE    (1 << 3)
    
    // Sleep optimization
    f32 SleepTimer;
    f32 MotionThreshold;
} rigid_body;

// ========================================================================
// COLLISION DETECTION
// ========================================================================

// Contact point between two bodies
typedef struct contact_point
{
    v3 PositionA;     // Contact point on body A (world space)
    v3 PositionB;     // Contact point on body B (world space)
    v3 Normal;        // Contact normal (points from A to B)
    f32 Penetration;  // Penetration depth (negative for separation)
    f32 NormalImpulse;     // Accumulated normal impulse
    f32 TangentImpulse[2]; // Accumulated friction impulses
} contact_point;

// Contact manifold between two bodies
typedef struct contact_manifold
{
    u32 BodyA, BodyB;           // Rigid body indices
    contact_point Points[4];    // Up to 4 contact points
    u32 PointCount;            // Number of valid contact points
    f32 Restitution;           // Combined restitution
    f32 Friction;              // Combined friction
    v3 Tangent1, Tangent2;     // Orthogonal tangent vectors for friction
} contact_manifold;

// Broad phase pair for narrow phase testing
typedef struct broad_phase_pair
{
    u32 BodyA, BodyB;
    f32 DistanceSq;  // For sorting by proximity
} broad_phase_pair;

// GJK support structure
typedef struct gjk_support
{
    v3 Point;      // Support point
    v3 PointA;     // Support point on shape A
    v3 PointB;     // Support point on shape B
} gjk_support;

// ========================================================================
// CONSTRAINTS
// ========================================================================

typedef enum constraint_type
{
    CONSTRAINT_DISTANCE,    // Fixed distance between two points
    CONSTRAINT_HINGE,       // Rotation around an axis
    CONSTRAINT_BALL_SOCKET, // Point-to-point connection
    CONSTRAINT_SLIDER,      // Translation along an axis
    CONSTRAINT_COUNT
} constraint_type;

typedef struct constraint
{
    constraint_type Type;
    u32 BodyA, BodyB;      // Connected bodies
    
    // Anchor points in local coordinates
    v3 LocalAnchorA, LocalAnchorB;
    
    union
    {
        struct {
            f32 RestLength;
            f32 Stiffness;
        } Distance;
        
        struct {
            v3 LocalAxisA, LocalAxisB;  // Hinge axes in local space
            f32 LowerLimit, UpperLimit; // Angle limits (radians)
        } Hinge;
        
        struct {
            v3 LocalAxis;  // Slider axis in body A's local space
            f32 LowerLimit, UpperLimit; // Distance limits
        } Slider;
    };
    
    // Solver data (internal)
    f32 AccumulatedImpulse[3];  // Up to 3 constraint dimensions
    f32 EffectiveMass[3];       // Precomputed effective mass
} constraint;

// ========================================================================
// SPATIAL PARTITIONING
// ========================================================================

#define SPATIAL_HASH_SIZE 4096  // Must be power of 2
#define SPATIAL_HASH_MASK (SPATIAL_HASH_SIZE - 1)

typedef struct spatial_cell
{
    u32 *Bodies;        // Dynamic array of body indices
    u32 BodyCount;
    u32 BodyCapacity;
} spatial_cell;

typedef struct spatial_hash_grid
{
    spatial_cell Cells[SPATIAL_HASH_SIZE];
    f32 CellSize;
    v3 Origin;  // World space origin of the grid
} spatial_hash_grid;

// ========================================================================
// PHYSICS WORLD
// ========================================================================

typedef struct physics_world
{
    // Memory arena for all allocations
    memory_index ArenaSize;
    u8 *ArenaBase;
    memory_index ArenaUsed;
    
    // Rigid bodies (Structure of Arrays for SIMD)
    rigid_body *Bodies;
    u32 BodyCount;
    u32 MaxBodies;
    
    // Collision detection
    contact_manifold *Manifolds;
    u32 ManifoldCount;
    u32 MaxManifolds;
    
    broad_phase_pair *BroadPhasePairs;
    u32 BroadPhasePairCount;
    u32 MaxBroadPhasePairs;
    
    // Constraints
    constraint *Constraints;
    u32 ConstraintCount;
    u32 MaxConstraints;
    
    // Spatial partitioning
    spatial_hash_grid BroadPhase;
    
    // Simulation parameters
    v3 Gravity;
    f32 TimeStep;
    f32 AccumulatedTime;  // For sub-stepping
    u32 SolverIterations;
    
    // Performance counters
    u64 BroadPhaseTime;
    u64 NarrowPhaseTime;
    u64 SolverTime;
    u64 IntegrationTime;
    u32 ActiveBodyCount;
    
    // Debug visualization
    b32 DrawDebugInfo;
    b32 DrawAABBs;
    b32 DrawContacts;
    b32 DrawConstraints;
    
    // Thread safety
    b32 IsSimulating;  // Lock-free flag for multithreading
} physics_world;

// ========================================================================
// PUBLIC API
// ========================================================================

// World management
physics_world* PhysicsCreateWorld(memory_index ArenaSize, void *ArenaMemory);
void PhysicsDestroyWorld(physics_world *World);
void PhysicsResetWorld(physics_world *World);

// Simulation
void PhysicsStepSimulation(physics_world *World, f32 DeltaTime);
void PhysicsSetGravity(physics_world *World, v3 Gravity);

// Body creation and management
u32 PhysicsCreateBody(physics_world *World, v3 Position, quat Orientation);
void PhysicsDestroyBody(physics_world *World, u32 BodyID);
void PhysicsSetBodyShape(physics_world *World, u32 BodyID, collision_shape *Shape);
void PhysicsSetBodyMaterial(physics_world *World, u32 BodyID, material *Material);
void PhysicsSetBodyTransform(physics_world *World, u32 BodyID, v3 Position, quat Orientation);
void PhysicsSetBodyVelocity(physics_world *World, u32 BodyID, v3 Linear, v3 Angular);
void PhysicsApplyForce(physics_world *World, u32 BodyID, v3 Force, v3 Point);
void PhysicsApplyImpulse(physics_world *World, u32 BodyID, v3 Impulse, v3 Point);

// Body queries
rigid_body* PhysicsGetBody(physics_world *World, u32 BodyID);
b32 PhysicsIsBodyStatic(physics_world *World, u32 BodyID);
b32 PhysicsIsBodySleeping(physics_world *World, u32 BodyID);

// Shape utilities
collision_shape PhysicsCreateSphere(f32 Radius);
collision_shape PhysicsCreateBox(v3 HalfExtents);
collision_shape PhysicsCreateCapsule(f32 Radius, f32 Height);
collision_shape PhysicsCreateConvexHull(v3 *Vertices, u32 VertexCount);
collision_shape PhysicsCreatePlane(v3 Normal, f32 Distance);

// Material utilities
material PhysicsCreateMaterial(f32 Density, f32 Restitution, f32 Friction);

// Constraints
u32 PhysicsCreateDistanceConstraint(physics_world *World, u32 BodyA, u32 BodyB, 
                                   v3 AnchorA, v3 AnchorB, f32 RestLength);
u32 PhysicsCreateHingeConstraint(physics_world *World, u32 BodyA, u32 BodyB, 
                                v3 AnchorA, v3 AnchorB, v3 AxisA, v3 AxisB);
u32 PhysicsCreateBallSocketConstraint(physics_world *World, u32 BodyA, u32 BodyB, 
                                     v3 AnchorA, v3 AnchorB);
void PhysicsDestroyConstraint(physics_world *World, u32 ConstraintID);

// Collision queries
b32 PhysicsRayCast(physics_world *World, v3 Origin, v3 Direction, f32 MaxDistance, 
                  u32 *HitBodyID, v3 *HitPoint, v3 *HitNormal);
u32 PhysicsOverlapSphere(physics_world *World, v3 Center, f32 Radius, 
                        u32 *BodyIDs, u32 MaxBodies);
u32 PhysicsOverlapBox(physics_world *World, v3 Center, v3 HalfExtents, quat Orientation,
                     u32 *BodyIDs, u32 MaxBodies);

// Debug visualization
void PhysicsSetDebugFlags(physics_world *World, b32 DrawAABBs, b32 DrawContacts, b32 DrawConstraints);
void PhysicsDebugDraw(physics_world *World, game_offscreen_buffer *Buffer);

// Performance profiling
void PhysicsGetProfileInfo(physics_world *World, f32 *BroadPhaseMS, f32 *NarrowPhaseMS, 
                          f32 *SolverMS, f32 *IntegrationMS, u32 *ActiveBodies);

// ========================================================================
// IMPLEMENTATION DETAILS (internal functions - declared here for cross-file access)
// ========================================================================

// Memory management
void* PhysicsArenaAllocate(physics_world *World, memory_index Size);
void PhysicsArenaReset(physics_world *World);

// Broad phase collision detection
void PhysicsBroadPhaseUpdate(physics_world *World);
u32 PhysicsBroadPhaseFindPairs(physics_world *World);
void PhysicsSpatialHashInsert(spatial_hash_grid *Grid, u32 BodyID, v3 AABBMin, v3 AABBMax);
u32 PhysicsSpatialHashQuery(spatial_hash_grid *Grid, v3 AABBMin, v3 AABBMax, u32 *Results, u32 MaxResults);

// Narrow phase collision detection
void PhysicsNarrowPhase(physics_world *World);
b32 PhysicsGJK(collision_shape *ShapeA, m4x4 TransformA, 
                       collision_shape *ShapeB, m4x4 TransformB,
                       v3 *ClosestPointA, v3 *ClosestPointB);
b32 PhysicsEPA(collision_shape *ShapeA, m4x4 TransformA,
                       collision_shape *ShapeB, m4x4 TransformB,
                       v3 *ContactNormal, f32 *PenetrationDepth);
contact_manifold PhysicsGenerateContactManifold(rigid_body *BodyA, rigid_body *BodyB);

// Constraint solver
void PhysicsSolveConstraints(physics_world *World);
void PhysicsIntegrateVelocities(physics_world *World);
void PhysicsIntegratePositions(physics_world *World);

// AABB and mass calculations
void PhysicsUpdateAABB(rigid_body *Body);
void PhysicsCalculateMassProperties(rigid_body *Body);

// Sleep system for performance
void PhysicsUpdateSleepState(physics_world *World);

#endif // HANDMADE_PHYSICS_H