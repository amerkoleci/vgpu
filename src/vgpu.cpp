//
// Copyright (c) 2019 Amer Koleci.
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

#define VGPU_IMPLEMENTATION
#include "vgpu_internal.h"
#include <assert.h>

VgpuBackend vgpuGetBackend()
{
#if defined(VGPU_D3D12) || defined(ALIMER_D3D12)
    return VGPU_BACKEND_D3D12;
#elif defined(VGPU_D3D11) || defined(ALIMER_D3D11)
    return VGPU_BACKEND_D3D11;
#elif defined(VGPU_VK) || defined(ALIMER_VULKAN)
    return VGPU_BACKEND_VULKAN;
#elif defined(VGPU_GL) || defined(ALIMER_OPENGL)
    return VGPU_BACKEND_OPENGL;
#else
    return VGPU_BACKEND_INVALID;
#endif
}

/* Pixel Format */
typedef struct VgpuPixelFormatDesc
{
    VgpuPixelFormat         format;
    const char*             name;
    VgpuPixelFormatType     type;
    uint8_t                 bitsPerPixel;
    struct
    {
        uint8_t             blockWidth;
        uint8_t             blockHeight;
        uint8_t             blockSize;
        uint8_t             minBlockX;
        uint8_t             minBlockY;
    } compression;

    struct
    {
        uint8_t             depth;
        uint8_t             stencil;
        uint8_t             red;
        uint8_t             green;
        uint8_t             blue;
        uint8_t             alpha;
    } bits;
} VgpuPixelFormatDesc;

