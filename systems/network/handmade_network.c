/*
 * Handmade Network Implementation
 * Core networking with custom UDP protocol
 * 
 * PERFORMANCE: Zero allocations after init
 * CACHE: Packet headers fit in single cache line
 * DETERMINISM: Fixed timestep, no floating point in protocol
 */

#include "handmade_network.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stddef.h>
#include <math.h>

#ifdef PLATFORM_LINUX
#include <sys/epoll.h>
#include <poll.h>
#endif

#define PROTOCOL_ID 0x484D4E45  // "HMNE" - Handmade Network Engine

// Get monotonic time in milliseconds
uint64_t net_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// CRC16 checksum for packet integrity
// PERFORMANCE: Table-driven CRC is faster than bit-by-bit
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    // ... (abbreviated for space, would include full table)
};

uint16_t net_checksum(const void* data, uint16_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t crc = 0xFFFF;
    
    // PERFORMANCE: Unroll by 4 for better throughput
    while (size >= 4) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *bytes++];
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *bytes++];
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *bytes++];
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *bytes++];
        size -= 4;
    }
    
    while (size--) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *bytes++];
    }
    
    return crc;
}

// Initialize network context
bool net_init(network_context_t* ctx, uint16_t port, bool is_server) {
    memset(ctx, 0, sizeof(network_context_t));
    
    ctx->port = port;
    ctx->is_server = is_server;
    ctx->current_time = net_get_time_ms();
    ctx->enable_prediction = true;
    ctx->enable_interpolation = true;
    ctx->enable_compression = true;
    
#ifdef PLATFORM_WINDOWS
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
#endif
    
    // Create UDP socket
    ctx->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx->socket == INVALID_SOCKET_VALUE) {
        return false;
    }
    
    // Set non-blocking mode
#ifdef PLATFORM_WINDOWS
    u_long mode = 1;
    ioctlsocket(ctx->socket, FIONBIO, &mode);
#else
    int flags = fcntl(ctx->socket, F_GETFL, 0);
    fcntl(ctx->socket, F_SETFL, flags | O_NONBLOCK);
#endif
    
    // Set socket buffer sizes for performance
    int buffer_size = 256 * 1024;  // 256KB
    setsockopt(ctx->socket, SOL_SOCKET, SO_SNDBUF, 
               (const char*)&buffer_size, sizeof(buffer_size));
    setsockopt(ctx->socket, SOL_SOCKET, SO_RCVBUF,
               (const char*)&buffer_size, sizeof(buffer_size));
    
    // Enable address reuse
    int reuse = 1;
    setsockopt(ctx->socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuse, sizeof(reuse));
    
    // Bind to port if server
    if (is_server) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(ctx->socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(ctx->socket);
            return false;
        }
    }
    
    return true;
}

// Shutdown network
void net_shutdown(network_context_t* ctx) {
    if (ctx->socket != INVALID_SOCKET_VALUE) {
        // Send disconnect to all connections
        for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
            if (ctx->connections[i].state == CONN_CONNECTED) {
                net_disconnect(ctx, i);
            }
        }
        
#ifdef PLATFORM_WINDOWS
        closesocket(ctx->socket);
        WSACleanup();
#else
        close(ctx->socket);
#endif
        ctx->socket = INVALID_SOCKET_VALUE;
    }
}

// Find or create connection for address
static uint32_t find_or_create_connection(network_context_t* ctx,
                                         struct sockaddr_in* addr) {
    // Look for existing connection
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->connections[i].state != CONN_DISCONNECTED &&
            ctx->connections[i].address.sin_addr.s_addr == addr->sin_addr.s_addr &&
            ctx->connections[i].address.sin_port == addr->sin_port) {
            return i;
        }
    }
    
    // Create new connection
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->connections[i].state == CONN_DISCONNECTED) {
            connection_t* conn = &ctx->connections[i];
            memset(conn, 0, sizeof(connection_t));
            conn->address = *addr;
            conn->state = CONN_CONNECTING;
            conn->connect_time = ctx->current_time;
            conn->last_received_time = ctx->current_time;
            ctx->connection_count++;
            return i;
        }
    }
    
    return UINT32_MAX;  // No space for new connection
}

