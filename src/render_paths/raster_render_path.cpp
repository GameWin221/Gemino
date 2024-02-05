#include "raster_render_path.hpp"
#include <common/utils.hpp>

RasterRenderPath::RasterRenderPath(Window& window, VSyncMode v_sync) : renderer(window, SwapchainConfig{
    .v_sync = v_sync,
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
}) {
    scene_material_buffer  = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Material) * MAX_SCENE_MATERIALS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    scene_draw_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
       .size = sizeof(VkDrawIndexedIndirectCommand) * MAX_SCENE_DRAWS,
       .buffer_usage_flags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
       .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    scene_object_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Object) * MAX_SCENE_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    scene_camera_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Camera),
        .buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    scene_vertex_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Vertex) * MAX_SCENE_VERTICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    scene_index_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(u32) * MAX_SCENE_INDICES,
        .buffer_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    DEBUG_LOG("\nVulkan Device memory usage:")
    DEBUG_LOG("MAX_SCENE_TEXTURES = " << MAX_SCENE_TEXTURES << ", " << (MAX_SCENE_TEXTURES * sizeof(Material) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_MATERIALS = " << MAX_SCENE_MATERIALS << ", " << (MAX_SCENE_MATERIALS * sizeof(Material) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_VERTICES = " << MAX_SCENE_VERTICES << ", " << (MAX_SCENE_VERTICES * sizeof(Vertex) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_INDICES = " << MAX_SCENE_INDICES << ", " << (MAX_SCENE_INDICES * sizeof(u32) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_DRAWS = " << MAX_SCENE_DRAWS << ", " << (MAX_SCENE_DRAWS * sizeof(VkDrawIndexedIndirectCommand) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("MAX_SCENE_OBJECTS = " << MAX_SCENE_OBJECTS << ", " << (MAX_SCENE_OBJECTS * sizeof(Object) / 1024.0 / 1024.0) << "mb")
    DEBUG_LOG("OVERALL_DEVICE_MEMORY_USAGE = " << OVERALL_DEVICE_MEMORY_USAGE / 1024.0 / 1024.0 << "mb")

    VkDeviceSize overall_host_memory_usage{};
    overall_host_memory_usage += PER_FRAME_UPLOAD_BUFFER_SIZE * frames_in_flight;

    DEBUG_LOG("\nVulkan Host memory usage:")
    DEBUG_LOG("PER_FRAME_UPLOAD_BUFFER_SIZE * frames_in_flight = " << PER_FRAME_UPLOAD_BUFFER_SIZE * frames_in_flight / 1024.0 / 1024.0 << "mb")
    DEBUG_LOG("OVERALL_HOST_MEMORY_USAGE = " << overall_host_memory_usage / 1024.0 / 1024.0 << "mb")

    forward_descriptor = renderer.resource_manager->create_descriptor(DescriptorCreateInfo{
        .bindings{
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<u32>(MAX_SCENE_TEXTURES) },
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        }
    });
    renderer.resource_manager->update_descriptor(forward_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = scene_object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = scene_material_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 3U,
                .buffer_info {
                    .buffer_handle = scene_camera_buffer
                }
            }
        }
    });

    u8 default_white_srgb_texture_data[] = { 255, 255, 255, 255 };
    u8 default_grey_unorm_texture_data[] = { 127, 127, 127, 255 };

    default_white_srgb_texture = create_u8_texture(TextureCreateInfo{
        .pixel_data = default_white_srgb_texture_data,
        .width = 1U,
        .height = 1U,
        .bytes_per_pixel = 4U,
        .is_srgb = true,
        .gen_mip_maps = false,
        .linear_filter = false
    });
    default_grey_unorm_texture = create_u8_texture(TextureCreateInfo{
        .pixel_data = default_grey_unorm_texture_data,
        .width = 1U,
        .height = 1U,
        .bytes_per_pixel = 4U,
        .is_srgb = false,
        .gen_mip_maps = false,
        .linear_filter = false
    });

    depth_image = renderer.resource_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = VkExtent3D{ window.get_size().x, window.get_size().y },
        .usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT
    });

    std::vector<ImageBarrier> init_swapchain_barriers{};
    for(u32 i{}; i < renderer.get_swapchain_index_count(); ++i){
        init_swapchain_barriers.push_back(ImageBarrier{
            .image_handle = renderer.get_swapchain_image_handle(i),
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
            .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        });
    }

    auto init_cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(init_cmd);
    renderer.image_barrier(init_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT , init_swapchain_barriers);
    renderer.image_barrier(init_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, { ImageBarrier{
        .image_handle = depth_image,
        .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
        .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    }});
    renderer.end_recording_commands(init_cmd);
    renderer.submit_commands_once(init_cmd);

    forward_pipeline = renderer.pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./res/shaders/forward.vert.spv",
        .fragment_shader_path = "./res/shaders/forward.frag.spv",

        //.push_constants_size = sizeof(PushConstants),
        .descriptors { forward_descriptor },

        .color_target {
            .format = renderer.swapchain->get_format(),
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        .depth_target {
            .format = renderer.resource_manager->get_image_data(depth_image).format,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },

        .enable_depth_test = true,
        .enable_depth_write = true,

        //.depth_compare_op = VK_COMPARE_OP_GREATER,

        .enable_vertex_input = true,
    });

    for(u32 i{}; i < frames_in_flight; ++i) {
        Frame frame{
            .command_list = renderer.command_manager->create_command_list(QueueFamily::Graphics),
            .present_semaphore = renderer.command_manager->create_semaphore(),
            .render_semaphore = renderer.command_manager->create_semaphore(),
            .fence = renderer.command_manager->create_fence(),
            .upload_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
                .size = PER_FRAME_UPLOAD_BUFFER_SIZE,
                .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
            })
        };

        frame.upload_ptr = renderer.resource_manager->map_buffer(frame.upload_buffer);

        frames.push_back(frame);
    }

    for(u32 i{}; i < renderer.swapchain->get_images().size(); ++i) {
        swapchain_targets.push_back(renderer.pipeline_manager->create_render_target(forward_pipeline, RenderTargetCreateInfo{
            .color_target_handle = renderer.get_swapchain_image_handle(i),
            .depth_target_handle = depth_image
        }));
    }
}
RasterRenderPath::~RasterRenderPath() {
    renderer.wait_for_device_idle();

    for(const auto& frame : frames) {
        renderer.resource_manager->unmap_buffer(frame.upload_buffer);
    }
}
void RasterRenderPath::resize(const Window &window) {
    renderer.wait_for_device_idle();

    frames_since_init = 0;

    renderer.recreate_swapchain(window.get_size(), renderer.get_swapchain_config());
    for(const auto& frame : frames) {
        renderer.reset_commands(frame.command_list);
    }

    renderer.resource_manager->destroy_image(depth_image);
    depth_image = renderer.resource_manager->create_image(ImageCreateInfo{
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = VkExtent3D{ window.get_size().x, window.get_size().y },
            .usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT
    });

    for(u32 i{}; i < swapchain_targets.size(); ++i) {
        renderer.pipeline_manager->destroy_render_target(swapchain_targets[i]);

        swapchain_targets[i] = renderer.pipeline_manager->create_render_target(forward_pipeline, RenderTargetCreateInfo{
                .color_target_handle = renderer.get_swapchain_image_handle(i),
                .depth_target_handle = depth_image
        });
    }

    std::vector<ImageBarrier> init_swapchain_barriers{};
    for(u32 i{}; i < renderer.get_swapchain_index_count(); ++i){
        init_swapchain_barriers.push_back(ImageBarrier{
                .image_handle = renderer.get_swapchain_image_handle(i),
                .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
                .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        });
    }

    auto init_cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(init_cmd);
    renderer.image_barrier(init_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT , init_swapchain_barriers);
    renderer.image_barrier(init_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, { ImageBarrier{
            .image_handle = depth_image,
            .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // LoadOp = Clear
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    }});
    renderer.end_recording_commands(init_cmd);
    renderer.submit_commands_once(init_cmd);
}
void RasterRenderPath::render(World& world, Handle<Camera> camera) {
    begin_recording_frame();
    update_world(world, camera);
    render_world(world, camera);
    end_recording_frame();
}
void RasterRenderPath::preload_world(World &world) {

}

