#version 450

layout(binding = 0) uniform ViewObject {
    mat4 View;
    mat4 Projection;
    vec4 ViewPosition;
} View;

layout(binding = 1) uniform MeshTransformObject {
    mat4 Model;
    mat4 ModelInv;
} MeshTransform;

layout(binding = 2) uniform sampler2D MeshTexture;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightPos = vec3(4., 0., 0.);
    vec3 lightDir   = normalize(lightPos - fragPos);
    vec3 viewDir    = normalize(View.ViewPosition.xyz - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(fragNormal, halfwayDir), 0.0), 16.0);
    float ambient = 0.1;
    float diffuse = max(dot(fragNormal, lightDir), 0.);
    outColor = (ambient + diffuse + spec) * texture(MeshTexture, fragTexCoord);
}
