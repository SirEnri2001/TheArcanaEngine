#pragma once

#include <string>
#include <vector>

#include "CoreMath.inl"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ---------------------------------------------------------------------------
// PathTrace scene presets.
//
// A PathTrace scene is just an array of ModelUniformObject entries uploaded to
// the PrimitiveBuffer (binding 2) plus one triangle mesh uploaded to the
// MeshVertices/MeshBVH buffers (bindings 3/4). Switching scenes therefore only
// needs those buffers rebuilt - no pipeline or shader recompilation.
//
// IMPORTANT: the shader hardcodes lightObjIndex = 0 for next-event estimation
// (see Sample_LightSourceByIndex / LightSquarePDF in PathTracerImpl.glsl), so
// every preset MUST put its emissive plane at index 0.
// ---------------------------------------------------------------------------

enum class EPathTraceScene : int {
    CornellBox = 0,
    Sponza = 1,
    Count = 2
};

inline const char* const kPathTraceSceneNames[] = { "Cornell Box", "Sponza" };
inline const char* const kPathTraceSceneComboItems = "Cornell Box\0Sponza\0";

// Must match `ModelUniform` in shaders/glsl/Test.comp and PTIteration.comp
// (std140): int, mat4, mat4, mat4, vec3, vec3 -> 240 bytes.
struct ModelUniformObject {
    int modelType = 0;                // 0: BVH triangle mesh, 1: box, 2: plane
    alignas(16) float4x4 model;
    alignas(16) float4x4 modelInv;
    alignas(16) float4x4 modelInvTranspose;
    alignas(16) float3 color;
    alignas(16) float3 emission;
};

inline void AddSceneUniform(int modelType,
                            std::vector<ModelUniformObject>& Objects,
                            const float4x4& ModelMat,
                            const float3& Color,
                            const float3& Emission)
{
    ModelUniformObject Object{};
    Object.modelType = modelType;
    Object.model = ModelMat;
    Object.modelInv = glm::inverse(ModelMat);
    Object.modelInvTranspose = glm::transpose(Object.modelInv);
    Object.color = Color;
    Object.emission = Emission;
    Objects.push_back(Object);
}

struct PathTraceScenePreset {
    std::vector<ModelUniformObject> Objects;

    // Navigation setup applied when the scene becomes active.
    float4x4 Camera = float4x4(1.f);
    glm::quat CameraOrientation = glm::quat(1.f, 0.f, 0.f, 0.f);
    float MovingSpeed = 0.5f;      // world units per second

    // Mesh source. Both scenes use the same BVH layout, so only the data
    // changes; bGLTFMesh selects LoadGLTFMesh() vs LoadMesh().
    std::string MeshPath;
    bool bGLTFMesh = false;
    bool bUseVertexColors = false; // Sponza has 25 materials but only one mesh sampler

    float3 BaseLightEmission = float3(1.f);
    int DispatchDepth = 4;          // path bounces; Sponza uses fewer to stay interactive
    std::string Description;
};

