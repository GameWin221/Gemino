#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/debug.hpp>

// Example file that tests most of Gemino's features

constexpr static u8 EXAMPLE_IMAGE_DATA[2U * 2U * 4U] = {
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff,
};

struct PushConstants {
    alignas(16) glm::vec2 offset{};
    alignas(16) glm::vec3 color{};
};

struct Frame {
    Handle<CommandList> command_list{};

    Handle<Semaphore> present_semaphore{};
    Handle<Semaphore> render_semaphore{};
    Handle<Fence> fence{};
};

int main() {
    Window window(WindowConfig{
        .title = "Gemino Engine Example"
    });

    Renderer renderer(window, RendererConfig{
       .v_sync = VSyncMode::Enabled
    });

    //World world{};

    u32 frames_in_flight = 3U;
    u32 frame_idx{};

    Handle<Buffer> upload_buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = 1024,
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU,
    });
    Handle<Image> image = renderer.resource_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = VkExtent3D{2U, 2U},
        .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        .create_flags = 0,
    });
    Handle<Sampler> sampler = renderer.resource_manager->create_sampler(SamplerCreateInfo{
        .filter = VK_FILTER_NEAREST
    });

    renderer.resource_manager->memcpy_to_buffer_once(upload_buffer, EXAMPLE_IMAGE_DATA, sizeof(EXAMPLE_IMAGE_DATA));

    Handle<CommandList> upload_cmd = renderer.command_manager->create_command_list(QueueFamily::Graphics);
    renderer.begin_recording_commands(upload_cmd);
    renderer.image_barrier(upload_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {ImageBarrier{
        .image_handle = image,
        .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    }});
    renderer.copy_buffer_to_image(upload_cmd, upload_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {VkBufferImageCopy{
        .imageSubresource {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1U
        },
        .imageExtent = VkExtent3D{
            .width = 2U,
            .height = 2U,
            .depth = 1U
        },
    }});
    renderer.image_barrier(upload_cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {ImageBarrier{
        .image_handle = image,
        .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }});
    renderer.end_recording_commands(upload_cmd);
    renderer.submit_commands_once(upload_cmd);

    renderer.command_manager->destroy_command_list(upload_cmd);

    Handle<Buffer> buffer = renderer.resource_manager->create_buffer(BufferCreateInfo{
        .size = 1024,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    Handle<Descriptor> main_descriptor = renderer.resource_manager->create_descriptor(DescriptorCreateInfo{
        .bindings{
            DescriptorBindingCreateInfo {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }
        }
    });
    renderer.resource_manager->update_descriptor(main_descriptor, DescriptorUpdateInfo{
       .bindings {
           DescriptorBindingUpdateInfo{
               .binding_index = 0U,
               .image_info {
                   .image_handle = image,
                   .image_sampler = sampler
               }
           }
       }
    });

    Handle<GraphicsPipeline> pipeline = renderer.pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./res/shaders/default.vert.spv",
        .fragment_shader_path = "./res/shaders/default.frag.spv",

        .push_constants_size = sizeof(PushConstants),
        .descriptors { main_descriptor },

        .color_target {
            .format = renderer.swapchain->get_format(),
            .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        //.depth_target {
        //    .format = VK_FORMAT_D16_UNORM,
        //    .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        //    .final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        //},

        .enable_depth_write = true,
    });

    Handle<ComputePipeline> kernel = renderer.pipeline_manager->create_compute_pipeline(ComputePipelineCreateInfo{
        .shader_path = "res/shaders/default.comp.spv",
        .push_constants_size = sizeof(glm::uvec4)
    });

    std::vector<Handle<RenderTarget>> swapchain_render_targets{};
    for(const auto& view : renderer.swapchain->get_image_views()) {
        swapchain_render_targets.push_back(renderer.pipeline_manager->create_render_target(pipeline, RenderTargetCreateInfo{
            .extent = renderer.swapchain->get_extent(),
            .color_target_view = view
        }));
    }

    std::vector<Frame> frames{};
    for(u32 i{}; i < frames_in_flight; ++i) {
        frames.push_back(Frame{
            .command_list = renderer.command_manager->create_command_list(QueueFamily::Graphics),
            .present_semaphore = renderer.command_manager->create_semaphore(),
            .render_semaphore = renderer.command_manager->create_semaphore(),
            .fence = renderer.command_manager->create_fence()
        });
    }

    std::vector<SubmitInfo> submit_infos{};
    for(const auto& frame : frames) {
        submit_infos.push_back(SubmitInfo{
            .fence = frame.fence,
            .wait_semaphores{ frame.present_semaphore },
            .signal_semaphores{ frame.render_semaphore },
            .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
        });
    }

    RenderTargetClear clear{
        .color{{0.0f, 0.0f, 1.0f, 1.0f}}
    };

    float time{};

    PushConstants pc{};
    glm::vec4 test_data{};

    while(!window.should_close()) {
        const Frame& frame = frames[frame_idx];

        renderer.wait_for_fence(frame.fence);
        renderer.reset_fence(frame.fence);

        u32 swapchain_idx = renderer.get_next_swapchain_index(frame.present_semaphore);

        pc.offset = glm::vec2(sinf(time) / 4.0f, cosf(time) / 4.0f);
        pc.color = glm::vec3((sinf(time) * 0.5f + 0.5f), (cosf(time) * 0.5f + 0.5f), (cosf(time + 1.4f) * 0.5f + 0.5f) / 4.0f);

        renderer.reset_commands(frame.command_list);

        renderer.begin_recording_commands(frame.command_list);
            renderer.begin_compute_pipeline(frame.command_list, kernel);
            renderer.push_compute_constants(frame.command_list, kernel, &test_data);
            renderer.dispatch_compute_pipeline(frame.command_list, glm::uvec3(1U, 1U, 1U));

            renderer.begin_graphics_pipeline(frame.command_list, pipeline, swapchain_render_targets[swapchain_idx], clear);
            renderer.push_graphics_constants(frame.command_list, pipeline, &pc);
            renderer.bind_graphics_descriptor(frame.command_list, pipeline, main_descriptor, 0U);
            renderer.draw_count(frame.command_list, 6U);
            renderer.end_graphics_pipeline(frame.command_list, pipeline, swapchain_render_targets[swapchain_idx]);
        renderer.end_recording_commands(frame.command_list);

        renderer.submit_commands(frame.command_list, submit_infos[frame_idx]);

        renderer.present_swapchain(frame.render_semaphore, swapchain_idx);

        // I'm not sure if this should be at the end or at the beginning of this function
        window.poll_events();

        time += 0.01f;

        frame_idx = (frame_idx + 1) % frames_in_flight;
    }

    renderer.wait_for_device_idle();
}