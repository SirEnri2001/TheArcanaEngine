#pragma once

#define RHI_IMPLEMENT
#include <RHI.h>
#include <d3d12.h>
#include <dxgi.h>

/// <summary>
/// Platform & application related: initialize devices, create window, configure swap chain...
/// </summary>



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

// ID3D12Debug debugInterface;

// IDXGIAdapter

//ComPtr<ID3DBlob> vertexShader;
//ComPtr<ID3DBlob> pixelShader;
//
//ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
//ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));