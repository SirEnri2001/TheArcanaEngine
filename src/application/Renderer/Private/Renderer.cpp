#define RENDERER_IMPLEMENT
#include "Renderer.h"

#include <chrono>
#include <CoreLog.inl>

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

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


void Renderer::Initialize(RendererContext* Context, std::vector<char> VS, std::vector<char> PS)
{
    Pipeline.SetShaders(VS, PS);
    Pipeline.AddBinding(0, sizeof(Mesh::VertexType));
    Pipeline.AddLayout(0, 0, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
    Pipeline.AddLayout(0, 1, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
    Pipeline.AddLayout(0, 2, R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
    Pipeline.AddLayout(0, 3, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));
    Pipeline.Initialize(&Context->Context, &Context->RenderPass);
    GraphicDispatcher.Initialize(&Context->Context);
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
    uint32_t ImageIndex;
    GraphicDispatcher.PrepareRenderPass(&Context, &RContext->WindowManager, &RContext->RenderPass, ImageIndex);
    GraphicDispatcher.BeginRenderPass(&Context, &RContext->WindowManager, &RContext->RenderPass, ImageIndex);

    for (auto& MeshProxy : MeshProxyPasses)
    {
        Pipeline.SetImageSamplerBinding(&MeshProxy->Texture, 1);
        GraphicDispatcher.BindIndexBuffer(&MeshProxy->RHIIndexBuffer, 0);
        GraphicDispatcher.BindVertexBuffer(&MeshProxy->RHIVertexBuffer, 0, 0);
        IndexBufferSize = MeshProxy->IndexBufferSize;
        GraphicDispatcher.Dispatch(&RContext->WindowManager, &Pipeline, IndexBufferSize, 0, 1);
    }
    // Comment this line if you don't want ImGUI
    RContext->ImGUI.DispatchImGUI(&GraphicDispatcher);

    GraphicDispatcher.EndRenderPass();
    GraphicDispatcher.Submit(&Context, &RContext->WindowManager, ImageIndex);
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
	RenderPass.Initialize(&Context, &WindowManager);
    ImGUI.Initialize(&Context, &WindowManager, &RenderPass);
}

bool RendererContext::IsWindowAlive()
{
    return WindowManager.IsAlive();
}


RendererContext* RendererContext::GInstance = nullptr;