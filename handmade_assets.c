/*
    Handmade Asset System Implementation
    
    Provides:
    - Filesystem scanning with dirent.h
    - Basic image loading (BMP for now, will add PNG)
    - Simple OBJ model parser
    - WAV sound loader
    - Thumbnail generation
*/

#include "handmade_assets.h"
#include "simple_gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

// === INITIALIZATION ===

void asset_browser_init(AssetBrowser *browser, const char *root_directory) {
    memset(browser, 0, sizeof(AssetBrowser));
    
    // Set default directory
    strncpy(browser->current_directory, root_directory, MAX_PATH_LENGTH - 1);
    
    // Default display options
    browser->show_thumbnails = true;
    browser->show_details = false;
    browser->thumbnail_scale = 1;
    browser->selected_asset_index = -1;
    browser->hovered_asset_index = -1;
    browser->type_filter = ASSET_TYPE_UNKNOWN; // Show all
    
    // Scan the initial directory
    asset_browser_scan_directory(browser, root_directory);
}

// === FILESYSTEM SCANNING ===

void asset_browser_scan_directory(AssetBrowser *browser, const char *directory) {
    browser->asset_count = 0;
    strncpy(browser->current_directory, directory, MAX_PATH_LENGTH - 1);
    
    DIR *dir = opendir(directory);
    if (!dir) {
        printf("Failed to open directory: %s\n", directory);
        return;
    }
    
    struct dirent *entry;
    struct stat file_stat;
    char full_path[MAX_PATH_LENGTH];
    
    // Add parent directory entry if not at root
    if (strcmp(directory, "/") != 0 && strcmp(directory, "./") != 0) {
        Asset *parent = &browser->assets[browser->asset_count++];
        strcpy(parent->name, "..");
        strcpy(parent->path, directory);
        parent->type = ASSET_TYPE_FOLDER;
        parent->is_folder = true;
        parent->state = ASSET_STATE_LOADED;
    }
    
    // Scan all entries
    while ((entry = readdir(dir)) != NULL && browser->asset_count < MAX_ASSETS) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
            
        // Skip hidden files
        if (entry->d_name[0] == '.')
            continue;
        
        // Build full path
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", directory, entry->d_name);
        
        // Get file info
        if (stat(full_path, &file_stat) != 0)
            continue;
        
        Asset *asset = &browser->assets[browser->asset_count];
        memset(asset, 0, sizeof(Asset));
        
        // Fill basic info
        strncpy(asset->name, entry->d_name, MAX_NAME_LENGTH - 1);
        strncpy(asset->path, full_path, MAX_PATH_LENGTH - 1);
        asset->file_size = file_stat.st_size;
        asset->last_modified = file_stat.st_mtime;
        asset->state = ASSET_STATE_UNLOADED;
        
        // Determine type
        if (S_ISDIR(file_stat.st_mode)) {
            asset->type = ASSET_TYPE_FOLDER;
            asset->is_folder = true;
            asset->state = ASSET_STATE_LOADED; // Folders don't need loading
        } else {
            asset->type = asset_get_type_from_extension(entry->d_name);
            asset->is_folder = false;
        }
        
        browser->asset_count++;
    }
    
    closedir(dir);
    
    printf("Scanned %d assets in %s\n", browser->asset_count, directory);
}

// === ASSET TYPE DETECTION ===

