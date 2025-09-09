/*
 * Vulkan Resource Management
 * Buffer, image, and memory management with custom allocator
 * 
 * PERFORMANCE: Sub-allocation from large memory blocks
 * MEMORY: Ring buffers for streaming data
 * CACHE: Aligned allocations for optimal GPU access
 */

#include "handmade_vulkan.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Ring Buffer for Streaming
// ============================================================================

typedef struct RingBuffer {
    VulkanBuffer buffer;
    VkDeviceSize size;
    VkDeviceSize write_offset;
    VkDeviceSize frame_offsets[MAX_FRAMES_IN_FLIGHT];
    void* mapped_data;
} RingBuffer;

static RingBuffer g_uniform_ring_buffer;
static RingBuffer g_vertex_ring_buffer;
static RingBuffer g_index_ring_buffer;

// ============================================================================
// Transfer Queue Management
// ============================================================================

typedef struct TransferCommand {
    VkCommandBuffer cmd;
    VkFence fence;
    VulkanBuffer staging_buffer;
    bool in_use;
} TransferCommand;

static TransferCommand g_transfer_commands[8];
static uint32_t g_transfer_command_count = 0;

// ============================================================================
// Image Creation and Management
// ============================================================================

VulkanImage vulkan_create_image(VulkanContext* ctx, uint32_t width, uint32_t height,
                                VkFormat format, VkImageUsageFlags usage) {
    VulkanImage image = {0};
    
    // Determine aspect mask
    VkImageAspectFlags aspect_mask = 0;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM) {
            aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else if (format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    
    // Calculate mip levels
    uint32_t mip_levels = 1;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        // Calculate mip levels for textures
        uint32_t max_dim = width > height ? width : height;
        while (max_dim > 1) {
            mip_levels++;
            max_dim >>= 1;
        }
    }
    
    // Create image
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1
        },
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .format = format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT
    };
    
    if (vkCreateImage(ctx->device, &image_info, NULL, &image.image) != VK_SUCCESS) {
        printf("Failed to create image!\n");
        return image;
    }
    
    // Get memory requirements
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(ctx->device, image.image, &mem_requirements);
    
    // Find memory type
    uint32_t memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < ctx->allocator.memory_properties.memoryTypeCount; i++) {
        if ((mem_requirements.memoryTypeBits & (1 << i)) &&
            (ctx->allocator.memory_properties.memoryTypes[i].propertyFlags & 
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memory_type = i;
            break;
        }
    }
    
    if (memory_type == UINT32_MAX) {
        vkDestroyImage(ctx->device, image.image, NULL);
        image.image = VK_NULL_HANDLE;
        return image;
    }
    
    // Allocate from memory block
    VulkanMemoryBlock* block = NULL;
    for (uint32_t i = 0; i < ctx->allocator.block_count; i++) {
        if (ctx->allocator.blocks[i].memory_type_index == memory_type) {
            VkDeviceSize aligned_offset = VULKAN_ALIGN(ctx->allocator.blocks[i].used);
            if (aligned_offset + mem_requirements.size <= ctx->allocator.blocks[i].size) {
                block = &ctx->allocator.blocks[i];
                break;
            }
        }
    }
    
    // Allocate new block if needed
    if (!block) {
        if (ctx->allocator.block_count >= MAX_MEMORY_TYPES * 4) {
            vkDestroyImage(ctx->device, image.image, NULL);
            image.image = VK_NULL_HANDLE;
            return image;
        }
        
        block = &ctx->allocator.blocks[ctx->allocator.block_count++];
        
        VkDeviceSize block_size = mem_requirements.size > VULKAN_DEVICE_MEMORY_BLOCK_SIZE ?
                                  mem_requirements.size : VULKAN_DEVICE_MEMORY_BLOCK_SIZE;
        
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = block_size,
            .memoryTypeIndex = memory_type
        };
        
        if (vkAllocateMemory(ctx->device, &alloc_info, NULL, &block->memory) != VK_SUCCESS) {
            ctx->allocator.block_count--;
            vkDestroyImage(ctx->device, image.image, NULL);
            image.image = VK_NULL_HANDLE;
            return image;
        }
        
        block->size = block_size;
        block->used = 0;
        block->memory_type_index = memory_type;
        block->allocation_count = 0;
        
        ctx->allocator.total_allocated += block_size;
    }
    
    // Bind memory
    image.memory_block = block;
    image.offset = VULKAN_ALIGN(block->used);
    image.size = mem_requirements.size;
    
    vkBindImageMemory(ctx->device, image.image, block->memory, image.offset);
    
    block->used = image.offset + mem_requirements.size;
    block->allocation_count++;
    ctx->allocator.total_used += mem_requirements.size;
    ctx->allocator.allocation_count++;
    
    // Store image properties
    image.format = format;
    image.extent = (VkExtent3D){width, height, 1};
    image.mip_levels = mip_levels;
    image.array_layers = 1;
    
    // Create image view
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    
    if (vkCreateImageView(ctx->device, &view_info, NULL, &image.view) != VK_SUCCESS) {
        printf("Failed to create image view!\n");
        // Still return the image, view creation can be retried
    }
    
    return image;
}

