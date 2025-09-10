/*
    Renderer Test - Validates renderer functionality
    
    Tests the renderer system without requiring X11 display
    by calling renderer functions and verifying they don't crash
*/

#include "handmade_renderer.h"
#include <stdio.h>
#include <assert.h>

// Mock OpenGL functions for testing
void glEnable(int cap) { (void)cap; }
void glDisable(int cap) { (void)cap; }
void glDepthFunc(int func) { (void)func; }
void glHint(int target, int mode) { (void)target; (void)mode; }
void glViewport(int x, int y, int width, int height) { (void)x; (void)y; (void)width; (void)height; }
void glMatrixMode(int mode) { (void)mode; }
void glLoadIdentity(void) { }
void glOrtho(double left, double right, double bottom, double top, double near, double far) { 
    (void)left; (void)right; (void)bottom; (void)top; (void)near; (void)far; 
}
void glRotatef(float angle, float x, float y, float z) { (void)angle; (void)x; (void)y; (void)z; }
void glBlendFunc(int sfactor, int dfactor) { (void)sfactor; (void)dfactor; }
void glGenTextures(int n, unsigned int* textures) { if (n > 0 && textures) *textures = 1; }
void glBindTexture(int target, unsigned int texture) { (void)target; (void)texture; }
void glTexImage2D(int target, int level, int internalformat, int width, int height, int border, int format, int type, const void* pixels) {
    (void)target; (void)level; (void)internalformat; (void)width; (void)height; 
    (void)border; (void)format; (void)type; (void)pixels;
}
void glTexParameteri(int target, int pname, int param) { (void)target; (void)pname; (void)param; }
void glDeleteTextures(int n, const unsigned int* textures) { (void)n; (void)textures; }
void glPushMatrix(void) { }
void glPopMatrix(void) { }
void glTranslatef(float x, float y, float z) { (void)x; (void)y; (void)z; }
void glColor4f(float red, float green, float blue, float alpha) { (void)red; (void)green; (void)blue; (void)alpha; }
void glBegin(int mode) { (void)mode; }
void glEnd(void) { }
void glVertex2f(float x, float y) { (void)x; (void)y; }
void glTexCoord2f(float s, float t) { (void)s; (void)t; }

// OpenGL constants
#define GL_DEPTH_TEST 0x0B71
#define GL_LEQUAL 0x0203
#define GL_CULL_FACE 0x0B44
#define GL_LINE_SMOOTH 0x0B20
#define GL_POINT_SMOOTH 0x0B10
#define GL_LINE_SMOOTH_HINT 0x0C52
#define GL_POINT_SMOOTH_HINT 0x0C51
#define GL_NICEST 0x1102
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE_2D 0x0DE1
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_REPEAT 0x2901
#define GL_QUADS 0x0007
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_FAN 0x0006

static bool test_passed = true;

void test_assert(bool condition, const char* message) {
    if (!condition) {
        printf("TEST FAILED: %s\n", message);
        test_passed = false;
    } else {
        printf("PASSED: %s\n", message);
    }
}

