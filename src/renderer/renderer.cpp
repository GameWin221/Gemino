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

        u32 level_count;
        if(barrier.level_count_override != 0) {
            level_count = barrier.level_count_override;
        } else {
            level_count = image.mip_level_count;
        }

        u32 layer_count;
        if(barrier.layer_count_override != 0) {
            layer_count = barrier.layer_count_override;
        } else {
            layer_count = image.array_layer_count;
        }

        if(barrier.base_level_override + level_count > image.mip_level_count) {
            DEBUG_PANIC("Invalid image barrier! - Image barrier[" << i << "] base_level + level_count is out of range, base_level = " << barrier.base_level_override << ", level_count = " << level_count << ", image.mip_level_count = " << image.mip_level_count)
        }

        if(barrier.base_array_layer_override + layer_count > image.array_layer_count) {
            DEBUG_PANIC("Invalid image barrier! - Image barrier[" << i << "] base_array_layer + array_layer_count is out of range, base_array_layer = " << barrier.base_array_layer_override << ", array_layer_count = " << layer_count << ", image.array_layer_count = " << image.array_layer_count)
        }

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
                .baseMipLevel = barrier.base_level_override,
                .levelCount = level_count,
                .baseArrayLayer = barrier.base_array_layer_override,
                .layerCount = layer_count,
            }
        };
    }

    vkCmdPipelineBarrier(command_manager->get_command_list_data(command_list).command_buffer, src_stage, dst_stage, 0U, 0U, nullptr, 0U, nullptr, static_cast<u32>(image_barriers.size()), image_barriers.data());
}
void Renderer::buffer_barrier(Handle<CommandList> command_list, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, const std::vector<BufferBarrier> &barriers) const {
    std::vector<VkBufferMemoryBarrier> buffer_barriers(barriers.size());
    for(usize i{}; i < barriers.size(); ++i) {
        const auto& barrier = barriers[i];
        const auto& buffer = resource_manager->get_buffer_data(barrier.buffer_handle);

        VkDeviceSize size;
        if(barrier.size_override != 0) {
            size = barrier.size_override;
        } else {
            size = buffer.size;
        }

        if(barrier.offset_override + size > buffer.size) {
            DEBUG_PANIC("Invalid buffer barrier! - Buffer barrier[" << i << "] offset + size is out of range, offset = " << barrier.offset_override << ", size = " << size << ", buffer.size = " << buffer.size)
        }

        buffer_barriers[i] = VkBufferMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = barrier.src_access_mask,
            .dstAccessMask = barrier.dst_access_mask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer.buffer,
            .offset = barrier.offset_override,
            .size = size,
        };
    }
    vkCmdPipelineBarrier(command_manager->get_command_list_data(command_list).command_buffer, src_stage, dst_stage, 0U, 0U, nullptr, static_cast<u32>(buffer_barriers.size()), buffer_barriers.data(), 0U, nullptr);
}

