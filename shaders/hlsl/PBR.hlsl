// #version 450

// layout(binding = 0) uniform UniformBufferObject {
//     mat4 model;
//     mat4 view;
//     mat4 proj;
//     vec4 viewPosition;
// } ubo;

// layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inColor;
// layout(location = 2) in vec2 inTexCoord;
// layout(location = 3) in vec3 inNormal;

// layout(location = 0) out vec3 fragColor;
// layout(location = 1) out vec2 fragTexCoord;
// layout(location = 2) out vec3 fragNormal;
// layout(location = 3) out vec3 fragPos;

// void main() {
//     gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
//     fragPos = (ubo.model * vec4(inPosition, 1.0)).xyz;
//     fragColor = inColor;
//     fragNormal = (transpose( inverse( ubo.model) ) * vec4(inNormal, 0.)).xyz;
//     fragTexCoord = inTexCoord;
// }

#include "PBRLighting.hlsl"

// HLSL Ver
struct Transformation
{
	float4x4    model;
	float4x4    view;
	float4x4    proj;
    float4      viewPosition;
};

struct MaterialProperty
{
    float3  albedo;      
    // float3  emission;
    float   smoothness;  
    float   metalness;   
    // float   occlusion;
};

cbuffer PerFrameUpdate : register(b0, space0) 
{ 
    Transformation ubo0; 
}

cbuffer StaticProperty : register(b1, space0)
{
    MaterialProperty material0;
}

cbuffer Lighting : register(b2, space0)
{
    float3 lightDirectionWS;
}

struct VSInput
{
    [[vk::location(0)]] float3 inPosition : POSITION0;
    [[vk::location(1)]] float3 inColor : COLOR0;
    [[vk::location(2)]] float2 inTexCoord : TEXCOORD0;
    [[vk::location(3)]] float3 inNormal : NORMAL0;
};

struct VSOutput
{
    [[vk::location(0)]] float3  fragColor       : COLOR0;
    [[vk::location(1)]] float2  fragTexCoord    : TEXCOORD0;
    [[vk::location(2)]] float3  fragNormal      : NORMAL0;
    [[vk::location(3)]] float3  fragPos         : POSITION0;
    [[vk::location(4)]] float4  pos             : SV_POSITION;
};

VSOutput VSMain(VSInput input) 
{
    VSOutput output = (VSOutput)0;
    output.pos          = mul(ubo0.proj * ubo0.view * ubo0.model, float4(input.inPosition, 1.f));
    output.fragPos      = mul(ubo0.model, float4(input.inPosition, 1.f)).xyz;
    output.fragColor    = input.inColor;

    float4x4 invTrModel = ubo0.model; // hlsl dose not support inverse(matrix), need to pass invTr mat instead
    output.fragNormal   = mul(invTrModel, float4(input.inNormal, 0.f)).xyz;   
    output.fragTexCoord = input.inTexCoord;

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{   
    // Process PBR input
    float3 viewDirWS = normalize(ubo0.viewPosition.xyz - input.fragPos);
    PBRSurfaceOutputStandard surfaceOutput;
    surfaceOutput.albedo        = input.fragColor;
    surfaceOutput.normalWS      = input.fragNormal;
    surfaceOutput.smoothness    = material0.smoothness;
    surfaceOutput.metalness     = material0.metalness;

    float3 retColor = BRDF( surfaceOutput, viewDirWS, lightDirectionWS);

    return float4(retColor, 1);
}