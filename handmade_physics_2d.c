/*
    Simple 2D Physics Implementation
    
    Following the handmade philosophy:
    - Understandable code
    - No hidden complexity
    - Predictable performance
*/

#include "handmade_physics_2d.h"
#include <string.h>
#include <stdio.h>

// ========================================================================
// WORLD MANAGEMENT
// ========================================================================

bool Physics2DInit(Physics2DWorld* world, MemoryArena* arena, u32 max_bodies) {
    if (!world || !arena) return false;
    
    memset(world, 0, sizeof(Physics2DWorld));
    
    world->arena = arena;
    world->max_bodies = max_bodies;
    world->max_contacts = PHYSICS_2D_MAX_CONTACTS;
    
    // Allocate body storage
    world->bodies = (RigidBody2D*)arena->base;
    arena->used += sizeof(RigidBody2D) * max_bodies;
    memset(world->bodies, 0, sizeof(RigidBody2D) * max_bodies);
    
    // Allocate contact storage
    world->contacts = (Contact2D*)(arena->base + arena->used);
    arena->used += sizeof(Contact2D) * world->max_contacts;
    
    // Initialize world settings
    world->gravity = V2(0.0f, PHYSICS_2D_GRAVITY_DEFAULT);
    world->air_friction = 0.01f;
    world->debug_draw_enabled = true;
    world->initialized = true;
    
    printf("Physics2D initialized: %u max bodies, %.2f KB memory\n", 
           max_bodies, (f32)arena->used / 1024.0f);
    
    return true;
}

void Physics2DShutdown(Physics2DWorld* world) {
    if (!world) return;
    // Since we use arena allocation, no individual frees needed
    world->initialized = false;
}

void Physics2DReset(Physics2DWorld* world) {
    if (!world || !world->initialized) return;
    
    // Clear all bodies
    memset(world->bodies, 0, sizeof(RigidBody2D) * world->max_bodies);
    world->body_count = 0;
    world->contact_count = 0;
    world->accumulator = 0.0f;
}

// ========================================================================
// BODY MANAGEMENT
// ========================================================================

RigidBody2D* Physics2DCreateBody(Physics2DWorld* world, v2 position, BodyType2D type) {
    if (!world || world->body_count >= world->max_bodies) {
        return NULL;
    }
    
    // Find first inactive slot
    RigidBody2D* body = NULL;
    for (u32 i = 0; i < world->max_bodies; i++) {
        if (!world->bodies[i].active) {
            body = &world->bodies[i];
            body->id = i;
            break;
        }
    }
    
    if (!body) return NULL;
    
    // Initialize body
    memset(body, 0, sizeof(RigidBody2D));
    body->active = true;
    body->type = type;
    body->position = position;
    body->rotation = 0.0f;
    
    // Default material
    body->material.restitution = 0.3f;
    body->material.friction = 0.7f;
    body->material.density = 1.0f;
    
    // Default color
    body->color = COLOR(0.5f, 0.7f, 1.0f, 1.0f);
    
    // Set mass based on type
    if (type == BODY_TYPE_STATIC) {
        body->mass = 0.0f;
        body->inv_mass = 0.0f;
        body->inertia = 0.0f;
        body->inv_inertia = 0.0f;
    }
    
    world->body_count++;
    return body;
}

void Physics2DDestroyBody(Physics2DWorld* world, RigidBody2D* body) {
    if (!world || !body) return;
    body->active = false;
    world->body_count--;
}

void Physics2DSetCircleShape(RigidBody2D* body, f32 radius) {
    if (!body) return;
    
    body->shape.type = SHAPE_2D_CIRCLE;
    body->shape.circle.radius = radius;
    
    // Calculate mass and inertia for dynamic bodies
    if (body->type == BODY_TYPE_DYNAMIC) {
        f32 area = 3.14159f * radius * radius;
        body->mass = body->material.density * area;
        body->inv_mass = 1.0f / body->mass;
        
        // Moment of inertia for circle: I = 0.5 * m * r^2
        body->inertia = 0.5f * body->mass * radius * radius;
        body->inv_inertia = 1.0f / body->inertia;
    }
}

