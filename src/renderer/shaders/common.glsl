vec3 rotate_vq(vec3 v, vec4 q) {
    // wxyz quaterions
    return v + 2.0 * cross(q.yzw, cross(q.yzw, v) + q.x * v);
}