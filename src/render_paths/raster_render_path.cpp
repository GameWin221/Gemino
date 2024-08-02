#include "raster_render_path.hpp"

RasterRenderPath::RasterRenderPath(Window &window, VSyncMode v_sync) : m_api(window, SwapchainConfig{
    .v_sync = v_sync,
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
}) {
    init_debug_info();
    init_scene_buffers();
    init_screen_images(window);
    init_descriptors(true);
    init_pipelines();
    init_render_targets();
    init_frames();
    init_defaults();
}
RasterRenderPath::~RasterRenderPath() {
    m_api.wait_for_device_idle();

    destroy_defaults();
    destroy_frames();
    destroy_render_targets();
    destroy_pipelines();
    destroy_descriptors(true);
    destroy_screen_images();
    destroy_scene_buffers();
}

void RasterRenderPath::init_debug_info() {
    DEBUG_LOG("\nVulkan Device memory usage:")
    DEBUG_LOG("MAX_SCENE_VERTICES = " << MAX_SCENE_VERTICES << ", " << (MAX_SCENE_VERTICES * sizeof(Vertex) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_INDICES = " << MAX_SCENE_INDICES << ", " << (MAX_SCENE_INDICES * sizeof(u32) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_TEXTURES = " << MAX_SCENE_TEXTURES << ", " << ((MAX_SCENE_TEXTURES * sizeof(VkDeviceSize) * 2) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_MATERIALS = " << MAX_SCENE_MATERIALS << ", " << (MAX_SCENE_MATERIALS * sizeof(Material) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_MESHES = " << MAX_SCENE_MESHES << ", " << (MAX_SCENE_MESHES * sizeof(Mesh) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_DRAWS = " << MAX_SCENE_DRAWS << ", " << (MAX_SCENE_DRAWS * sizeof(VkDrawIndexedIndirectCommand) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_DRAW_INDICES = " << MAX_SCENE_DRAWS << ", " << (MAX_SCENE_DRAWS * sizeof(u32) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_OBJECTS = " << MAX_SCENE_OBJECTS << ", " << (MAX_SCENE_OBJECTS * sizeof(Object) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("OVERALL_DEVICE_MEMORY_USAGE = " << OVERALL_DEVICE_MEMORY_USAGE / 1024.0 / 1024.0 << "mb")

    DEBUG_LOG("\nVulkan Host memory usage:")
    DEBUG_LOG("PER_FRAME_UPLOAD_BUFFER_SIZE * frames_in_flight = " << PER_FRAME_UPLOAD_BUFFER_SIZE * FRAMES_IN_FLIGHT / 1024.0 / 1024.0 << "mb")
    DEBUG_LOG("OVERALL_HOST_MEMORY_USAGE = " << OVERALL_HOST_MEMORY_USAGE / 1024.0 / 1024.0 << "mb")
}
void RasterRenderPath::init_scene_buffers() {
    m_scene_material_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Material) * MAX_SCENE_MATERIALS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_draw_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(VkDrawIndexedIndirectCommand) * MAX_SCENE_DRAWS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_draw_index_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(u32) * MAX_SCENE_DRAWS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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
    m_scene_object_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Object) * MAX_SCENE_OBJECTS,
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
        .buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    m_scene_index_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(u32) * MAX_SCENE_INDICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
}
void RasterRenderPath::init_screen_images(const Window &window) {
    VkExtent3D screen_size = VkExtent3D{ window.get_size().x, window.get_size().y };

    m_offscreen_rt_image = m_api.m_resource_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = screen_size,
        .usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT
    });
    m_offscreen_rt_sampler = m_api.m_resource_manager->create_sampler(SamplerCreateInfo{
        .filter = VK_FILTER_NEAREST
    });

    m_depth_image = m_api.m_resource_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = screen_size,
        .usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT
    });

    std::vector<ImageBarrier> init_swapchain_barriers{};
    for(u32 i{}; i < m_api.get_swapchain_index_count(); ++i){
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
            .image_handle = m_offscreen_rt_image,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});
    });
}
void RasterRenderPath::init_descriptors(bool create_new) {
    if (create_new) {
        m_draw_call_gen_descriptor = m_api.m_resource_manager->create_descriptor(DescriptorCreateInfo{
            .bindings {
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Mesh Buffer
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Object Buffer
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Draw Commands
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Draw Commands Count
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Draw Commands Index
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, // Camera Buffer
            }
        });
    }

    m_api.m_resource_manager->update_descriptor(m_draw_call_gen_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .buffer_info {
                    .buffer_handle = m_scene_mesh_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = m_scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = m_scene_draw_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 3U,
                .buffer_info {
                    .buffer_handle = m_scene_draw_count_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 4U,
                .buffer_info {
                    .buffer_handle = m_scene_draw_index_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 5U,
                .buffer_info {
                    .buffer_handle = m_scene_camera_buffer
                },
            }
        }
    });

    if (create_new) {
        m_forward_descriptor = m_api.m_resource_manager->create_descriptor(DescriptorCreateInfo{
            .bindings{
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,static_cast<u32>(MAX_SCENE_TEXTURES)},
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Scene Object Buffer
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Scene Material Buffer
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, // Scene Draw Index Buffer
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, // Scene Camera Buffer
            }
        });
    }
    m_api.m_resource_manager->update_descriptor(m_forward_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = m_scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = m_scene_material_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 3U,
                .buffer_info {
                    .buffer_handle = m_scene_draw_index_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 4U,
                .buffer_info {
                    .buffer_handle = m_scene_camera_buffer
                }
            }
        }
    });

    if (create_new) {
        m_offscreen_rt_to_swapchain_descriptor = m_api.m_resource_manager->create_descriptor(DescriptorCreateInfo{
            .bindings {
                DescriptorBindingCreateInfo{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
            }
        });
    }
    m_api.m_resource_manager->update_descriptor(m_offscreen_rt_to_swapchain_descriptor, DescriptorUpdateInfo{
        .bindings {
            DescriptorBindingUpdateInfo {
                .image_info {
                    .image_handle = m_offscreen_rt_image,
                    .image_sampler = m_offscreen_rt_sampler
                }
            }
        }
    });
}
void RasterRenderPath::init_pipelines() {
    m_draw_call_gen_pipeline = m_api.m_pipeline_manager->create_compute_pipeline(ComputePipelineCreateInfo{
        .shader_path = "res/shaders/draw_call_gen.comp.spv",
        .shader_constant_values {
            static_cast<uint32_t>(m_config_enable_dynamic_lod),
            static_cast<uint32_t>(m_config_enable_frustum_cull),
            static_cast<uint32_t>(m_api.m_instance->get_physical_device_preferred_warp_size()),
        },
        .push_constants_size = sizeof(DrawCallGenPC),
        .descriptors { m_draw_call_gen_descriptor }
    });

    m_forward_pipeline = m_api.m_pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./res/shaders/forward.vert.spv",
        .fragment_shader_path = "./res/shaders/forward.frag.spv",

        //.push_constants_size = sizeof(PushConstants),
        .descriptors { m_forward_descriptor },

        .color_target {
            .format = m_api.m_resource_manager->get_image_data(m_offscreen_rt_image).format,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        .depth_target {
            .format = m_api.m_resource_manager->get_image_data(m_depth_image).format,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },

        .enable_depth_test = true,
        .enable_depth_write = true,

        .depth_compare_op = VK_COMPARE_OP_GREATER,

        .enable_vertex_input = true,
    });

    m_offscreen_rt_to_swapchain_pipeline = m_api.m_pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "res/shaders/fullscreen_tri.vert.spv",
        .fragment_shader_path = "res/shaders/fullscreen_tri.frag.spv",
        .descriptors = { m_offscreen_rt_to_swapchain_descriptor } ,
        .color_target {
            .format = m_api.m_resource_manager->get_image_data(m_api.get_swapchain_image_handle(0u)).format,
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        .cull_mode = VK_CULL_MODE_NONE,
    });
}
void RasterRenderPath::init_render_targets() {
    m_offscreen_rt = m_api.m_pipeline_manager->create_render_target(m_forward_pipeline, RenderTargetCreateInfo{
        .color_target_handle = m_offscreen_rt_image,
        .depth_target_handle = m_depth_image
    });

    m_offscreen_rt_to_swapchain_targets.resize(m_api.get_swapchain_index_count());
    for(u32 i{}; i < m_api.get_swapchain_index_count(); ++i){
        m_offscreen_rt_to_swapchain_targets[i] = m_api.m_pipeline_manager->create_render_target(m_offscreen_rt_to_swapchain_pipeline, RenderTargetCreateInfo{
            .color_target_handle = m_api.get_swapchain_image_handle(i)
        });
    }
}
void RasterRenderPath::init_frames() {
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
void RasterRenderPath::init_defaults() {
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
}

void RasterRenderPath::destroy_scene_buffers() {
    m_api.m_resource_manager->destroy_buffer(m_scene_material_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_draw_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_draw_index_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_draw_count_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_mesh_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_object_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_camera_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_vertex_buffer);
    m_api.m_resource_manager->destroy_buffer(m_scene_index_buffer);
}
void RasterRenderPath::destroy_screen_images() {
    m_api.m_resource_manager->destroy_image(m_offscreen_rt_image);
    m_api.m_resource_manager->destroy_image(m_depth_image);

    m_api.m_resource_manager->destroy_sampler(m_offscreen_rt_sampler);
}
void RasterRenderPath::destroy_descriptors(bool create_new) {
    if(create_new) {
        m_api.m_resource_manager->destroy_descriptor(m_draw_call_gen_descriptor);
        m_api.m_resource_manager->destroy_descriptor(m_forward_descriptor);
        m_api.m_resource_manager->destroy_descriptor(m_offscreen_rt_to_swapchain_descriptor);
    }
}
void RasterRenderPath::destroy_pipelines() {
    m_api.m_pipeline_manager->destroy_compute_pipeline(m_draw_call_gen_pipeline);

    m_api.m_pipeline_manager->destroy_graphics_pipeline(m_forward_pipeline);
    m_api.m_pipeline_manager->destroy_graphics_pipeline(m_offscreen_rt_to_swapchain_pipeline);
}
void RasterRenderPath::destroy_render_targets() {
    m_api.m_pipeline_manager->destroy_render_target(m_offscreen_rt);

    for(const auto& handle : m_offscreen_rt_to_swapchain_targets) {
        m_api.m_pipeline_manager->destroy_render_target(handle);
    }

    m_offscreen_rt_to_swapchain_targets.clear();
}
void RasterRenderPath::destroy_frames() {
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
void RasterRenderPath::destroy_defaults() {
    m_api.m_resource_manager->destroy_image(m_default_white_srgb_texture);
    m_api.m_resource_manager->destroy_image(m_default_grey_unorm_texture);
}

void RasterRenderPath::resize(const Window &window) {
    m_api.wait_for_device_idle();

    m_frames_since_init = 0;

    for(const auto& frame : m_frames) {
        m_api.reset_commands(frame.command_list);
    }

    destroy_render_targets();
    destroy_descriptors(false);
    destroy_screen_images();

    m_api.recreate_swapchain(window.get_size(), m_api.get_swapchain_config());

    init_screen_images(window);
    init_descriptors(false);
    init_render_targets();

    m_api.wait_for_device_idle();
}
void RasterRenderPath::render(World &world, Handle<Camera> camera) {
    if (m_reload_pipelines_queued) {
        DEBUG_LOG("Reloading pipelines...")

        for (const auto &f : m_frames) {
            m_api.wait_for_fence(f.fence);
        }

        m_api.wait_for_device_idle();

        destroy_render_targets();
        destroy_pipelines();

        init_pipelines();
        init_render_targets();

        DEBUG_LOG("Reloaded pipelines!")
        m_reload_pipelines_queued = false;
        return;
    }

    begin_recording_frame();
    update_world(world, camera);
    render_world(world, camera);
    end_recording_frame();
}
void RasterRenderPath::reload_pipelines() {
    m_reload_pipelines_queued = true;
}

void RasterRenderPath::begin_recording_frame() {
    Frame &frame = m_frames[m_frame_in_flight_index];

    m_api.wait_for_fence(frame.fence);

    VkResult result = m_api.get_next_swapchain_index(frame.present_semaphore, &m_swapchain_target_index);
    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        DEBUG_ERROR("VK_ERROR_OUT_OF_DATE_KHR") // This message shouldn't be ever visible because a resize always occurs before rendering
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        DEBUG_PANIC("Failed to acquire a swapchain image!")
    }

    m_api.reset_fence(frame.fence);
    m_api.reset_commands(frame.command_list);
    m_api.begin_recording_commands(frame.command_list);
}
void RasterRenderPath::update_world(World &world, Handle<Camera> camera) {
    Frame &frame = m_frames[m_frame_in_flight_index];

    std::vector<VkBufferCopy> object_copy_regions{};
    std::vector<VkBufferCopy> camera_copy_regions{};

    object_copy_regions.reserve(world.get_changed_object_handles().size());
    //camera_copy_regions.reserve(world.get_changed_camera_handles().size());

    usize upload_buffer_size = m_api.m_resource_manager->get_buffer_data(frame.upload_buffer).size;

    usize upload_offset{};
    for(const auto& handle : world.get_changed_object_handles()) {
        if(upload_offset + sizeof(Object) >= upload_buffer_size) {
            DEBUG_PANIC("UPLOAD OVERFLOW (Object)")
        }
        if(handle >= MAX_SCENE_OBJECTS) {
            DEBUG_PANIC("BUFFER OVERFLOW (Object)")
        }

        *frame.access_upload<Object>(upload_offset) = world.get_object(handle);

        object_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(handle) * sizeof(Object),
            .size = sizeof(Object)
        });

        upload_offset += sizeof(Object);
    }

    if(upload_offset + sizeof(Camera) >= upload_buffer_size) {
        DEBUG_PANIC("UPLOAD OVERFLOW (Camera)")
    }

    if(camera != INVALID_HANDLE) {
        *frame.access_upload<Camera>(upload_offset) = world.get_camera(camera);

        camera_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(camera) * sizeof(Camera),
            .size = sizeof(Camera)
        });

        upload_offset += sizeof(Camera);
    }

    m_api.m_resource_manager->flush_mapped_buffer(frame.upload_buffer, upload_offset);

    world._clear_updates();

    // Ensure that other m_frames don't use these global buffers
    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_object_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = m_scene_camera_buffer,
            .src_access_mask = VK_ACCESS_UNIFORM_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
    });

    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_scene_object_buffer, object_copy_regions);
    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_scene_camera_buffer, camera_copy_regions);


    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_object_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = m_scene_camera_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_UNIFORM_READ_BIT
        },
    });
}
void RasterRenderPath::render_world(const World &world, Handle<Camera> camera) {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    u32 scene_objects_count = static_cast<u32>(world.get_objects().size());

    DrawCallGenPC draw_call_gen_pc{
        .draw_count_pre_cull = scene_objects_count,
        .global_lod_bias = m_config_global_lod_bias,
        .global_cull_dist_multiplier = m_config_global_cull_dist_multiplier
    };

    render_pass_draw_call_gen(scene_objects_count, draw_call_gen_pc);
    render_pass_geometry(scene_objects_count);

    render_pass_offscreen_rt_to_swapchain();
}
void RasterRenderPath::end_recording_frame() {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    SubmitInfo submit{
        .fence = frame.fence,
        .wait_semaphores = { frame.present_semaphore },
        .signal_semaphores{ frame.render_semaphore },
        .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
    };

    m_api.end_recording_commands(frame.command_list);
    m_api.submit_commands(frame.command_list, submit);

    if (m_api.present_swapchain(frame.render_semaphore, m_swapchain_target_index) != VK_SUCCESS) {
        DEBUG_PANIC("Failed to present swap chain image!")
    }

    m_frame_in_flight_index = (m_frame_in_flight_index + 1) % FRAMES_IN_FLIGHT;
    m_frames_since_init += 1;
}

