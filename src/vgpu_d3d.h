//
// Copyright (c) 2019-2020 Amer Koleci.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "vgpu/vgpu.h"
#include <d3dcommon.h>
#include <dxgiformat.h>

static DXGI_FORMAT _vgpu_d3d_get_format(VGPUTextureFormat format) {
    static DXGI_FORMAT formats[VGPUTextureFormat_Count] = {
        DXGI_FORMAT_UNKNOWN,
        // 8-bit pixel formats
        DXGI_FORMAT_R8_UNORM,
        DXGI_FORMAT_R8_SNORM,
        DXGI_FORMAT_R8_UINT,
        DXGI_FORMAT_R8_SINT,
        // 16-bit pixel formats
        DXGI_FORMAT_R16_UNORM,
        DXGI_FORMAT_R16_SNORM,
        DXGI_FORMAT_R16_UINT,
        DXGI_FORMAT_R16_SINT,
        DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R8G8_UNORM,
        DXGI_FORMAT_R8G8_SNORM,
        DXGI_FORMAT_R8G8_UINT,
        DXGI_FORMAT_R8G8_SINT,

        // 32-bit pixel formats
        DXGI_FORMAT_R32_UINT,
        DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R32_FLOAT,

        //DXGI_FORMAT_R16G16_UNORM,
        //DXGI_FORMAT_R16G16_SNORM,
        DXGI_FORMAT_R16G16_UINT,
        DXGI_FORMAT_R16G16_SINT,
        DXGI_FORMAT_R16G16_FLOAT,

        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_SNORM,
        DXGI_FORMAT_R8G8B8A8_UINT,
        DXGI_FORMAT_R8G8B8A8_SINT,

        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,

        // Packed 32-Bit Pixel formats
        DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R11G11B10_FLOAT,

        // 64-Bit Pixel Formats
        DXGI_FORMAT_R32G32_UINT,
        DXGI_FORMAT_R32G32_SINT,
        DXGI_FORMAT_R32G32_FLOAT,
        //DXGI_FORMAT_R16G16B16A16_UNORM,
        //DXGI_FORMAT_R16G16B16A16_SNORM,
        DXGI_FORMAT_R16G16B16A16_UINT,
        DXGI_FORMAT_R16G16B16A16_SINT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,

        // 128-Bit Pixel Formats
        DXGI_FORMAT_R32G32B32A32_UINT,
        DXGI_FORMAT_R32G32B32A32_SINT,
        DXGI_FORMAT_R32G32B32A32_FLOAT,

        // Depth-stencil formats
        DXGI_FORMAT_D16_UNORM,
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,

        // Compressed BC formats
        DXGI_FORMAT_BC1_UNORM,
        DXGI_FORMAT_BC1_UNORM_SRGB,
        DXGI_FORMAT_BC2_UNORM,
        DXGI_FORMAT_BC2_UNORM_SRGB,
        DXGI_FORMAT_BC3_UNORM,
        DXGI_FORMAT_BC3_UNORM_SRGB,
        DXGI_FORMAT_BC4_UNORM,
        DXGI_FORMAT_BC4_SNORM,
        DXGI_FORMAT_BC5_UNORM,
        DXGI_FORMAT_BC5_SNORM,
        DXGI_FORMAT_BC6H_UF16,
        DXGI_FORMAT_BC6H_SF16,
        DXGI_FORMAT_BC7_UNORM,
        DXGI_FORMAT_BC7_UNORM_SRGB,
    };
    return formats[format];
}

static DXGI_FORMAT _vgpu_d3d_get_typeless_format(VGPUTextureFormat format)
{
    switch (format)
    {
    case VGPUTextureFormat_Depth16Unorm:
        return DXGI_FORMAT_R16_TYPELESS;
    case VGPUTextureFormat_Depth32Float:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case VGPUTextureFormat_Depth24Plus:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case VGPUTextureFormat_Depth24PlusStencil8:
        return DXGI_FORMAT_R32_TYPELESS;
    default:
        assert(!vgpu_is_depth_format(format));
        return _vgpu_d3d_get_format(format);
    }
}

static DXGI_FORMAT _vgpu_d3d_swapchain_pixel_format(VGPUTextureFormat format) {
    switch (format)
    {
    case VGPUTextureFormat_Undefined:
    case VGPUTextureFormat_BGRA8Unorm:
    case VGPUTextureFormat_BGRA8UnormSrgb:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case VGPUTextureFormat_RGBA8Unorm:
    case VGPUTextureFormat_RGBA8UnormSrgb:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case VGPUTextureFormat_RGBA16Float:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case VGPUTextureFormat_RGB10A2Unorm:
        return DXGI_FORMAT_R10G10B10A2_UNORM;

    default:
        vgpu_log_error_format("PixelFormat (%u) is not supported for creating swapchain buffer", (uint32_t)format);
        return DXGI_FORMAT_UNKNOWN;
    }
}

static UINT vgpuD3DGetSyncInterval(VGPUPresentMode mode)
{
    switch (mode)
    {
    case VGPUPresentMode_Immediate:
        return 0;

    case VGPUPresentMode_Mailbox:
        return 2;

    case VGPUPresentMode_Fifo:
    default:
        return 1;
    }
}
