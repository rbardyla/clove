#ifndef HANDMADE_OCTREE_H
#define HANDMADE_OCTREE_H

#include "handmade_memory.h"
#include "handmade_entity_soa.h"
#include <float.h>

// Octree configuration
#define OCTREE_MAX_DEPTH 8
#define OCTREE_MAX_ENTITIES_PER_NODE 16
#define OCTREE_MIN_NODE_SIZE 1.0f

// AABB for spatial queries
typedef struct aabb {
    v3 min;
    v3 max;
} aabb;

// Octree node
typedef struct octree_node {
    aabb bounds;
    
    // Entity storage
    u32* entity_indices;
    u32 entity_count;
    u32 entity_capacity;
    
    // Children (8 for octree)
    struct octree_node* children[8];
    struct octree_node* parent;
    
    // Node properties
    u32 depth;
    u32 child_mask;  // Bitmask indicating which children exist
    u32 node_id;     // Unique identifier for debugging
} octree_node;

// Octree structure
typedef struct octree {
    octree_node* root;
    arena* node_arena;
    
    // Statistics
    u32 total_nodes;
    u32 total_entities;
    u32 max_depth_reached;
    u32 rebalance_count;
    
    // Configuration
    u32 max_depth;
    u32 max_entities_per_node;
    f32 min_node_size;
} octree;

// Ray for raycasting
typedef struct ray {
    v3 origin;
    v3 direction;
    f32 max_distance;
} ray;

// Frustum for culling
typedef struct frustum {
    // 6 planes: left, right, top, bottom, near, far
    struct {
        v3 normal;
        f32 d;
    } planes[6];
} frustum;

// Query results
typedef struct spatial_query_result {
    u32* entity_indices;
    f32* distances;  // For sorted results
    u32 count;
    u32 capacity;
} spatial_query_result;

// AABB operations
static inline aabb aabb_create(v3 min, v3 max) {
    return (aabb){min, max};
}

static inline v3 aabb_center(const aabb* box) {
    return (v3){
        (box->min.x + box->max.x) * 0.5f,
        (box->min.y + box->max.y) * 0.5f,
        (box->min.z + box->max.z) * 0.5f
    };
}

static inline v3 aabb_size(const aabb* box) {
    return (v3){
        box->max.x - box->min.x,
        box->max.y - box->min.y,
        box->max.z - box->min.z
    };
}

static inline int aabb_contains_point(const aabb* box, v3 point) {
    return point.x >= box->min.x && point.x <= box->max.x &&
           point.y >= box->min.y && point.y <= box->max.y &&
           point.z >= box->min.z && point.z <= box->max.z;
}

static inline int aabb_intersects_aabb(const aabb* a, const aabb* b) {
    return a->min.x <= b->max.x && a->max.x >= b->min.x &&
           a->min.y <= b->max.y && a->max.y >= b->min.y &&
           a->min.z <= b->max.z && a->max.z >= b->min.z;
}

static inline void aabb_expand(aabb* box, v3 point) {
    box->min.x = fminf(box->min.x, point.x);
    box->min.y = fminf(box->min.y, point.y);
    box->min.z = fminf(box->min.z, point.z);
    box->max.x = fmaxf(box->max.x, point.x);
    box->max.y = fmaxf(box->max.y, point.y);
    box->max.z = fmaxf(box->max.z, point.z);
}

static inline void aabb_expand_by_aabb(aabb* box, const aabb* other) {
    box->min.x = fminf(box->min.x, other->min.x);
    box->min.y = fminf(box->min.y, other->min.y);
    box->min.z = fminf(box->min.z, other->min.z);
    box->max.x = fmaxf(box->max.x, other->max.x);
    box->max.y = fmaxf(box->max.y, other->max.y);
    box->max.z = fmaxf(box->max.z, other->max.z);
}