AssetType asset_get_type_from_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return ASSET_TYPE_UNKNOWN;
    
    // Convert to lowercase for comparison
    char lower_ext[16];
    int i;
    for (i = 0; i < 15 && ext[i]; i++) {
        lower_ext[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    }
    lower_ext[i] = '\0';
    
    // Image formats
    if (strcmp(lower_ext, ".png") == 0 ||
        strcmp(lower_ext, ".jpg") == 0 ||
        strcmp(lower_ext, ".jpeg") == 0 ||
        strcmp(lower_ext, ".bmp") == 0 ||
        strcmp(lower_ext, ".tga") == 0) {
        return ASSET_TYPE_TEXTURE;
    }
    
    // Model formats
    if (strcmp(lower_ext, ".obj") == 0 ||
        strcmp(lower_ext, ".fbx") == 0 ||
        strcmp(lower_ext, ".gltf") == 0) {
        return ASSET_TYPE_MODEL;
    }
    
    // Sound formats
    if (strcmp(lower_ext, ".wav") == 0 ||
        strcmp(lower_ext, ".mp3") == 0 ||
        strcmp(lower_ext, ".ogg") == 0) {
        return ASSET_TYPE_SOUND;
    }
    
    // Shader formats
    if (strcmp(lower_ext, ".glsl") == 0 ||
        strcmp(lower_ext, ".vert") == 0 ||
        strcmp(lower_ext, ".frag") == 0) {
        return ASSET_TYPE_SHADER;
    }
    
    return ASSET_TYPE_UNKNOWN;
}

// === ASSET LOADING ===

bool asset_load(Asset *asset) {
    if (asset->state == ASSET_STATE_LOADED)
        return true;
        
    if (asset->is_folder)
        return true;
    
    asset->state = ASSET_STATE_LOADING;
    
    bool success = false;
    switch (asset->type) {
        case ASSET_TYPE_TEXTURE:
            success = asset_load_texture(asset);
            break;
        case ASSET_TYPE_MODEL:
            success = asset_load_obj_model(asset);
            break;
        case ASSET_TYPE_SOUND:
            success = asset_load_wav_sound(asset);
            break;
        default:
            break;
    }
    
    asset->state = success ? ASSET_STATE_LOADED : ASSET_STATE_ERROR;
    
    // Generate thumbnail after loading
    if (success) {
        asset_generate_thumbnail(asset);
    }
    
    return success;
}

void asset_unload(Asset *asset) {
    if (asset->state != ASSET_STATE_LOADED)
        return;
    
    switch (asset->type) {
        case ASSET_TYPE_TEXTURE:
            if (asset->data.texture.gl_texture_id) {
                glDeleteTextures(1, &asset->data.texture.gl_texture_id);
            }
            if (asset->data.texture.pixel_data) {
                free(asset->data.texture.pixel_data);
                asset->data.texture.pixel_data = NULL;
            }
            break;
            
        case ASSET_TYPE_MODEL:
            if (asset->data.model.vertices) free(asset->data.model.vertices);
            if (asset->data.model.normals) free(asset->data.model.normals);
            if (asset->data.model.tex_coords) free(asset->data.model.tex_coords);
            if (asset->data.model.indices) free(asset->data.model.indices);
            memset(&asset->data.model, 0, sizeof(ModelAsset));
            break;
            
        case ASSET_TYPE_SOUND:
            if (asset->data.sound.samples) {
                free(asset->data.sound.samples);
                asset->data.sound.samples = NULL;
            }
            break;
            
        default:
            break;
    }
    
    if (asset->thumbnail_texture_id) {
        glDeleteTextures(1, &asset->thumbnail_texture_id);
        asset->thumbnail_texture_id = 0;
    }
    
    asset->state = ASSET_STATE_UNLOADED;
}

// === TEXTURE LOADING (Simple BMP loader for now) ===

// Forward declaration for placeholder texture creation
bool asset_create_placeholder_texture(Asset *asset);

#pragma pack(push, 1)
typedef struct {
    u16 type;
    u32 size;
    u16 reserved1;
    u16 reserved2;
    u32 offset;
} BMPHeader;

typedef struct {
    u32 size;
    i32 width;
    i32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 image_size;
    i32 x_pixels_per_meter;
    i32 y_pixels_per_meter;
    u32 colors_used;
    u32 colors_important;
} BMPInfoHeader;
#pragma pack(pop)

