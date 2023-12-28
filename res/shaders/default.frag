#version 450

layout(location = 0) in vec2 f_texcoord;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(f_texcoord.x, f_texcoord.y, 0.0, 1.0);
}