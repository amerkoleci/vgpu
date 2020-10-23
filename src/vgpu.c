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

#include "vgpu_driver.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

/* Allocation */
void* vgpu_default_allocate_memory(void* user_data, size_t size) {
    VGPU_UNUSED(user_data);
    return malloc(size);
}

void* vgpu_default_allocate_cleard_memory(void* user_data, size_t size) {
    VGPU_UNUSED(user_data);
    void* mem = malloc(size);
    memset(mem, 0, size);
    return mem;
}

void vgpu_default_free_memory(void* user_data, void* ptr) {
    VGPU_UNUSED(user_data);
    free(ptr);
}

const vgpu_allocation_callbacks vgpu_default_alloc_cb = {
    NULL,
    vgpu_default_allocate_memory,
    vgpu_default_allocate_cleard_memory,
    vgpu_default_free_memory
};

const vgpu_allocation_callbacks* vgpu_alloc_cb = &vgpu_default_alloc_cb;
void* vgpu_allocation_user_data = NULL;

void vgpu_set_allocation_callbacks(const vgpu_allocation_callbacks* callbacks) {
    if (callbacks == NULL) {
        vgpu_alloc_cb = &vgpu_default_alloc_cb;
    }
    else {
        vgpu_alloc_cb = callbacks;
    }
}

#define VGPU_MAX_LOG_MESSAGE (4096)
static vgpu_log_callback s_log_function = NULL;
static void* s_log_user_data = NULL;

void vgpuSetLogCallback(vgpu_log_callback callback, void* user_data) {
    s_log_function = callback;
    s_log_user_data = user_data;
}

void vgpu_log(vgpu_log_level level, const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        s_log_function(s_log_user_data, level, message);
        va_end(args);
    }
}

void vgpu_log_error(const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        s_log_function(s_log_user_data, VGPU_LOG_LEVEL_ERROR, message);
        va_end(args);
    }
}

void vgpu_log_info(const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        s_log_function(s_log_user_data, VGPU_LOG_LEVEL_INFO, message);
        va_end(args);
    }
}

static const vgpu_driver* drivers[] = {
#if defined(VGPU_DRIVER_D3D11) 
    &d3d11_driver,
#endif
#if defined(VGPU_DRIVER_D3D12) && defined(TODO_D3D12)
    &d3d12_driver,
#endif
#if defined(VGPU_DRIVER_VULKAN)
    &vulkan_driver,
#endif
#if defined(VGPU_DRIVER_OPENGL)
    &gl_driver,
#endif
    NULL
};

VGPUDevice vgpuCreateDevice(vgpu_backend_type preferred_backend, const vgpu_device_info* info)
{
    VGPUDevice device = NULL;
    if (preferred_backend == VGPU_BACKEND_TYPE_DEFAULT || preferred_backend == VGPU_BACKEND_TYPE_COUNT) {
        for (uint32_t i = 0; _vgpu_count_of(drivers); i++) {
            if (drivers[i]->is_supported()) {
                device = drivers[i]->createDevice(info);
                break;
            }
        }
}
    else {
        for (uint32_t i = 0; _vgpu_count_of(drivers); i++) {
            if (drivers[i]->backendType == preferred_backend && drivers[i]->is_supported()) {
                device = drivers[i]->createDevice(info);
                break;
            }
        }
    }

    return device;
}

void vgpuDestroyDevice(VGPUDevice device) {
    if (device == NULL) {
        return;
    }

    device->destroy(device);
}

void vgpuGetDeviceCaps(VGPUDevice device, VGPUDeviceCaps* caps)
{
    VGPU_ASSERT(device);
    device->get_caps(device->renderer, caps);
}

VGPUTextureFormat vgpuGetDefaultDepthFormat(VGPUDevice device)
{
    VGPU_ASSERT(device);
    return device->getDefaultDepthFormat(device->renderer);
}

VGPUTextureFormat vgpuGetDefaultDepthStencilFormat(VGPUDevice device)
{
    VGPU_ASSERT(device);
    return device->getDefaultDepthStencilFormat(device->renderer);
}

vgpu_bool vgpuBeginFrame(VGPUDevice device) {
    return device->frame_begin(device->renderer);
}

void vgpuEndFrame(VGPUDevice device) {
    device->frame_end(device->renderer);
}

VGPUTexture vgpuGetBackbufferTexture(VGPUDevice device) {
    return device->getBackbufferTexture(device->renderer, 0);
}

