// RHI.h - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description RHI abstraction for different APIs 


#pragma once

#ifdef RHI_IMPLEMENT
#define RHI_API __declspec(dllexport)
#else
#ifdef RHI_INCLUDE
#define RHI_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define RHI_API
#endif
#endif

#include <cstdint>
#include <memory>
#include <vector>

class RHIRenderPass;
class RHIImageResource;
class RHIPipelineObject;
/** Pixel format used in all RHI implementations.
 * Every RHI implementation should always write its own convert function.
 */
enum RHIFormat
{
	R8G8B8A8_SRGB, /**< float4 in hlsl, used in color rendertargets */
	R8G8B8A8_UNORM, /**< float4 in hlsl, used in color rendertargets */
    R32G32B32A32_SFLOAT, /**< float3 */
    R32G32B32_SFLOAT, /**< float3 */
    R32G32_SFLOAT /**< float2, uv coords */
};

enum ImageUsage
{
    IU_GENERAL,  /**< General texture usage */
    IU_COLOR_RT, /**< Color rendertarget, created with RHIImageResource::InitializeRenderTarget */
    IU_DEPTH_RT, /**< Depth rendertarget, created with RHIImageResource::InitializeRenderTarget */
    IU_COLOR_PRESENT_RT, /**< Color rendertarget, created with RHIImageResource::InitializeRenderTarget, specified for present on surface */
    IU_DPETH_PRESENT_RT  /**< Depth rendertarget, created with RHIImageResource::InitializeRenderTarget, specified for present on surface */
};

enum BufferType
{
    GENERAL, /**< General buffer */
    VERTEX, /**< Vertex buffer */
    INDEX /**< Index buffer */
};

struct ImageExtent3D
{
    uint32_t Width;
    uint32_t Height;
    uint32_t Depth;
};

enum RHIImplementationSelection
{
	RHIImplement_Vulkan,
    RHIImplement_D3D12
};


extern RHI_API RHIImplementationSelection GRHIImplementationSelection; /**< Specify which implementation we are currently using in the application */

class RHIWindowManager;

class RHIPlatformSupportBase
{
public:
    RHIPlatformSupportBase() = default;
    RHIPlatformSupportBase(const RHIPlatformSupportBase&) = delete;
    virtual ~RHIPlatformSupportBase() = default;
    virtual void Initialize() = 0;
    virtual void Cleanup() = 0;
};

/**
 * RHIPlatformSupport provides Graphics API environment context. 
 * This class serves as a helper class to create & destroy window manager and context, and should be referenced as few as possible in the pipeline creation process. 
 * May vary by different hardware and OS. Should exist only one instance for each application.
 * In the initialization process, often we mix up the API layer initialization and window initialization.
 * So Initialize() function should doing all works before we have any window. Then use InitializePhysicalDevice to finalize the process.
 * @see RHIVulkanPlatformSupport Implementation for Vulkan API
 */
class RHI_API RHIPlatformSupport : public RHIPlatformSupportBase
{
	std::unique_ptr<RHIPlatformSupportBase> pImpl = nullptr;
    static RHIPlatformSupport* GInstance;
public:
    RHIPlatformSupport();
    virtual ~RHIPlatformSupport() override;
	/**
	 * Should be called first in application.
	 */
	virtual void Initialize() override;
	/**
	 * Should be called last in the graphical context.
	 */
	virtual void Cleanup() override;
    static RHIPlatformSupport* Get();
    RHIPlatformSupportBase* GetImpl() { return pImpl.get(); }
};

class RHIContextBase
{
public:
    RHIContextBase() = default;
    RHIContextBase(const RHIContextBase&) = delete;
    virtual ~RHIContextBase() = default;
    virtual void Initialize(RHIPlatformSupport* PlatformSupport) = 0;
    virtual void Cleanup() = 0;
    virtual void WaitDeviceIdle() = 0;
};

/** RHIContext serves as a general RHI resource allocator.
 * It should only has necessary fields and handles to create rendering resource like Images and Buffers.
 */
class RHI_API RHIContext : public RHIContextBase
{
    std::unique_ptr<RHIContextBase> pImpl = nullptr;
public:
    RHIContext();
    virtual ~RHIContext() override;
    virtual void Initialize(RHIPlatformSupport* PlatformSupport) override;
    virtual void Cleanup() override;
    virtual void WaitDeviceIdle() override;
    RHIContextBase* GetImpl() { return pImpl.get(); }
};

