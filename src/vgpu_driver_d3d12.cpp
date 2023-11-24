// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_D3D12_DRIVER)

// Use the C++ standard templated min/max
#ifndef NOMINMAX
#define NOMINMAX
#endif

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <dxgi1_6.h>
#include <directx/d3d12.h>
#include <directx/d3dx12_default.h>
#include <directx/d3dx12_resource_helpers.h>
#include <directx/d3dx12_pipeline_state_stream.h>
#include <directx/d3dx12_check_feature_support.h>
//#include <windows.ui.xaml.media.dxinterop.h>
#include <wrl/client.h>
#include "vgpu_driver.h"

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "third_party/D3D12MemAlloc.h"

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   include <dxgidebug.h>
#endif

#include <deque>
#include <sstream>
#include <mutex>
#include <unordered_map>

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

using Microsoft::WRL::ComPtr;

#define VHR(hr) if (FAILED(hr)) { VGPU_ASSERT(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

namespace
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    using PFN_CREATE_DXGI_FACTORY2 = decltype(&CreateDXGIFactory2);
    static PFN_CREATE_DXGI_FACTORY2 vgpuCreateDXGIFactory2 = nullptr;

    static PFN_D3D12_GET_DEBUG_INTERFACE vgpuD3D12GetDebugInterface = nullptr;
    static PFN_D3D12_CREATE_DEVICE vgpuD3D12CreateDevice = nullptr;
    static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE vgpuD3D12SerializeVersionedRootSignature = nullptr;

#if defined(_DEBUG)
    // Declare debug guids to avoid linking with "dxguid.lib"
    static constexpr IID VGFX_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8} };
    static constexpr IID VGFX_DXGI_DEBUG_DXGI = { 0x25cddaa4, 0xb1c6, 0x47e1, {0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a} };

    using PFN_DXGI_GET_DEBUG_INTERFACE1 = decltype(&DXGIGetDebugInterface1);
    static PFN_DXGI_GET_DEBUG_INTERFACE1 vgpuDXGIGetDebugInterface1 = nullptr;
#endif
#endif

    template <typename T>
    static bool IsPow2(T x) { return (x & (x - 1)) == 0; }

    // Aligns given value up to nearest multiply of align value. For example: AlignUp(11, 8) = 16.
    // Use types like UINT, uint64_t as T.
    template <typename T>
    static T AlignUp(T val, T alignment)
    {
        VGPU_ASSERT(IsPow2(alignment));
        return (val + alignment - 1) & ~(alignment - 1);
    }

    inline std::string WCharToUTF8(const wchar_t* input)
    {
        // The -1 argument asks WideCharToMultiByte to use the null terminator to know the size of
        // input. It will return a size that includes the null terminator.
        const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
        if (requiredSize < 0)
            return "";

        char* char_buffer = (char*)alloca(sizeof(char) * requiredSize);
        WideCharToMultiByte(CP_UTF8, 0, input, -1, char_buffer, requiredSize, nullptr, nullptr);
        return std::string(char_buffer, requiredSize);
    }

    inline std::wstring UTF8ToWStr(const char* input)
    {
        // The -1 argument asks MultiByteToWideChar to use the null terminator to know the size of
        // input. It will return a size that includes the null terminator.
        const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, input, -1, nullptr, 0);
        if (requiredSize < 0)
            return L"";

        wchar_t* char_buffer = (wchar_t*)alloca(sizeof(wchar_t) * requiredSize);
        MultiByteToWideChar(CP_UTF8, 0, input, -1, char_buffer, requiredSize);
        return std::wstring(char_buffer, requiredSize);
    }

    inline void D3D12SetName(ID3D12Object* obj, const char* name)
    {
        if (obj)
        {
            if (name != nullptr)
            {
                std::wstring wide_name = UTF8ToWStr(name);
                obj->SetName(wide_name.c_str());
            }
        }
    }

    static_assert(sizeof(VGPUViewport) == sizeof(D3D12_VIEWPORT));
    static_assert(offsetof(VGPUViewport, x) == offsetof(D3D12_VIEWPORT, TopLeftX));
    static_assert(offsetof(VGPUViewport, y) == offsetof(D3D12_VIEWPORT, TopLeftY));
    static_assert(offsetof(VGPUViewport, width) == offsetof(D3D12_VIEWPORT, Width));
    static_assert(offsetof(VGPUViewport, height) == offsetof(D3D12_VIEWPORT, Height));
    static_assert(offsetof(VGPUViewport, minDepth) == offsetof(D3D12_VIEWPORT, MinDepth));
    static_assert(offsetof(VGPUViewport, maxDepth) == offsetof(D3D12_VIEWPORT, MaxDepth));

    static_assert(sizeof(VGPUDispatchIndirectCommand) == sizeof(D3D12_DISPATCH_ARGUMENTS), "DispatchIndirectCommand mismatch");
    static_assert(offsetof(VGPUDispatchIndirectCommand, x) == offsetof(D3D12_DISPATCH_ARGUMENTS, ThreadGroupCountX), "Layout mismatch");
    static_assert(offsetof(VGPUDispatchIndirectCommand, y) == offsetof(D3D12_DISPATCH_ARGUMENTS, ThreadGroupCountY), "Layout mismatch");
    static_assert(offsetof(VGPUDispatchIndirectCommand, z) == offsetof(D3D12_DISPATCH_ARGUMENTS, ThreadGroupCountZ), "Layout mismatch");

    static_assert(sizeof(VGPUDrawIndirectCommand) == sizeof(D3D12_DRAW_ARGUMENTS), "DrawIndirectCommand mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, vertexCount) == offsetof(D3D12_DRAW_ARGUMENTS, VertexCountPerInstance), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, instanceCount) == offsetof(D3D12_DRAW_ARGUMENTS, InstanceCount), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, firstVertex) == offsetof(D3D12_DRAW_ARGUMENTS, StartVertexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, firstInstance) == offsetof(D3D12_DRAW_ARGUMENTS, StartInstanceLocation), "Layout mismatch");

    static_assert(sizeof(VGPUDrawIndexedIndirectCommand) == sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), "DrawIndexedIndirectCommand mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, indexCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, IndexCountPerInstance), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, instanceCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, InstanceCount), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, firstIndex) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartIndexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, baseVertex) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, BaseVertexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, firstInstance) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartInstanceLocation), "Layout mismatch");

    constexpr DXGI_FORMAT ToDxgiFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            // 8-bit formats
            case VGPUTextureFormat_R8Unorm:         return DXGI_FORMAT_R8_UNORM;
            case VGPUTextureFormat_R8Snorm:         return DXGI_FORMAT_R8_SNORM;
            case VGPUTextureFormat_R8Uint:          return DXGI_FORMAT_R8_UINT;
            case VGPUTextureFormat_R8Sint:          return DXGI_FORMAT_R8_SINT;
                // 16-bit formats
            case VGPUTextureFormat_R16Unorm:        return DXGI_FORMAT_R16_UNORM;
            case VGPUTextureFormat_R16Snorm:        return DXGI_FORMAT_R16_SNORM;
            case VGPUTextureFormat_R16Uint:         return DXGI_FORMAT_R16_UINT;
            case VGPUTextureFormat_R16Sint:         return DXGI_FORMAT_R16_SINT;
            case VGPUTextureFormat_R16Float:        return DXGI_FORMAT_R16_FLOAT;
            case VGPUTextureFormat_RG8Unorm:        return DXGI_FORMAT_R8G8_UNORM;
            case VGPUTextureFormat_RG8Snorm:        return DXGI_FORMAT_R8G8_SNORM;
            case VGPUTextureFormat_RG8Uint:         return DXGI_FORMAT_R8G8_UINT;
            case VGPUTextureFormat_RG8Sint:         return DXGI_FORMAT_R8G8_SINT;
                // Packed 16-Bit Pixel Formats
            case VGPUTextureFormat_BGRA4Unorm:      return DXGI_FORMAT_B4G4R4A4_UNORM;
            case VGPUTextureFormat_B5G6R5Unorm:     return DXGI_FORMAT_B5G6R5_UNORM;
            case VGPUTextureFormat_B5G5R5A1Unorm:   return DXGI_FORMAT_B5G5R5A1_UNORM;
                // 32-bit formats
            case VGPUTextureFormat_R32Uint:          return DXGI_FORMAT_R32_UINT;
            case VGPUTextureFormat_R32Sint:          return DXGI_FORMAT_R32_SINT;
            case VGPUTextureFormat_R32Float:         return DXGI_FORMAT_R32_FLOAT;
            case VGPUTextureFormat_RG16Uint:         return DXGI_FORMAT_R16G16_UINT;
            case VGPUTextureFormat_RG16Sint:         return DXGI_FORMAT_R16G16_SINT;
            case VGPUTextureFormat_RG16Unorm:        return DXGI_FORMAT_R16G16_UNORM;
            case VGPUTextureFormat_RG16Snorm:        return DXGI_FORMAT_R16G16_SNORM;
            case VGPUTextureFormat_RG16Float:        return DXGI_FORMAT_R16G16_FLOAT;
            case VGPUTextureFormat_RGBA8Uint:        return DXGI_FORMAT_R8G8B8A8_UINT;
            case VGPUTextureFormat_RGBA8Sint:        return DXGI_FORMAT_R8G8B8A8_SINT;
            case VGPUTextureFormat_RGBA8Unorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
            case VGPUTextureFormat_RGBA8UnormSrgb:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case VGPUTextureFormat_RGBA8Snorm:       return DXGI_FORMAT_R8G8B8A8_SNORM;
            case VGPUTextureFormat_BGRA8Unorm:       return DXGI_FORMAT_B8G8R8A8_UNORM;
            case VGPUTextureFormat_BGRA8UnormSrgb:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                // Packed 32-Bit formats
            case VGPUTextureFormat_RGB9E5Ufloat:     return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
            case VGPUTextureFormat_RGB10A2Unorm:     return DXGI_FORMAT_R10G10B10A2_UNORM;
            case VGPUTextureFormat_RGB10A2Uint:      return DXGI_FORMAT_R10G10B10A2_UINT;
            case VGPUTextureFormat_RG11B10Float:     return DXGI_FORMAT_R11G11B10_FLOAT;
                // 64-Bit formats
            case VGPUTextureFormat_RG32Uint:         return DXGI_FORMAT_R32G32_UINT;
            case VGPUTextureFormat_RG32Sint:         return DXGI_FORMAT_R32G32_SINT;
            case VGPUTextureFormat_RG32Float:        return DXGI_FORMAT_R32G32_FLOAT;
            case VGPUTextureFormat_RGBA16Unorm:      return DXGI_FORMAT_R16G16B16A16_UNORM;
            case VGPUTextureFormat_RGBA16Snorm:      return DXGI_FORMAT_R16G16B16A16_SNORM;
            case VGPUTextureFormat_RGBA16Uint:       return DXGI_FORMAT_R16G16B16A16_UINT;
            case VGPUTextureFormat_RGBA16Sint:       return DXGI_FORMAT_R16G16B16A16_SINT;
            case VGPUTextureFormat_RGBA16Float:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
                // 128-Bit formats
            case VGPUTextureFormat_RGBA32Uint:       return DXGI_FORMAT_R32G32B32A32_UINT;
            case VGPUTextureFormat_RGBA32Sint:       return DXGI_FORMAT_R32G32B32A32_SINT;
            case VGPUTextureFormat_RGBA32Float:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
                // Depth-stencil formats
            case VGPUTextureFormat_Depth16Unorm:		    return DXGI_FORMAT_D16_UNORM;
            case VGPUTextureFormat_Depth32Float:			return DXGI_FORMAT_D32_FLOAT;
            case VGPUTextureFormat_Stencil8:			    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth24UnormStencil8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth32FloatStencil8:    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                // Compressed BC formats
            case VGPUTextureFormat_Bc1RgbaUnorm:            return DXGI_FORMAT_BC1_UNORM;
            case VGPUTextureFormat_Bc1RgbaUnormSrgb:        return DXGI_FORMAT_BC1_UNORM_SRGB;
            case VGPUTextureFormat_Bc2RgbaUnorm:            return DXGI_FORMAT_BC2_UNORM;
            case VGPUTextureFormat_Bc2RgbaUnormSrgb:        return DXGI_FORMAT_BC2_UNORM_SRGB;
            case VGPUTextureFormat_Bc3RgbaUnorm:            return DXGI_FORMAT_BC3_UNORM;
            case VGPUTextureFormat_Bc3RgbaUnormSrgb:        return DXGI_FORMAT_BC3_UNORM_SRGB;
            case VGPUTextureFormat_Bc4RSnorm:               return DXGI_FORMAT_BC4_SNORM;
            case VGPUTextureFormat_Bc4RUnorm:               return DXGI_FORMAT_BC4_UNORM;
            case VGPUTextureFormat_Bc5RgSnorm:              return DXGI_FORMAT_BC5_SNORM;
            case VGPUTextureFormat_Bc5RgUnorm:              return DXGI_FORMAT_BC5_UNORM;
            case VGPUTextureFormat_Bc6hRgbUfloat:           return DXGI_FORMAT_BC6H_UF16;
            case VGPUTextureFormat_Bc6hRgbSfloat:           return DXGI_FORMAT_BC6H_SF16;
            case VGPUTextureFormat_Bc7RgbaUnorm:            return DXGI_FORMAT_BC7_UNORM;
            case VGPUTextureFormat_Bc7RgbaUnormSrgb:        return DXGI_FORMAT_BC7_UNORM_SRGB;

            default:
                return DXGI_FORMAT_UNKNOWN;
        }
    }

    DXGI_FORMAT ToDxgiFormat(VGPUVertexFormat format)
    {
        switch (format)
        {
            case VGPUVertexFormat_UByte2:              return DXGI_FORMAT_R8G8_UINT;
            case VGPUVertexFormat_UByte4:              return DXGI_FORMAT_R8G8B8A8_UINT;
            case VGPUVertexFormat_Byte2:               return DXGI_FORMAT_R8G8_SINT;
            case VGPUVertexFormat_Byte4:               return DXGI_FORMAT_R8G8B8A8_SINT;
            case VGPUVertexFormat_UByte2Normalized:    return DXGI_FORMAT_R8G8_UNORM;
            case VGPUVertexFormat_UByte4Normalized:    return DXGI_FORMAT_R8G8B8A8_UNORM;
            case VGPUVertexFormat_Byte2Normalized:     return DXGI_FORMAT_R8G8_SNORM;
            case VGPUVertexFormat_Byte4Normalized:     return DXGI_FORMAT_R8G8B8A8_SNORM;

            case VGPUVertexFormat_UShort2:             return DXGI_FORMAT_R16G16_UINT;
            case VGPUVertexFormat_UShort4:             return DXGI_FORMAT_R16G16B16A16_UINT;
            case VGPUVertexFormat_Short2:              return DXGI_FORMAT_R16G16_SINT;
            case VGPUVertexFormat_Short4:              return DXGI_FORMAT_R16G16B16A16_SINT;
            case VGPUVertexFormat_UShort2Normalized:   return DXGI_FORMAT_R16G16_UNORM;
            case VGPUVertexFormat_UShort4Normalized:   return DXGI_FORMAT_R16G16B16A16_UNORM;
            case VGPUVertexFormat_Short2Normalized:    return DXGI_FORMAT_R16G16_SNORM;
            case VGPUVertexFormat_Short4Normalized:    return DXGI_FORMAT_R16G16B16A16_SNORM;
            case VGPUVertexFormat_Half2:               return DXGI_FORMAT_R16G16_FLOAT;
            case VGPUVertexFormat_Half4:               return DXGI_FORMAT_R16G16B16A16_FLOAT;

            case VGPUVertexFormat_Float:               return DXGI_FORMAT_R32_FLOAT;
            case VGPUVertexFormat_Float2:              return DXGI_FORMAT_R32G32_FLOAT;
            case VGPUVertexFormat_Float3:              return DXGI_FORMAT_R32G32B32_FLOAT;
            case VGPUVertexFormat_Float4:              return DXGI_FORMAT_R32G32B32A32_FLOAT;

            case VGPUVertexFormat_UInt:                return DXGI_FORMAT_R32_UINT;
            case VGPUVertexFormat_UInt2:               return DXGI_FORMAT_R32G32_UINT;
            case VGPUVertexFormat_UInt3:               return DXGI_FORMAT_R32G32B32_UINT;
            case VGPUVertexFormat_UInt4:               return DXGI_FORMAT_R32G32B32A32_UINT;

            case VGPUVertexFormat_Int:                 return DXGI_FORMAT_R32_SINT;
            case VGPUVertexFormat_Int2:                return DXGI_FORMAT_R32G32_SINT;
            case VGPUVertexFormat_Int3:                return DXGI_FORMAT_R32G32B32_SINT;
            case VGPUVertexFormat_Int4:                return DXGI_FORMAT_R32G32B32A32_SINT;

            case VGPUVertexFormat_Int1010102Normalized:    return DXGI_FORMAT_R10G10B10A2_UNORM;
            case VGPUVertexFormat_UInt1010102Normalized:   return DXGI_FORMAT_R10G10B10A2_UINT;

            default:
                VGPU_UNREACHABLE();
        }
    }

    constexpr VGPUTextureFormat FromDxgiFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
            // 8-bit formats
            case DXGI_FORMAT_R8_UNORM:              return VGPUTextureFormat_R8Unorm;
            case DXGI_FORMAT_R8_SNORM:              return VGPUTextureFormat_R8Snorm;
            case DXGI_FORMAT_R8_UINT:               return VGPUTextureFormat_R8Uint;
            case DXGI_FORMAT_R8_SINT:               return VGPUTextureFormat_R8Sint;
                // 16-bit formats
            case DXGI_FORMAT_R16_UNORM:             return VGPUTextureFormat_R16Unorm;
            case DXGI_FORMAT_R16_SNORM:             return VGPUTextureFormat_R16Snorm;
            case DXGI_FORMAT_R16_UINT:              return VGPUTextureFormat_R16Uint;
            case DXGI_FORMAT_R16_SINT:              return VGPUTextureFormat_R16Sint;
            case DXGI_FORMAT_R16_FLOAT:             return VGPUTextureFormat_R16Float;
            case DXGI_FORMAT_R8G8_UNORM:            return VGPUTextureFormat_RG8Unorm;
            case DXGI_FORMAT_R8G8_SNORM:            return VGPUTextureFormat_RG8Snorm;
            case DXGI_FORMAT_R8G8_UINT:             return VGPUTextureFormat_RG8Uint;
            case DXGI_FORMAT_R8G8_SINT:             return VGPUTextureFormat_RG8Sint;
                // Packed 16-Bit Pixel Formats
            case DXGI_FORMAT_B4G4R4A4_UNORM:        return VGPUTextureFormat_BGRA4Unorm;
            case DXGI_FORMAT_B5G6R5_UNORM:          return VGPUTextureFormat_B5G6R5Unorm;
            case DXGI_FORMAT_B5G5R5A1_UNORM:        return VGPUTextureFormat_B5G5R5A1Unorm;
