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

struct ImDrawData;

enum RHIFormat
{
	R8G8B8A8_SRGB,
    R32G32B32_SFLOAT,
    R32G32_SFLOAT
};

enum ImageUsage
{
    IU_GENERAL,
    IU_COLOR_RT,
    IU_DEPTH_RT
};

enum BufferType
{
    GENERAL,
    VERTEX,
    INDEX
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
extern RHIImplementationSelection GRHIImplementationSelection;

class RHIWindowManager;

class RHIPlatformSupportBase
{
public:
    RHIPlatformSupportBase() = default;
    RHIPlatformSupportBase(const RHIPlatformSupportBase&) = delete;
    virtual ~RHIPlatformSupportBase() = default;
    virtual void Initialize() = 0;
    virtual void Cleanup() = 0;
    virtual void InitializePhysicalDevice(RHIWindowManager* WindowManager) = 0;
};

class RHI_API RHIPlatformSupport : public RHIPlatformSupportBase
{
	std::unique_ptr<RHIPlatformSupportBase> pImpl = nullptr;
    static RHIPlatformSupport* GInstance;
public:
    RHIPlatformSupport();
    virtual ~RHIPlatformSupport() override;
    virtual void Initialize() override;
    virtual void Cleanup() override;
    virtual void InitializePhysicalDevice(RHIWindowManager* WindowManager) override;
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
    virtual bool IsAlive() = 0;
    virtual uint32_t GetWindowHeight() = 0;
    virtual uint32_t GetWindowWidth() = 0;
};

class RHI_API RHIWindowManager : public RHIWindowManagerBase
{
    std::unique_ptr<RHIWindowManagerBase> pImpl = nullptr;
public:
    RHIWindowManager();
    virtual ~RHIWindowManager() override;
    virtual void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
    virtual void Cleanup(RHIPlatformSupport* PlatformSupport) override;
    virtual void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
    virtual void CleanupSwapchain(RHIContext* Context) override;
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
    virtual void Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel = -1) = 0;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

class RHI_API RHIImageResource : public RHIImageResourceBase
{
    std::unique_ptr<RHIImageResourceBase> pImpl = nullptr;
public:
    RHIImageResource();
    virtual ~RHIImageResource() override;
    virtual void Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel = -1) override;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) override;
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
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void CleanupSwapchainFramebuffer(RHIContext* Context) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

class RHI_API RHIRenderPass : public RHIRenderPassBase
{
    std::unique_ptr<RHIRenderPassBase> pImpl = nullptr;
public:
    RHIRenderPass();
    virtual ~RHIRenderPass() override;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void CleanupSwapchainFramebuffer(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIRenderPassBase* GetImpl() { return pImpl.get(); }
};

class RHIPipelineBase
{
public:
    RHIPipelineBase() = default;
    RHIPipelineBase(const RHIPipelineBase&) = delete;
    virtual ~RHIPipelineBase() = default;
    virtual void AddLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) = 0;
    virtual void AddBinding(uint32_t BindingIndex, uint32_t Stride) = 0;
    virtual void AddUniformBuffer(RHIUniform* Uniform, uint32_t Binding) = 0;
    virtual void AddImageSampler(RHIImageResource* ImageResource, uint32_t Binding) = 0;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) = 0;
    virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPassResource) = 0;
    virtual void Cleanup(RHIContext* Context) = 0;
};

class RHI_API RHIPipeline : public RHIPipelineBase
{
    std::unique_ptr<RHIPipelineBase> pImpl = nullptr;
public:
    RHIPipeline();
    virtual ~RHIPipeline() override;
    virtual void AddLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
    virtual void AddBinding(uint32_t BindingIndex, uint32_t Stride) override;
    virtual void AddUniformBuffer(RHIUniform* Uniform, uint32_t Binding) override;
    virtual void AddImageSampler(RHIImageResource* ImageResource, uint32_t Binding) override;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
    virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPassResource) override;
    virtual void Cleanup(RHIContext* Context) override;
    RHIPipelineBase* GetImpl() { return pImpl.get(); }
};

class RHIGraphicDispatcherBase
{
public:
    RHIGraphicDispatcherBase() = default;
    RHIGraphicDispatcherBase(const RHIGraphicDispatcherBase&) = delete;
    virtual ~RHIGraphicDispatcherBase() = default;
    virtual void Initialize(RHIContext* Context) = 0;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) = 0;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) = 0;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) = 0;
    virtual void Dispatch(RHIWindowManager* WindowManager, RHIPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) = 0;
    //virtual void DispatchImGUI(ImDrawData* draw_data, void* RenderFunctionPointer) = 0;
    virtual void PrepareRenderPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPass, uint32_t& OutImageIndex) = 0;
    virtual void BeginRenderPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPassResource, uint32_t InImageIndex) = 0;
    virtual void EndRenderPass() = 0;
    virtual void Submit(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t ImageIndex) = 0;
};

class RHI_API RHIGraphicDispatcher : public RHIGraphicDispatcherBase
{
    std::unique_ptr<RHIGraphicDispatcherBase> pImpl = nullptr;
public:
    RHIGraphicDispatcher();
    virtual ~RHIGraphicDispatcher() override;
    virtual void Initialize(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
    virtual void Dispatch(RHIWindowManager* WindowManager, RHIPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    
    virtual void PrepareRenderPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPass, uint32_t& OutImageIndex) override;
    virtual void BeginRenderPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPassResource, uint32_t InImageIndex) override;
    virtual void EndRenderPass() override;
    virtual void Submit(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t ImageIndex) override;
    RHIGraphicDispatcherBase* GetImpl() { return pImpl.get(); }
};

class RHIImGUIBase
{
public:
    RHIImGUIBase() = default;
    RHIImGUIBase(const RHIImGUIBase&) = delete;
    virtual ~RHIImGUIBase() = default;
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPass) = 0;
    virtual void DispatchImGUI(RHIGraphicDispatcher* Dispatcher) = 0;
    virtual void UpdateUI() = 0;
	virtual void Cleanup() = 0;
};

class RHI_API RHIImGUI : public RHIImGUIBase
{
    std::unique_ptr<RHIImGUIBase> pImpl = nullptr;
public:
    RHIImGUI();
    RHIImGUI(const RHIImGUI&) = delete;
    virtual ~RHIImGUI();
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPass) override;
    virtual void DispatchImGUI(RHIGraphicDispatcher* Dispatcher) override;
    virtual void UpdateUI() override;
	virtual void Cleanup() override;
    RHIImGUIBase* GetImpl() { return pImpl.get(); }
};