void Physics2DSetBoxShape(RigidBody2D* body, v2 half_extents) {
    if (!body) return;
    
    body->shape.type = SHAPE_2D_BOX;
    body->shape.box.half_extents = half_extents;
    
    // Calculate mass and inertia for dynamic bodies
    if (body->type == BODY_TYPE_DYNAMIC) {
        f32 area = 4.0f * half_extents.x * half_extents.y;
        body->mass = body->material.density * area;
        body->inv_mass = 1.0f / body->mass;
        
        // Moment of inertia for box: I = (1/12) * m * (w^2 + h^2)
        f32 w = 2.0f * half_extents.x;
        f32 h = 2.0f * half_extents.y;
        body->inertia = (1.0f / 12.0f) * body->mass * (w * w + h * h);
        body->inv_inertia = 1.0f / body->inertia;
    }
}

// ========================================================================
// COLLISION DETECTION
// ========================================================================

AABB2D Physics2DComputeAABB(RigidBody2D* body) {
    AABB2D aabb;
    
    if (body->shape.type == SHAPE_2D_CIRCLE) {
        f32 r = body->shape.circle.radius;
        aabb.min = V2(body->position.x - r, body->position.y - r);
        aabb.max = V2(body->position.x + r, body->position.y + r);
    } else if (body->shape.type == SHAPE_2D_BOX) {
        // For rotated box, we need to find the axis-aligned bounds
        v2 half = body->shape.box.half_extents;
        f32 c = cosf(body->rotation);
        f32 s = sinf(body->rotation);
        
        // Calculate rotated corners
        f32 rx = fabsf(half.x * c) + fabsf(half.y * s);
        f32 ry = fabsf(half.x * s) + fabsf(half.y * c);
        
        aabb.min = V2(body->position.x - rx, body->position.y - ry);
        aabb.max = V2(body->position.x + rx, body->position.y + ry);
    }
    
    return aabb;
}

bool Physics2DAABBOverlap(AABB2D a, AABB2D b) {
    return !(a.max.x < b.min.x || a.min.x > b.max.x ||
             a.max.y < b.min.y || a.min.y > b.max.y);
}

bool Physics2DTestCircleCircle(v2 pos_a, f32 radius_a, v2 pos_b, f32 radius_b, Contact2D* contact) {
    v2 delta = v2_sub(pos_b, pos_a);
    f32 dist_sq = v2_length_sq(delta);
    f32 radius_sum = radius_a + radius_b;
    
    if (dist_sq > radius_sum * radius_sum) {
        return false;
    }
    
    if (contact) {
        f32 dist = sqrtf(dist_sq);
        if (dist < 0.0001f) {
            // Bodies are at same position, push apart along arbitrary axis
            contact->normal = V2(1, 0);
            contact->penetration = radius_sum;
        } else {
            contact->normal = v2_scale(delta, 1.0f / dist);
            contact->penetration = radius_sum - dist;
        }
        
        // Contact point is between the two centers
        contact->point = v2_add(pos_a, v2_scale(contact->normal, radius_a));
    }
    
    return true;
}

bool Physics2DTestBoxBox(v2 pos_a, v2 half_a, f32 rot_a, v2 pos_b, v2 half_b, f32 rot_b, Contact2D* contact) {
    // Simplified SAT (Separating Axis Theorem) for 2D boxes
    // We need to test 4 axes (2 from each box)
    
    f32 min_penetration = 1000000.0f;
    v2 best_normal = V2(0, 0);
    
    // Transform from B to A's local space
    v2 delta = v2_sub(pos_b, pos_a);
    f32 angle_diff = rot_b - rot_a;
    
    // Rotate delta to A's local space
    f32 c = cosf(-rot_a);
    f32 s = sinf(-rot_a);
    v2 local_delta = V2(delta.x * c - delta.y * s, delta.x * s + delta.y * c);
    
    // Test A's axes
    {
        // X axis
        f32 separation = fabsf(local_delta.x) - (half_a.x + half_b.x);
        if (separation > 0) return false;
        if (separation > -min_penetration) {
            min_penetration = -separation;
            best_normal = V2(1, 0);
            if (local_delta.x < 0) best_normal.x = -1;
            // Rotate normal back to world space
            best_normal = v2_rotate(best_normal, rot_a);
        }
        
        // Y axis
        separation = fabsf(local_delta.y) - (half_a.y + half_b.y);
        if (separation > 0) return false;
        if (separation > -min_penetration) {
            min_penetration = -separation;
            best_normal = V2(0, 1);
            if (local_delta.y < 0) best_normal.y = -1;
            // Rotate normal back to world space
            best_normal = v2_rotate(best_normal, rot_a);
        }
    }
    
    if (contact) {
        contact->normal = best_normal;
        contact->penetration = min_penetration;
        // Simplified contact point (center to center)
        contact->point = v2_add(pos_a, v2_scale(delta, 0.5f));
    }
    
    return true;
}

