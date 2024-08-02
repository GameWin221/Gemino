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

    Object object = objects[object_id];

    vec3 v_world_space = rotateVQ(v_position, object.rotation) * object.scale + object.position;

    gl_Position = camera.view_proj * vec4(v_world_space, 1.0);

    f_position = v_world_space;

    f_normal = normalize(rotateVQ(v_normal, object.rotation));

    f_texcoord = v_texcoord;

    f_draw_id = object_id;
}