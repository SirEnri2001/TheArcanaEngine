#version 450

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform CameraUniform {
    vec3 eye;
    vec3 forward;
    vec3 up;
    ivec2 screenres;
    float time;
    int frameId;
} cameraubo;

layout(location = 0) in vec3 fragPos;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 screenxy = (fragPos.xy + 1.0) * 0.5;
    outColor = texture(texSampler, screenxy.xy);
}