void Renderer::blit_image(Handle<CommandList> command_list, Handle<Image> src_image_handle, VkImageLayout src_image_layout, Handle<Image> dst_image_handle, VkImageLayout dst_image_layout, VkFilter filter, const std::vector<ImageBlit>& blits) const {
    const Image& src_image = resource_manager->get_image_data(src_image_handle);
    const Image& dst_image = resource_manager->get_image_data(dst_image_handle);

#if DEBUG_MODE
    if(!(instance->get_format_properties(src_image.format).optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        DEBUG_PANIC("Failed to blit! - Source image's format does not support linear blitting, format = " << src_image.format)
    }

    if(!(instance->get_format_properties(dst_image.format).optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)){
        DEBUG_PANIC("Failed to blit! - Destination image's format does not support linear blitting, format = " << dst_image.format)
    }
#endif

    std::vector<VkImageBlit> image_blits(blits.size());
    for(usize i{}; i < image_blits.size(); ++i) {
        const auto& blit = blits[i];

        VkOffset3D src_lower_bounds{
            static_cast<i32>(blit.src_lower_bounds_override.width),
            static_cast<i32>(blit.src_lower_bounds_override.height),
            static_cast<i32>(blit.src_lower_bounds_override.depth)
        };
        VkOffset3D dst_lower_bounds{
            static_cast<i32>(blit.dst_lower_bounds_override.width),
            static_cast<i32>(blit.dst_lower_bounds_override.height),
            static_cast<i32>(blit.dst_lower_bounds_override.depth)
        };

        VkOffset3D src_upper_bounds;
        VkOffset3D dst_upper_bounds;
        if(blit.src_upper_bounds_override.width == 0 && blit.src_upper_bounds_override.height == 0 && blit.src_upper_bounds_override.depth == 0) {
            src_upper_bounds.x = static_cast<i32>(src_image.extent.width);
            src_upper_bounds.y = static_cast<i32>(src_image.extent.height);
            src_upper_bounds.z = static_cast<i32>(src_image.extent.depth);
        } else {
            src_upper_bounds.x = std::max(static_cast<i32>(blit.src_upper_bounds_override.width), 1);
            src_upper_bounds.y = std::max(static_cast<i32>(blit.src_upper_bounds_override.height), 1);
            src_upper_bounds.z = std::max(static_cast<i32>(blit.src_upper_bounds_override.depth), 1);
        }

        if(blit.dst_upper_bounds_override.width == 0 && blit.dst_upper_bounds_override.height == 0 && blit.dst_upper_bounds_override.depth == 0) {
            dst_upper_bounds.x = static_cast<i32>(dst_image.extent.width);
            dst_upper_bounds.y = static_cast<i32>(dst_image.extent.height);
            dst_upper_bounds.z = static_cast<i32>(dst_image.extent.depth);
        } else {
            dst_upper_bounds.x = std::max(static_cast<i32>(blit.dst_upper_bounds_override.width), 1);
            dst_upper_bounds.y = std::max(static_cast<i32>(blit.dst_upper_bounds_override.height), 1);
            dst_upper_bounds.z = std::max(static_cast<i32>(blit.dst_upper_bounds_override.depth), 1);
        }

        u32 src_layer_count;
        if(blit.src_layer_count_override != 0) {
            src_layer_count = blit.src_layer_count_override;
        } else {
            src_layer_count = src_image.array_layer_count;
        }

        if(blit.src_base_array_layer_override + src_layer_count > src_image.array_layer_count) {
            DEBUG_PANIC("Invalid image blit! - Image blit[" << i << "] src_base_array_layer_override + src_layer_count is out of range, src_base_array_layer_override = " << blit.src_base_array_layer_override << ", src_layer_count = " << src_layer_count << ", src_image.array_layer_count = " << src_image.array_layer_count)
        }

        u32 dst_layer_count;
        if(blit.dst_layer_count_override != 0) {
            dst_layer_count = blit.dst_layer_count_override;
        } else {
            dst_layer_count = dst_image.array_layer_count;
        }

        if(blit.dst_base_array_layer_override + dst_layer_count > dst_image.array_layer_count) {
            DEBUG_PANIC("Invalid image blit! - Image blit[" << i << "] dst_base_array_layer_override + dst_layer_count is out of range, dst_base_array_layer_override = " << blit.dst_base_array_layer_override << ", dst_layer_count = " << dst_layer_count << ", dst_image.array_layer_count = " << dst_image.array_layer_count)
        }

        if(src_layer_count != dst_layer_count) {
            DEBUG_PANIC("Invalid image blit! - Image blit[" << i << "] src_layer_count is not equal to dst_layer_count, src_layer_count = " << src_layer_count << ", dst_layer_count = " << dst_layer_count)
        }

        image_blits[i] = VkImageBlit{
            .srcSubresource {
                .aspectMask = src_image.aspect_flags,
                .mipLevel = blit.src_mipmap_level_override,
                .baseArrayLayer = blit.src_base_array_layer_override,
                .layerCount = src_layer_count
            },
            .srcOffsets {
                src_lower_bounds, src_upper_bounds
            },
            .dstSubresource {
                .aspectMask = dst_image.aspect_flags,
                .mipLevel = blit.dst_mipmap_level_override,
                .baseArrayLayer = blit.dst_base_array_layer_override,
                .layerCount = dst_layer_count,
            },
            .dstOffsets {
                dst_lower_bounds, dst_upper_bounds
            }
        };
    }

    vkCmdBlitImage(
        command_manager->get_command_list_data(command_list).command_buffer,
        src_image.image, src_image_layout, dst_image.image, dst_image_layout,
        static_cast<u32>(image_blits.size()), image_blits.data(),
        filter
    );
}

void Renderer::gen_mipmaps(Handle<CommandList> command_list, Handle<Image> target_image) const {
    const Image& image = resource_manager->get_image_data(target_image);
    const CommandList& cmd = command_manager->get_command_list_data(command_list);

    if(!(instance->get_format_properties(image.format).optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        DEBUG_PANIC("Failed to generate mipmaps! - Image's format does not support linear blitting, format = " << image.format)
    }

    i32 width = static_cast<i32>(image.extent.width);
    i32 height = static_cast<i32>(image.extent.height);

    for(u32 i = 1U; i < image.mip_level_count; ++i) {

    }
}

void Renderer::copy_buffer_to_buffer(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Buffer> dst, const std::vector<VkBufferCopy>& regions) const {
    const Buffer& src_buffer = resource_manager->get_buffer_data(src);
    const Buffer& dst_buffer = resource_manager->get_buffer_data(dst);
    const CommandList& cmd = command_manager->get_command_list_data(command_list);

    vkCmdCopyBuffer(cmd.command_buffer, src_buffer.buffer, dst_buffer.buffer, static_cast<u32>(regions.size()), regions.data());
}
void Renderer::copy_buffer_to_image(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Image> dst, VkImageLayout dst_layout, const std::vector<BufferToImageCopy> &regions) const {
    const Buffer& src_buffer = resource_manager->get_buffer_data(src);
    const Image& dst_image = resource_manager->get_image_data(dst);
    const CommandList& cmd = command_manager->get_command_list_data(command_list);

    std::vector<VkBufferImageCopy> image_copies(regions.size());
    for(usize i{}; i < image_copies.size(); ++i) {
        const auto& region = regions[i];

        u32 layer_count;
        if(region.layer_count_override != 0) {
            layer_count = region.layer_count_override;
        } else {
            layer_count = dst_image.array_layer_count;
        }

        if(region.base_array_layer_override + layer_count > dst_image.array_layer_count) {
            DEBUG_PANIC("Invalid image copy! - Image copy[" << i << "] base_array_layer_override + layer_count is out of range, base_array_layer_override = " << region.base_array_layer_override << ", layer_count = " << layer_count << ", dst_image.array_layer_count = " << dst_image.array_layer_count)
        }

        VkOffset3D offset{
            static_cast<i32>(region.dst_image_offset_override.width),
            static_cast<i32>(region.dst_image_offset_override.height),
            static_cast<i32>(region.dst_image_offset_override.depth)
        };

        VkExtent3D dst_extent;
        if(region.dst_image_extent_override.width == 0U && region.dst_image_extent_override.height == 0U && region.dst_image_extent_override.depth == 0U) {
            dst_extent = dst_image.extent;
        } else {
            dst_extent.width = std::max(region.dst_image_extent_override.width, 1U);
            dst_extent.height = std::max(region.dst_image_extent_override.height, 1U);
            dst_extent.depth = std::max(region.dst_image_extent_override.depth, 1U);
        }

        image_copies[i] = VkBufferImageCopy{
            .bufferOffset = region.src_buffer_offset,
            .imageSubresource {
                .aspectMask = dst_image.aspect_flags,
                .mipLevel = region.mipmap_level_override,
                .baseArrayLayer = region.base_array_layer_override,
                .layerCount = layer_count
            },
            .imageOffset = offset,
            .imageExtent = dst_extent
        };
    }

    vkCmdCopyBufferToImage(cmd.command_buffer, src_buffer.buffer, dst_image.image, dst_layout, static_cast<u32>(image_copies.size()), image_copies.data());
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

void Renderer::push_graphics_constants(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, const void* data, u8 size_override, u8 offset_override) const {
    const GraphicsPipeline& pipe = pipeline_manager->get_graphics_pipeline_data(pipeline);

    u8 size = pipe.create_info.push_constants_size;
    if(size_override != 0U) {
        size = size_override;

#if DEBUG_MODE
        if(size % 4 != 0) {
            DEBUG_PANIC("Push constant override_size must be a multiple of 4!")
        }
        if(size > pipe.create_info.push_constants_size) {
            DEBUG_PANIC("Push constants override_size is greater than the specified push constant size in the graphics pipeline create info!")
        }
#endif
    }

    u8 offset = 0U;
    if(offset_override != 0U) {
        offset = offset_override;

#if DEBUG_MODE
        if(offset % 4 != 0) {
            DEBUG_PANIC("Push constant override_offset must be a multiple of 4!")
        }
        if(offset + size > pipe.create_info.push_constants_size) {
            DEBUG_PANIC("Push constants override_size + offset_override is greater than the specified push constant size in the graphics pipeline create info!")
        }
#endif
    }

    vkCmdPushConstants(
        command_manager->get_command_list_data(command_list).command_buffer,
        pipe.layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        offset,
        size,
        data
    );
}
void Renderer::push_compute_constants(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline, const void *data, u8 size_override, u8 offset_override) const {
    const ComputePipeline& pipe = pipeline_manager->get_compute_pipeline_data(pipeline);

    u8 size = pipe.create_info.push_constants_size;
    if(size_override != 0U) {
        size = size_override;

#if DEBUG_MODE
        if(size % 4 != 0) {
            DEBUG_PANIC("Push constant override_size must be a multiple of 4!")
        }
        if(size > pipe.create_info.push_constants_size) {
            DEBUG_PANIC("Push constants override_size is greater than the specified push constant size in the compute pipeline create info!")
        }
#endif
    }

    u8 offset = 0U;
    if(offset_override != 0U) {
        offset = offset_override;


#if DEBUG_MODE
        if(offset % 4 != 0) {
            DEBUG_PANIC("Push constant override_offset must be a multiple of 4!")
        }
        if(offset + size > pipe.create_info.push_constants_size) {
            DEBUG_PANIC("Push constants override_size + offset_override is greater than the specified push constant size in the compute pipeline create info!")
        }
#endif
    }

    vkCmdPushConstants(
        command_manager->get_command_list_data(command_list).command_buffer,
        pipe.layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        offset,
        size,
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
    vkCmdDraw(command_manager->get_command_list_data(command_list).command_buffer, vertex_count, instance_count,first_vertex, 0U);
}
