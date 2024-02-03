#include "world.hpp"

void World::render_finished_tick() {
    changed_object_handles.clear();
    changed_transform_handles.clear();
}

Handle<Object> World::create_object(Handle<Mesh> mesh, Handle<Material> material, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale ) {
    Transform transform{
        .position = position,
        .rotation = rotation,
        .scale = scale
    };

    transform.matrix = calculate_model_matrix(transform);

    Handle<Transform> transform_handle = transforms.alloc(transform);
    Handle<Object> object_handle = objects.alloc(Object{
        .transform = transform_handle,
        .mesh = mesh,
        .material = material
    });

    changed_object_handles.insert(object_handle);
    changed_transform_handles.insert(transform_handle);

    return object_handle;
}
Handle<Camera> World::create_camera(glm::vec2 viewport_size, glm::vec3 position, float pitch, float yaw, float fov, float near_plane, float far_plane) {
    Camera camera{
        .position = position,
        .fov = fov,
        .pitch = pitch,
        .yaw = yaw,
        .near_plane = near_plane,
        .far_plane = far_plane,
        .viewport_size = viewport_size
    };

    update_vectors(camera);
    camera.view = calculate_view_matrix(camera);
    camera.proj = calculate_proj_matrix(camera);
    camera.view_proj = camera.proj * camera.view;

    return cameras.alloc(camera);
}

void World::set_position(Handle<Object> object, glm::vec3 position) {
    Handle<Transform> target_handle = objects.get_element(object).transform;
    Transform& target = transforms.get_element_mutable(target_handle);

    if(target.position == position) return;

    target.position = position;
    target.matrix = calculate_model_matrix(target);
    changed_transform_handles.insert(target_handle);
}
void World::set_rotation(Handle<Object> object, glm::vec3 rotation) {
    Handle<Transform> target_handle = objects.get_element(object).transform;
    Transform& target = transforms.get_element_mutable(target_handle);

    if(target.rotation == rotation) return;

    target.rotation = rotation;
    target.matrix = calculate_model_matrix(target);
    changed_transform_handles.insert(target_handle);
}
void World::set_scale(Handle<Object> object, glm::vec3 scale) {
    Handle<Transform> target_handle = objects.get_element(object).transform;
    Transform& target = transforms.get_element_mutable(target_handle);

    if(target.scale == scale) return;

    target.scale = scale;
    target.matrix = calculate_model_matrix(target);
    changed_transform_handles.insert(target_handle);
}

void World::set_transform(Handle<Object> object, const Transform& transform) {
    Handle<Transform> target_handle = objects.get_element(object).transform;
    Transform& target = transforms.get_element_mutable(target_handle);

    if(target == transform) return;

    target = transform;
    changed_transform_handles.insert(target_handle);
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
    target.view = calculate_view_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_rotation(Handle<Camera> camera, float pitch, float yaw) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.pitch == pitch && target.yaw == yaw) return;

    target.pitch = pitch;
    target.yaw = yaw;

    update_vectors(target);
    target.view = calculate_view_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_fov(Handle<Camera> camera, float fov) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.fov == fov) return;

    target.fov = fov;
    target.proj = calculate_proj_matrix(target);
    target.view_proj = target.proj * target.view;
}
void World::set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size) {
    Camera& target = cameras.get_element_mutable(camera);

    if(target.viewport_size == viewport_size) return;

    target.viewport_size = viewport_size;
    target.proj = calculate_proj_matrix(target);(target);
    target.view_proj = target.proj * target.view;
}

glm::mat4 World::calculate_model_matrix(const Transform& transform) {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), transform.position);

    if (transform.rotation.y != 0.0f) {
        mat = glm::rotate(mat, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
    }
    if(transform.rotation.z != 0.0f) {
        mat = glm::rotate(mat, glm::radians(transform.rotation.z), glm::vec3(0, 0, 1));
    }
    if(transform.rotation.x != 0.0f) {
        mat = glm::rotate(mat, glm::radians(transform.rotation.x), glm::vec3(1, 0, 0));
    }

    mat = glm::scale(mat, transform.scale);

    return mat;
}

glm::mat4 World::calculate_view_matrix(const Camera& camera) const {
    return glm::lookAt(camera.position, camera.position + camera.forward, camera.up);;
}
glm::mat4 World::calculate_proj_matrix(const Camera& camera) const {
    glm::mat4 proj = glm::perspective(glm::radians(camera.fov), camera.viewport_size.x / camera.viewport_size.y, camera.near_plane, camera.far_plane);
    proj[1][1] *= -1;
    return proj;
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