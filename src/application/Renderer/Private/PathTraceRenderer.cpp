#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>

#define PATHTRACERENDERER_IMPLEMENT
 #include "PathTraceRenderer.h"
#define SHADERCOMPILER_INCLUDE
#include "ShaderCompiler.h"

 #define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>

#include "BaseRenderer.h"
#include "stb_image_write.h"

#include "imgui.h"

void PathTraceRenderer::CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation, std::function<void(ImGuiSharedGlobals*)> EngineUIFunc) {
	BaseRenderer::CreateRenderer(Height, Width, Backend, bEnableValidation, EngineUIFunc);

}

void PathTraceRenderer::CreateResource()
{
    PTResource.RHIFullScreenQuadBuffer = Env.Context->CreateRHIBuffer();
    PTResource.RHIFullScreenQuadIndexBuffer = Env.Context->CreateRHIBuffer();
    PTResource.RHIScreenBuffer1 = Env.Context->CreateRHIImageResource();
    PTResource.RHIScreenBuffer2 = Env.Context->CreateRHIImageResource();
    PTResource.RHIScreenBufferDepth = Env.Context->CreateRHIImageResource();
    PTResource.RHIStoreImage = Env.Context->CreateRHIImageResource();
    PTResource.PTRenderPass = Env.Context->CreateRHIRenderPass();
    PTResource.CameraUniform = Env.Context->CreateRHIBuffer();
    PTResource.SceneUniform = Env.Context->CreateRHIBuffer();
    PTResource.FBuffer1 = Env.Context->CreateRHIFrameBuffer();
    PTResource.FBuffer2 = Env.Context->CreateRHIFrameBuffer();
    PTResource.StorageBuffer = Env.Context->CreateRHIBuffer();
    PTResource.PrimitiveBuffer = Env.Context->CreateRHIBuffer();
    PTResource.MeshVerticesBuffer = Env.Context->CreateRHIBuffer();
    PTResource.MeshBVHBuffer = Env.Context->CreateRHIBuffer();
    PTResource.RayStateBuffer = Env.Context->CreateRHIBuffer();

    std::vector<RHIFormat> ColorRTFormats = { RHIFormat::R8G8B8A8_SRGB };
    PTResource.PTRenderPass->Initialize(Env.Context, ColorRTFormats);

    {
        // Frame Resources
        ImageExtent3D ext = Env.Swapchain->GetFrameSize();
        PTResource.RHIScreenBuffer1->Initialize(Env.Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        PTResource.RHIScreenBuffer2->Initialize(Env.Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        PTResource.RHIScreenBufferDepth->Initialize(Env.Context, ext, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
        PTResource.RHIStoreImage->Initialize(Env.Context, ext, RHIFormat::R32G32B32A32_SFLOAT, RHIResourceState::SHADER_WRITE | RHIResourceState::SHADER_READ, 1);
        std::vector<IRHIImageResource*> ColorRT1 = { PTResource.RHIScreenBuffer1.get() };
        std::vector<IRHIImageResource*> ColorRT2 = { PTResource.RHIScreenBuffer2.get() };
        PTResource.FBuffer1->Initialize(Env.Context, PTResource.PTRenderPass.get(), ColorRT1, PTResource.RHIScreenBufferDepth.get());
        PTResource.FBuffer2->Initialize(Env.Context, PTResource.PTRenderPass.get(), ColorRT2, PTResource.RHIScreenBufferDepth.get());
    }


    PTResource.Pipeline.InitializeAsGraphics(Env.Context, *PTResource.PTRenderPass, PT_VERTSHADER, PT_FRAGSHADER, sizeof(float4));
    PTResource.PostPipeline.InitializeAsGraphics(Env.Context, *Env.RenderPass, PTPOST_VERTSHADER, PTPOST_FRAGSHADER, sizeof(float4));
    PTResource.TestCompPipeline.InitializeAsCompute(Env.Context, TEST_COMPSHADER);
    PTResource.IterationPipeline.InitializeAsCompute(Env.Context, PT_ITERATION_COMPSHADER);
    PTResource.Pipeline.Compile(Env.Context, PTResource.PTRenderPass.get());
    PTResource.PostPipeline.Compile(Env.Context, Env.RenderPass);
    PTResource.TestCompPipeline.Compile(Env.Context, std::nullopt);
    PTResource.IterationPipeline.Compile(Env.Context, std::nullopt);

    PTResource.RHIFullScreenQuadBuffer->Initialize(Env.Context, sizeof(float4) * 4, RHIResourceState::BUFFER_VERTEX);
    PTResource.RHIFullScreenQuadIndexBuffer->Initialize(Env.Context, sizeof(uint32_t) * 6, RHIResourceState::BUFFER_INDEX);
    float4 FullScreenVertices[4] = { float4(-1., -1., 0., 1.), float4(-1., 1., 0., 1.), float4(1., 1., 0., 1.), float4(1., -1., 0., 1.) };
    uint32_t FullScreenVerticesIndex[6] = { 0, 2, 1, 0, 3, 2 };
    PTResource.RHIFullScreenQuadBuffer->CopyToBuffer(Env.Context, FullScreenVertices, sizeof(float4) * 4);
    PTResource.RHIFullScreenQuadIndexBuffer->CopyToBuffer(Env.Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
    PTResource.CameraUniform->Initialize(Env.Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_UNIFORM);
    PTResource.StorageBuffer->Initialize(Env.Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_SHADER_STORAGE);
    PTResource.RayStateBuffer->Initialize(Env.Context, sizeof(RayObject) * Env.FrameSize.Width * Env.FrameSize.Height, RHIResourceState::BUFFER_SHADER_STORAGE);

    // ApplyScene() runs on the first BeginRender() and resets this again;
    // starting at 0 makes the initial sweep clear every tile before writing.
    cuo.frameId = 0;
    cuo.totalIters = 1;
    cuo.dispatchDepth = 4;
    cuo.roughness = 0.3f;
    cuo.prob_lambert = 0.5f;
    cuo.enableNEE = 1;
    cuo.useVertexColor = 0;

    // The mesh/BVH/primitive buffers are deliberately NOT initialized here.
    // ApplyScene() owns them and fills them on the first BeginRender(), which
    // is also what a scene switch does. Creating the spot OBJ here as well
    // would just load it twice on startup.

    // Load Texture
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("./models/spot/spot_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: ");
    }

    PTResource.RHIMeshTexture = Env.Context->CreateRHIImageResource();
    PTResource.RHIMeshTexture->Initialize(Env.Context, (uint32_t)texHeight, (uint32_t)texWidth, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::SHADER_READ | RHIResourceState::COPY_DST);

    // Copy texture data
    auto Cmd = Env.Context->CreateRHICommandBuffer();
    Cmd->Initialize(Env.Context);
    Cmd->BeginCommandBuffer();
    PTResource.RHIMeshTexture->CopyToTexture(Cmd.get(), Env.Context, pixels, 4);
    PTResource.RHIMeshTexture->Transition(Cmd.get(), RHIResourceState::SHADER_READ);
    Cmd->EndCommandBuffer();

    // In some BRDF/PathTracer engines, we might need to submit this Cmd immediately.
    // However, the IRHIContext doesn't expose a Submit method directly in the interface.
    // Swapchain->PresentFrameAndRelease usually handles submission. 
    // We'll trust the RHI implementation handles the copy.

    stbi_image_free(pixels);
}


 void PathTraceRenderer::LoadMesh(const std::string& MeshPath) {
     static_assert(sizeof(BVHBox<PTVertex, uint32_t>) == 48);
     // Load Mesh
     auto Mesh = TMesh<PTVertex, uint32_t>::LoadObj(MeshPath);
     CalculateNormal<PTVertex, uint32_t>(Mesh);
     BuildBVHOnMesh<PTVertex, uint32_t>(Mesh, BVHBoxes);
     PTResource.MeshVerticesBuffer->Initialize(Env.Context, sizeof(PTVertex) * Mesh.Vertices.size(), RHIResourceState::BUFFER_SHADER_STORAGE);
     PTResource.MeshVerticesBuffer->CopyToBuffer(Env.Context, Mesh.Vertices.data(), (uint32_t)(Mesh.Vertices.size() * sizeof(PTVertex)));

     PTResource.MeshBVHBuffer->Initialize(Env.Context, sizeof(BVHBox<PTVertex, uint32_t>) * BVHBoxes.size(), RHIResourceState::BUFFER_SHADER_STORAGE);
     PTResource.MeshBVHBuffer->CopyToBuffer(Env.Context, BVHBoxes.data(), (uint32_t)(BVHBoxes.size() * sizeof(BVHBox<PTVertex, uint32_t>)));
     cuo.vertexCount = Mesh.Vertices.size();
     SceneTriangleCount = (uint32_t)(Mesh.Vertices.size() / 3);
 }

namespace {

// The path tracer has exactly one mesh sampler (binding 5), so the 25 Sponza
// base-colour textures cannot all be bound at once. Instead, sample each
// texture on the CPU and store its average colour in the vertex Color field,
// which already exists in PTVertex/the std430 MeshVertices layout and was
// previously unused. Geometry.glsl switches to it via uniforms.useVertexColor.
std::vector<float3> ComputeMaterialAverageColors(const std::vector<std::string>& TexturePaths)
{
    std::vector<float3> Colors;
    Colors.reserve(TexturePaths.size());
    for (const std::string& Path : TexturePaths)
    {
        float3 Avg(0.62f, 0.58f, 0.52f); // neutral stone fallback
        if (!Path.empty())
        {
            int Width = 0, Height = 0, Channels = 0;
            stbi_uc* Pixels = stbi_load(Path.c_str(), &Width, &Height, &Channels, 3);
            if (Pixels && Width > 0 && Height > 0)
            {
                const int Step = std::max(1, std::max(Width, Height) / 64);
                double Sum[3] = { 0.0, 0.0, 0.0 };
                uint64_t Count = 0;
                for (int y = 0; y < Height; y += Step)
                {
                    for (int x = 0; x < Width; x += Step)
                    {
                        const size_t Index = (size_t(y) * Width + x) * 3;
                        Sum[0] += Pixels[Index + 0];
                        Sum[1] += Pixels[Index + 1];
                        Sum[2] += Pixels[Index + 2];
                        ++Count;
                    }
                }
                if (Count > 0)
                {
                    const double Inv = 1.0 / (255.0 * double(Count));
                    const float3 SRGB(float(Sum[0] * Inv), float(Sum[1] * Inv), float(Sum[2] * Inv));
                    // The spot texture is uploaded as _SRGB and decoded by the
                    // sampler; do the same conversion here so both scenes use
                    // linear-space albedo.
                    Avg = float3(std::pow(SRGB.r, 2.2f),
                                 std::pow(SRGB.g, 2.2f),
                                 std::pow(SRGB.b, 2.2f));
                }
                stbi_image_free(Pixels);
            }
        }
        Colors.push_back(Avg);
    }
    return Colors;
}

} // namespace

void PathTraceRenderer::LoadGLTFMesh(const std::string& GltfPath, int TriangleStride)
{
    static_assert(sizeof(BVHBox<PTVertex, uint32_t>) == 48);

    std::cout << "[PathTrace] Loading " << GltfPath << " ..." << std::endl;
    auto Start = std::chrono::steady_clock::now();
    auto Mesh = TMesh<PTVertex, uint32_t>::LoadGLTF(GltfPath);

    const std::vector<float3> MaterialColors = ComputeMaterialAverageColors(Mesh.MaterialTextures);

    // LoadGLTF expands indices into a flat non-indexed vertex stream. Walk the
    // per-primitive index counts in the same order and stamp the material
    // colour onto every vertex of that primitive.
    size_t VertexOffset = 0;
    for (size_t Prim = 0; Prim < Mesh.PrimitiveIndexCount.size(); ++Prim)
    {
        const int MaterialIndex = (Prim < Mesh.MaterialPrimIndices.size())
            ? Mesh.MaterialPrimIndices[Prim] : -1;
        const float3 Color = (MaterialIndex >= 0 && size_t(MaterialIndex) < MaterialColors.size())
            ? MaterialColors[size_t(MaterialIndex)] : float3(0.6f);

        const size_t Count = size_t(Mesh.PrimitiveIndexCount[Prim]);
        const size_t End = std::min(Mesh.Vertices.size(), VertexOffset + Count);
        for (size_t i = VertexOffset; i < End; ++i)
            Mesh.Vertices[i].Color = Color;
        VertexOffset += Count;
    }

    // LOD: cap the triangle count of EACH glTF primitive rather than taking a
    // global stride. Sponza's 262k triangles are heavily concentrated in a
    // handful of decorative primitives (28k / 23k / 14k / ...), so capping
    // leaves the ~90 architectural primitives completely intact and only
    // thins the high-poly detail. Stride 1 = full mesh.
    if (TriangleStride > 1)
    {
        const size_t Cap = size_t(std::max(64, 4096 / TriangleStride));
        std::vector<PTVertex> Reduced;
        Reduced.reserve(Mesh.Vertices.size() / size_t(TriangleStride) + 3);
        size_t PrimVertexOffset = 0;
        for (size_t Prim = 0; Prim < Mesh.PrimitiveIndexCount.size(); ++Prim)
        {
            const size_t PrimTriangles = size_t(Mesh.PrimitiveIndexCount[Prim]) / 3;
            const size_t Step = std::max(size_t(1), (PrimTriangles + Cap - 1) / Cap);
            for (size_t t = 0; t < PrimTriangles; t += Step)
            {
                const size_t Base = PrimVertexOffset + 3 * t;
                if (Base + 2 >= Mesh.Vertices.size()) break;
                Reduced.push_back(Mesh.Vertices[Base + 0]);
                Reduced.push_back(Mesh.Vertices[Base + 1]);
                Reduced.push_back(Mesh.Vertices[Base + 2]);
            }
            PrimVertexOffset += PrimTriangles * 3;
        }
        Mesh.Vertices = std::move(Reduced);
    }

    // glTF already supplies normals; CalculateNormal() would rebuild a
    // std::map over ~800k vertices and throw those away, so skip it here.
    BuildBVHOnMesh<PTVertex, uint32_t>(Mesh, BVHBoxes);

    PTResource.MeshVerticesBuffer->Initialize(Env.Context,
        (uint32_t)(sizeof(PTVertex) * Mesh.Vertices.size()), RHIResourceState::BUFFER_SHADER_STORAGE);
    PTResource.MeshVerticesBuffer->CopyToBuffer(Env.Context, Mesh.Vertices.data(),
        (uint32_t)(Mesh.Vertices.size() * sizeof(PTVertex)));

    PTResource.MeshBVHBuffer->Initialize(Env.Context,
        (uint32_t)(sizeof(BVHBox<PTVertex, uint32_t>) * BVHBoxes.size()), RHIResourceState::BUFFER_SHADER_STORAGE);
    PTResource.MeshBVHBuffer->CopyToBuffer(Env.Context, BVHBoxes.data(),
        (uint32_t)(BVHBoxes.size() * sizeof(BVHBox<PTVertex, uint32_t>)));

    cuo.vertexCount = Mesh.Vertices.size();
    SceneTriangleCount = (uint32_t)(Mesh.Vertices.size() / 3);

    const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
    std::cout << "[PathTrace] Sponza ready: " << SceneTriangleCount << " triangles, "
              << BVHBoxes.size() << " BVH nodes, " << Seconds << " s" << std::endl;
}

void PathTraceRenderer::UpdateLightEmission()
{
    if (!SceneObjects.empty() && ActiveSceneIndex >= 0)
    {
        SceneObjects[0].emission = BaseLightEmission * LightIntensity;
        PTResource.PrimitiveBuffer->CopyToBuffer(Env.Context, SceneObjects.data(),
            (uint32_t)(SceneObjects.size() * sizeof(ModelUniformObject)));
    }
}

void PathTraceRenderer::ApplyScene(int Index, RenderControl* Control)
{
    Index = std::clamp(Index, 0, (int)EPathTraceScene::Count - 1);
    const PathTraceScenePreset Preset = BuildPathTraceScenePreset(Index);
    std::cout << "[PathTrace] Scene -> " << kPathTraceSceneNames[Index]
              << " (" << Preset.Objects.size() << " primitives)" << std::endl;

    // The frame loop ends with WaitDeviceIdle(), and ApplyScene() runs from
    // BeginRender() before any command buffer for this frame is recorded, so
    // destroying and recreating these buffers is safe.
    const bool bFirstApply = (ActiveSceneIndex < 0);
    if (!bFirstApply)
    {
        PTResource.PrimitiveBuffer->Cleanup(Env.Context);
        PTResource.MeshVerticesBuffer->Cleanup(Env.Context);
        PTResource.MeshBVHBuffer->Cleanup(Env.Context);
    }

    SceneObjects = Preset.Objects;
    BaseLightEmission = Preset.BaseLightEmission;
    if (!SceneObjects.empty())
        SceneObjects[0].emission = BaseLightEmission * LightIntensity;

    // SceneUniform is a legacy UBO (nothing binds it), kept in sync for
    // debugging. The compute shaders read PrimitiveBuffer instead.
    if (bFirstApply)
    {
        PTResource.SceneUniform->Initialize(Env.Context, sizeof(ModelUniformObject), 8,
            RHIResourceState::BUFFER_UNIFORM);
    }
    PTResource.SceneUniform->CopyToBuffer(Env.Context, SceneObjects.data(),
        (uint32_t)(SceneObjects.size() * sizeof(ModelUniformObject)));

    PTResource.PrimitiveBuffer->Initialize(Env.Context,
        (uint32_t)(sizeof(ModelUniformObject) * SceneObjects.size()),
        RHIResourceState::BUFFER_SHADER_STORAGE);
    PTResource.PrimitiveBuffer->CopyToBuffer(Env.Context, SceneObjects.data(),
        (uint32_t)(SceneObjects.size() * sizeof(ModelUniformObject)));

    if (Preset.bGLTFMesh)
        LoadGLTFMesh(Preset.MeshPath, SponzaTriangleStride);
    else
        LoadMesh(Preset.MeshPath);

    cuo.modelUniformCount = (int32_t)SceneObjects.size();
    cuo.useVertexColor = Preset.bUseVertexColors ? 1 : 0;
    cuo.dispatchDepth = Preset.DispatchDepth;
    ResetAccumulation();           // restart the tile sweep + temporal accumulation

    if (Control)
    {
        cuo.camToWorld = Preset.Camera;
        Control->CameraTransformLocalToWorld = Preset.Camera;
        Control->CameraOrientation = Preset.CameraOrientation;
        Control->movingSpeed = Preset.MovingSpeed;
        Control->isClick = false;
        Control->bClear = false;
    }

    ActiveSceneIndex = Index;
    SceneStatus = Preset.Description + " | " + std::to_string(SceneTriangleCount) + " triangles";
}

void PathTraceRenderer::ResetAccumulation()
{
    cuo.frameId = 0;
    TileIndex = 0;
}

void PathTraceRenderer::UpdateTileRect()
{
    const int Width = std::max(1, (int)cuo.screenres.r);
    const int Height = std::max(1, (int)cuo.screenres.g);
    const int TileSize = GetTileSize();

    TilesX = std::max(1, (Width + TileSize - 1) / TileSize);
    TilesY = std::max(1, (Height + TileSize - 1) / TileSize);
    TileCount = TilesX * TilesY;

    if (TileIndex < 0 || TileIndex >= TileCount)
        TileIndex = 0;

    const int OriginX = (TileIndex % TilesX) * TileSize;
    const int OriginY = (TileIndex / TilesX) * TileSize;
    const int TileW = std::min(TileSize, Width - OriginX);
    const int TileH = std::min(TileSize, Height - OriginY);
    cuo.tileRect = int4(OriginX, OriginY, TileW, TileH);
}

void PathTraceRenderer::UpdateUniformBuffer(float4x4 camToWorld, float time, RenderControl* control) {
    // sensor size 32mm
    // focal length 45mm
    // fov = 2 * atan(sensor_size/2/focal_length)
    cuo.camToWorld = camToWorld;
    cuo.time = time;

    // A camera change invalidates every tile, so restart the sweep from tile 0
    // as well as clearing the temporal accumulation counter.
    if (control->isClick)
    {
        ResetAccumulation();
    }
    if (control->bClear) {
        ResetAccumulation();
        control->bClear = false;
    }
}

bool PathTraceRenderer::BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) {
    // RenderUI() ran just before this, so the combo selection is already up to
    // date. The previous frame ended with WaitDeviceIdle(), which makes it safe
    // to release and rebuild the scene buffers here.
    if (SceneIndex != ActiveSceneIndex || bSceneReloadRequested)
    {
        bSceneReloadRequested = false;
        ApplyScene(SceneIndex, control);
    }

    cuo.screenres.r = Env.Swapchain->GetFrameSize().Width;
    cuo.screenres.g = Env.Swapchain->GetFrameSize().Height;
    // tileRect must be written before the uniform buffers are uploaded: the
    // compute shaders read it from the same std140 block in this frame.
    UpdateTileRect();
    control->currentFrame = cuo.frameId;
    PTResource.CameraUniform->CopyToBuffer(Env.Context, &cuo, sizeof(CameraUniformObject));
    PTResource.StorageBuffer->CopyToBuffer(Env.Context, &cuo, sizeof(CameraUniformObject));

    return BaseRenderer::BeginRender(CommandBuffer, OutFrameBuffer, control);
}

void PathTraceRenderer::ResizeScreen() {
    if (Env.FrameSize != Env.Swapchain->GetFrameSize()) {
        PTResource.RHIScreenBuffer1->Cleanup(Env.Context);
        PTResource.RHIScreenBuffer2->Cleanup(Env.Context);
        PTResource.RHIScreenBufferDepth->Cleanup(Env.Context);
        PTResource.RHIStoreImage->Cleanup(Env.Context);
        PTResource.FBuffer1->Cleanup(Env.Context);
        PTResource.FBuffer2->Cleanup(Env.Context);
        PTResource.RayStateBuffer->Cleanup(Env.Context);

        ImageExtent3D ext = Env.Swapchain->GetFrameSize();
        Env.FrameSize = ext;
        PTResource.RHIScreenBuffer1->Initialize(Env.Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        PTResource.RHIScreenBuffer2->Initialize(Env.Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        PTResource.RHIScreenBufferDepth->Initialize(Env.Context, ext, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
        PTResource.RHIStoreImage->Initialize(Env.Context, ext, RHIFormat::R32G32B32A32_SFLOAT, RHIResourceState::SHADER_WRITE | RHIResourceState::SHADER_READ, 1);
        PTResource.RayStateBuffer->Initialize(Env.Context, sizeof(RayObject) * Env.FrameSize.Width * Env.FrameSize.Height, RHIResourceState::BUFFER_SHADER_STORAGE);
        std::vector<IRHIImageResource*> ColorRT1 = { PTResource.RHIScreenBuffer1.get() };
        std::vector<IRHIImageResource*> ColorRT2 = { PTResource.RHIScreenBuffer2.get() };
        PTResource.FBuffer1->Initialize(Env.Context, PTResource.PTRenderPass.get(), ColorRT1, PTResource.RHIScreenBufferDepth.get());
        PTResource.FBuffer2->Initialize(Env.Context, PTResource.PTRenderPass.get(), ColorRT2, PTResource.RHIScreenBufferDepth.get());
    }
}

void PathTraceRenderer::DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) {
    if (control->Recompile) {
        PTResource.Pipeline.Compile(Env.Context, PTResource.PTRenderPass.get());
        PTResource.PostPipeline.Compile(Env.Context, Env.RenderPass);
        PTResource.TestCompPipeline.Compile(Env.Context, std::nullopt);
        PTResource.IterationPipeline.Compile(Env.Context, std::nullopt);
        control->Recompile = false;
    }
    if (!control->RenderingPaused && !control->ShaderCompileHasError) {
        if (cuo.frameId < control->maxFrames || control->maxFrames < 0) {
            PTResource.RHIStoreImage->Transition(CommandBuffer, RHIResourceState::SHADER_WRITE);
            // One tile per frame: dispatch only the workgroups covering this
            // tile instead of the whole screen. EndRender() submits this
            // command buffer and waits, so each tile is its own GPU submission
            // and stays far below the Windows TDR timeout.
            const uint32_t GroupsX = (uint32_t(std::max(1, cuo.tileRect.z)) + 15u) / 16u;
            const uint32_t GroupsY = (uint32_t(std::max(1, cuo.tileRect.w)) + 15u) / 16u;
            // Debug shading is a single primary-ray pass by definition; never
            // route it through the multi-pass ray-state pipeline.
            if (control->useSinglePass || cuo.debugMode != 0) {
                PTResource.TestCompPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, PTResource.RHIStoreImage.get());
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.StorageBuffer.get(), 1);
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.PrimitiveBuffer.get(), 2);
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshVerticesBuffer.get(), 3);
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshBVHBuffer.get(), 4);
                PTResource.TestCompPipeline.PipelineObject->SetBindingResource(5, DescriptorType::SAMPLER2D, PTResource.RHIMeshTexture.get());
                PTResource.TestCompPipeline.PipelineObject->Dispatch(CommandBuffer, GroupsX, GroupsY, 1);
            } else {
                for (int i = 0; i < cuo.totalIters; ++i) {
                    PTResource.IterationPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, PTResource.RHIStoreImage.get());
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.StorageBuffer.get(), 1);
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.PrimitiveBuffer.get(), 2);
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshVerticesBuffer.get(), 3);
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshBVHBuffer.get(), 4);
                    PTResource.IterationPipeline.PipelineObject->SetBindingResource(5, DescriptorType::SAMPLER2D, PTResource.RHIMeshTexture.get());
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.RayStateBuffer.get(), 6);
                    PTResource.IterationPipeline.PipelineObject->Dispatch(CommandBuffer, GroupsX, GroupsY, 1);
                }
            }
            // frameId is the number of COMPLETED sweeps. It is both the
            // temporal blend weight and part of the RNG seed, so every tile in
            // a sweep must see the same value: advance it only after the last
            // tile, never once per rendered frame.
            ++TileIndex;
            if (TileIndex >= TileCount)
            {
                TileIndex = 0;
                ++cuo.frameId;
                std::cout << "[PathTrace] Sweep " << cuo.frameId << " complete ("
                          << TileCount << " tiles @ " << GetTileSize()
                          << "px, " << GroupsX << "x" << GroupsY
                          << " workgroups/dispatch)" << std::endl;
            }
        }
    }
    
    PTResource.RHIStoreImage->Transition(CommandBuffer, RHIResourceState::SHADER_READ);
    Env.RenderPass->BeginRenderPass(CommandBuffer, FrameBuffer);
    if (!control->RenderingPaused && !control->ShaderCompileHasError) {
        PTResource.PostPipeline.PipelineObject->SetBindingResource(0, DescriptorType::SAMPLER2D, PTResource.RHIStoreImage.get());
        PTResource.PostPipeline.PipelineObject->SetBindingResource(1, DescriptorType::UNIFORM, PTResource.CameraUniform.get());
        PTResource.PostPipeline.PipelineObject->BindIndexBuffer(PTResource.RHIFullScreenQuadIndexBuffer.get(), 0);
        PTResource.PostPipeline.PipelineObject->BindVertexBuffer(PTResource.RHIFullScreenQuadBuffer.get(), 0, 0);
        IndexBufferSize = 6;
        PTResource.PostPipeline.PipelineObject->Draw(CommandBuffer, IndexBufferSize, 0, 1);
    }
    if (Env.ImGUI) {
        Env.ImGUI->DispatchImGUI(CommandBuffer);
    }
    Env.RenderPass->EndRenderPass(CommandBuffer);
}

