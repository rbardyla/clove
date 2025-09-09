/*
 * Network State Synchronization
 * Authoritative server with client-side prediction
 * Priority-based updates and area of interest management
 * 
 * PERFORMANCE: Spatial hashing for AOI, bit packing for states
 * CACHE: Entity data laid out for sequential processing
 */

#include "handmade_network.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// Entity types for replication
typedef enum {
    ENTITY_PLAYER = 0,
    ENTITY_PROJECTILE,
    ENTITY_PICKUP,
    ENTITY_VEHICLE,
    ENTITY_DOOR,
    ENTITY_TRIGGER,
    ENTITY_MAX_TYPES
} entity_type_t;

// Entity replication priority
typedef enum {
    PRIORITY_CRITICAL = 0,  // Always replicate (players)
    PRIORITY_HIGH = 1,      // Nearby dynamic objects
    PRIORITY_MEDIUM = 2,    // Distant dynamic objects
    PRIORITY_LOW = 3,       // Static objects
    PRIORITY_LEVELS = 4
} entity_priority_t;

// Network entity representation
typedef struct {
    uint32_t id;
    uint32_t owner_id;  // Player who owns this entity
    entity_type_t type;
    entity_priority_t priority;
    
    // Transform
    float x, y, z;
    float vx, vy, vz;
    float yaw, pitch, roll;
    
    // State
    uint32_t state_flags;
    uint32_t health;
    uint32_t ammo;
    
    // Replication metadata
    uint32_t last_replicated_tick;
    uint32_t update_frequency;  // How often to send updates
    float relevance_score;       // For priority sorting
    uint32_t dirty_mask;         // Which fields changed
    
    // Interpolation data for clients
    float interp_x, interp_y, interp_z;
    float interp_yaw, interp_pitch;
    uint64_t interp_timestamp;
} network_entity_t;

// Spatial hash for area of interest
#define SPATIAL_HASH_SIZE 256
#define SPATIAL_CELL_SIZE 100.0f  // 100 units per cell

typedef struct spatial_node {
    uint32_t entity_id;
    struct spatial_node* next;
} spatial_node_t;

typedef struct {
    spatial_node_t* buckets[SPATIAL_HASH_SIZE];
    spatial_node_t node_pool[NET_MAX_PLAYERS * 64];  // Pre-allocated nodes
    uint32_t node_pool_used;
} spatial_hash_t;

// Entity manager
typedef struct {
    network_entity_t entities[NET_MAX_PLAYERS * 64];  // Max entities
    uint32_t entity_count;
    uint32_t next_entity_id;
    
    spatial_hash_t spatial_hash;
    
    // Priority queues for updates
    uint32_t priority_queues[PRIORITY_LEVELS][NET_MAX_PLAYERS * 64];
    uint32_t priority_counts[PRIORITY_LEVELS];
    
    // Delta compression state
    network_entity_t last_sent_state[NET_MAX_PLAYERS][NET_MAX_PLAYERS * 64];
    uint32_t last_sent_tick[NET_MAX_PLAYERS];
} entity_manager_t;

// Global entity manager (normally in context)
static entity_manager_t g_entity_manager;

// Spatial hash functions
static uint32_t spatial_hash_key(float x, float y, float z) {
    int32_t cx = (int32_t)(x / SPATIAL_CELL_SIZE);
    int32_t cy = (int32_t)(y / SPATIAL_CELL_SIZE);
    int32_t cz = (int32_t)(z / SPATIAL_CELL_SIZE);
    
    // Simple hash combining
    uint32_t hash = cx * 73856093;
    hash ^= cy * 19349663;
    hash ^= cz * 83492791;
    
    return hash % SPATIAL_HASH_SIZE;
}

