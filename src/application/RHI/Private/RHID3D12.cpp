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


void DescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
{
    assert(Heap == nullptr && FreeIndices.empty());
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

void DescriptorHeapAllocator::Destroy()
{
    Heap = nullptr;
    FreeIndices.clear();
}

void DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
{
    assert(FreeIndices.size() > 0);
    int idx = FreeIndices.back();
    FreeIndices.pop_back();
    out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
    if(out_gpu_desc_handle)
    {
		out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
}

void DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle)
{
    int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
    FreeIndices.push_back(cpu_idx);
}

// RHID3D12PlatformSupport implementation
void RHID3D12PlatformSupport::Initialize()
{
}

void RHID3D12PlatformSupport::Cleanup()
{
    // Placeholder implementation
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
    ThrowIfFailed(m_device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_srvheap)));
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_rtvheap)));
    HeapDesc.NumDescriptors = 32;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_dsvheap)));
    SRVHeapAllocator.Create(m_device.Get(), m_srvheap.Get());
    RTVHeapAllocator.Create(m_device.Get(), m_rtvheap.Get());
    DSVHeapAllocator.Create(m_device.Get(), m_dsvheap.Get());
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
    //RECT windowRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    //AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

 
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
    // Create frame resources.
    // Create a RTV for each frame.
    m_rtvHandles.resize(FrameCount);

    for (UINT n = 0; n < FrameCount; n++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&pBackBuffer)));
        D3D12Context->RTVHeapAllocator.Alloc(&m_rtvHandles[n], nullptr);
        D3D12Context->m_device->CreateRenderTargetView(pBackBuffer, nullptr, m_rtvHandles[n]);
        m_renderTargets[n] = pBackBuffer;
    }
        // Create the texture.
    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_D32_FLOAT;
    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.Flags =  D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_CLEAR_VALUE DSClearValue;
    DSClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    DSClearValue.DepthStencil.Depth = 1.0;
    DSClearValue.DepthStencil.Stencil = 0;

    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &DSClearValue,
        IID_PPV_ARGS(&m_dsRenderTarget)));
    
    D3D12Context->DSVHeapAllocator.Alloc(&dsvHandle, nullptr);
    D3D12Context->m_device->CreateDepthStencilView(m_dsRenderTarget.Get(), NULL, dsvHandle);
}

void RHID3D12WindowManager::CleanupSwapchain(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    D3D12Context->WaitForPreviousFrame();
    for(auto* rt_buffer : m_renderTargets)
    {
	    rt_buffer->Release();
    }
    //m_dsRenderTarget->Release();
}

void RHID3D12WindowManager::RecreateSwapchain(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    D3D12Context->WaitForPreviousFrame();
    CleanupSwapchain(Context);
    m_width = (UINT)LOWORD(ResizeParam);
    m_height = (UINT)HIWORD(ResizeParam);

    HRESULT hr = m_swapChain->ResizeBuffers(0,
        m_width,
        m_height,
        DXGI_FORMAT_UNKNOWN, 0);
    ThrowIfFailed(hr);

    for (UINT n = 0; n < FrameCount; n++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&pBackBuffer)));
        D3D12Context->m_device->CreateRenderTargetView(pBackBuffer, nullptr, m_rtvHandles[n]);
        m_renderTargets[n] = pBackBuffer;
    }
    // Create the texture.
    // Describe and create a Texture2D.
    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_D32_FLOAT;
    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.Flags =  D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_CLEAR_VALUE DSClearValue;
    DSClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    DSClearValue.DepthStencil.Depth = 1.0;
    DSClearValue.DepthStencil.Stencil = 0;

    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &DSClearValue,
        IID_PPV_ARGS(&m_dsRenderTarget)));
    
    D3D12Context->m_device->CreateDepthStencilView(m_dsRenderTarget.Get(), NULL, dsvHandle);

    D3D12Context->WaitForPreviousFrame();

}

void RHID3D12WindowManager::AddScreenSizeTexture(RHIImageResource* ImageResource)
{
	
}

void RHID3D12WindowManager::InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12RenderPass = static_cast<RHID3D12RenderPass*>(OutRenderPass->GetImpl());
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    D3D12RenderPass->ColorRTs.resize(1);
    D3D12RenderPass->ColorRTs[0] = m_rtvHandles[0];
    D3D12RenderPass->ColorRTFormats.resize(1);
    D3D12RenderPass->ColorRTFormats[0] = RTFormat;
    D3D12RenderPass->Height = m_height;
    D3D12RenderPass->Width = m_width;
}

