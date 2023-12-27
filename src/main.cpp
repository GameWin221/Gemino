#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/debug.hpp>

int main() {
    Window window(WindowConfig {
        .title = "Gemino Engine Example"
    });

    Renderer renderer(window, RendererConfig{
       .v_sync = VSyncMode::Enabled
    });

    World world{};

    Handle<Image> image = renderer.image_manager->create_image(ImageCreateInfo{
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = VkExtent3D {1024U, 512U},

        .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        .create_flags = 0,
    });

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
        .polygon_mode = VK_POLYGON_MODE_FILL,
    });

    Handle<ComputePipeline> kernel = renderer.pipeline_manager->create_compute_pipeline(ComputePipelineCreateInfo{
        .shader_path = "res/shaders/default.comp.spv"
    });

    std::vector<Handle<RenderTarget>> swapchain_render_targets{};
    for(const auto& view : renderer.swapchain->get_image_views()) {
        swapchain_render_targets.push_back(renderer.pipeline_manager->create_render_target(pipeline, RenderTargetCreateInfo {
            .extent = renderer.swapchain->get_extent(),
            .color_view = view
        }));
    }

    std::vector<Handle<CommandList>> command_lists{};
    for(const auto& _ : renderer.swapchain->get_image_views()) {
        command_lists.push_back(renderer.command_manager->create_command_list(QueueFamily::Graphics));
    }

    Handle<Fence> fence = renderer.command_manager->create_fence();
    Handle<Semaphore> present_semaphore = renderer.command_manager->create_semaphore();
    Handle<Semaphore> render_semaphore = renderer.command_manager->create_semaphore();

    RenderTargetClear clear{
        .color{{1.0f, 0.0f, 1.0f, 1.0f}}
    };

    SubmitInfo submit_info{
        .queue_family = QueueFamily::Graphics,

        .fence = fence,

        .wait_semaphores{ present_semaphore },
        .signal_semaphores{ render_semaphore },
        .signal_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
    };

    while(!window.should_close()) {
        renderer.wait_for_fence(fence);
        renderer.reset_fence(fence);

        u32 swapchain_idx = renderer.get_next_swapchain_index(present_semaphore);

        Handle<u32> cmd = command_lists[swapchain_idx];
        Handle<u32> swapchain_rt = swapchain_render_targets[swapchain_idx];

        renderer.reset_commands(cmd);
        renderer.begin_recording_commands(cmd);

        renderer.begin_graphics_pipeline(cmd, pipeline, swapchain_rt, clear);
        renderer.end_graphics_pipeline(cmd);

        renderer.end_recording_commands(cmd);
        renderer.submit_commands(cmd, submit_info);
        renderer.present_swapchain(render_semaphore, swapchain_idx);

        // I'm not sure if this should be at the end or at the beginning of this function
        window.poll_events();
    }
}