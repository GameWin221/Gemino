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
    m_scene_material_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Material) * MAX_SCENE_MATERIALS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_draw_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(DrawCommand) * MAX_SCENE_DRAWS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_draw_count_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(u32),
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_mesh_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Mesh) * MAX_SCENE_MESHES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_mesh_instance_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(MeshInstance) * MAX_SCENE_MESH_INSTANCES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_mesh_instance_materials_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Handle<Material>) * MAX_SCENE_MESH_INSTANCE_MATERIALS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_primitive_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Primitive) * MAX_SCENE_PRIMITIVES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_object_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Object) * MAX_SCENE_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_global_transform_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Transform) * MAX_SCENE_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_camera_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Camera),
        .buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_vertex_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Vertex) * MAX_SCENE_VERTICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_index_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(u32) * MAX_SCENE_INDICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
}
void Renderer::init_screen_images(glm::uvec2 size) {
    VkExtent3D screen_size{ size.x, size.y };

    m_offscreen_image = m_api.m_resource_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = screen_size,
        .usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT
    });
    m_offscreen_sampler = m_api.m_resource_manager->create_sampler(SamplerCreateInfo{
        .filter = VK_FILTER_NEAREST
    });

    m_depth_image = m_api.m_resource_manager->create_image(ImageCreateInfo{
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
            .image_handle = m_depth_image,
            .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }});
        m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { ImageBarrier{
            .image_handle = m_offscreen_image,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});
    });
}
void Renderer::init_descriptors() {
    m_scene_texture_descriptor = m_api.m_resource_manager->create_descriptor(DescriptorCreateInfo{
        .bindings = {
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<u32>(MAX_SCENE_TEXTURES) } // Bindless scene textures
        }
    });
}
void Renderer::init_passes(const Window &window) {
    m_draw_call_gen_pass.init(
        m_api,
        m_scene_mesh_buffer,
        m_scene_primitive_buffer,
        m_scene_draw_buffer,
        m_scene_object_buffer,
        m_scene_global_transform_buffer,
        m_scene_mesh_instance_buffer,
        m_scene_draw_count_buffer,
        m_scene_camera_buffer,
        m_config_enable_dynamic_lod,
        m_config_enable_frustum_cull
    );

    m_geometry_pass.init(
        m_api,
        m_scene_draw_buffer,
        m_scene_object_buffer,
        m_scene_global_transform_buffer,
        m_scene_material_buffer,
        m_scene_mesh_instance_buffer,
        m_scene_mesh_instance_materials_buffer,
        m_scene_camera_buffer,
        m_scene_vertex_buffer,
        m_offscreen_image,
        m_depth_image,
        m_scene_texture_descriptor
    );

    m_offscreen_to_swapchain_pass.init(m_api, m_offscreen_image, m_offscreen_sampler);

    m_ui_pass.init(m_api, window);
}
void Renderer::init_frames() {
    m_frames.resize(FRAMES_IN_FLIGHT);

    for(u32 i{}; i < FRAMES_IN_FLIGHT; ++i) {
        m_frames[i] = Frame{
            .command_list = m_api.m_command_manager->create_command_list(QueueFamily::Graphics),
            .present_semaphore = m_api.m_command_manager->create_semaphore(),
            .render_semaphore = m_api.m_command_manager->create_semaphore(),
            .fence = m_api.m_command_manager->create_fence(),
            .upload_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
                .size = PER_FRAME_UPLOAD_BUFFER_SIZE,
                .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
            })
        };

        m_frames[i].upload_ptr = m_api.m_resource_manager->map_buffer(m_frames[i].upload_buffer);
    }
}
void Renderer::init_defaults() {
    u8 default_white_srgb_texture_data[] = { 255, 255, 255, 255 };
    u8 default_grey_unorm_texture_data[] = { 127, 127, 127, 255 };

    m_default_white_srgb_texture = create_u8_texture(TextureCreateInfo{
        .pixel_data = default_white_srgb_texture_data,
        .width = 1U,
        .height = 1U,
        .bytes_per_pixel = 4U,
        .is_srgb = true,
        .gen_mip_maps = false,
        .linear_filter = false
    });
    m_default_grey_unorm_texture = create_u8_texture(TextureCreateInfo{
        .pixel_data = default_grey_unorm_texture_data,
        .width = 1U,
        .height = 1U,
        .bytes_per_pixel = 4U,
        .is_srgb = false,
        .gen_mip_maps = false,
        .linear_filter = false
    });
    m_default_material = create_material(MaterialCreateInfo {});
}

void Renderer::resize_passes(const Window &window) {
    m_draw_call_gen_pass.resize(m_api);

    m_geometry_pass.resize(
        m_api,
        m_offscreen_image,
        m_depth_image
    );

    m_offscreen_to_swapchain_pass.resize(m_api, m_offscreen_image, m_offscreen_sampler);

    m_ui_pass.resize(m_api, window);
}

void Renderer::destroy_scene_buffers() {
    m_api.m_resource_manager->destroy_buffer(m_scene_material_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_draw_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_draw_count_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_mesh_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_mesh_instance_materials_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_mesh_instance_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_primitive_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_object_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_global_transform_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_camera_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_vertex_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_index_buffer);
}
void Renderer::destroy_screen_images() {
    m_api.m_resource_manager->destroy_image(m_offscreen_image);
    m_api.m_resource_manager->destroy_image(m_depth_image);
    m_api.m_resource_manager->destroy_sampler(m_offscreen_sampler);
}
void Renderer::destroy_descriptors() {
    m_api.m_resource_manager->destroy_descriptor(m_scene_texture_descriptor);
}
void Renderer::destroy_passes() {
    m_draw_call_gen_pass.destroy(m_api);
    m_geometry_pass.destroy(m_api);
    m_ui_pass.destroy(m_api);
    m_offscreen_to_swapchain_pass.destroy(m_api);
}
void Renderer::destroy_frames() {
    for(const auto& frame : m_frames) {
        m_api.m_command_manager->destroy_command_list(frame.command_list);
        m_api.m_command_manager->destroy_semaphore(frame.present_semaphore);
        m_api.m_command_manager->destroy_semaphore(frame.render_semaphore);
        m_api.m_command_manager->destroy_fence(frame.fence);
        m_api.m_resource_manager->unmap_buffer(frame.upload_buffer);
        m_api.m_resource_manager->destroy_buffer(frame.upload_buffer);
    }

    m_frames.clear();
}
void Renderer::destroy_defaults() {
    destroy_texture(m_default_white_srgb_texture);
    destroy_texture(m_default_grey_unorm_texture);
    destroy_material(m_default_material);
}

void Renderer::set_config_global_lod_bias(float value) {
    m_config_global_lod_bias = value;
}
void Renderer::set_config_global_cull_dist_multiplier(float value) {
    m_config_global_cull_dist_multiplier = std::max(value, 0.0f);
}
void Renderer::set_config_enable_dynamic_lod(bool enable) {
    m_config_enable_dynamic_lod = enable;
    reload_pipelines();
}
void Renderer::set_config_enable_frustum_cull(bool enable) {
    m_config_enable_frustum_cull = enable;
    reload_pipelines();
}
