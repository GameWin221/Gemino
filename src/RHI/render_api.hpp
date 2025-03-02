#ifndef GEMINO_RENDER_API_HPP
#define GEMINO_RENDER_API_HPP

#include <window/window.hpp>

#include <RHI/resource_manager.hpp>
#include <RHI/instance.hpp>
#include <RHI/swapchain.hpp>

union ClearColor {
    f32 rgba_f32[4];
    u32 rgba_u32[4];
    i32 rgba_i32[4];
};

struct RenderTargetClear {
    ClearColor color{};
    f32 depth = 0.0f;
};

struct SubmitInfo {
    Handle<Fence> fence{};

    std::vector<Handle<Semaphore>> wait_semaphores{};
    std::vector<Handle<Semaphore>> signal_semaphores{};
    std::vector<VkPipelineStageFlags> signal_stages{};
};

struct ImageBarrier {
    Handle<Image> image_handle{};

    VkAccessFlags src_access_mask{};
    VkAccessFlags dst_access_mask{};

    VkImageLayout old_layout{};
    VkImageLayout new_layout{};

    u32 base_mipmap_level_override{};
    u32 mipmap_level_count_override{};
    u32 base_array_layer_override{};
    u32 array_layer_count_override{};
};
struct BufferBarrier {
    Handle<Buffer> buffer_handle{};

    VkAccessFlags src_access_mask{};
    VkAccessFlags dst_access_mask{};

    VkDeviceSize offset_override{};
    VkDeviceSize size_override{};
};
struct ImageBlit {
    VkExtent3D src_lower_bounds_override{};
    VkExtent3D src_upper_bounds_override{};

    VkExtent3D dst_lower_bounds_override{};
    VkExtent3D dst_upper_bounds_override{};

    u32 src_mipmap_level_override{};
    u32 src_base_array_layer_override{};
    u32 src_array_layer_count_override{};

    u32 dst_mipmap_level_override{};
    u32 dst_base_array_layer_override{};
    u32 dst_array_layer_count_override{};
};
struct BufferToImageCopy {
    VkDeviceSize src_buffer_offset{};

    VkExtent3D dst_image_offset_override{};
    VkExtent3D dst_image_extent_override{};

    u32 mipmap_level_override{};
    u32 base_array_layer_override{};
    u32 array_layer_count_override{};
};

class RenderAPI {
public:
    RenderAPI(const Window &window, const SwapchainConfig &config);

    /// Managers
    Unique<Instance> instance;
    Unique<Swapchain> swapchain;
    Unique<ResourceManager> rm;

    /// Rendering functions
    Handle<Image> get_swapchain_image_handle(u32 image_index) const;
    VkFormat get_swapchain_format() const;
    VkResult get_next_swapchain_index(Handle<Semaphore> signal_semaphore, u32 *swapchain_index) const;
    VkResult present_swapchain(Handle<Semaphore> wait_semaphore, u32 image_index) const;
    u32 get_swapchain_image_count() const;

    void recreate_swapchain(glm::uvec2 size, const SwapchainConfig &config);

    void wait_for_device_idle() const;
    void wait_for_fence(Handle<Fence> handle) const;
    void reset_fence(Handle<Fence> handle) const;

    void reset_commands(Handle<CommandList> handle) const;
    void begin_recording_commands(Handle<CommandList> handle, VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) const;
    void end_recording_commands(Handle<CommandList> handle) const;
    void submit_commands(Handle<CommandList> handle, const SubmitInfo &info) const;
    void submit_commands_once(Handle<CommandList> handle) const;
    void record_and_submit_once(std::function<void(Handle<CommandList>)> &&lambda) const;