/* Texture */
static VGPUTextureDescription _vgpu_texture_desc_defaults(const VGPUTextureDescription* desc) {
    VGPUTextureDescription def = *desc;
    def.type = _vgpu_def(desc->type, VGPU_TEXTURE_TYPE_2D);
    def.width = _vgpu_def(desc->width, 1u);
    def.height = _vgpu_def(desc->height, 1u);
    def.depth = _vgpu_def(desc->depth, 1u);
    def.format = _vgpu_def(def.format, VGPU_TEXTURE_FORMAT_RGBA8);
    def.mipLevelCount = _vgpu_def(def.mipLevelCount, 1);
    def.sampleCount = _vgpu_def(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDescription* desc)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(desc);

    VGPUTextureDescription desc_def = _vgpu_texture_desc_defaults(desc);
    return device->createTexture(device->renderer, &desc_def);
}

void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(texture);

    device->destroyTexture(device->renderer, texture);
}

/* Buffer */
VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDescriptor* descriptor)
{
    return device->bufferCreate(device->renderer, descriptor);
}

void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer)
{
    device->bufferDestroy(device->renderer, buffer);
}

/* Sampler */
VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDescriptor* descriptor)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(descriptor);

    return device->samplerCreate(device->renderer, descriptor);
}

void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler)
{
    VGPU_ASSERT(device);
    //VGPU_ASSERT(sampler.id != VGPU_INVALID_ID);

    device->samplerDestroy(device->renderer, sampler);
}

/* Commands */
void vgpuCmdBeginRenderPass(VGPUDevice device, const VGPURenderPassDescription* desc)
{
    device->cmdBeginRenderPass(device->renderer, desc);
}

void vgpuCmdEndRenderPass(VGPUDevice device) {
    device->cmdEndRenderPass(device->renderer);
}

