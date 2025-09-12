/*
    Handmade Asset Compiler
    Converts raw assets (PNG, OBJ, WAV) into .hma binary format
    
    Usage:
    ./asset_compiler texture test.png test_texture.hma
    ./asset_compiler mesh cube.obj cube_mesh.hma
    ./asset_compiler pack assets/ game_assets.hma
*/

#include "../systems/assets/handmade_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Simple texture loading (TGA format only - no dependencies)
typedef struct {
    u32 width;
    u32 height;
    u32 channels;
    u8* pixels;
} simple_texture;

// TGA header
typedef struct {
    u8 id_length;
    u8 colormap_type;
    u8 image_type;
    u16 colormap_index;
    u16 colormap_length;
    u8 colormap_size;
    u16 x_origin;
    u16 y_origin;
    u16 width;
    u16 height;
    u8 pixel_depth;
    u8 image_descriptor;
} __attribute__((packed)) tga_header;

simple_texture* load_tga(char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open TGA file: %s\n", filename);
        return 0;
    }
    
    tga_header header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        printf("Failed to read TGA header\n");
        fclose(file);
        return 0;
    }
    
    // Only support uncompressed RGB/RGBA
    if (header.image_type != 2) {
        printf("Unsupported TGA format (only uncompressed RGB supported)\n");
        fclose(file);
        return 0;
    }
    
    // Skip image ID
    if (header.id_length > 0) {
        fseek(file, header.id_length, SEEK_CUR);
    }
    
    u32 channels = header.pixel_depth / 8;
    u32 pixel_count = header.width * header.height;
    u32 data_size = pixel_count * channels;
    
    simple_texture* texture = malloc(sizeof(simple_texture));
    texture->width = header.width;
    texture->height = header.height;
    texture->channels = channels;
    texture->pixels = malloc(data_size);
    
    if (fread(texture->pixels, data_size, 1, file) != 1) {
        printf("Failed to read TGA pixel data\n");
        free(texture->pixels);
        free(texture);
        fclose(file);
        return 0;
    }
    
    fclose(file);
    
    // Convert BGR to RGB
    if (channels >= 3) {
        for (u32 i = 0; i < pixel_count; i++) {
            u8 temp = texture->pixels[i * channels];
            texture->pixels[i * channels] = texture->pixels[i * channels + 2];
            texture->pixels[i * channels + 2] = temp;
        }
    }
    
    return texture;
}

void free_texture(simple_texture* texture) {
    if (texture) {
        free(texture->pixels);
        free(texture);
    }
}

// Simple mesh loading (very basic OBJ parser)
typedef struct {
    u32 vertex_count;
    u32 index_count;
    float* vertices;  // x,y,z per vertex
    u32* indices;
} simple_mesh;

simple_mesh* load_obj(char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open OBJ file: %s\n", filename);
        return 0;
    }
    
    // Count vertices and faces first
    u32 vertex_count = 0;
    u32 face_count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') vertex_count++;
        if (line[0] == 'f' && line[1] == ' ') face_count++;
    }
    
    rewind(file);
    
    simple_mesh* mesh = malloc(sizeof(simple_mesh));
    mesh->vertex_count = vertex_count;
    mesh->index_count = face_count * 3; // Assuming triangles
    mesh->vertices = malloc(vertex_count * 3 * sizeof(float));
    mesh->indices = malloc(mesh->index_count * sizeof(u32));
    
    u32 v_index = 0;
    u32 i_index = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line, "v %f %f %f", &x, &y, &z) == 3) {
                mesh->vertices[v_index * 3 + 0] = x;
                mesh->vertices[v_index * 3 + 1] = y;
                mesh->vertices[v_index * 3 + 2] = z;
                v_index++;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            u32 a, b, c;
            if (sscanf(line, "f %u %u %u", &a, &b, &c) == 3) {
                mesh->indices[i_index++] = a - 1;  // OBJ is 1-indexed
                mesh->indices[i_index++] = b - 1;
                mesh->indices[i_index++] = c - 1;
            }
        }
    }
    
    fclose(file);
    return mesh;
}

void free_mesh(simple_mesh* mesh) {
    if (mesh) {
        free(mesh->vertices);
        free(mesh->indices);
        free(mesh);
    }
}

