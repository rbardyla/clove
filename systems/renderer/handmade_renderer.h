#ifndef HANDMADE_RENDERER_H
#define HANDMADE_RENDERER_H

/*
    Handmade OpenGL Renderer
    Unreal Engine-style rendering pipeline from scratch
    
    Features:
    - OpenGL 3.3 Core Profile
    - Shader compilation and management
    - Mesh rendering with vertex buffers
    - Texture management
    - Uniform buffer objects
    - Frame buffer objects for post-processing
    - Debug rendering
*/

#include "../../src/handmade.h"
#include "handmade_math.h"
#include "handmade_platform.h"

// OpenGL types (to avoid including GL headers here)
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

// Renderer limits
#define MAX_SHADERS 256
#define MAX_MESHES 4096
#define MAX_TEXTURES 2048
#define MAX_MATERIALS 1024
#define MAX_RENDER_TARGETS 32
#define MAX_UNIFORM_BUFFERS 128
#define MAX_VERTEX_ATTRIBUTES 16
#define MAX_TEXTURE_UNITS 32
#define MAX_LIGHTS 128

// Vertex formats
typedef struct vertex_p3f {
    v3 position;
} vertex_p3f;

typedef struct vertex_p3f_n3f {
    v3 position;
    v3 normal;
} vertex_p3f_n3f;

typedef struct vertex_p3f_n3f_t2f {
    v3 position;
    v3 normal;
    v2 texcoord;
} vertex_p3f_n3f_t2f;

typedef struct vertex_p3f_n3f_t2f_c4f {
    v3 position;
    v3 normal;
    v2 texcoord;
    v4 color;
} vertex_p3f_n3f_t2f_c4f;

// Shader types
typedef enum shader_type {
    SHADER_VERTEX = 0,
    SHADER_FRAGMENT,
    SHADER_GEOMETRY,
    SHADER_COMPUTE,
    SHADER_TYPE_COUNT
} shader_type;

// Shader program
typedef struct shader_program {
    GLuint id;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLuint geometry_shader;
    char name[64];
    b32 is_valid;
    
    // Uniform locations cache
    GLint uniform_locations[64];
    char uniform_names[64][64];
    u32 uniform_count;
} shader_program;

// Mesh data
typedef struct mesh {
    GLuint vao;  // Vertex Array Object
    GLuint vbo;  // Vertex Buffer Object
    GLuint ebo;  // Element Buffer Object
    
    u32 vertex_count;
    u32 index_count;
    u32 vertex_size;
    
    // Bounding volume
    v3 min_bounds;
    v3 max_bounds;
    v3 center;
    f32 radius;
    
    char name[64];
    b32 is_valid;
} mesh;

// Texture formats
typedef enum texture_format {
    TEXTURE_FORMAT_R8,
    TEXTURE_FORMAT_RG8,
    TEXTURE_FORMAT_RGB8,
    TEXTURE_FORMAT_RGBA8,
    TEXTURE_FORMAT_R16F,
    TEXTURE_FORMAT_RG16F,
    TEXTURE_FORMAT_RGB16F,
    TEXTURE_FORMAT_RGBA16F,
    TEXTURE_FORMAT_R32F,
    TEXTURE_FORMAT_RG32F,
    TEXTURE_FORMAT_RGB32F,
    TEXTURE_FORMAT_RGBA32F,
    TEXTURE_FORMAT_DEPTH24_STENCIL8,
    TEXTURE_FORMAT_DEPTH32F,
    TEXTURE_FORMAT_COUNT
} texture_format;

// Texture
typedef struct texture {
    GLuint id;
    u32 width;
    u32 height;
    u32 depth;
    texture_format format;
    b32 has_mipmaps;
    char name[64];
    b32 is_valid;
} texture;

// Material
typedef struct material {
    shader_program *shader;
    
    // PBR parameters
    v3 albedo;
    f32 metallic;
    f32 roughness;
    f32 ao;
    v3 emissive;
    
    // Textures
    texture *albedo_map;
    texture *normal_map;
    texture *metallic_map;
    texture *roughness_map;
    texture *ao_map;
    texture *emissive_map;
    
    // Render state
    b32 depth_test;
    b32 depth_write;
    b32 blend_enable;
    GLenum blend_src;
    GLenum blend_dst;
    b32 cull_enable;
    GLenum cull_face;
    
    char name[64];
    b32 is_valid;
} material;

