#include "world.hpp"

Handle<Object> World::create_object(const ObjectCreateInfo& create_info) {
    Object object{
        .mesh = create_info.mesh,
        .material = create_info.material,
        .parent = create_info.parent,
        .visible = static_cast<u32>(create_info.visible),
    };

    Transform local_transform{
        .position = create_info.local_position,
        .rotation = create_info.local_rotation,
        .scale = create_info.local_scale,
        .max_scale = glm::max(glm::max(create_info.local_scale.x, create_info.local_scale.y), create_info.local_scale.z),
    };

    Transform global_transform{};
    if (object.parent != INVALID_HANDLE) {
        const auto &parent_transform = m_global_transforms.get_element(object.parent);

        global_transform.position = parent_transform.rotation * local_transform.position * parent_transform.scale + parent_transform.position;
        global_transform.scale = local_transform.scale * parent_transform.scale;
        global_transform.rotation = parent_transform.rotation * local_transform.rotation;
        global_transform.max_scale = glm::max(glm::max(global_transform.scale.x, global_transform.scale.y), global_transform.scale.z);
    } else {
        global_transform = local_transform;
    }

    Handle<Object> object_handle = m_objects.alloc(object);
    Handle<Transform> local_t_handle = m_local_transforms.alloc(local_transform);
    Handle<Transform> global_t_handle = m_global_transforms.alloc(global_transform);
    Handle<std::vector<u32>> children_handle = m_children.alloc_in_place();

    DEBUG_ASSERT(object_handle == children_handle && object_handle == local_t_handle && local_t_handle == global_t_handle)

    if (object.parent != INVALID_HANDLE) {
        m_children.get_element_mutable(object.parent).push_back(object_handle);
    }

    m_changed_object_handles.insert(object_handle);

    return object_handle;
}
Handle<Camera> World::create_camera(const CameraCreateInfo& create_info) {
    Camera camera{
        .position = create_info.position,
        .fov = create_info.fov,
        .pitch = create_info.pitch,
        .yaw = create_info.yaw,
        .near = create_info.near_plane,
        .far = create_info.far_plane,
        .viewport_size = create_info.viewport_size
    };

    update_vectors(camera);
    camera.view = calculate_view_matrix(camera);
    camera.proj = calculate_proj_matrix(camera);
    camera.view_proj = camera.proj * camera.view;

    return m_cameras.alloc(camera);
}

void World::instantiate_scene_recursive(const SceneCreateInfo &create_info, std::vector<Handle<Object>> &instantiated, u32 object_id, Handle<Object> parent_handle) {
#if DEBUG_MODE
    if (create_info.objects.size() <= object_id) {
        DEBUG_PANIC("Scene object out of bounds! object_id = " << object_id)
    }
    if (create_info.objects.size() != create_info.children.size()) {
        DEBUG_PANIC("create_info.objects.size() != create_info.children.size()! create_info.objects.size() = " << create_info.objects.size() << ", create_info.children.size() = " << create_info.children.size())
    }
#endif

    ObjectCreateInfo obj = create_info.objects[object_id];
    obj.parent = parent_handle;

    if (parent_handle == INVALID_HANDLE) {
        obj.local_position = create_info.rotation * obj.local_position * create_info.scale + create_info.position;
        obj.local_scale *= create_info.scale;
        obj.local_rotation = create_info.rotation * obj.local_rotation;
    }

    Handle<Object> handle = create_object(obj);

    instantiated.push_back(handle);

    for (const auto &child_id : create_info.children[object_id]) {
        instantiate_scene_recursive(create_info, instantiated, child_id, handle);
    }
}
std::vector<Handle<Object>> World::instantiate_scene(const SceneCreateInfo &create_info) {
    std::vector<Handle<Object>> instantiated{};

    for (const auto &root_node_id : create_info.root_objects) {
        instantiate_scene_recursive(create_info, instantiated, root_node_id, INVALID_HANDLE);
    }

    return instantiated;
}
std::vector<Handle<Object>> World::instantiate_scene_object(const SceneCreateInfo &create_info, u32 object_id) {
    std::vector<Handle<Object>> instantiated{};

    instantiate_scene_recursive(create_info, instantiated, object_id, INVALID_HANDLE);

    return instantiated;
}

void World::set_position(Handle<Object> object, glm::vec3 position) {
    Transform &target = m_local_transforms.get_element_mutable(object);

    if(target.position == position) return;

    target.position = position;
    m_changed_object_handles.insert(object);
}
void World::set_rotation(Handle<Object> object, glm::quat rotation) {
    Transform &target = m_local_transforms.get_element_mutable(object);

    if(target.rotation == rotation) return;

    target.rotation = rotation;
    m_changed_object_handles.insert(object);
}
void World::set_scale(Handle<Object> object, glm::vec3 scale) {
    Transform &target = m_local_transforms.get_element_mutable(object);

    if(target.scale == scale) return;

    target.scale = scale;
    target.max_scale = glm::max(glm::max(scale.x, scale.y), scale.z);

    m_changed_object_handles.insert(object);
}

