#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragPos;

void main() {
    gl_Position = vec4(inPosition, 1.);
    fragPos = inPosition;
}
