//#pragma once
//
//#ifdef RENDERER_IMPLEMENT
//#define RENDERER_API __declspec(dllexport)
//#else
//#ifdef RENDERER_INCLUDE
//#define RENDERER_API __declspec(dllimport)
//#else
//#error Please specify API linkage before include this file
//#define RENDERER_API
//#endif
//#endif
//
//#define RHI_INCLUDE
//#define CORESCENE_INCLUDE
//#include "CoreMath.inl"
//#include "CoreScene.h"
//#include "RHI.h"
//
//class RendererContext;
//
//
//class RENDERER_API RenderProxyBase
//{
//};
//
//class RENDERER_API MeshRenderProxy : public RenderProxyBase
//{
//public:
//	IRHIBufferResource RHIVertexBuffer;
//	IRHIBufferResource RHIIndexBuffer;
//	IRHIImageResource Texture;
//	uint32_t IndexBufferSize;
//	void Initialize(RendererContext* Context, Mesh& InMesh);
//	~MeshRenderProxy();
//};
//
//class RENDERER_API SceneRenderProxy : public RenderProxyBase
//{
//	
//};
//
//class RENDERER_API UIRenderProxy : public RenderProxyBase
//{
//	
//};
//
//class RENDERER_API Renderer
//{
//public:
//	IRHIUniform* Uniform;
//	uint32_t IndexBufferSize;
//	IRHIPipelineFactory PipelineFactory;
//	IRHIPipelineObject PipelineObject;
//	IRHIPipelineObject PresentPipelineObject;
//	IRHIBufferResource RHIFullScreenQuadBuffer;
//	IRHIBufferResource RHIFullScreenQuadIndexBuffer;
//	IRHIGraphicsDispatcher GraphicDispatcher;
//	IRHIRenderPass RenderPass;
//	IRHIRenderPass PresentPass;
//	IRHIImageResource GBufferA;
//	IRHIImageResource GBufferD;
//	std::vector<MeshRenderProxy*> MeshProxyPasses;
//	void (*pFuncImDraw)(ImGuiSharedGlobals*);
//
//	void Initialize(RendererContext* Context, std::vector<char> VS, std::vector<char> PS, std::vector<char> VS1, std::vector<char> PS1);
//
//	void SetUniform(IRHIUniform* Uniform, uint32_t Binding);
//
//	void SetTextureSampler(IRHIImageResource* Texture, uint32_t Binding);
//
//	void DrawScene(RendererContext* Context, MeshRenderProxy& MeshProxy);
//
//	void UpdateFrame(RendererContext* RContext);
//};
//
//class RENDERER_API RendererContext // This is a context used in one viewport
//{
//public:
//	// RHI Objects
//	IRHIContext Context;
//	IRHIWindowManager WindowManager;
//	IRHIImGUI ImGUI;
//	IRHIImageResource ColorRenderTarget;
//	IRHIImageResource DepthRenderTarget;
//
//	void Initialize(int Width, int Height);
//	bool IsWindowAlive();
//	static RendererContext* Get();
//private:
//	bool bInitialized = false;
//
//	static RendererContext* GInstance;
//};