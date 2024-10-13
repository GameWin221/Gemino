#include "world.hpp"

Handle<Object> World::create_object(const ObjectCreateInfo& create_info) {
    Object object{
        .mesh_instance = create_info.mesh_instance,
        .parent = create_info.parent,
        .visible = static_cast<u32>(create_info.visible),
    };

    Transform local_transform{
        .position = create_info.local_position,
        .rotation = create_info.local_rotation,
        .scale = create_info.local_scale,
        .max_scale = glm::max(glm::max(create_info.local_scale.x, create_info.local_scale.y), create_info.local_scale.z),
    };

    // Global transform is updated automatically in the "update_objects()" function.
    Transform global_transform{};

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

Handle<Object> World::instantiate_scene_recursive(const SceneCreateInfo &create_info, u32 object_id, Handle<Object> parent_handle) {
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

    Transform obj_transform {
        .position = obj.local_position,
        .rotation = obj.local_rotation,
        .scale = obj.local_scale,
        .max_scale = glm::max(glm::max(obj.local_scale.x, obj.local_scale.y), obj.local_scale.z)
    };

    // create_info transform affects only scene roots
    if (parent_handle == INVALID_HANDLE) {
        Transform create_info_transform {
            .position = create_info.position,
            .rotation = create_info.rotation,
            .scale = create_info.scale,
            .max_scale = glm::max(glm::max(create_info.scale.x, create_info.scale.y), create_info.scale.z)
        };

        Transform new_local_transform = calculate_child_transform(obj_transform, create_info_transform);
        obj.local_position = new_local_transform.position;
        obj.local_scale = new_local_transform.scale;
        obj.local_rotation = new_local_transform.rotation;
    }

    Handle<Object> handle = create_object(obj);

    for (const auto &child_id : create_info.children[object_id]) {
        instantiate_scene_recursive(create_info, child_id, handle);
    }

    return handle;
}

Handle<Object> World::instantiate_scene(const SceneCreateInfo &create_info) {
    if(create_info.root_objects.size() > 1) {
        Handle<Object> new_root = create_object(ObjectCreateInfo {
            .local_position = create_info.position,
            .local_rotation = create_info.rotation,
            .local_scale = create_info.scale,
        });

        for (const auto &root_node_id : create_info.root_objects) {
            instantiate_scene_recursive(create_info, root_node_id, new_root);
        }

        return new_root;
    } else if(create_info.root_objects.size() == 1) {
        return instantiate_scene_recursive(create_info, create_info.root_objects[0], INVALID_HANDLE);
    } else {
        DEBUG_PANIC("Cannot instantiate an empty scene!");
        return INVALID_HANDLE;
    }
}
Handle<Object> World::instantiate_scene_object(const SceneCreateInfo &create_info, u32 object_id) {
    return instantiate_scene_recursive(create_info, object_id, INVALID_HANDLE);
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

void World::set_mesh_instance(Handle<Object> object, Handle<MeshInstance> mesh_instance) {
    Object &target = m_objects.get_element_mutable(object);

    if(target.mesh_instance == mesh_instance) return;

    target.mesh_instance = mesh_instance;
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
    f32 real_far = std::max(camera.near, camera.far);

    f32 aspect = camera.viewport_size.x / camera.viewport_size.y;
    f32 half_y = real_far * std::tan(glm::radians(camera.fov * 0.5f));
    f32 half_x = half_y * aspect;

    glm::vec3 far_plane = real_far * camera.forward;

    camera.right_plane = glm::normalize(glm::cross(camera.up, far_plane + camera.right * half_x));
    camera.left_plane = glm::normalize(glm::cross(far_plane - (camera.right * half_x), camera.up));
    camera.top_plane = glm::normalize(glm::cross(far_plane + (camera.up * half_y), camera.right));
    camera.bottom_plane = glm::normalize(glm::cross(camera.right, far_plane - (camera.up * half_y)));
}

Transform World::calculate_child_transform(const Transform &local_transform, const Transform &parent_transform) {
    Transform child_transform{
        .position = parent_transform.rotation * local_transform.position * parent_transform.scale + parent_transform.position,
        .rotation = parent_transform.rotation * local_transform.rotation,
        .scale = local_transform.scale * parent_transform.scale,
    };

    child_transform.max_scale = glm::max(glm::max(child_transform.scale.x, child_transform.scale.y), child_transform.scale.z);

    return child_transform;
}

void World::update_objects() {
    std::vector<Handle<Object>> changed_objects_copy{};
    changed_objects_copy.reserve(m_changed_object_handles.size());

    for (const auto &handle : m_changed_object_handles) {
        changed_objects_copy.push_back(handle);
    }

    for (const auto &obj : changed_objects_copy) {
        update_object_recursive(obj);
    }
}

void World::update_object_recursive(Handle<Object> object_handle) {
    const auto &object = m_objects.get_element(object_handle);
    const auto &local_transform = m_local_transforms.get_element(object_handle);

    m_changed_object_handles.insert(object_handle);

    if (object.parent != INVALID_HANDLE) {
        const auto &parent_transform = m_global_transforms.get_element(object.parent);

        m_global_transforms.get_element_mutable(object_handle) = calculate_child_transform(local_transform, parent_transform);
    } else {
        m_global_transforms.get_element_mutable(object_handle) = local_transform;
    }

    for (const auto &child_obj : m_children.get_element(object_handle)) {
        update_object_recursive(child_obj);
    }
}

void World::_clear_updates() {
    m_changed_object_handles.clear();
}
