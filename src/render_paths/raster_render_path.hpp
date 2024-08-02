#ifndef GEMINO_RASTER_RENDER_PATH_HPP
#define GEMINO_RASTER_RENDER_PATH_HPP

#include <render_api/render_api.hpp>
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

    glm::vec3 center_offset{};
    f32 radius{};

    static MeshLODCreateInfo from_sub_mesh_data(const Utils::SubMeshImportData& data) {
        return MeshLODCreateInfo{
            .vertex_data = data.vertices.data(),
            .vertex_count = static_cast<u32>(data.vertices.size()),
            .index_data = data.indices.data(),
            .index_count = static_cast<u32>(data.indices.size()),
            .center_offset = data.center_offset,
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

struct DrawCallGenPC {
    u32 draw_count_pre_cull{};
    f32 global_lod_bias{};
    f32 global_cull_dist_multiplier{};
};

class RasterRenderPath {
public:
    RasterRenderPath(Window &window, VSyncMode v_sync);
    ~RasterRenderPath();

    u32 get_frames_since_init() const { return m_frames_since_init; }

    void resize(const Window &window);
    void render(World &world, Handle<Camera> camera);
    void reload_pipelines();

    Handle<Mesh> create_mesh(const MeshCreateInfo &create_info);
    void destroy_mesh(Handle<Mesh> mesh_handle);

    Handle<Texture> create_u8_texture(const TextureCreateInfo &create_info);
    void destroy_texture(Handle<Texture> texture_handle);

    Handle<Material> create_material(const MaterialCreateInfo &create_info);
    void destroy_material(Handle<Material> material_handle);

    void set_config_global_lod_bias(float value);
    float get_config_global_lod_bias() const { return m_config_global_lod_bias; }

    void set_config_global_cull_dist_multiplier(float value);
    float get_config_global_culldist_multiplier() const { return m_config_global_cull_dist_multiplier; }

    void set_config_enable_dynamic_lod(bool enable);
    bool get_config_enable_dynamic_lod() const { return m_config_enable_dynamic_lod; }

    void set_config_enable_frustum_cull(bool enable);
    bool get_config_enable_frustum_cull() const { return m_config_enable_frustum_cull; }

    const u32 FRAMES_IN_FLIGHT = 2U;

    const VkDeviceSize MAX_SCENE_TEXTURES = 2048ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MATERIALS = 65535ull; // (device memory)
    const VkDeviceSize MAX_SCENE_VERTICES = (16ull * 1024ull * 1024ull) / sizeof(Vertex); // (device memory)
    const VkDeviceSize MAX_SCENE_INDICES = (64ull * 1024ull * 1024ull) / sizeof(u32); // (device memory)
    const VkDeviceSize MAX_SCENE_MESHES = 2048ull; // (device memory)
    const VkDeviceSize MAX_SCENE_DRAWS = 1ull * 1024ull * 1024ull; // (device memory)
    const VkDeviceSize MAX_SCENE_OBJECTS = MAX_SCENE_DRAWS; // (device memory)

    // I know it's huge for a per-frame buffer but until I don't implement batch uploading its ok I guess
    const VkDeviceSize PER_FRAME_UPLOAD_BUFFER_SIZE = 16ull * 1024ull * 1024ull; // (host memory)

    const VkDeviceSize OVERALL_DEVICE_MEMORY_USAGE =
        (MAX_SCENE_VERTICES * sizeof(Vertex)) +
        (MAX_SCENE_INDICES * sizeof(u32)) +
        (MAX_SCENE_MESHES * sizeof(Mesh)) +
        (MAX_SCENE_DRAWS * sizeof(VkDrawIndexedIndirectCommand)) +
        (MAX_SCENE_DRAWS * sizeof(u32)) +
        (MAX_SCENE_OBJECTS * sizeof(Object)) +
        (MAX_SCENE_MATERIALS * sizeof(Material)) +
        (MAX_SCENE_TEXTURES * sizeof(VkDeviceSize) * 2ull);

    const VkDeviceSize OVERALL_HOST_MEMORY_USAGE =
        (PER_FRAME_UPLOAD_BUFFER_SIZE * FRAMES_IN_FLIGHT);

private:
    void begin_recording_frame();
    void update_world(World &world, Handle<Camera> camera);
    void render_world(const World &world, Handle<Camera> camera);
    void end_recording_frame();

    void render_pass_draw_call_gen(u32 scene_objects_count, const DrawCallGenPC &draw_call_gen_pc);
    void render_pass_geometry(u32 scene_objects_count);
    void render_pass_offscreen_rt_to_swapchain();

    void init_scene_buffers();
    void init_screen_images(const Window& window);
    void init_debug_info();
    void init_descriptors(bool create_new);
    void init_pipelines();
    void init_render_targets();
    void init_frames();
    void init_defaults();

    void destroy_defaults();
    void destroy_frames();
    void destroy_render_targets();
    void destroy_pipelines();
    void destroy_descriptors(bool create_new);
    void destroy_screen_images();
    void destroy_scene_buffers();

    RenderAPI m_api;

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

    // Config start
    float m_config_global_lod_bias{};
    float m_config_global_cull_dist_multiplier = 1.0f;
    bool m_config_enable_dynamic_lod = true;
    bool m_config_enable_frustum_cull = true;

    u32 m_config_texture_anisotropy = 8U;
    float m_config_texture_mip_bias = 0.0f;
    // Config end

    u32 m_frame_in_flight_index{};
    u32 m_swapchain_target_index{};
    u32 m_frames_since_init{};

    bool m_reload_pipelines_queued{};

    std::vector<Frame> m_frames{};

    HandleAllocator<Mesh> m_mesh_allocator{};
    HandleAllocator<Texture> m_texture_allocator{};
    HandleAllocator<Material> m_material_allocator{};

    Handle<Descriptor> m_draw_call_gen_descriptor{};
    Handle<ComputePipeline> m_draw_call_gen_pipeline{};

    Handle<RenderTarget> m_offscreen_rt{};
    Handle<Image> m_offscreen_rt_image{};
    Handle<Sampler> m_offscreen_rt_sampler{};

    Handle<Descriptor> m_forward_descriptor{};
    Handle<GraphicsPipeline> m_forward_pipeline{};

    std::vector<Handle<RenderTarget>> m_offscreen_rt_to_swapchain_targets{};
    Handle<Descriptor> m_offscreen_rt_to_swapchain_descriptor{};
    Handle<GraphicsPipeline> m_offscreen_rt_to_swapchain_pipeline{};

    Handle<Image> m_depth_image{};

    Handle<Texture> m_default_white_srgb_texture{};
    Handle<Texture> m_default_grey_unorm_texture{};

    u32 m_allocated_vertices{};
    u32 m_allocated_indices{};

    Handle<Buffer> m_scene_vertex_buffer{};
    Handle<Buffer> m_scene_index_buffer{};
    Handle<Buffer> m_scene_mesh_buffer{};
    Handle<Buffer> m_scene_object_buffer{};
    Handle<Buffer> m_scene_material_buffer{};
    Handle<Buffer> m_scene_camera_buffer{};
    Handle<Buffer> m_scene_draw_buffer{};
    Handle<Buffer> m_scene_draw_index_buffer{};
    Handle<Buffer> m_scene_draw_count_buffer{};
};

#endif