bool Physics2DTestCircleBox(v2 circle_pos, f32 radius, v2 box_pos, v2 box_half, f32 box_rot, Contact2D* contact) {
    // Transform circle to box's local space
    v2 delta = v2_sub(circle_pos, box_pos);
    f32 c = cosf(-box_rot);
    f32 s = sinf(-box_rot);
    v2 local_circle = V2(delta.x * c - delta.y * s, delta.x * s + delta.y * c);
    
    // Find closest point on box to circle center
    v2 closest;
    closest.x = clampf(local_circle.x, -box_half.x, box_half.x);
    closest.y = clampf(local_circle.y, -box_half.y, box_half.y);
    
    // Check if closest point is inside circle
    v2 diff = v2_sub(local_circle, closest);
    f32 dist_sq = v2_length_sq(diff);
    
    if (dist_sq > radius * radius) {
        return false;
    }
    
    if (contact) {
        f32 dist = sqrtf(dist_sq);
        
        if (dist < 0.0001f) {
            // Circle center is inside box
            // Find the axis with minimum penetration
            f32 pen_x = box_half.x + radius - fabsf(local_circle.x);
            f32 pen_y = box_half.y + radius - fabsf(local_circle.y);
            
            if (pen_x < pen_y) {
                contact->normal = V2(local_circle.x > 0 ? 1.0f : -1.0f, 0);
                contact->penetration = pen_x;
            } else {
                contact->normal = V2(0, local_circle.y > 0 ? 1.0f : -1.0f);
                contact->penetration = pen_y;
            }
        } else {
            // Circle center is outside box
            contact->normal = v2_scale(diff, 1.0f / dist);
            contact->penetration = radius - dist;
        }
        
        // Transform normal back to world space
        contact->normal = v2_rotate(contact->normal, box_rot);
        
        // Contact point
        v2 world_closest = v2_rotate(closest, box_rot);
        contact->point = v2_add(box_pos, world_closest);
    }
    
    return true;
}

// ========================================================================
// COLLISION DETECTION AND RESPONSE
// ========================================================================

