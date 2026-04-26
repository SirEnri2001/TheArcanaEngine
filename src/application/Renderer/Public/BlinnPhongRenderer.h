#pragma once

#include <string>
#include <vector>
#include <memory>

#define RENDERER_IMPLEMENT
#include "Renderer.h"

#define BASERENDERER_INCLUDE
#include "BaseRenderer.h"

#define COREGEOMETRY_INCLUDE
#include "CoreGeometry.h"

#define BP_VERTSHADER "./glsl/BlinnPhong.vert"
#define BP_FRAGSHADER "./glsl/BlinnPhong.frag"

using BlinnPhongVertex = TVertex<float3, float3, float2, float3, float3>;

class RENDERER_API BlinnPhongRenderer : public BaseRenderer {
public:
    // Mesh resources
    std::unique_ptr<IRHIBuffer> VertexBuffer;
    std::unique_ptr<IRHIBuffer> IndexBuffer;
    uint32_t IndexCount = 0;

    // Texture resources
    std::unique_ptr<IRHIImageResource> Texture;

    // Uniform buffers
    std::unique_ptr<IRHIBuffer> UBO;
    std::unique_ptr<IRHIBuffer> ModelUBO;

    AutoPipeline Pipeline;

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

    BlinnPhongRenderer() = default;

    virtual void CreateResource() override;
    virtual void DrawImGUI() override;

protected:
    virtual bool BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) override;
    virtual void DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) override;
    virtual void EndRender(IRHICommandBuffer* CommandBuffer) override;

public:
    // Helpers
    void LoadMeshAndTexture(const std::string& MeshPath, const std::string& TexturePath);
    void UpdateUniforms(float4 ViewPos, RenderControl* control);
};