#if TODO
                // 32-bit formats
            case VGPUTextureFormat_R32UInt:          return DXGI_FORMAT_R32_UINT;
            case VGPUTextureFormat_R32SInt:          return DXGI_FORMAT_R32_SINT;
            case VGPUTextureFormat_R32Float:         return DXGI_FORMAT_R32_FLOAT;
            case VGPUTextureFormat_RG16UInt:         return DXGI_FORMAT_R16G16_UINT;
            case VGPUTextureFormat_RG16SInt:         return DXGI_FORMAT_R16G16_SINT;
            case VGPUTextureFormat_RG16UNorm:        return DXGI_FORMAT_R16G16_UNORM;
            case VGPUTextureFormat_RG16SNorm:        return DXGI_FORMAT_R16G16_SNORM;
            case VGPUTextureFormat_RG16Float:        return DXGI_FORMAT_R16G16_FLOAT;
            case VGPUTextureFormat_RGBA8UInt:        return DXGI_FORMAT_R8G8B8A8_UINT;
            case VGPUTextureFormat_RGBA8SInt:        return DXGI_FORMAT_R8G8B8A8_SINT;
            case VGPUTextureFormat_RGBA8UNorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
            case VGPUTextureFormat_RGBA8UNormSrgb:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case VGPUTextureFormat_RGBA8SNorm:       return DXGI_FORMAT_R8G8B8A8_SNORM;
            case VGPUTextureFormat_BGRA8UNorm:       return DXGI_FORMAT_B8G8R8A8_UNORM;
            case VGPUTextureFormat_BGRA8UNormSrgb:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                // Packed 32-Bit formats
            case VGPUTextureFormat_RGB10A2UNorm:     return DXGI_FORMAT_R10G10B10A2_UNORM;
            case VGPUTextureFormat_RG11B10Float:     return DXGI_FORMAT_R11G11B10_FLOAT;
            case VGPUTextureFormat_RGB9E5Float:      return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
                // 64-Bit formats
            case VGPUTextureFormat_RG32UInt:         return DXGI_FORMAT_R32G32_UINT;
            case VGPUTextureFormat_RG32SInt:         return DXGI_FORMAT_R32G32_SINT;
            case VGPUTextureFormat_RG32Float:        return DXGI_FORMAT_R32G32_FLOAT;
            case VGPUTextureFormat_RGBA16UInt:       return DXGI_FORMAT_R16G16B16A16_UINT;
            case VGPUTextureFormat_RGBA16SInt:       return DXGI_FORMAT_R16G16B16A16_SINT;
            case VGPUTextureFormat_RGBA16UNorm:      return DXGI_FORMAT_R16G16B16A16_UNORM;
            case VGPUTextureFormat_RGBA16SNorm:      return DXGI_FORMAT_R16G16B16A16_SNORM;
            case VGPUTextureFormat_RGBA16Float:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
                // 128-Bit formats
            case VGPUTextureFormat_RGBA32UInt:       return DXGI_FORMAT_R32G32B32A32_UINT;
            case VGPUTextureFormat_RGBA32SInt:       return DXGI_FORMAT_R32G32B32A32_SINT;
            case VGPUTextureFormat_RGBA32Float:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
                // Depth-stencil formats
            case VGPUTextureFormat_Depth16Unorm:		    return DXGI_FORMAT_D16_UNORM;
            case VGPUTextureFormat_Depth32Float:			return DXGI_FORMAT_D32_FLOAT;
            case VGPUTextureFormat_Stencil8:			    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth24UnormStencil8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth32FloatStencil8:    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                // Compressed BC formats
            case VGPUTextureFormat_BC1UNorm:            return DXGI_FORMAT_BC1_UNORM;
            case VGPUTextureFormat_BC1UNormSrgb:        return DXGI_FORMAT_BC1_UNORM_SRGB;
            case VGPUTextureFormat_BC2UNorm:            return DXGI_FORMAT_BC2_UNORM;
            case VGPUTextureFormat_BC2UNormSrgb:        return DXGI_FORMAT_BC2_UNORM_SRGB;
            case VGPUTextureFormat_BC3UNorm:            return DXGI_FORMAT_BC3_UNORM;
            case VGPUTextureFormat_BC3UNormSrgb:        return DXGI_FORMAT_BC3_UNORM_SRGB;
            case VGPUTextureFormat_BC4SNorm:            return DXGI_FORMAT_BC4_SNORM;
            case VGPUTextureFormat_BC4UNorm:            return DXGI_FORMAT_BC4_UNORM;
            case VGPUTextureFormat_BC5SNorm:            return DXGI_FORMAT_BC5_SNORM;
            case VGPUTextureFormat_BC5UNorm:            return DXGI_FORMAT_BC5_UNORM;
            case VGPUTextureFormat_BC6HUFloat:          return DXGI_FORMAT_BC6H_UF16;
            case VGPUTextureFormat_BC6HSFloat:          return DXGI_FORMAT_BC6H_SF16;
            case VGPUTextureFormat_BC7UNorm:            return DXGI_FORMAT_BC7_UNORM;
            case VGPUTextureFormat_BC7UNormSrgb:        return DXGI_FORMAT_BC7_UNORM_SRGB;
