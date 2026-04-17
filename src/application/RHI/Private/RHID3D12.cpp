#include "RHID3D12.h"

#include <d3dcompiler.h>


#include "RHID3D12Impl.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// Config for example app
static const int APP_NUM_FRAMES_IN_FLIGHT = 2;
static const int APP_NUM_BACK_BUFFERS = 2;
static const int APP_SRV_HEAP_SIZE = 64;

void DescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap, bool hasGpuHandle)
{
    Heap = heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    HeapType = desc.Type;
    HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
    if (hasGpuHandle)
    {
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        bAllocateGPU = true;
    }
    HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
    heapSize = desc.NumDescriptors;
    freeRanges.push_back({ 0, heapSize });
}

void DescriptorHeapAllocator::Destroy()
{
    Heap = nullptr;
}

void DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* OutCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE* OutGPUHandle, uint32_t Size)
{
    for (auto it = freeRanges.begin(); it != freeRanges.end(); ++it)
    {
        if (it->size >= Size)
        {
            uint32_t outStart = it->start;

            it->start += Size;
            it->size -= Size;

            if (it->size == 0)
                freeRanges.erase(it);
            OutCPUHandle->ptr = HeapStartCpu.ptr + outStart * HeapHandleIncrement;
            if (OutGPUHandle && bAllocateGPU)
            {
                OutGPUHandle->ptr = HeapStartGpu.ptr + outStart * HeapHandleIncrement;
            }
            return;
        }
    }
    throw std::runtime_error("Heap is full");
}

void DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle, uint32_t Size)
{
    uint32_t start = (CPUHandle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement;
    auto it = freeRanges.begin();
    for (; it != freeRanges.end(); ++it)
    {
        if (start < it->start)
            break;
    }

    auto inserted = freeRanges.insert(it, { start, Size });
    if (inserted != freeRanges.begin())
    {
        auto prev = std::prev(inserted);
        if (prev->start + prev->size == inserted->start)
        {
            prev->size += inserted->size;
            freeRanges.erase(inserted);
            inserted = prev;
        }
    }
    auto next = std::next(inserted);
    if (next != freeRanges.end() &&
        inserted->start + inserted->size == next->start)
    {
        inserted->size += next->size;
        freeRanges.erase(next);
    }
}

// RHID3D12PlatformSupport implementation
void RHID3D12PlatformSupport::Initialize()
{
}

void RHID3D12PlatformSupport::Cleanup()
{
    // Placeholder implementation
}

std::unique_ptr<IRHIContext> RHID3D12PlatformSupport::CreateRHIContext()
{
    return std::make_unique<RHID3D12Context>();
}

std::unique_ptr<IRHICommandBuffer     > RHID3D12Context::CreateRHICommandBuffer      () { return std::make_unique<RHID3D12CommandBuffer     >(); }
std::unique_ptr<IRHIFrameBuffer       > RHID3D12Context::CreateRHIFrameBuffer        () { return std::make_unique<RHID3D12FrameBuffer       >(); }
std::unique_ptr<IRHIImageResource     > RHID3D12Context::CreateRHIImageResource      () { return std::make_unique<RHID3D12ImageResource     >(); }
std::unique_ptr<IRHIImGUI             > RHID3D12Context::CreateRHIImGUI              () { return std::make_unique<RHID3D12ImGUI             >(); }
std::unique_ptr<IRHIPipelineFactory   > RHID3D12Context::CreateRHIPipelineFactory    () { return std::make_unique<RHID3D12PipelineFactory   >(); }
std::unique_ptr<IRHIPipelineObject    > RHID3D12Context::CreateRHIPipelineObject     () { return std::make_unique<RHID3D12PipelineObject    >(); }
std::unique_ptr<IRHIRenderPass        > RHID3D12Context::CreateRHIRenderPass         () { return std::make_unique<RHID3D12RenderPass        >(); }
std::unique_ptr<IRHISwapchain         > RHID3D12Context::CreateRHISwapchain          () { return std::make_unique<RHID3D12Swapchain         >(); }
std::unique_ptr<IRHIBuffer           > RHID3D12Context::CreateRHIBuffer            () { return std::make_unique<RHID3D12Buffer           >(); }

// RHID3D12Context implementation
void RHID3D12Context::Initialize(uint32_t WindowWidth, uint32_t WindowHeight)
{
    m_width = WindowWidth;
    m_height = WindowHeight;
    // Use HWND for window creation
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = "DXSampleClass";
    CHECK_SYSTEM_ERROR(RegisterClassEx(&windowClass));
    //RECT windowRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    //AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);


    // Create the main window. 
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RECT rect = { 0, 0, (LONG)m_width, (LONG)m_height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    CHECK_SYSTEM_ERROR(hWnd = CreateWindow(
        "DXSampleClass",        // name of window class 
        "TheArcanaEngine - D3D12 API",            // title-bar string 
        WS_OVERLAPPEDWINDOW, // top-level window 
        CW_USEDEFAULT,       // default horizontal position 
        CW_USEDEFAULT,       // default vertical position 
        rect.right - rect.left,
        rect.bottom - rect.top,
        (HWND)NULL,         // no owner window 
        (HMENU)NULL,        // use class menu 
        hInstance,           // handle to application instance 
        (LPVOID)NULL);      // no window-creation data 
    );
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(hWnd, 1);

    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    DescriptorFactory.InitializeFactory(m_device.Get());
    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void RHID3D12Context::Cleanup()
{
    CloseHandle(m_fenceEvent);
    // Placeholder implementation
}

void RHID3D12Context::WaitDeviceIdle()
{
    // Placeholder implementation
}

bool RHID3D12Context::IsWindowAlive()
{
    return true;
}

void RHID3D12Context::ProcessFrameInput()
{
	
}



// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK RHID3D12Context::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RHID3D12Context* Context = reinterpret_cast<RHID3D12Context*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
        
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    RECT* rect;
    switch (message)
    {
    case WM_SIZING:
        rect = reinterpret_cast<RECT*>(lParam); // Not being used yet
        return 0;
    case WM_SIZE:
        //Context->SetResized(true, lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void RHID3D12Context::WaitForPreviousFrame()
{
	    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}


void RHID3D12ImageResource::Initialize(IRHIContext* Context, uint32_t InHeight, uint32_t InWidth, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel)
{
    Initialize(Context, { InWidth, InHeight, 1 }, InFormat, InUsageMask, MipLevel);
}

void RHID3D12ImageResource::Initialize(IRHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);

    //Height = WindowManager->GetWindowHeight();
    //Width = WindowManager->GetWindowWidth();

    // Create the texture.
    // Describe and create a Texture2D.
    textureDesc.MipLevels = 1;
    textureDesc.Format = RHID3D12PlatformSupport::GetDXFormat(InFormat);
    textureDesc.Width = RTExtent.Width;
    textureDesc.Height = RTExtent.Height;
    textureDesc.Flags = HasFlag(InUsageMask, RHIResourceState::DEPTH_ATTACHMENT) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    if (HasFlag(InUsageMask, RHIResourceState::SHADER_WRITE) || HasFlag(InUsageMask, RHIResourceState::SHADER_READWRITE))
    {
        textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    D3D12_CLEAR_VALUE ColorRTClearValue;
    D3D12_CLEAR_VALUE DSClearValue;
    ColorRTClearValue.Format = RHID3D12PlatformSupport::GetDXFormat(InFormat);
    ColorRTClearValue.Color[0] = 1.0;
    ColorRTClearValue.Color[1] = 0.0;
    ColorRTClearValue.Color[2] = 1.0;
    ColorRTClearValue.Color[3] = 1.0;
    DSClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    DSClearValue.DepthStencil.Depth = 1.0;
    DSClearValue.DepthStencil.Stencil = 0;

    ResourceState = D3D12_RESOURCE_STATE_COMMON;
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        ResourceState,
        HasFlag(InUsageMask, RHIResourceState::DEPTH_ATTACHMENT) ? &DSClearValue : &ColorRTClearValue,
        IID_PPV_ARGS(&m_texture)));

    // Describe and create a SRV for the texture.
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = HasFlag(InUsageMask, RHIResourceState::DEPTH_ATTACHMENT) ? DXGI_FORMAT_R32_FLOAT : textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    StaticDescriptorSRV = D3D12Context->DescriptorFactory.CreateStaticDescriptor();
    D3D12Context->m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, StaticDescriptorSRV.CPUHandle);
    if (HasFlag(InUsageMask, RHIResourceState::SHADER_WRITE) || HasFlag(InUsageMask, RHIResourceState::SHADER_READWRITE))
    {
        uavDesc.Format = textureDesc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;
        StaticDescriptorUAV = D3D12Context->DescriptorFactory.CreateStaticDescriptor();
        D3D12Context->m_device->CreateUnorderedAccessView(m_texture.Get(), nullptr, &uavDesc, StaticDescriptorUAV.CPUHandle);
    }
    if (HasFlag(InUsageMask, RHIResourceState::COLOR_ATTACHMENT))
    {
        StaticDescriptorRTDSV = D3D12Context->DescriptorFactory.CreateRTVDescriptor();
        D3D12Context->m_device->CreateRenderTargetView(m_texture.Get(), NULL, StaticDescriptorRTDSV.CPUHandle);
    }
    else if (HasFlag(InUsageMask, RHIResourceState::DEPTH_ATTACHMENT))
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC ds_desc{};
        ds_desc.Texture2D.MipSlice = 1;
        ds_desc.Format = DXGI_FORMAT_D32_FLOAT;
        ds_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        StaticDescriptorRTDSV = D3D12Context->DescriptorFactory.CreateDSVDescriptor();
        D3D12Context->m_device->CreateDepthStencilView(m_texture.Get(), NULL, StaticDescriptorRTDSV.CPUHandle);\
    }
}

 void RHID3D12ImageResource::CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    ComPtr<ID3D12Resource> textureUploadHeap;

    RHID3D12CommandBuffer CmdBuffer;
    CmdBuffer.Initialize(Context);
    CmdBuffer.BeginCommandBuffer();
    // Create the GPU upload buffer.
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = Data;
    textureData.RowPitch = Stride * textureDesc.Width;
    textureData.SlicePitch = Stride * textureDesc.Width * textureDesc.Height;

    UpdateSubresources(CmdBuffer.m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
    CmdBuffer.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    CmdBuffer.EndCommandBuffer();
    ID3D12CommandList* ppCommandLists[] = { CmdBuffer.m_commandList.Get() };
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    D3D12Context->WaitForPreviousFrame();
}

