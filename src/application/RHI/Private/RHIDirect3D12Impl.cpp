#include<dxgi1_5.h>
#include <wrl/client.h>
#include "RHIDirect3D12.h"
#include <d3d12.h>


#define _DEBUG 1



/* ----------------------------- COM helper class and functions ------------------------------ */
inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}


class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT ErrorCode() const { return m_hr; }
private:
    const HRESULT m_hr;
};


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}


/* ---------------------------------- D3D Usage ---------------------------------- */

// Entry Point
void Initialize()
{
    HRESULT hr;
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> pDebugController;
        ComPtr<ID3D12Debug1> pDebugController1;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
        {
            pDebugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

            // Enable GPU-based validation
            if (SUCCEEDED(pDebugController1->QueryInterface(IID_PPV_ARGS(&pDebugController1))))
            {
                pDebugController1->SetEnableGPUBasedValidation(true);
                
            }
        }
    }
#endif


    ComPtr<IDXGIFactory5> pFactory;
    hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory));
    ThrowIfFailed(hr);


}






// Physical Device
// IDXGIAdapter (1?)


// Logical Device
// ID3D12Device


// Command Queue
// ID3D12CommandQueue


// CommandPool
// ID3D12CommandAllocator 
