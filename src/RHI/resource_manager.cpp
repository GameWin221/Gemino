#include "resource_manager.hpp"
#include <cstring>

ResourceManager::ResourceManager(VkDevice device, VmaAllocator allocator, u32 graphics_family_index, u32 transfer_family_index, u32 compute_family_index)
    : VK_DEVICE(device), VK_ALLOCATOR(allocator) {
    std::vector<VkDescriptorPoolSize> pool_sizes {
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, 128U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2048U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0U }
    };

    u32 max_sets{};
    for(const auto &size : pool_sizes) {
        max_sets += size.descriptorCount;
    }

    VkDescriptorPoolCreateInfo descriptor_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data()
    };

    DEBUG_ASSERT(vkCreateDescriptorPool(VK_DEVICE, &descriptor_pool_create_info, nullptr, &m_descriptor_pool) == VK_SUCCESS)

    // This map contains which index handles which families
    // (in a non-perfect scenario one index might handle more than one family at once)
    std::unordered_map<u32, std::vector<QueueFamily>> queue_families{};
    queue_families[graphics_family_index].emplace_back(QueueFamily::Graphics);
    queue_families[transfer_family_index].emplace_back(QueueFamily::Transfer);
    queue_families[compute_family_index].emplace_back(QueueFamily::Compute);

    // Create VkCommandPools for each unique family
    for(const auto& [index, families] : queue_families) {
        VkCommandPool command_pool{};
        VkCommandPoolCreateInfo cmd_pool_create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = index,
        };

        DEBUG_ASSERT(vkCreateCommandPool(VK_DEVICE, &cmd_pool_create_info, nullptr, &command_pool) == VK_SUCCESS)

        // Duplicate the pool for indices that handle more than one family at once
        for(const auto& family : families) {
            m_command_pools[family] = command_pool;
        }
    }

    VkQueryPool query_pool{};
    VkQueryPoolCreateInfo query_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO
    };

    query_pool_create_info.queryType = static_cast<VkQueryType>(QueryType::Timestamp);
    query_pool_create_info.queryCount = 128u;
    DEBUG_ASSERT(vkCreateQueryPool(VK_DEVICE, &query_pool_create_info, nullptr, &query_pool) == VK_SUCCESS)
    m_query_pools[QueryType::Timestamp] = query_pool;
    m_query_id_allocators[QueryType::Timestamp] = HandleAllocator<u32>{};

    query_pool_create_info.queryType = static_cast<VkQueryType>(QueryType::Occlusion);
    query_pool_create_info.queryCount = 16u;
    DEBUG_ASSERT(vkCreateQueryPool(VK_DEVICE, &query_pool_create_info, nullptr, &query_pool) == VK_SUCCESS)
    m_query_pools[QueryType::Occlusion] = query_pool;
    m_query_id_allocators[QueryType::Occlusion] = HandleAllocator<u32>{};

    query_pool_create_info.queryType = static_cast<VkQueryType>(QueryType::PipelineStatistics);
    query_pool_create_info.queryCount = 16u;
    query_pool_create_info.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

    DEBUG_ASSERT(vkCreateQueryPool(VK_DEVICE, &query_pool_create_info, nullptr, &query_pool) == VK_SUCCESS)
    m_query_pools[QueryType::PipelineStatistics] = query_pool;
    m_query_id_allocators[QueryType::PipelineStatistics] = HandleAllocator<u32>{};
}
ResourceManager::~ResourceManager() {
    for(const auto &handle : m_query_allocator.get_valid_handles()) {
        destroy(handle);
    }

    for(const auto &[query_type, pool] : m_query_pools) {
        vkDestroyQueryPool(VK_DEVICE, pool, nullptr);
    }

    std::unordered_set<VkCommandPool> unique_command_pools{};
    for(const auto &[family, pool] : m_command_pools) {
        unique_command_pools.insert(pool);
    }
    for(const auto &pool : unique_command_pools) {
        vkDestroyCommandPool(VK_DEVICE, pool, nullptr);
    }

    for(const auto &handle : m_fence_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }
    for(const auto &handle : m_semaphore_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }

    for(const auto &handle: m_graphics_pipeline_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }
    for(const auto &handle: m_compute_pipeline_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }
    for(const auto &handle: m_render_target_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }

    for(const auto &handle : m_image_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }
    for(const auto &handle : m_buffer_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }
    for(const auto &handle : m_descriptor_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }
    for(const auto &handle : m_sampler_allocator.get_valid_handles_copy()) {
        destroy(handle);
    }

    vkDestroyDescriptorPool(VK_DEVICE, m_descriptor_pool, nullptr);
}

