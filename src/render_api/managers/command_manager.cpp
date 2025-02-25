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
        VkCommandPoolCreateInfo cmd_pool_create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = index,
        };

        DEBUG_ASSERT(vkCreateCommandPool(VK_DEVICE, &cmd_pool_create_info, nullptr, &command_pool) == VK_SUCCESS)

        // Duplicate the pool for indices that handle more than one family at once
        for(const auto& family : families) {
            m_command_pools[family] = command_pool;
        }
    }

    VkQueryPool query_pool{};
    VkQueryPoolCreateInfo query_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO
    };

    query_pool_create_info.queryType = static_cast<VkQueryType>(QueryType::Timestamp);
    query_pool_create_info.queryCount = 128u;
    DEBUG_ASSERT(vkCreateQueryPool(VK_DEVICE, &query_pool_create_info, nullptr, &query_pool) == VK_SUCCESS)
    m_query_pools[QueryType::Timestamp] = query_pool;
    m_query_id_allocators[QueryType::Timestamp] = HandleAllocator<u32>{};

    query_pool_create_info.queryType = static_cast<VkQueryType>(QueryType::Occlusion);
    query_pool_create_info.queryCount = 16u;
    DEBUG_ASSERT(vkCreateQueryPool(VK_DEVICE, &query_pool_create_info, nullptr, &query_pool) == VK_SUCCESS)
    m_query_pools[QueryType::Occlusion] = query_pool;
    m_query_id_allocators[QueryType::Occlusion] = HandleAllocator<u32>{};

    query_pool_create_info.queryType = static_cast<VkQueryType>(QueryType::PipelineStatistics);
    query_pool_create_info.queryCount = 16u;
    query_pool_create_info.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

    DEBUG_ASSERT(vkCreateQueryPool(VK_DEVICE, &query_pool_create_info, nullptr, &query_pool) == VK_SUCCESS)
    m_query_pools[QueryType::PipelineStatistics] = query_pool;
    m_query_id_allocators[QueryType::PipelineStatistics] = HandleAllocator<u32>{};
}
CommandManager::~CommandManager() {
    for(const auto &handle : m_query_allocator.get_valid_handles()) {
        destroy_query(handle);
    }

    for(const auto &[query_type, pool] : m_query_pools) {
        vkDestroyQueryPool(VK_DEVICE, pool, nullptr);
    }

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

Handle<Query> CommandManager::create_query(QueryType q_type) {
    Query query{
        .query_type = q_type
    };

    query.local_id = m_query_id_allocators[q_type].alloc(0u);

    return m_query_allocator.alloc(query);
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

void CommandManager::destroy_query(Handle<Query> query_handle) {
    if (!m_query_allocator.is_handle_valid(query_handle)) {
        DEBUG_PANIC("Cannot delete query - Query with a handle id: = " << query_handle << ", does not exist!")
    }

    const Query &query = m_query_allocator.get_element(query_handle);

    m_query_id_allocators[query.query_type].free(query.local_id);
    m_query_allocator.free(query_handle);
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

const Query &CommandManager::get_query_data(Handle<Query> query_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!m_query_allocator.is_handle_valid(query_handle)) {
        DEBUG_PANIC("Cannot get query - Query with a handle id: = " << query_handle << ", does not exist!")
    }
#endif

    return m_query_allocator.get_element(query_handle);
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

const VkQueryPool &CommandManager::get_query_pool(QueryType query_type) const {
    return m_query_pools.at(query_type);
}
