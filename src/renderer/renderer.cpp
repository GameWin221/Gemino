#include "renderer.hpp"
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
    resource_manager = MakeUnique<ResourceManager>(instance->get_device(), instance->get_allocator());
    pipeline_manager = MakeUnique<PipelineManager>(instance->get_device(), resource_manager.get());
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
    VkSwapchainKHR present_swapchain = swapchain->get_handle();

    VkPresentInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1U,
        .pWaitSemaphores = &command_manager->get_semaphore_data(wait_semaphore).semaphore,
        .swapchainCount = 1U,
        .pSwapchains = &present_swapchain,
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
void Renderer::begin_recording_commands(Handle<CommandList> handle, VkCommandBufferUsageFlags usage) const {
    VkCommandBufferBeginInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = usage
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
    switch (command_manager->get_command_list_data(handle).family) {
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
void Renderer::submit_commands_once(Handle<CommandList> handle) const {
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1U,
        .pCommandBuffers = &command_manager->get_command_list_data(handle).command_buffer,
    };

    VkQueue queue;
    switch (command_manager->get_command_list_data(handle).family) {
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

    Handle<Fence> fence = command_manager->create_fence(false);

    DEBUG_ASSERT(vkQueueSubmit(queue, 1U, &submit_info, command_manager->get_fence_data(fence).fence) == VK_SUCCESS)

    wait_for_fence(fence);

    command_manager->destroy_fence(fence);
}

void Renderer::image_barrier(Handle<CommandList> command_list, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, const std::vector<ImageBarrier>& barriers) const {
    std::vector<VkImageMemoryBarrier> image_barriers(barriers.size());
    for(usize i{}; i < barriers.size(); ++i) {
        const auto& barrier = barriers[i];
        const Image& image = resource_manager->get_image_data(barrier.image_handle);

        image_barriers[i] = VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = barrier.src_access_mask,
            .dstAccessMask = barrier.dst_access_mask,
            .oldLayout = barrier.old_layout,
            .newLayout = barrier.new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image.image,
            .subresourceRange {
                .aspectMask = image.aspect_flags,
                .baseMipLevel = barrier.dst_level_count,
                .levelCount = barrier.level_count,
                .baseArrayLayer = barrier.dst_array_layer,
                .layerCount = barrier.layer_count,
            }
        };
    }

    vkCmdPipelineBarrier(command_manager->get_command_list_data(command_list).command_buffer, src_stage, dst_stage, 0U, 0U, nullptr, 0U, nullptr, static_cast<u32>(image_barriers.size()), image_barriers.data());
}

void Renderer::copy_buffer_to_buffer(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Buffer> dst, const std::vector<VkBufferCopy>& regions) const {
    const Buffer& src_buffer = resource_manager->get_buffer_data(src);
    const Buffer& dst_buffer = resource_manager->get_buffer_data(dst);
    const CommandList& cmd = command_manager->get_command_list_data(command_list);

    vkCmdCopyBuffer(cmd.command_buffer, src_buffer.buffer, dst_buffer.buffer, static_cast<u32>(regions.size()), regions.data());
}
void Renderer::copy_buffer_to_image(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Image> dst, VkImageLayout dst_layout, const std::vector<VkBufferImageCopy> &regions) const {
    const Buffer& src_buffer = resource_manager->get_buffer_data(src);
    const Image& dst_image = resource_manager->get_image_data(dst);
    const CommandList& cmd = command_manager->get_command_list_data(command_list);

    vkCmdCopyBufferToImage(cmd.command_buffer, src_buffer.buffer, dst_image.image, dst_layout, static_cast<u32>(regions.size()), regions.data());
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
void Renderer::end_graphics_pipeline(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<RenderTarget> render_target) const {
    vkCmdEndRenderPass(command_manager->get_command_list_data(command_list).command_buffer);

    /*
    const GraphicsPipeline& pipe = pipeline_manager->get_graphics_pipeline_data(pipeline);
    const RenderTarget& rt = pipeline_manager->get_render_target_data(render_target);

    u32 uses_color_target = static_cast<u32>(pipe.create_info.color_target.format != VK_FORMAT_UNDEFINED);
    u32 uses_depth_target = static_cast<u32>(pipe.create_info.depth_target.format != VK_FORMAT_UNDEFINED);
    if(uses_color_target) {
        resource_manager->update_image_layout(rt.)
    }
    if(uses_depth_target) {

    }
     */
}

void Renderer::begin_compute_pipeline(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline) const {
    vkCmdBindPipeline(
        command_manager->get_command_list_data(command_list).command_buffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline_manager->get_compute_pipeline_data(pipeline).pipeline
    );
}

void Renderer::push_graphics_constants(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, const void* data) const {
    const GraphicsPipeline& pipe = pipeline_manager->get_graphics_pipeline_data(pipeline);

    vkCmdPushConstants(
        command_manager->get_command_list_data(command_list).command_buffer,
        pipe.layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0U,
        pipe.create_info.push_constants_size,
        data
    );
}

void Renderer::push_compute_constants(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline, const void *data) const {
    const ComputePipeline& pipe = pipeline_manager->get_compute_pipeline_data(pipeline);

    vkCmdPushConstants(
        command_manager->get_command_list_data(command_list).command_buffer,
        pipe.layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0U,
        pipe.create_info.push_constants_size,
        data
    );
}

void Renderer::wait_for_device_idle() const {
    vkDeviceWaitIdle(instance->get_device());
}

void Renderer::dispatch_compute_pipeline(Handle<CommandList> command_list, glm::uvec3 groups) const {
    if(groups.x == 0) groups.x = 1;
    if(groups.y == 0) groups.y = 1;
    if(groups.z == 0) groups.z = 1;

    vkCmdDispatch(command_manager->get_command_list_data(command_list).command_buffer, groups.x, groups.y, groups.z);
}

void Renderer::bind_graphics_descriptor(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<Descriptor> descriptor, u32 dst_index) const {
    const GraphicsPipeline& pipe = pipeline_manager->get_graphics_pipeline_data(pipeline);
    const Descriptor& desc = resource_manager->get_descriptor_data(descriptor);

    vkCmdBindDescriptorSets(command_manager->get_command_list_data(command_list).command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout, 0U, 1U, &desc.set, 0U, nullptr);
}
void Renderer::bind_compute_descriptor(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline, Handle<Descriptor> descriptor, u32 dst_index) const {
    const ComputePipeline& pipe = pipeline_manager->get_compute_pipeline_data(pipeline);
    const Descriptor& desc = resource_manager->get_descriptor_data(descriptor);

    vkCmdBindDescriptorSets(command_manager->get_command_list_data(command_list).command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.layout, 0U, 1U, &desc.set, 0U, nullptr);
}

void Renderer::draw_count(Handle<CommandList> command_list, u32 vertex_count, u32 first_vertex, u32 instance_count) const {
    vkCmdDraw(command_manager->get_command_list_data(command_list).command_buffer, vertex_count, instance_count, first_vertex, 0U);
}