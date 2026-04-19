
SurfaceIntersect NewSurfaceIntersect(){
    SurfaceIntersect Inters;
    
    Inters.t = 1e30;
    Inters.pointIntersectWorld = vec3(0., 0., 0.);
    Inters.baseColor = vec3(0., 0., 0.);
    Inters.emissive = vec3(0., 0., 0.);
    Inters.worldNormal = vec3(0., 0., 0.);
    Inters.texcoord = vec2(0., 0.);
    Inters.objId = -1;
    return Inters;
}

#include "Geometry.glsl"

vec2 StratifiedSample2D(uint sampleIndex, uint sqrtN, vec2 pixel, float seed)
{
    uint x = sampleIndex % sqrtN;
    uint y = sampleIndex / sqrtN;
    vec2 jitter = hash22(vec2(pixel) + float(sampleIndex) + seed);

    return (vec2(x, y) + jitter) / float(sqrtN);
}


vec2 SampleXi_Stratify(in float seed, in uint tid){
    uint sqrtN = 16;
    uint frames = uniforms.frameId;
    uint sampleIndex = Permute(frames + Permute2(tid), (sqrtN * sqrtN), 123456789);
    //uint sampleIndex = lowbias32(frames) % (sqrtN * sqrtN);
    uint sx = sampleIndex % sqrtN;
    uint sy = sampleIndex / sqrtN;
    // uint sx = gl_GlobalInvocationID.x;
    // uint sy = gl_GlobalInvocationID.y;
    // // the mod might have efficiency issues
    ivec2 size = imageSize(outImage);
    vec2 fragCoord = vec2(tid % size.x, tid / size.x);
    vec2 jitter = hash23(vec3(frames, seed+fragCoord));
    return fract((vec2(sx, sy)+jitter) / float(sqrtN));// hash23(vec3(fragCoord, seed));
}

vec2 SampleXi(in float seed, in uint tid){
    return SampleXi_Stratify(seed, tid);
    // ivec2 size = imageSize(outImage);
    // vec2 size_f = vec2(size);
    // return hash23(vec3(vec2(float(tid / size.x) / size_f.y, float(tid % size.x) / size_f.x)*500., float(uniforms.frameId)*10.) + seed);
}


