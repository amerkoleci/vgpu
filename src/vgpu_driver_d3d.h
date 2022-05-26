// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#pragma once

#include "vgpu_driver.h"

#ifdef USING_DIRECTX_HEADERS
#   include <directx/dxgiformat.h>
#else
#   include <dxgiformat.h>
#endif
#include <dxgi1_2.h>

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   include <dxgidebug.h>
#endif

#include <string>

#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

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
            case VGPUTextureFormat_RGBA32UInt:       return DXGI_FORMAT_R32G32B32A32_UINT;
            case VGPUTextureFormat_RGBA32SInt:       return DXGI_FORMAT_R32G32B32A32_SINT;
            case VGPUTextureFormat_RGBA32Float:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
                // Depth-stencil formats
            case VGPUTextureFormat_Depth16UNorm:		    return DXGI_FORMAT_D16_UNORM;
            case VGPUTextureFormat_Depth24UNormStencil8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth32Float:			return DXGI_FORMAT_D32_FLOAT;
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

            default:
                return DXGI_FORMAT_UNKNOWN;
        }
    }

    constexpr VGFXTextureFormat ToDXGISwapChainFormat(VGFXTextureFormat format)
    {
        switch (format)
        {
            case VGFXTextureFormat_RGBA16Float:
                return VGFXTextureFormat_RGBA16Float;

            case VGFXTextureFormat_BGRA8UNorm:
            case VGFXTextureFormat_BGRA8UNormSrgb:
                return VGFXTextureFormat_BGRA8UNorm;

            case VGFXTextureFormat_RGBA8UNorm:
            case VGFXTextureFormat_RGBA8UNormSrgb:
                return VGFXTextureFormat_RGBA8UNorm;

            case VGFXTextureFormat_RGB10A2UNorm:
                return VGFXTextureFormat_RGB10A2UNorm;
        }

        return VGFXTextureFormat_BGRA8UNorm;
    }

    constexpr DXGI_FORMAT GetTypelessFormatFromDepthFormat(VGFXTextureFormat format)
    {
        switch (format)
        {
            case VGPUTextureFormat_Depth16UNorm:
                return DXGI_FORMAT_R16_TYPELESS;
            case VGPUTextureFormat_Depth32Float:
                return DXGI_FORMAT_R32_TYPELESS;
            case VGPUTextureFormat_Depth24UNormStencil8:
                return DXGI_FORMAT_R24G8_TYPELESS;
            case VGPUTextureFormat_Depth32FloatStencil8:
                return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            default:
                VGPU_ASSERT(vgpuIsDepthFormat(format) == false);
                return ToDXGIFormat(format);
        }
    }

    constexpr uint32_t PresentModeToBufferCount(VGPUPresentMode mode)
    {
        switch (mode)
        {
            case VGPU_PRESENT_MODE_IMMEDIATE:
            case VGPU_PRESENT_MODE_FIFO:
                return 2;
            case VGPU_PRESENT_MODE_MAILBOX:
                return 3;
            default:
                return 2;
        }
    }

    constexpr uint32_t PresentModeToSwapInterval(VGPUPresentMode mode)
    {
        switch (mode)
        {
            case VGPU_PRESENT_MODE_IMMEDIATE:
            case VGPU_PRESENT_MODE_MAILBOX:
                return 0;

            case VGPU_PRESENT_MODE_FIFO:
            default:
                return 1;
        }
    }
}