void RHID3D12ImageResource::CopyFromTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* OutData, uint32_t Stride)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    
    UINT64 rowPitch = (Stride * textureDesc.Width + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    UINT64 readbackBufferSize = rowPitch * textureDesc.Height;
    
    ComPtr<ID3D12Resource> readbackBuffer;
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readbackBuffer)));

    RHID3D12CommandBuffer CmdBuffer;
    CmdBuffer.Initialize(Context);
    CmdBuffer.BeginCommandBuffer();
    
    D3D12_RESOURCE_STATES PreviousState = ResourceState;
    CmdBuffer.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), PreviousState, D3D12_RESOURCE_STATE_COPY_SOURCE));
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Footprint.Format = textureDesc.Format;
    footprint.Footprint.Width = textureDesc.Width;
    footprint.Footprint.Height = textureDesc.Height;
    footprint.Footprint.Depth = 1;
    footprint.Footprint.RowPitch = (UINT)rowPitch;

    CD3DX12_TEXTURE_COPY_LOCATION dst(readbackBuffer.Get(), footprint);
    CD3DX12_TEXTURE_COPY_LOCATION src(m_texture.Get(), 0);
    CmdBuffer.m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    
    CmdBuffer.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, PreviousState));
    
    CmdBuffer.EndCommandBuffer();
    ID3D12CommandList* ppCommandLists[] = { CmdBuffer.m_commandList.Get() };
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    D3D12Context->WaitForPreviousFrame();

    // Map and copy back to OutData
    void* mappedData;
    CD3DX12_RANGE readRange(0, readbackBufferSize);
    ThrowIfFailed(readbackBuffer->Map(0, &readRange, &mappedData));
    
    uint8_t* pSrc = reinterpret_cast<uint8_t*>(mappedData);
    uint8_t* pDst = reinterpret_cast<uint8_t*>(OutData);
    for (uint32_t y = 0; y < textureDesc.Height; ++y)
    {
        memcpy(pDst + y * textureDesc.Width * Stride, pSrc + y * rowPitch, textureDesc.Width * Stride);
    }
    
    readbackBuffer->Unmap(0, nullptr);
}