Handle<Image> ResourceManager::create_image(const ImageCreateInfo &info) {
    VkExtent3D image_extent{
        .width = std::max(1U, info.extent.width),
        .height = std::max(1U, info.extent.height),
        .depth = std::max(1U, info.extent.depth)
    };

    u32 dimension_count =
        static_cast<u32>((info.extent.width > 0U)) +
        static_cast<u32>((info.extent.height > 0U)) +
        static_cast<u32>((info.extent.depth > 0U));

    DEBUG_ASSERT(dimension_count > 0U)

    Image image{
        .image_type = static_cast<VkImageType>(dimension_count - 1U),
        .format = info.format,
        .extent = image_extent,
        .usage_flags = info.usage_flags,
        .aspect_flags = info.aspect_flags,
        .create_flags = info.create_flags,
        .mip_level_count = info.mip_level_count,
        .array_layer_count = info.array_layer_count,
    };

    if(info.create_flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
        image.image_type = VK_IMAGE_TYPE_3D;
    }

    if (info.create_flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
        if (info.array_layer_count % 6 != 0) {
            DEBUG_PANIC("info.array_layer_count for cube textures must be a multiple of 6! Meanwhile info.array_layer_count = " << info.array_layer_count)
        }

        if (info.array_layer_count > 6U) {
            image.view_type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        } else {
            image.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
        }
    } else {
        if (info.array_layer_count > 1U) {
            image.view_type = static_cast<VkImageViewType>(dimension_count - 1U + 4U); // Array types values are always greater by 4 (except for VK_IMAGE_VIEW_TYPE_CUBE)
        } else {
            image.view_type = static_cast<VkImageViewType>(dimension_count - 1U);
        }
    }

    VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = image.create_flags,
        .imageType = image.image_type,
        .format = image.format,
        .extent = image_extent,
        .mipLevels = image.mip_level_count,
        .arrayLayers = image.array_layer_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = image.usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_create_info {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    DEBUG_ASSERT(vmaCreateImage(VK_ALLOCATOR, &image_create_info, &allocation_create_info, &image.image, &image.allocation, nullptr) == VK_SUCCESS)

    VkImageViewCreateInfo view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.image,
        .viewType = image.view_type,
        .format = image.format,

        .subresourceRange {
            .aspectMask = image.aspect_flags,
            .levelCount = image.mip_level_count,
            .layerCount = image.array_layer_count
        }
    };

    DEBUG_ASSERT(vkCreateImageView(VK_DEVICE, &view_create_info, nullptr, &image.view) == VK_SUCCESS)

    if(info.create_per_mip_views) {
        image.per_mip_views.resize(static_cast<usize>(info.mip_level_count));

        for(u32 i{}; i < info.mip_level_count; ++i) {
            view_create_info.subresourceRange.levelCount = 1U;
            view_create_info.subresourceRange.baseMipLevel = i;

            DEBUG_ASSERT(vkCreateImageView(VK_DEVICE, &view_create_info, nullptr, &image.per_mip_views[i]) == VK_SUCCESS)
        }
    }

    return m_image_allocator.alloc(image);
}
Handle<Image> ResourceManager::create_image_borrowed(VkImage borrowed_image, VkImageView borrowed_view, const ImageCreateInfo &info) {
    VkExtent3D image_extent{
        .width = std::max(1U, info.extent.width),
        .height = std::max(1U, info.extent.height),
        .depth = std::max(1U, info.extent.depth)
    };

    u32 dimension_count =
        static_cast<u32>((image_extent.width > 1U)) +
        static_cast<u32>((image_extent.height > 1U)) +
        static_cast<u32>((image_extent.depth > 1U));

    DEBUG_ASSERT(dimension_count > 0U)

    Image image{
        .image = borrowed_image,
        .image_type = static_cast<VkImageType>(dimension_count - 1U),
        .view = borrowed_view,
        .format = info.format,
        .extent = image_extent,
        .usage_flags = info.usage_flags,
        .aspect_flags = info.aspect_flags,
        .create_flags = info.create_flags,
        .mip_level_count = info.mip_level_count,
        .array_layer_count = info.array_layer_count
    };

    if(info.create_flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
        image.image_type = VK_IMAGE_TYPE_3D;
    }

    if (info.create_flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
        if (info.array_layer_count % 6 != 0) {
            DEBUG_PANIC("info.array_layer_count for cube textures must be a multiple of 6! Meanwhile info.array_layer_count = " << info.array_layer_count)
        }

        if (info.array_layer_count > 6U) {
            image.view_type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        } else {
            image.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
        }
    } else {
        if (info.array_layer_count > 1U) {
            image.view_type = static_cast<VkImageViewType>(dimension_count - 1U + 4U); // Array types values are always greater by 4 (except for VK_IMAGE_VIEW_TYPE_CUBE)
        } else {
            image.view_type = static_cast<VkImageViewType>(dimension_count - 1U);
        }
    }

    return m_image_allocator.alloc(image);
}
Handle<Buffer> ResourceManager::create_buffer(const BufferCreateInfo &info) {
    Buffer buffer{
        .size = info.size,
        .usage_flags = info.buffer_usage_flags,
    };

    VkBufferCreateInfo buffer_create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer.size,
        .usage = buffer.usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocation_create_info{
        .usage = info.memory_usage_flags
    };

    DEBUG_ASSERT(vmaCreateBuffer(VK_ALLOCATOR, &buffer_create_info, &allocation_create_info, &buffer.buffer, &buffer.allocation, nullptr) == VK_SUCCESS)

    return m_buffer_allocator.alloc(buffer);
}
Handle<Descriptor> ResourceManager::create_descriptor(const DescriptorCreateInfo &info) {
    Descriptor descriptor{
        .create_info = info
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings{};
    for(const auto& binding : info.bindings) {
        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = static_cast<u32>(bindings.size()),
            .descriptorType = binding.descriptor_type,
            .descriptorCount = binding.count,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
        });
    }

    std::vector<VkDescriptorBindingFlags> binding_flags(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
    // If needed for variable descriptor count
    //for (const auto& binding : bindings) {
    //    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
    //        binding_flags[binding.binding] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    //    }
    //}

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<u32>(binding_flags.size()),
        .pBindingFlags = binding_flags.data()
    };

    VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &extended_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data()
    };

    DEBUG_ASSERT(vkCreateDescriptorSetLayout(VK_DEVICE, &descriptor_layout_create_info, nullptr, &descriptor.layout) == VK_SUCCESS)

    VkDescriptorSetAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = 1U,
        .pSetLayouts = &descriptor.layout
    };

    DEBUG_ASSERT(vkAllocateDescriptorSets(VK_DEVICE, &allocate_info, &descriptor.set) == VK_SUCCESS)

    return m_descriptor_allocator.alloc(descriptor);
}
Handle<Sampler> ResourceManager::create_sampler(const SamplerCreateInfo &info) {
    Sampler sampler{};

    VkSamplerReductionModeCreateInfo sampler_reduction{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
        .reductionMode = info.reduction_mode
    };

    VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = (info.reduction_mode != VK_SAMPLER_REDUCTION_MODE_MAX_ENUM ? &sampler_reduction : nullptr),
        .magFilter = info.filter,
        .minFilter = info.filter,
        .mipmapMode = info.mipmap_mode,
        .addressModeU = info.address_mode,
        .addressModeV = info.address_mode,
        .addressModeW = info.address_mode,
        .mipLodBias = info.mipmap_bias,
        .anisotropyEnable = (info.anisotropy > 0.0f),
        .maxAnisotropy = std::min(info.anisotropy, 16.0f),
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = info.max_mipmap,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    };

    DEBUG_ASSERT(vkCreateSampler(VK_DEVICE, &create_info, nullptr, &sampler.sampler) == VK_SUCCESS)

    return m_sampler_allocator.alloc(sampler);
}
Handle<Query> ResourceManager::create_query(QueryType q_type) {
    Query query{
        .query_type = q_type
    };

    query.local_id = m_query_id_allocators[q_type].alloc(0u);

    return m_query_allocator.alloc(query);
}
Handle<CommandList> ResourceManager::create_command_list(QueueFamily family) {
    CommandList cmd{
        .family = family
    };

    VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pools.at(family),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1U,
    };

    DEBUG_ASSERT(vkAllocateCommandBuffers(VK_DEVICE, &allocate_info, &cmd.command_buffer) == VK_SUCCESS)

    return m_command_list_allocator.alloc(cmd);
}
Handle<Fence> ResourceManager::create_fence(bool signaled) {
    Fence fence{};

    VkFenceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = (signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0U),
    };

    DEBUG_ASSERT(vkCreateFence(VK_DEVICE, &info, nullptr, &fence.fence) == VK_SUCCESS)

    return m_fence_allocator.alloc(fence);
}
Handle<Semaphore> ResourceManager::create_semaphore() {
    Semaphore semaphore{};

    VkSemaphoreCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    DEBUG_ASSERT(vkCreateSemaphore(VK_DEVICE, &info, nullptr, &semaphore.semaphore) == VK_SUCCESS)

    return m_semaphore_allocator.alloc(semaphore);
}
Handle<GraphicsPipeline> ResourceManager::create_graphics_pipeline(const GraphicsPipelineCreateInfo &info) {
    GraphicsPipeline pipeline{
        .create_info = info
    };

    u32 uses_color_attachment = static_cast<u32>(info.color_target.format != VK_FORMAT_UNDEFINED);
    u32 uses_depth_attachment = static_cast<u32>(info.depth_target.format != VK_FORMAT_UNDEFINED);

    DEBUG_ASSERT(uses_color_attachment + uses_depth_attachment > 0)

    VkAttachmentDescription all_descriptions[2];
    VkAttachmentReference color_reference, depth_reference;

    VkSubpassDescription subpass_description{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    };

    VkSubpassDependency subpass_dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0U,
    };

    if (uses_color_attachment) {
        color_reference = VkAttachmentReference{
            .attachment = 0U,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        DEBUG_ASSERT(info.color_target.layout != VK_IMAGE_LAYOUT_UNDEFINED)

        all_descriptions[color_reference.attachment] = VkAttachmentDescription{
            .format = info.color_target.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = info.color_target.load_op,
            .storeOp = info.color_target.store_op,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = info.color_target.layout,
            .finalLayout = info.color_target.layout,
        };

        subpass_description.colorAttachmentCount = 1U;
        subpass_description.pColorAttachments = &color_reference;

        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if(info.color_target.load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
            subpass_dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        } else if(info.color_target.load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            subpass_dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        if(info.color_target.store_op == VK_ATTACHMENT_STORE_OP_STORE) {
            subpass_dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
    }
    if (uses_depth_attachment) {
        depth_reference = VkAttachmentReference{
            .attachment = uses_color_attachment,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        DEBUG_ASSERT(info.depth_target.layout != VK_IMAGE_LAYOUT_UNDEFINED)

        all_descriptions[depth_reference.attachment] = VkAttachmentDescription{
            .format = info.depth_target.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = info.depth_target.load_op,
            .storeOp = info.depth_target.store_op,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = info.depth_target.layout,
            .finalLayout = info.depth_target.layout,
        };

        subpass_description.pDepthStencilAttachment = &depth_reference;

        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        if(info.depth_target.load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
            subpass_dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        } else if(info.depth_target.load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
            subpass_dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if(info.depth_target.store_op == VK_ATTACHMENT_STORE_OP_STORE) {
            subpass_dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
    }

    VkRenderPassCreateInfo render_pass_create_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = uses_color_attachment + uses_depth_attachment,
        .pAttachments = all_descriptions,
        .subpassCount = 1U,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1U,
        .pDependencies = &subpass_dependency,
    };

    DEBUG_ASSERT(vkCreateRenderPass(VK_DEVICE, &render_pass_create_info, nullptr, &pipeline.render_pass) == VK_SUCCESS)

    VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = info.primitive_topology,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = 1.0f,
        .height = 1.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor{
        .offset = { 0U, 0U },
        .extent = { 1U, 1U}
    };

    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1U,
        .pViewports = &viewport,
        .scissorCount = 1U,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = info.polygon_mode,
        .cullMode = info.cull_mode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable = info.enable_blending,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo color_blending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1U,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = info.enable_depth_test,
        .depthWriteEnable = info.enable_depth_write,
        .depthCompareOp = info.depth_compare_op,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    std::vector<VkDynamicState> dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()
    };

    if(info.push_constants_size > 128U) {
        DEBUG_PANIC("Push constants size exceeded 128 bytes!")
    }
    if(info.push_constants_size % 4 != 0) {
        DEBUG_PANIC("Push constants size must be a multiple of 4!")
    }

    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = static_cast<u32>(info.push_constants_size)
    };

    std::vector<VkDescriptorSetLayout> layouts{};
    for(const auto &descriptor : info.descriptors) {
        layouts.push_back(get_data(descriptor).layout);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<u32>(info.push_constants_size != 0U),
        .pPushConstantRanges = &push_constant_range
    };

    DEBUG_ASSERT(vkCreatePipelineLayout(VK_DEVICE, &pipeline_layout_info, nullptr, &pipeline.layout) == VK_SUCCESS)

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
    std::vector<VkShaderModule> shader_modules{};
    std::vector<VkSpecializationInfo> shader_spec_infos{};
    std::vector<std::vector<VkSpecializationMapEntry>> shader_spec_maps{};

    // I know it looks ugly but that's the quick way to take care of dangling pointer problems in the shader structures...
    if(!info.vertex_shader_path.empty()) {
        shader_modules.push_back(create_shader_module(info.vertex_shader_path));
        shader_spec_maps.push_back(std::vector<VkSpecializationMapEntry>(info.vertex_constant_values.size()));

        auto &shader_spec_map = shader_spec_maps.back();

        for(u32 i{}; i < static_cast<u32>(shader_spec_map.size()); ++i) {
            shader_spec_map[i] = VkSpecializationMapEntry{
                .constantID = i,
                .offset = i * static_cast<u32>(sizeof(u32)),
                .size = sizeof(u32)
            };
        }

        shader_spec_infos.push_back(VkSpecializationInfo{
            .mapEntryCount = static_cast<u32>(shader_spec_map.size()),
            .pMapEntries = shader_spec_map.data(),
            .dataSize = static_cast<u32>(info.vertex_constant_values.size()) * sizeof(u32),
            .pData = info.vertex_constant_values.data(),
        });

        shader_stages.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shader_modules.back(),
            .pName = "main",
            .pSpecializationInfo = info.vertex_constant_values.empty() ? nullptr : &shader_spec_infos.back(),
        });
    }

    if(!info.fragment_shader_path.empty()) {
        shader_modules.push_back(create_shader_module(info.fragment_shader_path));
        shader_spec_maps.push_back(std::vector<VkSpecializationMapEntry>(info.fragment_constant_values.size()));

        auto& shader_spec_map = shader_spec_maps.back();

        for(u32 i{}; i < static_cast<u32>(shader_spec_map.size()); ++i) {
            shader_spec_map[i] = VkSpecializationMapEntry{
                .constantID = i,
                .offset = i * static_cast<u32>(sizeof(u32)),
                .size = sizeof(u32)
            };
        }

        shader_spec_infos.push_back(VkSpecializationInfo{
            .mapEntryCount = static_cast<u32>(shader_spec_map.size()),
            .pMapEntries = shader_spec_map.data(),
            .dataSize = static_cast<u32>(info.fragment_constant_values.size()) * sizeof(u32),
            .pData = info.fragment_constant_values.data(),
        });

        shader_stages.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shader_modules.back(),
            .pName = "main",
            .pSpecializationInfo = info.fragment_constant_values.empty() ? nullptr : &shader_spec_infos.back(),
        });
    }

    DEBUG_ASSERT(!shader_stages.empty())

    VkGraphicsPipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

        .stageCount = static_cast<u32>(shader_stages.size()),
        .pStages = shader_stages.data(),

        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,

        .layout = pipeline.layout,
        .renderPass = pipeline.render_pass,

        .basePipelineIndex = -1
    };

    DEBUG_ASSERT(vkCreateGraphicsPipelines(VK_DEVICE, nullptr, 1U, &pipeline_create_info, nullptr, &pipeline.pipeline) == VK_SUCCESS)

    for (const auto &module : shader_modules) {
        vkDestroyShaderModule(VK_DEVICE, module, nullptr);
    }

    return m_graphics_pipeline_allocator.alloc(pipeline);
}
Handle<ComputePipeline> ResourceManager::create_compute_pipeline(const ComputePipelineCreateInfo &info) {
    ComputePipeline pipeline{
        .create_info = info
    };

    if(info.push_constants_size > 128U) {
        DEBUG_PANIC("Push constants size exceeded 128 bytes!")
    }
    if(info.push_constants_size % 4 != 0) {
        DEBUG_PANIC("Push constants size must be a multiple of 4!")
    }

    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = static_cast<u32>(info.push_constants_size)
    };

    std::vector<VkDescriptorSetLayout> layouts{};
    for(const auto &descriptor : info.descriptors) {
        layouts.push_back(get_data(descriptor).layout);
    }

    VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<u32>(info.push_constants_size != 0U),
        .pPushConstantRanges = &push_constant_range
    };

    DEBUG_ASSERT(vkCreatePipelineLayout(VK_DEVICE, &layout_create_info, nullptr, &pipeline.layout) == VK_SUCCESS)

    VkShaderModule shader_module = create_shader_module(info.shader_path);

    std::vector<VkSpecializationMapEntry> spec_map_entries(info.shader_constant_values.size());
    for(u32 i{}; i < static_cast<u32>(spec_map_entries.size()); ++i) {
        spec_map_entries[i] = VkSpecializationMapEntry{
            .constantID = i,
            .offset = i * static_cast<u32>(sizeof(u32)),
            .size = sizeof(u32)
        };
    }

    VkSpecializationInfo spec_info{
        .mapEntryCount = static_cast<u32>(spec_map_entries.size()),
        .pMapEntries = spec_map_entries.data(),
        .dataSize = static_cast<u32>(info.shader_constant_values.size()) * sizeof(u32),
        .pData = info.shader_constant_values.data(),
    };

    VkComputePipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = "main",
            .pSpecializationInfo = &spec_info//((spec_info.mapEntryCount > 0U) ? &spec_info : nullptr),
        },
        .layout = pipeline.layout
    };

    DEBUG_ASSERT(vkCreateComputePipelines(VK_DEVICE, nullptr, 1U, &pipeline_create_info, nullptr, &pipeline.pipeline) == VK_SUCCESS)

    vkDestroyShaderModule(VK_DEVICE, shader_module, nullptr);

    return m_compute_pipeline_allocator.alloc(pipeline);
}
Handle<RenderTarget> ResourceManager::create_render_target(Handle<GraphicsPipeline> src_pipeline, const RenderTargetCreateInfo &info) {
    RenderTarget rt{
        .color_handle = info.color_target_handle,
        .depth_handle = info.depth_target_handle,
    };

    if(info.color_target_handle != INVALID_HANDLE) {
        const auto &image = get_data(info.color_target_handle);

        if(image.per_mip_views.empty()) {
            rt.color_view = image.view;
        } else {
            if(info.color_target_mip > static_cast<u32>(image.per_mip_views.size())) {
                DEBUG_PANIC("RenderTarget color target mip out of bounds! color_target_mip = " << info.color_target_mip)
            }

            rt.color_view = image.per_mip_views.at(info.color_target_mip);
        }

        VkExtent3D image_extent = get_data(info.color_target_handle).extent;
        rt.extent = VkExtent2D{
            std::max(image_extent.width / (1U << info.color_target_mip), 1U),
            std::max(image_extent.height / (1U << info.color_target_mip), 1U)
        };
    }

    if(info.depth_target_handle != INVALID_HANDLE) {
        const auto &image = get_data(info.depth_target_handle);

        if(image.per_mip_views.empty()) {
            rt.depth_view = image.view;
        } else {
            if(info.depth_target_mip > static_cast<u32>(image.per_mip_views.size())) {
                DEBUG_PANIC("RenderTarget depth target mip out of bounds! depth_target_mip = " << info.depth_target_mip)
            }

            rt.depth_view = image.per_mip_views.at(info.depth_target_mip);
        }

        VkExtent3D image_extent = get_data(info.depth_target_handle).extent;
        if(info.color_target_handle != INVALID_HANDLE) {
            if(rt.extent.width != image_extent.width || rt.extent.height != image_extent.height) {
                DEBUG_PANIC("Failed to create a render target! - If both color and depth targets are used, their extents must be identical")
            }
        }

        rt.extent = VkExtent2D{
            std::max(image_extent.width / (1U << info.depth_target_mip), 1U),
            std::max(image_extent.height / (1U << info.depth_target_mip), 1U)
        };
    }

    if(rt.color_view == nullptr && rt.depth_view == nullptr) {
        DEBUG_PANIC("Failed to create a render target! - Both color and depth targets were not used")
    }

    const GraphicsPipeline &pipeline = m_graphics_pipeline_allocator.get_element(src_pipeline);

    u32 uses_color_attachment = static_cast<u32>(rt.color_view != nullptr);
    u32 uses_depth_attachment = static_cast<u32>(rt.depth_view != nullptr);

    VkImageView image_views[2];
    if(uses_color_attachment) {
        image_views[0] = rt.color_view;
    }
    if(uses_depth_attachment) {
        image_views[uses_color_attachment] = rt.depth_view;
    }

    VkFramebufferCreateInfo framebuffer_create_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = pipeline.render_pass,
        .attachmentCount = uses_color_attachment + uses_depth_attachment,
        .pAttachments = image_views,
        .width = rt.extent.width,
        .height = rt.extent.height,
        .layers = 1U,
    };

    DEBUG_ASSERT(vkCreateFramebuffer(VK_DEVICE, &framebuffer_create_info, nullptr, &rt.framebuffer) == VK_SUCCESS)

    return m_render_target_allocator.alloc(rt);
}


