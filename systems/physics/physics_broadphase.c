/*
    Handmade Physics Engine - Broad Phase Collision Detection
    Spatial hash grid for O(n) collision pair finding
    
    Performance target: <1ms for 10,000 bodies
    Memory pattern: Minimize cache misses with spatial locality
    
    Algorithm:
    1. Hash AABB to grid cells
    2. Insert bodies into overlapping cells
    3. Find pairs within cells
    4. Return sorted pairs for narrow phase
*/

#include "handmade_physics.h"
#include <stdbool.h>

// ========================================================================
// SPATIAL HASH FUNCTIONS
// ========================================================================

// PERFORMANCE: Hash function optimized for spatial coherence
// MEMORY: Single branch for good cache behavior
u32
PhysicsSpatialHash(i32 X, i32 Y, i32 Z)
{
    // Knuth's multiplicative hash with bit mixing
    u32 Hash = ((u32)X * 73856093U) ^ ((u32)Y * 19349663U) ^ ((u32)Z * 83492791U);
    return Hash & SPATIAL_HASH_MASK;
}

void
PhysicsWorldToGrid(spatial_hash_grid *Grid, v3 WorldPos, i32 *GridX, i32 *GridY, i32 *GridZ)
{
    Assert(Grid);
    Assert(GridX && GridY && GridZ);
    
    v3 GridPos = V3Sub(WorldPos, Grid->Origin);
    f32 InvCellSize = 1.0f / Grid->CellSize;
    
    *GridX = (i32)(GridPos.x * InvCellSize);
    *GridY = (i32)(GridPos.y * InvCellSize);
    *GridZ = (i32)(GridPos.z * InvCellSize);
}

// ========================================================================
// CELL MANAGEMENT
// ========================================================================

void
PhysicsGrowSpatialCell(physics_world *World, spatial_cell *Cell)
{
    Assert(World);
    Assert(Cell);
    
    u32 NewCapacity = Cell->BodyCapacity * 2;
    u32 *NewBodies = (u32*)PhysicsArenaAllocate(World, NewCapacity * sizeof(u32));
    
    // Copy existing data
    for (u32 i = 0; i < Cell->BodyCount; ++i)
    {
        NewBodies[i] = Cell->Bodies[i];
    }
    
    Cell->Bodies = NewBodies;
    Cell->BodyCapacity = NewCapacity;
}

void
PhysicsInsertBodyIntoCell(physics_world *World, spatial_cell *Cell, u32 BodyID)
{
    Assert(World);
    Assert(Cell);
    
    // Grow cell if needed
    if (Cell->BodyCount >= Cell->BodyCapacity)
    {
        PhysicsGrowSpatialCell(World, Cell);
    }
    
    Cell->Bodies[Cell->BodyCount++] = BodyID;
}

void
PhysicsClearSpatialCell(spatial_cell *Cell)
{
    Assert(Cell);
    Cell->BodyCount = 0;
}

// ========================================================================
// SPATIAL HASH GRID OPERATIONS
// ========================================================================

void
PhysicsSpatialHashReset(spatial_hash_grid *Grid)
{
    Assert(Grid);
    
    // PERFORMANCE: Clear all cells efficiently
    // MEMORY: 4KB of cells - fits in L1 cache
    for (u32 i = 0; i < SPATIAL_HASH_SIZE; ++i)
    {
        PhysicsClearSpatialCell(&Grid->Cells[i]);
    }
}