void RHID3D12ImageResource::Cleanup(IRHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    // Placeholder implementation
}

void RHID3D12ImageResource::Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) {

}

void RHID3D12ImageResource::Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState)
{
    RHID3D12CommandBuffer* CmdBuffer = static_cast<RHID3D12CommandBuffer*>(CommandBuffer);
	CmdBuffer->m_commandList->ResourceBarrier(1, 
        &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), ResourceState, RHID3D12PlatformSupport::GetDXResourceStates(InState)));
    ResourceState = RHID3D12PlatformSupport::GetDXResourceStates(InState);
}

void RHID3D12Buffer::Initialize(IRHIContext* Context, uint32_t ElementSize, uint32_t NumElements, RHIResourceState InState)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);        // Describe and create a constant buffer view (CBV) descriptor heap.
    // Flags indicate that this descriptor heap can be bound to the pipeline 
    // and that descriptors contained in it can be referenced by a root table.
    if (HasFlag(InState, RHIResourceState::BUFFER_UNIFORM))
    {
        BufferSize = (ElementSize * NumElements - 1) / 256 * 256 + 256;
    }
    else
    {
        BufferSize = ElementSize * NumElements;
    }
    this->ElementSize = ElementSize;
    this->NumElements = NumElements;
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(BufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_Buffer)));
    bool CreateCBV = false, CreateUAV = false, CreateSRV = false;
    if (HasFlag(InState, RHIResourceState::BUFFER_UNIFORM))
    {
        CreateCBV = true;
    }
    if (HasFlag(InState, RHIResourceState::BUFFER_SHADER_STORAGE))
    {
        CreateSRV = true;
        CreateUAV = true;
    }
    if (CreateCBV)
    {
        CreateConstantBufferView(D3D12Context);
    }
    if (CreateSRV)
    {
        CreateShaderResourceView(D3D12Context);
    }
    if (CreateUAV)
    {
        CreateUnorderedAccessView(D3D12Context);
    }
}


void RHID3D12Buffer::Cleanup(IRHIContext* Context)
{
	
}

void RHID3D12Buffer::CreateConstantBufferView(RHID3D12Context* Context)
{
    pCBVInfo = std::make_unique<DescriptorInfo>();
    pCBVInfo->Desc.cbvDesc.BufferLocation = m_Buffer->GetGPUVirtualAddress();
    pCBVInfo->Desc.cbvDesc.SizeInBytes = BufferSize;
    pCBVInfo->Handle = Context->DescriptorFactory.CreateStaticDescriptor();
    Context->m_device->CreateConstantBufferView(&pCBVInfo->Desc.cbvDesc, pCBVInfo->Handle.CPUHandle);
}

void RHID3D12Buffer::CreateUnorderedAccessView(RHID3D12Context* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    pUAVInfo = std::make_unique<DescriptorInfo>();
    pUAVInfo->Desc.uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    pUAVInfo->Desc.uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    pUAVInfo->Desc.uavDesc.Buffer.FirstElement = 0;
    pUAVInfo->Desc.uavDesc.Buffer.NumElements = NumElements * ElementSize / 4;
    pUAVInfo->Desc.uavDesc.Buffer.StructureByteStride = 0;
	pUAVInfo->Desc.uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    pUAVInfo->Handle = Context->DescriptorFactory.CreateStaticDescriptor();
    Context->m_device->CreateUnorderedAccessView(m_Buffer.Get(), nullptr, &pUAVInfo->Desc.uavDesc, pUAVInfo->Handle.CPUHandle);
}

void RHID3D12Buffer::CreateShaderResourceView(RHID3D12Context* Context)
{
    pSRVInfo = std::make_unique<DescriptorInfo>();
    pSRVInfo->Desc.srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    pSRVInfo->Desc.srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    pSRVInfo->Desc.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    pSRVInfo->Desc.srvDesc.Buffer.FirstElement = 0;
    pSRVInfo->Desc.srvDesc.Buffer.NumElements = NumElements*ElementSize / 4;
    pSRVInfo->Desc.srvDesc.Buffer.StructureByteStride = 0;
    pSRVInfo->Desc.srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    pSRVInfo->Handle = Context->DescriptorFactory.CreateStaticDescriptor();
    Context->m_device->CreateShaderResourceView(m_Buffer.Get(), &pSRVInfo->Desc.srvDesc, pSRVInfo->Handle.CPUHandle);
}

void RHID3D12Buffer::CopyToBuffer(IRHIContext* Context, void* data, uint32_t TotalBytes)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    ComPtr<ID3D12Resource> UploadBuffer;
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(BufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&UploadBuffer)));
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    UINT8* MappedPtr;
    ThrowIfFailed(UploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&MappedPtr)));
    memcpy(MappedPtr, data, TotalBytes);
    UploadBuffer->Unmap(0, nullptr);

    RHID3D12CommandBuffer CmdBuffer;
    CmdBuffer.Initialize(Context);
    CmdBuffer.BeginCommandBuffer();
    // Create the command list.
    CmdBuffer.m_commandList->CopyBufferRegion(m_Buffer.Get(), 0, UploadBuffer.Get(), 0, BufferSize);
    CmdBuffer.EndCommandBuffer();
    ID3D12CommandList* ppCommandLists[] = { CmdBuffer.m_commandList.Get() };
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    D3D12Context->WaitForPreviousFrame();
}


DXGI_FORMAT GetFormat(RHIFormat Format)
{
	switch (Format)
	{
	case R8G8B8A8_SRGB:
        return DXGI_FORMAT_R32G32B32_FLOAT;
	case R32G32B32_SFLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
	case R32G32B32A32_SFLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case R32G32_SFLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
	}
}

