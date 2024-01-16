#version 450

layout(location = 0) in vec2 f_texcoord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D main_texture;

layout(push_constant, std430) uniform PushConstant {
    vec2 offset;
    vec3 color_multiplier;
} push_constants;

void main() {
    vec3 albedo = texture(main_texture, f_texcoord).rgb;

    out_color = vec4(albedo * push_constants.color_multiplier, 1.0);
}