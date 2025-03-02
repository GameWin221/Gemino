#include "debug_pass.hpp"

#include "common/utils.hpp"

void DebugPass::init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    std::vector<glm::vec3> sphere_mesh_positions{};
    std::vector<u32> sphere_mesh_indices{};

    for(u32 axis{}; axis < 3u; ++axis) {
        for(u32 i{}; i < VERTICES_PER_AXIS; ++i) {
            f32 angle = (static_cast<f32>(i) / VERTICES_PER_AXIS) * glm::pi<f32>() * 2.0f;

            switch (axis) {
                case 0u: sphere_mesh_positions.emplace_back(glm::sin(angle), glm::cos(angle), 0.0f); break;
                case 1u: sphere_mesh_positions.emplace_back(0.0f, glm::cos(angle), glm::sin(angle)); break;
                case 2u: sphere_mesh_positions.emplace_back(glm::sin(angle), 0.0f, glm::cos(angle)); break;
            }
        }
    }

    for(u32 axis{}; axis < 3u; ++axis) {
        for(u32 i = 1u; i <= VERTICES_PER_AXIS; ++i) {
            sphere_mesh_indices.push_back(axis * VERTICES_PER_AXIS + i - 1u);
            sphere_mesh_indices.push_back(axis * VERTICES_PER_AXIS + i % VERTICES_PER_AXIS);
        }
    }

    VkDeviceSize sphere_mesh_vertex_buffer_size = sphere_mesh_positions.size() * sizeof(sphere_mesh_positions[0]);
    m_sphere_mesh_vertex_buffer = api.rm->create_buffer(BufferCreateInfo{
        .size = sphere_mesh_vertex_buffer_size,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    VkDeviceSize sphere_mesh_index_buffer_size = sphere_mesh_indices.size() * sizeof(sphere_mesh_indices[0]);
    m_sphere_mesh_index_buffer = api.rm->create_buffer(BufferCreateInfo{
        .size = sphere_mesh_index_buffer_size,
        .buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    Handle<Buffer> staging = api.rm->create_buffer(BufferCreateInfo{
        .size = std::max({ sphere_mesh_vertex_buffer_size, sphere_mesh_index_buffer_size }),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    api.rm->memcpy_to_buffer_once(staging, sphere_mesh_positions.data(), sphere_mesh_vertex_buffer_size);
    api.record_and_submit_once([&api, &staging, this, sphere_mesh_vertex_buffer_size](Handle<CommandList> cmd) {
        api.copy_buffer_to_buffer(cmd, staging, m_sphere_mesh_vertex_buffer, { VkBufferCopy {
            .size = sphere_mesh_vertex_buffer_size
        }});
    });

    api.rm->memcpy_to_buffer_once(staging, sphere_mesh_indices.data(), sphere_mesh_index_buffer_size);
    api.record_and_submit_once([&api, &staging, this, sphere_mesh_index_buffer_size](Handle<CommandList> cmd) {
        api.copy_buffer_to_buffer(cmd, staging, m_sphere_mesh_index_buffer, { VkBufferCopy {
            .size = sphere_mesh_index_buffer_size
        }});
    });

    api.rm->destroy(staging);

    m_graphics_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings {
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Vertex Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // DrawCommands Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Object Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Global Transform Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Mesh Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // MeshInstance Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, // Camera Buffer
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Draw Count Buffer
        }
    });

    api.rm->update_descriptor(m_graphics_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .buffer_info {
                    .buffer_handle = m_sphere_mesh_vertex_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 1u,
                .buffer_info {
                    .buffer_handle = shared.scene_draw_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 2u,
                .buffer_info {
                    .buffer_handle = shared.scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 3u,
                .buffer_info {
                    .buffer_handle = shared.scene_global_transform_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 4u,
                .buffer_info {
                    .buffer_handle = shared.scene_mesh_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 5u,
                .buffer_info {
                    .buffer_handle = shared.scene_mesh_instance_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 6u,
                .buffer_info {
                    .buffer_handle = shared.scene_camera_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 7u,
                .buffer_info {
                    .buffer_handle = shared.scene_draw_count_buffer
                }
            }
        }
    });

    m_graphics_pipeline = api.rm->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./shaders/debug_shape.vert.spv",
        .fragment_shader_path = "./shaders/debug_shape.frag.spv",

        .push_constants_size = sizeof(f32),

        .descriptors { m_graphics_descriptor },

        .color_target {
            .format = api.rm->get_data(shared.offscreen_image).format,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
            .store_op = VK_ATTACHMENT_STORE_OP_STORE,
        },
        .depth_target = {
            .format = api.rm->get_data(shared.depth_image).format,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
            .store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE
        },

        .enable_depth_test = true,
        .enable_depth_write = false,
        .depth_compare_op = VK_COMPARE_OP_GREATER,


        .enable_blending = true,
        .primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
   });

    m_render_target = api.rm->create_render_target(m_graphics_pipeline, RenderTargetCreateInfo{
        .color_target_handle = shared.offscreen_image,
        .depth_target_handle = shared.depth_image
    });

    m_sphere_indirect_draw_buffer = api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(VkDrawIndexedIndirectCommand),
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    m_compute_descriptor = api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings{
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Output VkDrawIndexedIndirectCommand
            DescriptorBindingCreateInfo { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER }, // Draw Command Count
        }
    });

    api.rm->update_descriptor(m_compute_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .binding_index = 0u,
                .buffer_info {
                    .buffer_handle = m_sphere_indirect_draw_buffer
                }
            },
            DescriptorBindingUpdateInfo {
                .binding_index = 1u,
                .buffer_info {
                    .buffer_handle = shared.scene_draw_count_buffer
                }
            },
        }
    });

    m_compute_pipeline = api.rm->create_compute_pipeline(ComputePipelineCreateInfo{
        .shader_path = "./shaders/debug_shape.comp.spv",
        .shader_constant_values{ static_cast<u32>(sphere_mesh_indices.size()) },
        .descriptors { m_compute_descriptor }
    });
}
void DebugPass::resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) {
    api.rm->destroy(m_render_target);

    m_render_target = api.rm->create_render_target(m_graphics_pipeline, RenderTargetCreateInfo{
        .color_target_handle = shared.offscreen_image,
        .depth_target_handle = shared.depth_image
    });
}
void DebugPass::destroy(const RenderAPI &api) {
    api.rm->destroy(m_compute_pipeline);
    api.rm->destroy(m_compute_descriptor);

    api.rm->destroy(m_sphere_indirect_draw_buffer);
    api.rm->destroy(m_render_target);
    api.rm->destroy(m_graphics_pipeline);
    api.rm->destroy(m_graphics_descriptor);

    api.rm->destroy(m_sphere_mesh_vertex_buffer);
    api.rm->destroy(m_sphere_mesh_index_buffer);
}

void DebugPass::process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) {
    api.begin_compute_pipeline(cmd, m_compute_pipeline);
        api.bind_descriptor(cmd, m_compute_pipeline, m_compute_descriptor, 0u);
        api.dispatch_compute_pipeline(cmd);

        api.buffer_barrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, {
            BufferBarrier { m_sphere_indirect_draw_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT }
        });

    api.begin_graphics_pipeline(cmd, m_graphics_pipeline, m_render_target, RenderTargetClear{});
        api.push_constants(cmd, m_graphics_pipeline, &shared.config_debug_shape_opacity);
        api.bind_descriptor(cmd, m_graphics_pipeline, m_graphics_descriptor, 0u);
        api.bind_index_buffer(cmd, m_sphere_mesh_index_buffer);
        api.draw_indexed_indirect(cmd, m_sphere_indirect_draw_buffer, 1u, sizeof(VkDrawIndexedIndirectCommand));
    api.end_graphics_pipeline(cmd, m_graphics_pipeline);
}
