#ifndef GEMINO_BUFFER_MANAGER_HPP
#define GEMINO_BUFFER_MANAGER_HPP

#include <unordered_map>
#include <vk_mem_alloc.h>
#include <common/types.hpp>

struct Buffer {
    VkBuffer buffer{};
    VmaAllocation allocation{};

    VkDeviceSize size{};

    VkBufferUsageFlags usage_flags{};
};

struct BufferCreateInfo {
    VkDeviceSize size{};
    VkBufferUsageFlags buffer_usage_flags{};
    VmaMemoryUsage memory_usage_flags{};
};

class BufferManager {
public:
    BufferManager(VkDevice device, VmaAllocator allocator);
    ~BufferManager();

    BufferManager& operator=(const BufferManager& other) = delete;
    BufferManager& operator=(BufferManager&& other) noexcept = delete;

    Handle<Buffer> create_buffer(const BufferCreateInfo& info);

    void destroy_buffer(Handle<Buffer> buffer_handle);

    const Buffer& get_buffer_data(Handle<Buffer> buffer_handle) const;

private:
    const VkDevice vk_device;
    const VmaAllocator vk_allocator;

    std::unordered_map<Handle<Buffer>, Buffer> buffer_map{};

    u32 allocated_buffers_count{};
};

#endif