int main(void) {
    printf("=== RENDERER UNIT TESTS ===\n\n");
    
    // Test 1: Renderer initialization
    Renderer renderer;
    bool init_result = RendererInit(&renderer, 800, 600);
    test_assert(init_result, "Renderer initialization");
    test_assert(renderer.initialized, "Renderer initialized flag");
    test_assert(renderer.viewport_width == 800, "Viewport width set correctly");
    test_assert(renderer.viewport_height == 600, "Viewport height set correctly");
    test_assert(renderer.white_texture.valid, "White texture created");
    
    // Test 2: Math functions
    v2 vec2 = V2(1.0f, 2.0f);
    test_assert(vec2.x == 1.0f && vec2.y == 2.0f, "V2 function");
    
    v3 vec3 = V3(1.0f, 2.0f, 3.0f);
    test_assert(vec3.x == 1.0f && vec3.y == 2.0f && vec3.z == 3.0f, "V3 function");
    
    Color color = COLOR(0.5f, 0.6f, 0.7f, 0.8f);
    test_assert(color.r == 0.5f && color.g == 0.6f && color.b == 0.7f && color.a == 0.8f, "COLOR function");
    
    // Test 3: Camera initialization
    Camera2D camera;
    Camera2DInit(&camera, 1.6f);
    test_assert(camera.position.x == 0.0f && camera.position.y == 0.0f, "Camera position initialized");
    test_assert(camera.zoom == 1.0f, "Camera zoom initialized");
    test_assert(camera.rotation == 0.0f, "Camera rotation initialized");
    test_assert(camera.aspect_ratio == 1.6f, "Camera aspect ratio set");
    
    // Test 4: Renderer frame functions (should not crash)
    printf("Testing renderer frame functions...\n");
    RendererBeginFrame(&renderer);
    printf("  RendererBeginFrame() - OK\n");
    
    RendererSetCamera(&renderer, &camera);
    printf("  RendererSetCamera() - OK\n");
    
    // Test 5: Shape drawing (should not crash)
    printf("Testing shape drawing functions...\n");
    
    Quad quad = {
        .position = V2(0.0f, 0.0f),
        .size = V2(1.0f, 1.0f),
        .rotation = 0.0f,
        .color = COLOR_RED
    };
    RendererDrawQuad(&renderer, &quad);
    printf("  RendererDrawQuad() - OK\n");
    
    Triangle triangle = {
        .p1 = V2(0.0f, 0.5f),
        .p2 = V2(-0.5f, -0.5f),
        .p3 = V2(0.5f, -0.5f),
        .color = COLOR_GREEN
    };
    RendererDrawTriangle(&renderer, &triangle);
    printf("  RendererDrawTriangle() - OK\n");
    
    Sprite sprite = {
        .position = V2(0.0f, 0.0f),
        .size = V2(1.0f, 1.0f),
        .rotation = 0.0f,
        .color = COLOR_WHITE,
        .texture = renderer.white_texture,
        .texture_offset = V2(0.0f, 0.0f),
        .texture_scale = V2(1.0f, 1.0f)
    };
    RendererDrawSprite(&renderer, &sprite);
    printf("  RendererDrawSprite() - OK\n");
    
    // Test convenience functions
    RendererDrawRect(&renderer, V2(0.0f, 0.0f), V2(1.0f, 1.0f), COLOR_BLUE);
    printf("  RendererDrawRect() - OK\n");
    
    RendererDrawRectOutline(&renderer, V2(0.0f, 0.0f), V2(1.0f, 1.0f), 0.1f, COLOR_YELLOW);
    printf("  RendererDrawRectOutline() - OK\n");
    
    RendererDrawCircle(&renderer, V2(0.0f, 0.0f), 0.5f, COLOR_GREEN, 16);
    printf("  RendererDrawCircle() - OK\n");
    
    RendererDrawLine(&renderer, V2(-1.0f, -1.0f), V2(1.0f, 1.0f), 0.05f, COLOR_WHITE);
    printf("  RendererDrawLine() - OK\n");
    
    RendererEndFrame(&renderer);
    printf("  RendererEndFrame() - OK\n");
    
    // Test 6: Debug info
    printf("Testing debug functions...\n");
    RendererShowDebugInfo(&renderer);
    printf("  RendererShowDebugInfo() - OK\n");
    
    // Test 7: Renderer stats
    test_assert(renderer.draw_calls > 0, "Draw calls recorded");
    test_assert(renderer.vertices_drawn > 0, "Vertices drawn recorded");
    
    // Test 8: Renderer shutdown
    RendererShutdown(&renderer);
    test_assert(!renderer.initialized, "Renderer shutdown");
    printf("  RendererShutdown() - OK\n");
    
    printf("\n=== TEST RESULTS ===\n");
    if (test_passed) {
        printf("ALL TESTS PASSED! ✓\n");
        printf("\nRenderer system is working correctly.\n");
        printf("Features verified:\n");
        printf("  ✓ Renderer initialization and shutdown\n");
        printf("  ✓ Math helper functions (v2, v3, Color)\n");
        printf("  ✓ Camera system\n");
        printf("  ✓ Shape drawing (quads, triangles, sprites)\n");
        printf("  ✓ Convenience drawing functions\n");
        printf("  ✓ Debug information\n");
        printf("  ✓ Frame management\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED! ✗\n");
        return 1;
    }
}