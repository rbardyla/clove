/*
 * Handmade Network Demo
 * Multiplayer game demonstrating rollback netcode, compression, and sync
 * 
 * PERFORMANCE: 60Hz simulation, <1ms network overhead
 * FEATURES: 2-8 players, rollback, interpolation, lag compensation
 */

#include "handmade_network.h"

// Forward declarations for functions from other modules
extern void net_send_entity_updates(network_context_t* ctx, uint32_t player_id);
extern void net_interpolate_entities(network_context_t* ctx, float alpha);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

// Game constants
#define WORLD_SIZE 1000.0f
#define PLAYER_SPEED 100.0f  // Units per second
#define JUMP_VELOCITY 200.0f
#define GRAVITY -300.0f
#define PROJECTILE_SPEED 500.0f
#define PROJECTILE_LIFETIME 3000  // ms

// Player state
typedef struct {
    uint32_t id;
    float x, y, z;
    float vx, vy, vz;
    float yaw, pitch;
    uint32_t health;
    uint32_t score;
    uint32_t ammo;
    bool alive;
    char name[32];
    uint32_t color;  // RGB color
} player_state_t;

// Projectile
typedef struct {
    uint32_t id;
    uint32_t owner_id;
    float x, y, z;
    float vx, vy, vz;
    uint64_t spawn_time;
    bool active;
} projectile_t;

// Game state
typedef struct {
    player_state_t players[NET_MAX_PLAYERS];
    uint32_t player_count;
    
    projectile_t projectiles[256];
    uint32_t projectile_count;
    
    uint32_t local_player_id;
    bool is_server;
    bool running;
    
    // Statistics
    uint32_t frame_count;
    uint64_t frame_time_total;
    uint64_t network_time_total;
    uint64_t render_time_total;
    uint64_t physics_time_total;
    
    // Network
    network_context_t* net_ctx;
    
    // Input
    player_input_t current_input;
    bool keys[256];
    
    // Display
    char display_buffer[80 * 40];  // Simple text display
    uint32_t display_width;
    uint32_t display_height;
} game_state_t;

static game_state_t g_game;
static struct termios g_old_terminal;

// Terminal setup for input
static void setup_terminal(void) {
    struct termios new_terminal;
    tcgetattr(STDIN_FILENO, &g_old_terminal);
    new_terminal = g_old_terminal;
    new_terminal.c_lflag &= ~(ICANON | ECHO);
    new_terminal.c_cc[VMIN] = 0;
    new_terminal.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal);
}

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_terminal);
}

