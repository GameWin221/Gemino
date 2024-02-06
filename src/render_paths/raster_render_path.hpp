#ifndef GEMINO_RASTER_RENDER_PATH_HPP
#define GEMINO_RASTER_RENDER_PATH_HPP

#include <renderer/renderer.hpp>
#include <world/world.hpp>
#include <window/window.hpp>
#include <common/utils.hpp>

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
struct MeshLODCreateInfo {
    const Vertex* vertex_data{};
    u32 vertex_count{};

    const u32* index_data{};
    u32 index_count{};

    glm::vec3 center{};
    f32 radius{};

    static MeshLODCreateInfo from_sub_mesh_data(const Utils::SubMeshImportData& data) {
        return MeshLODCreateInfo{
            .vertex_data = data.vertices.data(),
            .vertex_count = static_cast<u32>(data.vertices.size()),
            .index_data = data.indices.data(),
            .index_count = static_cast<u32>(data.indices.size()),
            .center = data.center,
            .radius = data.radius
        };
    }
};
struct MeshCreateInfo {
    std::vector<MeshLODCreateInfo> lods{};

    f32 lod_bias{};
    f32 cull_distance = 1000.0f;

    // By default, LODs are ordered in the same order as objects are listed in the imported .obj file
    static MeshCreateInfo from_mesh_data(const Utils::MeshImportData& data, f32 cull_distance = 1000.0f, f32 lod_bias = 0.0f, const std::vector<u32>& custom_lod_order = std::vector<u32>()) {
        MeshCreateInfo info{
            .lod_bias = lod_bias,
            .cull_distance = cull_distance
        };

        if(custom_lod_order.empty()) {
            for(const auto& sub_mesh : data.sub_meshes) {
                info.lods.push_back(MeshLODCreateInfo::from_sub_mesh_data(sub_mesh));
            }
        } else {
            if(custom_lod_order.size() != data.sub_meshes.size()) {
                DEBUG_PANIC("If you specify a custom LOD order, custom_lod_order.size() must match the count of mesh's sub mesh count, custom_lod_order.size() = " << custom_lod_order.size() << ", data.sub_meshes.size() = " << data.sub_meshes.size())
            }

            for(const auto& lod : custom_lod_order) {
                if(lod >= static_cast<u32>(data.sub_meshes.size())) {
                    DEBUG_PANIC("Custtom LOD index out of bounds! lod = " << lod << ", data.sub_meshes.size() = " << data.sub_meshes.size())
                }

                info.lods.push_back(MeshLODCreateInfo::from_sub_mesh_data(data.sub_meshes[lod]));
            }
        }

        return info;
    }
};

class RasterRenderPath {
public:
    RasterRenderPath(Window& window, VSyncMode v_sync);
    ~RasterRenderPath();

    u32 get_frames_since_init() const { return frames_since_init; }

    void resize(const Window& window);
    void render(World& world, Handle<Camera> camera);

    // LOD meshes should be as close to vec3(0.0, 0.0, 0.0) as possible in order to make LOD picking more effective
    Handle<Mesh> create_mesh(const MeshCreateInfo& create_info);
    void destroy_mesh(Handle<Mesh> mesh_handle);

    Handle<Texture> create_u8_texture(const TextureCreateInfo& create_info);
    void destroy_texture(Handle<Texture> texture_handle);

    Handle<Material> create_material(const MaterialCreateInfo& create_info);
    void destroy_material(Handle<Material> material_handle);

    float global_lod_bias{};

    const u32 FRAMES_IN_FLIGHT = 2U;

    const VkDeviceSize MAX_SCENE_TEXTURES = 2048; // (device memory)
    const VkDeviceSize MAX_SCENE_MATERIALS = 65535; // (device memory)
    const VkDeviceSize MAX_SCENE_VERTICES = (16 * 1024 * 1024) / sizeof(Vertex); // (device memory)
    const VkDeviceSize MAX_SCENE_INDICES = (64 * 1024 * 1024) / sizeof(u32); // (device memory)
    const VkDeviceSize MAX_SCENE_LODS = 8192; // (device memory)
    const VkDeviceSize MAX_SCENE_MESHES = 2048; // (device memory)
    const VkDeviceSize MAX_SCENE_DRAWS = 1 * 1024 * 1024; // (device memory)
    const VkDeviceSize MAX_SCENE_OBJECTS = MAX_SCENE_DRAWS; // (device memory)

    const VkDeviceSize PER_FRAME_UPLOAD_BUFFER_SIZE = 16 * 1024 * 1024; // (host memory)

    const VkDeviceSize OVERALL_DEVICE_MEMORY_USAGE =
        (MAX_SCENE_VERTICES * sizeof(Vertex)) +
        (MAX_SCENE_INDICES * sizeof(u32)) +
        (MAX_SCENE_LODS * sizeof(MeshLOD)) +
        (MAX_SCENE_MESHES * sizeof(Mesh)) +
        (MAX_SCENE_DRAWS * sizeof(VkDrawIndexedIndirectCommand)) +
        (MAX_SCENE_DRAWS * sizeof(u32)) +
        (MAX_SCENE_OBJECTS * sizeof(Object)) +
        (MAX_SCENE_MATERIALS * sizeof(Material)) +
        (MAX_SCENE_TEXTURES * sizeof(VkDeviceSize) * 2);

    const VkDeviceSize OVERALL_HOST_MEMORY_USAGE =
        (PER_FRAME_UPLOAD_BUFFER_SIZE * FRAMES_IN_FLIGHT);

private:
    void begin_recording_frame();
    void update_world(World& world, Handle<Camera> camera);
    void render_world(const World& world, Handle<Camera> camera);
    void end_recording_frame();

    void init_scene_buffers();
    void init_debug_info();
    void init_lod_assign_pipeline();
    void init_forward_pipeline(const Window& window);
    void init_frames();
    void init_defaults();

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

    u32 frame_in_flight_index{};
    u32 swapchain_target_index{};
    u32 frames_since_init{};

    std::vector<Frame> frames{};
    std::vector<Handle<RenderTarget>> swapchain_targets{};

    HandleAllocator<MeshLOD> lod_allocator{};
    HandleAllocator<Mesh> mesh_allocator{};
    HandleAllocator<Texture> texture_allocator{};
    HandleAllocator<Material> material_allocator{};

    Handle<Descriptor> lod_assign_descriptor{};
    Handle<GraphicsPipeline> lod_assign_pipeline{};

    Handle<Descriptor> forward_descriptor{};
    Handle<GraphicsPipeline> forward_pipeline{};

    Handle<Image> depth_image{};

    Handle<Texture> default_white_srgb_texture{};
    Handle<Texture> default_grey_unorm_texture{};

    u32 allocated_vertices{};
    u32 allocated_indices{};

    Handle<Buffer> scene_vertex_buffer{};
    Handle<Buffer> scene_index_buffer{};
    Handle<Buffer> scene_lod_buffer{};
    Handle<Buffer> scene_mesh_buffer{};
    Handle<Buffer> scene_object_buffer{};
    Handle<Buffer> scene_material_buffer{};
    Handle<Buffer> scene_camera_buffer{};
    Handle<Buffer> scene_draw_buffer{};
    Handle<Buffer> scene_draw_index_buffer{};
    Handle<Buffer> scene_draw_count_buffer{};
};

#endif