// Ray-AABB intersection (slab method)
static int ray_intersects_aabb(const ray* r, const aabb* box, f32* t_min_out, f32* t_max_out) {
    f32 t_min = 0.0f;
    f32 t_max = r->max_distance;
    
    // X axis
    if (fabsf(r->direction.x) > 0.0001f) {
        f32 inv_d = 1.0f / r->direction.x;
        f32 t1 = (box->min.x - r->origin.x) * inv_d;
        f32 t2 = (box->max.x - r->origin.x) * inv_d;
        
        if (t1 > t2) {
            f32 temp = t1; t1 = t2; t2 = temp;
        }
        
        t_min = fmaxf(t_min, t1);
        t_max = fminf(t_max, t2);
        
        if (t_min > t_max) return 0;
    } else if (r->origin.x < box->min.x || r->origin.x > box->max.x) {
        return 0;
    }
    
    // Y axis
    if (fabsf(r->direction.y) > 0.0001f) {
        f32 inv_d = 1.0f / r->direction.y;
        f32 t1 = (box->min.y - r->origin.y) * inv_d;
        f32 t2 = (box->max.y - r->origin.y) * inv_d;
        
        if (t1 > t2) {
            f32 temp = t1; t1 = t2; t2 = temp;
        }
        
        t_min = fmaxf(t_min, t1);
        t_max = fminf(t_max, t2);
        
        if (t_min > t_max) return 0;
    } else if (r->origin.y < box->min.y || r->origin.y > box->max.y) {
        return 0;
    }
    
    // Z axis
    if (fabsf(r->direction.z) > 0.0001f) {
        f32 inv_d = 1.0f / r->direction.z;
        f32 t1 = (box->min.z - r->origin.z) * inv_d;
        f32 t2 = (box->max.z - r->origin.z) * inv_d;
        
        if (t1 > t2) {
            f32 temp = t1; t1 = t2; t2 = temp;
        }
        
        t_min = fmaxf(t_min, t1);
        t_max = fminf(t_max, t2);
        
        if (t_min > t_max) return 0;
    } else if (r->origin.z < box->min.z || r->origin.z > box->max.z) {
        return 0;
    }
    
    if (t_min_out) *t_min_out = t_min;
    if (t_max_out) *t_max_out = t_max;
    
    return 1;
}

// Frustum-AABB intersection
static int frustum_intersects_aabb(const frustum* f, const aabb* box) {
    v3 center = aabb_center(box);
    v3 half_size = {
        (box->max.x - box->min.x) * 0.5f,
        (box->max.y - box->min.y) * 0.5f,
        (box->max.z - box->min.z) * 0.5f
    };
    
    // Test against each frustum plane
    for (int i = 0; i < 6; i++) {
        f32 dist = f->planes[i].normal.x * center.x +
                  f->planes[i].normal.y * center.y +
                  f->planes[i].normal.z * center.z + f->planes[i].d;
        
        f32 radius = fabsf(f->planes[i].normal.x * half_size.x) +
                    fabsf(f->planes[i].normal.y * half_size.y) +
                    fabsf(f->planes[i].normal.z * half_size.z);
        
        if (dist < -radius) {
            return 0; // Completely outside this plane
        }
    }
    
    return 1; // Inside or intersecting all planes
}

// Initialize octree
static octree* octree_init(arena* a, aabb world_bounds) {
    octree* tree = arena_alloc(a, sizeof(octree));
    tree->node_arena = a;
    tree->max_depth = OCTREE_MAX_DEPTH;
    tree->max_entities_per_node = OCTREE_MAX_ENTITIES_PER_NODE;
    tree->min_node_size = OCTREE_MIN_NODE_SIZE;
    
    // Create root node
    tree->root = arena_alloc(a, sizeof(octree_node));
    tree->root->bounds = world_bounds;
    tree->root->entity_capacity = OCTREE_MAX_ENTITIES_PER_NODE;
    tree->root->entity_indices = arena_alloc_array(a, u32, tree->root->entity_capacity);
    tree->root->depth = 0;
    tree->root->node_id = tree->total_nodes++;
    
    return tree;
}

// Get child index for a point (0-7)
static u32 octree_get_child_index(const octree_node* node, v3 point) {
    v3 center = aabb_center(&node->bounds);
    u32 index = 0;
    
    if (point.x > center.x) index |= 1;
    if (point.y > center.y) index |= 2;
    if (point.z > center.z) index |= 4;
    
    return index;
}

