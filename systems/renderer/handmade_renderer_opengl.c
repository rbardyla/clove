/*
    OpenGL Renderer Implementation
    ===============================
    
    Zero-allocation renderer with command buffer and hot reload support.
*/

#include "handmade_renderer_new.h"
#include "../../handmade_platform.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SHADERS 1024
#define MAX_TEXTURES 4096
#define MAX_MESHES 2048
#define MAX_MATERIALS 2048
#define MAX_RENDER_TARGETS 256
#define MAX_COMMANDS 65536
#define MAX_SHADER_RELOAD_CALLBACKS 32
#define COMMAND_BUFFER_SIZE MEGABYTES(256)  // 256MB for commands

// OpenGL function pointers (loaded at runtime)
static PFNGLCREATESHADERPROC glCreateShader;
static PFNGLSHADERSOURCEPROC glShaderSource;
static PFNGLCOMPILESHADERPROC glCompileShader;
static PFNGLGETSHADERIVPROC glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC glCreateProgram;
static PFNGLATTACHSHADERPROC glAttachShader;
static PFNGLLINKPROGRAMPROC glLinkProgram;
static PFNGLGETPROGRAMIVPROC glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
static PFNGLUSEPROGRAMPROC glUseProgram;
static PFNGLDELETESHADERPROC glDeleteShader;
static PFNGLDELETEPROGRAMPROC glDeleteProgram;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM2FPROC glUniform2f;
static PFNGLUNIFORM3FPROC glUniform3f;
static PFNGLUNIFORM4FPROC glUniform4f;
static PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
static PFNGLGENBUFFERSPROC glGenBuffers;
static PFNGLBINDBUFFERPROC glBindBuffer;
static PFNGLBUFFERDATAPROC glBufferData;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLACTIVETEXTUREPROC glActiveTexture;

// OpenGL shader
typedef struct {
    GLuint program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    u32 generation;
    char* vertex_path;
    char* fragment_path;
    u64 vertex_timestamp;
    u64 fragment_timestamp;
    bool needs_reload;
    
    // Uniform locations cache
    struct {
        GLint mvp_matrix;
        GLint model_matrix;
        GLint view_matrix;
        GLint proj_matrix;
        GLint normal_matrix;
        GLint time;
        GLint resolution;
        GLint camera_position;
    } uniforms;
} GLShader;

// OpenGL texture
typedef struct {
    GLuint handle;
    GLenum target;
    GLenum format;
    GLenum internal_format;
    u32 width;
    u32 height;
    u32 generation;
    TextureFilter filter;
    TextureWrap wrap_u;
    TextureWrap wrap_v;
} GLTexture;

// OpenGL mesh
typedef struct {
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    u32 vertex_count;
    u32 index_count;
    VertexFormat format;
    PrimitiveType primitive;
    u32 generation;
} GLMesh;

// OpenGL material
typedef struct {
    ShaderHandle shader;
    RenderState render_state;
    MaterialProperty* properties;
    usize property_count;
    u32 generation;
    u8* uniform_buffer;
    usize uniform_buffer_size;
} GLMaterial;

// OpenGL render target
typedef struct {
    GLuint fbo;
    GLuint color_textures[8];
    GLuint depth_texture;
    u32 width;
    u32 height;
    u32 color_attachment_count;
    u32 generation;
} GLRenderTarget;

// Shader reload callback entry
typedef struct {
    ShaderReloadCallback callback;
    void* user_data;
} ShaderReloadCallbackEntry;

// OpenGL renderer state
struct Renderer {
    // Memory
    MemoryArena* arena;
    MemoryArena* command_arena;
    u8* command_buffer;
    usize command_buffer_used;
    
    // OpenGL context
    u32 width;
    u32 height;
    
    // Resources
    GLShader shaders[MAX_SHADERS];
    GLTexture textures[MAX_TEXTURES];
    GLMesh meshes[MAX_MESHES];
    GLMaterial materials[MAX_MATERIALS];
    GLRenderTarget render_targets[MAX_RENDER_TARGETS];
    
    // Resource tracking
    u32 shader_count;
    u32 texture_count;
    u32 mesh_count;
    u32 material_count;
    u32 render_target_count;
    u32 current_generation;
    
    // Commands
    RenderCommand* commands;
    u32 command_count;
    
    // Current state
    ShaderHandle current_shader;
    RenderTargetHandle current_render_target;
    RenderState current_render_state;
    Viewport current_viewport;
    
    // Hot reload
    ShaderReloadCallbackEntry reload_callbacks[MAX_SHADER_RELOAD_CALLBACKS];
    u32 reload_callback_count;
    f64 last_shader_check_time;
    
