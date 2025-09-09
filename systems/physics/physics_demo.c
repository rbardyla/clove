/*
    Handmade Physics Engine - Interactive Demo
    Demonstrates 1000+ rigid bodies with visual debugging
    
    Features:
    - Falling boxes and spheres
    - Ragdoll simulation
    - Vehicle dynamics
    - Real-time performance metrics
    - Debug visualization overlays
    
    Controls:
    - WASD: Camera movement
    - Mouse: Look around
    - Space: Add random objects
    - R: Reset simulation
    - F1-F4: Toggle debug visualizations
*/

#include "handmade_physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ========================================================================
// DEMO STATE AND CAMERA
// ========================================================================

typedef struct demo_camera
{
    v3 Position;
    v3 Target;
    v3 Up;
    f32 Yaw, Pitch;
    f32 Distance;
} demo_camera;

typedef struct physics_demo_state
{
    physics_world *PhysicsWorld;
    demo_camera Camera;
    
    // Demo objects
    u32 GroundBodyID;
    u32 *BoxBodies;
    u32 *SphereBodies;
    u32 BoxCount;
    u32 SphereCount;
    u32 MaxBodies;
    
    // Ragdoll
    u32 RagdollBodies[10];
    u32 RagdollConstraints[9];
    b32 RagdollActive;
    
    // Performance metrics
    f32 FrameTime;
    f32 PhysicsTime;
    u32 ActiveBodyCount;
    
    // Debug flags
    b32 ShowAABBs;
    b32 ShowContacts;
    b32 ShowConstraints;
    b32 ShowProfiling;
    
    // Input state
    b32 SpacePressed;
    b32 ResetPressed;
    b32 F1Pressed, F2Pressed, F3Pressed, F4Pressed;
    
    // Demo mode
    u32 DemoMode;  // 0=boxes, 1=spheres, 2=mixed, 3=ragdoll
} physics_demo_state;

global_variable physics_demo_state *DemoState = 0;
global_variable u8 PhysicsArena[Megabytes(64)];  // 64MB for physics

// ========================================================================
// CAMERA FUNCTIONS
// ========================================================================

internal void
UpdateCamera(demo_camera *Camera, game_input *Input, f32 dt)
{
    Assert(Camera);
    Assert(Input);
    
    f32 MoveSpeed = 10.0f;  // units per second
    f32 MouseSensitivity = 0.005f;
    
    // Mouse look
    if (Input->MouseX != 0 || Input->MouseY != 0)
    {
        Camera->Yaw += (f32)Input->MouseX * MouseSensitivity;
        Camera->Pitch += (f32)Input->MouseY * MouseSensitivity;
        
        // Clamp pitch
        Camera->Pitch = Maximum(-Pi32 * 0.45f, Minimum(Pi32 * 0.45f, Camera->Pitch));
    }
    
    // WASD movement
    v3 Forward = V3(cosf(Camera->Yaw) * cosf(Camera->Pitch), 
                   sinf(Camera->Pitch), 
                   sinf(Camera->Yaw) * cosf(Camera->Pitch));
    v3 Right = V3Cross(Forward, V3(0, 1, 0));
    Right = V3Normalize(Right);
    
    v3 Movement = V3(0, 0, 0);
    
    if (Input->Controllers[0].MoveUp.EndedDown)    // W
        Movement = V3Add(Movement, Forward);
    if (Input->Controllers[0].MoveDown.EndedDown)  // S
        Movement = V3Sub(Movement, Forward);
    if (Input->Controllers[0].MoveLeft.EndedDown)  // A
        Movement = V3Sub(Movement, Right);
    if (Input->Controllers[0].MoveRight.EndedDown) // D
        Movement = V3Add(Movement, Right);
    
    if (V3LengthSq(Movement) > 0.001f)
    {
        Movement = V3Normalize(Movement);
        Camera->Position = V3Add(Camera->Position, V3Mul(Movement, MoveSpeed * dt));
    }
    
    // Update camera target
    Camera->Target = V3Add(Camera->Position, Forward);
}

// ========================================================================
// PHYSICS OBJECT CREATION
// ========================================================================

