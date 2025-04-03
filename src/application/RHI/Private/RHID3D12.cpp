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

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64                      FenceValue;
};

// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    ImVector<int>               FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        IM_ASSERT(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n - 1);
    }
    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        IM_ASSERT(FreeIndices.Size > 0);
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }
};

// RHID3D12PlatformSupport implementation
void RHID3D12PlatformSupport::Initialize()
{
}

void RHID3D12PlatformSupport::Cleanup()
{
    // Placeholder implementation
}

void RHID3D12PlatformSupport::InitializePhysicalDevice(RHIWindowManager* WindowManager)
{
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    
}

// RHID3D12Context implementation
void RHID3D12Context::Initialize(RHIPlatformSupport* PlatformSupport)
{
    auto* D3D12PlatformSupport = static_cast<RHID3D12PlatformSupport*>(PlatformSupport->GetImpl());
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

    
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

    ThrowIfFailed(m_commandAllocator->Reset());

    D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_Heap)));
    
    hCPUHeapStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
    hGPUHeapStart = m_Heap->GetGPUDescriptorHandleForHeapStart();

    HandleIncrementSize = m_device->GetDescriptorHandleIncrementSize(HeapDesc.Type);
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

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK RHID3D12WindowManager::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RHID3D12WindowManager* WindowManager = reinterpret_cast<RHID3D12WindowManager*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
        
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    RECT* rect;
    switch (message)
    {
    case WM_SIZING:
        rect = reinterpret_cast<RECT*>(lParam); // Not being used yet
        return 0;
    case WM_SIZE:
        WindowManager->SetResized(true, lParam);
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

// RHID3D12WindowManager implementation
void RHID3D12WindowManager::Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth)
{
    auto* D3D12PlatformSupport = static_cast<RHID3D12PlatformSupport*>(PlatformSupport->GetImpl());
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
    RECT windowRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

 
    // Create the main window. 
 
    CHECK_SYSTEM_ERROR(hWnd = CreateWindow(
        "DXSampleClass",        // name of window class 
        "Sample",            // title-bar string 
        WS_OVERLAPPEDWINDOW, // top-level window 
        CW_USEDEFAULT,       // default horizontal position 
        CW_USEDEFAULT,       // default vertical position 
        CW_USEDEFAULT,       // default width 
        CW_USEDEFAULT,       // default height 
        (HWND) NULL,         // no owner window 
        (HMENU) NULL,        // use class menu 
        hInstance,           // handle to application instance 
        (LPVOID) NULL);      // no window-creation data 
    );
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(hWnd, 1);
}

void RHID3D12WindowManager::Cleanup(RHIPlatformSupport* PlatformSupport)
{
    auto* D3D12PlatformSupport = static_cast<RHID3D12PlatformSupport*>(PlatformSupport->GetImpl());
    // Placeholder implementation
}

void RHID3D12WindowManager::InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12PlatformSupport = static_cast<RHID3D12PlatformSupport*>(PlatformSupport->GetImpl());
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(D3D12Context->factory->CreateSwapChainForHwnd(
        D3D12Context->m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(D3D12Context->factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
}

void RHID3D12WindowManager::CleanupSwapchain(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    D3D12Context->WaitForPreviousFrame();
}

void RHID3D12WindowManager::RecreateSwapchain(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    m_swapChain->ResizeBuffers(0, (UINT)LOWORD(ResizeParam), (UINT)HIWORD(ResizeParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
}

bool RHID3D12WindowManager::IsAlive()
{
    //// Process any messages in the queue.
    //if (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
    //{
    //    TranslateMessage(&msg);
    //    DispatchMessage(&msg);
    //}
    //
    return IsWindow(hWnd);
}

uint32_t RHID3D12WindowManager::GetWindowHeight()
{
    // Placeholder implementation
    return m_height;
}

uint32_t RHID3D12WindowManager::GetWindowWidth()
{
    // Placeholder implementation
    return m_width;
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

void RHID3D12Context::AllocateDescriptorHeap(CD3DX12_CPU_DESCRIPTOR_HANDLE& OutCpuDescriptorHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& OutGpuDescriptorHandle)
{
	OutCpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_Heap->GetCPUDescriptorHandleForHeapStart(), HandleIncrementSize * m_HeapSize);
    OutGpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetGPUDescriptorHandleForHeapStart(), HandleIncrementSize * m_HeapSize);
    m_HeapSize++;
}

// RHID3D12ImageResource implementation
void RHID3D12ImageResource::Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

void RHID3D12ImageResource::Initialize(RHIContext* Context, void* Data, uint32_t Size, uint32_t InHeight, uint32_t InWidth, RHIFormat InFormat, uint32_t MipLevel)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    Height = InHeight;
    Width = InWidth;
    ComPtr<ID3D12Resource> textureUploadHeap;
    
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    // Create the command list.
    ThrowIfFailed(D3D12Context->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12Context->m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    // Create the texture.
    {
        // Describe and create a Texture2D.
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = Width;
        textureDesc.Height = Height;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_texture)));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

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
        textureData.RowPitch = Size / Height;
        textureData.SlicePitch = Size;

        UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Describe and create a SRV for the texture.
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12Context->m_Heap->GetCPUDescriptorHandleForHeapStart(), D3D12Context->HandleIncrementSize * D3D12Context->m_HeapSize);
        D3D12Context->m_HeapSize++;
        D3D12Context->m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, CpuDescriptorHandle);

        // Close the command list and execute it to begin the initial GPU setup.
        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        D3D12Context->m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    }

    D3D12Context->WaitForPreviousFrame();
    
}