void *ResourceManager::map_buffer(Handle<Buffer> buffer_handle) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot map buffer! - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    void *data;
    DEBUG_ASSERT(vmaMapMemory(VK_ALLOCATOR, m_buffer_allocator.get_element(buffer_handle).allocation, &data) == VK_SUCCESS)

    return data;
}
void ResourceManager::unmap_buffer(Handle<Buffer> buffer_handle) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot unmap buffer! - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    vmaUnmapMemory(VK_ALLOCATOR, m_buffer_allocator.get_element(buffer_handle).allocation);
}
void ResourceManager::flush_mapped_buffer(Handle<Buffer> buffer_handle, VkDeviceSize size, VkDeviceSize offset) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot flush mapped buffer! - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    const Buffer &buffer = m_buffer_allocator.get_element(buffer_handle);

    VkDeviceSize flush_size;
    if(size != 0) {
        flush_size = size;
    } else {
        flush_size = buffer.size;
    }

    if(flush_size + offset > buffer.size) {
        DEBUG_PANIC("Cannot flush mapped buffer! - Flush range out of bounds: flush_size = " << flush_size << ", offset = " << offset)
    }

    vmaFlushAllocation(VK_ALLOCATOR, buffer.allocation, offset, size);
}

void ResourceManager::memcpy_to_buffer_once(Handle<Buffer> buffer_handle, const void *src_data, usize size, usize dst_offset, usize src_offset) {
    void *mapped = reinterpret_cast<void*>(reinterpret_cast<usize>(map_buffer(buffer_handle)) + dst_offset);
    void *src = reinterpret_cast<void*>(reinterpret_cast<usize>(src_data) + src_offset);

    std::memcpy(mapped, src, size);

    unmap_buffer(buffer_handle);
}
void ResourceManager::memcpy_to_buffer(void *dst_mapped_buffer, const void *src_data, usize size, usize dst_offset, usize src_offset) {
    void *mapped = reinterpret_cast<void*>(reinterpret_cast<usize>(dst_mapped_buffer) + dst_offset);
    void *src = reinterpret_cast<void*>(reinterpret_cast<usize>(src_data) + src_offset);
    std::memcpy(mapped, src, size);
}

