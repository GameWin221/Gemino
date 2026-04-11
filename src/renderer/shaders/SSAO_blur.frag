#version 450

#include "../gpu_types.inl"

layout(location = 0) in vec2 f_texcoord;
layout(location = 0) out float out_ssao;

layout(set = 0, binding = 0) uniform sampler2D in_image;

layout(push_constant) uniform PushConstant {
    int blur_dir;
    int screen_wh_combined;
    float blur_radius;
};

void main() {
    int screen_width = screen_wh_combined & 0xffff;
    int screen_height = (screen_wh_combined >> 16) & 0xffff;
    vec2 texel = vec2(1.0 / float(screen_width), 1.0 / float(screen_height));

    // blur_dir=0 is horizontal, blur_dir=1 is vertical
    vec2 offset = texel * vec2(1 - blur_dir, blur_dir);

    float axis_val =
        texture(in_image, f_texcoord).r +
        texture(in_image, f_texcoord + offset * blur_radius).r +
        texture(in_image, f_texcoord + offset * blur_radius / 2.0).r +
        texture(in_image, f_texcoord - offset * blur_radius / 2.0).r +
        texture(in_image, f_texcoord - offset * blur_radius).r;

    out_ssao = axis_val / 5.0;
}