bool asset_load_texture(Asset *asset) {
    // For now, only load BMP files
    const char *ext = strrchr(asset->path, '.');
    if (!ext) return false;
    
    // Convert to lowercase
    char lower_ext[16];
    int i;
    for (i = 0; i < 15 && ext[i]; i++) {
        lower_ext[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    }
    lower_ext[i] = '\0';
    
    if (strcmp(lower_ext, ".bmp") != 0) {
        printf("Warning: Only BMP textures supported currently. File: %s\n", asset->path);
        // For now, create a placeholder texture for non-BMP files
        return asset_create_placeholder_texture(asset);
    }
    
    FILE *file = fopen(asset->path, "rb");
    if (!file) return false;
    
    BMPHeader header;
    BMPInfoHeader info;
    
    fread(&header, sizeof(BMPHeader), 1, file);
    fread(&info, sizeof(BMPInfoHeader), 1, file);
    
    // Validate BMP
    if (header.type != 0x4D42) { // "BM"
        fclose(file);
        return false;
    }
    
    // Allocate pixel data
    i32 width = info.width;
    i32 height = info.height;
    i32 channels = info.bits_per_pixel / 8;
    
    if (channels != 3 && channels != 4) {
        printf("Unsupported BMP format: %d bits per pixel\n", info.bits_per_pixel);
        fclose(file);
        return false;
    }
    
    u32 image_size = width * height * channels;
    u8 *pixel_data = (u8*)malloc(image_size);
    
    // Read pixel data
    fseek(file, header.offset, SEEK_SET);
    fread(pixel_data, 1, image_size, file);
    fclose(file);
    
    // BMP stores BGR, convert to RGB
    for (i32 i = 0; i < width * height; i++) {
        u8 temp = pixel_data[i * channels + 0];
        pixel_data[i * channels + 0] = pixel_data[i * channels + 2];
        pixel_data[i * channels + 2] = temp;
    }
    
    // Flip vertically (BMP is bottom-up)
    u8 *temp_row = (u8*)malloc(width * channels);
    for (i32 y = 0; y < height / 2; y++) {
        memcpy(temp_row, pixel_data + y * width * channels, width * channels);
        memcpy(pixel_data + y * width * channels, 
               pixel_data + (height - 1 - y) * width * channels, width * channels);
        memcpy(pixel_data + (height - 1 - y) * width * channels, temp_row, width * channels);
    }
    free(temp_row);
    
    // Create OpenGL texture
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    GLenum format = (channels == 3) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixel_data);
    
    // Store in asset
    asset->data.texture.gl_texture_id = texture_id;
    asset->data.texture.width = width;
    asset->data.texture.height = height;
    asset->data.texture.channels = channels;
    asset->data.texture.pixel_data = pixel_data; // Keep for thumbnail generation
    
    printf("Loaded texture: %s (%dx%d, %d channels)\n", asset->name, width, height, channels);
    
    return true;
}