RHID3D12PipelineFactory::RHID3D12PipelineFactory()
{
}

void RHID3D12PipelineFactory::AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
    // Define the vertex input layout.
    inputElementDescs.push_back({ "TEXCOORD", Location,
        GetFormat(Format), BindingIndex, Offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
}

void RHID3D12PipelineFactory::AddBufferBinding(uint32_t BindingIndex, uint32_t Stride)
{
    if (BufferViews.size() < BindingIndex + 1)
    {
        BufferViews.resize(BindingIndex + 1);
    }
    BufferViews[BindingIndex].StrideInBytes = Stride;
}

void RHID3D12PipelineFactory::RemoveAllBufferBindings()
{
    inputElementDescs.clear();
}

void RHID3D12PipelineFactory::SetDescriptorBinding(uint32_t BindingIndex, DescriptorType BindingDescriptorType, EPipelineStages BindingStage)
{
    RootParam.DescriptorTable.NumDescriptorRanges++;
    if (Ranges.size() <= BindingIndex)
    {
        Ranges.resize(BindingIndex + 1);
    }
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = BindingIndex;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    CD3DX12_DESCRIPTOR_RANGE1 UAVRange, SRVRange, CBVRange;
    switch (BindingDescriptorType)
    {
    case DescriptorType::IMAGE2D:
        UAVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, BindingIndex);
        Ranges[BindingIndex] = UAVRange;
        UAVCounts++;
        break;
    case DescriptorType::SAMPLER2D:
        SRVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, BindingIndex);
        Ranges[BindingIndex] = SRVRange;
        SRVCounts++;
        Samplers.push_back(sampler);
        break;
    case DescriptorType::UNIFORM:
        CBVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, BindingIndex);
        Ranges[BindingIndex] = CBVRange;
        CBVCounts++;
        break;
    case DescriptorType::STORAGE_READONLY:
        SRVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, BindingIndex);
        Ranges[BindingIndex] = SRVRange;
        SRVCounts++;
        break;
    case DescriptorType::STORAGE:
        UAVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, BindingIndex);
        Ranges[BindingIndex] = UAVRange;
        UAVCounts++;
        break;
    }
}


void RHID3D12PipelineFactory::RemoveAllGlobalBindings()
{
    Samplers.clear();
}

void RHID3D12PipelineFactory::SetShaders(const std::vector<char>& VertShaderSPIRV, const std::vector<char>& FragShaderSPIRV)
{
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    ComPtr<ID3DBlob> ErrorMsg;
    auto VertHLSL = SPIRVToHLSL(reinterpret_cast<const uint32_t*>(VertShaderSPIRV.data()), VertShaderSPIRV.size()/4);
    auto FragHLSL = SPIRVToHLSL(reinterpret_cast<const uint32_t*>(FragShaderSPIRV.data()), FragShaderSPIRV.size()/4);
    D3DCompile(VertHLSL.data(), VertHLSL.size(), NULL, nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexShader, &ErrorMsg);
    if(ErrorMsg.Get())
    {
	    Error("Compile vertex shader error: ", reinterpret_cast<char*>(ErrorMsg->GetBufferPointer()));
    }
    D3DCompile(FragHLSL.data(), FragHLSL.size(), NULL, nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pixelShader, &ErrorMsg);
    if(ErrorMsg.Get())
    {
	    Error("Compile fragment shader error: ", reinterpret_cast<char*>(ErrorMsg->GetBufferPointer()));
    }
}

void RHID3D12PipelineFactory::SetComputeShaders(const std::vector<char>& ComputeShader)
{
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    ComPtr<ID3DBlob> ErrorMsg;
    auto CompHLSL = SPIRVToHLSL(reinterpret_cast<const uint32_t*>(ComputeShader.data()), ComputeShader.size() / 4);
    D3DCompile(CompHLSL.data(), CompHLSL.size(), NULL, nullptr, nullptr, "main", "cs_5_1", compileFlags, 0, &computeShader, &ErrorMsg);
    if (ErrorMsg.Get())
    {
        Error("Compile compute shader error: ", reinterpret_cast<char*>(ErrorMsg->GetBufferPointer()));
    }
}
void RHID3D12PipelineFactory::InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context, IRHIRenderPass* RenderPassResource)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    auto* D3D12PipelineObject = static_cast<RHID3D12PipelineObject*>(OutPipelineObject);
    auto* D3D12RenderPassResource = static_cast<RHID3D12RenderPass*>(RenderPassResource);

    RootParam.InitAsDescriptorTable(Ranges.size(), Ranges.data());
    D3D12PipelineObject->RootParameters.resize(1);
    D3D12PipelineObject->RootParameters[0] = RootParam;
    D3D12PipelineObject->Heap = D3D12Context->DescriptorFactory.Heap_GPU.Get();
    D3D12PipelineObject->DescriptorTable = D3D12Context->DescriptorFactory.CreateGPUDescriptorTable(SRVCounts + UAVCounts + CBVCounts);

	// Create an empty root signature.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(D3D12Context->m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(1,
            &RootParam,
            Samplers.size(), Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        if(error.Get())
	    {
		    Error("D3DX12SerializeVersionedRootSignature error: ", reinterpret_cast<char*>(error->GetBufferPointer()));
	    }
        ThrowIfFailed(D3D12Context->m_device->CreateRootSignature(0, signature->GetBufferPointer(), 
            signature->GetBufferSize(), IID_PPV_ARGS(&D3D12PipelineObject->m_rootSignature)));
        assert(D3D12PipelineObject->m_rootSignature);
        D3D12PipelineObject->BoundBufferViews = BufferViews;
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ inputElementDescs.data(), static_cast<unsigned int>(inputElementDescs.size()) };
        psoDesc.pRootSignature = D3D12PipelineObject->m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = D3D12RenderPassResource->ColorRTFormats.size();
        DXGI_FORMAT* PSOColorFormats = psoDesc.RTVFormats;
        memcpy(PSOColorFormats, D3D12RenderPassResource->ColorRTFormats.data(), min(D3D12RenderPassResource->ColorRTFormats.size(), 8));
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = Samplers.size();
        ThrowIfFailed(D3D12Context->m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&D3D12PipelineObject->m_pipelineState)));
    }
}