internal void
CreateGround(physics_demo_state *State)
{
    Assert(State);
    
    // Create large static box as ground
    State->GroundBodyID = PhysicsCreateBody(State->PhysicsWorld, V3(0, -5, 0), QuaternionIdentity());
    
    collision_shape GroundShape = PhysicsCreateBox(V3(50, 1, 50));
    PhysicsSetBodyShape(State->PhysicsWorld, State->GroundBodyID, &GroundShape);
    
    material GroundMaterial = PhysicsCreateMaterial(1000.0f, 0.3f, 0.7f);
    PhysicsSetBodyMaterial(State->PhysicsWorld, State->GroundBodyID, &GroundMaterial);
    
    // Make it static
    rigid_body *GroundBody = PhysicsGetBody(State->PhysicsWorld, State->GroundBodyID);
    GroundBody->Flags |= RIGID_BODY_STATIC;
    PhysicsCalculateMassProperties(GroundBody);
}

internal void
AddRandomBox(physics_demo_state *State, v3 Position)
{
    Assert(State);
    
    if (State->BoxCount >= State->MaxBodies / 2) return;
    
    // Random size and orientation
    f32 SizeX = 0.5f + ((f32)rand() / RAND_MAX) * 1.0f;
    f32 SizeY = 0.5f + ((f32)rand() / RAND_MAX) * 1.0f;
    f32 SizeZ = 0.5f + ((f32)rand() / RAND_MAX) * 1.0f;
    
    f32 AngleX = ((f32)rand() / RAND_MAX) * Pi32 * 0.2f;
    f32 AngleY = ((f32)rand() / RAND_MAX) * Pi32 * 2.0f;
    f32 AngleZ = ((f32)rand() / RAND_MAX) * Pi32 * 0.2f;
    
    quaternion Orientation = QuaternionFromAxisAngle(V3(1, 0, 0), AngleX);
    Orientation = QuaternionMul(Orientation, QuaternionFromAxisAngle(V3(0, 1, 0), AngleY));
    Orientation = QuaternionMul(Orientation, QuaternionFromAxisAngle(V3(0, 0, 1), AngleZ));
    
    u32 BodyID = PhysicsCreateBody(State->PhysicsWorld, Position, Orientation);
    
    collision_shape BoxShape = PhysicsCreateBox(V3(SizeX, SizeY, SizeZ));
    PhysicsSetBodyShape(State->PhysicsWorld, BodyID, &BoxShape);
    
    material BoxMaterial = PhysicsCreateMaterial(1.0f, 0.4f, 0.6f);
    PhysicsSetBodyMaterial(State->PhysicsWorld, BodyID, &BoxMaterial);
    
    State->BoxBodies[State->BoxCount++] = BodyID;
}

internal void
AddRandomSphere(physics_demo_state *State, v3 Position)
{
    Assert(State);
    
    if (State->SphereCount >= State->MaxBodies / 2) return;
    
    f32 Radius = 0.3f + ((f32)rand() / RAND_MAX) * 0.7f;
    
    u32 BodyID = PhysicsCreateBody(State->PhysicsWorld, Position, QuaternionIdentity());
    
    collision_shape SphereShape = PhysicsCreateSphere(Radius);
    PhysicsSetBodyShape(State->PhysicsWorld, BodyID, &SphereShape);
    
    material SphereMaterial = PhysicsCreateMaterial(1.0f, 0.6f, 0.4f);
    PhysicsSetBodyMaterial(State->PhysicsWorld, BodyID, &SphereMaterial);
    
    State->SphereBodies[State->SphereCount++] = BodyID;
}