static void spatial_hash_insert(spatial_hash_t* hash, uint32_t entity_id, 
                               float x, float y, float z) {
    uint32_t key = spatial_hash_key(x, y, z);
    
    // Get node from pool
    if (hash->node_pool_used >= sizeof(hash->node_pool) / sizeof(hash->node_pool[0])) {
        return;  // Pool exhausted
    }
    
    spatial_node_t* node = &hash->node_pool[hash->node_pool_used++];
    node->entity_id = entity_id;
    node->next = hash->buckets[key];
    hash->buckets[key] = node;
}

static void spatial_hash_clear(spatial_hash_t* hash) {
    memset(hash->buckets, 0, sizeof(hash->buckets));
    hash->node_pool_used = 0;
}

// Find entities within range
// PERFORMANCE: Only check cells within range
static uint32_t find_entities_in_range(spatial_hash_t* hash,
                                      float x, float y, float z, float range,
                                      uint32_t* entity_ids, uint32_t max_entities) {
    uint32_t count = 0;
    int32_t cell_range = (int32_t)(range / SPATIAL_CELL_SIZE) + 1;
    
    int32_t cx = (int32_t)(x / SPATIAL_CELL_SIZE);
    int32_t cy = (int32_t)(y / SPATIAL_CELL_SIZE);
    int32_t cz = (int32_t)(z / SPATIAL_CELL_SIZE);
    
    // Check all cells within range
    for (int32_t dx = -cell_range; dx <= cell_range && count < max_entities; dx++) {
        for (int32_t dy = -cell_range; dy <= cell_range && count < max_entities; dy++) {
            for (int32_t dz = -cell_range; dz <= cell_range && count < max_entities; dz++) {
                // Calculate hash for this cell
                int32_t tcx = cx + dx;
                int32_t tcy = cy + dy;
                int32_t tcz = cz + dz;
                
                uint32_t hash_val = tcx * 73856093;
                hash_val ^= tcy * 19349663;
                hash_val ^= tcz * 83492791;
                uint32_t key = hash_val % SPATIAL_HASH_SIZE;
                
                // Check all entities in this cell
                spatial_node_t* node = hash->buckets[key];
                while (node && count < max_entities) {
                    // Get entity and check actual distance
                    network_entity_t* entity = &g_entity_manager.entities[node->entity_id];
                    
                    float dx_actual = entity->x - x;
                    float dy_actual = entity->y - y;
                    float dz_actual = entity->z - z;
                    float dist_sq = dx_actual * dx_actual + 
                                  dy_actual * dy_actual + 
                                  dz_actual * dz_actual;
                    
                    if (dist_sq <= range * range) {
                        entity_ids[count++] = node->entity_id;
                    }
                    
                    node = node->next;
                }
            }
        }
    }
    
    return count;
}

// Calculate entity relevance score
// OPTIMIZATION: Branchless score calculation
static float calculate_relevance(network_entity_t* entity, 
                                network_entity_t* viewer) {
    // Distance factor
    float dx = entity->x - viewer->x;
    float dy = entity->y - viewer->y;
    float dz = entity->z - viewer->z;
    float dist_sq = dx * dx + dy * dy + dz * dz;
    float distance_score = 10000.0f / (dist_sq + 1.0f);
    
    // Velocity factor (fast moving objects more important)
    float velocity_sq = entity->vx * entity->vx + 
                       entity->vy * entity->vy + 
                       entity->vz * entity->vz;
    float velocity_score = velocity_sq * 0.1f;
    
    // View direction factor (objects in front more important)
    float view_dir_x = cosf(viewer->yaw * M_PI / 180.0f);
    float view_dir_y = sinf(viewer->yaw * M_PI / 180.0f);
    float dot = (dx * view_dir_x + dy * view_dir_y) / sqrtf(dist_sq + 1.0f);
    float view_score = (dot + 1.0f) * 50.0f;
    
    // Type priority
    float type_score = 0;
    switch (entity->type) {
        case ENTITY_PLAYER: type_score = 1000.0f; break;
        case ENTITY_PROJECTILE: type_score = 500.0f; break;
        case ENTITY_VEHICLE: type_score = 300.0f; break;
        case ENTITY_PICKUP: type_score = 100.0f; break;
        default: type_score = 10.0f; break;
    }
    
    // Time since last update (stale entities need updates)
    uint32_t ticks_since_update = viewer->last_replicated_tick - 
                                 entity->last_replicated_tick;
    float staleness_score = ticks_since_update * 10.0f;
    
    return distance_score + velocity_score + view_score + 
           type_score + staleness_score;
}

