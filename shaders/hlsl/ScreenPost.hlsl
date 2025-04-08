struct VSInput{
    float3 inPosition : ATTRIBUTE0;
}

struct PSInput{
    float4 position : SV_POSITION;
}

Texture2D texture : register(t0);
SamplerState texSampler : register(s0);

void VSMain(VSInput input){
    PSInput output;
    output.position = float4(inPosition, 1.);
}

float4 PSMain(PSInput input) : SV_TARGET{
    return 1. - texture.Sample(texSampler, input.position.xy);
}