// Create placeholder texture for unsupported formats
bool asset_create_placeholder_texture(Asset *asset) {
    i32 width = 128;
    i32 height = 128;
    i32 channels = 3;
    
    u8 *pixel_data = (u8*)malloc(width * height * channels);
    
    // Create a checkerboard pattern
    for (i32 y = 0; y < height; y++) {
        for (i32 x = 0; x < width; x++) {
            i32 idx = (y * width + x) * channels;
            u8 val = ((x / 16) + (y / 16)) % 2 ? 200 : 100;
            pixel_data[idx + 0] = val;
            pixel_data[idx + 1] = val;
            pixel_data[idx + 2] = val;
        }
    }
    
    // Add colored border based on type
    color32 type_color = asset_get_type_color(asset->type);
    for (i32 i = 0; i < width; i++) {
        // Top and bottom
        i32 idx_top = i * channels;
        i32 idx_bot = ((height - 1) * width + i) * channels;
        pixel_data[idx_top + 0] = type_color.r;
        pixel_data[idx_top + 1] = type_color.g;
        pixel_data[idx_top + 2] = type_color.b;
        pixel_data[idx_bot + 0] = type_color.r;
        pixel_data[idx_bot + 1] = type_color.g;
        pixel_data[idx_bot + 2] = type_color.b;
    }
    for (i32 i = 0; i < height; i++) {
        // Left and right
        i32 idx_left = i * width * channels;
        i32 idx_right = (i * width + width - 1) * channels;
        pixel_data[idx_left + 0] = type_color.r;
        pixel_data[idx_left + 1] = type_color.g;
        pixel_data[idx_left + 2] = type_color.b;
        pixel_data[idx_right + 0] = type_color.r;
        pixel_data[idx_right + 1] = type_color.g;
        pixel_data[idx_right + 2] = type_color.b;
    }
    
    // Create OpenGL texture
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel_data);
    
    asset->data.texture.gl_texture_id = texture_id;
    asset->data.texture.width = width;
    asset->data.texture.height = height;
    asset->data.texture.channels = channels;
    asset->data.texture.pixel_data = pixel_data;
    
    return true;
}

// === MODEL LOADING (Simple OBJ parser) ===

bool asset_load_obj_model(Asset *asset) {
    FILE *file = fopen(asset->path, "r");
    if (!file) return false;
    
    // First pass: count elements
    u32 vertex_count = 0;
    u32 normal_count = 0;
    u32 texcoord_count = 0;
    u32 face_count = 0;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') vertex_count++;
        else if (line[0] == 'v' && line[1] == 'n') normal_count++;
        else if (line[0] == 'v' && line[1] == 't') texcoord_count++;
        else if (line[0] == 'f' && line[1] == ' ') face_count++;
    }
    
    // Allocate arrays
    f32 *vertices = (f32*)malloc(vertex_count * 3 * sizeof(f32));
    f32 *normals = normal_count > 0 ? (f32*)malloc(normal_count * 3 * sizeof(f32)) : NULL;
    f32 *texcoords = texcoord_count > 0 ? (f32*)malloc(texcoord_count * 2 * sizeof(f32)) : NULL;
    u32 *indices = (u32*)malloc(face_count * 3 * sizeof(u32)); // Assume triangles
    
    // Second pass: read data
    rewind(file);
    u32 v_idx = 0, n_idx = 0, t_idx = 0, f_idx = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            sscanf(line, "v %f %f %f", 
                   &vertices[v_idx * 3 + 0],
                   &vertices[v_idx * 3 + 1],
                   &vertices[v_idx * 3 + 2]);
            v_idx++;
        }
        else if (line[0] == 'v' && line[1] == 'n' && normals) {
            sscanf(line, "vn %f %f %f",
                   &normals[n_idx * 3 + 0],
                   &normals[n_idx * 3 + 1],
                   &normals[n_idx * 3 + 2]);
            n_idx++;
        }
        else if (line[0] == 'v' && line[1] == 't' && texcoords) {
            sscanf(line, "vt %f %f",
                   &texcoords[t_idx * 2 + 0],
                   &texcoords[t_idx * 2 + 1]);
            t_idx++;
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            // Simple case: just vertex indices
            u32 v1, v2, v3;
            if (sscanf(line, "f %u %u %u", &v1, &v2, &v3) == 3) {
                indices[f_idx * 3 + 0] = v1 - 1; // OBJ uses 1-based indexing
                indices[f_idx * 3 + 1] = v2 - 1;
                indices[f_idx * 3 + 2] = v3 - 1;
                f_idx++;
            }
            // Complex case: v/t/n format - just extract vertex indices for now
            else if (sscanf(line, "f %u/%*u/%*u %u/%*u/%*u %u/%*u/%*u", &v1, &v2, &v3) == 3) {
                indices[f_idx * 3 + 0] = v1 - 1;
                indices[f_idx * 3 + 1] = v2 - 1;
                indices[f_idx * 3 + 2] = v3 - 1;
                f_idx++;
            }
        }
    }
    
    fclose(file);
    
    // Store in asset
    asset->data.model.vertices = vertices;
    asset->data.model.normals = normals;
    asset->data.model.tex_coords = texcoords;
    asset->data.model.indices = indices;
    asset->data.model.vertex_count = vertex_count;
    asset->data.model.index_count = f_idx * 3;
    
    printf("Loaded model: %s (%u vertices, %u triangles)\n", 
           asset->name, vertex_count, f_idx);
    
    return true;
}

