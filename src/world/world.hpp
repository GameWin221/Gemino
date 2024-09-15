#ifndef GEMINO_WORLD_HPP
#define GEMINO_WORLD_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <array>
#include <common/handle_allocator.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct alignas(16) Camera {
    alignas(16) glm::mat4 view = glm::mat4(1.0f);
    alignas(16) glm::mat4 proj = glm::mat4(1.0f);
    alignas(16) glm::mat4 view_proj = glm::mat4(1.0f);

    alignas(16) glm::vec3 position{};
    alignas(4) float fov{};

    alignas(4) float pitch{};
    alignas(4) float yaw{};
    alignas(4) float near = 0.02f;
    alignas(4) float far = 2000.0f;

    alignas(8) glm::vec2 viewport_size = glm::vec2(1.0f);

    alignas(16) glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f);
    alignas(16) glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    alignas(16) glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    alignas(16) glm::vec3 left_plane{};
    alignas(16) glm::vec3 right_plane{};
    alignas(16) glm::vec3 top_plane{};
    alignas(16) glm::vec3 bottom_plane{};
    alignas(16) glm::vec4 _pad0{};
    alignas(16) glm::vec4 _pad1{};
};

struct MeshLOD {
    u32 index_count{};
    u32 first_index{};
    i32 vertex_offset{};
};
// std430
struct alignas(16) Mesh {
    glm::vec3 center_offset{};
    f32 radius{};

    f32 cull_distance = 1000.0f;
    f32 lod_bias{};

    u32 lod_count{};
    std::array<MeshLOD, 8> lods{};
};
struct alignas(16) Texture {
    alignas(4) u32 width{};
    alignas(4) u32 height{};
    alignas(4) u32 bytes_per_pixel{};
    alignas(4) u32 mip_level_count{};
    alignas(4) u32 is_srgb{};
    alignas(4) u32 use_linear_filter{};
    alignas(4) Handle<u32> image{};
    alignas(4) Handle<u32> sampler{};
};
// std140
struct alignas(16) Material {
    alignas(4) Handle<Texture> albedo_texture = INVALID_HANDLE;
    alignas(4) Handle<Texture> roughness_texture = INVALID_HANDLE;
    alignas(4) Handle<Texture> metalness_texture = INVALID_HANDLE;
    alignas(4) Handle<Texture> normal_texture = INVALID_HANDLE;
    alignas(16) glm::vec3 color = glm::vec3(1.0f);
};
// std140
struct alignas(16) Transform {
    alignas(16) glm::vec3 position{};
    alignas(16) glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    alignas(16) glm::vec3 scale = glm::vec3(1.0f);
    alignas(4) f32 max_scale = 1.0f;
};
// std140
struct alignas(16) Object {
    // Global transform and local transform are both allocated at the same handle as object
    alignas(4) Handle<Mesh> mesh = INVALID_HANDLE;
    alignas(4) Handle<Material> material = INVALID_HANDLE;
    alignas(4) Handle<Object> parent = INVALID_HANDLE;
    alignas(4) u32 visible = 1U;
};

struct ObjectCreateInfo{
    Handle<Mesh> mesh = INVALID_HANDLE;
    Handle<Material> material = INVALID_HANDLE;

    Handle<Object> parent = INVALID_HANDLE;

    bool visible = true;

    glm::vec3 local_position{};
    glm::quat local_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 local_scale = glm::vec3(1.0f);
};
struct CameraCreateInfo{
    glm::vec2 viewport_size{};
    glm::vec3 position{};

    float pitch = 0.0f;
    float yaw = 0.0f;
    float fov = 60.0f;

    float near_plane = 0.02f;
    float far_plane = 2000.0f;
};
struct SceneCreateInfo {
    std::vector<Handle<Mesh>> meshes{};
    std::vector<Handle<Material>> materials{};
    std::vector<Handle<Texture>> textures{};

    std::vector<u32> meshes_default_materials{};
    std::vector<std::vector<u32>> meshes_primitives{};

    std::vector<ObjectCreateInfo> objects{};
    std::vector<std::vector<u32>> children{};

    std::vector<u32> root_objects{};

    glm::vec3 position{};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

class World {
public:
    Handle<Object> create_object(const ObjectCreateInfo &create_info);
    Handle<Camera> create_camera(const CameraCreateInfo &create_info);
    std::vector<Handle<Object>> instantiate_scene(const SceneCreateInfo &create_info);
    std::vector<Handle<Object>> instantiate_scene_object(const SceneCreateInfo &create_info, u32 object_id);

    void set_position(Handle<Object> object, glm::vec3 position);
    void set_rotation(Handle<Object> object, glm::quat rotation);
    void set_scale(Handle<Object> object, glm::vec3 scale);

    void set_mesh(Handle<Object> object, Handle<Mesh> mesh);
    void set_material(Handle<Object> object, Handle<Material> material);
    void set_visibility(Handle<Object> object, bool visible);
    void set_parent(Handle<Object> object, Handle<Object> new_parent);

    const std::vector<Handle<Object>> &get_children(Handle<Object> object) const { return m_children.get_element(object); }
    const Transform &get_local_transform(Handle<Object> object) const { return m_local_transforms.get_element(object); }
    const Transform &get_global_transform(Handle<Object> object) const { return m_global_transforms.get_element(object); }
    const Object &get_object(Handle<Object> object) const { return m_objects.get_element(object); }
    const Camera &get_camera(Handle<Camera> camera) const { return m_cameras.get_element(camera); }
    bool get_visibility(Handle<Object> object) const { return static_cast<bool>(m_objects.get_element(object).visible); }

    void set_camera_position(Handle<Camera> camera, glm::vec3 position);
    void set_camera_rotation(Handle<Camera> camera, float pitch, float yaw);
    void set_camera_fov(Handle<Camera> camera, float fov);
    void set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size);

    const std::unordered_set<Handle<Object>> &get_valid_object_handles() const { return m_objects.get_valid_handles(); }
    const std::unordered_set<Handle<Camera>> &get_valid_camera_handles() const { return m_cameras.get_valid_handles(); }

    const std::vector<Object> &get_objects() const { return m_objects.get_all_elements(); };
    const std::vector<Camera> &get_cameras() const { return m_cameras.get_all_elements(); };

    const std::unordered_set<Handle<Object>> &get_changed_object_handles() const { return m_changed_object_handles; }

    const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);

    void _clear_updates();

private:
    void instantiate_scene_recursive(const SceneCreateInfo &create_info, std::vector<Handle<Object>> &instantiated, u32 object_id, Handle<Object> parent_handle);

    glm::mat4 calculate_view_matrix(const Camera &camera) const;
    glm::mat4 calculate_proj_matrix(const Camera &camera) const;
    void update_vectors(Camera &camera);
    void update_frustum(Camera &camera);

    HandleAllocator<Transform> m_local_transforms{};
    HandleAllocator<Transform> m_global_transforms{};
    HandleAllocator<Object> m_objects{};
    HandleAllocator<Camera> m_cameras{};
    HandleAllocator<std::vector<u32>> m_children{};

    std::unordered_set<Handle<Object>> m_changed_object_handles{};
};

#endif