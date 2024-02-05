#ifndef GEMINO_RASTER_RENDER_PATH_HPP
#define GEMINO_RASTER_RENDER_PATH_HPP

#include <renderer/renderer.hpp>
#include <world/world.hpp>
#include <window/window.hpp>

struct TextureCreateInfo {
    const void* pixel_data{};
    u32 width{};
    u32 height{};
    u32 bytes_per_pixel{};
    bool is_srgb = false;
    bool gen_mip_maps = false;
    bool linear_filter = true;
};
struct MaterialCreateInfo {
    Handle<Texture> albedo_texture = INVALID_HANDLE;
    Handle<Texture> roughness_texture = INVALID_HANDLE;
    Handle<Texture> metalness_texture = INVALID_HANDLE;
    Handle<Texture> normal_texture = INVALID_HANDLE;

    glm::vec3 color = glm::vec3(1.0f);
};
struct MeshCreateInfo {
    const Vertex* vertex_data{};
    u32 vertex_count{};

    const u32* index_data{};
    u32 index_count{};
};

class RasterRenderPath {
public:
    RasterRenderPath(Window& window, VSyncMode v_sync);
    ~RasterRenderPath();

    u32 get_frames_since_init() const { return frames_since_init; }

    void resize(const Window& window);
    void render(World& world, Handle<Camera> camera);
    void preload_world(World& world);

    Handle<Mesh> create_mesh(const MeshCreateInfo& create_info);
    void destroy_mesh(Handle<Mesh> mesh_handle);

    Handle<Texture> create_u8_texture(const TextureCreateInfo& create_info);
    void destroy_texture(Handle<Texture> texture_handle);

    Handle<Material> create_material(const MaterialCreateInfo& create_info);
    void destroy_material(Handle<Material> material_handle);

    const VkDeviceSize MAX_SCENE_TEXTURES = 16384;
    const VkDeviceSize MAX_SCENE_MATERIALS = (2 * 1024 * 1024) / sizeof(Material); // (device memory) of material data max
    const VkDeviceSize MAX_SCENE_VERTICES = (16 * 1024 * 1024) / sizeof(Vertex); // (device memory) of vertex data max
    const VkDeviceSize MAX_SCENE_INDICES = (64 * 1024 * 1024) / sizeof(u32); // (device memory) of index data max
    const VkDeviceSize MAX_SCENE_DRAWS = (4 * 1024 * 1024) / sizeof(VkDrawIndexedIndirectCommand); // (device memory) of draw data max
    const VkDeviceSize MAX_SCENE_OBJECTS = MAX_SCENE_DRAWS; // (device memory) of object data max

    const VkDeviceSize PER_FRAME_UPLOAD_BUFFER_SIZE = 16 * 1024 * 1024; // (host memory) of max data uploaded from cpu to gpu per frame

    const VkDeviceSize OVERALL_DEVICE_MEMORY_USAGE =
        (MAX_SCENE_VERTICES * sizeof(Vertex)) +
        (MAX_SCENE_INDICES * sizeof(u32)) +
        (MAX_SCENE_DRAWS * sizeof(VkDrawIndexedIndirectCommand)) +
        (MAX_SCENE_OBJECTS * sizeof(Object)) +
        (MAX_SCENE_MATERIALS * sizeof(Material)) +
        (MAX_SCENE_TEXTURES * sizeof(u64) * 2);

private:
    void begin_recording_frame();
    void update_world(World& world, Handle<Camera> camera);
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

    u32 texture_anisotropy = 8U;
    float texture_mip_bias = 0.0f;

    u32 frames_in_flight = 2U;
    u32 frame_in_flight_index{};
    u32 swapchain_target_index{};
    u32 frames_since_init{};

    std::vector<Frame> frames{};
    std::vector<Handle<RenderTarget>> swapchain_targets{};

    HandleAllocator<Mesh> mesh_allocator{};
    HandleAllocator<Texture> texture_allocator{};
    HandleAllocator<Material> material_allocator{};

    Handle<Descriptor> forward_descriptor{};
    Handle<GraphicsPipeline> forward_pipeline{};

    Handle<Image> depth_image{};

    Handle<Texture> default_white_srgb_texture{};
    Handle<Texture> default_grey_unorm_texture{};

    u32 allocated_vertices{};
    u32 allocated_indices{};

    Handle<Buffer> scene_vertex_buffer{};
    Handle<Buffer> scene_index_buffer{};
    Handle<Buffer> scene_object_buffer{};
    Handle<Buffer> scene_material_buffer{};
    Handle<Buffer> scene_camera_buffer{};
    Handle<Buffer> scene_draw_buffer{};
};

#endif
