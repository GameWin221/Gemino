#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 f_texcoord;
layout(location = 1) in flat uint f_draw_id;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D array_of_textures[];
layout(set = 0, binding = 1) uniform sampler2DArray textures_array;

layout(push_constant, std430) uniform PushConstant {
    vec2 offset;
    vec3 color_multiplier;
} push_constants;

void main() {
    vec3 albedo = texture(array_of_textures[nonuniformEXT(0)], f_texcoord).rgb;
    //vec3 albedo = texture(textures_array, vec3(f_texcoord, 0)).rgb;

    out_color = vec4(albedo * push_constants.color_multiplier, 1.0);
}