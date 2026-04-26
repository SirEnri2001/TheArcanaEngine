#include <iostream>
#include <fstream>

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

    cuo.frameId = 1;
    cuo.totalIters = 1;
    cuo.dispatchDepth = 4;
    cuo.roughness = 0.3f;
    cuo.prob_lambert = 0.5f;
    cuo.enableNEE = 1;
    LoadMesh("./models/spot/spot_triangulated.obj");

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

bool PathTraceRenderer::BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) {
    cuo.screenres.r = Env.Swapchain->GetFrameSize().Width;
    cuo.screenres.g = Env.Swapchain->GetFrameSize().Height;
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
            if (control->useSinglePass) {
                PTResource.TestCompPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, PTResource.RHIStoreImage.get());
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.StorageBuffer.get(), 1);
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.PrimitiveBuffer.get(), 2);
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshVerticesBuffer.get(), 3);
                PTResource.TestCompPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshBVHBuffer.get(), 4);
                PTResource.TestCompPipeline.PipelineObject->SetBindingResource(5, DescriptorType::SAMPLER2D, PTResource.RHIMeshTexture.get());
                PTResource.TestCompPipeline.PipelineObject->Dispatch(CommandBuffer, (Env.FrameSize.Width + 15) / 16, (Env.FrameSize.Height + 15) / 16, 1);
            } else {
                for (int i = 0; i < cuo.totalIters; ++i) {
                    PTResource.IterationPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, PTResource.RHIStoreImage.get());
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.StorageBuffer.get(), 1);
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.PrimitiveBuffer.get(), 2);
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshVerticesBuffer.get(), 3);
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.MeshBVHBuffer.get(), 4);
                    PTResource.IterationPipeline.PipelineObject->SetBindingResource(5, DescriptorType::SAMPLER2D, PTResource.RHIMeshTexture.get());
                    PTResource.IterationPipeline.PipelineObject->SetStorageBuffer(PTResource.RayStateBuffer.get(), 6);
                    PTResource.IterationPipeline.PipelineObject->Dispatch(CommandBuffer, (Env.FrameSize.Width + 15) / 16, (Env.FrameSize.Height + 15) / 16, 1);
                }
            }
            cuo.frameId++;
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
    //uint32_t width = FrameSize.Width;
    //uint32_t height = FrameSize.Height;
    //std::vector<float> pixels(width * height * 4);
    //
    //RHIStoreImage->CopyFromTexture(nullptr, Context, pixels.data(), sizeof(float) * 4);

    //std::vector<uint8_t> rgba(width * height * 4);
    //for (size_t i = 0; i < pixels.size(); i += 4) {
    //    float r = pixels[i];
    //    float g = pixels[i+1];
    //    float b = pixels[i+2];
    //    
    //    // Simple gamma correction
    //    r = pow(glm::clamp(r, 0.0f, 1.0f), 1.0f / 2.2f);
    //    g = pow(glm::clamp(g, 0.0f, 1.0f), 1.0f / 2.2f);
    //    b = pow(glm::clamp(b, 0.0f, 1.0f), 1.0f / 2.2f);

    //    rgba[i]   = (uint8_t)(r * 255.0f);
    //    rgba[i+1] = (uint8_t)(g * 255.0f);
    //    rgba[i+2] = (uint8_t)(b * 255.0f);
    //    rgba[i+3] = 255;
    //}
    //
    //stbi_write_png(Path.c_str(), width, height, 4, rgba.data(), width * 4);
}

void PathTraceRenderer::DrawImGUI()
{
    ImGui::Begin("Path Trace Parameters");
    ImGui::Text("Path Tracing Parameters");
    
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
        cuo.frameId = 0; // reset accumulation
    }

    ImGui::End();
}
