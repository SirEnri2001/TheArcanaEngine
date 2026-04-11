#version 450

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform CameraUniform {
    vec3 eye;
    vec3 forward;
    vec3 up;
    ivec2 screenres;
    float time;
    int frameId;
    float enableGamma;
} cameraubo;

layout(location = 0) in vec3 fragPos;
layout(location = 0) out vec4 outColor;

vec3 HDRToSRGB(vec3 color)
{
    // 1. exposure
    float exposure = 1.0;
    color *= exposure;

    // 2. ACES tone mapping
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    color = clamp((color*(a*color+b))/(color*(c*color+d)+e), 0., 1.);

    // 3. gamma (sRGB)
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}

void main() {
    vec2 screenxy = (fragPos.xy + 1.0) * 0.5;
    vec3 hdrColor = texture(texSampler, screenxy.xy).xyz;
    outColor.xyz = HDRToSRGB(hdrColor);
    //outColor.xyz = hdrColor;
    outColor.w = 1.;
}
