#include "raster_render_path.hpp"

RasterRenderPath::RasterRenderPath(Window& window, VSyncMode v_sync) : renderer(window, SwapchainConfig{
    .v_sync = v_sync,
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
}) {
    object_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Object) * MAX_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    transform_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Transform) * MAX_TRANSFORMS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });
    camera_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
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

    forward_descriptor = renderer.resource_manager->create_descriptor(DescriptorCreateInfo{
        .bindings{
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
            DescriptorBindingCreateInfo{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        }
    });
    renderer.resource_manager->update_descriptor(forward_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .buffer_info {
                    .buffer_handle = object_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 1U,
                .buffer_info {
                    .buffer_handle = transform_buffer
                }
            },
            DescriptorBindingUpdateInfo{
                .binding_index = 2U,
                .buffer_info {
                    .buffer_handle = camera_buffer
                }
            }
        }
    });

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
        .enable_vertex_input = true

        //.depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL
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

void RasterRenderPath::render(const World& world, Handle<Camera> camera) {
    begin_recording_frame();
    update_world(world, camera);
    render_world(world, camera);
    end_recording_frame();
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
void RasterRenderPath::update_world(const World& world, Handle<Camera> camera) {
    Frame& frame = frames[frame_in_flight_index];

    std::vector<VkBufferCopy> object_copy_regions{};
    std::vector<VkBufferCopy> transform_copy_regions{};
    std::vector<VkBufferCopy> camera_copy_regions{};
    object_copy_regions.reserve(world.get_changed_object_handles().size());
    transform_copy_regions.reserve(world.get_changed_transform_handles().size());
    //camera_copy_regions.reserve(world.get_changed_camera_handles().size());

    usize upload_buffer_size = renderer.resource_manager->get_buffer_data(frame.upload_buffer).size;

    usize upload_offset{};

    for(const auto& handle : world.get_changed_object_handles()) {
        if(upload_offset + sizeof(Object) >= upload_buffer_size) {
            DEBUG_PANIC("UPLOAD OVERFLOW")
        }

        frame.access_upload<Object>(upload_offset)->transform = world.get_objects()[handle].transform;

        object_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(handle) * sizeof(Object),
            .size = sizeof(Object)
        });

        upload_offset += sizeof(Object);
    }

    for(const auto& handle : world.get_changed_transform_handles()) {
        if(upload_offset + sizeof(Transform) >= upload_buffer_size) {
            DEBUG_PANIC("UPLOAD OVERFLOW")
        }

        frame.access_upload<Transform>(upload_offset)->matrix = world.get_transforms()[handle].matrix;

        transform_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(handle) * sizeof(Transform),
            .size = sizeof(Transform)
        });

        upload_offset += sizeof(Transform);
    }

    if(upload_offset + sizeof(Camera) >= upload_buffer_size) {
        DEBUG_PANIC("UPLOAD OVERFLOW")
    }

    *frame.access_upload<Camera>(upload_offset) = world.get_cameras()[camera];

    camera_copy_regions.push_back(VkBufferCopy{
        .srcOffset = upload_offset,
        .dstOffset = static_cast<VkDeviceSize>(camera) * sizeof(Camera),
        .size = sizeof(Camera)
    });

    upload_offset += sizeof(Camera);

    renderer.resource_manager->flush_mapped_buffer(frame.upload_buffer, upload_offset);

    // Ensure that other frames don't use these global buffers
    renderer.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        BufferBarrier{
            .buffer_handle = object_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = transform_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = camera_buffer,
            .src_access_mask = VK_ACCESS_UNIFORM_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
    });

    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, object_buffer, object_copy_regions);;
    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, transform_buffer, transform_copy_regions);
    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, camera_buffer, camera_copy_regions);

    renderer.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = object_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = transform_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = camera_buffer,
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

    for(const auto& object_handle : world.get_valid_object_handles()) {
        const Object& object = world.get_object(object_handle);
        const Mesh& mesh = mesh_allocator.get_element(object.mesh);

        renderer.draw_indexed(frame.command_list, mesh.index_count, mesh.first_index, mesh.vertex_offset);
    }

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
        DEBUG_PANIC("Failed to present swap chain image!");
    }

    frame_in_flight_index = (frame_in_flight_index + 1) % frames_in_flight;
    frames_since_init += 1;
}

Handle<Mesh> RasterRenderPath::create_mesh(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Mesh mesh{
        .index_count = static_cast<u32>(indices.size()),
        .first_index = allocated_indices,
        .vertex_offset = static_cast<i32>(allocated_vertices)
    };

    Handle<Buffer> staging_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
       .size = (vertices.size() * sizeof(Vertex)) + (indices.size() * sizeof(u32)),
       .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
       .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    renderer.resource_manager->memcpy_to_buffer_once(staging_buffer, vertices.data(), vertices.size() * sizeof(Vertex));
    renderer.resource_manager->memcpy_to_buffer_once(staging_buffer, indices.data(), indices.size() * sizeof(u32), vertices.size() * sizeof(Vertex));

    auto cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(cmd);
    renderer.copy_buffer_to_buffer(cmd, staging_buffer,  scene_vertex_buffer, { VkBufferCopy{
        .srcOffset = 0,
        .size = vertices.size() * sizeof(Vertex)
    }});
    renderer.copy_buffer_to_buffer(cmd, staging_buffer, scene_index_buffer, { VkBufferCopy{
        .srcOffset = vertices.size() * sizeof(Vertex),
        .size = indices.size() * sizeof(u32)
    }});
    renderer.end_recording_commands(cmd);
    renderer.submit_commands_once(cmd);

    renderer.command_manager->destroy_command_list(cmd);
    renderer.resource_manager->destroy_buffer(staging_buffer);

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


