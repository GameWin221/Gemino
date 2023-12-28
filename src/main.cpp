#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/debug.hpp>

struct Frame {
    Handle<CommandList> command_list{};

    Handle<Semaphore> present_semaphore{};
    Handle<Semaphore> render_semaphore{};
    Handle<Fence> fence{};
};

int main() {
    Window window(WindowConfig {
        .title = "Gemino Engine Example"
    });

    Renderer renderer(window, RendererConfig{
       .v_sync = VSyncMode::Enabled
    });

    //World world{};

    u32 frames_in_flight = 3U;
    u32 frame_idx{};

    // Image functionality test
    Handle<Image> image = renderer.image_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = VkExtent3D {1024U, 512U},

        .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        .create_flags = 0,
    });

    // Buffer functionality test
    Handle<Buffer> buffer = renderer.buffer_manager->create_buffer(BufferCreateInfo{
        .size = 1024,
        .buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY
    });

    Handle<GraphicsPipeline> pipeline = renderer.pipeline_manager->create_graphics_pipeline(GraphicsPipelineCreateInfo{
        .vertex_shader_path = "./res/shaders/default.vert.spv",
        .fragment_shader_path = "./res/shaders/default.frag.spv",

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

        .enable_depth_test = false,
        .enable_depth_write = true,
        .enable_vertex_input = false,

        .enable_blending = false,
        .polygon_mode = VK_POLYGON_MODE_FILL
    });

    // Compute pipeline functionality test
    Handle<ComputePipeline> kernel = renderer.pipeline_manager->create_compute_pipeline(ComputePipelineCreateInfo{
        .shader_path = "res/shaders/default.comp.spv"
    });

    std::vector<Handle<RenderTarget>> swapchain_render_targets{};
    for(const auto& view : renderer.swapchain->get_image_views()) {
        swapchain_render_targets.push_back(renderer.pipeline_manager->create_render_target(pipeline, RenderTargetCreateInfo{
            .extent = renderer.swapchain->get_extent(),
            .color_view = view
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
            .queue_family = QueueFamily::Graphics,
            .fence = frame.fence,
            .wait_semaphores{ frame.present_semaphore },
            .signal_semaphores{ frame.render_semaphore },
            .signal_stages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
        });
    }

    RenderTargetClear clear{
        .color{{0.0f, 0.0f, 1.0f, 1.0f}}
    };

    while(!window.should_close()) {
        const Frame& frame = frames[frame_idx];

        renderer.wait_for_fence(frame.fence);
        renderer.reset_fence(frame.fence);

        u32 swapchain_idx = renderer.get_next_swapchain_index(frame.present_semaphore);

        renderer.reset_commands(frame.command_list);

        renderer.begin_recording_commands(frame.command_list);
            renderer.begin_graphics_pipeline(frame.command_list, pipeline, swapchain_render_targets[swapchain_idx], clear);
            renderer.draw_count(frame.command_list, 3U);
            renderer.end_graphics_pipeline(frame.command_list);
        renderer.end_recording_commands(frame.command_list);

        renderer.submit_commands(frame.command_list, submit_infos[frame_idx]);

        renderer.present_swapchain(frame.render_semaphore, swapchain_idx);

        // I'm not sure if this should be at the end or at the beginning of this function
        window.poll_events();

        frame_idx = (frame_idx + 1) % frames_in_flight;
    }

    renderer.wait_for_device_idle();
}