internal void
CreateRagdoll(physics_demo_state *State, v3 Position)
{
    Assert(State);
    
    if (State->RagdollActive) return;
    
    // Create ragdoll bodies
    // Head
    State->RagdollBodies[0] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(0, 2.5f, 0)), 
                                               QuaternionIdentity());
    collision_shape HeadShape = PhysicsCreateSphere(0.3f);
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[0], &HeadShape);
    
    // Torso
    State->RagdollBodies[1] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(0, 1.5f, 0)), 
                                               QuaternionIdentity());
    collision_shape TorsoShape = PhysicsCreateBox(V3(0.4f, 0.6f, 0.2f));
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[1], &TorsoShape);
    
    // Arms
    State->RagdollBodies[2] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(-0.8f, 1.5f, 0)), 
                                               QuaternionIdentity());
    State->RagdollBodies[3] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(0.8f, 1.5f, 0)), 
                                               QuaternionIdentity());
    collision_shape ArmShape = PhysicsCreateBox(V3(0.35f, 0.15f, 0.15f));
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[2], &ArmShape);
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[3], &ArmShape);
    
    // Forearms
    State->RagdollBodies[4] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(-1.3f, 1.0f, 0)), 
                                               QuaternionIdentity());
    State->RagdollBodies[5] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(1.3f, 1.0f, 0)), 
                                               QuaternionIdentity());
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[4], &ArmShape);
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[5], &ArmShape);
    
    // Thighs
    State->RagdollBodies[6] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(-0.2f, 0.5f, 0)), 
                                               QuaternionIdentity());
    State->RagdollBodies[7] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(0.2f, 0.5f, 0)), 
                                               QuaternionIdentity());
    collision_shape ThighShape = PhysicsCreateBox(V3(0.15f, 0.4f, 0.15f));
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[6], &ThighShape);
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[7], &ThighShape);
    
    // Shins
    State->RagdollBodies[8] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(-0.2f, -0.3f, 0)), 
                                               QuaternionIdentity());
    State->RagdollBodies[9] = PhysicsCreateBody(State->PhysicsWorld, 
                                               V3Add(Position, V3(0.2f, -0.3f, 0)), 
                                               QuaternionIdentity());
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[8], &ThighShape);
    PhysicsSetBodyShape(State->PhysicsWorld, State->RagdollBodies[9], &ThighShape);
    
    // Set material for all parts
    material RagdollMaterial = PhysicsCreateMaterial(1.0f, 0.2f, 0.8f);
    for (u32 i = 0; i < 10; ++i)
    {
        PhysicsSetBodyMaterial(State->PhysicsWorld, State->RagdollBodies[i], &RagdollMaterial);
    }
    
    // Create joints
    State->RagdollConstraints[0] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld, 
                                                                    State->RagdollBodies[0], State->RagdollBodies[1],  // Head to torso
                                                                    V3(0, -0.3f, 0), V3(0, 0.6f, 0));
    
    State->RagdollConstraints[1] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[1], State->RagdollBodies[2],  // Torso to left arm
                                                                    V3(-0.4f, 0.3f, 0), V3(0.35f, 0, 0));
    
    State->RagdollConstraints[2] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[1], State->RagdollBodies[3],  // Torso to right arm
                                                                    V3(0.4f, 0.3f, 0), V3(-0.35f, 0, 0));
    
    State->RagdollConstraints[3] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[2], State->RagdollBodies[4],  // Left arm to forearm
                                                                    V3(-0.35f, 0, 0), V3(0.35f, 0, 0));
    
    State->RagdollConstraints[4] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[3], State->RagdollBodies[5],  // Right arm to forearm
                                                                    V3(0.35f, 0, 0), V3(-0.35f, 0, 0));
    
    State->RagdollConstraints[5] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[1], State->RagdollBodies[6],  // Torso to left thigh
                                                                    V3(-0.2f, -0.6f, 0), V3(0, 0.4f, 0));
    
    State->RagdollConstraints[6] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[1], State->RagdollBodies[7],  // Torso to right thigh
                                                                    V3(0.2f, -0.6f, 0), V3(0, 0.4f, 0));
    
    State->RagdollConstraints[7] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[6], State->RagdollBodies[8],  // Left thigh to shin
                                                                    V3(0, -0.4f, 0), V3(0, 0.4f, 0));
    
    State->RagdollConstraints[8] = PhysicsCreateBallSocketConstraint(State->PhysicsWorld,
                                                                    State->RagdollBodies[7], State->RagdollBodies[9],  // Right thigh to shin
                                                                    V3(0, -0.4f, 0), V3(0, 0.4f, 0));
    
    State->RagdollActive = true;
}

// ========================================================================
// DEMO SCENARIOS
// ========================================================================

internal void
CreateBoxTower(physics_demo_state *State)
{
    for (u32 Layer = 0; Layer < 20; ++Layer)
    {
        for (u32 x = 0; x < 5; ++x)
        {
            for (u32 z = 0; z < 5; ++z)
            {
                v3 Position = V3((f32)x - 2.0f, (f32)Layer * 2.2f + 1.0f, (f32)z - 2.0f);
                AddRandomBox(State, Position);
                
                if (State->BoxCount >= 500) return;  // Limit for performance
            }
        }
    }
}

internal void
CreateSpherePyramid(physics_demo_state *State)
{
    u32 BaseSize = 15;
    for (u32 Layer = 0; Layer < BaseSize; ++Layer)
    {
        u32 LayerSize = BaseSize - Layer;
        for (u32 x = 0; x < LayerSize; ++x)
        {
            for (u32 z = 0; z < LayerSize; ++z)
            {
                v3 Position = V3((f32)x - (f32)LayerSize * 0.5f + 0.5f, 
                                (f32)Layer * 1.8f + 1.0f, 
                                (f32)z - (f32)LayerSize * 0.5f + 0.5f);
                AddRandomSphere(State, Position);
                
                if (State->SphereCount >= 500) return;  // Limit for performance
            }
        }
    }
}

