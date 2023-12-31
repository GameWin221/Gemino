#include "pipeline_manager.hpp"

#include <common/file_utils.hpp>
#include <common/debug.hpp>

PipelineManager::PipelineManager(VkDevice device) : vk_device(device) {

}
PipelineManager::~PipelineManager() {
    for(const auto& [handle, pipeline] : graphics_pipeline_map) {
        destroy_graphics_pipeline_impl(pipeline);
    }
    for(const auto& [handle, pipeline] : compute_pipeline_map) {
        destroy_compute_pipeline_impl(pipeline);
    }
    for(const auto& [handle, rt] : render_target_map) {
        vkDestroyFramebuffer(vk_device, rt.framebuffer, nullptr);
    }
}

Handle<GraphicsPipeline> PipelineManager::create_graphics_pipeline(const GraphicsPipelineCreateInfo& info) {
    auto handle = static_cast<Handle<GraphicsPipeline>>((allocated_pipelines_count++));

    graphics_pipeline_map[handle] = create_graphics_pipeline_impl(info);

    return handle;
}
Handle<ComputePipeline> PipelineManager::create_compute_pipeline(const ComputePipelineCreateInfo &info) {
    auto handle = static_cast<Handle<ComputePipeline>>((allocated_pipelines_count++));

    compute_pipeline_map[handle] = create_compute_pipeline_impl(info);

    return handle;
}
Handle<RenderTarget> PipelineManager::create_render_target(Handle<GraphicsPipeline> src_pipeline, const RenderTargetCreateInfo& info) {
    RenderTarget rt{
        .extent = info.extent,
        .color_view = info.color_view,
        .depth_view = info.depth_view
    };

    const GraphicsPipeline& pipeline = graphics_pipeline_map.at(src_pipeline);

    u32 uses_color_attachment = static_cast<u32>(info.color_view != nullptr);
    u32 uses_depth_attachment = static_cast<u32>(info.depth_view != nullptr);

    VkImageView image_views[2];
    if(uses_color_attachment) {
        image_views[0] = info.color_view;
    }
    if(uses_depth_attachment) {
        image_views[uses_color_attachment] = info.depth_view;
    }

    VkFramebufferCreateInfo framebuffer_create_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = pipeline.render_pass,
        .attachmentCount = uses_color_attachment + uses_depth_attachment,
        .pAttachments = image_views,
        .width = info.extent.width,
        .height = info.extent.height,
        .layers = 1U,
    };

    DEBUG_ASSERT(vkCreateFramebuffer(vk_device, &framebuffer_create_info, nullptr, &rt.framebuffer) == VK_SUCCESS)

    auto handle = static_cast<Handle<RenderTarget>>((allocated_rt_count++));

    render_target_map[handle] = rt;

    return handle;
}

GraphicsPipeline PipelineManager::create_graphics_pipeline_impl(const GraphicsPipelineCreateInfo& info) {
    GraphicsPipeline pipeline{
        .initial_create_info = info
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

        DEBUG_ASSERT(info.color_target.final_layout != VK_IMAGE_LAYOUT_UNDEFINED)

        all_descriptions[color_reference.attachment] = VkAttachmentDescription{
            .format = info.color_target.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = info.color_target.load_op,
            .storeOp = info.color_target.store_op,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = info.color_target.initial_layout,
            .finalLayout = info.color_target.final_layout,
        };

        subpass_description.colorAttachmentCount = 1U;
        subpass_description.pColorAttachments = &color_reference;

        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpass_dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    }
    if (uses_depth_attachment) {
        depth_reference = VkAttachmentReference{
            .attachment = uses_color_attachment,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        DEBUG_ASSERT(info.depth_target.final_layout != VK_IMAGE_LAYOUT_UNDEFINED)

        all_descriptions[depth_reference.attachment] = VkAttachmentDescription{
            .format = info.depth_target.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = info.depth_target.load_op,
            .storeOp = info.depth_target.store_op,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = info.depth_target.initial_layout,
            .finalLayout = info.depth_target.final_layout,
        };

        subpass_description.pDepthStencilAttachment = &depth_reference;

        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        subpass_dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        subpass_dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
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

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        //.setLayoutCount = static_cast<uint32_t>(info.descriptorLayouts.size()),
        //.pSetLayouts = info.descriptorLayouts.data(),
        //.pushConstantRangeCount = static_cast<uint32_t>(info.pushConstantRanges.size()),
        //.pPushConstantRanges = info.pushConstantRanges.data()
    };

    DEBUG_ASSERT(vkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &pipeline.layout) == VK_SUCCESS)

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
    std::vector<VkShaderModule> shader_modules{};

    if(!info.vertex_shader_path.empty()) {
        auto [module, stage_info] = create_shader_data(info.vertex_shader_path, VK_SHADER_STAGE_VERTEX_BIT);
        shader_modules.push_back(module);
        shader_stages.push_back(stage_info);
    }

    if(!info.fragment_shader_path.empty()) {
        auto [module, stage_info] = create_shader_data(info.fragment_shader_path, VK_SHADER_STAGE_FRAGMENT_BIT);
        shader_modules.push_back(module);
        shader_stages.push_back(stage_info);
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
    };

    DEBUG_ASSERT(vkCreateGraphicsPipelines(vk_device, nullptr, 1U, &pipeline_create_info, nullptr, &pipeline.pipeline) == VK_SUCCESS)

    for (const auto& module : shader_modules) {
        vkDestroyShaderModule(vk_device, module, nullptr);
    }

    return pipeline;
}
ComputePipeline PipelineManager::create_compute_pipeline_impl(const ComputePipelineCreateInfo& info) {
    ComputePipeline pipeline{
        .initial_create_info = info
    };

    VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        ///.setLayoutCount,
        ///.pSetLayouts,
        ///.pushConstantRangeCount,
        ///.pPushConstantRanges,
    };

    DEBUG_ASSERT(vkCreatePipelineLayout(vk_device, &layout_create_info, nullptr, &pipeline.layout) == VK_SUCCESS)

    ShaderData shader = create_shader_data(info.shader_path, VK_SHADER_STAGE_COMPUTE_BIT);

    VkComputePipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader.module,

            .pName = "main",
        },
        .layout = pipeline.layout
    };

    DEBUG_ASSERT(vkCreateComputePipelines(vk_device, nullptr, 1U, &pipeline_create_info, nullptr, &pipeline.pipeline) == VK_SUCCESS)

    vkDestroyShaderModule(vk_device, shader.module, nullptr);

    return pipeline;
}

