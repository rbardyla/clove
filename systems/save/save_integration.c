/*
    Handmade Save System - Engine Integration
    
    Hooks into all engine systems for saving/loading
    Manages save/load state transitions
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Platform-specific implementations
#ifdef _WIN32
#include <windows.h>

b32 platform_save_write_file(char *path, void *data, u32 size) {
    // PERFORMANCE: Direct Win32 API, no CRT overhead
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, 
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    DWORD bytes_written;
    b32 result = WriteFile(file, data, size, &bytes_written, NULL);
    CloseHandle(file);
    
    return result && (bytes_written == size);
}

b32 platform_save_read_file(char *path, void *data, u32 max_size, u32 *actual_size) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    LARGE_INTEGER file_size;
    GetFileSizeEx(file, &file_size);
    
    if (file_size.QuadPart > max_size) {
        CloseHandle(file);
        return 0;
    }
    
    DWORD bytes_read;
    b32 result = ReadFile(file, data, (DWORD)file_size.QuadPart, &bytes_read, NULL);
    CloseHandle(file);
    
    if (result) {
        *actual_size = bytes_read;
    }
    
    return result;
}

b32 platform_save_delete_file(char *path) {
    return DeleteFileA(path);
}

b32 platform_save_file_exists(char *path) {
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && 
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

u64 platform_save_get_file_time(char *path) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    FILETIME write_time;
    GetFileTime(file, NULL, NULL, &write_time);
    CloseHandle(file);
    
    // Convert to Unix timestamp
    ULARGE_INTEGER uli;
    uli.LowPart = write_time.dwLowDateTime;
    uli.HighPart = write_time.dwHighDateTime;
    
    return uli.QuadPart / 10000000ULL - 11644473600ULL;
}

void platform_create_save_directory(void) {
    CreateDirectoryA("saves", NULL);
}

#else // Linux/Unix

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

b32 platform_save_write_file(char *path, void *data, u32 size) {
    // PERFORMANCE: Direct syscalls, O_DIRECT for DMA if available
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (fd < 0) {
        return 0;
    }
    
    ssize_t written = write(fd, data, size);
    close(fd);
    
    return written == size;
}

b32 platform_save_read_file(char *path, void *data, u32 max_size, u32 *actual_size) {
    int fd = open(path, O_RDONLY);
    
    if (fd < 0) {
        return 0;
    }
    
    struct stat st;
    fstat(fd, &st);
    
    if (st.st_size > max_size) {
        close(fd);
        return 0;
    }
    
    ssize_t bytes_read = read(fd, data, st.st_size);
    close(fd);
    
    if (bytes_read > 0) {
        *actual_size = bytes_read;
        return 1;
    }
    
    return 0;
}

b32 platform_save_delete_file(char *path) {
    return unlink(path) == 0;
}

b32 platform_save_file_exists(char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

u64 platform_save_get_file_time(char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (u64)st.st_mtime;
    }
    return 0;
}

void platform_create_save_directory(void) {
    mkdir("saves", 0755);
}

#endif

// Save/Load state machine
typedef enum save_state {
    SAVE_STATE_IDLE,
    SAVE_STATE_PREPARING,
    SAVE_STATE_PAUSING_GAME,
    SAVE_STATE_CAPTURING_STATE,
    SAVE_STATE_WRITING,
    SAVE_STATE_RESUMING,
    SAVE_STATE_COMPLETE,
    SAVE_STATE_ERROR
} save_state;

typedef struct save_operation {
    save_state state;
    i32 target_slot;
    f32 progress;
    f32 timer;
    char status_message[256];
    b32 async;
    b32 show_ui;
} save_operation;

global_variable save_operation g_save_op = {0};

// Pause game systems for save
internal void pause_game_for_save(game_state *game) {
    // PERFORMANCE: Ensure consistent state, stop simulation
    game->paused = 1;
    game->physics->paused = 1;
    game->audio->paused = 1;
    game->scripts->paused = 1;
    
    // Wait for current frame to complete
    // In production, would use proper synchronization
}

// Resume game systems after save
internal void resume_game_after_save(game_state *game) {
    game->paused = 0;
    game->physics->paused = 0;
    game->audio->paused = 0;
    game->scripts->paused = 0;
}

// Async save worker (would run on thread in production)
internal void async_save_worker(save_system *system, game_state *game) {
    // In production, this would run on a worker thread
    // For now, simulate with state machine
    
    switch (g_save_op.state) {
        case SAVE_STATE_PREPARING:
            strcpy(g_save_op.status_message, "Preparing to save...");
            g_save_op.progress = 0.1f;
            g_save_op.state = SAVE_STATE_PAUSING_GAME;
            break;
            
        case SAVE_STATE_PAUSING_GAME:
            strcpy(g_save_op.status_message, "Pausing game...");
            pause_game_for_save(game);
            g_save_op.progress = 0.2f;
            g_save_op.state = SAVE_STATE_CAPTURING_STATE;
            break;
            
        case SAVE_STATE_CAPTURING_STATE:
            strcpy(g_save_op.status_message, "Capturing game state...");
            g_save_op.progress = 0.5f;
            g_save_op.state = SAVE_STATE_WRITING;
            break;
            
        case SAVE_STATE_WRITING: {
            strcpy(g_save_op.status_message, "Writing to disk...");
            g_save_op.progress = 0.8f;
            
            b32 success = save_game(system, game, g_save_op.target_slot);
            
            if (success) {
                g_save_op.state = SAVE_STATE_RESUMING;
            } else {
                g_save_op.state = SAVE_STATE_ERROR;
                strcpy(g_save_op.status_message, "Save failed!");
            }
            break;
        }
            
        case SAVE_STATE_RESUMING:
            strcpy(g_save_op.status_message, "Resuming game...");
            resume_game_after_save(game);
            g_save_op.progress = 0.9f;
            g_save_op.state = SAVE_STATE_COMPLETE;
            break;
            
        case SAVE_STATE_COMPLETE:
            strcpy(g_save_op.status_message, "Save complete!");
            g_save_op.progress = 1.0f;
            g_save_op.timer = 2.0f; // Show message for 2 seconds
            break;
            
        case SAVE_STATE_ERROR:
            g_save_op.progress = 0.0f;
            g_save_op.timer = 3.0f; // Show error for 3 seconds
            resume_game_after_save(game);
            break;
            
        default:
            break;
    }
}

// Start async save operation
void save_start_async(save_system *system, game_state *game, i32 slot) {
    if (g_save_op.state != SAVE_STATE_IDLE) {
        printf("Save already in progress\n");
        return;
    }
    
    g_save_op.state = SAVE_STATE_PREPARING;
    g_save_op.target_slot = slot;
    g_save_op.progress = 0.0f;
    g_save_op.async = 1;
    g_save_op.show_ui = 1;
}

// Update save operation
void save_update_operation(save_system *system, game_state *game, f32 dt) {
    if (g_save_op.state == SAVE_STATE_IDLE) {
        return;
    }
    
    if (g_save_op.async) {
        async_save_worker(system, game);
    }
    
    // Handle completion timer
    if (g_save_op.state == SAVE_STATE_COMPLETE || 
        g_save_op.state == SAVE_STATE_ERROR) {
        g_save_op.timer -= dt;
        
        if (g_save_op.timer <= 0.0f) {
            g_save_op.state = SAVE_STATE_IDLE;
            g_save_op.show_ui = 0;
        }
    }
}

// Input handling for save system
void save_handle_input(save_system *system, game_state *game, input_state *input) {
    // Quick save (F5)
    if (input->keys[KEY_F5].pressed) {
        if (g_save_op.state == SAVE_STATE_IDLE) {
            printf("Quick saving...\n");
            
            // Quick save is synchronous and fast
            u64 start = __builtin_ia32_rdtsc();
            b32 success = quicksave(system, game);
            u64 end = __builtin_ia32_rdtsc();
            
            f32 time_ms = (f32)(end - start) / 3000000.0f; // Assume 3GHz
            
            if (success) {
                sprintf(g_save_op.status_message, "Quick saved! (%.1fms)", time_ms);
            } else {
                strcpy(g_save_op.status_message, "Quick save failed!");
            }
            
            g_save_op.state = SAVE_STATE_COMPLETE;
            g_save_op.timer = 1.5f;
            g_save_op.show_ui = 1;
        }
    }
    
    // Quick load (F9)
    if (input->keys[KEY_F9].pressed) {
        if (g_save_op.state == SAVE_STATE_IDLE) {
            printf("Quick loading...\n");
            
            u64 start = __builtin_ia32_rdtsc();
            b32 success = quickload(system, game);
            u64 end = __builtin_ia32_rdtsc();
            
            f32 time_ms = (f32)(end - start) / 3000000.0f;
            
            if (success) {
                sprintf(g_save_op.status_message, "Quick loaded! (%.1fms)", time_ms);
            } else {
                strcpy(g_save_op.status_message, "Quick load failed!");
            }
            
            g_save_op.state = SAVE_STATE_COMPLETE;
            g_save_op.timer = 1.5f;
            g_save_op.show_ui = 1;
        }
    }
    
    // Manual save menu (F6)
    if (input->keys[KEY_F6].pressed) {
        // In production, would open save menu UI
        printf("Opening save menu...\n");
    }
}

// Render save UI
void save_render_ui(save_system *system, render_state *renderer) {
    if (!g_save_op.show_ui) {
        return;
    }
    
    // PERFORMANCE: Simple immediate mode UI
    // In production, would use proper UI system
    
    // Background overlay
    if (g_save_op.state != SAVE_STATE_IDLE && 
        g_save_op.state != SAVE_STATE_COMPLETE) {
        // Draw semi-transparent overlay
        // renderer->draw_rect(0, 0, screen_width, screen_height, 0x80000000);
    }
    
    // Progress bar
    if (g_save_op.progress > 0.0f && g_save_op.progress < 1.0f) {
        i32 bar_width = 300;
        i32 bar_height = 20;
        i32 bar_x = (renderer->width - bar_width) / 2;
        i32 bar_y = renderer->height - 100;
        
        // Background
        // renderer->draw_rect(bar_x, bar_y, bar_width, bar_height, 0xFF333333);
        
        // Progress
        i32 progress_width = (i32)(bar_width * g_save_op.progress);
        // renderer->draw_rect(bar_x, bar_y, progress_width, bar_height, 0xFF00FF00);
    }
    
    // Status message
    if (g_save_op.status_message[0]) {
        i32 text_x = renderer->width / 2;
        i32 text_y = renderer->height - 60;
        
        // renderer->draw_text_centered(text_x, text_y, g_save_op.status_message, 0xFFFFFFFF);
    }
}

// Settings save/load
typedef struct game_settings {
    // Graphics
    u32 resolution_width;
    u32 resolution_height;
    b32 fullscreen;
    b32 vsync;
    u32 texture_quality;
    u32 shadow_quality;
    f32 render_scale;
    
    // Audio
    f32 master_volume;
    f32 music_volume;
    f32 sfx_volume;
    f32 voice_volume;
    b32 surround_sound;
    
    // Controls
    u32 key_bindings[256];
    f32 mouse_sensitivity;
    b32 invert_y;
    
    // Gameplay
    u32 difficulty;
    b32 auto_save;
    f32 auto_save_interval;
    b32 show_tutorials;
    b32 show_subtitles;
    
    // Performance
    b32 multi_threading;
    u32 thread_count;
    b32 gpu_particles;
} game_settings;

b32 save_settings(game_settings *settings) {
    // PERFORMANCE: Settings saved separately from game saves
    save_buffer buffer = {0};
    u8 data[4096];
    buffer.data = data;
    buffer.capacity = 4096;
    
    // Write settings version
    save_write_u32(&buffer, 1);
    
    // Graphics
    save_write_u32(&buffer, settings->resolution_width);
    save_write_u32(&buffer, settings->resolution_height);
    save_write_u8(&buffer, settings->fullscreen);
    save_write_u8(&buffer, settings->vsync);
    save_write_u32(&buffer, settings->texture_quality);
    save_write_u32(&buffer, settings->shadow_quality);
    save_write_f32(&buffer, settings->render_scale);
    
    // Audio
    save_write_f32(&buffer, settings->master_volume);
    save_write_f32(&buffer, settings->music_volume);
    save_write_f32(&buffer, settings->sfx_volume);
    save_write_f32(&buffer, settings->voice_volume);
    save_write_u8(&buffer, settings->surround_sound);
    
    // Controls
    save_write_bytes(&buffer, settings->key_bindings, sizeof(settings->key_bindings));
    save_write_f32(&buffer, settings->mouse_sensitivity);
    save_write_u8(&buffer, settings->invert_y);
    
    // Gameplay
    save_write_u32(&buffer, settings->difficulty);
    save_write_u8(&buffer, settings->auto_save);
    save_write_f32(&buffer, settings->auto_save_interval);
    save_write_u8(&buffer, settings->show_tutorials);
    save_write_u8(&buffer, settings->show_subtitles);
    
    // Performance
    save_write_u8(&buffer, settings->multi_threading);
    save_write_u32(&buffer, settings->thread_count);
    save_write_u8(&buffer, settings->gpu_particles);
    
    return platform_save_write_file("settings.cfg", buffer.data, buffer.size);
}

b32 load_settings(game_settings *settings) {
    u8 data[4096];
    u32 actual_size;
    
    if (!platform_save_read_file("settings.cfg", data, 4096, &actual_size)) {
        // Use defaults
        settings->resolution_width = 1920;
        settings->resolution_height = 1080;
        settings->fullscreen = 0;
        settings->vsync = 1;
        settings->texture_quality = 2;
        settings->shadow_quality = 2;
        settings->render_scale = 1.0f;
        
        settings->master_volume = 1.0f;
        settings->music_volume = 0.7f;
        settings->sfx_volume = 1.0f;
        settings->voice_volume = 1.0f;
        settings->surround_sound = 0;
        
        settings->mouse_sensitivity = 1.0f;
        settings->invert_y = 0;
        
        settings->difficulty = 1;
        settings->auto_save = 1;
        settings->auto_save_interval = 300.0f;
        settings->show_tutorials = 1;
        settings->show_subtitles = 0;
        
        settings->multi_threading = 1;
        settings->thread_count = 4;
        settings->gpu_particles = 1;
        
        return 0;
    }
    
    save_buffer buffer = {
        .data = data,
        .size = actual_size,
        .capacity = actual_size,
        .read_offset = 0
    };
    
    // Read settings version
    u32 version = save_read_u32(&buffer);
    
    if (version != 1) {
        // Handle version mismatch
        return 0;
    }
    
    // Graphics
    settings->resolution_width = save_read_u32(&buffer);
    settings->resolution_height = save_read_u32(&buffer);
    settings->fullscreen = save_read_u8(&buffer);
    settings->vsync = save_read_u8(&buffer);
    settings->texture_quality = save_read_u32(&buffer);
    settings->shadow_quality = save_read_u32(&buffer);
    settings->render_scale = save_read_f32(&buffer);
    
    // Audio
    settings->master_volume = save_read_f32(&buffer);
    settings->music_volume = save_read_f32(&buffer);
    settings->sfx_volume = save_read_f32(&buffer);
    settings->voice_volume = save_read_f32(&buffer);
    settings->surround_sound = save_read_u8(&buffer);
    
    // Controls
    save_read_bytes(&buffer, settings->key_bindings, sizeof(settings->key_bindings));
    settings->mouse_sensitivity = save_read_f32(&buffer);
    settings->invert_y = save_read_u8(&buffer);
    
    // Gameplay
    settings->difficulty = save_read_u32(&buffer);
    settings->auto_save = save_read_u8(&buffer);
    settings->auto_save_interval = save_read_f32(&buffer);
    settings->show_tutorials = save_read_u8(&buffer);
    settings->show_subtitles = save_read_u8(&buffer);
    
    // Performance
    settings->multi_threading = save_read_u8(&buffer);
    settings->thread_count = save_read_u32(&buffer);
    settings->gpu_particles = save_read_u8(&buffer);
    
    return 1;
}

// Initialize save system integration
void save_init_integration(save_system *system, game_state *game) {
    // Create save directory if needed
    platform_create_save_directory();
    
    // Register migration functions
    save_register_all_migrations(system);
    
    // Load settings
    game_settings settings;
    if (load_settings(&settings)) {
        // Apply settings to game - copy individual fields
        game->settings.resolution_width = settings.resolution_width;
        game->settings.resolution_height = settings.resolution_height;
        game->settings.fullscreen = settings.fullscreen;
        game->settings.vsync = settings.vsync;
        game->settings.master_volume = settings.master_volume;
        game->settings.music_volume = settings.music_volume;
        game->settings.sfx_volume = settings.sfx_volume;
        game->settings.auto_save = settings.auto_save;
        game->settings.auto_save_interval = settings.auto_save_interval;
        
        if (settings.auto_save) {
            save_enable_autosave(system, settings.auto_save_interval);
        }
    }
    
    // Enumerate existing saves
    save_enumerate_slots(system);
    
    // Check for crash recovery
    if (platform_save_file_exists("crash_recovery.hms")) {
        printf("Crash recovery save found!\n");
        // In production, would prompt user to load crash save
    }
}

// Crash recovery save
void save_crash_recovery(save_system *system, game_state *game) {
    // PERFORMANCE: Minimal save for crash recovery
    // Only save essential state
    
    save_buffer buffer = {0};
    buffer.data = system->compress_buffer.data;
    buffer.capacity = system->compress_buffer.capacity;
    
    // Simplified header
    save_header header = {
        .magic = SAVE_MAGIC_NUMBER,
        .version = SAVE_VERSION,
        .timestamp = (u64)time(NULL),
        .checksum = 0,
        .compressed = 0
    };
    
    save_write_bytes(&buffer, &header, sizeof(header));
    
    // Minimal metadata
    save_metadata metadata = {0};
    metadata.playtime_seconds = game->playtime_seconds;
    strncpy(metadata.level_name, game->current_level, 63);
    strncpy(metadata.player_name, game->player.name, 31);
    
    save_write_bytes(&buffer, &metadata, sizeof(metadata));
    
    // Only save player position and critical state
    save_write_f32(&buffer, game->player.position[0]);
    save_write_f32(&buffer, game->player.position[1]);
    save_write_f32(&buffer, game->player.position[2]);
    save_write_u32(&buffer, game->player.health);
    save_write_string(&buffer, game->current_level);
    
    // Write end marker
    save_chunk_header end_chunk = {
        .type = SAVE_CHUNK_END,
        .uncompressed_size = 0,
        .compressed_size = 0,
        .checksum = 0
    };
    save_write_bytes(&buffer, &end_chunk, sizeof(end_chunk));
    
    // Update checksum
    header.checksum = save_crc32(buffer.data + sizeof(header), 
                                 buffer.size - sizeof(header));
    memcpy(buffer.data, &header, sizeof(header));
    
    // Write crash save
    platform_save_write_file("crash_recovery.hms", buffer.data, buffer.size);
}