#version 450

#include "../gpu_types.inl"

layout (constant_id = 0) const uint SAMPLE_COUNT = 32u;

layout(location = 0) in vec2 f_texcoord;

layout(set = 0, binding = 0) uniform sampler2D in_depth;
layout(set = 0, binding = 1) uniform sampler2D in_normal;

layout(location = 0) out float out_ssao;

layout(set = 0, binding = 2) uniform CameraBuffer {
    Camera camera;
};

layout(push_constant) uniform PushConstant {
    int screen_wh_combined;

    float radius;
    float bias;
    float multiplier;
    float noise_scale_divider;
};

const vec3 kernels[64] = vec3[64](
    vec3(-0.068561 , 0.0591945, 0.0), vec3(-0.0722413, 0.064881 , 0.0), vec3(-0.0356842, 0.0528774, 0.0), vec3(-0.0503744,-0.024011 , 0.0),
    vec3(-0.0461418,-0.0327981, 0.0), vec3( 0.0715148, 0.0770443, 0.0), vec3( 0.0120626, 0.0119903, 0.0), vec3( 0.097976 , 0.047019 , 0.0),
    vec3(-0.0359385, 0.042113 , 0.0), vec3( 0.024831 , 0.0246512, 0.0), vec3(-0.0416858,-0.0301449, 0.0), vec3( 0.0592394,-0.0552211, 0.0),
    vec3( 0.0999734, 0.077195 , 0.0), vec3( 0.109393 , 0.0025727, 0.0), vec3(-0.034774 ,-0.116399 , 0.0), vec3( 0.0848333,-0.0563106, 0.0),
    vec3(-0.0583617, 0.103013 , 0.0), vec3( 0.0551693, 0.0546054, 0.0), vec3(-0.0657415,-0.0909375, 0.0), vec3(-0.0384386,-0.0381255, 0.0),
    vec3( 0.0049157, 0.0034071, 0.0), vec3(-0.132679 ,-0.10911  , 0.0), vec3(-0.0122596,-0.0158563, 0.0), vec3( 0.0973467, 0.148693 , 0.0),
    vec3(-0.0637455, 0.0331347, 0.0), vec3( 0.100462 , 0.0588527, 0.0), vec3( 0.0361528,-0.102876 , 0.0), vec3(-0.0165081,-0.0521415, 0.0),
    vec3(-0.186336 , 0.110243 , 0.0), vec3(-0.0651563,-0.0964584, 0.0), vec3(-0.128904 ,-0.0314065, 0.0), vec3( 0.246164 ,-0.0209147, 0.0),
    vec3( 0.219602 , 0.109242 , 0.0), vec3(-0.140972 , 0.193563 , 0.0), vec3(-0.218098 , 0.07883  , 0.0), vec3(-0.201637 , 0.125925 , 0.0),
    vec3(-0.0219822,-0.190489 , 0.0), vec3( 0.197557 , 0.117705 , 0.0), vec3( 0.224875 ,-0.095269 , 0.0), vec3(-0.0051506, 0.0057472, 0.0),
    vec3( 0.0907334, 0.0709684, 0.0), vec3( 0.0081909, 0.441157 , 0.0), vec3(-0.174515 , 0.397806 , 0.0), vec3( 0.290092 ,-0.0485296, 0.0),
    vec3(-0.0721474, 0.0099621, 0.0), vec3(-0.100516 , 0.0750153, 0.0), vec3( 0.373977 ,-0.292787 , 0.0), vec3(-0.0874456, 0.173854 , 0.0),
    vec3(-0.0793103, 0.124525 , 0.0), vec3( 0.0902032,-0.102222 , 0.0), vec3( 0.119788 ,-0.0441132, 0.0), vec3(-0.37276  , 0.521842 , 0.0),
    vec3( 0.30332  , 0.126207 , 0.0), vec3(-0.0141141, 0.0463802, 0.0), vec3( 0.287755 , 0.324245 , 0.0), vec3( 0.26092  , 0.495898 , 0.0),
    vec3( 0.0955977, 0.204283 , 0.0), vec3( 0.261691 , 0.18296  , 0.0), vec3( 0.0752712, 0.310255 , 0.0), vec3( 0.103214 ,-0.444275 , 0.0),
    vec3(-0.0105898,-0.0468894, 0.0), vec3( 0.111373 ,-0.532453 , 0.0), vec3(-0.4037   , 0.784518 , 0.0), vec3(-0.0934949, 0.117462 , 0.0)
);

const vec3 rotations[16] = vec3[16](
    vec3(-0.210184, 0.137647, 0.0), vec3(-0.225408,-0.061218, 0.0), vec3( 0.453909, -0.976196, 0.0), vec3(-0.22286 ,-0.325755, 0.0),
    vec3( 0.854986,-0.675635, 0.0), vec3(-0.127765, 0.588569, 0.0), vec3( 0.725356, -0.37757 , 0.0), vec3( 0.24072 , 0.057066, 0.0),
    vec3(-0.760906,-0.668703, 0.0), vec3(-0.056086, 0.203964, 0.0), vec3(-0.319561, -0.474057, 0.0), vec3( 0.059689, 0.308158, 0.0),
    vec3( 0.432201, 0.378429, 0.0), vec3( 0.976759, 0.496303, 0.0), vec3( 0.440987, -0.098916, 0.0), vec3( 0.825155,-0.832357, 0.0)
);

float get_view_z(vec2 uv) {
    float depth = texture(in_depth, uv).r;

    return camera.inv_proj[3][2] / (camera.inv_proj[2][3] * depth + camera.inv_proj[3][3]);
}
vec3 get_view_position(vec2 uv) {
    float depth = texture(in_depth, uv).r;

    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = camera.inv_proj * ndc;

    return view.xyz / view.w;
}

void main() {
    int screen_width = screen_wh_combined & 0xffff;
    int screen_height = (screen_wh_combined >> 16) & 0xffff;
    vec2 noise_scale = vec2(float(screen_width), float(screen_height)) / noise_scale_divider;

    vec3 view_pos = get_view_position(f_texcoord);
    vec3 view_normal = texture(in_normal, f_texcoord).xyz;

    int x = int(f_texcoord.x * noise_scale.x) % 4;
    int y = int(f_texcoord.y * noise_scale.y) % 4;

    vec3 randomVec = normalize(rotations[y * 4 + x]);

    vec3 tangent   = normalize(randomVec - view_normal * dot(randomVec, view_normal));
    vec3 bitangent = cross(view_normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, view_normal);

    float occlusion = 0.0;
    for(uint i = 0; i < SAMPLE_COUNT; ++i) {
        vec3 sample_pos = TBN * kernels[i];
        sample_pos = view_pos + sample_pos * radius;

        // Faster and shorter form of:
        //vec4 offset = camera.proj * vec4(sample_pos, 1.0);
        //offset.xyz /= offset.w;

        vec2 offsetXY = vec2(camera.proj[0][0] * sample_pos.x, camera.proj[1][1] * sample_pos.y);
        offsetXY /= camera.proj[2][3] * sample_pos.z;

        float sampleDepth = get_view_z(offsetXY * 0.5 + 0.5);

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(view_pos.z - sampleDepth));
        occlusion += (sampleDepth >= sample_pos.z + bias ? 1.0 : 0.0) * rangeCheck * multiplier;
    }

    out_ssao = 1.0 - (occlusion / SAMPLE_COUNT);
}