// Send raw packet
static bool send_packet(network_context_t* ctx, connection_t* conn,
                       packet_header_t* header, const void* data) {
    uint8_t packet[NET_MAX_PACKET_SIZE];
    
    // Fill header
    header->protocol_id = PROTOCOL_ID;
    header->sequence = conn->local_sequence++;
    header->ack = conn->remote_sequence;
    header->ack_bits = conn->remote_ack_bits;
    
    // Copy header and data
    memcpy(packet, header, sizeof(packet_header_t));
    if (data && header->payload_size > 0) {
        memcpy(packet + sizeof(packet_header_t), data, header->payload_size);
    }
    
    // Calculate checksum
    header->checksum = 0;
    header->checksum = net_checksum(packet, 
                                   sizeof(packet_header_t) + header->payload_size);
    memcpy(packet + offsetof(packet_header_t, checksum), 
           &header->checksum, sizeof(header->checksum));
    
    // Simulate packet loss if configured
    if (ctx->simulated_packet_loss > 0) {
        if ((rand() % 100) < (int)(ctx->simulated_packet_loss * 100)) {
            conn->stats.packets_lost++;
            return true;  // Pretend we sent it
        }
    }
    
    // Send packet
    int sent = sendto(ctx->socket, packet, 
                     sizeof(packet_header_t) + header->payload_size, 0,
                     (struct sockaddr*)&conn->address, sizeof(conn->address));
    
    if (sent > 0) {
        conn->stats.packets_sent++;
        conn->stats.bytes_sent += sent;
        conn->last_sent_time = ctx->current_time;
        
        // Simulate latency by delaying actual send
        if (ctx->simulated_latency_ms > 0) {
            // In real implementation, would queue for delayed send
            // For now, just track the intent
        }
        
        return true;
    }
    
    return false;
}