// Sort entities by priority
static int entity_priority_compare(const void* a, const void* b) {
    uint32_t id_a = *(uint32_t*)a;
    uint32_t id_b = *(uint32_t*)b;
    
    network_entity_t* entity_a = &g_entity_manager.entities[id_a];
    network_entity_t* entity_b = &g_entity_manager.entities[id_b];
    
    // Higher relevance first
    if (entity_a->relevance_score > entity_b->relevance_score) return -1;
    if (entity_a->relevance_score < entity_b->relevance_score) return 1;
    return 0;
}

// Pack entity update into buffer
// PERFORMANCE: Bit packing for minimal size
static uint32_t pack_entity_update(network_entity_t* entity,
                                  network_entity_t* baseline,
                                  uint8_t* buffer, uint32_t max_size) {
    if (max_size < 64) return 0;  // Need minimum space
    
    uint32_t pos = 0;
    
    // Header: entity ID and dirty mask
    *(uint32_t*)(buffer + pos) = entity->id;
    pos += 4;
    
    // Calculate what changed
    uint32_t dirty_mask = 0;
    
    #define CHECK_DIRTY(field, bit) \
        if (!baseline || entity->field != baseline->field) dirty_mask |= (1 << bit)
    
    CHECK_DIRTY(x, 0);
    CHECK_DIRTY(y, 1);
    CHECK_DIRTY(z, 2);
    CHECK_DIRTY(vx, 3);
    CHECK_DIRTY(vy, 4);
    CHECK_DIRTY(vz, 5);
    CHECK_DIRTY(yaw, 6);
    CHECK_DIRTY(pitch, 7);
    CHECK_DIRTY(state_flags, 8);
    CHECK_DIRTY(health, 9);
    CHECK_DIRTY(ammo, 10);
    
    *(uint32_t*)(buffer + pos) = dirty_mask;
    pos += 4;
    
    // Pack changed fields
    if (dirty_mask & (1 << 0)) {  // Position X
        // Quantize to 16 bits
        uint16_t qx = (uint16_t)((entity->x + 1000.0f) * 32.0f);
        *(uint16_t*)(buffer + pos) = qx;
        pos += 2;
    }
    
    if (dirty_mask & (1 << 1)) {  // Position Y
        uint16_t qy = (uint16_t)((entity->y + 1000.0f) * 32.0f);
        *(uint16_t*)(buffer + pos) = qy;
        pos += 2;
    }
    
    if (dirty_mask & (1 << 2)) {  // Position Z
        uint16_t qz = (uint16_t)((entity->z + 1000.0f) * 32.0f);
        *(uint16_t*)(buffer + pos) = qz;
        pos += 2;
    }
    
    if (dirty_mask & (7 << 3)) {  // Velocity (pack all 3)
        // Quantize to 8 bits each
        *(uint8_t*)(buffer + pos) = (uint8_t)((entity->vx + 50.0f) * 2.55f);
        *(uint8_t*)(buffer + pos + 1) = (uint8_t)((entity->vy + 50.0f) * 2.55f);
        *(uint8_t*)(buffer + pos + 2) = (uint8_t)((entity->vz + 50.0f) * 2.55f);
        pos += 3;
    }
    
    if (dirty_mask & (3 << 6)) {  // Rotation
        uint16_t rot = net_compress_rotation(entity->yaw, entity->pitch);
        *(uint16_t*)(buffer + pos) = rot;
        pos += 2;
    }
    
    if (dirty_mask & (1 << 8)) {  // State flags
        *(uint16_t*)(buffer + pos) = (uint16_t)entity->state_flags;
        pos += 2;
    }
    
    if (dirty_mask & (1 << 9)) {  // Health
        *(uint8_t*)(buffer + pos) = (uint8_t)entity->health;
        pos += 1;
    }
    
    if (dirty_mask & (1 << 10)) {  // Ammo
        *(uint8_t*)(buffer + pos) = (uint8_t)entity->ammo;
        pos += 1;
    }
    
    return pos;
}

