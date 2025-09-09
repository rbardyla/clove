/*
    Handmade OpenGL Renderer Implementation
    Core rendering functionality for the engine
*/

#include "handmade_renderer.h"
#include "handmade_opengl.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

internal GLuint compile_shader(GLenum type, char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("ERROR: Shader compilation failed:\n%s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

internal shader_program *get_free_shader(renderer_state *renderer) {
    for (u32 i = 0; i < MAX_SHADERS; i++) {
        if (!renderer->shaders[i].is_valid) {
            return &renderer->shaders[i];
        }
    }
    return NULL;
}

internal mesh *get_free_mesh(renderer_state *renderer) {
    for (u32 i = 0; i < MAX_MESHES; i++) {
        if (!renderer->meshes[i].is_valid) {
            return &renderer->meshes[i];
        }
    }
    return NULL;
}

internal texture *get_free_texture(renderer_state *renderer) {
    for (u32 i = 0; i < MAX_TEXTURES; i++) {
        if (!renderer->textures[i].is_valid) {
            return &renderer->textures[i];
        }
    }
    return NULL;
}

internal material *get_free_material(renderer_state *renderer) {
    for (u32 i = 0; i < MAX_MATERIALS; i++) {
        if (!renderer->materials[i].is_valid) {
            return &renderer->materials[i];
        }
    }
    return NULL;
}

internal render_target *get_free_render_target(renderer_state *renderer) {
    for (u32 i = 0; i < MAX_RENDER_TARGETS; i++) {
        if (!renderer->render_targets[i].is_valid) {
            return &renderer->render_targets[i];
        }
    }
    return NULL;
}

internal light *get_free_light(renderer_state *renderer) {
    for (u32 i = 0; i < MAX_LIGHTS; i++) {
        if (!renderer->lights[i].is_active) {
            return &renderer->lights[i];
        }
    }
    return NULL;
}

internal GLenum get_gl_format(texture_format format) {
    switch (format) {
        case TEXTURE_FORMAT_R8:
        case TEXTURE_FORMAT_R16F:
        case TEXTURE_FORMAT_R32F:
            return GL_RED;
        case TEXTURE_FORMAT_RG8:
        case TEXTURE_FORMAT_RG16F:
        case TEXTURE_FORMAT_RG32F:
            return GL_RG;
        case TEXTURE_FORMAT_RGB8:
        case TEXTURE_FORMAT_RGB16F:
        case TEXTURE_FORMAT_RGB32F:
            return GL_RGB;
        case TEXTURE_FORMAT_RGBA8:
        case TEXTURE_FORMAT_RGBA16F:
        case TEXTURE_FORMAT_RGBA32F:
            return GL_RGBA;
        case TEXTURE_FORMAT_DEPTH24_STENCIL8:
            return GL_DEPTH_STENCIL;
        case TEXTURE_FORMAT_DEPTH32F:
            return GL_DEPTH_COMPONENT;
        default:
            return GL_RGBA;
    }
}

internal GLenum get_gl_internal_format(texture_format format) {
    switch (format) {
        case TEXTURE_FORMAT_R8: return GL_R8;
        case TEXTURE_FORMAT_RG8: return GL_RG8;
        case TEXTURE_FORMAT_RGB8: return GL_RGB8;
        case TEXTURE_FORMAT_RGBA8: return GL_RGBA8;
        case TEXTURE_FORMAT_R16F: return GL_R16F;
        case TEXTURE_FORMAT_RG16F: return GL_RG16F;
        case TEXTURE_FORMAT_RGB16F: return GL_RGB16F;
        case TEXTURE_FORMAT_RGBA16F: return GL_RGBA16F;
        case TEXTURE_FORMAT_R32F: return GL_R32F;
        case TEXTURE_FORMAT_RG32F: return GL_RG32F;
        case TEXTURE_FORMAT_RGB32F: return GL_RGB32F;
        case TEXTURE_FORMAT_RGBA32F: return GL_RGBA32F;
        case TEXTURE_FORMAT_DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
        case TEXTURE_FORMAT_DEPTH32F: return GL_DEPTH_COMPONENT32F;
        default: return GL_RGBA8;
    }
}

internal GLenum get_gl_type(texture_format format) {
    switch (format) {
        case TEXTURE_FORMAT_R8:
        case TEXTURE_FORMAT_RG8:
        case TEXTURE_FORMAT_RGB8:
        case TEXTURE_FORMAT_RGBA8:
            return GL_UNSIGNED_BYTE;
        case TEXTURE_FORMAT_R16F:
        case TEXTURE_FORMAT_RG16F:
        case TEXTURE_FORMAT_RGB16F:
        case TEXTURE_FORMAT_RGBA16F:
        case TEXTURE_FORMAT_R32F:
        case TEXTURE_FORMAT_RG32F:
        case TEXTURE_FORMAT_RGB32F:
        case TEXTURE_FORMAT_RGBA32F:
        case TEXTURE_FORMAT_DEPTH32F:
            return GL_FLOAT;
        case TEXTURE_FORMAT_DEPTH24_STENCIL8:
            return 0x84FA; // GL_UNSIGNED_INT_24_8 value
        default:
            return GL_UNSIGNED_BYTE;
    }
}

// Additional GL constants not in our header
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_VERSION 0x1F02
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01

// Define PI if not already defined
#ifndef PI
#define PI 3.14159265358979323846f
#endif

// =============================================================================
// INITIALIZATION
// =============================================================================

renderer_state *renderer_init(platform_state *platform, u64 memory_size) {
    if (!platform) {
        printf("ERROR: Platform not provided to renderer\n");
        return NULL;
    }
    
    // Allocate renderer memory
    void *memory = platform_alloc(memory_size);
    if (!memory) {
        printf("ERROR: Failed to allocate renderer memory\n");
        return NULL;
    }
    
    renderer_state *renderer = (renderer_state *)memory;
    platform_zero_memory(renderer, sizeof(renderer_state));
    
    renderer->platform = platform;
    
    // Load OpenGL functions
    if (!gl_load_functions(platform_gl_get_proc_address)) {
        printf("ERROR: Failed to load OpenGL functions\n");
        platform_free(memory);
        return NULL;
    }
    
    // Print OpenGL info
    printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    
    // Set default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Create default shaders
    char *basic_vertex_shader = 
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aNormal;\n"
        "layout (location = 2) in vec2 aTexCoord;\n"
        "out vec3 FragPos;\n"
        "out vec3 Normal;\n"
        "out vec2 TexCoord;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    FragPos = vec3(model * vec4(aPos, 1.0));\n"
        "    Normal = mat3(transpose(inverse(model))) * aNormal;\n"
        "    TexCoord = aTexCoord;\n"
        "    gl_Position = projection * view * vec4(FragPos, 1.0);\n"
        "}\n";
    
    char *basic_fragment_shader = 
        "#version 330 core\n"
        "in vec3 FragPos;\n"
        "in vec3 Normal;\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform vec3 viewPos;\n"
        "uniform vec3 lightDir;\n"
        "uniform vec3 lightColor;\n"
        "uniform vec3 objectColor;\n"
        "void main() {\n"
        "    vec3 ambient = 0.15 * lightColor;\n"
        "    vec3 norm = normalize(Normal);\n"
        "    float diff = max(dot(norm, -lightDir), 0.0);\n"
        "    vec3 diffuse = diff * lightColor;\n"
        "    vec3 viewDir = normalize(viewPos - FragPos);\n"
        "    vec3 reflectDir = reflect(lightDir, norm);\n"
        "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "    vec3 specular = 0.5 * spec * lightColor;\n"
        "    vec3 result = (ambient + diffuse + specular) * objectColor;\n"
        "    FragColor = vec4(result, 1.0);\n"
        "}\n";
    
    renderer->basic_shader = renderer_create_shader(renderer, "basic", 
                                                   basic_vertex_shader, 
                                                   basic_fragment_shader);
    
    // Create debug shader
    char *debug_vertex_shader = 
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 mvp;\n"
        "void main() {\n"
        "    gl_Position = mvp * vec4(aPos, 1.0);\n"
        "}\n";
    
    char *debug_fragment_shader = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 color;\n"
        "void main() {\n"
        "    FragColor = vec4(color, 1.0);\n"
        "}\n";
    
    renderer->debug_shader = renderer_create_shader(renderer, "debug",
                                                   debug_vertex_shader,
                                                   debug_fragment_shader);
    
    // Create default meshes
    renderer->quad_mesh = renderer_create_quad(renderer);
    renderer->cube_mesh = renderer_create_cube(renderer);
    renderer->sphere_mesh = renderer_create_sphere(renderer, 32, 16);
    
    // Create default textures
    u32 white_pixels[1] = {0xFFFFFFFF};
    renderer->white_texture = renderer_create_texture(renderer, "white", 
                                                     white_pixels, 1, 1, 
                                                     TEXTURE_FORMAT_RGBA8, false);
    
    u32 black_pixels[1] = {0xFF000000};
    renderer->black_texture = renderer_create_texture(renderer, "black",
                                                     black_pixels, 1, 1,
                                                     TEXTURE_FORMAT_RGBA8, false);
    
    u32 normal_pixels[1] = {0xFFFF8080}; // Default normal (0.5, 0.5, 1.0)
    renderer->normal_texture = renderer_create_texture(renderer, "normal",
                                                      normal_pixels, 1, 1,
                                                      TEXTURE_FORMAT_RGBA8, false);
    
    // Create checkerboard texture
    u32 checkerboard[64*64];
    for (u32 y = 0; y < 64; y++) {
        for (u32 x = 0; x < 64; x++) {
            b32 white = ((x / 8) + (y / 8)) % 2;
            checkerboard[y * 64 + x] = white ? 0xFFFFFFFF : 0xFF404040;
        }
    }
    renderer->checkerboard_texture = renderer_create_texture(renderer, "checkerboard",
                                                            checkerboard, 64, 64,
                                                            TEXTURE_FORMAT_RGBA8, true);
    
    // Set default matrices
    renderer->view_matrix = m4x4_identity();
    renderer->projection_matrix = renderer_create_perspective(60.0f, 
                                                             (f32)platform->window_width / (f32)platform->window_height,
                                                             0.1f, 1000.0f);
    renderer->view_projection_matrix = m4x4_multiply(renderer->projection_matrix, renderer->view_matrix);
    
    printf("Renderer initialized successfully\n");
    return renderer;
}

void renderer_shutdown(renderer_state *renderer) {
    if (!renderer) return;
    
    // Delete all OpenGL resources
    for (u32 i = 0; i < MAX_SHADERS; i++) {
        if (renderer->shaders[i].is_valid) {
            glDeleteProgram(renderer->shaders[i].id);
        }
    }
    
    for (u32 i = 0; i < MAX_MESHES; i++) {
        if (renderer->meshes[i].is_valid) {
            glDeleteVertexArrays(1, &renderer->meshes[i].vao);
            glDeleteBuffers(1, &renderer->meshes[i].vbo);
            if (renderer->meshes[i].ebo) {
                glDeleteBuffers(1, &renderer->meshes[i].ebo);
            }
        }
    }
    
    for (u32 i = 0; i < MAX_TEXTURES; i++) {
        if (renderer->textures[i].is_valid) {
            glDeleteTextures(1, &renderer->textures[i].id);
        }
    }
    
    for (u32 i = 0; i < MAX_RENDER_TARGETS; i++) {
        if (renderer->render_targets[i].is_valid) {
            glDeleteFramebuffers(1, &renderer->render_targets[i].fbo);
        }
    }
    
    platform_free(renderer);
}

// =============================================================================
// FRAME MANAGEMENT
// =============================================================================

void renderer_begin_frame(renderer_state *renderer) {
    renderer->stats.draw_calls = 0;
    renderer->stats.triangles_rendered = 0;
    renderer->stats.vertices_processed = 0;
    renderer->stats.texture_switches = 0;
    renderer->stats.shader_switches = 0;
}

void renderer_end_frame(renderer_state *renderer) {
    renderer->last_frame_stats = renderer->stats;
}

void renderer_present(renderer_state *renderer) {
    platform_swap_buffers(renderer->platform);
}

void renderer_clear(renderer_state *renderer, v4 color, b32 clear_depth, b32 clear_stencil) {
    glClearColor(color.r, color.g, color.b, color.a);
    
    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if (clear_depth) mask |= GL_DEPTH_BUFFER_BIT;
    if (clear_stencil) mask |= GL_STENCIL_BUFFER_BIT;
    
    glClear(mask);
}

// =============================================================================
// VIEW/PROJECTION
// =============================================================================

void renderer_set_view_matrix(renderer_state *renderer, m4x4 view) {
    renderer->view_matrix = view;
    renderer->view_projection_matrix = m4x4_multiply(renderer->projection_matrix, renderer->view_matrix);
}

void renderer_set_projection_matrix(renderer_state *renderer, m4x4 projection) {
    renderer->projection_matrix = projection;
    renderer->view_projection_matrix = m4x4_multiply(renderer->projection_matrix, renderer->view_matrix);
}

void renderer_set_camera(renderer_state *renderer, v3 position, v3 forward, v3 up) {
    renderer->camera_position = position;
    renderer->camera_forward = v3_normalize(forward);
    renderer->camera_up = v3_normalize(up);
    renderer->camera_right = v3_normalize(v3_cross(forward, up));
    
    v3 target = v3_add(position, forward);
    renderer->view_matrix = m4x4_look_at(position, target, up);
    renderer->view_projection_matrix = m4x4_multiply(renderer->projection_matrix, renderer->view_matrix);
}

m4x4 renderer_create_perspective(f32 fov_degrees, f32 aspect, f32 near_plane, f32 far_plane) {
    return m4x4_perspective(fov_degrees * (PI / 180.0f), aspect, near_plane, far_plane);
}

m4x4 renderer_create_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near_plane, f32 far_plane) {
    return m4x4_orthographic(left, right, bottom, top, near_plane, far_plane);
}

// =============================================================================
// SHADER MANAGEMENT
// =============================================================================

shader_program *renderer_create_shader(renderer_state *renderer, char *name,
                                      char *vertex_source, char *fragment_source) {
    shader_program *shader = get_free_shader(renderer);
    if (!shader) {
        printf("ERROR: No free shader slots\n");
        return NULL;
    }
    
    // Compile shaders
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex_shader) return NULL;
    
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment_shader) {
        glDeleteShader(vertex_shader);
        return NULL;
    }
    
    // Link program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("ERROR: Shader linking failed:\n%s\n", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(program);
        return NULL;
    }
    
    // Store shader info
    shader->id = program;
    shader->vertex_shader = vertex_shader;
    shader->fragment_shader = fragment_shader;
    strncpy(shader->name, name, 63);
    shader->is_valid = true;
    shader->uniform_count = 0;
    
    renderer->shader_count++;
    
    printf("Created shader: %s\n", name);
    return shader;
}

