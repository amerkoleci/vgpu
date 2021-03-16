// Copyright (c) Amer Koleci.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#pragma once

#if defined(VGPU_DRIVER_D3D11) || defined(VGPU_DRIVER_D3D12)
#include "vgpu_driver.h"

#if defined(NTDDI_WIN10_RS2)
#   include <dxgi1_6.h>
#else
#   include <dxgi1_5.h>
#endif

#ifdef _DEBUG
#include <dxgidebug.h>

// Declare debug guids to avoid linking with "dxguid.lib"
static const IID D3D_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8} };
static const IID D3D_DXGI_DEBUG_DXGI = { 0x25cddaa4, 0xb1c6, 0x47e1, {0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a} };
#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY1)(REFIID _riid, void** _factory);
typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY2)(UINT flags, REFIID _riid, void** _factory);
typedef HRESULT(WINAPI* PFN_GET_DXGI_DEBUG_INTERFACE1)(UINT flags, REFIID _riid, void** _debug);
#endif

#define VHR(hr) if (FAILED(hr)) { VGPU_ASSERT(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

typedef enum {
    DXGIFACTORY_CAPS_FLIP_PRESENT = (1 << 0),
    DXGIFACTORY_CAPS_TEARING = (1 << 1),
} dxgi_factory_caps;

static inline int _vgpu_StringConvert(const wchar_t* from, char* to)
{
    int num = WideCharToMultiByte(CP_UTF8, 0, from, -1, NULL, 0, NULL, NULL);
    if (num > 0)
    {
        WideCharToMultiByte(CP_UTF8, 0, from, -1, &to[0], num, NULL, NULL);
    }

    return num;
}

