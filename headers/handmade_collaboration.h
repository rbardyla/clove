#ifndef HANDMADE_COLLABORATION_H
#define HANDMADE_COLLABORATION_H

/*
 * Handmade Engine Multi-User Collaborative Editing System
 * 
 * AAA Production-Grade Collaborative Editing
 * Similar to Unity Collaborate or Unreal's Multi-User Editing
 * 
 * Features:
 * - Real-time collaboration for 32 simultaneous users
 * - Operational Transform (OT) for conflict resolution
 * - User presence awareness (cursors, selections, viewports)
 * - Role-based permissions (Admin, Editor, Viewer)
 * - Low-latency networking (<50ms for operations)
 * - Delta compression for bandwidth optimization
 * - Graceful handling of network failures
 * - Complete integration with handmade editor
 * 
 * PERFORMANCE TARGETS:
 * - Support 32 concurrent users
 * - <50ms latency for remote operations
 * - <10KB/s bandwidth per user
 * - 99.9% operation delivery guarantee
 * - Conflict-free collaborative editing
 */

#include <stdint.h>
#include <stdbool.h>
#include "systems/network/handmade_network.h"
#include "systems/editor/handmade_main_editor.h"

// Configuration Constants
#define COLLAB_MAX_USERS 32
#define COLLAB_MAX_USERNAME_LENGTH 64
#define COLLAB_MAX_SESSION_NAME_LENGTH 128
#define COLLAB_MAX_OPERATIONS_PER_FRAME 256
#define COLLAB_MAX_PENDING_OPERATIONS 1024
#define COLLAB_MAX_OPERATION_HISTORY 4096
#define COLLAB_OPERATION_BUFFER_SIZE 16384
#define COLLAB_MAX_SELECTION_OBJECTS 128
#define COLLAB_MAX_CURSOR_TRAIL_LENGTH 32
#define COLLAB_HEARTBEAT_INTERVAL_MS 1000
#define COLLAB_PRESENCE_TIMEOUT_MS 5000
#define COLLAB_OPERATION_TIMEOUT_MS 10000
#define COLLAB_MAX_CHAT_MESSAGE_LENGTH 512
#define COLLAB_MAX_CHAT_HISTORY 100

// User Roles and Permissions
typedef enum collab_user_role {
    COLLAB_ROLE_ADMIN = 0,     // Full access: read/write/manage users
    COLLAB_ROLE_EDITOR = 1,    // Read/write access to scene
    COLLAB_ROLE_VIEWER = 2,    // Read-only access
    COLLAB_ROLE_COUNT
} collab_user_role;

typedef struct collab_permissions {
    bool can_create_objects;
    bool can_delete_objects;
    bool can_modify_objects;
    bool can_modify_materials;
    bool can_modify_scripts;
    bool can_modify_settings;
    bool can_manage_users;
    bool can_save_project;
    bool can_load_project;
    bool can_build_project;
} collab_permissions;

// Operation Types for Operational Transform
typedef enum collab_operation_type {
    COLLAB_OP_OBJECT_CREATE = 0,
    COLLAB_OP_OBJECT_DELETE,
    COLLAB_OP_OBJECT_MOVE,
    COLLAB_OP_OBJECT_ROTATE,
    COLLAB_OP_OBJECT_SCALE,
    COLLAB_OP_OBJECT_RENAME,
    COLLAB_OP_PROPERTY_SET,
    COLLAB_OP_MATERIAL_ASSIGN,
    COLLAB_OP_HIERARCHY_CHANGE,
    COLLAB_OP_COMPONENT_ADD,
    COLLAB_OP_COMPONENT_REMOVE,
    COLLAB_OP_SCRIPT_EDIT,
    COLLAB_OP_TERRAIN_MODIFY,
    COLLAB_OP_LIGHT_CHANGE,
    COLLAB_OP_CAMERA_MOVE,
    COLLAB_OP_ANIMATION_CHANGE,
    COLLAB_OP_PHYSICS_CHANGE,
    COLLAB_OP_COUNT
} collab_operation_type;