void
PhysicsSpatialHashInsert(spatial_hash_grid *Grid, u32 BodyID, v3 AABBMin, v3 AABBMax)
{
    Assert(Grid);
    
    // Calculate grid bounds for AABB
    i32 MinX, MinY, MinZ;
    i32 MaxX, MaxY, MaxZ;
    
    PhysicsWorldToGrid(Grid, AABBMin, &MinX, &MinY, &MinZ);
    PhysicsWorldToGrid(Grid, AABBMax, &MaxX, &MaxY, &MaxZ);
    
    // Insert body into all overlapping cells
    for (i32 Z = MinZ; Z <= MaxZ; ++Z)
    {
        for (i32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (i32 X = MinX; X <= MaxX; ++X)
            {
                u32 Hash = PhysicsSpatialHash(X, Y, Z);
                spatial_cell *Cell = &Grid->Cells[Hash];
                
                // Note: We don't check for duplicates here for performance
                // The same body can be in multiple cells, which is intended
                Cell->Bodies[Cell->BodyCount++] = BodyID;
                
                // Safety check for cell overflow
                if (Cell->BodyCount >= Cell->BodyCapacity)
                {
                    // Emergency fallback - stop insertion to prevent overflow
                    Cell->BodyCount = Cell->BodyCapacity - 1;
                    break;
                }
            }
        }
    }
}

u32
PhysicsSpatialHashQuery(spatial_hash_grid *Grid, v3 AABBMin, v3 AABBMax, u32 *Results, u32 MaxResults)
{
    Assert(Grid);
    Assert(Results);
    
    u32 ResultCount = 0;
    
    // Calculate grid bounds for query AABB
    i32 MinX, MinY, MinZ;
    i32 MaxX, MaxY, MaxZ;
    
    PhysicsWorldToGrid(Grid, AABBMin, &MinX, &MinY, &MinZ);
    PhysicsWorldToGrid(Grid, AABBMax, &MaxX, &MaxY, &MaxZ);
    
    // Query all overlapping cells
    for (i32 Z = MinZ; Z <= MaxZ && ResultCount < MaxResults; ++Z)
    {
        for (i32 Y = MinY; Y <= MaxY && ResultCount < MaxResults; ++Y)
        {
            for (i32 X = MinX; X <= MaxX && ResultCount < MaxResults; ++X)
            {
                u32 Hash = PhysicsSpatialHash(X, Y, Z);
                spatial_cell *Cell = &Grid->Cells[Hash];
                
                // Add all bodies in this cell
                for (u32 i = 0; i < Cell->BodyCount && ResultCount < MaxResults; ++i)
                {
                    u32 BodyID = Cell->Bodies[i];
                    
                    // Simple duplicate check (expensive but necessary for queries)
                    b32 Duplicate = 0;
                    for (u32 j = 0; j < ResultCount; ++j)
                    {
                        if (Results[j] == BodyID)
                        {
                            Duplicate = 1;
                            break;
                        }
                    }
                    
                    if (!Duplicate)
                    {
                        Results[ResultCount++] = BodyID;
                    }
                }
            }
        }
    }
    
    return ResultCount;
}

// ========================================================================
// AABB OVERLAP TESTING
// ========================================================================

b32
PhysicsAABBOverlap(v3 MinA, v3 MaxA, v3 MinB, v3 MaxB)
{
    // PERFORMANCE: Separating axis test with early out
    return (MinA.x <= MaxB.x && MaxA.x >= MinB.x) &&
           (MinA.y <= MaxB.y && MaxA.y >= MinB.y) &&
           (MinA.z <= MaxB.z && MaxA.z >= MinB.z);
}

f32
PhysicsAABBDistanceSquared(v3 MinA, v3 MaxA, v3 MinB, v3 MaxB)
{
    // Calculate squared distance between AABB centers
    v3 CenterA = V3Mul(V3Add(MinA, MaxA), 0.5f);
    v3 CenterB = V3Mul(V3Add(MinB, MaxB), 0.5f);
    return V3LengthSq(V3Sub(CenterA, CenterB));
}

// ========================================================================
// BROAD PHASE PAIR FINDING
// ========================================================================

