struct Transform {
    mat4 matrix;
};
struct Object {
    uint transform;
    uint mesh;
    uint visible;
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