const VgpuPixelFormatDesc FormatDesc[] =
{
    // format                               name,               type                                bpp         compression             bits
    { VGPU_PIXEL_FORMAT_UNKNOWN,            "Unknown",          VGPU_PIXEL_FORMAT_TYPE_UNKNOWN,     0,          {0, 0, 0, 0, 0},        {0, 0, 0, 0, 0, 0}},
    // 8-bit pixel formats
    { VGPU_PIXEL_FORMAT_A8_UNORM,           "A8",               VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {1, 1, 1, 1, 1},        {0, 0, 0, 0, 0, 8}},
    { VGPU_PIXEL_FORMAT_R8_UNORM,           "R8",               VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R8_SNORM,           "R8S",              VGPU_PIXEL_FORMAT_TYPE_SNORM,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R8_UINT,            "R8U",              VGPU_PIXEL_FORMAT_TYPE_UINT,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R8_SINT,            "R8I",              VGPU_PIXEL_FORMAT_TYPE_SINT,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},

    // 16-bit pixel formats
    { VGPU_PIXEL_FORMAT_R16_UNORM,          "R16",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R16_SNORM,          "R16S",             VGPU_PIXEL_FORMAT_TYPE_SNORM,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R16_UINT,           "R16U",             VGPU_PIXEL_FORMAT_TYPE_UINT,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R16_SINT,           "R16I",             VGPU_PIXEL_FORMAT_TYPE_SINT,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R16_FLOAT,          "R16F",             VGPU_PIXEL_FORMAT_TYPE_FLOAT,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG8_UNORM,          "RG8",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG8_SNORM,          "RG8S",             VGPU_PIXEL_FORMAT_TYPE_SNORM,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG8_UINT,           "RG8U",             VGPU_PIXEL_FORMAT_TYPE_UINT,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG8_SINT,           "RG8I",             VGPU_PIXEL_FORMAT_TYPE_SINT,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},

    // Packed 16-bit pixel formats
    { VGPU_PIXEL_FORMAT_R5G6B5_UNORM,       "R5G6B5",           VGPU_PIXEL_FORMAT_TYPE_UNORM,       16,         {1, 1, 2, 1, 1},        {0, 0, 5, 6, 5, 0}},
    { VGPU_PIXEL_FORMAT_RGBA4_UNORM,        "RGBA4",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       16,         {1, 1, 2, 1, 1},        {0, 0, 4, 4, 4, 4}},

    // 32-bit pixel formats
    { VGPU_PIXEL_FORMAT_R32_UINT,           "R32U",             VGPU_PIXEL_FORMAT_TYPE_UINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R32_SINT,           "R32I",             VGPU_PIXEL_FORMAT_TYPE_SINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R32_FLOAT,          "R32F",             VGPU_PIXEL_FORMAT_TYPE_FLOAT,       32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_UNORM,         "RG16",             VGPU_PIXEL_FORMAT_TYPE_UNORM,       32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_SNORM,         "RG16S",            VGPU_PIXEL_FORMAT_TYPE_SNORM,       32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_UINT,          "RG16U",            VGPU_PIXEL_FORMAT_TYPE_UINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_SINT,          "RG16I",            VGPU_PIXEL_FORMAT_TYPE_SINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_FLOAT,         "RG16F",            VGPU_PIXEL_FORMAT_TYPE_FLOAT,       32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RGBA8_UNORM,        "RGBA8",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_SNORM,        "RGBA8S",           VGPU_PIXEL_FORMAT_TYPE_SNORM,       32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_UINT,         "RGBA8U",           VGPU_PIXEL_FORMAT_TYPE_UINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_SINT,         "RGBA8I",           VGPU_PIXEL_FORMAT_TYPE_SINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_BGRA8_UNORM,        "BGRA8",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},

    // Packed 32-Bit Pixel formats
    { VGPU_PIXEL_FORMAT_RGB10A2_UNORM,      "RGB10A2",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    { VGPU_PIXEL_FORMAT_RGB10A2_UINT,       "RGB10A2U",         VGPU_PIXEL_FORMAT_TYPE_UINT,        32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    { VGPU_PIXEL_FORMAT_RG11B10_FLOAT,      "RG11B10F",         VGPU_PIXEL_FORMAT_TYPE_FLOAT,       32,         {1, 1, 4, 1, 1},        {0, 0, 11, 11, 10, 0}},
    { VGPU_PIXEL_FORMAT_RGB9E5_FLOAT,       "RGB9E5F",          VGPU_PIXEL_FORMAT_TYPE_FLOAT,       32,         {1, 1, 4, 1, 1},        {0, 0, 9, 9, 9, 5}},

    // 64-Bit Pixel Formats
    { VGPU_PIXEL_FORMAT_RG32_UINT,          "RG32U",            VGPU_PIXEL_FORMAT_TYPE_UINT,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG32_SINT,          "RG3I",             VGPU_PIXEL_FORMAT_TYPE_SINT,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG32_FLOAT,         "RG3F",             VGPU_PIXEL_FORMAT_TYPE_FLOAT,       64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPU_PIXEL_FORMAT_RGBA16_UNORM,       "RGBA16",           VGPU_PIXEL_FORMAT_TYPE_UNORM,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_SNORM,       "RGBA16S",          VGPU_PIXEL_FORMAT_TYPE_SNORM,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_UINT,        "RGBA16U",          VGPU_PIXEL_FORMAT_TYPE_UINT,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_SINT,        "RGBA16S",          VGPU_PIXEL_FORMAT_TYPE_SINT,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_FLOAT,       "RGBA16F",          VGPU_PIXEL_FORMAT_TYPE_FLOAT,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},

    // 128-Bit Pixel Formats
    { VGPU_PIXEL_FORMAT_RGBA32_UINT,        "RGBA32U",          VGPU_PIXEL_FORMAT_TYPE_UINT,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPU_PIXEL_FORMAT_RGBA32_SINT,        "RGBA32S",          VGPU_PIXEL_FORMAT_TYPE_SINT,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPU_PIXEL_FORMAT_RGBA32_FLOAT,       "RGBA32F",          VGPU_PIXEL_FORMAT_TYPE_FLOAT,       128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},

    // Depth-stencil
    { VGPU_PIXEL_FORMAT_D16,                "D16",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       16,         {1, 1, 2, 1, 1},        {16, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D24,                "D24",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       24,         {1, 1, 3, 1, 1},        {24, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D24S8,              "D24S8",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       32,         {1, 1, 4, 1, 1},        {24, 8, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D32,                "D32",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       32,         {1, 1, 4, 1, 1},        {32, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D16F,               "D16F",             VGPU_PIXEL_FORMAT_TYPE_FLOAT,       16,         {1, 1, 2, 1, 1},        {16, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D24F,               "D24F",             VGPU_PIXEL_FORMAT_TYPE_FLOAT,       24,         {1, 1, 3, 1, 1},        {24, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D32F,               "D32F",             VGPU_PIXEL_FORMAT_TYPE_FLOAT,       32,         {1, 1, 4, 1, 1},        {32, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D32FS8,             "D32FS8",           VGPU_PIXEL_FORMAT_TYPE_FLOAT,       32,         {1, 1, 4, 1, 1},        {32, 8, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D0S8,               "D0S8",             VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {1, 1, 1, 1, 1},        {0, 8, 0, 0, 0, 0}},

    // Compressed formats
    { VGPU_PIXEL_FORMAT_BC1_UNORM,          "BC1",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC2_UNORM,          "BC2",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC3_UNORM,          "BC3",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC4_UNORM,          "BC4",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC4_SNORM,          "BC4S",             VGPU_PIXEL_FORMAT_TYPE_SNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC5_UNORM,          "BC5",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC5_SNORM,          "BC5S",             VGPU_PIXEL_FORMAT_TYPE_SNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC6HS16,            "BC6HS16",          VGPU_PIXEL_FORMAT_TYPE_FLOAT,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC6HU16,            "BC6HU16",          VGPU_PIXEL_FORMAT_TYPE_FLOAT,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_BC7_UNORM,          "BC7",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},

    // Compressed PVRTC Pixel Formats
    { VGPU_PIXEL_FORMAT_PVRTC_RGB2,         "PVRTC_RGB2",       VGPU_PIXEL_FORMAT_TYPE_UNORM,       2,          {8, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_PVRTC_RGBA2,        "PVRTC_RGBA2",      VGPU_PIXEL_FORMAT_TYPE_UNORM,       2,          {8, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_PVRTC_RGB4,         "PVRTC_RGB4",       VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_PVRTC_RGBA4,        "PVRTC_RGBA4",      VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},

    // Compressed ETC Pixel Formats
    { VGPU_PIXEL_FORMAT_ETC2_RGB8,          "ETC2_RGB8",        VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ETC2_RGB8A1,        "ETC2_RGB8A1",      VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},

    // Compressed ASTC Pixel Formats
    { VGPU_PIXEL_FORMAT_ASTC4x4,            "ASTC4x4",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ASTC5x5,            "ASTC5x5",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       6,          {5, 5, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ASTC6x6,            "ASTC6x6",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {6, 6, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ASTC8x5,            "ASTC8x5",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {8, 5, 16, 1, 1},       {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC8x6,            "ASTC8x6",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {8, 6, 16, 1, 1},       {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC8x8,            "ASTC8x8",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {8, 8, 16, 1, 1},       {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC10x10,          "ASTC10x10",        VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {10, 10, 16, 1, 1},     {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC12x12,          "ASTC12x12",        VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {12, 12, 16, 1, 1},     {0, 0, 0, 0, 0, 0} },
};

uint32_t vgpuGetFormatBitsPerPixel(VgpuPixelFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].bitsPerPixel;
}

uint32_t vgpuGetFormatBlockSize(VgpuPixelFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].compression.blockSize;
}

uint32_t vgpuGetFormatBlockWidth(VgpuPixelFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].compression.blockWidth;
}

uint32_t vgpuGetFormatBlockHeight(VgpuPixelFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].compression.blockHeight;
}

VgpuPixelFormatType vgpuGetFormatType(VgpuPixelFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].type;
}

VgpuBool32 vgpuIsDepthFormat(VgpuPixelFormat format)
{
    assert(FormatDesc[format].format == format);
    return FormatDesc[format].bits.depth > 0;
}

VgpuBool32 vgpuIsStencilFormat(VgpuPixelFormat format)
{
    assert(FormatDesc[format].format == format);
    return FormatDesc[format].bits.stencil > 0;
}

VgpuBool32 vgpuIsDepthStencilFormat(VgpuPixelFormat format)
{
    return vgpuIsDepthFormat(format) || vgpuIsStencilFormat(format);
}

VgpuBool32 vgpuIsCompressedFormat(VgpuPixelFormat format)
{
    assert(FormatDesc[format].format == format);
    return format >= VGPU_PIXEL_FORMAT_BC1_UNORM && format <= VGPU_PIXEL_FORMAT_PVRTC_RGBA4;
}

const char* vgpuGetFormatName(VgpuPixelFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].name;
}


static VgpuRendererI* s_renderer = nullptr;

VgpuBackend vgpuGetDefaultPlatformBackend() {
#if defined(_WIN32) || defined(_WIN64)
    if (vgpuIsBackendSupported(VGPU_BACKEND_D3D12, false)) {
        return VGPU_BACKEND_D3D12;
    }

    if (vgpuIsBackendSupported(VGPU_BACKEND_VULKAN, false)) {
        return VGPU_BACKEND_VULKAN;
    }

    return VGPU_BACKEND_D3D11;
#elif defined(__linux__) || defined(__ANDROID__)
    return VGPU_BACKEND_OPENGL;
#elif defined(__APPLE__) 
    return VGPU_BACKEND_METAL;
#else
    return VGPU_BACKEND_OPENGL;
#endif
}

VgpuBool32 vgpuIsBackendSupported(VgpuBackend backend, VgpuBool32 headless) {
    if (backend == VGPU_BACKEND_DEFAULT)
    {
        backend = vgpuGetDefaultPlatformBackend();
    }

    switch (backend)
    {
    case VGPU_BACKEND_NULL:
        return VGPU_TRUE;
    case VGPU_BACKEND_VULKAN:
#if VGPU_VULKAN
        return vgpuIsVkSupported(headless);
#else
        return VGPU_FALSE;
#endif

    case VGPU_BACKEND_D3D12:
#if VGPU_D3D12
        return vgpuIsD3D12Supported(headless);
#else
        return VGPU_FALSE;
#endif

    case VGPU_BACKEND_OPENGL:
#if VGPU_OPENGL
        return VGPU_TRUE;
#else
        return VGPU_FALSE;
#endif
    default:
        return VGPU_FALSE;
    }
}

VgpuResult vgpuInitialize(const char* applicationName, const VgpuDescriptor* descriptor) {
    if (s_renderer != nullptr) {
        return VGPU_ALREADY_INITIALIZED;
    }

    VgpuBackend backend = descriptor->preferredBackend;
    if (backend == VGPU_BACKEND_DEFAULT)
    {
        backend = vgpuGetDefaultPlatformBackend();
    }

    VgpuRendererI* renderer = nullptr;
    switch (backend)
    {
    case VGPU_BACKEND_NULL:
        break;
    case VGPU_BACKEND_VULKAN:
#if VGPU_D3D12
        renderer = vgpuCreateVkBackend();
#else
        //ALIMER_LOGERROR("D3D12 backend is not supported");
#endif
        break;
    case VGPU_BACKEND_D3D12:
#if VGPU_D3D12
        renderer = vgpuCreateD3D12Backend();
#else
        //ALIMER_LOGERROR("D3D12 backend is not supported");
#endif
        break;
    case VGPU_BACKEND_OPENGL:
        break;
    default:
        break;
    }

    if (renderer != nullptr)
    {
        s_renderer = renderer;
        return renderer->initialize(applicationName, descriptor);
    }

    //s_logCallback = descriptor->logCallback;
    return VGPU_ERROR_INITIALIZATION_FAILED;
}

void vgpuShutdown()
{
    if (s_renderer != nullptr)
    {
        s_renderer->shutdown();
        delete s_renderer;
        s_renderer = nullptr;
    }
}

VgpuResult vgpuBeginFrame() {
    return s_renderer->beginFrame();
}

VgpuResult vgpuEndFrame() {
    return s_renderer->endFrame();
}

VgpuResult vgpuWaitIdle() {
    return s_renderer->waitIdle();
}

VgpuFramebuffer vgpuGetCurrentFramebuffer() {
    return nullptr;
}

/* Sampler */
VgpuSampler vgpuCreateSampler(const VgpuSamplerDescriptor* descriptor) {
    VGPU_ASSERT(descriptor);
    return s_renderer->createSampler(descriptor);
}

void vgpuDestroySampler(VgpuSampler sampler) {
    VGPU_ASSERT(sampler.id != VGPU_INVALID_ID);
    s_renderer->destroySampler(sampler);
}

/* CommandBuffer */
VgpuCommandBuffer vgpuCreateCommandBuffer(const VgpuCommandBufferDescriptor* descriptor) {
    VGPU_ASSERT(descriptor);
    return s_renderer->createCommandBuffer(descriptor);
}

void vgpuCmdBeginDefaultRenderPass(VgpuCommandBuffer commandBuffer, VgpuColor clearColor, float clearDepth, uint8_t clearStencil) {
    VGPU_ASSERT(commandBuffer);
    s_renderer->cmdBeginDefaultRenderPass(commandBuffer, clearColor, clearDepth, clearStencil);
}

void vgpuCmdBeginRenderPass(VgpuCommandBuffer commandBuffer, VgpuFramebuffer framebuffer) {
    VGPU_ASSERT(commandBuffer);
    VGPU_ASSERT(framebuffer);
    s_renderer->cmdBeginRenderPass(commandBuffer, framebuffer);
}

void vgpuCmdEndRenderPass(VgpuCommandBuffer commandBuffer) {
    s_renderer->cmdEndRenderPass(commandBuffer);
}