static void DetectCollisions(Physics2DWorld* world) {
    world->contact_count = 0;
    world->collision_checks = 0;
    
    // Simple O(n^2) broad phase - could be optimized with spatial hashing
    for (u32 i = 0; i < world->max_bodies; i++) {
        RigidBody2D* body_a = &world->bodies[i];
        if (!body_a->active) continue;
        
        // Update AABB
        body_a->aabb = Physics2DComputeAABB(body_a);
        
        for (u32 j = i + 1; j < world->max_bodies; j++) {
            RigidBody2D* body_b = &world->bodies[j];
            if (!body_b->active) continue;
            
            // Skip if both are static
            if (body_a->type == BODY_TYPE_STATIC && body_b->type == BODY_TYPE_STATIC) {
                continue;
            }
            
            world->collision_checks++;
            
            // Broad phase AABB check
            body_b->aabb = Physics2DComputeAABB(body_b);
            if (!Physics2DAABBOverlap(body_a->aabb, body_b->aabb)) {
                continue;
            }
            
            // Narrow phase
            Contact2D contact = {0};
            bool colliding = false;
            
            if (body_a->shape.type == SHAPE_2D_CIRCLE && body_b->shape.type == SHAPE_2D_CIRCLE) {
                colliding = Physics2DTestCircleCircle(
                    body_a->position, body_a->shape.circle.radius,
                    body_b->position, body_b->shape.circle.radius,
                    &contact
                );
            } else if (body_a->shape.type == SHAPE_2D_BOX && body_b->shape.type == SHAPE_2D_BOX) {
                colliding = Physics2DTestBoxBox(
                    body_a->position, body_a->shape.box.half_extents, body_a->rotation,
                    body_b->position, body_b->shape.box.half_extents, body_b->rotation,
                    &contact
                );
            } else if (body_a->shape.type == SHAPE_2D_CIRCLE && body_b->shape.type == SHAPE_2D_BOX) {
                colliding = Physics2DTestCircleBox(
                    body_a->position, body_a->shape.circle.radius,
                    body_b->position, body_b->shape.box.half_extents, body_b->rotation,
                    &contact
                );
            } else if (body_a->shape.type == SHAPE_2D_BOX && body_b->shape.type == SHAPE_2D_CIRCLE) {
                colliding = Physics2DTestCircleBox(
                    body_b->position, body_b->shape.circle.radius,
                    body_a->position, body_a->shape.box.half_extents, body_a->rotation,
                    &contact
                );
                // Flip normal since we swapped the order
                contact.normal = v2_scale(contact.normal, -1.0f);
            }
            
            if (colliding && world->contact_count < world->max_contacts) {
                contact.body_a = body_a;
                contact.body_b = body_b;
                contact.restitution = fminf(body_a->material.restitution, body_b->material.restitution);
                contact.friction = sqrtf(body_a->material.friction * body_b->material.friction);
                
                world->contacts[world->contact_count++] = contact;
                world->collision_count++;
            }
        }
    }
}

static void ResolveCollisions(Physics2DWorld* world) {
    for (u32 i = 0; i < world->contact_count; i++) {
        Contact2D* contact = &world->contacts[i];
        RigidBody2D* a = contact->body_a;
        RigidBody2D* b = contact->body_b;
        
        // Skip if both are static or kinematic
        if ((a->type != BODY_TYPE_DYNAMIC && b->type != BODY_TYPE_DYNAMIC)) {
            continue;
        }
        
        // Calculate relative velocity
        v2 rv = v2_sub(b->velocity, a->velocity);
        
        // Velocity along normal
        f32 velocity_along_normal = v2_dot(rv, contact->normal);
        
        // Don't resolve if velocities are separating
        if (velocity_along_normal > 0) {
            continue;
        }
        
        // Calculate impulse scalar
        f32 e = contact->restitution;
        f32 j = -(1.0f + e) * velocity_along_normal;
        j /= (a->inv_mass + b->inv_mass);
        
        // Apply impulse
        v2 impulse = v2_scale(contact->normal, j);
        
        if (a->type == BODY_TYPE_DYNAMIC) {
            a->velocity = v2_sub(a->velocity, v2_scale(impulse, a->inv_mass));
        }
        if (b->type == BODY_TYPE_DYNAMIC) {
            b->velocity = v2_add(b->velocity, v2_scale(impulse, b->inv_mass));
        }
        
        // Positional correction to prevent sinking
        const f32 percent = 0.2f; // Penetration percentage to correct
        const f32 slop = 0.01f;    // Allowable penetration
        f32 correction_magnitude = fmaxf(contact->penetration - slop, 0.0f) / (a->inv_mass + b->inv_mass) * percent;
        v2 correction = v2_scale(contact->normal, correction_magnitude);
        
        if (a->type == BODY_TYPE_DYNAMIC) {
            a->position = v2_sub(a->position, v2_scale(correction, a->inv_mass));
        }
        if (b->type == BODY_TYPE_DYNAMIC) {
            b->position = v2_add(b->position, v2_scale(correction, b->inv_mass));
        }
        
        // Apply friction
        v2 tangent = v2_sub(rv, v2_scale(contact->normal, velocity_along_normal));
        f32 tangent_length = v2_length(tangent);
        
        if (tangent_length > 0.0001f) {
            tangent = v2_scale(tangent, 1.0f / tangent_length);
            
            f32 jt = -v2_dot(rv, tangent);
            jt /= (a->inv_mass + b->inv_mass);
            
            // Coulomb friction
            v2 friction_impulse;
            if (fabsf(jt) < j * contact->friction) {
                friction_impulse = v2_scale(tangent, jt);
            } else {
                friction_impulse = v2_scale(tangent, -j * contact->friction);
            }
            
            if (a->type == BODY_TYPE_DYNAMIC) {
                a->velocity = v2_sub(a->velocity, v2_scale(friction_impulse, a->inv_mass));
            }
            if (b->type == BODY_TYPE_DYNAMIC) {
                b->velocity = v2_add(b->velocity, v2_scale(friction_impulse, b->inv_mass));
            }
        }
    }
}

