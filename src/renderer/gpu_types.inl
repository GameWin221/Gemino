#ifndef GPU_TYPES_INL
#define GPU_TYPES_INL

#define GPU_MAX_LOD_COUNT 8

#ifdef __cplusplus
#include <vulkan/vulkan.h>
#include <meshoptimizer.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>

struct alignas(16) Camera {
    alignas(16) glm::mat4 view = glm::mat4(1.0f);
    alignas(16) glm::mat4 proj = glm::mat4(1.0f);
    alignas(16) glm::mat4 view_proj = glm::mat4(1.0f);
    alignas(16) glm::mat4 inv_view_proj = glm::mat4(1.0f);
    alignas(16) glm::mat4 inv_view = glm::mat4(1.0f);
    alignas(16) glm::mat4 inv_proj = glm::mat4(1.0f);

    alignas(16) glm::vec3 position{};
    alignas(4) float fov{}; // euler angles

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


struct Vertex {
    bool operator==(const Vertex &other) const {
        return pos == other.pos && texcoord == other.texcoord && normal == other.normal;
    }

    void set_normal_from_f32(const glm::vec3 &n) {
        normal = glm::i8vec4(glm::normalize(n) * 127.0f, 0.0f);
    }

    void set_texcoord_from_f32(const glm::vec2 &uv) {
        texcoord = glm::u16vec2(meshopt_quantizeHalf(uv.x), meshopt_quantizeHalf(uv.y));
    }

    glm::f32vec3 pos{};
    glm::i8vec4 normal{};
    glm::u16vec2 texcoord{};
};

namespace std {
    template<>
    struct hash<Vertex> {
        std::size_t operator()(const Vertex &v) const noexcept {
            std::size_t p = (((std::size_t)v.pos.x) >> 2) | (((std::size_t)v.pos.y) << 14) | (((std::size_t)v.pos.z) << 34);
            std::size_t n = (((std::size_t)v.normal.x) >> 4) | (((std::size_t)v.normal.y) << 22) | (((std::size_t)v.normal.z) << 15);
            std::size_t t = (((std::size_t)v.texcoord.x) >> 1) | (((std::size_t)v.texcoord.y) << 16);

            return p ^ (n << 1) ^ (t << 2);
        }
    };
}

struct PrimitiveLOD {
    u32 index_start{};
    u32 index_count{};
};
struct Primitive {
    i32 vertex_start{};
    u32 vertex_count{};
    std::array<PrimitiveLOD, 8> lods{};
};
struct alignas(16) Mesh {
    glm::vec3 center_offset{};
    f32 radius{};

    u32 primitive_count{};
    u32 primitive_start{};
};
struct MeshInstance {
    Handle<Mesh> mesh = INVALID_HANDLE;
    u32 material_count{};
    u32 material_start{};
    f32 lod_bias{};
    f32 cull_dist_multiplier = 1.0f;
};
struct Texture {
    Handle<struct Image> image{};
    Handle<struct Sampler> sampler{};
    u16 width{};
    u16 height{};
    u16 bytes_per_pixel{};
    u16 mip_level_count{};
    u16 is_srgb{};
    u16 use_linear_filter{};
};
struct Material {
    Handle<Texture> albedo_texture = INVALID_HANDLE;
    Handle<Texture> roughness_texture = INVALID_HANDLE;
    Handle<Texture> metalness_texture = INVALID_HANDLE;
    Handle<Texture> normal_texture = INVALID_HANDLE;
    glm::vec4 color = glm::vec4(1.0f);
};
struct Transform {
    alignas(16) glm::vec3 position{};
    alignas(16) glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    alignas(16) glm::vec3 scale = glm::vec3(1.0f);
    alignas(4) f32 max_scale = 1.0f;
};
struct Object {
    // Global transform and local transform are both allocated at the same handle as object
    Handle<MeshInstance> mesh_instance = INVALID_HANDLE;
    Handle<Object> parent = INVALID_HANDLE;
    u32 visible = 1U;
};

struct DrawCommand {
    VkDrawIndexedIndirectCommand vk_cmd{};
    u32 object_id{};
    u32 primitive_id{};
};

#else

#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require

struct Camera {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    mat4 inv_view_proj;
    mat4 inv_view;
    mat4 inv_proj;

    vec3 position;
    float fov; // euler angles

    float pitch;
    float yaw;
    float near;
    float far;

    vec2 viewport_size;

    vec3 forward;
    vec3 right;
    vec3 up;

    vec3 left_plane;
    vec3 right_plane;
    vec3 top_plane;
    vec3 bottom_plane;

    vec4 _pad0;
    vec4 _pad1;
};

struct Vertex {
    vec3 position;
    i8vec4 normal;
    f16vec2 texcoord;
};
struct PrimitiveLOD {
    uint index_start;
    uint index_count;
};
struct Primitive {
    int vertex_start;
    uint vertex_count;
    PrimitiveLOD lods[GPU_MAX_LOD_COUNT];
};
struct Mesh {
    vec3 center_offset;
    float radius;

    uint primitive_count;
    uint primitive_start;
};
struct MeshInstance {
    uint mesh;
    uint material_count;
    uint material_start;
    float lod_bias;
    float cull_dist_multiplier;
};

struct Material {
    uint albedo_texture;
    uint roughness_texture;
    uint metalness_texture;
    uint normal_texture;

    vec4 color;
};
struct Transform {
    vec3 position;
    vec4 rotation;
    vec3 scale;
    float max_scale;
};
struct Object {
    uint mesh_instance;
    uint parent;
    uint visible;
};

struct DrawCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
    uint object_id;
    uint primitive_id;
};
#endif

#endif