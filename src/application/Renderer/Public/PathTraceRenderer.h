#pragma once

#ifdef PATHTRACERENDERER_IMPLEMENT
#define PATHTRACERENDERER_API __declspec(dllexport)
#else
#ifdef PATHTRACERENDERER_INCLUDE
#define PATHTRACERENDERER_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define PATHTRACERENDERER_API
#endif
#endif

#include <string>

#define RENDERER_INCLUDE
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
class PATHTRACERENDERER_API PathTraceRenderer : public IRenderer {
public:
    uint32_t IndexBufferSize;
    std::unique_ptr<IRHIContext> uptr_Context;
    IRHIContext* Context;
    std::unique_ptr<IRHIBuffer> RHIFullScreenQuadBuffer;
    std::unique_ptr<IRHIBuffer> RHIFullScreenQuadIndexBuffer;
    std::unique_ptr<IRHIImageResource> RHIScreenBuffer1;
    std::unique_ptr<IRHIImageResource> RHIScreenBuffer2;
    std::unique_ptr<IRHIImageResource> RHIScreenBufferDepth;
    std::unique_ptr<IRHIImageResource> RHIStoreImage;
    std::unique_ptr<IRHIImageResource> RHIMeshTexture;
    std::unique_ptr<IRHIRenderPass> PresentPass;
    std::unique_ptr<IRHIRenderPass> PTRenderPass;
    std::unique_ptr<IRHIBuffer> CameraUniform;
    std::unique_ptr<IRHIBuffer> SceneUniform;
    std::unique_ptr<IRHIBuffer> StorageBuffer;
    std::unique_ptr<IRHIBuffer> PrimitiveBuffer;
    std::unique_ptr<IRHIBuffer> MeshVerticesBuffer;
    std::unique_ptr<IRHIBuffer> MeshBVHBuffer;
    std::unique_ptr<IRHIBuffer> RayStateBuffer;
    std::unique_ptr<IRHISwapchain> Swapchain;
    std::unique_ptr<IRHIFrameBuffer> FBuffer1;
    std::unique_ptr<IRHIFrameBuffer> FBuffer2;
    std::unique_ptr<IRHIImGUI> ImGUI;

    AutoPipeline Pipeline;
    AutoPipeline PostPipeline;
    AutoPipeline TestCompPipeline;
    AutoPipeline IterationPipeline;
    RHIFormat SwapchainFormat;
    ImageExtent3D FrameSize;
    std::vector<BVHBox<PTVertex, uint32_t>> BVHBoxes;

    bool swap = false;

    void (*pFuncImDraw)(ImGuiSharedGlobals*);

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
    virtual void CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation = true) override;
    virtual void Render(float4 ViewPos, RenderControl* control) override;
    virtual void CaptureFrame(const std::string& Path) override;

    // --- PathTraceRenderer specific methods ---
    void UpdateUniformBuffer(float4x4 camToWorld, float time, RenderControl* control);
    void ProcessInput();
    void LoadMesh(const std::string& MeshPath);
};