shader_program *renderer_create_shader_from_file(renderer_state *renderer, char *name,
                                                char *vertex_path, char *fragment_path) {
    platform_file vertex_file = platform_read_file(vertex_path);
    if (!vertex_file.valid) {
        printf("ERROR: Failed to read vertex shader: %s\n", vertex_path);
        return NULL;
    }
    
    platform_file fragment_file = platform_read_file(fragment_path);
    if (!fragment_file.valid) {
        printf("ERROR: Failed to read fragment shader: %s\n", fragment_path);
        platform_free_file(&vertex_file);
        return NULL;
    }
    
    // Null-terminate the shader sources
    char *vertex_source = platform_alloc(vertex_file.size + 1);
    char *fragment_source = platform_alloc(fragment_file.size + 1);
    
    platform_copy_memory(vertex_source, vertex_file.data, vertex_file.size);
    platform_copy_memory(fragment_source, fragment_file.data, fragment_file.size);
    vertex_source[vertex_file.size] = '\0';
    fragment_source[fragment_file.size] = '\0';
    
    shader_program *shader = renderer_create_shader(renderer, name, vertex_source, fragment_source);
    
    platform_free(vertex_source);
    platform_free(fragment_source);
    platform_free_file(&vertex_file);
    platform_free_file(&fragment_file);
    
    return shader;
}

