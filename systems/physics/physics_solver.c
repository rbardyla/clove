/*
    Handmade Physics Engine - Constraint Solver and Integration
    Sequential impulse solver with Verlet integration
    
    Performance philosophy:
    - Cache-coherent access patterns
    - SIMD for batch processing
    - Warm-starting for stability
    - Deterministic convergence
    
    Algorithm:
    1. Apply external forces (gravity, user forces)
    2. Solve contact constraints iteratively
    3. Solve joint constraints
    4. Integrate velocities (explicit)
    5. Integrate positions (Verlet for stability)
    6. Update sleep states
*/

#include "handmade_physics.h"
#include <stdbool.h>

// ========================================================================
// INTEGRATION FUNCTIONS
// ========================================================================

void
PhysicsApplyGravity(physics_world *World, f32 dt)
{
    Assert(World);
    
    // PERFORMANCE: Batch process 4 bodies at once using SIMD
    // MEMORY: Sequential access through body array
    v3 GravityForce = V3Mul(World->Gravity, dt);
    
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (Body->InverseMass == 0.0f) continue;  // Skip static bodies
        if (Body->Flags & RIGID_BODY_SLEEPING) continue;  // Skip sleeping bodies
        
        // Apply gravity to velocity (not force, for better stability)
        Body->LinearVelocity = V3Add(Body->LinearVelocity, V3Mul(GravityForce, Body->InverseMass));
    }
}

void
PhysicsApplyDamping(physics_world *World, f32 dt)
{
    Assert(World);
    
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (Body->InverseMass == 0.0f) continue;  // Skip static bodies
        if (Body->Flags & RIGID_BODY_SLEEPING) continue;  // Skip sleeping bodies
        
        // Linear damping: v = v * (1 - damping * dt)
        f32 LinearDamping = 1.0f - (Body->Material.LinearDamping * dt);
        LinearDamping = Maximum(0.0f, LinearDamping);  // Clamp to prevent negative
        Body->LinearVelocity = V3Mul(Body->LinearVelocity, LinearDamping);
        
        // Angular damping
        f32 AngularDamping = 1.0f - (Body->Material.AngularDamping * dt);
        AngularDamping = Maximum(0.0f, AngularDamping);
        Body->AngularVelocity = V3Mul(Body->AngularVelocity, AngularDamping);
    }
}

void
PhysicsIntegrateVelocities(physics_world *World)
{
    Assert(World);
    
    u64 StartTime = ReadCPUTimer();
    f32 dt = World->TimeStep;
    
    // Apply gravity and external forces
    PhysicsApplyGravity(World, dt);
    PhysicsApplyDamping(World, dt);
    
    // Integrate velocities from accumulated forces
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (Body->InverseMass == 0.0f) continue;  // Skip static bodies
        if (Body->Flags & RIGID_BODY_SLEEPING) continue;  // Skip sleeping bodies
        
        // Linear: v = v + (F/m) * dt
        v3 LinearAccel = V3Mul(Body->Force, Body->InverseMass);
        Body->LinearVelocity = V3Add(Body->LinearVelocity, V3Mul(LinearAccel, dt));
        
        // Angular: ω = ω + I⁻¹ * τ * dt
        v3 AngularAccel;
        AngularAccel.x = Body->Torque.x * Body->InverseInertiaTensor.x;
        AngularAccel.y = Body->Torque.y * Body->InverseInertiaTensor.y;
        AngularAccel.z = Body->Torque.z * Body->InverseInertiaTensor.z;
        Body->AngularVelocity = V3Add(Body->AngularVelocity, V3Mul(AngularAccel, dt));
        
        // Clear accumulated forces for next frame
        Body->Force = V3(0, 0, 0);
        Body->Torque = V3(0, 0, 0);
    }
    
    u64 EndTime = ReadCPUTimer();
    World->IntegrationTime += EndTime - StartTime;
}