// Asset compilation
u32 calculate_checksum(u8* data, u64 size) {
    u32 crc = 0xFFFFFFFF;
    static u32 crc_table[256];
    static int table_computed = 0;
    
    if (!table_computed) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int j = 0; j < 8; j++) {
                if (c & 1) {
                    c = 0xEDB88320 ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            crc_table[i] = c;
        }
        table_computed = 1;
    }
    
    for (u64 i = 0; i < size; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

int compile_texture(char* input_path, char* output_path, char* asset_name) {
    printf("Compiling texture: %s -> %s\n", input_path, output_path);\n    \n    simple_texture* tex = load_tga(input_path);\n    if (!tex) {\n        return 1;\n    }\n    \n    FILE* out = fopen(output_path, \"wb\");\n    if (!out) {\n        printf("Failed to create output file: %s\n", output_path);\n        free_texture(tex);\n        return 1;\n    }\n    \n    // Create texture asset\n    texture_asset asset;\n    asset.width = tex->width;\n    asset.height = tex->height;\n    asset.channels = tex->channels;\n    asset.format = 0; // TODO: Set proper GL format\n    asset.pixels = tex->pixels;\n    \n    u64 asset_size = sizeof(texture_asset) + tex->width * tex->height * tex->channels;\n    \n    // Create header\n    asset_header header = {0};\n    header.magic = ASSET_MAGIC;\n    header.version = ASSET_VERSION;\n    header.type = ASSET_TYPE_TEXTURE;\n    header.compression = ASSET_COMPRESSION_NONE;\n    header.uncompressed_size = asset_size;\n    header.compressed_size = asset_size;\n    header.data_offset = 0;\n    strncpy(header.name, asset_name, sizeof(header.name) - 1);\n    \n    // Calculate checksum of asset data\n    u8* temp_buffer = malloc(asset_size);\n    memcpy(temp_buffer, &asset, sizeof(texture_asset));\n    memcpy(temp_buffer + sizeof(texture_asset), tex->pixels, \n           tex->width * tex->height * tex->channels);\n    \n    header.checksum = calculate_checksum(temp_buffer, asset_size);\n    \n    // Write header and data\n    fwrite(&header, sizeof(asset_header), 1, out);\n    fwrite(temp_buffer, asset_size, 1, out);\n    \n    fclose(out);\n    free(temp_buffer);\n    free_texture(tex);\n    \n    printf("Compiled texture: %ux%u, %u channels, %llu bytes\n",\n           asset.width, asset.height, asset.channels, asset_size);\n    \n    return 0;\n}\n\nint compile_mesh(char* input_path, char* output_path, char* asset_name) {\n    printf("Compiling mesh: %s -> %s\n", input_path, output_path);\n    \n    simple_mesh* mesh = load_obj(input_path);\n    if (!mesh) {\n        return 1;\n    }\n    \n    FILE* out = fopen(output_path, \"wb\");\n    if (!out) {\n        printf("Failed to create output file: %s\n", output_path);\n        free_mesh(mesh);\n        return 1;\n    }\n    \n    // Create mesh asset\n    mesh_asset asset;\n    asset.vertex_count = mesh->vertex_count;\n    asset.index_count = mesh->index_count;\n    asset.vertex_size = 3 * sizeof(float);  // x,y,z\n    asset.vertices = (u8*)mesh->vertices;\n    asset.indices = mesh->indices;\n    \n    // Calculate bounding box\n    asset.min_bounds = (v3){999999, 999999, 999999};\n    asset.max_bounds = (v3){-999999, -999999, -999999};\n    \n    for (u32 i = 0; i < mesh->vertex_count; i++) {\n        float x = mesh->vertices[i * 3 + 0];\n        float y = mesh->vertices[i * 3 + 1];\n        float z = mesh->vertices[i * 3 + 2];\n        \n        if (x < asset.min_bounds.x) asset.min_bounds.x = x;\n        if (y < asset.min_bounds.y) asset.min_bounds.y = y;\n        if (z < asset.min_bounds.z) asset.min_bounds.z = z;\n        \n        if (x > asset.max_bounds.x) asset.max_bounds.x = x;\n        if (y > asset.max_bounds.y) asset.max_bounds.y = y;\n        if (z > asset.max_bounds.z) asset.max_bounds.z = z;\n    }\n    \n    u64 vertex_data_size = mesh->vertex_count * 3 * sizeof(float);\n    u64 index_data_size = mesh->index_count * sizeof(u32);\n    u64 asset_size = sizeof(mesh_asset) + vertex_data_size + index_data_size;\n    \n    // Create header\n    asset_header header = {0};\n    header.magic = ASSET_MAGIC;\n    header.version = ASSET_VERSION;\n    header.type = ASSET_TYPE_MESH;\n    header.compression = ASSET_COMPRESSION_NONE;\n    header.uncompressed_size = asset_size;\n    header.compressed_size = asset_size;\n    header.data_offset = 0;\n    strncpy(header.name, asset_name, sizeof(header.name) - 1);\n    \n    // Calculate checksum\n    u8* temp_buffer = malloc(asset_size);\n    u8* ptr = temp_buffer;\n    \n    memcpy(ptr, &asset, sizeof(mesh_asset));\n    ptr += sizeof(mesh_asset);\n    \n    memcpy(ptr, mesh->vertices, vertex_data_size);\n    ptr += vertex_data_size;\n    \n    memcpy(ptr, mesh->indices, index_data_size);\n    \n    header.checksum = calculate_checksum(temp_buffer, asset_size);\n    \n    // Write header and data\n    fwrite(&header, sizeof(asset_header), 1, out);\n    fwrite(temp_buffer, asset_size, 1, out);\n    \n    fclose(out);\n    free(temp_buffer);\n    free_mesh(mesh);\n    \n    printf("Compiled mesh: %u vertices, %u indices, %llu bytes\n",\n           asset.vertex_count, asset.index_count, asset_size);\n    \n    return 0;\n}\n\nvoid print_usage(char* program_name) {\n    printf(\"Handmade Asset Compiler\\n\");\n    printf(\"Usage:\\n\");\n    printf(\"  %s texture <input.tga> <output.hma> [asset_name]\\n\", program_name);\n    printf(\"  %s mesh <input.obj> <output.hma> [asset_name]\\n\", program_name);\n    printf(\"\\n\");\n    printf(\"Supported formats:\\n\");\n    printf(\"  Textures: TGA (uncompressed RGB/RGBA)\\n\");\n    printf(\"  Meshes: OBJ (vertices and faces only)\\n\");\n}\n\nint main(int argc, char** argv) {\n    if (argc < 4) {\n        print_usage(argv[0]);\n        return 1;\n    }\n    \n    char* command = argv[1];\n    char* input_path = argv[2];\n    char* output_path = argv[3];\n    char* asset_name = argc > 4 ? argv[4] : \"unnamed\";\n    \n    if (strcmp(command, \"texture\") == 0) {\n        return compile_texture(input_path, output_path, asset_name);\n    } else if (strcmp(command, \"mesh\") == 0) {\n        return compile_mesh(input_path, output_path, asset_name);\n    } else {\n        printf("Unknown command: %s\n", command);\n        print_usage(argv[0]);\n        return 1;\n    }\n}\n"}