// Calculate child bounds
static aabb octree_child_bounds(const octree_node* node, u32 child_index) {
    v3 center = aabb_center(&node->bounds);
    aabb child_bounds;
    
    child_bounds.min.x = (child_index & 1) ? center.x : node->bounds.min.x;
    child_bounds.max.x = (child_index & 1) ? node->bounds.max.x : center.x;
    
    child_bounds.min.y = (child_index & 2) ? center.y : node->bounds.min.y;
    child_bounds.max.y = (child_index & 2) ? node->bounds.max.y : center.y;
    
    child_bounds.min.z = (child_index & 4) ? center.z : node->bounds.min.z;
    child_bounds.max.z = (child_index & 4) ? node->bounds.max.z : center.z;
    
    return child_bounds;
}

// Split node into 8 children
static void octree_split_node(octree* tree, octree_node* node) {
    if (node->depth >= tree->max_depth) return;
    
    v3 size = aabb_size(&node->bounds);
    if (size.x < tree->min_node_size || 
        size.y < tree->min_node_size || 
        size.z < tree->min_node_size) {
        return;
    }
    
    // Create children
    for (u32 i = 0; i < 8; i++) {
        octree_node* child = arena_alloc(tree->node_arena, sizeof(octree_node));
        child->bounds = octree_child_bounds(node, i);
        child->parent = node;
        child->depth = node->depth + 1;
        child->entity_capacity = tree->max_entities_per_node;
        child->entity_indices = arena_alloc_array(tree->node_arena, u32, child->entity_capacity);
        child->node_id = tree->total_nodes++;
        
        node->children[i] = child;
        node->child_mask |= (1 << i);
    }
    
    // Redistribute entities to children
    for (u32 i = 0; i < node->entity_count; i++) {
        u32 entity_idx = node->entity_indices[i];
        // In production, get entity position from transform system
        v3 pos = {0, 0, 0}; // Placeholder
        
        u32 child_idx = octree_get_child_index(node, pos);
        octree_node* child = node->children[child_idx];
        
        if (child->entity_count < child->entity_capacity) {
            child->entity_indices[child->entity_count++] = entity_idx;
        }
    }
    
    // Clear parent's entity list
    node->entity_count = 0;
    
    if (node->depth + 1 > tree->max_depth_reached) {
        tree->max_depth_reached = node->depth + 1;
    }
}

// Insert entity into octree
static void octree_insert(octree* tree, u32 entity_index, v3 position, aabb entity_bounds) {
    octree_node* node = tree->root;
    
    // Traverse to leaf node
    while (node->child_mask != 0) {
        u32 child_idx = octree_get_child_index(node, position);
        
        // Check if entity fits entirely in child
        octree_node* child = node->children[child_idx];
        if (aabb_intersects_aabb(&entity_bounds, &child->bounds)) {
            node = child;
        } else {
            break; // Entity spans multiple children, insert here
        }
    }
    
    // Insert entity
    if (node->entity_count < node->entity_capacity) {
        node->entity_indices[node->entity_count++] = entity_index;
        tree->total_entities++;
    } else {
        // Node is full, split if possible
        if (node->child_mask == 0 && node->depth < tree->max_depth) {
            octree_split_node(tree, node);
            // Retry insertion
            octree_insert(tree, entity_index, position, entity_bounds);
        } else {
            // Expand capacity
            u32 new_capacity = node->entity_capacity * 2;
            u32* new_indices = arena_alloc_array(tree->node_arena, u32, new_capacity);
            
            for (u32 i = 0; i < node->entity_count; i++) {
                new_indices[i] = node->entity_indices[i];
            }
            
            node->entity_indices = new_indices;
            node->entity_capacity = new_capacity;
            node->entity_indices[node->entity_count++] = entity_index;
            tree->total_entities++;
        }
    }
}

// Remove entity from octree
static void octree_remove(octree* tree, u32 entity_index, v3 position) {
    octree_node* node = tree->root;
    
    // Find node containing entity
    while (node->child_mask != 0) {
        u32 child_idx = octree_get_child_index(node, position);
        octree_node* child = node->children[child_idx];
        
        // Check if child contains entity
        int found = 0;
        for (u32 i = 0; i < child->entity_count; i++) {
            if (child->entity_indices[i] == entity_index) {
                found = 1;
                break;
            }
        }
        
        if (found) {
            node = child;
            break;
        } else {
            // Check current node
            break;
        }
    }
    
    // Remove entity from node
    for (u32 i = 0; i < node->entity_count; i++) {
        if (node->entity_indices[i] == entity_index) {
            // Swap with last and decrease count
            node->entity_indices[i] = node->entity_indices[--node->entity_count];
            tree->total_entities--;
            break;
        }
    }
}

