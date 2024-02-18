#include "pipeline_manager.hpp"

#include <common/utils.hpp>
#include <common/debug.hpp>

PipelineManager::PipelineManager(VkDevice device, const ResourceManager* resource_manager_ptr) : vk_device(device), resource_manager(resource_manager_ptr) {

}
PipelineManager::~PipelineManager() {
    for(const auto& handle: graphics_pipeline_allocator.get_valid_handles_copy()) {
        destroy_graphics_pipeline(handle);
    }
    for(const auto& handle: compute_pipeline_allocator.get_valid_handles_copy()) {
        destroy_compute_pipeline(handle);
    }
    for(const auto& handle: render_target_allocator.get_valid_handles_copy()) {
        destroy_render_target(handle);
    }
}

Handle<GraphicsPipeline> PipelineManager::create_graphics_pipeline(const GraphicsPipelineCreateInfo& info) {
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

    DEBUG_ASSERT(vkCreateRenderPass(vk_device, &render_pass_create_info, nullptr, &pipeline.render_pass) == VK_SUCCESS)

    VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    if(info.enable_vertex_input) {
        vertex_input_state.vertexBindingDescriptionCount = static_cast<u32>(info.vertex_binding_description.size());
        vertex_input_state.pVertexBindingDescriptions = info.vertex_binding_description.data();
        vertex_input_state.vertexAttributeDescriptionCount = static_cast<u32>(info.vertex_attribute_description.size());
        vertex_input_state.pVertexAttributeDescriptions = info.vertex_attribute_description.data();
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
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
    for(const auto& descriptor : info.descriptors) {
        layouts.push_back(resource_manager->get_descriptor_data(descriptor).layout);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<u32>(info.push_constants_size != 0U),
        .pPushConstantRanges = &push_constant_range
    };

    DEBUG_ASSERT(vkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &pipeline.layout) == VK_SUCCESS)

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
    std::vector<VkShaderModule> shader_modules{};
    std::vector<VkSpecializationInfo> shader_spec_infos{};
    std::vector<std::vector<VkSpecializationMapEntry>> shader_spec_maps{};

    // I know it looks ugly but that's the quick way to take care of dangling pointer problems in the shader structures...
    if(!info.vertex_shader_path.empty()) {
        shader_modules.push_back(create_shader_module(info.vertex_shader_path));
        shader_spec_maps.push_back(std::vector<VkSpecializationMapEntry>(info.vertex_constant_values.size()));

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
            .dataSize = static_cast<u32>(info.vertex_constant_values.size()) * sizeof(u32),
            .pData = info.vertex_constant_values.data(),
        });

        shader_stages.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shader_modules.back(),
            .pName = "main",
            .pSpecializationInfo = (info.vertex_constant_values.size() > 0 ? &shader_spec_infos.back() : nullptr),
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
            .pSpecializationInfo = (info.fragment_constant_values.size() > 0 ? &shader_spec_infos.back() : nullptr),
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

    DEBUG_ASSERT(vkCreateGraphicsPipelines(vk_device, nullptr, 1U, &pipeline_create_info, nullptr, &pipeline.pipeline) == VK_SUCCESS)

    for (const auto& module : shader_modules) {
        vkDestroyShaderModule(vk_device, module, nullptr);
    }

    return graphics_pipeline_allocator.alloc(pipeline);
}
Handle<ComputePipeline> PipelineManager::create_compute_pipeline(const ComputePipelineCreateInfo& info) {
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
    for(const auto& descriptor : info.descriptors) {
        layouts.push_back(resource_manager->get_descriptor_data(descriptor).layout);
    }

    VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<u32>(info.push_constants_size != 0U),
        .pPushConstantRanges = &push_constant_range
    };

    DEBUG_ASSERT(vkCreatePipelineLayout(vk_device, &layout_create_info, nullptr, &pipeline.layout) == VK_SUCCESS)

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

    DEBUG_ASSERT(vkCreateComputePipelines(vk_device, nullptr, 1U, &pipeline_create_info, nullptr, &pipeline.pipeline) == VK_SUCCESS)

    vkDestroyShaderModule(vk_device, shader_module, nullptr);

    return compute_pipeline_allocator.alloc(pipeline);
}
Handle<RenderTarget> PipelineManager::create_render_target(Handle<GraphicsPipeline> src_pipeline, const RenderTargetCreateInfo& info) {
    RenderTarget rt{
        .color_handle = info.color_target_handle,
        .depth_handle = info.depth_target_handle,
    };

    if(info.color_target_handle != INVALID_HANDLE) {
        const auto& image = resource_manager->get_image_data(info.color_target_handle);

        if(image.per_mip_views.empty()) {
            rt.color_view = image.view;
        } else {
            if(info.color_target_mip > static_cast<u32>(image.per_mip_views.size())) {
                DEBUG_PANIC("RenderTarget color target mip out of bounds! color_target_mip = " << info.color_target_mip)
            }

            rt.color_view = image.per_mip_views.at(info.color_target_mip);
        }

        VkExtent3D image_extent = resource_manager->get_image_data(info.color_target_handle).extent;
        rt.extent = VkExtent2D{
            std::max(image_extent.width / (1U << info.color_target_mip), 1U),
            std::max(image_extent.height / (1U << info.color_target_mip), 1U)
        };
    }

    if(info.depth_target_handle != INVALID_HANDLE) {
        const auto& image = resource_manager->get_image_data(info.depth_target_handle);

        if(image.per_mip_views.empty()) {
            rt.depth_view = image.view;
        } else {
            if(info.depth_target_mip > static_cast<u32>(image.per_mip_views.size())) {
                DEBUG_PANIC("RenderTarget depth target mip out of bounds! depth_target_mip = " << info.depth_target_mip)
            }

            rt.depth_view = image.per_mip_views.at(info.depth_target_mip);
        }

        VkExtent3D image_extent = resource_manager->get_image_data(info.depth_target_handle).extent;
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

    const GraphicsPipeline& pipeline = graphics_pipeline_allocator.get_element(src_pipeline);

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

    DEBUG_ASSERT(vkCreateFramebuffer(vk_device, &framebuffer_create_info, nullptr, &rt.framebuffer) == VK_SUCCESS)

    return render_target_allocator.alloc(rt);
}

void PipelineManager::destroy_graphics_pipeline(Handle<GraphicsPipeline> pipeline_handle) {
    if (!graphics_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot delete graphics pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }

    const GraphicsPipeline& pipeline = graphics_pipeline_allocator.get_element(pipeline_handle);
    vkDestroyPipelineLayout(vk_device, pipeline.layout, nullptr);
    vkDestroyRenderPass(vk_device, pipeline.render_pass, nullptr);
    vkDestroyPipeline(vk_device, pipeline.pipeline, nullptr);

    graphics_pipeline_allocator.free(pipeline_handle);
}
void PipelineManager::destroy_compute_pipeline(Handle<GraphicsPipeline> pipeline_handle) {
    if (!compute_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot delete compute pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }

    const ComputePipeline& pipeline = compute_pipeline_allocator.get_element(pipeline_handle);
    vkDestroyPipelineLayout(vk_device, pipeline.layout, nullptr);
    vkDestroyPipeline(vk_device, pipeline.pipeline, nullptr);

    compute_pipeline_allocator.free(pipeline_handle);
}
void PipelineManager::destroy_render_target(Handle<RenderTarget> rt_handle) {
    if (!render_target_allocator.is_handle_valid(rt_handle)) {
        DEBUG_PANIC("Cannot delete render target - Render target with a handle id: = " << rt_handle << ", does not exist!")
    }

    const RenderTarget& render_target = render_target_allocator.get_element(rt_handle);
    vkDestroyFramebuffer(vk_device, render_target.framebuffer, nullptr);

    render_target_allocator.free(rt_handle);
}

const GraphicsPipeline& PipelineManager::get_graphics_pipeline_data(Handle<GraphicsPipeline> pipeline_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!render_target_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot get graphics pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }
#endif

    return graphics_pipeline_allocator.get_element(pipeline_handle);
}
const ComputePipeline& PipelineManager::get_compute_pipeline_data(Handle<ComputePipeline> pipeline_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!compute_pipeline_allocator.is_handle_valid(pipeline_handle)) {
        DEBUG_PANIC("Cannot get compute pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }
#endif

    return compute_pipeline_allocator.get_element(pipeline_handle);
}
const RenderTarget& PipelineManager::get_render_target_data(Handle<RenderTarget> rt_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!render_target_allocator.is_handle_valid(rt_handle)) {
        DEBUG_PANIC("Cannot get render target - Render target with a handle id: = " << rt_handle << ", does not exist!")
    }
#endif

    return render_target_allocator.get_element(rt_handle);
}

VkShaderModule PipelineManager::create_shader_module(const std::string& path) {
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

    DEBUG_ASSERT(vkCreateShaderModule(vk_device, &module_create_info, nullptr, &shader_module) == VK_SUCCESS)

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