void RHID3D12ImageResource::CreateConstantBufferView(RHID3D12Context* Context, ID3D12DescriptorHeap* Heap)
{
}


void RHID3D12ImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    // Placeholder implementation
}

void RHID3D12ImageResource::Cleanup(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

// RHID3D12BufferResource implementation
void RHID3D12BufferResource::Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());

    // Note: using upload heaps to transfer static data like vert buffers is not 
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(ElementCounts* Stride),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_buffer)));
    if(Type==VERTEX)
    {
	    // Initialize the vertex buffer view.
	    m_vertexBufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();
	    m_vertexBufferView.StrideInBytes = Stride;
	    m_vertexBufferView.SizeInBytes = ElementCounts * Stride;
    }else if(Type==INDEX)
    {
	    // Initialize the vertex buffer view.
	    m_indexBufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();
	    m_indexBufferView.SizeInBytes = ElementCounts * Stride;
        m_indexBufferView.Format = DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
    }

}

void RHID3D12BufferResource::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
    // Copy the triangle data to the vertex buffer.
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, data, TotalBytes);
    m_buffer->Unmap(0, nullptr);
}

void RHID3D12BufferResource::Cleanup(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

// RHID3D12Uniform implementation
void RHID3D12Uniform::Initialize(RHIContext* Context, uint32_t UniformStructSize)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());        // Describe and create a constant buffer view (CBV) descriptor heap.
    // Flags indicate that this descriptor heap can be bound to the pipeline 
    // and that descriptors contained in it can be referenced by a root table.

    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(UniformStructSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    // Describe and create a constant buffer view.
    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = UniformStructSize;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12Context->m_Heap->GetCPUDescriptorHandleForHeapStart(), D3D12Context->HandleIncrementSize * D3D12Context->m_HeapSize);
	D3D12Context->m_HeapSize++;
    D3D12Context->m_device->CreateConstantBufferView(&cbvDesc, cbvDescriptorHandle);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
}

void RHID3D12Uniform::CreateConstantBufferView(RHID3D12Context* Context, ID3D12DescriptorHeap* Heap)
{
}

void RHID3D12Uniform::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    memcpy(m_pCbvDataBegin, data, TotalBytes);
    D3D12Context->WaitForPreviousFrame();
}

