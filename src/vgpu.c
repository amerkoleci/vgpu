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

#include "vgpu_backend.h"
#include <stdarg.h>

#if defined(__ANDROID__)
#   include <android/log.h>
#elif TARGET_OS_IOS || TARGET_OS_TV
#   include <sys/syslog.h>
#elif TARGET_OS_MAC || defined(__linux__)
#   include <unistd.h>
#elif defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <Windows.h>
#   include <strsafe.h>
#elif defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#endif

#define VGPU_MAX_LOG_MESSAGE (4096)
static const char* vgpu_log_priority_prefixes[VGPU_LOG_LEVEL_COUNT] = {
    NULL,
    "VERBOSE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "CRITICAL"
};

static void vgpu_default_log_callback(void* user_data, vgpu_log_level level, const char* message);
static vgpu_log_callback s_log_function = vgpu_default_log_callback;
static void* s_log_user_data = NULL;

void vgpu_get_log_callback_function(vgpu_log_callback* callback, void** user_data) {
    if (callback) {
        *callback = s_log_function;
    }
    if (user_data) {
        *user_data = s_log_user_data;
    }
}

void vgpu_set_log_callback_function(vgpu_log_callback callback, void* user_data) {
    s_log_function = callback;
    s_log_user_data = user_data;
}

void vgpu_default_log_callback(void* user_data, vgpu_log_level level, const char* message) {
#if defined(_WIN32)
    size_t length = strlen(vgpu_log_priority_prefixes[level]) + 2 + strlen(message) + 1 + 1 + 1;
    char* output = VGPU_ALLOCA(char, length);
    snprintf(output, length, "%s: %s\r\n", vgpu_log_priority_prefixes[level], message);

    const int buffer_size = MultiByteToWideChar(CP_UTF8, 0, output, (int)length, nullptr, 0);
    if (buffer_size == 0)
        return;

    WCHAR* buffer = VGPU_ALLOCA(WCHAR, buffer_size);
    if (MultiByteToWideChar(CP_UTF8, 0, message, -1, buffer, buffer_size) == 0)
        return;

    OutputDebugStringW(buffer);

#   if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG)
    HANDLE handle;
    switch (level)
    {
    case VGPU_LOG_LEVEL_ERROR:
    case VGPU_LOG_LEVEL_WARN:
        handle = GetStdHandle(STD_ERROR_HANDLE);
        break;
    default:
        handle = GetStdHandle(STD_OUTPUT_HANDLE);
        break;
    }

    WriteConsoleW(handle, buffer, (DWORD)wcslen(buffer), NULL, NULL);
#   endif

#endif
}

void vgpu_log(vgpu_log_level level, const char* message) {
    if (s_log_function) {
        s_log_function(s_log_user_data, level, message);
    }
}

void vgpu_log_message_v(vgpu_log_level level, const char* format, va_list args) {
    if (s_log_function) {
        char message[VGPU_MAX_LOG_MESSAGE];
        vsnprintf(message, VGPU_MAX_LOG_MESSAGE, format, args);
        vgpu_log(level, message);
    }
}

void vgpu_log_format(vgpu_log_level level, const char* format, ...) {
    if (s_log_function) {
        va_list args;
        va_start(args, format);
        vgpu_log_message_v(level, format, args);
        va_end(args);
    }
}

void vgpu_log_error(const char* message) {
    vgpu_log(VGPU_LOG_LEVEL_ERROR, message);
}