internal void
CreateMixedScene(physics_demo_state *State)
{
    // Mix of boxes and spheres falling from height
    for (u32 i = 0; i < 1000; ++i)
    {
        f32 x = ((f32)rand() / RAND_MAX - 0.5f) * 20.0f;
        f32 y = 10.0f + ((f32)rand() / RAND_MAX) * 30.0f;
        f32 z = ((f32)rand() / RAND_MAX - 0.5f) * 20.0f;
        
        v3 Position = V3(x, y, z);
        
        if (rand() % 2 == 0)
        {
            AddRandomBox(State, Position);
        }
        else
        {
            AddRandomSphere(State, Position);
        }
        
        if (State->BoxCount + State->SphereCount >= 1000) break;
    }
}

// ========================================================================
// DEBUG VISUALIZATION
// ========================================================================

internal void
DrawWireframeBox(game_offscreen_buffer *Buffer, v3 Center, v3 HalfExtents, u32 Color)
{
    // Simple box drawing (would need proper 3D projection in real implementation)
    i32 x = (i32)(Center.x * 10 + Buffer->Width / 2);
    i32 y = (i32)(-Center.z * 10 + Buffer->Height / 2);
    i32 w = (i32)(HalfExtents.x * 20);
    i32 h = (i32)(HalfExtents.z * 20);
    
    DrawRectangle(Buffer, x - w/2, y - h/2, w, h, Color);
}

internal void
DrawWireframeSphere(game_offscreen_buffer *Buffer, v3 Center, f32 Radius, u32 Color)
{
    // Simple circle drawing (top-down view)
    i32 x = (i32)(Center.x * 10 + Buffer->Width / 2);
    i32 y = (i32)(-Center.z * 10 + Buffer->Height / 2);
    i32 r = (i32)(Radius * 20);
    
    // Draw circle as square for simplicity
    DrawRectangle(Buffer, x - r, y - r, r * 2, r * 2, Color);
}

internal void
DrawPhysicsDebug(physics_demo_state *State, game_offscreen_buffer *Buffer)
{
    if (!State->PhysicsWorld) return;
    
    physics_world *World = State->PhysicsWorld;
    
    // Draw all bodies
    for (u32 i = 0; i < World->BodyCount; ++i)
    {
        rigid_body *Body = &World->Bodies[i];
        
        if (!(Body->Flags & RIGID_BODY_ACTIVE)) continue;
        
        u32 Color = COLOR_WHITE;
        if (Body->Flags & RIGID_BODY_STATIC) Color = COLOR_GRAY;
        else if (Body->Flags & RIGID_BODY_SLEEPING) Color = COLOR_DARK_GRAY;
        
        switch (Body->Shape.Type)
        {
            case SHAPE_BOX:
                DrawWireframeBox(Buffer, Body->Position, Body->Shape.Box.HalfExtents, Color);
                break;
            case SHAPE_SPHERE:
                DrawWireframeSphere(Buffer, Body->Position, Body->Shape.Sphere.Radius, Color);
                break;
            default:
                break;
        }
        
        // Draw AABB if enabled
        if (State->ShowAABBs)
        {
            v3 AABBCenter = V3Mul(V3Add(Body->AABBMin, Body->AABBMax), 0.5f);
            v3 AABBHalfSize = V3Mul(V3Sub(Body->AABBMax, Body->AABBMin), 0.5f);
            DrawWireframeBox(Buffer, AABBCenter, AABBHalfSize, COLOR_YELLOW);
        }
    }
    
    // Draw contact points if enabled
    if (State->ShowContacts)
    {
        for (u32 i = 0; i < World->ManifoldCount; ++i)
        {
            contact_manifold *Manifold = &World->Manifolds[i];
            
            for (u32 j = 0; j < Manifold->PointCount; ++j)
            {
                contact_point *Contact = &Manifold->Points[j];
                
                i32 x = (i32)(Contact->PositionA.x * 10 + Buffer->Width / 2);
                i32 y = (i32)(-Contact->PositionA.z * 10 + Buffer->Height / 2);
                
                DrawRectangle(Buffer, x - 2, y - 2, 4, 4, COLOR_RED);
            }
        }
    }
    
    // Draw constraints if enabled
    if (State->ShowConstraints)
    {
        for (u32 i = 0; i < World->ConstraintCount; ++i)
        {
            constraint *Constraint = &World->Constraints[i];
            
            rigid_body *BodyA = &World->Bodies[Constraint->BodyA];
            rigid_body *BodyB = &World->Bodies[Constraint->BodyB];
            
            i32 x1 = (i32)(BodyA->Position.x * 10 + Buffer->Width / 2);
            i32 y1 = (i32)(-BodyA->Position.z * 10 + Buffer->Height / 2);
            i32 x2 = (i32)(BodyB->Position.x * 10 + Buffer->Width / 2);
            i32 y2 = (i32)(-BodyB->Position.z * 10 + Buffer->Height / 2);
            
            // Draw line between bodies (simple implementation)
            DrawRectangle(Buffer, x1, y1, 2, 2, COLOR_CYAN);
            DrawRectangle(Buffer, x2, y2, 2, 2, COLOR_CYAN);
        }
    }
}

