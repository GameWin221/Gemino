#version 450

layout(location = 0) in vec2 f_texcoord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D main_texture;

layout(push_constant, std430) uniform PushConstant {
    layout(offset = 16)
    vec3 color_multiplier;
} push_constants;

void main() {
    vec3 albedo = texture(main_texture, f_texcoord).rgb;

    out_color = vec4(albedo, 1.0);//vec4(albedo.r * push_constants.color_multiplier.r, albedo.g * push_constants.color_multiplier.g, albedo.b * push_constants.color_multiplier.b, 1.0);
}