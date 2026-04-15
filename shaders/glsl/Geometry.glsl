
void RayIntersectBox(in ModelUniform ubo, in vec3 rayOrigin, in vec3 rayDir, in out SurfaceIntersect IntersectInfo){
    mat3 modelInvTranspose = mat3(
        ubo.modelInvTranspose[0].xyz, 
        ubo.modelInvTranspose[1].xyz, 
        ubo.modelInvTranspose[2].xyz
    );
    vec3 invertRayOrigin = vec3(ubo.modelInv * vec4(rayOrigin, 1.));
    vec3 invertRayDir = vec3(ubo.modelInv * vec4(rayDir, 0.));
    float tzplus  = ( 1. - invertRayOrigin.z) / invertRayDir.z;
    float tzminus = (-1. - invertRayOrigin.z) / invertRayDir.z;
    float typlus  = ( 1. - invertRayOrigin.y) / invertRayDir.y;
    float tyminus = (-1. - invertRayOrigin.y) / invertRayDir.y;
    float txplus  = ( 1. - invertRayOrigin.x) / invertRayDir.x;
    float txminus = (-1. - invertRayOrigin.x) / invertRayDir.x;
    bool isIntersected = false;
    if(tzplus> 0. && tzplus < IntersectInfo.t){
        vec3 Intersect = invertRayOrigin + tzplus * invertRayDir;
        if(abs(Intersect.x)<1. && abs(Intersect.y)<1.){
            isIntersected = true;
            IntersectInfo.t = tzplus;
            IntersectInfo.baseColor = ubo.color;
            IntersectInfo.worldNormal = vec3(modelInvTranspose * vec3(0., 0., 1.));
            IntersectInfo.emissive = ubo.emission;
            IntersectInfo.pointIntersectWorld = vec3(ubo.model * vec4(Intersect,1.0));
        }
    }
    if(tzminus> 0. && tzminus < IntersectInfo.t){
        vec3 Intersect = invertRayOrigin + tzminus * invertRayDir;
        if(abs(Intersect.x)<1. && abs(Intersect.y)<1.){
            isIntersected = true;
            IntersectInfo.t = tzminus;
            IntersectInfo.baseColor = ubo.color;
            IntersectInfo.worldNormal = vec3(modelInvTranspose * vec3(0., 0., -1.));
            IntersectInfo.emissive = ubo.emission;
            IntersectInfo.pointIntersectWorld = vec3(ubo.model * vec4(Intersect,1.0));
        }
    }
    if(typlus> 0. && typlus < IntersectInfo.t){
        vec3 Intersect = invertRayOrigin + typlus * invertRayDir;
        if(abs(Intersect.x)<1. && abs(Intersect.z)<1.){
            isIntersected = true;
            IntersectInfo.t = typlus;
            IntersectInfo.baseColor = ubo.color;
            IntersectInfo.worldNormal = vec3(modelInvTranspose * vec3(0., 1., 0.));
            IntersectInfo.emissive = ubo.emission;
            IntersectInfo.pointIntersectWorld = vec3(ubo.model * vec4(Intersect,1.0));
        }
    }
    if(tyminus> 0. && tyminus < IntersectInfo.t){
        vec3 Intersect = invertRayOrigin + tyminus * invertRayDir;
        if(abs(Intersect.x)<1. && abs(Intersect.z)<1.){
            isIntersected = true;
            IntersectInfo.t = tyminus;
            IntersectInfo.baseColor = ubo.color;
            IntersectInfo.worldNormal = vec3(modelInvTranspose * vec3(0., -1., 0.));
            IntersectInfo.emissive = ubo.emission;
            IntersectInfo.pointIntersectWorld = vec3(ubo.model * vec4(Intersect,1.0));
        }
    }
    if(txplus> 0. && txplus < IntersectInfo.t){
        vec3 Intersect = invertRayOrigin + txplus * invertRayDir;
        if(abs(Intersect.z)<1. && abs(Intersect.y)<1.){
            isIntersected = true;
            IntersectInfo.t = txplus;
            IntersectInfo.baseColor = ubo.color;
            IntersectInfo.worldNormal = vec3(modelInvTranspose * vec3(1., 0., 0.));
            IntersectInfo.emissive = ubo.emission;
            IntersectInfo.pointIntersectWorld = vec3(ubo.model * vec4(Intersect,1.0));
        }
    }
    if(txminus> 0. && txminus < IntersectInfo.t){
        vec3 Intersect = invertRayOrigin + txminus * invertRayDir;
        if(abs(Intersect.z)<1. && abs(Intersect.y)<1.){
            isIntersected = true;
            IntersectInfo.t = txminus;
            IntersectInfo.baseColor = ubo.color;
            IntersectInfo.worldNormal = vec3(modelInvTranspose * vec3(-1., 0., 0.));
            IntersectInfo.emissive = ubo.emission;
            IntersectInfo.pointIntersectWorld = vec3(ubo.model * vec4(Intersect,1.0));
        }
    }
    if (isIntersected){
        IntersectInfo.worldNormal = normalize(IntersectInfo.worldNormal);
    }
}

