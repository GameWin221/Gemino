#version 450

layout(location = 0) in vec2 f_texcoord;

layout(location = 0) out vec4 out_color;

layout(push_constant, std430) uniform PushConstant {
    layout(offset = 16)
    vec3 color_multiplier;
} push_constants;

void main() {
    out_color = vec4(f_texcoord.x * push_constants.color_multiplier.r, f_texcoord.y * push_constants.color_multiplier.g, push_constants.color_multiplier.b, 1.0);
}