// Light types
typedef enum light_type {
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,
    LIGHT_SPOT,
    LIGHT_TYPE_COUNT
} light_type;

// Light
typedef struct light {
    light_type type;
    
    v3 position;
    v3 direction;
    v3 color;
    f32 intensity;
    
    // Point/Spot light parameters
    f32 constant;
    f32 linear;
    f32 quadratic;
    
    // Spot light parameters
    f32 inner_cone;
    f32 outer_cone;
    
    // Shadow mapping
    b32 cast_shadows;
    texture *shadow_map;
    m4x4 shadow_matrix;
    
    b32 is_active;
} light;

// Render target (for post-processing)
typedef struct render_target {
    GLuint fbo;  // Frame Buffer Object
    texture *color_attachments[8];
    texture *depth_attachment;
    u32 attachment_count;
    u32 width;
    u32 height;
    char name[64];
    b32 is_valid;
} render_target;

// Uniform buffer
typedef struct uniform_buffer {
    GLuint id;
    u32 size;
    u32 binding_point;
    void *data;
    char name[64];
    b32 is_valid;
} uniform_buffer;

// Draw command (for batching)
typedef struct draw_command {
    mesh *mesh_data;
    material *mat;
    m4x4 model_matrix;
    u32 instance_count;
} draw_command;

// Renderer statistics
typedef struct renderer_stats {
    u32 draw_calls;
    u32 triangles_rendered;
    u32 vertices_processed;
    u32 texture_switches;
    u32 shader_switches;
    f32 frame_time_ms;
    f32 gpu_time_ms;
} renderer_stats;

// Renderer state
typedef struct renderer_state {
    // Platform
    platform_state *platform;
    
    // Resources
    shader_program shaders[MAX_SHADERS];
    mesh meshes[MAX_MESHES];
    texture textures[MAX_TEXTURES];
    material materials[MAX_MATERIALS];
    render_target render_targets[MAX_RENDER_TARGETS];
    uniform_buffer uniform_buffers[MAX_UNIFORM_BUFFERS];
    light lights[MAX_LIGHTS];
    
    // Resource counts
    u32 shader_count;
    u32 mesh_count;
    u32 texture_count;
    u32 material_count;
    u32 render_target_count;
    u32 uniform_buffer_count;
    u32 light_count;
    
    // Current state
    shader_program *current_shader;
    render_target *current_render_target;
    material *current_material;
    
    // View/Projection matrices
    m4x4 view_matrix;
    m4x4 projection_matrix;
    m4x4 view_projection_matrix;
    v3 camera_position;
    v3 camera_forward;
    v3 camera_right;
    v3 camera_up;
    
    // Built-in shaders
    shader_program *basic_shader;
    shader_program *phong_shader;
    shader_program *pbr_shader;
    shader_program *shadow_shader;
    shader_program *debug_shader;
    shader_program *post_process_shader;
    
    // Built-in meshes
    mesh *quad_mesh;
    mesh *cube_mesh;
    mesh *sphere_mesh;
    mesh *cylinder_mesh;
    mesh *cone_mesh;
    mesh *torus_mesh;
    
    // Default textures
    texture *white_texture;
    texture *black_texture;
    texture *normal_texture;
    texture *checkerboard_texture;
    
    // Frame buffers
    render_target *main_framebuffer;
    render_target *shadow_framebuffer;
    render_target *post_process_framebuffer;
    
    // Statistics
    renderer_stats stats;
    renderer_stats last_frame_stats;
    
    // Debug
    b32 wireframe_mode;
    b32 show_normals;
    b32 show_bounds;
    b32 show_lights;
    b32 show_grid;
} renderer_state;

// =============================================================================
// RENDERER API
// =============================================================================

// Initialization
renderer_state *renderer_init(platform_state *platform, u64 memory_size);
void renderer_shutdown(renderer_state *renderer);

// Frame management
void renderer_begin_frame(renderer_state *renderer);
void renderer_end_frame(renderer_state *renderer);
void renderer_present(renderer_state *renderer);
void renderer_clear(renderer_state *renderer, v4 color, b32 clear_depth, b32 clear_stencil);