// ========================================================================
// PHYSICS SIMULATION
// ========================================================================

void Physics2DStep(Physics2DWorld* world, f32 dt) {
    if (!world || !world->initialized) return;
    
    // Fixed timestep with accumulator for deterministic simulation
    world->accumulator += dt;
    
    while (world->accumulator >= PHYSICS_2D_TIMESTEP) {
        // Clear forces for static and kinematic bodies
        for (u32 i = 0; i < world->max_bodies; i++) {
            RigidBody2D* body = &world->bodies[i];
            if (!body->active) continue;
            
            if (body->type != BODY_TYPE_DYNAMIC) {
                body->force = V2(0, 0);
                body->torque = 0;
            }
        }
        
        // Apply gravity and integrate forces
        for (u32 i = 0; i < world->max_bodies; i++) {
            RigidBody2D* body = &world->bodies[i];
            if (!body->active || body->type != BODY_TYPE_DYNAMIC) continue;
            
            // Apply gravity
            v2 gravity_force = v2_scale(world->gravity, body->mass);
            body->force = v2_add(body->force, gravity_force);
            
            // Apply air friction (simple damping)
            body->force = v2_sub(body->force, v2_scale(body->velocity, world->air_friction));
            
            // Integrate velocity (F = ma, so a = F/m)
            v2 acceleration = v2_scale(body->force, body->inv_mass);
            body->velocity = v2_add(body->velocity, v2_scale(acceleration, PHYSICS_2D_TIMESTEP));
            
            // Clamp velocity
            f32 speed = v2_length(body->velocity);
            if (speed > PHYSICS_2D_MAX_VELOCITY) {
                body->velocity = v2_scale(v2_normalize(body->velocity), PHYSICS_2D_MAX_VELOCITY);
            }
            
            // Angular velocity from torque
            f32 angular_acceleration = body->torque * body->inv_inertia;
            body->angular_velocity += angular_acceleration * PHYSICS_2D_TIMESTEP;
            
            // Clear forces for next frame
            body->force = V2(0, 0);
            body->torque = 0;
        }
        
        // Integrate positions
        for (u32 i = 0; i < world->max_bodies; i++) {
            RigidBody2D* body = &world->bodies[i];
            if (!body->active || body->type == BODY_TYPE_STATIC) continue;
            
            body->position = v2_add(body->position, v2_scale(body->velocity, PHYSICS_2D_TIMESTEP));
            body->rotation += body->angular_velocity * PHYSICS_2D_TIMESTEP;
        }
        
        // Collision detection and response
        DetectCollisions(world);
        ResolveCollisions(world);
        
        world->accumulator -= PHYSICS_2D_TIMESTEP;
    }
}

// ========================================================================
// FORCE AND IMPULSE APPLICATION
// ========================================================================

void Physics2DApplyForce(RigidBody2D* body, v2 force) {
    if (!body || body->type != BODY_TYPE_DYNAMIC) return;
    body->force = v2_add(body->force, force);
}

