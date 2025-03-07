#include "geometry_pass.hpp"

#include "renderer/renderer.hpp"

void GeometryPass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    m_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
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
    
    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .buffer_info {
                    .buffer_handle = shared.scene_draw_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = shared.scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = shared.scene_global_transform_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 3U,
                .buffer_info {
                    .buffer_handle = shared.scene_material_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 4U,
                .buffer_info {
                    .buffer_handle = shared.scene_mesh_instance_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 5U,
                .buffer_info {
                    .buffer_handle = shared.scene_mesh_instance_materials_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 6U,
                .buffer_info {
                    .buffer_handle = shared.scene_camera_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 7U,
                .buffer_info {
                    .buffer_handle = shared.scene_vertex_buffer
                }
            }
        }
    });

    m_pipeline = api.rm->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./shaders/forward.vert.spv",
        .fragment_shader_path = "./shaders/forward.frag.spv",

        //.push_constants_size = sizeof(PushConstants),
        .descriptors { m_descriptor, shared.scene_texture_descriptor },

        .color_targets {
            RenderTargetCommonInfo {
                .format = api.rm->get_data(shared.albedo_image).format,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            RenderTargetCommonInfo {
                .format = api.rm->get_data(shared.normal_image).format,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            }
        },
        .depth_target {
            .format = api.rm->get_data(shared.depth_image).format,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },

        .enable_depth_test = true,
        .enable_depth_write = true,

        .depth_compare_op = VK_COMPARE_OP_GREATER,
    });

    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.albedo_image,
            },
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.normal_image,
            }
        },
        .depth_attachment = {
            .target_handle = shared.depth_image
        }
    });
}
void GeometryPass::resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    api.rm->destroy(m_render_target);

    m_render_target = api.rm->create_render_target(m_pipeline, RenderTargetCreateInfo{
        .color_attachments = {
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.albedo_image,
            },
            RenderTargetAttachmentCreateInfo {
                .target_handle = shared.normal_image,
            }
        },
        .depth_attachment = {
            .target_handle = shared.depth_image
        }
    });
}
void GeometryPass::destroy(const RenderAPI &api) {
    api.rm->destroy(m_render_target);
    api.rm->destroy(m_pipeline);
    api.rm->destroy(m_descriptor);
}

void GeometryPass::process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) {
    api.image_barrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {
        ImageBarrier{
            .image_handle = shared.albedo_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        ImageBarrier{
            .image_handle = shared.normal_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }
    });

    api.begin_graphics_pipeline(cmd, m_pipeline, m_render_target, {
        RenderTargetClear{
            .color = {0.0f, 0.0f, 0.0f, 0.0f},
        },
        RenderTargetClear{
            .color = {0.0f, 0.0f, 0.0f, 0.0f},
        }
    }, RenderTargetClear{
        .depth = 0.0f
    });

    // No vertex buffer bound because of programmable vertex fetching
    api.bind_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.bind_descriptor(cmd, m_pipeline, shared.scene_texture_descriptor, 1U);
    api.bind_index_buffer(cmd, shared.scene_index_buffer);
    api.draw_indexed_indirect_count(cmd,shared.scene_draw_buffer, shared.scene_draw_count_buffer, world.get_objects().size(), sizeof(DrawCommand));

    api.end_graphics_pipeline(cmd, m_pipeline);
}
