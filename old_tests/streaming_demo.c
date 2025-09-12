/*
    Interactive AAA Streaming Demo
    
    Use WASD to move camera, observe zero-hitch streaming
*/

#include "handmade_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

static StreamingSystem g_streaming;
static v3 g_camera_pos = {0, 100, 0};
static v3 g_camera_vel = {0, 0, 0};
static bool g_running = true;

// Non-blocking keyboard input
static int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

static void handle_input() {
    if (!kbhit()) return;
    
    char key = getchar();
    f32 speed = 100.0f;
    
    switch(key) {
        case 'w': case 'W':
            g_camera_vel.z = -speed;
            break;
        case 's': case 'S':
            g_camera_vel.z = speed;
            break;
        case 'a': case 'A':
            g_camera_vel.x = -speed;
            break;
        case 'd': case 'D':
            g_camera_vel.x = speed;
            break;
        case ' ':
            g_camera_vel.y = speed;
            break;
        case 'c': case 'C':
            g_camera_vel.y = -speed;
            break;
        case 'q': case 'Q':
            g_running = false;
            break;
        case 'r': case 'R':
            streaming_reset_stats(&g_streaming);
            printf("\nStats reset!\n");
            break;
        case 'p': case 'P':
            streaming_dump_state(&g_streaming, "streaming_debug.txt");
            printf("\nState dumped to streaming_debug.txt\n");
            break;
    }
}

static void draw_status() {
    // Clear screen
    printf("\033[2J\033[H");
    
    printf("=== AAA Streaming Demo ===\n");
    printf("Controls: WASD=Move, Space=Up, C=Down, R=Reset Stats, P=Dump State, Q=Quit\n\n");
    
    printf("Camera: (%.1f, %.1f, %.1f) Vel: (%.1f, %.1f, %.1f)\n",
           g_camera_pos.x, g_camera_pos.y, g_camera_pos.z,
           g_camera_vel.x, g_camera_vel.y, g_camera_vel.z);
    
    StreamingStats stats = streaming_get_stats(&g_streaming);
    
    printf("\n--- Memory ---\n");
    printf("Used:    %6.1f MB / %.1f MB\n",
           stats.current_memory_usage / (1024.0f * 1024.0f),
           STREAMING_MEMORY_BUDGET / (1024.0f * 1024.0f));
    printf("Peak:    %6.1f MB\n", stats.peak_memory_usage / (1024.0f * 1024.0f));
    
    usize used, available;
    f32 fragmentation;
    streaming_get_memory_stats(&g_streaming, &used, &available, &fragmentation);
    printf("Fragment: %4.1f%%\n", fragmentation * 100.0f);
    
    printf("\n--- Streaming ---\n");
    printf("Requests:  %6u total\n", stats.total_requests);
    printf("Complete:  %6u (%.1f%%)\n", stats.completed_requests,
           stats.total_requests > 0 ? 100.0f * stats.completed_requests / stats.total_requests : 0);
    printf("Failed:    %6u\n", stats.failed_requests);
    printf("Cache Hit: %5.1f%%\n",
           stats.total_requests > 0 ? 100.0f * stats.cache_hits / stats.total_requests : 0);
    
    printf("\n--- Performance ---\n");
    printf("Loaded:   %7.1f MB\n", stats.bytes_loaded / (1024.0f * 1024.0f));
    printf("Evicted:  %7.1f MB\n", stats.bytes_evicted / (1024.0f * 1024.0f));
    
    // Show request queue status
    printf("\n--- Queue Status ---\n");
    for (u32 i = 0; i < STREAM_PRIORITY_COUNT; i++) {
        const char* priority_names[] = {
            "CRITICAL", "HIGH", "NORMAL", "PREFETCH", "LOW"
        };
        printf("%8s: %3u requests\n", priority_names[i],
               g_streaming.request_queue.counts[i]);
    }
    
    // ASCII art visualization of streaming rings
    printf("\n--- Streaming Rings ---\n");
    char map[21][41];
    memset(map, ' ', sizeof(map));
    
    // Draw rings
    for (int y = 0; y < 21; y++) {
        for (int x = 0; x < 41; x++) {
            f32 dx = (x - 20) * 50.0f;
            f32 dy = (y - 10) * 50.0f;
            f32 dist = sqrtf(dx*dx + dy*dy);
            
            if (dist < 100) map[y][x] = '#';  // Critical
            else if (dist < 250) map[y][x] = '+';  // High
            else if (dist < 400) map[y][x] = '.';  // Normal
            else if (dist < 600) map[y][x] = ' ';  // Prefetch
        }
    }
    
    // Draw camera
    map[10][20] = '@';
    
    // Print map
    for (int y = 0; y < 21; y++) {
        for (int x = 0; x < 41; x++) {
            printf("%c", map[y][x]);
        }
        printf("\n");
    }
    
    printf("\n[#=Critical, +=High, .=Normal, @=Camera]\n");
}

int main() {
    printf("Initializing AAA Streaming Demo...\n");
    
    // Initialize streaming system
    streaming_init(&g_streaming, STREAMING_MEMORY_BUDGET);
    
    // Configure streaming rings
    StreamingRing rings[] = {
        {0, 100, STREAM_PRIORITY_CRITICAL, 50},
        {100, 250, STREAM_PRIORITY_HIGH, 100},
        {250, 400, STREAM_PRIORITY_NORMAL, 200},
        {400, 600, STREAM_PRIORITY_PREFETCH, 400},
        {600, 1000, STREAM_PRIORITY_LOW, 800}
    };
    streaming_configure_rings(&g_streaming, rings, 5);
    
    // Create some virtual textures
    for (u32 i = 0; i < 4; i++) {
        streaming_create_virtual_texture(&g_streaming, 8192, 8192, 4);
    }
    
    printf("Demo ready! Press any key to start...\n");
    getchar();
    
    // Main loop
    clock_t last_time = clock();
    u32 frame = 0;
    
    while (g_running) {
        clock_t current_time = clock();
        f32 dt = (f32)(current_time - last_time) / CLOCKS_PER_SEC;
        last_time = current_time;
        
        // Handle input
        handle_input();
        
        // Update camera with damping
        g_camera_pos = v3_add(g_camera_pos, v3_scale(g_camera_vel, dt));
        g_camera_vel = v3_scale(g_camera_vel, 0.9f);  // Damping
        
        // Update streaming
        streaming_update(&g_streaming, g_camera_pos, g_camera_vel, dt);
        
        // Simulate requesting assets based on camera position
        if (frame % 10 == 0) {
            // Request some assets in view
            for (u32 i = 0; i < 10; i++) {
                u64 asset_id = rand() % 10000;
                f32 dist = 100.0f + (rand() % 500);
                u32 lod = streaming_calculate_lod(dist, 10.0f, 60.0f * M_PI / 180.0f);
                
                StreamPriority priority = dist < 200 ? STREAM_PRIORITY_HIGH : STREAM_PRIORITY_NORMAL;
                streaming_request_asset(&g_streaming, asset_id, priority, lod);
            }
        }
        
        // Draw status
        if (frame % 6 == 0) {  // Update display ~10 FPS
            draw_status();
        }
        
        frame++;
        usleep(16667);  // ~60 FPS
    }
    
    printf("\nShutting down...\n");
    streaming_shutdown(&g_streaming);
    
    return 0;
}
