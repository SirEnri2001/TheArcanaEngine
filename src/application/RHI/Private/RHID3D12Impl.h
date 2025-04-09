#pragma once
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <stdexcept>
#include <CoreLog.inl>

#include "directx/d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;



//// Global variables (extern if needed in other files)
//extern CD3DX12_VIEWPORT m_viewport;
//extern CD3DX12_RECT m_scissorRect;
//extern 
//// Root assets path.
//extern std::wstring m_assetsPath;
//
//// Window title.
//extern std::wstring m_title;
//    // Viewport dimensions.
//extern float m_aspectRatio;
//
//inline std::string HrToString(HRESULT hr)
//{
//    char s_str[64] = {};
//    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
//    return std::string(s_str);
//}
//
//class HrException : public std::runtime_error
//{
//public:
//    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
//    HRESULT Error() const { return m_hr; }
//private:
//    const HRESULT m_hr;
//};
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
