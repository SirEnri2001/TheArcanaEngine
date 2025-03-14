#include "RHID3D12.h"

#include <d3dcompiler.h>


#include "RHID3D12Impl.h"

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
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
}

void RHID3D12Context::Cleanup()
{
    // Placeholder implementation
}

void RHID3D12Context::WaitDeviceIdle()
{
    // Placeholder implementation
}

LRESULT CALLBACK RHID3D12WindowManager::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Handle any messages the switch statement didn't.
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
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
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
    
    ShowWindow(hWnd, 1);

    m_viewport.Width = m_width;
    m_viewport.Height = m_height;
    m_scissorRect = CD3DX12_RECT(0.,0., m_width, m_height);
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
    swapChainDesc.BufferCount = FrameCount;
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
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = 2;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(D3D12Context->m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = D3D12Context->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            D3D12Context->m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }
    
    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(D3D12Context->m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
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
        WaitForPreviousFrame(D3D12Context);
    }
}

void RHID3D12WindowManager::CleanupSwapchain(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame(D3D12Context);

    CloseHandle(m_fenceEvent);
}

void RHID3D12WindowManager::RecreateSwapchain(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

bool RHID3D12WindowManager::IsAlive()
{
    // Placeholder implementation
    return true;
}

uint32_t RHID3D12WindowManager::GetWindowHeight()
{
    // Placeholder implementation
    return 0;
}

uint32_t RHID3D12WindowManager::GetWindowWidth()
{
    // Placeholder implementation
    return 0;
}

void RHID3D12WindowManager::WaitForPreviousFrame(RHID3D12Context* D3D12Context)
{
	    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(D3D12Context->m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}


// RHID3D12ImageResource implementation
void RHID3D12ImageResource::Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
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
    // Placeholder implementation
}

void RHID3D12BufferResource::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

void RHID3D12BufferResource::Cleanup(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

// RHID3D12Uniform implementation
void RHID3D12Uniform::Initialize(RHIContext* Context, uint32_t UniformStructSize)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

void RHID3D12Uniform::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

void RHID3D12Uniform::Cleanup(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    // Placeholder implementation
}

void RHID3D12PipelineFactory::AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
    // TODO: Implement D3D12 buffer layout setup
}

void RHID3D12PipelineFactory::AddBufferBinding(uint32_t BindingIndex, uint32_t Stride)
{
    // TODO: Implement D3D12 buffer binding setup
}

void RHID3D12PipelineFactory::RemoveAllBufferBindings()
{
    // TODO: Implement D3D12 buffer bindings cleanup
}

void RHID3D12PipelineFactory::SetUniformBinding(uint32_t Binding)
{
    // TODO: Implement D3D12 uniform binding setup
}

void RHID3D12PipelineFactory::SetImageSamplerBinding(uint32_t Binding)
{
    // TODO: Implement D3D12 image sampler binding setup
}

void RHID3D12PipelineFactory::RemoveAllGlobalBindings()
{
    // TODO: Implement D3D12 global bindings cleanup
}

void RHID3D12PipelineFactory::SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
    // TODO: Implement D3D12 shader setup
}

void RHID3D12PipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
	//auto* D3D12RenderPassResource = static_cast<RHID3D12RenderPass*>(RenderPassResource->GetImpl());
	auto* D3D12PipelineObject = static_cast<RHID3D12PipelineObject*>(OutPipelineObject->GetImpl());
    //auto* D3D12RenderPassResource = static_cast<RHID3D12RenderPass*>(RenderPassResource->GetImpl());

	// Create an empty root signature.
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(D3D12Context->m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&D3D12PipelineObject->m_rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
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
    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * 1.f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * 1.f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * 1.f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(D3D12Context->m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&D3D12PipelineObject->m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(D3D12PipelineObject->m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        D3D12PipelineObject->m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        D3D12PipelineObject->m_vertexBufferView.BufferLocation = D3D12PipelineObject->m_vertexBuffer->GetGPUVirtualAddress();
        D3D12PipelineObject->m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        D3D12PipelineObject->m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }
    
    ThrowIfFailed(D3D12Context->m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&D3D12PipelineObject->m_commandAllocator)));
    // Create the command list.
    ThrowIfFailed(D3D12Context->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12PipelineObject->m_commandAllocator.Get(), 
        D3D12PipelineObject->m_pipelineState.Get(), IID_PPV_ARGS(&D3D12PipelineObject->m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(D3D12PipelineObject->m_commandList->Close());
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
}

void RHID3D12PipelineObject::SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding)
{
}

