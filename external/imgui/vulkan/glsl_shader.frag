#version 450 core
layout(location = 0) out vec4 fColor;

layout(set=0, binding=0) uniform sampler2D sTexture;

layout(location = 0) in vec4 Color;
layout(location = 1) in vec2 UV;

void main()
{
    fColor = Color * texture(sTexture, UV.st);
}