// === SOUND LOADING (Simple WAV parser) ===

bool asset_load_wav_sound(Asset *asset) {
    FILE *file = fopen(asset->path, "rb");
    if (!file) return false;
    
    // WAV header structures
    struct {
        char riff[4];
        u32 size;
        char wave[4];
    } header;
    
    struct {
        char id[4];
        u32 size;
    } chunk;
    
    struct {
        u16 format;
        u16 channels;
        u32 sample_rate;
        u32 byte_rate;
        u16 block_align;
        u16 bits_per_sample;
    } fmt;
    
    // Read main header
    fread(&header, sizeof(header), 1, file);
    if (memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0) {
        fclose(file);
        return false;
    }
    
    // Find format chunk
    bool found_fmt = false;
    bool found_data = false;
    
    while (!found_data && fread(&chunk, sizeof(chunk), 1, file) == 1) {
        if (memcmp(chunk.id, "fmt ", 4) == 0) {
            fread(&fmt, sizeof(fmt), 1, file);
            found_fmt = true;
        }
        else if (memcmp(chunk.id, "data", 4) == 0) {
            if (!found_fmt) {
                fclose(file);
                return false;
            }
            
            // Allocate and read sample data
            u32 sample_count = chunk.size / (fmt.bits_per_sample / 8);
            i16 *samples = (i16*)malloc(chunk.size);
            fread(samples, 1, chunk.size, file);
            
            // Store in asset
            asset->data.sound.samples = samples;
            asset->data.sound.sample_count = sample_count;
            asset->data.sound.sample_rate = fmt.sample_rate;
            asset->data.sound.channels = fmt.channels;
            
            found_data = true;
            
            printf("Loaded sound: %s (%u samples, %u Hz, %u channels)\n",
                   asset->name, sample_count, fmt.sample_rate, fmt.channels);
        }
        else {
            // Skip unknown chunk
            fseek(file, chunk.size, SEEK_CUR);
        }
    }
    
    fclose(file);
    return found_data;
}

// === THUMBNAIL GENERATION ===

bool asset_generate_thumbnail(Asset *asset) {
    switch (asset->type) {
        case ASSET_TYPE_TEXTURE:
            asset->thumbnail_texture_id = asset_create_texture_thumbnail(&asset->data.texture, THUMBNAIL_SIZE);
            asset->has_thumbnail = (asset->thumbnail_texture_id != 0);
            return asset->has_thumbnail;
            
        case ASSET_TYPE_MODEL:
            asset->thumbnail_texture_id = asset_create_model_thumbnail(&asset->data.model, THUMBNAIL_SIZE);
            asset->has_thumbnail = (asset->thumbnail_texture_id != 0);
            return asset->has_thumbnail;
            
        default:
            return false;
    }
}

