#include "ssao_pass.hpp"

void SSAOPass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    m_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, // Depth Image
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, // Normal Image
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, // Camera
        }
    });
    m_blur_descriptors[0] = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
        }
    });
    m_blur_descriptors[1] = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
        }
    });

    const auto &ssao_image_info = api.rm->get_data(shared.ssao_output_image);

    m_pingpong_image = api.rm->create_image(ImageCreateInfo{
        .format = ssao_image_info.format,
        .extent = ssao_image_info.extent,
        .usage_flags = ssao_image_info.usage_flags,
        .aspect_flags = ssao_image_info.aspect_flags
    });

    api.record_and_submit_once([&api, this](Handle<CommandList> cmd){
        api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, { ImageBarrier{
            .image_handle = m_pingpong_image,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }});
    });

    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = shared.depth_image,
                    .image_sampler = shared.offscreen_sampler
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 1u,
                .image_info {
                    .image_handle = shared.normal_image,
                    .image_sampler = shared.offscreen_sampler
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 2u,
                .buffer_info = {
                    .buffer_handle = shared.scene_camera_buffer
                }
            }
        }
    });
    api.rm->update_descriptor(m_blur_descriptors[0], DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = shared.ssao_output_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });
    api.rm->update_descriptor(m_blur_descriptors[1], DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = m_pingpong_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });

    m_pipeline = api.rm->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./shaders/fullscreen_tri.vert.spv",
        .fragment_shader_path = "./shaders/SSAO.frag.spv",
        .fragment_constant_values { shared.config_ssao_samples },
        .push_constants_size = sizeof(SSAOPushConstant),
        .descriptors = { m_descriptor } ,
        .color_targets = {
            RenderTargetCommonInfo {
                .format = api.rm->get_data(shared.ssao_output_image).format,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }
        },
        .cull_mode = VK_CULL_MODE_NONE,
    });

    m_blur_pipeline = api.rm->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./shaders/fullscreen_tri.vert.spv",
        .fragment_shader_path = "./shaders/SSAO_blur.frag.spv",
        //.fragment_constant_values { shared.config_ssao_samples },
        .push_constants_size = sizeof(SSAOBlurPushConstant),
        .descriptors = { m_blur_descriptors[0] } ,
        .color_targets = {
            RenderTargetCommonInfo {
                .format = api.rm->get_data(shared.ssao_output_image).format,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }
        },
        .cull_mode = VK_CULL_MODE_NONE,
    });

    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.ssao_output_image
            }
        }
    });

    m_blur_rts[0] = api.rm->create_render_target(m_blur_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = m_pingpong_image
            }
        }
    });
    m_blur_rts[1] = api.rm->create_render_target(m_blur_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.ssao_output_image
            }
        }
    });
}
void SSAOPass::resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = shared.depth_image,
                    .image_sampler = shared.offscreen_sampler
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 1u,
                .image_info {
                    .image_handle = shared.normal_image,
                    .image_sampler = shared.offscreen_sampler
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 2u,
                .buffer_info = {
                    .buffer_handle = shared.scene_camera_buffer
                }
            }
        }
    });

    const auto &ssao_image_info = api.rm->get_data(shared.ssao_output_image);

    api.rm->destroy(m_pingpong_image);
    m_pingpong_image = api.rm->create_image(ImageCreateInfo{
        .format = ssao_image_info.format,
        .extent = ssao_image_info.extent,
        .usage_flags = ssao_image_info.usage_flags,
        .aspect_flags = ssao_image_info.aspect_flags
    });
    api.record_and_submit_once([&api, this](Handle<CommandList> cmd){
        api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, { ImageBarrier{
            .image_handle = m_pingpong_image,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }});
    });

    api.rm->update_descriptor(m_blur_descriptors[0], DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = shared.depth_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });
    api.rm->update_descriptor(m_blur_descriptors[1], DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = m_pingpong_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });

    api.rm->destroy(m_render_target);
    api.rm->destroy(m_blur_rts[0]);
    api.rm->destroy(m_blur_rts[1]);
    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.ssao_output_image
            }
        }
    });

    m_blur_rts[0] = api.rm->create_render_target(m_blur_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = m_pingpong_image
            }
        }
    });
    m_blur_rts[1] = api.rm->create_render_target(m_blur_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.ssao_output_image
            }
        }
    });
}
void SSAOPass::destroy(const RenderAPI &api) {
    api.rm->destroy(m_blur_rts[0]);
    api.rm->destroy(m_blur_rts[1]);
    api.rm->destroy(m_pingpong_image);
    api.rm->destroy(m_blur_pipeline);
    api.rm->destroy(m_blur_descriptors[0]);
    api.rm->destroy(m_blur_descriptors[1]);

    api.rm->destroy(m_render_target);
    api.rm->destroy(m_pipeline);
    api.rm->destroy(m_descriptor);
}