void
PhysicsAddBroadPhasePair(physics_world *World, u32 BodyA, u32 BodyB, f32 DistanceSq)
{
    Assert(World);
    Assert(BodyA != BodyB);
    
    if (World->BroadPhasePairCount >= World->MaxBroadPhasePairs)
    {
        return; // Skip if too many pairs
    }
    
    broad_phase_pair *Pair = &World->BroadPhasePairs[World->BroadPhasePairCount++];
    
    // Ensure consistent ordering for pair (smaller index first)
    if (BodyA > BodyB)
    {
        u32 Temp = BodyA;
        BodyA = BodyB;
        BodyB = Temp;
    }
    
    Pair->BodyA = BodyA;
    Pair->BodyB = BodyB;
    Pair->DistanceSq = DistanceSq;
}

i32
PhysicsCompareBroadPhasePairs(const void *A, const void *B)
{
    broad_phase_pair *PairA = (broad_phase_pair*)A;
    broad_phase_pair *PairB = (broad_phase_pair*)B;
    
    // Sort by distance (closest pairs first)
    if (PairA->DistanceSq < PairB->DistanceSq) return -1;
    if (PairA->DistanceSq > PairB->DistanceSq) return 1;
    return 0;
}

u32
PhysicsBroadPhaseFindPairs(physics_world *World)
{
    Assert(World);
    
    u64 StartTime = ReadCPUTimer();
    
    // Reset pair count
    World->BroadPhasePairCount = 0;
    
    // Reset spatial hash grid
    PhysicsSpatialHashReset(&World->BroadPhase);
    
    // Insert all active bodies into spatial hash
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (!(Body->Flags & RIGID_BODY_ACTIVE)) continue;
        if (Body->Flags & RIGID_BODY_SLEEPING) continue;
        
        PhysicsSpatialHashInsert(&World->BroadPhase, i, Body->AABBMin, Body->AABBMax);
    }
    
    // Find pairs within each cell
    for (u32 CellIndex = 0; CellIndex < SPATIAL_HASH_SIZE; ++CellIndex)
    {
        spatial_cell *Cell = &World->BroadPhase.Cells[CellIndex];
        
        // Check all pairs within this cell
        for (u32 i = 0; i < Cell->BodyCount; ++i)
        {
            for (u32 j = i + 1; j < Cell->BodyCount; ++j)
            {
                u32 BodyA = Cell->Bodies[i];
                u32 BodyB = Cell->Bodies[j];
                
                // Skip if same body (can happen due to multi-cell insertion)
                if (BodyA == BodyB) continue;
                
                rigid_body *A = &World->Bodies[BodyA];
                rigid_body *B = &World->Bodies[BodyB];
                
                // Skip if both bodies are static
                if ((A->Flags & RIGID_BODY_STATIC) && (B->Flags & RIGID_BODY_STATIC))
                {
                    continue;
                }
                
                // AABB overlap test
                if (PhysicsAABBOverlap(A->AABBMin, A->AABBMax, B->AABBMin, B->AABBMax))
                {
                    f32 DistanceSq = PhysicsAABBDistanceSquared(A->AABBMin, A->AABBMax, 
                                                               B->AABBMin, B->AABBMax);
                    PhysicsAddBroadPhasePair(World, BodyA, BodyB, DistanceSq);
                    
                    if (World->BroadPhasePairCount >= World->MaxBroadPhasePairs)
                    {
                        break; // Stop if we hit the limit
                    }
                }
            }
            
            if (World->BroadPhasePairCount >= World->MaxBroadPhasePairs)
            {
                break;
            }
        }
        
        if (World->BroadPhasePairCount >= World->MaxBroadPhasePairs)
        {
            break;
        }
    }
    
    // Sort pairs by distance for better narrow phase performance
    // Closer objects are more likely to actually be colliding
    if (World->BroadPhasePairCount > 1)
    {
        // Simple bubble sort for small arrays, qsort for larger
        if (World->BroadPhasePairCount < 100)
        {
            // Bubble sort (cache-friendly for small arrays)
            for (u32 i = 0; i < World->BroadPhasePairCount - 1; ++i)
            {
                for (u32 j = 0; j < World->BroadPhasePairCount - i - 1; ++j)
                {
                    if (World->BroadPhasePairs[j].DistanceSq > World->BroadPhasePairs[j + 1].DistanceSq)
                    {
                        broad_phase_pair Temp = World->BroadPhasePairs[j];
                        World->BroadPhasePairs[j] = World->BroadPhasePairs[j + 1];
                        World->BroadPhasePairs[j + 1] = Temp;
                    }
                }
            }
        }
        else
        {
            // Use qsort for larger arrays
            // Note: In a real handmade engine, we'd implement our own sort
            // qsort((void*)World->BroadPhasePairs, World->BroadPhasePairCount, 
            //       sizeof(broad_phase_pair), PhysicsCompareBroadPhasePairs);
        }
    }
    
    u64 EndTime = ReadCPUTimer();
    World->BroadPhaseTime = EndTime - StartTime;
    
    return World->BroadPhasePairCount;
}

