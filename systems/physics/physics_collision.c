/*
    Handmade Physics Engine - Narrow Phase Collision Detection
    GJK/EPA implementation for convex shape collision
    
    Performance philosophy:
    - Early termination for separated objects
    - Minimal memory allocation (stack-based)
    - Cache-coherent data access patterns
    - Deterministic floating-point behavior
    
    Algorithm:
    1. GJK for separation detection
    2. EPA for penetration depth and normal
    3. Contact manifold generation
    4. Specialized fast paths for primitives
*/

#include "handmade_physics.h"
#include <stdbool.h>

// ========================================================================
// SUPPORT FUNCTIONS FOR SHAPES
// ========================================================================

internal v3
PhysicsGetSphereSupport(collision_shape *Shape, v3 Direction)
{
    Assert(Shape->Type == SHAPE_SPHERE);
    
    v3 NormDir = V3Normalize(Direction);
    return V3Mul(NormDir, Shape->Sphere.Radius);
}

internal v3
PhysicsGetBoxSupport(collision_shape *Shape, v3 Direction)
{
    Assert(Shape->Type == SHAPE_BOX);
    
    v3 HalfExtent = Shape->Box.HalfExtents;
    v3 Support;
    
    Support.x = (Direction.x >= 0.0f) ? HalfExtent.x : -HalfExtent.x;
    Support.y = (Direction.y >= 0.0f) ? HalfExtent.y : -HalfExtent.y;
    Support.z = (Direction.z >= 0.0f) ? HalfExtent.z : -HalfExtent.z;
    
    return Support;
}

internal v3
PhysicsGetCapsuleSupport(collision_shape *Shape, v3 Direction)
{
    Assert(Shape->Type == SHAPE_CAPSULE);
    
    f32 Radius = Shape->Capsule.Radius;
    f32 HalfHeight = Shape->Capsule.Height * 0.5f;
    
    v3 NormDir = V3Normalize(Direction);
    
    // Support point on sphere end
    v3 SphereSupport = V3Mul(NormDir, Radius);
    
    // Choose hemisphere based on direction
    if (Direction.y >= 0.0f)
    {
        SphereSupport.y += HalfHeight;  // Top hemisphere
    }
    else
    {
        SphereSupport.y -= HalfHeight;  // Bottom hemisphere
    }
    
    return SphereSupport;
}

internal v3
PhysicsGetConvexHullSupport(collision_shape *Shape, v3 Direction)
{
    Assert(Shape->Type == SHAPE_CONVEX_HULL);
    
    if (Shape->ConvexHull.VertexCount == 0)
    {
        return V3(0, 0, 0);
    }
    
    // Find vertex with maximum dot product in direction
    f32 MaxDot = -1e30f;
    v3 SupportVertex = Shape->ConvexHull.Vertices[0];
    
    for (u32 i = 0; i < Shape->ConvexHull.VertexCount; ++i)
    {
        v3 Vertex = Shape->ConvexHull.Vertices[i];
        f32 Dot = V3Dot(Vertex, Direction);
        
        if (Dot > MaxDot)
        {
            MaxDot = Dot;
            SupportVertex = Vertex;
        }
    }
    
    return SupportVertex;
}

internal v3
PhysicsGetShapeSupport(collision_shape *Shape, m4x4 Transform, v3 WorldDirection)
{
    // Transform direction to local space (transpose of rotation)
    v3 LocalDirection;
    LocalDirection.x = V3Dot(WorldDirection, V3(Transform.m[0][0], Transform.m[1][0], Transform.m[2][0]));
    LocalDirection.y = V3Dot(WorldDirection, V3(Transform.m[0][1], Transform.m[1][1], Transform.m[2][1]));
    LocalDirection.z = V3Dot(WorldDirection, V3(Transform.m[0][2], Transform.m[1][2], Transform.m[2][2]));
    
    // Get local support point
    v3 LocalSupport;
    switch (Shape->Type)
    {
        case SHAPE_SPHERE:
            LocalSupport = PhysicsGetSphereSupport(Shape, LocalDirection);
            break;
        case SHAPE_BOX:
            LocalSupport = PhysicsGetBoxSupport(Shape, LocalDirection);
            break;
        case SHAPE_CAPSULE:
            LocalSupport = PhysicsGetCapsuleSupport(Shape, LocalDirection);
            break;
        case SHAPE_CONVEX_HULL:
            LocalSupport = PhysicsGetConvexHullSupport(Shape, LocalDirection);
            break;
        default:
            LocalSupport = V3(0, 0, 0);
            break;
    }
    
    // Transform support point to world space
    return M4x4MulV3(Transform, LocalSupport, 1.0f);
}