void RasterRenderPath::begin_recording_frame() {
    Frame& frame = frames[frame_in_flight_index];

    renderer.wait_for_fence(frame.fence);

    VkResult result = renderer.get_next_swapchain_index(frame.present_semaphore, &swapchain_target_index);
    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        DEBUG_LOG("VK_ERROR_OUT_OF_DATE_KHR") // This message shouldn't be ever visible because a resize always occurs before rendering
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        DEBUG_PANIC("Failed to acquire a swapchain image!")
    }

    renderer.reset_fence(frame.fence);
    renderer.reset_commands(frame.command_list);
    renderer.begin_recording_commands(frame.command_list);
}
void RasterRenderPath::update_world(World& world, Handle<Camera> camera) {
    Frame& frame = frames[frame_in_flight_index];

    std::vector<VkBufferCopy> draw_copy_regions{};
    std::vector<VkBufferCopy> object_copy_regions{};
    std::vector<VkBufferCopy> camera_copy_regions{};

    draw_copy_regions.reserve(world.get_changed_object_handles().size());
    object_copy_regions.reserve(world.get_changed_object_handles().size());
    //camera_copy_regions.reserve(world.get_changed_camera_handles().size());

    usize upload_buffer_size = renderer.resource_manager->get_buffer_data(frame.upload_buffer).size;

    usize upload_offset{};

    for(const auto& handle : world.get_changed_object_handles()) {
        if(upload_offset + sizeof(VkDrawIndexedIndirectCommand) >= upload_buffer_size) {
            DEBUG_PANIC("UPLOAD OVERFLOW (VkDrawIndexedIndirectCommand)")
        }
        if(handle >= MAX_SCENE_DRAWS) {
            DEBUG_PANIC("BUFFER OVERFLOW (VkDrawIndexedIndirectCommand)")
        }

        const auto& object = world.get_object(handle);

        VkDrawIndexedIndirectCommand draw_command{};
        if(object.mesh != INVALID_HANDLE && object.visible) {
            const auto& mesh = mesh_allocator.get_element(object.mesh);
            draw_command.indexCount = mesh.index_count;
            draw_command.instanceCount = 1U;
            draw_command.firstIndex = mesh.first_index;
            draw_command.vertexOffset = mesh.vertex_offset;
        }

        *frame.access_upload<VkDrawIndexedIndirectCommand>(upload_offset) = draw_command;

        draw_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(handle) * sizeof(VkDrawIndexedIndirectCommand),
            .size = sizeof(VkDrawIndexedIndirectCommand)
        });

        upload_offset += Utils::align(16, sizeof(VkDrawIndexedIndirectCommand));
    }

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

    renderer.resource_manager->flush_mapped_buffer(frame.upload_buffer, upload_offset);

    world._clear_updates();

    // Ensure that other frames don't use these global buffers
    renderer.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        BufferBarrier{
            .buffer_handle = scene_draw_buffer,
            .src_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        }
    });
    renderer.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        BufferBarrier{
            .buffer_handle = scene_object_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = scene_camera_buffer,
            .src_access_mask = VK_ACCESS_UNIFORM_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
    });

    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, scene_draw_buffer, draw_copy_regions);
    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, scene_object_buffer, object_copy_regions);
    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, scene_camera_buffer, camera_copy_regions);

    renderer.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, {
        BufferBarrier{
            .buffer_handle = scene_draw_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        }
    });
    renderer.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = scene_object_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = scene_camera_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_UNIFORM_READ_BIT
        },
    });
}
void RasterRenderPath::render_world(const World& world, Handle<Camera> camera) {
    const Frame& frame = frames[frame_in_flight_index];

    renderer.begin_graphics_pipeline(frame.command_list, forward_pipeline, swapchain_targets[swapchain_target_index], RenderTargetClear{
        .color {0.0f, 0.0f, 0.0f, 1.0f},
        .depth = 1.0f//.depth = 0.0f
    });

    renderer.bind_graphics_descriptor(frame.command_list, forward_pipeline, forward_descriptor, 0U);
    renderer.bind_vertex_buffer(frame.command_list, scene_vertex_buffer);
    renderer.bind_index_buffer(frame.command_list, scene_index_buffer);
    renderer.draw_indexed_indirect(frame.command_list,scene_draw_buffer, static_cast<u32>(world.get_objects().size()),sizeof(VkDrawIndexedIndirectCommand));

    renderer.end_graphics_pipeline(frame.command_list, forward_pipeline);
}
void RasterRenderPath::end_recording_frame() {
    const Frame& frame = frames[frame_in_flight_index];

    SubmitInfo submit{
        .fence = frame.fence,
        .wait_semaphores = { frame.present_semaphore },
        .signal_semaphores{ frame.render_semaphore },
        .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
    };

    renderer.end_recording_commands(frame.command_list);
    renderer.submit_commands(frame.command_list, submit);

    if (renderer.present_swapchain(frame.render_semaphore, swapchain_target_index) != VK_SUCCESS) {
        DEBUG_PANIC("Failed to present swap chain image!")
    }

    frame_in_flight_index = (frame_in_flight_index + 1) % frames_in_flight;
    frames_since_init += 1;
}