/*
void ResourceManager::update_image_layout(Handle<Image> image_handle, VkImageLayout new_layout) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_image_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot update image layout - Image with a handle id: = " << image_handle << ", does not exist!")
    }
#endif

    m_image_allocator.get_element_mutable(image_handle).current_layout = new_layout;
}
*/
void ResourceManager::update_descriptor(Handle<Descriptor> descriptor_handle, const DescriptorUpdateInfo &info) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_descriptor_allocator.is_handle_valid(descriptor_handle)) {
        DEBUG_PANIC("Cannot update descriptor - Descriptor with a handle id: = " << descriptor_handle << ", does not exist!")
    }
#endif

    const Descriptor &descriptor = m_descriptor_allocator.get_element(descriptor_handle);

    std::vector<VkWriteDescriptorSet> descriptor_writes{};
    std::vector<u32> descriptor_info_indices{};

    std::vector<VkDescriptorImageInfo> descriptor_images{};
    std::vector<VkDescriptorBufferInfo> descriptor_buffers{};

    for(const auto& binding : info.bindings) {
#if DEBUG_MODE
        if(binding.binding_index >= static_cast<u32>(descriptor.create_info.bindings.size())) {
            DEBUG_PANIC("Binding write failed! - Binding index out of range, binding_index = " << binding.binding_index << ", bindings.size() = " << static_cast<u32>(descriptor.create_info.bindings.size()))
        }
        if(binding.array_index >= descriptor.create_info.bindings[binding.binding_index].count) {
            DEBUG_PANIC("Binding write failed! - Binding array index out of range, array_index = " << binding.array_index << ", descriptor.create_info.bindings[binding.binding_index].count = " << descriptor.create_info.bindings[binding.binding_index].count)
        }
#endif

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor.set,
            .dstBinding = binding.binding_index,
            .dstArrayElement = binding.array_index,
            .descriptorCount = 1U,
            .descriptorType = descriptor.create_info.bindings[binding.binding_index].descriptor_type
        };

        if(binding.buffer_info.buffer_handle != INVALID_HANDLE && binding.image_info.image_handle == INVALID_HANDLE) {
            descriptor_info_indices.push_back(static_cast<u32>(descriptor_buffers.size()));
            descriptor_buffers.push_back(VkDescriptorBufferInfo{
                .buffer = get_data(binding.buffer_info.buffer_handle).buffer,
                .range = get_data(binding.buffer_info.buffer_handle).size
            });
        } else if(binding.buffer_info.buffer_handle == INVALID_HANDLE && binding.image_info.image_handle != INVALID_HANDLE) {
            descriptor_info_indices.push_back(static_cast<u32>(descriptor_images.size()));

            const auto& image = get_data(binding.image_info.image_handle);

            VkImageView image_view{};
            if(image.per_mip_views.empty() || binding.image_info.image_mip == INVALID_HANDLE) {
                image_view = image.view;
            } else {
                if(binding.image_info.image_mip > static_cast<u32>(image.per_mip_views.size())) {
                    DEBUG_PANIC("Descriptor binding image target mip out of bounds! image_mip = " << binding.image_info.image_mip)
                }

                image_view = image.per_mip_views.at(binding.image_info.image_mip);
            }

            VkImageLayout image_layout{};
            VkDescriptorType descriptor_type = descriptor.create_info.bindings.at(binding.binding_index).descriptor_type;
            if(descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                image_layout = VK_IMAGE_LAYOUT_GENERAL;
            } else if(descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else if(descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            } else {
                DEBUG_PANIC("Binding write failed! - descriptor_type is invalid, descriptor_type = " << descriptor_type << ", binding_index = " << binding.binding_index)
            }

            descriptor_images.push_back(VkDescriptorImageInfo {
                .sampler = get_data(binding.image_info.image_sampler).sampler,
                .imageView = image_view,
                .imageLayout = image_layout
            });
        } else {
            DEBUG_PANIC("Binding write failed! - Both or none binding handles were valid, binding_index = " << binding.binding_index)
        }

        descriptor_writes.push_back(write);
    }

    for(usize i{}; i < descriptor_writes.size(); ++i) {
        auto &write = descriptor_writes[i];

        if(write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            write.pImageInfo = &descriptor_images[descriptor_info_indices[i]];
        } else if(write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            write.pBufferInfo = &descriptor_buffers[descriptor_info_indices[i]];
        } else {
            DEBUG_PANIC("Binding write failed! - Invalid descriptor type, dstBinding = " << write.dstBinding << ", dstArrayElement = "<< write.dstArrayElement <<", descriptorType = " << write.descriptorType)
        }
    }

    vkUpdateDescriptorSets(VK_DEVICE, static_cast<u32>(descriptor_writes.size()), descriptor_writes.data(), 0U, nullptr);
}

