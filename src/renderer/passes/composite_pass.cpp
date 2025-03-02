#include "composite_pass.hpp"

void CompositePass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    m_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, // Albedo
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, // Normal
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}  // SSAO
        }
    });

    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = shared.albedo_image,
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
                .image_info {
                    .image_handle = shared.ssao_output_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });

    m_pipeline = api.rm->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./shaders/fullscreen_tri.vert.spv",
        .fragment_shader_path = "./shaders/composite.frag.spv",
        .descriptors = { m_descriptor } ,
        .color_targets = {
            RenderTargetCommonInfo {
                .format = api.rm->get_data(shared.offscreen_image).format,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }
        },
        .cull_mode = VK_CULL_MODE_NONE,
    });

    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.offscreen_image
            }
        }
    });
}
void CompositePass::resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .image_info {
                    .image_handle = shared.albedo_image,
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
                .image_info {
                    .image_handle = shared.ssao_output_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });

    api.rm->destroy(m_render_target);
    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.offscreen_image
            }
        }
    });
}
void CompositePass::destroy(const RenderAPI &api) {
    api.rm->destroy(m_render_target);
    api.rm->destroy(m_pipeline);
    api.rm->destroy(m_descriptor);
}

void CompositePass::process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) {
    api.image_barrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        ImageBarrier{
            .image_handle = shared.albedo_image,
            .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        ImageBarrier{
            .image_handle = shared.ssao_output_image,
            .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }
    });

    api.image_barrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {
        ImageBarrier{
            .image_handle = shared.offscreen_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
    });

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_target, {RenderTargetClear{}}, RenderTargetClear{});
    api.bind_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.draw_count(cmd, 3U),
    api.end_graphics_pipeline(cmd, m_pipeline);
}
