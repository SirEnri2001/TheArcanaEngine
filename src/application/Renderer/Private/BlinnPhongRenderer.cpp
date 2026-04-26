#define BLINNPHONGRENDERER_IMPLEMENT
#include "BlinnPhongRenderer.h"

#define STB_IMAGE_IMPLEMENTATION
 #include <stb_image.h>
#include <stb_image_write.h>

#include <iostream>
#include <stdexcept>

#include "imgui.h"

void BlinnPhongRenderer::CreateResource() {
    VertexBuffer = Env.Context->CreateRHIBuffer();
    IndexBuffer = Env.Context->CreateRHIBuffer();
    UBO = Env.Context->CreateRHIBuffer();
    ModelUBO = Env.Context->CreateRHIBuffer();

    Pipeline.InitializeAsGraphics(Env.Context, *Env.RenderPass, BP_VERTSHADER, BP_FRAGSHADER, sizeof(BlinnPhongVertex));
    Pipeline.Compile(Env.Context, Env.RenderPass);

    LoadMeshAndTexture("./models/spot/spot_triangulated.obj", "./models/spot/spot_texture.png");

    UBO->Initialize(Env.Context, sizeof(UniformBufferObject), RHIResourceState::BUFFER_UNIFORM);
    ModelUBO->Initialize(Env.Context, sizeof(ModelUniform), RHIResourceState::BUFFER_UNIFORM);
}

void BlinnPhongRenderer::LoadMeshAndTexture(const std::string& MeshPath, const std::string& TexturePath) {
    // Load Mesh
    auto Mesh = TMesh<BlinnPhongVertex, uint32_t>::LoadObj(MeshPath);
    CalculateNormal<BlinnPhongVertex, uint32_t>(Mesh);
    IndexCount = (uint32_t)Mesh.Indices.size();

    VertexBuffer->Initialize(Env.Context, sizeof(BlinnPhongVertex), (uint32_t)Mesh.Vertices.size(), RHIResourceState::BUFFER_VERTEX);
    IndexBuffer->Initialize(Env.Context, sizeof(uint32_t), (uint32_t)Mesh.Indices.size(), RHIResourceState::BUFFER_INDEX);

    VertexBuffer->CopyToBuffer(Env.Context, Mesh.Vertices.data(), (uint32_t)(Mesh.Vertices.size() * sizeof(BlinnPhongVertex)));
    IndexBuffer->CopyToBuffer(Env.Context, Mesh.Indices.data(), (uint32_t)(Mesh.Indices.size() * sizeof(uint32_t)));

    // Load Texture
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + TexturePath);
    }

    Texture = Env.Context->CreateRHIImageResource();
    Texture->Initialize(Env.Context, (uint32_t)texHeight, (uint32_t)texWidth, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::SHADER_READ | RHIResourceState::COPY_DST);
    
    // Copy texture data
    auto Cmd = Env.Context->CreateRHICommandBuffer();
    Cmd->Initialize(Env.Context);
    Cmd->BeginCommandBuffer();
    Texture->CopyToTexture(Cmd.get(), Env.Context, pixels, 4);
    Texture->Transition(Cmd.get(), RHIResourceState::SHADER_READ);
    Cmd->EndCommandBuffer();
    
    // In some BRDF/PathTracer engines, we might need to submit this Cmd immediately.
    // However, the IRHIContext doesn't expose a Submit method directly in the interface.
    // Swapchain->PresentFrameAndRelease usually handles submission. 
    // We'll trust the RHI implementation handles the copy.
    
    stbi_image_free(pixels);
}

bool BlinnPhongRenderer::BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) {
    UpdateUniforms(float4(1.0f), control); // We'll just pass a dummy ViewPos or use control's lookat
    return BaseRenderer::BeginRender(CommandBuffer, OutFrameBuffer, control);
}

void BlinnPhongRenderer::DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) {
    Env.RenderPass->BeginRenderPass(CommandBuffer, FrameBuffer);

    if (!control->RenderingPaused && !control->ShaderCompileHasError) {
        Pipeline.PipelineObject->SetUniform(UBO.get(), 0);
        Pipeline.PipelineObject->SetImageSampler(Texture.get(), 1);
        Pipeline.PipelineObject->SetUniform(ModelUBO.get(), 2);

        Pipeline.PipelineObject->BindVertexBuffer(VertexBuffer.get(), 0, 0);
        Pipeline.PipelineObject->BindIndexBuffer(IndexBuffer.get(), 0);

        Pipeline.PipelineObject->Draw(CommandBuffer, IndexCount, 0, 1);
    }

    if (Env.ImGUI) {
        Env.ImGUI->DispatchImGUI(CommandBuffer);
    }
    Env.RenderPass->EndRenderPass(CommandBuffer);
}

void BlinnPhongRenderer::EndRender(IRHICommandBuffer* CommandBuffer) {
    Pipeline.PipelineObject->CopyDescriptors(Env.Context);
    BaseRenderer::EndRender(CommandBuffer);
}

void BlinnPhongRenderer::UpdateUniforms(float4 ViewPos, RenderControl* control) {
    // UBO
    uboData.view = glm::lookAt(float3(-1., 0., 0.), float3(0., 0., 0.), float3(0., 0., 1.)) *  glm::inverse(control->CameraTransformLocalToWorld);
    float Aspect = (float)Env.FrameSize.Width / (float)Env.FrameSize.Height;
    uboData.proj = glm::perspective(glm::radians(45.0f), Aspect, 0.01f, 100.0f);
    uboData.viewPosition = ViewPos;
    UBO->CopyToBuffer(Env.Context, &uboData, sizeof(UniformBufferObject));

    // ModelUBO
    modelData.model = float4x4(1.0f); // Identity for now, or use control transform
    modelData.modelInv = inverse(modelData.model);
     modelData.color = float3(1.0f, 1.0f, 1.0f);
    ModelUBO->CopyToBuffer(Env.Context, &modelData, sizeof(ModelUniform));
}

void BlinnPhongRenderer::DrawImGUI()
{
    static bool check = false;
    ImGui::Begin("Blinn Phong");
    ImGui::Checkbox("Check", &check);
    ImGui::End();
}