void World::set_mesh(Handle<Object> object, Handle<Mesh> mesh) {
    Object &target = m_objects.get_element_mutable(object);

    if(target.mesh == mesh) return;

    target.mesh = mesh;
    m_changed_object_handles.insert(object);
}
void World::set_material(Handle<Object> object, Handle<Material> material) {
    Object &target = m_objects.get_element_mutable(object);

    if(target.material == material) return;

    target.material = material;
    m_changed_object_handles.insert(object);
}
void World::set_visibility(Handle<Object> object, bool visible) {
    Object &target = m_objects.get_element_mutable(object);

    if(target.visible == static_cast<u32>(visible)) return;

    target.visible = visible;
    m_changed_object_handles.insert(object);
}
void World::set_parent(Handle<Object> object, Handle<Object> new_parent) {
    Object &target = m_objects.get_element_mutable(object);

    if(target.parent == new_parent) return;

    target.parent = new_parent;

    m_children.get_element_mutable(new_parent).push_back(object);

    m_changed_object_handles.insert(object);
}

void World::set_camera_position(Handle<Camera> camera, glm::vec3 position) {
    Camera &target = m_cameras.get_element_mutable(camera);

    if(target.position == position) return;

    target.position = position;

    update_vectors(target);
    update_frustum(target);
    target.view = calculate_view_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_rotation(Handle<Camera> camera, float pitch, float yaw) {
    Camera &target = m_cameras.get_element_mutable(camera);

    if(target.pitch == pitch && target.yaw == yaw) return;

    target.pitch = pitch;
    target.yaw = yaw;

    update_vectors(target);
    update_frustum(target);
    target.view = calculate_view_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_fov(Handle<Camera> camera, float fov) {
    Camera &target = m_cameras.get_element_mutable(camera);

    if(target.fov == fov) return;

    target.fov = fov;

    update_frustum(target);
    target.proj = calculate_proj_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size) {
    Camera &target = m_cameras.get_element_mutable(camera);

    if(target.viewport_size == viewport_size) return;

    target.viewport_size = viewport_size;

    update_frustum(target);
    target.proj = calculate_proj_matrix(target);
    target.view_proj = target.proj * target.view;
}

glm::mat4 World::calculate_view_matrix(const Camera &camera) const {
    return glm::lookAt(camera.position, camera.position + camera.forward, WORLD_UP);
}
glm::mat4 World::calculate_proj_matrix(const Camera &camera) const {
    // Infinite far plane with reverse Z depth
    f32 invHalfTan = 1.0f / tanf(glm::radians(camera.fov) / 2.0f);
    f32 aspect = camera.viewport_size.x / camera.viewport_size.y;

    return {
        invHalfTan / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, -invHalfTan, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, camera.near, 0.0f
    };
}

void World::update_vectors(Camera &camera) {
    camera.pitch = glm::clamp(camera.pitch, -89.999f, 89.999f);

    camera.forward = glm::normalize(glm::vec3(
        std::cos(glm::radians(camera.yaw)) * std::cos(glm::radians(camera.pitch)),
        std::sin(glm::radians(camera.pitch)),
        std::sin(glm::radians(camera.yaw)) * std::cos(glm::radians(camera.pitch))
    ));

    camera.right = glm::normalize(glm::cross(camera.forward, WORLD_UP));
    camera.up = glm::normalize(glm::cross(camera.right, camera.forward));
}
void World::update_frustum(Camera &camera) {
    float real_far = std::max(camera.near, camera.far);

    float aspect = camera.viewport_size.x / camera.viewport_size.y;
    float half_y = real_far * std::tan(glm::radians(camera.fov * 0.5f));
    float half_x = half_y * aspect;

    glm::vec3 far_plane = real_far * camera.forward;

    camera.right_plane = glm::normalize(glm::cross(camera.up, far_plane + camera.right * half_x));
    camera.left_plane = glm::normalize(glm::cross(far_plane - (camera.right * half_x), camera.up));
    camera.top_plane =  glm::normalize(glm::cross(far_plane + (camera.up * half_y), camera.right));
    camera.bottom_plane = glm::normalize(glm::cross(camera.right, far_plane - (camera.up * half_y)));
}

void World::_clear_updates() {
    m_changed_object_handles.clear();
}
