#version 450

layout(location = 0) out vec2 f_texcoord;

void main() {
    f_texcoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(f_texcoord * 2.0 - 1.0, 0.0, 1.0);
}