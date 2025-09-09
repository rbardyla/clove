#ifndef HANDMADE_RENDERER_NEW_H
#define HANDMADE_RENDERER_NEW_H

/*
    Handmade Renderer System
    =========================
    
    Production-grade OpenGL renderer with:
    - Zero-allocation render loop
    - Command buffer pattern (256MB arena)
    - Shader hot reload (<100ms)
    - Material system with live preview
    - Render graph for multi-pass rendering
*/

#include "../../handmade_platform.h"
#include <GL/gl.h>

// Forward declarations
typedef struct Renderer Renderer;
typedef struct RenderCommand RenderCommand;
typedef struct Material Material;
typedef struct Shader Shader;
typedef struct Texture Texture;
typedef struct Mesh Mesh;
typedef struct RenderTarget RenderTarget;

// Resource handles (stable across hot reload)
typedef struct { u32 id; u32 generation; } ShaderHandle;
typedef struct { u32 id; u32 generation; } TextureHandle;
typedef struct { u32 id; u32 generation; } MeshHandle;
typedef struct { u32 id; u32 generation; } MaterialHandle;
typedef struct { u32 id; u32 generation; } RenderTargetHandle;

#define INVALID_SHADER_HANDLE ((ShaderHandle){0, 0})
#define INVALID_TEXTURE_HANDLE ((TextureHandle){0, 0})
#define INVALID_MESH_HANDLE ((MeshHandle){0, 0})
#define INVALID_MATERIAL_HANDLE ((MaterialHandle){0, 0})
#define INVALID_RENDER_TARGET_HANDLE ((RenderTargetHandle){0, 0})

// Math types (temporary - will use your math library)
typedef struct { f32 x, y; } Vec2;
typedef struct { f32 x, y, z; } Vec3;
typedef struct { f32 x, y, z, w; } Vec4;
typedef struct { f32 m[16]; } Mat4;
typedef struct { f32 x, y, width, height; } Rect;

// Vertex formats
typedef enum {
    VERTEX_FORMAT_P3F,           // Position only (3 floats)
    VERTEX_FORMAT_P3F_N3F,       // Position + Normal
    VERTEX_FORMAT_P3F_N3F_T2F,   // Position + Normal + TexCoord
    VERTEX_FORMAT_P3F_N3F_T2F_T3F_B3F, // + Tangent + Bitangent
    VERTEX_FORMAT_P3F_N3F_T2F_C4U8,    // + Color
    VERTEX_FORMAT_COUNT
} VertexFormat;

// Primitive types
typedef enum {
    PRIMITIVE_TRIANGLES,
    PRIMITIVE_LINES,
    PRIMITIVE_POINTS,
    PRIMITIVE_TRIANGLE_STRIP,
    PRIMITIVE_COUNT
} PrimitiveType;

// Texture formats
typedef enum {
    TEXTURE_FORMAT_R8,
    TEXTURE_FORMAT_RG8,
    TEXTURE_FORMAT_RGB8,
    TEXTURE_FORMAT_RGBA8,
    TEXTURE_FORMAT_R16F,
    TEXTURE_FORMAT_RG16F,
    TEXTURE_FORMAT_RGB16F,
    TEXTURE_FORMAT_RGBA16F,
    TEXTURE_FORMAT_R32F,
    TEXTURE_FORMAT_DEPTH24_STENCIL8,
    TEXTURE_FORMAT_DEPTH32F,
    TEXTURE_FORMAT_COUNT
} TextureFormat;

// Texture filtering
typedef enum {
    TEXTURE_FILTER_NEAREST,
    TEXTURE_FILTER_LINEAR,
    TEXTURE_FILTER_TRILINEAR,
    TEXTURE_FILTER_ANISOTROPIC_2X,
    TEXTURE_FILTER_ANISOTROPIC_4X,
    TEXTURE_FILTER_ANISOTROPIC_8X,
    TEXTURE_FILTER_ANISOTROPIC_16X,
} TextureFilter;

