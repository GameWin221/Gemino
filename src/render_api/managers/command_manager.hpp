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
enum struct QueryType : u32 {
    Timestamp = VK_QUERY_TYPE_TIMESTAMP,
    Occlusion = VK_QUERY_TYPE_OCCLUSION,
    PipelineStatistics = VK_QUERY_TYPE_PIPELINE_STATISTICS,
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
struct Query {
    Handle<u32> local_id{};

    QueryType query_type{};
};
struct QueryPipelineStatisticsResults {
    u32 input_assembly_vertices = UINT32_MAX;
    u32 input_assembly_primitives = UINT32_MAX;
    u32 vertex_shader_invocations = UINT32_MAX;
    u32 geometry_shader_invocations = UINT32_MAX;
    u32 geometry_shader_primitives = UINT32_MAX;
    u32 clipping_invocations = UINT32_MAX;
    u32 clipping_primitives = UINT32_MAX;
    u32 fragment_shader_invocations = UINT32_MAX;
    u32 tessellation_control_shader_patches = UINT32_MAX;
    u32 tessellation_evaluation_shader_invocations = UINT32_MAX;
    u32 compute_shader_invocations = UINT32_MAX;
};

class CommandManager {
public:
    CommandManager(VkDevice device, u32 graphics_family_index, u32 transfer_family_index, u32 compute_family_index);
    ~CommandManager();

    CommandManager &operator=(const CommandManager &other) = delete;
    CommandManager &operator=(CommandManager &&other) noexcept = delete;

    Handle<Query> create_query(QueryType q_type);
    Handle<CommandList> create_command_list(QueueFamily family);
    Handle<Fence> create_fence(bool signaled = true);
    Handle<Semaphore> create_semaphore();

    void destroy_query(Handle<Query> query_handle);
    void destroy_command_list(Handle<CommandList> command_list_handle);
    void destroy_fence(Handle<Fence> fence_handle);
    void destroy_semaphore(Handle<Semaphore> semaphore_handle);

    const Query &get_query_data(Handle<Query> query_handle) const;
    const CommandList &get_command_list_data(Handle<CommandList> command_list_handle) const;
    const Fence &get_fence_data(Handle<Fence> fence_handle) const;
    const Semaphore &get_semaphore_data(Handle<Semaphore> semaphore_handle) const;
    const VkQueryPool &get_query_pool(QueryType query_type) const;

private:
    const VkDevice VK_DEVICE;

    std::unordered_map<QueueFamily, VkCommandPool> m_command_pools{};
    std::unordered_map<QueryType, VkQueryPool> m_query_pools{};
    std::unordered_map<QueryType, HandleAllocator<u32>> m_query_id_allocators{};

    HandleAllocator<CommandList> m_command_list_allocator{};
    HandleAllocator<Fence> m_fence_allocator{};
    HandleAllocator<Semaphore> m_semaphore_allocator{};

    HandleAllocator<Query> m_query_allocator{};
};

#endif