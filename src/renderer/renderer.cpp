#include "renderer.hpp"
#include <cstring>
#include <bit>

Renderer::Renderer(const Window& window, const RendererConfig& config) : renderer_config(config) {
    instance = MakeUnique<Instance>(window.get_native_handle());

    swapchain = MakeUnique<Swapchain>(
        instance->get_device(),
        instance->get_physical_device(),
        instance->get_surface(),
        window.get_size(),
        renderer_config.v_sync
    );

    const auto& family_indices = instance->get_queue_family_indices();
    image_manager = MakeUnique<ImageManager>(instance->get_device(), instance->get_allocator());
    buffer_manager = MakeUnique<BufferManager>(instance->get_device(), instance->get_allocator());
    pipeline_manager = MakeUnique<PipelineManager>(instance->get_device());
    command_manager = MakeUnique<CommandManager>(
        instance->get_device(),
        family_indices.graphics.value(),
        family_indices.transfer.value(),
        family_indices.compute.value()
    );
}

u32 Renderer::get_next_swapchain_index(Handle<Semaphore> wait_semaphore) const {
    u32 index{};
    DEBUG_ASSERT(vkAcquireNextImageKHR(
            instance->get_device(),
            swapchain->get_handle(),
            renderer_config.frame_timeout,
            command_manager->get_semaphore_data(wait_semaphore).semaphore,
            nullptr,
            &index
    ) == VK_SUCCESS)

    return index;
}
void Renderer::present_swapchain(Handle<Semaphore> wait_semaphore, u32 image_index) const {
    VkSwapchainKHR swapchains[] = { swapchain->get_handle() };

    VkPresentInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1U,
            .pWaitSemaphores = &command_manager->get_semaphore_data(wait_semaphore).semaphore,
            .swapchainCount = 1U,
            .pSwapchains = swapchains,
            .pImageIndices = &image_index,
    };

    DEBUG_ASSERT(vkQueuePresentKHR(instance->get_graphics_queue(), &info) == VK_SUCCESS);
}

void Renderer::wait_for_fence(Handle<Fence> handle) const {
    DEBUG_ASSERT(vkWaitForFences(
            instance->get_device(),
            1U,
            &command_manager->get_fence_data(handle).fence,
            VK_TRUE,
            renderer_config.frame_timeout
    ) == VK_SUCCESS)
}
void Renderer::reset_fence(Handle<Fence> handle) const {
    DEBUG_ASSERT(vkResetFences(instance->get_device(), 1U, &command_manager->get_fence_data(handle).fence) == VK_SUCCESS)
}

void Renderer::reset_commands(Handle<CommandList> handle) const {
    DEBUG_ASSERT(vkResetCommandBuffer(command_manager->get_command_list_data(handle).command_buffer, 0U) == VK_SUCCESS)
}
void Renderer::begin_recording_commands(Handle<CommandList> handle) const {
    VkCommandBufferBeginInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    DEBUG_ASSERT(vkBeginCommandBuffer(command_manager->get_command_list_data(handle).command_buffer, &info) == VK_SUCCESS)
}
void Renderer::end_recording_commands(Handle<CommandList> handle) const {
    DEBUG_ASSERT(vkEndCommandBuffer(command_manager->get_command_list_data(handle).command_buffer) == VK_SUCCESS)
}
void Renderer::submit_commands(Handle<CommandList> handle, const SubmitInfo &info) const {
    std::vector<VkSemaphore> wait_semaphores{};
    for(const auto& semaphore_handle : info.wait_semaphores) {
        wait_semaphores.push_back(command_manager->get_semaphore_data(semaphore_handle).semaphore);
    }

    std::vector<VkSemaphore> signal_semaphores{};
    for(const auto& semaphore_handle : info.signal_semaphores) {
        signal_semaphores.push_back(command_manager->get_semaphore_data(semaphore_handle).semaphore);
    }

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<u32>(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = info.signal_stages.data(),
        .commandBufferCount = 1U,
        .pCommandBuffers = &command_manager->get_command_list_data(handle).command_buffer,
        .signalSemaphoreCount = static_cast<u32>(signal_semaphores.size()),
        .pSignalSemaphores = signal_semaphores.data(),
    };

    VkQueue queue;
    switch (info.queue_family) {
        case QueueFamily::Graphics:
            queue = instance->get_graphics_queue();
            break;
        case QueueFamily::Transfer:
            queue = instance->get_transfer_queue();
            break;
        case QueueFamily::Compute:
            queue = instance->get_compute_queue();
            break;
        default:
            DEBUG_PANIC("Unknown QueueFamily!")
            break;
    }

    VkFence fence = command_manager->get_fence_data(info.fence).fence;

    DEBUG_ASSERT(vkQueueSubmit(queue, 1U, &submit_info, fence) == VK_SUCCESS)
}

void Renderer::begin_graphics_pipeline(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<RenderTarget> render_target, const RenderTargetClear& clear) const {
    const GraphicsPipeline& pipe = pipeline_manager->get_graphics_pipeline_data(pipeline);
    const RenderTarget& rt = pipeline_manager->get_render_target_data(render_target);
    const CommandList& cmd = command_manager->get_command_list_data(command_list);

    VkClearValue clear_values[2]{};

    u32 uses_color_target = static_cast<u32>(pipe.create_info.color_target.format != VK_FORMAT_UNDEFINED);
    u32 uses_depth_target = static_cast<u32>(pipe.create_info.depth_target.format != VK_FORMAT_UNDEFINED);
    if(uses_color_target) {
        clear_values[0].color = std::bit_cast<VkClearColorValue>(clear.color);
    }
    if(uses_depth_target) {
        clear_values[uses_color_target].depthStencil = { clear.depth, 0U};
    }

    VkRenderPassBeginInfo info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pipe.render_pass,
        .framebuffer = rt.framebuffer,
        .renderArea {
            .extent = rt.extent,
        },
        .clearValueCount = uses_color_target + uses_depth_target,
        .pClearValues = clear_values,
    };

    VkViewport viewport{
        .width = static_cast<f32>(rt.extent.width),
        .height = static_cast<f32>(rt.extent.height),
        .maxDepth = 1.0f,
    };

    VkRect2D scissor{
        .extent = rt.extent
    };

    vkCmdBeginRenderPass(cmd.command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline);
    vkCmdSetViewport(cmd.command_buffer, 0U, 1U, &viewport);
    vkCmdSetScissor(cmd.command_buffer, 0U, 1U, &scissor);
}
void Renderer::end_graphics_pipeline(Handle<CommandList> command_list) const {
    vkCmdEndRenderPass(command_manager->get_command_list_data(command_list).command_buffer);
}

void Renderer::draw_count(Handle<CommandList> command_list, u32 vertex_count, u32 first_vertex, u32 instance_count) const {
    vkCmdDraw(command_manager->get_command_list_data(command_list).command_buffer, vertex_count, instance_count, first_vertex, 0U);
}

void Renderer::wait_for_device_idle() const {
    vkDeviceWaitIdle(instance->get_device());
}
