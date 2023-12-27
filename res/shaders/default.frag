#version 450

layout(location = 0) in vec2 f_texcoord;

layout(location = 0) out vec4 out_color;

void main() {
    // draw red
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
}