void RasterRenderPath::render_pass_draw_call_gen(u32 scene_objects_count, const DrawCallGenPC &draw_call_gen_pc) {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    m_api.fill_buffer(frame.command_list, m_scene_draw_count_buffer, 0U, sizeof(u32));

    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_draw_count_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        }
    });

    m_api.begin_compute_pipeline(frame.command_list, m_draw_call_gen_pipeline);
    m_api.bind_compute_descriptor(frame.command_list, m_draw_call_gen_pipeline, m_draw_call_gen_descriptor, 0U);
    m_api.push_compute_constants(frame.command_list, m_draw_call_gen_pipeline, &draw_call_gen_pc);
    m_api.dispatch_compute_pipeline(frame.command_list, glm::uvec3(Utils::div_ceil(scene_objects_count, m_api.m_instance->get_physical_device_preferred_warp_size()), 1u, 1u));

    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_draw_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        },
        BufferBarrier{
            .buffer_handle = m_scene_draw_index_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        },
        BufferBarrier{
            .buffer_handle = m_scene_draw_count_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        },
    });
}
void RasterRenderPath::render_pass_geometry(u32 scene_objects_count) {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    m_api.image_barrier(frame.command_list, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {
        ImageBarrier{
            .image_handle = m_offscreen_rt_image,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }
    });

    m_api.begin_graphics_pipeline(frame.command_list, m_forward_pipeline, m_offscreen_rt, RenderTargetClear{
        .color = {0.0f, 0.0f, 0.0f, 1.0f},
        .depth = 0.0f
    });

    m_api.bind_graphics_descriptor(frame.command_list, m_forward_pipeline, m_forward_descriptor, 0U);
    m_api.bind_vertex_buffer(frame.command_list, m_scene_vertex_buffer);
    m_api.bind_index_buffer(frame.command_list, m_scene_index_buffer);
    m_api.draw_indexed_indirect_count(frame.command_list,m_scene_draw_buffer, m_scene_draw_count_buffer,scene_objects_count, sizeof(VkDrawIndexedIndirectCommand));

    m_api.end_graphics_pipeline(frame.command_list, m_forward_pipeline);
}
void RasterRenderPath::render_pass_offscreen_rt_to_swapchain() {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    m_api.image_barrier(frame.command_list, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { ImageBarrier{
        .image_handle = m_offscreen_rt_image,
        .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }});

    m_api.begin_graphics_pipeline(frame.command_list, m_offscreen_rt_to_swapchain_pipeline, m_offscreen_rt_to_swapchain_targets[m_swapchain_target_index], RenderTargetClear{});
    m_api.bind_graphics_descriptor(frame.command_list, m_offscreen_rt_to_swapchain_pipeline, m_offscreen_rt_to_swapchain_descriptor, 0U);
    m_api.draw_count(frame.command_list, 3U),
    m_api.end_graphics_pipeline(frame.command_list, m_offscreen_rt_to_swapchain_pipeline);
}

