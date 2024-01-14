#include "buffer_manager.hpp"
#include <common/debug.hpp>

BufferManager::BufferManager(VkDevice device, VmaAllocator allocator)
    : vk_allocator(allocator), vk_device(device) {

}
BufferManager::~BufferManager() {
    for(const auto& handle : handle_allocator.get_valid_handles_copy()) {
        destroy_buffer(handle);
    }
}

Handle<Buffer> BufferManager::create_buffer(const BufferCreateInfo &info) {
    Buffer buffer{
        .size = info.size,
        .usage_flags = info.buffer_usage_flags,
    };

    VkBufferCreateInfo buffer_create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer.size,
        .usage = buffer.usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocation_create_info{
        .usage = info.memory_usage_flags
    };

    DEBUG_ASSERT(vmaCreateBuffer(vk_allocator, &buffer_create_info, &allocation_create_info, &buffer.buffer, &buffer.allocation, nullptr) == VK_SUCCESS)

    return handle_allocator.alloc(buffer);
}

void BufferManager::destroy_buffer(Handle<Buffer> buffer_handle) {
    if (!handle_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot delete buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }

    const Buffer& buffer = handle_allocator.get_element(buffer_handle);

    vmaDestroyBuffer(vk_allocator, buffer.buffer, buffer.allocation);

    handle_allocator.free(buffer_handle);
}

const Buffer &BufferManager::get_buffer_data(Handle<Buffer> buffer_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!handle_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot get buffer data - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    return handle_allocator.get_element(buffer_handle);
}
