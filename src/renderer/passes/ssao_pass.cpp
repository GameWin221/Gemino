#include "ssao_pass.hpp"

void SSAOPass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    m_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, // Depth Image
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, // Normal Image
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, // Camera
        }
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

    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
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

    api.rm->destroy(m_render_target);
    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.ssao_output_image
            }
        }
    });
}
void SSAOPass::destroy(const RenderAPI &api) {
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

    SSAOPushConstant pc {
        .screen_wh_combined = static_cast<i32>(image_size.width | (image_size.height << 16)),
        .radius = shared.config_ssao_radius,
        .bias = shared.config_ssao_bias,
        .multiplier = shared.config_ssao_multiplier,
        .noise_scale = shared.config_ssao_noise_scale_divider
    };

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_target, {RenderTargetClear{}}, RenderTargetClear{});
    api.bind_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.push_constants(cmd, m_pipeline, &pc);
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
}
