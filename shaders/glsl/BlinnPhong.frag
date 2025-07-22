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

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightPos = vec3(-0.27, 0.27, 0.53);
    vec3 lightDir   = normalize(lightPos - fragPos);
    vec3 viewDir    = normalize(ubo.viewPosition.xyz - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(fragNormal, halfwayDir), 0.0), 16.0);
    float ambient = 0.1;
    float diffuse = max(dot(fragNormal, lightDir), 0.);
    outColor = (ambient + diffuse + spec) * vec4(modelubo.color, 1.0);
}
