#ifndef GEMINO_RESOURCE_MANAGER_HPP
#define GEMINO_RESOURCE_MANAGER_HPP

#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#include <vk_mem_alloc.h>
#include <unordered_map>

#include <common/utils.hpp>
#include <common/types.hpp>
#include <common/handle_allocator.hpp>
#include <common/range_allocator.hpp>

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

struct Image {
    VkImage image{};
    VkImageType image_type{};

    VkImageView view{};
    VkImageViewType view_type{};

    VkFormat format{};
    VkExtent3D extent{};

    VkImageUsageFlags usage_flags{};
    VkImageAspectFlags aspect_flags{};
    VkImageCreateFlags create_flags{};

    u32 mip_level_count{};
    u32 array_layer_count{};

    std::vector<VkImageView> per_mip_views{};

    VmaAllocation allocation{};
};
struct ImageCreateInfo {
    VkFormat format{};
    VkExtent3D extent{};

    VkImageUsageFlags usage_flags{};
    VkImageAspectFlags aspect_flags{};
    VkImageCreateFlags create_flags{};

    u32 mip_level_count = 1U;
    u32 array_layer_count = 1U;

    bool create_per_mip_views = false;
};

struct SamplerCreateInfo {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerReductionMode reduction_mode = VK_SAMPLER_REDUCTION_MODE_MAX_ENUM;
    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    float max_mipmap = VK_LOD_CLAMP_NONE;
    float mipmap_bias{};
    float anisotropy{};
};
struct Sampler {
    VkSampler sampler{};
};

struct DescriptorBindingUpdateInfo{
    u32 binding_index{};
    u32 array_index{};

    struct {
        Handle<Image> image_handle = INVALID_HANDLE;
        Handle<Sampler> image_sampler = INVALID_HANDLE;
        u32 image_mip = INVALID_HANDLE;
    } image_info;

    struct {
        Handle<Buffer> buffer_handle = INVALID_HANDLE;
    } buffer_info;
};
struct DescriptorUpdateInfo{
    std::vector<DescriptorBindingUpdateInfo> bindings{};
};
struct DescriptorBindingCreateInfo {
    VkDescriptorType descriptor_type{};
    u32 count = 1U;
};
struct DescriptorCreateInfo {
    std::vector<DescriptorBindingCreateInfo> bindings{};
};
struct Descriptor {
    VkDescriptorSet set{};
    VkDescriptorSetLayout layout{};

    DescriptorCreateInfo create_info{};
};

struct RenderTargetCreateInfo {
    Handle<Image> color_target_handle = INVALID_HANDLE;
    u32 color_target_mip = 0U;

    Handle<Image> depth_target_handle = INVALID_HANDLE;
    u32 depth_target_mip = 0U;
};
struct RenderTarget {
    VkFramebuffer framebuffer{};
    VkExtent2D extent{};

    VkImageView color_view{};
    VkImageView depth_view{};

    Handle<Image> color_handle = INVALID_HANDLE;
    Handle<Image> depth_handle = INVALID_HANDLE;
};
struct RenderTargetCommonInfo {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
};

struct GraphicsPipelineCreateInfo {
    std::string vertex_shader_path{};
    std::string fragment_shader_path{};

    std::vector<u32> vertex_constant_values{};
    std::vector<u32> fragment_constant_values{};

    u8 push_constants_size{};

    std::vector<Handle<Descriptor>> descriptors{};

    RenderTargetCommonInfo color_target{};
    RenderTargetCommonInfo depth_target{};

    bool enable_depth_test = false;
    bool enable_depth_write = false;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;

    bool enable_blending = false;
    VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
};
struct GraphicsPipeline {
    VkPipeline pipeline{};
    VkPipelineLayout layout{};
    VkRenderPass render_pass{};

    GraphicsPipelineCreateInfo create_info{};
};

struct ComputePipelineCreateInfo {
    std::string shader_path{};
    std::vector<u32> shader_constant_values{};

    u8 push_constants_size{};

    std::vector<Handle<Descriptor>> descriptors{};
};
struct ComputePipeline {
    VkPipeline pipeline{};
    VkPipelineLayout layout{};

    ComputePipelineCreateInfo create_info{};
};


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

class ResourceManager {
public:
    ResourceManager(VkDevice device, VmaAllocator allocator, u32 graphics_family_index, u32 transfer_family_index, u32 compute_family_index);
    ~ResourceManager();

    ResourceManager &operator=(const ResourceManager &other) = delete;
    ResourceManager &operator=(ResourceManager &&other) noexcept = delete;

