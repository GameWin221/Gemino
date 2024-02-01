#version 450

#extension GL_EXT_nonuniform_qualifier : require

#include "forward.glsl"

layout(location = 0) in vec3 f_position;
layout(location = 1) in vec3 f_normal;
layout(location = 2) in vec2 f_texcoord;
layout(location = 3) in flat uint f_draw_id;

layout(location = 0) out vec4 out_color;

layout(std140, set = 0, binding = 0) readonly buffer ObjectBuffer{
    Object objects[];
};
layout(std140, set = 0, binding = 1) readonly buffer TransformBuffer{
    Transform transforms[];
};
layout(set = 0, binding = 2) uniform CameraBuffer {
    Camera camera;
};

void main() {
    //vec3 albedo = texture(array_of_textures[nonuniformEXT(0)], f_texcoord).rgb;
    //vec3 albedo = texture(textures_array, vec3(f_texcoord, 0)).rgb;

    vec3 albedo = vec3(f_texcoord, 0.0);

    albedo *= max(dot(vec3(1.0), f_normal), 0.1);

    out_color = vec4(albedo, 1.0);
}