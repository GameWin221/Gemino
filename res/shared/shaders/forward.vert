#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_nonuniform_qualifier : require

#include "forward.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec3 f_position;
layout(location = 1) out vec3 f_normal;
layout(location = 2) out vec2 f_texcoord;
layout(location = 3) out flat uint f_object_id;
layout(location = 4) out flat uint f_primitive_id;

layout(set = 0, binding = 1) readonly buffer DrawCommandBuffer {
    DrawCommand draw_commands[];
};
layout(set = 0, binding = 2) readonly buffer ObjectBuffer {
    Object objects[];
};
layout(set = 0, binding = 3) readonly buffer GlobalTransformBuffer {
    Transform global_transforms[];
};
layout(set = 0, binding = 4) readonly buffer MaterialBuffer {
    Material materials[];
};
layout(set = 0, binding = 5) readonly buffer MeshInstanceBuffer {
    MeshInstance mesh_instances[];
};
layout(set = 0, binding = 6) readonly buffer MeshInstanceMaterialsBuffer {
    uint mesh_instance_materials[];
};
layout(set = 0, binding = 7) uniform CameraBuffer {
    Camera camera;
};

void main() {
    uint object_id = draw_commands[gl_DrawIDARB].object_id;
    uint primitive_id = draw_commands[gl_DrawIDARB].primitive_id;

    Transform transform = global_transforms[object_id];

    vec3 v_world_space = rotate_vq(v_position * transform.scale, transform.rotation) + transform.position;

    gl_Position = camera.view_proj * vec4(v_world_space, 1.0);

    f_position = v_world_space;
    f_normal = normalize(rotate_vq(v_normal, transform.rotation));
    f_texcoord = v_texcoord;

    f_object_id = object_id;
    f_primitive_id = primitive_id;
}