void RHID3D12PipelineFactory::InitializeComputePipelineObject(IRHIPipelineObject* OutComputePipelineObject, IRHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    auto* D3D12PipelineObject = static_cast<RHID3D12PipelineObject*>(OutComputePipelineObject);

    RootParam.InitAsDescriptorTable(Ranges.size(), Ranges.data());
    D3D12PipelineObject->RootParameters.resize(1);
    D3D12PipelineObject->RootParameters[0] = RootParam;
    D3D12PipelineObject->Heap = D3D12Context->DescriptorFactory.Heap_GPU.Get();
    D3D12PipelineObject->DescriptorTable = D3D12Context->DescriptorFactory.CreateGPUDescriptorTable(SRVCounts + UAVCounts + CBVCounts);

    // Create root signature for compute pipeline
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(D3D12Context->m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(1, 
            &RootParam,
            Samplers.size(), Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_NONE);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if(error.Get())
        {
            Error("D3DX12SerializeVersionedRootSignature error: ", reinterpret_cast<char*>(error->GetBufferPointer()));
        }
        ThrowIfFailed(D3D12Context->m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&D3D12PipelineObject->m_rootSignature)));
        assert(D3D12PipelineObject->m_rootSignature);
    }

    // Create the compute pipeline state object (PSO)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = D3D12PipelineObject->m_rootSignature.Get();
        psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
        
        ThrowIfFailed(D3D12Context->m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&D3D12PipelineObject->m_pipelineState)));
    }
}

void RHID3D12PipelineFactory::Cleanup(IRHIContext* Context)
{
    // TODO: Implement D3D12 pipeline factory cleanup
}

// RHID3D12PipelineObject implementation
void RHID3D12PipelineObject::Cleanup(IRHIContext* Context)
{
    // Free the descriptor table
    if (DescriptorTable.ConsecutiveHandles.size() > 0) {
        DescriptorTable.Free();
        DescriptorTable.ConsecutiveHandles.clear();
    }
}

void RHID3D12PipelineObject::SetUniform(IRHIBuffer* Buffer, uint32_t Binding)
{
    auto* D3D12Buffer = static_cast<RHID3D12Buffer*>(Buffer);
    if (Binding>=cbvDescriptions.size())
    {
        cbvDescriptions.resize(Binding + 1);
    }
    cbvDescriptions[Binding] = D3D12Buffer->pCBVInfo->Desc.cbvDesc;
    DescriptorTable.SetDescriptorHandle(Binding, D3D12Buffer->pCBVInfo->Handle);
}

void RHID3D12PipelineObject::SetStorageBuffer(IRHIBuffer* StorageBuffer, uint32_t Binding)
{
    auto* D3D12Buffer = static_cast<RHID3D12Buffer*>(StorageBuffer);
    if (Binding >= uavDescriptions.size())
    {
        uavDescriptions.resize(Binding + 1);
    }
    uavDescriptions[Binding] = D3D12Buffer->pUAVInfo->Desc.uavDesc;
    DescriptorTable.SetDescriptorHandle(Binding, D3D12Buffer->pUAVInfo->Handle);
}

void RHID3D12PipelineObject::SetImageSampler(IRHIImageResource* ImageResource, uint32_t Binding)
{
    auto* D3D12ImageResource = static_cast<RHID3D12ImageResource*>(ImageResource);
    if (Binding>=srvDescriptions.size())
    {
        srvDescriptions.resize(Binding + 1);
    }
    srvDescriptions[Binding] = D3D12ImageResource->srvDesc;
    ImageToTransition.push_back(D3D12ImageResource);
    DescriptorTable.SetDescriptorHandle(Binding, D3D12ImageResource->StaticDescriptorSRV);
}

void RHID3D12PipelineObject::SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIImageResource* ImageResource)
{
    auto* D3D12ImageResource = static_cast<RHID3D12ImageResource*>(ImageResource);
    switch (BindingDescriptorType)
    {
    case IMAGE2D:
        if (BindingIndex >= uavDescriptions.size())
        {
            uavDescriptions.resize(BindingIndex + 1);
        }
        uavDescriptions[BindingIndex] = D3D12ImageResource->uavDesc;
        DescriptorTable.SetDescriptorHandle(BindingIndex, D3D12ImageResource->StaticDescriptorUAV);
        break;
    case SAMPLER2D:
        if (BindingIndex >= srvDescriptions.size())
        {
            srvDescriptions.resize(BindingIndex + 1);
        }
        srvDescriptions[BindingIndex] = D3D12ImageResource->srvDesc;
        DescriptorTable.SetDescriptorHandle(BindingIndex, D3D12ImageResource->StaticDescriptorSRV);
        ImageToTransition.push_back(D3D12ImageResource);
        break;
    }
    
}

void RHID3D12PipelineObject::SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIBuffer* Buffer)
{
    auto* D3D12Buffer = static_cast<RHID3D12Buffer*>(Buffer);
    switch (BindingDescriptorType)
    {
    case DescriptorType::UNIFORM:
        SetUniform(Buffer, BindingIndex);
        break;
    case DescriptorType::STORAGE_READONLY:
        if (BindingIndex >= srvDescriptions.size())
        {
            srvDescriptions.resize(BindingIndex + 1);
        }
        srvDescriptions[BindingIndex] = D3D12Buffer->pSRVInfo->Desc.srvDesc;
        DescriptorTable.SetDescriptorHandle(BindingIndex, D3D12Buffer->pSRVInfo->Handle);
        break;
    case DescriptorType::STORAGE:
        SetStorageBuffer(Buffer, BindingIndex);
        break;
    case DescriptorType::IMAGE2D:
    case DescriptorType::SAMPLER2D:
        throw std::runtime_error("Cannot use this function bind sampler2d | image2d");

    }
}

void RHID3D12PipelineObject::BindVertexBuffer(IRHIBuffer* Buffer, uint32_t Offset, uint32_t BindingIndex)
{
    auto* D3D12Buffer = static_cast<RHID3D12Buffer*>(Buffer);
    BoundBufferViews[BindingIndex].BufferLocation = D3D12Buffer->m_Buffer->GetGPUVirtualAddress() + Offset;
    BoundBufferViews[BindingIndex].SizeInBytes = D3D12Buffer->BufferSize;
}

