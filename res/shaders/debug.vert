#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_nonuniform_qualifier : require

#include "forward.glsl"

layout(location = 0) out vec3 f_color;

const vec2[] vertices = vec2[6](
    vec2(-1.0f, -1.0f), vec2(1.0f, -1.0f), vec2(-1.0f, 1.0f),
    vec2( 1.0f, -1.0f), vec2(1.0f,  1.0f), vec2(-1.0f, 1.0f)
);

layout(push_constant) uniform PushConstant {
    uint draw_count_pre_cull;
    float global_lod_bias;
    float depth_hierarchy_width;
    float depth_hierarchy_height;
};

layout(set = 0, binding = 0) readonly buffer MeshLODBuffer {
    MeshLOD lods[];
};
layout(set = 0, binding = 1) readonly buffer MeshBuffer {
    Mesh meshes[];
};
layout(set = 0, binding = 2) readonly buffer ObjectBuffer {
    Object objects[];
};
layout(set = 0, binding = 3) readonly buffer DrawCommandIndexBuffer {
    uint draw_command_indices[];
};
layout(set = 0, binding = 4) readonly buffer AABBBuffer {
    vec4 debug_aabb[];
};
layout(set = 0, binding = 5) readonly buffer VisibilityBuffer {
    uint visibility_buffer[];
};
layout(set = 0, binding = 6) buffer DrawCommandCountBuffer {
    uint draw_command_count;
};

void main() {
    if(gl_InstanceIndex >= draw_command_count) {
        gl_Position = vec4(0.0);
        return;
    }

    uint object_id = draw_command_indices[gl_InstanceIndex];

    vec4 aabb = debug_aabb[object_id];

    vec2 center = ((aabb.xy + aabb.zw) * 0.5) * 2.0 - 1.0;
    vec2 extent = vec2(aabb.z - aabb.x, aabb.w - aabb.y);

    if(visibility_buffer[object_id] == 0u/* || abs(center.x) > 1.0 || abs(center.y) > 1.0*/) {
        gl_Position = vec4(0.0);
        return;
    }

    vec2 corner = vertices[gl_VertexIndex] * extent + center;

    float width = (aabb.z - aabb.x) * depth_hierarchy_width;
    float height = (aabb.w - aabb.y) * depth_hierarchy_height;

    float level = floor(log2(max(width, height)));

    f_color = vec3(1.0 - (level / 12.0), 0.0, 0.0);

    gl_Position = vec4(corner, 0.0, 1.0);
}