void PipelineManager::destroy_graphics_pipeline(Handle<GraphicsPipeline> pipeline_handle) {
    if (!graphics_pipeline_map.contains(pipeline_handle)) {
        DEBUG_PANIC("Cannot delete graphics pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }

    destroy_graphics_pipeline_impl(graphics_pipeline_map.at(pipeline_handle));

    graphics_pipeline_map.erase(pipeline_handle);
}
void PipelineManager::destroy_compute_pipeline(Handle<GraphicsPipeline> pipeline_handle) {
    if (!compute_pipeline_map.contains(pipeline_handle)) {
        DEBUG_PANIC("Cannot delete compute pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }

    destroy_compute_pipeline_impl(compute_pipeline_map.at(pipeline_handle));

    compute_pipeline_map.erase(pipeline_handle);
}
void PipelineManager::destroy_render_target(Handle<RenderTarget> rt_handle) {
    if (!render_target_map.contains(rt_handle)) {
        DEBUG_PANIC("Cannot delete render target - Render target with a handle id: = " << rt_handle << ", does not exist!")
    }

    render_target_map.erase(rt_handle);
}

void PipelineManager::destroy_graphics_pipeline_impl(const GraphicsPipeline& pipeline) {
    vkDestroyPipelineLayout(vk_device, pipeline.layout, nullptr);
    vkDestroyRenderPass(vk_device, pipeline.render_pass, nullptr);
    vkDestroyPipeline(vk_device, pipeline.pipeline, nullptr);
}
void PipelineManager::destroy_compute_pipeline_impl(const ComputePipeline& pipeline) {
    vkDestroyPipelineLayout(vk_device, pipeline.layout, nullptr);
    vkDestroyPipeline(vk_device, pipeline.pipeline, nullptr);
}

const GraphicsPipeline& PipelineManager::get_graphics_pipeline_data(Handle<GraphicsPipeline> pipeline_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!graphics_pipeline_map.contains(pipeline_handle)) {
        DEBUG_PANIC("Cannot get graphics pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }
#endif

    return graphics_pipeline_map.at(pipeline_handle);
}
const ComputePipeline& PipelineManager::get_compute_pipeline_data(Handle<ComputePipeline> pipeline_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!compute_pipeline_map.contains(pipeline_handle)) {
        DEBUG_PANIC("Cannot get compute pipeline - Pipeline with a handle id: = " << pipeline_handle << ", does not exist!")
    }
#endif

    return compute_pipeline_map.at(pipeline_handle);
}
const RenderTarget& PipelineManager::get_render_target_data(Handle<RenderTarget> rt_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!render_target_map.contains(rt_handle)) {
        DEBUG_PANIC("Cannot get render target - Render target with a handle id: = " << rt_handle << ", does not exist!")
    }
#endif

    return render_target_map.at(rt_handle);
}

ShaderData PipelineManager::create_shader_data(const std::string &path, VkShaderStageFlagBits stage) {
    ShaderData data{};

    std::vector<u8> spirv_code = FileUtils::read_file_bytes(path);

    VkShaderModuleCreateInfo module_create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = static_cast<u32>(spirv_code.size()),
        .pCode = reinterpret_cast<const u32*>(spirv_code.data()),
    };

    DEBUG_ASSERT(vkCreateShaderModule(vk_device, &module_create_info, nullptr, &data.module) == VK_SUCCESS)

    VkPipelineShaderStageCreateInfo stage_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = data.module,
        .pName = "main"
    };

    data.stage_info = stage_create_info;

    return data;
}