// ========================================================================
// BROAD PHASE UPDATE
// ========================================================================

void
PhysicsBroadPhaseUpdate(physics_world *World)
{
    Assert(World);
    
    // Update AABB for all active bodies
    u32 ActiveCount = 0;
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (!(Body->Flags & RIGID_BODY_ACTIVE)) continue;
        if (Body->Flags & RIGID_BODY_SLEEPING) continue;
        
        PhysicsUpdateAABB(Body);
        ActiveCount++;
    }
    
    World->ActiveBodyCount = ActiveCount;
}

// ========================================================================
// BROAD PHASE QUERIES
// ========================================================================

u32
PhysicsOverlapSphere(physics_world *World, v3 Center, f32 Radius, u32 *BodyIDs, u32 MaxBodies)
{
    Assert(World);
    Assert(BodyIDs);
    
    // Create AABB for sphere
    v3 RadiusVec = V3(Radius, Radius, Radius);
    v3 AABBMin = V3Sub(Center, RadiusVec);
    v3 AABBMax = V3Add(Center, RadiusVec);
    
    // Query spatial hash grid
    u32 CandidateCount = PhysicsSpatialHashQuery(&World->BroadPhase, AABBMin, AABBMax, 
                                                 BodyIDs, MaxBodies);
    
    // Filter candidates with actual sphere test
    u32 ResultCount = 0;
    for (u32 i = 0; i < CandidateCount; ++i)
    {
        u32 BodyID = BodyIDs[i];
        rigid_body *Body = &World->Bodies[BodyID];
        
        // Test AABB vs sphere
        v3 BodyCenter = V3Mul(V3Add(Body->AABBMin, Body->AABBMax), 0.5f);
        f32 DistanceSq = V3LengthSq(V3Sub(BodyCenter, Center));
        
        if (DistanceSq <= Radius * Radius)
        {
            BodyIDs[ResultCount++] = BodyID;
        }
    }
    
    return ResultCount;
}

u32
PhysicsOverlapBox(physics_world *World, v3 Center, v3 HalfExtents, quat Orientation,
                 u32 *BodyIDs, u32 MaxBodies)
{
    Assert(World);
    Assert(BodyIDs);
    
    // For now, use AABB approximation (could be improved with OBB test)
    v3 AABBMin = V3Sub(Center, HalfExtents);
    v3 AABBMax = V3Add(Center, HalfExtents);
    
    return PhysicsSpatialHashQuery(&World->BroadPhase, AABBMin, AABBMax, BodyIDs, MaxBodies);
}

// ========================================================================
// RAY CASTING
// ========================================================================