void renderer_use_shader(renderer_state *renderer, shader_program *shader) {
    if (!shader || !shader->is_valid) return;
    
    if (renderer->current_shader != shader) {
        glUseProgram(shader->id);
        renderer->current_shader = shader;
        renderer->stats.shader_switches++;
    }
}

internal GLint get_uniform_location(shader_program *shader, char *name) {
    // Check cache first
    for (u32 i = 0; i < shader->uniform_count; i++) {
        if (strcmp(shader->uniform_names[i], name) == 0) {
            return shader->uniform_locations[i];
        }
    }
    
    // Get location and cache it
    GLint location = glGetUniformLocation(shader->id, name);
    if (location != -1 && shader->uniform_count < 64) {
        shader->uniform_locations[shader->uniform_count] = location;
        strncpy(shader->uniform_names[shader->uniform_count], name, 63);
        shader->uniform_count++;
    }
    
    return location;
}

void renderer_set_uniform_f32(shader_program *shader, char *name, f32 value) {
    GLint location = get_uniform_location(shader, name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void renderer_set_uniform_v2(shader_program *shader, char *name, v2 value) {
    GLint location = get_uniform_location(shader, name);
    if (location != -1) {
        glUniform2f(location, value.x, value.y);
    }
}

void renderer_set_uniform_v3(shader_program *shader, char *name, v3 value) {
    GLint location = get_uniform_location(shader, name);
    if (location != -1) {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void renderer_set_uniform_v4(shader_program *shader, char *name, v4 value) {
    GLint location = get_uniform_location(shader, name);
    if (location != -1) {
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }
}

void renderer_set_uniform_m4x4(shader_program *shader, char *name, m4x4 value) {
    GLint location = get_uniform_location(shader, name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)&value);
    }
}

void renderer_set_uniform_texture(shader_program *shader, char *name, texture *tex, u32 unit) {
    GLint location = get_uniform_location(shader, name);
    if (location != -1) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, tex ? tex->id : 0);
        glUniform1i(location, unit);
    }
}

