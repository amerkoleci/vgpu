// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#pragma once

#include "vgfx_driver.h"

#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#ifdef USING_DIRECTX_HEADERS
#   include <directx/dxgiformat.h>
#else
#   include <dxgiformat.h>
#endif
#include <d3dcommon.h>

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   include <dxgidebug.h>
#endif

namespace
{
#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    // Declare debug guids to avoid linking with "dxguid.lib"
    static constexpr IID VGFX_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8} };
    static constexpr IID VGFX_DXGI_DEBUG_DXGI = { 0x25cddaa4, 0xb1c6, 0x47e1, {0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a} };
#endif

    template <typename T>
    void SafeRelease(T& resource)
    {
        if (resource)
        {
            resource->Release();
            resource = nullptr;
        }
    }

    constexpr DXGI_FORMAT ToDXGIFormat(VGFXTextureFormat format)
    {
        switch (format)
        {
            // 8-bit formats
            case VGFXTextureFormat_R8UInt:          return DXGI_FORMAT_R8_UINT;
            case VGFXTextureFormat_R8SInt:          return DXGI_FORMAT_R8_SINT;
            case VGFXTextureFormat_R8UNorm:         return DXGI_FORMAT_R8_UNORM;
            case VGFXTextureFormat_R8SNorm:         return DXGI_FORMAT_R8_SNORM;
                // 16-bit formats
            case VGFXTextureFormat_R16UInt:         return DXGI_FORMAT_R16_UINT;
            case VGFXTextureFormat_R16SInt:         return DXGI_FORMAT_R16_SINT;
            case VGFXTextureFormat_R16UNorm:        return DXGI_FORMAT_R16_UNORM;
            case VGFXTextureFormat_R16SNorm:        return DXGI_FORMAT_R16_SNORM;
            case VGFXTextureFormat_R16Float:        return DXGI_FORMAT_R16_FLOAT;
            case VGFXTextureFormat_RG8UNorm:        return DXGI_FORMAT_R8G8_UNORM;
            case VGFXTextureFormat_RG8SNorm:        return DXGI_FORMAT_R8G8_SNORM;
            case VGFXTextureFormat_RG8UInt:         return DXGI_FORMAT_R8G8_UINT;
            case VGFXTextureFormat_RG8SInt:         return DXGI_FORMAT_R8G8_SINT;
                // Packed 16-Bit Pixel Formats
            case VGFXTextureFormat_BGRA4UNorm:      return DXGI_FORMAT_B4G4R4A4_UNORM;
            case VGFXTextureFormat_B5G6R5UNorm:     return DXGI_FORMAT_B5G6R5_UNORM;
            case VGFXTextureFormat_B5G5R5A1UNorm:   return DXGI_FORMAT_B5G5R5A1_UNORM;
                // 32-bit formats
            case VGFXTextureFormat_R32UInt:          return DXGI_FORMAT_R32_UINT;
            case VGFXTextureFormat_R32SInt:          return DXGI_FORMAT_R32_SINT;
            case VGFXTextureFormat_R32Float:         return DXGI_FORMAT_R32_FLOAT;
            case VGFXTextureFormat_RG16UInt:         return DXGI_FORMAT_R16G16_UINT;
            case VGFXTextureFormat_RG16SInt:         return DXGI_FORMAT_R16G16_SINT;
            case VGFXTextureFormat_RG16UNorm:        return DXGI_FORMAT_R16G16_UNORM;
            case VGFXTextureFormat_RG16SNorm:        return DXGI_FORMAT_R16G16_SNORM;
            case VGFXTextureFormat_RG16Float:        return DXGI_FORMAT_R16G16_FLOAT;
            case VGFXTextureFormat_RGBA8UInt:        return DXGI_FORMAT_R8G8B8A8_UINT;
            case VGFXTextureFormat_RGBA8SInt:        return DXGI_FORMAT_R8G8B8A8_SINT;
            case VGFXTextureFormat_RGBA8UNorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
            case VGFXTextureFormat_RGBA8UNormSrgb:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case VGFXTextureFormat_RGBA8SNorm:       return DXGI_FORMAT_R8G8B8A8_SNORM;
            case VGFXTextureFormat_BGRA8UNorm:       return DXGI_FORMAT_B8G8R8A8_UNORM;
            case VGFXTextureFormat_BGRA8UNormSrgb:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                // Packed 32-Bit formats
            case VGFXTextureFormat_RGB10A2UNorm:     return DXGI_FORMAT_R10G10B10A2_UNORM;
            case VGFXTextureFormat_RG11B10Float:     return DXGI_FORMAT_R11G11B10_FLOAT;
            case VGFXTextureFormat_RGB9E5Float:      return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
                // 64-Bit formats
            case VGFXTextureFormat_RG32UInt:         return DXGI_FORMAT_R32G32_UINT;
            case VGFXTextureFormat_RG32SInt:         return DXGI_FORMAT_R32G32_SINT;
            case VGFXTextureFormat_RG32Float:        return DXGI_FORMAT_R32G32_FLOAT;
            case VGFXTextureFormat_RGBA16UInt:       return DXGI_FORMAT_R16G16B16A16_UINT;
            case VGFXTextureFormat_RGBA16SInt:       return DXGI_FORMAT_R16G16B16A16_SINT;
            case VGFXTextureFormat_RGBA16UNorm:      return DXGI_FORMAT_R16G16B16A16_UNORM;
            case VGFXTextureFormat_RGBA16SNorm:      return DXGI_FORMAT_R16G16B16A16_SNORM;
            case VGFXTextureFormat_RGBA16Float:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
                // 128-Bit formats
            case VGFXTextureFormat_RGBA32UInt:       return DXGI_FORMAT_R32G32B32A32_UINT;
            case VGFXTextureFormat_RGBA32SInt:       return DXGI_FORMAT_R32G32B32A32_SINT;
            case VGFXTextureFormat_RGBA32Float:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
                // Depth-stencil formats
            case VGFXTextureFormat_Depth16UNorm:		    return DXGI_FORMAT_D16_UNORM;
            case VGFXTextureFormat_Depth24UNormStencil8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGFXTextureFormat_Depth32Float:			return DXGI_FORMAT_D32_FLOAT;
            case VGFXTextureFormat_Depth32FloatStencil8:    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                // Compressed BC formats
            case VGFXTextureFormat_BC1UNorm:            return DXGI_FORMAT_BC1_UNORM;
            case VGFXTextureFormat_BC1UNormSrgb:        return DXGI_FORMAT_BC1_UNORM_SRGB;
            case VGFXTextureFormat_BC2UNorm:            return DXGI_FORMAT_BC2_UNORM;
            case VGFXTextureFormat_BC2UNormSrgb:        return DXGI_FORMAT_BC2_UNORM_SRGB;
            case VGFXTextureFormat_BC3UNorm:            return DXGI_FORMAT_BC3_UNORM;
            case VGFXTextureFormat_BC3UNormSrgb:        return DXGI_FORMAT_BC3_UNORM_SRGB;
            case VGFXTextureFormat_BC4SNorm:            return DXGI_FORMAT_BC4_SNORM;
            case VGFXTextureFormat_BC4UNorm:            return DXGI_FORMAT_BC4_UNORM;
            case VGFXTextureFormat_BC5SNorm:            return DXGI_FORMAT_BC5_SNORM;
            case VGFXTextureFormat_BC5UNorm:            return DXGI_FORMAT_BC5_UNORM;
            case VGFXTextureFormat_BC6HUFloat:          return DXGI_FORMAT_BC6H_UF16;
            case VGFXTextureFormat_BC6HSFloat:          return DXGI_FORMAT_BC6H_SF16;
            case VGFXTextureFormat_BC7UNorm:            return DXGI_FORMAT_BC7_UNORM;
            case VGFXTextureFormat_BC7UNormSrgb:        return DXGI_FORMAT_BC7_UNORM_SRGB;

            default:
                return DXGI_FORMAT_UNKNOWN;
        }
    }

    constexpr DXGI_FORMAT ToDXGISwapChainFormat(VGFXTextureFormat format)
    {
        switch (format)
        {
            case VGFXTextureFormat_RGBA16Float:
                return DXGI_FORMAT_R16G16B16A16_FLOAT;

            case VGFXTextureFormat_BGRA8UNorm:
            case VGFXTextureFormat_BGRA8UNormSrgb:
                return DXGI_FORMAT_B8G8R8A8_UNORM;

            case VGFXTextureFormat_RGBA8UNorm:
            case VGFXTextureFormat_RGBA8UNormSrgb:
                return DXGI_FORMAT_R8G8B8A8_UNORM;

            case VGFXTextureFormat_RGB10A2UNorm:
                return DXGI_FORMAT_R10G10B10A2_UNORM;
        }

        return DXGI_FORMAT_B8G8R8A8_UNORM;
    }

    constexpr DXGI_FORMAT GetTypelessFormatFromDepthFormat(VGFXTextureFormat format)
    {
        switch (format)
        {
            case VGFXTextureFormat_Depth16UNorm:
                return DXGI_FORMAT_R16_TYPELESS;
            case VGFXTextureFormat_Depth32Float:
                return DXGI_FORMAT_R32_TYPELESS;
            case VGFXTextureFormat_Depth24UNormStencil8:
                return DXGI_FORMAT_R24G8_TYPELESS;
            case VGFXTextureFormat_Depth32FloatStencil8:
                return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            default:
                VGFX_ASSERT(vgfxIsDepthFormat(format) == false);
                return ToDXGIFormat(format);
        }
    }
}