void RHID3D12PipelineObject::BindIndexBuffer(IRHIBuffer* Buffer, uint32_t Offset)
{
    auto* D3D12Buffer = static_cast<RHID3D12Buffer*>(Buffer);
    BoundIndexBufferView.BufferLocation = D3D12Buffer->m_Buffer->GetGPUVirtualAddress();
    BoundIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    BoundIndexBufferView.SizeInBytes = D3D12Buffer->BufferSize;
}

void RHID3D12PipelineObject::CopyDescriptors(IRHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    DescriptorTable.UploadTable(D3D12Context->m_device.Get());
}


void RHID3D12PipelineObject::Draw(IRHICommandBuffer* CommandBuffer, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
    ComPtr<ID3D12GraphicsCommandList> cmdList = static_cast<RHID3D12CommandBuffer*>(CommandBuffer)->m_commandList;
    for (auto* ImageResource : ImageToTransition)
    {
        if (ImageResource->ResourceState != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
        {
            cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                ImageResource->m_texture.Get(),
                ImageResource->ResourceState, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
            ImageResource->ResourceState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        }
    }
    // Set necessary state.
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetDescriptorHeaps(1, &Heap);
    assert(RootParameters.size() == 1 && RootParameters[0].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
    cmdList->SetGraphicsRootDescriptorTable(0, std::get<1>(DescriptorTable.ConsecutiveHandles[0]));
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, BoundBufferViews.size(), BoundBufferViews.data());
    cmdList->IASetIndexBuffer(&BoundIndexBufferView);
    cmdList->DrawIndexedInstanced(IndexCount, InstanceCount, IndexOffset, 0, 0);
}

void RHID3D12PipelineObject::Dispatch(IRHICommandBuffer* CommandBuffer, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ)
{
    ComPtr<ID3D12GraphicsCommandList> cmdList = static_cast<RHID3D12CommandBuffer*>(CommandBuffer)->m_commandList;
    // Set compute pipeline state
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->SetComputeRootSignature(m_rootSignature.Get());
    cmdList->SetDescriptorHeaps(1, &Heap);

    // Set root parameters (similar to graphics dispatcher)
    assert(RootParameters.size() == 1 && RootParameters[0].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
    cmdList->SetComputeRootDescriptorTable(0, std::get<1>(DescriptorTable.ConsecutiveHandles[0]));
    // Dispatch compute shader
    cmdList->Dispatch(ThreadGroupX, ThreadGroupY, ThreadGroupZ);
}


void RHID3D12RenderPass::BeginRenderPass(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* Framebuffer)
{
    RHID3D12FrameBuffer* D3D12FB = static_cast<RHID3D12FrameBuffer*>(Framebuffer);
    ComPtr<ID3D12GraphicsCommandList> cmdList = static_cast<RHID3D12CommandBuffer*>(CommandBuffer)->m_commandList;
    CD3DX12_VIEWPORT m_viewport(0., 0., D3D12FB->Width, D3D12FB->Height);
    CD3DX12_RECT m_scissorRect{};
    m_scissorRect = CD3DX12_RECT(0., 0., D3D12FB->Width, D3D12FB->Height);

    for(auto* ImageResource : TransitionList)
    {
        if(ImageResource->ResourceState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				ImageResource->m_texture.Get(),
				ImageResource->ResourceState, D3D12_RESOURCE_STATE_RENDER_TARGET));
	        ImageResource->ResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
    }

    // Record commands.
    cmdList->RSSetViewports(1, &m_viewport);
    cmdList->RSSetScissorRects(1, &m_scissorRect);
    cmdList->OMSetRenderTargets(D3D12FB->ColorRTDescHandles.size(), D3D12FB->ColorRTDescHandles.data(),
        FALSE, &D3D12FB->DepthRTDescHandle);
    const float clearColor[] = { 1.0f, 0.0f, 1.0f, 1.0f };
    for(auto& ColorRT : D3D12FB->ColorRTDescHandles)
    {
        cmdList->ClearRenderTargetView(ColorRT, clearColor, 0, nullptr);
    }
    cmdList->ClearDepthStencilView(D3D12FB->DepthRTDescHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0, 0, 0, NULL);
}

void RHID3D12RenderPass::EndRenderPass(IRHICommandBuffer* CommandBuffer)
{
	
}


void RHID3D12RenderPass::Initialize(IRHIContext* Context, std::vector<RHIFormat> InColorRTFormats, bool hasDepth, RHIFormat InDepthFormat)
{
    ColorRTFormats.resize(InColorRTFormats.size());
    for (int i = 0; i < InColorRTFormats.size(); i++)
    {
        ColorRTFormats[i] = RHID3D12PlatformSupport::GetDXFormat(InColorRTFormats[i]);
    }
    DepthRTFormat = RHID3D12PlatformSupport::GetDXFormat(InDepthFormat);
}

void RHID3D12RenderPass::Cleanup(IRHIContext* Context)
{
}


static DescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
// RHID3D12ImGUI implementation
void RHID3D12ImGUI::Initialize(IRHIContext* Context, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    auto* D3D12PresentPass = static_cast<RHID3D12RenderPass*>(PresentRenderPass);
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGlobals.Context = ImGui::CreateContext();
	ImGui::GetAllocatorFunctions(&ImGlobals.MemAllocFunc, &ImGlobals.MemFreeFunc, &ImGlobals.UserData);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(D3D12Context->hWnd);

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = D3D12Context->m_device.Get();
    init_info.CommandQueue = D3D12Context->m_commandQueue.Get();
    init_info.NumFramesInFlight = APP_NUM_FRAMES_IN_FLIGHT;
    init_info.RTVFormat = D3D12PresentPass->ColorRTFormats[0];
    init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)


    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = APP_SRV_HEAP_SIZE;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (D3D12Context->m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            throw std::runtime_error("Cannot create heap for imgui");
        g_pd3dSrvDescHeapAlloc.Create(D3D12Context->m_device.Get(), g_pd3dSrvDescHeap, true);
    }

    init_info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    {
        return g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
    };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
	    return g_pd3dSrvDescHeapAlloc.Free(cpu_handle, 1);
    };
    ImGui_ImplDX12_Init(&init_info);
}