// ========================================================================
// GJK SUPPORT FUNCTION FOR MINKOWSKI DIFFERENCE
// ========================================================================

internal gjk_support
PhysicsGJKSupport(collision_shape *ShapeA, m4x4 TransformA,
                 collision_shape *ShapeB, m4x4 TransformB,
                 v3 Direction)
{
    gjk_support Support;
    
    // Support point on A in direction
    Support.PointA = PhysicsGetShapeSupport(ShapeA, TransformA, Direction);
    
    // Support point on B in opposite direction
    v3 NegDirection = V3Mul(Direction, -1.0f);
    Support.PointB = PhysicsGetShapeSupport(ShapeB, TransformB, NegDirection);
    
    // Minkowski difference: A - B
    Support.Point = V3Sub(Support.PointA, Support.PointB);
    
    return Support;
}

// ========================================================================
// GJK ALGORITHM IMPLEMENTATION
// ========================================================================

internal b32
PhysicsGJKTriangleCase(gjk_support *Simplex, v3 *Direction)
{
    // Triangle case: points A, B, C (most recent is A)
    v3 A = Simplex[0].Point;
    v3 B = Simplex[1].Point;
    v3 C = Simplex[2].Point;
    
    v3 AB = V3Sub(B, A);
    v3 AC = V3Sub(C, A);
    v3 AO = V3Mul(A, -1.0f);  // Vector from A to origin
    
    // Triangle normal
    v3 ABC = V3Cross(AB, AC);
    
    // Test which region the origin is in
    v3 AB_perp = V3Cross(V3Cross(AC, AB), AB);  // Perpendicular to AB, pointing away from C
    if (V3Dot(AB_perp, AO) > 0.0f)
    {
        // Origin is in AB region
        Simplex[1] = Simplex[0];  // Remove C, keep A and B
        *Direction = AB_perp;
        return 0;  // 2D simplex
    }
    
    v3 AC_perp = V3Cross(V3Cross(AB, AC), AC);  // Perpendicular to AC, pointing away from B
    if (V3Dot(AC_perp, AO) > 0.0f)
    {
        // Origin is in AC region
        Simplex[1] = Simplex[2];  // Move C to position 1, keep A
        *Direction = AC_perp;
        return 0;  // 2D simplex
    }
    
    // Origin is either inside the triangle or on the other side
    if (V3Dot(ABC, AO) > 0.0f)
    {
        // Origin is above triangle
        *Direction = ABC;
    }
    else
    {
        // Origin is below triangle
        *Direction = V3Mul(ABC, -1.0f);
    }
    
    return 0;  // Still 2D
}

