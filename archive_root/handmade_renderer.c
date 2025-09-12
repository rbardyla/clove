#include "handmade_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// BMP file structures for texture loading
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

// Internal helper functions
static void CreateWhiteTexture(Renderer* renderer);
static void CreateDefaultFont(Renderer* renderer);
static void SetupOpenGLState(void);
static void ApplyCameraTransform(Camera2D* camera);

bool RendererInit(Renderer* renderer, u32 viewport_width, u32 viewport_height) {
    if (!renderer) return false;
    
    memset(renderer, 0, sizeof(Renderer));
    
    renderer->viewport_width = viewport_width;
    renderer->viewport_height = viewport_height;
    
    // Initialize camera
    f32 aspect_ratio = (f32)viewport_width / (f32)viewport_height;
    Camera2DInit(&renderer->camera, aspect_ratio);
    
    // Setup OpenGL state
    SetupOpenGLState();
    
    // Create default white texture for solid color rendering
    CreateWhiteTexture(renderer);
    
    // Create default bitmap font for text rendering
    CreateDefaultFont(renderer);
    
    renderer->initialized = true;
    
    printf("Renderer initialized: %dx%d viewport\n", viewport_width, viewport_height);
    return true;
}

void RendererShutdown(Renderer* renderer) {
    if (!renderer || !renderer->initialized) return;
    
    if (renderer->white_texture.valid) {
        glDeleteTextures(1, &renderer->white_texture.id);
    }
    
    if (renderer->default_font.valid) {
        glDeleteTextures(1, &renderer->default_font.texture.id);
    }
    
    renderer->initialized = false;
    printf("Renderer shutdown\n");
}

void RendererBeginFrame(Renderer* renderer) {
    if (!renderer || !renderer->initialized) return;
    
    // Reset frame stats
    renderer->draw_calls = 0;
    renderer->vertices_drawn = 0;
    renderer->quad_count = 0;
    renderer->triangle_count = 0;
    
    // Set viewport
    glViewport(0, 0, renderer->viewport_width, renderer->viewport_height);
    
    // Setup projection matrix for 2D rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Setup camera transform
    ApplyCameraTransform(&renderer->camera);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable textures
    glEnable(GL_TEXTURE_2D);
}

void RendererEndFrame(Renderer* renderer) {
    if (!renderer || !renderer->initialized) return;
    
    // Disable states
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void RendererSetViewport(Renderer* renderer, u32 width, u32 height) {
    if (!renderer || !renderer->initialized) return;
    
    renderer->viewport_width = width;
    renderer->viewport_height = height;
    renderer->camera.aspect_ratio = (f32)width / (f32)height;
}

void RendererSetCamera(Renderer* renderer, Camera2D* camera) {
    if (!renderer || !renderer->initialized || !camera) return;
    renderer->camera = *camera;
}

Texture RendererLoadTextureBMP(Renderer* renderer, const char* filepath) {
    Texture result = {0};
    
    if (!renderer || !renderer->initialized || !filepath) {
        return result;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        printf("Failed to open BMP file: %s\n", filepath);
        return result;
    }
    
    // Read BMP header
    BMPHeader header;
    if (fread(&header, sizeof(BMPHeader), 1, file) != 1) {
        printf("Failed to read BMP header\n");
        fclose(file);
        return result;
    }
    
    // Check if it's a BMP file
    if (header.type != 0x4D42) { // "BM" in little endian
        printf("File is not a BMP: %s\n", filepath);
        fclose(file);
        return result;
    }
    
    // Read info header
    BMPInfoHeader info;
    if (fread(&info, sizeof(BMPInfoHeader), 1, file) != 1) {
        printf("Failed to read BMP info header\n");
        fclose(file);
        return result;
    }
    
    // We only support 24-bit RGB and 32-bit RGBA BMPs
    if (info.bits_per_pixel != 24 && info.bits_per_pixel != 32) {
        printf("Unsupported BMP format: %d bits per pixel\n", info.bits_per_pixel);
        fclose(file);
        return result;
    }
    
    if (info.compression != 0) {
        printf("Compressed BMPs not supported\n");
        fclose(file);
        return result;
    }
    
    u32 width = (u32)abs(info.width);
    u32 height = (u32)abs(info.height);
    u32 bytes_per_pixel = info.bits_per_pixel / 8;
    u32 image_size = width * height * bytes_per_pixel;
    
    // Seek to image data
    fseek(file, header.offset, SEEK_SET);
    
    // Allocate temporary buffer
    u8* image_data = malloc(image_size);
    if (!image_data) {
        printf("Failed to allocate memory for BMP data\n");
        fclose(file);
        return result;
    }
    
    // Read image data
    if (fread(image_data, image_size, 1, file) != 1) {
        printf("Failed to read BMP image data\n");
        free(image_data);
        fclose(file);
        return result;
    }
    
    fclose(file);
    
    // Convert BGR to RGB (BMPs store colors in BGR format)
    for (u32 i = 0; i < image_size; i += bytes_per_pixel) {
        u8 temp = image_data[i];     // Blue
        image_data[i] = image_data[i + 2]; // Red -> Blue
        image_data[i + 2] = temp;    // Blue -> Red
        // Green stays in place
    }
    
    // Create OpenGL texture
    glGenTextures(1, &result.id);
    glBindTexture(GL_TEXTURE_2D, result.id);
    
    GLenum format = (bytes_per_pixel == 4) ? GL_RGBA : GL_RGB;
    
    // Flip vertically if needed (BMPs can be stored upside down)
    if (info.height < 0) {
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);
    } else {
        // Need to flip the image
        u8* flipped_data = malloc(image_size);
        u32 row_size = width * bytes_per_pixel;
        for (u32 y = 0; y < height; y++) {
            u32 src_row = height - 1 - y;
            memcpy(flipped_data + y * row_size, image_data + src_row * row_size, row_size);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, flipped_data);
        free(flipped_data);
    }
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    free(image_data);
    
    result.width = width;
    result.height = height;
    result.valid = true;
    
    printf("Loaded BMP texture: %s (%dx%d, %d bpp)\n", filepath, width, height, info.bits_per_pixel);
    return result;
}

