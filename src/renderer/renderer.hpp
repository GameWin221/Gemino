#ifndef GEMINO_RENDERER_HPP
#define GEMINO_RENDERER_HPP

#include <window/window.hpp>

#include <renderer/managers/image_manager.hpp>
#include <renderer/managers/buffer_manager.hpp>
#include <renderer/managers/pipeline_manager.hpp>
#include <renderer/managers/command_manager.hpp>
#include <renderer/instance.hpp>
#include <renderer/swapchain.hpp>

union ClearColor {
    f32 rgba_f32[4];
    u32 rgba_u32[4];
    i32 rgba_i32[4];
};

struct RenderTargetClear {
    ClearColor color{};
    f32 depth = 1.0f;
};

struct SubmitInfo {
    Handle<Fence> fence{};

    std::vector<Handle<Semaphore>> wait_semaphores{};
    std::vector<Handle<Semaphore>> signal_semaphores{};
    std::vector<VkPipelineStageFlags> signal_stages{};
};

struct RendererConfig {
    VSyncMode v_sync = VSyncMode::Enabled;
    u64 frame_timeout = 1000000000; // 1 second
};

enum GraphicsStage {
    Vertex = 0U,
    Fragment = 1U
};

class Renderer {
public:
    Renderer(const Window& window, const RendererConfig& config);

    /// Managers
    Unique<Instance> instance;
    Unique<Swapchain> swapchain;

    Unique<ImageManager> image_manager;
    Unique<BufferManager> buffer_manager;
    Unique<PipelineManager> pipeline_manager;
    Unique<CommandManager> command_manager;

    /// Rendering functions
    u32 get_next_swapchain_index(Handle<Semaphore> wait_semaphore) const;
    void present_swapchain(Handle<Semaphore> wait_semaphore, u32 image_index) const;

    void wait_for_device_idle() const;
    void wait_for_fence(Handle<Fence> handle) const;
    void reset_fence(Handle<Fence> handle) const;

    void reset_commands(Handle<CommandList> handle) const;
    void begin_recording_commands(Handle<CommandList> handle, VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) const;
    void end_recording_commands(Handle<CommandList> handle) const;
    void submit_commands(Handle<CommandList> handle, const SubmitInfo& info) const;

    void copy_buffer_to_buffer(Handle<CommandList> command_list, Handle<Buffer> src, Handle<Buffer> dst, const std::vector<VkBufferCopy>& regions) const;

    void begin_graphics_pipeline(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, Handle<RenderTarget> render_target, const RenderTargetClear& clear) const;
    void end_graphics_pipeline(Handle<CommandList> command_list) const;

    void begin_compute_pipeline(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline);
    void dispatch_compute_pipeline(Handle<CommandList> command_list, glm::uvec3 groups) const;

    void push_graphics_constants(Handle<CommandList> command_list, Handle<GraphicsPipeline> pipeline, GraphicsStage stage, const void* data) const;
    void push_compute_constants(Handle<CommandList> command_list, Handle<ComputePipeline> pipeline, const void* data) const;

    void draw_count(Handle<CommandList> command_list, u32 vertex_count, u32 first_vertex = 0U, u32 instance_count = 1U) const;

private:
    RendererConfig renderer_config{};
};


#endif