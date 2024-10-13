#include "renderer.hpp"

void Renderer::resize(const Window &window) {
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
void Renderer::render(World &world, Handle<Camera> camera) {
    if (!world.get_valid_camera_handles().contains(camera)) {
        DEBUG_PANIC("Cannot render the world from the given camera! - Camera with handle id: " << camera << " is invalid.")
    }

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
void Renderer::reload_pipelines() {
    m_reload_pipelines_queued = true;
}

void Renderer::begin_recording_frame() {
    Frame &frame = m_frames[m_frame_in_flight_index];

    m_api.wait_for_fence(frame.fence);

    VkResult result = m_api.get_next_swapchain_index(frame.present_semaphore, &m_swapchain_target_index);
    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        DEBUG_ERROR("VK_ERROR_OUT_OF_DATE_KHR") // This message shouldn't be ever visible because a resize always occurs before rendering
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        DEBUG_PANIC("Failed to acquire a swapchain image! result=" << result)
    }

    m_api.reset_fence(frame.fence);
    m_api.reset_commands(frame.command_list);
    m_api.begin_recording_commands(frame.command_list);
}
void Renderer::update_world(World &world, Handle<Camera> camera) {
    Frame &frame = m_frames[m_frame_in_flight_index];

    std::vector<VkBufferCopy> object_copy_regions{};
    std::vector<VkBufferCopy> global_transforms_copy_regions{};
    std::vector<VkBufferCopy> camera_copy_regions{};

    object_copy_regions.reserve(world.get_changed_object_handles().size());
    global_transforms_copy_regions.reserve(world.get_changed_object_handles().size());
    //camera_copy_regions.reserve(world.get_changed_camera_handles().size());

    usize upload_buffer_size = m_api.m_resource_manager->get_buffer_data(frame.upload_buffer).size;

    usize upload_offset{};
    // Make sure that camera is always uploaded, no matter what
    if(camera != INVALID_HANDLE) {
        *frame.access_upload<Camera>(upload_offset) = world.get_camera(camera);

        camera_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(camera) * sizeof(Camera),
            .size = sizeof(Camera)
        });

        upload_offset += Utils::align(16u, sizeof(Camera));
    }

    std::vector<Handle<Object>> handles_to_clear{};
    for(const auto &handle : world.get_changed_object_handles()) {
        if(handle >= MAX_SCENE_OBJECTS) {
            DEBUG_PANIC("Failed to upload object with handle id: " << handle << "! MAX_SCENE_OBJECTS: " << MAX_SCENE_OBJECTS)
        }

        auto &object = *frame.access_upload<Object>(upload_offset);
        if(upload_offset + sizeof(object) >= upload_buffer_size) {
            DEBUG_WARNING("Upload buffer falling behind!")
            break;
        }

        object = world.get_object(handle);
        if (object.mesh_instance == INVALID_HANDLE) {
            object.visible = false;
        }

        object_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(handle) * sizeof(object),
            .size = sizeof(object)
        });

        upload_offset += Utils::align(16u, sizeof(object));

        auto &transform = *frame.access_upload<Transform>(upload_offset);
        if(upload_offset + sizeof(transform) >= upload_buffer_size) {
            DEBUG_WARNING("Upload source buffer falling behind!")
            break;
        }

        transform = world.get_global_transform(handle);

        global_transforms_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(handle) * sizeof(transform),
            .size = sizeof(transform)
        });

        upload_offset += Utils::align(16u, sizeof(transform));
        handles_to_clear.push_back(handle);
    }

    m_api.m_resource_manager->flush_mapped_buffer(frame.upload_buffer, upload_offset);

    world._clear_updates(handles_to_clear);

    // Ensure that other m_frames don't use these global buffers
    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_object_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = m_scene_global_transform_buffer,
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
    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_scene_global_transform_buffer, global_transforms_copy_regions);
    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_scene_camera_buffer, camera_copy_regions);

    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_object_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = m_scene_global_transform_buffer,
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
void Renderer::render_world(const World &world, Handle<Camera> camera) {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    u32 scene_objects_count = static_cast<u32>(world.get_objects().size());

    DrawCallGenPC draw_call_gen_pc{
        .object_count_pre_cull = scene_objects_count,
        .global_lod_bias = m_config_global_lod_bias,
        .global_cull_dist_multiplier = m_config_global_cull_dist_multiplier
    };
    
    render_pass_draw_call_gen(scene_objects_count, draw_call_gen_pc);
    render_pass_geometry();
    render_pass_offscreen_rt_to_swapchain();
}
void Renderer::end_recording_frame() {
    const Frame &frame = m_frames[m_frame_in_flight_index];

    SubmitInfo submit{
        .fence = frame.fence,
        .wait_semaphores = { frame.present_semaphore },
        .signal_semaphores{ frame.render_semaphore },
        .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
    };

    m_api.end_recording_commands(frame.command_list);
    m_api.submit_commands(frame.command_list, submit);

    VkResult result = m_api.present_swapchain(frame.render_semaphore, m_swapchain_target_index);
    if (result != VK_SUCCESS) {
        DEBUG_PANIC("Failed to present swap chain image! result=" << result)
    }

    m_frame_in_flight_index = (m_frame_in_flight_index + 1) % FRAMES_IN_FLIGHT;
    m_frames_since_init += 1;
}

void Renderer::render_pass_draw_call_gen(u32 scene_objects_count, const DrawCallGenPC &draw_call_gen_pc) {
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

    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = m_scene_draw_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
        },
        BufferBarrier{
            .buffer_handle = m_scene_draw_count_buffer,
            .src_access_mask = VK_ACCESS_SHADER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        },
    });
}
void Renderer::render_pass_geometry() {
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
    m_api.draw_indexed_indirect_count(frame.command_list,m_scene_draw_buffer, m_scene_draw_count_buffer, MAX_SCENE_DRAWS, static_cast<u32>(sizeof(DrawCommand)));

    m_api.end_graphics_pipeline(frame.command_list, m_forward_pipeline);
}
void Renderer::render_pass_offscreen_rt_to_swapchain() {
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