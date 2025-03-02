#ifndef GEMINO_RENDERER_HPP
#define GEMINO_RENDERER_HPP

#include <RHI/render_api.hpp>
#include <world/world.hpp>
#include <window/window.hpp>
#include <common/utils.hpp>
#include <renderer/gpu_types.inl>
#include <renderer/renderer_shared_objects.hpp>

#include "passes/draw_call_gen_pass.hpp"
#include "passes/geometry_pass.hpp"
#include "passes/offscreen_to_swapchain_pass.hpp"
#include "passes/ui_pass.hpp"
#include "passes/debug_pass.hpp"

struct SceneLoadInfo {
    std::string path{};
    bool import_textures = true;
    bool import_materials = true;
    u32 lod_bias_vert_threshold = UINT32_MAX;
    f32 lod_bias{}; // Applied if MeshInstance has more than 'lod_bias_threshold' vertices in total
    f32 simplify_target_error = -1.0f; // 0.0f -> 1.0f, less = better quality
    f32 cull_dist_multiplier = 1.0f;
};
struct TextureLoadInfo {
    std::string path{};
    bool is_srgb = false;
    bool gen_mip_maps = false;
    bool linear_filter = true;
    u32 desired_channels = 4u;
};

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

    glm::vec4 color = glm::vec4(1.0f);
};
struct PrimitiveCreateInfo {
    const Vertex* vertex_data{};
    u32 vertex_count{};

    const u32* index_data{};
    u32 index_count{};
};
struct MeshCreateInfo {
    std::vector<PrimitiveCreateInfo> primitives{};
    f32 simplify_target_error = 0.05f;
};
struct MeshInstanceCreateInfo {
    Handle<Mesh> mesh = INVALID_HANDLE;
    std::vector<Handle<Material>> materials{};
    f32 lod_bias{};
    f32 cull_dist_multiplier = 1.0f;
};

struct RegisteredPassRef {
    bool enabled{};
    bool query_statistics{};
    u32 order{};
    BasePass *pass_ptr{};
};
struct RegisteredPass {
    bool enabled = true;
    bool query_statistics = false;
    u32 order{};
    Unique<BasePass> pass_ptr{};

    RegisteredPassRef as_ref() const {
        return RegisteredPassRef {
            .enabled = enabled,
            .query_statistics = query_statistics,
            .order = order,
            .pass_ptr = pass_ptr.get()
        };
    }
};

class Renderer {
public:
    Renderer(Window &window, VSyncMode v_sync);
    ~Renderer();

    void resize(Window &window);
    void render(Window &window, World &world, Handle<Camera> camera);
    void reload_pipelines();

    SceneCreateInfo load_gltf_scene(const SceneLoadInfo &load_info);

    Handle<Mesh> create_mesh(const MeshCreateInfo &create_info);
    void destroy(Handle<Mesh> mesh_handle);

    Handle<MeshInstance> create_mesh_instance(const MeshInstanceCreateInfo &create_info);
    void destroy(Handle<MeshInstance> mesh_instance_handle);

    Handle<Texture> load_u8_texture(const TextureLoadInfo &load_info);
    Handle<Texture> create_u8_texture(const TextureCreateInfo &create_info);
    void destroy(Handle<Texture> texture_handle);

    Handle<Material> create_material(const MaterialCreateInfo &create_info);
    void destroy(Handle<Material> material_handle);

    void set_config_global_lod_bias(f32 value);
    void set_config_global_cull_dist_multiplier(f32 value);
    void set_config_enable_dynamic_lod(bool enable);
    void set_config_enable_frustum_cull(bool enable);
    void set_config_enable_debug_shape_view(bool enable);
    void set_config_debug_shape_opacity(f32 value);
    void set_config_lod_sphere_visible_angle(f32 value);

    void set_ui_draw_callback(UIPassDrawFn draw_callback);

    const HandleAllocator<Mesh> &get_mesh_allocator() const { return m_mesh_allocator; }
    const HandleAllocator<MeshInstance> &get_mesh_instance_allocator() const { return m_mesh_instance_allocator; }
    const HandleAllocator<Texture> &get_texture_allocator() const { return m_texture_allocator; }
    const HandleAllocator<Material> &get_material_allocator() const { return m_material_allocator; }

    const RangeAllocator<Handle<Material>, RangeAllocatorType::InPlace> &get_mesh_instance_materials_allocator() const { return m_mesh_instance_materials_allocator; }
    const RangeAllocator<Primitive, RangeAllocatorType::InPlace> &get_primitive_allocator() const { return m_primitive_allocator; }
    const RangeAllocator<Vertex, RangeAllocatorType::External> &get_vertex_allocator() const { return m_vertex_allocator; }
    const RangeAllocator<u32, RangeAllocatorType::External> &get_index_allocator() const { return m_index_allocator; }