void RendererFreeTexture(Renderer* renderer, Texture* texture) {
    if (!renderer || !texture || !texture->valid) return;
    
    glDeleteTextures(1, &texture->id);
    memset(texture, 0, sizeof(Texture));
}

void RendererDrawQuad(Renderer* renderer, Quad* quad) {
    if (!renderer || !renderer->initialized || !quad) return;
    
    glBindTexture(GL_TEXTURE_2D, renderer->white_texture.id);
    
    glPushMatrix();
    glTranslatef(quad->position.x, quad->position.y, 0.0f);
    glRotatef(quad->rotation * 180.0f / M_PI, 0.0f, 0.0f, 1.0f);
    
    glColor4f(quad->color.r, quad->color.g, quad->color.b, quad->color.a);
    
    f32 half_width = quad->size.x * 0.5f;
    f32 half_height = quad->size.y * 0.5f;
    
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-half_width, -half_height);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( half_width, -half_height);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( half_width,  half_height);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-half_width,  half_height);
    glEnd();
    
    glPopMatrix();
    
    renderer->quad_count++;
    renderer->draw_calls++;
    renderer->vertices_drawn += 4;
}

void RendererDrawTriangle(Renderer* renderer, Triangle* triangle) {
    if (!renderer || !renderer->initialized || !triangle) return;
    
    glBindTexture(GL_TEXTURE_2D, renderer->white_texture.id);
    glColor4f(triangle->color.r, triangle->color.g, triangle->color.b, triangle->color.a);
    
    glBegin(GL_TRIANGLES);
        glTexCoord2f(0.5f, 1.0f); glVertex2f(triangle->p1.x, triangle->p1.y);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(triangle->p2.x, triangle->p2.y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(triangle->p3.x, triangle->p3.y);
    glEnd();
    
    renderer->triangle_count++;
    renderer->draw_calls++;
    renderer->vertices_drawn += 3;
}

void RendererDrawSprite(Renderer* renderer, Sprite* sprite) {
    if (!renderer || !renderer->initialized || !sprite) return;
    
    u32 texture_id = sprite->texture.valid ? sprite->texture.id : renderer->white_texture.id;
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glPushMatrix();
    glTranslatef(sprite->position.x, sprite->position.y, 0.0f);
    glRotatef(sprite->rotation * 180.0f / M_PI, 0.0f, 0.0f, 1.0f);
    
    glColor4f(sprite->color.r, sprite->color.g, sprite->color.b, sprite->color.a);
    
    f32 half_width = sprite->size.x * 0.5f;
    f32 half_height = sprite->size.y * 0.5f;
    
    // Calculate texture coordinates
    f32 u1 = sprite->texture_offset.x;
    f32 v1 = sprite->texture_offset.y;
    f32 u2 = u1 + sprite->texture_scale.x;
    f32 v2 = v1 + sprite->texture_scale.y;
    
    glBegin(GL_QUADS);
        glTexCoord2f(u1, v1); glVertex2f(-half_width, -half_height);
        glTexCoord2f(u2, v1); glVertex2f( half_width, -half_height);
        glTexCoord2f(u2, v2); glVertex2f( half_width,  half_height);
        glTexCoord2f(u1, v2); glVertex2f(-half_width,  half_height);
    glEnd();
    
    glPopMatrix();
    
    renderer->draw_calls++;
    renderer->vertices_drawn += 4;
}

void RendererDrawRect(Renderer* renderer, v2 position, v2 size, Color color) {
    Quad quad = {
        .position = position,
        .size = size,
        .rotation = 0.0f,
        .color = color
    };
    RendererDrawQuad(renderer, &quad);
}

void RendererDrawRectOutline(Renderer* renderer, v2 position, v2 size, f32 thickness, Color color) {
    if (!renderer || !renderer->initialized) return;
    
    f32 half_width = size.x * 0.5f;
    f32 half_height = size.y * 0.5f;
    f32 half_thickness = thickness * 0.5f;
    
    // Top
    RendererDrawRect(renderer, V2(position.x, position.y + half_height - half_thickness), 
                    V2(size.x, thickness), color);
    // Bottom
    RendererDrawRect(renderer, V2(position.x, position.y - half_height + half_thickness), 
                    V2(size.x, thickness), color);
    // Left
    RendererDrawRect(renderer, V2(position.x - half_width + half_thickness, position.y), 
                    V2(thickness, size.y - 2 * thickness), color);
    // Right
    RendererDrawRect(renderer, V2(position.x + half_width - half_thickness, position.y), 
                    V2(thickness, size.y - 2 * thickness), color);
}

void RendererDrawCircle(Renderer* renderer, v2 center, f32 radius, Color color, u32 segments) {
    if (!renderer || !renderer->initialized || segments < 3) return;
    
    glBindTexture(GL_TEXTURE_2D, renderer->white_texture.id);
    glColor4f(color.r, color.g, color.b, color.a);
    
    glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(center.x, center.y);
        
        for (u32 i = 0; i <= segments; i++) {
            f32 angle = 2.0f * M_PI * i / segments;
            f32 x = center.x + radius * cosf(angle);
            f32 y = center.y + radius * sinf(angle);
            f32 u = 0.5f + 0.5f * cosf(angle);
            f32 v = 0.5f + 0.5f * sinf(angle);
            glTexCoord2f(u, v);
            glVertex2f(x, y);
        }
    glEnd();
    
    renderer->draw_calls++;
    renderer->vertices_drawn += segments + 2;
}

void RendererDrawLine(Renderer* renderer, v2 start, v2 end, f32 thickness, Color color) {
    if (!renderer || !renderer->initialized) return;
    
    // Calculate perpendicular vector for thickness
    v2 dir = V2(end.x - start.x, end.y - start.y);
    f32 length = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (length < 1e-6f) return;
    
    dir.x /= length;
    dir.y /= length;
    
    v2 perp = V2(-dir.y * thickness * 0.5f, dir.x * thickness * 0.5f);
    
    glBindTexture(GL_TEXTURE_2D, renderer->white_texture.id);
    glColor4f(color.r, color.g, color.b, color.a);
    
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(start.x - perp.x, start.y - perp.y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(start.x + perp.x, start.y + perp.y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(end.x + perp.x, end.y + perp.y);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(end.x - perp.x, end.y - perp.y);
    glEnd();
    
    renderer->draw_calls++;
    renderer->vertices_drawn += 4;
}

void RendererDrawText(Renderer* renderer, v2 position, const char* text, f32 scale, Color color) {
    if (!renderer || !renderer->initialized || !text || !renderer->default_font.valid) return;
    
    f32 char_width = (f32)renderer->default_font.char_width * scale;
    f32 char_height = (f32)renderer->default_font.char_height * scale;
    f32 cursor_x = position.x;
    
    glBindTexture(GL_TEXTURE_2D, renderer->default_font.texture.id);
    glColor4f(color.r, color.g, color.b, color.a);
    
    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            cursor_x = position.x;
            position.y -= char_height;
            continue;
        }
        
        u8 ch = (u8)*c;
        if (ch < 32 || ch > 126) ch = '?'; // Replace invalid chars with '?'
        
        // Calculate UV coordinates for this character
        u32 char_index = ch - 32; // ASCII offset
        u32 char_x = char_index % renderer->default_font.chars_per_row;
        u32 char_y = char_index / renderer->default_font.chars_per_row;
        
        f32 u1 = (f32)char_x / renderer->default_font.chars_per_row;
        f32 v1 = (f32)char_y / 8.0f; // Assume 8 rows
        f32 u2 = u1 + (1.0f / renderer->default_font.chars_per_row);
        f32 v2 = v1 + (1.0f / 8.0f);
        
        // Draw character quad
        glBegin(GL_QUADS);
            glTexCoord2f(u1, v2); glVertex2f(cursor_x, position.y);
            glTexCoord2f(u2, v2); glVertex2f(cursor_x + char_width, position.y);
            glTexCoord2f(u2, v1); glVertex2f(cursor_x + char_width, position.y + char_height);
            glTexCoord2f(u1, v1); glVertex2f(cursor_x, position.y + char_height);
        glEnd();
        
        cursor_x += char_width;
        renderer->vertices_drawn += 4;
    }
    
    renderer->draw_calls++;
}

v2 RendererGetTextSize(Renderer* renderer, const char* text, f32 scale) {
    if (!renderer || !text || !renderer->default_font.valid) return V2(0, 0);
    
    f32 char_width = (f32)renderer->default_font.char_width * scale;
    f32 char_height = (f32)renderer->default_font.char_height * scale;
    
    f32 max_width = 0.0f;
    f32 current_width = 0.0f;
    f32 height = char_height;
    
    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            if (current_width > max_width) max_width = current_width;
            current_width = 0.0f;
            height += char_height;
        } else {
            current_width += char_width;
        }
    }
    
    if (current_width > max_width) max_width = current_width;
    
    return V2(max_width, height);
}