void RHID3D12Uniform::Cleanup(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
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
void RHID3D12PipelineFactory::AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
    // Define the vertex input layout.
    inputElementDescs.push_back({ "ATTRIBUTE", Location,
        GetFormat(Format), BindingIndex, Offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
}

void RHID3D12PipelineFactory::AddBufferBinding(uint32_t BindingIndex, uint32_t Stride)
{

}

void RHID3D12PipelineFactory::RemoveAllBufferBindings()
{
    inputElementDescs.clear();
}

void RHID3D12PipelineFactory::SetUniformBinding(uint32_t Binding)
{
    if (RootParameters.size()<=Binding)
    {
        RootParameters.resize(Binding + 1);
    }
    uint32_t BaseRegister = Binding>>16;
    CD3DX12_DESCRIPTOR_RANGE1 ranges{};
    ranges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, BaseRegister, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    CD3DX12_ROOT_PARAMETER1 rootParameters{};
    rootParameters.InitAsConstantBufferView(0);
    //rootParameters.InitAsDescriptorTable(Ranges.size(), Ranges.data(), D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[Binding] = rootParameters;
}

void RHID3D12PipelineFactory::SetImageSamplerBinding(uint32_t Binding)
{
    if (RootParameters.size() <= Binding)
    {
        RootParameters.resize(Binding + 1);
    }
    uint32_t BaseRegister = Binding>>16;
    CD3DX12_DESCRIPTOR_RANGE1 ranges{};
    ranges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, BaseRegister, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    CD3DX12_ROOT_PARAMETER1 rootParameters{};
    Ranges.push_back(ranges);
    rootParameters.InitAsDescriptorTable(1, &Ranges.back(), D3D12_SHADER_VISIBILITY_PIXEL);
    RootParameters[Binding] = rootParameters;

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
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    Samplers.push_back(sampler);
}

void RHID3D12PipelineFactory::RemoveAllGlobalBindings()
{
    Samplers.clear();
    RootParameters.clear();
    Ranges.clear();
}

void RHID3D12PipelineFactory::SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    ThrowIfFailed(D3DCompile(VertShader.data(), VertShader.size(), NULL, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompile(FragShader.data(), FragShader.size(), NULL, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
}

void RHID3D12PipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
	//auto* D3D12RenderPassResource = static_cast<RHID3D12RenderPass*>(RenderPassResource->GetImpl());
	auto* D3D12PipelineObject = static_cast<RHID3D12PipelineObject*>(OutPipelineObject->GetImpl());
    //auto* D3D12RenderPassResource = static_cast<RHID3D12RenderPass*>(RenderPassResource->GetImpl());
    D3D12PipelineObject->RootParameters = RootParameters;
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
        rootSignatureDesc.Init_1_1(RootParameters.size(), 
            RootParameters.data(), 
            Samplers.size(), Samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(D3D12Context->m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&D3D12PipelineObject->m_rootSignature)));
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
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(D3D12Context->m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&D3D12PipelineObject->m_pipelineState)));
    }
}

void RHID3D12PipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPass)
{
    // TODO: Implement D3D12 pipeline object initialization for present pass
}

void RHID3D12PipelineFactory::Cleanup(RHIContext* Context)
{
    // TODO: Implement D3D12 pipeline factory cleanup
}

// RHID3D12PipelineObject implementation
void RHID3D12PipelineObject::Cleanup(RHIContext* Context)
{
    // TODO: Implement D3D12 pipeline cleanup
}

void RHID3D12PipelineObject::SetUniform(RHIUniform* Uniform, uint32_t Binding)
{
    auto* D3D12Uniform = static_cast<RHID3D12Uniform*>(Uniform->GetImpl());
    if(Binding>=GpuDescriptorHandles.size())
    {
	    GpuDescriptorHandles.resize(Binding+1);
    }
    GpuDescriptorHandles[Binding] = D3D12Uniform->GpuDescriptorHandle;
    cbvDescriptions.push_back(D3D12Uniform->cbvDesc);
}

void RHID3D12PipelineObject::SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding)
{
    auto* D3D12ImageResource = static_cast<RHID3D12ImageResource*>(ImageResource->GetImpl());
    if(Binding>=GpuDescriptorHandles.size())
    {
	    GpuDescriptorHandles.resize(Binding+1);
    }
    GpuDescriptorHandles[Binding] = D3D12ImageResource->GpuDescriptorHandle;
    srvDescriptions.push_back(D3D12ImageResource->srvDesc);
}

// RHID3D12GraphicsDispatcher implementation
void RHID3D12GraphicsDispatcher::Initialize(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());

    // Create the command list.
    ThrowIfFailed(D3D12Context->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12Context->m_commandAllocator.Get(),
        NULL, IID_PPV_ARGS(&m_commandList)));
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());
}

void RHID3D12GraphicsDispatcher::Cleanup(RHIContext* Context, RHIWindowManager* WindowManager)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());

}

void RHID3D12GraphicsDispatcher::BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex)
{
    auto* D3D12Buffer = static_cast<RHID3D12BufferResource*>(BufferResource->GetImpl());
    if (BoundBufferViews.size()<=BindingIndex)
    {
        BoundBufferViews.resize(BindingIndex + 1);
    }

    BoundBufferViews[BindingIndex] = D3D12Buffer->m_vertexBufferView;
}