// Reference: https://pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#ConcentricSampleDisk
vec2 ConcentricSampleDisk(vec2 Xi){
    vec2 uOffset = 2. * Xi - vec2(1., 1.);
    if(uOffset.x==0. && uOffset.y==0.){
        return vec2(0.);
    }
    float theta, r;
    if(abs(uOffset.x) > abs(uOffset.y)){
        r = uOffset.x;
        theta = PI / 4. * (uOffset.y / uOffset.x);
    }else{
        r = uOffset.y;
        theta = PI / 2. - PI / 4. * (uOffset.x / uOffset.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

// Reference: https://pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations
vec3 CosineSampleHemisphere(in vec2 Xi, out float pdf){
    pdf = 0.;
    vec2 d = ConcentricSampleDisk(Xi);
    float z = sqrt(max(0., 1. - d.x * d.x - d.y * d.y));
    pdf = z / PI;
    return vec3(d.x, d.y, z);
}

float CosineSampleHemispherePDF(in vec3 wi_local){
    return wi_local.z / PI;
}

void Sample_Square(in vec2 Xi, in ModelUniform ubo, out vec3 worldPoint, out vec3 worldNormal, out float pdf)
{
    vec3 localPoint = vec3(Xi - vec2(0.5, 0.5), 0.);
    worldPoint = vec3(ubo.model * vec4(localPoint, 1.));
    worldNormal = vec3(ubo.modelInvTranspose * vec4(vec3(0., 0., 1.), 0.));
    float area = length(cross(vec3(ubo.model * vec4(0.5, 0., 0., 1.)) , vec3(ubo.model * vec4(0., 0.5, 0., 1.))));
    pdf = 1. / area;
}

float LightSquarePDF(in ModelUniform ubo){
    float area = length(cross(vec3(ubo.model * vec4(0.5, 0., 0., 1.)) , vec3(ubo.model * vec4(0., 0.5, 0., 1.))));
    return 1. / area;
}

void SampleDirectLight(in vec2 Xi, in ModelUniform ubo_light, in vec3 rayOrigin, 
    out bool isBlocked, out vec3 rayDirection, out vec3 surfaceNormal, out float pdf) {
    isBlocked = false;
    vec3 lightWorldPoint = vec3(0., 0., 0.);
    surfaceNormal = vec3(0., 0., 0.);
    Sample_Square(Xi, ubo_light, lightWorldPoint, surfaceNormal, pdf);
    rayDirection = normalize(lightWorldPoint - rayOrigin);
    SurfaceIntersect Inters;
    
    Inters.t = 1e30;
    Inters.pointIntersectWorld = vec3(0., 0., 0.);
    Inters.baseColor = vec3(0., 0., 0.);
    Inters.emissive = vec3(0., 0., 0.);
    Inters.worldNormal = vec3(0., 0., 0.);
    Inters.texcoord = vec2(0., 0.);

    RayIntersect(rayOrigin, rayDirection, Inters);
    
    if (length(Inters.emissive) < 0.001 || abs(length(lightWorldPoint - rayOrigin) - Inters.t) > 1e-4){
        isBlocked = true;
    }
    pdf *= pow(length(lightWorldPoint - rayOrigin), 2.) / clamp(dot(-rayDirection, surfaceNormal), 0., 1.);
    // if (clamp(dot(-rayDirection, surfaceNormal), 0., 1.) < 0.0001){
    //     pdf = 0.;
    // }
}

void TestSampling(){
    // TEST SAMPLING
    
    ivec2 size = imageSize(outImage);
    uint tid = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * size.x;
    if(tid>100 || gl_GlobalInvocationID.y > 0){
        return;
    }
    vec2 Xi = SampleXi(514., tid);
    vec2 d = ConcentricSampleDisk(Xi);
    d = d*0.5+0.5;
    vec2 size_f = vec2(512,512);
    vec2 pixel_f = d * size_f;
    ivec2 pixel = ivec2(pixel_f);
    //vec4 inColor = imageLoad(outImage, pixel);
    vec4 outColor = vec4(0., 0., 0., 1.);
    outColor = vec4(1., 1., 1., 1.);
    // outColor = mix(inColor, outColor, 1.0 / float(uniforms.frameId));
    // outColor.w = 1.0;
    imageStore(outImage, pixel, outColor);
    return;
}

void Sample_LightSourceByIndex(vec2 Xi, int lightObjIndex, vec3 rayOriginW, out vec3 rayDirW, out vec3 lightNormalW, out SurfaceIntersect Result, out float pdf, out bool isBlocked){
    Result = NewSurfaceIntersect();
    pdf = 0.;
    isBlocked = false;

    vec3 lightPointW = vec3(0., 0., 0.);
    lightNormalW = vec3(0., 0., 0.);
    Sample_Square(Xi, primitives.data[lightObjIndex], lightPointW, lightNormalW, pdf);
    rayDirW = normalize(lightPointW - rayOriginW);
    RayIntersect(rayOriginW, rayDirW, Result);
    
    if (length(Result.emissive) < 0.001 || abs(length(lightPointW - rayOriginW) - Result.t) > 1e-4){
        isBlocked = true;
    }
    pdf *= pow(length(lightPointW - rayOriginW), 2.) / clamp(dot(-rayDirW, lightNormalW), 0., 1.);
}

void Sample_BSDF_Lambertian(vec2 Xi, vec3 rayOriginWorld, vec3 surfaceNormal, out vec3 nextRayDirW, out SurfaceIntersect Result, out float pdf_bsdf){
    vec3 w_i_local;
    w_i_local = CosineSampleHemisphere(Xi, pdf_bsdf);
    vec3 tangent_world = vec3(0., 0., 1.);
    if(length(cross(tangent_world, surfaceNormal))<0.0001){
        tangent_world = vec3(0., 1., 0.);
    }
    vec3 bitangent_world = normalize(cross(surfaceNormal, tangent_world));
    tangent_world = normalize(cross(bitangent_world, surfaceNormal));
    nextRayDirW = w_i_local.x * tangent_world + w_i_local.y * bitangent_world + w_i_local.z * surfaceNormal;

    Result = NewSurfaceIntersect();
    RayIntersect(rayOriginWorld, nextRayDirW, Result);
}

float PDF_BSDF_Lambertian(vec3 surfaceNormal, vec3 wi){
    return dot(surfaceNormal, wi);
}

float PDF_BSDF_Specular(vec3 N, vec3 wo, vec3 wi, float roughness){
    vec3 H = normalize(wo + wi);
    float NdotH = max(dot(N, H), 0.0);
    float a = roughness * roughness;
    float a2 = a * a;
    float denomD = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    float D = a2 / (PI * denomD * denomD);
    return D * dot(N, H) / (4. * dot(wo, H));
}

void Sample_BSDF_Specular(vec2 Xi, vec3 rayOriginWorld, vec3 N, vec3 wo, out vec3 nextRayDirW, out SurfaceIntersect Result, out float pdf_bsdf, float roughness){
    // 1. 在本地坐标系采样 H 向量
    float a2 = pow(roughness, 4.);
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // 2. 将 H 从切线空间转到世界空间 (TBN 变换)
    // from tangent-space H vector to world-space sample vector
    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 whW = tangent * H.x + bitangent * H.y + N * H.z;

    // 3. 通过反射得到入射光方向 L (即 wi)
    nextRayDirW = reflect(-wo, whW);

    Result = NewSurfaceIntersect();
    RayIntersect(rayOriginWorld, nextRayDirW, Result);
    pdf_bsdf = PDF_BSDF_Specular(N, wo, nextRayDirW, roughness);
}


float G_Schlick(float cos_w, float r){
    float k = (r + 1.) * (r + 1.) / 8.;
    return cos_w / (cos_w * (1.-k) + k);
}

vec3 BSDF_CookTorrance(in out SurfaceIntersect inters, in vec3 wo, in vec3 wi, float roughness, float metallic) {
    vec3 N = inters.worldNormal;
    vec3 V = wo;
    vec3 L = wi;
    vec3 H = normalize(V + L);

    // 预计算点积，确保不为负
    float NdotV = max(dot(N, V), 0.0001); // 避免除以0
    float NdotL = max(dot(N, L), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // 1. D 项 (GGX/Trowbridge-Reitz)
    float a = roughness * roughness;
    float a2 = a * a;
    float denomD = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    float D = a2 / (PI * denomD * denomD);

    // 2. F 项 (Schlick Approximation)
    // 绝缘体 F0 默认为 0.04，金属则由 baseColor 决定
    vec3 F0 = mix(vec3(0.04), inters.baseColor, metallic);
    vec3 F = F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);

    // 3. G 项 (Smith-Schlick GGX)
    float G = G_Schlick(NdotV, roughness) * G_Schlick(NdotL, roughness);

    // --- 组合 ---
    // 镜面反射部分
    vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL);
    
    // 漫反射部分 (基于能量守恒: 1 - F)
    // 只有非金属才有漫反射
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * inters.baseColor / PI;

    return diffuse + specular;
}