void RHID3D12WindowManager::RemoveScreenSizeTexture(RHIImageResource* ImageResource)
{
	
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


void RHID3D12ImageResource::Initialize(RHIContext* Context, uint32_t InHeight, uint32_t InWidth, RHIFormat InFormat, int32_t MipLevel)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    Height = InHeight;
    Width = InWidth;
    
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
        ResourceStates = D3D12_RESOURCE_STATE_COPY_DEST;
        // Describe and create a SRV for the texture.
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        D3D12Context->SRVHeapAllocator.Alloc(&CpuDescriptorHandle, &GpuDescriptorHandle);
        D3D12Context->m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, CpuDescriptorHandle);
    }
}


void RHID3D12ImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());

    Height = WindowManager->GetWindowHeight();
    Width = WindowManager->GetWindowWidth();

    // Create the texture.
    // Describe and create a Texture2D.
    textureDesc.MipLevels = 1;
    textureDesc.Format = InUsage == ImageUsage::IU_DEPTH_RT ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = Width;
    textureDesc.Height = Height;
    textureDesc.Flags =  InUsage==ImageUsage::IU_DEPTH_RT? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_CLEAR_VALUE ColorRTClearValue;
	D3D12_CLEAR_VALUE DSClearValue;
    ColorRTClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorRTClearValue.Color[0] = 1.0;
    ColorRTClearValue.Color[1] = 0.0;
    ColorRTClearValue.Color[2] = 1.0;
    ColorRTClearValue.Color[3] = 1.0;
    DSClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    DSClearValue.DepthStencil.Depth = 1.0;
    DSClearValue.DepthStencil.Stencil = 0;
    
    ResourceStates = InUsage == ImageUsage::IU_DEPTH_RT ? D3D12_RESOURCE_STATE_DEPTH_WRITE: D3D12_RESOURCE_STATE_RENDER_TARGET;
    ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        ResourceStates,
        InUsage == ImageUsage::IU_DEPTH_RT ? &DSClearValue : &ColorRTClearValue,
        IID_PPV_ARGS(&m_texture)));
    
    // Describe and create a SRV for the texture.
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = InUsage == ImageUsage::IU_DEPTH_RT ? DXGI_FORMAT_R32_FLOAT : textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    D3D12Context->SRVHeapAllocator.Alloc(&CpuDescriptorHandle, &GpuDescriptorHandle);
    D3D12Context->m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, CpuDescriptorHandle);
    if (InUsage == ImageUsage::IU_COLOR_RT || InUsage == ImageUsage::IU_COLOR_PRESENT_RT)
    {
        D3D12Context->RTVHeapAllocator.Alloc(&RTDSVCpuDescriptorHandle, &RTDSVGpuDescriptorHandle);
        D3D12Context->m_device->CreateRenderTargetView(m_texture.Get(), NULL, RTDSVCpuDescriptorHandle);
    }else
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC ds_desc{};
        ds_desc.Texture2D.MipSlice = 1;
        ds_desc.Format = DXGI_FORMAT_D32_FLOAT;
        ds_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        D3D12Context->DSVHeapAllocator.Alloc(&RTDSVCpuDescriptorHandle, &RTDSVGpuDescriptorHandle);
        D3D12Context->m_device->CreateDepthStencilView(m_texture.Get(), NULL, RTDSVCpuDescriptorHandle);
    }
}

void RHID3D12ImageResource::CopyToTexture(RHIContext* Context, void* Data, uint32_t Stride)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    ComPtr<ID3D12Resource> textureUploadHeap;
    
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    // Create the command list.
    ThrowIfFailed(D3D12Context->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12Context->m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
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
    textureData.RowPitch = Stride * Width;
    textureData.SlicePitch = Stride * Width * Height;

    UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        // Close the command list and execute it to begin the initial GPU setup.
        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        D3D12Context->m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    D3D12Context->WaitForPreviousFrame();
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
    UniformStructSize = (UniformStructSize - 1)/256 * 256 + 256;
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

    D3D12Context->SRVHeapAllocator.Alloc(&CpuDescriptorHandle, &GpuDescriptorHandle);
    D3D12Context->m_device->CreateConstantBufferView(&cbvDesc, CpuDescriptorHandle);
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
    sampler.ShaderRegister = Binding;
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
    ComPtr<ID3DBlob> ErrorMsg;
    D3DCompile(VertShader.data(), VertShader.size(), NULL, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &ErrorMsg);
    if(ErrorMsg.Get())
    {
	    Error("Compile vertex shader error: ", reinterpret_cast<char*>(ErrorMsg->GetBufferPointer()));
    }
    D3DCompile(FragShader.data(), FragShader.size(), NULL, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &ErrorMsg);
    if(ErrorMsg.Get())
    {
	    Error("Compile fragment shader error: ", reinterpret_cast<char*>(ErrorMsg->GetBufferPointer()));
    }
}

