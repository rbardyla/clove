/*
 * Handmade Vulkan Core Implementation
 * Direct Vulkan API usage with zero abstractions
 * 
 * PERFORMANCE: All hot paths optimized for minimal driver overhead
 * MEMORY: Custom allocator with fixed-size pools
 * CACHE: Memory layout optimized for GPU cache efficiency
 */

#include "handmade_vulkan.h"
// #include "platform.h"  // Not needed for standalone build
#include <string.h>
#include <stdio.h>
#define PLATFORM_LINUX 1

// Platform structure definition for standalone build
#ifdef PLATFORM_LINUX
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
struct Platform {
    xcb_connection_t* xcb_connection;
    xcb_window_t xcb_window;
};
#elif defined(PLATFORM_WINDOWS)
#include <vulkan/vulkan_win32.h>
#endif

// ============================================================================
// Debug Callback
// ============================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        printf("Vulkan: %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}

// ============================================================================
// Memory Management
// ============================================================================

static uint32_t find_memory_type(VulkanContext* ctx, uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties* mem_props = &ctx->allocator.memory_properties;
    
    for (uint32_t i = 0; i < mem_props->memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && 
            (mem_props->memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    printf("Failed to find suitable memory type!\n");
    return UINT32_MAX;
}

static VulkanMemoryBlock* allocate_memory_block(VulkanContext* ctx,
                                               VkDeviceSize size,
                                               uint32_t memory_type_index) {
    if (ctx->allocator.block_count >= MAX_MEMORY_TYPES * 4) {
        return NULL;
    }
    
    VulkanMemoryBlock* block = &ctx->allocator.blocks[ctx->allocator.block_count++];
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .memoryTypeIndex = memory_type_index
    };
    
    if (vkAllocateMemory(ctx->device, &alloc_info, NULL, &block->memory) != VK_SUCCESS) {
        ctx->allocator.block_count--;
        return NULL;
    }
    
    block->size = size;
    block->used = 0;
    block->memory_type_index = memory_type_index;
    block->allocation_count = 0;
    
    // Map if host visible
    VkMemoryPropertyFlags flags = ctx->allocator.memory_properties.memoryTypes[memory_type_index].propertyFlags;
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        vkMapMemory(ctx->device, block->memory, 0, size, 0, &block->mapped_ptr);
    }
    
    ctx->allocator.total_allocated += size;
    
    return block;
}

static VulkanMemoryBlock* find_or_allocate_memory(VulkanContext* ctx,
                                                  VkMemoryRequirements requirements,
                                                  VkMemoryPropertyFlags properties) {
    uint32_t memory_type = find_memory_type(ctx, requirements.memoryTypeBits, properties);
    if (memory_type == UINT32_MAX) {
        return NULL;
    }
    
    // PERFORMANCE: First-fit allocation strategy for speed
    for (uint32_t i = 0; i < ctx->allocator.block_count; i++) {
        VulkanMemoryBlock* block = &ctx->allocator.blocks[i];
        if (block->memory_type_index == memory_type) {
            VkDeviceSize aligned_offset = VULKAN_ALIGN(block->used);
            if (aligned_offset + requirements.size <= block->size) {
                return block;
            }
        }
    }
    
    // Allocate new block
    VkDeviceSize block_size = requirements.size > VULKAN_DEVICE_MEMORY_BLOCK_SIZE ?
                              requirements.size : VULKAN_DEVICE_MEMORY_BLOCK_SIZE;
    return allocate_memory_block(ctx, block_size, memory_type);
}

// ============================================================================
// Instance and Device Creation
// ============================================================================

static bool create_instance(VulkanContext* ctx) {
    // Application info
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Handmade Vulkan",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Handmade Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };
    
    // Extensions
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef PLATFORM_LINUX
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(PLATFORM_WINDOWS)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
    };
    uint32_t extension_count = sizeof(extensions) / sizeof(extensions[0]);
    
    // Layers
    const char* layers[] = {
#ifdef DEBUG
        "VK_LAYER_KHRONOS_validation"
#endif
    };
    uint32_t layer_count = 0;
#ifdef DEBUG
    layer_count = 1;
#endif
    
    // Create instance
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers
    };
    
    if (vkCreateInstance(&create_info, NULL, &ctx->instance) != VK_SUCCESS) {
        printf("Failed to create Vulkan instance!\n");
        return false;
    }
    
