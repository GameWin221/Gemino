struct Object {
    uint mesh;
    uint material;
    uint visible;

    mat4 matrix;
    vec3 position;
    vec3 rotation;
    vec3 scale;
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

    vec3 rotation;

    float near_plane;
    float far_plane;
    vec2 viewport_size;

    vec3 forward;
    vec3 right;
    vec3 up;
};
