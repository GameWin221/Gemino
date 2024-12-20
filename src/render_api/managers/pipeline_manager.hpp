#ifndef GEMINO_PIPELINE_MANAGER_HPP
#define GEMINO_PIPELINE_MANAGER_HPP

#include <string>
#include <vulkan/vulkan.h>
#include <render_api/managers/resource_manager.hpp>
#include <common/handle_allocator.hpp>
#include <common/types.hpp>

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

class PipelineManager {
public:
    PipelineManager(VkDevice device, const ResourceManager *resource_manager_ptr);
    ~PipelineManager();

    PipelineManager &operator=(const PipelineManager &other) = delete;
    PipelineManager &operator=(PipelineManager &&other) noexcept = delete;

    Handle<GraphicsPipeline> create_graphics_pipeline(const GraphicsPipelineCreateInfo &info);
    Handle<ComputePipeline> create_compute_pipeline(const ComputePipelineCreateInfo &info);
    Handle<RenderTarget> create_render_target(Handle<GraphicsPipeline> src_pipeline, const RenderTargetCreateInfo &info);

    void destroy_graphics_pipeline(Handle<GraphicsPipeline> pipeline_handle);
    void destroy_compute_pipeline(Handle<ComputePipeline> pipeline_handle);
    void destroy_render_target(Handle<RenderTarget> rt_handle);

    const GraphicsPipeline &get_graphics_pipeline_data(Handle<GraphicsPipeline> pipeline_handle) const;
    const ComputePipeline &get_compute_pipeline_data(Handle<ComputePipeline> pipeline_handle) const;
    const RenderTarget &get_render_target_data(Handle<RenderTarget> rt_handle) const;

private:
    VkShaderModule create_shader_module(const std::string& path);

    const VkDevice VK_DEVICE;

    const ResourceManager *const m_resource_manager_ptr; // ResourceManager is guaranteed to live as long as PipelineManager

    HandleAllocator<GraphicsPipeline> m_graphics_pipeline_allocator{};
    HandleAllocator<ComputePipeline> m_compute_pipeline_allocator{};
    HandleAllocator<RenderTarget> m_render_target_allocator{};
};


#endif //GEMINO_PIPELINE_MANAGER_HPP
