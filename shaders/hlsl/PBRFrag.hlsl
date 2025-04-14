#include "PBRCommon.hlsli"
#include "PBRLighting.hlsl"

float4 main(VSOutput input) : SV_TARGET
{   
    // Process PBR input
    float3 viewDirWS = normalize(ubo0.viewPosition.xyz - input.fragPos);
    PBRSurfaceOutputStandard surfaceOutput;
    surfaceOutput.albedo        = material0.albedo;
    surfaceOutput.normalWS      = input.fragNormal;
    surfaceOutput.smoothness    = material0.smoothness;
    surfaceOutput.metalness     = material0.metalness;

    float3 retColor = BRDF( surfaceOutput, viewDirWS, lightDirectionWS);

    return float4(retColor, 1);
}