Handle<Mesh> RasterRenderPath::create_mesh(const MeshCreateInfo &create_info) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Mesh mesh{
        .center_offset = glm::vec3{},
        .radius = 0.0f,
        .cull_distance = create_info.cull_distance,
        .lod_bias = create_info.lod_bias,
        .lod_count = static_cast<u32>(create_info.lods.size())
    };

    if(mesh.lod_count == 0U) {
        DEBUG_PANIC("Mesh must use more than 0 LODs!")
    }
    if(mesh.lod_count > 8U) {
        DEBUG_PANIC("Mesh cannot use more than 8 LODs! lod_count = " << mesh.lod_count)
    }

    for(u32 lod_id{}; lod_id < mesh.lod_count; ++lod_id) {
        const auto &lod = create_info.lods[lod_id];

        mesh.lods[lod_id] = MeshLOD {
            .index_count = lod.index_count,
            .first_index = m_allocated_indices,
            .vertex_offset = static_cast<i32>(m_allocated_vertices)
        };

        // Pick the biggest bounding sphere out of all LODs
        if (lod.radius > mesh.radius) {
            mesh.radius = lod.radius;
            mesh.center_offset = lod.center_offset;
        }

        Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
            .size = (lod.vertex_count * sizeof(Vertex)) + (lod.index_count * sizeof(u32)) + sizeof(MeshLOD),
            .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
        });

        VkDeviceSize upload_offset{};

        m_api.record_and_submit_once([this, &upload_offset, &lod, staging_buffer](Handle<CommandList> cmd) {
            // Vertices
            m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, lod.vertex_data, lod.vertex_count * sizeof(Vertex), upload_offset);
            m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_vertex_buffer, { VkBufferCopy{
                .srcOffset = upload_offset,
                .dstOffset = m_allocated_vertices * sizeof(Vertex),
                .size = lod.vertex_count * sizeof(Vertex)
            }});

            upload_offset += lod.vertex_count * sizeof(Vertex);

            // Indices
            m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, lod.index_data, lod.index_count * sizeof(u32), upload_offset);
            m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_index_buffer, { VkBufferCopy{
                .srcOffset = upload_offset,
                .dstOffset = m_allocated_indices * sizeof(u32),
                .size = lod.index_count * sizeof(u32)
            }});

            upload_offset += lod.index_count * sizeof(u32);
        });

        m_api.m_resource_manager->destroy_buffer(staging_buffer);

        m_allocated_vertices += lod.vertex_count;
        m_allocated_indices += lod.index_count;
    }

    Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Mesh),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, &mesh, sizeof(Mesh));

    Handle<Mesh> mesh_handle = m_mesh_allocator.alloc(mesh);

    m_api.record_and_submit_once([this, mesh_handle, staging_buffer](Handle<CommandList> cmd) {
        m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_mesh_buffer, {VkBufferCopy{
            .dstOffset = mesh_handle * sizeof(Mesh),
            .size = sizeof(Mesh)
        }});
    });

    m_api.m_resource_manager->destroy_buffer(staging_buffer);

    return mesh_handle;
}
void RasterRenderPath::destroy_mesh(Handle<Mesh> mesh_handle) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    if(!m_mesh_allocator.is_handle_valid(mesh_handle)) {
        DEBUG_PANIC("Cannot delete mesh - Mesh with a handle id: = " << mesh_handle << ", does not exist!")
    }

    const Mesh &mesh = m_mesh_allocator.get_element(mesh_handle);

    m_mesh_allocator.free(mesh_handle);
}

