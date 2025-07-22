#version 450

layout(location = 0) out vec4 outColor;
struct ModelUniform {
    mat4 model;
    mat4 modelInv;
    vec3 color;
};

layout(binding = 0) uniform ModelUniforms {
    ModelUniform modelubo[8];
} modelus;

layout(binding = 1) uniform CameraUniform {
    vec3 eye;
    vec3 forward;
    vec3 up;
    ivec2 screenres;
} cameraubo;

void RayIntersect(in vec3 rayPos, in vec3 rayDir, out vec3 baseColor, out vec3 worldNormal){
    float t = 999999999.;
    baseColor = vec3(0.);
    worldNormal = vec3(0.);
    for(int i = 0;i < 8;i ++){
        ModelUniform ubo = modelus.modelubo[i];
        vec3 invertRayPos = vec3(ubo.modelInv * vec4(rayPos, 1.));
        vec3 invertRayDir = vec3(ubo.modelInv * vec4(rayDir, 0.));
        float tzplus  = ( 1. - invertRayPos.z) / invertRayDir.z;
        float tzminus = (-1. - invertRayPos.z) / invertRayDir.z;
        float typlus  = ( 1. - invertRayPos.y) / invertRayDir.y;
        float tyminus = (-1. - invertRayPos.y) / invertRayDir.y;
        float txplus  = ( 1. - invertRayPos.x) / invertRayDir.x;
        float txminus = (-1. - invertRayPos.x) / invertRayDir.x;
        if(tzplus> 0. && tzplus < t){
            vec3 Intersect = invertRayPos + tzplus * invertRayDir;
            if(abs(Intersect.x)<1. && abs(Intersect.y)<1.){
                t = tzplus;
                baseColor = ubo.color;
                worldNormal = vec3(transpose(ubo.modelInv) * vec4(0., 0., 1., 0.));
            }
        }
        if(tzminus> 0. && tzminus < t){
            vec3 Intersect = invertRayPos + tzminus * invertRayDir;
            if(abs(Intersect.x)<1. && abs(Intersect.y)<1.){
                t = tzminus;
                baseColor = ubo.color;
                worldNormal = vec3(transpose(ubo.modelInv) * vec4(0., 0., -1., 0.));
            }
        }
        if(typlus> 0. && typlus < t){
            vec3 Intersect = invertRayPos + typlus * invertRayDir;
            if(abs(Intersect.x)<1. && abs(Intersect.z)<1.){
                t = typlus;
                baseColor = ubo.color;
                worldNormal = vec3(transpose(ubo.modelInv) * vec4(0., 1., 0., 0.));
            }
        }
        if(tyminus> 0. && tyminus < t){
            vec3 Intersect = invertRayPos + tyminus * invertRayDir;
            if(abs(Intersect.x)<1. && abs(Intersect.z)<1.){
                t = tyminus;
                baseColor = ubo.color;
                worldNormal = vec3(transpose(ubo.modelInv) * vec4(0., -1., 0., 0.));
            }
        }
        if(txplus> 0. && txplus < t){
            vec3 Intersect = invertRayPos + txplus * invertRayDir;
            if(abs(Intersect.z)<1. && abs(Intersect.y)<1.){
                t = txplus;
                baseColor = ubo.color;
                worldNormal = vec3(transpose(ubo.modelInv) * vec4(1., 0., 0., 0.));
            }
        }
        if(txminus> 0. && txminus < t){
            vec3 Intersect = invertRayPos + txminus * invertRayDir;
            if(abs(Intersect.z)<1. && abs(Intersect.y)<1.){
                t = txminus;
                baseColor = ubo.color;
                worldNormal = vec3(transpose(ubo.modelInv) * vec4(1., 0., 0., 0.));
            }
        }
    }
}

void main() {
    // sensor size 32mm
    // focal length 45mm
    // fov = 2 * atan(sensor_size/2/focal_length)
    float halffov = atan(0.032/2./0.045);
    // assume our camera near plane width is 2 as in ndc.
    float cameraDistance = 1. / tan(halffov);
    vec3 ndc = vec3(gl_FragCoord.x / cameraubo.screenres.x * 2 - 1, 1 - gl_FragCoord.y / cameraubo.screenres.y * 2, 0.);
    vec3 rayUnnorm = vec3(ndc.x, ndc.y, cameraDistance);
    vec3 rayCamSpaceDir = normalize(rayUnnorm);
    mat3 camToWorld1;
    camToWorld1[0] = cross(cameraubo.forward, cameraubo.up);
    camToWorld1[1] = cameraubo.up;
    camToWorld1[2] = cameraubo.forward;
    vec3 rayWorldSpaceDir = (camToWorld1 * rayCamSpaceDir);
    vec3 baseColor;
    vec3 worldNormal;
    RayIntersect(cameraubo.eye, rayWorldSpaceDir, baseColor, worldNormal);
    outColor = vec4(worldNormal, 1.0);
}