#endif // TODO


            default:
                return VGPUTextureFormat_Undefined;
        }
    }

    constexpr VGPUTextureFormat ToDXGISwapChainFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            case VGPUTextureFormat_RGBA16Float:
                return VGPUTextureFormat_RGBA16Float;

            case VGPUTextureFormat_BGRA8Unorm:
            case VGPUTextureFormat_BGRA8UnormSrgb:
                return VGPUTextureFormat_BGRA8Unorm;

            case VGPUTextureFormat_RGBA8Unorm:
            case VGPUTextureFormat_RGBA8UnormSrgb:
                return VGPUTextureFormat_RGBA8Unorm;

            case VGPUTextureFormat_RGB10A2Unorm:
                return VGPUTextureFormat_RGB10A2Unorm;
        }

        return VGPUTextureFormat_BGRA8Unorm;
    }

    constexpr DXGI_FORMAT GetTypelessFormatFromDepthFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            case VGPUTextureFormat_Stencil8:
                return DXGI_FORMAT_R24G8_TYPELESS;
            case VGPUTextureFormat_Depth16Unorm:
                return DXGI_FORMAT_R16_TYPELESS;
            case VGPUTextureFormat_Depth32Float:
                return DXGI_FORMAT_R32_TYPELESS;
            case VGPUTextureFormat_Depth24UnormStencil8:
                return DXGI_FORMAT_R24G8_TYPELESS;
            case VGPUTextureFormat_Depth32FloatStencil8:
                return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            default:
                VGPU_ASSERT(vgpuIsDepthStencilFormat(format) == false);
                return ToDxgiFormat(format);
        }
    }

    constexpr uint32_t PresentModeToBufferCount(VGPUPresentMode mode)
    {
        switch (mode)
        {
            case VGPUPresentMode_Immediate:
            case VGPUPresentMode_Fifo:
                return 2;
            case VGPUPresentMode_Mailbox:
                return 3;
            default:
                return 2;
        }
    }

    constexpr uint32_t PresentModeToSwapInterval(VGPUPresentMode mode)
    {
        switch (mode)
        {
            case VGPUPresentMode_Immediate:
            case VGPUPresentMode_Mailbox:
                return 0u;

            case VGPUPresentMode_Fifo:
            default:
                return 1u;
        }
    }


    constexpr D3D_PRIMITIVE_TOPOLOGY ToD3DPrimitiveTopology(VGPUPrimitiveTopology type, uint32_t patchControlPoints = 1)
    {
        switch (type)
        {
            case VGPUPrimitiveTopology_PointList:
                return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            case VGPUPrimitiveTopology_LineList:
                return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case VGPUPrimitiveTopology_LineStrip:
                return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case VGPUPrimitiveTopology_TriangleList:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case VGPUPrimitiveTopology_TriangleStrip:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            case VGPUPrimitiveTopology_PatchList:
                if (patchControlPoints == 0 || patchControlPoints > 32)
                {
                    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
                }

                return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (patchControlPoints - 1));

            default:
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }
    }

    constexpr D3D12_COMMAND_LIST_TYPE ToD3D12(VGPUCommandQueue type)
    {
        switch (type)
        {
            case VGPUCommandQueue_Graphics:
                return D3D12_COMMAND_LIST_TYPE_DIRECT;

            case VGPUCommandQueue_Compute:
                return D3D12_COMMAND_LIST_TYPE_COMPUTE;

            case VGPUCommandQueue_Copy:
                return D3D12_COMMAND_LIST_TYPE_COPY;

            default:
                VGPU_UNREACHABLE();
        }
    }

    constexpr D3D12_COMPARISON_FUNC ToD3D12(VGPUCompareFunction function)
    {
        switch (function)
        {
            case VGPUCompareFunction_Never:        return D3D12_COMPARISON_FUNC_NEVER;
            case VGPUCompareFunction_Less:         return D3D12_COMPARISON_FUNC_LESS;
            case VGPUCompareFunction_Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
            case VGPUCompareFunction_LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case VGPUCompareFunction_Greater:      return D3D12_COMPARISON_FUNC_GREATER;
            case VGPUCompareFunction_NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case VGPUCompareFunction_GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case VGPUCompareFunction_Always:       return D3D12_COMPARISON_FUNC_ALWAYS;

            default:
                return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    constexpr D3D12_STENCIL_OP ToD3D12(VGPUStencilOperation op)
    {
        switch (op)
        {
            case VGPUStencilOperation_Keep:            return D3D12_STENCIL_OP_KEEP;
            case VGPUStencilOperation_Zero:            return D3D12_STENCIL_OP_ZERO;
            case VGPUStencilOperation_Replace:         return D3D12_STENCIL_OP_REPLACE;
            case VGPUStencilOperation_IncrementClamp:  return D3D12_STENCIL_OP_INCR_SAT;
            case VGPUStencilOperation_DecrementClamp:  return D3D12_STENCIL_OP_DECR_SAT;
            case VGPUStencilOperation_Invert:          return D3D12_STENCIL_OP_INVERT;
            case VGPUStencilOperation_IncrementWrap:   return D3D12_STENCIL_OP_INCR;
            case VGPUStencilOperation_DecrementWrap:   return D3D12_STENCIL_OP_DECR;
            default:
                return D3D12_STENCIL_OP_KEEP;
        }
    }

    constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE ToD3D12(VGPULoadAction action)
    {
        switch (action)
        {
            default:
            case VGPULoadAction_Load:
                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;

            case VGPULoadAction_Clear:
                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;

            case VGPULoadAction_DontCare:
                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        }
    }

    constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE ToD3D12(VGPUStoreAction action)
    {
        switch (action)
        {
            default:
            case VGPUStoreAction_Store:
                return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

            case VGPUStoreAction_DontCare:
                return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        }
    }

    constexpr D3D12_FILTER_TYPE ToD3D12FilterType(VGPUSamplerFilter value)
    {
        switch (value)
        {
            case VGPUSamplerFilter_Linear:
                return D3D12_FILTER_TYPE_LINEAR;
            default:
            case VGPUSamplerFilter_Nearest:
                return D3D12_FILTER_TYPE_POINT;
        }
    }

    constexpr D3D12_FILTER_TYPE ToD3D12FilterType(VGPUSamplerMipFilter value)
    {
        switch (value)
        {
            case VGPUSamplerMipFilter_Linear:
                return D3D12_FILTER_TYPE_LINEAR;
            default:
            case VGPUSamplerMipFilter_Nearest:
                return D3D12_FILTER_TYPE_POINT;
        }
    }

    constexpr D3D12_TEXTURE_ADDRESS_MODE ToD3D12AddressMode(VGPUSamplerAddressMode mode)
    {
        switch (mode)
        {
            case VGPUSamplerAddressMode_Mirror:
                return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case VGPUSamplerAddressMode_Clamp:
                return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case VGPUSamplerAddressMode_Border:
                return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                //case VGPUSamplerAddressMode_MirrorOnce:
                //    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;

            default:
            case VGPUSamplerAddressMode_Wrap:
                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

    constexpr D3D12_BLEND D3D12Blend(VGPUBlendFactor factor, bool alphaBlendFactorSupported)
    {
        switch (factor)
        {
            case VGPUBlendFactor_Zero:                      return D3D12_BLEND_ZERO;
            case VGPUBlendFactor_One:                       return D3D12_BLEND_ONE;
            case VGPUBlendFactor_SourceColor:               return D3D12_BLEND_SRC_COLOR;
            case VGPUBlendFactor_OneMinusSourceColor:       return D3D12_BLEND_INV_SRC_COLOR;
            case VGPUBlendFactor_SourceAlpha:               return D3D12_BLEND_SRC_ALPHA;
            case VGPUBlendFactor_OneMinusSourceAlpha:       return D3D12_BLEND_INV_SRC_ALPHA;
            case VGPUBlendFactor_DestinationColor:          return D3D12_BLEND_DEST_COLOR;
            case VGPUBlendFactor_OneMinusDestinationColor:  return D3D12_BLEND_INV_DEST_COLOR;
            case VGPUBlendFactor_DestinationAlpha:          return D3D12_BLEND_DEST_ALPHA;
            case VGPUBlendFactor_OneMinusDestinationAlpha:  return D3D12_BLEND_INV_DEST_ALPHA;
            case VGPUBlendFactor_SourceAlphaSaturated:      return D3D12_BLEND_SRC_ALPHA_SAT;
            case VGPUBlendFactor_BlendColor:                return D3D12_BLEND_BLEND_FACTOR;
            case VGPUBlendFactor_OneMinusBlendColor:        return D3D12_BLEND_INV_BLEND_FACTOR;
            case VGPUBlendFactor_BlendAlpha:                return alphaBlendFactorSupported ? D3D12_BLEND_ALPHA_FACTOR : D3D12_BLEND_BLEND_FACTOR;
            case VGPUBlendFactor_OneMinusBlendAlpha:        return alphaBlendFactorSupported ? D3D12_BLEND_INV_ALPHA_FACTOR : D3D12_BLEND_INV_BLEND_FACTOR;
            case VGPUBlendFactor_Source1Color:              return D3D12_BLEND_SRC1_COLOR;
            case VGPUBlendFactor_OneMinusSource1Color:      return D3D12_BLEND_INV_SRC1_COLOR;
            case VGPUBlendFactor_Source1Alpha:              return D3D12_BLEND_SRC1_ALPHA;
            case VGPUBlendFactor_OneMinusSource1Alpha:      return D3D12_BLEND_INV_SRC1_ALPHA;
            default:
                return D3D12_BLEND_ZERO;
        }
    }

    constexpr D3D12_BLEND D3D12AlphaBlend(VGPUBlendFactor factor, bool alphaBlendFactorSupported)
    {
        switch (factor)
        {
            case VGPUBlendFactor_SourceColor:
                return D3D12_BLEND_SRC_ALPHA;
            case VGPUBlendFactor_OneMinusSourceColor:
                return D3D12_BLEND_INV_SRC_ALPHA;
            case VGPUBlendFactor_DestinationColor:
                return D3D12_BLEND_DEST_ALPHA;
            case VGPUBlendFactor_OneMinusDestinationColor:
                return D3D12_BLEND_INV_DEST_ALPHA;
                //case VGPUBlendFactor_Source1Color:
                //    return D3D12_BLEND_SRC1_ALPHA;
                //case VGPUBlendFactor_OneMinusSource1Color:
                //    return D3D12_BLEND_INV_SRC1_ALPHA;
                    // Other blend factors translate to the same D3D12 enum as the color blend factors.
            default:
                return D3D12Blend(factor, alphaBlendFactorSupported);
        }
    }

    constexpr D3D12_BLEND_OP D3D12BlendOperation(VGPUBlendOperation operation)
    {
        switch (operation)
        {
            case VGPUBlendOperation_Add:                return D3D12_BLEND_OP_ADD;
            case VGPUBlendOperation_Subtract:           return D3D12_BLEND_OP_SUBTRACT;
            case VGPUBlendOperation_ReverseSubtract:    return D3D12_BLEND_OP_REV_SUBTRACT;
            case VGPUBlendOperation_Min:                return D3D12_BLEND_OP_MIN;
            case VGPUBlendOperation_Max:                return D3D12_BLEND_OP_MAX;
            default:                                    return D3D12_BLEND_OP_ADD;
        }
    }

    constexpr UINT8 D3D12RenderTargetWriteMask(VGPUColorWriteMaskFlags writeMask)
    {
        UINT8 result = 0;
        if (writeMask & VGPUColorWriteMask_Red) {
            result |= D3D12_COLOR_WRITE_ENABLE_RED;
        }
        if (writeMask & VGPUColorWriteMask_Green) {
            result |= D3D12_COLOR_WRITE_ENABLE_GREEN;
        }
        if (writeMask & VGPUColorWriteMask_Blue) {
            result |= D3D12_COLOR_WRITE_ENABLE_BLUE;
        }
        if (writeMask & VGPUColorWriteMask_Alpha) {
            result |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
        }

        return result;
    }

    constexpr D3D12_SHADER_VISIBILITY ToD3D12(VGPUShaderStageFlags stage)
    {
        switch (stage)  // NOLINT(clang-diagnostic-switch-enum)
        {
            case VGPUShaderStage_Vertex:
                return D3D12_SHADER_VISIBILITY_VERTEX;
            case VGPUShaderStage_Hull:
                return D3D12_SHADER_VISIBILITY_HULL;
            case VGPUShaderStage_Domain:
                return D3D12_SHADER_VISIBILITY_DOMAIN;
            case VGPUShaderStage_Geometry:
                return D3D12_SHADER_VISIBILITY_GEOMETRY;
            case VGPUShaderStage_Fragment:
                return D3D12_SHADER_VISIBILITY_PIXEL;
            case  VGPUShaderStage_Amplification:
                return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
            case VGPUShaderStage_Mesh:
                return D3D12_SHADER_VISIBILITY_MESH;

            default:
            case VGPUShaderStage_All:
                return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    constexpr D3D12_FILL_MODE ToD3D12(VGPUFillMode mode)
    {
        switch (mode)
        {
            default:
            case VGPUFillMode_Solid:
                return D3D12_FILL_MODE_SOLID;

            case VGPUFillMode_Wireframe:
                return D3D12_FILL_MODE_WIREFRAME;
        }
    }

    constexpr D3D12_CULL_MODE ToD3D12(VGPUCullMode mode)
    {
        switch (mode)
        {
            default:
            case VGPUCullMode_Back:
                return D3D12_CULL_MODE_BACK;

            case VGPUCullMode_None:
                return D3D12_CULL_MODE_NONE;
            case VGPUCullMode_Front:
                return D3D12_CULL_MODE_FRONT;
        }
    }


    D3D12_DEPTH_STENCILOP_DESC ToD3D12StencilOpDesc(const VGPUStencilFaceState& state)
    {
        D3D12_DEPTH_STENCILOP_DESC desc = {};
        desc.StencilFailOp = ToD3D12(state.failOperation);
        desc.StencilDepthFailOp = ToD3D12(state.depthFailOperation);
        desc.StencilPassOp = ToD3D12(state.passOperation);
        desc.StencilFunc = ToD3D12(state.compareFunction);
        return desc;
    }

    constexpr D3D12_QUERY_HEAP_TYPE ToD3D12(VGPUQueryType type)
    {
        switch (type)
        {
            case VGPUQueryType_Occlusion:
            case VGPUQueryType_BinaryOcclusion:
                return D3D12_QUERY_HEAP_TYPE_OCCLUSION;

            case VGPUQueryType_Timestamp:
                return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

            case VGPUQueryType_PipelineStatistics:
                return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;

            default:
                VGPU_UNREACHABLE();
        }
    }

    constexpr D3D12_QUERY_TYPE ToD3D12QueryType(VGPUQueryType type)
    {
        switch (type)
        {
            case VGPUQueryType_Occlusion:
                return D3D12_QUERY_TYPE_OCCLUSION;

            case VGPUQueryType_BinaryOcclusion:
                return D3D12_QUERY_TYPE_BINARY_OCCLUSION;

            case VGPUQueryType_Timestamp:
                return D3D12_QUERY_TYPE_TIMESTAMP;

            case VGPUQueryType_PipelineStatistics:
                return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;

            default:
                VGPU_UNREACHABLE();
        }
    }

    constexpr uint32_t GetQueryResultSize(VGPUQueryType type)
    {
        switch (type)
        {
            case VGPUQueryType_Occlusion:
            case VGPUQueryType_BinaryOcclusion:
            case VGPUQueryType_Timestamp:
                return sizeof(uint64_t);

            case VGPUQueryType_PipelineStatistics:
                return sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);

            default:
                VGPU_UNREACHABLE();
            }
        }
}

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define vgpuCreateDXGIFactory2 CreateDXGIFactory2
#define vgpuD3D12CreateDevice D3D12CreateDevice
#define vgpuD3D12GetDebugInterface D3D12GetDebugInterface
#define vgpuD3D12SerializeVersionedRootSignature D3D12SerializeVersionedRootSignature
#endif


using DescriptorIndex = uint32_t;
using RootParameterIndex = uint32_t;

struct D3D12DescriptorAllocator final
{
    ID3D12Device* device = nullptr;
    std::mutex locker;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    std::vector<ID3D12DescriptorHeap*> heaps;
    uint32_t descriptorSize = 0;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> freelist;

    void Init(ID3D12Device* device_, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerBlock)
    {
        device = device_;

        desc.Type = type;
        desc.NumDescriptors = numDescriptorsPerBlock;
        descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    }

    void Shutdown()
    {
        for (auto heap : heaps)
        {
            heap->Release();
        }
        heaps.clear();
    }

    void BlockAllocate()
    {
        heaps.emplace_back();
        HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heaps.back()));
        VGPU_UNUSED(hr);
        VGPU_ASSERT(SUCCEEDED(hr));

        D3D12_CPU_DESCRIPTOR_HANDLE heap_start = heaps.back()->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < desc.NumDescriptors; ++i)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_start;
            handle.ptr += i * descriptorSize;
            freelist.push_back(handle);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate()
    {
        locker.lock();
        if (freelist.empty())
        {
            BlockAllocate();
        }
        VGPU_ASSERT(!freelist.empty());
        D3D12_CPU_DESCRIPTOR_HANDLE handle = freelist.back();
        freelist.pop_back();
        locker.unlock();
        return handle;
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE index)
    {
        locker.lock();
        freelist.push_back(index);
        locker.unlock();
    }
};

struct D3D12GpuDescriptorHeap final
{
    uint32_t numDescriptors = 0;
    ID3D12DescriptorHeap* handle = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = {};

    // CPU status
    std::atomic<uint64_t> allocationOffset{ 0 };

    // GPU status
    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    uint64_t cachedCompletedValue = 0;

    void SignalGPU(ID3D12CommandQueue* queue)
    {
        // Descriptor heaps' progress is recorded by the GPU:
        fenceValue = allocationOffset.load();
        VHR(queue->Signal(fence, fenceValue));
        cachedCompletedValue = fence->GetCompletedValue();
    }
};

class D3D12Device;

struct D3D12Resource
{
    D3D12Device* renderer = nullptr;
    ID3D12Resource* handle = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES transitioningState = (D3D12_RESOURCE_STATES)-1;
};

struct D3D12Buffer final : public VGPUBufferImpl, public D3D12Resource
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint64_t size = 0;
    VGPUBufferUsageFlags usage = 0;
    uint64_t allocatedSize = 0;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = {};
    void* pMappedData{ nullptr };

    ~D3D12Buffer() override;
    void SetLabel(const char* label) override;

    uint64_t GetSize() const override { return size; }
    VGPUBufferUsageFlags GetUsage() const override { return usage; }
    VGPUDeviceAddress GetGpuAddress() const override { return gpuAddress; }
};

struct D3D12Texture final : public VGPUTextureImpl, public D3D12Resource
{
    VGPUTextureDimension dimension{};
    VGPUTextureFormat format{};
    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

    std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> rtvCache;
    std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> dsvCache;

    ~D3D12Texture() override;
    void SetLabel(const char* label) override;

    VGPUTextureDimension GetDimension() const override { return dimension; }
    VGPUTextureFormat GetFormat() const override { return format; }
};

struct D3D12Sampler final : public VGPUSamplerImpl
{
    D3D12Device* renderer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE handle{};

    ~D3D12Sampler() override;
    void SetLabel(const char* label) override;
};

struct D3D12PipelineLayout final : public VGPUPipelineLayoutImpl
{
    D3D12Device* renderer = nullptr;
    ID3D12RootSignature* handle = nullptr;

    RootParameterIndex pushConstantsBaseIndex = ~0u;

    ~D3D12PipelineLayout() override;
    void SetLabel(const char* label) override;
};

struct D3D12ShaderModule final : public VGPUShaderModuleImpl
{
    D3D12Device* renderer = nullptr;
    void* pByteCode = nullptr;
    D3D12_SHADER_BYTECODE handle = {};

    ~D3D12ShaderModule() override;
    void SetLabel(const char* label) override;
};

struct D3D12Pipeline final : public VGPUPipelineImpl
{
    D3D12Device* renderer = nullptr;
    VGPUPipelineType type = VGPUPipelineType_Render;
    D3D12PipelineLayout* pipelineLayout = nullptr;
    ID3D12PipelineState* handle = nullptr;
    uint32_t numVertexBindings = 0;
    uint32_t strides[D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT] = {};
    D3D_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    ~D3D12Pipeline() override;
    void SetLabel(const char* label) override;
    VGPUPipelineType GetType() const override { return type; }
};

struct D3D12QueryHeap final : public VGPUQueryHeapImpl
{
    D3D12Device* renderer = nullptr;
    VGPUQueryType type;
    uint32_t count;
    ID3D12QueryHeap* handle = nullptr;
    D3D12_QUERY_TYPE d3dQueryType = D3D12_QUERY_TYPE_OCCLUSION;
    uint32_t queryResultSize = 0;

    ~D3D12QueryHeap() override;
    void SetLabel(const char* label) override;

    VGPUQueryType GetType() const override { return type; }
    uint32_t GetCount() const override { return count; }
};

struct D3D12SwapChain final : public VGPUSwapChainImpl
{
    D3D12Device* renderer = nullptr;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window = nullptr;
#else
    IUnknown* window = nullptr;
#endif
    IDXGISwapChain3* handle = nullptr;
    VGPUTextureFormat colorFormat;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t backBufferCount;
    uint32_t syncInterval;
    std::vector<D3D12Texture*> backbufferTextures;

    ~D3D12SwapChain() override;
    void SetLabel(const char* label) override;
    VGPUTextureFormat GetFormat() const override { return colorFormat; }
    uint32_t GetWidth() const override { return width; }
    uint32_t GetHeight() const override { return height; }
};

struct D3D12_UploadContext
{
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Fence* fence = nullptr;

    uint64_t uploadBufferSize = 0;
    ID3D12Resource* uploadBuffer = nullptr;
    D3D12MA::Allocation* uploadBufferAllocation = nullptr;
    void* uploadBufferData = nullptr;

    inline bool IsValid() const { return commandList != nullptr; }
};

static constexpr UINT PIX_EVENT_UNICODE_VERSION = 0;

class D3D12CommandBuffer final : public VGPUCommandBufferImpl
{
public:
    D3D12Device* renderer;
    VGPUCommandQueue queueType;
    bool hasLabel = false;

    ID3D12CommandAllocator* commandAllocators[VGPU_MAX_INFLIGHT_FRAMES];
    ID3D12GraphicsCommandList4* commandList;

    D3D12_RESOURCE_BARRIER resourceBarriers[16];
    UINT numBarriersToFlush;

    bool insideRenderPass = false;
    bool hasRenderPassLabel = false;
    D3D12Pipeline* currentPipeline = nullptr;

    std::vector<D3D12SwapChain*> swapChains;

private:
    D3D12_VERTEX_BUFFER_VIEW vboViews[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};

    D3D12_RENDER_PASS_RENDER_TARGET_DESC RTVs[VGPU_MAX_COLOR_ATTACHMENTS] = {};
    // Due to a API bug, this resolve_subresources array must be kept alive between BeginRenderpass() and EndRenderpass()!
    D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS resolveSubresources[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};

public:
    ~D3D12CommandBuffer() override;
    void Reset();
    void Begin(uint32_t frameIndex, const char* label);

    void FlushResourceBarriers()
    {
        if (numBarriersToFlush > 0)
        {
            commandList->ResourceBarrier(numBarriersToFlush, resourceBarriers);
            numBarriersToFlush = 0;
        }
    }

    void InsertUAVBarrier(D3D12Resource* resource, bool flushImmediate = false)
    {
        VGPU_ASSERT(numBarriersToFlush < 16 && "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& BarrierDesc = resourceBarriers[numBarriersToFlush++];

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        BarrierDesc.UAV.pResource = resource->handle;

        if (flushImmediate)
        {
            FlushResourceBarriers();
        }
    }

    void TransitionResource(D3D12Resource* resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false)
    {
        D3D12_RESOURCE_STATES oldState = resource->state;

        if (queueType == VGPUCommandQueue_Compute)
        {
            VGPU_ASSERT((oldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == oldState);
            VGPU_ASSERT((newState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == newState);
        }

        if (oldState != newState)
        {
            VGPU_ASSERT(numBarriersToFlush < 16 && "Exceeded arbitrary limit on buffered barriers");
            D3D12_RESOURCE_BARRIER& BarrierDesc = resourceBarriers[numBarriersToFlush++];

            BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            BarrierDesc.Transition.pResource = resource->handle;
            BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            BarrierDesc.Transition.StateBefore = oldState;
            BarrierDesc.Transition.StateAfter = newState;

            // Check to see if we already started the transition
            if (newState == resource->transitioningState)
            {
                BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
                resource->transitioningState = (D3D12_RESOURCE_STATES)-1;
            }
            else
            {
                BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            }

            resource->state = newState;
        }
        else if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            InsertUAVBarrier(resource, flushImmediate);
        }

        if (flushImmediate || numBarriersToFlush == 16)
        {
            FlushResourceBarriers();
        }
    }

    void PushDebugGroup(const char* groupLabel) override;
    void PopDebugGroup() override;
    void InsertDebugMarker(const char* debugLabel) override;
    void SetPipeline(VGPUPipeline pipeline) override;
    void SetPushConstants(uint32_t pushConstantIndex, const void* data, uint32_t size) override;

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void DispatchIndirect(VGPUBuffer buffer, uint64_t offset) override;

    VGPUTexture AcquireSwapchainTexture(VGPUSwapChain swapChain) override;

    void BeginRenderPass(const VGPURenderPassDesc* desc) override;
    void EndRenderPass() override;

    void SetViewport(const VGPUViewport* viewport) override;
    void SetViewports(uint32_t count, const VGPUViewport* viewports) override;
    void SetScissorRect(const VGPURect* rects) override;
    void SetScissorRects(uint32_t count, const VGPURect* rects) override;

    void SetVertexBuffer(uint32_t index, VGPUBuffer buffer, uint64_t offset) override;
    void SetIndexBuffer(VGPUBuffer buffer, VGPUIndexType type, uint64_t offset) override;
    void SetStencilReference(uint32_t reference) override;

    void BeginQuery(VGPUQueryHeap heap, uint32_t index) override;
    void EndQuery(VGPUQueryHeap heap, uint32_t index) override;
    void ResolveQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count, VGPUBuffer destinationBuffer, uint64_t destinationOffset) override;
    void ResetQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count) override;

    void PrepareDraw();
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) override;
    void DrawIndirect(VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset) override;
    void DrawIndexedIndirect(VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset) override;
};

struct D3D12Queue final
{
    ID3D12CommandQueue* handle = nullptr;
    ID3D12Fence* fence = nullptr;
    ID3D12Fence* frameFences[VGPU_MAX_INFLIGHT_FRAMES] = {};
    std::vector<ID3D12CommandList*> submitCommandLists;
};

class D3D12Device final : public VGPUDeviceImpl
{
public:
    ~D3D12Device() override;

    void SetLabel(const char* label) override;
    void WaitIdle() override;
    VGPUBackend GetBackendType() const override { return VGPUBackend_D3D12; }
    VGPUBool32 QueryFeatureSupport(VGPUFeature feature) const override;
    void GetAdapterProperties(VGPUAdapterProperties* properties) const override;
    void GetLimits(VGPULimits* limits) const override;
    uint64_t GetTimestampFrequency() const  override { return timestampFrequency; }

    VGPUBuffer CreateBuffer(const VGPUBufferDesc* desc, const void* pInitialData) override;
    VGPUTexture CreateTexture(const VGPUTextureDesc* desc, const VGPUTextureData* pInitialData) override;
    VGPUSampler CreateSampler(const VGPUSamplerDesc* desc) override;

    VGPUBindGroupLayout CreateBindGroupLayout(const VGPUBindGroupLayoutDesc* desc) override;
    VGPUPipelineLayout CreatePipelineLayout(const VGPUPipelineLayoutDesc* desc) override;

    VGPUShaderModule CreateShaderModule(const VGPUShaderModuleDesc* desc) override;

    VGPUPipeline CreateRenderPipeline(const VGPURenderPipelineDesc* desc) override;
    VGPUPipeline CreateComputePipeline(const VGPUComputePipelineDesc* desc) override;
    VGPUPipeline CreateRayTracingPipeline(const VGPURayTracingPipelineDesc* desc) override;

    VGPUQueryHeap CreateQueryHeap(const VGPUQueryHeapDesc* desc) override;

    VGPUSwapChain CreateSwapChain(const VGPUSwapChainDesc* desc) override;
    void UpdateSwapChain(D3D12SwapChain* swapChain);

