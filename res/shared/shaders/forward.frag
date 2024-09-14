#version 450

#extension GL_EXT_nonuniform_qualifier : require

#include "forward.glsl"

layout(location = 0) in vec3 f_position;
layout(location = 1) in vec3 f_normal;
layout(location = 2) in vec2 f_texcoord;
layout(location = 3) in flat uint f_draw_id;

layout(location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler2D textures[];

layout(set = 0, binding = 1) readonly buffer ObjectBuffer {
    Object objects[];
};
layout(set = 0, binding = 2) readonly buffer LocalTransformBuffer {
    Transform local_transforms[];
};
layout(set = 0, binding = 3) readonly buffer GlobalTransformBuffer {
    Transform global_transforms[];
};
layout(set = 0, binding = 4) readonly buffer MaterialBuffer {
    Material materials[];
};
layout(set = 0, binding = 5) readonly buffer DrawCommandIndexBuffer {
    uint draw_command_indices[];
};
layout(set = 0, binding = 6) uniform CameraBuffer {
    Camera camera;
};

void main() {
    Material material = materials[objects[f_draw_id].material];
    vec3 albedo = texture(textures[material.albedo_texture], f_texcoord).rgb * material.color;

    albedo *= max(dot(vec3(1.0), f_normal), 0.1);

    out_color = vec4(albedo, 1.0);
}