// =============================================================================
// MESH MANAGEMENT
// =============================================================================

mesh *renderer_create_mesh(renderer_state *renderer, char *name,
                          void *vertices, u32 vertex_count, u32 vertex_size,
                          u32 *indices, u32 index_count) {
    mesh *mesh_data = get_free_mesh(renderer);
    if (!mesh_data) {
        printf("ERROR: No free mesh slots\n");
        return NULL;
    }
    
    // Generate and bind VAO
    glGenVertexArrays(1, &mesh_data->vao);
    glBindVertexArray(mesh_data->vao);
    
    // Create and upload vertex buffer
    glGenBuffers(1, &mesh_data->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh_data->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * vertex_size, vertices, GL_STATIC_DRAW);
    
    // Create and upload index buffer if provided
    if (indices && index_count > 0) {
        glGenBuffers(1, &mesh_data->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_data->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(u32), indices, GL_STATIC_DRAW);
    }
    
    // Setup vertex attributes based on vertex size
    // Assume standard layout: position, normal, texcoord
    if (vertex_size == sizeof(vertex_p3f)) {
        // Position only
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)0);
        glEnableVertexAttribArray(0);
    } else if (vertex_size == sizeof(vertex_p3f_n3f)) {
        // Position + Normal
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)(3 * sizeof(f32)));
        glEnableVertexAttribArray(1);
    } else if (vertex_size == sizeof(vertex_p3f_n3f_t2f)) {
        // Position + Normal + TexCoord
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)(3 * sizeof(f32)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vertex_size, (void*)(6 * sizeof(f32)));
        glEnableVertexAttribArray(2);
    } else if (vertex_size == sizeof(vertex_p3f_n3f_t2f_c4f)) {
        // Position + Normal + TexCoord + Color
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, (void*)(3 * sizeof(f32)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vertex_size, (void*)(6 * sizeof(f32)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, vertex_size, (void*)(8 * sizeof(f32)));
        glEnableVertexAttribArray(3);
    }
    
    // Unbind
    glBindVertexArray(0);
    
    // Store mesh info
    mesh_data->vertex_count = vertex_count;
    mesh_data->index_count = index_count;
    mesh_data->vertex_size = vertex_size;
    strncpy(mesh_data->name, name, 63);
    mesh_data->is_valid = true;
    
    // Calculate bounds (simplified - assumes position is first)
    if (vertices && vertex_count > 0) {
        v3 *positions = (v3*)vertices;
        mesh_data->min_bounds = positions[0];
        mesh_data->max_bounds = positions[0];
        
        for (u32 i = 1; i < vertex_count; i++) {
            v3 pos = positions[i * (vertex_size / sizeof(f32)) / 3];
            mesh_data->min_bounds = v3_min(mesh_data->min_bounds, pos);
            mesh_data->max_bounds = v3_max(mesh_data->max_bounds, pos);
        }
        
        mesh_data->center = v3_scale(v3_add(mesh_data->min_bounds, mesh_data->max_bounds), 0.5f);
        mesh_data->radius = v3_length(v3_sub(mesh_data->max_bounds, mesh_data->center));
    }
    
    renderer->mesh_count++;
    
    printf("Created mesh: %s (%u vertices, %u indices)\n", name, vertex_count, index_count);
    return mesh_data;
}