void ResourceManager::destroy(Handle<Image> image_handle) {
    if (!m_image_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot delete image - Image with a handle id: = " << image_handle << ", does not exist!")
    }

    const Image &image = m_image_allocator.get_element(image_handle);

    // If it is a borrowed image
    if(image.allocation == nullptr) {
        return;
    }

    for(const auto& view : image.per_mip_views) {
        vkDestroyImageView(VK_DEVICE, view, nullptr);
    }

    vkDestroyImageView(VK_DEVICE, image.view, nullptr);
    vmaDestroyImage(VK_ALLOCATOR, image.image, image.allocation);

    m_image_allocator.free(image_handle);
}
void ResourceManager::destroy(Handle<Buffer> buffer_handle) {
    if (!m_buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot delete buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }

    const Buffer &buffer = m_buffer_allocator.get_element(buffer_handle);

    vmaDestroyBuffer(VK_ALLOCATOR, buffer.buffer, buffer.allocation);

    m_buffer_allocator.free(buffer_handle);
}
void ResourceManager::destroy(Handle<Descriptor> descriptor_handle) {
    if (!m_descriptor_allocator.is_handle_valid(descriptor_handle)) {
        DEBUG_PANIC("Cannot delete descriptor - Descriptor with a handle id: = " << descriptor_handle << ", does not exist!")
    }

    const Descriptor &descriptor = m_descriptor_allocator.get_element(descriptor_handle);

    vkFreeDescriptorSets(VK_DEVICE, m_descriptor_pool, 1U, &descriptor.set);
    vkDestroyDescriptorSetLayout(VK_DEVICE, descriptor.layout, nullptr);

    m_descriptor_allocator.free(descriptor_handle);
}
void ResourceManager::destroy(Handle<Sampler> sampler_handle) {
    if (!m_sampler_allocator.is_handle_valid(sampler_handle)) {
        DEBUG_PANIC("Cannot delete sampler - Sampler with a handle id: = " << sampler_handle << ", does not exist!")
    }

    const Sampler &sampler = m_sampler_allocator.get_element(sampler_handle);

    vkDestroySampler(VK_DEVICE, sampler.sampler, nullptr);

    m_sampler_allocator.free(sampler_handle);
}
void ResourceManager::destroy(Handle<Query> query_handle) {
    if (!m_query_allocator.is_handle_valid(query_handle)) {
        DEBUG_PANIC("Cannot delete query - Query with a handle id: = " << query_handle << ", does not exist!")
    }

    const Query &query = m_query_allocator.get_element(query_handle);

    m_query_id_allocators[query.query_type].free(query.local_id);
    m_query_allocator.free(query_handle);
}
void ResourceManager::destroy(Handle<CommandList> command_list_handle) {
    if (!m_command_list_allocator.is_handle_valid(command_list_handle)) {
        DEBUG_PANIC("Cannot delete command list - Command list with a handle id: = " << command_list_handle << ", does not exist!")
    }

    const CommandList &cmd = m_command_list_allocator.get_element(command_list_handle);

    vkFreeCommandBuffers(VK_DEVICE, m_command_pools.at(cmd.family), 1U, &cmd.command_buffer);

    m_command_list_allocator.free(command_list_handle);
}
void ResourceManager::destroy(Handle<Fence> fence_handle) {
    if (!m_fence_allocator.is_handle_valid(fence_handle)) {
        DEBUG_PANIC("Cannot delete fence - Fence with a handle id: = " << fence_handle << ", does not exist!")
    }

    const Fence &fence = m_fence_allocator.get_element(fence_handle);

    vkDestroyFence(VK_DEVICE, fence.fence, nullptr);

    m_fence_allocator.free(fence_handle);
}
void ResourceManager::destroy(Handle<Semaphore> semaphore_handle) {
    if (!m_semaphore_allocator.is_handle_valid(semaphore_handle)) {
        DEBUG_PANIC("Cannot delete semaphore - Semaphore with a handle id: = " << semaphore_handle << ", does not exist!")
    }

    const Semaphore &semaphore = m_semaphore_allocator.get_element(semaphore_handle);

    vkDestroySemaphore(VK_DEVICE, semaphore.semaphore, nullptr);

    m_semaphore_allocator.free(semaphore_handle);
}
void ResourceManager::destroy(Handle<GraphicsPipeline> pipeline_handle) {
    if (!m_graphics_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot delete graphics pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }

    const GraphicsPipeline &pipeline = m_graphics_pipeline_allocator.get_element(pipeline_handle);
    vkDestroyPipelineLayout(VK_DEVICE, pipeline.layout, nullptr);
    vkDestroyRenderPass(VK_DEVICE, pipeline.render_pass, nullptr);
    vkDestroyPipeline(VK_DEVICE, pipeline.pipeline, nullptr);

    m_graphics_pipeline_allocator.free(pipeline_handle);
}
void ResourceManager::destroy(Handle<ComputePipeline> pipeline_handle) {
    if (!m_compute_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot delete compute pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }

    const ComputePipeline &pipeline = m_compute_pipeline_allocator.get_element(pipeline_handle);
    vkDestroyPipelineLayout(VK_DEVICE, pipeline.layout, nullptr);
    vkDestroyPipeline(VK_DEVICE, pipeline.pipeline, nullptr);

    m_compute_pipeline_allocator.free(pipeline_handle);
}
void ResourceManager::destroy(Handle<RenderTarget> rt_handle) {
    if (!m_render_target_allocator.is_handle_valid(rt_handle)) {
        DEBUG_PANIC("Cannot delete render target - Render target with a handle id: = " << rt_handle << ", does not exist!")
    }

    const RenderTarget &render_target = m_render_target_allocator.get_element(rt_handle);
    vkDestroyFramebuffer(VK_DEVICE, render_target.framebuffer, nullptr);

    m_render_target_allocator.free(rt_handle);
}