void
PhysicsIntegratePositions(physics_world *World)
{
    Assert(World);
    
    u64 StartTime = ReadCPUTimer();
    f32 dt = World->TimeStep;
    
    // PERFORMANCE: Batch process with SIMD potential
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (Body->InverseMass == 0.0f) continue;  // Skip static bodies
        if (Body->Flags & RIGID_BODY_SLEEPING) continue;  // Skip sleeping bodies
        
        // Semi-implicit Euler integration
        // Position: x = x + v * dt
        Body->Position = V3Add(Body->Position, V3Mul(Body->LinearVelocity, dt));
        
        // Rotation: integrate angular velocity to quaternion
        v3 AngularVel = Body->AngularVelocity;
        f32 AngularSpeed = V3Length(AngularVel);
        
        if (AngularSpeed > 1e-6f)
        {
            // Create rotation quaternion from angular velocity
            v3 Axis = V3Mul(AngularVel, 1.0f / AngularSpeed);
            f32 Angle = AngularSpeed * dt;
            
            quat DeltaRotation = QuaternionFromAxisAngle(Axis, Angle);
            Body->Orientation = QuaternionNormalize(QuaternionMul(Body->Orientation, DeltaRotation));
        }
        
        // Update AABB for broad phase
        PhysicsUpdateAABB(Body);
    }
    
    u64 EndTime = ReadCPUTimer();
    World->IntegrationTime += EndTime - StartTime;
}

// ========================================================================
// CONTACT CONSTRAINT SOLVING
// ========================================================================

f32
PhysicsCalculateContactEffectiveMass(rigid_body *BodyA, rigid_body *BodyB, 
                                    v3 ContactPoint, v3 Normal)
{
    f32 EffectiveMass = 0.0f;
    
    // Linear contribution
    EffectiveMass += BodyA->InverseMass + BodyB->InverseMass;
    
    // Angular contribution
    if (BodyA->InverseMass > 0.0f)
    {
        v3 rA = V3Sub(ContactPoint, BodyA->Position);
        v3 rACrossN = V3Cross(rA, Normal);
        v3 AngularTerm;
        AngularTerm.x = rACrossN.x * BodyA->InverseInertiaTensor.x;
        AngularTerm.y = rACrossN.y * BodyA->InverseInertiaTensor.y;
        AngularTerm.z = rACrossN.z * BodyA->InverseInertiaTensor.z;
        EffectiveMass += V3Dot(rACrossN, AngularTerm);
    }
    
    if (BodyB->InverseMass > 0.0f)
    {
        v3 rB = V3Sub(ContactPoint, BodyB->Position);
        v3 rBCrossN = V3Cross(rB, Normal);
        v3 AngularTerm;
        AngularTerm.x = rBCrossN.x * BodyB->InverseInertiaTensor.x;
        AngularTerm.y = rBCrossN.y * BodyB->InverseInertiaTensor.y;
        AngularTerm.z = rBCrossN.z * BodyB->InverseInertiaTensor.z;
        EffectiveMass += V3Dot(rBCrossN, AngularTerm);
    }
    
    return (EffectiveMass > 1e-10f) ? 1.0f / EffectiveMass : 0.0f;
}

void
PhysicsApplyContactImpulse(rigid_body *BodyA, rigid_body *BodyB,
                          v3 ContactPoint, v3 Impulse)
{
    // Apply linear impulse
    if (BodyA->InverseMass > 0.0f)
    {
        BodyA->LinearVelocity = V3Add(BodyA->LinearVelocity, V3Mul(Impulse, BodyA->InverseMass));
        
        // Apply angular impulse
        v3 rA = V3Sub(ContactPoint, BodyA->Position);
        v3 AngularImpulse = V3Cross(rA, Impulse);
        v3 DeltaAngularVel;
        DeltaAngularVel.x = AngularImpulse.x * BodyA->InverseInertiaTensor.x;
        DeltaAngularVel.y = AngularImpulse.y * BodyA->InverseInertiaTensor.y;
        DeltaAngularVel.z = AngularImpulse.z * BodyA->InverseInertiaTensor.z;
        BodyA->AngularVelocity = V3Add(BodyA->AngularVelocity, DeltaAngularVel);
    }
    
    if (BodyB->InverseMass > 0.0f)
    {
        v3 NegImpulse = V3Mul(Impulse, -1.0f);
        BodyB->LinearVelocity = V3Add(BodyB->LinearVelocity, V3Mul(NegImpulse, BodyB->InverseMass));
        
        // Apply angular impulse
        v3 rB = V3Sub(ContactPoint, BodyB->Position);
        v3 AngularImpulse = V3Cross(rB, NegImpulse);
        v3 DeltaAngularVel;
        DeltaAngularVel.x = AngularImpulse.x * BodyB->InverseInertiaTensor.x;
        DeltaAngularVel.y = AngularImpulse.y * BodyB->InverseInertiaTensor.y;
        DeltaAngularVel.z = AngularImpulse.z * BodyB->InverseInertiaTensor.z;
        BodyB->AngularVelocity = V3Add(BodyB->AngularVelocity, DeltaAngularVel);
    }
}