#ifdef DEBUG
    // Setup debug messenger
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vulkan_debug_callback
    };
    
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(ctx->instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        func(ctx->instance, &debug_create_info, NULL, &ctx->debug_messenger);
    }
#endif
    
    return true;
}

static bool select_physical_device(VulkanContext* ctx) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);
    
    if (device_count == 0) {
        printf("No Vulkan devices found!\n");
        return false;
    }
    
    VkPhysicalDevice devices[32];
    device_count = device_count > 32 ? 32 : device_count;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);
    
    // PERFORMANCE: Prefer discrete GPU for maximum performance
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            ctx->physical_device = devices[i];
            printf("Selected GPU: %s\n", props.deviceName);
            
            // Store memory properties
            vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, 
                                               &ctx->allocator.memory_properties);
            return true;
        }
    }
    
    // Fallback to first device
    ctx->physical_device = devices[0];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx->physical_device, &props);
    printf("Selected GPU: %s\n", props.deviceName);
    
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, 
                                       &ctx->allocator.memory_properties);
    return true;
}

static bool find_queue_families(VulkanContext* ctx, uint32_t* graphics_family,
                               uint32_t* compute_family, uint32_t* transfer_family) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_family_count, NULL);
    
    VkQueueFamilyProperties queue_families[32];
    queue_family_count = queue_family_count > 32 ? 32 : queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_family_count, queue_families);
    
    *graphics_family = UINT32_MAX;
    *compute_family = UINT32_MAX;
    *transfer_family = UINT32_MAX;
    
    // PERFORMANCE: Find dedicated queues for async compute and transfer
    for (uint32_t i = 0; i < queue_family_count; i++) {
        // Graphics queue (also supports compute and transfer)
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (*graphics_family == UINT32_MAX) {
                VkBool32 present_support = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(ctx->physical_device, i, 
                                                     ctx->surface, &present_support);
                if (present_support) {
                    *graphics_family = i;
                }
            }
        }
        
        // Dedicated compute queue
        if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            if (*compute_family == UINT32_MAX) {
                *compute_family = i;
            }
        }
        
        // Dedicated transfer queue
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            if (*transfer_family == UINT32_MAX) {
                *transfer_family = i;
            }
        }
    }
    
    // Fallback to graphics queue for compute/transfer if no dedicated queues
    if (*compute_family == UINT32_MAX) *compute_family = *graphics_family;
    if (*transfer_family == UINT32_MAX) *transfer_family = *graphics_family;
    
    return *graphics_family != UINT32_MAX;
}

static bool create_logical_device(VulkanContext* ctx) {
    uint32_t graphics_family, compute_family, transfer_family;
    if (!find_queue_families(ctx, &graphics_family, &compute_family, &transfer_family)) {
        return false;
    }
    
    // Queue create infos
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos[3];
    uint32_t queue_create_count = 0;
    
    uint32_t unique_families[3] = {graphics_family, compute_family, transfer_family};
    uint32_t unique_count = 1;
    
    // Remove duplicates
    for (uint32_t i = 1; i < 3; i++) {
        bool is_unique = true;
        for (uint32_t j = 0; j < unique_count; j++) {
            if (unique_families[i] == unique_families[j]) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) {
            unique_families[unique_count++] = unique_families[i];
        }
    }
    
    for (uint32_t i = 0; i < unique_count; i++) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
        queue_create_count++;
    }
    
    // Device features
    VkPhysicalDeviceFeatures device_features = {
        .geometryShader = VK_TRUE,
        .tessellationShader = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
        .wideLines = VK_TRUE,
        .largePoints = VK_TRUE,
        .multiDrawIndirect = VK_TRUE,
        .drawIndirectFirstInstance = VK_TRUE,
        .shaderStorageImageExtendedFormats = VK_TRUE
    };
    
    // Extensions
    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };
    
    // Enable bindless textures
    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE
    };
    
    // Create device
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &indexing_features,
        .queueCreateInfoCount = queue_create_count,
        .pQueueCreateInfos = queue_create_infos,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]),
        .ppEnabledExtensionNames = device_extensions
    };
    
    if (vkCreateDevice(ctx->physical_device, &create_info, NULL, &ctx->device) != VK_SUCCESS) {
        printf("Failed to create logical device!\n");
        return false;
    }
    
    // Get queues
    ctx->graphics_queue.family_index = graphics_family;
    ctx->compute_queue.family_index = compute_family;
    ctx->transfer_queue.family_index = transfer_family;
    
    vkGetDeviceQueue(ctx->device, graphics_family, 0, &ctx->graphics_queue.queue);
    vkGetDeviceQueue(ctx->device, compute_family, 0, &ctx->compute_queue.queue);
    vkGetDeviceQueue(ctx->device, transfer_family, 0, &ctx->transfer_queue.queue);
    
    return true;
}

