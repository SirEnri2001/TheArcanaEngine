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

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap, bool hasGpuHandle);
    void Destroy();
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle);
};

// RHID3D12PlatformSupport
class RHID3D12PlatformSupport : public IRHIPlatformSupport
{
public:
    RHID3D12PlatformSupport() = default;
    virtual ~RHID3D12PlatformSupport() override = default;

    virtual void Initialize() override;
    virtual void Cleanup() override;
    virtual std::unique_ptr<IRHIContext> CreateRHIContext() override;

    static DXGI_FORMAT GetDXFormat(RHIFormat InFormat)
    {
        switch (InFormat)
        {
        case R8G8B8A8_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case B8G8R8A8_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case R32G32B32_SFLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case R32G32_SFLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case R32G32B32A32_SFLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case D32_SFLOAT:
            return DXGI_FORMAT_D32_FLOAT;
        case R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        return DXGI_FORMAT_UNKNOWN;
    }

    static D3D12_RESOURCE_STATES GetDXResourceStates(RHIResourceState InState) {
        D3D12_RESOURCE_STATES ResourceState = D3D12_RESOURCE_STATE_COMMON;
        // Principle: wide stage bits, narrow access bits - by ChatGPT
        switch (InState)
        {
        case RHIResourceState::UNDEFINED:
            ResourceState = D3D12_RESOURCE_STATE_COMMON;
            break;
        case RHIResourceState::SHADER_READ:
            break;
        case RHIResourceState::SHADER_WRITE:
            break;
        case RHIResourceState::SHADER_READWRITE:
            break;
        case RHIResourceState::COLOR_ATTACHMENT:
            ResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            break;
        case RHIResourceState::DEPTH_ATTACHMENT:
            ResourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            break;
        case RHIResourceState::COPY_SRC:
            break;
        case RHIResourceState::COPY_DST:
            break;
        case RHIResourceState::PRESENTABLE:
            break;
        default:
            break;
        }
        return ResourceState;
    }
};

// RHID3D12Context
class RHID3D12Context : public IRHIContext
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
    DescriptorHeapAllocator StaticHeapAllocator;
    DescriptorHeapAllocator RTVHeapAllocator;
    DescriptorHeapAllocator DSVHeapAllocator;

    ComPtr<ID3D12DescriptorHeap> m_srvheap;
    ComPtr<ID3D12DescriptorHeap> m_staticheap;
    ComPtr<ID3D12DescriptorHeap> m_rtvheap;
    ComPtr<ID3D12DescriptorHeap> m_dsvheap;

    MSG msg;
    HWND hWnd;
    UINT m_width = 720;
    UINT m_height = 720;
    bool bResized = false;
    LPARAM ResizeParam;
    UINT m_frameIndex;
    DXGI_FORMAT RTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    RHID3D12Context() = default;
    virtual ~RHID3D12Context() override = default;

    virtual void Initialize(uint32_t WindowWidth, uint32_t WindowHeight) override;
    virtual void Cleanup() override;
    virtual void WaitDeviceIdle() override;
    
    virtual void ProcessFrameInput() override;
    virtual bool IsWindowAlive() override;
    virtual std::unique_ptr<IRHICommandBuffer     > CreateRHICommandBuffer      () override;
    virtual std::unique_ptr<IRHIFrameBuffer       > CreateRHIFrameBuffer        () override;
    virtual std::unique_ptr<IRHIImageResource     > CreateRHIImageResource      () override;
    virtual std::unique_ptr<IRHIImGUI             > CreateRHIImGUI              () override;
    virtual std::unique_ptr<IRHIPipelineFactory   > CreateRHIPipelineFactory    () override;
    virtual std::unique_ptr<IRHIPipelineObject    > CreateRHIPipelineObject     () override;
    virtual std::unique_ptr<IRHIRenderPass        > CreateRHIRenderPass         () override;
    virtual std::unique_ptr<IRHISwapchain         > CreateRHISwapchain          () override;
    virtual std::unique_ptr<IRHIBuffer           > CreateRHIBuffer            () override;
    void WaitForPreviousFrame();
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

// RHID3D12ImageResource
class RHID3D12ImageResource : public IRHIImageResource
{
public:
    RHID3D12ImageResource() = default;
    virtual ~RHID3D12ImageResource() override = default;
    D3D12_RESOURCE_DESC textureDesc = {};
    ComPtr<ID3D12Resource> m_texture;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;

    virtual void Initialize(IRHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel = -1) override;
    virtual void Initialize(IRHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel = -1)  override;
    virtual void CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride) override;
    virtual void Cleanup(IRHIContext* Context) override;
    virtual void Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) override;
    virtual void Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState) override;

    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandleSRV;
    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandleSRV;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandleUAV;
    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandleUAV;
    CD3DX12_GPU_DESCRIPTOR_HANDLE RTDSVGpuDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE RTDSVCpuDescriptorHandle;

    D3D12_RESOURCE_STATES ResourceStates;
};

