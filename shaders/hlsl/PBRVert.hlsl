#include "PBRCommon.hlsli"

VSOutput main(VSInput input) 
{
    VSOutput output = (VSOutput)0;
    float4x4 transMat   = mul(ubo0.proj, mul(ubo0.view, ubo0.model));
    output.pos          = mul(transMat, float4(input.inPosition, 1.f));
    output.fragPos      = mul(ubo0.model, float4(input.inPosition, 1.f)).xyz;
    output.fragColor    = input.inColor;

    float4x4 invTrModel = ubo0.model; // hlsl dose not support inverse(matrix), need to pass invTr mat instead
    output.fragNormal   = mul(invTrModel, float4(input.inNormal, 0.f)).xyz;   
    output.fragTexCoord = input.inTexCoord;

    return output;
}