internal void
DrawUI(physics_demo_state *State, game_offscreen_buffer *Buffer)
{
    char TextBuffer[256];
    
    // Performance info
    if (State->ShowProfiling)
    {
        f32 BroadPhaseMS, NarrowPhaseMS, SolverMS, IntegrationMS;
        u32 ActiveBodies;
        
        PhysicsGetProfileInfo(State->PhysicsWorld, &BroadPhaseMS, &NarrowPhaseMS, 
                             &SolverMS, &IntegrationMS, &ActiveBodies);
        
        sprintf(TextBuffer, "FPS: %.1f | Physics: %.2fms", 
                1.0f / State->FrameTime, State->PhysicsTime * 1000.0f);
        // Would draw text here in a real implementation
        
        sprintf(TextBuffer, "Bodies: %u/%u Active: %u", 
                State->BoxCount + State->SphereCount, State->MaxBodies, ActiveBodies);
        
        sprintf(TextBuffer, "Broad: %.2fms | Narrow: %.2fms | Solver: %.2fms | Integration: %.2fms",
                BroadPhaseMS, NarrowPhaseMS, SolverMS, IntegrationMS);
        
        sprintf(TextBuffer, "Contacts: %u | Constraints: %u", 
                State->PhysicsWorld->ManifoldCount, State->PhysicsWorld->ConstraintCount);
    }
    
    // Controls help
    sprintf(TextBuffer, "SPACE: Add objects | R: Reset | F1-F4: Debug views | Mode: %u", State->DemoMode);
}

// ========================================================================
// INPUT HANDLING
// ========================================================================

internal void
HandleInput(physics_demo_state *State, game_input *Input)
{
    // Space to add objects
    b32 SpaceDown = Input->Controllers[0].ActionDown.EndedDown;
    if (SpaceDown && !State->SpacePressed)
    {
        // Add objects at camera position
        v3 SpawnPos = V3Add(State->Camera.Position, V3(0, 0, -3));
        
        switch (State->DemoMode)
        {
            case 0:  // Boxes
                for (u32 i = 0; i < 5; ++i)
                {
                    v3 Offset = V3(((f32)rand() / RAND_MAX - 0.5f) * 2.0f, 
                                  ((f32)rand() / RAND_MAX) * 2.0f,
                                  ((f32)rand() / RAND_MAX - 0.5f) * 2.0f);
                    AddRandomBox(State, V3Add(SpawnPos, Offset));
                }
                break;
            case 1:  // Spheres
                for (u32 i = 0; i < 5; ++i)
                {
                    v3 Offset = V3(((f32)rand() / RAND_MAX - 0.5f) * 2.0f, 
                                  ((f32)rand() / RAND_MAX) * 2.0f,
                                  ((f32)rand() / RAND_MAX - 0.5f) * 2.0f);
                    AddRandomSphere(State, V3Add(SpawnPos, Offset));
                }
                break;
            case 3:  // Ragdoll
                CreateRagdoll(State, SpawnPos);
                break;
        }
    }
    State->SpacePressed = SpaceDown;
    
    // R to reset
    b32 ResetDown = Input->Controllers[0].ActionLeft.EndedDown;  // R key
    if (ResetDown && !State->ResetPressed)
    {
        PhysicsResetWorld(State->PhysicsWorld);
        State->BoxCount = 0;
        State->SphereCount = 0;
        State->RagdollActive = false;
        
        CreateGround(State);
        
        // Create initial scene based on mode
        switch (State->DemoMode)
        {
            case 0: CreateBoxTower(State); break;
            case 1: CreateSpherePyramid(State); break;
            case 2: CreateMixedScene(State); break;
            case 3: CreateRagdoll(State, V3(0, 5, 0)); break;
        }
    }
    State->ResetPressed = ResetDown;
    
    // F1-F4 for debug toggles
    b32 F1Down = Input->Controllers[0].LeftShoulder.EndedDown;  // F1
    if (F1Down && !State->F1Pressed)
    {
        State->ShowAABBs = !State->ShowAABBs;
    }
    State->F1Pressed = F1Down;
    
    b32 F2Down = Input->Controllers[0].RightShoulder.EndedDown;  // F2
    if (F2Down && !State->F2Pressed)
    {
        State->ShowContacts = !State->ShowContacts;
    }
    State->F2Pressed = F2Down;
    
    b32 F3Down = Input->Controllers[0].Back.EndedDown;  // F3
    if (F3Down && !State->F3Pressed)
    {
        State->ShowConstraints = !State->ShowConstraints;
    }
    State->F3Pressed = F3Down;
    
    b32 F4Down = Input->Controllers[0].Start.EndedDown;  // F4
    if (F4Down && !State->F4Pressed)
    {
        State->ShowProfiling = !State->ShowProfiling;
    }
    State->F4Pressed = F4Down;
    
    // Tab to cycle demo modes
    b32 TabDown = Input->Controllers[0].ActionUp.EndedDown;  // Tab
    if (TabDown)
    {
        State->DemoMode = (State->DemoMode + 1) % 4;
    }
}

