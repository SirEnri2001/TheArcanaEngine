#pragma once
#include "BaseRenderer.h"

#include <string>

#define RENDERER_IMPLEMENT
#include "Renderer.h"

#define COREGEOMETRY_INCLUDE
#include "CoreGeometry.h"
#include "PathTraceScene.h"

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
        // 0 = sample meshTex (spot), 1 = use the per-vertex material colour.
        int32_t useVertexColor = 0;
        // 0 = path trace, 1 = flat triangle normal, 2 = normal + depth cue.
        // Both fields sit in the std140 block's trailing padding; the block
        // grew from 112 to 128 bytes and must match both .comp shaders.
        int32_t debugMode = 0;
        // std140 aligns an ivec4 to 16 B, so this starts at offset 128 and
        // the block grows to 144 B. Must match `ivec4 tileRect` in Test.comp
        // and PTIteration.comp. xy = tile origin, zw = clamped tile size.
        alignas(16) int4 tileRect = int4(0, 0, 0, 0);
    } cuo;

    // The GLSL std140 block must stay byte-for-byte identical, otherwise the
    // shaders read garbage from every field after camToWorld.
    static_assert(sizeof(CameraUniformObject) == 144,
                  "CameraUniformObject must match the 144-byte std140 block in the .comp shaders");

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
    // --- Scene selection (driven by the ImGui combo in DrawImGUI) ---
    // SceneIndex is the UI value; ActiveSceneIndex is what the GPU buffers
    // currently hold. BeginRender() applies the difference, which happens
    // after the previous frame's WaitDeviceIdle() so the buffers are safe to
    // destroy/recreate.
    int SceneIndex = 0;                 // EPathTraceScene
    int ActiveSceneIndex = -1;          // -1 = never applied
    bool bSceneReloadRequested = false; // set by the LOD slider
    // 4 (~53k triangles) stays responsive; tile accumulation also makes
    // stride 1 (full 262k) safe because each dispatch only covers one tile.
    int SponzaTriangleStride = 4;
    float LightIntensity = 1.f;
    uint32_t SceneTriangleCount = 0;
    std::string SceneStatus;

    // --- Tile accumulation -------------------------------------------------
    // One tile is dispatched per rendered frame. A full sweep of all tiles is
    // one path-traced sample per pixel, and cuo.frameId (the temporal
    // accumulation counter) only advances when a sweep finishes. Each frame
    // already ends with its own vkQueueSubmit + WaitDeviceIdle, so every GPU
    // submission stays far below the Windows TDR timeout instead of issuing
    // one monolithic full-screen dispatch.
    inline static constexpr int kTileSizes[4] = { 64, 128, 256, 512 };
    int TileSizeIndex = 1;   // default 128 px -> 8x8 = 64 tiles at 1000x1000
    int TileIndex = 0;       // next tile in the current sweep
    int TilesX = 1;
    int TilesY = 1;
    int TileCount = 1;       // TilesX * TilesY

    int GetTileSize() const { return kTileSizes[TileSizeIndex]; }
    int GetTileCount() const { return TileCount; }
    int GetCompletedSweeps() const { return cuo.frameId; }
    // Recomputes the tile grid and writes cuo.tileRect for this frame. Called
    // from BeginRender() BEFORE the uniform buffers are uploaded.
    void UpdateTileRect();
    // Restarts the sweep from tile 0 and clears temporal accumulation.
    void ResetAccumulation();

    // --- PathTraceRenderer specific methods ---
    void UpdateUniformBuffer(float4x4 camToWorld, float time, RenderControl* control);
    void ProcessInput();
    void LoadMesh(const std::string& MeshPath);
    void LoadGLTFMesh(const std::string& GltfPath, int TriangleStride);

    // Rebuilds primitives, mesh BVH, camera and accumulation state for a scene.
    void ApplyScene(int Index, RenderControl* Control);
    void SetPathTraceScene(int Index) { SceneIndex = Index; }

private:
    std::vector<ModelUniformObject> SceneObjects;
    float3 BaseLightEmission = float3(1.f);
    void UpdateLightEmission();
};
