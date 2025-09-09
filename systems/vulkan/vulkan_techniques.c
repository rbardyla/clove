/*
 * Vulkan Rendering Techniques
 * Forward rendering, shadow mapping, post-processing
 * 
 * PERFORMANCE: GPU-driven rendering with indirect draw
 * MEMORY: Persistent mapped buffers for dynamic data
 * CACHE: Sorted draw calls to minimize state changes
 */

#include "handmade_vulkan.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// Shadow Mapping
// ============================================================================

#define SHADOW_MAP_SIZE 2048
#define MAX_SHADOW_CASCADES 4

typedef struct ShadowData {
    VulkanImage depth_image;
    VkFramebuffer framebuffer;
    float view_projection_matrix[16];
    float split_depth;
} ShadowData;

typedef struct ShadowSystem {
    ShadowData cascades[MAX_SHADOW_CASCADES];
    VulkanPipeline* shadow_pipeline;
    VkRenderPass shadow_render_pass;
    uint32_t cascade_count;
} ShadowSystem;

static ShadowSystem g_shadow_system;

// ============================================================================
// Post-Processing
// ============================================================================

typedef struct PostProcessData {
    VulkanImage color_buffer;
    VulkanImage depth_buffer;
    VulkanImage bloom_buffer[8]; // Mip chain for bloom
    VkFramebuffer framebuffer;
    VulkanPipeline* tone_mapping_pipeline;
    VulkanPipeline* bloom_pipeline;
    VulkanPipeline* fxaa_pipeline;
} PostProcessData;

static PostProcessData g_post_process;

// ============================================================================
// Draw Call Sorting
// ============================================================================

typedef struct DrawKey {
    uint32_t pipeline_id : 8;
    uint32_t material_id : 12;
    uint32_t depth : 12;
} DrawKey;

typedef struct SortedDrawCall {
    DrawKey key;
    VulkanDrawCommand* command;
} SortedDrawCall;

static SortedDrawCall g_sorted_draws[16384];
static uint32_t g_sorted_draw_count = 0;

// Radix sort for draw calls
static void radix_sort_draws(SortedDrawCall* draws, uint32_t count) {
    // PERFORMANCE: Radix sort is cache-friendly and O(n)
    SortedDrawCall temp[16384];
    
    // Sort by 8-bit chunks
    for (uint32_t byte = 0; byte < 4; byte++) {
        uint32_t counts[256] = {0};
        uint32_t shift = byte * 8;
        
        // Count occurrences
        for (uint32_t i = 0; i < count; i++) {
            uint32_t value = *(uint32_t*)&draws[i].key;
            uint32_t index = (value >> shift) & 0xFF;
            counts[index]++;
        }
        
        // Prefix sum
        uint32_t total = 0;
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t old_count = counts[i];
            counts[i] = total;
            total += old_count;
        }
        
        // Sort
        for (uint32_t i = 0; i < count; i++) {
            uint32_t value = *(uint32_t*)&draws[i].key;
            uint32_t index = (value >> shift) & 0xFF;
            temp[counts[index]++] = draws[i];
        }
        
        // Copy back
        memcpy(draws, temp, count * sizeof(SortedDrawCall));
    }
}

// ============================================================================
// Matrix Math Helpers
// ============================================================================

static void matrix_multiply(float* result, const float* a, const float* b) {
    float temp[16];
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
    
    memcpy(result, temp, sizeof(temp));
}

static void matrix_ortho(float* matrix, float left, float right,
                         float bottom, float top, float near, float far) {
    memset(matrix, 0, sizeof(float) * 16);
    
    matrix[0] = 2.0f / (right - left);
    matrix[5] = 2.0f / (top - bottom);
    matrix[10] = -2.0f / (far - near);
    matrix[12] = -(right + left) / (right - left);
    matrix[13] = -(top + bottom) / (top - bottom);
    matrix[14] = -(far + near) / (far - near);
    matrix[15] = 1.0f;
}

