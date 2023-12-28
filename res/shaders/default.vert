#version 450

layout(location = 0) out vec2 f_texcoord;

vec2 vert_positions[3] = vec2[](
    vec2(-0.5, 0.5), vec2(0.5, 0.5), vec2(0.0, -0.5)
);

void main() {
    f_texcoord = vert_positions[gl_VertexIndex] + vec2(0.5);
    gl_Position = vec4(vert_positions[gl_VertexIndex], 0.0, 1.0);
}