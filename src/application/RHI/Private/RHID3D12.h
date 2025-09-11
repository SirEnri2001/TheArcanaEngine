#pragma once

#include <dxgi1_4.h>
#include <directx/d3dx12_core.h>
#include <directx/d3dx12_root_signature.h>
#include <wrl/client.h>

#include "RHIImGuiHelper.h"
#define RHI_IMPLEMENT
#include "RHI.h"

using Microsoft::WRL::ComPtr;

struct DescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    std::vector<int>               FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap);
    void Destroy();
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle);
};

// RHID3D12PlatformSupport
class RHID3D12PlatformSupport : public RHIPlatformSupportBase
{
public:
    RHID3D12PlatformSupport() = default;
    virtual ~RHID3D12PlatformSupport() override = default;

    virtual void Initialize() override;
    virtual void Cleanup() override;
};

// RHID3D12Context
class RHID3D12Context : public RHIContextBase
{
public:
    bool m_useWarpDevice = false;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    DescriptorHeapAllocator SRVHeapAllocator;
    DescriptorHeapAllocator RTVHeapAllocator;
    DescriptorHeapAllocator DSVHeapAllocator;

    ComPtr<ID3D12DescriptorHeap> m_srvheap;
    ComPtr<ID3D12DescriptorHeap> m_rtvheap;
    ComPtr<ID3D12DescriptorHeap> m_dsvheap;

    RHID3D12Context() = default;
    virtual ~RHID3D12Context() override = default;

    virtual void Initialize(RHIPlatformSupport* PlatformSupport) override;
    virtual void Cleanup() override;
    virtual void WaitDeviceIdle() override;
    void WaitForPreviousFrame();
};

// RHID3D12WindowManager
class RHID3D12WindowManager : public RHIWindowManagerBase
{
public:
    MSG msg;
    HWND hWnd;
    UINT m_width = 1280;
    UINT m_height = 720;
    ComPtr<IDXGISwapChain3> m_swapChain;
    bool bResized = false;
    LPARAM ResizeParam;
    ID3D12Resource* m_renderTargets[2];
    ComPtr<ID3D12Resource> m_dsRenderTarget;
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
    const UINT FrameCount = 2;
    UINT m_frameIndex;
    DXGI_FORMAT RTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    RHID3D12WindowManager() = default;
    virtual ~RHID3D12WindowManager() override = default;

    virtual void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
    virtual void Cleanup(RHIPlatformSupport* PlatformSupport) override;
    virtual void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
    virtual void CleanupSwapchain(RHIContext* Context) override;
    virtual void RecreateSwapchain(RHIContext* Context) override;
    virtual void AddScreenSizeTexture(RHIImageResource* ImageResource) override;
    virtual void RemoveScreenSizeTexture(RHIImageResource* ImageResource) override;
    virtual void InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context) override;
    virtual bool IsAlive() override;

    virtual RHIImageResource* GetColorRenderTarget() override {
        return nullptr;
    }
    virtual RHIImageResource* GetDepthRenderTarget() override {
        return nullptr;
    }
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void SetResized(bool val, LPARAM Param){bResized = val; ResizeParam = Param; }
};

// RHID3D12ImageResource
class RHID3D12ImageResource : public RHIImageResourceBase
{
public:
    RHID3D12ImageResource() = default;
    virtual ~RHID3D12ImageResource() override = default;
    uint32_t Height;
    uint32_t Width;

    D3D12_RESOURCE_DESC textureDesc = {};
    ComPtr<ID3D12Resource> m_texture;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;

    virtual void Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, ImageUsage InUsage = IU_COLOR_RT, int32_t MipLevel = -1) override;
    virtual void Initialize(RHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, ImageUsage InUsage = IU_COLOR_RT, int32_t MipLevel = -1)  override;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) override;
    virtual void CopyToTexture(RHIContext* Context, void* Data, uint32_t Stride) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void Resize(RHIContext* Context, uint32_t Height, uint32_t Width) override;

    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE RTDSVGpuDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE RTDSVCpuDescriptorHandle;

    D3D12_RESOURCE_STATES ResourceStates;
};

// RHID3D12BufferResource
class RHID3D12BufferResource : public RHIBufferResourceBase
{
public:
    ComPtr<ID3D12Resource> m_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    RHID3D12BufferResource() = default;
    virtual ~RHID3D12BufferResource() override = default;

    virtual void Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) override;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(RHIContext* Context) override;
};

// RHID3D12Uniform
class RHID3D12Uniform : public RHIUniformBase
{
public:
    RHID3D12Uniform() = default;
    virtual ~RHID3D12Uniform() override = default;

    virtual void Initialize(RHIContext* Context, uint32_t UniformStructSize) override;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(RHIContext* Context) override;

    void CreateConstantBufferView(RHID3D12Context* Context, ID3D12DescriptorHeap* Heap);
    //ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_pCbvDataBegin;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
};