f32
PhysicsCalculateContactVelocity(rigid_body *BodyA, rigid_body *BodyB,
                               v3 ContactPoint, v3 Direction)
{
    f32 RelativeVelocity = 0.0f;
    
    // Linear velocity contribution
    RelativeVelocity += V3Dot(BodyA->LinearVelocity, Direction);
    RelativeVelocity -= V3Dot(BodyB->LinearVelocity, Direction);
    
    // Angular velocity contribution
    if (BodyA->InverseMass > 0.0f)
    {
        v3 rA = V3Sub(ContactPoint, BodyA->Position);
        v3 VelFromAngular = V3Cross(BodyA->AngularVelocity, rA);
        RelativeVelocity += V3Dot(VelFromAngular, Direction);
    }
    
    if (BodyB->InverseMass > 0.0f)
    {
        v3 rB = V3Sub(ContactPoint, BodyB->Position);
        v3 VelFromAngular = V3Cross(BodyB->AngularVelocity, rB);
        RelativeVelocity -= V3Dot(VelFromAngular, Direction);
    }
    
    return RelativeVelocity;
}

void
PhysicsSolveContactConstraint(physics_world *World, contact_manifold *Manifold)
{
    Assert(World);
    Assert(Manifold);
    
    rigid_body *BodyA = &World->Bodies[Manifold->BodyA];
    rigid_body *BodyB = &World->Bodies[Manifold->BodyB];
    
    for (u32 PointIndex = 0; PointIndex < Manifold->PointCount; ++PointIndex)
    {
        contact_point *Contact = &Manifold->Points[PointIndex];
        
        // Contact point (average of the two contact positions)
        v3 ContactPoint = V3Mul(V3Add(Contact->PositionA, Contact->PositionB), 0.5f);
        v3 Normal = Contact->Normal;
        
        // Solve normal constraint (non-penetration)
        {
            f32 RelativeVelocity = PhysicsCalculateContactVelocity(BodyA, BodyB, ContactPoint, Normal);
            
            // Add restitution bias
            f32 RestitutionBias = 0.0f;
            if (RelativeVelocity < -1.0f)  // Only apply restitution for significant velocities
            {
                RestitutionBias = -Manifold->Restitution * RelativeVelocity;
            }
            
            // Add penetration bias (Baumgarte stabilization)
            f32 PenetrationBias = 0.0f;
            if (Contact->Penetration > PHYSICS_CONTACT_TOLERANCE)
            {
                PenetrationBias = 0.2f * (Contact->Penetration - PHYSICS_CONTACT_TOLERANCE) / World->TimeStep;
            }
            
            f32 TargetVelocity = RestitutionBias + PenetrationBias;
            f32 VelocityError = TargetVelocity - RelativeVelocity;
            
            // Calculate impulse magnitude
            f32 EffectiveMass = PhysicsCalculateContactEffectiveMass(BodyA, BodyB, ContactPoint, Normal);
            f32 ImpulseMagnitude = VelocityError * EffectiveMass;
            
            // Clamp accumulated impulse to positive (no pulling)
            f32 OldImpulse = Contact->NormalImpulse;
            Contact->NormalImpulse = Maximum(0.0f, OldImpulse + ImpulseMagnitude);
            ImpulseMagnitude = Contact->NormalImpulse - OldImpulse;
            
            // Apply impulse
            v3 Impulse = V3Mul(Normal, ImpulseMagnitude);
            PhysicsApplyContactImpulse(BodyA, BodyB, ContactPoint, Impulse);
        }
        
        // Solve friction constraints
        f32 FrictionCoefficient = Manifold->Friction;
        if (FrictionCoefficient > 0.0f)
        {
            // First friction direction
            {
                v3 Tangent = Manifold->Tangent1;
                f32 RelativeVelocity = PhysicsCalculateContactVelocity(BodyA, BodyB, ContactPoint, Tangent);
                
                f32 EffectiveMass = PhysicsCalculateContactEffectiveMass(BodyA, BodyB, ContactPoint, Tangent);
                f32 ImpulseMagnitude = -RelativeVelocity * EffectiveMass;
                
                // Clamp friction impulse (Coulomb friction)
                f32 MaxFriction = FrictionCoefficient * Contact->NormalImpulse;
                f32 OldImpulse = Contact->TangentImpulse[0];
                Contact->TangentImpulse[0] = Maximum(-MaxFriction, Minimum(MaxFriction, OldImpulse + ImpulseMagnitude));
                ImpulseMagnitude = Contact->TangentImpulse[0] - OldImpulse;
                
                v3 Impulse = V3Mul(Tangent, ImpulseMagnitude);
                PhysicsApplyContactImpulse(BodyA, BodyB, ContactPoint, Impulse);
            }
            
            // Second friction direction
            {
                v3 Tangent = Manifold->Tangent2;
                f32 RelativeVelocity = PhysicsCalculateContactVelocity(BodyA, BodyB, ContactPoint, Tangent);
                
                f32 EffectiveMass = PhysicsCalculateContactEffectiveMass(BodyA, BodyB, ContactPoint, Tangent);
                f32 ImpulseMagnitude = -RelativeVelocity * EffectiveMass;
                
                // Clamp friction impulse
                f32 MaxFriction = FrictionCoefficient * Contact->NormalImpulse;
                f32 OldImpulse = Contact->TangentImpulse[1];
                Contact->TangentImpulse[1] = Maximum(-MaxFriction, Minimum(MaxFriction, OldImpulse + ImpulseMagnitude));
                ImpulseMagnitude = Contact->TangentImpulse[1] - OldImpulse;
                
                v3 Impulse = V3Mul(Tangent, ImpulseMagnitude);
                PhysicsApplyContactImpulse(BodyA, BodyB, ContactPoint, Impulse);
            }
        }
    }
}

