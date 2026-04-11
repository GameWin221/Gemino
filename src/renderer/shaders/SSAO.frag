#version 450

#include "../gpu_types.inl"

layout (constant_id = 0) const uint SAMPLE_COUNT = 32u;
layout (constant_id = 1) const uint RECONSTRUCT_DEPTH = 0u;

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

const vec3 KERNELS[64] = vec3[64](
    vec3(0.000139, 0.025367, 0.021611), vec3(-0.011798, 0.029870, 0.045279), vec3(0.004532, -0.006560, 0.002645), vec3(0.004324, -0.002869, 0.000112),
    vec3(-0.034755, 0.042280, 0.015959), vec3(0.062946, 0.032171, 0.049013), vec3(0.053943, 0.054054, 0.037766), vec3(0.030899, -0.061110, 0.008916),
    vec3(0.036284, 0.059354, 0.021328), vec3(0.007992, 0.020404, 0.047849), vec3(-0.083642, -0.023095, 0.029153), vec3(0.008808, 0.001102, 0.000648),
    vec3(0.014434, 0.119990, 0.042573), vec3(0.050265, 0.073103, 0.058489), vec3(0.047735, -0.092192, 0.013656), vec3(0.042557, -0.051466, 0.066527),
    vec3(0.108911, -0.044227, 0.048311), vec3(-0.086438, -0.025284, 0.038949), vec3(0.056763, -0.048798, 0.139526), vec3(0.010705, -0.158709, 0.048795),
    vec3(-0.110327, 0.086870, 0.001949), vec3(-0.001589, 0.103583, 0.093301), vec3(0.082093, 0.038357, 0.092484), vec3(0.001165, -0.025283, 0.024750),
    vec3(0.048499, -0.119756, 0.032343), vec3(-0.075094, -0.135019, 0.018945), vec3(0.021117, 0.042586, 0.034145), vec3(-0.067420, -0.112460, 0.135929),
    vec3(0.151954, -0.129060, 0.046943), vec3(0.177008, -0.117778, 0.071884), vec3(-0.007972, -0.006927, 0.062789), vec3(0.000103, -0.000468, 0.000473),
    vec3(-0.074803, 0.035894, 0.104616), vec3(0.009087, 0.254554, 0.153323), vec3(0.012324, -0.107919, 0.087516), vec3(0.066724, -0.069070, 0.017316),
    vec3(-0.101884, 0.238811, 0.260016), vec3(-0.012135, 0.055739, 0.272493), vec3(-0.077100, -0.146479, 0.205354), vec3(0.020009, 0.034209, 0.003888),
    vec3(-0.005679, -0.032170, 0.063135), vec3(0.276980, 0.347408, 0.047277), vec3(-0.055644, -0.029000, 0.023189), vec3(0.287928, 0.136529, 0.313993),
    vec3(0.096442, 0.131718, 0.026621), vec3(0.129663, -0.089933, 0.032203), vec3(0.006427, -0.126122, 0.231291), vec3(0.383362, 0.187301, 0.379630),
    vec3(0.019983, -0.082412, 0.506157), vec3(-0.297500, -0.197894, 0.046182), vec3(0.049642, -0.104590, 0.138150), vec3(0.053083, 0.020026, 0.145455),
    vec3(-0.088401, -0.566860, 0.238678), vec3(0.079420, 0.029858, 0.085918), vec3(-0.025377, -0.005580, 0.002926), vec3(-0.033431, -0.278818, 0.111578),
    vec3(0.582174, 0.324844, 0.177544), vec3(-0.086912, 0.106299, 0.068030), vec3(-0.071862, -0.084915, 0.007179), vec3(0.133271, -0.130354, 0.296777),
    vec3(-0.574664, -0.310522, 0.071267), vec3(0.467377, -0.343187, 0.253032), vec3(-0.446773, 0.103682, 0.333749), vec3(0.073223, -0.053064, 0.017178)
);
const vec3 ROTATIONS[16] = vec3[16](
    vec3(0.959794, -0.871323, 0.000000), vec3(-0.479293, 0.069678, 0.000000), vec3(0.725066, -0.210037, 0.000000), vec3(-0.391610, -0.032497, 0.000000),
    vec3(-0.325192, 0.609448, 0.000000), vec3(-0.929616, 0.114368, 0.000000), vec3(-0.172465, -0.490180, 0.000000), vec3(-0.099494, -0.485109, 0.000000),
    vec3(0.997406, -0.193665, 0.000000), vec3(-0.270902, 0.171265, 0.000000), vec3(-0.623436, -0.675670, 0.000000), vec3(-0.285734, 0.910691, 0.000000),
    vec3(0.545194, 0.522630, 0.000000), vec3(-0.880723, -0.574698, 0.000000), vec3(-0.564600, -0.624145, 0.000000), vec3(-0.240742, -0.877891, 0.000000)
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
    vec3 view_normal;
    if (RECONSTRUCT_DEPTH == 0u) {
        view_normal = normalize(camera.view * vec4(texture(in_normal, f_texcoord).xyz, 0.0)).xyz;
    } else {
        view_normal = normalize(cross(dFdy(view_pos), dFdx(view_pos)));
    }

    int x = int(f_texcoord.x * noise_scale.x) % 4;
    int y = int(f_texcoord.y * noise_scale.y) % 4;

    vec3 randomVec = normalize(ROTATIONS[y * 4 + x]);

    vec3 tangent   = normalize(randomVec - view_normal * dot(randomVec, view_normal));
    vec3 bitangent = cross(view_normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, view_normal);

    float occlusion = 0.0;
    for(uint i = 0; i < SAMPLE_COUNT; ++i) {
        vec3 sample_pos = TBN * KERNELS[i];
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