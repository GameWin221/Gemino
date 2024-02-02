#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_nonuniform_qualifier : require

#include "forward.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec3 f_position;
layout(location = 1) out vec3 f_normal;
layout(location = 2) out vec2 f_texcoord;
layout(location = 3) out flat uint f_draw_id;

layout (set = 0, binding = 0) uniform texture2D textures[];

layout(std140, set = 0, binding = 1) readonly buffer ObjectBuffer{
    Object objects[];
};
layout(std140, set = 0, binding = 2) readonly buffer TransformBuffer{
    Transform transforms[];
};
layout(std140, set = 0, binding = 3) readonly buffer MaterialBuffer{
    Material materials[];
};
layout(set = 0, binding = 4) uniform CameraBuffer {
    Camera camera;
};

void main() {
    vec4 v_world_space = transforms[objects[gl_DrawIDARB].transform].matrix * vec4(v_position, 1.0);

    gl_Position = camera.view_proj * v_world_space;

    f_position = v_world_space.xyz;

    f_normal = normalize(mat3(transforms[gl_DrawIDARB].matrix) * v_normal);

    f_texcoord = v_texcoord;

    f_draw_id = gl_DrawIDARB;
}