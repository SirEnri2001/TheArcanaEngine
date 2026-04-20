#pragma once
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <stdexcept>
#include <CoreLog.inl>

#include "directx/d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

_Use_decl_annotations_
void GetHardwareAdapter(
    IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter = false);
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed");
    }
}

#define CHECK_SYSTEM_ERROR(syscall) do{syscall;if(auto ErrorCode = GetLastError()){Error("SystemErrorCode ", ErrorCode, " at ", #syscall);}}while(0)

std::vector<uint32_t> read_spirv_file(const char* path);
std::string SPIRVToHLSL(const uint32_t* SPIRV_DATA, uint32_t size);

std::string SPIRVToGLSL(const uint32_t* SPIRV_DATA, uint32_t size);
