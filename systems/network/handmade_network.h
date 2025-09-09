/*
 * Handmade Network Stack
 * Zero-dependency, deterministic networking for multiplayer games
 * 
 * Architecture:
 * - Custom UDP protocol with reliability
 * - Lock-free ring buffers for packet queues
 * - Fixed memory allocation (no malloc in runtime)
 * - Deterministic simulation with rollback
 * 
 * PERFORMANCE TARGETS:
 * - <50ms RTT handling
 * - 60Hz tick rate (16.67ms per frame)
 * - <1KB bandwidth per player per second
 * - Support 32 concurrent players
 * - Handle 10% packet loss gracefully
 */

#ifndef HANDMADE_NETWORK_H
#define HANDMADE_NETWORK_H

#include <stdint.h>
#include <stdbool.h>

// Platform detection
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    #define PLATFORM_LINUX
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
#endif

// Configuration constants
#define NET_MAX_PLAYERS 32
#define NET_MAX_PACKET_SIZE 1400  // Below MTU to avoid fragmentation
#define NET_PACKET_HEADER_SIZE 20  // Our custom header
#define NET_MAX_PAYLOAD_SIZE (NET_MAX_PACKET_SIZE - NET_PACKET_HEADER_SIZE)
#define NET_SEND_BUFFER_SIZE (64 * 1024)  // 64KB per connection
#define NET_RECV_BUFFER_SIZE (64 * 1024)
#define NET_MAX_PENDING_RELIABLE 256  // Max unacked reliable packets
#define NET_SEQUENCE_BUFFER_SIZE 1024  // For detecting duplicates
#define NET_SNAPSHOT_BUFFER_SIZE 60  // 1 second of snapshots at 60Hz
#define NET_INPUT_BUFFER_SIZE 120  // 2 seconds of input at 60Hz
#define NET_TICK_RATE 60  // Simulation tick rate
#define NET_TICK_MS (1000 / NET_TICK_RATE)
#define NET_HEARTBEAT_INTERVAL_MS 1000
#define NET_TIMEOUT_MS 5000
#define NET_MAX_FRAGMENT_SIZE 1024
#define NET_MAX_FRAGMENTS 16

// Packet types
typedef enum {
    PACKET_UNRELIABLE = 0,
    PACKET_RELIABLE_ORDERED = 1,
    PACKET_RELIABLE_UNORDERED = 2,
    PACKET_FRAGMENT = 3,
    PACKET_HEARTBEAT = 4,
    PACKET_CONNECT = 5,
    PACKET_ACCEPT = 6,
    PACKET_DISCONNECT = 7,
    PACKET_INPUT = 8,
    PACKET_SNAPSHOT = 9,
    PACKET_DELTA_SNAPSHOT = 10,
    PACKET_ACK = 11,
    PACKET_NAK = 12,
    PACKET_PING = 13,
    PACKET_PONG = 14,
} packet_type_t;

// Connection states
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_DISCONNECTING,
} connection_state_t;

// Packet header (20 bytes)
// CACHE: Fits in single cache line with room for payload start
typedef struct {
    uint32_t protocol_id;     // Magic number for our protocol
    uint16_t sequence;         // Packet sequence number
    uint16_t ack;             // Latest received sequence
    uint32_t ack_bits;        // Bitfield of received packets
    uint8_t type;             // packet_type_t
    uint8_t fragment_id;      // For fragmented packets
    uint8_t fragment_count;   // Total fragments
    uint8_t fragment_index;   // Current fragment index
    uint16_t payload_size;    // Size of payload
    uint16_t checksum;        // CRC16 of header + payload
} __attribute__((packed)) packet_header_t;

// Player input (8 bytes) - fits nicely in cache
typedef struct {
    uint32_t buttons;         // Button state bitfield
    int16_t move_x;          // Movement X axis (-32768 to 32767)
    int16_t move_y;          // Movement Y axis
} __attribute__((packed)) player_input_t;

// Network statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t packets_lost;
    uint64_t packets_acked;
    float packet_loss_percent;
    float rtt_ms;  // Round trip time
    float jitter_ms;
    float bandwidth_up_kbps;
    float bandwidth_down_kbps;
} net_stats_t;

// Fragment assembly buffer
typedef struct {
    uint8_t fragments[NET_MAX_FRAGMENTS][NET_MAX_FRAGMENT_SIZE];
    uint16_t fragment_sizes[NET_MAX_FRAGMENTS];
    uint8_t received_mask;  // Bitfield of received fragments
    uint8_t total_fragments;
    uint8_t fragment_id;
    uint64_t timestamp;
} fragment_assembly_t;

