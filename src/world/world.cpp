#include "world.hpp"

void World::update_tick() {

}
void World::post_render_tick() {
    changed_object_handles.clear();
    changed_transform_handles.clear();
}

Handle<Object> World::create_object(const glm::mat4& matrix) {
    Handle<Transform> transform_handle = transforms.alloc(Transform{
        .matrix = matrix
    });
    Handle<Object> object_handle = objects.alloc(Object{
        .transform = transform_handle
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

void World::set_transform(Handle<Object> object, const glm::mat4& matrix) {
    Handle<Transform> target_handle = objects.get_element(object).transform;
    Transform& target = transforms.get_element_mutable(target_handle);

    if(target.matrix == matrix) return;

    target.matrix = matrix;
    changed_transform_handles.insert(target_handle);
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
