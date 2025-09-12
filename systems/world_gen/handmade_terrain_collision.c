/*
    Terrain Collision and Raycasting Implementation
    Fast height queries and ray-terrain intersection
*/

#include "handmade_terrain.h"
#include <math.h>
#include <float.h>

// =============================================================================
// HEIGHT QUERIES
// =============================================================================

f32 terrain_get_height_interpolated(terrain_system* terrain, f32 world_x, f32 world_z) {
    // Get the four surrounding height samples
    f32 sample_size = terrain->params.horizontal_scale;
    
    f32 x0 = floorf(world_x / sample_size) * sample_size;
    f32 x1 = x0 + sample_size;
    f32 z0 = floorf(world_z / sample_size) * sample_size;
    f32 z1 = z0 + sample_size;
    
    // Sample heights at corners
    f32 h00 = terrain_sample_height(terrain, x0, z0);
    f32 h10 = terrain_sample_height(terrain, x1, z0);
    f32 h01 = terrain_sample_height(terrain, x0, z1);
    f32 h11 = terrain_sample_height(terrain, x1, z1);
    
    // Bilinear interpolation
    f32 fx = (world_x - x0) / sample_size;
    f32 fz = (world_z - z0) / sample_size;
    
    f32 h0 = h00 * (1.0f - fx) + h10 * fx;
    f32 h1 = h01 * (1.0f - fx) + h11 * fx;
    
    return h0 * (1.0f - fz) + h1 * fz;
}

// Get terrain normal at world position
v3 terrain_get_normal(terrain_system* terrain, f32 world_x, f32 world_z) {
    // Sample heights around the point
    f32 delta = terrain->params.horizontal_scale;
    
    f32 h_center = terrain_get_height_interpolated(terrain, world_x, world_z);
    f32 h_right = terrain_get_height_interpolated(terrain, world_x + delta, world_z);
    f32 h_forward = terrain_get_height_interpolated(terrain, world_x, world_z + delta);
    
    // Calculate normal from height differences
    v3 right = {delta, h_right - h_center, 0};
    v3 forward = {0, h_forward - h_center, delta};
    
    // Cross product to get normal
    v3 normal = {
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x
    };
    
    // Normalize
    f32 len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > 0.0001f) {
        normal.x /= len;
        normal.y /= len;
        normal.z /= len;
    } else {
        normal = (v3){0, 1, 0};
    }
    
    return normal;
}

// =============================================================================
// SPHERE COLLISION
// =============================================================================

b32 terrain_sphere_collision(terrain_system* terrain, v3 sphere_pos, f32 radius,
                            v3* out_position, v3* out_normal) {
    // Get terrain height at sphere position
    f32 terrain_height = terrain_get_height_interpolated(terrain, sphere_pos.x, sphere_pos.z);
    
    // Check if sphere intersects terrain
    f32 sphere_bottom = sphere_pos.y - radius;
    
    if (sphere_bottom <= terrain_height) {
        // Calculate penetration depth
        f32 penetration = terrain_height - sphere_bottom;
        
        // Get terrain normal at contact point
        v3 normal = terrain_get_normal(terrain, sphere_pos.x, sphere_pos.z);
        
        // Adjust sphere position to resolve collision
        if (out_position) {
            out_position->x = sphere_pos.x;
            out_position->y = terrain_height + radius;
            out_position->z = sphere_pos.z;
        }
        
        if (out_normal) {
            *out_normal = normal;
        }
        
        return 1;
    }
    
    return 0;
}

// =============================================================================
// CAPSULE COLLISION
// =============================================================================