// Unpack entity update from buffer
static uint32_t unpack_entity_update(const uint8_t* buffer, uint32_t size,
                                    network_entity_t* entity,
                                    network_entity_t* baseline) {
    if (size < 8) return 0;
    
    uint32_t pos = 0;
    
    // Read header
    uint32_t entity_id = *(uint32_t*)(buffer + pos);
    pos += 4;
    
    uint32_t dirty_mask = *(uint32_t*)(buffer + pos);
    pos += 4;
    
    // Start from baseline if available
    if (baseline && baseline->id == entity_id) {
        *entity = *baseline;
    }
    
    entity->id = entity_id;
    
    // Unpack changed fields
    if (dirty_mask & (1 << 0) && pos + 2 <= size) {
        uint16_t qx = *(uint16_t*)(buffer + pos);
        entity->x = (qx / 32.0f) - 1000.0f;
        pos += 2;
    }
    
    if (dirty_mask & (1 << 1) && pos + 2 <= size) {
        uint16_t qy = *(uint16_t*)(buffer + pos);
        entity->y = (qy / 32.0f) - 1000.0f;
        pos += 2;
    }
    
    if (dirty_mask & (1 << 2) && pos + 2 <= size) {
        uint16_t qz = *(uint16_t*)(buffer + pos);
        entity->z = (qz / 32.0f) - 1000.0f;
        pos += 2;
    }
    
    if (dirty_mask & (7 << 3) && pos + 3 <= size) {
        entity->vx = (*(uint8_t*)(buffer + pos) / 2.55f) - 50.0f;
        entity->vy = (*(uint8_t*)(buffer + pos + 1) / 2.55f) - 50.0f;
        entity->vz = (*(uint8_t*)(buffer + pos + 2) / 2.55f) - 50.0f;
        pos += 3;
    }
    
    if (dirty_mask & (3 << 6) && pos + 2 <= size) {
        uint16_t rot = *(uint16_t*)(buffer + pos);
        net_decompress_rotation(rot, &entity->yaw, &entity->pitch);
        pos += 2;
    }
    
    if (dirty_mask & (1 << 8) && pos + 2 <= size) {
        entity->state_flags = *(uint16_t*)(buffer + pos);
        pos += 2;
    }
    
    if (dirty_mask & (1 << 9) && pos + 1 <= size) {
        entity->health = *(uint8_t*)(buffer + pos);
        pos += 1;
    }
    
    if (dirty_mask & (1 << 10) && pos + 1 <= size) {
        entity->ammo = *(uint8_t*)(buffer + pos);
        pos += 1;
    }
    
    return pos;
}

// Create entity
uint32_t net_create_entity(entity_type_t type, float x, float y, float z) {
    if (g_entity_manager.entity_count >= 
        sizeof(g_entity_manager.entities) / sizeof(g_entity_manager.entities[0])) {
        return UINT32_MAX;  // No space
    }
    
    uint32_t id = g_entity_manager.next_entity_id++;
    uint32_t index = g_entity_manager.entity_count++;
    
    network_entity_t* entity = &g_entity_manager.entities[index];
    memset(entity, 0, sizeof(network_entity_t));
    
    entity->id = id;
    entity->type = type;
    entity->x = x;
    entity->y = y;
    entity->z = z;
    
    // Set default priority based on type
    switch (type) {
        case ENTITY_PLAYER:
            entity->priority = PRIORITY_CRITICAL;
            entity->update_frequency = 1;  // Every tick
            entity->health = 100;
            break;
        case ENTITY_PROJECTILE:
            entity->priority = PRIORITY_HIGH;
            entity->update_frequency = 2;  // Every other tick
            break;
        case ENTITY_VEHICLE:
            entity->priority = PRIORITY_HIGH;
            entity->update_frequency = 3;
            entity->health = 200;
            break;
        case ENTITY_PICKUP:
            entity->priority = PRIORITY_MEDIUM;
            entity->update_frequency = 10;  // Every 10 ticks
            break;
        default:
            entity->priority = PRIORITY_LOW;
            entity->update_frequency = 30;  // Every half second
            break;
    }
    
    return id;
}