void RHID3D12GraphicsDispatcher::BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset)
{
    auto* D3D12BufferResource = static_cast<RHID3D12BufferResource*>(BufferResource->GetImpl());
    BoundIndexBufferView = D3D12BufferResource->m_indexBufferView;
}

void RHID3D12GraphicsDispatcher::Dispatch(RHIWindowManager* WindowManager, RHIPipelineObject* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    auto* D3D12Pipeline = static_cast<RHID3D12PipelineObject*>(Pipeline->GetImpl());

    //ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), D3D12Pipeline->m_pipelineState.Get()));
    // Set necessary state.
    m_commandList->SetPipelineState(D3D12Pipeline->m_pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(D3D12Pipeline->m_rootSignature.Get());
    m_commandList->SetDescriptorHeaps(1, &pHeaps);
    uint32_t cbvIndex = 0;
    for (int i = 0; i < D3D12Pipeline->RootParameters.size(); i++)
    {
        const auto& RootParam = D3D12Pipeline->RootParameters[i];
	    if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
	    {
            m_commandList->SetGraphicsRootDescriptorTable(i, CD3DX12_GPU_DESCRIPTOR_HANDLE(pHeaps->GetGPUDescriptorHandleForHeapStart(), i * DescriptorHeapOffset));
	    }
    	else if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
	    {
            m_commandList->SetGraphicsRootConstantBufferView(i, D3D12Pipeline->cbvDescriptions[cbvIndex].BufferLocation);
            cbvIndex++;
	    }
    }
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, BoundBufferViews.size(), BoundBufferViews.data());
    m_commandList->IASetIndexBuffer(&BoundIndexBufferView);
    m_commandList->DrawIndexedInstanced(IndexCount, InstanceCount, IndexOffset, 0, 0);
    //ThrowIfFailed(m_commandList->Close());

}

void RHID3D12GraphicsDispatcher::BeginRenderPass(RHIContext* Context, RHIRenderPass* RenderPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12RenderPass = static_cast<RHID3D12RenderPass*>(RenderPass->GetImpl());
    // Placeholder implementation
}

void RHID3D12GraphicsDispatcher::EndRenderPass(RHIRenderPass* RenderPass)
{
    // Placeholder implementation
}


void RHID3D12GraphicsDispatcher::BeginPresentPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPassResource)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    D3D12PresentPass = static_cast<RHID3D12PresentPass*>(PresentPassResource->GetImpl());

    if(D3D12WindowManager->bResized)
    {
        PresentPassResource->OnWindowResize(Context, WindowManager);
	    D3D12WindowManager->SetResized(false, NULL);
    }

    D3D12PresentPass->m_frameIndex = D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex();
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = D3D12Context->m_fenceValue;
    ThrowIfFailed(D3D12Context->m_commandQueue->Signal(D3D12Context->m_fence.Get(), fence));
    D3D12Context->m_fenceValue++;

    // Wait until the previous frame is finished.
    if (D3D12Context->m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(D3D12Context->m_fence->SetEventOnCompletion(fence, D3D12Context->m_fenceEvent));
        WaitForSingleObject(D3D12Context->m_fenceEvent, INFINITE);
    }

    D3D12PresentPass->m_frameIndex = D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex();
    rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12PresentPass->m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        D3D12PresentPass->m_frameIndex, D3D12PresentPass->m_rtvDescriptorSize);

    ThrowIfFailed(m_commandList->Reset(D3D12Context->m_commandAllocator.Get(), nullptr));
    pHeaps = D3D12Context->m_Heap.Get();
    DescriptorHeapOffset = D3D12Context->HandleIncrementSize;
    // Execute the command list.
    //ID3D12CommandList* pCommandList = m_commandList.Get();
    //ThrowIfFailed(m_commandList->Close());
    //D3D12Context->m_commandQueue->ExecuteCommandLists(1, &pCommandList);
        // Indicate that the back buffer will be used as a render target.
    
    
    CD3DX12_VIEWPORT m_viewport{};
    CD3DX12_RECT m_scissorRect{};
    m_viewport.Width = WindowManager->GetWindowWidth();
    m_viewport.Height = WindowManager->GetWindowHeight();
    m_scissorRect = CD3DX12_RECT(0., 0., m_viewport.Width, m_viewport.Height);
    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(D3D12PresentPass->m_renderTargets[D3D12PresentPass->m_frameIndex],
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

}