// ============================================================================
// Swapchain Management
// ============================================================================

static VkSurfaceFormatKHR choose_swap_surface_format(VkSurfaceFormatKHR* formats, 
                                                     uint32_t format_count) {
    // PERFORMANCE: Prefer BGRA8 SRGB for optimal performance
    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }
    
    return formats[0];
}

static VkPresentModeKHR choose_swap_present_mode(VkPresentModeKHR* present_modes,
                                                 uint32_t present_mode_count) {
    // PERFORMANCE: Prefer mailbox for low latency, fallback to FIFO
    for (uint32_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_modes[i];
        }
    }
    
    return VK_PRESENT_MODE_FIFO_KHR;
}

static bool create_swapchain(VulkanContext* ctx, uint32_t width, uint32_t height) {
    // Query swap chain support
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device, ctx->surface, &capabilities);
    
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface, &format_count, NULL);
    VkSurfaceFormatKHR formats[32];
    format_count = format_count > 32 ? 32 : format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface, &format_count, formats);
    
    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface, &present_mode_count, NULL);
    VkPresentModeKHR present_modes[8];
    present_mode_count = present_mode_count > 8 ? 8 : present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface, &present_mode_count, present_modes);
    
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(formats, format_count);
    VkPresentModeKHR present_mode = choose_swap_present_mode(present_modes, present_mode_count);
    
    // Swap extent
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = width;
        extent.height = height;
        
        extent.width = extent.width < capabilities.minImageExtent.width ? 
                      capabilities.minImageExtent.width : extent.width;
        extent.width = extent.width > capabilities.maxImageExtent.width ? 
                      capabilities.maxImageExtent.width : extent.width;
        extent.height = extent.height < capabilities.minImageExtent.height ? 
                       capabilities.minImageExtent.height : extent.height;
        extent.height = extent.height > capabilities.maxImageExtent.height ? 
                       capabilities.maxImageExtent.height : extent.height;
    }
    
    // Image count (triple buffering if possible)
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }
    if (image_count > MAX_SWAPCHAIN_IMAGES) {
        image_count = MAX_SWAPCHAIN_IMAGES;
    }
    
    // Create swapchain
    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = ctx->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    
    if (vkCreateSwapchainKHR(ctx->device, &create_info, NULL, &ctx->swapchain.swapchain) != VK_SUCCESS) {
        printf("Failed to create swap chain!\n");
        return false;
    }
    
    ctx->swapchain.format = surface_format.format;
    ctx->swapchain.extent = extent;
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain.swapchain, &ctx->swapchain.image_count, NULL);
    vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain.swapchain, &ctx->swapchain.image_count, ctx->swapchain.images);
    
    // Create image views
    for (uint32_t i = 0; i < ctx->swapchain.image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = ctx->swapchain.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = ctx->swapchain.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        
        if (vkCreateImageView(ctx->device, &view_info, NULL, &ctx->swapchain.image_views[i]) != VK_SUCCESS) {
            printf("Failed to create image view!\n");
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Command Pool and Buffers
// ============================================================================

static bool create_command_pools(VulkanContext* ctx) {
    // Graphics command pool
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->graphics_queue.family_index
    };
    
    if (vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->graphics_queue.command_pool) != VK_SUCCESS) {
        return false;
    }
    
    // Compute command pool
    pool_info.queueFamilyIndex = ctx->compute_queue.family_index;
    if (vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->compute_queue.command_pool) != VK_SUCCESS) {
        return false;
    }
    
    // Transfer command pool
    pool_info.queueFamilyIndex = ctx->transfer_queue.family_index;
    if (vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->transfer_queue.command_pool) != VK_SUCCESS) {
        return false;
    }
    
    // Allocate command buffers for frames in flight
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->graphics_queue.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };
    
    VkCommandBuffer frame_buffers[MAX_FRAMES_IN_FLIGHT];
    if (vkAllocateCommandBuffers(ctx->device, &alloc_info, frame_buffers) != VK_SUCCESS) {
        return false;
    }
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ctx->frames[i].command_buffer = frame_buffers[i];
    }
    
    return true;
}