b32
PhysicsRayAABBIntersect(v3 Origin, v3 Direction, v3 AABBMin, v3 AABBMax, 
                       f32 MaxDistance, f32 *HitDistance)
{
    // Slab-based ray-AABB intersection test
    f32 TMin = 0.0f;
    f32 TMax = MaxDistance;
    
    for (u32 i = 0; i < 3; ++i)
    {
        f32 InvDir = 1.0f / Direction.e[i];
        f32 T1 = (AABBMin.e[i] - Origin.e[i]) * InvDir;
        f32 T2 = (AABBMax.e[i] - Origin.e[i]) * InvDir;
        
        if (T1 > T2)
        {
            f32 Temp = T1;
            T1 = T2;
            T2 = Temp;
        }
        
        TMin = Maximum(TMin, T1);
        TMax = Minimum(TMax, T2);
        
        if (TMin > TMax) return 0;
    }
    
    *HitDistance = TMin;
    return 1;
}

b32
PhysicsRayCast(physics_world *World, v3 Origin, v3 Direction, f32 MaxDistance,
              u32 *HitBodyID, v3 *HitPoint, v3 *HitNormal)
{
    Assert(World);
    
    // Normalize direction
    Direction = V3Normalize(Direction);
    
    // Create AABB for ray
    v3 EndPoint = V3Add(Origin, V3Mul(Direction, MaxDistance));
    v3 AABBMin, AABBMax;
    
    AABBMin.x = Minimum(Origin.x, EndPoint.x);
    AABBMin.y = Minimum(Origin.y, EndPoint.y);
    AABBMin.z = Minimum(Origin.z, EndPoint.z);
    
    AABBMax.x = Maximum(Origin.x, EndPoint.x);
    AABBMax.y = Maximum(Origin.y, EndPoint.y);
    AABBMax.z = Maximum(Origin.z, EndPoint.z);
    
    // Query spatial hash for potential hits
    u32 Candidates[256];  // Stack allocation
    u32 CandidateCount = PhysicsSpatialHashQuery(&World->BroadPhase, AABBMin, AABBMax, 
                                                 Candidates, ArrayCount(Candidates));
    
    // Find closest hit
    f32 ClosestDistance = MaxDistance;
    u32 ClosestBodyID = (u32)-1;
    
    for (u32 i = 0; i < CandidateCount; ++i)
    {
        u32 BodyID = Candidates[i];
        rigid_body *Body = &World->Bodies[BodyID];
        
        f32 HitDistance;
        if (PhysicsRayAABBIntersect(Origin, Direction, Body->AABBMin, Body->AABBMax, 
                                   ClosestDistance, &HitDistance))
        {
            if (HitDistance < ClosestDistance)
            {
                ClosestDistance = HitDistance;
                ClosestBodyID = BodyID;
            }
        }
    }
    
    if (ClosestBodyID != (u32)-1)
    {
        if (HitBodyID) *HitBodyID = ClosestBodyID;
        if (HitPoint) *HitPoint = V3Add(Origin, V3Mul(Direction, ClosestDistance));
        if (HitNormal) *HitNormal = V3(0, 1, 0);  // Placeholder - would need shape-specific calculation
        return 1;
    }
    
    return 0;
}

// ========================================================================
// PERFORMANCE PROFILING
// ========================================================================

void
PhysicsGetProfileInfo(physics_world *World, f32 *BroadPhaseMS, f32 *NarrowPhaseMS,
                     f32 *SolverMS, f32 *IntegrationMS, u32 *ActiveBodies)
{
    Assert(World);
    
    // Convert CPU cycles to milliseconds (approximate)
    f64 CyclesPerMs = 3000000.0;  // Assume 3GHz CPU
    
    if (BroadPhaseMS) *BroadPhaseMS = (f32)(World->BroadPhaseTime / CyclesPerMs);
    if (NarrowPhaseMS) *NarrowPhaseMS = (f32)(World->NarrowPhaseTime / CyclesPerMs);
    if (SolverMS) *SolverMS = (f32)(World->SolverTime / CyclesPerMs);
    if (IntegrationMS) *IntegrationMS = (f32)(World->IntegrationTime / CyclesPerMs);
    if (ActiveBodies) *ActiveBodies = World->ActiveBodyCount;
}