// Operational Transform Operation
typedef struct collab_operation {
    uint64_t id;                    // Unique operation ID
    uint32_t user_id;              // User who performed the operation
    uint32_t sequence_number;       // Sequence number for ordering
    uint64_t timestamp;            // When operation was created
    collab_operation_type type;     // Type of operation
    uint32_t object_id;            // Target object ID (0 if N/A)
    uint32_t parent_operation_id;   // For dependent operations
    
    // Operation state
    bool is_applied;
    bool is_transformed;
    bool needs_undo;
    
    // Operation data (varies by type)
    union {
        struct {  // OBJECT_CREATE
            uint32_t object_type;
            char name[64];
            v3 position;
            quaternion rotation;
            v3 scale;
            uint32_t parent_id;
        } create;
        
        struct {  // OBJECT_DELETE
            uint32_t backup_data_size;
            uint8_t* backup_data;
        } delete;
        
        struct {  // OBJECT_MOVE/ROTATE/SCALE
            v3 old_value;
            v3 new_value;
            bool is_relative;
        } transform;
        
        struct {  // OBJECT_RENAME
            char old_name[64];
            char new_name[64];
        } rename;
        
        struct {  // PROPERTY_SET
            uint32_t property_hash;
            uint32_t old_value_size;
            uint32_t new_value_size;
            uint8_t* old_value;
            uint8_t* new_value;
        } property;
        
        struct {  // HIERARCHY_CHANGE
            uint32_t old_parent_id;
            uint32_t new_parent_id;
            uint32_t old_sibling_index;
            uint32_t new_sibling_index;
        } hierarchy;
        
        struct {  // COMPONENT_ADD/REMOVE
            uint32_t component_type;
            uint32_t component_data_size;
            uint8_t* component_data;
        } component;
        
        struct {  // SCRIPT_EDIT
            uint32_t script_id;
            uint32_t line_number;
            uint32_t column;
            uint32_t old_text_length;
            uint32_t new_text_length;
            char* old_text;
            char* new_text;
        } script_edit;
    } data;
    
    // OT metadata
    uint32_t context_vector[COLLAB_MAX_USERS];  // Vector clocks for causality
    uint8_t compressed_data[1024];              // Compressed operation data
    uint16_t compressed_size;
} collab_operation;

// User Presence Information
typedef struct collab_user_presence {
    uint32_t user_id;
    char username[COLLAB_MAX_USERNAME_LENGTH];
    collab_user_role role;
    
    // Visual representation
    uint32_t color;  // RGBA color for user identification
    uint32_t avatar_texture_id;
    
    // Current state
    bool is_active;
    uint64_t last_seen;
    bool is_typing;
    
    // Editor state
    v3 camera_position;
    quaternion camera_rotation;
    uint32_t focused_object_id;
    
    // Selection state
    uint32_t selected_objects[COLLAB_MAX_SELECTION_OBJECTS];
    uint32_t selected_object_count;
    
    // Cursor/interaction state
    v2 cursor_screen_pos;
    v3 cursor_world_pos;
    v3 cursor_trail[COLLAB_MAX_CURSOR_TRAIL_LENGTH];
    uint32_t cursor_trail_head;
    
    // Tool state
    uint32_t active_tool;
    uint32_t active_gizmo;
    bool is_manipulating;
    
    // Viewport state
    uint32_t active_viewport;
    v2 viewport_scroll;
    f32 viewport_zoom;
} collab_user_presence;

// Chat System
typedef struct collab_chat_message {
    uint32_t user_id;
    char username[COLLAB_MAX_USERNAME_LENGTH];
    char message[COLLAB_MAX_CHAT_MESSAGE_LENGTH];
    uint64_t timestamp;
    bool is_system_message;
} collab_chat_message;

// Network Protocol Messages
typedef enum collab_message_type {
    COLLAB_MSG_USER_JOIN = 0,
    COLLAB_MSG_USER_LEAVE,
    COLLAB_MSG_USER_LIST,
    COLLAB_MSG_OPERATION,
    COLLAB_MSG_OPERATION_ACK,
    COLLAB_MSG_OPERATION_BATCH,
    COLLAB_MSG_PRESENCE_UPDATE,
    COLLAB_MSG_SELECTION_UPDATE,
    COLLAB_MSG_CURSOR_UPDATE,
    COLLAB_MSG_CHAT_MESSAGE,
    COLLAB_MSG_PERMISSION_CHANGE,
    COLLAB_MSG_SESSION_INFO,
    COLLAB_MSG_SYNC_REQUEST,
    COLLAB_MSG_SYNC_RESPONSE,
    COLLAB_MSG_HEARTBEAT,
    COLLAB_MSG_ERROR,
    COLLAB_MSG_COUNT
} collab_message_type;

// Network message header
typedef struct collab_message_header {
    collab_message_type type;
    uint32_t user_id;
    uint32_t sequence_number;
    uint16_t message_size;
    uint16_t checksum;
    uint64_t timestamp;
} __attribute__((packed)) collab_message_header;

// Session Information
typedef struct collab_session_info {
    char session_name[COLLAB_MAX_SESSION_NAME_LENGTH];
    char project_path[512];
    uint32_t session_id;
    uint64_t created_time;
    uint32_t max_users;
    uint32_t current_user_count;
    uint32_t host_user_id;
    bool requires_password;
    bool is_public;
    uint64_t last_operation_id;
    uint32_t operation_count;
} collab_session_info;

