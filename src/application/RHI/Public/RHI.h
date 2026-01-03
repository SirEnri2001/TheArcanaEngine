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
class RHICommandBuffer;
/** Pixel format used in all RHI implementations.
 * Every RHI implementation should always write its own convert function.
 */
enum RHIFormat
{
	R8G8B8A8_SRGB, /**< float4 in hlsl, used in color rendertargets */
    B8G8R8A8_SRGB, /**< float4 in hlsl, used in color rendertargets */
	R8G8B8A8_UNORM, /**< float4 in hlsl, used in color rendertargets */
    R32G32B32A32_SFLOAT, /**< float3 */
    R32G32B32_SFLOAT, /**< float3 */
    R32G32_SFLOAT, /**< float2, uv coords */
    D32_SFLOAT /**< float, depth value */
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
    virtual RHIImageResource* GetColorRenderTarget() = 0;
    virtual RHIImageResource* GetDepthRenderTarget() = 0;
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
    virtual RHIImageResource* GetColorRenderTarget() override;
    virtual RHIImageResource* GetDepthRenderTarget() override;
    RHIWindowManagerBase* GetImpl() { return pImpl.get(); }
};

class RHIImageResourceBase
{
public:
    RHIImageResourceBase() = default;
    RHIImageResourceBase(const RHIImageResourceBase&) = delete;
    virtual ~RHIImageResourceBase() = default;
    virtual void Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, ImageUsage InUsage = IU_COLOR_RT, int32_t MipLevel = -1) = 0;
    virtual void Initialize(RHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, ImageUsage InUsage = IU_COLOR_RT, int32_t MipLevel = -1) = 0;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) = 0;
    virtual void CopyToTexture(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* Data, uint32_t Stride) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
    virtual void Resize(RHIContext* Context, uint32_t Height, uint32_t Width) = 0;
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

    virtual void Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, ImageUsage InUsage = IU_COLOR_RT, int32_t MipLevel = -1) override;
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
    virtual void Initialize(RHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, ImageUsage InUsage = IU_COLOR_RT, int32_t MipLevel = -1)  override;
    /**
     * Copy data to texture
     * @param CommandBuffer Command buffer to record copy commands
     * @param Context 
     * @param Data 
     * @param Stride 
     */
    virtual void CopyToTexture(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* Data, uint32_t Stride) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void Resize(RHIContext* Context, uint32_t Height, uint32_t Width) override;
    RHIImageResourceBase* GetImpl() { return pImpl.get(); }
};

class RHIBufferResourceBase
{
public:
    RHIBufferResourceBase() = default;
    RHIBufferResourceBase(const RHIBufferResourceBase&) = delete;
    virtual ~RHIBufferResourceBase() = default;
    virtual void Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) = 0;
    virtual void CopyToBuffer(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* data, uint32_t TotalBytes) = 0;
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
    /**
     * Copy data to buffer
     * @param CommandBuffer Command buffer to record copy commands
     * @param Context 
     * @param data 
     * @param TotalBytes 
     */
    virtual void CopyToBuffer(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* data, uint32_t TotalBytes) override;
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
    virtual void Initialize(RHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
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
	virtual void Initialize(RHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth = true, RHIFormat DepthFormat = RHIFormat::D32_SFLOAT) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIRenderPassBase* GetImpl() { return pImpl.get(); }
};

class RHIFrameBufferBase
{
public:
    RHIFrameBufferBase() = default;
    RHIFrameBufferBase(const RHIFrameBufferBase&) = delete;
    virtual ~RHIFrameBufferBase() = default;
    virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPass,
        std::vector<RHIImageResource*> ColorRTs, RHIImageResource* DepthRT) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

class RHI_API RHIFrameBuffer : public RHIFrameBufferBase
{
    std::unique_ptr<RHIFrameBufferBase> pImpl = nullptr;
public:
    RHIFrameBuffer();
    RHIFrameBuffer(const RHIFrameBuffer&) = delete;
    virtual ~RHIFrameBuffer() override;
    virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPass,
        std::vector<RHIImageResource*> ColorRTs, RHIImageResource* DepthRT) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIFrameBufferBase* GetImpl() { return pImpl.get(); }
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

class RHIGraphicsDispatcher;