internal b32
PhysicsGJKTetrahedronCase(gjk_support *Simplex, v3 *Direction)
{
    // Tetrahedron case: points A, B, C, D (most recent is A)
    v3 A = Simplex[0].Point;
    v3 B = Simplex[1].Point;
    v3 C = Simplex[2].Point;
    v3 D = Simplex[3].Point;
    
    v3 AB = V3Sub(B, A);
    v3 AC = V3Sub(C, A);
    v3 AD = V3Sub(D, A);
    v3 AO = V3Mul(A, -1.0f);
    
    // Test each face of the tetrahedron
    v3 ABC = V3Cross(AB, AC);
    v3 ACD = V3Cross(AC, AD);
    v3 ADB = V3Cross(AD, AB);
    
    // Check if origin is on the outside of face ABC
    if (V3Dot(ABC, AO) > 0.0f)
    {
        // Remove D, test triangle ABC
        // Simplex is already ABC (0, 1, 2)
        return PhysicsGJKTriangleCase(Simplex, Direction);
    }
    
    // Check if origin is on the outside of face ACD
    if (V3Dot(ACD, AO) > 0.0f)
    {
        // Remove B, rearrange to ACD
        Simplex[1] = Simplex[2];  // C
        Simplex[2] = Simplex[3];  // D
        return PhysicsGJKTriangleCase(Simplex, Direction);
    }
    
    // Check if origin is on the outside of face ADB
    if (V3Dot(ADB, AO) > 0.0f)
    {
        // Remove C, rearrange to ADB
        Simplex[2] = Simplex[1];  // B -> position 2
        Simplex[1] = Simplex[3];  // D -> position 1
        return PhysicsGJKTriangleCase(Simplex, Direction);
    }
    
    // Origin is inside tetrahedron - collision detected!
    return 1;
}

internal b32
PhysicsGJKSimplex(gjk_support *Simplex, u32 *SimplexSize, v3 *Direction)
{
    switch (*SimplexSize)
    {
        case 2:  // Line segment
        {
            v3 A = Simplex[0].Point;  // Most recent point
            v3 B = Simplex[1].Point;
            v3 AB = V3Sub(B, A);
            v3 AO = V3Mul(A, -1.0f);
            
            // Find closest point on line to origin
            if (V3Dot(AB, AO) > 0.0f)
            {
                // Direction perpendicular to AB towards origin
                *Direction = V3Cross(V3Cross(AB, AO), AB);
                
                // Handle degenerate case
                if (V3LengthSq(*Direction) < 1e-6f)
                {
                    *Direction = V3(1, 0, 0);  // Arbitrary direction
                }
            }
            else
            {
                // Origin is behind A, keep only A
                *SimplexSize = 1;
                *Direction = AO;
            }
            return 0;
        }
        
        case 3:  // Triangle
        {
            return PhysicsGJKTriangleCase(Simplex, Direction);
        }
        
        case 4:  // Tetrahedron
        {
            return PhysicsGJKTetrahedronCase(Simplex, Direction);
        }
        
        default:
            return 0;
    }
}