void RHID3D12GraphicsDispatcher::EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
    //ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(D3D12PresentPass->m_renderTargets[D3D12PresentPass->m_frameIndex],
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Execute the command list.
    ID3D12CommandList* pCommandList = m_commandList.Get();
    ThrowIfFailed(m_commandList->Close());
    D3D12Context->m_commandQueue->ExecuteCommandLists(1, &pCommandList);

    // Present the frame.
    ThrowIfFailed(D3D12WindowManager->m_swapChain->Present(1, 0));
    //Error("Error: ", D3D12Context->m_device->GetDeviceRemovedReason());

    D3D12Context->WaitForPreviousFrame();

    ThrowIfFailed(D3D12Context->m_commandAllocator->Reset());
}

void RHID3D12GraphicsDispatcher::BeginFrame()
{
}

void RHID3D12GraphicsDispatcher::WaitForGPUIdle(RHIContext* Context)
{
}

void RHID3D12RenderPass::Initialize(RHIContext* Context, uint32_t SizeX, uint32_t SizeY)
{
	
}


void RHID3D12RenderPass::AddColorRenderTarget(RHIImageResource* ColorRT)
{
	
}

void RHID3D12RenderPass::SetDepthRenderTarget(RHIImageResource* DepthRT)
{

}

void RHID3D12RenderPass::Cleanup(RHIContext* Context)
{
}


void RHID3D12PresentPass::Cleanup(RHIContext* Context)
{
}

void RHID3D12PresentPass::CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    m_frameIndex = D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex();
	// Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
			ID3D12Resource* pBackBuffer = nullptr;
            ThrowIfFailed(D3D12WindowManager->m_swapChain->GetBuffer(n, IID_PPV_ARGS(&pBackBuffer)));
            D3D12Context->m_device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
            m_renderTargets[n] = pBackBuffer;
        }
    }
}

void RHID3D12PresentPass::CleanupSwapchainFramebuffer(RHIContext* Context)
{
    // Create a RTV for each frame.
    for (UINT n = 0; n < FrameCount; n++)
    {
        m_renderTargets[n]->Release();
    }
}

void RHID3D12PresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(D3D12Context->m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = D3D12Context->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }
    D3D12Context->WaitForPreviousFrame();
    CreateSwapchainFramebuffer(Context, WindowManager);
}

void RHID3D12PresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    D3D12Context->WaitForPreviousFrame();
	CleanupSwapchainFramebuffer(Context);

	UINT width = (UINT)LOWORD(D3D12WindowManager->ResizeParam);
    UINT height = (UINT)HIWORD(D3D12WindowManager->ResizeParam);

    HRESULT hr = D3D12WindowManager->m_swapChain->ResizeBuffers(0,
        width,
		height, 
        DXGI_FORMAT_UNKNOWN, 0);
    ThrowIfFailed(hr);
    D3D12Context->WaitForPreviousFrame();
    CreateSwapchainFramebuffer(Context, WindowManager);
}

static ExampleDescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
// RHID3D12ImGUI implementation
void RHID3D12ImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    auto* D3D12PresentPass = static_cast<RHID3D12PresentPass*>(PresentPass->GetImpl());
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(D3D12WindowManager->hWnd);

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = D3D12Context->m_device.Get();
    init_info.CommandQueue = D3D12Context->m_commandQueue.Get();
    init_info.NumFramesInFlight = APP_NUM_FRAMES_IN_FLIGHT;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)


    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = APP_SRV_HEAP_SIZE;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (D3D12Context->m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            throw std::runtime_error("Cannot create heap for imgui");
        g_pd3dSrvDescHeapAlloc.Create(D3D12Context->m_device.Get(), g_pd3dSrvDescHeap);
    }

    init_info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    {
        return g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
    };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
	    return g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle);
    };
    ImGui_ImplDX12_Init(&init_info);
}

void RHID3D12ImGUI::DispatchImGUI(RHIGraphicsDispatcher* Dispatcher)
{
    auto* D3D12Dispatcher = static_cast<RHID3D12GraphicsDispatcher*>(Dispatcher->GetImpl());

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // Poll and handle messages (inputs, window resize, etc.)
    // See the WndProc() function below for our to dispatch events to the Win32 backend.
    MSG msg;
    if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }

    // Rendering
    ImGui::Render();
    
    D3D12Dispatcher->m_commandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), D3D12Dispatcher->m_commandList.Get());
}

void RHID3D12ImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context))
{
    // Placeholder implementation
}

void RHID3D12ImGUI::Cleanup()
{    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}