void Physics2DApplyImpulse(RigidBody2D* body, v2 impulse) {
    if (!body || body->type != BODY_TYPE_DYNAMIC) return;
    body->velocity = v2_add(body->velocity, v2_scale(impulse, body->inv_mass));
}

void Physics2DApplyTorque(RigidBody2D* body, f32 torque) {
    if (!body || body->type != BODY_TYPE_DYNAMIC) return;
    body->torque += torque;
}

void Physics2DSetVelocity(RigidBody2D* body, v2 velocity) {
    if (!body) return;
    body->velocity = velocity;
}

// ========================================================================
// WORLD SETTINGS
// ========================================================================

void Physics2DSetGravity(Physics2DWorld* world, v2 gravity) {
    if (!world) return;
    world->gravity = gravity;
}

void Physics2DSetAirFriction(Physics2DWorld* world, f32 friction) {
    if (!world) return;
    world->air_friction = friction;
}

// ========================================================================
// DEBUG RENDERING
// ========================================================================

void Physics2DDebugDrawBody(RigidBody2D* body, Renderer* renderer) {
    if (!body || !body->active || !renderer) return;
    
    if (body->shape.type == SHAPE_2D_CIRCLE) {
        RendererDrawCircle(renderer, body->position, body->shape.circle.radius, body->color, 32);
        
        // Draw rotation indicator
        v2 dir = v2_rotate(V2(body->shape.circle.radius, 0), body->rotation);
        v2 end = v2_add(body->position, dir);
        RendererDrawLine(renderer, body->position, end, 0.02f, COLOR_WHITE);
    } else if (body->shape.type == SHAPE_2D_BOX) {
        // Draw rotated box
        v2 corners[4];
        v2 half = body->shape.box.half_extents;
        
        // Calculate corners in local space
        corners[0] = V2(-half.x, -half.y);
        corners[1] = V2( half.x, -half.y);
        corners[2] = V2( half.x,  half.y);
        corners[3] = V2(-half.x,  half.y);
        
        // Rotate and translate to world space
        for (int i = 0; i < 4; i++) {
            corners[i] = v2_rotate(corners[i], body->rotation);
            corners[i] = v2_add(corners[i], body->position);
        }
        
        // Draw lines between corners
        for (int i = 0; i < 4; i++) {
            int next = (i + 1) % 4;
            RendererDrawLine(renderer, corners[i], corners[next], 0.02f, body->color);
        }
    }
}

void Physics2DDebugDraw(Physics2DWorld* world, Renderer* renderer) {
    if (!world || !world->initialized || !world->debug_draw_enabled || !renderer) return;
    
    // Draw all bodies
    for (u32 i = 0; i < world->max_bodies; i++) {
        RigidBody2D* body = &world->bodies[i];
        if (!body->active) continue;
        
        Physics2DDebugDrawBody(body, renderer);
        
        // Draw AABB if enabled
        if (world->debug_draw_aabb) {
            v2 size = v2_sub(body->aabb.max, body->aabb.min);
            v2 center = v2_add(body->aabb.min, v2_scale(size, 0.5f));
            RendererDrawRectOutline(renderer, center, v2_scale(size, 0.5f), 0.01f, 
                                    COLOR(0.5f, 0.5f, 0.5f, 0.5f));
        }
        
        // Draw velocity vector if enabled
        if (world->debug_draw_velocities && body->type == BODY_TYPE_DYNAMIC) {
            v2 vel_end = v2_add(body->position, v2_scale(body->velocity, 0.1f));
            RendererDrawLine(renderer, body->position, vel_end, 0.015f, COLOR_GREEN);
        }
    }
    
    // Draw contact points if enabled
    if (world->debug_draw_contacts) {
        for (u32 i = 0; i < world->contact_count; i++) {
            Contact2D* contact = &world->contacts[i];
            
            // Draw contact point
            RendererDrawCircle(renderer, contact->point, 0.05f, COLOR_RED, 16);
            
            // Draw contact normal
            v2 normal_end = v2_add(contact->point, v2_scale(contact->normal, 0.3f));
            RendererDrawLine(renderer, contact->point, normal_end, 0.02f, COLOR_YELLOW);
        }
    }
}