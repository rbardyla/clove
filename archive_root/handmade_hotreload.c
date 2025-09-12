/*
    Hot Reload Implementation
    =========================
    
    State-preserving hot reload system for zero-allocation architecture.
*/

#define _GNU_SOURCE
#include "handmade_hotreload.h"
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Platform-specific DLL operations
#ifdef __linux__
    #define DLL_EXPORT __attribute__((visibility("default")))
    #define DLL_EXTENSION ".so"
#elif _WIN32
    #define DLL_EXPORT __declspec(dllexport)
    #define DLL_EXTENSION ".dll"
#endif

// Internal state
typedef struct {
    GameModuleAPI api;
    HotReloadState state;
    bool initialized;
} HotReloadInternal;

static HotReloadInternal g_hotreload = {0};

// Forward declarations
static bool LoadGameModule(HotReloadState* state, const char* dll_path);
static void UnloadGameModule(HotReloadState* state);
static u64 GetFileWriteTime(const char* path);
static void CopyFile(const char* src, const char* dst);
static u64 CalculateChecksum(void* data, usize size);

// Initialize hot reload system
static bool InitHotReload(HotReloadState* state, const char* dll_path) {
    if (g_hotreload.initialized) {
        return true;
    }
    
    // Set up paths
    strncpy(state->dll_path, dll_path, sizeof(state->dll_path) - 1);
    snprintf(state->dll_temp_path, sizeof(state->dll_temp_path), 
             "%s.tmp%d%s", dll_path, getpid(), DLL_EXTENSION);
    
    // Allocate state preservation buffer
    state->state_buffer = mmap(NULL, HOTRELOAD_STATE_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (state->state_buffer == MAP_FAILED) {
        fprintf(stderr, "Failed to allocate hot reload state buffer\n");
        return false;
    }
    state->state_buffer_size = HOTRELOAD_STATE_SIZE;
    
    // Initial load
    if (!LoadGameModule(state, dll_path)) {
        munmap(state->state_buffer, HOTRELOAD_STATE_SIZE);
        return false;
    }
    
    state->current_generation = 1;
    state->reload_count = 0;
    g_hotreload.initialized = true;
    
    printf("[Hot Reload] Initialized with module: %s\n", dll_path);
    return true;
}

// Check if module needs reload
static bool CheckForReload(HotReloadState* state) {
    u64 write_time = GetFileWriteTime(state->dll_path);
    if (write_time > state->last_dll_write_time) {
        // Wait a bit to ensure write is complete
        usleep(100000); // 100ms
        
        u64 write_time2 = GetFileWriteTime(state->dll_path);
        if (write_time2 == write_time) {
            return true;
        }
    }
    return false;
}

// Perform hot reload
static bool PerformReload(HotReloadState* state, PlatformState* platform) {
    f64 start_time = PlatformGetTime();
    
    printf("[Hot Reload] Starting reload #%d...\n", state->reload_count + 1);
    
    // Step 1: Save current state
    if (state->serialize_state || g_hotreload.api.GameSerializeState) {
        usize state_size = 0;
        
        if (g_hotreload.api.GameSerializeState) {
            g_hotreload.api.GameSerializeState(state->state_buffer, &state_size);
        } else if (state->serialize_state) {
            state->serialize_state(state->state_buffer, &state_size);
        }
        
        // Add header
        HotReloadStateHeader* header = (HotReloadStateHeader*)state->state_buffer;
        header->magic = HOTRELOAD_STATE_MAGIC;
        header->version = HOTRELOAD_MODULE_VERSION;
        header->state_size = state_size;
        header->checksum = CalculateChecksum((u8*)state->state_buffer + sizeof(HotReloadStateHeader),
                                            state_size - sizeof(HotReloadStateHeader));
        header->timestamp = start_time;
        
        printf("[Hot Reload] Serialized %zu bytes of state\n", state_size);
    }
    
    // Step 2: Notify module of unload
    if (g_hotreload.api.GameOnUnload) {
        g_hotreload.api.GameOnUnload(platform);
    }
    
    // Step 3: Unload current module
    UnloadGameModule(state);
    
    // Step 4: Copy and load new module
    CopyFile(state->dll_path, state->dll_temp_path);
    if (!LoadGameModule(state, state->dll_temp_path)) {
        fprintf(stderr, "[Hot Reload] Failed to load new module\n");
        return false;
    }
    
    // Step 5: Restore state
    if (state->deserialize_state || g_hotreload.api.GameDeserializeState) {
        HotReloadStateHeader* header = (HotReloadStateHeader*)state->state_buffer;
        
        if (header->magic == HOTRELOAD_STATE_MAGIC) {
            u64 checksum = CalculateChecksum((u8*)state->state_buffer + sizeof(HotReloadStateHeader),
                                            header->state_size - sizeof(HotReloadStateHeader));
            
            if (checksum == header->checksum) {
                if (g_hotreload.api.GameDeserializeState) {
                    g_hotreload.api.GameDeserializeState(state->state_buffer, header->state_size);
                } else if (state->deserialize_state) {
                    state->deserialize_state(state->state_buffer, header->state_size);
                }
                printf("[Hot Reload] Restored %u bytes of state\n", header->state_size);
            } else {
                fprintf(stderr, "[Hot Reload] State checksum mismatch!\n");
            }
        }
    }
    
    // Step 6: Patch function pointers
    if (state->patch_function_pointers) {
        state->patch_function_pointers();
    }
    
    // Step 7: Remap asset handles
    u32 old_generation = state->current_generation;
    state->current_generation++;
    
    if (state->remap_asset_handles) {
        state->remap_asset_handles(old_generation, state->current_generation);
    }
    
    // Step 8: Notify module of reload
    if (g_hotreload.api.GameOnReload) {
        g_hotreload.api.GameOnReload(platform);
    }
    
    // Update statistics
    f64 reload_time = PlatformGetTime() - start_time;
    state->last_reload_time = reload_time;
    state->total_reload_time += reload_time;
    state->reload_count++;
    
    printf("[Hot Reload] Completed in %.2fms\n", reload_time * 1000.0);
    return true;
}

// Load game module
static bool LoadGameModule(HotReloadState* state, const char* dll_path) {
    // Open the shared library
    state->dll_handle = dlopen(dll_path, RTLD_NOW);
    if (!state->dll_handle) {
        fprintf(stderr, "Failed to load module: %s\n", dlerror());
        return false;
    }
    
    // Get the API export function
    typedef GameModuleAPI* (*GetAPIFunc)(void);
    GetAPIFunc get_api = (GetAPIFunc)dlsym(state->dll_handle, "GetGameModuleAPI");
    
    if (get_api) {
        // New-style module with full API
        GameModuleAPI* api = get_api();
        g_hotreload.api = *api;
    } else {
        // Fallback to individual function loading
        g_hotreload.api.GameInit = (void*)dlsym(state->dll_handle, "GameInit");
        g_hotreload.api.GameUpdate = (void*)dlsym(state->dll_handle, "GameUpdate");
        g_hotreload.api.GameRender = (void*)dlsym(state->dll_handle, "GameRender");
        g_hotreload.api.GameShutdown = (void*)dlsym(state->dll_handle, "GameShutdown");
        g_hotreload.api.GameOnReload = (void*)dlsym(state->dll_handle, "GameOnReload");
        g_hotreload.api.GameSerializeState = (void*)dlsym(state->dll_handle, "GameSerializeState");
        g_hotreload.api.GameDeserializeState = (void*)dlsym(state->dll_handle, "GameDeserializeState");
    }
    
    // Update write time
    state->last_dll_write_time = GetFileWriteTime(dll_path);
    
    return true;
}

// Unload game module
static void UnloadGameModule(HotReloadState* state) {
    if (state->dll_handle) {
        dlclose(state->dll_handle);
        state->dll_handle = NULL;
    }
    memset(&g_hotreload.api, 0, sizeof(g_hotreload.api));
}

// Get file modification time
static u64 GetFileWriteTime(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

// Copy file (for loading without locking original)
static void CopyFile(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    FILE* out = fopen(dst, "wb");
    
    if (in && out) {
        char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            fwrite(buffer, 1, bytes, out);
        }
    }
    
    if (in) fclose(in);
    if (out) fclose(out);
}

// Calculate simple checksum
static u64 CalculateChecksum(void* data, usize size) {
    u64 checksum = 0;
    u8* bytes = (u8*)data;
    for (usize i = 0; i < size; i++) {
        checksum = checksum * 31 + bytes[i];
    }
    return checksum;
}

// Asset handle management
static AssetHandle CreateAssetHandle(HotReloadState* state, u32 type) {
    AssetHandle handle = {
        .generation = state->current_generation,
        .index = state->asset_handle_count++,
        .type = type,
        .flags = 0
    };
    
    // TODO: Add to handle array
    return handle;
}

static bool ValidateAssetHandle(HotReloadState* state, AssetHandle handle) {
    return handle.generation > 0 && handle.index < state->asset_handle_count;
}

// Hot reload statistics
static void DumpHotReloadStats(HotReloadState* state) {
    printf("=== Hot Reload Statistics ===\n");
    printf("Reload count: %u\n", state->reload_count);
    printf("Last reload time: %.2fms\n", state->last_reload_time * 1000.0);
    printf("Average reload time: %.2fms\n", 
           state->reload_count > 0 ? (state->total_reload_time / state->reload_count) * 1000.0 : 0.0);
    printf("Current generation: %u\n", state->current_generation);
    printf("Asset handles: %u\n", state->asset_handle_count);
    printf("State buffer size: %zu bytes\n", state->state_buffer_size);
}

// Initialize API structure
static HotReloadAPI g_api = {
    .InitHotReload = InitHotReload,
    .CheckForReload = CheckForReload,
    .PerformReload = PerformReload,
    .CreateAssetHandle = CreateAssetHandle,
    .ValidateAssetHandle = ValidateAssetHandle,
    .DumpHotReloadStats = DumpHotReloadStats,
};

HotReloadAPI* HotReload = &g_api;

// Get the game module API (for platform layer)
GameModuleAPI* GetCurrentGameAPI(void) {
    return &g_hotreload.api;
}