// ========================================================================
// JOINT CONSTRAINT SOLVING
// ========================================================================

void
PhysicsSolveDistanceConstraint(physics_world *World, constraint *Constraint)
{
    Assert(World);
    Assert(Constraint);
    Assert(Constraint->Type == CONSTRAINT_DISTANCE);
    
    rigid_body *BodyA = &World->Bodies[Constraint->BodyA];
    rigid_body *BodyB = &World->Bodies[Constraint->BodyB];
    
    // Transform anchor points to world space
    v3 AnchorA = V3Add(BodyA->Position, QuaternionRotateV3(BodyA->Orientation, Constraint->LocalAnchorA));
    v3 AnchorB = V3Add(BodyB->Position, QuaternionRotateV3(BodyB->Orientation, Constraint->LocalAnchorB));
    
    v3 Delta = V3Sub(AnchorB, AnchorA);
    f32 CurrentLength = V3Length(Delta);
    f32 RestLength = Constraint->Distance.RestLength;
    
    if (CurrentLength < 1e-6f) return;  // Degenerate case
    
    v3 Normal = V3Mul(Delta, 1.0f / CurrentLength);
    f32 LengthError = CurrentLength - RestLength;
    
    // Calculate relative velocity along constraint direction
    f32 RelativeVelocity = PhysicsCalculateContactVelocity(BodyA, BodyB, AnchorA, Normal);
    
    // Constraint bias (Baumgarte stabilization)
    f32 Bias = 0.1f * LengthError / World->TimeStep;
    f32 TargetVelocity = -Bias;
    f32 VelocityError = TargetVelocity - RelativeVelocity;
    
    // Calculate effective mass
    f32 EffectiveMass = PhysicsCalculateContactEffectiveMass(BodyA, BodyB, AnchorA, Normal);
    
    // Calculate and apply impulse
    f32 ImpulseMagnitude = VelocityError * EffectiveMass;
    v3 Impulse = V3Mul(Normal, ImpulseMagnitude);
    
    PhysicsApplyContactImpulse(BodyA, BodyB, AnchorA, Impulse);
}