const Image &ResourceManager::get_data(Handle<Image> image_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_image_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot get image data - Image with a handle id: = " << image_handle << ", does not exist!")
    }
#endif

    return m_image_allocator.get_element(image_handle);
}
const Buffer &ResourceManager::get_data(Handle<Buffer> buffer_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot get buffer data - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    return m_buffer_allocator.get_element(buffer_handle);
}
const Descriptor &ResourceManager::get_data(Handle<Descriptor> descriptor_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_descriptor_allocator.is_handle_valid(descriptor_handle)) {
        DEBUG_PANIC("Cannot get descriptor data - Descriptor with a handle id: = " << descriptor_handle << ", does not exist!")
    }
#endif

    return m_descriptor_allocator.get_element(descriptor_handle);
}
const Sampler &ResourceManager::get_data(Handle<Sampler> sampler_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_sampler_allocator.is_handle_valid(sampler_handle)) {
        DEBUG_PANIC("Cannot get sampler data - Sampler with a handle id: = " << sampler_handle << ", does not exist!")
    }
#endif

    return m_sampler_allocator.get_element(sampler_handle);
}
const Query &ResourceManager::get_data(Handle<Query> query_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_query_allocator.is_handle_valid(query_handle)) {
        DEBUG_PANIC("Cannot get query - Query with a handle id: = " << query_handle << ", does not exist!")
    }