Handle<Texture> RasterRenderPath::create_u8_texture(const TextureCreateInfo &create_info) {
    if(!create_info.pixel_data) {
        DEBUG_PANIC("create_u8_texture failed! | create_info.pixel_data cannot be nullptr!")
    }

    if(create_info.width == 0U) {
        DEBUG_PANIC("create_u8_texture failed! | create_info.width must be greater than 0!")
    }
    if(create_info.height == 0U) {
        DEBUG_PANIC("create_u8_texture failed! | create_info.height must be greater than 0!")
    }

    VkFormat format{};
    if(create_info.bytes_per_pixel == 1U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM);
    } else if(create_info.bytes_per_pixel == 2U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM);
    } else if(create_info.bytes_per_pixel == 3U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM);
    } else if(create_info.bytes_per_pixel == 4U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM);
    } else {
        DEBUG_PANIC("create_u8_texture failed! | create_info.bytes_per_pixel must be between 1 and 4, bytes_per_pixel=" << create_info.bytes_per_pixel)
    }

    DEBUG_ASSERT(m_config_texture_anisotropy <= 16U)

    Texture texture{
        .width = create_info.width,
        .height = create_info.height,
        .bytes_per_pixel = create_info.bytes_per_pixel,
        .mip_level_count = (create_info.gen_mip_maps ? Utils::calculate_mipmap_levels_xy(create_info.width, create_info.height) : 1U),
        .is_srgb = static_cast<u32>(create_info.is_srgb),
        .use_linear_filter = static_cast<u32>(create_info.linear_filter)
    };

    texture.image = m_api.m_resource_manager->create_image(ImageCreateInfo{
        .format = format,
        .extent {
            .width = texture.width,
            .height = texture.height
        },
        .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (create_info.gen_mip_maps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : static_cast<VkImageUsageFlags>(0)),
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        .mip_level_count = texture.mip_level_count
    });

    texture.sampler = m_api.m_resource_manager->create_sampler(SamplerCreateInfo{
        .filter = create_info.linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmap_mode = create_info.linear_filter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,

        .max_mipmap = static_cast<f32>(texture.mip_level_count),
        .mipmap_bias = m_config_texture_mip_bias,
        .anisotropy = static_cast<f32>(m_config_texture_anisotropy)
    });

    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = create_info.width * create_info.height * create_info.bytes_per_pixel,
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, create_info.pixel_data, create_info.width * create_info.height * create_info.bytes_per_pixel);
    m_api.record_and_submit_once([this, &create_info, &texture, staging_buffer](Handle<CommandList> cmd) {
        m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {ImageBarrier{
            .image_handle = texture.image,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        }});

        m_api.copy_buffer_to_image(cmd, staging_buffer, texture.image, {BufferToImageCopy{}});

        if (create_info.gen_mip_maps) {
            m_api.gen_mipmaps(cmd, texture.image, create_info.linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT
            );
        } else {
            m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {ImageBarrier{
                .image_handle = texture.image,
                .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }});
        }
    });
    m_api.m_resource_manager->destroy_buffer(staging_buffer);

    auto handle = m_texture_allocator.alloc(texture);

    m_api.m_resource_manager->update_descriptor(m_forward_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .array_index = static_cast<u32>(handle),
                .image_info {
                    .image_handle = texture.image,
                    .image_sampler = texture.sampler
                }
            }
        }
    });

    return handle;
}
void RasterRenderPath::destroy_texture(Handle<Texture> texture_handle) {
    if(!m_texture_allocator.is_handle_valid(texture_handle)) {
        DEBUG_PANIC("Cannot delete texture - Texture with a handle id: = " << texture_handle << ", does not exist!")
    }

    const Texture &texture = m_texture_allocator.get_element(texture_handle);
    m_api.m_resource_manager->destroy_image(texture.image);
    m_api.m_resource_manager->destroy_sampler(texture.sampler);

    m_texture_allocator.free(texture_handle);
}