void PathTraceRenderer::EndRender(IRHICommandBuffer* CommandBuffer) {
    PTResource.TestCompPipeline.PipelineObject->CopyDescriptors(Env.Context);
    PTResource.IterationPipeline.PipelineObject->CopyDescriptors(Env.Context);
    PTResource.PostPipeline.PipelineObject->CopyDescriptors(Env.Context);
    BaseRenderer::EndRender(CommandBuffer);
    swap = !swap;
}

 void PathTraceRenderer::ProcessInput()
{
    Env.Context->ProcessFrameInput();
}

void PathTraceRenderer::CaptureFrame(const std::string& Path) {
    const uint32_t Width = Env.FrameSize.Width;
    const uint32_t Height = Env.FrameSize.Height;
    if (Width == 0 || Height == 0) return;

    // RHIStoreImage is R32G32B32A32_SFLOAT -> 16 bytes per pixel.
    const size_t FloatCount = size_t(Width) * Height * 4;
    std::vector<float> Pixels(FloatCount, 0.f);

    // Both RHI backends create their own one-time command buffer inside
    // CopyFromTexture, but the interface requires a command buffer handle.
    auto Cmd = Env.Context->CreateRHICommandBuffer();
    Cmd->Initialize(Env.Context);
    PTResource.RHIStoreImage->CopyFromTexture(Cmd.get(), Env.Context, Pixels.data(), 4 * sizeof(float));
    // Do NOT call Cleanup() here: the Vulkan implementation frees the command
    // buffer/pool without nulling the handles, and its destructor frees them
    // again -> double free at shutdown. Let the unique_ptr destructor handle it.

    std::vector<uint8_t> RGBA(FloatCount, 255);
    auto ACES = [](float x) {
        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.f, 1.f);
    };
    for (size_t i = 0; i < FloatCount; ++i)
    {
        const float v = std::clamp(Pixels[i], 0.f, 1.f);
        // Debug normals are already a display-space [0,1] encoding; write them
        // verbatim so the PNG is a literal normal map. Path-traced HDR output
        // gets the same ACES + gamma as ScreenPost.frag.
        const float Out = (cuo.debugMode != 0) ? v : std::pow(ACES(v), 1.f / 2.2f);
        RGBA[i] = (uint8_t)std::lround(Out * 255.f);
    }

    if (stbi_write_png(Path.c_str(), (int)Width, (int)Height, 4, RGBA.data(), (int)Width * 4))
    {
        std::cout << "[PathTrace] Wrote " << Path << " (" << Width << "x" << Height << ")" << std::endl;
    }
    else
    {
        std::cerr << "[PathTrace] Failed to write " << Path << std::endl;
    }
}