GLuint asset_create_texture_thumbnail(TextureAsset *texture, i32 size) {
    if (!texture->pixel_data) return 0;
    
    // Create downsampled version
    u8 *thumbnail_data = (u8*)malloc(size * size * 3);
    
    f32 scale_x = (f32)texture->width / size;
    f32 scale_y = (f32)texture->height / size;
    
    for (i32 y = 0; y < size; y++) {
        for (i32 x = 0; x < size; x++) {
            i32 src_x = (i32)(x * scale_x);
            i32 src_y = (i32)(y * scale_y);
            
            if (src_x >= texture->width) src_x = texture->width - 1;
            if (src_y >= texture->height) src_y = texture->height - 1;
            
            i32 src_idx = (src_y * texture->width + src_x) * texture->channels;
            i32 dst_idx = (y * size + x) * 3;
            
            thumbnail_data[dst_idx + 0] = texture->pixel_data[src_idx + 0];
            thumbnail_data[dst_idx + 1] = texture->pixel_data[src_idx + 1];
            thumbnail_data[dst_idx + 2] = texture->pixel_data[src_idx + 2];
        }
    }
    
    // Create OpenGL texture
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, thumbnail_data);
    
    free(thumbnail_data);
    
    return texture_id;
}

GLuint asset_create_model_thumbnail(ModelAsset *model, i32 size) {
    // For now, create a simple wireframe thumbnail
    // In a full implementation, this would render the model to a texture
    
    u8 *thumbnail_data = (u8*)malloc(size * size * 3);
    memset(thumbnail_data, 40, size * size * 3); // Dark gray background
    
    // Draw a simple 3D cube icon
    for (i32 y = size/4; y < size*3/4; y++) {
        for (i32 x = size/4; x < size*3/4; x++) {
            i32 idx = (y * size + x) * 3;
            // Simple gradient to suggest 3D
            u8 val = 100 + (x - size/4) * 100 / (size/2);
            thumbnail_data[idx + 0] = val;
            thumbnail_data[idx + 1] = val;
            thumbnail_data[idx + 2] = 150; // Blueish tint for models
        }
    }
    
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, thumbnail_data);
    
    free(thumbnail_data);
    
    return texture_id;
}

// === GUI INTEGRATION ===