mesh *renderer_create_quad(renderer_state *renderer) {
    vertex_p3f_n3f_t2f vertices[] = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };
    
    u32 indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    return renderer_create_mesh(renderer, "quad", 
                               vertices, 4, sizeof(vertex_p3f_n3f_t2f),
                               indices, 6);
}

mesh *renderer_create_cube(renderer_state *renderer) {
    vertex_p3f_n3f_t2f vertices[] = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
        
        // Back face
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
        
        // Right face
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        
        // Left face
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        
        // Top face
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
        
        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
    };
    
    u32 indices[] = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Right
        12, 13, 14, 14, 15, 12, // Left
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };
    
    return renderer_create_mesh(renderer, "cube",
                               vertices, 24, sizeof(vertex_p3f_n3f_t2f),
                               indices, 36);
}

mesh *renderer_create_sphere(renderer_state *renderer, u32 segments, u32 rings) {
    u32 vertex_count = (rings + 1) * (segments + 1);
    u32 index_count = rings * segments * 6;
    
    vertex_p3f_n3f_t2f *vertices = platform_alloc(vertex_count * sizeof(vertex_p3f_n3f_t2f));
    u32 *indices = platform_alloc(index_count * sizeof(u32));
    
    // Generate vertices
    u32 vertex_index = 0;
    for (u32 ring = 0; ring <= rings; ring++) {
        f32 v = (f32)ring / (f32)rings;
        f32 phi = v * PI;
        
        for (u32 segment = 0; segment <= segments; segment++) {
            f32 u = (f32)segment / (f32)segments;
            f32 theta = u * 2.0f * PI;
            
            f32 x = cosf(theta) * sinf(phi);
            f32 y = cosf(phi);
            f32 z = sinf(theta) * sinf(phi);
            
            vertices[vertex_index].position = (v3){x, y, z};
            vertices[vertex_index].normal = (v3){x, y, z}; // Normalized position for sphere
            vertices[vertex_index].texcoord = (v2){u, v};
            vertex_index++;
        }
    }
    
    // Generate indices
    u32 index = 0;
    for (u32 ring = 0; ring < rings; ring++) {
        for (u32 segment = 0; segment < segments; segment++) {
            u32 current = ring * (segments + 1) + segment;
            u32 next = current + (segments + 1);
            
            // First triangle
            indices[index++] = current;
            indices[index++] = next;
            indices[index++] = current + 1;
            
            // Second triangle
            indices[index++] = current + 1;
            indices[index++] = next;
            indices[index++] = next + 1;
        }
    }
    
    mesh *sphere = renderer_create_mesh(renderer, "sphere",
                                       vertices, vertex_count, sizeof(vertex_p3f_n3f_t2f),
                                       indices, index_count);
    
    platform_free(vertices);
    platform_free(indices);
    
    return sphere;
}

