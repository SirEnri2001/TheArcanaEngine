
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

bool IntersectAABB(vec3 origin, vec3 invDir, vec3 boxMin, vec3 boxMax, out float tMin) {
    vec3 t1 = (boxMin - origin) * invDir;
    vec3 t2 = (boxMax - origin) * invDir;
    vec3 vMin = min(t1, t2);
    vec3 vMax = max(t1, t2);
    tMin = max(max(vMin.x, vMin.y), vMin.z);
    float tMax = min(min(vMax.x, vMax.y), vMax.z);
    return tMax >= max(tMin, 0.0);
}

void RayIntersect(in vec3 rayOrigin, in vec3 rayDir, in out SurfaceIntersect GlobalIntersectInfo){
    
    // Intersect analytical primitives (boxes)
    for(int i = 0; i < 6; i++){
        ModelUniform ubo = primitives.data[i];
        RayIntersectBox(ubo, rayOrigin, rayDir, GlobalIntersectInfo);
    }
    SurfaceIntersect IntersectInfo;
    IntersectInfo.t = 1e30;
    ModelUniform ubo = primitives.data[6];
    vec3 GlobalRayOrigin = rayOrigin;
    rayOrigin = vec3(ubo.modelInv * vec4(rayOrigin, 1.));
    rayDir = normalize(vec3(ubo.modelInv * vec4(rayDir, 0.)));

    // BVH Traversal
    vec3 invDir = 1.0 / rayDir;
    int stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // Root node index
    bool intersectMesh = false;
    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        BVH node = b_BVH.bvhs[nodeIdx];

        float tBox;
        if (!IntersectAABB(rayOrigin, invDir, node.BoxMin, node.BoxMax, tBox) || tBox >= IntersectInfo.t) {
            continue;
        }
        
        if (node.ChildIndex1 == -1 && node.ChildIndex2 == -1) {
            // Leaf node: intersect triangles
            for (int i = node.TriangleIndexStart; i < node.TriangleIndexEnd; i++) {
                vec3 v0 = b_MeshVertices.Vertices[3 * i + 0].Position;
                vec3 v1 = b_MeshVertices.Vertices[3 * i + 1].Position;
                vec3 v2 = b_MeshVertices.Vertices[3 * i + 2].Position;
                if (RayIntersectTriangle(v0, v1, v2, rayOrigin, rayDir, IntersectInfo)) {
                    IntersectInfo.baseColor = vec3(0.1, 0.1, 0.1); // Default mesh color
                    IntersectInfo.emissive = vec3(0., 0., 0.);
                    vec3 P = IntersectInfo.pointIntersectWorld;
                    float S = length(cross(v2 - v0, v1 - v0));
                    float Sa = length(cross(v2 - P, v1 - P));
                    float Sb = length(cross(v0 - P, v2 - P));
                    IntersectInfo.worldNormal = 
                        (b_MeshVertices.Vertices[3 * i + 0].Normal * Sa +  
                        b_MeshVertices.Vertices[3 * i + 1].Normal * Sb + 
                        b_MeshVertices.Vertices[3 * i + 2].Normal * (S - Sa - Sb)) / S;
                    IntersectInfo.texcoord = 
                        (b_MeshVertices.Vertices[3 * i + 0].TexCoord * Sa +  
                        b_MeshVertices.Vertices[3 * i + 1].TexCoord * Sb + 
                        b_MeshVertices.Vertices[3 * i + 2].TexCoord * (S - Sa - Sb)) / S; 
                    intersectMesh = true;
                    // Interpolate other attributes if needed
                }
            }
        } else {
            // Internal node: push children
            // Optimization: push further child first to process closer child earlier
            if (node.ChildIndex1 != -1) stack[stackPtr++] = node.ChildIndex1;
            if (node.ChildIndex2 != -1) stack[stackPtr++] = node.ChildIndex2;
        }

        // Safety break to prevent infinite loops (should not happen with correct BVH)
        if (stackPtr >= 64) break;
    }
    if (intersectMesh){
        vec3 worldTarget = vec3(ubo.model * vec4(rayOrigin + IntersectInfo.t * rayDir, 1.));
        float world_t = length(GlobalRayOrigin - worldTarget);
        if (world_t < GlobalIntersectInfo.t){
            GlobalIntersectInfo.t = world_t;
            GlobalIntersectInfo.pointIntersectWorld = worldTarget;
            GlobalIntersectInfo.baseColor = texture(meshTex, IntersectInfo.texcoord).xyz;
            GlobalIntersectInfo.worldNormal = normalize(vec3(ubo.modelInvTranspose * vec4(IntersectInfo.worldNormal, 0.)));
            GlobalIntersectInfo.emissive = vec3(0., 0., 0.);
            GlobalIntersectInfo.texcoord = IntersectInfo.texcoord;
        }
    }
}