// Send unreliable packet
bool net_send_unreliable(network_context_t* ctx, uint32_t player_id,
                        const void* data, uint16_t size) {
    if (player_id >= NET_MAX_PLAYERS || size > NET_MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    connection_t* conn = &ctx->connections[player_id];
    if (conn->state != CONN_CONNECTED) {
        return false;
    }
    
    packet_header_t header = {0};
    header.type = PACKET_UNRELIABLE;
    header.payload_size = size;
    
    return send_packet(ctx, conn, &header, data);
}

// Send reliable packet
bool net_send_reliable(network_context_t* ctx, uint32_t player_id,
                      const void* data, uint16_t size) {
    if (player_id >= NET_MAX_PLAYERS || size > NET_MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    connection_t* conn = &ctx->connections[player_id];
    if (conn->state != CONN_CONNECTED) {
        return false;
    }
    
    // Check if we need to fragment
    if (size > NET_MAX_FRAGMENT_SIZE) {
        uint8_t fragment_count = (size + NET_MAX_FRAGMENT_SIZE - 1) / NET_MAX_FRAGMENT_SIZE;
        if (fragment_count > NET_MAX_FRAGMENTS) {
            return false;
        }
        
        static uint8_t fragment_id = 0;
        fragment_id++;
        
        const uint8_t* bytes = (const uint8_t*)data;
        for (uint8_t i = 0; i < fragment_count; i++) {
            uint16_t fragment_size = (i == fragment_count - 1) ?
                                    (size % NET_MAX_FRAGMENT_SIZE) :
                                    NET_MAX_FRAGMENT_SIZE;
            
            packet_header_t header = {0};
            header.type = PACKET_FRAGMENT;
            header.fragment_id = fragment_id;
            header.fragment_count = fragment_count;
            header.fragment_index = i;
            header.payload_size = fragment_size;
            
            if (!send_packet(ctx, conn, &header, bytes + i * NET_MAX_FRAGMENT_SIZE)) {
                return false;
            }
        }
        
        return true;
    }
    
    // Add to pending reliable queue
    if (conn->pending_reliable_count >= NET_MAX_PENDING_RELIABLE) {
        return false;  // Queue full
    }
    
    uint32_t index = conn->pending_reliable_count++;
    conn->pending_reliable[index].sequence = conn->local_sequence;
    conn->pending_reliable[index].size = size;
    conn->pending_reliable[index].send_time = ctx->current_time;
    conn->pending_reliable[index].retry_count = 0;
    
    // Allocate from memory pool
    if (ctx->packet_memory_used + size > sizeof(ctx->packet_memory_pool)) {
        conn->pending_reliable_count--;
        return false;  // Out of memory
    }
    
    conn->pending_reliable[index].data = ctx->packet_memory_pool + ctx->packet_memory_used;
    memcpy(conn->pending_reliable[index].data, data, size);
    ctx->packet_memory_used += size;
    
    // Send immediately
    packet_header_t header = {0};
    header.type = PACKET_RELIABLE_ORDERED;
    header.payload_size = size;
    
    return send_packet(ctx, conn, &header, data);
}

// Broadcast to all connected players
bool net_broadcast(network_context_t* ctx, const void* data, uint16_t size) {
    bool success = true;
    
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        if (ctx->connections[i].state == CONN_CONNECTED) {
            if (!net_send_unreliable(ctx, i, data, size)) {
                success = false;
            }
        }
    }
    
    return success;
}

// Process received packet
static void process_packet(network_context_t* ctx, uint32_t player_id,
                          packet_header_t* header, const void* data) {
    connection_t* conn = &ctx->connections[player_id];
    
    // Update sequence tracking
    conn->remote_sequence = header->sequence;
    conn->last_received_time = ctx->current_time;
    
    // Track received sequences for duplicate detection
    conn->received_sequences[conn->sequence_index % NET_SEQUENCE_BUFFER_SIZE] = header->sequence;
    conn->sequence_index++;
    
    // Update ack bits
    if (header->sequence > conn->remote_sequence) {
        uint16_t diff = header->sequence - conn->remote_sequence;
        if (diff < 32) {
            conn->remote_ack_bits <<= diff;
            conn->remote_ack_bits |= 1;
        }
    }
    
    // Process acks for our sent packets
    for (uint32_t i = 0; i < conn->pending_reliable_count; i++) {
        if (conn->pending_reliable[i].sequence == header->ack ||
            (header->ack_bits & (1 << (header->ack - conn->pending_reliable[i].sequence)))) {
            // Packet was acked, remove from pending
            conn->stats.packets_acked++;
            
            // Calculate RTT
            uint64_t rtt = ctx->current_time - conn->pending_reliable[i].send_time;
            conn->rtt_samples[conn->rtt_sample_index % 32] = rtt;
            conn->rtt_sample_index++;
            
            // Remove from pending (swap with last)
            conn->pending_reliable[i] = conn->pending_reliable[conn->pending_reliable_count - 1];
            conn->pending_reliable_count--;
            i--;
        }
    }
    
    // Handle packet based on type
    switch (header->type) {
        case PACKET_CONNECT:
            if (ctx->is_server && conn->state == CONN_CONNECTING) {
                conn->state = CONN_CONNECTED;
                
                // Send accept packet
                packet_header_t accept_header = {0};
                accept_header.type = PACKET_ACCEPT;
                accept_header.payload_size = sizeof(uint32_t);
                uint32_t assigned_id = player_id;
                send_packet(ctx, conn, &accept_header, &assigned_id);
            }
            break;
            
        case PACKET_ACCEPT:
            if (!ctx->is_server && conn->state == CONN_CONNECTING) {
                conn->state = CONN_CONNECTED;
                if (header->payload_size == sizeof(uint32_t)) {
                    memcpy(&ctx->local_player_id, data, sizeof(uint32_t));
                }
            }
            break;
            
        case PACKET_DISCONNECT:
            conn->state = CONN_DISCONNECTED;
            ctx->connection_count--;
            break;
            
        case PACKET_HEARTBEAT:
            // Just acknowledgment, already updated last_received_time
            break;
            
        case PACKET_FRAGMENT:
            // Assemble fragments
            if (header->fragment_id != conn->fragment_assembly.fragment_id) {
                // New fragment group
                memset(&conn->fragment_assembly, 0, sizeof(fragment_assembly_t));
                conn->fragment_assembly.fragment_id = header->fragment_id;
                conn->fragment_assembly.total_fragments = header->fragment_count;
                conn->fragment_assembly.timestamp = ctx->current_time;
            }
            
            if (header->fragment_index < NET_MAX_FRAGMENTS) {
                memcpy(conn->fragment_assembly.fragments[header->fragment_index],
                       data, header->payload_size);
                conn->fragment_assembly.fragment_sizes[header->fragment_index] = header->payload_size;
                conn->fragment_assembly.received_mask |= (1 << header->fragment_index);
                
                // Check if all fragments received
                if (conn->fragment_assembly.received_mask == 
                    ((1 << conn->fragment_assembly.total_fragments) - 1)) {
                    // Reassemble
                    uint8_t reassembled[NET_MAX_FRAGMENTS * NET_MAX_FRAGMENT_SIZE];
                    uint16_t total_size = 0;
                    
                    for (uint8_t i = 0; i < conn->fragment_assembly.total_fragments; i++) {
                        memcpy(reassembled + total_size,
                               conn->fragment_assembly.fragments[i],
                               conn->fragment_assembly.fragment_sizes[i]);
                        total_size += conn->fragment_assembly.fragment_sizes[i];
                    }
                    
                    // Process reassembled packet
                    // (Would call appropriate handler here)
                    
                    // Clear assembly buffer
                    memset(&conn->fragment_assembly, 0, sizeof(fragment_assembly_t));
                }
            }
            break;
            
        case PACKET_INPUT:
            // Store input in buffer
            if (header->payload_size == sizeof(player_input_t)) {
                input_command_t cmd;
                cmd.tick = ctx->current_tick;
                cmd.player_id = player_id;
                memcpy(&cmd.input, data, sizeof(player_input_t));
                
                uint32_t index = ctx->input_head % NET_INPUT_BUFFER_SIZE;
                ctx->input_buffer[index] = cmd;
                ctx->input_head++;
            }
            break;
            
        case PACKET_SNAPSHOT:
        case PACKET_DELTA_SNAPSHOT:
            // Store snapshot for rollback
            if (header->payload_size <= sizeof(game_snapshot_t)) {
                uint32_t index = ctx->snapshot_head % NET_SNAPSHOT_BUFFER_SIZE;
                memcpy(&ctx->snapshots[index], data, header->payload_size);
                ctx->snapshot_head++;
                
                // Update confirmed tick
                game_snapshot_t* snap = (game_snapshot_t*)data;
                if (snap->tick > ctx->confirmed_tick) {
                    ctx->confirmed_tick = snap->tick;
                }
            }
            break;
            
        case PACKET_PING:
            // Send pong
            {
                packet_header_t pong_header = {0};
                pong_header.type = PACKET_PONG;
                pong_header.payload_size = header->payload_size;
                send_packet(ctx, conn, &pong_header, data);
            }
            break;
            
        case PACKET_PONG:
            // Calculate RTT from ping timestamp
            if (header->payload_size == sizeof(uint64_t)) {
                uint64_t ping_time;
                memcpy(&ping_time, data, sizeof(uint64_t));
                uint64_t rtt = ctx->current_time - ping_time;
                conn->stats.rtt_ms = (float)rtt;
            }
            break;
    }
    
    // Add to receive buffer for application
    if (header->type == PACKET_UNRELIABLE || 
        header->type == PACKET_RELIABLE_ORDERED) {
        uint32_t space = NET_RECV_BUFFER_SIZE - 
                        (conn->recv_head - conn->recv_tail);
        if (space >= header->payload_size + sizeof(uint16_t)) {
            // Store size then data
            uint32_t index = conn->recv_head % NET_RECV_BUFFER_SIZE;
            memcpy(conn->recv_buffer + index, &header->payload_size, sizeof(uint16_t));
            index = (index + sizeof(uint16_t)) % NET_RECV_BUFFER_SIZE;
            memcpy(conn->recv_buffer + index, data, header->payload_size);
            conn->recv_head += sizeof(uint16_t) + header->payload_size;
        }
    }
}

// Receive packets
bool net_receive(network_context_t* ctx, void* buffer, uint16_t* size,
                uint32_t* from_player_id) {
    uint8_t packet[NET_MAX_PACKET_SIZE];
    struct sockaddr_in from_addr;
    socklen_t addr_len = sizeof(from_addr);
    
    int received = recvfrom(ctx->socket, packet, sizeof(packet), 0,
                          (struct sockaddr*)&from_addr, &addr_len);
    
    if (received < (int)sizeof(packet_header_t)) {
        return false;  // No packet or too small
    }
    
    packet_header_t* header = (packet_header_t*)packet;
    
    // Validate packet
    if (header->protocol_id != PROTOCOL_ID) {
        return false;  // Wrong protocol
    }
    
    if (header->payload_size > NET_MAX_PAYLOAD_SIZE ||
        received != (int)(sizeof(packet_header_t) + header->payload_size)) {
        return false;  // Invalid size
    }
    
    // Verify checksum
    uint16_t received_checksum = header->checksum;
    header->checksum = 0;
    uint16_t calculated_checksum = net_checksum(packet, received);
    if (received_checksum != calculated_checksum) {
        return false;  // Corrupted packet
    }
    header->checksum = received_checksum;
    
    // Find or create connection
    uint32_t player_id = find_or_create_connection(ctx, &from_addr);
    if (player_id == UINT32_MAX) {
        return false;  // No space for connection
    }
    
    // Update stats
    ctx->connections[player_id].stats.packets_received++;
    ctx->connections[player_id].stats.bytes_received += received;
    
    // Process packet
    process_packet(ctx, player_id, header, packet + sizeof(packet_header_t));
    
    // Return data to application
    if (from_player_id) *from_player_id = player_id;
    if (size) *size = header->payload_size;
    if (buffer && header->payload_size > 0) {
        memcpy(buffer, packet + sizeof(packet_header_t), header->payload_size);
    }
    
    return true;
}

// Connect to server
bool net_connect(network_context_t* ctx, const char* address, uint16_t port) {
    if (ctx->is_server) {
        return false;  // Servers don't connect
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
        return false;  // Invalid address
    }
    
    // Create connection
    uint32_t player_id = find_or_create_connection(ctx, &server_addr);
    if (player_id == UINT32_MAX) {
        return false;
    }
    
    // Send connect packet
    packet_header_t header = {0};
    header.type = PACKET_CONNECT;
    header.payload_size = 0;
    
    return send_packet(ctx, &ctx->connections[player_id], &header, NULL);
}

// Disconnect player
void net_disconnect(network_context_t* ctx, uint32_t player_id) {
    if (player_id >= NET_MAX_PLAYERS) {
        return;
    }
    
    connection_t* conn = &ctx->connections[player_id];
    if (conn->state == CONN_DISCONNECTED) {
        return;
    }
    
    // Send disconnect packet
    packet_header_t header = {0};
    header.type = PACKET_DISCONNECT;
    header.payload_size = 0;
    send_packet(ctx, conn, &header, NULL);
    
    // Clear connection
    conn->state = CONN_DISCONNECTED;
    ctx->connection_count--;
}

// Update network state
void net_update(network_context_t* ctx, uint64_t current_time_ms) {
    ctx->current_time = current_time_ms;
    
    // Process all pending receives
    while (true) {
        uint8_t buffer[NET_MAX_PAYLOAD_SIZE];
        uint16_t size;
        uint32_t from_player;
        
        if (!net_receive(ctx, buffer, &size, &from_player)) {
            break;  // No more packets
        }
    }
    
    // Update connections
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        connection_t* conn = &ctx->connections[i];
        
        if (conn->state == CONN_DISCONNECTED) {
            continue;
        }
        
        // Check for timeout
        if (ctx->current_time - conn->last_received_time > NET_TIMEOUT_MS) {
            conn->state = CONN_DISCONNECTED;
            ctx->connection_count--;
            continue;
        }
        
        // Send heartbeat if needed
        if (ctx->current_time - conn->last_sent_time > NET_HEARTBEAT_INTERVAL_MS) {
            packet_header_t header = {0};
            header.type = PACKET_HEARTBEAT;
            header.payload_size = 0;
            send_packet(ctx, conn, &header, NULL);
        }
        
        // Retry reliable packets
        for (uint32_t j = 0; j < conn->pending_reliable_count; j++) {
            if (ctx->current_time - conn->pending_reliable[j].send_time > 100) {
                // Retry after 100ms
                conn->pending_reliable[j].send_time = ctx->current_time;
                conn->pending_reliable[j].retry_count++;
                
                if (conn->pending_reliable[j].retry_count > 10) {
                    // Give up after 10 retries
                    conn->pending_reliable[j] = 
                        conn->pending_reliable[conn->pending_reliable_count - 1];
                    conn->pending_reliable_count--;
                    j--;
                } else {
                    // Resend
                    packet_header_t header = {0};
                    header.type = PACKET_RELIABLE_ORDERED;
                    header.payload_size = conn->pending_reliable[j].size;
                    send_packet(ctx, conn, &header, conn->pending_reliable[j].data);
                }
            }
        }
        
        // Update statistics
        if (conn->rtt_sample_index > 0) {
            uint64_t total_rtt = 0;
            uint32_t count = (conn->rtt_sample_index < 32) ? 
                           conn->rtt_sample_index : 32;
            
            for (uint32_t j = 0; j < count; j++) {
                total_rtt += conn->rtt_samples[j];
            }
            
            conn->stats.rtt_ms = (float)total_rtt / count;
            
            // Calculate jitter
            float avg_rtt = conn->stats.rtt_ms;
            float jitter_sum = 0;
            for (uint32_t j = 0; j < count; j++) {
                float diff = (float)conn->rtt_samples[j] - avg_rtt;
                jitter_sum += diff * diff;
            }
            conn->stats.jitter_ms = sqrtf(jitter_sum / count);
        }
        
        // Calculate packet loss
        if (conn->stats.packets_sent > 0) {
            conn->stats.packet_loss_percent = 
                (float)conn->stats.packets_lost / conn->stats.packets_sent * 100.0f;
        }
        
        // Calculate bandwidth
        static uint64_t last_bandwidth_calc = 0;
        if (ctx->current_time - last_bandwidth_calc > 1000) {
            conn->stats.bandwidth_up_kbps = 
                (float)conn->stats.bytes_sent * 8.0f / 1024.0f;
            conn->stats.bandwidth_down_kbps = 
                (float)conn->stats.bytes_received * 8.0f / 1024.0f;
            
            // Reset counters
            conn->stats.bytes_sent = 0;
            conn->stats.bytes_received = 0;
            last_bandwidth_calc = ctx->current_time;
        }
    }
    
    // Update simulation tick
    if (ctx->current_time - ctx->last_tick_time >= NET_TICK_MS) {
        ctx->current_tick++;
        ctx->last_tick_time += NET_TICK_MS;
        
        // Process inputs for this tick
        // (Game simulation would happen here)
    }
}

