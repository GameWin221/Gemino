#ifndef GEMINO_COMMAND_MANAGER_HPP
#define GEMINO_COMMAND_MANAGER_HPP

#include <unordered_map>

#include <vulkan/vulkan.h>
#include <common/types.hpp>
#include <common/handle_allocator.hpp>

enum struct QueueFamily : u32 {
    Graphics = 0U,
    Transfer,
    Compute
};

struct CommandList {
    VkCommandBuffer command_buffer{};

    QueueFamily family{};
};

struct Fence {
    VkFence fence{};
};
struct Semaphore {
    VkSemaphore semaphore{};
};

class CommandManager {
public:
    CommandManager(VkDevice device, u32 graphics_family_index, u32 transfer_family_index, u32 compute_family_index);
    ~CommandManager();

    CommandManager &operator=(const CommandManager &other) = delete;
    CommandManager &operator=(CommandManager &&other) noexcept = delete;

    Handle<CommandList> create_command_list(QueueFamily family);
    Handle<Fence> create_fence(bool signaled = true);
    Handle<Semaphore> create_semaphore();

    void destroy_command_list(Handle<CommandList> command_list_handle);
    void destroy_fence(Handle<Fence> fence_handle);
    void destroy_semaphore(Handle<Semaphore> semaphore_handle);

    const CommandList &get_command_list_data(Handle<CommandList> command_list_handle) const;
    const Fence &get_fence_data(Handle<Fence> fence_handle) const;
    const Semaphore &get_semaphore_data(Handle<Semaphore> semaphore_handle) const;

private:
    const VkDevice VK_DEVICE;

    std::unordered_map<QueueFamily, VkCommandPool> m_command_pools{};

    HandleAllocator<CommandList> m_command_list_allocator{};
    HandleAllocator<Fence> m_fence_allocator{};
    HandleAllocator<Semaphore> m_semaphore_allocator{};
};

#endif