// The original analytic Cornell box: one ceiling light, five walls, a floor
// and the spot mesh as the "cow". Kept bit-for-bit identical to the scene that
// used to be hardcoded in Engine::Initialize.
inline PathTraceScenePreset BuildCornellBoxScene()
{
    PathTraceScenePreset P;
    P.Description = "Analytic Cornell box + spot mesh (8 primitives)";
    P.MeshPath = "./models/spot/spot_triangulated.obj";
    P.MovingSpeed = 0.5f;
    P.BaseLightEmission = float3(1.f, 1.f, 1.f);

    // Rz(90 deg) -> +X forward becomes world +Y, up stays +Z.
    P.Camera = glm::translate(float4x4(1.f), float3(-0.27f, -0.8f, 0.27f)) *
               glm::rotate(float4x4(1.f), glm::radians(90.f), float3(0.f, 0.f, 1.f));
    P.CameraOrientation = glm::normalize(glm::quat_cast(P.Camera));

    auto& O = P.Objects;
    AddSceneUniform(2, O,
        glm::translate(float4x4(1.f), float3(-0.27f, 0.27f, 0.535f)) *
        glm::rotate(float4x4(1.f), glm::radians(180.f), float3(0.f, 1.f, 0.f)) *
        glm::scale(float4x4(1.f), float3(0.065f, 0.05f, 1.f) * 2.f),
        float3(1.f), float3(1.f, 1.f, 1.f));

    AddSceneUniform(1, O,
        glm::translate(float4x4(1.f), float3(-0.27f, 0.27f, 0.f)) *
        glm::scale(float4x4(1.f), float3(0.27f, 0.27f, 0.001f)),
        float3(0.8f), float3(0.f));                                    // floor
    AddSceneUniform(1, O,
        glm::translate(float4x4(1.f), float3(-0.27f, 0.27f, 0.54f)) *
        glm::scale(float4x4(1.f), float3(0.27f, 0.27f, 0.001f)),
        float3(0.8f), float3(0.f));                                    // ceiling
    AddSceneUniform(1, O,
        glm::translate(float4x4(1.f), float3(-0.54f, 0.27f, 0.27f)) *
        glm::scale(float4x4(1.f), float3(0.001f, 0.27f, 0.27f)),
        float3(1.f, 0.f, 0.f), float3(0.f));                           // left (red)
    AddSceneUniform(1, O,
        glm::translate(float4x4(1.f), float3(0.f, 0.27f, 0.27f)) *
        glm::scale(float4x4(1.f), float3(0.001f, 0.27f, 0.27f)),
        float3(0.f, 1.f, 0.f), float3(0.f));                           // right (green)
    AddSceneUniform(1, O,
        glm::translate(float4x4(1.f), float3(-0.27f, 0.54f, 0.27f)) *
        glm::scale(float4x4(1.f), float3(0.27f, 0.001f, 0.27f)),
        float3(0.8f), float3(0.f));                                    // back wall

    AddSceneUniform(0, O,
        glm::translate(float4x4(1.f), float3(-0.25f, 0.25f, 0.17f)) *
        glm::rotate(float4x4(1.f), glm::radians(180.f + 45.f), float3(0.f, 0.f, 1.f)) *
        glm::rotate(float4x4(1.f), glm::radians(90.f), float3(1.f, 0.f, 0.f)) *
        glm::scale(float4x4(1.f), float3(0.15f)),
        float3(1.f, 1.f, 0.f), float3(0.f));                           // cow / spot mesh

    return P;
}

// Sponza (glTF, 103 primitives / ~262k triangles / 25 materials).
//
// The glTF is Y-up with a single node carrying scale 0.008; TMesh::LoadGLTF()
// deliberately ignores node transforms, so we bake that scale plus a Y-up ->
// Z-up rotation into the mesh instance's model matrix. This puts the building
// roughly in X[-15, 14], Y[-9, 9], Z[-6, 6.4].
inline PathTraceScenePreset BuildSponzaScene()
{
    PathTraceScenePreset P;
    P.Description = "Sponza atrium (glTF, 262k triangles at stride 1)";
    P.MeshPath = "./models/gltf/Sponza/glTF/Sponza.gltf";
    P.bGLTFMesh = true;
    P.bUseVertexColors = true;
    P.MovingSpeed = 4.f;
    P.BaseLightEmission = float3(1.f, 0.94f, 0.86f);
    P.DispatchDepth = 2;           // 1000x1000 rays x 262k triangles is heavy


    // Identity rotation: the shader's camera-local forward is +X and up is +Z,
    // so this looks straight down the long axis of the atrium.
    P.Camera = glm::translate(float4x4(1.f), float3(-11.f, 0.f, -3.5f));
    P.CameraOrientation = glm::quat(1.f, 0.f, 0.f, 0.f);

    // Index 0 is the area light used by NEE. Rotate 180 deg about Y so the
    // plane's +Z normal points down into the open roof.
    AddSceneUniform(2, P.Objects,
        glm::translate(float4x4(1.f), float3(0.f, 0.f, 5.5f)) *
        glm::rotate(float4x4(1.f), glm::radians(180.f), float3(0.f, 1.f, 0.f)) *
        glm::scale(float4x4(1.f), float3(6.f, 6.f, 1.f)),
        float3(1.f), P.BaseLightEmission);

    // The whole glTF is one BVH mesh instance.
    AddSceneUniform(0, P.Objects,
        glm::translate(float4x4(1.f), float3(0.f, 0.f, -5.f)) *
        glm::rotate(float4x4(1.f), glm::radians(90.f), float3(1.f, 0.f, 0.f)) *
        glm::scale(float4x4(1.f), float3(0.008f)),
        float3(1.f), float3(0.f));

    return P;
}

inline PathTraceScenePreset BuildPathTraceScenePreset(int SceneIndex)
{
    return SceneIndex == static_cast<int>(EPathTraceScene::Sponza)
        ? BuildSponzaScene()
        : BuildCornellBoxScene();
}
