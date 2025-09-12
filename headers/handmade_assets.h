/*
    Handmade Asset System
    
    Practical asset loading and management system that:
    - Scans filesystem for assets
    - Loads common formats (images, models, sounds)
    - Generates thumbnails for preview
    - Integrates with the GUI file browser
    
    Philosophy: Keep it simple, keep it working
*/

#ifndef HANDMADE_ASSETS_H
#define HANDMADE_ASSETS_H

#include "handmade_platform.h"
#include "minimal_renderer.h"
#include "simple_gui.h"  // Need this for GUI integration
#include <GL/gl.h>

// Maximum limits
#define MAX_ASSETS 1024
#define MAX_PATH_LENGTH 256
#define MAX_NAME_LENGTH 64
#define THUMBNAIL_SIZE 64

// Asset types we support
typedef enum {
    ASSET_TYPE_UNKNOWN = 0,
    ASSET_TYPE_TEXTURE,     // .png, .jpg, .bmp
    ASSET_TYPE_MODEL,       // .obj
    ASSET_TYPE_SOUND,       // .wav
    ASSET_TYPE_SHADER,      // .glsl
    ASSET_TYPE_FOLDER,      // Directory
    ASSET_TYPE_COUNT
} AssetType;

// Asset state
typedef enum {
    ASSET_STATE_UNLOADED = 0,
    ASSET_STATE_LOADING,
    ASSET_STATE_LOADED,
    ASSET_STATE_ERROR
} AssetState;

// Texture data
typedef struct {
    GLuint gl_texture_id;
    i32 width;
    i32 height;
    i32 channels;
    u8 *pixel_data;  // Keep a copy for thumbnail generation
} TextureAsset;

// Simple OBJ model data
typedef struct {
    f32 *vertices;      // x,y,z triplets
    f32 *normals;       // x,y,z triplets
    f32 *tex_coords;    // u,v pairs
    u32 *indices;       // Triangle indices
    u32 vertex_count;
    u32 index_count;
} ModelAsset;

// Sound data (WAV format)
typedef struct {
    i16 *samples;
    u32 sample_count;
    u32 sample_rate;
    u32 channels;
} SoundAsset;

// Asset metadata
typedef struct {
    char name[MAX_NAME_LENGTH];
    char path[MAX_PATH_LENGTH];
    AssetType type;
    AssetState state;
    u64 file_size;
    u64 last_modified;
    
    // Thumbnail for preview
    GLuint thumbnail_texture_id;
    bool has_thumbnail;
    
    // Asset data (union based on type)
    union {
        TextureAsset texture;
        ModelAsset model;
        SoundAsset sound;
    } data;
    
    // For folder navigation
    bool is_folder;
    i32 parent_index;  // Index of parent folder in asset array
} Asset;

// Asset browser state
typedef struct {
    Asset assets[MAX_ASSETS];
    i32 asset_count;
    
    // Current directory being viewed
    char current_directory[MAX_PATH_LENGTH];
    i32 current_folder_index;
    
    // Selection
    i32 selected_asset_index;
    i32 hovered_asset_index;
    
    // Display options
    bool show_thumbnails;
    bool show_details;
    i32 thumbnail_scale;  // 1=64x64, 2=128x128, etc
    
    // Search/filter
    char search_filter[64];
    AssetType type_filter;
    
    // Performance tracking
    f32 scan_time_ms;
    f32 load_time_ms;
    i32 textures_loaded;
    i32 models_loaded;
    i32 sounds_loaded;
} AssetBrowser;

// === Core Functions ===

// Initialize asset browser
void asset_browser_init(AssetBrowser *browser, const char *root_directory);

// Scan directory for assets
void asset_browser_scan_directory(AssetBrowser *browser, const char *directory);

// Load asset into memory/GPU
bool asset_load(Asset *asset);
void asset_unload(Asset *asset);

// === Specific Asset Loaders ===

// Texture loading (using minimal STB implementation)
bool asset_load_texture(Asset *asset);
bool asset_load_texture_from_memory(TextureAsset *texture, const u8 *data, i32 len);

// Model loading (simple OBJ parser)
bool asset_load_obj_model(Asset *asset);

// Sound loading (simple WAV parser)
bool asset_load_wav_sound(Asset *asset);

// === Thumbnail Generation ===

// Generate thumbnail for asset
bool asset_generate_thumbnail(Asset *asset);

// Create thumbnail from texture
GLuint asset_create_texture_thumbnail(TextureAsset *texture, i32 size);

// Create thumbnail for model (render to texture)
GLuint asset_create_model_thumbnail(ModelAsset *model, i32 size);

// === GUI Integration ===

// Draw asset browser panel
void asset_browser_draw(AssetBrowser *browser, simple_gui *gui, i32 x, i32 y, i32 width, i32 height);

// Draw single asset item (icon + name)
void asset_draw_item(Asset *asset, simple_gui *gui, i32 x, i32 y, i32 size, bool selected);

// Handle asset selection/double-click
bool asset_browser_handle_input(AssetBrowser *browser, PlatformState *platform);

// === Utility Functions ===

// Get asset type from file extension
AssetType asset_get_type_from_extension(const char *filename);

// Get icon color for asset type
color32 asset_get_type_color(AssetType type);

// Get human-readable type name
const char* asset_get_type_name(AssetType type);

// Format file size for display
void asset_format_size(u64 bytes, char *buffer, i32 buffer_size);

// === File Operations ===

// Check if file exists
bool asset_file_exists(const char *path);

// Get file modification time
u64 asset_get_file_time(const char *path);

// Get file size
u64 asset_get_file_size(const char *path);

// Read entire file into memory (caller must free)
u8* asset_read_entire_file(const char *path, u64 *out_size);

#endif // HANDMADE_ASSETS_H