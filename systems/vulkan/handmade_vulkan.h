/*
 * Handmade Vulkan Renderer
 * Zero-dependency Vulkan implementation following handmade philosophy
 * 
 * PERFORMANCE: Target 5000+ draw calls at 60 FPS
 * MEMORY: Zero allocations per frame, custom memory management
 * CACHE: All resources aligned to 64-byte boundaries
 */

#ifndef HANDMADE_VULKAN_H
#define HANDMADE_VULKAN_H

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct Platform Platform;

// PERFORMANCE: Fixed-size pools to avoid dynamic allocation
#define MAX_FRAMES_IN_FLIGHT 2
#define MAX_DESCRIPTOR_SETS 1024
#define MAX_PIPELINES 64
#define MAX_RENDER_PASSES 16
#define MAX_FRAMEBUFFERS 8
#define MAX_COMMAND_BUFFERS 64
#define MAX_BUFFERS 4096
#define MAX_IMAGES 2048
#define MAX_SAMPLERS 32
#define MAX_SHADER_MODULES 128
#define MAX_MEMORY_TYPES 32
#define MAX_SWAPCHAIN_IMAGES 4

// CACHE: Align to cache line for optimal access
#define VULKAN_CACHE_LINE 64
#define VULKAN_ALIGN(size) (((size) + VULKAN_CACHE_LINE - 1) & ~(VULKAN_CACHE_LINE - 1))

// Memory allocation sizes
#define VULKAN_DEVICE_MEMORY_BLOCK_SIZE (256 * 1024 * 1024) // 256MB blocks
#define VULKAN_STAGING_BUFFER_SIZE (64 * 1024 * 1024)       // 64MB staging
#define VULKAN_UNIFORM_BUFFER_SIZE (16 * 1024 * 1024)       // 16MB uniforms

// ============================================================================
// Core Structures
// ============================================================================

typedef struct VulkanMemoryBlock {
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize used;
    uint32_t memory_type_index;
    void* mapped_ptr;
    uint32_t allocation_count;
    
    // CACHE: Align to cache line
    uint8_t padding[VULKAN_CACHE_LINE - 40];
} VulkanMemoryBlock;

typedef struct VulkanMemoryAllocator {
    VulkanMemoryBlock blocks[MAX_MEMORY_TYPES * 4];
    uint32_t block_count;
    VkPhysicalDeviceMemoryProperties memory_properties;
    
    // Statistics
    uint64_t total_allocated;
    uint64_t total_used;
    uint32_t allocation_count;
} VulkanMemoryAllocator;

typedef struct VulkanBuffer {
    VkBuffer buffer;
    VulkanMemoryBlock* memory_block;
    VkDeviceSize offset;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    void* mapped_ptr;
} VulkanBuffer;

typedef struct VulkanImage {
    VkImage image;
    VkImageView view;
    VulkanMemoryBlock* memory_block;
    VkDeviceSize offset;
    VkDeviceSize size;
    VkFormat format;
    VkExtent3D extent;
    uint32_t mip_levels;
    uint32_t array_layers;
} VulkanImage;

typedef struct VulkanSwapchain {
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage images[MAX_SWAPCHAIN_IMAGES];
    VkImageView image_views[MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];
} VulkanSwapchain;

typedef struct VulkanQueue {
    VkQueue queue;
    uint32_t family_index;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_COMMAND_BUFFERS];
    uint32_t command_buffer_count;
} VulkanQueue;

typedef struct VulkanPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout descriptor_set_layout;
    VkRenderPass render_pass;
    uint32_t push_constant_size;
    VkShaderStageFlags push_constant_stages;
} VulkanPipeline;

typedef struct VulkanFrameData {
    VkCommandBuffer command_buffer;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;
    
    // Per-frame resources
    VulkanBuffer uniform_buffer;
    VkDescriptorSet descriptor_sets[MAX_DESCRIPTOR_SETS];
    uint32_t descriptor_set_count;
    
    // CACHE: Align to cache line
    uint8_t padding[VULKAN_CACHE_LINE - 56];
} VulkanFrameData;