void RHID3D12ImGUI::DispatchImGUI(IRHICommandBuffer* CommandBuffer)
{
    ComPtr<ID3D12GraphicsCommandList> cmdList = static_cast<RHID3D12CommandBuffer*>(CommandBuffer)->m_commandList;
    MSG msg;
    if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    cmdList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList.Get());
}

void RHID3D12ImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context))
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
	pFuncDrawUI(&ImGlobals);
    // Rendering
    ImGui::Render();
}

void RHID3D12ImGUI::Cleanup()
{    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

RHID3D12FrameBuffer::RHID3D12FrameBuffer() {

}
RHID3D12FrameBuffer::~RHID3D12FrameBuffer() {

}
void RHID3D12FrameBuffer::Initialize(IRHIContext* Context, IRHIRenderPass* RenderPass,
    std::vector<IRHIImageResource*> InColorRTs, IRHIImageResource* InDepthRT) {
    RHID3D12ImageResource* D3D12DepthRT = static_cast<RHID3D12ImageResource*>(InDepthRT);
    DepthRTDescHandle = D3D12DepthRT->StaticDescriptorRTDSV.CPUHandle;
    DepthRTFormat = D3D12DepthRT->textureDesc.Format;
    Height = D3D12DepthRT->textureDesc.Height;
    Width = D3D12DepthRT->textureDesc.Width;
    size_t i = 0;
    ColorRTDescHandles.resize(InColorRTs.size());
    ColorRTFormats.resize(InColorRTs.size());
    for (IRHIImageResource* InColorRT : InColorRTs)
    {
        RHID3D12ImageResource* D3D12ColorRT = static_cast<RHID3D12ImageResource*>(InColorRT);
        ColorRTDescHandles[i] = D3D12ColorRT->StaticDescriptorRTDSV.CPUHandle;
        ColorRTFormats[i] = D3D12ColorRT->textureDesc.Format;
        i++;
    }
}
void RHID3D12FrameBuffer::Cleanup(IRHIContext* Context) {

}

RHID3D12Swapchain::RHID3D12Swapchain()
{
	
}

RHID3D12Swapchain::~RHID3D12Swapchain()
{
	
}

void RHID3D12Swapchain::Initialize(IRHIContext* Context, RHIFormat InSwapchainFormat)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    SwapchainRHIFormat = InSwapchainFormat;
    // Describe and create the swap chain.
    SwapChainDesc = DXGI_SWAP_CHAIN_DESC1{};
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.Width = D3D12Context->m_width;
    SwapChainDesc.Height = D3D12Context->m_height;
    SwapChainDesc.Format = RHID3D12PlatformSupport::GetDXFormat(InSwapchainFormat);
    // cannot specify SRGB in swapchain, we define SRGB in RTV
    if (InSwapchainFormat==RHIFormat::B8G8R8A8_SRGB)
    {
        SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    if (InSwapchainFormat==RHIFormat::R8G8B8A8_SRGB)
    {
        SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(D3D12Context->factory->CreateSwapChainForHwnd(
        D3D12Context->m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        D3D12Context->hWnd,
        &SwapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));
    SwapChainDesc.Format = RHID3D12PlatformSupport::GetDXFormat(InSwapchainFormat);

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(D3D12Context->factory->MakeWindowAssociation(D3D12Context->hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    // Create frame resources.
    // Create a RTV for each frame.
    m_rtvHandles.resize(FrameCount);

    for (UINT n = 0; n < FrameCount; n++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&pBackBuffer)));
        D3D12Context->DescriptorFactory.RTVAllocator.Alloc(&m_rtvHandles[n], nullptr);
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = RHID3D12PlatformSupport::GetDXFormat(InSwapchainFormat);
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        D3D12Context->m_device->CreateRenderTargetView(pBackBuffer, &rtvDesc, m_rtvHandles[n]);
        m_renderTargets[n] = pBackBuffer;
    }
    // Create the texture.
// Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_D32_FLOAT;
    textureDesc.Width = D3D12Context->m_width;
    textureDesc.Height = D3D12Context->m_height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    D3D12_CLEAR_VALUE DSClearValue;
    DSClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    DSClearValue.DepthStencil.Depth = 1.0;
    DSClearValue.DepthStencil.Stencil = 0;

    DepthFormat = DXGI_FORMAT_D32_FLOAT;

    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &DSClearValue,
        IID_PPV_ARGS(&m_dsRenderTarget)));

    D3D12Context->DescriptorFactory.DSVAllocator.Alloc(&dsvHandle, nullptr);
    D3D12Context->m_device->CreateDepthStencilView(m_dsRenderTarget.Get(), NULL, dsvHandle);

    FrameBuffers.resize(FrameCount);
    for (int i = 0; i < FrameCount; i ++)
    {
        FrameBuffers[i] = std::make_unique<RHID3D12FrameBuffer>();
        RHID3D12FrameBuffer* D3D12FB = FrameBuffers[i].get();
        D3D12FB->Height = SwapChainDesc.Height;
        D3D12FB->Width = SwapChainDesc.Width;
        D3D12FB->ColorRTDescHandles.resize(1);
        D3D12FB->ColorRTFormats.resize(1);
        D3D12FB->ColorRTDescHandles[0] = m_rtvHandles[i];
        D3D12FB->ColorRTFormats[0] = RHID3D12PlatformSupport::GetDXFormat(InSwapchainFormat);
        D3D12FB->DepthRTDescHandle = dsvHandle;
        D3D12FB->DepthRTFormat = DepthFormat;
    }
}

void RHID3D12Swapchain::Cleanup(IRHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    D3D12Context->WaitForPreviousFrame();
    for (auto* rt_buffer : m_renderTargets)
    {
        rt_buffer->Release();
    }
}

