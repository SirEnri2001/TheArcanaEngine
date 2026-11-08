#pragma once

// The vendored Microsoft DirectX-Headers declare COM interfaces with
// MIDL_INTERFACE("..."), which only attaches a UUID attribute on MSVC.
// MinGW-w64 emulates __uuidof() through __mingw_uuidof<T>() template
// specializations registered by __CRT_UUID_DECL. DirectX-Headers and
// MinGW's own d3d12.h share the __d3d12_h__ include guard, so the vendored
// header wins and those registrations never happen. Register the interfaces
// consumed by IID_PPV_ARGS / ComPtr::As in this module explicitly.
#if defined(__MINGW32__) && defined(__CRT_UUID_DECL)

__CRT_UUID_DECL(ID3D12Device,          0x189819f1, 0x1db6, 0x4b57, 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7)
__CRT_UUID_DECL(ID3D12CommandQueue,    0x0ec870a6, 0x5d7e, 0x4c22, 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed)
__CRT_UUID_DECL(ID3D12DescriptorHeap,  0x8efb471d, 0x616c, 0x4f49, 0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51)
__CRT_UUID_DECL(ID3D12InfoQueue1,      0x2852dd88, 0xb484, 0x4c0c, 0xb6, 0xb1, 0x67, 0x16, 0x85, 0x00, 0xe6, 0x00)

#endif