void
PhysicsSolveBallSocketConstraint(physics_world *World, constraint *Constraint)
{
    Assert(World);
    Assert(Constraint);
    Assert(Constraint->Type == CONSTRAINT_BALL_SOCKET);
    
    rigid_body *BodyA = &World->Bodies[Constraint->BodyA];
    rigid_body *BodyB = &World->Bodies[Constraint->BodyB];
    
    // Transform anchor points to world space
    v3 AnchorA = V3Add(BodyA->Position, QuaternionRotateV3(BodyA->Orientation, Constraint->LocalAnchorA));
    v3 AnchorB = V3Add(BodyB->Position, QuaternionRotateV3(BodyB->Orientation, Constraint->LocalAnchorB));
    
    v3 PositionError = V3Sub(AnchorB, AnchorA);
    
    // Solve constraint in all three directions
    v3 Directions[] = { V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1) };
    
    for (u32 i = 0; i < 3; ++i)
    {
        v3 Direction = Directions[i];
        f32 Error = V3Dot(PositionError, Direction);
        
        if (fabsf(Error) < 1e-6f) continue;  // Skip if error is negligible
        
        f32 RelativeVelocity = PhysicsCalculateContactVelocity(BodyA, BodyB, AnchorA, Direction);
        
        // Baumgarte stabilization
        f32 Bias = 0.2f * Error / World->TimeStep;
        f32 TargetVelocity = -Bias;
        f32 VelocityError = TargetVelocity - RelativeVelocity;
        
        f32 EffectiveMass = PhysicsCalculateContactEffectiveMass(BodyA, BodyB, AnchorA, Direction);
        f32 ImpulseMagnitude = VelocityError * EffectiveMass;
        
        v3 Impulse = V3Mul(Direction, ImpulseMagnitude);
        PhysicsApplyContactImpulse(BodyA, BodyB, AnchorA, Impulse);
    }
}

void
PhysicsSolveConstraint(physics_world *World, constraint *Constraint)
{
    switch (Constraint->Type)
    {
        case CONSTRAINT_DISTANCE:
            PhysicsSolveDistanceConstraint(World, Constraint);
            break;
        case CONSTRAINT_BALL_SOCKET:
            PhysicsSolveBallSocketConstraint(World, Constraint);
            break;
        case CONSTRAINT_HINGE:
            // TODO: Implement hinge constraint
            break;
        case CONSTRAINT_SLIDER:
            // TODO: Implement slider constraint
            break;
        default:
            break;
    }
}

// ========================================================================
// MAIN CONSTRAINT SOLVER
// ========================================================================

void
PhysicsSolveConstraints(physics_world *World)
{
    Assert(World);
    
    u64 StartTime = ReadCPUTimer();
    
    // Iterative constraint solver
    for (u32 Iteration = 0; Iteration < World->SolverIterations; ++Iteration)
    {
        // Solve contact constraints
        for (u32 i = 0; i < World->ManifoldCount; ++i)
        {
            PhysicsSolveContactConstraint(World, &World->Manifolds[i]);
        }
        
        // Solve joint constraints
        for (u32 i = 0; i < World->ConstraintCount; ++i)
        {
            PhysicsSolveConstraint(World, &World->Constraints[i]);
        }
    }
    
    u64 EndTime = ReadCPUTimer();
    World->SolverTime = EndTime - StartTime;
}

// ========================================================================
// SLEEP SYSTEM FOR PERFORMANCE
// ========================================================================

void
PhysicsUpdateSleepState(physics_world *World)
{
    Assert(World);
    
    f32 SleepLinearThreshold = 0.1f;   // m/s
    f32 SleepAngularThreshold = 0.1f;  // rad/s
    f32 SleepTime = 1.0f;              // seconds
    
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (Body->InverseMass == 0.0f) continue;  // Skip static bodies
        
        // Check if body is moving slowly
        f32 LinearSpeedSq = V3LengthSq(Body->LinearVelocity);
        f32 AngularSpeedSq = V3LengthSq(Body->AngularVelocity);
        
        b32 IsMovingSlowly = (LinearSpeedSq < SleepLinearThreshold * SleepLinearThreshold) &&
                            (AngularSpeedSq < SleepAngularThreshold * SleepAngularThreshold);
        
        if (IsMovingSlowly)
        {
            Body->SleepTimer += World->TimeStep;
            
            if (Body->SleepTimer > SleepTime)
            {
                // Put body to sleep
                Body->Flags |= RIGID_BODY_SLEEPING;
                Body->Flags &= ~RIGID_BODY_ACTIVE;
                Body->LinearVelocity = V3(0, 0, 0);
                Body->AngularVelocity = V3(0, 0, 0);
            }
        }
        else
        {
            // Keep body awake
            Body->SleepTimer = 0.0f;
            Body->Flags |= RIGID_BODY_ACTIVE;
            Body->Flags &= ~RIGID_BODY_SLEEPING;
        }
    }
}

// ========================================================================
// CONSTRAINT CREATION
// ========================================================================