void RendererShowDebugInfo(Renderer* renderer) {
    if (!renderer || !renderer->initialized) return;
    
    printf("Renderer Debug Info:\n");
    printf("  Draw calls: %u\n", renderer->draw_calls);
    printf("  Vertices: %u\n", renderer->vertices_drawn);
    printf("  Quads: %u\n", renderer->quad_count);
    printf("  Triangles: %u\n", renderer->triangle_count);
    printf("  Camera: pos(%.2f, %.2f) zoom=%.2f rot=%.2f\n", 
           renderer->camera.position.x, renderer->camera.position.y,
           renderer->camera.zoom, renderer->camera.rotation);
}

// Internal helper functions
static void CreateWhiteTexture(Renderer* renderer) {
    u32 white_pixel = 0xFFFFFFFF; // White RGBA
    
    glGenTextures(1, &renderer->white_texture.id);
    glBindTexture(GL_TEXTURE_2D, renderer->white_texture.id);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    renderer->white_texture.width = 1;
    renderer->white_texture.height = 1;
    renderer->white_texture.valid = true;
}

static void CreateDefaultFont(Renderer* renderer) {
    // Create a simple 8x8 pixel bitmap font for ASCII characters 32-126
    const u32 FONT_WIDTH = 8;
    const u32 FONT_HEIGHT = 8;
    const u32 CHARS_PER_ROW = 16;
    const u32 CHAR_COUNT = 95; // ASCII 32-126
    const u32 ROWS = (CHAR_COUNT + CHARS_PER_ROW - 1) / CHARS_PER_ROW;
    
    const u32 TEXTURE_WIDTH = CHARS_PER_ROW * FONT_WIDTH;
    const u32 TEXTURE_HEIGHT = ROWS * FONT_HEIGHT;
    
    u8* font_data = malloc(TEXTURE_WIDTH * TEXTURE_HEIGHT);
    if (!font_data) {
        printf("Failed to allocate memory for font\n");
        return;
    }
    
    // Clear font data
    memset(font_data, 0, TEXTURE_WIDTH * TEXTURE_HEIGHT);
    
    // Simple hardcoded patterns for some common characters
    // This is a minimal implementation - in a real engine you'd load actual font data
    
    // Define some basic character patterns (8x8 bitmaps)
    u8 char_patterns[95][8] = {0}; // Initialize all to zero
    
    // Space (ASCII 32) - already zero
    
    // Exclamation mark (ASCII 33)
    char_patterns[33-32][0] = 0x18; // 00011000
    char_patterns[33-32][1] = 0x18; // 00011000
    char_patterns[33-32][2] = 0x18; // 00011000
    char_patterns[33-32][3] = 0x18; // 00011000
    char_patterns[33-32][4] = 0x00; // 00000000
    char_patterns[33-32][5] = 0x18; // 00011000
    char_patterns[33-32][6] = 0x00; // 00000000
    
    // A (ASCII 65)
    char_patterns[65-32][0] = 0x3C; // 00111100
    char_patterns[65-32][1] = 0x66; // 01100110
    char_patterns[65-32][2] = 0x66; // 01100110
    char_patterns[65-32][3] = 0x7E; // 01111110
    char_patterns[65-32][4] = 0x66; // 01100110
    char_patterns[65-32][5] = 0x66; // 01100110
    char_patterns[65-32][6] = 0x66; // 01100110
    
    // B (ASCII 66)
    char_patterns[66-32][0] = 0x7C; // 01111100
    char_patterns[66-32][1] = 0x66; // 01100110
    char_patterns[66-32][2] = 0x66; // 01100110
    char_patterns[66-32][3] = 0x7C; // 01111100
    char_patterns[66-32][4] = 0x66; // 01100110
    char_patterns[66-32][5] = 0x66; // 01100110
    char_patterns[66-32][6] = 0x7C; // 01111100
    
    // Add a simple rectangle pattern for unknown characters
    for (int i = 0; i < 95; i++) {
        bool is_empty = true;
        for (int j = 0; j < 8; j++) {
            if (char_patterns[i][j] != 0) {
                is_empty = false;
                break;
            }
        }
        
        // If character is empty, create a simple box pattern
        if (is_empty) {
            char_patterns[i][0] = 0xFF; // 11111111
            char_patterns[i][1] = 0x81; // 10000001
            char_patterns[i][2] = 0x81; // 10000001
            char_patterns[i][3] = 0x81; // 10000001
            char_patterns[i][4] = 0x81; // 10000001
            char_patterns[i][5] = 0x81; // 10000001
            char_patterns[i][6] = 0x81; // 10000001
            char_patterns[i][7] = 0xFF; // 11111111
        }
    }
    
    // Fill font texture data
    for (u32 char_idx = 0; char_idx < CHAR_COUNT; char_idx++) {
        u32 char_x = char_idx % CHARS_PER_ROW;
        u32 char_y = char_idx / CHARS_PER_ROW;
        
        for (u32 row = 0; row < FONT_HEIGHT; row++) {
            u8 pattern = char_patterns[char_idx][row];
            
            for (u32 col = 0; col < FONT_WIDTH; col++) {
                u32 x = char_x * FONT_WIDTH + col;
                u32 y = char_y * FONT_HEIGHT + row;
                u32 pixel_idx = y * TEXTURE_WIDTH + x;
                
                if (pixel_idx < TEXTURE_WIDTH * TEXTURE_HEIGHT) {
                    font_data[pixel_idx] = (pattern & (0x80 >> col)) ? 255 : 0;
                }
            }
        }
    }
    
    // Create OpenGL texture
    glGenTextures(1, &renderer->default_font.texture.id);
    glBindTexture(GL_TEXTURE_2D, renderer->default_font.texture.id);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEXTURE_WIDTH, TEXTURE_HEIGHT, 
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, font_data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    free(font_data);
    
    renderer->default_font.texture.width = TEXTURE_WIDTH;
    renderer->default_font.texture.height = TEXTURE_HEIGHT;
    renderer->default_font.texture.valid = true;
    renderer->default_font.char_width = FONT_WIDTH;
    renderer->default_font.char_height = FONT_HEIGHT;
    renderer->default_font.chars_per_row = CHARS_PER_ROW;
    renderer->default_font.valid = true;
    
    printf("Created default font texture: %dx%d\n", TEXTURE_WIDTH, TEXTURE_HEIGHT);
}

static void SetupOpenGLState(void) {
    // Enable depth testing but set up for 2D
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // Disable culling for 2D
    glDisable(GL_CULL_FACE);
    
    // Enable smooth lines and points
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

static void ApplyCameraTransform(Camera2D* camera) {
    f32 left = -camera->aspect_ratio / camera->zoom;
    f32 right = camera->aspect_ratio / camera->zoom;
    f32 bottom = -1.0f / camera->zoom;
    f32 top = 1.0f / camera->zoom;
    
    // Apply camera position offset
    left += camera->position.x;
    right += camera->position.x;
    bottom += camera->position.y;
    top += camera->position.y;
    
    // Set orthographic projection
    glOrtho(left, right, bottom, top, -1000.0f, 1000.0f);
    
    // Apply rotation if needed
    if (fabsf(camera->rotation) > 1e-6f) {
        glRotatef(camera->rotation * 180.0f / M_PI, 0.0f, 0.0f, 1.0f);
    }
}