// ============================================================================
// Synchronization Objects
// ============================================================================

static bool create_sync_objects(VulkanContext* ctx) {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(ctx->device, &semaphore_info, NULL, 
                             &ctx->frames[i].image_available_semaphore) != VK_SUCCESS ||
            vkCreateSemaphore(ctx->device, &semaphore_info, NULL, 
                             &ctx->frames[i].render_finished_semaphore) != VK_SUCCESS ||
            vkCreateFence(ctx->device, &fence_info, NULL, 
                         &ctx->frames[i].in_flight_fence) != VK_SUCCESS) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Descriptor Pool
// ============================================================================

static bool create_descriptor_pool(VulkanContext* ctx) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 }
    };
    
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_DESCRIPTOR_SETS,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };
    
    return vkCreateDescriptorPool(ctx->device, &pool_info, NULL, &ctx->descriptor_pool) == VK_SUCCESS;
}

// ============================================================================
// Samplers
// ============================================================================

static bool create_samplers(VulkanContext* ctx) {
    // Linear sampler
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 1000.0f
    };
    
    if (vkCreateSampler(ctx->device, &sampler_info, NULL, &ctx->linear_sampler) != VK_SUCCESS) {
        return false;
    }
    
    // Nearest sampler
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.anisotropyEnable = VK_FALSE;
    
    if (vkCreateSampler(ctx->device, &sampler_info, NULL, &ctx->nearest_sampler) != VK_SUCCESS) {
        return false;
    }
    
    // Shadow sampler
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.compareEnable = VK_TRUE;
    sampler_info.compareOp = VK_COMPARE_OP_LESS;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    
    if (vkCreateSampler(ctx->device, &sampler_info, NULL, &ctx->shadow_sampler) != VK_SUCCESS) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool vulkan_init(VulkanContext* ctx, Platform* platform, uint32_t width, uint32_t height) {
    // Zero initialize
    memset(ctx, 0, sizeof(VulkanContext));
    
    // Create instance
    if (!create_instance(ctx)) {
        return false;
    }
    
    // Create surface (platform-specific)
#ifdef PLATFORM_LINUX
    VkXcbSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = platform->xcb_connection,
        .window = platform->xcb_window
    };
    
    if (vkCreateXcbSurfaceKHR(ctx->instance, &surface_info, NULL, &ctx->surface) != VK_SUCCESS) {
        printf("Failed to create window surface!\n");
        return false;
    }
#elif defined(PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = platform->hinstance,
        .hwnd = platform->hwnd
    };
    
    if (vkCreateWin32SurfaceKHR(ctx->instance, &surface_info, NULL, &ctx->surface) != VK_SUCCESS) {
        printf("Failed to create window surface!\n");
        return false;
    }
#endif
    
    // Select physical device
    if (!select_physical_device(ctx)) {
        return false;
    }
    
    // Create logical device
    if (!create_logical_device(ctx)) {
        return false;
    }
    
    // Create swapchain
    if (!create_swapchain(ctx, width, height)) {
        return false;
    }
    
    // Create command pools
    if (!create_command_pools(ctx)) {
        return false;
    }
    
    // Create sync objects
    if (!create_sync_objects(ctx)) {
        return false;
    }
    
    // Create descriptor pool
    if (!create_descriptor_pool(ctx)) {
        return false;
    }
    
    // Create samplers
    if (!create_samplers(ctx)) {
        return false;
    }
    
    printf("Vulkan initialized successfully!\n");
    return true;
}

