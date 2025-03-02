#include "draw_call_gen_pass.hpp"

#include "common/utils.hpp"

void DrawCallGenPass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    m_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Mesh Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Mesh Instance Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Primitive Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Object Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Global transform Buffer
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Draw Commands
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Draw Commands Count
            DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, // Camera Buffer
        }
    });

    api.rm->update_descriptor(m_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .buffer_info {
                    .buffer_handle = shared.scene_mesh_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = shared.scene_mesh_instance_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = shared.scene_primitive_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 3U,
                .buffer_info {
                    .buffer_handle = shared.scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 4U,
                .buffer_info {
                    .buffer_handle = shared.scene_global_transform_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 5U,
                .buffer_info {
                    .buffer_handle = shared.scene_draw_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 6U,
                .buffer_info {
                    .buffer_handle = shared.scene_draw_count_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 7U,
                .buffer_info {
                    .buffer_handle = shared.scene_camera_buffer
                },
            }
        }
    });

    m_pipeline = api.rm->create_compute_pipeline(ComputePipelineCreateInfo{
        .shader_path = "./shaders/draw_call_gen.comp.spv",
        .shader_constant_values {
            static_cast<uint32_t>(shared.config_enable_dynamic_lod),
            static_cast<uint32_t>(shared.config_enable_frustum_cull),
            api.instance->get_physical_device_preferred_warp_size(),
        },
        .push_constants_size = sizeof(DrawCallGenPushConstant),
        .descriptors { m_descriptor }
    });
}
void DrawCallGenPass::resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {

}
void DrawCallGenPass::destroy(const RenderAPI &api) {
    api.rm->destroy(m_pipeline);
    api.rm->destroy(m_descriptor);
}


void DrawCallGenPass::process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) {
    api.fill_buffer(cmd, shared.scene_draw_count_buffer, 0U, sizeof(u32));

    api.buffer_barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = shared.scene_draw_count_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        }
    });

    u32 scene_objects_count = static_cast<u32>(world.get_objects().size());

    DrawCallGenPushConstant push_constant{
        .object_count_pre_cull = scene_objects_count,
        .global_lod_bias = shared.config_global_lod_bias,
        .global_cull_dist_multiplier = shared.config_global_cull_dist_multiplier,
        .lod_sphere_visible_angle = shared.config_lod_sphere_visible_angle
    };

    api.begin_compute_pipeline(cmd, m_pipeline);
    api.bind_descriptor(cmd, m_pipeline, m_descriptor, 0U);
    api.push_constants(cmd, m_pipeline, &push_constant);
    api.dispatch_compute_pipeline(cmd, Utils::div_ceil(scene_objects_count, api.instance->get_physical_device_preferred_warp_size()));

    api.buffer_barrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = shared.scene_draw_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        },
        BufferBarrier{
            .buffer_handle = shared.scene_draw_count_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        },
    });
}