//// RHID3D12Uniform
//class RHID3D12Uniform : public IRHIBuffer
//{
//public:
//    RHID3D12Uniform() = default;
//    virtual ~RHID3D12Uniform() override = default;
//
//    virtual void Initialize(IRHIContext* Context, uint32_t BufferSize, RHIResourceState InState) override;
//    virtual void CopyToBuffer(IRHIContext* Context, void* data, uint32_t TotalBytes) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//
//    void CreateConstantBufferView(RHID3D12Context* Context, ID3D12DescriptorHeap* Heap);
//    //ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
//    ComPtr<ID3D12Resource> m_constantBuffer;
//    UINT8* m_pCbvDataBegin;
//    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
//    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandle;
//    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
//};


class RHID3D12Buffer : public IRHIBuffer
{
public:
    enum class EDescriptorType
    {
        NONE = 0,
        CBV = 1,
        SRV = 2,
        UAV = 4,
    };
    struct DescriptorInfo
    {
	    EDescriptorType Type;
        union DescStruct
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        } Desc;
        CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandle;
        CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
    };
    RHID3D12Buffer() = default;
    virtual ~RHID3D12Buffer() override = default;

    virtual void Initialize(IRHIContext* Context, uint32_t BufferSize, RHIResourceState InState) override
    {
        return Initialize(Context, BufferSize, 1, InState);
    }
    void Initialize(IRHIContext* Context, uint32_t ElementSize, uint32_t NumElements, RHIResourceState InState) override;
    virtual void CopyToBuffer(IRHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(IRHIContext* Context) override;

    void CreateConstantBufferView(RHID3D12Context* Context);
    void CreateUnorderedAccessView(RHID3D12Context* Context);
    void CreateShaderResourceView(RHID3D12Context* Context);
    ComPtr<ID3D12Resource> m_Buffer;
    UINT8* m_pDataBegin;
    std::unique_ptr<DescriptorInfo> pCBVInfo;
    std::unique_ptr<DescriptorInfo> pSRVInfo;
    std::unique_ptr<DescriptorInfo> pUAVInfo;
    uint32_t BufferSize;
    uint32_t ElementSize;
    uint32_t NumElements;
};

// RHID3D12RenderPass
class RHID3D12RenderPass : public IRHIRenderPass
{
public:
    std::vector<DXGI_FORMAT> ColorRTFormats;
    DXGI_FORMAT DepthRTFormat;
    std::vector<RHID3D12ImageResource*> TransitionList;
	RHID3D12RenderPass() = default;
	virtual ~RHID3D12RenderPass() override = default;
	virtual void Initialize(IRHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat) override;
	virtual void Cleanup(IRHIContext* Context) override;

    virtual void BeginRenderPass(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* Framebuffer) override;
    virtual void EndRenderPass(IRHICommandBuffer* CommandBuffer) override;
};

// RHID3D12Pipeline
class RHID3D12PipelineObject : public IRHIPipelineObject
{
public:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    std::vector<D3D12_CONSTANT_BUFFER_VIEW_DESC> cbvDescriptions;
    std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDescriptions;
    std::vector<D3D12_UNORDERED_ACCESS_VIEW_DESC> uavDescriptions;
    //std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> GpuDescriptorHandles;
    std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
    std::vector<RHID3D12ImageResource*> ImageToTransition;
    std::vector<D3D12_VERTEX_BUFFER_VIEW> BoundBufferViews;
    D3D12_INDEX_BUFFER_VIEW BoundIndexBufferView;
    DXGI_FORMAT VertexFormat;
    ID3D12DescriptorHeap* Heap;
    uint32_t HeapCount = 0;
    uint32_t SRVCounts = 0;
    uint32_t UAVCounts = 0;
    uint32_t CBVCounts = 0;
    bool ShouldCopyDescriptors = false;
    std::vector<std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> ConsecutiveDescriptors; // use as a descriptor table, first SRV, then UAV, and CBV
    std::vector<std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> PendingCopyDescriptors; // static descriptors that will be copied to ConsecutiveDescriptors
    RHID3D12PipelineObject() = default;
    virtual ~RHID3D12PipelineObject() override = default;

    virtual void Cleanup(IRHIContext* Context) override;
    
    virtual void SetUniform(IRHIBuffer* Uniform, uint32_t Binding) override;
    virtual void SetStorageBuffer(IRHIBuffer* StorageBuffer, uint32_t Binding) override;
    virtual void SetImageSampler(IRHIImageResource* ImageResource, uint32_t Binding) override;
    virtual void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIBuffer* Buffer) override;
    virtual void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIImageResource* ImageResource) override;
    virtual void BindVertexBuffer(IRHIBuffer* Buffer, uint32_t Offset, uint32_t BindingIndex) override;
    virtual void BindIndexBuffer(IRHIBuffer* Buffer, uint32_t Offset) override;
    virtual void Draw(IRHICommandBuffer* CommandBuffer, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    virtual void Dispatch(IRHICommandBuffer* CommandBuffer, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) override;

    virtual void CopyDescriptors(IRHIContext* Context) override;

    bool bShouldInitHeap = false;
};