/* Pixel Format */
typedef struct VgpuPixelFormatDesc
{
    VGPUTextureFormat       format;
    VGPUTextureFormatType   type;
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
    // format                                  type                                bpp         compression             bits
    { VGPU_TEXTURE_FORMAT_UNDEFINED,           VGPUTextureFormatType_Unknown,     0,          {0, 0, 0, 0, 0},        {0, 0, 0, 0, 0, 0}},
    // 8-bit pixel formats
    { VGPUTextureFormat_R8UNorm,               VGPUTextureFormatType_Unorm,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8SNorm,               VGPUTextureFormatType_Snorm,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8UInt,                VGPUTextureFormatType_Uint,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8SInt,                VGPUTextureFormatType_Sint,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},

    // 16-bit pixel formats
    { VGPUTextureFormat_R16UNorm,              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16SNorm,              VGPUTextureFormatType_Snorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16UInt,               VGPUTextureFormatType_Uint,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16SInt,               VGPUTextureFormatType_Sint,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Float,              VGPUTextureFormatType_Float,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_RG8UNorm,              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8SNorm,              VGPUTextureFormatType_Snorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8UInt,               VGPUTextureFormatType_Uint,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8SInt,               VGPUTextureFormatType_Sint,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},

    // Packed 16-bit pixel formats
    //{ VGPU_PIXEL_FORMAT_R5G6B5_UNORM,        VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 5, 6, 5, 0}},
    //{ VGPU_PIXEL_FORMAT_RGBA4_UNORM,         VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 4, 4, 4, 4}},

    // 32-bit pixel formats
    { VGPUTextureFormat_R32UInt,                VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPUTextureFormat_R32SInt,                VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPUTextureFormat_R32Float,                VGPUTextureFormatType_Float,        32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    //{ VGPUTextureFormat_RG16Unorm,                VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    //{ VGPUTextureFormat_RG16Snorm,               VGPUTextureFormatType_Snorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16UInt,                VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16SInt,                VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16Float,                VGPUTextureFormatType_Float,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_TEXTURE_FORMAT_RGBA8,                VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_TEXTURE_FORMAT_RGBA8_SRGB,            VGPUTextureFormatType_UnormSrgb,    32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8SNorm,               VGPUTextureFormatType_Snorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8UInt,               VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8SInt,               VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_TEXTURE_FORMAT_BGRA8,                VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_TEXTURE_FORMAT_BGRA8_SRGB,            VGPUTextureFormatType_UnormSrgb,    32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},

    // Packed 32-Bit Pixel formats
    { VGPU_TEXTURE_FORMAT_RGB10A2,            VGPUTextureFormatType_Unorm,       32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    //{ VGPU_PIXEL_FORMAT_RGB10A2_UINT,        VGPUTextureFormatType_Uint,        32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    { VGPU_TEXTURE_FORMAT_RG11B10F,           VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {0, 0, 11, 11, 10, 0}},
    //{ VGPU_PIXEL_FORMAT_RGB9E5_FLOAT,        VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {0, 0, 9, 9, 9, 5}},

    // 64-Bit Pixel Formats
    { VGPUTextureFormat_RG32UInt,                 VGPUTextureFormatType_Uint,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPUTextureFormat_RG32SInt,                 VGPUTextureFormatType_Sint,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPUTextureFormat_RG32Float,                VGPUTextureFormatType_Float,       64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    //{ VGPU_PIXEL_FORMAT_RGBA16_UNORM,           VGPUTextureFormatType_Unorm,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    //{ VGPU_PIXEL_FORMAT_RGBA16_SNORM,           VGPUTextureFormatType_Snorm,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16UInt,                 VGPUTextureFormatType_Uint,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16SInt,                 VGPUTextureFormatType_Sint,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16Float,             VGPUTextureFormatType_Float,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},

    // 128-Bit Pixel Formats
    { VGPUTextureFormat_RGBA32UInt,               VGPUTextureFormatType_Uint,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPUTextureFormat_RGBA32SInt,               VGPUTextureFormatType_Sint,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPUTextureFormat_RGBA32Float,               VGPUTextureFormatType_Float,       128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},

    // Depth-stencil
    { VGPUTextureFormat_Depth16UNorm,           VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {16, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_Depth32Float,           VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {32, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_Depth24UnormStencil8,   VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {24, 8, 0, 0, 0, 0}},
    { VGPUTextureFormat_Depth32FloatStencil8,   VGPUTextureFormatType_Float,       48,         {1, 1, 4, 1, 1},        {32, 8, 0, 0, 0, 0}},

    // Compressed formats
    { VGPU_TEXTURE_FORMAT_BC1,             VGPUTextureFormatType_Unorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC1_SRGB,         VGPUTextureFormatType_UnormSrgb,    4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC2,             VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC2_SRGB,         VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC3,             VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC3_SRGB,         VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC4,                VGPUTextureFormatType_Unorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC4S,                VGPUTextureFormatType_Snorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC5,               VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC5S,               VGPUTextureFormatType_Snorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC6H_UFLOAT,            VGPUTextureFormatType_Float,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC6H_SFLOAT,            VGPUTextureFormatType_Float,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC7,             VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_TEXTURE_FORMAT_BC7_SRGB,         VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},

    /*
    // Compressed PVRTC Pixel Formats
    { VGPU_PIXEL_FORMAT_PVRTC_RGB2,             "PVRTC_RGB2",           VGPU_PIXEL_FORMAT_TYPE_UNORM,       2,          {8, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_PVRTC_RGBA2,            "PVRTC_RGBA2",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       2,          {8, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_PVRTC_RGB4,             "PVRTC_RGB4",           VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_PVRTC_RGBA4,            "PVRTC_RGBA4",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 2, 2},        {0, 0, 0, 0, 0, 0}},

    // Compressed ETC Pixel Formats
    { VGPU_PIXEL_FORMAT_ETC2_RGB8,              "ETC2_RGB8",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ETC2_RGB8A1,            "ETC2_RGB8A1",          VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},

    // Compressed ASTC Pixel Formats
    { VGPU_PIXEL_FORMAT_ASTC4x4,                "ASTC4x4",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ASTC5x5,                "ASTC5x5",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       6,          {5, 5, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ASTC6x6,                "ASTC6x6",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {6, 6, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_ASTC8x5,                "ASTC8x5",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       4,          {8, 5, 16, 1, 1},       {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC8x6,                "ASTC8x6",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {8, 6, 16, 1, 1},       {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC8x8,                "ASTC8x8",              VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {8, 8, 16, 1, 1},       {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC10x10,              "ASTC10x10",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {10, 10, 16, 1, 1},     {0, 0, 0, 0, 0, 0} },
    { VGPU_PIXEL_FORMAT_ASTC12x12,              "ASTC12x12",            VGPU_PIXEL_FORMAT_TYPE_UNORM,       3,          {12, 12, 16, 1, 1},     {0, 0, 0, 0, 0, 0} },*/
};

uint32_t vgpu_get_format_bits_per_pixel(VGPUTextureFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].bitsPerPixel;
}

uint32_t vgpu_get_format_block_size(VGPUTextureFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].compression.blockSize;
}

uint32_t vgpuGetFormatBlockWidth(VGPUTextureFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].compression.blockWidth;
}

uint32_t vgpuGetFormatBlockHeight(VGPUTextureFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].compression.blockHeight;
}

VGPUTextureFormatType vgpuGetFormatType(VGPUTextureFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].type;
}

vgpu_bool vgpuIsDepthFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return FormatDesc[format].bits.depth > 0;
}

vgpu_bool vgpuIsStencilFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return FormatDesc[format].bits.stencil > 0;
}

vgpu_bool vgpuIsDepthStencilFormat(VGPUTextureFormat format)
{
    return vgpuIsDepthFormat(format) || vgpuIsStencilFormat(format);
}

vgpu_bool vgpuIsCompressedFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return format >= VGPU_TEXTURE_FORMAT_BC1 && format <= VGPU_TEXTURE_FORMAT_BC7_SRGB;
}