void RHID3D12Swapchain::AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    OutFrameBuffer = FrameBuffers[m_swapChain->GetCurrentBackBufferIndex()].get();
    RHID3D12CommandBuffer TransitionCmdBuffer;
    TransitionCmdBuffer.Initialize(Context);
    TransitionCmdBuffer.BeginCommandBuffer();
    TransitionCmdBuffer.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_swapChain->GetCurrentBackBufferIndex()],
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    TransitionCmdBuffer.EndCommandBuffer();
    ID3D12CommandList* pCommandList1 = TransitionCmdBuffer.m_commandList.Get();
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, &pCommandList1);

    D3D12Context->WaitForPreviousFrame();

}

void RHID3D12Swapchain::PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer)
{
    RHID3D12CommandBuffer TransitionCmdBuffer;
    TransitionCmdBuffer.Initialize(Context);
    TransitionCmdBuffer.BeginCommandBuffer();


    ComPtr<ID3D12GraphicsCommandList> cmdList = static_cast<RHID3D12CommandBuffer*>(CommandBuffer)->m_commandList;
    //ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    // Indicate that the back buffer will now be used to present.
    TransitionCmdBuffer.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_swapChain->GetCurrentBackBufferIndex()],
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    TransitionCmdBuffer.EndCommandBuffer();
    // Execute the command list.
    ID3D12CommandList* pCommandList = cmdList.Get();
    ID3D12CommandList* pCommandList1 = TransitionCmdBuffer.m_commandList.Get();
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, &pCommandList);
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, &pCommandList1);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));
    D3D12Context->WaitForPreviousFrame();
}

ImageExtent3D RHID3D12Swapchain::GetFrameSize()
{
    return ImageExtent3D{ SwapChainDesc.Width, SwapChainDesc.Height, 1};
}

void RHID3D12CommandBuffer::Initialize(IRHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context);
    // Create the command list.
    ThrowIfFailed(D3D12Context->m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    ThrowIfFailed(D3D12Context->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(),
        NULL, IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_commandList->Close());
}

void RHID3D12CommandBuffer::Cleanup(IRHIContext* Context)
{
	
}

RHID3D12CommandBuffer::~RHID3D12CommandBuffer()
{
	
}


void RHID3D12CommandBuffer::BeginCommandBuffer()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
}

void RHID3D12CommandBuffer::EndCommandBuffer()
{
    ThrowIfFailed(m_commandList->Close());
}

void RHID3D12CommandBuffer::ResetCommandBuffer()
{
	
}

void D3D12DescriptorHandle::Free()
{
    Allocator->Free(CPUHandle, 1);
}

void D3D12DescriptorTable::Free()
{
    Allocator->Free(std::get<0>(ConsecutiveHandles[0]), ConsecutiveHandles.size());
}


D3D12DescriptorFactory::D3D12DescriptorFactory()
{

}

void D3D12DescriptorFactory::InitializeFactory(ID3D12Device* Device)
{
    D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap_GPU)));
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap_Static)));
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap_RTV)));
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap_DSV)));

    GPUAllocator.Create(Device, Heap_GPU.Get(), true);
    StaticAllocator.Create(Device, Heap_Static.Get(), false);
    RTVAllocator.Create(Device, Heap_RTV.Get(), false);
    DSVAllocator.Create(Device, Heap_DSV.Get(), false);
}

D3D12DescriptorHandle D3D12DescriptorFactory::CreateStaticDescriptor()
{
    D3D12DescriptorHandle OutDescriptor;
    StaticAllocator.Alloc(&OutDescriptor.CPUHandle, &OutDescriptor.GPUHandle);
    OutDescriptor.Type = D3D12DescriptorHandle::EType::STATIC;
    OutDescriptor.Allocator = &StaticAllocator;
    return OutDescriptor;
}

D3D12DescriptorHandle D3D12DescriptorFactory::CreateGPUDescriptor()
{
    D3D12DescriptorHandle OutDescriptor;
    GPUAllocator.Alloc(&OutDescriptor.CPUHandle, &OutDescriptor.GPUHandle);
    OutDescriptor.Type = D3D12DescriptorHandle::EType::GPU;
    OutDescriptor.Allocator = &GPUAllocator;
    return OutDescriptor;
}

D3D12DescriptorHandle D3D12DescriptorFactory::CreateRTVDescriptor()
{
    D3D12DescriptorHandle OutDescriptor;
    RTVAllocator.Alloc(&OutDescriptor.CPUHandle, &OutDescriptor.GPUHandle);
    OutDescriptor.Type = D3D12DescriptorHandle::EType::RTV;
    OutDescriptor.Allocator = &RTVAllocator;
    return OutDescriptor;
}

D3D12DescriptorHandle D3D12DescriptorFactory::CreateDSVDescriptor()
{
    D3D12DescriptorHandle OutDescriptor;
    DSVAllocator.Alloc(&OutDescriptor.CPUHandle, &OutDescriptor.GPUHandle);
    OutDescriptor.Type = D3D12DescriptorHandle::EType::DSV;
    OutDescriptor.Allocator = &DSVAllocator;
    return OutDescriptor;
}

D3D12DescriptorTable D3D12DescriptorFactory::CreateGPUDescriptorTable(uint32_t TableSize)
{
    D3D12DescriptorTable OutTable;
    OutTable.ConsecutiveHandles.resize(TableSize);
    OutTable.HandlesToBeSet.resize(TableSize);
    for (int i = 0; i < TableSize; i ++)
    {
        auto& Handle = OutTable.ConsecutiveHandles[i];
        GPUAllocator.Alloc(&std::get<0>(Handle), &std::get<1>(Handle));
    }
    OutTable.Allocator = &GPUAllocator;
    return OutTable;
}

void D3D12DescriptorTable::SetDescriptorHandle(uint32_t Index, D3D12DescriptorHandle& Handle)
{
    HandlesToBeSet[Index] = Handle;
}

void D3D12DescriptorTable::UploadTable(ID3D12Device* Device)
{
    for (int i = 0; i < ConsecutiveHandles.size(); i++)
    {
        auto& SrcCPUDescriptor = HandlesToBeSet[i].CPUHandle;
        auto& DestCPUDescriptor = std::get<0>(ConsecutiveHandles[i]);
        Device->CopyDescriptorsSimple(1, DestCPUDescriptor, SrcCPUDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}

