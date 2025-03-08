#pragma once

#ifdef RENDERER_IMPLEMENT
#define RENDERER_API __declspec(dllexport)
#else
#ifdef RENDERER_INCLUDE
#define RENDERER_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define RENDERER_API
#endif
#endif

#define RHI_INCLUDE
#define CORESCENE_INCLUDE
#include "CoreMath.inl"
#include "CoreScene.h"
#include "RHI.h"

class RendererContext;

struct UniformBufferObject {
	alignas(16) float4x4 model;
	alignas(16) float4x4 view;
	alignas(16) float4x4 proj;
	alignas(16) float4 viewPosition;
};


class RENDERER_API RenderProxyBase
{
	
};

class RENDERER_API MeshRenderProxy : public RenderProxyBase
{
public:
	RHIVulkanBufferResource RHIVertexBuffer;
	RHIVulkanBufferResource RHIIndexBuffer;
	RHIVulkanImageResource Texture;
	uint32_t IndexBufferSize;
	void Initialize(RendererContext* Context, Mesh& InMesh);
};

class RENDERER_API SceneRenderProxy : public RenderProxyBase
{
	
};

class RENDERER_API UIRenderProxy : public RenderProxyBase
{
	
};

class RENDERER_API Renderer
{
public:
	RHIVulkanUniform Uniform;
	RHIVulkanPipeline Pipeline;

	RHIVulkanGraphicDispatcher GraphicDispatcher;

	uint32_t UniformBufferSize;
	UniformBufferObject ubo;
	MeshRenderProxy* MeshProxy;
	void Initialize(RendererContext* Context);

	void DrawScene(RendererContext* Context, MeshRenderProxy& MeshProxy, std::vector<char> VS, std::vector<char> PS);

	void DrawUI();

	void UpdateFrame(RendererContext* RContext);
};

class RENDERER_API RendererContext // This is a context used in one viewport
{
public:
	// RHI Objects
	RHIVulkanContext Context;
	RHIVulkanRenderPass RenderPass;

	void Initialize(int Width, int Height);
	bool IsWindowAlive();
	static RendererContext* Get();
private:
	bool bInitialized = false;

	static RendererContext* GInstance;
};