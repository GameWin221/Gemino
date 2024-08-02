struct MeshLOD {
    uint index_count;
    uint first_index;
    int vertex_offset;
};
struct Mesh {
    vec3 center_offset;
    float radius;

    float cull_distance;
    float lod_bias;

    uint lod_count;

    MeshLOD lods[8];
};
struct Object {
    vec3 position;
    vec4 rotation;
    vec3 scale;
    float max_scale;

    uint mesh;
    uint material;
    uint visible;
};
struct Material {
    uint albedo_texture;
    uint roughness_texture;
    uint metalness_texture;
    uint normal_texture;

    vec3 color;
};
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
struct DrawCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

#define LOD_DIV_CONSTANT 50.0

vec3 rotateVQ(vec3 v, vec4 q) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}