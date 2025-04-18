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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

void main() {
    gl_Position = View.Projection * View.View * MeshTransform.ModelInv * vec4(inPosition, 1.0);
    fragPos = (MeshTransform.Model * vec4(inPosition, 1.0)).xyz;
    fragColor = inColor;
    fragNormal = (transpose( MeshTransform.ModelInv ) * vec4(inNormal, 0.)).xyz;
    fragTexCoord = inTexCoord;
}
