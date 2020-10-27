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

void vgpuSetAllocationCallbacks(const vgpu_allocation_callbacks* callbacks) {
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

void vgpuLog(VGPULogLevel level, const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        s_log_function(s_log_user_data, level, message);
        va_end(args);
    }
}

void vgpuLogError(const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        s_log_function(s_log_user_data, VGPULogLevel_Error, message);
        va_end(args);
    }
}

void vgpuLogInfo(const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        s_log_function(s_log_user_data, VGPULogLevel_Info, message);
        va_end(args);
    }
}

static const VGPU_Driver* drivers[] = {
#if defined(VGPU_DRIVER_D3D12)
    &D3D12_Driver,
#endif
#if defined(VGPU_DRIVER_D3D11) 
    &D3D11_Driver,
#endif

#if defined(VGPU_DRIVER_VULKAN)
    &vulkan_driver,
#endif
#if defined(VGPU_DRIVER_OPENGL)
    &gl_driver,
#endif
    NULL
};

VGPUDevice vgpuCreateDevice(VGPUBackendType preferredBackend, const VGPUDeviceDescriptor* descriptor)
{
    VGPU_ASSERT(descriptor);

    VGPUDevice device = NULL;
    if (preferredBackend == VGPUBackendType_Count) {
        for (uint32_t i = 0; _vgpu_count_of(drivers); i++) {
            if (drivers[i]->is_supported()) {
                device = drivers[i]->createDevice(descriptor);
                break;
            }
        }
    }
    else {
        for (uint32_t i = 0; _vgpu_count_of(drivers); i++) {
            if (drivers[i]->backendType == preferredBackend && drivers[i]->is_supported()) {
                device = drivers[i]->createDevice(descriptor);
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
    def.format = _vgpu_def(def.format, VGPUTextureFormat_RGBA8Unorm);
    def.mipLevelCount = _vgpu_def(def.mipLevelCount, 1);
    def.sampleCount = _vgpu_def(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDescription* descriptor)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(descriptor);

    VGPUTextureDescription desc_def = _vgpu_texture_desc_defaults(descriptor);
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

    VGPU_ASSERT(device);
    VGPU_ASSERT(descriptor);

    return device->createBuffer(device->renderer, descriptor);
}

void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer)
{
    device->destroyBuffer(device->renderer, buffer);
}

/* Sampler */
VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDescriptor* descriptor)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(descriptor);

    return device->createSampler(device->renderer, descriptor);
}

void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(sampler);

    device->destroySampler(device->renderer, sampler);
}

/* Shader */
VGPUShaderModule vgpuCreateShaderModule(VGPUDevice device, const VGPUShaderModuleDescriptor* descriptor)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(descriptor);

    return device->createShaderModule(device->renderer, descriptor);
}

void vgpuDestroyShaderModule(VGPUDevice device, VGPUShaderModule shader)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(shader);

    device->destroyShaderModule(device->renderer, shader);
}

/* Pipeline */
VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDescriptor* descriptor)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(descriptor);

    return NULL;
}

VGPU_API void vgpuDestroyPipeline(VGPUDevice device, VGPUPipeline pipeline)
{
    VGPU_ASSERT(device);
    VGPU_ASSERT(pipeline);

    //device->destroyPipeline(device->renderer, pipeline);
}

