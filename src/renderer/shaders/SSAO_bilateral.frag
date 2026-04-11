#version 450

#include "../gpu_types.inl"

layout(location = 0) in vec2 f_texcoord;
layout(location = 0) out float out_ssao;

layout(set = 0, binding = 0) uniform sampler2D in_image;
layout(set = 0, binding = 1) uniform sampler2D in_depth;
layout(set = 0, binding = 2) uniform CameraBuffer {
    Camera camera;
};
layout(push_constant) uniform PushConstant {
    int blur_dir;
    int screen_wh_combined;
    float blur_radius;
};

#define SIGMA 15.0
//#define BSIGMA 0.1
#define BSIGMA 0.1
#define MSIZE 7

float normpdf(in float x, in float sigma) {
    return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

float get_view_z(vec2 uv) {
    float depth = texture(in_depth, uv).r;

    return camera.inv_proj[3][2] / (camera.inv_proj[2][3] * depth + camera.inv_proj[3][3]);
}

void main(void) {
    int screen_width = screen_wh_combined & 0xffff;
    int screen_height = (screen_wh_combined >> 16) & 0xffff;
    vec2 texel = vec2(1.0 / float(screen_width), 1.0 / float(screen_height));

    float c = texture(in_image, f_texcoord).r;
    float d = get_view_z(f_texcoord);

    //declare stuff
    const int kSize = (MSIZE-1)/2;
    float kernel[MSIZE];
    float bfinal_colour = 0.0;

    float bZ = 0.0;

    //create the 1-D kernel
    for (int j = 0; j <= kSize; ++j) {
        float norm = normpdf(float(j), SIGMA);
        kernel[kSize+j] = norm;
        kernel[kSize-j] = norm;
    }

    float cc;
    float gfactor;
    float bfactor;
    float bZnorm = 1.0/normpdf(0.0, BSIGMA);
    //read out the texels
    for (int i=-kSize; i <= kSize; ++i) {
        for (int j=-kSize; j <= kSize; ++j) {
            // color at pixel in the neighborhood
            vec2 coord = f_texcoord + vec2(float(i), float(j))*texel;
            //cc = texture(in_image, coord).r;
            //// compute both the gaussian smoothed and bilateral
            //gfactor = kernel[kSize+j]*kernel[kSize+i];
            //bfactor = normpdf(cc-c, BSIGMA)*bZnorm*gfactor;
            //bZ += bfactor;
            //bfinal_colour += bfactor*cc;
            float dd = get_view_z(coord);
            // compute both the gaussian smoothed and bilateral
            gfactor = kernel[kSize+j]*kernel[kSize+i];
            bfactor = normpdf(dd-d, BSIGMA)*bZnorm*gfactor;
            bZ += bfactor;
            bfinal_colour += bfactor*texture(in_image, coord).r;
        }
    }

    out_ssao = bfinal_colour / bZ;
}