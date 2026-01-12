

#define RHI_IMPLEMENT
#include "RHI.h"
#include "RHID3D12Impl.h"

#include "spirv_hlsl.hpp"

#include <stdexcept>
#include <d3dcompiler.h>
using Microsoft::WRL::ComPtr;
using namespace DirectX;

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

std::vector<uint32_t> read_spirv_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open SPIR-V file: %s\n", path);
        return {};
    }

    fseek(file, 0, SEEK_END);
    long len = ftell(file) / sizeof(uint32_t);
    rewind(file);

    std::vector<uint32_t> spirv(len);
    if (fread(spirv.data(), sizeof(uint32_t), len, file) != size_t(len))
        spirv.clear();

    fclose(file);
    return spirv;
}

std::string SPIRVToHLSL(const uint32_t* SPIRV_DATA, uint32_t size)
{
    spirv_cross::CompilerHLSL HLSL(SPIRV_DATA, size);
    
    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = HLSL.get_shader_resources();

    // Get all sampled images in the shader.
    for (auto& resource : resources.sampled_images)
    {
        unsigned set = HLSL.get_decoration(resource.id, spv::DecorationDescriptorSet);
        unsigned binding = HLSL.get_decoration(resource.id, spv::DecorationBinding);
        printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

        // Modify the decoration to prepare it for GLSL.
        HLSL.unset_decoration(resource.id, spv::DecorationDescriptorSet);

        // Some arbitrary remapping if we want.
        HLSL.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
    }

    // Set some options.
    spirv_cross::CompilerGLSL::Options options;
    options.es = true;
    HLSL.set_common_options(options);
    auto HLSLOptions = HLSL.get_hlsl_options();
    HLSLOptions.shader_model = 40;
    HLSL.set_hlsl_options(HLSLOptions);
    spirv_cross::HLSLVertexAttributeRemap VertexAttribRemap;
    VertexAttribRemap.location = 0;
    VertexAttribRemap.semantic = "ATTRIBUTE0";
    HLSL.add_vertex_attribute_remap(VertexAttribRemap);

    // Compile to GLSL, ready to give to GL driver.
    std::string source = HLSL.compile();
    return source;
}

std::string SPIRVToGLSL(const uint32_t* SPIRV_DATA, uint32_t size)
{
    spirv_cross::CompilerGLSL GLSL(SPIRV_DATA, size);

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = GLSL.get_shader_resources();

    // Get all sampled images in the shader.
    for (auto& resource : resources.sampled_images)
    {
        unsigned set = GLSL.get_decoration(resource.id, spv::DecorationDescriptorSet);
        unsigned binding = GLSL.get_decoration(resource.id, spv::DecorationBinding);
        printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

        // Modify the decoration to prepare it for GLSL.
        GLSL.unset_decoration(resource.id, spv::DecorationDescriptorSet);

        // Some arbitrary remapping if we want.
        GLSL.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
    }

    // Set some options.
    spirv_cross::CompilerGLSL::Options options;
    options.version = 310;
    options.es = true;
    GLSL.set_common_options(options);

    // Compile to GLSL, ready to give to GL driver.
    std::string source = GLSL.compile();
    return source;
}
