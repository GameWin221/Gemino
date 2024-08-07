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
    u32 width{};
    u32 height{};
    u32 bytes_per_pixel{};
    u32 mip_level_count{};
    u32 is_srgb{};
    u32 use_linear_filter{};
    Handle<u32> image{};
    Handle<u32> sampler{};
};
// std140
struct alignas(16) Material {
    Handle<Texture> albedo_texture = INVALID_HANDLE;
    Handle<Texture> roughness_texture = INVALID_HANDLE;
    Handle<Texture> metalness_texture = INVALID_HANDLE;
    Handle<Texture> normal_texture = INVALID_HANDLE;
    alignas(16) glm::vec3 color = glm::vec3(1.0f);
};
// std140
struct alignas(16) Object {
    alignas(16) glm::vec3 position{};
    alignas(16) glm::quat rotation{};
    alignas(16) glm::vec3 scale = glm::vec3(1.0f);
    f32 max_scale = 1.0f;

    Handle<Mesh> mesh = INVALID_HANDLE;
    Handle<Material> material = INVALID_HANDLE;
    u32 visible = 1U;
};

struct ObjectCreateInfo{
    Handle<Mesh> mesh = INVALID_HANDLE;
    Handle<Material> material = INVALID_HANDLE;

    bool visible = true;

    glm::vec3 position{};
    glm::quat rotation{};
    glm::vec3 scale = glm::vec3(1.0f);
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

class World {
public:
    Handle<Object> create_object(const ObjectCreateInfo &create_info);
    Handle<Camera> create_camera(const CameraCreateInfo &create_info);

    void set_position(Handle<Object> object, glm::vec3 position);
    void set_rotation(Handle<Object> object, glm::quat rotation);
    void set_scale(Handle<Object> object, glm::vec3 scale);

    void set_mesh(Handle<Object> object, Handle<Mesh> mesh);
    void set_material(Handle<Object> object, Handle<Material> material);
    void set_visibility(Handle<Object> object, bool visible);

    const Object &get_object(Handle<Object> object) const { return m_objects.get_element(object); };
    const Camera &get_camera(Handle<Camera> camera) const { return m_cameras.get_element(camera); }
    bool get_visibility(Handle<Object> object) const { return static_cast<bool>(m_objects.get_element(object).visible); };

    void set_camera_position(Handle<Camera> camera, glm::vec3 position);
    void set_camera_rotation(Handle<Camera> camera, float pitch, float yaw);
    void set_camera_fov(Handle<Camera> camera, float fov);
    void set_camera_viewport(Handle<Camera> camera, glm::vec2 viewport_size);

    const std::unordered_set<Handle<Object>>& get_valid_object_handles() const { return m_objects.get_valid_handles(); }
    const std::unordered_set<Handle<Camera>>& get_valid_camera_handles() const { return m_cameras.get_valid_handles(); }

    const std::vector<Object> &get_objects() const { return m_objects.get_all_elements(); };
    const std::vector<Camera> &get_cameras() const { return m_cameras.get_all_elements(); };

    const std::unordered_set<Handle<Object>> &get_changed_object_handles() const { return m_changed_object_handles; }

    const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);

    void _clear_updates();

private:
    glm::mat4 calculate_model_matrix(const Object &object);

    glm::mat4 calculate_view_matrix(const Camera &camera) const;
    glm::mat4 calculate_proj_matrix(const Camera &camera) const;
    void update_vectors(Camera &camera);
    void update_frustum(Camera &camera);

    HandleAllocator<Object> m_objects{};
    HandleAllocator<Camera> m_cameras{};

    std::unordered_set<Handle<Object>> m_changed_object_handles{};
};

#endif