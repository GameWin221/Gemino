#include "geometry_pass.hpp"

void GeometryPass::init(
    const RenderAPI &api,
    Handle<Buffer> scene_draw_buffer,
    Handle<Buffer> scene_object_buffer,
    Handle<Buffer> scene_global_transform_buffer,
    Handle<Buffer> scene_material_buffer,
    Handle<Buffer> scene_mesh_instance_buffer,
    Handle<Buffer> scene_mesh_instance_materials_buffer,
    Handle<Buffer> scene_camera_buffer,
    Handle<Buffer> scene_vertex_buffer,
    Handle<Image> offscreen_image,
    Handle<Image> depth_image,
    Handle<Descriptor> scene_texture_descriptor
) {
    m_descriptor = api.m_resource_manager->create_descriptor(DescriptorCreateInfo{
        .bindings{
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Draw Command Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Scene Object Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Global transform Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Material Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Mesh Instance Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Mesh Instance Materials Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, // Camera Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Vertex Buffer
        }
    });
    
    api.m_resource_manager->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .buffer_info {
                    .buffer_handle = scene_draw_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = scene_global_transform_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 3U,
                .buffer_info {
                    .buffer_handle = scene_material_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 4U,
                .buffer_info {
                    .buffer_handle = scene_mesh_instance_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 5U,
                .buffer_info {
                    .buffer_handle = scene_mesh_instance_materials_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 6U,
                .buffer_info {
                    .buffer_handle = scene_camera_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 7U,
                .buffer_info {
                    .buffer_handle = scene_vertex_buffer
                }
            }
        }
    });

    m_pipeline = api.m_pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./res/shared/shaders/forward.vert.spv",
        .fragment_shader_path = "./res/shared/shaders/forward.frag.spv",

        //.push_constants_size = sizeof(PushConstants),
        .descriptors { m_descriptor, scene_texture_descriptor },

        .color_target {
            .format = api.m_resource_manager->get_image_data(offscreen_image).format,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        .depth_target {
            .format = api.m_resource_manager->get_image_data(depth_image).format,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },

        .enable_depth_test = true,
        .enable_depth_write = true,

        .depth_compare_op = VK_COMPARE_OP_GREATER,
    });

    m_render_target = api.m_pipeline_manager->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_target_handle = offscreen_image,
        .depth_target_handle = depth_image
    });
}
void GeometryPass::destroy(const RenderAPI &api) {
    api.m_pipeline_manager->destroy_render_target(m_render_target);
    api.m_pipeline_manager->destroy_graphics_pipeline(m_pipeline);
    api.m_resource_manager->destroy_descriptor(m_descriptor);
}

void GeometryPass::resize(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Image> depth_image) {
    api.m_pipeline_manager->destroy_render_target(m_render_target);

    m_render_target = api.m_pipeline_manager->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_target_handle = offscreen_image,
        .depth_target_handle = depth_image
    });
}

void GeometryPass::process(
    const RenderAPI &api,
    Handle<CommandList> cmd,
    Handle<Image> offscreen_image,
    Handle<Descriptor> scene_texture_descriptor,
    Handle<Buffer> scene_index_buffer,
    Handle<Buffer> scene_draw_buffer,
    Handle<Buffer> scene_draw_count_buffer,
    u32 max_draws,
    u32 stride
) {
    api.image_barrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {
        ImageBarrier{
            .image_handle = offscreen_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }
    });

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_target, RenderTargetClear{
        .color = {0.0f, 0.0f, 0.0f, 1.0f},
        .depth = 0.0f
    });

    // No vertex buffer bound because of programmable vertex fetching
    api.bind_graphics_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.bind_graphics_descriptor(cmd, m_pipeline, scene_texture_descriptor, 1U);
    api.bind_index_buffer(cmd, scene_index_buffer);
    api.draw_indexed_indirect_count(cmd,scene_draw_buffer, scene_draw_count_buffer, max_draws, stride);

    api.end_graphics_pipeline(cmd, m_pipeline);
}