void PathTraceRenderer::DrawImGUI()
{
    ImGui::Begin("Path Trace Parameters");
    ImGui::Text("Path Tracing Parameters");

    ImGui::SeparatorText("Render Mode");
    if (ImGui::Combo("##PathTraceRenderMode", &cuo.debugMode,
                     "Path Traced\0Triangle Normal (no bounce)\0Triangle Normal + Depth\0"))
    {
        ResetAccumulation();
    }
    if (cuo.debugMode != 0)
    {
        ImGui::TextDisabled("Primary ray only; flat triangle normal as colour. "
                            "Misses are dark navy. No bounce, NEE or accumulation.");
    }

    ImGui::SeparatorText("Scene");

    // Drives a full rebuild of primitives + mesh BVH + camera in BeginRender().
    if (ImGui::Combo("##PathTraceScene", &SceneIndex, kPathTraceSceneComboItems))
    {
        ResetAccumulation();
    }

    if (SceneIndex == (int)EPathTraceScene::Sponza)
    {
        ImGui::TextWrapped("Sponza is a 262k-triangle glTF. The LOD stride caps "
                           "each of its 103 primitives: 1 = 262k, 4 = ~53k, "
                           "12 = ~20k. Tile accumulation keeps each dispatch "
                           "small, so stride 1 is safe in every render mode.");
        if (ImGui::SliderInt("Sponza LOD stride", &SponzaTriangleStride, 1, 12))
        {
            bSceneReloadRequested = true;
            ResetAccumulation();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(1 = full detail, 12 = fastest)");
    }

    if (ImGui::SliderFloat("Light Intensity", &LightIntensity, 0.1f, 20.f, "%.2f"))
    {
        UpdateLightEmission();
        ResetAccumulation();
    }

    if (!SceneStatus.empty())
    {
        ImGui::TextDisabled("%s", SceneStatus.c_str());
    }
    if (SceneIndex != ActiveSceneIndex || bSceneReloadRequested)
    {
        ImGui::TextColored(ImVec4(1.f, 0.75f, 0.25f, 1.f), "Loading scene...");
    }

    ImGui::SeparatorText("Tile accumulation");
    if (ImGui::Combo("##PathTraceTileSize", &TileSizeIndex,
                     "64 px\0" "128 px\0" "256 px\0" "512 px\0"))
    {
        // A different tile grid changes the sweep length, so start a fresh
        // accumulation instead of mixing sweeps with different frameIds.
        ResetAccumulation();
    }
    ImGui::Text("Sweep %d | next tile %d / %d | %dx%d workgroups per dispatch",
                cuo.frameId, TileIndex + 1, TileCount,
                (std::max(1, cuo.tileRect.z) + 15) / 16,
                (std::max(1, cuo.tileRect.w) + 15) / 16);
    ImGui::TextDisabled("One tile per submit; a full sweep = one sample per pixel.");

    ImGui::SeparatorText("Sampling");

    int prevIters = cuo.totalIters;
    int prevDepth = cuo.dispatchDepth;
    float prevRough = cuo.roughness;
    float prevProb = cuo.prob_lambert;
    int prevNEE = cuo.enableNEE;

    ImGui::SliderInt("Total Iterations", &cuo.totalIters, 1, 10);
    ImGui::SliderInt("Dispatch Depth", &cuo.dispatchDepth, 1, 24);
    ImGui::SliderFloat("Roughness", &cuo.roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Lambert Prob", &cuo.prob_lambert, 0.0f, 1.0f);
    
    bool enableNEE = (cuo.enableNEE != 0);
    if (ImGui::Checkbox("Enable NEE", &enableNEE)) {
        cuo.enableNEE = enableNEE ? 1 : 0;
    }

    if (prevIters != cuo.totalIters ||
        prevDepth != cuo.dispatchDepth ||
        prevRough != cuo.roughness ||
        prevProb != cuo.prob_lambert ||
        prevNEE != cuo.enableNEE) 
    {
        ResetAccumulation(); // reset accumulation
    }

    ImGui::End();
}
