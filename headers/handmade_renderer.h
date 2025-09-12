#ifndef HANDMADE_RENDERER_H
#define HANDMADE_RENDERER_H

#include "handmade_platform.h"
#include <GL/gl.h>

// Math types and functions for renderer
typedef struct {
    f32 x, y;
} v2;

typedef struct {
    f32 x, y, z;
} v3;

typedef struct {
    f32 x, y, z, w;
} v4;

typedef struct {
    f32 r, g, b, a;
} Color;

// Camera for 2D rendering
typedef struct {
    v2 position;
    f32 zoom;
    f32 rotation;
    f32 aspect_ratio;
} Camera2D;

// Texture representation
typedef struct {
    u32 id;
    u32 width;
    u32 height;
    bool valid;
} Texture;

// Sprite for 2D rendering
typedef struct {
    v2 position;
    v2 size;
    f32 rotation;
    Color color;
    Texture texture;
    v2 texture_offset;  // UV offset
    v2 texture_scale;   // UV scale
} Sprite;

// Basic shapes
typedef struct {
    v2 position;
    v2 size;
    f32 rotation;
    Color color;
} Quad;

typedef struct {
    v2 p1, p2, p3;
    Color color;
} Triangle;

// Font structure for text rendering
typedef struct {
    Texture texture;
    u32 char_width;
    u32 char_height;
    u32 chars_per_row;
    bool valid;
} Font;

// Renderer state
typedef struct {
    bool initialized;
    
    // Viewport
    u32 viewport_width;
    u32 viewport_height;
    
    // Camera
    Camera2D camera;
    
    // Batch rendering (simple immediate mode for now)
    u32 quad_count;
    u32 triangle_count;
    
    // Default white texture for solid colors
    Texture white_texture;
    
    // Default bitmap font for text rendering
    Font default_font;
    
    // Debug info
    u32 draw_calls;
    u32 vertices_drawn;
} Renderer;

// Math helper functions
static inline v2 V2(f32 x, f32 y) {
    v2 result = {x, y};
    return result;
}

static inline v3 V3(f32 x, f32 y, f32 z) {
    v3 result = {x, y, z};
    return result;
}

static inline Color COLOR(f32 r, f32 g, f32 b, f32 a) {
    Color result = {r, g, b, a};
    return result;
}

// Common colors
#define COLOR_WHITE  COLOR(1.0f, 1.0f, 1.0f, 1.0f)
#define COLOR_BLACK  COLOR(0.0f, 0.0f, 0.0f, 1.0f)
#define COLOR_RED    COLOR(1.0f, 0.0f, 0.0f, 1.0f)
#define COLOR_GREEN  COLOR(0.0f, 1.0f, 0.0f, 1.0f)
#define COLOR_BLUE   COLOR(0.0f, 0.0f, 1.0f, 1.0f)
#define COLOR_YELLOW COLOR(1.0f, 1.0f, 0.0f, 1.0f)

// Camera functions
static inline void Camera2DInit(Camera2D* camera, f32 aspect_ratio) {
    camera->position = V2(0.0f, 0.0f);
    camera->zoom = 1.0f;
    camera->rotation = 0.0f;
    camera->aspect_ratio = aspect_ratio;
}

// Main renderer functions
bool RendererInit(Renderer* renderer, u32 viewport_width, u32 viewport_height);
void RendererShutdown(Renderer* renderer);
void RendererBeginFrame(Renderer* renderer);
void RendererEndFrame(Renderer* renderer);
void RendererSetViewport(Renderer* renderer, u32 width, u32 height);
void RendererSetCamera(Renderer* renderer, Camera2D* camera);

// Texture functions
Texture RendererLoadTextureBMP(Renderer* renderer, const char* filepath);
void RendererFreeTexture(Renderer* renderer, Texture* texture);

// Shape rendering functions
void RendererDrawQuad(Renderer* renderer, Quad* quad);
void RendererDrawTriangle(Renderer* renderer, Triangle* triangle);
void RendererDrawSprite(Renderer* renderer, Sprite* sprite);

// Convenience drawing functions
void RendererDrawRect(Renderer* renderer, v2 position, v2 size, Color color);
void RendererDrawRectOutline(Renderer* renderer, v2 position, v2 size, f32 thickness, Color color);
void RendererDrawCircle(Renderer* renderer, v2 center, f32 radius, Color color, u32 segments);
void RendererDrawLine(Renderer* renderer, v2 start, v2 end, f32 thickness, Color color);

// Text rendering functions
void RendererDrawText(Renderer* renderer, v2 position, const char* text, f32 scale, Color color);
v2 RendererGetTextSize(Renderer* renderer, const char* text, f32 scale);

// Debug functions
void RendererShowDebugInfo(Renderer* renderer);

#endif // HANDMADE_RENDERER_H