// Texture wrap modes
typedef enum {
    TEXTURE_WRAP_REPEAT,
    TEXTURE_WRAP_CLAMP,
    TEXTURE_WRAP_MIRROR,
    TEXTURE_WRAP_BORDER,
} TextureWrap;

// Blend modes
typedef enum {
    BLEND_MODE_OPAQUE,
    BLEND_MODE_ALPHA,
    BLEND_MODE_ADDITIVE,
    BLEND_MODE_MULTIPLY,
    BLEND_MODE_PREMULTIPLIED,
    BLEND_MODE_COUNT
} BlendMode;

// Cull modes
typedef enum {
    CULL_MODE_NONE,
    CULL_MODE_BACK,
    CULL_MODE_FRONT,
    CULL_MODE_COUNT
} CullMode;

// Depth test modes
typedef enum {
    DEPTH_TEST_NONE,
    DEPTH_TEST_LESS,
    DEPTH_TEST_LESS_EQUAL,
    DEPTH_TEST_EQUAL,
    DEPTH_TEST_GREATER,
    DEPTH_TEST_GREATER_EQUAL,
    DEPTH_TEST_ALWAYS,
    DEPTH_TEST_COUNT
} DepthTestMode;

// Viewport
typedef struct {
    i32 x, y;
    u32 width, height;
    f32 min_depth, max_depth;
} Viewport;

// Render state
typedef struct {
    BlendMode blend_mode;
    CullMode cull_mode;
    DepthTestMode depth_test;
    bool depth_write;
    bool scissor_test;
    Rect scissor_rect;
} RenderState;

// Clear flags
typedef enum {
    CLEAR_COLOR = (1 << 0),
    CLEAR_DEPTH = (1 << 1),
    CLEAR_STENCIL = (1 << 2),
} ClearFlags;

// Command types
typedef enum {
    CMD_CLEAR,
    CMD_SET_VIEWPORT,
    CMD_SET_RENDER_TARGET,
    CMD_SET_SHADER,
    CMD_SET_MATERIAL,
    CMD_SET_RENDER_STATE,
    CMD_DRAW_MESH,
    CMD_DRAW_INDEXED,
    CMD_DRAW_IMMEDIATE,
    CMD_DISPATCH_COMPUTE,
    CMD_COPY_TEXTURE,
    CMD_PUSH_DEBUG_GROUP,
    CMD_POP_DEBUG_GROUP,
} CommandType;

// Render command
struct RenderCommand {
    CommandType type;
    union {
        struct {
            Vec4 color;
            f32 depth;
            u8 stencil;
            u32 flags;
        } clear;
        
        struct {
            Viewport viewport;
        } set_viewport;
        
        struct {
            RenderTargetHandle handle;
        } set_render_target;
        
        struct {
            ShaderHandle handle;
        } set_shader;
        
        struct {
            MaterialHandle handle;
        } set_material;
        
        struct {
            RenderState state;
        } set_render_state;
        
        struct {
            MeshHandle handle;
            u32 instance_count;
            Mat4* transforms;
        } draw_mesh;
        
        struct {
            void* vertices;
            u32 vertex_count;
            VertexFormat format;
            PrimitiveType primitive;
        } draw_immediate;
        
        struct {
            const char* name;
        } debug_group;
    };
};

// Material property types
typedef enum {
    MATERIAL_PROPERTY_FLOAT,
    MATERIAL_PROPERTY_VEC2,
    MATERIAL_PROPERTY_VEC3,
    MATERIAL_PROPERTY_VEC4,
    MATERIAL_PROPERTY_MAT4,
    MATERIAL_PROPERTY_TEXTURE,
    MATERIAL_PROPERTY_COUNT
} MaterialPropertyType;

