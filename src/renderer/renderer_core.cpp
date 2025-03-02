#include "renderer.hpp"

Renderer::Renderer(Window &window, VSyncMode v_sync) : m_api(window, SwapchainConfig{
    .v_sync = v_sync,
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
}) {
    init_scene_buffers();
    init_screen_images(window.get_size());
    init_descriptors();
    init_passes(window);
    init_frames();
    init_defaults();
}
Renderer::~Renderer() {
    m_api.wait_for_device_idle();

    destroy_defaults();
    destroy_frames();
    destroy_passes();
    destroy_descriptors();
    destroy_screen_images();
    destroy_scene_buffers();
}

void Renderer::init_scene_buffers() {
    m_shared.scene_material_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Material) * MAX_SCENE_MATERIALS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_draw_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(DrawCommand) * MAX_SCENE_DRAWS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_draw_count_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(u32),
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_mesh_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Mesh) * MAX_SCENE_MESHES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_mesh_instance_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(MeshInstance) * MAX_SCENE_MESH_INSTANCES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_mesh_instance_materials_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Handle<Material>) * MAX_SCENE_MESH_INSTANCE_MATERIALS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_primitive_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Primitive) * MAX_SCENE_PRIMITIVES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_object_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Object) * MAX_SCENE_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_global_transform_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Transform) * MAX_SCENE_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_camera_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Camera),
        .buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_vertex_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(Vertex) * MAX_SCENE_VERTICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_shared.scene_index_buffer = m_api.rm->create_buffer(BufferCreateInfo{
        .size = sizeof(u32) * MAX_SCENE_INDICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
}
void Renderer::init_screen_images(glm::uvec2 size) {
    VkExtent3D screen_size{ size.x, size.y };

    m_shared.offscreen_image = m_api.rm->create_image(ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = screen_size,
        .usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT
    });
    m_shared.offscreen_sampler = m_api.rm->create_sampler(SamplerCreateInfo{
        .filter = VK_FILTER_NEAREST
    });

    m_shared.depth_image = m_api.rm->create_image(ImageCreateInfo{
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = screen_size,
        .usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT
    });

    std::vector<ImageBarrier> init_swapchain_barriers{};
    for(u32 i{}; i < m_api.get_swapchain_image_count(); ++i){
        init_swapchain_barriers.push_back(ImageBarrier{
            .image_handle = m_api.get_swapchain_image_handle(i),
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
            .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        });
    }

    m_api.record_and_submit_once([this, &init_swapchain_barriers](Handle<CommandList> cmd){
        m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT , init_swapchain_barriers);
        m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, { ImageBarrier{
            .image_handle = m_shared.depth_image,
            .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }});
        m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { ImageBarrier{
            .image_handle = m_shared.offscreen_image,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});
    });
}
void Renderer::init_descriptors() {
    m_shared.scene_texture_descriptor = m_api.rm->create_descriptor(DescriptorCreateInfo{
        .bindings = {
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<u32>(MAX_SCENE_TEXTURES) } // Bindless scene textures
        }
    });
}
void Renderer::init_passes(const Window &window) {
    m_registered_passes["Draw Call Generation Pass"] = RegisteredPass {
        .order = 0u,
        .pass_ptr = MakeUnique<DrawCallGenPass>()
    };
    m_registered_passes["Geometry Pass"] = RegisteredPass {
        .query_statistics = true,
        .order = 1u,
        .pass_ptr = MakeUnique<GeometryPass>()
    };
    m_registered_passes["Debug Pass"] = RegisteredPass {
        .order = 2u,
        .pass_ptr = MakeUnique<DebugPass>()
    };
    m_registered_passes["Offscreen To Swapchain Pass"] = RegisteredPass {
        .order = 3u,
        .pass_ptr = MakeUnique<OffscreenToSwapchainPass>()
    };
    m_registered_passes["UI Pass"] = RegisteredPass {
        .order = 4u,
        .pass_ptr = MakeUnique<UIPass>()
    };

    for(auto &[name, registered_pass] : m_registered_passes) {
        registered_pass.pass_ptr->init(m_api, m_shared, window);
    }
}
void Renderer::init_frames() {
    m_frames.resize(FRAMES_IN_FLIGHT);

    for(u32 i{}; i < static_cast<u32>(m_frames.size()); ++i) {
        auto &frame = m_frames[i];

        frame = Frame{
            .command_list = m_api.rm->create_command_list(QueueFamily::Graphics),
            .present_semaphore = m_api.rm->create_semaphore(),
            .render_semaphore = m_api.rm->create_semaphore(),
            .fence = m_api.rm->create_fence(),
            .upload_buffer = m_api.rm->create_buffer(BufferCreateInfo{
                .size = PER_FRAME_UPLOAD_BUFFER_SIZE,
                .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
            })
        };

        frame.upload_ptr = m_api.rm->map_buffer(frame.upload_buffer);

        std::vector<std::string> timestamp_query_names{
            "Buffers Copy",
            "Total GPU Time"
        };
        std::vector<std::string> pipeline_statistics_query_names{};

        for(const auto &[name, registered_pass] : m_registered_passes) {
            timestamp_query_names.push_back(name);

            if (registered_pass.query_statistics) {
                pipeline_statistics_query_names.push_back(name);
            }
        }

        for (const auto &name : timestamp_query_names) {
            frame.gpu_timing[name] = std::make_pair(
                std::make_pair(
                    m_api.rm->create_query(QueryType::Timestamp),
                    m_api.rm->create_query(QueryType::Timestamp)
                ),
                std::make_pair(0.0, 0.0)
            );
        }
        for (const auto &name : pipeline_statistics_query_names) {
            frame.gpu_pipeline_statistics[name] = std::make_pair(m_api.rm->create_query(QueryType::PipelineStatistics), QueryPipelineStatisticsResults{});
        }

        std::vector<Handle<Query>> queries_to_be_reset{};
        for(auto &[name, data] : frame.gpu_timing) {
            auto &[queries, time] = data;
            queries_to_be_reset.push_back(queries.first);
            queries_to_be_reset.push_back(queries.second);
        }
        for(auto &[name, data] : frame.gpu_pipeline_statistics) {
            auto &[query, time] = data;
            queries_to_be_reset.push_back(query);
        }
        m_api.reset_queries_immediate(queries_to_be_reset);
    }
}
void Renderer::init_defaults() {
    u8 default_white_srgb_texture_data[] = { 255, 255, 255, 255 };
    u8 default_grey_unorm_texture_data[] = { 127, 127, 127, 255 };

    m_shared.default_white_srgb_texture = create_u8_texture(TextureCreateInfo{
        .pixel_data = default_white_srgb_texture_data,
        .width = 1U,
        .height = 1U,
        .bytes_per_pixel = 4U,
        .is_srgb = true,
        .gen_mip_maps = false,
        .linear_filter = false
    });
    m_shared.default_grey_unorm_texture = create_u8_texture(TextureCreateInfo{
        .pixel_data = default_grey_unorm_texture_data,
        .width = 1U,
        .height = 1U,
        .bytes_per_pixel = 4U,
        .is_srgb = false,
        .gen_mip_maps = false,
        .linear_filter = false
    });
    m_shared.default_material = create_material(MaterialCreateInfo {});

    auto props = m_api.instance->get_physical_device_properties_vk_1_0();
    m_default_timestamp_period = props.limits.timestampPeriod;
}

