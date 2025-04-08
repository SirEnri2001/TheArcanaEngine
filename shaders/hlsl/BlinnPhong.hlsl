struct VSInput{
    float3 inPosition : ATTRIBUTE0;
    float3 inColor : ATTRIBUTE1;
    float2 inTexCoord : ATTRIBUTE2;
    float3 inNormal : ATTRIBUTE3;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 fragColor : COLOR0;
    float2 fragTexCoord : TEXCOORD0;
    float3 fragNormal : NORMAL;
    float3 fragPos : POSITION0;
};

Texture2D g_texture : register(t0);
SamplerState texSampler : register(s0);
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 viewPosition;
    float4x4 modelInv;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    float4x4 mpv = mul(proj, mul(view, model));
    result.position = mul(mpv, float4(input.inPosition, 1.0));
    result.fragPos = mul(model, float4(input.inPosition, 1.)).xyz;
    result.fragColor = input.inColor;
    result.fragNormal = mul(transpose(modelInv), float4(input.inNormal, 0.)).xyz;
    result.fragTexCoord = input.inTexCoord;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 lightPos = float3(4., 0., 0.);
    float3 lightDir = normalize(lightPos - input.fragPos);
    float3 viewDir = normalize(viewPosition.xyz - input.fragPos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(input.fragNormal, halfwayDir), 0.), 16.);
    float ambient = 0.1;
    float diffuse = max(dot(input.fragNormal, lightDir), 0.);
    return (ambient + diffuse + spec) * g_texture.Sample(texSampler, input.fragTexCoord);
}