VulkanImage vulkan_create_texture(VulkanContext* ctx, uint32_t width, uint32_t height,
                                  VkFormat format, void* pixels) {
    // Create image with transfer dst and sampled usage
    VulkanImage image = vulkan_create_image(ctx, width, height, format,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_SAMPLED_BIT);
    
    if (!image.image) {
        return image;
    }
    
    // Calculate buffer size
    VkDeviceSize image_size = width * height * 4; // Assuming RGBA8
    if (format == VK_FORMAT_R8_UNORM) {
        image_size = width * height;
    } else if (format == VK_FORMAT_R8G8_UNORM) {
        image_size = width * height * 2;
    }
    
    // Create staging buffer
    VulkanBuffer staging_buffer = vulkan_create_buffer(ctx, image_size,
                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (!staging_buffer.buffer) {
        vulkan_destroy_image(ctx, &image);
        return (VulkanImage){0};
    }
    
    // Copy pixel data to staging buffer
    void* data = vulkan_map_buffer(ctx, &staging_buffer);
    if (data) {
        memcpy(data, pixels, image_size);
    }
    
    // Start transfer
    VkCommandBuffer cmd = vulkan_begin_single_time_commands(ctx);
    
    // Transition image to transfer destination
    vulkan_transition_image_layout(ctx, cmd, image.image, format,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    
    vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Generate mipmaps if needed
    if (image.mip_levels > 1) {
        // Transition first mip level to transfer source
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = image.image,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = 0,
                .layerCount = 1,
                .levelCount = 1
            }
        };
        
        int32_t mip_width = width;
        int32_t mip_height = height;
        
        for (uint32_t i = 1; i < image.mip_levels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, NULL, 0, NULL, 1, &barrier);
            
            // Blit from previous level
            VkImageBlit blit = {
                .srcOffsets = {{0, 0, 0}, {mip_width, mip_height, 1}},
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i - 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .dstOffsets = {{0, 0, 0}, {mip_width > 1 ? mip_width / 2 : 1,
                                          mip_height > 1 ? mip_height / 2 : 1, 1}},
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };
            
            vkCmdBlitImage(cmd,
                image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);
            
            // Transition previous level to shader read
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, NULL, 0, NULL, 1, &barrier);
            
            if (mip_width > 1) mip_width /= 2;
            if (mip_height > 1) mip_height /= 2;
        }
        
        // Transition last mip level
        barrier.subresourceRange.baseMipLevel = image.mip_levels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, NULL, 0, NULL, 1, &barrier);
    } else {
        // Transition to shader read
        vulkan_transition_image_layout(ctx, cmd, image.image, format,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    
    vulkan_end_single_time_commands(ctx, cmd);
    
    // Cleanup staging buffer
    vulkan_destroy_buffer(ctx, &staging_buffer);
    
    return image;
}

void vulkan_destroy_image(VulkanContext* ctx, VulkanImage* image) {
    if (image->view) {
        vkDestroyImageView(ctx->device, image->view, NULL);
    }
    
    if (image->image) {
        vkDestroyImage(ctx->device, image->image, NULL);
        
        if (image->memory_block) {
            image->memory_block->allocation_count--;
            ctx->allocator.total_used -= image->size;
            ctx->allocator.allocation_count--;
        }
    }
    
    memset(image, 0, sizeof(VulkanImage));
}

void vulkan_transition_image_layout(VulkanContext* ctx, VkCommandBuffer cmd,
                                    VkImage image, VkFormat format,
                                    VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    
    // Depth format handling
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM ||
        format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;
    
    // Determine pipeline stages and access masks
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && 
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && 
               new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && 
               new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | 
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        // Generic transition
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    
    vkCmdPipelineBarrier(cmd, source_stage, destination_stage, 0,
                        0, NULL, 0, NULL, 1, &barrier);
}

// ============================================================================
// Mesh Creation and Management
// ============================================================================

VulkanMesh vulkan_create_mesh(VulkanContext* ctx,
                              const VulkanVertex* vertices, uint32_t vertex_count,
                              const uint32_t* indices, uint32_t index_count) {
    VulkanMesh mesh = {0};
    
    // Calculate sizes
    VkDeviceSize vertex_size = sizeof(VulkanVertex) * vertex_count;
    VkDeviceSize index_size = sizeof(uint32_t) * index_count;
    
    // Create vertex buffer
    mesh.vertex_buffer = vulkan_create_buffer(ctx, vertex_size,
                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (!mesh.vertex_buffer.buffer) {
        return mesh;
    }
    
    // Create index buffer
    mesh.index_buffer = vulkan_create_buffer(ctx, index_size,
                                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (!mesh.index_buffer.buffer) {
        vulkan_destroy_buffer(ctx, &mesh.vertex_buffer);
        return mesh;
    }
    
    // Create staging buffer for both
    VkDeviceSize staging_size = vertex_size + index_size;
    VulkanBuffer staging_buffer = vulkan_create_buffer(ctx, staging_size,
                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (!staging_buffer.buffer) {
        vulkan_destroy_buffer(ctx, &mesh.vertex_buffer);
        vulkan_destroy_buffer(ctx, &mesh.index_buffer);
        return mesh;
    }
    
    // Copy data to staging buffer
    void* data = vulkan_map_buffer(ctx, &staging_buffer);
    if (data) {
        memcpy(data, vertices, vertex_size);
        memcpy((uint8_t*)data + vertex_size, indices, index_size);
    }
    
    // Transfer to GPU
    VkCommandBuffer cmd = vulkan_begin_single_time_commands(ctx);
    
    // Copy vertices
    VkBufferCopy vertex_copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = vertex_size
    };
    vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.vertex_buffer.buffer, 1, &vertex_copy);
    
    // Copy indices
    VkBufferCopy index_copy = {
        .srcOffset = vertex_size,
        .dstOffset = 0,
        .size = index_size
    };
    vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.index_buffer.buffer, 1, &index_copy);
    
    // Memory barrier to ensure transfer completes before vertex input
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT
    };
    
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 1, &barrier, 0, NULL, 0, NULL);
    
    vulkan_end_single_time_commands(ctx, cmd);
    
    // Cleanup staging buffer
    vulkan_destroy_buffer(ctx, &staging_buffer);
    
    // Store mesh properties
    mesh.vertex_count = vertex_count;
    mesh.index_count = index_count;
    mesh.index_type = VK_INDEX_TYPE_UINT32;
    
    return mesh;
}

