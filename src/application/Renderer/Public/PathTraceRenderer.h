#pragma once
#include "BaseRenderer.h"

#include <string>

#define RENDERER_IMPLEMENT
#include "Renderer.h"

#define COREGEOMETRY_INCLUDE
#include "CoreGeometry.h"

#define PT_VERTSHADER     "./glsl/PathTracer.vert"
#define PT_FRAGSHADER     "./glsl/PathTracer.frag"
#define PTPOST_VERTSHADER "./glsl/ScreenPost.vert"
#define PTPOST_FRAGSHADER "./glsl/ScreenPost.frag"
#define TEST_COMPSHADER   "./glsl/Test.comp"
#define PT_ITERATION_COMPSHADER "./glsl/PTIteration.comp"

struct RayObject {
    alignas(16) float4 rayOrigin;
    alignas(16) float4 rayDir;
    alignas(16) float4 throughput;
    alignas(16) float4 accumulatedColor;
    alignas(16) float4 hitPoint;
    alignas(16) float4 normal;
    alignas(16) float4 emissive;
    alignas(16) float4 baseColor;
    alignas(16) float4 texcoord_t_objId; // .xy = texcoord, .z = t, .w = (float)objId
    alignas(16) int4 iter_etc; // .x = iter, .y = active
};

using PTVertex = TVertex<float3, float3, float2, float3, float3>;
class RENDERER_API PathTraceRenderer : public BaseRenderer {
public:
    struct OwnedResource
    {
        std::unique_ptr<IRHIBuffer> RHIFullScreenQuadBuffer;
        std::unique_ptr<IRHIBuffer> RHIFullScreenQuadIndexBuffer;
        std::unique_ptr<IRHIImageResource> RHIScreenBuffer1;
        std::unique_ptr<IRHIImageResource> RHIScreenBuffer2;
        std::unique_ptr<IRHIImageResource> RHIScreenBufferDepth;
        std::unique_ptr<IRHIImageResource> RHIStoreImage;
        std::unique_ptr<IRHIImageResource> RHIMeshTexture;
        std::unique_ptr<IRHIRenderPass> PTRenderPass;
        std::unique_ptr<IRHIBuffer> CameraUniform;
        std::unique_ptr<IRHIBuffer> SceneUniform;
        std::unique_ptr<IRHIBuffer> StorageBuffer;
        std::unique_ptr<IRHIBuffer> PrimitiveBuffer;
        std::unique_ptr<IRHIBuffer> MeshVerticesBuffer;
        std::unique_ptr<IRHIBuffer> MeshBVHBuffer;
        std::unique_ptr<IRHIBuffer> RayStateBuffer;
        std::unique_ptr<IRHIFrameBuffer> FBuffer1;
        std::unique_ptr<IRHIFrameBuffer> FBuffer2;

        AutoPipeline Pipeline;
        AutoPipeline PostPipeline;
        AutoPipeline TestCompPipeline;
        AutoPipeline IterationPipeline;
    } PTResource;

    uint32_t IndexBufferSize;

    std::vector<BVHBox<PTVertex, uint32_t>> BVHBoxes;

    bool swap = false;

    struct CameraUniformObject {
        alignas(16) float4x4 camToWorld;
        alignas(8) glm::uvec2 screenres; // not sure 8 or 16 to use here, renderdoc says shader uses 8
        float time;
        int32_t frameId;
        int32_t vertexCount;
        int32_t modelUniformCount;
        int32_t totalIters;
        int32_t dispatchDepth;
        float roughness;
        float prob_lambert;
        int32_t enableNEE;
    } cuo;

    PathTraceRenderer() = default;

     // --- IRenderer interface implementation ---
    virtual void CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation = true, std::function<void(ImGuiSharedGlobals*)> EngineUIFunc = nullptr) override;
    virtual void CreateResource() override;
    virtual void CaptureFrame(const std::string& Path) override;
    virtual void DrawImGUI() override;

protected:
    virtual bool BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) override;
    virtual void ResizeScreen() override;
    virtual void DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) override;
    virtual void EndRender(IRHICommandBuffer* CommandBuffer) override;

public:
    // --- PathTraceRenderer specific methods ---
    void UpdateUniformBuffer(float4x4 camToWorld, float time, RenderControl* control);
    void ProcessInput();
    void LoadMesh(const std::string& MeshPath);
};