void Renderer::resize_passes(const Window &window) {
    for(auto &[name, registered_pass] : m_registered_passes) {
        registered_pass.pass_ptr->resize(m_api, m_shared, window);
    }
}

void Renderer::destroy_scene_buffers() {
    m_api.rm->destroy(m_shared.scene_material_buffer);
    m_api.rm->destroy(m_shared.scene_draw_buffer);
    m_api.rm->destroy(m_shared.scene_draw_count_buffer);
    m_api.rm->destroy(m_shared.scene_mesh_buffer);
    m_api.rm->destroy(m_shared.scene_mesh_instance_materials_buffer);
    m_api.rm->destroy(m_shared.scene_mesh_instance_buffer);
    m_api.rm->destroy(m_shared.scene_primitive_buffer);
    m_api.rm->destroy(m_shared.scene_object_buffer);
    m_api.rm->destroy(m_shared.scene_global_transform_buffer);
    m_api.rm->destroy(m_shared.scene_camera_buffer);
    m_api.rm->destroy(m_shared.scene_vertex_buffer);
    m_api.rm->destroy(m_shared.scene_index_buffer);
}
void Renderer::destroy_screen_images() {
    m_api.rm->destroy(m_shared.offscreen_image);
    m_api.rm->destroy(m_shared.depth_image);
    m_api.rm->destroy(m_shared.offscreen_sampler);
}
void Renderer::destroy_descriptors() {
    m_api.rm->destroy(m_shared.scene_texture_descriptor);
}
void Renderer::destroy_passes() {
    for(auto &[name, registered_pass] : m_registered_passes) {
        registered_pass.pass_ptr->destroy(m_api);
    }

    m_registered_passes.clear();
}
void Renderer::destroy_frames() {
    for(auto& frame : m_frames) {
        for(const auto &[name, data] : frame.gpu_timing) {
            const auto &[queries, time] = data;
            m_api.rm->destroy(queries.first);
            m_api.rm->destroy(queries.second);
        }

        for(const auto &[name, queries] : frame.gpu_pipeline_statistics) {
            m_api.rm->destroy(queries.first);
        }

        frame.cpu_timing.clear();
        frame.gpu_timing.clear();
        frame.gpu_pipeline_statistics.clear();

        m_api.rm->destroy(frame.command_list);
        m_api.rm->destroy(frame.present_semaphore);
        m_api.rm->destroy(frame.render_semaphore);
        m_api.rm->destroy(frame.fence);
        m_api.rm->unmap_buffer(frame.upload_buffer);
        m_api.rm->destroy(frame.upload_buffer);
    }

    m_frames.clear();
}
void Renderer::destroy_defaults() {
    destroy(m_shared.default_white_srgb_texture);
    destroy(m_shared.default_grey_unorm_texture);
    destroy(m_shared.default_material);
}

void Renderer::set_config_global_lod_bias(f32 value) {
    m_shared.config_global_lod_bias = value;
}
void Renderer::set_config_global_cull_dist_multiplier(f32 value) {
    m_shared.config_global_cull_dist_multiplier = std::max(value, 0.0f);
}
void Renderer::set_config_enable_dynamic_lod(bool enable) {
    m_shared.config_enable_dynamic_lod = enable;
    reload_pipelines();
}
void Renderer::set_config_enable_frustum_cull(bool enable) {
    m_shared.config_enable_frustum_cull = enable;
    reload_pipelines();
}

void Renderer::set_config_enable_debug_shape_view(bool enable) {
    m_shared.config_enable_debug_shape_view = enable;
}

void Renderer::set_config_debug_shape_opacity(f32 value) {
    m_shared.config_debug_shape_opacity = std::max(std::min(value, 1.0f), 0.0f);
}

void Renderer::set_config_lod_sphere_visible_angle(f32 value) {
    m_shared.config_lod_sphere_visible_angle = std::max(std::min(value, 179.99f), 0.0002f);
}

void Renderer::set_ui_draw_callback(UIPassDrawFn draw_callback) {
    m_shared.ui_pass_draw_fn = draw_callback;
}