void RHID3D12PipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
	auto* D3D12PipelineObject = static_cast<RHID3D12PipelineObject*>(OutPipelineObject->GetImpl());
    auto* D3D12RenderPassResource = static_cast<RHID3D12RenderPass*>(RenderPassResource->GetImpl());
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
        D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if(error.Get())
	    {
		    Error("D3DX12SerializeVersionedRootSignature error: ", reinterpret_cast<char*>(error->GetBufferPointer()));
	    }
        ThrowIfFailed(D3D12Context->m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&D3D12PipelineObject->m_rootSignature)));
        assert(D3D12PipelineObject->m_rootSignature);
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
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = D3D12RenderPassResource->ColorRTs.size();
        DXGI_FORMAT* PSOColorFormats = psoDesc.RTVFormats;
        memcpy(PSOColorFormats, D3D12RenderPassResource->ColorRTFormats.data(), min(D3D12RenderPassResource->ColorRTs.size(), 8));
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = Samplers.size();
        ThrowIfFailed(D3D12Context->m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&D3D12PipelineObject->m_pipelineState)));
    }
}

void RHID3D12PipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPassResource)
{
    
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
    if (Binding>=cbvDescriptions.size())
    {
        cbvDescriptions.resize(Binding + 1);
    }
    cbvDescriptions[Binding] = D3D12Uniform->cbvDesc;
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
    ImageToTransition.push_back(D3D12ImageResource);
}

// RHID3D12GraphicsDispatcher implementation
void RHID3D12GraphicsDispatcher::Initialize(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());

    // Create the command list.
    ThrowIfFailed(D3D12Context->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12Context->m_commandAllocator.Get(),
        NULL, IID_PPV_ARGS(&m_commandList)));
    pHeaps = D3D12Context->m_srvheap.Get();
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    //ThrowIfFailed(m_commandList->Close());
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

void RHID3D12GraphicsDispatcher::Dispatch(RHIPipelineObject* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
    auto* D3D12Pipeline = static_cast<RHID3D12PipelineObject*>(Pipeline->GetImpl());

    for(auto* ImageResource : D3D12Pipeline->ImageToTransition)
    {
        if(ImageResource->ResourceStates != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
        {
	        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				ImageResource->m_texture.Get(),
				ImageResource->ResourceStates, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
	        ImageResource->ResourceStates = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        }
    }
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
            // set samplers
            m_commandList->SetGraphicsRootDescriptorTable(i, D3D12Pipeline->GpuDescriptorHandles[i]);
	    }
    	else if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
	    {
            m_commandList->SetGraphicsRootConstantBufferView(i, D3D12Pipeline->cbvDescriptions[i].BufferLocation);
            cbvIndex++;
	    }
    }
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, BoundBufferViews.size(), BoundBufferViews.data());
    m_commandList->IASetIndexBuffer(&BoundIndexBufferView);
    m_commandList->DrawIndexedInstanced(IndexCount, InstanceCount, IndexOffset, 0, 0);
    //ThrowIfFailed(m_commandList->Close());

}

void RHID3D12GraphicsDispatcher::BeginRenderPass(RHIRenderPass* RenderPass)
{
    auto* D3D12RenderPass = static_cast<RHID3D12RenderPass*>(RenderPass->GetImpl());
    // Execute the command list.
    //ID3D12CommandList* pCommandList = m_commandList.Get();
    //ThrowIfFailed(m_commandList->Close());
    //D3D12Context->m_commandQueue->ExecuteCommandLists(1, &pCommandList);
        // Indicate that the back buffer will be used as a render target.


    CD3DX12_VIEWPORT m_viewport(0., 0., D3D12RenderPass->Width, D3D12RenderPass->Height);
    CD3DX12_RECT m_scissorRect{};
    m_scissorRect = CD3DX12_RECT(0., 0., D3D12RenderPass->Width, D3D12RenderPass->Height);

    for(auto* ImageResource : D3D12RenderPass->TransitionList)
    {
        if(ImageResource->ResourceStates != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
	        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				ImageResource->m_texture.Get(),
				ImageResource->ResourceStates, D3D12_RESOURCE_STATE_RENDER_TARGET));
	        ImageResource->ResourceStates = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
    }

    // Record commands.
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    m_commandList->OMSetRenderTargets(D3D12RenderPass->ColorRTs.size(), D3D12RenderPass->ColorRTs.data(), 
        FALSE, &D3D12RenderPass->DepthRT);
    const float clearColor[] = { 1.0f, 0.0f, 1.0f, 1.0f };
    for(auto& ColorRT : D3D12RenderPass->ColorRTs)
    {
		m_commandList->ClearRenderTargetView(ColorRT, clearColor, 0, nullptr);
    }
	m_commandList->ClearDepthStencilView(D3D12RenderPass->DepthRT, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0, 0, 0, NULL);
}

