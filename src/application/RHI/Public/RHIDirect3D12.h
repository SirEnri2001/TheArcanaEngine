#pragma once

#define RHI_IMPLEMENT
#include <RHI.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>


// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;


/// <summary>
/// Platform & application related: initialize devices, create window, configure swap chain...
/// </summary>
class RHI_API RHID3D12Context {
private:
	static constexpr UINT FrameCount = 2;	// May change later to support triple buffering
public:
	// DXGI_ADAPTER_DESC



	// Pipeline objects.
	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;
	ComPtr<ID3D12Device> Device;
	ComPtr<ID3D12Resource> RenderTargets[FrameCount];
	ComPtr<IDXGISwapChain> SwapChain;
	ComPtr<ID3D12CommandAllocator> CommandAllocator;
	ComPtr<ID3D12CommandQueue> CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> CommandList;
	ComPtr<ID3D12RootSignature> RootSignature;
	ComPtr<ID3D12PipelineState> PipelineState;
	ComPtr<ID3D12DescriptorHeap> RTVHeap;
	UINT RVTDescriptorSize;

	
};


/// <summary>
/// Rendering related: for resource accquisition, create buffers, texture formats...
/// </summary>




/// <summary>
/// Pipeline config related: create pipeline, bind shaders...
/// </summary>

		// D3D12_GRAPHICS_PIPELINE_STATE_DESC

		/* Resource bindings -------------------- */
		// Graphics pipeline states set outside of the pipeline state object
		// ID3D12GraphicsCommandList::IASetIndexBuffer();
		// ID3D12GraphicsCommandList4::BeginRenderPass


/// <summary>
/// CommandBuffer related: 
/// </summary>

		// ID3D12Device::CreateCommandList
		// ID3D12Device::CreateCommandAllocator




/* To be sorted ----------------------- */
// D3D12_GPU_VIRTUAL_ADDRESS
// IDXGIFactory::CreateSwapChain

//ID3D12Debug debugInterface;
//
//IDXGIAdapter