void asset_browser_draw(AssetBrowser *browser, simple_gui *gui, i32 x, i32 y, i32 width, i32 height) {
    // Draw background
    renderer_fill_rect(gui->r, x, y, width, height, rgb(30, 30, 30));
    
    // Draw header with current path
    renderer_fill_rect(gui->r, x, y, width, 24, rgb(50, 50, 50));
    simple_gui_text(gui, x + 4, y + 4, browser->current_directory);
    
    // View options buttons
    i32 btn_x = x + width - 120;
    if (simple_gui_button(gui, btn_x, y + 2, browser->show_thumbnails ? "List" : "Thumbs")) {
        browser->show_thumbnails = !browser->show_thumbnails;
    }
    
    // Asset grid/list area
    i32 content_y = y + 28;
    i32 content_height = height - 28;
    
    if (browser->show_thumbnails) {
        // Thumbnail grid view
        i32 item_size = THUMBNAIL_SIZE * browser->thumbnail_scale + 20;
        i32 cols = width / (item_size + 8);
        if (cols < 1) cols = 1;
        
        i32 item_x = x + 8;
        i32 item_y = content_y + 8;
        i32 col = 0;
        
        for (i32 i = 0; i < browser->asset_count; i++) {
            Asset *asset = &browser->assets[i];
            
            // Check if item is visible
            if (item_y > y + height) break;
            
            bool selected = (i == browser->selected_asset_index);
            bool hovered = false;
            
            // Check hover
            if (gui->mouse_x >= item_x && gui->mouse_x < item_x + item_size &&
                gui->mouse_y >= item_y && gui->mouse_y < item_y + item_size) {
                hovered = true;
                browser->hovered_asset_index = i;
                
                // Handle click
                if (gui->mouse_left_clicked) {
                    browser->selected_asset_index = i;
                    
                    // Double-click to open folder
                    static i32 last_click_index = -1;
                    static f32 last_click_time = 0;
                    f32 current_time = gui->frame_time;
                    
                    if (last_click_index == i && current_time - last_click_time < 0.5f) {
                        if (asset->is_folder) {
                            // Navigate to folder
                            if (strcmp(asset->name, "..") == 0) {
                                // Go to parent directory
                                char *last_slash = strrchr(browser->current_directory, '/');
                                if (last_slash && last_slash != browser->current_directory) {
                                    *last_slash = '\0';
                                    asset_browser_scan_directory(browser, browser->current_directory);
                                }
                            } else {
                                // Enter subdirectory
                                char new_path[MAX_PATH_LENGTH];
                                snprintf(new_path, MAX_PATH_LENGTH, "%s/%s", 
                                        browser->current_directory, asset->name);
                                asset_browser_scan_directory(browser, new_path);
                            }
                        } else {
                            // Load asset if not already loaded
                            if (asset->state == ASSET_STATE_UNLOADED) {
                                asset_load(asset);
                            }
                        }
                    }
                    
                    last_click_index = i;
                    last_click_time = current_time;
                }
            }
            
            // Draw item
            asset_draw_item(asset, gui, item_x, item_y, item_size, selected || hovered);
            
            // Move to next position
            col++;
            if (col >= cols) {
                col = 0;
                item_x = x + 8;
                item_y += item_size + 8;
            } else {
                item_x += item_size + 8;
            }
        }
    } else {
        // List view
        i32 line_height = 20;
        i32 list_y = content_y + 4;
        
        for (i32 i = 0; i < browser->asset_count; i++) {
            if (list_y > y + height) break;
            
            Asset *asset = &browser->assets[i];
            bool selected = (i == browser->selected_asset_index);
            
            // Draw selection background
            if (selected) {
                renderer_fill_rect(gui->r, x + 2, list_y - 2, width - 4, line_height, rgb(70, 70, 150));
            }
            
            // Draw icon based on type
            color32 type_color = asset_get_type_color(asset->type);
            renderer_fill_rect(gui->r, x + 8, list_y + 2, 12, 12, type_color);
            
            // Draw name
            simple_gui_text(gui, x + 24, list_y, asset->name);
            
            // Draw size
            if (!asset->is_folder) {
                char size_str[32];
                asset_format_size(asset->file_size, size_str, sizeof(size_str));
                simple_gui_text(gui, x + width - 80, list_y, size_str);
            }
            
            list_y += line_height;
        }
    }
}

void asset_draw_item(Asset *asset, simple_gui *gui, i32 x, i32 y, i32 size, bool selected) {
    // Draw background
    color32 bg_color = selected ? rgb(70, 70, 150) : rgb(45, 45, 45);
    renderer_fill_rect(gui->r, x, y, size, size, bg_color);
    
    i32 thumb_size = size - 20;
    i32 thumb_x = x + 10;
    i32 thumb_y = y + 4;
    
    // Draw thumbnail or icon
    if (asset->has_thumbnail && asset->thumbnail_texture_id) {
        // Draw actual thumbnail
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, asset->thumbnail_texture_id);
        
        glColor3f(1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(thumb_x, thumb_y);
        glTexCoord2f(1, 0); glVertex2i(thumb_x + thumb_size, thumb_y);
        glTexCoord2f(1, 1); glVertex2i(thumb_x + thumb_size, thumb_y + thumb_size);
        glTexCoord2f(0, 1); glVertex2i(thumb_x, thumb_y + thumb_size);
        glEnd();
        
        glDisable(GL_TEXTURE_2D);
    } else {
        // Draw placeholder icon
        color32 type_color = asset_get_type_color(asset->type);
        renderer_fill_rect(gui->r, thumb_x, thumb_y, thumb_size, thumb_size, type_color);
        
        // Draw type letter
        const char *type_name = asset_get_type_name(asset->type);
        char type_letter[2] = {type_name[0], '\0'};
        simple_gui_text(gui, thumb_x + thumb_size/2 - 4, thumb_y + thumb_size/2 - 8, type_letter);
    }
    
    // Draw name (truncated if needed)
    char truncated_name[16];
    strncpy(truncated_name, asset->name, 15);
    truncated_name[15] = '\0';
    if (strlen(asset->name) > 15) {
        truncated_name[12] = '.';
        truncated_name[13] = '.';
        truncated_name[14] = '.';
    }
    
    simple_gui_text(gui, x + 2, y + size - 16, truncated_name);
    
    // Draw loading indicator
    if (asset->state == ASSET_STATE_LOADING) {
        renderer_draw_rect(gui->r, x + 2, y + 2, size - 4, size - 4, rgb(255, 255, 0));
    } else if (asset->state == ASSET_STATE_ERROR) {
        renderer_draw_rect(gui->r, x + 2, y + 2, size - 4, size - 4, rgb(255, 0, 0));
    }
}