// Send player input
void net_send_input(network_context_t* ctx, player_input_t* input) {
    // Send to server if client, or process locally if server
    if (!ctx->is_server) {
        // Find server connection (assumed to be player 0)
        net_send_unreliable(ctx, 0, input, sizeof(player_input_t));
    }
    
    // Store locally for prediction
    input_command_t cmd;
    cmd.tick = ctx->current_tick;
    cmd.player_id = ctx->local_player_id;
    cmd.input = *input;
    
    uint32_t index = ctx->input_head % NET_INPUT_BUFFER_SIZE;
    ctx->input_buffer[index] = cmd;
    ctx->input_head++;
}

// Get player input
bool net_get_input(network_context_t* ctx, uint32_t player_id,
                  player_input_t* input) {
    // Search input buffer for latest input from player
    for (int32_t i = ctx->input_head - 1; i >= (int32_t)ctx->input_tail; i--) {
        uint32_t index = i % NET_INPUT_BUFFER_SIZE;
        if (ctx->input_buffer[index].player_id == player_id &&
            ctx->input_buffer[index].tick == ctx->current_tick) {
            *input = ctx->input_buffer[index].input;
            return true;
        }
    }
    
    return false;
}

// Get network statistics
void net_get_stats(network_context_t* ctx, uint32_t player_id, net_stats_t* stats) {
    if (player_id < NET_MAX_PLAYERS) {
        *stats = ctx->connections[player_id].stats;
    }
}

// Simulate network conditions for testing
void net_simulate_conditions(network_context_t* ctx, float latency_ms,
                            float packet_loss_percent) {
    ctx->simulated_latency_ms = latency_ms;
    ctx->simulated_packet_loss = packet_loss_percent / 100.0f;
}

// Get connection state string
const char* net_connection_state_string(connection_state_t state) {
    switch (state) {
        case CONN_DISCONNECTED: return "Disconnected";
        case CONN_CONNECTING: return "Connecting";
        case CONN_CONNECTED: return "Connected";
        case CONN_DISCONNECTING: return "Disconnecting";
        default: return "Unknown";
    }
}