bool RayIntersectBVH(in vec3 boxMin, in vec3 boxMax, in vec3 rayOrigin, in vec3 rayDir, in out SurfaceIntersect IntersectInfo){
    float tzplus  = ( boxMax.z - rayOrigin.z) / rayDir.z;
    float tzminus = ( boxMin.z - rayOrigin.z) / rayDir.z;
    float typlus  = ( boxMax.y - rayOrigin.y) / rayDir.y;
    float tyminus = ( boxMin.y - rayOrigin.y) / rayDir.y;
    float txplus  = ( boxMax.x - rayOrigin.x) / rayDir.x;
    float txminus = ( boxMin.x - rayOrigin.x) / rayDir.x;
    bool isIntersected = false;
    if(tzplus> 0. && tzplus < IntersectInfo.t){
        vec3 Intersect = rayOrigin + tzplus * rayDir;
        if(Intersect.x < boxMax.x && Intersect.x > boxMin.x && Intersect.y < boxMax.y && Intersect.y > boxMin.y){
            isIntersected = true;
            IntersectInfo.t = tzplus;
            IntersectInfo.worldNormal = vec3(0., 0., 1.);
            IntersectInfo.pointIntersectWorld = Intersect;
        }
    }
    if(tzminus> 0. && tzminus < IntersectInfo.t){
        vec3 Intersect = rayOrigin + tzminus * rayDir;
        if(Intersect.x < boxMax.x && Intersect.x > boxMin.x && Intersect.y < boxMax.y && Intersect.y > boxMin.y){
            isIntersected = true;
            IntersectInfo.t = tzminus;
            IntersectInfo.worldNormal = vec3(0., 0., -1.);
            IntersectInfo.pointIntersectWorld = Intersect;
        }
    }
    if(typlus> 0. && typlus < IntersectInfo.t){
        vec3 Intersect = rayOrigin + typlus * rayDir;
        if(Intersect.x < boxMax.x && Intersect.x > boxMin.x && Intersect.z < boxMax.z && Intersect.z > boxMin.z){
            isIntersected = true;
            IntersectInfo.t = typlus;
            IntersectInfo.worldNormal = vec3(0., 1., 0.);
            IntersectInfo.pointIntersectWorld = Intersect;
        }
    }
    if(tyminus> 0. && tyminus < IntersectInfo.t){
        vec3 Intersect = rayOrigin + tyminus * rayDir;
        if(Intersect.x < boxMax.x && Intersect.x > boxMin.x && Intersect.z < boxMax.z && Intersect.z > boxMin.z){
            isIntersected = true;
            IntersectInfo.t = tyminus;
            IntersectInfo.worldNormal =vec3(0., -1., 0.);
            IntersectInfo.pointIntersectWorld = Intersect;
        }
    }
    if(txplus> 0. && txplus < IntersectInfo.t){
        vec3 Intersect = rayOrigin + txplus * rayDir;
        if(Intersect.y < boxMax.y && Intersect.y > boxMin.y && Intersect.z < boxMax.z && Intersect.z > boxMin.z){
            isIntersected = true;
            IntersectInfo.t = txplus;
            IntersectInfo.worldNormal = vec3(1., 0., 0.);
            IntersectInfo.pointIntersectWorld = Intersect;
        }
    }
    if(txminus> 0. && txminus < IntersectInfo.t){
        vec3 Intersect = rayOrigin + txminus * rayDir;
        if(Intersect.y < boxMax.y && Intersect.y > boxMin.y && Intersect.z < boxMax.z && Intersect.z > boxMin.z){
            isIntersected = true;
            IntersectInfo.t = txminus;
            IntersectInfo.worldNormal = vec3(-1., 0., 0.);
            IntersectInfo.pointIntersectWorld = Intersect;
        }
    }
    if (isIntersected){
        IntersectInfo.worldNormal = normalize(IntersectInfo.worldNormal);
    }
    return isIntersected;
}

bool RayIntersectTriangle(vec3 Pa, vec3 Pb, vec3 Pc, vec3 rayOrigin, vec3 rayDir, in out SurfaceIntersect IntersectInfo) {
    vec3 edge1 = Pb - Pa;
    vec3 edge2 = Pc - Pa;
    vec3 pvec = cross(rayDir, edge2);
    float det = dot(edge1, pvec);

    if (abs(det) < 1e-6) return false;
    
    float invDet = 1.0 / det;

    vec3 tvec = rayOrigin - Pa;
    float u = dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    vec3 qvec = cross(tvec, edge1);
    float v = dot(rayDir, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    float t = dot(edge2, qvec) * invDet;

    if (t > 0.0 && t < IntersectInfo.t) {
        IntersectInfo.t = t;
        IntersectInfo.worldNormal = normalize(cross(edge1, edge2));
        IntersectInfo.pointIntersectWorld = rayOrigin + t * rayDir;
        return true;
    }

    return false;
}

void RayIntersect(in vec3 rayOrigin, in vec3 rayDir, in out SurfaceIntersect IntersectInfo){
    IntersectInfo.t = 9999999.;
    for(int i = 0;i < 8;i ++){
        ModelUniform ubo = primitives.data[i];
        RayIntersectBox(ubo, rayOrigin, rayDir, IntersectInfo);
    }

    // for (int i = 0; i < uniforms.vertexCount; i+=3){
    //     MeshVertices v1 = b_MeshVertices.Vertices[i];
    //     MeshVertices v2 = b_MeshVertices.Vertices[i + 1];
    //     MeshVertices v3 = b_MeshVertices.Vertices[i + 2];
    //     vec3 translate = vec3(-0.25,0.25,0.25);
    //     if (RayIntersectTriangle(v1.Position, v2.Position, v3.Position, rayOrigin, rayDir, IntersectInfo)){
    //         IntersectInfo.baseColor = vec3(0., 1., 0.);
    //     }
    // }
    BVH bvh = b_BVH.bvhs[0];
    int child1Index = -1;
    int child2Index = -1;
    BVH child1bvh;
    BVH child1bvh;
    if (RayIntersectBVH(bvh.BoxMin, bvh.BoxMax, rayOrigin, rayDir, IntersectInfo)){
        
    }
}