#endif

    return m_query_allocator.get_element(query_handle);
}
const CommandList &ResourceManager::get_data(Handle<CommandList> command_list_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_command_list_allocator.is_handle_valid(command_list_handle)) {
        DEBUG_PANIC("Cannot get command list - Command list with a handle id: = " << command_list_handle << ", does not exist!")
    }
#endif

    return m_command_list_allocator.get_element(command_list_handle);
}
const Fence &ResourceManager::get_data(Handle<Fence> fence_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_fence_allocator.is_handle_valid(fence_handle)) {
        DEBUG_PANIC("Cannot get fence - Fence with a handle id: = " << fence_handle << ", does not exist!")
    }
#endif

    return m_fence_allocator.get_element(fence_handle);
}
const Semaphore &ResourceManager::get_data(Handle<Semaphore> semaphore_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_semaphore_allocator.is_handle_valid(semaphore_handle)) {
        DEBUG_PANIC("Cannot get semaphore - Semaphore with a handle id: = " << semaphore_handle << ", does not exist!")
    }
#endif

    return m_semaphore_allocator.get_element(semaphore_handle);
}
const GraphicsPipeline &ResourceManager::get_data(Handle<GraphicsPipeline> pipeline_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_graphics_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot get graphics pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }
#endif

    return m_graphics_pipeline_allocator.get_element(pipeline_handle);
}
const ComputePipeline &ResourceManager::get_data(Handle<ComputePipeline> pipeline_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_compute_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot get compute pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }
#endif

    return m_compute_pipeline_allocator.get_element(pipeline_handle);
}
const RenderTarget &ResourceManager::get_data(Handle<RenderTarget> rt_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_render_target_allocator.is_handle_valid(rt_handle)) {
        DEBUG_PANIC("Cannot get render target - Render target with a handle id: = " << rt_handle << ", does not exist!")
    }
