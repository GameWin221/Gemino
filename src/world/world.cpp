#include "world.hpp"

Handle<Object> World::create_object(const ObjectCreateInfo& create_info) {
    Object object{
        .mesh = create_info.mesh,
        .material = create_info.material,
        .visible = static_cast<u32>(create_info.visible),
        .position = create_info.position,
        .rotation = create_info.rotation,
        .scale = create_info.scale,
        .max_scale = glm::max(glm::max(create_info.scale.x, create_info.scale.y), create_info.scale.z)
    };

    object.matrix = calculate_model_matrix(object);

    Handle<Object> object_handle = objects.alloc(object);

    changed_object_handles.insert(object_handle);

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

    return cameras.alloc(camera);
}

void World::set_position(Handle<Object> object, glm::vec3 position) {
    Object& target = objects.get_element_mutable(object);

    if(target.position == position) return;

    target.position = position;
    target.matrix = calculate_model_matrix(target);
    changed_object_handles.insert(object);
}
void World::set_rotation(Handle<Object> object, glm::vec3 rotation) {
    Object& target = objects.get_element_mutable(object);

    if(target.rotation == rotation) return;

    target.rotation = rotation;
    target.matrix = calculate_model_matrix(target);
    changed_object_handles.insert(object);
}
void World::set_scale(Handle<Object> object, glm::vec3 scale) {
    Object& target = objects.get_element_mutable(object);

    if(target.scale == scale) return;

    target.scale = scale;
    target.max_scale = glm::max(glm::max(scale.x, scale.y), scale.z);
    target.matrix = calculate_model_matrix(target);
    changed_object_handles.insert(object);
}

void World::set_mesh(Handle<Object> object, Handle<Mesh> mesh) {
    Object& target = objects.get_element_mutable(object);

    if(target.mesh == mesh) return;

    target.mesh = mesh;
    changed_object_handles.insert(object);
}
void World::set_material(Handle<Object> object, Handle<Material> material) {
    Object& target = objects.get_element_mutable(object);

    if(target.material == material) return;

    target.material = material;
    changed_object_handles.insert(object);
}
void World::set_visibility(Handle<Object> object, bool visible) {
    Object& target = objects.get_element_mutable(object);

    if(target.visible == static_cast<u32>(visible)) return;

    target.visible = visible;
    changed_object_handles.insert(object);
}

void World::set_camera_position(Handle<Camera> camera, glm::vec3 position) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.position == position) return;

    target.position = position;

    update_vectors(target);
    update_frustum(target);
    target.view = calculate_view_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_rotation(Handle<Camera> camera, float pitch, float yaw) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.pitch == pitch && target.yaw == yaw) return;

    target.pitch = pitch;
    target.yaw = yaw;

    update_vectors(target);
    update_frustum(target);
    target.view = calculate_view_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_fov(Handle<Camera> camera, float fov) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.fov == fov) return;

    target.fov = fov;

    update_frustum(target);
    target.proj = calculate_proj_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.viewport_size == viewport_size) return;

    target.viewport_size = viewport_size;

    update_frustum(target);
    target.proj = calculate_proj_matrix(target);
    target.view_proj = target.proj * target.view;
}

glm::mat4 World::calculate_model_matrix(const Object& object) {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), object.position);

    if (object.rotation.y != 0.0f) {
        mat = glm::rotate(mat, glm::radians(object.rotation.y), glm::vec3(0, 1, 0));
    }
    if(object.rotation.z != 0.0f) {
        mat = glm::rotate(mat, glm::radians(object.rotation.z), glm::vec3(0, 0, 1));
    }
    if(object.rotation.x != 0.0f) {
        mat = glm::rotate(mat, glm::radians(object.rotation.x), glm::vec3(1, 0, 0));
    }

    mat = glm::scale(mat, object.scale);

    return mat;
}

glm::mat4 World::calculate_view_matrix(const Camera& camera) const {
    return glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
}
glm::mat4 World::calculate_proj_matrix(const Camera& camera) const {
    // Swap Z near and far to achieve inverted depth
    glm::mat4 proj = glm::perspective(glm::radians(camera.fov), camera.viewport_size.x / camera.viewport_size.y, camera.far, camera.near);
    proj[1][1] *= -1;
    return proj;
}

void World::update_vectors(Camera& camera) {
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
    changed_object_handles.clear();
}