// RHID3D12RenderPass
class RHID3D12RenderPass : public RHIRenderPassBase
{
public:
    uint32_t Height;
    uint32_t Width;
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> ColorRTs;
    std::vector<DXGI_FORMAT> ColorRTFormats;
    std::vector<RHID3D12ImageResource*> TransitionList;
    CD3DX12_CPU_DESCRIPTOR_HANDLE DepthRT;
    DXGI_FORMAT DepthRTFormat;
	RHID3D12RenderPass() = default;
	virtual ~RHID3D12RenderPass() override = default;
	virtual void Initialize(RHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat) override;
	virtual void Cleanup(RHIContext* Context) override;
};

class RHID3D12PresentPass : public RHIPresentPassBase
{
public:
	RHID3D12PresentPass() = default;
	virtual ~RHID3D12PresentPass() override = default;
	virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT) override;
	virtual void Cleanup(RHIContext* Context) override;
	virtual void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) override;
};

enum DescriptorType
{
	
};

// RHID3D12Pipeline
class RHID3D12PipelineObject : public RHIPipelineObjectBase
{
public:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    std::vector<D3D12_CONSTANT_BUFFER_VIEW_DESC> cbvDescriptions;
    std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDescriptions;
    std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> GpuDescriptorHandles;
    std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
    std::vector<RHID3D12ImageResource*> ImageToTransition;
    RHID3D12PipelineObject() = default;
    virtual ~RHID3D12PipelineObject() override = default;
    
    virtual void SetUniform(RHIUniform* Uniform, uint32_t Binding) override;
    virtual void SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding) override;
    virtual void Cleanup(RHIContext* Context) override;

    bool bShouldInitHeap = false;
};

class RHID3D12PipelineFactory : public RHIPipelineFactoryBase
{
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
    std::vector<CD3DX12_DESCRIPTOR_RANGE1> Ranges;
    std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> Samplers;

    uint32_t HeapCount = 0;
public:
    RHID3D12PipelineFactory() = default;
    virtual ~RHID3D12PipelineFactory() override = default;

    virtual void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
    virtual void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) override;
    virtual void RemoveAllBufferBindings() override;
    virtual void SetUniformBinding(uint32_t Binding) override;
    virtual void SetImageSamplerBinding(uint32_t Binding) override;
    virtual void RemoveAllGlobalBindings() override;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
    virtual void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource) override;
    virtual void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPass) override;
    virtual void Cleanup(RHIContext* Context) override;
};

// RHID3D12GraphicsDispatcher
class RHID3D12GraphicsDispatcher : public RHIGraphicsDispatcherBase
{
public:
    std::vector<D3D12_VERTEX_BUFFER_VIEW> BoundBufferViews;
    D3D12_INDEX_BUFFER_VIEW BoundIndexBufferView;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ID3D12DescriptorHeap* pHeaps;
    RHID3D12GraphicsDispatcher() = default;
    virtual ~RHID3D12GraphicsDispatcher() override = default;

    virtual void Initialize(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
    virtual void Dispatch(RHIPipelineObject* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    virtual void BeginFrame(RHIContext* Context, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) override;
	virtual void BeginRenderPass(RHIRenderPass* RenderPass, RHIFrameBuffer* Framebuffer) override;
    virtual void EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager, RHIFrameBuffer* PresentFrameBuffer = nullptr) override;
    virtual void EndRenderPass(RHIRenderPass* RenderPass) override;
    virtual void WaitForGPUIdle(RHIContext* Context) override;
    virtual void TransitionImageAsShaderRead(RHIImageResource* Image) override;
    virtual void TransitionImageAsRenderTarget(RHIImageResource* Image) override;
};

// RHID3D12ImGUI
class RHID3D12ImGUI : public RHIImGUIBase
{
public:
	ImGuiSharedGlobals ImGlobals;
    RHID3D12ImGUI() = default;
    virtual ~RHID3D12ImGUI() override = default;

    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) override;
    virtual void DispatchImGUI(RHIGraphicsDispatcher* Dispatcher) override;
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
    virtual void Cleanup() override;
};

class RHID3D12FrameBuffer : public RHIFrameBufferBase {
public:
    RHID3D12FrameBuffer();
    RHID3D12FrameBuffer(const RHID3D12FrameBuffer&) = delete;
    virtual ~RHID3D12FrameBuffer() override;
    virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPass,
        std::vector<RHIImageResource*> ColorRTs, RHIImageResource* DepthRT) override;
    virtual void Cleanup(RHIContext* Context) override;
};

class RHID3D12Swapchain : public RHISwapchainBase
{
public:
    RHID3D12Swapchain();
    RHID3D12Swapchain(const RHID3D12Swapchain&) = delete;
    virtual ~RHID3D12Swapchain();
    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void AcquireFrame(RHIContext* Context, RHIFrameBuffer*& OutFrameBuffer, RHIRenderPass* InRenderPass) override;
    virtual void PresentFrameAndRelease(RHIContext* Context, RHIGraphicsDispatcher* GDispatcher) override;
    ImageExtent3D GetFrameSize() override;
};