    [[nodiscard]] const RendererSharedObjects &get_shared_objects() const { return m_shared; }

    const auto &get_cpu_timing() { return m_frames[m_frame_in_flight_index].cpu_timing; }
    const auto &get_gpu_timing() { return m_frames[m_frame_in_flight_index].gpu_timing; }
    const auto &get_gpu_statistics() { return m_frames[m_frame_in_flight_index].gpu_pipeline_statistics; }

    const u32 FRAMES_IN_FLIGHT = 2U;

    const VkDeviceSize MAX_SCENE_TEXTURES = 2048ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MATERIALS = 65535ull; // (device memory)
    const VkDeviceSize MAX_SCENE_VERTICES = (64ull * 1024ull * 1024ull) / sizeof(Vertex); // (device memory)
    const VkDeviceSize MAX_SCENE_INDICES = (256ull * 1024ull * 1024ull) / sizeof(u32); // (device memory)
    const VkDeviceSize MAX_SCENE_MESHES = 2048ull * 8ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MESH_INSTANCES = MAX_SCENE_MESHES * 2ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MESH_INSTANCE_MATERIALS = MAX_SCENE_MESH_INSTANCES * 4u; // (device memory)
    const VkDeviceSize MAX_SCENE_PRIMITIVES = MAX_SCENE_MESHES * 4ull; // (device memory)
    const VkDeviceSize MAX_SCENE_OBJECTS = 1ull * 1024ull * 1024ull; // (device memory)
    const VkDeviceSize MAX_SCENE_DRAWS = MAX_SCENE_OBJECTS * 2ull; // (device memory)

    const VkDeviceSize PER_FRAME_UPLOAD_BUFFER_SIZE = 16ull * 1024ull * 1024ull; // (host memory)

private:
    void begin_recording_frame();
    void update_world(World &world, Handle<Camera> camera);
    void render_world(World &world, Handle<Camera> camera);
    void end_recording_frame();

    void init_scene_buffers();
    void init_screen_images(glm::uvec2 size);
    void init_descriptors();
    void init_passes(const Window &window);
    void init_frames();
    void init_defaults();

    void resize_passes(const Window &window);

    void destroy_defaults();
    void destroy_frames();
    void destroy_passes();
    void destroy_descriptors();
    void destroy_screen_images();
    void destroy_scene_buffers();

    std::unordered_map<std::string, RegisteredPass> m_registered_passes{};

    RenderAPI m_api;
    UIPass m_ui_pass{};
    OffscreenToSwapchainPass m_offscreen_to_swapchain_pass{};
    GeometryPass m_geometry_pass{};
    DrawCallGenPass m_draw_call_gen_pass{};
    DebugPass m_debug_pass{};

    struct Frame {
        Handle<CommandList> command_list{};

        Handle<Semaphore> present_semaphore{};
        Handle<Semaphore> render_semaphore{};
        Handle<Fence> fence{};

        Handle<Buffer> upload_buffer{};

        void* upload_ptr{};

        template<typename T>
        T* access_upload(usize offset) {
#if DEBUG_MODE
            DEBUG_ASSERT((reinterpret_cast<usize>(upload_ptr) + offset) % alignof(T) == 0);
#endif

            return reinterpret_cast<T*>(reinterpret_cast<usize>(upload_ptr) + offset);
        }

        std::unordered_map<std::string, f64> cpu_timing{};
        std::unordered_map<std::string, std::pair<std::pair<Handle<Query>, Handle<Query>>, std::pair<f64, f64>>> gpu_timing{};
        std::unordered_map<std::string, std::pair<Handle<Query>, QueryPipelineStatisticsResults>> gpu_pipeline_statistics{};

    };

    u32 m_frame_in_flight_index{};
    f32 m_default_timestamp_period{};

    bool m_reload_pipelines_queued{};

    std::vector<Frame> m_frames{};

    HandleAllocator<Mesh> m_mesh_allocator{};
    HandleAllocator<MeshInstance> m_mesh_instance_allocator{};
    HandleAllocator<Texture> m_texture_allocator{};
    HandleAllocator<Material> m_material_allocator{};

    RangeAllocator<Handle<Material>, RangeAllocatorType::InPlace> m_mesh_instance_materials_allocator{};
    RangeAllocator<Primitive, RangeAllocatorType::InPlace> m_primitive_allocator{};
    RangeAllocator<Vertex, RangeAllocatorType::External> m_vertex_allocator{};
    RangeAllocator<u32, RangeAllocatorType::External> m_index_allocator{};

    RendererSharedObjects m_shared{};
};

#endif