// Destroy entity
void net_destroy_entity(uint32_t entity_id) {
    // Find entity
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        if (g_entity_manager.entities[i].id == entity_id) {
            // Swap with last and remove
            g_entity_manager.entities[i] = 
                g_entity_manager.entities[g_entity_manager.entity_count - 1];
            g_entity_manager.entity_count--;
            break;
        }
    }
}

// Update entity state
void net_update_entity(uint32_t entity_id, network_entity_t* update) {
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        if (g_entity_manager.entities[i].id == entity_id) {
            network_entity_t* entity = &g_entity_manager.entities[i];
            
            // Track what changed
            entity->dirty_mask = 0;
            
            if (entity->x != update->x) entity->dirty_mask |= (1 << 0);
            if (entity->y != update->y) entity->dirty_mask |= (1 << 1);
            if (entity->z != update->z) entity->dirty_mask |= (1 << 2);
            
            // Apply update
            *entity = *update;
            break;
        }
    }
}

// Build priority queues for replication
static void build_priority_queues(network_entity_t* viewer, uint32_t current_tick) {
    // Clear queues
    memset(g_entity_manager.priority_counts, 0, sizeof(g_entity_manager.priority_counts));
    
    // Rebuild spatial hash
    spatial_hash_clear(&g_entity_manager.spatial_hash);
    
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        network_entity_t* entity = &g_entity_manager.entities[i];
        
        // Add to spatial hash
        spatial_hash_insert(&g_entity_manager.spatial_hash, i,
                          entity->x, entity->y, entity->z);
        
        // Calculate relevance
        entity->relevance_score = calculate_relevance(entity, viewer);
        
        // Check if update needed
        uint32_t ticks_since_update = current_tick - entity->last_replicated_tick;
        if (ticks_since_update < entity->update_frequency && 
            entity->dirty_mask == 0) {
            continue;  // No update needed yet
        }
        
        // Add to appropriate priority queue
        uint32_t queue_index = g_entity_manager.priority_counts[entity->priority]++;
        g_entity_manager.priority_queues[entity->priority][queue_index] = i;
    }
    
    // Sort each priority queue by relevance
    for (uint32_t p = 0; p < PRIORITY_LEVELS; p++) {
        qsort(g_entity_manager.priority_queues[p],
              g_entity_manager.priority_counts[p],
              sizeof(uint32_t),
              entity_priority_compare);
    }
}

