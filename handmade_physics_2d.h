#ifndef HANDMADE_PHYSICS_2D_H
#define HANDMADE_PHYSICS_2D_H

/*
    Simple 2D Physics System for Handmade Engine
    
    Features:
    - Basic rigid body dynamics (position, velocity, acceleration)
    - Circle and box collision detection
    - Simple collision response with bouncing
    - Gravity and friction
    - Debug visualization using existing renderer
    
    Following handmade philosophy:
    - Zero external dependencies
    - Simple and understandable
    - Arena-based memory management
    - Cache-coherent data structures
*/

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include <math.h>

// ========================================================================
// PHYSICS CONSTANTS
// ========================================================================

#define PHYSICS_2D_MAX_BODIES 1000
#define PHYSICS_2D_TIMESTEP (1.0f / 60.0f)
#define PHYSICS_2D_GRAVITY_DEFAULT -9.81f
#define PHYSICS_2D_MAX_VELOCITY 50.0f
#define PHYSICS_2D_CONTACT_THRESHOLD 0.001f
#define PHYSICS_2D_MAX_CONTACTS 2000

// ========================================================================
// PHYSICS TYPES
// ========================================================================

typedef enum {
    BODY_TYPE_STATIC,    // Never moves (walls, ground)
    BODY_TYPE_DYNAMIC,   // Full physics simulation
    BODY_TYPE_KINEMATIC  // Moves but doesn't respond to forces
} BodyType2D;

typedef enum {
    SHAPE_2D_CIRCLE,
    SHAPE_2D_BOX
} ShapeType2D;

// Material properties for physics bodies
typedef struct {
    f32 restitution;  // Bounciness: 0.0 = no bounce, 1.0 = perfect bounce
    f32 friction;     // Friction coefficient: 0.0 = ice, 1.0 = rough
    f32 density;      // Mass = density * area
} Material2D;

// Axis-Aligned Bounding Box for broad phase
typedef struct {
    v2 min;
    v2 max;
} AABB2D;

// Collision shape
typedef struct {
    ShapeType2D type;
    union {
        struct {
            f32 radius;
        } circle;
        struct {
            v2 half_extents;  // Half width and height
        } box;
    };
} Shape2D;

// Rigid body for 2D physics
typedef struct {
    // Identification
    u32 id;
    bool active;
    
    // Body type
    BodyType2D type;
    
    // Transform
    v2 position;
    f32 rotation;  // In radians
    
    // Motion
    v2 velocity;
    f32 angular_velocity;
    
    // Forces (accumulated each frame)
    v2 force;
    f32 torque;
    
    // Physical properties
    f32 mass;
    f32 inv_mass;  // 1/mass (0 for infinite mass)
    f32 inertia;
    f32 inv_inertia;
    
    // Shape and material
    Shape2D shape;
    Material2D material;
    
    // Broad phase AABB (computed each frame)
    AABB2D aabb;
    
    // Rendering
    Color color;
    
    // User data pointer
    void* user_data;
} RigidBody2D;

// Contact information for collision resolution
typedef struct {
    RigidBody2D* body_a;
    RigidBody2D* body_b;
    v2 point;           // Contact point in world space
    v2 normal;          // Contact normal (from A to B)
    f32 penetration;    // Penetration depth
    f32 restitution;    // Combined restitution
    f32 friction;       // Combined friction
} Contact2D;

// Main physics world
typedef struct {
    bool initialized;
    
    // Memory arena for allocations
    MemoryArena* arena;
    
    // Bodies (Structure of Arrays for cache efficiency)
    RigidBody2D* bodies;
    u32 body_count;
    u32 max_bodies;
    
    // Contacts
    Contact2D* contacts;
    u32 contact_count;
    u32 max_contacts;
    
    // World settings
    v2 gravity;
    f32 air_friction;  // Global damping
    
    // Time accumulator for fixed timestep
    f32 accumulator;
    
    // Statistics
    u32 collision_checks;
    u32 collision_count;
    f32 simulation_time;
    
    // Debug settings
    bool debug_draw_enabled;
    bool debug_draw_aabb;
    bool debug_draw_contacts;
    bool debug_draw_velocities;
} Physics2DWorld;

// ========================================================================
// MATH HELPERS
// ========================================================================

static inline v2 v2_add(v2 a, v2 b) {
    return V2(a.x + b.x, a.y + b.y);
}

static inline v2 v2_sub(v2 a, v2 b) {
    return V2(a.x - b.x, a.y - b.y);
}

static inline v2 v2_scale(v2 v, f32 s) {
    return V2(v.x * s, v.y * s);
}

static inline f32 v2_dot(v2 a, v2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline f32 v2_length_sq(v2 v) {
    return v.x * v.x + v.y * v.y;
}

static inline f32 v2_length(v2 v) {
    return sqrtf(v2_length_sq(v));
}

static inline v2 v2_normalize(v2 v) {
    f32 len = v2_length(v);
    if (len > 0.0001f) {
        return v2_scale(v, 1.0f / len);
    }
    return V2(0, 0);
}

static inline v2 v2_rotate(v2 v, f32 angle) {
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    return V2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static inline f32 v2_cross(v2 a, v2 b) {
    return a.x * b.y - a.y * b.x;
}

static inline v2 v2_perp(v2 v) {
    return V2(-v.y, v.x);
}

static inline f32 clampf(f32 value, f32 min, f32 max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// ========================================================================
// PHYSICS API
// ========================================================================

// World management
bool Physics2DInit(Physics2DWorld* world, MemoryArena* arena, u32 max_bodies);
void Physics2DShutdown(Physics2DWorld* world);
void Physics2DReset(Physics2DWorld* world);

// Body management
RigidBody2D* Physics2DCreateBody(Physics2DWorld* world, v2 position, BodyType2D type);
void Physics2DDestroyBody(Physics2DWorld* world, RigidBody2D* body);
void Physics2DSetCircleShape(RigidBody2D* body, f32 radius);
void Physics2DSetBoxShape(RigidBody2D* body, v2 half_extents);

// Physics simulation
void Physics2DStep(Physics2DWorld* world, f32 dt);
void Physics2DApplyForce(RigidBody2D* body, v2 force);
void Physics2DApplyImpulse(RigidBody2D* body, v2 impulse);
void Physics2DApplyTorque(RigidBody2D* body, f32 torque);
void Physics2DSetVelocity(RigidBody2D* body, v2 velocity);

// Collision detection
bool Physics2DTestCircleCircle(v2 pos_a, f32 radius_a, v2 pos_b, f32 radius_b, Contact2D* contact);
bool Physics2DTestBoxBox(v2 pos_a, v2 half_a, f32 rot_a, v2 pos_b, v2 half_b, f32 rot_b, Contact2D* contact);
bool Physics2DTestCircleBox(v2 circle_pos, f32 radius, v2 box_pos, v2 box_half, f32 box_rot, Contact2D* contact);

// Debug rendering
void Physics2DDebugDraw(Physics2DWorld* world, Renderer* renderer);
void Physics2DDebugDrawBody(RigidBody2D* body, Renderer* renderer);

// Utilities
void Physics2DSetGravity(Physics2DWorld* world, v2 gravity);
void Physics2DSetAirFriction(Physics2DWorld* world, f32 friction);
AABB2D Physics2DComputeAABB(RigidBody2D* body);
bool Physics2DAABBOverlap(AABB2D a, AABB2D b);

#endif // HANDMADE_PHYSICS_2D_H