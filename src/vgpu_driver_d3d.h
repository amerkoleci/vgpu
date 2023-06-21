// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#pragma once

#if defined(VGPU_D3D11_DRIVER) || defined(VGPU_D3D12_DRIVER)

#include "vgpu_driver.h"

#if defined(VGPU_D3D12_DRIVER)
#include <directx/dxgiformat.h>
#else
#include <dxgiformat.h>
#endif
#include <d3dcommon.h>

namespace vgpu
{
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
}

#endif /* defined(VGPU_D3D11_DRIVER) || defined(VGPU_D3D12_DRIVER) */
