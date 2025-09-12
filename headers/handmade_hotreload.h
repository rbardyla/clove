#ifndef HANDMADE_HOTRELOAD_H
#define HANDMADE_HOTRELOAD_H

/*
    Hot Reload System
    =================
    
    Production-grade hot reload with state preservation.
    - Zero-allocation design maintained
    - State serialization to fixed memory region
    - Function pointer patching
    - Asset handle stability
    - Module versioning for safety
    
    Architecture:
    [Platform] -> [Hot Reload Controller] -> [Game Module]
                            |
                    [State Preservation]
*/

#include "handmade_platform.h"

// Module version for detecting incompatible changes
#define HOTRELOAD_MODULE_VERSION 1

// State preservation magic number
#define HOTRELOAD_STATE_MAGIC 0x484F544C4F414421  // "HOTLOAD!"

// Maximum size for serialized state
#define HOTRELOAD_STATE_SIZE MEGABYTES(64)

// Function table version for vtable patching
typedef struct {
    u32 version;
    u32 function_count;
    void** functions;
} HotReloadFunctionTable;

// Serialization header for state preservation
typedef struct {
    u64 magic;
    u32 version;
    u32 state_size;
    u64 checksum;
    f64 timestamp;
} HotReloadStateHeader;

// Asset handle for stability across reloads
typedef struct {
    u32 generation;  // Incremented on reload
    u32 index;       // Stable index in asset array
    u32 type;        // Asset type (texture, mesh, shader, etc.)
    u32 flags;       // Status flags
} AssetHandle;

// Hot reload state (preserved across reloads)
typedef struct {
    // State preservation buffer
    void* state_buffer;
    usize state_buffer_size;
    
    // Function tables for patching
    HotReloadFunctionTable* function_tables;
    u32 function_table_count;
    
    // Asset handle mapping
    AssetHandle* asset_handles;
    u32 asset_handle_count;
    u32 current_generation;
    
    // Module tracking
    u64 last_dll_write_time;
    void* dll_handle;
    char dll_path[256];
    char dll_temp_path[256];
    
    // Reload statistics
    u32 reload_count;
    f64 total_reload_time;
    f64 last_reload_time;
    
    // State serialization callbacks
    void (*serialize_state)(void* buffer, usize* size);
    void (*deserialize_state)(void* buffer, usize size);
    
    // Function pointer patching callback
    void (*patch_function_pointers)(void);
    
    // Asset handle remapping callback
    void (*remap_asset_handles)(u32 old_generation, u32 new_generation);
} HotReloadState;

// Module exports structure
typedef struct {
    // Required exports
    void (*GameInit)(PlatformState* platform);
    void (*GameUpdate)(PlatformState* platform, f32 dt);
    void (*GameRender)(PlatformState* platform);
    void (*GameShutdown)(PlatformState* platform);
    
    // Hot reload specific
    void (*GameSerializeState)(void* buffer, usize* size);
    void (*GameDeserializeState)(void* buffer, usize size);
    void (*GameOnReload)(PlatformState* platform);
    void (*GameOnUnload)(PlatformState* platform);
    
    // Version info
    u32 (*GameGetVersion)(void);
    const char* (*GameGetBuildInfo)(void);
} GameModuleAPI;

// Hot reload API functions
typedef struct {
    // Core hot reload
    bool (*InitHotReload)(HotReloadState* state, const char* dll_path);
    bool (*CheckForReload)(HotReloadState* state);
    bool (*PerformReload)(HotReloadState* state, PlatformState* platform);
    void (*ShutdownHotReload)(HotReloadState* state);
    
    // State management
    bool (*SaveState)(HotReloadState* state);
    bool (*RestoreState)(HotReloadState* state);
    
    // Function patching
    void (*PatchFunctionTable)(HotReloadFunctionTable* table, void** new_functions);
    void (*RegisterFunctionTable)(HotReloadState* state, HotReloadFunctionTable* table);
    
    // Asset handle management
    AssetHandle (*CreateAssetHandle)(HotReloadState* state, u32 type);
    bool (*ValidateAssetHandle)(HotReloadState* state, AssetHandle handle);
    void (*RemapAssetHandles)(HotReloadState* state);
    
    // Debugging
    void (*DumpHotReloadStats)(HotReloadState* state);
    bool (*ValidateModuleCompatibility)(u32 module_version);
} HotReloadAPI;

// Global hot reload API
extern HotReloadAPI* HotReload;

// Macros for module export
#ifdef HOT_RELOAD_MODULE

#define GAME_MODULE_EXPORT extern "C" __attribute__((visibility("default")))

// Export the module API
GAME_MODULE_EXPORT GameModuleAPI* GetGameModuleAPI(void);

// Mark functions that should survive reload
#define HOT_RELOAD_PERSIST static
#define HOT_RELOAD_STATE static

#else

#define GAME_MODULE_EXPORT
#define HOT_RELOAD_PERSIST
#define HOT_RELOAD_STATE

#endif

// Helper macros for state serialization
#define SERIALIZE_VALUE(buffer, value) \
    do { \
        memcpy(buffer, &(value), sizeof(value)); \
        buffer = (u8*)buffer + sizeof(value); \
    } while(0)

#define DESERIALIZE_VALUE(buffer, value) \
    do { \
        memcpy(&(value), buffer, sizeof(value)); \
        buffer = (u8*)buffer + sizeof(value); \
    } while(0)

// Asset handle helpers
#define INVALID_ASSET_HANDLE ((AssetHandle){0, 0, 0, 0})
#define IS_VALID_ASSET_HANDLE(h) ((h).generation != 0 && (h).index != 0)

// Function table helpers
#define REGISTER_FUNCTION_TABLE(state, name, functions) \
    do { \
        static HotReloadFunctionTable name##_table = { \
            .version = HOTRELOAD_MODULE_VERSION, \
            .function_count = ARRAY_COUNT(functions), \
            .functions = (void**)functions \
        }; \
        HotReload->RegisterFunctionTable(state, &name##_table); \
    } while(0)

// Get current game API (for platform layer)
GameModuleAPI* GetCurrentGameAPI(void);

#endif // HANDMADE_HOTRELOAD_H