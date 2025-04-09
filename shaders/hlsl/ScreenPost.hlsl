struct VSInput{
    float3 inPosition : ATTRIBUTE0;
};

struct PSInput{
    float4 sv_position : SV_POSITION;
    float4 position : POSITION0;
};

Texture2D screen_texture : register(t0);
SamplerState texSampler : register(s0);

PSInput VSMain(VSInput input){
    PSInput output;
    output.position = float4(input.inPosition, 1.);
    output.sv_position = float4(input.inPosition, 1.);
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET{
    return 1. - screen_texture.Sample(texSampler, input.position);
}