    void begin_query(Handle<CommandList> command_list, Handle<Query> query_handle);
    void end_query(Handle<CommandList> command_list, Handle<Query> query_handle);
    void reset_queries_immediate(const std::vector<Handle<Query>> &query_handles);
    void reset_queries_cmd(Handle<CommandList> command_list, const std::vector<Handle<Query>> &query_handles);
    void write_timestamp(Handle<CommandList> command_list, Handle<Query> query_handle, VkPipelineStageFlagBits pipeline_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    void read_queries(const std::vector<Handle<Query>> &query_handles, std::unordered_map<Handle<Query>, u64> *scalar_results, std::unordered_map<Handle<Query>, QueryPipelineStatisticsResults> *statistics_results, bool wait_for_results = false);

    void image_barrier(Handle<CommandList> command_list, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, const std::vector<ImageBarrier> &barriers) const;
    void buffer_barrier(Handle<CommandList> command_list, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, const std::vector<BufferBarrier> &barriers) const;

    void blit_image(Handle<CommandList> command_list, Handle<Image> src_image_handle, VkImageLayout src_image_layout, Handle<Image> dst_image_handle, VkImageLayout dst_image_layout, VkFilter filter, const std::vector<ImageBlit> &blits) const;
    void gen_mipmaps(Handle<CommandList> command_list, Handle<Image> target_image, VkFilter filter, VkImageLayout src_layout, VkPipelineStageFlags src_stage, VkAccessFlags src_access, VkImageLayout dst_layout, VkPipelineStageFlags dst_stage, VkAccessFlags dst_access) const;

    void copy_buffer_to_buffer(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Buffer> dst, const std::vector<VkBufferCopy> &regions) const;
    void copy_buffer_to_image(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Image> dst, const std::vector<BufferToImageCopy> &regions) const;

    void fill_buffer(Handle<CommandList> command_list, Handle<Buffer> handle, u32 data, VkDeviceSize size, VkDeviceSize offset = 0) const;

    void begin_graphics_pipeline(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<RenderTarget> render_target, const std::vector<RenderTargetClear> &color_clears, const RenderTargetClear &depth_clear) const;
    void end_graphics_pipeline(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline) const;

    void begin_compute_pipeline(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline) const;
    void dispatch_compute_pipeline(Handle<CommandList> command_list, u32 x_groups = 1u, u32 y_groups = 1u, u32 z_groups = 1u) const;
    void dispatch_indirect_compute_pipeline(Handle<CommandList> command_list, Handle<Buffer> buffer, VkDeviceSize offset = 0) const;

    void push_constants(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, const void *data, u8 size_override = 0U, u8 offset_override = 0U) const;
    void push_constants(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline, const void *data, u8 size_override = 0U, u8 offset_override = 0U) const;

    void bind_descriptor(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<Descriptor> descriptor, u32 dst_index) const;
    void bind_descriptor(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline, Handle<Descriptor> descriptor, u32 dst_index) const;

    void bind_vertex_buffer(Handle<CommandList> command_list, Handle<Buffer> buffer, u32 index = 0U, VkDeviceSize offset = 0) const;
    void bind_index_buffer(Handle<CommandList> command_list, Handle<Buffer> buffer, VkDeviceSize offset = 0) const;

    void clear_color_attachments(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<RenderTarget> rt, const std::vector<RenderTargetClear> &clear_colors) const;
    void clear_depth_attachment(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<RenderTarget> rt, const RenderTargetClear &clear) const;
    void clear_color_image(Handle<CommandList> command_list, Handle<Image> target, VkImageLayout layout, const RenderTargetClear &clear) const;
    void clear_depth_image(Handle<CommandList> command_list, Handle<Image> target, VkImageLayout layout, const RenderTargetClear &clear) const;

    void draw_count(Handle<CommandList> command_list, u32 vertex_count, u32 first_vertex = 0U, u32 instance_count = 1U) const;
    void draw_indexed(Handle<CommandList> command_list, u32 index_count, u32 first_index = 0U, i32 vertex_offset = 0U, u32 instance_count = 1U) const;
    void draw_indexed_indirect(Handle<CommandList> command_list, Handle<Buffer> indirect_buffer, u32 draw_count, u32 stride) const;
    void draw_indexed_indirect_count(Handle<CommandList> command_list, Handle<Buffer> indirect_buffer, Handle<Buffer> count_buffer, u32 max_draws, u32 stride) const;

    const SwapchainConfig &get_swapchain_config() const { return m_swapchain_config; }

private:
    SwapchainConfig m_swapchain_config{};

    std::vector<Handle<Image>> m_borrowed_swapchain_images{};
};


#endif