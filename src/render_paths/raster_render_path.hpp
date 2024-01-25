#ifndef GEMINO_RASTER_RENDER_PATH_HPP
#define GEMINO_RASTER_RENDER_PATH_HPP

#include <render_paths/base_render_path.hpp>

class RasterRenderPath : public BaseRenderPath {
public:
    RasterRenderPath(const Window &window, const RendererConfig& render_config);
    ~RasterRenderPath();

    void begin_recording_frame() override;
    void update_world(const World& world, Handle<Camera> camera);
    void render_world(const World& world, Handle<Camera> camera);
    void end_recording_frame() override;

    const usize MAX_OBJECTS = 1024;
    const usize MAX_TRANSFORMS = 1024;
    const usize PER_FRAME_UPLOAD_BUFFER_SIZE = 1024 * 1024; // 1MB = 1024KB = 1024 * 1024B - Max data uploaded from cpu to gpu per frame

private:
    struct Frame {
        Handle<CommandList> command_list{};

        Handle<Semaphore> present_semaphore{};
        Handle<Semaphore> render_semaphore{};
        Handle<Fence> fence{};

        Handle<RenderTarget> swapchain_target{};

        Handle<Buffer> upload_buffer{};
        void* upload_ptr{};

        template<typename T>
        T* access_upload(usize offset) {
            return reinterpret_cast<T*>(reinterpret_cast<usize>(upload_ptr) + offset);
        }
    };

    u32 frames_in_flight = 3U;
    u32 frame_in_flight_index{};
    u32 swapchain_target_index{};
    u32 frames_since_start{};

    std::vector<Frame> frames{};
    std::vector<SubmitInfo> submit_infos{};

    Handle<Descriptor> forward_descriptor{};
    Handle<GraphicsPipeline> forward_pipeline{};

    Handle<Image> depth_image{};

    Handle<Buffer> transform_buffer{};
    Handle<Buffer> object_buffer{};
    Handle<Buffer> camera_buffer{};
};

#endif