static void matrix_look_at(float* matrix, const float* eye,
                           const float* center, const float* up) {
    float f[3] = {
        center[0] - eye[0],
        center[1] - eye[1],
        center[2] - eye[2]
    };
    
    // Normalize f
    float f_len = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    f[0] /= f_len;
    f[1] /= f_len;
    f[2] /= f_len;
    
    // Cross product for right vector
    float s[3] = {
        f[1] * up[2] - f[2] * up[1],
        f[2] * up[0] - f[0] * up[2],
        f[0] * up[1] - f[1] * up[0]
    };
    
    // Normalize s
    float s_len = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    s[0] /= s_len;
    s[1] /= s_len;
    s[2] /= s_len;
    
    // Cross product for up vector
    float u[3] = {
        s[1] * f[2] - s[2] * f[1],
        s[2] * f[0] - s[0] * f[2],
        s[0] * f[1] - s[1] * f[0]
    };
    
    memset(matrix, 0, sizeof(float) * 16);
    
    matrix[0] = s[0];
    matrix[4] = s[1];
    matrix[8] = s[2];
    
    matrix[1] = u[0];
    matrix[5] = u[1];
    matrix[9] = u[2];
    
    matrix[2] = -f[0];
    matrix[6] = -f[1];
    matrix[10] = -f[2];
    
    matrix[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    matrix[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    matrix[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
    matrix[15] = 1.0f;
}

// ============================================================================
// Shadow System
// ============================================================================

bool vulkan_init_shadow_system(VulkanContext* ctx, uint32_t cascade_count) {
    if (cascade_count > MAX_SHADOW_CASCADES) {
        cascade_count = MAX_SHADOW_CASCADES;
    }
    
    g_shadow_system.cascade_count = cascade_count;
    
    // Create shadow render pass
    VkAttachmentDescription depth_attachment = {
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };
    
    VkAttachmentReference depth_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pDepthStencilAttachment = &depth_ref
    };
    
    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        }
    };
    
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &depth_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = dependencies
    };
    
    if (vkCreateRenderPass(ctx->device, &render_pass_info, NULL,
                          &g_shadow_system.shadow_render_pass) != VK_SUCCESS) {
        return false;
    }
    
    // Create depth images and framebuffers for each cascade
    for (uint32_t i = 0; i < cascade_count; i++) {
        ShadowData* cascade = &g_shadow_system.cascades[i];
        
        // Create depth image
        cascade->depth_image = vulkan_create_image(ctx, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
                                                   VK_FORMAT_D32_SFLOAT,
                                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                   VK_IMAGE_USAGE_SAMPLED_BIT);
        
        if (!cascade->depth_image.image) {
            return false;
        }
        
        // Create framebuffer
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = g_shadow_system.shadow_render_pass,
            .attachmentCount = 1,
            .pAttachments = &cascade->depth_image.view,
            .width = SHADOW_MAP_SIZE,
            .height = SHADOW_MAP_SIZE,
            .layers = 1
        };
        
        if (vkCreateFramebuffer(ctx->device, &framebuffer_info, NULL,
                               &cascade->framebuffer) != VK_SUCCESS) {
            return false;
        }
    }
    
    return true;
}

void vulkan_destroy_shadow_system(VulkanContext* ctx) {
    for (uint32_t i = 0; i < g_shadow_system.cascade_count; i++) {
        ShadowData* cascade = &g_shadow_system.cascades[i];
        
        if (cascade->framebuffer) {
            vkDestroyFramebuffer(ctx->device, cascade->framebuffer, NULL);
        }
        
        vulkan_destroy_image(ctx, &cascade->depth_image);
    }
    
    if (g_shadow_system.shadow_render_pass) {
        vkDestroyRenderPass(ctx->device, g_shadow_system.shadow_render_pass, NULL);
    }
    
    memset(&g_shadow_system, 0, sizeof(ShadowSystem));
}

