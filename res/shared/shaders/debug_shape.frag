#version 450

#include "forward.glsl"

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstant {
    float debug_shape_opacity;
};

void main() {
    out_color = vec4(0.0, 1.0, 0.0, debug_shape_opacity);
}