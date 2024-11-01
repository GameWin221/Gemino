#ifndef GEMINO_RENDERER_HPP
#define GEMINO_RENDERER_HPP

#include <render_api/render_api.hpp>
#include <world/world.hpp>
#include <window/window.hpp>
#include <common/utils.hpp>

#include "passes/draw_call_gen_pass.hpp"
#include "passes/geometry_pass.hpp"
#include "passes/offscreen_to_swapchain_pass.hpp"
#include "passes/ui_pass.hpp"

struct SceneLoadInfo {
    std::string path{};
    bool import_textures = true;
    bool import_materials = true;
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
};
struct MeshInstanceCreateInfo {
    Handle<Mesh> mesh = INVALID_HANDLE;
    std::vector<Handle<Material>> materials{};
};

struct DrawCommand {
    VkDrawIndexedIndirectCommand vk_cmd{};
    u32 object_id{};
    u32 primitive_id{};
};

class Renderer {
public:
    Renderer(Window &window, VSyncMode v_sync);
    ~Renderer();

    u32 get_frames_since_init() const { return m_frames_since_init; }

    void resize(Window &window);
    void render(Window &window, World &world, Handle<Camera> camera);
    void reload_pipelines();

    SceneCreateInfo load_gltf_scene(const SceneLoadInfo &load_info);

    Handle<Mesh> create_mesh(const MeshCreateInfo &create_info);
    void destroy_mesh(Handle<Mesh> mesh_handle);

    Handle<MeshInstance> create_mesh_instance(const MeshInstanceCreateInfo &create_info);
    void destroy_mesh_instance(Handle<MeshInstance> mesh_instance_handle);

    Handle<Texture> load_u8_texture(const TextureLoadInfo &load_info);
    Handle<Texture> create_u8_texture(const TextureCreateInfo &create_info);
    void destroy_texture(Handle<Texture> texture_handle);

    Handle<Material> create_material(const MaterialCreateInfo &create_info);
    void destroy_material(Handle<Material> material_handle);

    std::function<void()> m_ui_pass_draw_fn{};

    void set_config_global_lod_bias(float value);
    float get_config_global_lod_bias() const { return m_config_global_lod_bias; }

    void set_config_global_cull_dist_multiplier(float value);
    float get_config_global_culldist_multiplier() const { return m_config_global_cull_dist_multiplier; }

    void set_config_enable_dynamic_lod(bool enable);
    bool get_config_enable_dynamic_lod() const { return m_config_enable_dynamic_lod; }

    void set_config_enable_frustum_cull(bool enable);
    bool get_config_enable_frustum_cull() const { return m_config_enable_frustum_cull; }

    const HandleAllocator<Mesh> &get_mesh_allocator() const { return m_mesh_allocator; }
    const RangeAllocator<Primitive, RangeAllocatorType::InPlace> &get_primitive_allocator() const { return m_primitive_allocator; }
    const HandleAllocator<MeshInstance> &get_mesh_instance_allocator() const { return m_mesh_instance_allocator; }
    const HandleAllocator<Texture> &get_texture_allocator() const { return m_texture_allocator; }
    const HandleAllocator<Material> &get_material_allocator() const { return m_material_allocator; }

    Handle<Material> get_default_material() const { return m_default_material  ; }
    Handle<Texture> get_default_white_srgb_texture() const { return m_default_white_srgb_texture; }
    Handle<Texture> get_default_grey_unorm_texture() const { return m_default_grey_unorm_texture; }

    const u32 FRAMES_IN_FLIGHT = 2U;

    const VkDeviceSize MAX_SCENE_TEXTURES = 2048ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MATERIALS = 65535ull; // (device memory)
    const VkDeviceSize MAX_SCENE_VERTICES = (128ull * 1024ull * 1024ull) / sizeof(Vertex); // (device memory)
    const VkDeviceSize MAX_SCENE_INDICES = (64ull * 1024ull * 1024ull) / sizeof(u32); // (device memory)
    const VkDeviceSize MAX_SCENE_MESHES = 2048ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MESH_INSTANCES = MAX_SCENE_MESHES * 2ull; // (device memory)
    const VkDeviceSize MAX_SCENE_MESH_INSTANCE_MATERIALS = MAX_SCENE_MESH_INSTANCES * 4u; // (device memory)
    const VkDeviceSize MAX_SCENE_PRIMITIVES = MAX_SCENE_MESHES * 4ull; // (device memory)
    const VkDeviceSize MAX_SCENE_OBJECTS = 1ull * 1024ull * 1024ull; // (device memory)
    const VkDeviceSize MAX_SCENE_DRAWS = MAX_SCENE_OBJECTS * 2ull; // (device memory)

    const VkDeviceSize PER_FRAME_UPLOAD_BUFFER_SIZE = 16ull * 1024ull * 1024ull; // (host memory)

private:
    void begin_recording_frame();
    void update_world(World &world, Handle<Camera> camera);
    void render_world(const World &world, Handle<Camera> camera);
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

    RenderAPI m_api;
    UIPass m_ui_pass{};
    OffscreenToSwapchainPass m_offscreen_to_swapchain_pass{};
    GeometryPass m_geometry_pass{};
    DrawCallGenPass m_draw_call_gen_pass{};

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

    Handle<Image> m_offscreen_image{};
    Handle<Sampler> m_offscreen_sampler{};
    Handle<Image> m_depth_image{};

    Handle<Material> m_default_material{};
    Handle<Texture> m_default_white_srgb_texture{};
    Handle<Texture> m_default_grey_unorm_texture{};

    Handle<Descriptor> m_scene_texture_descriptor{};
    Handle<Buffer> m_scene_vertex_buffer{};
    Handle<Buffer> m_scene_index_buffer{};
    Handle<Buffer> m_scene_primitive_buffer{};
    Handle<Buffer> m_scene_mesh_buffer{};
    Handle<Buffer> m_scene_mesh_instance_buffer{};
    Handle<Buffer> m_scene_mesh_instance_materials_buffer{};
    Handle<Buffer> m_scene_object_buffer{};
    Handle<Buffer> m_scene_global_transform_buffer{};
    Handle<Buffer> m_scene_material_buffer{};
    Handle<Buffer> m_scene_camera_buffer{};
    Handle<Buffer> m_scene_draw_buffer{};
    Handle<Buffer> m_scene_draw_count_buffer{};
};

#endif
