

#define RHI_IMPLEMENT
#include "RHI.h"
#include "RHID3D12Impl.h"
#include <stdexcept>
#include <d3dcompiler.h>
using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

//// Pipeline objects.
//ComPtr<IDXGISwapChain3> m_swapChain;
//ComPtr<ID3D12Device> m_device;
//ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
//ComPtr<ID3D12CommandAllocator> m_commandAllocator;
//ComPtr<ID3D12CommandQueue> m_commandQueue;
//ComPtr<ID3D12RootSignature> m_rootSignature;
//ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
//ComPtr<ID3D12PipelineState> m_pipelineState;
//ComPtr<ID3D12GraphicsCommandList> m_commandList;
//UINT m_rtvDescriptorSize;
//
//// App resources.
//ComPtr<ID3D12Resource> m_vertexBuffer;
//D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
//
//// Synchronization objects.
//UINT m_frameIndex;
//HANDLE m_fenceEvent;
//ComPtr<ID3D12Fence> m_fence;
//UINT64 m_fenceValue;
//
//bool m_useWarpDevice = false;
//
//// Root assets path.
//std::wstring m_assetsPath;
//
//// Window title.
//std::wstring m_title;
//    // Viewport dimensions.
//float m_aspectRatio;



// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
void GetHardwareAdapter(
    IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if(adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }
    
    *ppAdapter = adapter.Detach();
}