// RHID3D12GraphicsDispatcher implementation
void RHID3D12GraphicsDispatcher::Initialize(RHIContext* Context)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
}

void RHID3D12GraphicsDispatcher::Cleanup(RHIContext* Context, RHIWindowManager* WindowManager)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());

}

void RHID3D12GraphicsDispatcher::BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex)
{
    auto* D3D12BufferResource = static_cast<RHID3D12BufferResource*>(BufferResource->GetImpl());
    // Placeholder implementation
}

void RHID3D12GraphicsDispatcher::BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset)
{
    auto* D3D12BufferResource = static_cast<RHID3D12BufferResource*>(BufferResource->GetImpl());
    // Placeholder implementation
}

void RHID3D12GraphicsDispatcher::Dispatch(RHIWindowManager* WindowManager, RHIPipelineObject* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    auto* D3D12Pipeline = static_cast<RHID3D12PipelineObject*>(Pipeline->GetImpl());
        

    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(D3D12Pipeline->m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(D3D12Pipeline->m_commandList->Reset(D3D12Pipeline->m_commandAllocator.Get(), 
        D3D12Pipeline->m_pipelineState.Get()));

    // Set necessary state.
    D3D12Pipeline->m_commandList->SetGraphicsRootSignature( D3D12Pipeline->m_rootSignature.Get());
    D3D12Pipeline->m_commandList->RSSetViewports(1, &D3D12WindowManager->m_viewport);
    D3D12Pipeline->m_commandList->RSSetScissorRects(1, &D3D12WindowManager->m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    D3D12Pipeline->m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(D3D12WindowManager->m_renderTargets[D3D12WindowManager->m_frameIndex].Get(), 
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(D3D12WindowManager->m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        D3D12WindowManager->m_frameIndex, D3D12WindowManager->m_rtvDescriptorSize);
    D3D12Pipeline->m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    D3D12Pipeline->m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    D3D12Pipeline->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12Pipeline->m_commandList->IASetVertexBuffers(0, 1, & D3D12Pipeline->m_vertexBufferView);
    D3D12Pipeline->m_commandList->DrawInstanced(3, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    D3D12Pipeline->m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(D3D12WindowManager->m_renderTargets[D3D12WindowManager->m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(D3D12Pipeline->m_commandList->Close());
    CommandLists.push_back(D3D12Pipeline->m_commandList.Get());
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
    auto* D3D12RenderPass = static_cast<RHID3D12PresentPass*>(PresentPassResource->GetImpl());
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = D3D12WindowManager->m_fenceValue;
    ThrowIfFailed(D3D12Context->m_commandQueue->Signal(D3D12WindowManager->m_fence.Get(), fence));
    D3D12WindowManager->m_fenceValue++;

    // Wait until the previous frame is finished.
    if (D3D12WindowManager->m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(D3D12WindowManager->m_fence->SetEventOnCompletion(fence, D3D12WindowManager->m_fenceEvent));
        WaitForSingleObject(D3D12WindowManager->m_fenceEvent, INFINITE);
    }

    D3D12WindowManager->m_frameIndex = D3D12WindowManager->m_swapChain->GetCurrentBackBufferIndex();
    CommandLists.clear();
}

void RHID3D12GraphicsDispatcher::EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    // Execute the command list.
    D3D12Context->m_commandQueue->ExecuteCommandLists(CommandLists.size(), CommandLists.data());

    // Present the frame.
    ThrowIfFailed(D3D12WindowManager->m_swapChain->Present(1, 0));

    D3D12WindowManager->WaitForPreviousFrame(D3D12Context);
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
	
}

void RHID3D12PresentPass::CleanupSwapchainFramebuffer(RHIContext* Context)
{
}

void RHID3D12PresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT)
{
	
}

void RHID3D12PresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	
}

// RHID3D12ImGUI implementation
void RHID3D12ImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass)
{
    auto* D3D12Context = static_cast<RHID3D12Context*>(Context->GetImpl());
    auto* D3D12WindowManager = static_cast<RHID3D12WindowManager*>(WindowManager->GetImpl());
    auto* D3D12PresentPass = static_cast<RHID3D12PresentPass*>(PresentPass->GetImpl());
    // Placeholder implementation
}

void RHID3D12ImGUI::DispatchImGUI(RHIGraphicsDispatcher* Dispatcher)
{
    auto* D3D12Dispatcher = static_cast<RHID3D12GraphicsDispatcher*>(Dispatcher->GetImpl());
    // Placeholder implementation
}

void RHID3D12ImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context))
{
    // Placeholder implementation
}

void RHID3D12ImGUI::Cleanup()
{
    // Placeholder implementation
}