class RHIWindowManagerBase
{
public:
    RHIWindowManagerBase() = default;
    RHIWindowManagerBase(const RHIWindowManagerBase&) = delete;
    virtual ~RHIWindowManagerBase() = default;
    virtual void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) = 0;
    virtual void Cleanup(RHIPlatformSupport* PlatformSupport) = 0;
    virtual void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) = 0;
    virtual void CleanupSwapchain(RHIContext* Context) = 0;
    virtual void RecreateSwapchain(RHIContext* Context) = 0;
    virtual void AddScreenSizeTexture(RHIImageResource* ImageResource) = 0;
    virtual void RemoveScreenSizeTexture(RHIImageResource* ImageResource) = 0;
    virtual void InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context) = 0;
    virtual bool IsAlive() = 0;
    virtual uint32_t GetWindowHeight() = 0;
    virtual uint32_t GetWindowWidth() = 0;
};

/** RHIWindowManager provide a wrapper for window, e.g.\ glfwwindow.
 * RHIWindowManager is also responsible for dealing with window resize, full screen mode and v-sync, and swapchain recreation.
 */
class RHI_API RHIWindowManager : public RHIWindowManagerBase
{
    std::unique_ptr<RHIWindowManagerBase> pImpl = nullptr;
public:
    RHIWindowManager();
    virtual ~RHIWindowManager() override;
    /**
     * Initialize and create a new window. 
     * @param PlatformSupport Initialized Platform support. Should immediately call RHIPlatformSupport::InitializePhysicalDevice after calling this function
     * @param WindowHeight 
     * @param WindowWidth 
     */
    virtual void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
    virtual void Cleanup(RHIPlatformSupport* PlatformSupport) override;
    /**
     * Call this after RHIPlatformSupport::InitializePhysicalDevice
     * @param Context 
     * @param PlatformSupport 
     */
    virtual void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
    virtual void CleanupSwapchain(RHIContext* Context) override;
    virtual void RecreateSwapchain(RHIContext* Context) override;
    virtual void AddScreenSizeTexture(RHIImageResource* ImageResource) override;
    virtual void RemoveScreenSizeTexture(RHIImageResource* ImageResource) override;
    virtual void InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context) override;
    virtual bool IsAlive() override;
    virtual uint32_t GetWindowHeight() override;
    virtual uint32_t GetWindowWidth() override;
    RHIWindowManagerBase* GetImpl() { return pImpl.get(); }
};

class RHIImageResourceBase
{
public:
    RHIImageResourceBase() = default;
    RHIImageResourceBase(const RHIImageResourceBase&) = delete;
    virtual ~RHIImageResourceBase() = default;
    virtual void Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, int32_t MipLevel = -1) = 0;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) = 0;
    virtual void CopyToTexture(RHIContext* Context, void* Data, uint32_t Stride) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};


/**
 * RHIImageResource stores textures and rendertargets. 
 */
class RHI_API RHIImageResource : public RHIImageResourceBase
{
    std::unique_ptr<RHIImageResourceBase> pImpl = nullptr;
public:
    RHIImageResource();
    virtual ~RHIImageResource() override;

    virtual void Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, int32_t MipLevel = -1) override;
    /**
     * Create a rendertarget that can be displayed on screen.
     * @param Context 
     * @param WindowManager 
     * @param RTExtent Generally should be {WindowManager.GetWidth(), WindowManager.GetHeight(), 1}. 
     * @param InUsage Specify rendertarget to store color or depth
     * @param MultiSamplesCount Use 1, 2, 4, 8, ... for msaa
     * @see RHIRenderPass::Initialize()
     */
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) override;
    virtual void CopyToTexture(RHIContext* Context, void* Data, uint32_t Stride) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIImageResourceBase* GetImpl() { return pImpl.get(); }
};

class RHIBufferResourceBase
{
public:
    RHIBufferResourceBase() = default;
    RHIBufferResourceBase(const RHIBufferResourceBase&) = delete;
    virtual ~RHIBufferResourceBase() = default;
    virtual void Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) = 0;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

/**
 * RHIBufferResource stores vertex and index buffers.
 */
class RHI_API RHIBufferResource : public RHIBufferResourceBase
{
    std::unique_ptr<RHIBufferResourceBase> pImpl = nullptr;
public:
    RHIBufferResource();
    virtual ~RHIBufferResource() override;
    virtual void Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) override;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIBufferResourceBase* GetImpl() { return pImpl.get(); }
};