void vulkan_destroy_mesh(VulkanContext* ctx, VulkanMesh* mesh) {
    vulkan_destroy_buffer(ctx, &mesh->vertex_buffer);
    vulkan_destroy_buffer(ctx, &mesh->index_buffer);
    memset(mesh, 0, sizeof(VulkanMesh));
}

// ============================================================================
// Ring Buffer Management
// ============================================================================

static bool init_ring_buffer(VulkanContext* ctx, RingBuffer* ring, 
                             VkDeviceSize size, VkBufferUsageFlags usage) {
    ring->size = size;
    ring->write_offset = 0;
    
    ring->buffer = vulkan_create_buffer(ctx, size, usage,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (!ring->buffer.buffer) {
        return false;
    }
    
    ring->mapped_data = vulkan_map_buffer(ctx, &ring->buffer);
    
    // Initialize frame offsets
    VkDeviceSize frame_size = size / MAX_FRAMES_IN_FLIGHT;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ring->frame_offsets[i] = i * frame_size;
    }
    
    return true;
}

static void* allocate_from_ring_buffer(RingBuffer* ring, VkDeviceSize size, 
                                       VkDeviceSize* offset, uint32_t frame) {
    // CACHE: Align allocations to 256 bytes (common UBO alignment)
    size = (size + 255) & ~255;
    
    VkDeviceSize frame_size = ring->size / MAX_FRAMES_IN_FLIGHT;
    VkDeviceSize frame_start = frame * frame_size;
    VkDeviceSize frame_end = frame_start + frame_size;
    
    // Check if allocation fits in current frame's region
    if (ring->frame_offsets[frame] + size > frame_end) {
        // Wrap to beginning of frame's region
        ring->frame_offsets[frame] = frame_start;
    }
    
    *offset = ring->frame_offsets[frame];
    ring->frame_offsets[frame] += size;
    
    return (uint8_t*)ring->mapped_data + *offset;
}

