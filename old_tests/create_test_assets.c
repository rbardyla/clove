/*
    Create Test Assets
    
    Simple program to generate test BMP textures and OBJ models
    for testing the asset browser.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// BMP file structures
#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;
#pragma pack(pop)

// Create a simple BMP texture
void create_bmp_texture(const char *filename, int width, int height, 
                       uint8_t r, uint8_t g, uint8_t b, int pattern) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to create %s\n", filename);
        return;
    }
    
    BMPHeader header = {0};
    BMPInfoHeader info = {0};
    
    int row_size = ((width * 3 + 3) / 4) * 4; // BMP rows are padded to 4 bytes
    int image_size = row_size * height;
    
    header.type = 0x4D42; // "BM"
    header.size = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + image_size;
    header.offset = sizeof(BMPHeader) + sizeof(BMPInfoHeader);
    
    info.size = sizeof(BMPInfoHeader);
    info.width = width;
    info.height = height;
    info.planes = 1;
    info.bits_per_pixel = 24;
    info.compression = 0;
    info.image_size = image_size;
    
    fwrite(&header, sizeof(BMPHeader), 1, file);
    fwrite(&info, sizeof(BMPInfoHeader), 1, file);
    
    // Create pixel data with different patterns
    uint8_t *row = (uint8_t*)calloc(row_size, 1);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t pr = r, pg = g, pb = b;
            
            switch (pattern) {
                case 0: // Solid color
                    break;
                    
                case 1: // Checkerboard
                    if (((x / 32) + (y / 32)) % 2) {
                        pr = 255 - r; pg = 255 - g; pb = 255 - b;
                    }
                    break;
                    
                case 2: // Gradient horizontal
                    pr = (r * x) / width;
                    pg = (g * x) / width;
                    pb = (b * x) / width;
                    break;
                    
                case 3: // Gradient vertical
                    pr = (r * y) / height;
                    pg = (g * y) / height;
                    pb = (b * y) / height;
                    break;
                    
                case 4: // Circle
                    {
                        int cx = width / 2;
                        int cy = height / 2;
                        int dx = x - cx;
                        int dy = y - cy;
                        if (dx*dx + dy*dy < (width/4)*(width/4)) {
                            pr = 255; pg = 255; pb = 255;
                        }
                    }
                    break;
                    
                case 5: // Stripes
                    if ((x / 16) % 2) {
                        pr = 255 - r; pg = 255 - g; pb = 255 - b;
                    }
                    break;
            }
            
            // BMP stores BGR
            row[x * 3 + 0] = pb;
            row[x * 3 + 1] = pg;
            row[x * 3 + 2] = pr;
        }
        fwrite(row, row_size, 1, file);
    }
    
    free(row);
    fclose(file);
    
    printf("Created %s (%dx%d)\n", filename, width, height);
}

// Create a simple OBJ model
void create_obj_model(const char *filename, int type) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Failed to create %s\n", filename);
        return;
    }
    
    fprintf(file, "# Simple OBJ model generated for testing\n");
    fprintf(file, "# Type: %d\n", type);
    
    switch (type) {
        case 0: // Cube
            fprintf(file, "# Cube\n");
            fprintf(file, "v -1.0 -1.0 -1.0\n");
            fprintf(file, "v  1.0 -1.0 -1.0\n");
            fprintf(file, "v  1.0  1.0 -1.0\n");
            fprintf(file, "v -1.0  1.0 -1.0\n");
            fprintf(file, "v -1.0 -1.0  1.0\n");
            fprintf(file, "v  1.0 -1.0  1.0\n");
            fprintf(file, "v  1.0  1.0  1.0\n");
            fprintf(file, "v -1.0  1.0  1.0\n");
            
            fprintf(file, "# Faces\n");
            fprintf(file, "f 1 2 3\n");
            fprintf(file, "f 1 3 4\n");
            fprintf(file, "f 5 7 6\n");
            fprintf(file, "f 5 8 7\n");
            fprintf(file, "f 1 5 6\n");
            fprintf(file, "f 1 6 2\n");
            fprintf(file, "f 2 6 7\n");
            fprintf(file, "f 2 7 3\n");
            fprintf(file, "f 3 7 8\n");
            fprintf(file, "f 3 8 4\n");
            fprintf(file, "f 4 8 5\n");
            fprintf(file, "f 4 5 1\n");
            break;
            
        case 1: // Pyramid
            fprintf(file, "# Pyramid\n");
            fprintf(file, "v  0.0  1.0  0.0\n");
            fprintf(file, "v -1.0 -1.0 -1.0\n");
            fprintf(file, "v  1.0 -1.0 -1.0\n");
            fprintf(file, "v  1.0 -1.0  1.0\n");
            fprintf(file, "v -1.0 -1.0  1.0\n");
            
            fprintf(file, "# Faces\n");
            fprintf(file, "f 1 2 3\n");
            fprintf(file, "f 1 3 4\n");
            fprintf(file, "f 1 4 5\n");
            fprintf(file, "f 1 5 2\n");
            fprintf(file, "f 2 5 4\n");
            fprintf(file, "f 2 4 3\n");
            break;
            
        case 2: // Simple plane
            fprintf(file, "# Plane\n");
            fprintf(file, "v -2.0 0.0 -2.0\n");
            fprintf(file, "v  2.0 0.0 -2.0\n");
            fprintf(file, "v  2.0 0.0  2.0\n");
            fprintf(file, "v -2.0 0.0  2.0\n");
            
            fprintf(file, "# Texture coords\n");
            fprintf(file, "vt 0.0 0.0\n");
            fprintf(file, "vt 1.0 0.0\n");
            fprintf(file, "vt 1.0 1.0\n");
            fprintf(file, "vt 0.0 1.0\n");
            
            fprintf(file, "# Normals\n");
            fprintf(file, "vn 0.0 1.0 0.0\n");
            
            fprintf(file, "# Faces\n");
            fprintf(file, "f 1/1/1 2/2/1 3/3/1\n");
            fprintf(file, "f 1/1/1 3/3/1 4/4/1\n");
            break;
    }
    
    fclose(file);
    printf("Created %s\n", filename);
}

// Create a simple WAV sound
void create_wav_sound(const char *filename, float frequency, float duration) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to create %s\n", filename);
        return;
    }
    
    // WAV header
    int sample_rate = 44100;
    int num_samples = (int)(sample_rate * duration);
    int byte_rate = sample_rate * 2; // 16-bit mono
    
    // RIFF header
    fwrite("RIFF", 4, 1, file);
    uint32_t chunk_size = 36 + num_samples * 2;
    fwrite(&chunk_size, 4, 1, file);
    fwrite("WAVE", 4, 1, file);
    
    // Format chunk
    fwrite("fmt ", 4, 1, file);
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels = 1; // Mono
    uint16_t bits_per_sample = 16;
    uint16_t block_align = 2;
    
    fwrite(&fmt_size, 4, 1, file);
    fwrite(&audio_format, 2, 1, file);
    fwrite(&num_channels, 2, 1, file);
    fwrite(&sample_rate, 4, 1, file);
    fwrite(&byte_rate, 4, 1, file);
    fwrite(&block_align, 2, 1, file);
    fwrite(&bits_per_sample, 2, 1, file);
    
    // Data chunk
    fwrite("data", 4, 1, file);
    uint32_t data_size = num_samples * 2;
    fwrite(&data_size, 4, 1, file);
    
    // Generate sine wave
    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float value = sinf(2.0f * 3.14159f * frequency * t);
        int16_t sample = (int16_t)(value * 32767.0f * 0.5f); // 50% volume
        fwrite(&sample, 2, 1, file);
    }
    
    fclose(file);
    printf("Created %s (%.1f Hz, %.1f sec)\n", filename, frequency, duration);
}

// Create a simple GLSL shader
void create_glsl_shader(const char *filename, int type) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Failed to create %s\n", filename);
        return;
    }
    
    switch (type) {
        case 0: // Vertex shader
            fprintf(file, "// Simple vertex shader\n");
            fprintf(file, "#version 330 core\n\n");
            fprintf(file, "layout(location = 0) in vec3 position;\n");
            fprintf(file, "layout(location = 1) in vec2 texCoord;\n\n");
            fprintf(file, "out vec2 uv;\n\n");
            fprintf(file, "uniform mat4 mvpMatrix;\n\n");
            fprintf(file, "void main() {\n");
            fprintf(file, "    gl_Position = mvpMatrix * vec4(position, 1.0);\n");
            fprintf(file, "    uv = texCoord;\n");
            fprintf(file, "}\n");
            break;
            
        case 1: // Fragment shader
            fprintf(file, "// Simple fragment shader\n");
            fprintf(file, "#version 330 core\n\n");
            fprintf(file, "in vec2 uv;\n");
            fprintf(file, "out vec4 fragColor;\n\n");
            fprintf(file, "uniform sampler2D texture0;\n");
            fprintf(file, "uniform vec4 tintColor;\n\n");
            fprintf(file, "void main() {\n");
            fprintf(file, "    vec4 texColor = texture(texture0, uv);\n");
            fprintf(file, "    fragColor = texColor * tintColor;\n");
            fprintf(file, "}\n");
            break;
    }
    
    fclose(file);
    printf("Created %s\n", filename);
}

int main() {
    printf("Creating test assets for the asset browser...\n\n");
    
    // Create textures
    printf("Creating textures...\n");
    create_bmp_texture("assets/textures/red_solid.bmp", 256, 256, 200, 50, 50, 0);
    create_bmp_texture("assets/textures/green_checker.bmp", 256, 256, 50, 200, 50, 1);
    create_bmp_texture("assets/textures/blue_gradient.bmp", 256, 256, 50, 50, 200, 2);
    create_bmp_texture("assets/textures/yellow_circle.bmp", 256, 256, 200, 200, 50, 4);
    create_bmp_texture("assets/textures/purple_stripes.bmp", 256, 256, 200, 50, 200, 5);
    create_bmp_texture("assets/textures/grass.bmp", 512, 512, 50, 150, 50, 1);
    create_bmp_texture("assets/textures/stone.bmp", 512, 512, 150, 150, 150, 1);
    create_bmp_texture("assets/textures/water.bmp", 512, 512, 50, 100, 200, 3);
    
    // Create models
    printf("\nCreating models...\n");
    create_obj_model("assets/models/cube.obj", 0);
    create_obj_model("assets/models/pyramid.obj", 1);
    create_obj_model("assets/models/plane.obj", 2);
    
    // Create sounds
    printf("\nCreating sounds...\n");
    create_wav_sound("assets/sounds/beep_440.wav", 440.0f, 0.5f);
    create_wav_sound("assets/sounds/beep_880.wav", 880.0f, 0.3f);
    create_wav_sound("assets/sounds/tone_low.wav", 220.0f, 1.0f);
    create_wav_sound("assets/sounds/tone_high.wav", 1760.0f, 0.2f);
    
    // Create shaders
    printf("\nCreating shaders...\n");
    create_glsl_shader("assets/shaders/basic.vert", 0);
    create_glsl_shader("assets/shaders/basic.frag", 1);
    
    // Create some placeholder files to show other types
    FILE *f;
    f = fopen("assets/README.txt", "w");
    if (f) {
        fprintf(f, "Test assets for the Handmade Engine Asset Browser\n");
        fprintf(f, "These are simple generated files for testing.\n");
        fclose(f);
    }
    
    f = fopen("assets/config.json", "w");
    if (f) {
        fprintf(f, "{\n  \"version\": \"1.0\",\n  \"test\": true\n}\n");
        fclose(f);
    }
    
    printf("\nAll test assets created successfully!\n");
    printf("Assets are in the 'assets/' directory.\n");
    
    return 0;
}