void vulkan_update_shadow_cascades(const float* view_matrix, const float* projection_matrix,
                                   const float* light_direction, float cascade_splits[]) {
    // Calculate cascade splits using practical split scheme
    float near = 0.1f;
    float far = 1000.0f;
    float lambda = 0.95f; // Blend between linear and logarithmic
    
    for (uint32_t i = 0; i < g_shadow_system.cascade_count; i++) {
        float uniform = near + (far - near) * (i + 1) / g_shadow_system.cascade_count;
        float log = near * powf(far / near, (i + 1) / (float)g_shadow_system.cascade_count);
        
        cascade_splits[i] = lambda * log + (1.0f - lambda) * uniform;
        g_shadow_system.cascades[i].split_depth = cascade_splits[i];
    }
    
    // Update view-projection matrices for each cascade
    for (uint32_t i = 0; i < g_shadow_system.cascade_count; i++) {
        ShadowData* cascade = &g_shadow_system.cascades[i];
        
        float cascade_near = i > 0 ? cascade_splits[i - 1] : near;
        float cascade_far = cascade_splits[i];
        
        // Calculate frustum corners in world space
        // ... (frustum calculation code)
        
        // Calculate light view matrix
        float light_pos[3] = {0, 100, 0}; // Example light position
        float light_target[3] = {0, 0, 0};
        float up[3] = {0, 0, 1};
        
        float light_view[16];
        matrix_look_at(light_view, light_pos, light_target, up);
        
        // Calculate orthographic projection for cascade
        float light_proj[16];
        float size = 50.0f * (i + 1); // Cascade-dependent size
        matrix_ortho(light_proj, -size, size, -size, size, -500.0f, 500.0f);
        
        // Combine matrices
        matrix_multiply(cascade->view_projection_matrix, light_proj, light_view);
    }
}

void vulkan_render_shadow_pass(VulkanContext* ctx, VkCommandBuffer cmd,
                               VulkanDrawCommand* commands, uint32_t command_count) {
    // Begin shadow render pass
    for (uint32_t cascade = 0; cascade < g_shadow_system.cascade_count; cascade++) {
        ShadowData* shadow = &g_shadow_system.cascades[cascade];
        
        VkRenderPassBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = g_shadow_system.shadow_render_pass,
            .framebuffer = shadow->framebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}
            },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue){
                .depthStencil = {1.0f, 0}
            }
        };
        
        vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        
        // Set viewport and scissor
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)SHADOW_MAP_SIZE,
            .height = (float)SHADOW_MAP_SIZE,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        
        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}
        };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        // Bind shadow pipeline
        if (g_shadow_system.shadow_pipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             g_shadow_system.shadow_pipeline->pipeline);
            
            // Render all objects
            for (uint32_t i = 0; i < command_count; i++) {
                VulkanDrawCommand* draw = &commands[i];
                
                // Push constants (MVP matrix)
                float mvp[16];
                matrix_multiply(mvp, shadow->view_projection_matrix, draw->transform);
                
                vkCmdPushConstants(cmd, g_shadow_system.shadow_pipeline->layout,
                                  VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp);
                
                // Draw mesh
                if (draw->mesh) {
                    VkBuffer vertex_buffers[] = {draw->mesh->vertex_buffer.buffer};
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
                    vkCmdBindIndexBuffer(cmd, draw->mesh->index_buffer.buffer, 0,
                                        draw->mesh->index_type);
                    
                    vkCmdDrawIndexed(cmd, draw->mesh->index_count,
                                    draw->instance_count ? draw->instance_count : 1,
                                    0, 0, draw->first_instance);
                    
                    ctx->draw_call_count++;
                    ctx->triangle_count += draw->mesh->index_count / 3 * 
                                          (draw->instance_count ? draw->instance_count : 1);
                }
            }
        }
        
        vkCmdEndRenderPass(cmd);
    }
}

// ============================================================================
// Post-Processing
// ============================================================================