Handle<Material> RasterRenderPath::create_material(const MaterialCreateInfo &create_info) {
    Material material{
        .albedo_texture = (create_info.albedo_texture == INVALID_HANDLE) ? m_default_white_srgb_texture : create_info.albedo_texture,
        .roughness_texture = (create_info.roughness_texture == INVALID_HANDLE) ? m_default_grey_unorm_texture : create_info.roughness_texture,
        .metalness_texture = (create_info.metalness_texture == INVALID_HANDLE) ? m_default_grey_unorm_texture : create_info.metalness_texture,
        .normal_texture = (create_info.normal_texture == INVALID_HANDLE) ? m_default_grey_unorm_texture : create_info.normal_texture,
        .color = create_info.color
    };

    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Material),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, &material, sizeof(Material));

    auto handle = m_material_allocator.alloc(material);

    m_api.record_and_submit_once([this, handle, staging_buffer](Handle<CommandList> cmd){
        m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_material_buffer, {
            VkBufferCopy {
                .dstOffset = handle * sizeof(Material),
                .size = sizeof(Material),
            }
        });
    });

    m_api.m_resource_manager->destroy_buffer(staging_buffer);

    return handle;
}
void RasterRenderPath::destroy_material(Handle<Material> material_handle) {
    if(!m_material_allocator.is_handle_valid(material_handle)) {
        DEBUG_PANIC("Cannot delete material - Material with a handle id: = " << material_handle << ", does not exist!")
    }

    const Material& material = m_material_allocator.get_element(material_handle);

    m_material_allocator.free(material_handle);
}

void RasterRenderPath::set_config_global_lod_bias(float value) {
    m_config_global_lod_bias = value;
}
void RasterRenderPath::set_config_global_cull_dist_multiplier(float value) {
    m_config_global_cull_dist_multiplier = std::max(value, 0.0f);
}
void RasterRenderPath::set_config_enable_dynamic_lod(bool enable) {
    m_config_enable_dynamic_lod = enable;
    reload_pipelines();
}
void RasterRenderPath::set_config_enable_frustum_cull(bool enable) {
    m_config_enable_frustum_cull = enable;
    reload_pipelines();
}