// Send entity updates to player
void net_send_entity_updates(network_context_t* ctx, uint32_t player_id) {
    if (!ctx->is_server) {
        return;
    }
    
    // Get viewer entity (assumed to be player entity)
    network_entity_t* viewer = NULL;
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        if (g_entity_manager.entities[i].owner_id == player_id &&
            g_entity_manager.entities[i].type == ENTITY_PLAYER) {
            viewer = &g_entity_manager.entities[i];
            break;
        }
    }
    
    if (!viewer) {
        return;  // No viewer entity
    }
    
    // Build priority queues
    build_priority_queues(viewer, ctx->current_tick);
    
    // Prepare update packet
    uint8_t packet[NET_MAX_PAYLOAD_SIZE];
    uint32_t packet_pos = 0;
    
    // Packet header
    *(uint32_t*)(packet + packet_pos) = ctx->current_tick;
    packet_pos += 4;
    
    uint32_t* entity_count_ptr = (uint32_t*)(packet + packet_pos);
    packet_pos += 4;
    
    uint32_t entities_sent = 0;
    uint32_t bandwidth_used = 0;
    const uint32_t max_bandwidth = 1024;  // 1KB per update
    
    // Send entities by priority
    for (uint32_t p = 0; p < PRIORITY_LEVELS && bandwidth_used < max_bandwidth; p++) {
        for (uint32_t i = 0; i < g_entity_manager.priority_counts[p] && 
             bandwidth_used < max_bandwidth; i++) {
            
            uint32_t entity_index = g_entity_manager.priority_queues[p][i];
            network_entity_t* entity = &g_entity_manager.entities[entity_index];
            
            // Get baseline for delta compression
            network_entity_t* baseline = NULL;
            if (g_entity_manager.last_sent_tick[player_id] > 0) {
                baseline = &g_entity_manager.last_sent_state[player_id][entity_index];
            }
            
            // Pack entity update
            uint32_t update_size = pack_entity_update(entity, baseline,
                                                     packet + packet_pos,
                                                     NET_MAX_PAYLOAD_SIZE - packet_pos);
            
            if (update_size == 0) {
                break;  // No more space
            }
            
            packet_pos += update_size;
            bandwidth_used += update_size;
            entities_sent++;
            
            // Update baseline
            g_entity_manager.last_sent_state[player_id][entity_index] = *entity;
            entity->last_replicated_tick = ctx->current_tick;
            entity->dirty_mask = 0;
        }
    }
    
    *entity_count_ptr = entities_sent;
    g_entity_manager.last_sent_tick[player_id] = ctx->current_tick;
    
    // Send packet
    if (entities_sent > 0) {
        net_send_unreliable(ctx, player_id, packet, packet_pos);
    }
}

// Receive entity updates (client)
void net_receive_entity_updates(network_context_t* ctx, const uint8_t* data, uint32_t size) {
    if (ctx->is_server) {
        return;
    }
    
    uint32_t pos = 0;
    
    // Read header
    if (size < 8) return;
    
    // uint32_t tick = *(uint32_t*)(data + pos);  // Currently unused
    pos += 4;
    
    uint32_t entity_count = *(uint32_t*)(data + pos);
    pos += 4;
    
    // Process each entity update
    for (uint32_t i = 0; i < entity_count && pos < size; i++) {
        network_entity_t entity;
        uint32_t bytes_read = unpack_entity_update(data + pos, size - pos,
                                                  &entity, NULL);
        
        if (bytes_read == 0) {
            break;
        }
        
        pos += bytes_read;
        
        // Apply to local entity state
        bool found = false;
        for (uint32_t j = 0; j < g_entity_manager.entity_count; j++) {
            if (g_entity_manager.entities[j].id == entity.id) {
                // Store previous state for interpolation
                network_entity_t* local = &g_entity_manager.entities[j];
                local->interp_x = local->x;
                local->interp_y = local->y;
                local->interp_z = local->z;
                local->interp_yaw = local->yaw;
                local->interp_pitch = local->pitch;
                local->interp_timestamp = ctx->current_time;
                
                // Apply update
                *local = entity;
                found = true;
                break;
            }
        }
        
        // Create new entity if not found
        if (!found && g_entity_manager.entity_count < 
            sizeof(g_entity_manager.entities) / sizeof(g_entity_manager.entities[0])) {
            g_entity_manager.entities[g_entity_manager.entity_count++] = entity;
        }
    }
}