    // Statistics
    RenderStats stats;
    
    // Debug
    bool debug_enabled;
    u32 debug_group_depth;
};

// Forward declarations
static void LoadOpenGLFunctions(void);
static u64 GetFileModificationTime(const char* path);
static char* LoadFileToString(MemoryArena* arena, const char* path);
static GLuint CompileShader(GLenum type, const char* source);
static bool LinkShaderProgram(GLuint program);
static void CacheUniformLocations(GLShader* shader);

// Load OpenGL function pointers
static void LoadOpenGLFunctions(void) {
    glCreateShader = (PFNGLCREATESHADERPROC)Platform->gl.GetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)Platform->gl.GetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)Platform->gl.GetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)Platform->gl.GetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)Platform->gl.GetProcAddress("glGetShaderInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)Platform->gl.GetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)Platform->gl.GetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)Platform->gl.GetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)Platform->gl.GetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)Platform->gl.GetProcAddress("glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)Platform->gl.GetProcAddress("glUseProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)Platform->gl.GetProcAddress("glDeleteShader");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)Platform->gl.GetProcAddress("glDeleteProgram");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)Platform->gl.GetProcAddress("glGetUniformLocation");
    glUniform1f = (PFNGLUNIFORM1FPROC)Platform->gl.GetProcAddress("glUniform1f");
    glUniform2f = (PFNGLUNIFORM2FPROC)Platform->gl.GetProcAddress("glUniform2f");
    glUniform3f = (PFNGLUNIFORM3FPROC)Platform->gl.GetProcAddress("glUniform3f");
    glUniform4f = (PFNGLUNIFORM4FPROC)Platform->gl.GetProcAddress("glUniform4f");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)Platform->gl.GetProcAddress("glUniformMatrix4fv");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)Platform->gl.GetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)Platform->gl.GetProcAddress("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)Platform->gl.GetProcAddress("glDeleteVertexArrays");
    glGenBuffers = (PFNGLGENBUFFERSPROC)Platform->gl.GetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)Platform->gl.GetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)Platform->gl.GetProcAddress("glBufferData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)Platform->gl.GetProcAddress("glDeleteBuffers");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)Platform->gl.GetProcAddress("glEnableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)Platform->gl.GetProcAddress("glVertexAttribPointer");
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)Platform->gl.GetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)Platform->gl.GetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)Platform->gl.GetProcAddress("glFramebufferTexture2D");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)Platform->gl.GetProcAddress("glCheckFramebufferStatus");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)Platform->gl.GetProcAddress("glDeleteFramebuffers");
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)Platform->gl.GetProcAddress("glActiveTexture");
}

