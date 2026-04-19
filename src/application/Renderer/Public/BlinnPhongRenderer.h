#pragma once

#ifdef BLINNPHONGRENDERER_IMPLEMENT
#define BLINNPHONGRENDERER_API __declspec(dllexport)
#else
#ifdef BLINNPHONGRENDERER_INCLUDE
#define BLINNPHONGRENDERER_API __declspec(dllimport)
#else
#define BLINNPHONGRENDERER_API
#endif
#endif

#include <string>
#include <vector>
#include <memory>

#define RENDERER_INCLUDE
#include "Renderer.h"

#define COREGEOMETRY_INCLUDE
#include "CoreGeometry.h"

#define BP_VERTSHADER "./glsl/BlinnPhong.vert"
#define BP_FRAGSHADER "./glsl/BlinnPhong.frag"

using BlinnPhongVertex = TVertex<float3, float3, float2, float3, float3>;


class BlinnPhongPipeline : public IPipeline {
public:
    BlinnPhongPipeline() = default;
    virtual void SetAllShaderBindings(IRHIContext* Context) override;
};

class BLINNPHONGRENDERER_API BlinnPhongRenderer : public IRenderer {
public:
    std::unique_ptr<IRHIContext> uptr_Context;
    IRHIContext* Context;
    
    std::unique_ptr<IRHISwapchain> Swapchain;
    std::unique_ptr<IRHIRenderPass> RenderPass;
    std::unique_ptr<IRHIFrameBuffer> FrameBuffer;
    std::unique_ptr<IRHIImGUI> ImGUI;

    // Mesh resources
    std::unique_ptr<IRHIBuffer> VertexBuffer;
    std::unique_ptr<IRHIBuffer> IndexBuffer;
    uint32_t IndexCount = 0;

    // Texture resources
    std::unique_ptr<IRHIImageResource> Texture;

    // Uniform buffers
    std::unique_ptr<IRHIBuffer> UBO;
    std::unique_ptr<IRHIBuffer> ModelUBO;

    BlinnPhongPipeline Pipeline;
    RHIFormat SwapchainFormat;
    ImageExtent3D FrameSize;

    struct UniformBufferObject {
        float4x4 view;
        float4x4 proj;
        float4 viewPosition;
    } uboData;

    struct ModelUniform {
        float4x4 model;
        float4x4 modelInv;
        alignas(16) float3 color;
    } modelData;

    void (*pFuncImDraw)(ImGuiSharedGlobals*);

    BlinnPhongRenderer() = default;

     // --- IRenderer interface ---
    virtual void CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation = true) override;
    virtual void Render(float4 ViewPos, RenderControl* control) override;
    virtual void CaptureFrame(const std::string& Path) override;

    // Helpers
    void LoadMeshAndTexture(const std::string& MeshPath, const std::string& TexturePath);
    void UpdateUniforms(float4 ViewPos, RenderControl* control);
};