// Interpolate entities for smooth rendering
void net_interpolate_entities(network_context_t* ctx, float alpha) {
    (void)alpha;  // Currently unused
    if (!ctx->enable_interpolation) {
        return;
    }
    
    uint64_t current_time = ctx->current_time;
    
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        network_entity_t* entity = &g_entity_manager.entities[i];
        
        // Skip local player (using prediction instead)
        if (entity->owner_id == ctx->local_player_id &&
            entity->type == ENTITY_PLAYER) {
            continue;
        }
        
        // Calculate interpolation factor
        if (entity->interp_timestamp > 0) {
            float time_diff = (current_time - entity->interp_timestamp) / 1000.0f;
            float interp_duration = NET_TICK_MS / 1000.0f;
            float t = time_diff / interp_duration;
            
            if (t < 1.0f) {
                // Smooth interpolation
                t = t * t * (3.0f - 2.0f * t);  // Smoothstep
                
                entity->x = entity->interp_x + (entity->x - entity->interp_x) * t;
                entity->y = entity->interp_y + (entity->y - entity->interp_y) * t;
                entity->z = entity->interp_z + (entity->z - entity->interp_z) * t;
                
                // Angular interpolation for rotation
                float yaw_diff = entity->yaw - entity->interp_yaw;
                if (yaw_diff > 180.0f) yaw_diff -= 360.0f;
                if (yaw_diff < -180.0f) yaw_diff += 360.0f;
                entity->yaw = entity->interp_yaw + yaw_diff * t;
                
                float pitch_diff = entity->pitch - entity->interp_pitch;
                entity->pitch = entity->interp_pitch + pitch_diff * t;
            }
        }
    }
}

// Get entities visible to player (for culling)
uint32_t net_get_visible_entities(uint32_t player_id, uint32_t* entity_ids,
                                 uint32_t max_entities) {
    // Find player entity
    network_entity_t* viewer = NULL;
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        if (g_entity_manager.entities[i].owner_id == player_id &&
            g_entity_manager.entities[i].type == ENTITY_PLAYER) {
            viewer = &g_entity_manager.entities[i];
            break;
        }
    }
    
    if (!viewer) {
        return 0;
    }
    
    // Use spatial hash to find nearby entities
    const float VIEW_RANGE = 500.0f;  // 500 units view distance
    
    return find_entities_in_range(&g_entity_manager.spatial_hash,
                                 viewer->x, viewer->y, viewer->z,
                                 VIEW_RANGE, entity_ids, max_entities);
}

// Debug: Print replication statistics
void net_debug_replication_stats(void) {
    printf("=== Replication Statistics ===\n");
    printf("Total Entities: %u\n", g_entity_manager.entity_count);
    
    // Count by type
    uint32_t type_counts[ENTITY_MAX_TYPES] = {0};
    for (uint32_t i = 0; i < g_entity_manager.entity_count; i++) {
        type_counts[g_entity_manager.entities[i].type]++;
    }
    
    printf("By Type:\n");
    printf("  Players: %u\n", type_counts[ENTITY_PLAYER]);
    printf("  Projectiles: %u\n", type_counts[ENTITY_PROJECTILE]);
    printf("  Vehicles: %u\n", type_counts[ENTITY_VEHICLE]);
    printf("  Pickups: %u\n", type_counts[ENTITY_PICKUP]);
    
    // Priority queue stats
    printf("Priority Queues:\n");
    for (uint32_t p = 0; p < PRIORITY_LEVELS; p++) {
        printf("  Priority %u: %u entities\n", p, 
               g_entity_manager.priority_counts[p]);
    }
    
    // Spatial hash stats
    uint32_t used_buckets = 0;
    uint32_t max_chain = 0;
    for (uint32_t i = 0; i < SPATIAL_HASH_SIZE; i++) {
        if (g_entity_manager.spatial_hash.buckets[i]) {
            used_buckets++;
            
            uint32_t chain_len = 0;
            spatial_node_t* node = g_entity_manager.spatial_hash.buckets[i];
            while (node) {
                chain_len++;
                node = node->next;
            }
            if (chain_len > max_chain) {
                max_chain = chain_len;
            }
        }
    }
    
    printf("Spatial Hash:\n");
    printf("  Used Buckets: %u/%u (%.1f%%)\n", used_buckets, SPATIAL_HASH_SIZE,
           (float)used_buckets / SPATIAL_HASH_SIZE * 100.0f);
    printf("  Max Chain Length: %u\n", max_chain);
}