// Conflict Resolution Context
typedef struct collab_conflict_context {
    collab_operation* local_op;
    collab_operation* remote_op;
    uint32_t conflict_type;
    f32 conflict_severity;  // 0.0 = no conflict, 1.0 = total conflict
    
    // Resolution strategy
    enum {
        CONFLICT_RESOLVE_MERGE,
        CONFLICT_RESOLVE_LOCAL_WINS,
        CONFLICT_RESOLVE_REMOTE_WINS,
        CONFLICT_RESOLVE_USER_DECIDES
    } resolution_strategy;
    
    // Additional context for smart resolution
    uint64_t local_timestamp;
    uint64_t remote_timestamp;
    f32 operation_priority;
    bool affects_same_object;
    bool affects_same_property;
} collab_conflict_context;

// Delta Compression State
typedef struct collab_delta_state {
    // Object snapshots for delta compression
    uint8_t object_snapshots[4096 * 32];  // 32 object snapshots of 4KB each
    uint32_t snapshot_object_ids[32];
    uint32_t snapshot_checksums[32];
    uint32_t snapshot_count;
    
    // Property change tracking
    uint32_t changed_object_ids[256];
    uint32_t changed_property_hashes[256];
    uint32_t change_count;
    
    // Compression dictionaries
    uint8_t compression_dictionary[8192];
    uint32_t dictionary_size;
} collab_delta_state;

// Main Collaboration Context
typedef struct collab_context {
    // Network integration
    network_context_t* network;
    
    // Session state
    collab_session_info session;
    bool is_host;
    bool is_connected;
    uint32_t local_user_id;
    
    // Users
    collab_user_presence users[COLLAB_MAX_USERS];
    uint32_t user_count;
    collab_permissions permission_matrix[COLLAB_MAX_USERS];
    
    // Operation management
    collab_operation operation_buffer[COLLAB_MAX_PENDING_OPERATIONS];
    uint32_t operation_buffer_head;
    uint32_t operation_buffer_tail;
    
    collab_operation operation_history[COLLAB_MAX_OPERATION_HISTORY];
    uint32_t history_head;
    uint32_t history_tail;
    
    uint64_t next_operation_id;
    uint32_t local_sequence_number;
    uint32_t remote_sequence_numbers[COLLAB_MAX_USERS];
    
    // Operational Transform state
    uint32_t context_vector[COLLAB_MAX_USERS];  // Vector clock for causality
    collab_operation* pending_local_ops[256];
    uint32_t pending_local_count;
    
    // Delta compression
    collab_delta_state delta_state;
    
    // Chat system
    collab_chat_message chat_history[COLLAB_MAX_CHAT_HISTORY];
    uint32_t chat_head;
    uint32_t chat_tail;
    
    // Performance monitoring
    struct {
        uint64_t operations_sent;
        uint64_t operations_received;
        uint64_t operations_transformed;
        uint64_t conflicts_resolved;
        f64 average_operation_latency;
        f64 bandwidth_usage_kbps;
        uint32_t active_connections;
    } stats;
    
    // Memory management
    arena* permanent_arena;
    arena* frame_arena;
    uint8_t operation_memory[COLLAB_OPERATION_BUFFER_SIZE];
    uint32_t operation_memory_used;
    
    // State
    bool is_syncing;
    uint64_t last_heartbeat_time;
    uint64_t last_presence_update_time;
    main_editor* editor;  // Integration with main editor
} collab_context;

// =============================================================================
// CORE COLLABORATION API
// =============================================================================

// Initialization and cleanup
collab_context* collab_create(main_editor* editor, arena* permanent_arena, arena* frame_arena);
void collab_destroy(collab_context* ctx);

// Session management
bool collab_host_session(collab_context* ctx, const char* session_name, 
                        uint16_t port, uint32_t max_users);
bool collab_join_session(collab_context* ctx, const char* server_address, 
                        uint16_t port, const char* username);
void collab_leave_session(collab_context* ctx);

// User management
uint32_t collab_add_user(collab_context* ctx, const char* username, collab_user_role role);
void collab_remove_user(collab_context* ctx, uint32_t user_id);
void collab_set_user_role(collab_context* ctx, uint32_t user_id, collab_user_role role);
collab_user_presence* collab_get_user(collab_context* ctx, uint32_t user_id);

// =============================================================================
// OPERATIONAL TRANSFORM API
// =============================================================================

// Operation creation
collab_operation* collab_create_operation(collab_context* ctx, collab_operation_type type, 
                                         uint32_t object_id);
void collab_submit_operation(collab_context* ctx, collab_operation* op);

// Operational Transform core
collab_operation* collab_transform_operation(collab_context* ctx, 
                                           collab_operation* local_op,
                                           collab_operation* remote_op);
