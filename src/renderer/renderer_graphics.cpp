#include <algorithm>

#include "renderer.hpp"

void Renderer::resize(Window &window) {
    m_api.wait_for_device_idle();

    m_shared.frames_since_init = 0;

    for(const auto& frame : m_frames) {
        m_api.reset_commands(frame.command_list);
    }

    destroy_screen_images();

    m_api.recreate_swapchain(window.get_size(), m_api.get_swapchain_config());

    init_screen_images(window.get_size());

    resize_passes(window);

    m_api.wait_for_device_idle();
}
void Renderer::render(Window &window, World &world, Handle<Camera> camera) {
    if (!world.get_valid_camera_handles().contains(camera)) {
        DEBUG_PANIC("Cannot render the world from the given camera! - Camera with handle id: " << camera << " is invalid.")
    }

    if (m_reload_pipelines_queued) {
        DEBUG_LOG("Reloading pipelines...")

        for (const auto &f : m_frames) {
            m_api.wait_for_fence(f.fence);
        }

        m_api.wait_for_device_idle();

        destroy_passes();
        init_passes(window);

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
    DEBUG_TIMESTAMP(start);

    Frame &frame = m_frames[m_frame_in_flight_index];
    m_api.wait_for_fence(frame.fence);

    std::unordered_map<Handle<Query>, QueryPipelineStatisticsResults> pipeline_statistics_results{};
    std::unordered_map<Handle<Query>, u64> scalar_query_results{};

    std::vector<Handle<Query>> queries_to_be_read_and_reset{};
    for(const auto &[name, data] : frame.gpu_timing) {
        const auto &[queries, time] = data;
        queries_to_be_read_and_reset.push_back(queries.first);
        queries_to_be_read_and_reset.push_back(queries.second);
    }
    for(auto &[name, data] : frame.gpu_pipeline_statistics) {
        auto &[query, time] = data;
        queries_to_be_read_and_reset.push_back(query);
    }

    // Thanks to m_api.wait_for_fence() above we are certain that we can read the results (either populated or empty)
    m_api.read_queries(queries_to_be_read_and_reset, &scalar_query_results, &pipeline_statistics_results, false);

    for(auto &[name, data] : frame.gpu_timing) {
        auto &[queries, time_pair] = data;
        time_pair.first = static_cast<f64>(scalar_query_results.at(queries.first)) * static_cast<f64>(m_default_timestamp_period) / 1000.0 / 1000.0;
        time_pair.second = static_cast<f64>(scalar_query_results.at(queries.second)) * static_cast<f64>(m_default_timestamp_period) / 1000.0 / 1000.0;
    }
    for(auto &[name, data] : frame.gpu_pipeline_statistics) {
        auto &[query, results] = data;
        results = pipeline_statistics_results.at(query);
    }

    VkResult result = m_api.get_next_swapchain_index(frame.present_semaphore, &m_shared.swapchain_target_index);
    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        DEBUG_ERROR("VK_ERROR_OUT_OF_DATE_KHR") // This message shouldn't be ever visible because a resize always occurs before rendering
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        DEBUG_PANIC("Failed to acquire a swapchain image! result=" << result)
    }

    m_api.reset_fence(frame.fence);
    m_api.reset_commands(frame.command_list);
    m_api.begin_recording_commands(frame.command_list);

    m_api.reset_queries_cmd(frame.command_list, queries_to_be_read_and_reset);

    m_api.write_timestamp(frame.command_list, frame.gpu_timing["Total GPU Time"].first.first);

    DEBUG_TIMESTAMP(stop);
    frame.cpu_timing[__FUNCTION__] = DEBUG_TIME_DIFF(start, stop);
}
void Renderer::update_world(World &world, Handle<Camera> camera) {
    DEBUG_TIMESTAMP(start);

    Frame &frame = m_frames[m_frame_in_flight_index];

    std::vector<VkBufferCopy> object_copy_regions{};
    std::vector<VkBufferCopy> global_transforms_copy_regions{};
    std::vector<VkBufferCopy> camera_copy_regions{};

    object_copy_regions.reserve(world.get_changed_object_handles().size());
    global_transforms_copy_regions.reserve(world.get_changed_object_handles().size());
    //camera_copy_regions.reserve(world.get_changed_camera_handles().size());

    usize upload_buffer_size = m_api.rm->get_data(frame.upload_buffer).size;

    usize upload_offset{};
    // Make sure that camera is always uploaded, no matter what
    if(camera != INVALID_HANDLE) {
        *frame.access_upload<Camera>(upload_offset) = world.get_camera(camera);

        camera_copy_regions.push_back(VkBufferCopy{
            .srcOffset = upload_offset,
            .dstOffset = static_cast<VkDeviceSize>(camera.as_u32()) * sizeof(Camera),
            .size = sizeof(Camera)
        });

        upload_offset += Utils::align(16u, sizeof(Camera));
    }

    std::vector<Handle<Object>> handles_to_clear{};
    for(const auto &handle : world.get_changed_object_handles()) {
        if(handle.as_u32() >= static_cast<u32>(MAX_SCENE_OBJECTS)) {
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
            .dstOffset = static_cast<VkDeviceSize>(handle.as_u32()) * sizeof(object),
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
            .dstOffset = static_cast<VkDeviceSize>(handle.as_u32()) * sizeof(transform),
            .size = sizeof(transform)
        });

        upload_offset += Utils::align(16u, sizeof(transform));
        handles_to_clear.push_back(handle);
    }

    m_api.rm->flush_mapped_buffer(frame.upload_buffer, upload_offset);

    world._clear_updates(handles_to_clear);

    m_api.write_timestamp(frame.command_list, frame.gpu_timing.at("Buffers Copy").first.first);

    // Ensure that other m_frames don't use these global buffers
    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        BufferBarrier{
            .buffer_handle = m_shared.scene_object_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = m_shared.scene_global_transform_buffer,
            .src_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
        BufferBarrier{
            .buffer_handle = m_shared.scene_camera_buffer,
            .src_access_mask = VK_ACCESS_UNIFORM_READ_BIT,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT
        },
    });

    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_shared.scene_object_buffer, object_copy_regions);
    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_shared.scene_global_transform_buffer, global_transforms_copy_regions);
    m_api.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, m_shared.scene_camera_buffer, camera_copy_regions);

    m_api.buffer_barrier(frame.command_list, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {
        BufferBarrier{
            .buffer_handle = m_shared.scene_object_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = m_shared.scene_global_transform_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT
        },
        BufferBarrier{
            .buffer_handle = m_shared.scene_camera_buffer,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_UNIFORM_READ_BIT
        },
    });

    m_api.write_timestamp(frame.command_list, frame.gpu_timing.at("Buffers Copy").first.second);

    DEBUG_TIMESTAMP(stop);
    frame.cpu_timing[__FUNCTION__] = DEBUG_TIME_DIFF(start, stop);
}
void Renderer::render_world(World &world, Handle<Camera> camera) {
    DEBUG_TIMESTAMP(start);

    Frame &frame = m_frames[m_frame_in_flight_index];

    m_registered_passes["Debug Pass"].enabled = m_shared.config_enable_debug_shape_view;

    std::vector<std::pair<std::string, RegisteredPassRef>> passes_sorted{};
    for(const auto &[name, registered_pass] : m_registered_passes) {
        passes_sorted.emplace_back(name, registered_pass.as_ref());
    }
    std::sort(passes_sorted.begin(), passes_sorted.end(), [](const auto &left, const auto &right) {
        const auto &[l_name, l_registered_pass] = left;
        const auto &[r_name, r_registered_pass] = right;
        return l_registered_pass.order < r_registered_pass.order;
    });

    for(const auto &[name, registered_pass] : passes_sorted) {
        m_api.write_timestamp(frame.command_list, frame.gpu_timing.at(name).first.first);

        if(registered_pass.query_statistics) {
            if(!frame.gpu_pipeline_statistics.contains(name)) {
                DEBUG_PANIC("Failed to begin query \"" << name << "\"! You cannot enable \"query_statistics\" of a registered pass at runtime.")
            }

            m_api.begin_query(frame.command_list, frame.gpu_pipeline_statistics.at(name).first);
        }

        if (registered_pass.enabled) {
            registered_pass.pass_ptr->process(frame.command_list, m_api, m_shared, world);
        }

        if(registered_pass.query_statistics) {
            m_api.end_query(frame.command_list, frame.gpu_pipeline_statistics.at(name).first);
        }

        m_api.write_timestamp(frame.command_list, frame.gpu_timing.at(name).first.second);
    }

    DEBUG_TIMESTAMP(stop);
    frame.cpu_timing[__FUNCTION__] = DEBUG_TIME_DIFF(start, stop);
}
void Renderer::end_recording_frame() {
    DEBUG_TIMESTAMP(start);

    Frame &frame = m_frames[m_frame_in_flight_index];

    SubmitInfo submit{
        .fence = frame.fence,
        .wait_semaphores = { frame.present_semaphore },
        .signal_semaphores{ frame.render_semaphore },
        .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
    };

    m_api.write_timestamp(frame.command_list, frame.gpu_timing["Total GPU Time"].first.second);

    m_api.end_recording_commands(frame.command_list);
    m_api.submit_commands(frame.command_list, submit);

    VkResult result = m_api.present_swapchain(frame.render_semaphore, m_shared.swapchain_target_index);
    if (result != VK_SUCCESS) {
        DEBUG_PANIC("Failed to present swap chain image! result=" << result)
    }

    m_frame_in_flight_index = (m_frame_in_flight_index + 1) % static_cast<u32>(m_frames.size());
    m_shared.frames_since_init += 1;

    DEBUG_TIMESTAMP(stop);
    frame.cpu_timing[__FUNCTION__] = DEBUG_TIME_DIFF(start, stop);
}