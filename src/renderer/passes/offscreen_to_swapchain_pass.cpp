#include "offscreen_to_swapchain_pass.hpp"

void OffscreenToSwapchainPass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    m_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
        }
    });

    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .image_info {
                    .image_handle = shared.offscreen_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });

    m_pipeline = api.rm->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./shaders/fullscreen_tri.vert.spv",
        .fragment_shader_path = "./shaders/fullscreen_tri.frag.spv",
        .descriptors = { m_descriptor } ,
        .color_targets {
            RenderTargetCommonInfo{
                .format = api.swapchain->get_format(),
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            }
        },
        .cull_mode = VK_CULL_MODE_NONE,
    });

    m_render_targets.resize(api.get_swapchain_image_count());
    for(u32 i{}; i < api.get_swapchain_image_count(); ++i){
        m_render_targets[i] = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
            .color_attachments = {
                RenderTargetAttachmentCreateInfo{
                    .target_handle = api.get_swapchain_image_handle(i)
                }
            }
        });
    }
}
void OffscreenToSwapchainPass::resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .image_info {
                    .image_handle = shared.offscreen_image,
                    .image_sampler = shared.offscreen_sampler
                }
            }
        }
    });

    for (const auto &target : m_render_targets) {
        api.rm->destroy(target);
    }
    m_render_targets.clear();

    m_render_targets.resize(api.get_swapchain_image_count());
    for(u32 i{}; i < api.get_swapchain_image_count(); ++i){
        m_render_targets[i] = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
            .color_attachments = {
                RenderTargetAttachmentCreateInfo{
                    .target_handle = api.get_swapchain_image_handle(i)
                }
            }
        });
    }
}
void OffscreenToSwapchainPass::destroy(const RenderAPI &api) {
    for (const auto &target : m_render_targets) {
        api.rm->destroy(target);
    }
    m_render_targets.clear();

    api.rm->destroy(m_pipeline);
    api.rm->destroy(m_descriptor);
}

void OffscreenToSwapchainPass::process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) {
    api.image_barrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { ImageBarrier{
        .image_handle = shared.offscreen_image,
        .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }});

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_targets[shared.swapchain_target_index], {RenderTargetClear{}}, RenderTargetClear{});
    api.bind_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.draw_count(cmd, 3U),
    api.end_graphics_pipeline(cmd, m_pipeline);
}
