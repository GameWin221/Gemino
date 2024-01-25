#version 450

#extension GL_ARB_shader_draw_parameters : require

#include "forward.glsl"

layout(location = 0) out vec2 f_texcoord;
layout(location = 1) out flat uint f_draw_id;

layout(std140, set = 0, binding = 0) readonly buffer ObjectBuffer{
    Object objects[];
};
layout(std140, set = 0, binding = 1) readonly buffer TransformBuffer{
    Transform transforms[];
};
layout(set = 0, binding = 2) uniform CameraBuffer {
    Camera camera;
};

vec2 vert_positions[6] = vec2[](
    vec2(-0.5, 0.5), vec2(0.5, 0.5), vec2(0.5, -0.5),
    vec2(-0.5, 0.5), vec2(0.5, -0.5), vec2(-0.5, -0.5)
);

void main() {
    f_draw_id = gl_InstanceIndex;//gl_DrawIDARB;
    f_texcoord = vert_positions[gl_VertexIndex] + vec2(0.5);
    gl_Position = camera.view_proj * transforms[objects[f_draw_id].transform].matrix * vec4(vert_positions[gl_VertexIndex].x, 0.0, vert_positions[gl_VertexIndex].y, 1.0);
}