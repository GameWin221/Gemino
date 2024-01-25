#include "raster_render_path.hpp"

RasterRenderPath::RasterRenderPath(const Window &window, const RendererConfig& render_config) : BaseRenderPath(window, render_config) {
    object_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Object) * MAX_OBJECTS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY,
    });
    transform_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Transform) * MAX_TRANSFORMS,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY,
    });
    camera_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Camera),
        .buffer_usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY,
    });

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
            .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        .depth_target {
            .format = VK_FORMAT_D32_SFLOAT,
            .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },

        .enable_depth_test = true,
        .enable_depth_write = true,
    });

    depth_image = renderer.resource_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = VkExtent3D{ window.get_size().x, window.get_size().y },
        .usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT
    });

    for(u32 i{}; i < frames_in_flight; ++i) {
        Frame frame{
            .command_list = renderer.command_manager->create_command_list(QueueFamily::Graphics),
            .present_semaphore = renderer.command_manager->create_semaphore(),
            .render_semaphore = renderer.command_manager->create_semaphore(),
            .fence = renderer.command_manager->create_fence(),
            .swapchain_target = renderer.pipeline_manager->create_render_target(forward_pipeline, RenderTargetCreateInfo{
                .view_extent = renderer.swapchain->get_extent(),
                .color_target_view = renderer.swapchain->get_image_views()[i],
                .depth_target_handle = depth_image
            }),
            .upload_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
                .size = PER_FRAME_UPLOAD_BUFFER_SIZE,
                .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
            })
        };

        frame.upload_ptr = renderer.resource_manager->map_buffer(frame.upload_buffer);

        frames.push_back(frame);
    }
    for(const auto& frame : frames) {
        submit_infos.push_back(SubmitInfo{
            .fence = frame.fence,
            .wait_semaphores{ frame.present_semaphore },
            .signal_semaphores{ frame.render_semaphore },
            .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
        });
    }
}
RasterRenderPath::~RasterRenderPath() {
    renderer.wait_for_device_idle();

    for(const auto& frame : frames) {
        renderer.resource_manager->unmap_buffer(frame.upload_buffer);
    }
}

void RasterRenderPath::begin_recording_frame() {
    const Frame& frame = frames[frame_in_flight_index];

    renderer.wait_for_fence(frame.fence);
    renderer.reset_fence(frame.fence);

    swapchain_target_index = renderer.get_next_swapchain_index(frame.present_semaphore);

    renderer.reset_commands(frame.command_list);
    renderer.begin_recording_commands(frame.command_list);
}
void RasterRenderPath::update_world(const World &world, Handle<Camera> camera) {
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

    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, object_buffer, object_copy_regions);
    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, transform_buffer, transform_copy_regions);
    renderer.copy_buffer_to_buffer(frame.command_list, frame.upload_buffer, camera_buffer, camera_copy_regions);
}
void RasterRenderPath::render_world(const World& world, Handle<Camera> camera) {
    const Frame& frame = frames[frame_in_flight_index];

    renderer.begin_graphics_pipeline(frame.command_list, forward_pipeline, frame.swapchain_target, RenderTargetClear{
        .color {0.0f, 0.0f, 0.0f, 1.0f}
    });
    renderer.bind_graphics_descriptor(frame.command_list, forward_pipeline, forward_descriptor, 0U);
    renderer.draw_count(frame.command_list, 6U, 0U, static_cast<u32>(world.get_objects().size()));
    renderer.end_graphics_pipeline(frame.command_list, forward_pipeline, frame.swapchain_target);
}
void RasterRenderPath::end_recording_frame() {
    const Frame& frame = frames[frame_in_flight_index];

    renderer.end_recording_commands(frame.command_list);
    renderer.submit_commands(frame.command_list, submit_infos[frame_in_flight_index]);
    renderer.present_swapchain(frame.render_semaphore, swapchain_target_index);

    frame_in_flight_index = (frame_in_flight_index + 1) % frames_in_flight;
    frames_since_start += 1;
}
