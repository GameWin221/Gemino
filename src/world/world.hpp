#ifndef GEMINO_WORLD_HPP
#define GEMINO_WORLD_HPP

#include <common/handle_allocator.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct alignas(16) Camera {
    alignas(16) glm::mat4 view = glm::mat4(1.0f);
    alignas(16) glm::mat4 proj = glm::mat4(1.0f);
    alignas(16) glm::mat4 view_proj = glm::mat4(1.0f);

    alignas(16) glm::vec3 position{};
    alignas(4) float fov{};

    alignas(4) float pitch{};
    alignas(4) float yaw{};
    alignas(4) float _padding0{};
    alignas(4) float _padding1{};

    alignas(4) float near_plane = 0.02f;
    alignas(4) float far_plane = 1000.0f;
    alignas(4) glm::vec2 viewport_size = glm::vec2(1.0f);

    alignas(16) glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f);
    alignas(16) glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    alignas(16) glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
};
struct alignas(16) Transform {
    alignas(16) glm::mat4 matrix = glm::mat4(1.0f);
};
struct alignas(16) Object {
    alignas(4) Handle<Transform> transform{};
};

class World {
public:
    void update_tick();
    void post_render_tick();

    Handle<Object> create_object(const glm::mat4& matrix = glm::mat4(1.0f));
    Handle<Camera> create_camera(glm::vec2 viewport_size, glm::vec3 position = glm::vec3(0.0f), float pitch = 0.0f, float yaw = 0.0f, float fov = 60.0f, float near_plane = 0.02f, float far_plane = 1000.0f);

    void set_transform(Handle<Object> object, const glm::mat4& matrix);
    const glm::mat4& get_transform(Handle<Object> object) const { return transforms.get_element(object).matrix;}

    void set_camera_position(Handle<Camera> camera, glm::vec3 position);
    void set_camera_rotation(Handle<Camera> camera, float pitch, float yaw);
    void set_camera_fov(Handle<Camera> camera, float fov);
    void set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size);

    const glm::vec3& get_camera_position(Handle<Camera> camera) const { return cameras.get_element(camera).position; }
    float get_camera_yaw(Handle<Camera> camera) const { return cameras.get_element(camera).yaw; }
    float get_camera_pitch(Handle<Camera> camera) const { return cameras.get_element(camera).pitch; }
    float get_camera_fov(Handle<Camera> camera) const { return cameras.get_element(camera).fov; }
    glm::vec2 get_camera_viewport(Handle<Camera> camera) const { return cameras.get_element(camera).viewport_size; }

    const std::vector<Object>& get_objects() const { return objects.get_all_elements(); };
    const std::vector<Transform>& get_transforms() const { return transforms.get_all_elements(); };
    const std::vector<Camera>& get_cameras() const { return cameras.get_all_elements(); };

    const std::unordered_set<Handle<Object>>& get_changed_object_handles() const { return changed_object_handles; }
    const std::unordered_set<Handle<Transform>>& get_changed_transform_handles() const { return changed_transform_handles; }

    const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);

private:
    glm::mat4 calculate_view_matrix(const Camera& camera) const;
    glm::mat4 calculate_proj_matrix(const Camera& camera) const;
    void update_vectors(Camera& camera);

    HandleAllocator<Object> objects{};
    HandleAllocator<Transform> transforms{};
    HandleAllocator<Camera> cameras{};

    std::unordered_set<Handle<Object>> changed_object_handles{};
    std::unordered_set<Handle<Transform>> changed_transform_handles{};
};

#endif