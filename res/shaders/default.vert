#version 450

#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) out vec2 f_texcoord;
layout(location = 1) out flat uint f_draw_id;

layout(push_constant, std430) uniform PushConstant {
    vec2 offset;
    vec3 color_multiplier;
} push_constants;

vec2 vert_positions[6] = vec2[](
    vec2(-0.5, 0.5), vec2(0.5, 0.5), vec2(0.5, -0.5),
    vec2(-0.5, 0.5), vec2(0.5, -0.5), vec2(-0.5, -0.5)
);

void main() {
    f_draw_id = gl_DrawIDARB;
    f_texcoord = vert_positions[gl_VertexIndex] + vec2(0.5);
    gl_Position = vec4(vert_positions[gl_VertexIndex] + push_constants.offset, 0.0, 1.0);
}