Handle<Mesh> RasterRenderPath::create_mesh(const MeshCreateInfo& create_info) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Mesh mesh{
        .index_count = create_info.index_count,
        .first_index = allocated_indices,
        .vertex_offset = -static_cast<i32>(allocated_vertices)
    };

    Handle<Buffer> staging_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
       .size = (create_info.vertex_count * sizeof(Vertex)) + (create_info.index_count * sizeof(u32)),
       .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
       .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    renderer.resource_manager->memcpy_to_buffer_once(staging_buffer, create_info.vertex_data, create_info.vertex_count * sizeof(Vertex));
    renderer.resource_manager->memcpy_to_buffer_once(staging_buffer, create_info.index_data, create_info.index_count * sizeof(u32), create_info.vertex_count * sizeof(Vertex));

    auto cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(cmd);
    renderer.copy_buffer_to_buffer(cmd, staging_buffer,  scene_vertex_buffer, { VkBufferCopy{
        .srcOffset = 0,
        .size = create_info.vertex_count * sizeof(Vertex)
    }});
    renderer.copy_buffer_to_buffer(cmd, staging_buffer, scene_index_buffer, { VkBufferCopy{
        .srcOffset = create_info.vertex_count * sizeof(Vertex),
        .size = create_info.index_count * sizeof(u32)
    }});
    renderer.end_recording_commands(cmd);
    renderer.submit_commands_once(cmd);

    renderer.command_manager->destroy_command_list(cmd);
    renderer.resource_manager->destroy_buffer(staging_buffer);

    allocated_vertices += create_info.vertex_count;
    allocated_indices += create_info.index_count;

    return mesh_allocator.alloc(mesh);
}
void RasterRenderPath::destroy_mesh(Handle<Mesh> mesh_handle) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    if(!mesh_allocator.is_handle_valid(mesh_handle)) {
        DEBUG_PANIC("Cannot delete mesh - Mesh with a handle id: = " << mesh_handle << ", does not exist!")
    }

    const Mesh& mesh = mesh_allocator.get_element(mesh_handle);

    // IMPL IMPL IMPL
    // IMPL IMPL IMPL
    // IMPL IMPL IMPL
    // IMPL IMPL IMPL

    mesh_allocator.free(mesh_handle);
}

