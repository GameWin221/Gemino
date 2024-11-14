#ifndef GEMINO_WORLD_HPP
#define GEMINO_WORLD_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <common/handle_allocator.hpp>
#include <common/range_allocator.hpp>
#include <renderer/gpu_types.inl>

struct ObjectCreateInfo{
    std::string name{};
    Handle<MeshInstance> mesh_instance = INVALID_HANDLE;
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
    std::vector<Handle<MeshInstance>> mesh_instances{};
    std::vector<Handle<Material>> materials{};
    std::vector<Handle<Texture>> textures{};

    std::vector<ObjectCreateInfo> objects{};
    std::vector<std::vector<u32>> children{};

    std::vector<u32> root_objects{};

    glm::vec3 position{};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

class World {
    friend class Renderer;

public:
    Handle<Object> create_object(const ObjectCreateInfo &create_info);
    Handle<Camera> create_camera(const CameraCreateInfo &create_info);
    Handle<Object> instantiate_scene(const SceneCreateInfo &create_info);
    Handle<Object> instantiate_scene_object(const SceneCreateInfo &create_info, u32 object_id);

    void destroy_object(Handle<Object> object);
    void destroy_camera(Handle<Camera> camera);

    void set_position(Handle<Object> object, glm::vec3 position);
    void set_rotation(Handle<Object> object, glm::quat rotation);
    void set_scale(Handle<Object> object, glm::vec3 scale);

    void set_mesh_instance(Handle<Object> object, Handle<MeshInstance> mesh_instance);
    void set_visibility(Handle<Object> object, bool visible);
    void set_parent(Handle<Object> object, Handle<Object> new_parent);

    const std::vector<Handle<Object>> &get_children(Handle<Object> object) const { return m_children.get_element(object.into<std::vector<Handle<Object>>>()); }
    const Transform &get_local_transform(Handle<Object> object) const { return m_local_transforms.get_element(object.into<Transform>()); }
    const Transform &get_global_transform(Handle<Object> object) const { return m_global_transforms.get_element(object.into<Transform>()); }
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

    Transform calculate_child_transform(const Transform &local_transform, const Transform &parent_transform);

    void update_objects();

private:
    void _clear_updates(const std::vector<Handle<Object>> &handles_to_clear);

    Handle<Object> instantiate_scene_recursive(const SceneCreateInfo &create_info, u32 object_id, Handle<Object> parent_handle);
    void update_object_recursive(Handle<Object> object_handle);

    glm::mat4 calculate_view_matrix(const Camera &camera) const;
    glm::mat4 calculate_proj_matrix(const Camera &camera) const;
    void update_vectors(Camera &camera);
    void update_frustum(Camera &camera);

    HandleAllocator<Transform> m_local_transforms{};
    HandleAllocator<Transform> m_global_transforms{};
    HandleAllocator<Object> m_objects{};
    HandleAllocator<Camera> m_cameras{};
    HandleAllocator<std::vector<Handle<Object>>> m_children{};

    std::unordered_set<Handle<Object>> m_changed_object_handles{};
};

#endif