// Query entities in AABB
static void octree_query_aabb_recursive(octree_node* node, const aabb* query_box,
                                        spatial_query_result* result) {
    if (!aabb_intersects_aabb(&node->bounds, query_box)) {
        return;
    }
    
    // Add entities from this node
    for (u32 i = 0; i < node->entity_count; i++) {
        if (result->count < result->capacity) {
            result->entity_indices[result->count++] = node->entity_indices[i];
        }
    }
    
    // Recurse to children
    if (node->child_mask != 0) {
        for (u32 i = 0; i < 8; i++) {
            if (node->child_mask & (1 << i)) {
                octree_query_aabb_recursive(node->children[i], query_box, result);
            }
        }
    }
}

static spatial_query_result octree_query_aabb(octree* tree, arena* temp_arena, const aabb* query_box) {
    spatial_query_result result = {0};
    result.capacity = 1024; // Initial capacity
    result.entity_indices = arena_alloc_array(temp_arena, u32, result.capacity);
    
    octree_query_aabb_recursive(tree->root, query_box, &result);
    
    return result;
}

// Query entities in sphere
static spatial_query_result octree_query_sphere(octree* tree, arena* temp_arena, 
                                               v3 center, f32 radius) {
    aabb sphere_box = {
        {center.x - radius, center.y - radius, center.z - radius},
        {center.x + radius, center.y + radius, center.z + radius}
    };
    
    spatial_query_result aabb_result = octree_query_aabb(tree, temp_arena, &sphere_box);
    
    // Filter to actual sphere
    spatial_query_result result = {0};
    result.capacity = aabb_result.count;
    result.entity_indices = arena_alloc_array(temp_arena, u32, result.capacity);
    result.distances = arena_alloc_array(temp_arena, f32, result.capacity);
    
    for (u32 i = 0; i < aabb_result.count; i++) {
        // In production, get actual entity position
        v3 entity_pos = {0, 0, 0}; // Placeholder
        
        f32 dx = entity_pos.x - center.x;
        f32 dy = entity_pos.y - center.y;
        f32 dz = entity_pos.z - center.z;
        f32 dist_sq = dx*dx + dy*dy + dz*dz;
        
        if (dist_sq <= radius * radius) {
            result.entity_indices[result.count] = aabb_result.entity_indices[i];
            result.distances[result.count] = sqrtf(dist_sq);
            result.count++;
        }
    }
    
    return result;
}

// Raycast query
static void octree_raycast_recursive(octree_node* node, const ray* r,
                                     spatial_query_result* result) {
    f32 t_min, t_max;
    if (!ray_intersects_aabb(r, &node->bounds, &t_min, &t_max)) {
        return;
    }
    
    // Check entities in this node
    for (u32 i = 0; i < node->entity_count; i++) {
        if (result->count < result->capacity) {
            // In production, perform actual ray-entity intersection
            result->entity_indices[result->count] = node->entity_indices[i];
            result->distances[result->count] = t_min; // Placeholder distance
            result->count++;
        }
    }
    
    // Recurse to children in front-to-back order
    if (node->child_mask != 0) {
        // Sort children by distance for early termination
        struct {
            u32 index;
            f32 distance;
        } child_order[8];
        u32 child_count = 0;
        
        for (u32 i = 0; i < 8; i++) {
            if (node->child_mask & (1 << i)) {
                aabb child_bounds = octree_child_bounds(node, i);
                f32 child_t_min;
                if (ray_intersects_aabb(r, &child_bounds, &child_t_min, NULL)) {
                    child_order[child_count].index = i;
                    child_order[child_count].distance = child_t_min;
                    child_count++;
                }
            }
        }
        
        // Simple insertion sort by distance
        for (u32 i = 1; i < child_count; i++) {
            u32 j = i;
            while (j > 0 && child_order[j].distance < child_order[j-1].distance) {
                u32 temp_index = child_order[j].index;
                f32 temp_distance = child_order[j].distance;
                child_order[j] = child_order[j-1];
                child_order[j-1].index = temp_index;
                child_order[j-1].distance = temp_distance;
                j--;
            }
        }
        
        // Recurse in sorted order
        for (u32 i = 0; i < child_count; i++) {
            octree_raycast_recursive(node->children[child_order[i].index], r, result);
        }
    }
}