// Get file modification time
static u64 GetFileModificationTime(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

// Load file to string
static char* LoadFileToString(MemoryArena* arena, const char* path) {
    PlatformFile file = Platform->ReadFile(path, arena);
    if (file.data) {
        return (char*)file.data;
    }
    return NULL;
}

// Compile shader
static GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        printf("Shader compilation failed: %s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

// Link shader program
static bool LinkShaderProgram(GLuint program) {
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        printf("Shader linking failed: %s\n", info_log);
        return false;
    }
    
    return true;
}

// Cache uniform locations
static void CacheUniformLocations(GLShader* shader) {
    shader->uniforms.mvp_matrix = glGetUniformLocation(shader->program, "u_MVPMatrix");
    shader->uniforms.model_matrix = glGetUniformLocation(shader->program, "u_ModelMatrix");
    shader->uniforms.view_matrix = glGetUniformLocation(shader->program, "u_ViewMatrix");
    shader->uniforms.proj_matrix = glGetUniformLocation(shader->program, "u_ProjMatrix");
    shader->uniforms.normal_matrix = glGetUniformLocation(shader->program, "u_NormalMatrix");
    shader->uniforms.time = glGetUniformLocation(shader->program, "u_Time");
    shader->uniforms.resolution = glGetUniformLocation(shader->program, "u_Resolution");
    shader->uniforms.camera_position = glGetUniformLocation(shader->program, "u_CameraPosition");
}

// Create renderer
static Renderer* RendererCreate(PlatformState* platform, MemoryArena* arena, u32 width, u32 height) {
    // Allocate renderer from arena
    Renderer* renderer = PushStruct(arena, Renderer);
    
    renderer->arena = arena;
    renderer->width = width;
    renderer->height = height;
    renderer->current_generation = 1;
    
    // Allocate command buffer (256MB)
    renderer->command_buffer = PushSize(arena, COMMAND_BUFFER_SIZE);
    renderer->command_arena = PushStruct(arena, MemoryArena);
    renderer->command_arena->base = renderer->command_buffer;
    renderer->command_arena->size = COMMAND_BUFFER_SIZE;
    renderer->command_arena->used = 0;
    
    // Allocate command array
    renderer->commands = PushArray(arena, RenderCommand, MAX_COMMANDS);
    
    // Load OpenGL functions
    LoadOpenGLFunctions();
    
    // Set default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    printf("[Renderer] Initialized OpenGL renderer (%dx%d)\n", width, height);
    printf("[Renderer] Command buffer: %.1f MB\n", COMMAND_BUFFER_SIZE / (1024.0 * 1024.0));
    
    return renderer;
}

// Create shader
static ShaderHandle RendererCreateShader(Renderer* renderer, const char* vertex_path, const char* fragment_path) {
    if (renderer->shader_count >= MAX_SHADERS) {
        printf("[Renderer] Error: Max shaders reached\n");
        return INVALID_SHADER_HANDLE;
    }
    
    // Load shader sources
    TempMemory temp = BeginTempMemory(&renderer->command_arena);
    
    char* vertex_source = LoadFileToString(&renderer->command_arena, vertex_path);
    char* fragment_source = LoadFileToString(&renderer->command_arena, fragment_path);
    
    if (!vertex_source || !fragment_source) {
        printf("[Renderer] Error: Failed to load shader files\n");
        EndTempMemory(temp);
        return INVALID_SHADER_HANDLE;
    }
    
    // Compile shaders
    GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
    
    if (!vertex_shader || !fragment_shader) {
        if (vertex_shader) glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        EndTempMemory(temp);
        return INVALID_SHADER_HANDLE;
    }
    
    // Create and link program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    
    if (!LinkShaderProgram(program)) {
        glDeleteProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        EndTempMemory(temp);
        return INVALID_SHADER_HANDLE;
    }
    
    // Store shader
    u32 index = renderer->shader_count++;
    GLShader* shader = &renderer->shaders[index];
    
    shader->program = program;
    shader->vertex_shader = vertex_shader;
    shader->fragment_shader = fragment_shader;
    shader->generation = renderer->current_generation;
    
    // Store paths for hot reload
    usize vertex_path_len = strlen(vertex_path) + 1;
    usize fragment_path_len = strlen(fragment_path) + 1;
    shader->vertex_path = PushArray(renderer->arena, char, vertex_path_len);
    shader->fragment_path = PushArray(renderer->arena, char, fragment_path_len);
    memcpy(shader->vertex_path, vertex_path, vertex_path_len);
    memcpy(shader->fragment_path, fragment_path, fragment_path_len);
    
    // Store timestamps
    shader->vertex_timestamp = GetFileModificationTime(vertex_path);
    shader->fragment_timestamp = GetFileModificationTime(fragment_path);
    
    // Cache uniform locations
    CacheUniformLocations(shader);
    
    EndTempMemory(temp);
    
    printf("[Renderer] Created shader %u from %s + %s\n", index, vertex_path, fragment_path);
    
    return (ShaderHandle){index, shader->generation};
}

// Check shader reloads
static void RendererCheckShaderReloads(Renderer* renderer) {
    f64 current_time = PlatformGetTime();
    
    // Only check every 0.5 seconds
    if (current_time - renderer->last_shader_check_time < 0.5) {
        return;
    }
    renderer->last_shader_check_time = current_time;
    
    for (u32 i = 0; i < renderer->shader_count; i++) {
        GLShader* shader = &renderer->shaders[i];
        
        u64 vertex_time = GetFileModificationTime(shader->vertex_path);
        u64 fragment_time = GetFileModificationTime(shader->fragment_path);
        
        if (vertex_time > shader->vertex_timestamp || fragment_time > shader->fragment_timestamp) {
            printf("[Renderer] Shader %u needs reload\n", i);
            
            // Reload shader
            TempMemory temp = BeginTempMemory(&renderer->command_arena);
            
            char* vertex_source = LoadFileToString(&renderer->command_arena, shader->vertex_path);
            char* fragment_source = LoadFileToString(&renderer->command_arena, shader->fragment_path);
            
            if (vertex_source && fragment_source) {
                GLuint new_vertex = CompileShader(GL_VERTEX_SHADER, vertex_source);
                GLuint new_fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
                
                if (new_vertex && new_fragment) {
                    GLuint new_program = glCreateProgram();
                    glAttachShader(new_program, new_vertex);
                    glAttachShader(new_program, new_fragment);
                    
                    if (LinkShaderProgram(new_program)) {
                        // Success! Replace old shader
                        glDeleteProgram(shader->program);
                        glDeleteShader(shader->vertex_shader);
                        glDeleteShader(shader->fragment_shader);
                        
                        shader->program = new_program;
                        shader->vertex_shader = new_vertex;
                        shader->fragment_shader = new_fragment;
                        shader->vertex_timestamp = vertex_time;
                        shader->fragment_timestamp = fragment_time;
                        shader->generation++;
                        
                        // Re-cache uniforms
                        CacheUniformLocations(shader);
                        
                        printf("[Renderer] Shader %u reloaded successfully\n", i);
                        
                        // Call reload callbacks
                        ShaderHandle handle = {i, shader->generation};
                        for (u32 j = 0; j < renderer->reload_callback_count; j++) {
                            renderer->reload_callbacks[j].callback(handle, renderer->reload_callbacks[j].user_data);
                        }
                    } else {
                        glDeleteProgram(new_program);
                        glDeleteShader(new_vertex);
                        glDeleteShader(new_fragment);
                        printf("[Renderer] Shader %u reload failed (link error)\n", i);
                    }
                } else {
                    if (new_vertex) glDeleteShader(new_vertex);
                    if (new_fragment) glDeleteShader(new_fragment);
                    printf("[Renderer] Shader %u reload failed (compile error)\n", i);
                }
            }
            
            EndTempMemory(temp);
        }
    }
}

// Begin frame
static void RendererBeginFrame(Renderer* renderer) {
    // Reset command buffer
    renderer->command_arena->used = 0;
    renderer->command_count = 0;
    
    // Check for shader reloads
    RendererCheckShaderReloads(renderer);
    
    // Reset stats
    renderer->stats.draw_calls = 0;
    renderer->stats.triangles = 0;
    renderer->stats.vertices = 0;
    renderer->stats.texture_switches = 0;
    renderer->stats.shader_switches = 0;
}

// End frame and execute commands
static void RendererEndFrame(Renderer* renderer) {
    // Execute all commands
    for (u32 i = 0; i < renderer->command_count; i++) {
        RenderCommand* cmd = &renderer->commands[i];
        
        switch (cmd->type) {
            case CMD_CLEAR: {
                glClearColor(cmd->clear.color.x, cmd->clear.color.y, 
                           cmd->clear.color.z, cmd->clear.color.w);
                glClearDepth(cmd->clear.depth);
                glClearStencil(cmd->clear.stencil);
                
                GLbitfield mask = 0;
                if (cmd->clear.flags & CLEAR_COLOR) mask |= GL_COLOR_BUFFER_BIT;
                if (cmd->clear.flags & CLEAR_DEPTH) mask |= GL_DEPTH_BUFFER_BIT;
                if (cmd->clear.flags & CLEAR_STENCIL) mask |= GL_STENCIL_BUFFER_BIT;
                
                glClear(mask);
                break;
            }
            
            case CMD_SET_VIEWPORT: {
                glViewport(cmd->set_viewport.viewport.x, cmd->set_viewport.viewport.y,
                          cmd->set_viewport.viewport.width, cmd->set_viewport.viewport.height);
                break;
            }
            
            case CMD_SET_SHADER: {
                if (cmd->set_shader.handle.id < renderer->shader_count) {
                    GLShader* shader = &renderer->shaders[cmd->set_shader.handle.id];
                    glUseProgram(shader->program);
                    renderer->stats.shader_switches++;
                }
                break;
            }
            
            // ... handle other commands ...
        }
    }
}

// Clear command
static void RendererClear(Renderer* renderer, Vec4 color, f32 depth, u8 stencil, u32 flags) {
    if (renderer->command_count >= MAX_COMMANDS) return;
    
    RenderCommand* cmd = &renderer->commands[renderer->command_count++];
    cmd->type = CMD_CLEAR;
    cmd->clear.color = color;
    cmd->clear.depth = depth;
    cmd->clear.stencil = stencil;
    cmd->clear.flags = flags;
}

// Get stats
static RenderStats RendererGetStats(Renderer* renderer) {
    return renderer->stats;
}

// Initialize renderer API
static RendererAPI g_renderer_api = {
    .Create = RendererCreate,
    .CreateShader = RendererCreateShader,
    .CheckShaderReloads = RendererCheckShaderReloads,
    .BeginFrame = RendererBeginFrame,
    .EndFrame = RendererEndFrame,
    .Clear = RendererClear,
    .GetStats = RendererGetStats,
    // ... other functions ...
};

RendererAPI* Render = &g_renderer_api;