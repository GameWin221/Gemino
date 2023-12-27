#include "buffer_manager.hpp"
#include <common/debug.hpp>

BufferManager::BufferManager(VkDevice device, VmaAllocator allocator)
    : vk_allocator(allocator), vk_device(device) {

}
BufferManager::~BufferManager() {
    for(const auto& [handle, buffer] : buffer_map) {
        vmaDestroyBuffer(vk_allocator, buffer.buffer, buffer.allocation);
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

    auto handle = static_cast<Handle<Buffer>>((allocated_buffers_count++));

    buffer_map[handle] = buffer;

    return handle;
}

void BufferManager::destroy_buffer(Handle<Buffer> buffer_handle) {
    if (!buffer_map.contains(buffer_handle)) {
        DEBUG_PANIC("Cannot delete buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }

    const Buffer& buffer = buffer_map.at(buffer_handle);

    vmaDestroyBuffer(vk_allocator, buffer.buffer, buffer.allocation);

    buffer_map.erase(buffer_handle);
}

const Buffer &BufferManager::get_buffer_data(Handle<Buffer> buffer_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!buffer_map.contains(buffer_handle)) {
        DEBUG_PANIC("Cannot get buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    return buffer_map.at(buffer_handle);
}