    VGPUCommandBuffer BeginCommandBuffer(VGPUCommandQueue queueType, const char* label) override;
    uint64_t Submit(VGPUCommandBuffer* commandBuffers, uint32_t count) override;

    uint64_t GetFrameCount() override { return frameCount; }
    uint32_t GetFrameIndex() override { return frameIndex; }

    void* GetNativeObject(VGPUNativeObjectType objectType) const override;

    void DeferDestroy(IUnknown* resource, D3D12MA::Allocation* allocation = nullptr);
    void ProcessDeletionQueue();

    IDXGIFactory6* factory = nullptr;
    bool tearingSupported = false;
    ID3D12Device5* device = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};
    DWORD callbackCookie{};

    DXGI_ADAPTER_DESC1 adapterDesc{};
    std::string driverDescription;
    uint64_t timestampFrequency = 0;

    D3D12MA::Allocator* allocator = nullptr;
    CD3DX12FeatureSupport d3dFeatures;
    D3D12Queue queues[_VGPUCommandQueue_Count] = {};

    /* Command contexts */
    std::mutex cmdBuffersLocker;
    uint32_t cmdBuffersCount{ 0 };
    std::vector<D3D12CommandBuffer*> commandBuffersPool;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;

    std::mutex uploadLocker;
    std::vector<D3D12_UploadContext> uploadFreeList;

    D3D12DescriptorAllocator resourceAllocator;
    D3D12DescriptorAllocator samplerAllocator;
    D3D12DescriptorAllocator rtvAllocator;
    D3D12DescriptorAllocator dsvAllocator;

    D3D12GpuDescriptorHeap resourceDescriptorHeap;
    D3D12GpuDescriptorHeap samplerDescriptorHeap;

    ID3D12CommandSignature* dispatchIndirectCommandSignature = nullptr;
    ID3D12CommandSignature* drawIndirectCommandSignature = nullptr;
    ID3D12CommandSignature* drawIndexedIndirectCommandSignature = nullptr;
    ID3D12CommandSignature* dispatchMeshIndirectCommandSignature = nullptr;

    bool shuttingDown = false;
    std::mutex destroyMutex;
    std::deque<std::pair<D3D12MA::Allocation*, uint64_t>> deferredAllocations;
    std::deque<std::pair<IUnknown*, uint64_t>> deferredReleases;
};

void* D3D12Device::GetNativeObject(VGPUNativeObjectType objectType) const
{
    switch (objectType)
    {
        case VGPU_NATIVE_D3D12Device:
            return device;
        default:
            return 0;
    }
}

void D3D12Device::DeferDestroy(IUnknown* resource, D3D12MA::Allocation* allocation)
{
    if (resource == nullptr)
    {
        return;
    }

    destroyMutex.lock();

    if (shuttingDown || device == nullptr)
    {
        resource->Release();
        SAFE_RELEASE(allocation);

        destroyMutex.unlock();
        return;
    }

    deferredReleases.push_back(std::make_pair(resource, frameCount));
    if (allocation != nullptr)
    {
        deferredAllocations.push_back(std::make_pair(allocation, frameCount));
    }
    destroyMutex.unlock();
}

void D3D12Device::ProcessDeletionQueue()
{
    destroyMutex.lock();

    while (!deferredAllocations.empty())
    {
        if (deferredAllocations.front().second + VGPU_MAX_INFLIGHT_FRAMES < frameCount)
        {
            auto item = deferredAllocations.front();
            deferredAllocations.pop_front();
            item.first->Release();
        }
        else
        {
            break;
        }
    }

    while (!deferredReleases.empty())
    {
        if (deferredReleases.front().second + VGPU_MAX_INFLIGHT_FRAMES < frameCount)
        {
            auto item = deferredReleases.front();
            deferredReleases.pop_front();
            item.first->Release();
        }
        else
        {
            break;
        }
    }

    destroyMutex.unlock();
}

static D3D12_UploadContext d3d12_AllocateUpload(D3D12Device* renderer, uint64_t size)
{
    D3D12_UploadContext context;

    renderer->uploadLocker.lock();
    // Try to search for a staging buffer that can fit the request:
    for (size_t i = 0; i < renderer->uploadFreeList.size(); ++i)
    {
        if (renderer->uploadFreeList[i].uploadBuffer != nullptr &&
            renderer->uploadFreeList[i].uploadBufferSize >= size)
        {
            if (renderer->uploadFreeList[i].fence->GetCompletedValue() == 1)
            {
                VHR(renderer->uploadFreeList[i].fence->Signal(0));
                context = std::move(renderer->uploadFreeList[i]);
                std::swap(renderer->uploadFreeList[i], renderer->uploadFreeList.back());
                renderer->uploadFreeList.pop_back();
                break;
            }
        }
    }
    renderer->uploadLocker.unlock();

    // If no buffer was found that fits the data, create one:
    if (!context.IsValid())
    {
        VHR(renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&context.commandAllocator)));
        VHR(renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, context.commandAllocator, nullptr, IID_PPV_ARGS(&context.commandList)));
        VHR(context.commandList->Close());
        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context.fence)));

        context.uploadBufferSize = vgpuNextPowerOfTwo(size);

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC resourceDesc;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = context.uploadBufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        const HRESULT hr = renderer->allocator->CreateResource(
            &allocationDesc,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            &context.uploadBufferAllocation,
            IID_PPV_ARGS(&context.uploadBuffer)
        );
        VHR(hr);

        D3D12_RANGE readRange = {};
        VHR(context.uploadBuffer->Map(0, &readRange, &context.uploadBufferData));
    }

    // Begin command list in valid state
    VHR(context.commandAllocator->Reset());
    VHR(context.commandList->Reset(context.commandAllocator, nullptr));

    return context;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_GetRTV(D3D12Device* renderer, D3D12Texture* texture, uint32_t mipLevel, uint32_t slice)
{
    size_t hash = 0;
    //hash_combine(hash, format);
    hash_combine(hash, mipLevel);
    hash_combine(hash, slice);

    auto it = texture->rtvCache.find(hash);
    if (it == texture->rtvCache.end())
    {
        const D3D12_RESOURCE_DESC& resourceDesc = texture->handle->GetDesc();

        D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
        viewDesc.Format = texture->dxgiFormat;

        switch (resourceDesc.Dimension)
        {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                    viewDesc.Texture1DArray.MipSlice = mipLevel;
                    viewDesc.Texture1DArray.FirstArraySlice = slice;
                    viewDesc.Texture1DArray.ArraySize = 1;
                }
                else
                {
                    viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1D.MipSlice = mipLevel;
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        viewDesc.Texture2DMSArray.FirstArraySlice = slice;
                        viewDesc.Texture2DMSArray.ArraySize = 1;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        viewDesc.Texture2DArray.MipSlice = mipLevel;
                        viewDesc.Texture2DArray.FirstArraySlice = slice;
                        viewDesc.Texture2DArray.ArraySize = 1;
                    }
                }
                else
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2D.MipSlice = mipLevel;
                    }
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            {
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipSlice = mipLevel;
                viewDesc.Texture3D.FirstWSlice = slice;
                viewDesc.Texture3D.WSize = static_cast<UINT>(-1);
                break;
            }
            break;

            default:
                vgpuLogError("D3D12: Invalid texture dimension");
                return {};
        }

        D3D12_CPU_DESCRIPTOR_HANDLE newView = renderer->rtvAllocator.Allocate();
        renderer->device->CreateRenderTargetView(texture->handle, &viewDesc, newView);
        texture->rtvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_GetDSV(D3D12Device* renderer, D3D12Texture* texture, uint32_t mipLevel, uint32_t slice)
{
    size_t hash = 0;
    hash_combine(hash, mipLevel);
    hash_combine(hash, slice);
    //hash_combine(hash, format);

    auto it = texture->dsvCache.find(hash);
    if (it == texture->dsvCache.end())
    {
        const D3D12_RESOURCE_DESC& resourceDesc = texture->handle->GetDesc();

        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
        viewDesc.Format = texture->dxgiFormat;

        switch (resourceDesc.Dimension)
        {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                    viewDesc.Texture1DArray.MipSlice = mipLevel;
                    viewDesc.Texture1DArray.FirstArraySlice = slice;
                    viewDesc.Texture1DArray.ArraySize = 1;
                }
                else
                {
                    viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1D.MipSlice = mipLevel;
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                        viewDesc.Texture2DMSArray.FirstArraySlice = slice;
                        viewDesc.Texture2DMSArray.ArraySize = 1;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                        viewDesc.Texture2DArray.MipSlice = mipLevel;
                        viewDesc.Texture2DArray.FirstArraySlice = slice;
                        viewDesc.Texture2DArray.ArraySize = 1;
                    }
                }
                else
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2D.MipSlice = mipLevel;
                    }
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
                vgpuLogError("D3D12: Cannot create 3D texture DSV");
                return {};

            default:
                vgpuLogError("D3D12: Invalid texture dimension");
                return {};
        }

        D3D12_CPU_DESCRIPTOR_HANDLE newView = renderer->dsvAllocator.Allocate();
        renderer->device->CreateDepthStencilView(texture->handle, &viewDesc, newView);
        texture->dsvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static void d3d12_UploadSubmit(D3D12Device* renderer, D3D12_UploadContext context)
{
    VHR(context.commandList->Close());

    ID3D12CommandList* commandlists[] = {
        context.commandList
    };
    renderer->queues[VGPUCommandQueue_Copy].handle->ExecuteCommandLists(1, commandlists);
    VHR(renderer->queues[VGPUCommandQueue_Copy].handle->Signal(context.fence, 1));

    VHR(renderer->queues[VGPUCommandQueue_Graphics].handle->Wait(context.fence, 1));
    VHR(renderer->queues[VGPUCommandQueue_Compute].handle->Wait(context.fence, 1));

    renderer->uploadLocker.lock();
    renderer->uploadFreeList.push_back(context);
    renderer->uploadLocker.unlock();
}

/* D3D12Buffer */
D3D12Buffer::~D3D12Buffer()
{
    renderer->DeferDestroy(handle, allocation);
}

void D3D12Buffer::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

/* D3D12Texture */
D3D12Texture::~D3D12Texture()
{
    renderer->DeferDestroy(handle, allocation);
    for (auto& it : rtvCache)
    {
        renderer->rtvAllocator.Free(it.second);
    }
    rtvCache.clear();
    for (auto& it : dsvCache)
    {
        renderer->dsvAllocator.Free(it.second);
    }
    dsvCache.clear();
}


void D3D12Texture::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

/* D3D12Sampler */
D3D12Sampler::~D3D12Sampler()
{
    renderer->samplerAllocator.Free(handle);
}

void D3D12Sampler::SetLabel(const char*/* label*/)
{
}

/* D3D12Device */
D3D12Device::~D3D12Device()
{
    // Wait idle
    WaitIdle();
    shuttingDown = true;

    frameCount = UINT64_MAX;
    ProcessDeletionQueue();
    frameCount = 0;

    // Destroy command buffers first
    for (size_t i = 0; i < commandBuffersPool.size(); ++i)
    {
        D3D12CommandBuffer* commandBuffer = commandBuffersPool[i];
        delete commandBuffer;
    }
    commandBuffersPool.clear();

    // Upload/Copy allocations
    {
        for (auto& item : uploadFreeList)
        {
            item.uploadBuffer->Release();
            item.uploadBufferAllocation->Release();
            item.commandAllocator->Release();
            item.commandList->Release();
            item.fence->Release();
        }
        uploadFreeList.clear();
    }

    // CPU descriptor allocators
    {
        resourceAllocator.Shutdown();
        samplerAllocator.Shutdown();
        rtvAllocator.Shutdown();
        dsvAllocator.Shutdown();

        // GPU Heaps
        SAFE_RELEASE(resourceDescriptorHeap.handle);
        SAFE_RELEASE(resourceDescriptorHeap.fence);
        SAFE_RELEASE(samplerDescriptorHeap.handle);
        SAFE_RELEASE(samplerDescriptorHeap.fence);
    }

    SAFE_RELEASE(dispatchIndirectCommandSignature);
    SAFE_RELEASE(drawIndirectCommandSignature);
    SAFE_RELEASE(drawIndexedIndirectCommandSignature);
    SAFE_RELEASE(dispatchMeshIndirectCommandSignature);

    for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
    {
        SAFE_RELEASE(queues[queue].handle);
        SAFE_RELEASE(queues[queue].fence);

        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            SAFE_RELEASE(queues[queue].frameFences[i]);
        }
    }

    // Allocator.
    if (allocator != nullptr)
    {
        D3D12MA::TotalStatistics stats;
        allocator->CalculateStatistics(&stats);
        if (stats.Total.Stats.AllocationBytes > 0)
        {
            //vgpu_log_warn("Total device memory leaked: {} bytes.", stats.Total.UsedBytes);
        }

        SAFE_RELEASE(allocator);
    }

    if (device)
    {
        const ULONG deviceRefCount = device->Release();

#if defined(_DEBUG)
        if (deviceRefCount)
        {
            //DebugString("There are %d unreleased references left on the D3D device!", ref_count);

            ID3D12DebugDevice* debugDevice = nullptr;
            if (SUCCEEDED(device->QueryInterface(&debugDevice)))
            {
                debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
                debugDevice->Release();
            }

        }
#else
        (void)deviceRefCount; // avoid warning
#endif
        device = nullptr;
    }

    SAFE_RELEASE(factory);

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    {
        IDXGIDebug1* dxgiDebug = nullptr;
        if (vgpuDXGIGetDebugInterface1 != nullptr
            && SUCCEEDED(vgpuDXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(VGFX_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            dxgiDebug->Release();
        }
    }
#endif
}

void D3D12Device::UpdateSwapChain(D3D12SwapChain* swapChain)
{
    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    hr = swapChain->handle->GetDesc1(&swapChainDesc);
    VGPU_ASSERT(SUCCEEDED(hr));

    swapChain->width = swapChainDesc.Width;
    swapChain->height = swapChainDesc.Height;
    swapChain->backbufferTextures.resize(swapChainDesc.BufferCount);
    for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        D3D12Texture* texture = new D3D12Texture();
        texture->renderer = this;
        texture->dimension = VGPUTextureDimension_2D;
        texture->format = swapChain->colorFormat;
        texture->state = D3D12_RESOURCE_STATE_PRESENT;
        texture->width = swapChainDesc.Width;
        texture->height = swapChainDesc.Height;
        texture->dxgiFormat = ToDxgiFormat(swapChain->colorFormat);
        hr = swapChain->handle->GetBuffer(i, IID_PPV_ARGS(&texture->handle));
        VGPU_ASSERT(SUCCEEDED(hr));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", i);
        texture->handle->SetName(name);
        swapChain->backbufferTextures[i] = texture;
    }
}

void D3D12Device::WaitIdle()
{
    ComPtr<ID3D12Fence> fence;
    VHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // Wait for the GPU to fully catch up with the CPU
    for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
    {
        VHR(queues[queue].handle->Signal(fence.Get(), 1));

        if (fence->GetCompletedValue() < 1)
        {
            VHR(fence->SetEventOnCompletion(1, NULL));
        }

        VHR(fence->Signal(0));
    }

    // Safe delete deferred destroys
    ProcessDeletionQueue();
}

VGPUBool32 D3D12Device::QueryFeatureSupport(VGPUFeature feature) const
{
    switch (feature)
    {
        case VGPUFeature_DepthClipControl:
        case VGPUFeature_Depth32FloatStencil8:
        case VGPUFeature_TimestampQuery:
        case VGPUFeature_PipelineStatisticsQuery:
        case VGPUFeature_TextureCompressionBC:
        case VGPUFeature_IndirectFirstInstance:
        case VGPUFeature_GeometryShader:
        case VGPUFeature_TessellationShader:
        case VGPUFeature_DescriptorIndexing:
        case VGPUFeature_Predication:
            return true;

        case VGPUFeature_TextureCompressionETC2:
        case VGPUFeature_TextureCompressionASTC:
            return false;

        case VGPUFeature_ShaderFloat16:
            //const bool supportsDP4a = d3dFeatures.HighestShaderModel() >= D3D_SHADER_MODEL_6_4;
            return d3dFeatures.HighestShaderModel() >= D3D_SHADER_MODEL_6_2 && d3dFeatures.Native16BitShaderOpsSupported();

        case VGPUFeature_CacheCoherentUMA:
            return (d3dFeatures.CacheCoherentUMA() == TRUE);

        case VGPUFeature_DepthBoundsTest:
            return (d3dFeatures.DepthBoundsTestSupported() == TRUE);

            // https://docs.microsoft.com/en-us/windows/win32/direct3d11/tiled-resources-texture-sampling-features
        case VGPUFeature_SamplerMinMax:
            if (d3dFeatures.TiledResourcesTier() >= D3D12_TILED_RESOURCES_TIER_2)
            {
                // Tier 2 for tiled resources
                // https://learn.microsoft.com/en-us/windows/win32/direct3d11/tiled-resources-texture-sampling-features
            }

            return (d3dFeatures.MaxSupportedFeatureLevel() >= D3D_FEATURE_LEVEL_11_1);

        case VGPUFeature_ShaderOutputViewportIndex:
            return (d3dFeatures.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation() == TRUE);

        case VGPUFeature_VariableRateShading:
            return (d3dFeatures.VariableShadingRateTier() >= D3D12_VARIABLE_SHADING_RATE_TIER_1);

        case VGPUFeature_VariableRateShadingTier2:
            return (d3dFeatures.VariableShadingRateTier() >= D3D12_VARIABLE_SHADING_RATE_TIER_2);

        case VGPUFeature_RayTracing:
            return (d3dFeatures.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_0);

        case VGPUFeature_RayTracingTier2:
            return (d3dFeatures.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_1);

        case VGPUFeature_MeshShader:
            return (d3dFeatures.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1);;

        default:
            return false;
    }
}

