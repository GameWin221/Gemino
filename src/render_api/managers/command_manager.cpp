#include "command_manager.hpp"
#include <unordered_set>
#include <vector>

#include <common/debug.hpp>

CommandManager::CommandManager(VkDevice device, u32 graphics_family_index, u32 transfer_family_index, u32 compute_family_index)
    : VK_DEVICE(device) {

    // This map contains which index handles which families
    // (in a non-perfect scenario one index might handle more than one family at once)
    std::unordered_map<u32, std::vector<QueueFamily>> queue_families{};
    queue_families[graphics_family_index].emplace_back(QueueFamily::Graphics);
    queue_families[transfer_family_index].emplace_back(QueueFamily::Transfer);
    queue_families[compute_family_index].emplace_back(QueueFamily::Compute);

    // Create VkCommandPools for each unique family
    for(const auto& [index, families] : queue_families) {
        VkCommandPool command_pool{};
        VkCommandPoolCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = index,
        };

        DEBUG_ASSERT(vkCreateCommandPool(VK_DEVICE, &create_info, nullptr, &command_pool) == VK_SUCCESS)

        // Duplicate the pool for indices that handle more than one family at once
        for(const auto& family : families) {
            m_command_pools[family] = command_pool;
        }
    }
}
CommandManager::~CommandManager() {
    std::unordered_set<VkCommandPool> unique_command_pools{};
    for(const auto &[family, pool] : m_command_pools) {
        unique_command_pools.insert(pool);
    }
    for(const auto &pool : unique_command_pools) {
        vkDestroyCommandPool(VK_DEVICE, pool, nullptr);
    }

    for(const auto &handle : m_fence_allocator.get_valid_handles_copy()) {
        destroy_fence(handle);
    }
    for(const auto &handle : m_semaphore_allocator.get_valid_handles_copy()) {
        destroy_semaphore(handle);
    }
}

Handle<CommandList> CommandManager::create_command_list(QueueFamily family) {
    CommandList cmd{
        .family = family
    };

    VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pools.at(family),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1U,
    };

    DEBUG_ASSERT(vkAllocateCommandBuffers(VK_DEVICE, &allocate_info, &cmd.command_buffer) == VK_SUCCESS)

    return m_command_list_allocator.alloc(cmd);
}
Handle<Fence> CommandManager::create_fence(bool signaled) {
    Fence fence{};

    VkFenceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = (signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0U),
    };

    DEBUG_ASSERT(vkCreateFence(VK_DEVICE, &info, nullptr, &fence.fence) == VK_SUCCESS)

    return m_fence_allocator.alloc(fence);
}
Handle<Semaphore> CommandManager::create_semaphore() {
    Semaphore semaphore{};

    VkSemaphoreCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    DEBUG_ASSERT(vkCreateSemaphore(VK_DEVICE, &info, nullptr, &semaphore.semaphore) == VK_SUCCESS)

    return m_semaphore_allocator.alloc(semaphore);
}

void CommandManager::destroy_command_list(Handle<CommandList> command_list_handle) {
    if (!m_command_list_allocator.is_handle_valid(command_list_handle)) {
        DEBUG_PANIC("Cannot delete command list - Command list with a handle id: = " << command_list_handle << ", does not exist!")
    }

    const CommandList &cmd = m_command_list_allocator.get_element(command_list_handle);

    vkFreeCommandBuffers(VK_DEVICE, m_command_pools.at(cmd.family), 1U, &cmd.command_buffer);

    m_command_list_allocator.free(command_list_handle);
}
void CommandManager::destroy_fence(Handle<Fence> fence_handle) {
    if (!m_fence_allocator.is_handle_valid(fence_handle)) {
        DEBUG_PANIC("Cannot delete fence - Fence with a handle id: = " << fence_handle << ", does not exist!")
    }

    const Fence &fence = m_fence_allocator.get_element(fence_handle);

    vkDestroyFence(VK_DEVICE, fence.fence, nullptr);

    m_fence_allocator.free(fence_handle);
}
void CommandManager::destroy_semaphore(Handle<Semaphore> semaphore_handle) {
    if (!m_semaphore_allocator.is_handle_valid(semaphore_handle)) {
        DEBUG_PANIC("Cannot delete semaphore - Semaphore with a handle id: = " << semaphore_handle << ", does not exist!")
    }

    const Semaphore &semaphore = m_semaphore_allocator.get_element(semaphore_handle);

    vkDestroySemaphore(VK_DEVICE, semaphore.semaphore, nullptr);

    m_semaphore_allocator.free(semaphore_handle);
}

const CommandList &CommandManager::get_command_list_data(Handle<CommandList> command_list_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_command_list_allocator.is_handle_valid(command_list_handle)) {
        DEBUG_PANIC("Cannot get command list - Command list with a handle id: = " << command_list_handle << ", does not exist!")
    }
#endif

    return m_command_list_allocator.get_element(command_list_handle);
}
const Fence &CommandManager::get_fence_data(Handle<Fence> fence_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_fence_allocator.is_handle_valid(fence_handle)) {
        DEBUG_PANIC("Cannot get fence - Fence with a handle id: = " << fence_handle << ", does not exist!")
    }
#endif

    return m_fence_allocator.get_element(fence_handle);
}
const Semaphore &CommandManager::get_semaphore_data(Handle<Semaphore> semaphore_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_semaphore_allocator.is_handle_valid(semaphore_handle)) {
        DEBUG_PANIC("Cannot get semaphore - Semaphore with a handle id: = " << semaphore_handle << ", does not exist!")
    }
#endif

    return m_semaphore_allocator.get_element(semaphore_handle);
}
