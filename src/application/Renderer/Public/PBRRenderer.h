#pragma once
#define RHI_INCLUDE
#define CORESCENE_INCLUDE

#if defined(PBR_RENDERER_IMPLEMENT)
    #define PBR_RENDERER_API _declspec(dllexport)
#elif defined(PBR_RENDERER_INCLUDE)
    #define PBR_RENDERER_API _declspec(dllimport)
#else
    #error Please Specify API Linkage PBR_RENDERER
    #define PBR_RENDERER_API
#endif


#include <string>
#include <vector>
#include <map>
#include "RHI.h"
#include "CoreMath.inl"
#include "CoreScene.h"

class RendererContext;

// TODO: Add a universal material type with textures
// TODO: Add material imgui support

namespace PBR
{
    /* cbuffers */
    struct TransformationData {
        float4x4	model;
        float4x4	view;
        float4x4    proj;
        float4      viewPosition;
    };

    struct MaterialPropertyData
    {
        float3		Albedo;
        float		Smoothness;
        float		Metalness;
    };

    struct LightingData
    {
        float3		LightDirection;
    };

    /* render objects */
    PBR_RENDERER_API struct PBRMeshRenderProxy
    {
        MaterialPropertyData* material;	// TODO: to a universal material type

        RHIBufferResource RHIVertexBuffer;
        RHIBufferResource RHIIndexBuffer;
        uint32_t IndexBufferSize;
        PBR_RENDERER_API void Initialize(RendererContext* Context, Mesh& InMesh, MaterialPropertyData* InMaterial);
    };

    enum class RendererUniformType
    {
        Transformation,
        MaterialProperty,
        Lighting
    };
}



PBR_RENDERER_API class PBRRenderer
{
private:
    void BindMaterial(PBR::MaterialPropertyData* materialData);
    static std::unordered_map<PBR::RendererUniformType, uint32_t> UniformBindings;
    static uint32_t GetUniformBinding(PBR::RendererUniformType uniformType);

public:
    RendererContext* RContext;

    std::map<PBR::MaterialPropertyData*, std::vector<PBR::PBRMeshRenderProxy*>> MaterialMap;

    RHIUniform TransformationRelatedUniform;
    RHIUniform MaterialPropertyRelatedUniform;
    RHIUniform LightingRelatedUniform;

    RHIPipelineFactory PipelineFactory;
    RHIPipelineObject PresentPipelineObject;
    RHIGraphicsDispatcher GraphicDispatcher;

    PBR_RENDERER_API void AddSceneObject(PBR::PBRMeshRenderProxy& MeshProxy);

    PBR_RENDERER_API void Initialize(RendererContext* rendererContext, std::vector<char>& vs, std::vector<char>& ps);

    PBR_RENDERER_API bool SetUniform(PBR::RendererUniformType uniformType, void* data);

    PBR_RENDERER_API void UpdateFrame();
};