void vgpu_log_error_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vgpu_log_message_v(VGPU_LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

VGPUBackendType vgpuGetDefaultPlatformBackend(void) {
#if defined(_WIN32) || defined(_WIN64)
    if (vgpuIsBackendSupported(VGPUBackendType_D3D12)) {
        return VGPUBackendType_D3D12;
    }

    if (vgpuIsBackendSupported(VGPUBackendType_Vulkan)) {
        return VGPUBackendType_Vulkan;
    }

    if (vgpuIsBackendSupported(VGPUBackendType_D3D11)) {
        return VGPUBackendType_D3D11;
    }

    if (vgpuIsBackendSupported(VGPUBackendType_OpenGL)) {
        return VGPUBackendType_OpenGL;
    }

    return VGPUBackendType_Null;
#elif defined(__linux__) || defined(__ANDROID__)
    return VGPUBackendType_OpenGL;
#elif defined(__APPLE__)
    return VGPUBackendType_Vulkan;
#else
    return VGPUBackendType_OpenGL;
#endif
}

bool vgpuIsBackendSupported(VGPUBackendType backend) {
    if (backend == VGPUBackendType_Force32) {
        backend = vgpuGetDefaultPlatformBackend();
    }

    switch (backend)
    {
    case VGPUBackendType_Null:
        return true;
#if defined(VGPU_VULKAN)
    case VGPUBackendType_Vulkan:
        return vgpu_vk_supported();
#endif
#if defined(VGPU_D3D12)
    case VGPU_BACKEND_DIRECT3D12:
        return vgpu_d3d12_supported();
#endif /* defined(VGPU_D3D12_BACKEND) */

#if defined(VGPU_D3D11)
    case VGPUBackendType_D3D11:
        return vgpu_d3d11_supported();
#endif

#if defined(VGPU_BACKEND_GL)
    case VGPU_BACKEND_OPENGL:
        return agpu_gl_supported();
#endif // defined(AGPU_BACKEND_GL)

    default:
        return false;
    }
}

VGPUDevice vgpuDeviceCreate(const char* app_name, const VGpuDeviceDescriptor* descriptor) {
    VGPUBackendType backend = descriptor->preferredBackend;
    if (backend == VGPUBackendType_Force32) {
        backend = vgpuGetDefaultPlatformBackend();
    }

    VGPUDevice device = nullptr;
    switch (backend)
    {
    case VGPUBackendType_Null:
        break;

#if defined(VGPU_VULKAN)
    case VGPUBackendType_Vulkan:
        device = vk_create_device();
        break;
#endif

#if defined(VGPU_D3D11)
    case VGPUBackendType_D3D11:
        device = d3d11_create_device();
        break;
#endif
    }

    if (device == nullptr ||
        !device->init(device, app_name, descriptor)) {
        return nullptr;
    }

    return device;
}

void vgpuDeviceDestroy(VGPUDevice device) {
    if (device == nullptr) {
        return;
    }

    device->destroy(device);
}

VGPUBackendType vgpuGetBackend(VGPUDevice device) {
    return device->getBackend();
}

vgpu_features vgpu_get_features(VGPUDevice device) {
    return device->get_features(device->renderer);
}

vgpu_limits vgpu_get_limits(VGPUDevice device) {
    return device->get_limits(device->renderer);
}

vgpu_render_pass* vgpu_get_default_render_pass(VGPUDevice device) {
    return device->get_default_render_pass(device->renderer);
}

void vgpu_begin_frame(VGPUDevice device) {
    device->begin_frame(device->renderer);
}

void vgpu_end_frame(VGPUDevice device) {
    device->end_frame(device->renderer);
}

VGPUTexture vgpuTextureCreate(VGPUDevice device, const VGPUTextureDescriptor* descriptor, const void* initial_data) {
    return device->textureCreate(device->renderer, descriptor, initial_data);
}

VGPUTexture vgpuTextureCreateExternal(VGPUDevice device, const VGPUTextureDescriptor* descriptor, void* handle) {
    return device->textureCreateExternal(device->renderer, descriptor, handle);
}

void vgpuTextureDestroy(VGPUDevice device, VGPUTexture texture) {
    device->textureDestroy(device->renderer, texture);
}

VGPUSampler vgpuSamplerCreate(VGPUDevice device, const VGPUSamplerDescriptor* descriptor) {
    return device->samplerCreate(device->renderer, descriptor);
}

void vgpuSamplerDestroy(VGPUDevice device, VGPUSampler sampler) {
    device->samplerDestroy(device->renderer, sampler);
}

vgpu_render_pass* vgpu_render_pass_create(const vgpu_render_pass_desc* desc) {
    return nullptr;
    //return gpu_device->render_pass_create(gpu_device->renderer, desc);
}

void vgpu_render_pass_destroy(vgpu_render_pass* render_pass) {
    //gpu_device->render_pass_destroy(gpu_device->renderer, render_pass);
}

void vgpu_render_pass_get_extent(vgpu_render_pass* render_pass, uint32_t* width, uint32_t* height) {
    //gpu_device->render_pass_get_extent(gpu_device->renderer, render_pass, width, height);
}

/* Commands */
VGPU_EXPORT void vgpu_cmd_begin_render_pass(const vgpu_begin_render_pass_desc* begin_pass_desc) {
    VGPU_ASSERT(begin_pass_desc);
    //gpu_device->cmd_begin_render_pass(gpu_device->renderer, begin_pass_desc);
}

void vgpu_cmd_end_render_pass(void) {
    //gpu_device->cmd_end_render_pass(gpu_device->renderer);
}

/* Pixel Format */
typedef struct VgpuPixelFormatDesc
{
    VGPUTextureFormat       format;
    const char*             name;
    VGPUTextureFormatType    type;
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
    // format                                   name,                   type                                bpp         compression             bits
    { VGPUTextureFormat_Undefined,              "Undefined",            VGPUTextureFormatType_Unknown,     0,          {0, 0, 0, 0, 0},        {0, 0, 0, 0, 0, 0}},
    // 8-bit pixel formats
    { VGPUTextureFormat_R8Unorm,               "R8Unorm",               VGPUTextureFormatType_Unorm,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8Snorm,               "R8Snorm",               VGPUTextureFormatType_Snorm,       8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8Uint,                "R8Uint",                VGPUTextureFormatType_Uint,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},
    { VGPUTextureFormat_R8Sint,                "R8Sint",                VGPUTextureFormatType_Sint,        8,          {1, 1, 1, 1, 1},        {0, 0, 8, 0, 0, 0}},

    // 16-bit pixel formats
    { VGPUTextureFormat_R16Unorm,              "R16Unorm",              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Snorm,              "R16Snorm",              VGPUTextureFormatType_Snorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Uint,               "R16Uint",               VGPUTextureFormatType_Uint,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Sint,               "R16Sint",               VGPUTextureFormatType_Sint,        16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_R16Float,              "R16Float",              VGPUTextureFormatType_Float,       16,         {1, 1, 2, 1, 1},        {0, 0, 16, 0, 0, 0}},
    { VGPUTextureFormat_RG8Unorm,              "RG8Unorm",              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8Snorm,              "RG8Snorm",              VGPUTextureFormatType_Snorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8Uint,               "RG8Uint",               VGPUTextureFormatType_Uint,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},
    { VGPUTextureFormat_RG8Sint,               "RG8Sint",               VGPUTextureFormatType_Sint,        16,         {1, 1, 2, 1, 1},        {0, 0, 8, 8, 0, 0}},

    // Packed 16-bit pixel formats
    //{ VGPU_PIXEL_FORMAT_R5G6B5_UNORM,           "R5G6B5",             VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 5, 6, 5, 0}},
    //{ VGPU_PIXEL_FORMAT_RGBA4_UNORM,            "RGBA4",              VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {0, 0, 4, 4, 4, 4}},

    // 32-bit pixel formats
    { VGPU_PIXEL_FORMAT_R32_UINT,               "R32U",                 VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R32_SINT,               "R32I",                 VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_R32_FLOAT,              "R32F",                 VGPUTextureFormatType_Float,        32,         {1, 1, 4, 1, 1},        {0, 0, 32, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_UNORM,             "RG16",                 VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_SNORM,             "RG16S",                VGPUTextureFormatType_Snorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_UINT,              "RG16U",                VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_SINT,              "RG16I",                VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG16_FLOAT,             "RG16F",                VGPUTextureFormatType_Float,        32,         {1, 1, 4, 1, 1},        {0, 0, 16, 16, 0, 0}},
    { VGPU_PIXEL_FORMAT_RGBA8_UNORM,            "RGBA8",                VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_UNORM_SRGB,       "RGBA8Srgb",            VGPUTextureFormatType_UnormSrgb,    32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_SNORM,            "RGBA8S",               VGPUTextureFormatType_Snorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_UINT,             "RGBA8U",               VGPUTextureFormatType_Uint,         32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_RGBA8_SINT,             "RGBA8I",               VGPUTextureFormatType_Sint,         32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_BGRA8_UNORM,            "BGRA8",                VGPUTextureFormatType_Unorm,        32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},
    { VGPU_PIXEL_FORMAT_BGRA8_UNORM_SRGB,       "BGRA8Srgb",            VGPUTextureFormatType_UnormSrgb,    32,         {1, 1, 4, 1, 1},        {0, 0, 8, 8, 8, 8}},

    // Packed 32-Bit Pixel formats
    { VGPU_PIXEL_FORMAT_RGB10A2_UNORM,          "RGB10A2",              VGPUTextureFormatType_Unorm,       32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    { VGPU_PIXEL_FORMAT_RGB10A2_UINT,           "RGB10A2U",             VGPUTextureFormatType_Uint,        32,         {1, 1, 4, 1, 1},        {0, 0, 10, 10, 10, 2}},
    { VGPU_PIXEL_FORMAT_RG11B10_FLOAT,          "RG11B10F",             VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {0, 0, 11, 11, 10, 0}},
    { VGPU_PIXEL_FORMAT_RGB9E5_FLOAT,           "RGB9E5F",              VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {0, 0, 9, 9, 9, 5}},

    // 64-Bit Pixel Formats
    { VGPU_PIXEL_FORMAT_RG32_UINT,              "RG32U",                VGPUTextureFormatType_Uint,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG32_SINT,              "RG3I",                 VGPUTextureFormatType_Sint,        64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPU_PIXEL_FORMAT_RG32_FLOAT,             "RG3F",                 VGPUTextureFormatType_Float,       64,         {1, 1, 8, 1, 1},        {0, 0, 32, 32, 0, 0}},
    { VGPU_PIXEL_FORMAT_RGBA16_UNORM,           "RGBA16",               VGPUTextureFormatType_Unorm,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_SNORM,           "RGBA16S",              VGPUTextureFormatType_Snorm,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_UINT,            "RGBA16U",              VGPUTextureFormatType_Uint,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_SINT,            "RGBA16S",              VGPUTextureFormatType_Sint,        64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},
    { VGPU_PIXEL_FORMAT_RGBA16_FLOAT,           "RGBA16F",              VGPUTextureFormatType_Float,       64,         {1, 1, 8, 1, 1},        {0, 0, 16, 16, 16, 16}},

    // 128-Bit Pixel Formats
    { VGPU_PIXEL_FORMAT_RGBA32_UINT,            "RGBA32U",              VGPUTextureFormatType_Uint,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPU_PIXEL_FORMAT_RGBA32_SINT,            "RGBA32S",              VGPUTextureFormatType_Sint,        128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},
    { VGPU_PIXEL_FORMAT_RGBA32_FLOAT,           "RGBA32F",              VGPUTextureFormatType_Float,       128,        {1, 1, 16, 1, 1},       {0, 0, 32, 32, 32, 32}},

    // Depth-stencil
    { VGPU_PIXEL_FORMAT_D16_UNORM,              "Depth16UNorm",         VGPUTextureFormatType_Unorm,       16,         {1, 1, 2, 1, 1},        {16, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D32_FLOAT,              "Depth32Float",         VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {32, 0, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D24_UNORM_S8_UINT,      "Depth24UNormStencil8", VGPUTextureFormatType_Unorm,       32,         {1, 1, 4, 1, 1},        {24, 8, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_D32_FLOAT_S8_UINT,      "Depth32FloatStencil8", VGPUTextureFormatType_Float,       32,         {1, 1, 4, 1, 1},        {32, 8, 0, 0, 0, 0}},
    { VGPU_PIXEL_FORMAT_S8,                     "Stencil8",             VGPUTextureFormatType_Unorm,       8,          {1, 1, 1, 1, 1},        {0, 8, 0, 0, 0, 0}},

    // Compressed formats
    { VGPUTextureFormat_BC1RGBAUnorm,           "BC1RGBAUnorm",         VGPUTextureFormatType_Unorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC1RGBAUnormSrgb,       "BC1RGBAUnormSrgb",     VGPUTextureFormatType_UnormSrgb,    4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC2RGBAUnorm,           "BC2RGBAUnorm",         VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC2RGBAUnormSrgb,       "BC2RGBAUnormSrgb",     VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC3RGBAUnorm,           "BC3RGBAUnorm",         VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC3RGBAUnormSrgb,       "BC3RGBAUnormSrgb",     VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC4RUnorm,              "BC4RUnorm",            VGPUTextureFormatType_Unorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC4RSnorm,              "BC4RSnorm",            VGPUTextureFormatType_Snorm,        4,          {4, 4, 8, 1, 1},        {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC5RGUnorm,             "BC5RGUnorm",           VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC5RGSnorm,             "BC5RGSnorm",           VGPUTextureFormatType_Snorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC6HRGBUfloat,          "BC6HRGBUfloat",        VGPUTextureFormatType_Float,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC6HRGBSfloat,          "BC6HRGBSfloat",        VGPUTextureFormatType_Float,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC7RGBAUnorm,           "BC7RGBAUnorm",         VGPUTextureFormatType_Unorm,        8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},
    { VGPUTextureFormat_BC7RGBAUnormSrgb,       "BC7Srgb",              VGPUTextureFormatType_UnormSrgb,    8,          {4, 4, 16, 1, 1},       {0, 0, 0, 0, 0, 0}},

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

bool vgpu_is_depth_format(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return FormatDesc[format].bits.depth > 0;
}

bool vgpu_is_stencil_format(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return FormatDesc[format].bits.stencil > 0;
}

bool vgpu_is_depth_stencil_format(VGPUTextureFormat format)
{
    return vgpu_is_depth_format(format) || vgpu_is_stencil_format(format);
}

bool vgpu_is_compressed_format(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[format].format == format);
    return format >= VGPUTextureFormat_BC1RGBAUnorm && format <= VGPUTextureFormat_BC7RGBAUnormSrgb;
}

const char* vgpu_get_format_name(VGPUTextureFormat format)
{
    VGPU_ASSERT(FormatDesc[(uint32_t)format].format == format);
    return FormatDesc[(uint32_t)format].name;
}
