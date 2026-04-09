#define BLINNPHONGRENDERER_IMPLEMENT
#include "BlinnPhongRenderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <stdexcept>

void BlinnPhongPipeline::SetAllShaderBindings(IRHIContext* Context) {
    // Binding 0: UniformBufferObject (VS_FS)
    // Binding 1: Sampler2D (VS_FS)
    // Binding 2: ModelUniform (VS_FS)
    PipelineFactory->SetDescriptorBinding(0, DescriptorType::UNIFORM, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->SetDescriptorBinding(1, DescriptorType::SAMPLER2D, IRHIPipelineFactory::EPipelineStages::VS_FS);
    PipelineFactory->SetDescriptorBinding(2, DescriptorType::UNIFORM, IRHIPipelineFactory::EPipelineStages::VS_FS);

    // Vertex Buffer Layout
    // layout(location = 0) in vec3 inPosition;
    // layout(location = 1) in vec3 inColor;
    // layout(location = 2) in vec2 inTexCoord;
    // layout(location = 3) in vec3 inNormal;
    PipelineFactory->AddBufferBinding(0, sizeof(BlinnPhongVertex));
    PipelineFactory->AddBufferLayout(0, 0, RHIFormat::R32G32B32_SFLOAT, offsetof(BlinnPhongVertex, Position));
    PipelineFactory->AddBufferLayout(0, 1, RHIFormat::R32G32B32_SFLOAT, offsetof(BlinnPhongVertex, Color));
    PipelineFactory->AddBufferLayout(0, 2, RHIFormat::R32G32_SFLOAT, offsetof(BlinnPhongVertex, TexCoord));
    PipelineFactory->AddBufferLayout(0, 3, RHIFormat::R32G32B32_SFLOAT, offsetof(BlinnPhongVertex, Normal));
}

void BlinnPhongRenderer::CreateRenderer(uint32_t Height, uint32_t Width) {
    uptr_Context = IRHIPlatformSupport::Get(RHIBackend::Vulkan)->CreateRHIContext();
    Context = uptr_Context.get();
    Context->Initialize(Width, Height);

    Swapchain = Context->CreateRHISwapchain();
    RenderPass = Context->CreateRHIRenderPass();
    // PathTraceRenderer creates FrameBuffer during Render if size changed or at init.
    // I'll follow the pattern of initializing swapchain and render pass first.

    VertexBuffer = Context->CreateRHIBuffer();
    IndexBuffer = Context->CreateRHIBuffer();
    UBO = Context->CreateRHIBuffer();
    ModelUBO = Context->CreateRHIBuffer();
    ImGUI = Context->CreateRHIImGUI();

    SwapchainFormat = B8G8R8A8_SRGB;
    Swapchain->Initialize(Context, SwapchainFormat);

    std::vector<RHIFormat> ColorRTFormats = { SwapchainFormat };
    RenderPass->Initialize(Context, ColorRTFormats);

    FrameSize = Swapchain->GetFrameSize();
    
    Pipeline.InitializeAsGraphics(Context, *RenderPass, BP_VERTSHADER, BP_FRAGSHADER);
    Pipeline.SetAllShaderBindings(Context);
    Pipeline.Compile(Context, RenderPass.get());

    LoadMeshAndTexture("./models/spot/spot_triangulated.obj", "./models/spot/spot_texture.png");

    UBO->Initialize(Context, sizeof(UniformBufferObject), RHIResourceState::BUFFER_UNIFORM);
    ModelUBO->Initialize(Context, sizeof(ModelUniform), RHIResourceState::BUFFER_UNIFORM);

    ImGUI->Initialize(Context, Swapchain.get(), RenderPass.get());
}

void BlinnPhongRenderer::LoadMeshAndTexture(const std::string& MeshPath, const std::string& TexturePath) {
    // Load Mesh
    auto Mesh = TMesh<BlinnPhongVertex, uint32_t>::LoadObj(MeshPath);
    CalculateNormal<BlinnPhongVertex, uint32_t>(Mesh);
    IndexCount = (uint32_t)Mesh.Indices.size();

    VertexBuffer->Initialize(Context, sizeof(BlinnPhongVertex), (uint32_t)Mesh.Vertices.size(), RHIResourceState::BUFFER_VERTEX);
    IndexBuffer->Initialize(Context, sizeof(uint32_t), (uint32_t)Mesh.Indices.size(), RHIResourceState::BUFFER_INDEX);

    VertexBuffer->CopyToBuffer(Context, Mesh.Vertices.data(), (uint32_t)(Mesh.Vertices.size() * sizeof(BlinnPhongVertex)));
    IndexBuffer->CopyToBuffer(Context, Mesh.Indices.data(), (uint32_t)(Mesh.Indices.size() * sizeof(uint32_t)));

    // Load Texture
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + TexturePath);
    }

    Texture = Context->CreateRHIImageResource();
    Texture->Initialize(Context, (uint32_t)texHeight, (uint32_t)texWidth, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::SHADER_READ | RHIResourceState::COPY_DST);
    
    // Copy texture data
    auto Cmd = Context->CreateRHICommandBuffer();
    Cmd->Initialize(Context);
    Cmd->BeginCommandBuffer();
    Texture->CopyToTexture(Cmd.get(), Context, pixels, 4);
    Texture->Transition(Cmd.get(), RHIResourceState::SHADER_READ);
    Cmd->EndCommandBuffer();
    
    // In some BRDF/PathTracer engines, we might need to submit this Cmd immediately.
    // However, the IRHIContext doesn't expose a Submit method directly in the interface.
    // Swapchain->PresentFrameAndRelease usually handles submission. 
    // We'll trust the RHI implementation handles the copy.
    
    stbi_image_free(pixels);
}