// View/Projection
void renderer_set_view_matrix(renderer_state *renderer, m4x4 view);
void renderer_set_projection_matrix(renderer_state *renderer, m4x4 projection);
void renderer_set_camera(renderer_state *renderer, v3 position, v3 forward, v3 up);
m4x4 renderer_create_perspective(f32 fov_degrees, f32 aspect, f32 near_plane, f32 far_plane);
m4x4 renderer_create_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near_plane, f32 far_plane);

// Shader management
shader_program *renderer_create_shader(renderer_state *renderer, char *name, 
                                      char *vertex_source, char *fragment_source);
shader_program *renderer_create_shader_from_file(renderer_state *renderer, char *name,
                                                char *vertex_path, char *fragment_path);
void renderer_use_shader(renderer_state *renderer, shader_program *shader);
void renderer_set_uniform_f32(shader_program *shader, char *name, f32 value);
void renderer_set_uniform_v2(shader_program *shader, char *name, v2 value);
void renderer_set_uniform_v3(shader_program *shader, char *name, v3 value);
void renderer_set_uniform_v4(shader_program *shader, char *name, v4 value);
void renderer_set_uniform_m4x4(shader_program *shader, char *name, m4x4 value);
void renderer_set_uniform_texture(shader_program *shader, char *name, texture *tex, u32 unit);

// Mesh management
mesh *renderer_create_mesh(renderer_state *renderer, char *name,
                          void *vertices, u32 vertex_count, u32 vertex_size,
                          u32 *indices, u32 index_count);
mesh *renderer_create_quad(renderer_state *renderer);
mesh *renderer_create_cube(renderer_state *renderer);
mesh *renderer_create_sphere(renderer_state *renderer, u32 segments, u32 rings);
mesh *renderer_create_cylinder(renderer_state *renderer, u32 segments);
mesh *renderer_create_cone(renderer_state *renderer, u32 segments);
mesh *renderer_create_torus(renderer_state *renderer, f32 outer_radius, f32 inner_radius, u32 segments, u32 rings);
void renderer_draw_mesh(renderer_state *renderer, mesh *mesh_data, m4x4 model_matrix);
void renderer_draw_mesh_instanced(renderer_state *renderer, mesh *mesh_data, 
                                 m4x4 *model_matrices, u32 instance_count);

// Texture management
texture *renderer_create_texture(renderer_state *renderer, char *name,
                                void *data, u32 width, u32 height, 
                                texture_format format, b32 generate_mipmaps);
texture *renderer_create_texture_from_file(renderer_state *renderer, char *name, char *path);
texture *renderer_create_render_texture(renderer_state *renderer, char *name,
                                       u32 width, u32 height, texture_format format);
void renderer_bind_texture(renderer_state *renderer, texture *tex, u32 unit);

// Material management  
material *renderer_create_material(renderer_state *renderer, char *name, shader_program *shader);
void renderer_use_material(renderer_state *renderer, material *mat);
void renderer_set_material_texture(material *mat, char *slot, texture *tex);

// Light management
light *renderer_add_directional_light(renderer_state *renderer, v3 direction, v3 color, f32 intensity);
light *renderer_add_point_light(renderer_state *renderer, v3 position, v3 color, f32 intensity, f32 radius);
light *renderer_add_spot_light(renderer_state *renderer, v3 position, v3 direction, 
                              v3 color, f32 intensity, f32 inner_cone, f32 outer_cone);
void renderer_update_lights(renderer_state *renderer);

// Render targets
render_target *renderer_create_render_target(renderer_state *renderer, char *name,
                                            u32 width, u32 height, u32 color_attachments,
                                            texture_format color_format, b32 depth);
void renderer_bind_render_target(renderer_state *renderer, render_target *rt);
void renderer_unbind_render_target(renderer_state *renderer);

// Debug rendering
void renderer_draw_line(renderer_state *renderer, v3 start, v3 end, v3 color);
void renderer_draw_box(renderer_state *renderer, v3 min, v3 max, v3 color);
void renderer_draw_sphere_wireframe(renderer_state *renderer, v3 center, f32 radius, v3 color);
void renderer_draw_grid(renderer_state *renderer, f32 size, u32 divisions, v3 color);
void renderer_draw_axis(renderer_state *renderer, m4x4 transform, f32 size);

// Statistics
renderer_stats renderer_get_stats(renderer_state *renderer);
void renderer_reset_stats(renderer_state *renderer);

#endif // HANDMADE_RENDERER_H