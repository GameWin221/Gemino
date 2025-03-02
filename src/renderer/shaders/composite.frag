#version 450

layout(location = 0) in vec2 f_texcoord;

layout(set = 0, binding = 0) uniform sampler2D in_albedo;
layout(set = 0, binding = 1) uniform sampler2D in_normal;
layout(set = 0, binding = 2) uniform sampler2D in_ssao;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 albedo = texture(in_albedo, f_texcoord).rgb;
    vec3 normal = texture(in_normal, f_texcoord).xyz;
    float ssao = texture(in_ssao, f_texcoord).r;

    vec3 ambient = ssao * vec3(0.1, 0.1, 0.15) * 2.0;
    float light = max(0.0, dot(normal, normalize(vec3(1.0))));

    out_color = vec4(albedo * (ambient + light), 1.0);
}