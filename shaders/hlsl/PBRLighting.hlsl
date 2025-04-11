#include "Numeric.hlsli"

struct PBRSurfaceOutputStandard
{
    float3  albedo;
    float3  normalWS;
    // float3  emission;
    float   smoothness;
    float   metalness;
    // float   occlusion;
};

// PBR Shading from Unreal Engine 4 https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// Cook-Torrance BRDF
float3 BRDF(PBRSurfaceOutputStandard surfaceOutput, float3 viewDirectionWS, float3 lightDirectionWS)
{
    float3 h = normalize(viewDirectionWS + lightDirectionWS);
    float hDotV = max(0.f, dot(h, viewDirectionWS));
    float nDotH = max(0.f, dot(surfaceOutput.normalWS, h));
    float nDotV = max(0.f, dot(surfaceOutput.normalWS, viewDirectionWS));
    float nDotL = max(0.f, dot(surfaceOutput.normalWS, lightDirectionWS));
    
    /* Diffuse Part*/
    float3 lambert = surfaceOutput.albedo / PI;
    
    /* Specular Part*/
    float roughness = 1 - surfaceOutput.smoothness;
    roughness = clamp(roughness, 0.25f, 1.f - 0.25f);   
    float alpha = roughness;
    float alphaSqr = alpha * alpha;
    
    // NDF: Trowbridge-Reiz GGX
    float D = alphaSqr / ( PI * pow((nDotH * nDotH * (alphaSqr - 1.f) + 1.f), 2) );

    // Geometric Attenuation: Smith's
    // k = roughness^2 / 2
    // Disney's modification uses roughness = 0.5(roughness+1) to reduce hotness, but it only applies to analytic light sources
    float kDirect = pow(roughness + 1.f, 2) / 8.f;
    float Gv = nDotV / ( (nDotV * (1.f - kDirect)) + kDirect);
    float Gl = nDotL / ( (nDotL * (1.f - kDirect)) + kDirect);
    float G = Gv * Gl;  

    // Fresnel
    float3 F0 = (0.04f).xxx; // dielectric reflactance
    F0 = lerp(F0, surfaceOutput.albedo, surfaceOutput.metalness);
    float3 F = F0 + (1.f - F0) * pow((1 - nDotL), 5);
    
    /* Combine BRDF */
    float3 lo = (1.f - F) * lambert + F * D * G / max(EPSILON, 4.f * nDotV * nDotV);
    return lo;
}

/*
The following are parameters from Disney’s model that we chose not to adopt for our base material
model and instead treat as special cases:

Subsurface Samples shadow maps differently
Anisotropy Requires many IBL samples
Clearcoat Requires double IBL samples
Sheen Not well defined in Burley’s notes

*/