class RHIUniformBase
{
public:
    RHIUniformBase() = default;
    RHIUniformBase(const RHIUniformBase&) = delete;
    virtual ~RHIUniformBase() = default;
    virtual void Initialize(RHIContext* Context, uint32_t UniformStructSize) = 0;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

/**
 * RHIUniform stores uniform buffers.
 */
class RHI_API RHIUniform : public RHIUniformBase
{
    std::unique_ptr<RHIUniformBase> pImpl = nullptr;
public:
    RHIUniform();
    virtual ~RHIUniform() override;
    virtual void Initialize(RHIContext* Context, uint32_t UniformStructSize) override;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIUniformBase* GetImpl() { return pImpl.get(); }
};

class RHIRenderPassBase
{
public:
    RHIRenderPassBase() = default;
    RHIRenderPassBase(const RHIRenderPassBase&) = delete;
    virtual ~RHIRenderPassBase() = default;
    virtual void Initialize(RHIContext* Context, uint32_t SizeX, uint32_t SizeY) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
    virtual void AddColorRenderTarget(RHIImageResource* ColorRT) = 0;
    virtual void SetDepthRenderTarget(RHIImageResource* DepthRT) = 0;
};

/**
 * RHIRenderPass stores framebuffers, and manages rendertargets.
 */
class RHI_API RHIRenderPass : public RHIRenderPassBase
{
    std::unique_ptr<RHIRenderPassBase> pImpl = nullptr;
public:
    RHIRenderPass();
    virtual ~RHIRenderPass() override;
    virtual void Initialize(RHIContext* Context, uint32_t SizeX, uint32_t SizeY) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void AddColorRenderTarget(RHIImageResource* ColorRT) override;
    virtual void SetDepthRenderTarget(RHIImageResource* DepthRT) override;
    RHIRenderPassBase* GetImpl() { return pImpl.get(); }
};


class RHIPresentPassBase
{
public:
    RHIPresentPassBase() = default;
    RHIPresentPassBase(const RHIPresentPassBase&) = delete;
    virtual ~RHIPresentPassBase() = default;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
    virtual void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
};

/**
 * RHIRenderPass stores framebuffers, and manages rendertargets.
 */
class RHI_API RHIPresentPass : public RHIPresentPassBase
{
    std::unique_ptr<RHIPresentPassBase> pImpl = nullptr;
public:
    RHIPresentPass();
    virtual ~RHIPresentPass() override;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) override;
    RHIPresentPassBase* GetImpl() { return pImpl.get(); }
};

class RHIPipelineFactoryBase
{
public:
    RHIPipelineFactoryBase() = default;
    RHIPipelineFactoryBase(const RHIPipelineFactoryBase&) = delete;
    virtual ~RHIPipelineFactoryBase() = default;
    virtual void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) = 0;
    virtual void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) = 0;
    virtual void RemoveAllBufferBindings() = 0;
    virtual void SetUniformBinding(uint32_t Binding) = 0;
    virtual void SetImageSamplerBinding(uint32_t Binding) = 0;
    virtual void RemoveAllGlobalBindings() = 0;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) = 0;
    virtual void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource) = 0;
    virtual void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPass) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

/**
 * RHIPipelineFactory defines shader pipeline and vertex buffer descriptions. 
 */
class RHI_API RHIPipelineFactory : public RHIPipelineFactoryBase
{
    std::unique_ptr<RHIPipelineFactoryBase> pImpl = nullptr;
public:
    RHIPipelineFactory();
    virtual ~RHIPipelineFactory() override;
    /**
     * 
     * @param BindingIndex Always use AddBinding add this binding index
     * @param Location defined in glsl: layout(location = xxx)
     * @param Format size of this vertex attribute
     * @param Offset use offset() macro
     */
    virtual void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
    /**
     * 
     * @param BindingIndex BindingIndex for buffer. Not necessarily differs from binding index of uniforms' and image samplers', but should be the same as in RHIGraphicsDispatcher::BindVertexBuffer / BindIndexBuffer
     * @param Stride 
     */
    virtual void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) override;
    virtual void RemoveAllBufferBindings() override;
    virtual void SetUniformBinding(uint32_t Binding) override;
    virtual void SetImageSamplerBinding(uint32_t Binding) override;
    virtual void RemoveAllGlobalBindings() override;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
    virtual void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource) override;
    virtual void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPass) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIPipelineFactoryBase* GetImpl() { return pImpl.get(); }
};