#endif

    return m_render_target_allocator.get_element(rt_handle);
}

const VkQueryPool &ResourceManager::get_query_pool(QueryType query_type) const {
    return m_query_pools.at(query_type);
}
VkShaderModule ResourceManager::create_shader_module(const std::string &path) {
    VkShaderModule shader_module{};

    std::vector<u8> spirv_code = Utils::read_file_bytes(path);
    if(spirv_code.empty()) {
        DEBUG_PANIC("Failed to read shader spir-v code from \"" << path << "\"")
    }

    VkShaderModuleCreateInfo module_create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = static_cast<u32>(spirv_code.size()),
        .pCode = reinterpret_cast<const u32*>(spirv_code.data()),
    };

    DEBUG_ASSERT(vkCreateShaderModule(VK_DEVICE, &module_create_info, nullptr, &shader_module) == VK_SUCCESS)

    //data.spec_map_entries.resize(constant_count);
    //for(u32 i{}; i < constant_count; ++i) {
    //    data.spec_map_entries[i] = VkSpecializationMapEntry{
    //        .constantID = i,
    //        .offset = i * static_cast<u32>(sizeof(u32)),
    //        .size = sizeof(u32)
    //    };
    //}
//
    //data.spec_info = VkSpecializationInfo{
    //    .mapEntryCount = static_cast<u32>(data.spec_map_entries.size()),
    //    .pMapEntries = data.spec_map_entries.data(),
    //    .dataSize = static_cast<u32>(data.spec_map_entries.size()) * sizeof(u32),
    //    .pData = constant_data,
    //};
//
    //data.stage_info = VkPipelineShaderStageCreateInfo {
    //    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    //    .stage = stage,
    //    .module = data.module,
    //    .pName = "main",
    //    .pSpecializationInfo = (constant_count > 0U ? &data.spec_info : nullptr)
    //};

    return shader_module;
}