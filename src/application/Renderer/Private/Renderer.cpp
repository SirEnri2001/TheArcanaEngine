#define RENDERER_IMPLEMENT
#include "Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <chrono>
#include <CoreLog.inl>

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

struct FullScreenVertex
{
	
};

void MeshRenderProxy::Initialize(RendererContext* Context, Mesh& InMesh)
{
	RHIVertexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::VERTEX);
	RHIIndexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::INDEX);
	RHIVertexBuffer.CopyToBuffer(&Context->Context, InMesh.Vertices.data(), InMesh.Vertices.size() * sizeof(Mesh::VertexType));
	RHIIndexBuffer.CopyToBuffer(&Context->Context, InMesh.Indices.data(), InMesh.Indices.size() * sizeof(uint32_t));
    int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(InMesh.TexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	assert(texHeight > 0 && texWidth > 0);
    Texture.Initialize(&Context->Context, texHeight, texWidth, RHIFormat::R8G8B8A8_SRGB);
	Texture.CopyToTexture(&Context->Context, pixels, 4);
    IndexBufferSize = InMesh.Indices.size();
	stbi_image_free(pixels);
}

MeshRenderProxy::~MeshRenderProxy()
{
    Log("Release Render Proxy");
}


void Renderer::Initialize(RendererContext* Context, std::vector<char> VS, std::vector<char> PS, std::vector<char> VS1, std::vector<char> PS1)
{
    GBufferA.InitializeRenderTarget(&Context->Context, &Context->WindowManager,
        { Context->WindowManager.GetWindowWidth(), Context->WindowManager.GetWindowHeight(), 1 }, IU_COLOR_RT);
    GBufferD.InitializeRenderTarget(&Context->Context, &Context->WindowManager,
        { Context->WindowManager.GetWindowWidth(), Context->WindowManager.GetWindowHeight(), 1 }, IU_DEPTH_RT);
    RenderPass.AddColorRenderTarget(&GBufferA);
    RenderPass.SetDepthRenderTarget(&GBufferD);
    RenderPass.Initialize(&Context->Context, Context->WindowManager.GetWindowWidth(), Context->WindowManager.GetWindowHeight());
    Context->WindowManager.InitializeRenderPassAsPresent(&PresentPass, &Context->Context);
    Context->ImGUI.Initialize(&Context->Context, &Context->WindowManager, &PresentPass);
    PipelineFactory.SetShaders(VS, PS);
    PipelineFactory.SetUniformBinding(0);
    PipelineFactory.SetImageSamplerBinding(1);
    PipelineFactory.AddBufferBinding(0, sizeof(Mesh::VertexType));
    PipelineFactory.AddBufferLayout(0, 0, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
    PipelineFactory.AddBufferLayout(0, 1, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
    PipelineFactory.AddBufferLayout(0, 2, R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
    PipelineFactory.AddBufferLayout(0, 3, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));
    PipelineFactory.InitializePipelineObject(&PipelineObject, &Context->Context, &RenderPass);

    PipelineFactory.RemoveAllGlobalBindings();
    PipelineFactory.RemoveAllBufferBindings();
    PipelineFactory.SetImageSamplerBinding(0);
    PipelineFactory.SetShaders(VS1, PS1);
    PipelineFactory.AddBufferBinding(0, sizeof(float3));
    PipelineFactory.AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
    PipelineFactory.InitializePipelineObject(&PresentPipelineObject, &Context->Context, &PresentPass);

    GraphicDispatcher.Initialize(&Context->Context);

    RHIFullScreenQuadBuffer.Initialize(&Context->Context, sizeof(float3), 4, BufferType::VERTEX);
    RHIFullScreenQuadIndexBuffer.Initialize(&Context->Context, sizeof(uint32_t), 6, BufferType::INDEX);
    float3 FullScreenVertices[4] = {float3(-0.5, -0.5, 0.5), float3(-0.5, 0.5, 0.5), float3(0.5, 0.5, 0.5), float3(0.5, -0.5, 0.5)};
    uint32_t FullScreenVerticesIndex[6] = {0, 1, 2, 0, 2, 3};
    RHIFullScreenQuadBuffer.CopyToBuffer(&Context->Context, FullScreenVertices, sizeof(float3) * 4);
    RHIFullScreenQuadIndexBuffer.CopyToBuffer(&Context->Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
}

void Renderer::SetUniform(RHIUniform* InUniform, uint32_t Binding)
{
    Uniform = InUniform;
}

void Renderer::SetTextureSampler(RHIImageResource* Texture, uint32_t Binding)
{
    //PipelineFactory.SetImageSamplerBinding(Binding);
}


void Renderer::DrawScene(RendererContext* Context, MeshRenderProxy& InMeshProxy)
{
    MeshProxyPasses.push_back(&InMeshProxy);
}

void Renderer::UpdateFrame(RendererContext* RContext)
{
    auto& Context = RContext->Context;
    RContext->ImGUI.UpdateUI(pFuncImDraw);
    GraphicDispatcher.WaitForGPUIdle(&Context);
    GraphicDispatcher.BeginFrame(&Context, &RContext->WindowManager, &PresentPass);
    GraphicDispatcher.BeginRenderPass(&RenderPass);
    for (auto& MeshProxy : MeshProxyPasses)
    {
		PipelineObject.SetUniform(Uniform, 0);
        PipelineObject.SetImageSampler(&MeshProxy->Texture, 1);
        GraphicDispatcher.BindIndexBuffer(&MeshProxy->RHIIndexBuffer, 0);
        GraphicDispatcher.BindVertexBuffer(&MeshProxy->RHIVertexBuffer, 0, 0);
        IndexBufferSize = MeshProxy->IndexBufferSize;
        GraphicDispatcher.Dispatch(&PipelineObject, IndexBufferSize, 0, 1);
    }
    GraphicDispatcher.EndRenderPass(&RenderPass);

    PresentPipelineObject.SetImageSampler(&GBufferA, 0);
    GraphicDispatcher.BeginRenderPass(&PresentPass);
    GraphicDispatcher.BindIndexBuffer(&RHIFullScreenQuadIndexBuffer, 0);
    GraphicDispatcher.BindVertexBuffer(&RHIFullScreenQuadBuffer, 0, 0);
    GraphicDispatcher.Dispatch(&PresentPipelineObject, 6, 0, 1);
    // Comment this line if you don't want ImGUI
    RContext->ImGUI.DispatchImGUI(&GraphicDispatcher);
    GraphicDispatcher.EndRenderPass(&PresentPass);
    GraphicDispatcher.EndFrameAndSubmit(&Context, &RContext->WindowManager);
}


RendererContext* RendererContext::Get()
{
	if (GInstance==nullptr)
	{
		GInstance = new RendererContext();
	}
	return GInstance;
}

void RendererContext::Initialize(int Width, int Height)
{
	if (bInitialized)
	{
		return;
	}
    RHIPlatformSupport::Get()->Initialize();
    WindowManager.Initialize(RHIPlatformSupport::Get(), Height, Width);
    RHIPlatformSupport::Get()->InitializePhysicalDevice(&WindowManager);
	Context.Initialize(RHIPlatformSupport::Get());
    WindowManager.InitializeSwapchain(&Context, RHIPlatformSupport::Get());
    ColorRenderTarget.InitializeRenderTarget(&Context, &WindowManager, {WindowManager.GetWindowWidth(), WindowManager.GetWindowHeight(), 1}, IU_COLOR_PRESENT_RT, 4);
    DepthRenderTarget.InitializeRenderTarget(&Context, &WindowManager, {WindowManager.GetWindowWidth(), WindowManager.GetWindowHeight(), 1}, IU_DEPTH_RT, 4);
	//PresentPass.Initialize(&Context, &WindowManager, 4, &ColorRenderTarget, &DepthRenderTarget);
}

bool RendererContext::IsWindowAlive()
{
    return WindowManager.IsAlive();
}


RendererContext* RendererContext::GInstance = nullptr;