/* Commands */
void vgpuCmdBeginRenderPass(VGPUDevice device, const VGPURenderPassDescriptor* descriptor)
{
    VGPU_ASSERT(descriptor);

    device->cmdBeginRenderPass(device->renderer, descriptor);
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
    { VGPUTextureFormat_R8Unorm,               VGPUTextureFormatType_Unorm,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8Snorm,               VGPUTextureFormatType_SNorm,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8Uint,                VGPUTextureFormatType_Uint,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8Sint,                VGPUTextureFormatType_Sint,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},

    // 16-bit pixel formats
    { VGPUTextureFormat_R16Unorm,              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Snorm,              VGPUTextureFormatType_SNorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Uint,               VGPUTextureFormatType_Uint,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Sint,               VGPUTextureFormatType_Sint,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Float,              VGPUTextureFormatType_Float,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_RG8Unorm,              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8Snorm,              VGPUTextureFormatType_SNorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8Uint,               VGPUTextureFormatType_Uint,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8Sint,               VGPUTextureFormatType_Sint,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},

    // Packed 16-bit pixel formats
    //{ VGPU_PIXEL_FORMAT_R5G6B5_UNORM,        VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 5, 6, 5, 0}},
    //{ VGPU_PIXEL_FORMAT_RGBA4_UNORM,         VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 4, 4, 4, 4}},

    // 32-bit pixel formats
    { VGPUTextureFormat_R32UInt,                VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPUTextureFormat_R32SInt,                VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPUTextureFormat_R32Float,               VGPUTextureFormatType_Float,        32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPUTextureFormat_RG16UNorm,              VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16SNorm,              VGPUTextureFormatType_SNorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16UInt,               VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16SInt,               VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RG16Float,              VGPUTextureFormatType_Float,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPUTextureFormat_RGBA8Unorm,             VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8UnormSrgb,         VGPUTextureFormatType_UnormSrgb,    32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8Snorm,             VGPUTextureFormatType_SNorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8Uint,              VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_RGBA8Sint,              VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_BGRA8Unorm,             VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPUTextureFormat_BGRA8UnormSrgb,         VGPUTextureFormatType_UnormSrgb,    32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},

    // Packed 32-Bit formats
    { VGPUTextureFormat_RGB10A2Unorm,       VGPUTextureFormatType_Unorm,       32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    { VGPUTextureFormat_RG11B10Float,       VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {0, 0, 11, 11, 10, 0}},
    { VGPUTextureFormat_RGB9E5Float,        VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {0, 0, 9, 9, 9, 5}},

    // 64-Bit Pixel Formats
    { VGPUTextureFormat_RG32Uint,           VGPUTextureFormatType_Uint,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPUTextureFormat_RG32Sint,           VGPUTextureFormatType_Sint,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPUTextureFormat_RG32Float,          VGPUTextureFormatType_Float,       64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPUTextureFormat_RGBA16Unorm,        VGPUTextureFormatType_Unorm,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16Snorm,        VGPUTextureFormatType_SNorm,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16Uint,         VGPUTextureFormatType_Uint,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16Sint,         VGPUTextureFormatType_Sint,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPUTextureFormat_RGBA16Float,        VGPUTextureFormatType_Float,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},

    // 128-Bit Pixel Formats
    { VGPUTextureFormat_RGBA32Uint,         VGPUTextureFormatType_Uint,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPUTextureFormat_RGBA32Sint,         VGPUTextureFormatType_Sint,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPUTextureFormat_RGBA32Float,        VGPUTextureFormatType_Float,       128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},

    // Depth-stencil
    { VGPUTextureFormat_Depth16Unorm,           VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {16, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_Depth32Float,           VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {32, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_Depth24UnormStencil8,   VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {24, 8, 0, 0, 0, 0}},
    { VGPUTextureFormat_Depth32FloatStencil8,   VGPUTextureFormatType_Float,       48,         {1, 1, 4, 1, 1},        {32, 8, 0, 0, 0, 0}},

    // Compressed formats
    { VGPUTextureFormat_BC1RGBAUnorm,           VGPUTextureFormatType_Unorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC1RGBAUnormSrgb,       VGPUTextureFormatType_UnormSrgb,    4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC2RGBAUnorm,           VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC2RGBAUnormSrgb,       VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC3RGBAUnorm,           VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC3RGBAUnormSrgb,       VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC4RUnorm,              VGPUTextureFormatType_Unorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC4RSnorm,              VGPUTextureFormatType_SNorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC5RGUnorm,             VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC5RGSnorm,             VGPUTextureFormatType_SNorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC6HRGBUFloat,          VGPUTextureFormatType_Float,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC6HRGBFloat,           VGPUTextureFormatType_Float,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC7RGBAUnorm,           VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC7RGBAUnormSrgb,       VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},

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

uint32_t vgpuGetFormatBitsPerPixel(VGPUTextureFormat format)
{
    assert(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].bitsPerPixel;
}

uint32_t vgpuGetFormatBlockSize(VGPUTextureFormat format)
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
    return format >= VGPUTextureFormat_BC1RGBAUnorm && format <= VGPUTextureFormat_BC7RGBAUnormSrgb;
}

vgpu_bool vgpuIsSrgbFormat(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return FormatDesc[(uint32_t)format].type == VGPUTextureFormatType_UnormSrgb;
}

VGPUTextureFormat vgpuSRGBToLinearFormat(VGPUTextureFormat format)
{
    switch (format)
    {
    case VGPUTextureFormat_BC1RGBAUnormSrgb:
        return VGPUTextureFormat_BC1RGBAUnorm;
    case VGPUTextureFormat_BC2RGBAUnormSrgb:
        return VGPUTextureFormat_BC2RGBAUnorm;
    case VGPUTextureFormat_BC3RGBAUnormSrgb:
        return VGPUTextureFormat_BC3RGBAUnorm;
    case VGPUTextureFormat_BGRA8UnormSrgb:
        return VGPUTextureFormat_BGRA8Unorm;
    case VGPUTextureFormat_RGBA8UnormSrgb:
        return VGPUTextureFormat_RGBA8Unorm;
    case VGPUTextureFormat_BC7RGBAUnormSrgb:
        return VGPUTextureFormat_BC7RGBAUnorm;
    default:
        VGPU_ASSERT(vgpuIsSrgbFormat(format) == false);
        return format;
    }
}