void RHID3D12GraphicsDispatcher::EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
	//ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        D3D12WindowManager->m_renderTargets[D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex()],
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
    ThrowIfFailed(m_commandList->Reset(D3D12Context->m_commandAllocator.Get(), nullptr));
}


void RHID3D12GraphicsDispatcher::EndRenderPass(RHIRenderPass* RenderPass)
{
}


void RHID3D12GraphicsDispatcher::BeginFrame(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    auto* D3D12PresentRenderPass = static_cast<RHID3D12RenderPass*>(PresentRenderPass->GetImpl());

    if (D3D12WindowManager->bResized)
    {
        D3D12WindowManager->RecreateSwapchain(Context);
        D3D12WindowManager->SetResized(false, NULL);
        D3D12Context->WaitForPreviousFrame();
    }
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        D3D12WindowManager->m_renderTargets[D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex()],
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    D3D12PresentRenderPass->ColorRTs.resize(1);
    D3D12PresentRenderPass->ColorRTs[0] = D3D12WindowManager->m_rtvHandles[D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex()];
    D3D12PresentRenderPass->ColorRTFormats.resize(1);
    D3D12PresentRenderPass->ColorRTFormats[0] = D3D12WindowManager->RTFormat;
    D3D12PresentRenderPass->DepthRT = D3D12WindowManager->dsvHandle;
    D3D12PresentRenderPass->Height = D3D12WindowManager->m_height;
    D3D12PresentRenderPass->Width = D3D12WindowManager->m_width;
}

void RHID3D12GraphicsDispatcher::WaitForGPUIdle(RHIContext* Context)
{
}

void RHID3D12GraphicsDispatcher::TransitionImageAsRenderTarget(RHIImageResource* Image)
{
}

void RHID3D12GraphicsDispatcher::TransitionImageAsShaderRead(RHIImageResource* Image)
{
}

void RHID3D12RenderPass::Initialize(RHIContext* Context, uint32_t SizeX, uint32_t SizeY)
{
    Height = SizeY;
    Width = SizeX;
}


void RHID3D12RenderPass::AddColorRenderTarget(RHIImageResource* ColorRT)
{
    auto* D3D12ColorRT = static_cast<RHID3D12ImageResource*>(ColorRT->GetImpl());
    ColorRTs.push_back(D3D12ColorRT->RTDSVCpuDescriptorHandle);
    ColorRTFormats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);
    TransitionList.push_back(D3D12ColorRT);
}

void RHID3D12RenderPass::SetDepthRenderTarget(RHIImageResource* InDepthRT)
{
    auto* D3D12DepthRT = static_cast<RHID3D12ImageResource*>(InDepthRT->GetImpl());
    DepthRT = D3D12DepthRT->RTDSVCpuDescriptorHandle;
    DepthRTFormat = DXGI_FORMAT_D32_FLOAT;
}

void RHID3D12RenderPass::Cleanup(RHIContext* Context)
{
}


void RHID3D12PresentPass::Cleanup(RHIContext* Context)
{
}

void RHID3D12PresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    D3D12Context->WaitForPreviousFrame();
}

void RHID3D12PresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    D3D12Context->WaitForPreviousFrame();

	UINT width = (UINT)LOWORD(D3D12WindowManager->ResizeParam);
    UINT height = (UINT)HIWORD(D3D12WindowManager->ResizeParam);

    HRESULT hr = D3D12WindowManager->m_swapChain->ResizeBuffers(0,
        width,
		height, 
        DXGI_FORMAT_UNKNOWN, 0);
    ThrowIfFailed(hr);
    D3D12Context->WaitForPreviousFrame();
}

static DescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
// RHID3D12ImGUI implementation
void RHID3D12ImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    //auto* D3D12PresentPass = static_cast<RHID3D12PresentPass*>(PresentPass->GetImpl());
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
    ImGui_ImplWin32_Init(D3D12WindowManager->hWnd);

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = D3D12Context->m_device.Get();
    init_info.CommandQueue = D3D12Context->m_commandQueue.Get();
    init_info.NumFramesInFlight = APP_NUM_FRAMES_IN_FLIGHT;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
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
        g_pd3dSrvDescHeapAlloc.Create(D3D12Context->m_device.Get(), g_pd3dSrvDescHeap);
    }

    init_info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    {
        return g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
    };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
	    return g_pd3dSrvDescHeapAlloc.Free(cpu_handle);
    };
    ImGui_ImplDX12_Init(&init_info);
}

void RHID3D12ImGUI::DispatchImGUI(RHIGraphicsDispatcher* Dispatcher)
{
    auto* D3D12Dispatcher = static_cast<RHID3D12GraphicsDispatcher*>(Dispatcher->GetImpl());

    MSG msg;
    if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    D3D12Dispatcher->m_commandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), D3D12Dispatcher->m_commandList.Get());
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
