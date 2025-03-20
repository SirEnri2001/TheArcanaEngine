#define RENDERER_IMPLEMENT
#include "Renderer.h"

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
	Texture.Initialize(&Context->Context, InMesh.TexturePath.c_str(), RHIFormat::R8G8B8A8_SRGB);
    IndexBufferSize = InMesh.Indices.size();
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
    Pipeline.SetShaders(VS, PS);
    Pipeline.AddBinding(0, sizeof(Mesh::VertexType));
    Pipeline.AddLayout(0, 0, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
    Pipeline.AddLayout(0, 1, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
    Pipeline.AddLayout(0, 2, R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
    Pipeline.AddLayout(0, 3, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));
    Pipeline.Initialize(&Context->Context, &RenderPass);

    PresentPipeline.SetImageSamplerBinding(&GBufferA, 0);
    PresentPipeline.SetShaders(VS1, PS1);
    PresentPipeline.AddBinding(0, sizeof(float3));
    PresentPipeline.AddLayout(0, 0, R32G32B32_SFLOAT, 0);
    PresentPipeline.Initialize(&Context->Context, &Context->PresentPass);

    GraphicDispatcher.Initialize(&Context->Context);

    RHIFullScreenQuadBuffer.Initialize(&Context->Context, sizeof(float3), 4, BufferType::VERTEX);
    RHIFullScreenQuadIndexBuffer.Initialize(&Context->Context, sizeof(uint32_t), 6, BufferType::INDEX);
    float3 FullScreenVertices[4] = {float3(-1, -1, 1), float3(-1, 1, 1), float3( 1, 1, 1), float3(1, -1, 1)};
    uint32_t FullScreenVerticesIndex[6] = {0, 1, 2, 0, 2, 3};
    RHIFullScreenQuadBuffer.CopyToBuffer(&Context->Context, FullScreenVertices, sizeof(float3) * 4);
    RHIFullScreenQuadIndexBuffer.CopyToBuffer(&Context->Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
}

void Renderer::SetUniform(RHIUniform* Uniform, uint32_t Binding)
{
    Pipeline.SetUniformBinding(Uniform, Binding);
}

void Renderer::SetTextureSampler(RHIImageResource* Texture, uint32_t Binding)
{
    Pipeline.SetImageSamplerBinding(Texture, Binding);
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
    GraphicDispatcher.BeginFrame();
    GraphicDispatcher.BeginRenderPass(&Context, &RenderPass);
    for (auto& MeshProxy : MeshProxyPasses)
    {
        Pipeline.SetImageSamplerBinding(&MeshProxy->Texture, 1);
        GraphicDispatcher.BindIndexBuffer(&MeshProxy->RHIIndexBuffer, 0);
        GraphicDispatcher.BindVertexBuffer(&MeshProxy->RHIVertexBuffer, 0, 0);
        IndexBufferSize = MeshProxy->IndexBufferSize;
        GraphicDispatcher.Dispatch(&RContext->WindowManager, &Pipeline, IndexBufferSize, 0, 1);
    }
    GraphicDispatcher.EndRenderPass(&RenderPass);

    PresentPipeline.SetImageSamplerBinding(&GBufferA, 0);
    GraphicDispatcher.BeginPresentPass(&Context, &RContext->WindowManager, &RContext->PresentPass);
    GraphicDispatcher.BindIndexBuffer(&RHIFullScreenQuadIndexBuffer, 0);
    GraphicDispatcher.BindVertexBuffer(&RHIFullScreenQuadBuffer, 0, 0);
    GraphicDispatcher.Dispatch(&RContext->WindowManager, &PresentPipeline, 6, 0, 1);
    // Comment this line if you don't want ImGUI
    RContext->ImGUI.DispatchImGUI(&GraphicDispatcher);
    GraphicDispatcher.EndPresentPassAndSubmit(&Context, &RContext->WindowManager);
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
	PresentPass.Initialize(&Context, &WindowManager, 4, &ColorRenderTarget, &DepthRenderTarget);
    ImGUI.Initialize(&Context, &WindowManager, &PresentPass);
}

bool RendererContext::IsWindowAlive()
{
    return WindowManager.IsAlive();
}


RendererContext* RendererContext::GInstance = nullptr;