void vulkan_shutdown(VulkanContext* ctx) {
    vkDeviceWaitIdle(ctx->device);
    
    // Destroy samplers
    if (ctx->linear_sampler) vkDestroySampler(ctx->device, ctx->linear_sampler, NULL);
    if (ctx->nearest_sampler) vkDestroySampler(ctx->device, ctx->nearest_sampler, NULL);
    if (ctx->shadow_sampler) vkDestroySampler(ctx->device, ctx->shadow_sampler, NULL);
    
    // Destroy descriptor pool
    if (ctx->descriptor_pool) vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, NULL);
    
    // Destroy sync objects
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(ctx->device, ctx->frames[i].image_available_semaphore, NULL);
        vkDestroySemaphore(ctx->device, ctx->frames[i].render_finished_semaphore, NULL);
        vkDestroyFence(ctx->device, ctx->frames[i].in_flight_fence, NULL);
    }
    
    // Destroy command pools
    vkDestroyCommandPool(ctx->device, ctx->graphics_queue.command_pool, NULL);
    vkDestroyCommandPool(ctx->device, ctx->compute_queue.command_pool, NULL);
    vkDestroyCommandPool(ctx->device, ctx->transfer_queue.command_pool, NULL);
    
    // Destroy swapchain
    for (uint32_t i = 0; i < ctx->swapchain.image_count; i++) {
        vkDestroyImageView(ctx->device, ctx->swapchain.image_views[i], NULL);
    }
    vkDestroySwapchainKHR(ctx->device, ctx->swapchain.swapchain, NULL);
    
    // Free memory blocks
    for (uint32_t i = 0; i < ctx->allocator.block_count; i++) {
        if (ctx->allocator.blocks[i].mapped_ptr) {
            vkUnmapMemory(ctx->device, ctx->allocator.blocks[i].memory);
        }
        vkFreeMemory(ctx->device, ctx->allocator.blocks[i].memory, NULL);
    }
    
    // Destroy device
    vkDestroyDevice(ctx->device, NULL);
    
    // Destroy surface
    vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
    
#ifdef DEBUG
    // Destroy debug messenger
    if (ctx->debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(ctx->instance, ctx->debug_messenger, NULL);
        }
    }
#endif
    
    // Destroy instance
    vkDestroyInstance(ctx->instance, NULL);
}

bool vulkan_recreate_swapchain(VulkanContext* ctx, uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(ctx->device);
    
    // Destroy old swapchain resources
    for (uint32_t i = 0; i < ctx->swapchain.image_count; i++) {
        vkDestroyImageView(ctx->device, ctx->swapchain.image_views[i], NULL);
        if (ctx->swapchain.framebuffers[i]) {
            vkDestroyFramebuffer(ctx->device, ctx->swapchain.framebuffers[i], NULL);
        }
    }
    
    VkSwapchainKHR old_swapchain = ctx->swapchain.swapchain;
    
    // Create new swapchain
    if (!create_swapchain(ctx, width, height)) {
        return false;
    }
    
    // Destroy old swapchain
    vkDestroySwapchainKHR(ctx->device, old_swapchain, NULL);
    
    return true;
}

bool vulkan_begin_frame(VulkanContext* ctx) {
    VulkanFrameData* frame = &ctx->frames[ctx->current_frame];
    
    // Wait for previous frame
    vkWaitForFences(ctx->device, 1, &frame->in_flight_fence, VK_TRUE, UINT64_MAX);
    
    // Acquire next image
    VkResult result = vkAcquireNextImageKHR(ctx->device, ctx->swapchain.swapchain,
                                           UINT64_MAX, frame->image_available_semaphore,
                                           VK_NULL_HANDLE, &ctx->image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Need to recreate swapchain
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("Failed to acquire swap chain image!\n");
        return false;
    }
    
    vkResetFences(ctx->device, 1, &frame->in_flight_fence);
    
    // Reset command buffer
    vkResetCommandBuffer(frame->command_buffer, 0);
    
    // Begin command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    if (vkBeginCommandBuffer(frame->command_buffer, &begin_info) != VK_SUCCESS) {
        printf("Failed to begin recording command buffer!\n");
        return false;
    }
    
    // Reset frame statistics
    ctx->draw_call_count = 0;
    ctx->triangle_count = 0;
    
    return true;
}

void vulkan_end_frame(VulkanContext* ctx) {
    VulkanFrameData* frame = &ctx->frames[ctx->current_frame];
    
    // End command buffer
    if (vkEndCommandBuffer(frame->command_buffer) != VK_SUCCESS) {
        printf("Failed to record command buffer!\n");
        return;
    }
    
    // Submit
    VkSemaphore wait_semaphores[] = {frame->image_available_semaphore};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {frame->render_finished_semaphore};
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame->command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores
    };
    
    if (vkQueueSubmit(ctx->graphics_queue.queue, 1, &submit_info, frame->in_flight_fence) != VK_SUCCESS) {
        printf("Failed to submit draw command buffer!\n");
        return;
    }
    
    // Present
    VkSwapchainKHR swapchains[] = {ctx->swapchain.swapchain};
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &ctx->image_index
    };
    
    vkQueuePresentKHR(ctx->graphics_queue.queue, &present_info);
    
    ctx->current_frame = (ctx->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    ctx->frame_count++;
}