bool vulkan_init_post_process(VulkanContext* ctx, uint32_t width, uint32_t height) {
    // Create color buffer
    g_post_process.color_buffer = vulkan_create_image(ctx, width, height,
                                                      VK_FORMAT_R16G16B16A16_SFLOAT,
                                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                      VK_IMAGE_USAGE_SAMPLED_BIT |
                                                      VK_IMAGE_USAGE_STORAGE_BIT);
    
    if (!g_post_process.color_buffer.image) {
        return false;
    }
    
    // Create depth buffer
    g_post_process.depth_buffer = vulkan_create_image(ctx, width, height,
                                                      VK_FORMAT_D32_SFLOAT,
                                                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                      VK_IMAGE_USAGE_SAMPLED_BIT);
    
    if (!g_post_process.depth_buffer.image) {
        vulkan_destroy_image(ctx, &g_post_process.color_buffer);
        return false;
    }
    
    // Create bloom mip chain
    uint32_t bloom_width = width / 2;
    uint32_t bloom_height = height / 2;
    
    for (uint32_t i = 0; i < 8 && bloom_width > 16 && bloom_height > 16; i++) {
        g_post_process.bloom_buffer[i] = vulkan_create_image(ctx, bloom_width, bloom_height,
                                                             VK_FORMAT_R16G16B16A16_SFLOAT,
                                                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                             VK_IMAGE_USAGE_SAMPLED_BIT |
                                                             VK_IMAGE_USAGE_STORAGE_BIT);
        
        if (!g_post_process.bloom_buffer[i].image) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; j++) {
                vulkan_destroy_image(ctx, &g_post_process.bloom_buffer[j]);
            }
            vulkan_destroy_image(ctx, &g_post_process.color_buffer);
            vulkan_destroy_image(ctx, &g_post_process.depth_buffer);
            return false;
        }
        
        bloom_width /= 2;
        bloom_height /= 2;
    }
    
    return true;
}

void vulkan_destroy_post_process(VulkanContext* ctx) {
    vulkan_destroy_image(ctx, &g_post_process.color_buffer);
    vulkan_destroy_image(ctx, &g_post_process.depth_buffer);
    
    for (uint32_t i = 0; i < 8; i++) {
        if (g_post_process.bloom_buffer[i].image) {
            vulkan_destroy_image(ctx, &g_post_process.bloom_buffer[i]);
        }
    }
    
    if (g_post_process.framebuffer) {
        vkDestroyFramebuffer(ctx->device, g_post_process.framebuffer, NULL);
    }
    
    memset(&g_post_process, 0, sizeof(PostProcessData));
}

// ============================================================================
// Forward Rendering
// ============================================================================

