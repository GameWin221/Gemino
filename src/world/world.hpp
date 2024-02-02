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
struct alignas(16) Mesh {
    alignas(4) u32 index_count{};
    alignas(4) u32 first_index{};
    alignas(4) i32 vertex_offset{};
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
struct alignas(16) Material {
    alignas(4) Handle<Texture> albedo_texture = INVALID_HANDLE;
    alignas(4) Handle<Texture> roughness_texture = INVALID_HANDLE;
    alignas(4) Handle<Texture> metalness_texture = INVALID_HANDLE;
    alignas(4) Handle<Texture> normal_texture = INVALID_HANDLE;
    alignas(16) glm::vec3 color = glm::vec3(1.0f);
};
struct alignas(16) Transform {
    alignas(16) glm::mat4 matrix = glm::mat4(1.0f);
};
struct alignas(16) Object {
    alignas(4) Handle<Transform> transform = INVALID_HANDLE;
    alignas(4) Handle<Mesh> mesh = INVALID_HANDLE;
    alignas(4) Handle<Material> material = INVALID_HANDLE;
    alignas(4) u32 visible = 1U;
};

class World {
public:
    void render_finished_tick();

    Handle<Object> create_object(Handle<Mesh> mesh = INVALID_HANDLE, Handle<Material> material = INVALID_HANDLE, const glm::mat4& matrix = glm::mat4(1.0f));
    Handle<Camera> create_camera(glm::vec2 viewport_size, glm::vec3 position = glm::vec3(0.0f), float pitch = 0.0f, float yaw = 0.0f, float fov = 60.0f, float near_plane = 0.02f, float far_plane = 1000.0f);

    void set_transform(Handle<Object> object, const glm::mat4& matrix);
    void set_mesh(Handle<Object> object, Handle<Mesh> mesh);
    void set_material(Handle<Object> object, Handle<Material> material);
    void set_visibility(Handle<Object> object, bool visible);

    const Object& get_object(Handle<Object> object) const { return objects.get_element(object); };
    const Transform& get_transform(Handle<Transform> transform) const { return transforms.get_element(transform); }
    const Camera& get_camera(Handle<Camera> camera) const { return cameras.get_element(camera); }
    bool get_visibility(Handle<Object> object) const { return static_cast<bool>(objects.get_element(object).visible); };

    void set_camera_position(Handle<Camera> camera, glm::vec3 position);
    void set_camera_rotation(Handle<Camera> camera, float pitch, float yaw);
    void set_camera_fov(Handle<Camera> camera, float fov);
    void set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size);

    const glm::vec3& get_camera_position(Handle<Camera> camera) const { return cameras.get_element(camera).position; }
    float get_camera_yaw(Handle<Camera> camera) const { return cameras.get_element(camera).yaw; }
    float get_camera_pitch(Handle<Camera> camera) const { return cameras.get_element(camera).pitch; }
    float get_camera_fov(Handle<Camera> camera) const { return cameras.get_element(camera).fov; }
    glm::vec2 get_camera_viewport(Handle<Camera> camera) const { return cameras.get_element(camera).viewport_size; }

    const std::unordered_set<Handle<Object>>& get_valid_object_handles() const { return objects.get_valid_handles(); }
    const std::unordered_set<Handle<Transform>>& get_valid_transform_handles() const { return transforms.get_valid_handles(); }
    const std::unordered_set<Handle<Camera>>& get_valid_camera_handles() const { return cameras.get_valid_handles(); }

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