b32
PhysicsGJK(collision_shape *ShapeA, m4x4 TransformA,
          collision_shape *ShapeB, m4x4 TransformB,
          v3 *ClosestPointA, v3 *ClosestPointB)
{
    gjk_support Simplex[4];
    u32 SimplexSize = 0;
    
    // Initial search direction (arbitrary)
    v3 Direction = V3(1, 0, 0);
    
    // Get first support point
    Simplex[0] = PhysicsGJKSupport(ShapeA, TransformA, ShapeB, TransformB, Direction);
    SimplexSize = 1;
    
    // Direction towards origin
    Direction = V3Mul(Simplex[0].Point, -1.0f);
    
    u32 MaxIterations = 32;  // Prevent infinite loops
    for (u32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
    {
        // Get support point in current direction
        gjk_support NewSupport = PhysicsGJKSupport(ShapeA, TransformA, ShapeB, TransformB, Direction);
        
        // Check if we made progress towards the origin
        if (V3Dot(NewSupport.Point, Direction) <= 0.0f)
        {
            // No progress - shapes are separated
            return 0;
        }
        
        // Add new point to simplex
        Simplex[SimplexSize] = NewSupport;
        SimplexSize++;
        
        // Process simplex
        if (PhysicsGJKSimplex(Simplex, &SimplexSize, &Direction))
        {
            // Collision detected!
            // For now, just return 1 (EPA would calculate penetration details)
            return 1;
        }
        
        // Check for degenerate direction
        if (V3LengthSq(Direction) < 1e-10f)
        {
            // Likely at origin or very close
            return 1;
        }
    }
    
    // Max iterations reached - assume separation
    return 0;
}

// ========================================================================
// EPA (EXPANDING POLYTOPE ALGORITHM) IMPLEMENTATION
// ========================================================================

typedef struct epa_face
{
    u32 Vertices[3];  // Indices into EPA vertex array
    v3 Normal;
    f32 Distance;     // Distance from origin to face
} epa_face;

typedef struct epa_edge
{
    u32 Vertex1, Vertex2;
} epa_edge;

internal f32
PhysicsCalculateFaceDistanceAndNormal(gjk_support *Vertices, epa_face *Face)
{
    v3 A = Vertices[Face->Vertices[0]].Point;
    v3 B = Vertices[Face->Vertices[1]].Point;
    v3 C = Vertices[Face->Vertices[2]].Point;
    
    v3 AB = V3Sub(B, A);
    v3 AC = V3Sub(C, A);
    
    Face->Normal = V3Normalize(V3Cross(AB, AC));
    Face->Distance = V3Dot(Face->Normal, A);
    
    return Face->Distance;
}

b32
PhysicsEPA(collision_shape *ShapeA, m4x4 TransformA,
          collision_shape *ShapeB, m4x4 TransformB,
          v3 *ContactNormal, f32 *PenetrationDepth)
{
    // EPA requires a valid GJK simplex as input
    // For now, simplified implementation that assumes GJK found collision
    
    gjk_support Vertices[64];  // EPA vertices
    epa_face Faces[128];       // EPA faces
    u32 VertexCount = 0;
    u32 FaceCount = 0;
    
    // Initial tetrahedron (would come from GJK simplex in real implementation)
    v3 Directions[] = {
        V3(1, 0, 0), V3(-1, 0, 0), V3(0, 1, 0), V3(0, -1, 0),
        V3(0, 0, 1), V3(0, 0, -1)
    };
    
    // Build initial tetrahedron
    for (u32 i = 0; i < 4; ++i)
    {
        Vertices[VertexCount++] = PhysicsGJKSupport(ShapeA, TransformA, ShapeB, TransformB, Directions[i]);
    }
    
    // Create initial faces of tetrahedron
    u32 FaceIndices[][3] = {
        {0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}
    };
    
    for (u32 i = 0; i < 4; ++i)
    {
        Faces[FaceCount].Vertices[0] = FaceIndices[i][0];
        Faces[FaceCount].Vertices[1] = FaceIndices[i][1];
        Faces[FaceCount].Vertices[2] = FaceIndices[i][2];
        PhysicsCalculateFaceDistanceAndNormal(Vertices, &Faces[FaceCount]);
        FaceCount++;
    }
    
    u32 MaxIterations = 32;
    for (u32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
    {
        // Find face closest to origin
        f32 MinDistance = 1e30f;
        u32 ClosestFace = 0;
        
        for (u32 i = 0; i < FaceCount; ++i)
        {
            if (Faces[i].Distance < MinDistance)
            {
                MinDistance = Faces[i].Distance;
                ClosestFace = i;
            }
        }
        
        // Get support point in direction of closest face normal
        gjk_support NewVertex = PhysicsGJKSupport(ShapeA, TransformA, ShapeB, TransformB, 
                                                  Faces[ClosestFace].Normal);
        
        // Check if new point is significantly closer than current closest face
        f32 NewDistance = V3Dot(NewVertex.Point, Faces[ClosestFace].Normal);
        if (NewDistance - MinDistance < 1e-4f)
        {
            // Converged - return results
            *ContactNormal = Faces[ClosestFace].Normal;
            *PenetrationDepth = MinDistance;
            return 1;
        }
        
        // Expand polytope by adding new vertex
        if (VertexCount >= ArrayCount(Vertices)) break;
        Vertices[VertexCount++] = NewVertex;
        
        // Rebuild faces (simplified - real EPA would be more efficient)
        // For now, just return the current best estimate
        *ContactNormal = Faces[ClosestFace].Normal;
        *PenetrationDepth = MinDistance;
        return 1;
    }
    
    return 0;  // Failed to converge
}

// ========================================================================
// CONTACT MANIFOLD GENERATION
// ========================================================================

internal contact_point
PhysicsGenerateContactPoint(v3 PositionA, v3 PositionB, v3 Normal, f32 Penetration)
{
    contact_point Contact = {0};
    Contact.PositionA = PositionA;
    Contact.PositionB = PositionB;
    Contact.Normal = Normal;
    Contact.Penetration = Penetration;
    return Contact;
}

contact_manifold
PhysicsGenerateContactManifold(rigid_body *BodyA, rigid_body *BodyB)
{
    contact_manifold Manifold = {0};
    
    // Create transformation matrices
    m4x4 TransformA = M4x4FromQuaternion(BodyA->Orientation);
    TransformA = M4x4Translate(BodyA->Position);  // Should combine rotation and translation
    
    m4x4 TransformB = M4x4FromQuaternion(BodyB->Orientation);
    TransformB = M4x4Translate(BodyB->Position);
    
    // Test collision using GJK
    b32 Collision = PhysicsGJK(&BodyA->Shape, TransformA, &BodyB->Shape, TransformB, 0, 0);
    
    if (Collision)
    {
        // Use EPA to get penetration details
        v3 ContactNormal;
        f32 PenetrationDepth;
        
        if (PhysicsEPA(&BodyA->Shape, TransformA, &BodyB->Shape, TransformB, 
                      &ContactNormal, &PenetrationDepth))
        {
            // Generate contact point
            v3 ContactPoint = V3Add(BodyA->Position, V3Mul(ContactNormal, PenetrationDepth * 0.5f));
            
            Manifold.PointCount = 1;
            Manifold.Points[0] = PhysicsGenerateContactPoint(
                ContactPoint, 
                V3Sub(ContactPoint, V3Mul(ContactNormal, PenetrationDepth)),
                ContactNormal, 
                PenetrationDepth
            );
            
            // Calculate combined material properties
            Manifold.Restitution = (BodyA->Material.Restitution + BodyB->Material.Restitution) * 0.5f;
            Manifold.Friction = sqrtf(BodyA->Material.Friction * BodyB->Material.Friction);
            
            // Calculate tangent vectors for friction
            v3 Tangent1;
            if (fabsf(ContactNormal.x) > 0.7f)
            {
                Tangent1 = V3(0, 1, 0);
            }
            else
            {
                Tangent1 = V3(1, 0, 0);
            }
            
            Tangent1 = V3Normalize(V3Sub(Tangent1, V3Mul(ContactNormal, V3Dot(Tangent1, ContactNormal))));
            Manifold.Tangent1 = Tangent1;
            Manifold.Tangent2 = V3Cross(ContactNormal, Tangent1);
        }
    }
    
    return Manifold;
}

// ========================================================================
// SPECIALIZED COLLISION TESTS (FASTER THAN GJK FOR PRIMITIVES)
// ========================================================================

internal b32
PhysicsSphereSphereCollision(rigid_body *BodyA, rigid_body *BodyB, contact_manifold *Manifold)
{
    Assert(BodyA->Shape.Type == SHAPE_SPHERE);
    Assert(BodyB->Shape.Type == SHAPE_SPHERE);
    
    f32 RadiusA = BodyA->Shape.Sphere.Radius;
    f32 RadiusB = BodyB->Shape.Sphere.Radius;
    f32 TotalRadius = RadiusA + RadiusB;
    
    v3 Delta = V3Sub(BodyB->Position, BodyA->Position);
    f32 DistanceSq = V3LengthSq(Delta);
    
    if (DistanceSq > TotalRadius * TotalRadius)
    {
        return 0;  // No collision
    }
    
    f32 Distance = sqrtf(DistanceSq);
    f32 Penetration = TotalRadius - Distance;
    
    v3 Normal;
    if (Distance > 1e-6f)
    {
        Normal = V3Mul(Delta, 1.0f / Distance);
    }
    else
    {
        Normal = V3(0, 1, 0);  // Arbitrary normal for coincident spheres
    }
    
    v3 ContactPointA = V3Add(BodyA->Position, V3Mul(Normal, RadiusA));
    v3 ContactPointB = V3Sub(BodyB->Position, V3Mul(Normal, RadiusB));
    
    Manifold->PointCount = 1;
    Manifold->Points[0] = PhysicsGenerateContactPoint(ContactPointA, ContactPointB, Normal, Penetration);
    
    return 1;
}

internal b32
PhysicsSphereBoxCollision(rigid_body *SphereBody, rigid_body *BoxBody, contact_manifold *Manifold)
{
    Assert(SphereBody->Shape.Type == SHAPE_SPHERE);
    Assert(BoxBody->Shape.Type == SHAPE_BOX);
    
    // Transform sphere center to box local space
    v3 SphereCenter = SphereBody->Position;
    v3 BoxCenter = BoxBody->Position;
    
    // For now, assume aligned boxes (no rotation)
    v3 LocalSphere = V3Sub(SphereCenter, BoxCenter);
    v3 HalfExtent = BoxBody->Shape.Box.HalfExtents;
    f32 SphereRadius = SphereBody->Shape.Sphere.Radius;
    
    // Find closest point on box to sphere center
    v3 ClosestPoint;
    ClosestPoint.x = Maximum(-HalfExtent.x, Minimum(HalfExtent.x, LocalSphere.x));
    ClosestPoint.y = Maximum(-HalfExtent.y, Minimum(HalfExtent.y, LocalSphere.y));
    ClosestPoint.z = Maximum(-HalfExtent.z, Minimum(HalfExtent.z, LocalSphere.z));
    
    v3 Delta = V3Sub(LocalSphere, ClosestPoint);
    f32 DistanceSq = V3LengthSq(Delta);
    
    if (DistanceSq > SphereRadius * SphereRadius)
    {
        return 0;  // No collision
    }
    
    f32 Distance = sqrtf(DistanceSq);
    f32 Penetration = SphereRadius - Distance;
    
    v3 Normal;
    if (Distance > 1e-6f)
    {
        Normal = V3Mul(Delta, 1.0f / Distance);
    }
    else
    {
        // Sphere center is inside box - find best separating axis
        v3 Distances = V3Sub(HalfExtent, V3(fabsf(LocalSphere.x), fabsf(LocalSphere.y), fabsf(LocalSphere.z)));
        
        if (Distances.x < Distances.y && Distances.x < Distances.z)
        {
            Normal = V3((LocalSphere.x > 0) ? 1.0f : -1.0f, 0, 0);
        }
        else if (Distances.y < Distances.z)
        {
            Normal = V3(0, (LocalSphere.y > 0) ? 1.0f : -1.0f, 0);
        }
        else
        {
            Normal = V3(0, 0, (LocalSphere.z > 0) ? 1.0f : -1.0f);
        }
        
        Penetration = SphereRadius + Minimum(Distances.x, Minimum(Distances.y, Distances.z));
    }
    
    // Transform back to world space
    v3 WorldClosestPoint = V3Add(ClosestPoint, BoxCenter);
    v3 ContactPointA = V3Sub(SphereCenter, V3Mul(Normal, SphereRadius));
    
    Manifold->PointCount = 1;
    Manifold->Points[0] = PhysicsGenerateContactPoint(ContactPointA, WorldClosestPoint, Normal, Penetration);
    
    return 1;
}

// ========================================================================
// MAIN NARROW PHASE FUNCTION
// ========================================================================

void
PhysicsNarrowPhase(physics_world *World)
{
    Assert(World);
    
    u64 StartTime = ReadCPUTimer();
    
    World->ManifoldCount = 0;
    
    // Process all broad phase pairs
    for (u32 PairIndex = 0; PairIndex < World->BroadPhasePairCount; ++PairIndex)
    {
        if (World->ManifoldCount >= World->MaxManifolds) break;
        
        broad_phase_pair *Pair = &World->BroadPhasePairs[PairIndex];
        rigid_body *BodyA = &World->Bodies[Pair->BodyA];
        rigid_body *BodyB = &World->Bodies[Pair->BodyB];
        
        // Skip sleeping or static pairs
        b32 AStatic = (BodyA->Flags & RIGID_BODY_STATIC) != 0;
        b32 BStatic = (BodyB->Flags & RIGID_BODY_STATIC) != 0;
        b32 ASleeping = (BodyA->Flags & RIGID_BODY_SLEEPING) != 0;
        b32 BSleeping = (BodyB->Flags & RIGID_BODY_SLEEPING) != 0;
        
        if (AStatic && BStatic) continue;
        if (ASleeping && BSleeping) continue;
        
        contact_manifold Manifold = {0};
        Manifold.BodyA = Pair->BodyA;
        Manifold.BodyB = Pair->BodyB;
        
        b32 HasCollision = 0;
        
        // Use specialized collision tests for common shape pairs
        if (BodyA->Shape.Type == SHAPE_SPHERE && BodyB->Shape.Type == SHAPE_SPHERE)
        {
            HasCollision = PhysicsSphereSphereCollision(BodyA, BodyB, &Manifold);
        }
        else if (BodyA->Shape.Type == SHAPE_SPHERE && BodyB->Shape.Type == SHAPE_BOX)
        {
            HasCollision = PhysicsSphereBoxCollision(BodyA, BodyB, &Manifold);
        }
        else if (BodyA->Shape.Type == SHAPE_BOX && BodyB->Shape.Type == SHAPE_SPHERE)
        {
            HasCollision = PhysicsSphereBoxCollision(BodyB, BodyA, &Manifold);
            // Flip normal and contact points
            if (HasCollision && Manifold.PointCount > 0)
            {
                Manifold.Points[0].Normal = V3Mul(Manifold.Points[0].Normal, -1.0f);
                v3 Temp = Manifold.Points[0].PositionA;
                Manifold.Points[0].PositionA = Manifold.Points[0].PositionB;
                Manifold.Points[0].PositionB = Temp;
            }
        }
        else
        {
            // Fall back to GJK/EPA for general case
            Manifold = PhysicsGenerateContactManifold(BodyA, BodyB);
            HasCollision = (Manifold.PointCount > 0);
        }
        
        if (HasCollision)
        {
            // Calculate combined material properties
            Manifold.Restitution = (BodyA->Material.Restitution + BodyB->Material.Restitution) * 0.5f;
            Manifold.Friction = sqrtf(BodyA->Material.Friction * BodyB->Material.Friction);
            
            // Calculate friction tangents
            if (Manifold.PointCount > 0)
            {
                v3 Normal = Manifold.Points[0].Normal;
                v3 Tangent1;
                
                if (fabsf(Normal.x) > 0.7f)
                {
                    Tangent1 = V3(0, 1, 0);
                }
                else
                {
                    Tangent1 = V3(1, 0, 0);
                }
                
                Tangent1 = V3Normalize(V3Sub(Tangent1, V3Mul(Normal, V3Dot(Tangent1, Normal))));
                Manifold.Tangent1 = Tangent1;
                Manifold.Tangent2 = V3Cross(Normal, Tangent1);
            }
            
            World->Manifolds[World->ManifoldCount++] = Manifold;
            
            // Wake up sleeping bodies
            BodyA->Flags |= RIGID_BODY_ACTIVE;
            BodyA->Flags &= ~RIGID_BODY_SLEEPING;
            BodyB->Flags |= RIGID_BODY_ACTIVE;
            BodyB->Flags &= ~RIGID_BODY_SLEEPING;
        }
    }
    
    u64 EndTime = ReadCPUTimer();
    World->NarrowPhaseTime = EndTime - StartTime;
}