void vulkan_wait_idle(VulkanContext* ctx) {
    vkDeviceWaitIdle(ctx->device);
}

// ============================================================================
// Buffer Management
// ============================================================================

VulkanBuffer vulkan_create_buffer(VulkanContext* ctx, VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags properties) {
    VulkanBuffer buffer = {0};
    
    // Create buffer
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (vkCreateBuffer(ctx->device, &buffer_info, NULL, &buffer.buffer) != VK_SUCCESS) {
        printf("Failed to create buffer!\n");
        return buffer;
    }
    
    // Get memory requirements
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(ctx->device, buffer.buffer, &mem_requirements);
    
    // Allocate memory
    VulkanMemoryBlock* block = find_or_allocate_memory(ctx, mem_requirements, properties);
    if (!block) {
        vkDestroyBuffer(ctx->device, buffer.buffer, NULL);
        buffer.buffer = VK_NULL_HANDLE;
        return buffer;
    }
    
    // Bind memory
    buffer.memory_block = block;
    buffer.offset = VULKAN_ALIGN(block->used);
    buffer.size = size;
    buffer.usage = usage;
    
    vkBindBufferMemory(ctx->device, buffer.buffer, block->memory, buffer.offset);
    
    block->used = buffer.offset + size;
    block->allocation_count++;
    ctx->allocator.total_used += size;
    ctx->allocator.allocation_count++;
    
    // Set mapped pointer if host visible
    if (block->mapped_ptr) {
        buffer.mapped_ptr = (uint8_t*)block->mapped_ptr + buffer.offset;
    }
    
    return buffer;
}

void vulkan_destroy_buffer(VulkanContext* ctx, VulkanBuffer* buffer) {
    if (buffer->buffer) {
        vkDestroyBuffer(ctx->device, buffer->buffer, NULL);
        
        if (buffer->memory_block) {
            buffer->memory_block->allocation_count--;
            ctx->allocator.total_used -= buffer->size;
            ctx->allocator.allocation_count--;
        }
        
        memset(buffer, 0, sizeof(VulkanBuffer));
    }
}

void vulkan_copy_buffer(VulkanContext* ctx, VulkanBuffer* src,
                       VulkanBuffer* dst, VkDeviceSize size) {
    VkCommandBuffer cmd = vulkan_begin_single_time_commands(ctx);
    
    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    
    vkCmdCopyBuffer(cmd, src->buffer, dst->buffer, 1, &copy_region);
    
    vulkan_end_single_time_commands(ctx, cmd);
}

void* vulkan_map_buffer(VulkanContext* ctx, VulkanBuffer* buffer) {
    if (buffer->mapped_ptr) {
        return buffer->mapped_ptr;
    }
    
    if (buffer->memory_block && buffer->memory_block->mapped_ptr) {
        return (uint8_t*)buffer->memory_block->mapped_ptr + buffer->offset;
    }
    
    return NULL;
}

void vulkan_unmap_buffer(VulkanContext* ctx, VulkanBuffer* buffer) {
    // Memory stays mapped for the lifetime of the block
}

// ============================================================================
// Command Buffer Utilities
// ============================================================================

VkCommandBuffer vulkan_begin_single_time_commands(VulkanContext* ctx) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = ctx->transfer_queue.command_pool,
        .commandBufferCount = 1
    };
    
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &command_buffer);
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    vkBeginCommandBuffer(command_buffer, &begin_info);
    
    return command_buffer;
}

void vulkan_end_single_time_commands(VulkanContext* ctx, VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer
    };
    
    vkQueueSubmit(ctx->transfer_queue.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->transfer_queue.queue);
    
    vkFreeCommandBuffers(ctx->device, ctx->transfer_queue.command_pool, 1, &command_buffer);
}

// ============================================================================
// Statistics
// ============================================================================

void vulkan_get_stats(VulkanContext* ctx, uint32_t* draw_calls,
                     uint32_t* triangles, double* gpu_time_ms) {
    if (draw_calls) *draw_calls = ctx->draw_call_count;
    if (triangles) *triangles = ctx->triangle_count;
    if (gpu_time_ms) *gpu_time_ms = ctx->gpu_time_ms;
}