// Connection info
typedef struct {
    struct sockaddr_in address;
    connection_state_t state;
    uint16_t local_sequence;   // Our sequence number
    uint16_t remote_sequence;  // Their latest sequence
    uint32_t remote_ack_bits;  // Their ack bitfield
    uint64_t last_received_time;
    uint64_t last_sent_time;
    uint64_t connect_time;
    
    // Reliability
    struct {
        uint16_t sequence;
        uint8_t* data;
        uint16_t size;
        uint64_t send_time;
        int retry_count;
    } pending_reliable[NET_MAX_PENDING_RELIABLE];
    uint16_t pending_reliable_count;
    
    // Fragment assembly
    fragment_assembly_t fragment_assembly;
    
    // Ring buffers for send/recv
    uint8_t send_buffer[NET_SEND_BUFFER_SIZE];
    uint32_t send_head;
    uint32_t send_tail;
    
    uint8_t recv_buffer[NET_RECV_BUFFER_SIZE];
    uint32_t recv_head;
    uint32_t recv_tail;
    
    // Statistics
    net_stats_t stats;
    uint64_t rtt_samples[32];
    uint32_t rtt_sample_index;
    
    // Sequence number tracking for duplicate detection
    uint16_t received_sequences[NET_SEQUENCE_BUFFER_SIZE];
    uint32_t sequence_index;
} connection_t;

// Game snapshot for rollback
typedef struct {
    uint32_t tick;           // Simulation tick
    uint64_t timestamp;      // Real time
    uint32_t checksum;       // State checksum
    
    // Player states (simplified for demo)
    struct {
        float x, y, z;       // Position
        float vx, vy, vz;    // Velocity
        float yaw, pitch;    // Rotation
        uint32_t state;      // State flags
        uint32_t health;     // Health
    } players[NET_MAX_PLAYERS];
    
    // World state
    uint32_t entity_count;
    uint8_t compressed_entities[4096];  // Delta compressed
} game_snapshot_t;

// Input command for prediction
typedef struct {
    uint32_t tick;
    player_input_t input;
    uint32_t player_id;
} input_command_t;

// Network context
typedef struct {
    socket_t socket;
    uint16_t port;
    bool is_server;
    
    // Connections
    connection_t connections[NET_MAX_PLAYERS];
    uint32_t connection_count;
    uint32_t local_player_id;
    
    // Timing
    uint64_t current_time;
    uint64_t last_tick_time;
    uint32_t current_tick;
    
    // Rollback state
    game_snapshot_t snapshots[NET_SNAPSHOT_BUFFER_SIZE];
    uint32_t snapshot_head;
    uint32_t snapshot_tail;
    uint32_t confirmed_tick;  // Latest server-confirmed tick
    
    // Input buffers
    input_command_t input_buffer[NET_INPUT_BUFFER_SIZE];
    uint32_t input_head;
    uint32_t input_tail;
    
    // Memory pools
    uint8_t packet_memory_pool[256 * 1024];  // 256KB for packets
    uint32_t packet_memory_used;
    
    // Configuration
    float simulated_latency_ms;
    float simulated_packet_loss;
    bool enable_prediction;
    bool enable_interpolation;
    bool enable_compression;
} network_context_t;

// Core networking functions
bool net_init(network_context_t* ctx, uint16_t port, bool is_server);
void net_shutdown(network_context_t* ctx);
bool net_connect(network_context_t* ctx, const char* address, uint16_t port);
void net_disconnect(network_context_t* ctx, uint32_t player_id);
void net_update(network_context_t* ctx, uint64_t current_time_ms);

// Packet sending
bool net_send_unreliable(network_context_t* ctx, uint32_t player_id, 
                         const void* data, uint16_t size);
bool net_send_reliable(network_context_t* ctx, uint32_t player_id,
                      const void* data, uint16_t size);
bool net_broadcast(network_context_t* ctx, const void* data, uint16_t size);

// Packet receiving
bool net_receive(network_context_t* ctx, void* buffer, uint16_t* size,
                uint32_t* from_player_id);

// Input handling
void net_send_input(network_context_t* ctx, player_input_t* input);
bool net_get_input(network_context_t* ctx, uint32_t player_id, 
                  player_input_t* input);

// Snapshot/state management
void net_create_snapshot(network_context_t* ctx, game_snapshot_t* snapshot);
void net_send_snapshot(network_context_t* ctx);
void net_apply_snapshot(network_context_t* ctx, game_snapshot_t* snapshot);

// Rollback functions
void net_save_state(network_context_t* ctx, uint32_t tick);
bool net_rollback_to_tick(network_context_t* ctx, uint32_t tick);
void net_predict_tick(network_context_t* ctx, uint32_t tick);
void net_confirm_tick(network_context_t* ctx, uint32_t tick);

// Compression utilities
uint32_t net_compress_position(float x, float y, float z, uint8_t* buffer);
void net_decompress_position(const uint8_t* buffer, float* x, float* y, float* z);
uint16_t net_compress_rotation(float yaw, float pitch);
void net_decompress_rotation(uint16_t compressed, float* yaw, float* pitch);

// Statistics
void net_get_stats(network_context_t* ctx, uint32_t player_id, net_stats_t* stats);
void net_simulate_conditions(network_context_t* ctx, float latency_ms, 
                            float packet_loss_percent);

// Utilities
uint16_t net_checksum(const void* data, uint16_t size);
uint64_t net_get_time_ms(void);
const char* net_connection_state_string(connection_state_t state);

#endif // HANDMADE_NETWORK_H