static spatial_query_result octree_raycast(octree* tree, arena* temp_arena, const ray* r) {
    spatial_query_result result = {0};
    result.capacity = 256;
    result.entity_indices = arena_alloc_array(temp_arena, u32, result.capacity);
    result.distances = arena_alloc_array(temp_arena, f32, result.capacity);
    
    octree_raycast_recursive(tree->root, r, &result);
    
    // Sort results by distance
    for (u32 i = 1; i < result.count; i++) {
        u32 j = i;
        while (j > 0 && result.distances[j] < result.distances[j-1]) {
            // Swap
            f32 temp_dist = result.distances[j];
            u32 temp_idx = result.entity_indices[j];
            result.distances[j] = result.distances[j-1];
            result.entity_indices[j] = result.entity_indices[j-1];
            result.distances[j-1] = temp_dist;
            result.entity_indices[j-1] = temp_idx;
            j--;
        }
    }
    
    return result;
}

// Frustum culling
static void octree_frustum_cull_recursive(octree_node* node, const frustum* f,
                                          spatial_query_result* result) {
    if (!frustum_intersects_aabb(f, &node->bounds)) {
        return;
    }
    
    // Add all entities if node is entirely inside frustum
    // (Optimization: check if node is entirely inside all planes)
    
    // Add entities from this node
    for (u32 i = 0; i < node->entity_count; i++) {
        if (result->count < result->capacity) {
            result->entity_indices[result->count++] = node->entity_indices[i];
        }
    }
    
    // Recurse to children
    if (node->child_mask != 0) {
        for (u32 i = 0; i < 8; i++) {
            if (node->child_mask & (1 << i)) {
                octree_frustum_cull_recursive(node->children[i], f, result);
            }
        }
    }
}

static spatial_query_result octree_frustum_cull(octree* tree, arena* temp_arena, const frustum* f) {
    spatial_query_result result = {0};
    result.capacity = 2048;
    result.entity_indices = arena_alloc_array(temp_arena, u32, result.capacity);
    
    octree_frustum_cull_recursive(tree->root, f, &result);
    
    return result;
}

// Update entity position (remove and reinsert)
static void octree_update_entity(octree* tree, u32 entity_index, 
                                 v3 old_pos, v3 new_pos, aabb new_bounds) {
    octree_remove(tree, entity_index, old_pos);
    octree_insert(tree, entity_index, new_pos, new_bounds);
}

// Debug visualization
static void octree_debug_draw_recursive(octree_node* node, u32 max_depth) {
    if (node->depth > max_depth) return;
    
    // Draw node bounds
    printf("Node %u: depth=%u, entities=%u, bounds=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
           node->node_id, node->depth, node->entity_count,
           node->bounds.min.x, node->bounds.min.y, node->bounds.min.z,
           node->bounds.max.x, node->bounds.max.y, node->bounds.max.z);
    
    // Recurse to children
    if (node->child_mask != 0) {
        for (u32 i = 0; i < 8; i++) {
            if (node->child_mask & (1 << i)) {
                octree_debug_draw_recursive(node->children[i], max_depth);
            }
        }
    }
}

static void octree_print_stats(octree* tree) {
    printf("=== Octree Statistics ===\n");
    printf("Total Nodes: %u\n", tree->total_nodes);
    printf("Total Entities: %u\n", tree->total_entities);
    printf("Max Depth Reached: %u / %u\n", tree->max_depth_reached, tree->max_depth);
    printf("Rebalance Count: %u\n", tree->rebalance_count);
    
    // Calculate memory usage
    u64 node_memory = tree->total_nodes * sizeof(octree_node);
    u64 index_memory = tree->total_nodes * tree->max_entities_per_node * sizeof(u32);
    printf("Memory Usage: %.2f MB\n", (node_memory + index_memory) / (1024.0 * 1024.0));
}

#endif // HANDMADE_OCTREE_H