// ========================================================================
// MAIN DEMO FUNCTIONS
// ========================================================================

GAME_UPDATE_AND_RENDER(PhysicsDemoUpdateAndRender)
{
    Assert(sizeof(physics_demo_state) <= Memory->PermanentStorageSize);
    
    physics_demo_state *State = (physics_demo_state*)Memory->PermanentStorage;
    
    // Initialize on first frame
    if (!Memory->IsInitialized)
    {
        State->PhysicsWorld = PhysicsCreateWorld(sizeof(PhysicsArena), PhysicsArena);
        State->MaxBodies = 2000;
        State->BoxBodies = (u32*)((u8*)Memory->PermanentStorage + sizeof(physics_demo_state));
        State->SphereBodies = State->BoxBodies + State->MaxBodies / 2;
        
        // Initialize camera
        State->Camera.Position = V3(0, 5, 10);
        State->Camera.Target = V3(0, 0, 0);
        State->Camera.Up = V3(0, 1, 0);
        State->Camera.Yaw = 0.0f;
        State->Camera.Pitch = -0.3f;
        
        // Initialize demo
        State->DemoMode = 2;  // Start with mixed scene
        State->ShowProfiling = true;
        
        srand((unsigned int)time(0));  // Initialize random seed
        
        CreateGround(State);
        CreateMixedScene(State);
        
        PhysicsSetDebugFlags(State->PhysicsWorld, true, true, true);
        
        Memory->IsInitialized = true;
    }
    
    DemoState = State;
    
    f32 dt = Input->dtForFrame;
    State->FrameTime = dt;
    
    // Handle input
    HandleInput(State, Input);
    
    // Update camera
    UpdateCamera(&State->Camera, Input, dt);
    
    // Step physics
    u64 PhysicsStartTime = ReadCPUTimer();
    PhysicsStepSimulation(State->PhysicsWorld, dt);
    u64 PhysicsEndTime = ReadCPUTimer();
    
    State->PhysicsTime = (f32)(PhysicsEndTime - PhysicsStartTime) / (3000000000.0f);  // Assume 3GHz CPU
    
    // Clear screen
    ClearBuffer(Buffer, COLOR_BLACK);
    
    // Draw physics world
    DrawPhysicsDebug(State, Buffer);
    
    // Draw UI
    DrawUI(State, Buffer);
}

// ========================================================================
// DEBUG VISUALIZATION API IMPLEMENTATION
// ========================================================================

void
PhysicsSetDebugFlags(physics_world *World, b32 DrawAABBs, b32 DrawContacts, b32 DrawConstraints)
{
    Assert(World);
    World->DrawAABBs = DrawAABBs;
    World->DrawContacts = DrawContacts;
    World->DrawConstraints = DrawConstraints;
}

void
PhysicsDebugDraw(physics_world *World, game_offscreen_buffer *Buffer)
{
    if (!World || !Buffer) return;
    
    if (DemoState)
    {
        DrawPhysicsDebug(DemoState, Buffer);
    }
}