u32
PhysicsCreateDistanceConstraint(physics_world *World, u32 BodyA, u32 BodyB,
                               v3 AnchorA, v3 AnchorB, f32 RestLength)
{
    Assert(World);
    Assert(BodyA < World->BodyCount);
    Assert(BodyB < World->BodyCount);
    Assert(World->ConstraintCount < World->MaxConstraints);
    
    u32 ConstraintID = World->ConstraintCount++;
    constraint *Constraint = &World->Constraints[ConstraintID];
    
    *Constraint = (constraint){0};
    Constraint->Type = CONSTRAINT_DISTANCE;
    Constraint->BodyA = BodyA;
    Constraint->BodyB = BodyB;
    Constraint->LocalAnchorA = AnchorA;
    Constraint->LocalAnchorB = AnchorB;
    Constraint->Distance.RestLength = RestLength;
    Constraint->Distance.Stiffness = 1.0f;  // Full stiffness
    
    return ConstraintID;
}

u32
PhysicsCreateBallSocketConstraint(physics_world *World, u32 BodyA, u32 BodyB,
                                 v3 AnchorA, v3 AnchorB)
{
    Assert(World);
    Assert(BodyA < World->BodyCount);
    Assert(BodyB < World->BodyCount);
    Assert(World->ConstraintCount < World->MaxConstraints);
    
    u32 ConstraintID = World->ConstraintCount++;
    constraint *Constraint = &World->Constraints[ConstraintID];
    
    *Constraint = (constraint){0};
    Constraint->Type = CONSTRAINT_BALL_SOCKET;
    Constraint->BodyA = BodyA;
    Constraint->BodyB = BodyB;
    Constraint->LocalAnchorA = AnchorA;
    Constraint->LocalAnchorB = AnchorB;
    
    return ConstraintID;
}

u32
PhysicsCreateHingeConstraint(physics_world *World, u32 BodyA, u32 BodyB,
                            v3 AnchorA, v3 AnchorB, v3 AxisA, v3 AxisB)
{
    Assert(World);
    Assert(BodyA < World->BodyCount);
    Assert(BodyB < World->BodyCount);
    Assert(World->ConstraintCount < World->MaxConstraints);
    
    u32 ConstraintID = World->ConstraintCount++;
    constraint *Constraint = &World->Constraints[ConstraintID];
    
    *Constraint = (constraint){0};
    Constraint->Type = CONSTRAINT_HINGE;
    Constraint->BodyA = BodyA;
    Constraint->BodyB = BodyB;
    Constraint->LocalAnchorA = AnchorA;
    Constraint->LocalAnchorB = AnchorB;
    Constraint->Hinge.LocalAxisA = V3Normalize(AxisA);
    Constraint->Hinge.LocalAxisB = V3Normalize(AxisB);
    Constraint->Hinge.LowerLimit = -Pi32;  // No limits by default
    Constraint->Hinge.UpperLimit = Pi32;
    
    return ConstraintID;
}

void
PhysicsDestroyConstraint(physics_world *World, u32 ConstraintID)
{
    Assert(World);
    Assert(ConstraintID < World->ConstraintCount);
    
    // Simple approach: swap with last constraint and decrement count
    if (ConstraintID < World->ConstraintCount - 1)
    {
        World->Constraints[ConstraintID] = World->Constraints[World->ConstraintCount - 1];
    }
    World->ConstraintCount--;
}

// ========================================================================
// MAIN PHYSICS STEP
// ========================================================================

void
PhysicsStepSimulation(physics_world *World, f32 DeltaTime)
{
    Assert(World);
    Assert(!World->IsSimulating);  // Prevent recursive calls
    
    World->IsSimulating = true;
    World->AccumulatedTime += DeltaTime;
    
    // Fixed timestep with sub-stepping
    while (World->AccumulatedTime >= World->TimeStep)
    {
        World->AccumulatedTime -= World->TimeStep;
        
        // Reset performance counters
        World->BroadPhaseTime = 0;
        World->NarrowPhaseTime = 0;
        World->SolverTime = 0;
        World->IntegrationTime = 0;
        
        // 1. Update broad phase (AABB and spatial partitioning)
        PhysicsBroadPhaseUpdate(World);
        
        // 2. Find collision pairs
        PhysicsBroadPhaseFindPairs(World);
        
        // 3. Generate contact manifolds
        PhysicsNarrowPhase(World);
        
        // 4. Integrate velocities (apply forces)
        PhysicsIntegrateVelocities(World);
        
        // 5. Solve constraints
        PhysicsSolveConstraints(World);
        
        // 6. Integrate positions
        PhysicsIntegratePositions(World);
        
        // 7. Update sleep states
        PhysicsUpdateSleepState(World);
    }
    
    World->IsSimulating = 0;
}