class RHICommandBufferBase {
public:
    RHICommandBufferBase() = default;
    RHICommandBufferBase(const RHICommandBufferBase&) = delete;
    virtual ~RHICommandBufferBase() = default;
    virtual void Initialize(RHIContext* Context) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

/**
 * RHICommandBuffer stores command buffers for recording rendering commands.
 */
class RHI_API RHICommandBuffer
{
    std::unique_ptr<RHICommandBufferBase> pImpl = nullptr;
public:
    RHICommandBuffer();
    RHICommandBuffer(const RHICommandBuffer&) = delete;
    ~RHICommandBuffer();
    void Initialize(RHIContext* Context);
    void Cleanup(RHIContext* Context);
    RHICommandBufferBase* GetImpl() { return pImpl.get(); }
};

class RHISwapchainBase {
public:
    RHISwapchainBase() = default;
    RHISwapchainBase(const RHISwapchainBase&) = delete;
    virtual ~RHISwapchainBase() = default;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
    virtual void AcquireFrame(RHIContext* Context, RHIFrameBuffer*& OutFrameBuffer, RHIRenderPass* InRenderPass) = 0;
    virtual void PresentFrameAndRelease(RHIContext* Context, RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* GDispatcher) = 0;
    virtual ImageExtent3D GetFrameSize() = 0;
};

class RHI_API RHISwapchain : public RHISwapchainBase {
    std::unique_ptr<RHISwapchainBase> pImpl = nullptr;
public:
    RHISwapchain();
    RHISwapchain(const RHISwapchain&) = delete;
    virtual ~RHISwapchain();
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void AcquireFrame(RHIContext* Context, RHIFrameBuffer*& OutFrameBuffer, RHIRenderPass* InRenderPass) override;
    virtual void PresentFrameAndRelease(RHIContext* Context, RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* GDispatcher) override;
    virtual ImageExtent3D GetFrameSize() override;
    RHISwapchainBase* GetImpl() { return pImpl.get(); }
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
    virtual void Draw(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) = 0;
    virtual void BeginRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass, RHIFrameBuffer* Framebuffer) = 0;
    virtual void EndRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass) = 0;
    virtual void BeginFrame(RHICommandBuffer* CommandBuffer, RHIContext* Context, ::RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) = 0;
    virtual void EndFrameAndSubmit(RHICommandBuffer* CommandBuffer, RHIContext* Context, RHIWindowManager* WindowManager, RHIFrameBuffer* PresentFrameBuffer = nullptr) = 0;
    virtual void WaitForGPUIdle(RHIContext* Context) = 0;
    virtual void TransitionImageAsShaderRead(RHICommandBuffer* CommandBuffer, RHIImageResource* Image) = 0;
    virtual void TransitionImageAsRenderTarget(RHICommandBuffer* CommandBuffer, RHIImageResource* Image) = 0;
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
     * @param CommandBuffer Command buffer to record commands
     * @param PipelineObject 
     * @param IndexCount 
     * @param IndexOffset 
     * @param InstanceCount 
     */
    virtual void Draw(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    virtual void BeginRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass, RHIFrameBuffer* Framebuffer) override;
    virtual void EndRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass) override;
    virtual void BeginFrame(RHICommandBuffer* CommandBuffer, RHIContext* Context, ::RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) override;
    virtual void EndFrameAndSubmit(RHICommandBuffer* CommandBuffer, RHIContext* Context, RHIWindowManager* WindowManager, RHIFrameBuffer* PresentFrameBuffer = nullptr) override;
    virtual void WaitForGPUIdle(RHIContext* Context) override;
    virtual void TransitionImageAsShaderRead(RHICommandBuffer* CommandBuffer, RHIImageResource* Image) override;
    virtual void TransitionImageAsRenderTarget(RHICommandBuffer* CommandBuffer, RHIImageResource* Image) override;
    RHIGraphicsDispatcherBase* GetImpl() { return pImpl.get(); }
};

class RHIComputeDispatcherBase {
public:
    RHIComputeDispatcherBase() = default;
    RHIComputeDispatcherBase(const RHIComputeDispatcherBase&) = delete;
    virtual ~RHIComputeDispatcherBase() = default;
    virtual void Initialize(RHIContext* Context) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
    virtual void Dispatch(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) = 0;
    virtual void WaitForGPUIdle(RHIContext* Context) = 0;
};

/**
 * RHIComputeDispatcher dispatches compute shader work.
 */
class RHI_API RHIComputeDispatcher : public RHIComputeDispatcherBase
{
    std::unique_ptr<RHIComputeDispatcherBase> pImpl = nullptr;
public:
    RHIComputeDispatcher();
    RHIComputeDispatcher(const RHIComputeDispatcher&) = delete;
    virtual ~RHIComputeDispatcher();
    virtual void Initialize(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context) override;
    /**
     * Dispatch compute shader work
     * @param CommandBuffer Command buffer to record compute dispatch commands
     * @param PipelineObject Compute pipeline object
     * @param ThreadGroupX Number of thread groups in X dimension
     * @param ThreadGroupY Number of thread groups in Y dimension
     * @param ThreadGroupZ Number of thread groups in Z dimension
     */
    virtual void Dispatch(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) override;
    virtual void WaitForGPUIdle(RHIContext* Context) override;
    RHIComputeDispatcherBase* GetImpl() { return pImpl.get(); }
};

struct ImGuiSharedGlobals;
class RHIImGUIBase
{
public:
    RHIImGUIBase() = default;
    RHIImGUIBase(const RHIImGUIBase&) = delete;
    virtual ~RHIImGUIBase() = default;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) = 0;
    virtual void DispatchImGUI(RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* Dispatcher) = 0;
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
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) override;
    /**
     * Executes drawcalls for ImGUI.
     * @param CommandBuffer Command buffer to record ImGUI draw commands
     */
    virtual void DispatchImGUI(RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* Dispatcher) override;
    /**
     * UI components are defined here. 
     */
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
	virtual void Cleanup() override;
    RHIImGUIBase* GetImpl() { return pImpl.get(); }
};