// Non-blocking keyboard input
static int kbhit(void) {
    int ch = getchar();
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

// Handle input
static void process_input(game_state_t* game) {
    if (kbhit()) {
        int ch = getchar();
        
        switch (ch) {
            case 'w': case 'W': game->keys['w'] = true; break;
            case 's': case 'S': game->keys['s'] = true; break;
            case 'a': case 'A': game->keys['a'] = true; break;
            case 'd': case 'D': game->keys['d'] = true; break;
            case ' ': game->keys[' '] = true; break;  // Jump
            case 'f': case 'F': game->keys['f'] = true; break;  // Fire
            case 'q': case 'Q': game->running = false; break;  // Quit
            
            // Debug keys
            case '1': // Simulate 100ms latency
                net_simulate_conditions(game->net_ctx, 100.0f, 0.0f);
                printf("Simulating 100ms latency\n");
                break;
            case '2': // Simulate 200ms latency
                net_simulate_conditions(game->net_ctx, 200.0f, 0.0f);
                printf("Simulating 200ms latency\n");
                break;
            case '3': // Simulate 5% packet loss
                net_simulate_conditions(game->net_ctx, 0.0f, 5.0f);
                printf("Simulating 5%% packet loss\n");
                break;
            case '4': // Simulate bad connection
                net_simulate_conditions(game->net_ctx, 150.0f, 10.0f);
                printf("Simulating bad connection (150ms, 10%% loss)\n");
                break;
            case '0': // Clear simulation
                net_simulate_conditions(game->net_ctx, 0.0f, 0.0f);
                printf("Cleared network simulation\n");
                break;
        }
    }
    
    // Build input packet
    game->current_input.buttons = 0;
    game->current_input.move_x = 0;
    game->current_input.move_y = 0;
    
    if (game->keys['w']) game->current_input.move_y = 32767;
    if (game->keys['s']) game->current_input.move_y = -32767;
    if (game->keys['a']) game->current_input.move_x = -32767;
    if (game->keys['d']) game->current_input.move_x = 32767;
    if (game->keys[' ']) game->current_input.buttons |= 0x01;  // Jump
    if (game->keys['f']) game->current_input.buttons |= 0x02;  // Fire
    
    // Clear single-press keys
    game->keys[' '] = false;
    game->keys['f'] = false;
}

// Spawn projectile
static void spawn_projectile(game_state_t* game, uint32_t owner_id,
                            float x, float y, float z,
                            float yaw, float pitch) {
    for (uint32_t i = 0; i < 256; i++) {
        if (!game->projectiles[i].active) {
            projectile_t* proj = &game->projectiles[i];
            proj->id = i;
            proj->owner_id = owner_id;
            proj->x = x;
            proj->y = y;
            proj->z = z + 10.0f;  // Spawn at head height
            
            // Calculate velocity from angles
            float yaw_rad = yaw * M_PI / 180.0f;
            float pitch_rad = pitch * M_PI / 180.0f;
            
            proj->vx = cosf(yaw_rad) * cosf(pitch_rad) * PROJECTILE_SPEED;
            proj->vy = sinf(yaw_rad) * cosf(pitch_rad) * PROJECTILE_SPEED;
            proj->vz = sinf(pitch_rad) * PROJECTILE_SPEED;
            
            proj->spawn_time = net_get_time_ms();
            proj->active = true;
            
            if (game->projectile_count < 256) {
                game->projectile_count++;
            }
            break;
        }
    }
}

// Update physics
static void update_physics(game_state_t* game, float dt) {
    uint64_t start_time = net_get_time_ms();
    
    // Update players
    for (uint32_t i = 0; i < game->player_count; i++) {
        player_state_t* player = &game->players[i];
        
        if (!player->alive) continue;
        
        // Apply gravity
        player->vz += GRAVITY * dt;
        
        // Update position
        player->x += player->vx * dt;
        player->y += player->vy * dt;
        player->z += player->vz * dt;
        
        // Ground collision
        if (player->z <= 0) {
            player->z = 0;
            player->vz = 0;
        }
        
        // World boundaries
        if (player->x < -WORLD_SIZE) player->x = -WORLD_SIZE;
        if (player->x > WORLD_SIZE) player->x = WORLD_SIZE;
        if (player->y < -WORLD_SIZE) player->y = -WORLD_SIZE;
        if (player->y > WORLD_SIZE) player->y = WORLD_SIZE;
        
        // Friction
        player->vx *= 0.9f;
        player->vy *= 0.9f;
    }
    
    // Update projectiles
    uint64_t current_time = net_get_time_ms();
    for (uint32_t i = 0; i < 256; i++) {
        projectile_t* proj = &game->projectiles[i];
        
        if (!proj->active) continue;
        
        // Lifetime check
        if (current_time - proj->spawn_time > PROJECTILE_LIFETIME) {
            proj->active = false;
            continue;
        }
        
        // Update position
        proj->x += proj->vx * dt;
        proj->y += proj->vy * dt;
        proj->z += proj->vz * dt;
        
        // Gravity for projectiles
        proj->vz += GRAVITY * 0.1f * dt;  // Less gravity for projectiles
        
        // Ground collision
        if (proj->z <= 0) {
            proj->active = false;
            continue;
        }
        
        // Check player collisions
        for (uint32_t j = 0; j < game->player_count; j++) {
            player_state_t* player = &game->players[j];
            
            if (!player->alive || player->id == proj->owner_id) continue;
            
            float dx = player->x - proj->x;
            float dy = player->y - proj->y;
            float dz = player->z - proj->z;
            float dist_sq = dx * dx + dy * dy + dz * dz;
            
            if (dist_sq < 100.0f) {  // 10 unit hit radius
                // Hit!
                player->health -= 25;
                if (player->health <= 0) {
                    player->alive = false;
                    player->health = 0;
                    
                    // Award score to shooter
                    for (uint32_t k = 0; k < game->player_count; k++) {
                        if (game->players[k].id == proj->owner_id) {
                            game->players[k].score++;
                            break;
                        }
                    }
                }
                
                proj->active = false;
                break;
            }
        }
    }
    
    game->physics_time_total += net_get_time_ms() - start_time;
}

// Apply player input
static void apply_input(game_state_t* game, uint32_t player_id, 
                       player_input_t* input, float dt) {
    (void)dt;  // Currently using fixed speed instead of dt
    player_state_t* player = NULL;
    
    for (uint32_t i = 0; i < game->player_count; i++) {
        if (game->players[i].id == player_id) {
            player = &game->players[i];
            break;
        }
    }
    
    if (!player || !player->alive) return;
    
    // Movement
    // float move_speed = PLAYER_SPEED * dt;  // Unused
    player->vx = (input->move_x / 32767.0f) * PLAYER_SPEED;
    player->vy = (input->move_y / 32767.0f) * PLAYER_SPEED;
    
    // Jump
    if ((input->buttons & 0x01) && player->z == 0) {
        player->vz = JUMP_VELOCITY;
    }
    
    // Fire
    if (input->buttons & 0x02) {
        if (player->ammo > 0) {
            spawn_projectile(game, player_id, 
                           player->x, player->y, player->z,
                           player->yaw, player->pitch);
            player->ammo--;
        }
    }
}

// Render game state to terminal
static void render_game(game_state_t* game) {
    uint64_t start_time = net_get_time_ms();
    
    // Clear screen
    printf("\033[2J\033[H");  // ANSI escape codes
    
    // Header
    printf("=== HANDMADE NETWORK DEMO ===\n");
    printf("Mode: %s | Players: %u | FPS: %u\n",
           game->is_server ? "SERVER" : "CLIENT",
           game->player_count,
           (game->frame_count > 0 && game->frame_time_total > 0) ? 
           (uint32_t)(1000 * game->frame_count / game->frame_time_total) : 0);
    
    // Network stats
    net_stats_t stats;
    net_get_stats(game->net_ctx, 0, &stats);
    printf("Network: RTT %.1fms | Loss %.1f%% | Up %.1f KB/s | Down %.1f KB/s\n",
           stats.rtt_ms, stats.packet_loss_percent,
           stats.bandwidth_up_kbps / 8.0f, stats.bandwidth_down_kbps / 8.0f);
    
    // Performance stats
    if (game->frame_count > 0) {
        printf("Perf: Physics %.1fms | Network %.1fms | Render %.1fms\n",
               game->frame_count > 0 ? (float)game->physics_time_total / game->frame_count : 0.0f,
               game->frame_count > 0 ? (float)game->network_time_total / game->frame_count : 0.0f,
               game->frame_count > 0 ? (float)game->render_time_total / game->frame_count : 0.0f);
    }
    
    printf("\n");
    
    // Players
    printf("PLAYERS:\n");
    for (uint32_t i = 0; i < game->player_count; i++) {
        player_state_t* player = &game->players[i];
        
        printf("  [%c] %s: HP %3u | Score %3u | Ammo %3u | Pos (%.0f, %.0f, %.0f)%s\n",
               player->id == game->local_player_id ? '*' : ' ',
               player->name,
               player->health,
               player->score,
               player->ammo,
               player->x, player->y, player->z,
               player->alive ? "" : " [DEAD]");
    }
    
    // Active projectiles count
    uint32_t active_projectiles = 0;
    for (uint32_t i = 0; i < 256; i++) {
        if (game->projectiles[i].active) active_projectiles++;
    }
    printf("\nProjectiles: %u\n", active_projectiles);
    
    // Top-down map view (ASCII)
    printf("\nMAP (Top-down view):\n");
    
    const int map_width = 60;
    const int map_height = 20;
    char map[map_height][map_width + 1];
    
    // Initialize map
    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            map[y][x] = '.';
        }
        map[y][map_width] = '\0';
    }
    
    // Draw border
    for (int x = 0; x < map_width; x++) {
        map[0][x] = '-';
        map[map_height - 1][x] = '-';
    }
    for (int y = 0; y < map_height; y++) {
        map[y][0] = '|';
        map[y][map_width - 1] = '|';
    }
    
    // Place players on map
    for (uint32_t i = 0; i < game->player_count; i++) {
        player_state_t* player = &game->players[i];
        
        if (!player->alive) continue;
        
        int map_x = (int)((player->x + WORLD_SIZE) / (2 * WORLD_SIZE) * (map_width - 2)) + 1;
        int map_y = (int)((player->y + WORLD_SIZE) / (2 * WORLD_SIZE) * (map_height - 2)) + 1;
        
        if (map_x >= 1 && map_x < map_width - 1 &&
            map_y >= 1 && map_y < map_height - 1) {
            
            if (player->id == game->local_player_id) {
                map[map_y][map_x] = '@';  // Local player
            } else {
                map[map_y][map_x] = '0' + (player->id % 10);  // Other players
            }
        }
    }
    
    // Place projectiles on map
    for (uint32_t i = 0; i < 256; i++) {
        projectile_t* proj = &game->projectiles[i];
        
        if (!proj->active) continue;
        
        int map_x = (int)((proj->x + WORLD_SIZE) / (2 * WORLD_SIZE) * (map_width - 2)) + 1;
        int map_y = (int)((proj->y + WORLD_SIZE) / (2 * WORLD_SIZE) * (map_height - 2)) + 1;
        
        if (map_x >= 1 && map_x < map_width - 1 &&
            map_y >= 1 && map_y < map_height - 1) {
            
            if (map[map_y][map_x] == '.') {
                map[map_y][map_x] = '*';  // Projectile
            }
        }
    }
    
    // Print map
    for (int y = 0; y < map_height; y++) {
        printf("%s\n", map[y]);
    }
    
    // Controls
    printf("\nCONTROLS:\n");
    printf("  WASD: Move | Space: Jump | F: Fire | Q: Quit\n");
    printf("  1-4: Simulate network conditions | 0: Clear simulation\n");
    
    game->render_time_total += net_get_time_ms() - start_time;
}

