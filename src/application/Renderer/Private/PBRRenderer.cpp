#define PBR_RENDERER_IMPLEMENT
#define RENDERER_INCLUDE

#include "PBRRenderer.h"
#include <CoreLog.inl>
#include <imgui.h>
#include <directx/d3d12.h>

#include "Renderer.h"

using PBR::TransformationData;
using PBR::MaterialPropertyData;
using PBR::PBRMeshRenderProxy;
using PBR::RendererUniformType;
using PBR::LightingData;

void PBRMeshRenderProxy::Initialize(RendererContext* Context, Mesh& InMesh, MaterialPropertyData* InMaterial)
{
    material = InMaterial;

    RHIVertexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::VERTEX);
    RHIIndexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Indices.size(), BufferType::INDEX);
    RHICommandBuffer CmdBuffer;
    CmdBuffer.Initialize(&Context->Context);
    RHIVertexBuffer.CopyToBuffer(&CmdBuffer, &Context->Context, InMesh.Vertices.data(),
        InMesh.Vertices.size() * sizeof(Mesh::VertexType));
    RHIIndexBuffer.CopyToBuffer(&CmdBuffer, &Context->Context, InMesh.Indices.data(), InMesh.Indices.size() * sizeof(uint32_t));

    IndexBufferSize = InMesh.Indices.size();
}

std::unordered_map<PBR::RendererUniformType, uint32_t> PBRRenderer::UniformBindings
{
    {RendererUniformType::Transformation, 0},
    {RendererUniformType::MaterialProperty, 1},
    {RendererUniformType::Lighting, 2}
};

uint32_t PBRRenderer::GetUniformBinding(PBR::RendererUniformType uniformType)
{
    if (UniformBindings.find(uniformType) != UniformBindings.end())
    {
        return UniformBindings[uniformType];
    }
    return -1;
}


void PBRRenderer::AddSceneObject(PBRMeshRenderProxy& MeshProxy)
{
    MaterialMap[MeshProxy.material].push_back(&MeshProxy);
}


void PBRRenderer::Initialize(RendererContext* rendererContext, std::vector<char>& vs, std::vector<char>& ps)
{
    RContext = rendererContext;
    RContext->WindowManager.InitializeRenderPassAsPresent(&PresentPass, &RContext->Context);

    PipelineFactory.SetShaders(vs, ps);

    /* Vertex Data*/
    PipelineFactory.AddBufferBinding(0, sizeof(Mesh::VertexType));
    PipelineFactory.AddBufferLayout(0, 0, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
    PipelineFactory.AddBufferLayout(0, 1, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
    PipelineFactory.AddBufferLayout(0, 2, R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
    PipelineFactory.AddBufferLayout(0, 3, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));

    /* Uniforms */
    PipelineFactory.SetUniformBinding(0); // MVP etc.
    PipelineFactory.SetUniformBinding(1); // PBR Material
    PipelineFactory.SetUniformBinding(2); // Lighting

    TransformationRelatedUniform.Initialize(&RContext->Context, sizeof(TransformationData));
    MaterialPropertyRelatedUniform.Initialize(&RContext->Context, sizeof(MaterialPropertyData));
    LightingRelatedUniform.Initialize(&RContext->Context, sizeof(LightingData));

    PipelineFactory.InitializePipelineObject(&PresentPipelineObject, &RContext->Context, &PresentPass);

    GraphicDispatcher.Initialize(&RContext->Context);
}


bool PBRRenderer::SetUniform(RendererUniformType uniformType, void* data)
{
    bool success = false;
    uint32_t bindingLocation;
    TransformationData* transformData;
    LightingData* lightingData;
    MaterialPropertyData* materialData;
    switch (uniformType)
    {
    case RendererUniformType::Transformation:
        transformData = static_cast<TransformationData*>(data);
        TransformationRelatedUniform.CopyToBuffer(&RContext->Context, transformData, sizeof(*transformData));
        PresentPipelineObject.SetUniform(&TransformationRelatedUniform,
            GetUniformBinding(RendererUniformType::Transformation));
        success = true;
        break;
    case RendererUniformType::Lighting:
        lightingData = static_cast<LightingData*>(data);
        LightingRelatedUniform.CopyToBuffer(&RContext->Context, lightingData, sizeof(*lightingData));
        PresentPipelineObject.SetUniform(&LightingRelatedUniform, GetUniformBinding(RendererUniformType::Lighting));
        success = true;
        break;
    case RendererUniformType::MaterialProperty:
        // TODO: Remove the following, material bound with PBRMeshProxy, shouldn't set manually
        materialData = static_cast<MaterialPropertyData*>(data);
        MaterialPropertyRelatedUniform.CopyToBuffer(&RContext->Context, materialData, sizeof(*materialData));
        PresentPipelineObject.SetUniform(&MaterialPropertyRelatedUniform,
            GetUniformBinding(RendererUniformType::MaterialProperty));
        break;
    default:
        break;
    }
    return success;
}

void PBRRenderer::BindMaterial(PBR::MaterialPropertyData* materialData)
{
    // TODO: change to descriptor binding instead of coherent mapped memory?
    /*MaterialPropertyRelatedUniform.CopyToBuffer(&RContext->Context, materialData, sizeof(*materialData));
    uint32_t bindingLocation = GetUniformBinding(RendererUniformType::MaterialProperty);
    PresentPipelineObject.SetUniform(&MaterialPropertyRelatedUniform, bindingLocation);*/
    SetUniform(RendererUniformType::MaterialProperty, materialData);
}

void PBRRenderer::UpdateFrame()
{
    /*
    auto& rhiContext = RContext->Context;
    auto& windowManager = RContext->WindowManager;

    GraphicDispatcher.WaitForGPUIdle(&rhiContext);
    GraphicDispatcher.BeginFrame(&rhiContext, &windowManager, &PresentPass);
    {
        //GraphicDispatcher.BeginRenderPass(&PresentPass, TODO);
        {
            for (const auto& [material, meshProxyList] : MaterialMap)
            {
                BindMaterial(material);
                // TODO: replace the following with draw instanced (batch?)
                for (PBRMeshRenderProxy* meshProxy : meshProxyList)
                {
                    GraphicDispatcher.BindIndexBuffer(&meshProxy->RHIIndexBuffer, 0);
                    GraphicDispatcher.BindVertexBuffer(&meshProxy->RHIVertexBuffer, 0, 0);
                    GraphicDispatcher.Dispatch(&PresentPipelineObject, meshProxy->IndexBufferSize, 0, 1);
                }
                // break;  // TODO: remove the break for multiple materials (currently bugged)
            }
        }
        GraphicDispatcher.EndRenderPass(&PresentPass);
    }
    GraphicDispatcher.EndFrameAndSubmit(&rhiContext, &windowManager);
	*/
}