void D3D12Device::GetAdapterProperties(VGPUAdapterProperties* properties) const
{
    std::string adapterName = WCharToUTF8(adapterDesc.Description);

    properties->vendorId = adapterDesc.VendorId;
    properties->deviceId = adapterDesc.DeviceId;
    strncpy(properties->name, adapterName.c_str(), _VGPU_MIN(VGPU_ADAPTER_NAME_MAX_LENGTH, 128));
    properties->driverDescription = driverDescription.c_str();

    if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
    {
        properties->type = VGPUAdapterType_CPU;
    }
    else
    {
        properties->type = (d3dFeatures.UMA() == TRUE) ? VGPUAdapterType_IntegratedGPU : VGPUAdapterType_DiscreteGPU;
    }
}

void D3D12Device::GetLimits(VGPULimits* limits) const
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS featureData = {};
    HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
    if (FAILED(hr))
    {
    }

    limits->maxTextureDimension1D = D3D12_REQ_TEXTURE1D_U_DIMENSION;
    limits->maxTextureDimension2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    limits->maxTextureDimension3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    limits->maxTextureDimensionCube = D3D12_REQ_TEXTURECUBE_DIMENSION;
    limits->maxTextureArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    limits->maxConstantBufferBindingSize = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    // D3D12 has no documented limit on the size of a storage buffer binding.
    limits->maxStorageBufferBindingSize = 4294967295;
    limits->minUniformBufferOffsetAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    limits->minStorageBufferOffsetAlignment = 32;
    limits->maxVertexBuffers = 16;
    limits->maxVertexAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    limits->maxVertexBufferArrayStride = 2048u;

    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-compute-shaders
    // Thread Group Shared Memory is limited to 16Kb on downlevel hardware. This is less than
    // the 32Kb that is available to Direct3D 11 hardware. D3D12 is also 32kb.
    limits->maxComputeWorkgroupStorageSize = 32768;

    // https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numthreads
    limits->maxComputeInvocationsPerWorkGroup = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
    limits->maxComputeWorkGroupSizeX = D3D12_CS_THREAD_GROUP_MAX_X;
    limits->maxComputeWorkGroupSizeY = D3D12_CS_THREAD_GROUP_MAX_X;
    limits->maxComputeWorkGroupSizeZ = D3D12_CS_THREAD_GROUP_MAX_X;

    // https://docs.maxComputeWorkgroupSizeXmicrosoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_dispatch_arguments
    limits->maxComputeWorkGroupsPerDimension = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;

    limits->maxViewports = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    limits->maxViewportDimensions[0] = D3D12_VIEWPORT_BOUNDS_MAX;
    limits->maxViewportDimensions[1] = D3D12_VIEWPORT_BOUNDS_MAX;
    limits->maxColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;

    if (d3dFeatures.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_0)
    {
        limits->rayTracingShaderGroupIdentifierSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
        limits->rayTracingShaderTableAligment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        limits->rayTracingShaderTableMaxStride = std::numeric_limits<uint64_t>::max();
        limits->rayTracingShaderRecursionMaxDepth = D3D12_RAYTRACING_MAX_DECLARABLE_TRACE_RECURSION_DEPTH;
        limits->rayTracingMaxGeometryCount = (1 << 24) - 1;
    }

}

void D3D12Device::SetLabel(const char* label)
{
    D3D12SetName(device, label);
}

/* Buffer */
VGPUBuffer D3D12Device::CreateBuffer(const VGPUBufferDesc* desc, const void* pInitialData)
{
    if (desc->handle)
    {
        D3D12Buffer* buffer = new D3D12Buffer();
        buffer->renderer = this;
        buffer->handle = (ID3D12Resource*)desc->handle;
        buffer->handle->AddRef();
        buffer->allocation = nullptr;
        buffer->state = D3D12_RESOURCE_STATE_COMMON;
        buffer->size = desc->size;
        buffer->usage = desc->usage;
        buffer->allocatedSize = 0u;

        if (desc->label)
        {
            D3D12SetName(buffer->handle, desc->label);
        }

        buffer->gpuAddress = buffer->handle->GetGPUVirtualAddress();

        return (VGPUBuffer)buffer;
    }

    UINT64 alignedSize = desc->size;
    if (desc->usage & VGPUBufferUsage_Constant)
    {
        alignedSize = AlignUp<UINT64>(alignedSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (desc->usage & VGPUBufferUsage_ShaderWrite)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (!(desc->usage & VGPUBufferUsage_ShaderRead) &&
        !(desc->usage & VGPUBufferUsage_RayTracing))
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    if (desc->cpuAccess == VGPUCpuAccessMode_Read)
    {
        allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
        resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    else if (desc->cpuAccess == VGPUCpuAccessMode_Write)
    {
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

    D3D12Buffer* buffer = new D3D12Buffer();
    buffer->renderer = this;
    buffer->state = resourceState;
    buffer->size = desc->size;
    buffer->usage = desc->usage;

    device->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &buffer->footprint, nullptr, nullptr, &buffer->allocatedSize);

    HRESULT hr = allocator->CreateResource(
        &allocationDesc,
        &resourceDesc,
        resourceState,
        nullptr,
        &buffer->allocation,
        IID_PPV_ARGS(&buffer->handle)
    );

    if (FAILED(hr))
    {
        vgpuLogError("D3D12: Failed to create buffer");
        delete buffer;
        return nullptr;
    }

    if (desc->label)
    {
        buffer->SetLabel(desc->label);
    }

    buffer->gpuAddress = buffer->handle->GetGPUVirtualAddress();

    if (desc->cpuAccess == VGPUCpuAccessMode_Read)
    {
        buffer->handle->Map(0, nullptr, &buffer->pMappedData);
    }
    else if (desc->cpuAccess == VGPUCpuAccessMode_Write)
    {
        D3D12_RANGE readRange = {};
        buffer->handle->Map(0, &readRange, &buffer->pMappedData);
    }

    // Issue data copy.
    if (pInitialData != nullptr)
    {
        if (desc->cpuAccess == VGPUCpuAccessMode_Write)
        {
            memcpy(buffer->pMappedData, pInitialData, desc->size);
        }
        else
        {
            auto context = d3d12_AllocateUpload(this, desc->size);
            memcpy(context.uploadBufferData, pInitialData, desc->size);

            context.commandList->CopyBufferRegion(
                buffer->handle,
                0,
                context.uploadBuffer,
                0,
                desc->size
            );

            d3d12_UploadSubmit(this, context);
        }
    }

    if (desc->usage & VGPUBufferUsage_ShaderRead)
    {
        // Create Raw Buffer
        const uint64_t offset = 0;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = offset / sizeof(uint32_t);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.NumElements = (UINT)(desc->size / sizeof(uint32_t));

        D3D12_CPU_DESCRIPTOR_HANDLE handle = resourceAllocator.Allocate();
        device->CreateShaderResourceView(buffer->handle, &srvDesc, handle);

        // bindless allocation.
        //const uint32_t bindlessSRV = AllocateBindlessResource(handle);
        //buffer->SetBindlessSRV(bindlessSRV);
    }

    if (desc->usage & VGPUBufferUsage_ShaderWrite)
    {
        // Create Raw Buffer
        const uint64_t offset = 0;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = offset / sizeof(uint32_t);
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Buffer.NumElements = (UINT)(desc->size / sizeof(uint32_t));

        D3D12_CPU_DESCRIPTOR_HANDLE handle = resourceAllocator.Allocate();
        device->CreateUnorderedAccessView(buffer->handle, nullptr, &uavDesc, handle);

        // bindless allocation.
        //const uint32_t bindlessUAV = AllocateBindlessResource(handle);
        //buffer->SetBindlessUAV(bindlessUAV);
    }

    return buffer;
}

/* Texture */
VGPUTexture D3D12Device::CreateTexture(const VGPUTextureDesc* desc, const VGPUTextureData* pInitialData)
{
    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc;
    if (desc->dimension == VGPUTextureDimension_1D)
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }
    else if (desc->dimension == VGPUTextureDimension_3D)
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }
    resourceDesc.Alignment = 0;
    resourceDesc.Width = desc->width;
    resourceDesc.Height = desc->height;
    resourceDesc.DepthOrArraySize = (UINT16)desc->depthOrArrayLayers;
    resourceDesc.MipLevels = (UINT16)desc->mipLevelCount;
    resourceDesc.Format = ToDxgiFormat(desc->format);
    resourceDesc.SampleDesc.Count = desc->sampleCount;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

    if (pInitialData == nullptr)
    {
        if (desc->usage & VGPUTextureUsage_RenderTarget)
        {
            if (vgpuIsDepthStencilFormat(desc->format))
            {
                resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            }
            else
            {
                resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            }
        }
        if (desc->usage & VGPUTextureUsage_ShaderRead)
        {
            resourceState |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }

        if (desc->usage & VGPUTextureUsage_ShaderWrite)
        {
            resourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
    }

    if (desc->usage & VGPUTextureUsage_ShaderWrite)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        if (vgpuIsDepthStencilFormat(desc->format))
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            if (!(desc->usage & VGPUTextureUsage_ShaderRead))
            {
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
        }
        else
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
    }

    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        clearValue.Format = resourceDesc.Format;
        if (vgpuIsDepthStencilFormat(desc->format))
        {
            clearValue.DepthStencil.Depth = 1.0f;
        }
        pClearValue = &clearValue;
    }

    // If shader read/write and depth format, set to typeless
    if (vgpuIsDepthFormat(desc->format) && desc->usage & (VGPUTextureUsage_ShaderRead | VGPUTextureUsage_ShaderWrite))
    {
        resourceDesc.Format = GetTypelessFormatFromDepthFormat(desc->format);
        pClearValue = nullptr;
    }

    // TODO: TextureLayout/State
    D3D12Texture* texture = new D3D12Texture();
    texture->renderer = this;
    texture->dimension = desc->dimension;
    texture->format = desc->format;
    texture->state = resourceState;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->dxgiFormat = resourceDesc.Format;

    HRESULT hr = allocator->CreateResource(
        &allocationDesc,
        &resourceDesc,
        texture->state,
        pClearValue,
        &texture->allocation,
        IID_PPV_ARGS(&texture->handle)
    );

    if (FAILED(hr))
    {
        vgpuLogError("D3D12: Failed to create texture");
        delete texture;
        return nullptr;
    }

    if (desc->label)
    {
        D3D12SetName(texture->handle, desc->label);
    }

    return texture;
}

/* Sampler */
VGPUSampler D3D12Device::CreateSampler(const VGPUSamplerDesc* desc)
{
    const D3D12_FILTER_REDUCTION_TYPE d3dReductionType = desc->compareFunction != VGPUCompareFunction_Never ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    const D3D12_FILTER_TYPE d3dMinFilter = ToD3D12FilterType(desc->minFilter);
    const D3D12_FILTER_TYPE d3dMagFilter = ToD3D12FilterType(desc->magFilter);
    const D3D12_FILTER_TYPE d3dMipFilter = ToD3D12FilterType(desc->mipFilter);

    D3D12_SAMPLER_DESC samplerDesc{};

    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_sampler_desc
    if (desc->maxAnisotropy > 1)
    {
        samplerDesc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(d3dReductionType);
    }
    else
    {
        samplerDesc.Filter = D3D12_ENCODE_BASIC_FILTER(d3dMinFilter, d3dMagFilter, d3dMipFilter, d3dReductionType);
    }

    samplerDesc.AddressU = ToD3D12AddressMode(desc->addressU);
    samplerDesc.AddressV = ToD3D12AddressMode(desc->addressV);
    samplerDesc.AddressW = ToD3D12AddressMode(desc->addressW);
    samplerDesc.MipLODBias = desc->mipLodBias;
    samplerDesc.MaxAnisotropy = _VGPU_MIN(desc->maxAnisotropy, 16u);
    samplerDesc.ComparisonFunc = ToD3D12(desc->compareFunction);
    switch (desc->borderColor)
    {
        case VGPUSamplerBorderColor_OpaqueBlack:
            samplerDesc.BorderColor[0] = 0.0f;
            samplerDesc.BorderColor[1] = 0.0f;
            samplerDesc.BorderColor[2] = 0.0f;
            samplerDesc.BorderColor[3] = 1.0f;
            break;

        case VGPUSamplerBorderColor_OpaqueWhite:
            samplerDesc.BorderColor[0] = 1.0f;
            samplerDesc.BorderColor[1] = 1.0f;
            samplerDesc.BorderColor[2] = 1.0f;
            samplerDesc.BorderColor[3] = 1.0f;
            break;
        default:
            samplerDesc.BorderColor[0] = 0.0f;
            samplerDesc.BorderColor[1] = 0.0f;
            samplerDesc.BorderColor[2] = 0.0f;
            samplerDesc.BorderColor[3] = 0.0f;
            break;
    }

    samplerDesc.MinLOD = desc->lodMinClamp;
    samplerDesc.MaxLOD = desc->lodMaxClamp;

    D3D12Sampler* sampler = new D3D12Sampler();
    sampler->renderer = this;
    sampler->handle = samplerAllocator.Allocate();
    device->CreateSampler(&samplerDesc, sampler->handle);
    //sampler->bindlessIndex = AllocateBindlessSampler(sampler->handle);

    return sampler;
}

/* BindGroupLayout */
VGPUBindGroupLayout D3D12Device::CreateBindGroupLayout(const VGPUBindGroupLayoutDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

/* PipelineLayout */
D3D12PipelineLayout::~D3D12PipelineLayout()
{
    renderer->DeferDestroy(handle, nullptr);
}

void D3D12PipelineLayout::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

static HRESULT d3d12_CreateRootSignature(ID3D12Device* device, ID3D12RootSignature** rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = { };
    versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versionedDesc.Desc_1_1 = desc;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = vgpuD3D12SerializeVersionedRootSignature(&versionedDesc, &signature, &error);
    if (FAILED(hr))
    {
        const char* errString = error ? reinterpret_cast<const char*>(error->GetBufferPointer()) : "";

        vgpuLogError("Failed to create root signature: %S", errString);
    }

    return device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature));
}

VGPUPipelineLayout D3D12Device::CreatePipelineLayout(const VGPUPipelineLayoutDesc* desc)
{
    D3D12PipelineLayout* layout = new D3D12PipelineLayout();
    layout->renderer = this;

    uint32_t rangeMax = 0;
    for (uint32_t i = 0; i < desc->descriptorSetCount; i++)
    {
        rangeMax += desc->descriptorSets[i].rangeCount;
    }

    uint32_t totalRangeNum = 0;
    std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
    std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges(rangeMax);
    std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

    if (desc->pushConstantRangeCount > 0)
    {
        layout->pushConstantsBaseIndex = RootParameterIndex(rootParameters.size());

        for (uint32_t i = 0; i < desc->pushConstantRangeCount; i++)
        {
            const VGPUPushConstantRange& pushConstantRange = desc->pushConstantRanges[i];

            D3D12_ROOT_PARAMETER1 rootParameter = {};
            rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rootParameter.ShaderVisibility = ToD3D12(pushConstantRange.visibility);
            rootParameter.Constants.ShaderRegister = pushConstantRange.shaderRegister;
            rootParameter.Constants.RegisterSpace = 0;
            rootParameter.Constants.Num32BitValues = pushConstantRange.size / 4;

            rootParameters.push_back(rootParameter);
        }
    }
    VGPU_UNUSED(totalRangeNum);

    D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = { };
    rootSignatureDesc.NumParameters = (UINT)rootParameters.size();
    rootSignatureDesc.pParameters = rootParameters.data();
    rootSignatureDesc.NumStaticSamplers = (UINT)staticSamplers.size();
    rootSignatureDesc.pStaticSamplers = staticSamplers.data();
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

#ifdef USING_D3D12_AGILITY_SDK
    rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
#endif

    const HRESULT hr = d3d12_CreateRootSignature(device, &layout->handle, rootSignatureDesc);
    if (FAILED(hr))
    {
        delete layout;
        return nullptr;
    }

    return layout;
}

/* ShaderModule */
D3D12ShaderModule::~D3D12ShaderModule()
{
    ::free(pByteCode);
    handle = {};
    pByteCode = nullptr;
}

void D3D12ShaderModule::SetLabel(const char* label)
{
    VGPU_UNUSED(label);
}

VGPUShaderModule D3D12Device::CreateShaderModule(const VGPUShaderModuleDesc* desc)
{
    D3D12ShaderModule* shaderModule = new D3D12ShaderModule();
    shaderModule->renderer = this;
    shaderModule->pByteCode = malloc(desc->codeSize);
    memcpy(shaderModule->pByteCode, desc->pCode, desc->codeSize);
    shaderModule->handle.pShaderBytecode = shaderModule->pByteCode;
    shaderModule->handle.BytecodeLength = desc->codeSize;
    return shaderModule;
}

/* Pipeline */
D3D12Pipeline::~D3D12Pipeline()
{
    pipelineLayout->Release();
    renderer->DeferDestroy(handle, nullptr);
}

void D3D12Pipeline::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

