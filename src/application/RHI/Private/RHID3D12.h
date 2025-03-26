#pragma once

#define RHI_IMPLEMENT
#include <dxgi1_4.h>
#include <directx/d3dx12_core.h>
#include <directx/d3dx12_root_signature.h>
#include <wrl/client.h>

#include "RHI.h"

using Microsoft::WRL::ComPtr;

// RHID3D12PlatformSupport
class RHID3D12PlatformSupport : public RHIPlatformSupportBase
{
public:
    RHID3D12PlatformSupport() = default;
    virtual ~RHID3D12PlatformSupport() override = default;

    virtual void Initialize() override;
    virtual void Cleanup() override;
    virtual void InitializePhysicalDevice(RHIWindowManager* WindowManager) override;
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
    RHID3D12WindowManager() = default;
    virtual ~RHID3D12WindowManager() override = default;

    virtual void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
    virtual void Cleanup(RHIPlatformSupport* PlatformSupport) override;
    virtual void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
    virtual void CleanupSwapchain(RHIContext* Context) override;
    virtual void RecreateSwapchain(RHIContext* Context) override;
    virtual bool IsAlive() override;
    virtual uint32_t GetWindowHeight() override;
    virtual uint32_t GetWindowWidth() override;
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

// RHID3D12ImageResource
class RHID3D12ImageResource : public RHIImageResourceBase
{
public:
    RHID3D12ImageResource() = default;
    virtual ~RHID3D12ImageResource() override = default;
    uint32_t Height;
    uint32_t Width;
    
    ComPtr<ID3D12Resource> m_texture;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    virtual void Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel = -1) override;
    virtual void Initialize(RHIContext* Context, void* Data, uint32_t Size, uint32_t Height, uint32_t Width, RHIFormat InFormat, uint32_t MipLevel) override;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) override;
    virtual void Cleanup(RHIContext* Context) override;
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
};

// RHID3D12RenderPass
class RHID3D12RenderPass : public RHIRenderPassBase
{
public:
	RHID3D12RenderPass() = default;
	virtual ~RHID3D12RenderPass() override = default;
	virtual void Initialize(RHIContext* Context, uint32_t SizeX, uint32_t SizeY) override;
	virtual void Cleanup(RHIContext* Context) override;
	virtual void AddColorRenderTarget(RHIImageResource* ColorRT) override;
	virtual void SetDepthRenderTarget(RHIImageResource* DepthRT) override;
};

class RHID3D12PresentPass : public RHIPresentPassBase
{
public:
    ComPtr<ID3D12Resource> m_renderTargets[2];
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize;
    const UINT FrameCount = 2;
    UINT m_frameIndex;
	RHID3D12PresentPass() = default;
	virtual ~RHID3D12PresentPass() override = default;
	virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT) override;
	virtual void CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager) override;
	virtual void CleanupSwapchainFramebuffer(RHIContext* Context) override;
	virtual void Cleanup(RHIContext* Context) override;
	virtual void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) override;
};

// RHID3D12Pipeline
class RHID3D12PipelineObject : public RHIPipelineObjectBase
{
public:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    std::vector< ID3D12DescriptorHeap* > Heaps;

    RHID3D12PipelineObject() = default;
    virtual ~RHID3D12PipelineObject() override = default;
    
    virtual void SetUniform(RHIUniform* Uniform, uint32_t Binding) override;
    virtual void SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding) override;
    virtual void Cleanup(RHIContext* Context) override;
};

class RHID3D12PipelineFactory : public RHIPipelineFactoryBase
{
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
    std::vector<CD3DX12_DESCRIPTOR_RANGE1> Ranges;
    std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> Samplers;
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
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    RHID3D12PresentPass* D3D12PresentPass;
    RHID3D12GraphicsDispatcher() = default;
    virtual ~RHID3D12GraphicsDispatcher() override = default;

    virtual void Initialize(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
    virtual void Dispatch(RHIWindowManager* WindowManager, RHIPipelineObject* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    virtual void BeginFrame() override;
	virtual void BeginRenderPass(RHIContext* Context, RHIRenderPass* RenderPass) override;
    virtual void EndRenderPass(RHIRenderPass* RenderPass) override;
	virtual void BeginPresentPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPassResource) override;
	virtual void EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager) override;
	virtual void WaitForGPUIdle(RHIContext* Context) override;
};

// RHID3D12ImGUI
class RHID3D12ImGUI : public RHIImGUIBase
{
public:
    RHID3D12ImGUI() = default;
    virtual ~RHID3D12ImGUI() override = default;

    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass) override;
    virtual void DispatchImGUI(RHIGraphicsDispatcher* Dispatcher) override;
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
    virtual void Cleanup() override;
};

