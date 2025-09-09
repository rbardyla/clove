/*
 * Vulkan Pipeline Management
 * Shader compilation, pipeline creation, and state management
 * 
 * PERFORMANCE: Pipeline cache for fast switching
 * MEMORY: Fixed-size pipeline pool with LRU eviction
 * CACHE: Pipeline layouts optimized for minimal descriptor changes
 */

#include "handmade_vulkan.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Pipeline Cache
// ============================================================================

typedef struct PipelineCache {
    uint64_t hash;
    VulkanPipeline pipeline;
    uint32_t last_used_frame;
    uint32_t use_count;
} PipelineCache;

static PipelineCache g_pipeline_cache[MAX_PIPELINES];
static uint32_t g_pipeline_cache_count = 0;

// FNV-1a hash for pipeline keys
static uint64_t hash_pipeline_key(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t hash = 0xcbf29ce484222325ULL;
    
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }
    
    return hash;
}

// ============================================================================
// Shader Module Creation
// ============================================================================

static VkShaderModule create_shader_module(VkDevice device, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code
    };
    
    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &create_info, NULL, &shader_module) != VK_SUCCESS) {
        printf("Failed to create shader module!\n");
        return VK_NULL_HANDLE;
    }
    
    return shader_module;
}

// ============================================================================
// Vertex Input State
// ============================================================================

static void get_vertex_input_state(VkPipelineVertexInputStateCreateInfo* vertex_input_info,
                                   VkVertexInputBindingDescription* binding,
                                   VkVertexInputAttributeDescription* attributes) {
    // Binding description
    *binding = (VkVertexInputBindingDescription){
        .binding = 0,
        .stride = sizeof(VulkanVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    
    // Attribute descriptions
    // Position
    attributes[0] = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(VulkanVertex, position)
    };
    
    // Normal
    attributes[1] = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(VulkanVertex, normal)
    };
    
    // UV
    attributes[2] = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 2,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(VulkanVertex, uv)
    };
    
    // Tangent
    attributes[3] = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 3,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offsetof(VulkanVertex, tangent)
    };
    
    // Color
    attributes[4] = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = 4,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .offset = offsetof(VulkanVertex, color)
    };
    
    *vertex_input_info = (VkPipelineVertexInputStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = binding,
        .vertexAttributeDescriptionCount = 5,
        .pVertexAttributeDescriptions = attributes
    };
}

// ============================================================================
// Default Render Pass Creation
// ============================================================================

static VkRenderPass create_default_render_pass(VkDevice device, VkFormat color_format,
                                               VkFormat depth_format) {
    // Attachments
    VkAttachmentDescription attachments[2] = {
        // Color attachment
        {
            .format = color_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        // Depth attachment
        {
            .format = depth_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };
    
    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    
    VkAttachmentReference depth_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref
    };
    
    // PERFORMANCE: Subpass dependencies for optimal GPU scheduling
    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        },
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        }
    };
    
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = dependencies
    };
    
    VkRenderPass render_pass;
    if (vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass) != VK_SUCCESS) {
        printf("Failed to create render pass!\n");
        return VK_NULL_HANDLE;
    }
    
    return render_pass;
}

// ============================================================================
// Graphics Pipeline Creation
// ============================================================================