void renderer_draw_mesh(renderer_state *renderer, mesh *mesh_data, m4x4 model_matrix) {
    if (!mesh_data || !mesh_data->is_valid) return;
    if (!renderer->current_shader) return;
    
    // Set matrices
    renderer_set_uniform_m4x4(renderer->current_shader, "model", model_matrix);
    renderer_set_uniform_m4x4(renderer->current_shader, "view", renderer->view_matrix);
    renderer_set_uniform_m4x4(renderer->current_shader, "projection", renderer->projection_matrix);
    renderer_set_uniform_v3(renderer->current_shader, "viewPos", renderer->camera_position);
    
    // Bind mesh
    glBindVertexArray(mesh_data->vao);
    
    // Draw
    if (mesh_data->index_count > 0) {
        glDrawElements(GL_TRIANGLES, mesh_data->index_count, GL_UNSIGNED_INT, 0);
        renderer->stats.triangles_rendered += mesh_data->index_count / 3;
    } else {
        glDrawArrays(GL_TRIANGLES, 0, mesh_data->vertex_count);
        renderer->stats.triangles_rendered += mesh_data->vertex_count / 3;
    }
    
    renderer->stats.draw_calls++;
    renderer->stats.vertices_processed += mesh_data->vertex_count;
    
    glBindVertexArray(0);
}

