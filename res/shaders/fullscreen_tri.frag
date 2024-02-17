#version 450

layout(location = 0) in vec2 f_texcoord;

layout(set = 0, binding = 0) uniform sampler2D in_image;

layout(location = 0) out vec4 f_color;

void main() {
    f_color = texture(in_image, f_texcoord);
}