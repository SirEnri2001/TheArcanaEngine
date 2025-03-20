#version 450

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPos;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = 1. - texture(texSampler, fragPos.xy);
}
