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

layout (set = 0, binding = 0) uniform sampler2D textures[];

layout(set = 0, binding = 1) readonly buffer ObjectBuffer {
    Object objects[];
};
layout(set = 0, binding = 2) readonly buffer MaterialBuffer {
    Material materials[];
};
layout(set = 0, binding = 3) readonly buffer DrawCommandIndexBuffer {
    uint draw_command_indices[];
};
layout(set = 0, binding = 4) uniform CameraBuffer {
    Camera camera;
};

void main() {
    uint object_id = draw_command_indices[gl_DrawIDARB];

    vec4 v_world_space = objects[object_id].matrix * vec4(v_position, 1.0);

    gl_Position = camera.view_proj * v_world_space;

    f_position = v_world_space.xyz;

    f_normal = normalize(mat3(objects[object_id].matrix) * v_normal);

    f_texcoord = v_texcoord;

    f_draw_id = object_id;
}