VGPUPipeline D3D12Device::CreateRenderPipeline(const VGPURenderPipelineDesc* desc)
{
    D3D12Pipeline* pipeline = new D3D12Pipeline();
    pipeline->renderer = this;
    pipeline->type = VGPUPipelineType_Render;
    pipeline->pipelineLayout = (D3D12PipelineLayout*)desc->layout;
    pipeline->pipelineLayout->AddRef();

    // PipelineStream
    struct PSO_STREAM
    {
        struct PSO_STREAM1
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE    IBStripCutValue;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
            CD3DX12_PIPELINE_STATE_STREAM_HS                    HS;
            CD3DX12_PIPELINE_STATE_STREAM_DS                    DS;
            CD3DX12_PIPELINE_STATE_STREAM_GS                    GS;
            CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
            CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendState;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1        DepthStencilState;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            RasterizerState;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
            CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
            CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK           SampleMask;
        } stream1 = {};

        struct PSO_STREAM2
        {
            CD3DX12_PIPELINE_STATE_STREAM_AS AS;
            CD3DX12_PIPELINE_STATE_STREAM_MS MS;
        } stream2 = {};
    } stream = {};

    stream.stream1.pRootSignature = pipeline->pipelineLayout->handle;

    // InputLayout
    UINT NumElements = 0;
    D3D12_INPUT_ELEMENT_DESC inputElements[VGPU_MAX_VERTEX_ATTRIBUTES] = {};

    for (uint32_t binding = 0; binding < desc->vertex.layoutCount; ++binding)
    {
        const VGPUVertexBufferLayout& layout = desc->vertex.layouts[binding];

        for (uint32_t attributeIndex = 0; attributeIndex < layout.attributeCount; ++attributeIndex)
        {
            const VGPUVertexAttribute& attribute = layout.attributes[attributeIndex];

            D3D12_INPUT_ELEMENT_DESC& inputElement = inputElements[NumElements++];
            inputElement.SemanticName = "ATTRIBUTE";
            inputElement.SemanticIndex = attribute.shaderLocation;
            inputElement.Format = ToDxgiFormat(attribute.format);
            inputElement.InputSlot = binding;
            inputElement.AlignedByteOffset = attribute.offset;

            pipeline->numVertexBindings = _VGPU_MAX(binding + 1, pipeline->numVertexBindings);
            pipeline->strides[binding] = layout.stride;

            if (layout.stepMode == VGPUVertexStepMode_Vertex)
            {
                inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                inputElement.InstanceDataStepRate = 0;
            }
            else
            {
                inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                inputElement.InstanceDataStepRate = 1;
            }
        }
    }

    D3D12_INPUT_LAYOUT_DESC inputLayout = {};
    inputLayout.pInputElementDescs = inputElements;
    inputLayout.NumElements = NumElements;
    stream.stream1.InputLayout = inputLayout;

    // Handle index strip
    if (desc->primitiveTopology != VGPUPrimitiveTopology_TriangleStrip &&
        desc->primitiveTopology != VGPUPrimitiveTopology_LineStrip)
    {
        stream.stream1.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    }
    else
    {
        stream.stream1.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
    }

    for (uint32_t i = 0; i < desc->shaderStageCount; i++)
    {
        const VGPUShaderStageDesc& shader = desc->shaderStages[i];
        const D3D12_SHADER_BYTECODE& bytecode = static_cast<D3D12ShaderModule*>(desc->shaderStages[i].module)->handle;

        if (shader.stage == VGPUShaderStage_Vertex)
        {
            stream.stream1.VS = bytecode;
        }
        else if (shader.stage == VGPUShaderStage_Hull)
        {
            stream.stream1.HS = bytecode;
        }
        else if (shader.stage == VGPUShaderStage_Domain)
        {
            stream.stream1.DS = bytecode;
        }
        else if (shader.stage == VGPUShaderStage_Geometry)
        {
            stream.stream1.GS = bytecode;
        }
        else if (shader.stage == VGPUShaderStage_Fragment)
        {
            stream.stream1.PS = bytecode;
        }
        else if (shader.stage == VGPUShaderStage_Amplification)
        {
            stream.stream2.AS = bytecode;
        }
        else if (shader.stage == VGPUShaderStage_Mesh)
        {
            stream.stream2.MS = bytecode;
        }
    }

    // Color Attachments + RTV
    const bool alphaBlendFactorSupported = d3dFeatures.AlphaBlendFactorSupported();
    D3D12_RT_FORMAT_ARRAY RTVFormats = {};
    RTVFormats.NumRenderTargets = 0;

    CD3DX12_BLEND_DESC blendState = {};
    blendState.AlphaToCoverageEnable = desc->blendState.alphaToCoverageEnable ? TRUE : FALSE;
    blendState.IndependentBlendEnable = desc->blendState.independentBlendEnable ? TRUE : FALSE;
    for (uint32_t i = 0; i < desc->colorFormatCount; ++i)
    {
        VGPU_ASSERT(desc->colorFormats[i] != VGPUTextureFormat_Undefined);

        const VGPURenderTargetBlendState& attachment = desc->blendState.renderTargets[i];

        blendState.RenderTarget[i].BlendEnable = attachment.blendEnabled;
        blendState.RenderTarget[i].LogicOpEnable = FALSE;
        blendState.RenderTarget[i].SrcBlend = D3D12Blend(attachment.srcColorBlendFactor, alphaBlendFactorSupported);
        blendState.RenderTarget[i].DestBlend = D3D12Blend(attachment.dstColorBlendFactor, alphaBlendFactorSupported);
        blendState.RenderTarget[i].BlendOp = D3D12BlendOperation(attachment.colorBlendOperation);
        blendState.RenderTarget[i].SrcBlendAlpha = D3D12AlphaBlend(attachment.srcAlphaBlendFactor, alphaBlendFactorSupported);
        blendState.RenderTarget[i].DestBlendAlpha = D3D12AlphaBlend(attachment.dstAlphaBlendFactor, alphaBlendFactorSupported);
        blendState.RenderTarget[i].BlendOpAlpha = D3D12BlendOperation(attachment.alphaBlendOperation);
        blendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        blendState.RenderTarget[i].RenderTargetWriteMask = D3D12RenderTargetWriteMask(attachment.colorWriteMask);

        // RTV
        RTVFormats.RTFormats[RTVFormats.NumRenderTargets] = ToDxgiFormat(desc->colorFormats[i]);
        RTVFormats.NumRenderTargets++;
    }
    stream.stream1.RTVFormats = RTVFormats;
    stream.stream1.BlendState = blendState;

    // RasterizerState
    CD3DX12_RASTERIZER_DESC rasterizerState{};
    rasterizerState.FillMode = ToD3D12(desc->rasterizerState.fillMode);
    rasterizerState.CullMode = ToD3D12(desc->rasterizerState.cullMode);
    rasterizerState.FrontCounterClockwise = desc->rasterizerState.frontFaceCounterClockwise ? TRUE : FALSE;
    rasterizerState.DepthBias = static_cast<INT>(desc->rasterizerState.depthBias);
    rasterizerState.DepthBiasClamp = desc->rasterizerState.depthBiasClamp;
    rasterizerState.SlopeScaledDepthBias = desc->rasterizerState.slopeScaledDepthBias;
    rasterizerState.DepthClipEnable = (desc->rasterizerState.depthClipMode == VGPUDepthClipMode_Clip) ? TRUE : FALSE;
    rasterizerState.MultisampleEnable = desc->sampleCount > 1 ? TRUE : FALSE;
    rasterizerState.AntialiasedLineEnable = FALSE;
    rasterizerState.ForcedSampleCount = 0;
    rasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    stream.stream1.RasterizerState = rasterizerState;

    // DepthStencilState
    CD3DX12_DEPTH_STENCIL_DESC1 depthStencilState{};
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    if (desc->depthStencilFormat != VGPUTextureFormat_Undefined)
    {
        depthStencilState.DepthEnable = desc->depthStencilState.depthCompareFunction != VGPUCompareFunction_Always || desc->depthStencilState.depthWriteEnabled;
        depthStencilState.DepthWriteMask = desc->depthStencilState.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilState.DepthFunc = ToD3D12(desc->depthStencilState.depthCompareFunction);
        depthStencilState.StencilEnable = vgpuStencilTestEnabled(&desc->depthStencilState);
        depthStencilState.StencilReadMask = static_cast<UINT8>(desc->depthStencilState.stencilReadMask);
        depthStencilState.StencilWriteMask = static_cast<UINT8>(desc->depthStencilState.stencilWriteMask);
        depthStencilState.FrontFace = ToD3D12StencilOpDesc(desc->depthStencilState.stencilFront);
        depthStencilState.BackFace = ToD3D12StencilOpDesc(desc->depthStencilState.stencilBack);

        if (d3dFeatures.DepthBoundsTestSupported() == TRUE)
        {
            depthStencilState.DepthBoundsTestEnable = desc->depthStencilState.depthBoundsTestEnable ? TRUE : FALSE;
        }
        else
        {
            depthStencilState.DepthBoundsTestEnable = FALSE;
        }
    }
    else
    {
        depthStencilState.DepthEnable = FALSE;
        depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        depthStencilState.StencilEnable = FALSE;
        depthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        depthStencilState.FrontFace = defaultStencilOp;
        depthStencilState.BackFace = defaultStencilOp;
        depthStencilState.DepthBoundsTestEnable = FALSE;
    }
    stream.stream1.DepthStencilState = depthStencilState;
    stream.stream1.DSVFormat = ToDxgiFormat(desc->depthStencilFormat);

    switch (desc->primitiveTopology)
    {
        case VGPUPrimitiveTopology_PointList:
            stream.stream1.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            break;
        case VGPUPrimitiveTopology_LineList:
        case VGPUPrimitiveTopology_LineStrip:
            stream.stream1.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            break;
        case VGPUPrimitiveTopology_TriangleList:
        case VGPUPrimitiveTopology_TriangleStrip:
            stream.stream1.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            break;
        case VGPUPrimitiveTopology_PatchList:
            stream.stream1.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            break;
    }
    pipeline->primitiveTopology = ToD3DPrimitiveTopology(desc->primitiveTopology, desc->patchControlPoints);

    // SampleDesc and SampleMask
    DXGI_SAMPLE_DESC sampleDesc = {};
    sampleDesc.Count = desc->sampleCount;
    sampleDesc.Quality = 0;
    stream.stream1.SampleDesc = sampleDesc;
    stream.stream1.SampleMask = UINT_MAX;

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
    streamDesc.pPipelineStateSubobjectStream = &stream;
    streamDesc.SizeInBytes = sizeof(stream.stream1);
    if (QueryFeatureSupport(VGPUFeature_MeshShader))
    {
        streamDesc.SizeInBytes += sizeof(stream.stream2);
    }

    if (FAILED(device->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pipeline->handle))))
    {
        delete pipeline;
        return nullptr;
    }

    if (desc->label)
    {
        pipeline->SetLabel(desc->label);
    }

    return pipeline;
}

VGPUPipeline D3D12Device::CreateComputePipeline(const VGPUComputePipelineDesc* desc)
{
    D3D12Pipeline* pipeline = new D3D12Pipeline();
    pipeline->renderer = this;
    pipeline->type = VGPUPipelineType_Compute;
    pipeline->pipelineLayout = (D3D12PipelineLayout*)desc->layout;
    pipeline->pipelineLayout->AddRef();

    struct PSO_STREAM
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } stream;

    stream.pRootSignature = pipeline->pipelineLayout->handle;
    stream.CS = static_cast<D3D12ShaderModule*>(desc->computeShader.module)->handle;

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
    streamDesc.pPipelineStateSubobjectStream = &stream;
    streamDesc.SizeInBytes = sizeof(stream);

    if (FAILED(device->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pipeline->handle))))
    {
        delete pipeline;
        return nullptr;
    }

    if (desc->label)
    {
        pipeline->SetLabel(desc->label);
    }

    return pipeline;
}

VGPUPipeline D3D12Device::CreateRayTracingPipeline(const VGPURayTracingPipelineDesc* desc)
{
    VGPU_UNUSED(desc);

    D3D12Pipeline* pipeline = new D3D12Pipeline();
    pipeline->renderer = this;
    pipeline->type = VGPUPipelineType_RayTracing;
    pipeline->pipelineLayout = (D3D12PipelineLayout*)desc->layout;
    pipeline->pipelineLayout->AddRef();
    return pipeline;
}


/*D3D12QueryHeap */
D3D12QueryHeap::~D3D12QueryHeap()
{
    renderer->DeferDestroy(handle, nullptr);
}

void D3D12QueryHeap::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

VGPUQueryHeap D3D12Device::CreateQueryHeap(const VGPUQueryHeapDesc* desc)
{
    D3D12_QUERY_HEAP_DESC d3dDesc = {};
    d3dDesc.Type = ToD3D12(desc->type);
    d3dDesc.Count = desc->count;
    d3dDesc.NodeMask = 0;

    ID3D12QueryHeap* handle = nullptr;
    HRESULT hr = device->CreateQueryHeap(&d3dDesc, IID_PPV_ARGS(&handle));
    if (FAILED(hr))
    {
        return nullptr;
    }

    D3D12QueryHeap* heap = new D3D12QueryHeap();
    heap->renderer = this;
    heap->type = desc->type;
    heap->count = desc->count;
    heap->d3dQueryType = ToD3D12QueryType(desc->type);
    heap->handle = handle;
    heap->queryResultSize = GetQueryResultSize(desc->type);

    if (desc->label)
    {
        heap->SetLabel(desc->label);
    }

    return heap;
}

/* SwapChain */
D3D12SwapChain::~D3D12SwapChain()
{
    for (size_t i = 0, count = backbufferTextures.size(); i < count; ++i)
    {
        backbufferTextures[i]->Release();
    }
    backbufferTextures.clear();
    handle->Release();
}

void D3D12SwapChain::SetLabel(const char* label)
{
    VGPU_UNUSED(label);
}

VGPUSwapChain D3D12Device::CreateSwapChain(const VGPUSwapChainDesc* desc)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = desc->width;
    swapChainDesc.Height = desc->height;
    swapChainDesc.Format = ToDxgiFormat(ToDXGISwapChainFormat(desc->format));
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = PresentModeToBufferCount(desc->presentMode);
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

    IDXGISwapChain1* tempSwapChain = nullptr;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window = (HWND)desc->windowHandle;
    VGPU_ASSERT(IsWindow(window));

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = !desc->isFullscreen;

    // Create a swap chain for the window.
    HRESULT hr = factory->CreateSwapChainForHwnd(
        queues[VGPUCommandQueue_Graphics].handle,
        window,
        &swapChainDesc,
        &fsSwapChainDesc,
        nullptr,
        &tempSwapChain
    );

    if (FAILED(hr))
    {
        return nullptr;
    }

    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
    hr = factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
#else
    swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;

    IUnknown* window = static_cast<IUnknown*>(windowHandle);

    HRESULT hr = factory->CreateSwapChainForCoreWindow(
        queues[VGPUCommandQueue_Graphics].handle,
        window,
        &swapChainDesc,
        nullptr,
        &tempSwapChain
    );

    // SwapChain panel
    //ComPtr<ISwapChainPanelNative> swapChainPanelNative;
    //swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    //swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
    //hr = factory->CreateSwapChainForComposition(
    //    graphicsQueue,
    //    &swapChainDesc,
    //    nullptr,
    //    tempSwapChain.GetAddressOf()
    //);
    //
    //hr = tempSwapChain.As(&swapChainPanelNative);
    //if (FAILED(hr))
    //{
    //    vgpuLogError("Failed to get ISwapChainPanelNative from IDXGISwapChain1");
    //    return nullptr;
    //}
    //
    //hr = swapChainPanelNative->SetSwapChain(tempSwapChain.Get());
    //if (FAILED(hr))
    //{
    //    vgpuLogError("Failed to set ISwapChainPanelNative - SwapChain");
    //    return nullptr;
    //}
#endif
    if (FAILED(hr))
    {
        return nullptr;
    }

    IDXGISwapChain3* handle = nullptr;
    hr = tempSwapChain->QueryInterface(&handle);
    if (FAILED(hr))
    {
        return nullptr;
    }
    SAFE_RELEASE(tempSwapChain);

    D3D12SwapChain* swapChain = new D3D12SwapChain();
    swapChain->renderer = this;
    swapChain->window = window;
    swapChain->handle = handle;
    swapChain->colorFormat = desc->format;
    swapChain->backBufferCount = swapChainDesc.BufferCount;
    swapChain->syncInterval = PresentModeToSwapInterval(desc->presentMode);
    UpdateSwapChain(swapChain);
    return swapChain;
}

/* CommandBuffer */
D3D12CommandBuffer::~D3D12CommandBuffer()
{
    Reset();

    for (uint32_t frameIndex = 0; frameIndex < VGPU_MAX_INFLIGHT_FRAMES; ++frameIndex)
    {
        SAFE_RELEASE(commandAllocators[frameIndex]);
    }

    SAFE_RELEASE(commandList);
}
void D3D12CommandBuffer::Reset()
{
    hasLabel = false;
    hasRenderPassLabel = false;
    insideRenderPass = false;
    numBarriersToFlush = 0;

    if (currentPipeline)
    {
        currentPipeline->Release();
        currentPipeline = nullptr;
    }
}

