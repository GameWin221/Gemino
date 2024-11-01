#include "offscreen_to_swapchain_pass.hpp"

void OffscreenToSwapchainPass::init(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Sampler> offscreen_sampler) {
    m_descriptor = api.m_resource_manager->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
        }
    });

    api.m_resource_manager->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .image_info {
                    .image_handle = offscreen_image,
                    .image_sampler = offscreen_sampler
                }
            }
        }
    });

    m_pipeline = api.m_pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "res/shared/shaders/fullscreen_tri.vert.spv",
        .fragment_shader_path = "res/shared/shaders/fullscreen_tri.frag.spv",
        .descriptors = { m_descriptor } ,
        .color_target {
            .format = api.m_swapchain->get_format(),
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        .cull_mode = VK_CULL_MODE_NONE,
    });

    m_render_targets.resize(api.get_swapchain_image_count());
    for(u32 i{}; i < api.get_swapchain_image_count(); ++i){
        m_render_targets[i] = api.m_pipeline_manager->create_render_target(m_pipeline, RenderTargetCreateInfo{
            .color_target_handle = api.get_swapchain_image_handle(i)
        });
    }
}
void OffscreenToSwapchainPass::destroy(const RenderAPI &api) {
    for (const auto &target : m_render_targets) {
        api.m_pipeline_manager->destroy_render_target(target);
    }
    m_render_targets.clear();

    api.m_pipeline_manager->destroy_graphics_pipeline(m_pipeline);
    api.m_resource_manager->destroy_descriptor(m_descriptor);
}

void OffscreenToSwapchainPass::resize(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Sampler> offscreen_sampler) {
    api.m_resource_manager->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .image_info {
                    .image_handle = offscreen_image,
                    .image_sampler = offscreen_sampler
                }
            }
        }
    });

    for (const auto &target : m_render_targets) {
        api.m_pipeline_manager->destroy_render_target(target);
    }
    m_render_targets.clear();

    m_render_targets.resize(api.get_swapchain_image_count());
    for(u32 i{}; i < api.get_swapchain_image_count(); ++i){
        m_render_targets[i] = api.m_pipeline_manager->create_render_target(m_pipeline, RenderTargetCreateInfo{
            .color_target_handle = api.get_swapchain_image_handle(i)
        });
    }
}

void OffscreenToSwapchainPass::process(const RenderAPI &api, Handle<CommandList> cmd, Handle<Image> offscreen_image, u32 swapchain_target_index) {
    api.image_barrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { ImageBarrier{
        .image_handle = offscreen_image,
        .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }});

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_targets[swapchain_target_index], RenderTargetClear{});
    api.bind_graphics_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.draw_count(cmd, 3U),
    api.end_graphics_pipeline(cmd, m_pipeline);
}