class RHID3D12PipelineFactory : public IRHIPipelineFactory
{
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> computeShader;
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
    //std::vector<CD3DX12_DESCRIPTOR_RANGE1> Ranges;
    //std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
    CD3DX12_ROOT_PARAMETER1 RootParam;
    std::vector<DXGI_FORMAT> VertexFormat;
    std::vector<D3D12_STATIC_SAMPLER_DESC> Samplers;
    std::vector<D3D12_VERTEX_BUFFER_VIEW> BufferViews;
    std::vector<CD3DX12_DESCRIPTOR_RANGE1> Ranges;

    uint32_t HeapCount = 0;
    uint32_t SRVCounts = 0;
    uint32_t UAVCounts = 0;
    uint32_t CBVCounts = 0;

public:
    RHID3D12PipelineFactory();
    virtual ~RHID3D12PipelineFactory() override = default;

    virtual void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
    virtual void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) override;
    virtual void RemoveAllBufferBindings() override;
    virtual void SetUniformBinding(uint32_t Binding) override;
    virtual void SetStorageBufferBinding(uint32_t Binding) override;
    virtual void SetImageSamplerBinding(uint32_t Binding) override;
    virtual void RemoveAllGlobalBindings() override;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
    virtual void SetComputeShaders(const std::vector<char>& ComputeShader) override;
    virtual void SetDescriptorBinding(uint32_t BindingIndex, DescriptorType BindingDescriptorType) override;
    virtual void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context, IRHIRenderPass* RenderPassResource) override;
    virtual void InitializeComputePipelineObject(IRHIPipelineObject* OutComputePipelineObject, IRHIContext* Context) override;
    virtual void Cleanup(IRHIContext* Context) override;
};

// RHID3D12ImGUI
class RHID3D12ImGUI : public IRHIImGUI
{
public:
	ImGuiSharedGlobals ImGlobals;
    RHID3D12ImGUI() = default;
    virtual ~RHID3D12ImGUI() override = default;

    virtual void Initialize(IRHIContext* Context, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) override;
    virtual void DispatchImGUI(IRHICommandBuffer* CommandBuffer) override;
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
    virtual void Cleanup() override;
};

class RHID3D12FrameBuffer : public IRHIFrameBuffer {
public:
    uint32_t Height;
    uint32_t Width;
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> ColorRTDescHandles;
    CD3DX12_CPU_DESCRIPTOR_HANDLE DepthRTDescHandle;
    std::vector<DXGI_FORMAT> ColorRTFormats;
    DXGI_FORMAT DepthRTFormat;
    RHID3D12FrameBuffer();
    RHID3D12FrameBuffer(const RHID3D12FrameBuffer&) = delete;
    virtual ~RHID3D12FrameBuffer() override;
    virtual void Initialize(IRHIContext* Context, IRHIRenderPass* RenderPass,
        std::vector<IRHIImageResource*> ColorRTs, IRHIImageResource* DepthRT) override;
    virtual void Cleanup(IRHIContext* Context) override;
};

class RHID3D12Swapchain : public IRHISwapchain
{
public:
    ComPtr<IDXGISwapChain3> m_swapChain;
    const UINT FrameCount = 2;

    ID3D12Resource* m_renderTargets[2];
    ComPtr<ID3D12Resource> m_dsRenderTarget;
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc;
    DXGI_FORMAT DepthFormat;
    RHIFormat SwapchainRHIFormat;

    std::vector<std::unique_ptr<RHID3D12FrameBuffer>> FrameBuffers;
    RHID3D12Swapchain();
    RHID3D12Swapchain(const RHID3D12Swapchain&) = delete;
    virtual ~RHID3D12Swapchain();
    virtual void Initialize(IRHIContext* Context, RHIFormat InSwapchainFormat) override;
    virtual void Cleanup(IRHIContext* Context) override;
    virtual void AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass) override;
    virtual void PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer) override;
    ImageExtent3D GetFrameSize() override;
    RHIFormat GetFrameFormat() override
    {
        return SwapchainRHIFormat;
    }
};

class RHID3D12CommandBuffer : public IRHICommandBuffer
{
public:
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    RHID3D12CommandBuffer() = default;
    RHID3D12CommandBuffer(const RHID3D12CommandBuffer&) = delete;
    virtual ~RHID3D12CommandBuffer() override;

    virtual void Initialize(IRHIContext* Context) override;
    virtual void Cleanup(IRHIContext* Context) override;

    virtual void BeginCommandBuffer() override;
    virtual void EndCommandBuffer() override;
    virtual void ResetCommandBuffer() override;
};