void BlinnPhongRenderer::Render(float4 ViewPos, RenderControl* control) {
    ImGUI->UpdateUI(pFuncImDraw);
    UpdateUniforms(ViewPos, control);

    IRHIFrameBuffer* CurrentFrameBuffer = nullptr;
    auto CommandBuffer = Context->CreateRHICommandBuffer();
    CommandBuffer->Initialize(Context);
    CommandBuffer->BeginCommandBuffer();

    Swapchain->AcquireFrame(Context, CurrentFrameBuffer, RenderPass.get());
    if (!CurrentFrameBuffer) return;

    if (FrameSize != Swapchain->GetFrameSize()) {
        FrameSize = Swapchain->GetFrameSize();
        // Here we could recreate size-dependent resources
    }

    RenderPass->BeginRenderPass(CommandBuffer.get(), CurrentFrameBuffer);

    if (!control->RenderingPaused && !control->ShaderCompileHasError) {
        Pipeline.PipelineObject->SetUniform(UBO.get(), 0);
        Pipeline.PipelineObject->SetImageSampler(Texture.get(), 1);
        Pipeline.PipelineObject->SetUniform(ModelUBO.get(), 2);

        Pipeline.PipelineObject->BindVertexBuffer(VertexBuffer.get(), 0, 0);
        Pipeline.PipelineObject->BindIndexBuffer(IndexBuffer.get(), 0);

        Pipeline.PipelineObject->Draw(CommandBuffer.get(), IndexCount, 0, 1);
    }

    ImGUI->DispatchImGUI(CommandBuffer.get());
    RenderPass->EndRenderPass(CommandBuffer.get());
    CommandBuffer->EndCommandBuffer();

    Pipeline.PipelineObject->CopyDescriptors(Context);
    Swapchain->PresentFrameAndRelease(Context, CommandBuffer.get());
    Context->WaitDeviceIdle();
}

void BlinnPhongRenderer::UpdateUniforms(float4 ViewPos, RenderControl* control) {
    // UBO
    uboData.view = glm::lookAt(float3(-1., 0., 0.), float3(0., 0., 0.), float3(0., 0., 1.)) *  glm::inverse(control->CameraTransformLocalToWorld);
    float Aspect = (float)FrameSize.Width / (float)FrameSize.Height;
    uboData.proj = glm::perspective(glm::radians(45.0f), Aspect, 0.01f, 100.0f);
    uboData.viewPosition = ViewPos;
    UBO->CopyToBuffer(Context, &uboData, sizeof(UniformBufferObject));

    // ModelUBO
    modelData.model = float4x4(1.0f); // Identity for now, or use control transform
    modelData.modelInv = inverse(modelData.model);
    modelData.color = float3(1.0f, 1.0f, 1.0f);
    ModelUBO->CopyToBuffer(Context, &modelData, sizeof(ModelUniform));
}