void D3D12CommandBuffer::Begin(uint32_t frameIndex, const char* label)
{
    Reset();

    VHR(commandAllocators[frameIndex]->Reset());
    VHR(commandList->Reset(commandAllocators[frameIndex], nullptr));

    if (queueType == VGPUCommandQueue_Graphics ||
        queueType == VGPUCommandQueue_Compute)
    {
        ID3D12DescriptorHeap* heaps[2] = {
            renderer->resourceDescriptorHeap.handle,
            renderer->samplerDescriptorHeap.handle
        };
        commandList->SetDescriptorHeaps(2u, heaps);
    }

    if (queueType == VGPUCommandQueue_Graphics)
    {
        for (uint32_t i = 0; i < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
        {
            vboViews[i] = {};
        }

        D3D12_RECT pRects[D3D12_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1];
        for (uint32_t i = 0; i < _countof(pRects); ++i)
        {
            pRects[i].bottom = D3D12_VIEWPORT_BOUNDS_MAX;
            pRects[i].left = D3D12_VIEWPORT_BOUNDS_MIN;
            pRects[i].right = D3D12_VIEWPORT_BOUNDS_MAX;
            pRects[i].top = D3D12_VIEWPORT_BOUNDS_MIN;
        }
        commandList->RSSetScissorRects(_countof(pRects), pRects);

        static constexpr float defaultBlendFactor[4] = { 0, 0, 0, 0 };
        commandList->OMSetBlendFactor(defaultBlendFactor);
        commandList->OMSetStencilRef(0);
    }

    if (label)
    {
        PushDebugGroup(label);
        hasLabel = true;
    }
}

void D3D12CommandBuffer::PushDebugGroup(const char* groupLabel)
{
    std::wstring wide_name = UTF8ToWStr(groupLabel);
    UINT size = static_cast<UINT>((strlen(groupLabel) + 1) * sizeof(wchar_t));
    commandList->BeginEvent(PIX_EVENT_UNICODE_VERSION, wide_name.c_str(), size);
}

void D3D12CommandBuffer::PopDebugGroup()
{
    commandList->EndEvent();
}

void D3D12CommandBuffer::InsertDebugMarker(const char* markerLabel)
{
    std::wstring wide_name = UTF8ToWStr(markerLabel);
    UINT size = static_cast<UINT>((strlen(markerLabel) + 1) * sizeof(wchar_t));
    commandList->SetMarker(PIX_EVENT_UNICODE_VERSION, wide_name.c_str(), size);
}

void D3D12CommandBuffer::SetPipeline(VGPUPipeline pipeline)
{
    D3D12Pipeline* newPipeline = (D3D12Pipeline*)pipeline;

    if (currentPipeline == newPipeline)
        return;

    currentPipeline = newPipeline;
    currentPipeline->AddRef();

    commandList->SetPipelineState(newPipeline->handle);
    if (newPipeline->type == VGPUPipelineType_Render)
    {
        commandList->IASetPrimitiveTopology(newPipeline->primitiveTopology);
        commandList->SetGraphicsRootSignature(newPipeline->pipelineLayout->handle);
    }
    else
    {
        commandList->SetGraphicsRootSignature(newPipeline->pipelineLayout->handle);
    }
}

void D3D12CommandBuffer::SetPushConstants(uint32_t pushConstantIndex, const void* data, uint32_t size)
{
    VGPU_ASSERT(currentPipeline);
    //VGPU_ASSERT(size <= device->limits.pushConstantsMaxSize);
    VGPU_ASSERT(size % 4 == 0);

    const uint32_t rootParameterIndex = currentPipeline->pipelineLayout->pushConstantsBaseIndex + pushConstantIndex;
    const uint32_t num32BitValuesToSet = size / 4;

    if (currentPipeline->type == VGPUPipelineType_Render)
    {
        commandList->SetGraphicsRoot32BitConstants(
            rootParameterIndex,
            num32BitValuesToSet,
            data,
            0
        );
    }
    else
    {
        commandList->SetComputeRoot32BitConstants(
            rootParameterIndex,
            num32BitValuesToSet,
            data,
            0
        );
    }
}

void D3D12CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    VGPU_VERIFY(!insideRenderPass);

    commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void D3D12CommandBuffer::DispatchIndirect(VGPUBuffer buffer, uint64_t offset)
{
    VGPU_VERIFY(!insideRenderPass);
    D3D12Resource* d3dBuffer = (D3D12Resource*)buffer;

    commandList->ExecuteIndirect(renderer->dispatchIndirectCommandSignature,
        1,
        d3dBuffer->handle, offset,
        nullptr,
        0);
}

VGPUTexture D3D12CommandBuffer::AcquireSwapchainTexture(VGPUSwapChain swapChain)
{
    D3D12SwapChain* d3d12SwapChain = (D3D12SwapChain*)swapChain;

    HRESULT hr = S_OK;

    /* Check for window size changes and resize the swapchain if needed. */
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    d3d12SwapChain->handle->GetDesc1(&swapChainDesc);

    uint32_t width = 0;
    uint32_t height = 0;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    RECT windowRect;
    GetClientRect(d3d12SwapChain->window, &windowRect);
    width = static_cast<uint32_t>(windowRect.right - windowRect.left);
    height = static_cast<uint32_t>(windowRect.bottom - windowRect.top);
#else

#endif

    // Check if window is minimized
    if (width == 0 ||
        height == 0)
    {
        return nullptr;
    }

    if (width != swapChainDesc.Width ||
        height != swapChainDesc.Height)
    {
        renderer->WaitIdle();

        // Release resources that are tied to the swap chain and update fence values.
        for (size_t i = 0, count = d3d12SwapChain->backbufferTextures.size(); i < count; ++i)
        {
            delete d3d12SwapChain->backbufferTextures[i];
        }
        d3d12SwapChain->backbufferTextures.clear();

        hr = d3d12SwapChain->handle->ResizeBuffers(
            d3d12SwapChain->backBufferCount,
            width,
            height,
            DXGI_FORMAT_UNKNOWN, /* Keep the old format */
            (renderer->tearingSupported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
        );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? renderer->device->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            //HandleDeviceLost();

            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
            // and correctly set up the new device.
            return nullptr;
        }
        else
        {
            if (FAILED(hr))
            {
                vgpuLogError("Could not resize swapchain");
                return nullptr;
            }

            renderer->UpdateSwapChain(d3d12SwapChain);
        }
    }

    D3D12Texture* swapChainTexture =
        d3d12SwapChain->backbufferTextures[d3d12SwapChain->handle->GetCurrentBackBufferIndex()];

    // Transition to RenderTarget state
    TransitionResource(swapChainTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    swapChains.push_back(d3d12SwapChain);
    return swapChainTexture;
}

void D3D12CommandBuffer::BeginRenderPass(const VGPURenderPassDesc* desc)
{
    uint32_t width = UINT32_MAX;
    uint32_t height = UINT32_MAX;
    uint32_t numRTVS = 0;
    D3D12_RENDER_PASS_FLAGS renderPassFlags = D3D12_RENDER_PASS_FLAG_NONE;
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC DSV = {};

    if (desc->label)
    {
        PushDebugGroup(desc->label);
        hasRenderPassLabel = true;
    }

    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
    {
        const VGPURenderPassColorAttachment* attachment = &desc->colorAttachments[i];
        D3D12Texture* texture = (D3D12Texture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        RTVs[i].cpuDescriptor = d3d12_GetRTV(renderer, texture, level, slice);

        // Transition to RenderTarget
        TransitionResource(texture, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

        RTVs[numRTVS].BeginningAccess.Type = ToD3D12(attachment->loadAction);
        if (attachment->loadAction == VGPULoadAction_Clear)
        {
            RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
            RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[0] = attachment->clearColor.r;
            RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[1] = attachment->clearColor.g;
            RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[2] = attachment->clearColor.b;
            RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[3] = attachment->clearColor.a;
        }

        // TODO: Resolve
        RTVs[numRTVS].EndingAccess.Type = ToD3D12(attachment->storeAction);

        width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
        height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

        numRTVS++;
    }

    const bool hasDepthStencil = desc->depthStencilAttachment != nullptr;
    if (hasDepthStencil)
    {
        const VGPURenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
        D3D12Texture* texture = (D3D12Texture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
        height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

        DSV.cpuDescriptor = d3d12_GetDSV(renderer, texture, level, slice);

        DSV.DepthBeginningAccess.Type = ToD3D12(attachment->depthLoadAction);
        if (attachment->depthLoadAction == VGPULoadAction_Clear)
        {
            DSV.DepthBeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
            DSV.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = attachment->depthClearValue;
        }
        DSV.DepthEndingAccess.Type = ToD3D12(attachment->depthStoreAction);

        DSV.StencilBeginningAccess.Type = ToD3D12(attachment->stencilLoadAction);
        if (attachment->stencilLoadAction == VGPULoadAction_Clear)
        {
            DSV.StencilBeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
            DSV.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = static_cast<UINT8>(attachment->stencilClearValue);
        }
        DSV.StencilEndingAccess.Type = ToD3D12(attachment->stencilStoreAction);
    }

    commandList->BeginRenderPass(numRTVS, RTVs, hasDepthStencil ? &DSV : nullptr, renderPassFlags);

    // Set the viewport.
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, LONG(width), LONG(height) };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    insideRenderPass = true;
}

void D3D12CommandBuffer::EndRenderPass()
{
    commandList->EndRenderPass();

    if (hasRenderPassLabel)
    {
        PopDebugGroup();
    }

    insideRenderPass = false;
}

void D3D12CommandBuffer::SetViewport(const VGPUViewport* viewport)
{
    commandList->RSSetViewports(1, (const D3D12_VIEWPORT*)viewport);
}

void D3D12CommandBuffer::SetViewports(uint32_t count, const VGPUViewport* viewports)
{
    VGPU_ASSERT(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    commandList->RSSetViewports(count, (const D3D12_VIEWPORT*)viewports);
}

void D3D12CommandBuffer::SetScissorRect(const VGPURect* rect)
{
    D3D12_RECT d3d_rect = {};
    d3d_rect.left = LONG(rect->x);
    d3d_rect.top = LONG(rect->y);
    d3d_rect.right = LONG(rect->x + rect->width);
    d3d_rect.bottom = LONG(rect->y + rect->height);
    commandList->RSSetScissorRects(1u, &d3d_rect);
}

void D3D12CommandBuffer::SetScissorRects(uint32_t count, const VGPURect* rects)
{
    VGPU_ASSERT(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

    D3D12_RECT d3dScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    for (uint32_t i = 0; i < count; i++)
    {
        d3dScissorRects[i].left = LONG(rects[i].x);
        d3dScissorRects[i].top = LONG(rects[i].y);
        d3dScissorRects[i].right = LONG(rects[i].x + rects[i].width);
        d3dScissorRects[i].bottom = LONG(rects[i].y + rects[i].height);
    }
    commandList->RSSetScissorRects(count, d3dScissorRects);
}

void D3D12CommandBuffer::SetVertexBuffer(uint32_t index, VGPUBuffer buffer, uint64_t offset)
{
    D3D12Buffer* d3d12Buffer = (D3D12Buffer*)buffer;

    vboViews[index].BufferLocation = d3d12Buffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
    vboViews[index].SizeInBytes = (UINT)(d3d12Buffer->size - offset);
    vboViews[index].StrideInBytes = 0;
}

void D3D12CommandBuffer::SetIndexBuffer(VGPUBuffer buffer, VGPUIndexType type, uint64_t offset)
{
    D3D12Buffer* d3d12Buffer = (D3D12Buffer*)buffer;

    D3D12_INDEX_BUFFER_VIEW view{};
    view.BufferLocation = d3d12Buffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
    view.SizeInBytes = (UINT)(d3d12Buffer->size - offset);
    view.Format = (type == VGPUIndexType_Uint16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
    commandList->IASetIndexBuffer(&view);
}

void D3D12CommandBuffer::SetStencilReference(uint32_t reference)
{
    commandList->OMSetStencilRef(reference);
}

void D3D12CommandBuffer::BeginQuery(VGPUQueryHeap heap, uint32_t index)
{
    D3D12QueryHeap* d3dHeap = static_cast<D3D12QueryHeap*>(heap);

    commandList->BeginQuery(d3dHeap->handle, d3dHeap->d3dQueryType, index);
}

void D3D12CommandBuffer::EndQuery(VGPUQueryHeap heap, uint32_t index)
{
    D3D12QueryHeap* d3dHeap = static_cast<D3D12QueryHeap*>(heap);

    commandList->EndQuery(d3dHeap->handle, d3dHeap->d3dQueryType, index);
}

void D3D12CommandBuffer::ResolveQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count, VGPUBuffer destinationBuffer, uint64_t destinationOffset)
{
    D3D12QueryHeap* d3dHeap = static_cast<D3D12QueryHeap*>(heap);
    D3D12Buffer* d3dDestBuffer = static_cast<D3D12Buffer*>(destinationBuffer);

    commandList->ResolveQueryData(
        d3dHeap->handle,
        d3dHeap->d3dQueryType,
        index,
        count,
        d3dDestBuffer->handle,
        destinationOffset
    );
}

void D3D12CommandBuffer::ResetQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count)
{
    VGPU_UNUSED(heap);
    VGPU_UNUSED(index);
    VGPU_UNUSED(count);
}

void D3D12CommandBuffer::PrepareDraw()
{
    VGPU_VERIFY(insideRenderPass);

    if (currentPipeline->numVertexBindings > 0)
    {
        for (uint32_t i = 0; i < currentPipeline->numVertexBindings; ++i)
        {
            vboViews[i].StrideInBytes = currentPipeline->strides[i];
        }

        commandList->IASetVertexBuffers(0, currentPipeline->numVertexBindings, vboViews);
    }
}

void D3D12CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    PrepareDraw();
    commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void D3D12CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    PrepareDraw();
    commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void D3D12CommandBuffer::DrawIndirect(VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset)
{
    VGPU_ASSERT(indirectBuffer);
    PrepareDraw();

    D3D12Buffer* backendBuffer = static_cast<D3D12Buffer*>(indirectBuffer);
    commandList->ExecuteIndirect(
        renderer->drawIndirectCommandSignature,
        1,
        backendBuffer->handle,
        indirectBufferOffset,
        nullptr,
        0);
}

void D3D12CommandBuffer::DrawIndexedIndirect(VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset)
{
    VGPU_ASSERT(indirectBuffer);
    PrepareDraw();

    D3D12Buffer* backendBuffer = static_cast<D3D12Buffer*>(indirectBuffer);
    commandList->ExecuteIndirect(
        renderer->drawIndexedIndirectCommandSignature,
        1,
        backendBuffer->handle,
        indirectBufferOffset,
        nullptr,
        0);
}

VGPUCommandBuffer D3D12Device::BeginCommandBuffer(VGPUCommandQueue queueType, const char* label)
{
    HRESULT hr = S_OK;
    D3D12CommandBuffer* commandBuffer = nullptr;

    cmdBuffersLocker.lock();
    uint32_t cmd_current = cmdBuffersCount++;
    if (cmd_current >= commandBuffersPool.size())
    {
        D3D12_COMMAND_LIST_TYPE d3dCommandListType = ToD3D12(queueType);

        commandBuffer = new D3D12CommandBuffer();
        commandBuffer->renderer = this;
        commandBuffer->queueType = queueType;

        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            VHR(device->CreateCommandAllocator(d3dCommandListType, IID_PPV_ARGS(&commandBuffer->commandAllocators[i])));
        }

        hr = device->CreateCommandList1(0, d3dCommandListType, D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&commandBuffer->commandList)
        );
        VHR(hr);

        commandBuffersPool.push_back(commandBuffer);
    }
    else
    {
        commandBuffer = commandBuffersPool.back();
    }

    cmdBuffersLocker.unlock();

    // Start the command list in a default state.
    commandBuffer->Begin(frameIndex, label);

    return commandBuffersPool.back();
}

uint64_t D3D12Device::Submit(VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    HRESULT hr = S_OK;
    std::vector<D3D12SwapChain*> presentSwapChains;
    for (uint32_t i = 0; i < count; i += 1)
    {
        D3D12CommandBuffer* commandBuffer = static_cast<D3D12CommandBuffer*>(commandBuffers[i]);

        // Present acquired SwapChains
        for (size_t swapChainIndex = 0; swapChainIndex < commandBuffer->swapChains.size(); ++swapChainIndex)
        {
            D3D12SwapChain* swapChain = commandBuffer->swapChains[swapChainIndex];

            /* Transition SwapChain textures to present */
            D3D12Resource* texture = (D3D12Resource*)swapChain->backbufferTextures[swapChain->handle->GetCurrentBackBufferIndex()];

            commandBuffer->TransitionResource(texture, D3D12_RESOURCE_STATE_PRESENT);

            presentSwapChains.push_back(swapChain);
        }
        commandBuffer->swapChains.clear();

        // Push debug group label -> if any
        if (commandBuffer->hasLabel)
        {
            commandBuffer->PopDebugGroup();
        }

        // Flush any pending barriers 
        commandBuffer->FlushResourceBarriers();

        hr = commandBuffer->commandList->Close();
        if (FAILED(hr))
        {
            vgpuLogError("Failed to close command list");
            return 0;
        }

        D3D12Queue& queue = queues[commandBuffer->queueType];
        queue.submitCommandLists.push_back(commandBuffer->commandList);
    }

    for (uint32_t i = 0; i < _VGPUCommandQueue_Count; ++i)
    {
        D3D12Queue& queue = queues[i];

        if (!queue.submitCommandLists.empty())
        {
            queue.handle->ExecuteCommandLists(
                (UINT)queue.submitCommandLists.size(),
                queue.submitCommandLists.data()
            );
            queue.submitCommandLists.clear();
        }

        VHR(queue.handle->Signal(queue.frameFences[frameIndex], 1));
    }

    cmdBuffersCount = 0;

    // Present acquired SwapChains
    for (size_t i = 0; i < presentSwapChains.size() && SUCCEEDED(hr); ++i)
    {
        D3D12SwapChain* swapChain = presentSwapChains[i];

        UINT presentFlags = 0;
        BOOL fullscreen = FALSE;
        swapChain->handle->GetFullscreenState(&fullscreen, nullptr);

        if (!swapChain->syncInterval && !fullscreen)
        {
            presentFlags = DXGI_PRESENT_ALLOW_TEARING;
        }

        hr = swapChain->handle->Present(swapChain->syncInterval, presentFlags);

        // If the device was reset we must completely reinitialize the renderer.
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? device->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            //HandleDeviceLost();
            return 0;
        }
    }

    resourceDescriptorHeap.SignalGPU(queues[VGPUCommandQueue_Graphics].handle);
    samplerDescriptorHeap.SignalGPU(queues[VGPUCommandQueue_Graphics].handle);

    // Begin new frame
    frameCount++;
    frameIndex = frameCount % VGPU_MAX_INFLIGHT_FRAMES;

    for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
    {
        if (frameCount >= VGPU_MAX_INFLIGHT_FRAMES &&
            queues[queue].frameFences[frameIndex]->GetCompletedValue() < 1)
        {
            // NULL event handle will simply wait immediately:
            // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion#remarks
            hr = queues[queue].frameFences[frameIndex]->SetEventOnCompletion(1, nullptr);
            VHR(hr);
        }
    }

    // Begin new frame
    // Safe delete deferred destroys
    ProcessDeletionQueue();

    // Return current frame
    return frameCount - 1;
}

static VGPUBool32 d3d12_isSupported(void)
{
    static bool available_initialized = false;
    static bool available = false;

    if (available_initialized) {
        return available;
    }

    available_initialized = true;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HMODULE dxgiDLL = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE d3d12DLL = LoadLibraryExW(L"d3d12.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (dxgiDLL == nullptr ||
        d3d12DLL == nullptr)
    {
        return false;
    }

    vgpuCreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgiDLL, "CreateDXGIFactory2");
    if (vgpuCreateDXGIFactory2 == nullptr)
    {
        return false;
    }

#if defined(_DEBUG)
    vgpuDXGIGetDebugInterface1 = (PFN_DXGI_GET_DEBUG_INTERFACE1)GetProcAddress(dxgiDLL, "DXGIGetDebugInterface1");
#endif

    vgpuD3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(d3d12DLL, "D3D12GetDebugInterface");
    vgpuD3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12DLL, "D3D12CreateDevice");
    if (!vgpuD3D12CreateDevice)
    {
        return false;
    }

    vgpuD3D12SerializeVersionedRootSignature = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(d3d12DLL, "D3D12SerializeVersionedRootSignature");
    if (!vgpuD3D12SerializeVersionedRootSignature) {
        return false;
    }
#endif

    ComPtr<IDXGIFactory4> dxgiFactory;
    if (FAILED(vgpuCreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory))))
    {
        return false;
    }

    bool foundCompatibleDevice = true;
    ComPtr<IDXGIAdapter1> dxgiAdapter;
    for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, dxgiAdapter.ReleaseAndGetAddressOf()); ++i)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        // Don't select the Basic Render Driver adapter.
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        // Check to see if the adapter supports Direct3D 12,
        // but don't create the actual device.
        if (SUCCEEDED(vgpuD3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
        {
            foundCompatibleDevice = true;
            break;
        }
    }

    if (foundCompatibleDevice)
    {
        available = true;
        return true;
    }

    return false;
}