static inline DXGI_FORMAT _vgpu_to_dxgi_format(VGPUTextureFormat format)
{
    switch (format)
    {
        // 8-bit pixel formats
    case VGPUTextureFormat_R8Unorm:         return DXGI_FORMAT_R8_UNORM;
    case VGPUTextureFormat_R8Snorm:         return DXGI_FORMAT_R8_SNORM;
    case VGPUTextureFormat_R8Uint:          return DXGI_FORMAT_R8_UINT;
    case VGPUTextureFormat_R8Sint:          return DXGI_FORMAT_R8_SINT;
        // 16-bit formats.
    case VGPUTextureFormat_R16Unorm:        return DXGI_FORMAT_R16_UNORM;
    case VGPUTextureFormat_R16Snorm:        return DXGI_FORMAT_R16_SNORM;
    case VGPUTextureFormat_R16Uint:         return DXGI_FORMAT_R16_UINT;
    case VGPUTextureFormat_R16Sint:         return DXGI_FORMAT_R16_SINT;
    case VGPUTextureFormat_R16Float:        return DXGI_FORMAT_R16_FLOAT;
    case VGPUTextureFormat_RG8Unorm:        return DXGI_FORMAT_R8G8_UNORM;
    case VGPUTextureFormat_RG8Snorm:        return DXGI_FORMAT_R8G8_SNORM;
    case VGPUTextureFormat_RG8Uint:         return DXGI_FORMAT_R8G8_UINT;
    case VGPUTextureFormat_RG8Sint:         return DXGI_FORMAT_R8G8_SINT;
        // 32-bit formats.
    case VGPUTextureFormat_R32UInt:         return DXGI_FORMAT_R32_UINT;
    case VGPUTextureFormat_R32SInt:         return DXGI_FORMAT_R32_SINT;
    case VGPUTextureFormat_R32Float:        return DXGI_FORMAT_R32_FLOAT;
    case VGPUTextureFormat_RG16UNorm:       return DXGI_FORMAT_R16G16_UNORM;
    case VGPUTextureFormat_RG16SNorm:       return DXGI_FORMAT_R16G16_SNORM;
    case VGPUTextureFormat_RG16UInt:        return DXGI_FORMAT_R16G16_UINT;
    case VGPUTextureFormat_RG16SInt:        return DXGI_FORMAT_R16G16_SINT;
    case VGPUTextureFormat_RG16Float:       return DXGI_FORMAT_R16G16_FLOAT;
    case VGPUTextureFormat_RGBA8Unorm:      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case VGPUTextureFormat_RGBA8UnormSrgb:  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case VGPUTextureFormat_RGBA8Snorm:      return DXGI_FORMAT_R8G8B8A8_SNORM;
    case VGPUTextureFormat_RGBA8Uint:       return DXGI_FORMAT_R8G8B8A8_UINT;
    case VGPUTextureFormat_RGBA8Sint:       return DXGI_FORMAT_R8G8B8A8_SINT;
    case VGPUTextureFormat_BGRA8Unorm:      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case VGPUTextureFormat_BGRA8UnormSrgb:  return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        // Packed 32-Bit formats.
    case VGPUTextureFormat_RGB10A2Unorm:    return DXGI_FORMAT_R10G10B10A2_UNORM;
    case VGPUTextureFormat_RG11B10Float:    return DXGI_FORMAT_R11G11B10_FLOAT;
    case VGPUTextureFormat_RGB9E5Float:     return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        // 64-Bit formats.
    case VGPUTextureFormat_RG32Uint:        return DXGI_FORMAT_R32G32_UINT;
    case VGPUTextureFormat_RG32Sint:        return DXGI_FORMAT_R32G32_SINT;
    case VGPUTextureFormat_RG32Float:       return DXGI_FORMAT_R32G32_FLOAT;
    case VGPUTextureFormat_RGBA16Unorm:     return DXGI_FORMAT_R16G16B16A16_UNORM;
    case VGPUTextureFormat_RGBA16Snorm:     return DXGI_FORMAT_R16G16B16A16_SNORM;
    case VGPUTextureFormat_RGBA16Uint:      return DXGI_FORMAT_R16G16B16A16_UINT;
    case VGPUTextureFormat_RGBA16Sint:      return DXGI_FORMAT_R16G16B16A16_SINT;
    case VGPUTextureFormat_RGBA16Float:     return DXGI_FORMAT_R16G16B16A16_FLOAT;
        // 128-Bit formats.
    case VGPUTextureFormat_RGBA32Uint:      return DXGI_FORMAT_R32G32B32A32_UINT;
    case VGPUTextureFormat_RGBA32Sint:      return DXGI_FORMAT_R32G32B32A32_SINT;
    case VGPUTextureFormat_RGBA32Float:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        // Depth-stencil formats.
    case VGPUTextureFormat_Depth16Unorm:            return DXGI_FORMAT_D16_UNORM;
    case VGPUTextureFormat_Depth32Float:            return DXGI_FORMAT_D32_FLOAT;
    case VGPUTextureFormat_Depth24UnormStencil8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case VGPUTextureFormat_Depth32FloatStencil8:    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        // Compressed BC formats.
    case VGPUTextureFormat_BC1RGBAUnorm:        return DXGI_FORMAT_BC1_UNORM;
    case VGPUTextureFormat_BC1RGBAUnormSrgb:    return DXGI_FORMAT_BC1_UNORM_SRGB;
    case VGPUTextureFormat_BC2RGBAUnorm:        return DXGI_FORMAT_BC2_UNORM;
    case VGPUTextureFormat_BC2RGBAUnormSrgb:    return DXGI_FORMAT_BC2_UNORM_SRGB;
    case VGPUTextureFormat_BC3RGBAUnorm:        return DXGI_FORMAT_BC3_UNORM;
    case VGPUTextureFormat_BC3RGBAUnormSrgb:    return DXGI_FORMAT_BC3_UNORM_SRGB;
    case VGPUTextureFormat_BC4RUnorm:           return DXGI_FORMAT_BC4_UNORM;
    case VGPUTextureFormat_BC4RSnorm:           return DXGI_FORMAT_BC4_SNORM;
    case VGPUTextureFormat_BC5RGUnorm:          return DXGI_FORMAT_BC5_UNORM;
    case VGPUTextureFormat_BC5RGSnorm:          return DXGI_FORMAT_BC5_SNORM;
    case VGPUTextureFormat_BC6HRGBUFloat:       return DXGI_FORMAT_BC6H_UF16;
    case VGPUTextureFormat_BC6HRGBFloat:        return DXGI_FORMAT_BC6H_SF16;
    case VGPUTextureFormat_BC7RGBAUnorm:        return DXGI_FORMAT_BC7_UNORM;
    case VGPUTextureFormat_BC7RGBAUnormSrgb:    return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:
        VGPU_UNREACHABLE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

static inline DXGI_FORMAT _vgpu_d3d_typeless_from_depth_format(VGPUTextureFormat format)
{
    switch (format)
    {
    case VGPUTextureFormat_Depth16Unorm:            return DXGI_FORMAT_R16_TYPELESS;
    case VGPUTextureFormat_Depth32Float:            return DXGI_FORMAT_R32_TYPELESS;
    case VGPUTextureFormat_Depth24UnormStencil8:    return DXGI_FORMAT_R24G8_TYPELESS;
    case VGPUTextureFormat_Depth32FloatStencil8:    return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

    default:
        VGPU_ASSERT(vgpu_is_depth_format(format) == false);
        return _vgpu_to_dxgi_format(format);
    }
}

static inline DXGI_FORMAT _vgpu_d3d_format_with_usage(VGPUTextureFormat format, VGPUTextureUsageFlags usage)
{
    // If depth and either ua or sr, set to typeless
    if (vgpu_is_depth_stencil_format(format) &&
        ((usage & (VGPUTextureUsage_Sampled | VGPUTextureUsage_Storage)) != 0))
    {
        return _vgpu_d3d_typeless_from_depth_format(format);
    }

    return _vgpu_to_dxgi_format(format);
}


static inline DXGI_FORMAT vgpu_d3d_swapchain_format(VGPUTextureFormat format)
{
    switch (format) {
    case VGPUTextureFormat_RGBA16Float:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case VGPUTextureFormat_BGRA8Unorm:
    case VGPUTextureFormat_BGRA8UnormSrgb:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case VGPUTextureFormat_RGBA8Unorm:
    case VGPUTextureFormat_RGBA8UnormSrgb:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case VGPUTextureFormat_RGB10A2Unorm:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    }

    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

static inline IDXGISwapChain1* vgpu_d3d_create_swapchain(
    IDXGIFactory2* dxgi_factory, uint32_t dxgi_factory_caps,
    IUnknown* deviceOrCommandQueue,
    void* window_handle,
    uint32_t width, uint32_t height,
    VGPUTextureFormat format,
    uint32_t backbuffer_count,
    vgpu_bool windowed)
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window = (HWND)window_handle;
    if (!IsWindow(window)) {
        vgpu_log_error("Invalid HWND handle");
        return NULL;
    }
#else
    IUnknown* window = (IUnknown*)handle;
#endif

    UINT flags = 0;

    if (dxgi_factory_caps & DXGIFACTORY_CAPS_TEARING)
    {
        flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    DXGI_SCALING scaling = DXGI_SCALING_STRETCH;
    DXGI_SWAP_EFFECT swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    if (!(dxgi_factory_caps & DXGIFACTORY_CAPS_FLIP_PRESENT))
    {
        swapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }
#else
    DXGI_SCALING scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
    DXGI_SWAP_EFFECT swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = width;
    swapchainDesc.Height = height;
    swapchainDesc.Format = vgpu_d3d_swapchain_format(format);
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = backbuffer_count;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapchainDesc.Scaling = scaling;
    swapchainDesc.SwapEffect = swapEffect;
    swapchainDesc.Flags = flags;

    IDXGISwapChain1* result = NULL;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // Create a SwapChain from a Win32 window.
    VHR(dxgi_factory->CreateSwapChainForHwnd(
        deviceOrCommandQueue,
        window,
        &swapchainDesc,
        &fsSwapChainDesc,
        NULL,
        &result
    ));

    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
    VHR(dxgi_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
#else
    VHR(dxgi_factory->CreateSwapChainForCoreWindow(
        deviceOrCommandQueue,
        window,
        &swapchain_desc,
        NULL,
        &result
    ));
#endif

    return result;
    }

#endif /* defined(VGPU_DRIVER_D3D11) || defined(VGPU_DRIVER_D3D12) */