void SSAOPass::process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) {
    api.image_barrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        ImageBarrier{
            .image_handle = shared.depth_image,
            .src_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
    });
    api.image_barrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        ImageBarrier{
            .image_handle = shared.normal_image,
            .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }
    });

    api.image_barrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {
        ImageBarrier{
            .image_handle = shared.ssao_output_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
    });

    VkExtent3D image_size = api.rm->get_data(shared.ssao_output_image).extent;

    SSAOPushConstant ssao_pc {
        .screen_wh_combined = static_cast<i32>(image_size.width | (image_size.height << 16)),
        .radius = shared.config_ssao_radius,
        .bias = shared.config_ssao_bias,
        .multiplier = shared.config_ssao_multiplier,
        .noise_scale = shared.config_ssao_noise_scale_divider
    };

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_target, {RenderTargetClear{}}, RenderTargetClear{});
    api.bind_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.push_constants(cmd, m_pipeline, &ssao_pc);
    api.draw_count(cmd, 3U),
    api.end_graphics_pipeline(cmd, m_pipeline);

    api.image_barrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, {
        ImageBarrier{
            .image_handle = shared.depth_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    });

    SSAOBlurPushConstant blur_pc {
        .screen_wh_combined = static_cast<i32>(image_size.width | (image_size.height << 16)),
        .blur_radius = shared.config_ssao_blur_radius
    };

    std::vector<VkPipelineStageFlags> pingpong_stages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
    std::vector<VkAccessFlags> pingpong_access_masks = { VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
    std::vector<VkImageLayout> pingpong_layouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    // 0 - horizontal, 1 - vertical
    for (int dir = 0; dir <= 1; ++dir) {
        api.image_barrier(cmd, pingpong_stages[dir], pingpong_stages[1 - dir], {
            ImageBarrier{
                .image_handle = shared.ssao_output_image,
                .src_access_mask = pingpong_access_masks[dir],
                .dst_access_mask = pingpong_access_masks[1 - dir],
                .old_layout = pingpong_layouts[dir],
                .new_layout = pingpong_layouts[1 - dir]
            },
        });

        blur_pc.blur_dir = dir;

        api.begin_graphics_pipeline(cmd, m_blur_pipeline, m_blur_rts[dir], {RenderTargetClear{}}, RenderTargetClear{});
        api.bind_descriptor(cmd, m_blur_pipeline, m_blur_descriptors[dir], 0U);
        api.push_constants(cmd, m_blur_pipeline, &blur_pc);
        api.draw_count(cmd, 3U),
        api.end_graphics_pipeline(cmd, m_blur_pipeline);

        api.image_barrier(cmd, pingpong_stages[dir], pingpong_stages[1 - dir], {
            ImageBarrier{
                .image_handle = m_pingpong_image,
                .src_access_mask = pingpong_access_masks[dir],
                .dst_access_mask = pingpong_access_masks[1 - dir],
                .old_layout = pingpong_layouts[dir],
                .new_layout = pingpong_layouts[1 - dir]
            },
        });
    }
}
