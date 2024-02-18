#version 450

layout(location = 0) in vec2 f_texcoord;

layout(location = 0) out float out_depth;

layout (set = 0, binding = 0) uniform sampler2D in_depth_buffer;

void main() {
    out_depth = texture(in_depth_buffer, f_texcoord).x;
}