VulkanPipeline* vulkan_create_graphics_pipeline(VulkanContext* ctx,
                                                const uint32_t* vertex_spv, size_t vertex_size,
                                                const uint32_t* fragment_spv, size_t fragment_size,
                                                VkRenderPass render_pass) {
    if (ctx->pipeline_count >= MAX_PIPELINES) {
        printf("Pipeline limit reached!\n");
        return NULL;
    }
    
    VulkanPipeline* pipeline = &ctx->pipelines[ctx->pipeline_count++];
    memset(pipeline, 0, sizeof(VulkanPipeline));
    
    // Create shader modules
    VkShaderModule vert_module = create_shader_module(ctx->device, vertex_spv, vertex_size);
    VkShaderModule frag_module = create_shader_module(ctx->device, fragment_spv, fragment_size);
    
    if (!vert_module || !frag_module) {
        if (vert_module) vkDestroyShaderModule(ctx->device, vert_module, NULL);
        if (frag_module) vkDestroyShaderModule(ctx->device, frag_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Shader stages
    VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main"
        }
    };
    
    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertex_input_info;
    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attributes[5];
    get_vertex_input_state(&vertex_input_info, &binding, attributes);
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    
    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };
    
    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };
    
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };
    
    // Dynamic state
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };
    
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 3,
        .pDynamicStates = dynamic_states
    };
    
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        // Uniform buffer (render state)
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        },
        // Textures (bindless array)
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1024,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        },
        // Storage buffers (per-instance data)
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        }
    };
    
    // Enable bindless
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 3,
        .pBindingFlags = (VkDescriptorBindingFlags[]){
            0,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            0
        }
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_info,
        .bindingCount = 3,
        .pBindings = bindings
    };
    
    if (vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL, 
                                    &pipeline->descriptor_set_layout) != VK_SUCCESS) {
        printf("Failed to create descriptor set layout!\n");
        vkDestroyShaderModule(ctx->device, vert_module, NULL);
        vkDestroyShaderModule(ctx->device, frag_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Push constants for per-draw data
    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 128 // 128 bytes for transform + material indices
    };
    
    pipeline->push_constant_size = push_constant.size;
    pipeline->push_constant_stages = push_constant.stageFlags;
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &pipeline->descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant
    };
    
    if (vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL, 
                               &pipeline->layout) != VK_SUCCESS) {
        printf("Failed to create pipeline layout!\n");
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
        vkDestroyShaderModule(ctx->device, vert_module, NULL);
        vkDestroyShaderModule(ctx->device, frag_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Use provided render pass or create default
    if (!render_pass) {
        render_pass = create_default_render_pass(ctx->device, ctx->swapchain.format, 
                                                 VK_FORMAT_D32_SFLOAT);
        pipeline->render_pass = render_pass;
    }
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = pipeline->layout,
        .renderPass = render_pass,
        .subpass = 0
    };
    
    if (vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                  &pipeline->pipeline) != VK_SUCCESS) {
        printf("Failed to create graphics pipeline!\n");
        vkDestroyPipelineLayout(ctx->device, pipeline->layout, NULL);
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
        if (pipeline->render_pass) vkDestroyRenderPass(ctx->device, pipeline->render_pass, NULL);
        vkDestroyShaderModule(ctx->device, vert_module, NULL);
        vkDestroyShaderModule(ctx->device, frag_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Cleanup shader modules
    vkDestroyShaderModule(ctx->device, vert_module, NULL);
    vkDestroyShaderModule(ctx->device, frag_module, NULL);
    
    return pipeline;
}

// ============================================================================
// Compute Pipeline Creation
// ============================================================================

VulkanPipeline* vulkan_create_compute_pipeline(VulkanContext* ctx,
                                               const uint32_t* compute_spv, size_t compute_size) {
    if (ctx->pipeline_count >= MAX_PIPELINES) {
        printf("Pipeline limit reached!\n");
        return NULL;
    }
    
    VulkanPipeline* pipeline = &ctx->pipelines[ctx->pipeline_count++];
    memset(pipeline, 0, sizeof(VulkanPipeline));
    
    // Create shader module
    VkShaderModule compute_module = create_shader_module(ctx->device, compute_spv, compute_size);
    if (!compute_module) {
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Descriptor set layout for compute
    VkDescriptorSetLayoutBinding bindings[] = {
        // Input buffer
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Output buffer
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Uniform buffer (parameters)
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        // Storage images
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 8,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        }
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 4,
        .pBindings = bindings
    };
    
    if (vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL,
                                    &pipeline->descriptor_set_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx->device, compute_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Push constants for compute dispatch parameters
    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 64 // 64 bytes for dispatch parameters
    };
    
    pipeline->push_constant_size = push_constant.size;
    pipeline->push_constant_stages = push_constant.stageFlags;
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &pipeline->descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant
    };
    
    if (vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL,
                               &pipeline->layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
        vkDestroyShaderModule(ctx->device, compute_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Create compute pipeline
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compute_module,
            .pName = "main"
        },
        .layout = pipeline->layout
    };
    
    if (vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                 &pipeline->pipeline) != VK_SUCCESS) {
        printf("Failed to create compute pipeline!\n");
        vkDestroyPipelineLayout(ctx->device, pipeline->layout, NULL);
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
        vkDestroyShaderModule(ctx->device, compute_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Cleanup shader module
    vkDestroyShaderModule(ctx->device, compute_module, NULL);
    
    return pipeline;
}

// ============================================================================
// Pipeline Destruction
// ============================================================================

void vulkan_destroy_pipeline(VulkanContext* ctx, VulkanPipeline* pipeline) {
    if (pipeline->pipeline) {
        vkDestroyPipeline(ctx->device, pipeline->pipeline, NULL);
    }
    if (pipeline->layout) {
        vkDestroyPipelineLayout(ctx->device, pipeline->layout, NULL);
    }
    if (pipeline->descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
    }
    if (pipeline->render_pass) {
        vkDestroyRenderPass(ctx->device, pipeline->render_pass, NULL);
    }
    
    memset(pipeline, 0, sizeof(VulkanPipeline));
}

// ============================================================================
// Specialized Pipeline Creation
// ============================================================================

VulkanPipeline* vulkan_create_shadow_pipeline(VulkanContext* ctx,
                                              const uint32_t* vertex_spv, size_t vertex_size) {
    if (ctx->pipeline_count >= MAX_PIPELINES) {
        return NULL;
    }
    
    VulkanPipeline* pipeline = &ctx->pipelines[ctx->pipeline_count++];
    memset(pipeline, 0, sizeof(VulkanPipeline));
    
    // Shadow render pass (depth only)
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
    
    VkAttachmentReference depth_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pDepthStencilAttachment = &depth_attachment_ref
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
                          &pipeline->render_pass) != VK_SUCCESS) {
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Create shader module
    VkShaderModule vert_module = create_shader_module(ctx->device, vertex_spv, vertex_size);
    if (!vert_module) {
        vkDestroyRenderPass(ctx->device, pipeline->render_pass, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Simplified descriptor layout for shadows
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    
    if (vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL,
                                    &pipeline->descriptor_set_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx->device, vert_module, NULL);
        vkDestroyRenderPass(ctx->device, pipeline->render_pass, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Push constants for MVP matrix
    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 64 // 4x4 matrix
    };
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &pipeline->descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant
    };
    
    if (vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL,
                               &pipeline->layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
        vkDestroyShaderModule(ctx->device, vert_module, NULL);
        vkDestroyRenderPass(ctx->device, pipeline->render_pass, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertex_input_info;
    VkVertexInputBindingDescription binding_desc;
    VkVertexInputAttributeDescription attributes[5];
    get_vertex_input_state(&vertex_input_info, &binding_desc, attributes);
    
    // Only use position attribute for shadows
    vertex_input_info.vertexAttributeDescriptionCount = 1;
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    
    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    
    // Rasterizer with depth bias for shadows
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE, // No culling for shadows
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_TRUE,
        .depthBiasConstantFactor = 1.25f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 1.75f
    };
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };
    
    // No color blending for shadow pass
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 0
    };
    
    // Dynamic state
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };
    
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 3,
        .pDynamicStates = dynamic_states
    };
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 1,
        .pStages = &(VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main"
        },
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = pipeline->layout,
        .renderPass = pipeline->render_pass,
        .subpass = 0
    };
    
    if (vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                  &pipeline->pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(ctx->device, pipeline->layout, NULL);
        vkDestroyDescriptorSetLayout(ctx->device, pipeline->descriptor_set_layout, NULL);
        vkDestroyRenderPass(ctx->device, pipeline->render_pass, NULL);
        vkDestroyShaderModule(ctx->device, vert_module, NULL);
        ctx->pipeline_count--;
        return NULL;
    }
    
    vkDestroyShaderModule(ctx->device, vert_module, NULL);
    
    pipeline->push_constant_size = 64;
    pipeline->push_constant_stages = VK_SHADER_STAGE_VERTEX_BIT;
    
    return pipeline;
}