bool collab_operations_conflict(collab_operation* op1, collab_operation* op2);
void collab_resolve_conflict(collab_context* ctx, collab_conflict_context* conflict);

// Operation application
bool collab_apply_operation(collab_context* ctx, collab_operation* op);
bool collab_undo_operation(collab_context* ctx, collab_operation* op);

// =============================================================================
// PRESENCE AND AWARENESS API
// =============================================================================

// Presence updates
void collab_update_presence(collab_context* ctx, collab_user_presence* presence);
void collab_update_cursor_position(collab_context* ctx, v2 screen_pos, v3 world_pos);
void collab_update_selection(collab_context* ctx, uint32_t* object_ids, uint32_t count);
void collab_update_camera(collab_context* ctx, v3 position, quaternion rotation);

// Presence queries
collab_user_presence* collab_get_all_users(collab_context* ctx, uint32_t* count);
bool collab_is_object_selected_by_others(collab_context* ctx, uint32_t object_id, uint32_t* by_user_id);
bool collab_is_object_being_edited(collab_context* ctx, uint32_t object_id);

// =============================================================================
// PERMISSION SYSTEM API
// =============================================================================

// Permission checks
bool collab_user_can_perform_operation(collab_context* ctx, uint32_t user_id, 
                                      collab_operation_type op_type);
bool collab_user_has_permission(collab_context* ctx, uint32_t user_id, 
                               const char* permission_name);
void collab_set_user_permissions(collab_context* ctx, uint32_t user_id, 
                                collab_permissions* permissions);

// Role management
collab_permissions collab_get_role_permissions(collab_user_role role);
void collab_enforce_permissions(collab_context* ctx, collab_operation* op);

// =============================================================================
// NETWORKING AND SYNCHRONIZATION API
// =============================================================================

// Network integration
void collab_send_message(collab_context* ctx, uint32_t to_user_id, 
                        collab_message_type type, const void* data, uint16_t size);
void collab_broadcast_message(collab_context* ctx, collab_message_type type, 
                             const void* data, uint16_t size);
bool collab_receive_message(collab_context* ctx, collab_message_header* header, 
                           void* buffer, uint16_t buffer_size);

// Synchronization
void collab_request_full_sync(collab_context* ctx);
void collab_send_sync_data(collab_context* ctx, uint32_t to_user_id);
bool collab_is_synchronized(collab_context* ctx);

// Delta compression
uint32_t collab_compress_operation(collab_context* ctx, collab_operation* op, 
                                  uint8_t* buffer, uint32_t buffer_size);
collab_operation* collab_decompress_operation(collab_context* ctx, 
                                             const uint8_t* buffer, uint32_t size);

// =============================================================================
// CHAT SYSTEM API
// =============================================================================

void collab_send_chat_message(collab_context* ctx, const char* message);
void collab_add_system_message(collab_context* ctx, const char* message);
collab_chat_message* collab_get_chat_history(collab_context* ctx, uint32_t* count);

// =============================================================================
// MAIN LOOP INTEGRATION API
// =============================================================================

// Main update function
void collab_update(collab_context* ctx, f32 dt);
void collab_handle_network_events(collab_context* ctx);
void collab_process_pending_operations(collab_context* ctx);

// Editor integration
void collab_on_object_selected(collab_context* ctx, uint32_t object_id);
void collab_on_object_deselected(collab_context* ctx, uint32_t object_id);
void collab_on_object_modified(collab_context* ctx, uint32_t object_id, 
                              const char* property_name, const void* old_value, 
                              const void* new_value, uint32_t value_size);

// GUI integration
void collab_show_user_list(collab_context* ctx, gui_context* gui);
void collab_show_chat_window(collab_context* ctx, gui_context* gui);
void collab_show_session_info(collab_context* ctx, gui_context* gui);
void collab_render_user_cursors(collab_context* ctx, renderer_state* renderer);
void collab_render_user_selections(collab_context* ctx, renderer_state* renderer);

// =============================================================================
// UTILITIES AND HELPERS
// =============================================================================

// Operation utilities
const char* collab_operation_type_string(collab_operation_type type);
uint32_t collab_operation_get_memory_size(collab_operation* op);
collab_operation* collab_operation_clone(collab_context* ctx, collab_operation* op);

// User utilities
uint32_t collab_user_get_color(uint32_t user_id);
const char* collab_user_role_string(collab_user_role role);

// Network utilities
uint16_t collab_message_checksum(const void* data, uint16_t size);
bool collab_message_verify_checksum(collab_message_header* header, const void* data);

// Performance monitoring
void collab_get_performance_stats(collab_context* ctx, 
                                 uint64_t* operations_per_second,
                                 f64* average_latency,
                                 f64* bandwidth_usage);

#endif // HANDMADE_COLLABORATION_H