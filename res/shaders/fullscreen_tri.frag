#version 450

layout(location = 0) in vec2 f_texcoord;

layout(set = 0, binding = 0) uniform sampler2D in_image;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(in_image, f_texcoord);
}