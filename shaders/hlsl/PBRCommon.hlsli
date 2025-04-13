#ifndef PBR_COMMON
#define PBR_COMMON

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
#endif