b32 terrain_capsule_collision(terrain_system* terrain, v3 bottom, v3 top, f32 radius,
                             v3* out_position, v3* out_normal) {
    // Sample along the capsule axis
    const u32 SAMPLES = 5;
    b32 collision = 0;
    v3 deepest_point = bottom;
    f32 max_penetration = 0;
    v3 collision_normal = {0, 1, 0};
    
    for (u32 i = 0; i <= SAMPLES; i++) {
        f32 t = (f32)i / SAMPLES;
        v3 sample_pos = {
            bottom.x + (top.x - bottom.x) * t,
            bottom.y + (top.y - bottom.y) * t,
            bottom.z + (top.z - bottom.z) * t
        };
        
        f32 terrain_height = terrain_get_height_interpolated(terrain, sample_pos.x, sample_pos.z);
        f32 penetration = terrain_height - (sample_pos.y - radius);
        
        if (penetration > max_penetration) {
            max_penetration = penetration;
            deepest_point = sample_pos;
            collision = 1;
        }
    }
    
    if (collision) {
        // Get normal at deepest penetration point
        collision_normal = terrain_get_normal(terrain, deepest_point.x, deepest_point.z);
        
        if (out_position) {
            // Move entire capsule up by penetration amount
            out_position->x = bottom.x;
            out_position->y = bottom.y + max_penetration;
            out_position->z = bottom.z;
        }
        
        if (out_normal) {
            *out_normal = collision_normal;
        }
    }
    
    return collision;
}

// =============================================================================
// RAY-TERRAIN INTERSECTION
// =============================================================================

b32 terrain_raycast(terrain_system* terrain, v3 origin, v3 direction,
                   f32 max_distance, v3* hit_point, v3* hit_normal) {
    // Normalize direction
    f32 dir_len = sqrtf(direction.x * direction.x + 
                       direction.y * direction.y + 
                       direction.z * direction.z);
    if (dir_len < 0.0001f) return 0;
    
    v3 dir = {
        direction.x / dir_len,
        direction.y / dir_len,
        direction.z / dir_len
    };
    
    // Ray marching parameters
    f32 step_size = terrain->params.horizontal_scale * 0.5f;
    f32 distance = 0;
    v3 current_pos = origin;
    
    // Early out if ray is going up and starts above terrain
    if (dir.y > 0) {
        f32 terrain_height = terrain_get_height_interpolated(terrain, origin.x, origin.z);
        if (origin.y > terrain_height + 100.0f) {
            return 0;  // Ray going up from high above terrain
        }
    }
    
    // March along ray
    while (distance < max_distance) {
        f32 terrain_height = terrain_get_height_interpolated(terrain, 
                                                            current_pos.x, 
                                                            current_pos.z);
        
        // Check for intersection
        if (current_pos.y <= terrain_height) {
            // Binary search for exact intersection point
            v3 hit_pos = current_pos;
            v3 prev_pos = {
                current_pos.x - dir.x * step_size,
                current_pos.y - dir.y * step_size,
                current_pos.z - dir.z * step_size
            };
            
            // Refine hit position
            for (u32 i = 0; i < 8; i++) {
                v3 mid_pos = {
                    (hit_pos.x + prev_pos.x) * 0.5f,
                    (hit_pos.y + prev_pos.y) * 0.5f,
                    (hit_pos.z + prev_pos.z) * 0.5f
                };
                
                f32 mid_height = terrain_get_height_interpolated(terrain, mid_pos.x, mid_pos.z);
                
                if (mid_pos.y <= mid_height) {
                    hit_pos = mid_pos;
                } else {
                    prev_pos = mid_pos;
                }
            }
            
            if (hit_point) {
                *hit_point = hit_pos;
            }
            
            if (hit_normal) {
                *hit_normal = terrain_get_normal(terrain, hit_pos.x, hit_pos.z);
            }
            
            return 1;
        }
        
        // Adaptive step size based on distance to terrain
        f32 height_diff = current_pos.y - terrain_height;
        f32 adaptive_step = fminf(step_size * (1.0f + height_diff * 0.1f), step_size * 10.0f);
        
        // March forward
        current_pos.x += dir.x * adaptive_step;
        current_pos.y += dir.y * adaptive_step;
        current_pos.z += dir.z * adaptive_step;
        distance += adaptive_step;
    }
    
    return 0;
}

// =============================================================================
// BOX COLLISION
// =============================================================================

