#ifndef GEMINO_RASTER_RENDER_PATH_HPP
#define GEMINO_RASTER_RENDER_PATH_HPP

#include <renderer/renderer.hpp>
#include <world/world.hpp>
#include <window/window.hpp>

class RasterRenderPath {
public:
    RasterRenderPath(Window& window, VSyncMode v_sync);
    ~RasterRenderPath();

    void resize(const Window& window);
    void render(const World& world, Handle<Camera> camera);

    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    Handle<Mesh> create_mesh(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);
    void destroy_mesh(Handle<Mesh> mesh_handle);

    const VkDeviceSize MAX_SCENE_VERTICES = (16 * 1024 * 1024) / sizeof(Vertex); // 16mb of vertex data max
    const VkDeviceSize MAX_SCENE_INDICES = (64 * 1024 * 1024) / sizeof(u32); // 64mb of index data max

    const VkDeviceSize MAX_OBJECTS = 1024;
    const VkDeviceSize MAX_TRANSFORMS = 1024;
    const VkDeviceSize PER_FRAME_UPLOAD_BUFFER_SIZE = 1 * 1024 * 1024; // 1MB = 1024KB = 1024 * 1024B - Max data uploaded from cpu to gpu per frame

private:
    void begin_recording_frame();
    void update_world(const World& world, Handle<Camera> camera);
    void render_world(const World& world, Handle<Camera> camera);
    void end_recording_frame();

    Renderer renderer;

    struct Frame {
        Handle<CommandList> command_list{};

        Handle<Semaphore> present_semaphore{};
        Handle<Semaphore> render_semaphore{};
        Handle<Fence> fence{};

        Handle<Buffer> upload_buffer{};
        void* upload_ptr{};

        template<typename T>
        T* access_upload(usize offset) {
            return reinterpret_cast<T*>(reinterpret_cast<usize>(upload_ptr) + offset);
        }
    };

    u32 frames_in_flight = 2U;
    u32 frame_in_flight_index{};
    u32 swapchain_target_index{};
    u32 frames_since_init{};

    std::vector<Frame> frames{};
    std::vector<Handle<RenderTarget>> swapchain_targets{};

    HandleAllocator<Mesh> mesh_allocator{};

    Handle<Descriptor> forward_descriptor{};
    Handle<GraphicsPipeline> forward_pipeline{};

    Handle<Image> depth_image{};

    Handle<Buffer> scene_vertex_buffer{};
    Handle<Buffer> scene_index_buffer{};

    u32 allocated_vertices{};
    u32 allocated_indices{};

    Handle<Buffer> transform_buffer{};
    Handle<Buffer> object_buffer{};
    Handle<Buffer> camera_buffer{};
};

#endif