// Server thread
static void* server_thread(void* arg) {
    game_state_t* game = (game_state_t*)arg;
    
    while (game->running) {
        uint64_t start_time = net_get_time_ms();
        
        // Process network
        net_update(game->net_ctx, start_time);
        
        // Send snapshots to clients
        if (game->net_ctx->current_tick % 2 == 0) {  // Every other tick
            net_send_snapshot(game->net_ctx);
        }
        
        // Send entity updates
        for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
            if (game->net_ctx->connections[i].state == CONN_CONNECTED) {
                net_send_entity_updates(game->net_ctx, i);
            }
        }
        
        game->network_time_total += net_get_time_ms() - start_time;
        
        // Sleep to maintain tick rate
        uint64_t elapsed = net_get_time_ms() - start_time;
        if (elapsed < NET_TICK_MS) {
            usleep((NET_TICK_MS - elapsed) * 1000);
        }
    }
    
    return NULL;
}

// Initialize game
static bool init_game(game_state_t* game, bool is_server, uint16_t port) {
    memset(game, 0, sizeof(game_state_t));
    
    game->is_server = is_server;
    game->running = true;
    game->display_width = 80;
    game->display_height = 40;
    
    // Create network context
    game->net_ctx = malloc(sizeof(network_context_t));
    if (!game->net_ctx) {
        return false;
    }
    
    if (!net_init(game->net_ctx, port, is_server)) {
        printf("Failed to initialize network on port %u\n", port);
        free(game->net_ctx);
        return false;
    }
    
    // Create local player
    if (!is_server) {
        game->local_player_id = 0;  // Will be assigned by server
    } else {
        game->local_player_id = 0;  // Server is always player 0
        
        // Add server player
        player_state_t* player = &game->players[0];
        player->id = 0;
        player->x = 0;
        player->y = 0;
        player->z = 0;
        player->health = 100;
        player->ammo = 100;
        player->alive = true;
        snprintf(player->name, sizeof(player->name), "Server");
        player->color = 0xFF0000;  // Red
        game->player_count = 1;
    }
    
    setup_terminal();
    
    return true;
}