b32 terrain_box_collision(terrain_system* terrain, v3 box_min, v3 box_max,
                         v3* out_position, v3* out_normal) {
    // Sample terrain heights under the box
    const u32 SAMPLES_X = 4;
    const u32 SAMPLES_Z = 4;
    
    f32 max_terrain_height = -FLT_MAX;
    v3 highest_point = {0, 0, 0};
    
    for (u32 z = 0; z < SAMPLES_Z; z++) {
        for (u32 x = 0; x < SAMPLES_X; x++) {
            f32 fx = (f32)x / (SAMPLES_X - 1);
            f32 fz = (f32)z / (SAMPLES_Z - 1);
            
            f32 sample_x = box_min.x + (box_max.x - box_min.x) * fx;
            f32 sample_z = box_min.z + (box_max.z - box_min.z) * fz;
            
            f32 terrain_height = terrain_get_height_interpolated(terrain, sample_x, sample_z);
            
            if (terrain_height > max_terrain_height) {
                max_terrain_height = terrain_height;
                highest_point.x = sample_x;
                highest_point.z = sample_z;
            }
        }
    }
    
    // Check for collision
    if (box_min.y <= max_terrain_height) {
        f32 penetration = max_terrain_height - box_min.y;
        
        if (out_position) {
            // Move box up to resolve collision
            out_position->x = box_min.x;
            out_position->y = box_min.y + penetration;
            out_position->z = box_min.z;
        }
        
        if (out_normal) {
            *out_normal = terrain_get_normal(terrain, highest_point.x, highest_point.z);
        }
        
        return 1;
    }
    
    return 0;
}

// =============================================================================
// TERRAIN LINE OF SIGHT
// =============================================================================

b32 terrain_line_of_sight(terrain_system* terrain, v3 from, v3 to) {
    v3 direction = {
        to.x - from.x,
        to.y - from.y,
        to.z - from.z
    };
    
    f32 distance = sqrtf(direction.x * direction.x + 
                        direction.y * direction.y + 
                        direction.z * direction.z);
    
    // Use raycast to check for terrain obstruction
    v3 hit_point, hit_normal;
    if (terrain_raycast(terrain, from, direction, distance, &hit_point, &hit_normal)) {
        // Check if hit is before the target
        f32 hit_dist_sq = (hit_point.x - from.x) * (hit_point.x - from.x) +
                         (hit_point.y - from.y) * (hit_point.y - from.y) +
                         (hit_point.z - from.z) * (hit_point.z - from.z);
        
        f32 target_dist_sq = distance * distance;
        
        // Allow small tolerance for floating point errors
        return (hit_dist_sq >= target_dist_sq - 0.01f);
    }
    
    return 1;  // No terrain obstruction
}

// =============================================================================
// TERRAIN SLOPE QUERY
// =============================================================================

f32 terrain_get_slope(terrain_system* terrain, f32 world_x, f32 world_z) {
    v3 normal = terrain_get_normal(terrain, world_x, world_z);
    
    // Slope is the angle from vertical
    // Dot product with up vector gives cosine of angle
    f32 cos_angle = normal.y;  // Assuming Y is up
    
    // Convert to degrees
    f32 angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, cos_angle)));
    f32 angle_deg = angle_rad * (180.0f / 3.14159265f);
    
    return angle_deg;
}

// Check if position is walkable based on slope
b32 terrain_is_walkable(terrain_system* terrain, f32 world_x, f32 world_z, f32 max_slope_degrees) {
    f32 slope = terrain_get_slope(terrain, world_x, world_z);
    return slope <= max_slope_degrees;
}

// =============================================================================
// TERRAIN WATER LEVEL
// =============================================================================

b32 terrain_is_underwater(terrain_system* terrain, v3 position) {
    return position.y < terrain->params.sea_level;
}

f32 terrain_water_depth(terrain_system* terrain, v3 position) {
    if (position.y < terrain->params.sea_level) {
        return terrain->params.sea_level - position.y;
    }
    return 0.0f;
}