typedef struct VulkanContext {
    // Core objects
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkSurfaceKHR surface;
    
    // Queues
    VulkanQueue graphics_queue;
    VulkanQueue compute_queue;
    VulkanQueue transfer_queue;
    
    // Swapchain
    VulkanSwapchain swapchain;
    
    // Memory management
    VulkanMemoryAllocator allocator;
    
    // Pipelines
    VulkanPipeline pipelines[MAX_PIPELINES];
    uint32_t pipeline_count;
    
    // Frame data
    VulkanFrameData frames[MAX_FRAMES_IN_FLIGHT];
    uint32_t current_frame;
    uint32_t image_index;
    
    // Global resources
    VkDescriptorPool descriptor_pool;
    VkSampler linear_sampler;
    VkSampler nearest_sampler;
    VkSampler shadow_sampler;
    
    // Debug
    VkDebugUtilsMessengerEXT debug_messenger;
    
    // Statistics
    uint64_t frame_count;
    double gpu_time_ms;
    uint32_t draw_call_count;
    uint32_t triangle_count;
    
    // CACHE: Ensure structure is cache-aligned
    uint8_t padding[VULKAN_CACHE_LINE - 8];
} VulkanContext;

// ============================================================================
// Rendering Data Structures
// ============================================================================

typedef struct VulkanVertex {
    float position[3];
    float normal[3];
    float uv[2];
    float tangent[4];
    uint32_t color;
    uint32_t padding[3]; // CACHE: Align to 64 bytes
} VulkanVertex;

typedef struct VulkanMesh {
    VulkanBuffer vertex_buffer;
    VulkanBuffer index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;
    VkIndexType index_type;
} VulkanMesh;

typedef struct VulkanMaterial {
    VulkanImage* albedo_texture;
    VulkanImage* normal_texture;
    VulkanImage* metallic_roughness_texture;
    VulkanImage* occlusion_texture;
    VulkanImage* emissive_texture;
    
    float base_color[4];
    float metallic;
    float roughness;
    float emissive[3];
    float alpha_cutoff;
} VulkanMaterial;

typedef struct VulkanDrawCommand {
    VulkanMesh* mesh;
    VulkanMaterial* material;
    float transform[16]; // Column-major 4x4 matrix
    uint32_t instance_count;
    uint32_t first_instance;
} VulkanDrawCommand;

typedef struct VulkanRenderState {
    float view_matrix[16];
    float projection_matrix[16];
    float view_projection_matrix[16];
    float inverse_view_projection[16];
    
    float camera_position[4];
    float camera_direction[4];
    
    float sun_direction[4];
    float sun_color[4];
    
    float time;
    float delta_time;
    uint32_t frame_index;
    uint32_t screen_width;
    uint32_t screen_height;
    
    uint32_t padding[3]; // CACHE: Align to 256 bytes (common UBO alignment)
} VulkanRenderState;

// ============================================================================
// Ray Marching Structures
// ============================================================================

typedef struct VulkanRayMarchSettings {
    float max_distance;
    float epsilon;
    uint32_t max_steps;
    uint32_t shadow_steps;
    
    float fog_density;
    float fog_height;
    float fog_falloff;
    
    float cloud_scale;
    float cloud_speed;
    float cloud_coverage;
    float cloud_light_absorption;
    
    uint32_t gi_samples;
    float gi_distance;
    float gi_intensity;
    
    uint32_t volumetric_samples;
    float volumetric_scattering;
    float volumetric_absorption;
} VulkanRayMarchSettings;

// ============================================================================
// Public API
// ============================================================================

// Core initialization
bool vulkan_init(VulkanContext* ctx, Platform* platform, uint32_t width, uint32_t height);
void vulkan_shutdown(VulkanContext* ctx);
bool vulkan_recreate_swapchain(VulkanContext* ctx, uint32_t width, uint32_t height);

// Frame management
bool vulkan_begin_frame(VulkanContext* ctx);
void vulkan_end_frame(VulkanContext* ctx);
void vulkan_wait_idle(VulkanContext* ctx);

