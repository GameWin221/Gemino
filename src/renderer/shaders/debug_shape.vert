#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_scalar_block_layout : require

#include "common.glsl"
#include "../gpu_types.inl"

layout(scalar, set = 0, binding = 0) readonly buffer VertexBuffer {
    vec3 positions[];
};
layout(set = 0, binding = 1) readonly buffer DrawCommandBuffer {
    DrawCommand draw_commands[];
};
layout(set = 0, binding = 2) readonly buffer ObjectBuffer {
    Object objects[];
};
layout(set = 0, binding = 3) readonly buffer GlobalTransformBuffer {
    Transform global_transforms[];
};
layout(set = 0, binding = 4) readonly buffer MeshBuffer {
    Mesh meshes[];
};
layout(set = 0, binding = 5) readonly buffer MeshInstanceBuffer {
    MeshInstance mesh_instances[];
};
layout(set = 0, binding = 6) uniform CameraBuffer {
    Camera camera;
};
layout(set = 0, binding = 7) readonly buffer DrawCommandCountBuffer {
    uint draw_command_count;
};

void main() {
    if(gl_InstanceIndex >= draw_command_count) {
        gl_Position = vec4(0.0);
        return;
    }

    uint object_id = draw_commands[gl_InstanceIndex].object_id;

    Transform transform = global_transforms[object_id];

    Object object = objects[object_id];
    MeshInstance mesh_instance = mesh_instances[object.mesh_instance];
    Mesh mesh = meshes[mesh_instance.mesh];
    float effective_radius = mesh.radius * transform.max_scale;

    vec3 v_world_space = rotate_vq(positions[gl_VertexIndex] * effective_radius + mesh.center_offset * transform.scale, transform.rotation) + transform.position;

    gl_Position = camera.view_proj * vec4(v_world_space, 1.0);
}