#define LOD_DIV_CONSTANT 50.0
#define LOD_COUNT 8

struct Camera {
    mat4 view;
    mat4 proj;
    mat4 view_proj;

    vec3 position;
    float fov;

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
struct PrimitiveLOD {
    uint index_count;
    uint first_index;
    int vertex_offset;
    uint _pad0;
};
struct Primitive {
    PrimitiveLOD lods[LOD_COUNT];
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

vec3 rotate_vq(vec3 v, vec4 q) {
    // wxyz quaterions
    return v + 2.0 * cross(q.yzw, cross(q.yzw, v) + q.x * v);
}