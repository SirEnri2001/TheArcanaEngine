#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 viewPosition;
} ubo;

layout(binding = 2) uniform ModelUniform {
    mat4 model;
    mat4 modelInv;
    vec3 color;
} modelubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

void main() {
    gl_Position = ubo.proj * ubo.view * modelubo.model * vec4(inPosition, 1.0);
    fragPos = (modelubo.model * vec4(inPosition, 1.0)).xyz;
    fragColor = inColor;
    fragNormal = normalize((transpose(modelubo.modelInv) * vec4(inNormal, 0.)).xyz);
    fragTexCoord = inTexCoord;
}