inline void __stdcall _D3D12_DebugMessageCallback(
    D3D12_MESSAGE_CATEGORY Category,
    D3D12_MESSAGE_SEVERITY Severity,
    D3D12_MESSAGE_ID ID,
    LPCSTR pDescription,
    void* pContext)
{
    VGPU_UNUSED(Category);
    VGPU_UNUSED(ID);
    VGPU_UNUSED(pContext);

    if (Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION || Severity == D3D12_MESSAGE_SEVERITY_ERROR)
    {
        vgpuLogError("%s", pDescription);
        VGPU_UNREACHABLE();
    }
    else if (Severity == D3D12_MESSAGE_SEVERITY_WARNING)
    {
        vgpuLogWarn("%s", pDescription);
    }
    else
    {
        vgpuLogInfo("%s", pDescription);
    }
}

static VGPUDeviceImpl* d3d12_createDevice(const VGPUDeviceDescriptor* info)
{
    VGPU_ASSERT(info);

    D3D12Device* renderer = new D3D12Device();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGPUValidationMode_Disabled)
    {
        ComPtr<ID3D12Debug> debugController;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        if (vgpuD3D12GetDebugInterface != nullptr &&
            SUCCEEDED(vgpuD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
#else
        if (SUCCEEDED(vgpuD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
#endif
        {
            debugController->EnableDebugLayer();

            if (info->validationMode == VGPUValidationMode_GPU)
            {
                ComPtr<ID3D12Debug1> debugController1;
                if (SUCCEEDED(debugController.As(&debugController1)))
                {
                    debugController1->SetEnableGPUBasedValidation(TRUE);
                    debugController1->SetEnableSynchronizedCommandQueueValidation(TRUE);
                }

                ComPtr<ID3D12Debug2> debugController2;
                if (SUCCEEDED(debugController.As(&debugController2)))
                {
                    debugController2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_NONE);
                }
            }
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (vgpuDXGIGetDebugInterface1 != nullptr &&
            SUCCEEDED(vgpuDXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(VGFX_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(VGFX_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(VGFX_DXGI_DEBUG_DXGI, &filter);
        }
#endif
    }

    if (FAILED(vgpuCreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&renderer->factory))))
    {
        delete renderer;
        return nullptr;
    }

    // Determines whether tearing support is available for fullscreen borderless windows.
    {
        BOOL allowTearing = FALSE;
        HRESULT hr = renderer->factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));

        if (FAILED(hr) || !allowTearing)
        {
            renderer->tearingSupported = false;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
        else
        {
            renderer->tearingSupported = true;
        }
    }

    {
        const DXGI_GPU_PREFERENCE gpuPreference = (info->powerPreference == VGPUPowerPreference_LowPower) ? DXGI_GPU_PREFERENCE_MINIMUM_POWER : DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;

        static constexpr D3D_FEATURE_LEVEL s_featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        ComPtr<IDXGIAdapter1> dxgiAdapter = nullptr;
        for (uint32_t i = 0;
            renderer->factory->EnumAdapterByGpuPreference(i, gpuPreference, IID_PPV_ARGS(dxgiAdapter.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND;
            ++i)
        {
            DXGI_ADAPTER_DESC1 adapterDesc;
            dxgiAdapter->GetDesc1(&adapterDesc);

            // Don't select the Basic Render Driver adapter.
            if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            for (auto& featurelevel : s_featureLevels)
            {
                if (SUCCEEDED(vgpuD3D12CreateDevice(dxgiAdapter.Get(), featurelevel, IID_PPV_ARGS(&renderer->device))))
                {
                    break;
                }
            }

            if (renderer->device != nullptr)
                break;
        }

        VGPU_ASSERT(dxgiAdapter != nullptr);
        if (dxgiAdapter == nullptr)
        {
            vgpuLogError("DXGI: No capable adapter found!");
            delete renderer;
            return nullptr;
        }

        // Init feature check (https://devblogs.microsoft.com/directx/introducing-a-new-api-for-checking-feature-support-in-direct3d-12/)
        VHR(renderer->d3dFeatures.Init(renderer->device));

        if (renderer->d3dFeatures.HighestRootSignatureVersion() < D3D_ROOT_SIGNATURE_VERSION_1_1)
        {
            vgpuLogError("Direct3D12: Root signature version 1.1 not supported!");
            delete renderer;
            return nullptr;
        }

        // Assign label object.
        if (info->label)
        {
            renderer->SetLabel(info->label);
        }

        if (info->validationMode != VGPUValidationMode_Disabled)
        {
            // Configure debug device (if active).
            ID3D12InfoQueue* infoQueue = nullptr;
            if (SUCCEEDED(renderer->device->QueryInterface(&infoQueue)))
            {
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

                std::vector<D3D12_MESSAGE_SEVERITY> enabledSeverities;
                std::vector<D3D12_MESSAGE_ID> disabledMessages;

                // These severities should be seen all the time
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_CORRUPTION);
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_ERROR);
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_WARNING);
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_MESSAGE);

                if (info->validationMode == VGPUValidationMode_Verbose)
                {
                    // Verbose only filters
                    enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_INFO);
                }

                disabledMessages.push_back(D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED);
                disabledMessages.push_back(D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND);
                disabledMessages.push_back(D3D12_MESSAGE_ID_STOREPIPELINE_DUPLICATENAME);

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.AllowList.NumSeverities = static_cast<UINT>(enabledSeverities.size());
                filter.AllowList.pSeverityList = enabledSeverities.data();
                filter.DenyList.NumIDs = static_cast<UINT>(disabledMessages.size());
                filter.DenyList.pIDList = disabledMessages.data();

                // Clear out the existing filters since we're taking full control of them
                infoQueue->PushEmptyStorageFilter();

                infoQueue->AddStorageFilterEntries(&filter);
                infoQueue->Release();
            }

            ID3D12InfoQueue1* infoQueue1 = nullptr;
            if (SUCCEEDED(renderer->device->QueryInterface(&infoQueue1)))
            {
                infoQueue1->RegisterMessageCallback(_D3D12_DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, renderer, &renderer->callbackCookie);
                infoQueue1->Release();
            }
        }

        // Create allocator
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = renderer->device;
        allocatorDesc.pAdapter = dxgiAdapter.Get();
        allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
        if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &renderer->allocator)))
        {
            delete renderer;
            return nullptr;
        }

        // Init adapter info.
        dxgiAdapter->GetDesc1(&renderer->adapterDesc);

        // Convert the adapter's D3D12 driver version to a readable string like "24.21.13.9793".
        LARGE_INTEGER umdVersion;
        if (dxgiAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion) != DXGI_ERROR_UNSUPPORTED)
        {
            uint64_t encodedVersion = umdVersion.QuadPart;
            std::ostringstream o;
            o << "D3D12 driver version ";
            uint16_t driverVersion[4] = {};

            for (size_t i = 0; i < 4; ++i) {
                driverVersion[i] = (encodedVersion >> (48 - 16 * i)) & 0xFFFF;
                o << driverVersion[i] << ".";
            }

            renderer->driverDescription = o.str();
        }
    }

    // Create command queues
    {
        for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
        {
            VGPUCommandQueue queueType = (VGPUCommandQueue)queue;

            D3D12_COMMAND_QUEUE_DESC queueDesc{};

            queueDesc.Type = ToD3D12(queueType);
            queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.NodeMask = 0;
            VHR(
                renderer->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer->queues[queue].handle))
            );
            VHR(
                renderer->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&renderer->queues[queue].fence))
            );

            switch (queueType)
            {
                case VGPUCommandQueue_Graphics:
                    renderer->queues[queue].handle->SetName(L"Graphics Queue");
                    renderer->queues[queue].fence->SetName(L"GraphicsQueue - Fence");
                    break;
                case VGPUCommandQueue_Compute:
                    renderer->queues[queue].handle->SetName(L"Compute Queue");
                    renderer->queues[queue].fence->SetName(L"ComputeQueue - Fence");
                    break;
                case VGPUCommandQueue_Copy:
                    renderer->queues[queue].handle->SetName(L"CopyQueue");
                    renderer->queues[queue].fence->SetName(L"CopyQueue - Fence");
                    break;
            }

            // Create frame-resident resources:
            for (uint32_t frameIndex = 0; frameIndex < VGPU_MAX_INFLIGHT_FRAMES; ++frameIndex)
            {
                VHR(
                    renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer->queues[queue].frameFences[frameIndex]))
                );

#if defined(_DEBUG)
                wchar_t fenceName[64];

                switch (queueType)
                {
                    case VGPUCommandQueue_Graphics:
                        swprintf(fenceName, 64, L"GraphicsQueue - Frame Fence %u", frameIndex);
                        break;
                    case VGPUCommandQueue_Compute:
                        swprintf(fenceName, 64, L"ComputeQueue - Frame Fence %u", frameIndex);
                        break;
                    case VGPUCommandQueue_Copy:
                        swprintf(fenceName, 64, L"CopyQueue - Frame Fence %u", frameIndex);
                        break;
                }

                renderer->queues[queue].frameFences[frameIndex]->SetName(fenceName);
#endif
            }
        }
    }

    // Init CPU descriptor allocators
    renderer->resourceAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);
    renderer->samplerAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256);
    renderer->rtvAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512);
    renderer->dsvAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);

    // Resource descriptor heap (shader visible)
    {
        renderer->resourceDescriptorHeap.numDescriptors = 1000000; // tier 2 limit

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = renderer->resourceDescriptorHeap.numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask = 0;
        VHR(renderer->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&renderer->resourceDescriptorHeap.handle)));
        renderer->resourceDescriptorHeap.cpuStart = renderer->resourceDescriptorHeap.handle->GetCPUDescriptorHandleForHeapStart();
        renderer->resourceDescriptorHeap.gpuStart = renderer->resourceDescriptorHeap.handle->GetGPUDescriptorHandleForHeapStart();

        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&renderer->resourceDescriptorHeap.fence)));
        renderer->resourceDescriptorHeap.fenceValue = renderer->resourceDescriptorHeap.fence->GetCompletedValue();

        //for (uint32_t i = 0; i < kBindlessResourceCapacity; ++i)
        //{
        //    freeBindlessResources.push_back(kBindlessResourceCapacity - i - 1);
        //}

        renderer->samplerDescriptorHeap.numDescriptors = 2048; // tier 2 limit

        heapDesc.NodeMask = 0;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.NumDescriptors = renderer->samplerDescriptorHeap.numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        VHR(renderer->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&renderer->samplerDescriptorHeap.handle)));

        renderer->samplerDescriptorHeap.cpuStart = renderer->samplerDescriptorHeap.handle->GetCPUDescriptorHandleForHeapStart();
        renderer->samplerDescriptorHeap.gpuStart = renderer->samplerDescriptorHeap.handle->GetGPUDescriptorHandleForHeapStart();

        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&renderer->samplerDescriptorHeap.fence)));
        renderer->samplerDescriptorHeap.fenceValue = renderer->samplerDescriptorHeap.fence->GetCompletedValue();

        //for (uint32_t i = 0; i < kBindlessSamplerCapacity; ++i)
        //{
        //    freeBindlessSamplers.push_back(kBindlessSamplerCapacity - i - 1);
        //}
    }

    // Create common indirect command signatures
    {
        // DispatchIndirectCommand
        D3D12_INDIRECT_ARGUMENT_DESC dispatchArg{};
        dispatchArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC cmdSignatureDesc = {};
        cmdSignatureDesc.ByteStride = sizeof(VGPUDispatchIndirectCommand);
        cmdSignatureDesc.NumArgumentDescs = 1;
        cmdSignatureDesc.pArgumentDescs = &dispatchArg;
        VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->dispatchIndirectCommandSignature)));

        // DrawIndirectCommand
        D3D12_INDIRECT_ARGUMENT_DESC drawInstancedArg{};
        drawInstancedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        cmdSignatureDesc.ByteStride = sizeof(VGPUDrawIndirectCommand);
        cmdSignatureDesc.NumArgumentDescs = 1;
        cmdSignatureDesc.pArgumentDescs = &drawInstancedArg;
        VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->drawIndirectCommandSignature)));

        // DrawIndexedIndirectCommand
        D3D12_INDIRECT_ARGUMENT_DESC drawIndexedInstancedArg{};
        drawIndexedInstancedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        cmdSignatureDesc.ByteStride = sizeof(VGPUDrawIndexedIndirectCommand);
        cmdSignatureDesc.NumArgumentDescs = 1;
        cmdSignatureDesc.pArgumentDescs = &drawIndexedInstancedArg;
        VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->drawIndexedIndirectCommandSignature)));

        if (renderer->d3dFeatures.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1)
        {
            D3D12_INDIRECT_ARGUMENT_DESC dispatchMeshArg{};
            dispatchMeshArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

            cmdSignatureDesc.ByteStride = sizeof(VGPUDispatchIndirectCommand);
            cmdSignatureDesc.NumArgumentDescs = 1;
            cmdSignatureDesc.pArgumentDescs = &dispatchMeshArg;
            VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->dispatchMeshIndirectCommandSignature)));
        }
    }

    // Init features
    renderer->featureLevel = renderer->d3dFeatures.MaxSupportedFeatureLevel();
    VHR(renderer->queues[VGPUCommandQueue_Graphics].handle->GetTimestampFrequency(&renderer->timestampFrequency));

    // Log some info
    vgpuLogInfo("VGPU Driver: D3D12");
    vgpuLogInfo("D3D12 Adapter: %ls", renderer->adapterDesc.Description);

    return renderer;
}

VGPUDriver D3D12_Driver = {
    VGPUBackend_D3D12,
    d3d12_isSupported,
    d3d12_createDevice
};

#undef VHR
#undef SAFE_RELEASE

uint32_t vgpuToDxgiFormat(VGPUTextureFormat format)
{
    return (uint32_t)ToDxgiFormat(format);
}

VGPUTextureFormat vgpuFromDxgiFormat(uint32_t dxgiFormat)
{
    return FromDxgiFormat((DXGI_FORMAT)dxgiFormat);
}

#else

uint32_t vgpuToDxgiFormat(VGPUTextureFormat format)
{
    _VGPU_UNUSED(format);
    return 0;
}


VGPUTextureFormat vgpuFromDxgiFormat(uint32_t dxgiFormat)
{
    _VGPU_UNUSED(dxgiFormat);
    return VGPUTextureFormat_Undefined;
}

#endif /* VGPU_D3D12_DRIVER */