Handle<Texture> RasterRenderPath::create_u8_texture(const TextureCreateInfo& create_info) {
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

    DEBUG_ASSERT(texture_anisotropy <= 16U)

    Texture texture{
        .width = create_info.width,
        .height = create_info.height,
        .bytes_per_pixel = create_info.bytes_per_pixel,
        .mip_level_count = (create_info.gen_mip_maps ? Utils::calculate_mipmap_levels_xy(create_info.width, create_info.height) : 1U),
        .is_srgb = static_cast<u32>(create_info.is_srgb),
        .use_linear_filter = static_cast<u32>(create_info.linear_filter)
    };

    texture.image = renderer.resource_manager->create_image(ImageCreateInfo{
        .format = format,
        .extent {
            .width = texture.width,
            .height = texture.height
        },
        .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (create_info.gen_mip_maps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : static_cast<VkImageUsageFlags>(0)),
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        .mip_level_count = texture.mip_level_count
    });

    texture.sampler = renderer.resource_manager->create_sampler(SamplerCreateInfo{
        .filter = create_info.linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmap_mode = create_info.linear_filter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,

        .max_mipmap = static_cast<f32>(texture.mip_level_count),
        .mipmap_bias = texture_mip_bias,
        .anisotropy = static_cast<f32>(texture_anisotropy)
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

    Handle<Buffer> staging_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = create_info.width * create_info.height * create_info.bytes_per_pixel,
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    renderer.resource_manager->memcpy_to_buffer_once(staging_buffer, create_info.pixel_data, create_info.width * create_info.height * create_info.bytes_per_pixel);

    auto cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(cmd);
    renderer.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, { ImageBarrier{
        .image_handle = texture.image,
        .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    }});
    renderer.copy_buffer_to_image(cmd, staging_buffer, texture.image, { BufferToImageCopy{}});
    if(create_info.gen_mip_maps) {
        renderer.gen_mipmaps(cmd, texture.image, create_info.linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT
        );
    } else {
        renderer.image_barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, { ImageBarrier{
            .image_handle = texture.image,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});
    }
    renderer.end_recording_commands(cmd);
    renderer.submit_commands_once(cmd);

    renderer.command_manager->destroy_command_list(cmd);
    renderer.resource_manager->destroy_buffer(staging_buffer);

    auto handle = texture_allocator.alloc(texture);

    renderer.resource_manager->update_descriptor(forward_descriptor, DescriptorUpdateInfo{
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
    if(!texture_allocator.is_handle_valid(texture_handle)) {
        DEBUG_PANIC("Cannot delete texture - Texture with a handle id: = " << texture_handle << ", does not exist!")
    }

    const Texture& texture = texture_allocator.get_element(texture_handle);
    renderer.resource_manager->destroy_image(texture.image);
    renderer.resource_manager->destroy_sampler(texture.sampler);

    texture_allocator.free(texture_handle);
}

Handle<Material> RasterRenderPath::create_material(const MaterialCreateInfo& create_info) {
    Material material{
        .albedo_texture = (create_info.albedo_texture == INVALID_HANDLE) ? default_white_srgb_texture : create_info.albedo_texture,
        .roughness_texture = (create_info.roughness_texture == INVALID_HANDLE) ? default_grey_unorm_texture : create_info.roughness_texture,
        .metalness_texture = (create_info.metalness_texture == INVALID_HANDLE) ? default_grey_unorm_texture : create_info.metalness_texture,
        .normal_texture = (create_info.normal_texture == INVALID_HANDLE) ? default_grey_unorm_texture : create_info.normal_texture,
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

    Handle<Buffer> staging_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Material),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    renderer.resource_manager->memcpy_to_buffer_once(staging_buffer, &material, sizeof(Material));

    auto handle = material_allocator.alloc(material);

    auto cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(cmd);
    renderer.copy_buffer_to_buffer(cmd, staging_buffer, scene_material_buffer, {
       VkBufferCopy {
           .dstOffset = handle * sizeof(Material),
           .size = sizeof(Material),
       }
    });
    renderer.end_recording_commands(cmd);
    renderer.submit_commands_once(cmd);

    renderer.command_manager->destroy_command_list(cmd);
    renderer.resource_manager->destroy_buffer(staging_buffer);

    return handle;
}
void RasterRenderPath::destroy_material(Handle<Material> material_handle) {
    if(!material_allocator.is_handle_valid(material_handle)) {
        DEBUG_PANIC("Cannot delete material - Material with a handle id: = " << material_handle << ", does not exist!")
    }

    const Material& material = material_allocator.get_element(material_handle);

    material_allocator.free(material_handle);
}
