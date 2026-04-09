#include <iostream>
#include <fstream>

#define PATHTRACERENDERER_IMPLEMENT
#include "PathTraceRenderer.h"
#define SHADERCOMPILER_INCLUDE
#include "ShaderCompiler.h"

void ComputePipeline::SetAllShaderBindings(IRHIContext* Context) {
    PipelineFactory->SetDescriptorBinding(0, DescriptorType::IMAGE2D, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(1, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
    PipelineFactory->SetDescriptorBinding(2, DescriptorType::STORAGE_READONLY, IRHIPipelineFactory::EPipelineStages::CS);
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

void PathTraceRenderer::CreateRenderer(uint32_t Height, uint32_t Width) {
    uptr_Context = IRHIPlatformSupport::Get(RHIBackend::D3D12)->CreateRHIContext();
    Context = uptr_Context.get();
    Context->Initialize(Width, Height);
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
    Pipeline.SetAllShaderBindings(Context);
    PostPipeline.SetAllShaderBindings(Context);
    TestCompPipeline.SetAllShaderBindings(Context);
    Pipeline.Compile(Context, PTRenderPass.get());
    PostPipeline.Compile(Context, PresentPass.get());
    TestCompPipeline.Compile(Context, std::nullopt);

    RHIFullScreenQuadBuffer->Initialize(Context, sizeof(float3) * 8, RHIResourceState::BUFFER_VERTEX);
    RHIFullScreenQuadIndexBuffer->Initialize(Context, sizeof(uint32_t) * 6, RHIResourceState::BUFFER_INDEX);
    float3 FullScreenVertices[4] = { float3(-1., -1., 0.), float3(-1., 1., 0.), float3(1., 1., 0.), float3(1., -1., 0.) };
    uint32_t FullScreenVerticesIndex[6] = { 0, 1, 2, 0, 2, 3 };
    RHIFullScreenQuadBuffer->CopyToBuffer(Context, FullScreenVertices, sizeof(float3) * 8);
    RHIFullScreenQuadIndexBuffer->CopyToBuffer(Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
    CameraUniform->Initialize(Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_UNIFORM);
    StorageBuffer->Initialize(Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_SHADER_STORAGE);
    ImGUI->Initialize(Context, Swapchain.get(), PresentPass.get());

    cuo.frameId = 1;
}

void PathTraceRenderer::UpdateUniformBuffer(float4x4 camToWorld, float time, RenderControl* control) {
    // sensor size 32mm
    // focal length 45mm
    // fov = 2 * atan(sensor_size/2/focal_length)
    cuo.camToWorld = camToWorld;
    cuo.time = time;
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

    if (control->Recompile) {
        Pipeline.Compile(Context, PTRenderPass.get());
        PostPipeline.Compile(Context, PresentPass.get());
        TestCompPipeline.Compile(Context, std::nullopt);
        control->Recompile = false;
    }
    if (cuo.frameId < control->maxFrames || control->maxFrames < 0) {
        RHIStoreImage->Transition(CommandBuffer.get(), RHIResourceState::SHADER_WRITE);
        TestCompPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, RHIStoreImage.get());
        TestCompPipeline.PipelineObject->SetStorageBuffer(StorageBuffer.get(), 1);
        TestCompPipeline.PipelineObject->SetStorageBuffer(PrimitiveBuffer.get(), 2);
        TestCompPipeline.PipelineObject->Dispatch(CommandBuffer.get(), (FrameSize.Width + 15) / 16, (FrameSize.Height + 15) / 16, 1);
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
    PostPipeline.PipelineObject->CopyDescriptors(Context);
    Swapchain->PresentFrameAndRelease(Context, CommandBuffer.get());
    Context->WaitDeviceIdle();
    swap = !swap;
}

void PathTraceRenderer::ProcessInput()
{
    Context->ProcessFrameInput();
}