// Material property
typedef struct {
    const char* name;
    MaterialPropertyType type;
    union {
        f32 float_value;
        Vec2 vec2_value;
        Vec3 vec3_value;
        Vec4 vec4_value;
        Mat4 mat4_value;
        TextureHandle texture_value;
    };
} MaterialProperty;

// Render statistics
typedef struct {
    u32 draw_calls;
    u32 triangles;
    u32 vertices;
    u32 texture_switches;
    u32 shader_switches;
    u32 render_target_switches;
    f64 frame_time;
    f64 gpu_time;
    u64 memory_used;
} RenderStats;

// Shader reload callback
typedef void (*ShaderReloadCallback)(ShaderHandle shader, void* user_data);

// Renderer API
typedef struct {
    // Initialization
    Renderer* (*Create)(PlatformState* platform, MemoryArena* arena, u32 width, u32 height);
    void (*Destroy)(Renderer* renderer);
    void (*Resize)(Renderer* renderer, u32 width, u32 height);
    
    // Resource creation
    ShaderHandle (*CreateShader)(Renderer* renderer, const char* vertex_path, const char* fragment_path);
    TextureHandle (*CreateTexture)(Renderer* renderer, u32 width, u32 height, TextureFormat format, void* data);
    MeshHandle (*CreateMesh)(Renderer* renderer, void* vertices, u32 vertex_count, u32* indices, u32 index_count, VertexFormat format);
    MaterialHandle (*CreateMaterial)(Renderer* renderer, ShaderHandle shader);
    RenderTargetHandle (*CreateRenderTarget)(Renderer* renderer, u32 width, u32 height, TextureFormat* formats, u32 count, bool depth);
    
    // Resource destruction
    void (*DestroyShader)(Renderer* renderer, ShaderHandle handle);
    void (*DestroyTexture)(Renderer* renderer, TextureHandle handle);
    void (*DestroyMesh)(Renderer* renderer, MeshHandle handle);
    void (*DestroyMaterial)(Renderer* renderer, MaterialHandle handle);
    void (*DestroyRenderTarget)(Renderer* renderer, RenderTargetHandle handle);
    
    // Material system
    void (*SetMaterialProperty)(Renderer* renderer, MaterialHandle material, const char* name, void* value, MaterialPropertyType type);
    void (*UpdateMaterial)(Renderer* renderer, MaterialHandle material);
    
    // Command buffer
    void (*BeginFrame)(Renderer* renderer);
    void (*EndFrame)(Renderer* renderer);
    void (*ExecuteCommands)(Renderer* renderer);
    
    // Immediate commands
    void (*Clear)(Renderer* renderer, Vec4 color, f32 depth, u8 stencil, u32 flags);
    void (*SetViewport)(Renderer* renderer, Viewport viewport);
    void (*SetRenderTarget)(Renderer* renderer, RenderTargetHandle handle);
    void (*SetShader)(Renderer* renderer, ShaderHandle handle);
    void (*SetMaterial)(Renderer* renderer, MaterialHandle handle);
    void (*SetRenderState)(Renderer* renderer, RenderState state);
    void (*DrawMesh)(Renderer* renderer, MeshHandle handle, u32 instance_count, Mat4* transforms);
    void (*DrawImmediate)(Renderer* renderer, void* vertices, u32 vertex_count, VertexFormat format, PrimitiveType primitive);
    
    // Hot reload
    void (*RegisterShaderReloadCallback)(Renderer* renderer, ShaderReloadCallback callback, void* user_data);
    void (*CheckShaderReloads)(Renderer* renderer);
    void (*ReloadShader)(Renderer* renderer, ShaderHandle handle);
    
    // Debug
    void (*PushDebugGroup)(Renderer* renderer, const char* name);
    void (*PopDebugGroup)(Renderer* renderer);
    RenderStats (*GetStats)(Renderer* renderer);
    void (*ResetStats)(Renderer* renderer);
} RendererAPI;

// Global renderer API
extern RendererAPI* Render;

#endif // HANDMADE_RENDERER_NEW_H