    Handle<Image> create_image(const ImageCreateInfo &info);
    Handle<Image> create_image_borrowed(VkImage borrowed_image, VkImageView borrowed_view, const ImageCreateInfo &info);
    Handle<Buffer> create_buffer(const BufferCreateInfo &info);
    Handle<Descriptor> create_descriptor(const DescriptorCreateInfo &info);
    Handle<Sampler> create_sampler(const SamplerCreateInfo &info);
    Handle<GraphicsPipeline> create_graphics_pipeline(const GraphicsPipelineCreateInfo &info);
    Handle<ComputePipeline> create_compute_pipeline(const ComputePipelineCreateInfo &info);
    Handle<RenderTarget> create_render_target(Handle<GraphicsPipeline> src_pipeline, const RenderTargetCreateInfo &info);
    Handle<Query> create_query(QueryType q_type);
    Handle<CommandList> create_command_list(QueueFamily family);
    Handle<Fence> create_fence(bool signaled = true);
    Handle<Semaphore> create_semaphore();

    void *map_buffer(Handle<Buffer> buffer_handle);
    void unmap_buffer(Handle<Buffer> buffer_handle);
    void flush_mapped_buffer(Handle<Buffer> buffer_handle, VkDeviceSize size = 0, VkDeviceSize offset = 0);

    void memcpy_to_buffer_once(Handle<Buffer> buffer_handle, const void *src_data, usize size, usize dst_offset = 0, usize src_offset = 0);

    // Requires an already mapped buffer
    void memcpy_to_buffer(void *dst_mapped_buffer, const void *src_data, usize size, usize dst_offset = 0, usize src_offset = 0);

    void update_descriptor(Handle<Descriptor> descriptor_handle, const DescriptorUpdateInfo &info);

    void destroy(Handle<Image> image_handle);
    void destroy(Handle<Buffer> buffer_handle);
    void destroy(Handle<Descriptor> descriptor_handle);
    void destroy(Handle<Sampler> sampler_handle);
    void destroy(Handle<Query> query_handle);
    void destroy(Handle<CommandList> command_list_handle);
    void destroy(Handle<Fence> fence_handle);
    void destroy(Handle<Semaphore> semaphore_handle);
    void destroy(Handle<GraphicsPipeline> pipeline_handle);
    void destroy(Handle<ComputePipeline> pipeline_handle);
    void destroy(Handle<RenderTarget> rt_handle);

    [[nodiscard]] const Image &get_data(Handle<Image> image_handle) const;
    [[nodiscard]] const Buffer &get_data(Handle<Buffer> buffer_handle) const;
    [[nodiscard]] const Descriptor &get_data(Handle<Descriptor> descriptor_handle) const;
    [[nodiscard]] const Sampler &get_data(Handle<Sampler> sampler_handle) const;
    [[nodiscard]] const Query &get_data(Handle<Query> query_handle) const;
    [[nodiscard]] const CommandList &get_data(Handle<CommandList> command_list_handle) const;
    [[nodiscard]] const Fence &get_data(Handle<Fence> fence_handle) const;
    [[nodiscard]] const Semaphore &get_data(Handle<Semaphore> semaphore_handle) const;
    [[nodiscard]] const GraphicsPipeline &get_data(Handle<GraphicsPipeline> pipeline_handle) const;
    [[nodiscard]] const ComputePipeline &get_data(Handle<ComputePipeline> pipeline_handle) const;
    [[nodiscard]] const RenderTarget &get_data(Handle<RenderTarget> rt_handle) const;

    [[nodiscard]] const VkQueryPool &get_query_pool(QueryType query_type) const;
    [[nodiscard]] VkDescriptorPool get_descriptor_pool() const { return m_descriptor_pool; }

private:
    const VkDevice VK_DEVICE;
    const VmaAllocator VK_ALLOCATOR;

    VkShaderModule create_shader_module(const std::string& path);
    VkDescriptorPool m_descriptor_pool{};

    HandleAllocator<Buffer> m_buffer_allocator{};
    HandleAllocator<Image> m_image_allocator{};
    HandleAllocator<Descriptor> m_descriptor_allocator{};
    HandleAllocator<Sampler> m_sampler_allocator{};

    std::unordered_map<QueueFamily, VkCommandPool> m_command_pools{};
    std::unordered_map<QueryType, VkQueryPool> m_query_pools{};
    std::unordered_map<QueryType, HandleAllocator<u32>> m_query_id_allocators{};

    HandleAllocator<CommandList> m_command_list_allocator{};
    HandleAllocator<Fence> m_fence_allocator{};
    HandleAllocator<Semaphore> m_semaphore_allocator{};

    HandleAllocator<Query> m_query_allocator{};

    HandleAllocator<GraphicsPipeline> m_graphics_pipeline_allocator{};
    HandleAllocator<ComputePipeline> m_compute_pipeline_allocator{};
    HandleAllocator<RenderTarget> m_render_target_allocator{};
};

#endif