// Command recording
VkCommandBuffer vulkan_begin_single_time_commands(VulkanContext* ctx);
void vulkan_end_single_time_commands(VulkanContext* ctx, VkCommandBuffer cmd);

// Memory management
VulkanBuffer vulkan_create_buffer(VulkanContext* ctx, VkDeviceSize size, 
                                  VkBufferUsageFlags usage, 
                                  VkMemoryPropertyFlags properties);
void vulkan_destroy_buffer(VulkanContext* ctx, VulkanBuffer* buffer);
void vulkan_copy_buffer(VulkanContext* ctx, VulkanBuffer* src, 
                        VulkanBuffer* dst, VkDeviceSize size);
void* vulkan_map_buffer(VulkanContext* ctx, VulkanBuffer* buffer);
void vulkan_unmap_buffer(VulkanContext* ctx, VulkanBuffer* buffer);

// Image management
VulkanImage vulkan_create_image(VulkanContext* ctx, uint32_t width, uint32_t height,
                                VkFormat format, VkImageUsageFlags usage);
VulkanImage vulkan_create_texture(VulkanContext* ctx, uint32_t width, uint32_t height,
                                  VkFormat format, void* pixels);
void vulkan_destroy_image(VulkanContext* ctx, VulkanImage* image);
void vulkan_transition_image_layout(VulkanContext* ctx, VkCommandBuffer cmd,
                                    VkImage image, VkFormat format,
                                    VkImageLayout old_layout, VkImageLayout new_layout);

// Pipeline management
VulkanPipeline* vulkan_create_graphics_pipeline(VulkanContext* ctx,
                                                const uint32_t* vertex_spv, size_t vertex_size,
                                                const uint32_t* fragment_spv, size_t fragment_size,
                                                VkRenderPass render_pass);
VulkanPipeline* vulkan_create_compute_pipeline(VulkanContext* ctx,
                                               const uint32_t* compute_spv, size_t compute_size);
void vulkan_destroy_pipeline(VulkanContext* ctx, VulkanPipeline* pipeline);

// Mesh management
VulkanMesh vulkan_create_mesh(VulkanContext* ctx, 
                              const VulkanVertex* vertices, uint32_t vertex_count,
                              const uint32_t* indices, uint32_t index_count);
void vulkan_destroy_mesh(VulkanContext* ctx, VulkanMesh* mesh);

// Drawing
void vulkan_draw_mesh(VulkanContext* ctx, VkCommandBuffer cmd,
                      VulkanMesh* mesh, VulkanPipeline* pipeline);
void vulkan_draw_instanced(VulkanContext* ctx, VkCommandBuffer cmd,
                           VulkanMesh* mesh, VulkanPipeline* pipeline,
                           uint32_t instance_count);
void vulkan_dispatch_compute(VulkanContext* ctx, VkCommandBuffer cmd,
                             VulkanPipeline* pipeline,
                             uint32_t group_x, uint32_t group_y, uint32_t group_z);

// Synchronization
void vulkan_pipeline_barrier(VulkanContext* ctx, VkCommandBuffer cmd,
                             VkPipelineStageFlags src_stage,
                             VkPipelineStageFlags dst_stage);
void vulkan_memory_barrier(VulkanContext* ctx, VkCommandBuffer cmd,
                           VkAccessFlags src_access, VkAccessFlags dst_access);

// Debug utilities
void vulkan_set_object_name(VulkanContext* ctx, uint64_t object, 
                            VkObjectType type, const char* name);
void vulkan_begin_debug_label(VulkanContext* ctx, VkCommandBuffer cmd, 
                              const char* label, float color[4]);
void vulkan_end_debug_label(VulkanContext* ctx, VkCommandBuffer cmd);

// Statistics
void vulkan_get_stats(VulkanContext* ctx, uint32_t* draw_calls, 
                     uint32_t* triangles, double* gpu_time_ms);

#endif // HANDMADE_VULKAN_H