class RHIPipelineObjectBase
{
public:
    RHIPipelineObjectBase() = default;
    RHIPipelineObjectBase(const RHIPipelineObjectBase&) = delete;
    virtual ~RHIPipelineObjectBase() = default;
    virtual void SetUniform(RHIUniform* Uniform, uint32_t Binding) = 0;
    virtual void SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

/**
 * RHIPipelineFactory defines shader pipeline and vertex buffer descriptions. 
 */
class RHI_API RHIPipelineObject : public RHIPipelineObjectBase
{
    std::unique_ptr<RHIPipelineObjectBase> pImpl = nullptr;
public:
    RHIPipelineObject();
    virtual ~RHIPipelineObject() override;
    virtual void SetUniform(RHIUniform* Uniform, uint32_t Binding) override;
    virtual void SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIPipelineObjectBase* GetImpl() { return pImpl.get(); }
};

class RHIGraphicsDispatcherBase
{
public:
    RHIGraphicsDispatcherBase() = default;
    RHIGraphicsDispatcherBase(const RHIGraphicsDispatcherBase&) = delete;
    virtual ~RHIGraphicsDispatcherBase() = default;
    virtual void Initialize(RHIContext* Context) = 0;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) = 0;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) = 0;
    virtual void Dispatch(RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) = 0;
    virtual void BeginRenderPass(RHIRenderPass* RenderPass) = 0;
    virtual void EndRenderPass(RHIRenderPass* RenderPass) = 0;
    virtual void BeginFrame(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass) = 0;
    virtual void EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void WaitForGPUIdle(RHIContext* Context) = 0;
};

/**
 * RHIGraphicsDispatcher binds the actual buffers with the pipeline, and conduct the actual rendering (record commandbuffers and submit to surface)
 */
class RHI_API RHIGraphicsDispatcher : public RHIGraphicsDispatcherBase
{
    std::unique_ptr<RHIGraphicsDispatcherBase> pImpl = nullptr;
public:
    RHIGraphicsDispatcher();
    virtual ~RHIGraphicsDispatcher() override;
    virtual void Initialize(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
    /**
     * Call this between BeginRenderPass & EndRenderPass 
     * @param PipelineObject 
     * @param IndexCount 
     * @param IndexOffset 
     * @param InstanceCount 
     */
    virtual void Dispatch(RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    virtual void BeginRenderPass(RHIRenderPass* RenderPass) override;
    virtual void EndRenderPass(RHIRenderPass* RenderPass) override;
    virtual void BeginFrame(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass) override;
    virtual void EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void WaitForGPUIdle(RHIContext* Context) override;
    RHIGraphicsDispatcherBase* GetImpl() { return pImpl.get(); }
};

class RHIComputeDispatcherBase {
public:
    RHIComputeDispatcherBase() = default;
    RHIComputeDispatcherBase(const RHIComputeDispatcherBase&) = delete;
    virtual ~RHIComputeDispatcherBase() = default;
    virtual void Initialize(RHIContext* Context) = 0;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) = 0;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) = 0;
    virtual void Dispatch(RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) = 0;
    virtual void BeginRenderPass(RHIRenderPass* RenderPass) = 0;
    virtual void EndRenderPass(RHIRenderPass* RenderPass) = 0;
    virtual void BeginFrame(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass) = 0;
    virtual void EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void WaitForGPUIdle(RHIContext* Context) = 0;
};

class RHI_API RHIComputeDispatcher : public RHIComputeDispatcherBase {

};

struct ImGuiSharedGlobals;
class RHIImGUIBase
{
public:
    RHIImGUIBase() = default;
    RHIImGUIBase(const RHIImGUIBase&) = delete;
    virtual ~RHIImGUIBase() = default;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass) = 0;
    virtual void DispatchImGUI(RHIGraphicsDispatcher* Dispatcher) = 0;
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) = 0;
	virtual void Cleanup() = 0;
};

/**
 * RHIImGUI manages ImGUI objects with the rest of RHI interfaces.
 */
class RHI_API RHIImGUI : public RHIImGUIBase
{
    std::unique_ptr<RHIImGUIBase> pImpl = nullptr;
public:
    RHIImGUI();
    RHIImGUI(const RHIImGUI&) = delete;
    virtual ~RHIImGUI();
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass) override;
    /**
     * Executes drawcalls for ImGUI.
     */
    virtual void DispatchImGUI(RHIGraphicsDispatcher* Dispatcher) override;
    /**
     * UI components are defined here. 
     */
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
	virtual void Cleanup() override;
    RHIImGUIBase* GetImpl() { return pImpl.get(); }
};