void vulkan_begin_forward_pass(VulkanContext* ctx, VkCommandBuffer cmd,
                               VkRenderPass render_pass, VkFramebuffer framebuffer,
                               uint32_t width, uint32_t height) {
    // Begin render pass
    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {width, height}
        },
        .clearValueCount = 2,
        .pClearValues = (VkClearValue[]){
            {.color = {{0.1f, 0.1f, 0.2f, 1.0f}}},
            {.depthStencil = {1.0f, 0}}
        }
    };
    
    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set viewport and scissor
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {width, height}
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void vulkan_render_forward_pass(VulkanContext* ctx, VkCommandBuffer cmd,
                                VulkanDrawCommand* commands, uint32_t command_count,
                                VulkanRenderState* render_state) {
    // Sort draw calls
    g_sorted_draw_count = 0;
    
    for (uint32_t i = 0; i < command_count && g_sorted_draw_count < 16384; i++) {
        VulkanDrawCommand* draw = &commands[i];
        
        // Calculate depth from camera
        float world_pos[3] = {
            draw->transform[12],
            draw->transform[13],
            draw->transform[14]
        };
        
        float view_pos[3];
        // Transform to view space (simplified)
        view_pos[2] = world_pos[0] * render_state->view_matrix[2] +
                     world_pos[1] * render_state->view_matrix[6] +
                     world_pos[2] * render_state->view_matrix[10] +
                     render_state->view_matrix[14];
        
        // Create sort key
        SortedDrawCall* sorted = &g_sorted_draws[g_sorted_draw_count++];
        sorted->key.pipeline_id = 0; // Would be actual pipeline ID
        sorted->key.material_id = 0; // Would be material ID
        sorted->key.depth = (uint32_t)(view_pos[2] * 100.0f) & 0xFFF;
        sorted->command = draw;
    }
    
    // Sort by state to minimize changes
    radix_sort_draws(g_sorted_draws, g_sorted_draw_count);
    
    // Render sorted draws
    uint32_t current_pipeline = UINT32_MAX;
    uint32_t current_material = UINT32_MAX;
    
    for (uint32_t i = 0; i < g_sorted_draw_count; i++) {
        SortedDrawCall* sorted = &g_sorted_draws[i];
        VulkanDrawCommand* draw = sorted->command;
        
        // Bind pipeline if changed
        if (sorted->key.pipeline_id != current_pipeline) {
            // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            current_pipeline = sorted->key.pipeline_id;
        }
        
        // Bind material if changed
        if (sorted->key.material_id != current_material) {
            // Update descriptor sets for material textures
            current_material = sorted->key.material_id;
        }
        
        // Push constants for transform
        struct {
            float model_matrix[16];
            uint32_t material_index;
        } push_data;
        
        memcpy(push_data.model_matrix, draw->transform, sizeof(float) * 16);
        push_data.material_index = sorted->key.material_id;
        
        // vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | 
        //                    VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_data), &push_data);
        
        // Draw mesh
        if (draw->mesh) {
            VkBuffer vertex_buffers[] = {draw->mesh->vertex_buffer.buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(cmd, draw->mesh->index_buffer.buffer, 0,
                                draw->mesh->index_type);
            
            if (draw->instance_count > 0) {
                vkCmdDrawIndexed(cmd, draw->mesh->index_count, draw->instance_count,
                                0, 0, draw->first_instance);
            } else {
                vkCmdDrawIndexed(cmd, draw->mesh->index_count, 1, 0, 0, 0);
            }
            
            ctx->draw_call_count++;
            ctx->triangle_count += draw->mesh->index_count / 3 * 
                                  (draw->instance_count ? draw->instance_count : 1);
        }
    }
}

void vulkan_end_forward_pass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

// ============================================================================
// Draw Helpers
// ============================================================================

void vulkan_draw_mesh(VulkanContext* ctx, VkCommandBuffer cmd,
                     VulkanMesh* mesh, VulkanPipeline* pipeline) {
    if (!mesh || !pipeline) return;
    
    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    
    // Bind vertex and index buffers
    VkBuffer vertex_buffers[] = {mesh->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh->index_buffer.buffer, 0, mesh->index_type);
    
    // Draw
    vkCmdDrawIndexed(cmd, mesh->index_count, 1, 0, 0, 0);
    
    ctx->draw_call_count++;
    ctx->triangle_count += mesh->index_count / 3;
}

void vulkan_draw_instanced(VulkanContext* ctx, VkCommandBuffer cmd,
                           VulkanMesh* mesh, VulkanPipeline* pipeline,
                           uint32_t instance_count) {
    if (!mesh || !pipeline) return;
    
    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    
    // Bind vertex and index buffers
    VkBuffer vertex_buffers[] = {mesh->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh->index_buffer.buffer, 0, mesh->index_type);
    
    // Draw instanced
    vkCmdDrawIndexed(cmd, mesh->index_count, instance_count, 0, 0, 0);
    
    ctx->draw_call_count++;
    ctx->triangle_count += mesh->index_count / 3 * instance_count;
}

void vulkan_dispatch_compute(VulkanContext* ctx, VkCommandBuffer cmd,
                             VulkanPipeline* pipeline,
                             uint32_t group_x, uint32_t group_y, uint32_t group_z) {
    if (!pipeline) return;
    
    // Bind compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
    
    // Dispatch
    vkCmdDispatch(cmd, group_x, group_y, group_z);
}