// Cleanup
static void cleanup_game(game_state_t* game) {
    restore_terminal();
    
    if (game->net_ctx) {
        net_shutdown(game->net_ctx);
        free(game->net_ctx);
    }
}

// Signal handler
static void signal_handler(int sig) {
    (void)sig;  // Unused parameter
    g_game.running = false;
}

// Main game loop
int main(int argc, char* argv[]) {
    bool is_server = false;
    uint16_t port = 27015;
    char* server_address = NULL;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-server") == 0) {
            is_server = true;
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-connect") == 0 && i + 1 < argc) {
            server_address = argv[++i];
        } else if (strcmp(argv[i], "-help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -server          Run as server\n");
            printf("  -port <port>     Port number (default: 27015)\n");
            printf("  -connect <addr>  Connect to server address\n");
            printf("  -help           Show this help\n");
            return 0;
        }
    }
    
    // Initialize
    if (!init_game(&g_game, is_server, port)) {
        printf("Failed to initialize game\n");
        return 1;
    }
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Starting %s on port %u...\n", 
           is_server ? "server" : "client", port);
    
    // Connect to server if client
    if (!is_server && server_address) {
        printf("Connecting to %s:%u...\n", server_address, port);
        if (!net_connect(g_game.net_ctx, server_address, port)) {
            printf("Failed to connect to server\n");
            cleanup_game(&g_game);
            return 1;
        }
    }
    
    // Start server thread if server
    pthread_t server_thread_id;
    if (is_server) {
        pthread_create(&server_thread_id, NULL, server_thread, &g_game);
    }
    
    // Main game loop
    uint64_t last_frame_time = net_get_time_ms();
    uint64_t last_render_time = 0;
    
    while (g_game.running) {
        uint64_t current_time = net_get_time_ms();
        uint64_t frame_start = current_time;
        
        float dt = (current_time - last_frame_time) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;  // Cap at 100ms
        
        // Process input
        process_input(&g_game);
        
        // Apply local input
        apply_input(&g_game, g_game.local_player_id, 
                   &g_game.current_input, dt);
        
        // Send input to network
        if (!g_game.is_server) {
            net_send_input(g_game.net_ctx, &g_game.current_input);
        }
        
        // Update physics
        update_physics(&g_game, dt);
        
        // Network update
        uint64_t net_start = net_get_time_ms();
        net_update(g_game.net_ctx, current_time);
        g_game.network_time_total += net_get_time_ms() - net_start;
        
        // Interpolate entities
        net_interpolate_entities(g_game.net_ctx, dt);
        
        // Render at 30 FPS
        if (current_time - last_render_time >= 33) {
            render_game(&g_game);
            last_render_time = current_time;
        }
        
        // Frame timing
        g_game.frame_count++;
        g_game.frame_time_total += net_get_time_ms() - frame_start;
        last_frame_time = current_time;
        
        // Sleep to maintain 60 FPS
        uint64_t frame_time = net_get_time_ms() - frame_start;
        if (frame_time < 16) {
            usleep((16 - frame_time) * 1000);
        }
    }
    
    // Cleanup
    printf("\nShutting down...\n");
    
    if (is_server) {
        pthread_join(server_thread_id, NULL);
    }
    
    cleanup_game(&g_game);
    
    return 0;
}