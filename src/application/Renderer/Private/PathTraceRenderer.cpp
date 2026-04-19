#include <iostream>
#include <fstream>

#define PATHTRACERENDERER_IMPLEMENT
 #include "PathTraceRenderer.h"
#define SHADERCOMPILER_INCLUDE
#include "ShaderCompiler.h"

 #define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include "stb_image_write.h"

void ComputePipeline::SetAllShaderBindings(IRHIContext* Context) {
    PipelineFactory->SetDescriptorBinding(0, DescriptorType::IMAGE2D, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(1, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(2, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(3, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(4, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(5, DescriptorType::SAMPLER2D, IRHIPipelineFactory::EPipelineStages::CS);
}

void IterationComputePipeline::SetAllShaderBindings(IRHIContext* Context) {
    PipelineFactory->SetDescriptorBinding(0, DescriptorType::IMAGE2D, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(1, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(2, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(3, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(4, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(5, DescriptorType::SAMPLER2D, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(6, DescriptorType::STORAGE, IRHIPipelineFactory::EPipelineStages::CS);
}

void PathTracerPipeline::SetAllShaderBindings(IRHIContext* Context) {
    PipelineFactory->SetDescriptorBinding(0, DescriptorType::UNIFORM, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->SetDescriptorBinding(1, DescriptorType::UNIFORM, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->SetDescriptorBinding(2, DescriptorType::SAMPLER2D, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->AddBufferBinding(0, sizeof(float3));
    PipelineFactory->AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
}

void PTPostPipeline::SetAllShaderBindings(IRHIContext* Context) {
    PipelineFactory->SetDescriptorBinding(0, DescriptorType::SAMPLER2D, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->SetDescriptorBinding(1, DescriptorType::UNIFORM, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->AddBufferBinding(0, sizeof(float3));
    PipelineFactory->AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
}

 void PathTraceRenderer::CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation) {
    uptr_Context = IRHIPlatformSupport::Get(Backend)->CreateRHIContext();
    Context = uptr_Context.get();

    ContextCreateParams Params;
    Params.WindowWidth = Width;
    Params.WindowHeight = Height;
    Params.bEnableValidation = bEnableValidation;
    Context->Initialize(Params);
    RHIFullScreenQuadBuffer = Context->CreateRHIBuffer();
    RHIFullScreenQuadIndexBuffer = Context->CreateRHIBuffer();
    RHIScreenBuffer1 = Context->CreateRHIImageResource();
    RHIScreenBuffer2 = Context->CreateRHIImageResource();
    RHIScreenBufferDepth = Context->CreateRHIImageResource();
    RHIStoreImage = Context->CreateRHIImageResource();
    PresentPass = Context->CreateRHIRenderPass();
    PTRenderPass = Context->CreateRHIRenderPass();
    CameraUniform = Context->CreateRHIBuffer();
    SceneUniform = Context->CreateRHIBuffer();
    Swapchain = Context->CreateRHISwapchain();
    FBuffer1 = Context->CreateRHIFrameBuffer();
    FBuffer2 = Context->CreateRHIFrameBuffer();
    ImGUI = Context->CreateRHIImGUI();
    StorageBuffer = Context->CreateRHIBuffer();
    PrimitiveBuffer = Context->CreateRHIBuffer();
    MeshVerticesBuffer = Context->CreateRHIBuffer();
    MeshBVHBuffer = Context->CreateRHIBuffer();
    RayStateBuffer = Context->CreateRHIBuffer();

    SwapchainFormat = B8G8R8A8_SRGB; //R8G8B8A8_UNORM; // R8G8B8A8_UNORM is compatible for both Vulkan and D3D12 backend

    // create renderer
    Swapchain->Initialize(Context, SwapchainFormat);

    std::vector<RHIFormat> ColorRTFormats = { RHIFormat::R8G8B8A8_SRGB };
    std::vector<RHIFormat> PresentRTFormats = { SwapchainFormat };
    //std::vector<RHIFormat> ColorRTFormats1 = { RHIFormat::R8G8B8A8_UNORM };
    PTRenderPass->Initialize(Context, ColorRTFormats);
    PresentPass->Initialize(Context, PresentRTFormats);

    {
        // Frame Resources
        ImageExtent3D ext = Swapchain->GetFrameSize();
        FrameSize = ext;
        RHIScreenBuffer1->Initialize(Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        RHIScreenBuffer2->Initialize(Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        RHIScreenBufferDepth->Initialize(Context, ext, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
        RHIStoreImage->Initialize(Context, ext, RHIFormat::R32G32B32A32_SFLOAT, RHIResourceState::SHADER_WRITE | RHIResourceState::SHADER_READ, 1);
        std::vector<IRHIImageResource*> ColorRT1 = { RHIScreenBuffer1.get() };
        std::vector<IRHIImageResource*> ColorRT2 = { RHIScreenBuffer2.get() };
        FBuffer1->Initialize(Context, PTRenderPass.get(), ColorRT1, RHIScreenBufferDepth.get());
        FBuffer2->Initialize(Context, PTRenderPass.get(), ColorRT2, RHIScreenBufferDepth.get());
    }


    Pipeline.InitializeAsGraphics(Context, *PTRenderPass, PT_VERTSHADER, PT_FRAGSHADER);
    PostPipeline.InitializeAsGraphics(Context, *PresentPass, PTPOST_VERTSHADER, PTPOST_FRAGSHADER);
    TestCompPipeline.InitializeAsCompute(Context, TEST_COMPSHADER);
    IterationPipeline.InitializeAsCompute(Context, PT_ITERATION_COMPSHADER);
    Pipeline.SetAllShaderBindings(Context);
    PostPipeline.SetAllShaderBindings(Context);
    TestCompPipeline.SetAllShaderBindings(Context);
    IterationPipeline.SetAllShaderBindings(Context);
    Pipeline.Compile(Context, PTRenderPass.get());
    PostPipeline.Compile(Context, PresentPass.get());
    TestCompPipeline.Compile(Context, std::nullopt);
    IterationPipeline.Compile(Context, std::nullopt);

    RHIFullScreenQuadBuffer->Initialize(Context, sizeof(float3) * 8, RHIResourceState::BUFFER_VERTEX);
    RHIFullScreenQuadIndexBuffer->Initialize(Context, sizeof(uint32_t) * 6, RHIResourceState::BUFFER_INDEX);
    float3 FullScreenVertices[4] = { float3(-1., -1., 0.), float3(-1., 1., 0.), float3(1., 1., 0.), float3(1., -1., 0.) };
    uint32_t FullScreenVerticesIndex[6] = { 0, 2, 1, 0, 3, 2 };
    RHIFullScreenQuadBuffer->CopyToBuffer(Context, FullScreenVertices, sizeof(float3) * 8);
    RHIFullScreenQuadIndexBuffer->CopyToBuffer(Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
    CameraUniform->Initialize(Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_UNIFORM);
    StorageBuffer->Initialize(Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_SHADER_STORAGE);
    RayStateBuffer->Initialize(Context, sizeof(RayObject) * FrameSize.Width * FrameSize.Height, RHIResourceState::BUFFER_SHADER_STORAGE);
    ImGUI->Initialize(Context, Swapchain.get(), PresentPass.get());

    cuo.frameId = 1;
    LoadMesh("./models/spot/spot_triangulated.obj");

    // Load Texture
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("./models/spot/spot_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: ");
    }

    RHIMeshTexture = Context->CreateRHIImageResource();
    RHIMeshTexture->Initialize(Context, (uint32_t)texHeight, (uint32_t)texWidth, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::SHADER_READ | RHIResourceState::COPY_DST);

    // Copy texture data
    auto Cmd = Context->CreateRHICommandBuffer();
    Cmd->Initialize(Context);
    Cmd->BeginCommandBuffer();
    RHIMeshTexture->CopyToTexture(Cmd.get(), Context, pixels, 4);
    RHIMeshTexture->Transition(Cmd.get(), RHIResourceState::SHADER_READ);
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
     MeshVerticesBuffer->Initialize(Context, sizeof(PTVertex) * Mesh.Vertices.size(), RHIResourceState::BUFFER_SHADER_STORAGE);
     MeshVerticesBuffer->CopyToBuffer(Context, Mesh.Vertices.data(), (uint32_t)(Mesh.Vertices.size() * sizeof(PTVertex)));

     MeshBVHBuffer->Initialize(Context, sizeof(BVHBox<PTVertex, uint32_t>) * BVHBoxes.size(), RHIResourceState::BUFFER_SHADER_STORAGE);
     MeshBVHBuffer->CopyToBuffer(Context, BVHBoxes.data(), (uint32_t)(BVHBoxes.size() * sizeof(BVHBox<PTVertex, uint32_t>)));
     cuo.vertexCount = Mesh.Vertices.size();
 }

void PathTraceRenderer::UpdateUniformBuffer(float4x4 camToWorld, float time, RenderControl* control) {
    // sensor size 32mm
    // focal length 45mm
    // fov = 2 * atan(sensor_size/2/focal_length)
    cuo.camToWorld = camToWorld;
    cuo.time = time;

    // Check if parameters changed to reset accumulation
    if (cuo.totalIters != control->totalIters ||
        cuo.dispatchDepth != control->dispatchDepth ||
        cuo.roughness != control->roughness ||
        cuo.prob_lambert != control->prob_lambert ||
        cuo.enableNEE != (control->enableNEE ? 1 : 0)) 
    {
        cuo.frameId = 0;
    }

    cuo.totalIters = control->totalIters;
    cuo.dispatchDepth = control->dispatchDepth;
    cuo.roughness = control->roughness;
    cuo.prob_lambert = control->prob_lambert;
    cuo.enableNEE = control->enableNEE ? 1 : 0;

    if (control->isClick)
    {
        cuo.frameId = 0;
    }
    if (control->bClear) {
        cuo.frameId = 0;
        control->bClear = false;
    }
}

void PathTraceRenderer::Render(float4 ViewPos, RenderControl* control) {
    ImGUI->UpdateUI(pFuncImDraw);
    cuo.screenres.r = Swapchain->GetFrameSize().Width;
    cuo.screenres.g = Swapchain->GetFrameSize().Height;
    control->currentFrame = cuo.frameId;
    CameraUniform->CopyToBuffer(Context, &cuo, sizeof(CameraUniformObject));
    StorageBuffer->CopyToBuffer(Context, &cuo, sizeof(CameraUniformObject));
    IRHIFrameBuffer* FrameBuffer = nullptr;
    std::unique_ptr<IRHICommandBuffer> CommandBuffer;
    CommandBuffer = Context->CreateRHICommandBuffer();
    CommandBuffer->Initialize(Context);
    CommandBuffer->BeginCommandBuffer();
    Swapchain->AcquireFrame(Context, FrameBuffer, PresentPass.get());
    if (FrameBuffer == nullptr)
    {
        return;
    }

    if (FrameSize != Swapchain->GetFrameSize())
    {
        // recreate frame resources
        RHIScreenBuffer1->Cleanup(Context);
        RHIScreenBuffer2->Cleanup(Context);
        RHIScreenBufferDepth->Cleanup(Context);
        RHIStoreImage->Cleanup(Context);
        FBuffer1->Cleanup(Context);
        FBuffer2->Cleanup(Context);
        RayStateBuffer->Cleanup(Context);

        ImageExtent3D ext = Swapchain->GetFrameSize();
        FrameSize = ext;
        RHIScreenBuffer1->Initialize(Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        RHIScreenBuffer2->Initialize(Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        RHIScreenBufferDepth->Initialize(Context, ext, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
        RHIStoreImage->Initialize(Context, ext, RHIFormat::R32G32B32A32_SFLOAT, RHIResourceState::SHADER_WRITE | RHIResourceState::SHADER_READ, 1);
        RayStateBuffer->Initialize(Context, sizeof(RayObject) * FrameSize.Width * FrameSize.Height, RHIResourceState::BUFFER_SHADER_STORAGE);
        std::vector<IRHIImageResource*> ColorRT1 = { RHIScreenBuffer1.get() };
        std::vector<IRHIImageResource*> ColorRT2 = { RHIScreenBuffer2.get() };
        FBuffer1->Initialize(Context, PTRenderPass.get(), ColorRT1, RHIScreenBufferDepth.get());
        FBuffer2->Initialize(Context, PTRenderPass.get(), ColorRT2, RHIScreenBufferDepth.get());
    }

    if (control->Recompile) {
        Pipeline.Compile(Context, PTRenderPass.get());
        PostPipeline.Compile(Context, PresentPass.get());
        TestCompPipeline.Compile(Context, std::nullopt);
        IterationPipeline.Compile(Context, std::nullopt);
        control->Recompile = false;
    }
    if (cuo.frameId < control->maxFrames || control->maxFrames < 0) {
        RHIStoreImage->Transition(CommandBuffer.get(), RHIResourceState::SHADER_WRITE);
        if (control->useSinglePass) {
            TestCompPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, RHIStoreImage.get());
            TestCompPipeline.PipelineObject->SetStorageBuffer(StorageBuffer.get(), 1);
            TestCompPipeline.PipelineObject->SetStorageBuffer(PrimitiveBuffer.get(), 2);
            TestCompPipeline.PipelineObject->SetStorageBuffer(MeshVerticesBuffer.get(), 3);
            TestCompPipeline.PipelineObject->SetStorageBuffer(MeshBVHBuffer.get(), 4);
            TestCompPipeline.PipelineObject->SetBindingResource(5, DescriptorType::SAMPLER2D, RHIMeshTexture.get());
            TestCompPipeline.PipelineObject->Dispatch(CommandBuffer.get(), (FrameSize.Width + 15) / 16, (FrameSize.Height + 15) / 16, 1);
        }
        else {
            // Multi-pass dispatch with batching
            for (int i = 0; i < cuo.totalIters; ++i) {
                IterationPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, RHIStoreImage.get());
                IterationPipeline.PipelineObject->SetStorageBuffer(StorageBuffer.get(), 1);
                IterationPipeline.PipelineObject->SetStorageBuffer(PrimitiveBuffer.get(), 2);
                IterationPipeline.PipelineObject->SetStorageBuffer(MeshVerticesBuffer.get(), 3);
                IterationPipeline.PipelineObject->SetStorageBuffer(MeshBVHBuffer.get(), 4);
                IterationPipeline.PipelineObject->SetBindingResource(5, DescriptorType::SAMPLER2D, RHIMeshTexture.get());
                IterationPipeline.PipelineObject->SetStorageBuffer(RayStateBuffer.get(), 6);
                IterationPipeline.PipelineObject->Dispatch(CommandBuffer.get(), (FrameSize.Width + 15) / 16, (FrameSize.Height + 15) / 16, 1);

                // Note: In some RHIs, we might need a memory barrier here between dispatches for RWStructuredBuffer
                // But if they are accessing unique locations (per-pixel), it might be safe without explicit barrier if RHI handles it.
            }
        }
        cuo.frameId++;
    }
    RHIStoreImage->Transition(CommandBuffer.get(), RHIResourceState::SHADER_READ);
    PresentPass->BeginRenderPass(CommandBuffer.get(), FrameBuffer);
    // Comment this line if you don't want ImGUI
    if (!control->RenderingPaused && !control->ShaderCompileHasError) {
        PostPipeline.PipelineObject->SetUniform(CameraUniform.get(), 1);
        PostPipeline.PipelineObject->SetImageSampler(RHIStoreImage.get(), 0);
        PostPipeline.PipelineObject->BindIndexBuffer(RHIFullScreenQuadIndexBuffer.get(), 0);
        PostPipeline.PipelineObject->BindVertexBuffer(RHIFullScreenQuadBuffer.get(), 0, 0);
        IndexBufferSize = 6;
        PostPipeline.PipelineObject->Draw(CommandBuffer.get(), IndexBufferSize, 0, 1);
    }
    ImGUI->DispatchImGUI(CommandBuffer.get());
    PresentPass->EndRenderPass(CommandBuffer.get());
    CommandBuffer->EndCommandBuffer();
    TestCompPipeline.PipelineObject->CopyDescriptors(Context);
    IterationPipeline.PipelineObject->CopyDescriptors(Context);
    PostPipeline.PipelineObject->CopyDescriptors(Context);
    Swapchain->PresentFrameAndRelease(Context, CommandBuffer.get());
    Context->WaitDeviceIdle();
    swap = !swap;
}

 void PathTraceRenderer::ProcessInput()
{
    Context->ProcessFrameInput();
}

void PathTraceRenderer::CaptureFrame(const std::string& Path) {
    uint32_t width = FrameSize.Width;
    uint32_t height = FrameSize.Height;
    std::vector<float> pixels(width * height * 4);
    
    RHIStoreImage->CopyFromTexture(nullptr, Context, pixels.data(), sizeof(float) * 4);

    std::vector<uint8_t> rgba(width * height * 4);
    for (size_t i = 0; i < pixels.size(); i += 4) {
        float r = pixels[i];
        float g = pixels[i+1];
        float b = pixels[i+2];
        
        // Simple gamma correction
        r = pow(glm::clamp(r, 0.0f, 1.0f), 1.0f / 2.2f);
        g = pow(glm::clamp(g, 0.0f, 1.0f), 1.0f / 2.2f);
        b = pow(glm::clamp(b, 0.0f, 1.0f), 1.0f / 2.2f);

        rgba[i]   = (uint8_t)(r * 255.0f);
        rgba[i+1] = (uint8_t)(g * 255.0f);
        rgba[i+2] = (uint8_t)(b * 255.0f);
        rgba[i+3] = 255;
    }
    
    stbi_write_png(Path.c_str(), width, height, 4, rgba.data(), width * 4);
}