bool vulkan_init_streaming_buffers(VulkanContext* ctx) {
    // Initialize uniform ring buffer
    if (!init_ring_buffer(ctx, &g_uniform_ring_buffer, VULKAN_UNIFORM_BUFFER_SIZE,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) {
        return false;
    }
    
    // Initialize vertex ring buffer for dynamic vertices
    if (!init_ring_buffer(ctx, &g_vertex_ring_buffer, 16 * 1024 * 1024,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
        vulkan_destroy_buffer(ctx, &g_uniform_ring_buffer.buffer);
        return false;
    }
    
    // Initialize index ring buffer for dynamic indices
    if (!init_ring_buffer(ctx, &g_index_ring_buffer, 4 * 1024 * 1024,
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
        vulkan_destroy_buffer(ctx, &g_uniform_ring_buffer.buffer);
        vulkan_destroy_buffer(ctx, &g_vertex_ring_buffer.buffer);
        return false;
    }
    
    return true;
}

void vulkan_destroy_streaming_buffers(VulkanContext* ctx) {
    vulkan_destroy_buffer(ctx, &g_uniform_ring_buffer.buffer);
    vulkan_destroy_buffer(ctx, &g_vertex_ring_buffer.buffer);
    vulkan_destroy_buffer(ctx, &g_index_ring_buffer.buffer);
}

void* vulkan_allocate_uniform(VulkanContext* ctx, VkDeviceSize size, 
                              VkDeviceSize* offset) {
    return allocate_from_ring_buffer(&g_uniform_ring_buffer, size, offset, 
                                     ctx->current_frame);
}

void* vulkan_allocate_vertices(VulkanContext* ctx, uint32_t vertex_count,
                               VkDeviceSize* offset) {
    VkDeviceSize size = sizeof(VulkanVertex) * vertex_count;
    return allocate_from_ring_buffer(&g_vertex_ring_buffer, size, offset,
                                     ctx->current_frame);
}

void* vulkan_allocate_indices(VulkanContext* ctx, uint32_t index_count,
                              VkDeviceSize* offset) {
    VkDeviceSize size = sizeof(uint32_t) * index_count;
    return allocate_from_ring_buffer(&g_index_ring_buffer, size, offset,
                                     ctx->current_frame);
}

// ============================================================================
// Barrier and Synchronization
// ============================================================================

void vulkan_pipeline_barrier(VulkanContext* ctx, VkCommandBuffer cmd,
                             VkPipelineStageFlags src_stage,
                             VkPipelineStageFlags dst_stage) {
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT
    };
    
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
                        1, &barrier, 0, NULL, 0, NULL);
}

void vulkan_memory_barrier(VulkanContext* ctx, VkCommandBuffer cmd,
                           VkAccessFlags src_access, VkAccessFlags dst_access) {
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access
    };
    
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &barrier, 0, NULL, 0, NULL);
}

// ============================================================================
// Async Transfer Operations
// ============================================================================

bool vulkan_begin_async_transfer(VulkanContext* ctx, TransferCommand** transfer) {
    // Find available transfer command
    for (uint32_t i = 0; i < g_transfer_command_count; i++) {
        if (!g_transfer_commands[i].in_use) {
            // Check if fence is signaled
            if (vkGetFenceStatus(ctx->device, g_transfer_commands[i].fence) == VK_SUCCESS) {
                *transfer = &g_transfer_commands[i];
                
                // Reset fence and command buffer
                vkResetFences(ctx->device, 1, &(*transfer)->fence);
                vkResetCommandBuffer((*transfer)->cmd, 0);
                
                // Begin command buffer
                VkCommandBufferBeginInfo begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
                };
                
                vkBeginCommandBuffer((*transfer)->cmd, &begin_info);
                (*transfer)->in_use = true;
                
                return true;
            }
        }
    }
    
    // Allocate new transfer command if needed
    if (g_transfer_command_count < 8) {
        TransferCommand* new_transfer = &g_transfer_commands[g_transfer_command_count++];
        
        // Allocate command buffer
        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = ctx->transfer_queue.command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        
        vkAllocateCommandBuffers(ctx->device, &alloc_info, &new_transfer->cmd);
        
        // Create fence
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        };
        
        vkCreateFence(ctx->device, &fence_info, NULL, &new_transfer->fence);
        
        // Begin command buffer
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        
        vkBeginCommandBuffer(new_transfer->cmd, &begin_info);
        new_transfer->in_use = true;
        
        *transfer = new_transfer;
        return true;
    }
    
    return false;
}

void vulkan_submit_async_transfer(VulkanContext* ctx, TransferCommand* transfer) {
    vkEndCommandBuffer(transfer->cmd);
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transfer->cmd
    };
    
    vkQueueSubmit(ctx->transfer_queue.queue, 1, &submit_info, transfer->fence);
}

void vulkan_wait_async_transfer(VulkanContext* ctx, TransferCommand* transfer) {
    vkWaitForFences(ctx->device, 1, &transfer->fence, VK_TRUE, UINT64_MAX);
    transfer->in_use = false;
    
    // Cleanup staging buffer if present
    if (transfer->staging_buffer.buffer) {
        vulkan_destroy_buffer(ctx, &transfer->staging_buffer);
    }
}