// =============================================================================
// TEXTURE MANAGEMENT
// =============================================================================

texture *renderer_create_texture(renderer_state *renderer, char *name,
                                void *data, u32 width, u32 height,
                                texture_format format, b32 generate_mipmaps) {
    texture *tex = get_free_texture(renderer);
    if (!tex) {
        printf("ERROR: No free texture slots\n");
        return NULL;
    }
    
    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generate_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload texture data
    GLenum gl_format = get_gl_format(format);
    GLenum gl_internal_format = get_gl_internal_format(format);
    GLenum gl_type = get_gl_type(format);
    
    glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, width, height, 0, gl_format, gl_type, data);
    
    if (generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    
    // Store texture info
    tex->width = width;
    tex->height = height;
    tex->depth = 0;
    tex->format = format;
    tex->has_mipmaps = generate_mipmaps;
    strncpy(tex->name, name, 63);
    tex->is_valid = true;
    
    renderer->texture_count++;
    
    printf("Created texture: %s (%ux%u)\n", name, width, height);
    return tex;
}

void renderer_bind_texture(renderer_state *renderer, texture *tex, u32 unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex ? tex->id : 0);
    if (tex) {
        renderer->stats.texture_switches++;
    }
}

// =============================================================================
// DEBUG RENDERING
// =============================================================================

void renderer_draw_line(renderer_state *renderer, v3 start, v3 end, v3 color) {
    if (!renderer->debug_shader) return;
    
    // Create line vertices
    v3 vertices[] = {start, end};
    
    // Create temporary VAO/VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(v3), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Draw
    renderer_use_shader(renderer, renderer->debug_shader);
    m4x4 mvp = m4x4_multiply(renderer->view_projection_matrix, m4x4_identity());
    renderer_set_uniform_m4x4(renderer->debug_shader, "mvp", mvp);
    renderer_set_uniform_v3(renderer->debug_shader, "color", color);
    
    glDrawArrays(GL_LINES, 0, 2);
    
    // Cleanup
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void renderer_draw_grid(renderer_state *renderer, f32 size, u32 divisions, v3 color) {
    f32 half_size = size * 0.5f;
    f32 step = size / (f32)divisions;
    
    for (u32 i = 0; i <= divisions; i++) {
        f32 pos = -half_size + (f32)i * step;
        
        // Lines along X
        renderer_draw_line(renderer,
                         (v3){pos, 0, -half_size},
                         (v3){pos, 0, half_size},
                         color);
        
        // Lines along Z
        renderer_draw_line(renderer,
                         (v3){-half_size, 0, pos},
                         (v3){half_size, 0, pos},
                         color);
    }
}

// =============================================================================
// STATISTICS
// =============================================================================

renderer_stats renderer_get_stats(renderer_state *renderer) {
    return renderer->last_frame_stats;
}

void renderer_reset_stats(renderer_state *renderer) {
    platform_zero_memory(&renderer->stats, sizeof(renderer_stats));
}