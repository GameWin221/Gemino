struct MeshLOD {
    vec3 center;
    float radius;

    uint index_count;
    uint first_index;
    int vertex_offset;
};
struct Mesh {
    float cull_distance;
    float lod_bias;
    uint lod_count;
    uint lods[8];
};
struct Object {
    uint mesh;
    uint material;
    uint visible;

    mat4 matrix;
    vec3 position;
    vec3 rotation;
    vec3 scale;
    float max_scale;
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

// Niagara, Arseny Kapoulkine
// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool project_sphere(vec3 c, float r, float znear, float P00, float P11, out vec4 aabb) {
    if (c.z < r + znear) {
        return false;
    }

    vec3 cr = c * r;
    float czr2 = c.z * c.z - r * r;

    float vx = sqrt(c.x * c.x + czr2);
    float minx = (vx * c.x - cr.z) / (vx * c.z + cr.x);
    float maxx = (vx * c.x + cr.z) / (vx * c.z - cr.x);

    float vy = sqrt(c.y * c.y + czr2);
    float miny = (vy * c.y - cr.z) / (vy * c.z + cr.y);
    float maxy = (vy * c.y + cr.z) / (vy * c.z - cr.y);

    aabb = vec4(minx * P00, miny * P11, maxx * P00, maxy * P11);
    aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f); // clip space -> uv space

    return true;
}