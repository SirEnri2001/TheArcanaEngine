//#pragma once
//
//#include <dxgi1_4.h>
//#include <directx/d3dx12_core.h>
//#include <directx/d3dx12_root_signature.h>
//#include <wrl/client.h>
//
//#include "RHIImGuiHelper.h"
//#define RHI_IMPLEMENT
//#include "RHI.h"
//
//using Microsoft::WRL::ComPtr;
//
//struct DescriptorHeapAllocator
//{
//    ID3D12DescriptorHeap* Heap = nullptr;
//    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
//    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
//    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
//    UINT                        HeapHandleIncrement;
//    std::vector<int>               FreeIndices;
//
//    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap);
//    void Destroy();
//    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);
//    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle);
//};
//
//// RHID3D12PlatformSupport
//class RHID3D12PlatformSupport : public IRHIPlatformSupport
//{
//public:
//    RHID3D12PlatformSupport() = default;
//    virtual ~RHID3D12PlatformSupport() override = default;
//
//    virtual void Initialize() override;
//    virtual void Cleanup() override;
//};
//
//// RHID3D12Context
//class RHID3D12Context : public IRHIContext
//{
//public:
//    bool m_useWarpDevice = false;
//    ComPtr<ID3D12CommandQueue> m_commandQueue;
//    ComPtr<ID3D12Device> m_device;
//    ComPtr<IDXGIFactory4> factory;
//    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
//    HANDLE m_fenceEvent;
//    ComPtr<ID3D12Fence> m_fence;
//    UINT64 m_fenceValue;
//    DescriptorHeapAllocator SRVHeapAllocator;
//    DescriptorHeapAllocator RTVHeapAllocator;
//    DescriptorHeapAllocator DSVHeapAllocator;
//
//    ComPtr<ID3D12DescriptorHeap> m_srvheap;
//    ComPtr<ID3D12DescriptorHeap> m_rtvheap;
//    ComPtr<ID3D12DescriptorHeap> m_dsvheap;
//
//    RHID3D12Context() = default;
//    virtual ~RHID3D12Context() override = default;
//
//    virtual void Initialize(IRHIPlatformSupport* PlatformSupport) override;
//    virtual void Cleanup() override;
//    virtual void WaitDeviceIdle() override;
//    void WaitForPreviousFrame();
//};
//
//// RHID3D12WindowManager
//class RHID3D12WindowManager : public IRHIWindowManager
//{
//public:
//    MSG msg;
//    HWND hWnd;
//    UINT m_width = 1280;
//    UINT m_height = 720;
//    ComPtr<IDXGISwapChain3> m_swapChain;
//    bool bResized = false;
//    LPARAM ResizeParam;
//    ID3D12Resource* m_renderTargets[2];
//    ComPtr<ID3D12Resource> m_dsRenderTarget;
//    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles;
//    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
//    const UINT FrameCount = 2;
//    UINT m_frameIndex;
//    DXGI_FORMAT RTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
//    RHID3D12WindowManager() = default;
//    virtual ~RHID3D12WindowManager() override = default;
//
//    virtual void Initialize(IRHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
//    virtual void Cleanup(IRHIPlatformSupport* PlatformSupport) override;
//    virtual void InitializeSwapchain(IRHIContext* Context, IRHIPlatformSupport* PlatformSupport) override;
//    virtual void CleanupSwapchain(IRHIContext* Context) override;
//    virtual void RecreateSwapchain(IRHIContext* Context) override;
//    virtual void AddScreenSizeTexture(IRHIImageResource* ImageResource) override;
//    virtual void RemoveScreenSizeTexture(IRHIImageResource* ImageResource) override;
//    virtual void InitializeRenderPassAsPresent(IRHIRenderPass* OutRenderPass, IRHIContext* Context) override;
//    virtual bool IsAlive() override;
//
//    virtual IRHIImageResource* GetColorRenderTarget() override {
//        return nullptr;
//    }
//    virtual IRHIImageResource* GetDepthRenderTarget() override {
//        return nullptr;
//    }
//    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
//    void SetResized(bool val, LPARAM Param){bResized = val; ResizeParam = Param; }
//};
//
//// RHID3D12ImageResource
//class RHID3D12ImageResource : public IRHIImageResource
//{
//public:
//    RHID3D12ImageResource() = default;
//    virtual ~RHID3D12ImageResource() override = default;
//    uint32_t Height;
//    uint32_t Width;
//
//    D3D12_RESOURCE_DESC textureDesc = {};
//    ComPtr<ID3D12Resource> m_texture;
//    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
//
//    virtual void Initialize(IRHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel = -1) override;
//    virtual void Initialize(IRHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel = -1)  override;
//    virtual void InitializeRenderTarget(IRHIContext* Context, IRHIWindowManager* WindowManager, ImageExtent3D RTExtent, RHIResourceState InUsage, uint32_t MultiSamplesCount = 1) override;
//    virtual void CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//    virtual void Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) override;
//    virtual void Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState) override;
//
//    CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHandle;
//    CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle;
//    CD3DX12_GPU_DESCRIPTOR_HANDLE RTDSVGpuDescriptorHandle;
//    CD3DX12_CPU_DESCRIPTOR_HANDLE RTDSVCpuDescriptorHandle;
//
//    D3D12_RESOURCE_STATES ResourceStates;
//};
//
//// RHID3D12BufferResource
//class RHID3D12BufferResource : public IRHIBufferResource
//{
//public:
//    ComPtr<ID3D12Resource> m_buffer;
//    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
//    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
//    RHID3D12BufferResource() = default;
//    virtual ~RHID3D12BufferResource() override = default;
//
//    virtual void Initialize(IRHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) override;
//    virtual void CopyToBuffer(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* data, uint32_t TotalBytes) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//};
//
//// RHID3D12Uniform
//class RHID3D12Uniform : public IRHIUniform
//{
//public:
//    RHID3D12Uniform() = default;
//    virtual ~RHID3D12Uniform() override = default;
//
//    virtual void Initialize(IRHIContext* Context, uint32_t UniformStructSize) override;
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
//
//// RHID3D12RenderPass
//class RHID3D12RenderPass : public IRHIRenderPass
//{
//public:
//    uint32_t Height;
//    uint32_t Width;
//    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> ColorRTs;
//    std::vector<DXGI_FORMAT> ColorRTFormats;
//    std::vector<RHID3D12ImageResource*> TransitionList;
//    CD3DX12_CPU_DESCRIPTOR_HANDLE DepthRT;
//    DXGI_FORMAT DepthRTFormat;
//	RHID3D12RenderPass() = default;
//	virtual ~RHID3D12RenderPass() override = default;
//	virtual void Initialize(IRHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat) override;
//	virtual void Cleanup(IRHIContext* Context) override;
//};
//
//class RHID3D12PresentPass : public IRHIPresentPass
//{
//public:
//	RHID3D12PresentPass() = default;
//	virtual ~RHID3D12PresentPass() override = default;
//	virtual void Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager, uint32_t MSAASamples, IRHIImageResource* ColorRT, IRHIImageResource* DepthRT) override;
//	virtual void Cleanup(IRHIContext* Context) override;
//	virtual void OnWindowResize(IRHIContext* Context, IRHIWindowManager* WindowManager) override;
//};
//
//// RHID3D12Pipeline
//class RHID3D12PipelineObject : public IRHIPipelineObject
//{
//public:
//    ComPtr<ID3D12RootSignature> m_rootSignature;
//    ComPtr<ID3D12PipelineState> m_pipelineState;
//    std::vector<D3D12_CONSTANT_BUFFER_VIEW_DESC> cbvDescriptions;
//    std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDescriptions;
//    std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> GpuDescriptorHandles;
//    std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
//    std::vector<RHID3D12ImageResource*> ImageToTransition;
//    RHID3D12PipelineObject() = default;
//    virtual ~RHID3D12PipelineObject() override = default;
//    
//    virtual void SetUniform(IRHIUniform* Uniform, uint32_t Binding) override;
//    virtual void SetImageSampler(IRHIImageResource* ImageResource, uint32_t Binding) override;
//    virtual void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIUniform* Uniform) override {}
//    virtual void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIImageResource* ImageResource) override {}
//    virtual void Cleanup(IRHIContext* Context) override;
//
//    bool bShouldInitHeap = false;
//};
//
//class RHID3D12PipelineFactory : public IRHIPipelineFactory
//{
//    ComPtr<ID3DBlob> vertexShader;
//    ComPtr<ID3DBlob> pixelShader;
//    ComPtr<ID3DBlob> computeShader;
//    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
//    std::vector<CD3DX12_DESCRIPTOR_RANGE1> Ranges;
//    std::vector<CD3DX12_ROOT_PARAMETER1> RootParameters;
//    std::vector<D3D12_STATIC_SAMPLER_DESC> Samplers;
//
//    uint32_t HeapCount = 0;
//public:
//    RHID3D12PipelineFactory() = default;
//    virtual ~RHID3D12PipelineFactory() override = default;
//
//    virtual void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
//    virtual void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) override;
//    virtual void RemoveAllBufferBindings() override;
//    virtual void SetUniformBinding(uint32_t Binding) override;
//    virtual void SetImageSamplerBinding(uint32_t Binding) override;
//    virtual void RemoveAllGlobalBindings() override;
//    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
//    virtual void SetComputeShaders(const std::vector<char>& ComputeShader) override;
//    virtual void SetDescriptorBinding(uint32_t BindingIndex, DescriptorType BindingDescriptorType) override {}
//    virtual void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context, IRHIRenderPass* RenderPassResource) override;
//    virtual void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context, IRHIPresentPass* PresentPass) override;
//    virtual void InitializeComputePipelineObject(IRHIPipelineObject* OutComputePipelineObject, IRHIContext* Context) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//};
//
//// RHID3D12GraphicsDispatcher
//class RHID3D12GraphicsDispatcher : public IRHIGraphicsDispatcher
//{
//public:
//    std::vector<D3D12_VERTEX_BUFFER_VIEW> BoundBufferViews;
//    D3D12_INDEX_BUFFER_VIEW BoundIndexBufferView;
//    ComPtr<ID3D12GraphicsCommandList> m_commandList;
//    ID3D12DescriptorHeap* pHeaps;
//    RHID3D12GraphicsDispatcher() = default;
//    virtual ~RHID3D12GraphicsDispatcher() override = default;
//
//    virtual void Initialize(IRHIContext* Context) override;
//    virtual void Cleanup(IRHIContext* Context, IRHIWindowManager* WindowManager) override;
//    virtual void BindVertexBuffer(IRHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
//    virtual void BindIndexBuffer(IRHIBufferResource* BufferResource, uint32_t Offset) override;
//    virtual void Draw(IRHICommandBuffer* CommandBuffer, IRHIPipelineObject* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
//    virtual void BeginFrame(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) override;
//	virtual void BeginRenderPass(IRHICommandBuffer* CommandBuffer, IRHIRenderPass* RenderPass, IRHIFrameBuffer* Framebuffer) override;
//    virtual void EndFrameAndSubmit(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, IRHIWindowManager* WindowManager, IRHIFrameBuffer* PresentFrameBuffer = nullptr) override;
//    virtual void EndRenderPass(IRHICommandBuffer* CommandBuffer, IRHIRenderPass* RenderPass) override;
//    virtual void WaitForGPUIdle(IRHIContext* Context) override;
//    virtual void TransitionImageAsShaderRead(IRHICommandBuffer* CommandBuffer, IRHIImageResource* Image) override;
//    virtual void TransitionImageAsRenderTarget(IRHICommandBuffer* CommandBuffer, IRHIImageResource* Image) override;
//};
//
//// RHID3D12ComputeDispatcher
//class RHID3D12ComputeDispatcher : public IRHIComputeDispatcher
//{
//public:
//    ComPtr<ID3D12GraphicsCommandList> m_commandList;
//    ID3D12DescriptorHeap* pHeaps;
//    RHID3D12ComputeDispatcher() = default;
//    virtual ~RHID3D12ComputeDispatcher() override = default;
//
//    virtual void Initialize(IRHIContext* Context) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//    virtual void Dispatch(IRHICommandBuffer* CommandBuffer, IRHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) override;
//    virtual void WaitForGPUIdle(IRHIContext* Context) override;
//};
//
//// RHID3D12ImGUI
//class RHID3D12ImGUI : public IRHIImGUI
//{
//public:
//	ImGuiSharedGlobals ImGlobals;
//    RHID3D12ImGUI() = default;
//    virtual ~RHID3D12ImGUI() override = default;
//
//    virtual void Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) override;
//    virtual void DispatchImGUI(IRHICommandBuffer* CommandBuffer, IRHIGraphicsDispatcher* Dispatcher) override;
//    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
//    virtual void Cleanup() override;
//};
//
//class RHID3D12FrameBuffer : public IRHIFrameBuffer {
//public:
//    RHID3D12FrameBuffer();
//    RHID3D12FrameBuffer(const RHID3D12FrameBuffer&) = delete;
//    virtual ~RHID3D12FrameBuffer() override;
//    virtual void Initialize(IRHIContext* Context, IRHIRenderPass* RenderPass,
//        std::vector<IRHIImageResource*> ColorRTs, IRHIImageResource* DepthRT) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//};
//
//class RHID3D12Swapchain : public IRHISwapchain
//{
//public:
//    RHID3D12Swapchain();
//    RHID3D12Swapchain(const RHID3D12Swapchain&) = delete;
//    virtual ~RHID3D12Swapchain();
//    virtual void Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager) override;
//    virtual void Cleanup(IRHIContext* Context) override;
//    virtual void AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass) override;
//    virtual void PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer, IRHIGraphicsDispatcher* GDispatcher) override;
//    ImageExtent3D GetFrameSize() override;
//};