// === UTILITY FUNCTIONS ===

color32 asset_get_type_color(AssetType type) {
    switch (type) {
        case ASSET_TYPE_TEXTURE: return rgb(100, 200, 100); // Green
        case ASSET_TYPE_MODEL:   return rgb(100, 150, 200); // Blue
        case ASSET_TYPE_SOUND:   return rgb(200, 150, 100); // Orange
        case ASSET_TYPE_SHADER:  return rgb(200, 100, 200); // Purple
        case ASSET_TYPE_FOLDER:  return rgb(200, 200, 100); // Yellow
        default:                 return rgb(150, 150, 150); // Gray
    }
}

const char* asset_get_type_name(AssetType type) {
    switch (type) {
        case ASSET_TYPE_TEXTURE: return "Texture";
        case ASSET_TYPE_MODEL:   return "Model";
        case ASSET_TYPE_SOUND:   return "Sound";
        case ASSET_TYPE_SHADER:  return "Shader";
        case ASSET_TYPE_FOLDER:  return "Folder";
        default:                 return "Unknown";
    }
}

void asset_format_size(u64 bytes, char *buffer, i32 buffer_size) {
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%lu B", (unsigned long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

// === FILE OPERATIONS ===

bool asset_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

u64 asset_get_file_time(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

u64 asset_get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

u8* asset_read_entire_file(const char *path, u64 *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    u8 *buffer = (u8*)malloc(size);
    fread(buffer, 1, size, file);
    fclose(file);
    
    if (out_size) *out_size = size;
    return buffer;
}

// Handle asset browser input (keyboard navigation, etc)
bool asset_browser_handle_input(AssetBrowser *browser, PlatformState *platform) {
    // Handle keyboard navigation
    if (platform->input.keys[KEY_UP].pressed && browser->selected_asset_index > 0) {
        browser->selected_asset_index--;
        return true;
    }
    
    if (platform->input.keys[KEY_DOWN].pressed && browser->selected_asset_index < browser->asset_count - 1) {
        browser->selected_asset_index++;
        return true;
    }
    
    // Enter key to open folder or load asset
    if (platform->input.keys[KEY_ENTER].pressed && browser->selected_asset_index >= 0) {
        Asset *asset = &browser->assets[browser->selected_asset_index];
        
        if (asset->is_folder) {
            if (strcmp(asset->name, "..") == 0) {
                // Go to parent directory
                char *last_slash = strrchr(browser->current_directory, '/');
                if (last_slash && last_slash != browser->current_directory) {
                    *last_slash = '\0';
                    asset_browser_scan_directory(browser, browser->current_directory);
                }
            } else {
                // Enter subdirectory
                char new_path[MAX_PATH_LENGTH];
                snprintf(new_path, MAX_PATH_LENGTH, "%s/%s", 
                        browser->current_directory, asset->name);
                asset_browser_scan_directory(browser, new_path);
            }
        } else {
            // Load asset if not already loaded
            if (asset->state == ASSET_STATE_UNLOADED) {
                asset_load(asset);
            }
        }
        return true;
    }
    
    return false;
}