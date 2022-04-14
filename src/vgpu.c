// Copyright � Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgpu_driver.h"
#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#elif defined(_WIN32)
#   define NOMINMAX
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

#define MAX_MESSAGE_SIZE 1024

static void gfxDefaultLogCallback(VGFXLogLevel level, const char* message)
{
#if defined(__EMSCRIPTEN__)
    switch (level)
    {
        case VGFX_LOG_LEVEL_WARN:
            emscripten_log(EM_LOG_CONSOLE | EM_LOG_WARN, "%s", message);
            break;
        case VGFX_LOG_LEVEL_ERROR:
            emscripten_log(EM_LOG_CONSOLE | EM_LOG_ERROR, "%s", message);
            break;
        default:
            emscripten_log(EM_LOG_CONSOLE, "%s", message);
            break;
    }
#elif defined(_WIN32)
    _VGFX_UNUSED(level);
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#else
    _VGFX_UNUSED(level);
    _VGFX_UNUSED(message);
#endif
}

static vgfxLogFunc s_LogFunc = gfxDefaultLogCallback;

void vgfxLogInfo(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(char) * MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGFXLogLevel_Info, msg);
}

void vgfxLogWarn(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(char) * MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGFXLogLevel_Warn, msg);
}

void vgfxLogError(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(char) * MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGFXLogLevel_Error, msg);
}

void vgfxSetLogFunc(vgfxLogFunc func)
{
    s_LogFunc = func;
}

static const VGFXDriver* drivers[] = {
#if defined(VGPU_D3D12_DRIVER)
    &d3d12_driver,
#endif
#if defined(VGPU_D3D11_DRIVER)
    &d3d11_driver,
#endif
#if defined(VGPU_VULKAN_DRIVER)
    &vulkan_driver,
#endif
#if defined(VGPU_WEBGPU_DRIVER)
    &webgpu_driver,
#endif
    NULL
};

#define NULL_RETURN(name) if (name == NULL) { return; }
#define NULL_RETURN_NULL(name) if (name == NULL) { return NULL; }

VGFXSurface vgfxAllocSurface(VGFXSurfaceType type) {
    VGFXSurface surface = (VGFXSurface_T*)VGFX_MALLOC(sizeof(VGFXSurface_T));
    surface->type = type;
    return surface;
}

VGFXSurface vgfxCreateSurfaceWin32(void* hinstance, void* hwnd)
{
    VGFXSurface surface = vgfxAllocSurface(VGFXSurfaceType_Win32);
#if defined(_WIN32)
    surface->hinstance = (HINSTANCE)hinstance;
    surface->window = (HWND)hwnd;
    if (!IsWindow(surface->window))
    {
        VGFX_FREE(surface);
        return NULL;
    }
#else
    _VGFX_UNUSED(hinstance);
    _VGFX_UNUSED(hwnd);
#endif
    return surface;
}

VGFXSurface vgfxCreateSurfaceXlib(void* display, uint32_t window)
{
    VGFXSurface surface = vgfxAllocSurface(VGFXSurfaceType_Xlib);
#if defined(__linux__)
    surface->display = display;
    surface->window = window;
#else
    _VGFX_UNUSED(display);
    _VGFX_UNUSED(window);
#endif
    return surface;
}

VGFXSurface vgfxCreateSurfaceWeb(const char* selector)
{
    VGFXSurface surface = vgfxAllocSurface(VGFXSurfaceType_Web);
#if defined(__EMSCRIPTEN__)
    surface->selector = selector;
#else
    _VGFX_UNUSED(selector);
#endif
    return surface;
}

void vgfxDestroySurface(VGFXSurface surface)
{
    NULL_RETURN(surface);
    VGFX_FREE(surface);
}

VGFXSurfaceType vgfxGetSurfaceType(VGFXSurface surface)
{
    if (surface == NULL) {
        return VGFXSurfaceType_Unknown;
    }

    return surface->type;
}

bool vgfxIsSupported(VGFXBackendType backend)
{
    for (uint32_t i = 0; i < _VGFX_COUNT_OF(drivers); ++i)
    {
        if (!drivers[i])
            break;

        if (drivers[i]->backend == backend)
        {
            return drivers[i]->isSupported();
        }
    }

    return false;
}

VGFXDevice vgfxCreateDevice(const VGFXDeviceDesc* desc)
{
    NULL_RETURN_NULL(desc);

    VGFXBackendType backend = desc->preferredBackend;

retry:
    if (backend == VGFXBackendType_Default)
    {
        for (uint32_t i = 0; i < _VGFX_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->isSupported())
            {
                return drivers[i]->createDevice(desc);
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < _VGFX_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->backend == backend)
            {
                if (drivers[i]->isSupported())
                {
                    return drivers[i]->createDevice(desc);
                }
                else
                {
                    vgfxLogWarn("Wanted API not supported, fallback to default");
                    backend = VGFXBackendType_Default;
                    goto retry;
                }
            }
        }
    }

    return NULL;
}

void vgfxDestroyDevice(VGFXDevice device)
{
    NULL_RETURN(device);
    device->destroyDevice(device);
}

void vgfxFrame(VGFXDevice device)
{
    NULL_RETURN(device);
    device->frame(device->driverData);
}

void vgfxWaitIdle(VGFXDevice device)
{
    NULL_RETURN(device);
    device->waitIdle(device->driverData);
}

bool vgpuQueryFeature(VGFXDevice device, VGPUFeature feature)
{
    if (device == NULL) {
        return false;
    }

    return device->hasFeature(device->driverData, feature);
}

void vgpuGetAdapterProperties(VGFXDevice device, VGPUAdapterProperties* properties)
{
    NULL_RETURN(device);
    NULL_RETURN(properties);

    device->getAdapterProperties(device->driverData, properties);
}

void vgpuGetLimits(VGFXDevice device, VGPULimits* limits)
{
    NULL_RETURN(device);
    NULL_RETURN(limits);

    device->getLimits(device->driverData, limits);
}

/* Buffer */
static VGFXBufferDesc _vgfxBufferDescDef(const VGFXBufferDesc* desc)
{
    VGFXBufferDesc def = *desc;
    def.size = _VGFX_DEF(def.size, 4);
    return def;
}

VGFXBuffer vgfxCreateBuffer(VGFXDevice device, const VGFXBufferDesc* desc, const void* pInitialData)
{
     NULL_RETURN_NULL(device);
     NULL_RETURN_NULL(desc);

     VGFXBufferDesc desc_def = _vgfxBufferDescDef(desc);
     return device->createBuffer(device->driverData, &desc_def, pInitialData);
}

void vgfxDestroyBuffer(VGFXDevice device, VGFXBuffer buffer)
{
    NULL_RETURN(device);
    NULL_RETURN(buffer);

    device->destroyBuffer(device->driverData, buffer);
}

/* Texture */
static VGFXTextureDesc _vgfxTextureDescDef(const VGFXTextureDesc* desc)
{
    VGFXTextureDesc def = *desc;
    def.type = _VGFX_DEF(def.type, VGFXTextureType2D);
    def.format = _VGFX_DEF(def.type, VGFXTextureFormat_RGBA8UNorm);
    //def.usage = _VGFX_DEF(def.type, VGFXTextureUsage_ShaderRead);
    def.width = _VGFX_DEF(def.width, 1);
    def.height = _VGFX_DEF(def.height, 1);
    def.depthOrArraySize = _VGFX_DEF(def.depthOrArraySize, 1);
    def.mipLevelCount = _VGFX_DEF(def.mipLevelCount, 1);
    def.sampleCount = _VGFX_DEF(def.sampleCount, 1);
    return def;
}

VGFXTexture vgfxCreateTexture(VGFXDevice device, const VGFXTextureDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    VGFXTextureDesc desc_def = _vgfxTextureDescDef(desc);
    return device->createTexture(device->driverData, &desc_def);
}

void vgfxDestroyTexture(VGFXDevice device, VGFXTexture texture)
{
    NULL_RETURN(device);
    NULL_RETURN(texture);

    device->destroyTexture(device->driverData, texture);
}

/* SwapChain */
VGFXSwapChain vgfxCreateSwapChain(VGFXDevice device, VGFXSurface surface, const VGFXSwapChainDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(surface);
    NULL_RETURN_NULL(desc);

    return device->createSwapChain(device->driverData, surface, desc);
}

void vgfxDestroySwapChain(VGFXDevice device, VGFXSwapChain swapChain)
{
    NULL_RETURN(device);
    NULL_RETURN(swapChain);

    device->destroySwapChain(device->driverData, swapChain);
}

void vgfxSwapChainGetSize(VGFXDevice device, VGFXSwapChain swapChain, VGFXSize2D* pSize)
{
    NULL_RETURN(device);
    NULL_RETURN(swapChain);
    NULL_RETURN(pSize);

    device->getSwapChainSize(device->driverData, swapChain, pSize);
}

VGFXTexture vgfxSwapChainAcquireNextTexture(VGFXDevice device, VGFXSwapChain swapChain)
{
    NULL_RETURN_NULL(device);
    return device->acquireNextTexture(device->driverData, swapChain);
}

/* Commands */
void vgfxBeginRenderPass(VGFXDevice device, const VGFXRenderPassDesc* desc)
{
    NULL_RETURN(device);
    NULL_RETURN(desc);

    device->beginRenderPass(device->driverData, desc);
}

void vgfxEndRenderPass(VGFXDevice device)
{
    NULL_RETURN(device);

    device->endRenderPass(device->driverData);
}

// Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
static const VGFXFormatInfo c_FormatInfo[] = {
    //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencl signed  srgb
    { VGFXTextureFormat_Undefined,      "Undefined",           0,   0,  VGFXFormatKind_Integer,      false, false, false, false, false, false, false, false },
    { VGFXTextureFormat_R8UInt,           "R8_UINT",           1,   1, VGFXFormatKind_Integer,      true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R8SInt,           "R8_SINT",           1,   1, VGFXFormatKind_Integer,      true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_R8UNorm,          "R8_UNORM",          1,   1, VGFXFormatKind_Normalized,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R8SNorm,          "R8_SNORM",          1,   1, VGFXFormatKind_Normalized,   true,  false, false, false, false, false, false, false },

    { VGFXTextureFormat_R16UInt,          "R16_UINT",          2,   1, VGFXFormatKind_Integer,      true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R16SInt,          "R16_SINT",          2,   1, VGFXFormatKind_Integer,      true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_R16UNorm,         "R16_UNORM",         2,   1, VGFXFormatKind_Normalized,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R16SNorm,         "R16_SNORM",         2,   1, VGFXFormatKind_Normalized,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R16Float,         "R16_FLOAT",         2,   1, VGFXFormatKind_Float,        true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_RG8UInt,          "RG8_UINT",          2,   1, VGFXFormatKind_Integer,      true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG8SInt,          "RG8_SINT",          2,   1, VGFXFormatKind_Integer,      true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RG8UNorm,         "RG8_UNORM",         2,   1, VGFXFormatKind_Normalized,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG8SNorm,         "RG8_SNORM",         2,   1, VGFXFormatKind_Normalized,   true,  true,  false, false, false, false, false, false },

    { VGFXTextureFormat_BGRA4UNorm,       "BGRA4_UNORM",       2,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_B5G6R5UNorm,      "B5G6R5_UNORM",      2,   1, VGFXFormatKind_Normalized,   true,  true,  true,  false, false, false, false, false },
    { VGFXTextureFormat_B5G5R5A1UNorm,    "B5G5R5A1_UNORM",    2,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },

    { VGFXTextureFormat_R32UInt,          "R32_UINT",          4,   1, VGFXFormatKind_Integer,      true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_R32SInt,          "R32_SINT",          4,   1, VGFXFormatKind_Integer,      true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_R32Float,         "R32_FLOAT",         4,   1, VGFXFormatKind_Float,        true,  false, false, false, false, false, true,  false },
    { VGFXTextureFormat_RG16UInt,         "RG16_UINT",         4,   1, VGFXFormatKind_Integer,      true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG16SInt,         "RG16_SINT",         4,   1, VGFXFormatKind_Integer,      true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RG16UNorm,        "RG16_UNORM",        4,   1, VGFXFormatKind_Normalized,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG16SNorm,        "RG16_SNORM",        4,   1, VGFXFormatKind_Normalized,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG16Float,        "RG16_FLOAT",        4,   1, VGFXFormatKind_Float,        true,  true,  false, false, false, false, true,  false },

    { VGFXTextureFormat_RGBA8UInt,        "RGBA8_UINT",        4,   1, VGFXFormatKind_Integer,      true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA8SInt,        "RGBA8_SINT",        4,   1, VGFXFormatKind_Integer,      true,  true,  true,  true,  false, false, true,  false },
    { VGFXTextureFormat_RGBA8UNorm,       "RGBA8_UNORM",       4,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA8UNormSrgb,      "SRGBA8_UNORM",      4,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, true  },
    { VGFXTextureFormat_RGBA8SNorm,       "RGBA8_SNORM",       4,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BGRA8UNorm,       "BGRA8_UNORM",       4,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BGRA8UNormSrgb,      "SBGRA8_UNORM",      4,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },

    { VGFXTextureFormat_RGB10A2UNorm,   "R10G10B10A2_UNORM", 4,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RG11B10Float,   "R11G11B10_FLOAT",   4,   1, VGFXFormatKind_Float,        true,  true,  true,  false, false, false, false, false },
    { VGFXTextureFormat_RGB9E5Float,    "RGB9E5Float",   4,   1, VGFXFormatKind_Float,        true,  true,  true,  false, false, false, false, false },
    
    { VGFXTextureFormat_RG32UInt,         "RG32_UINT",         8,   1, VGFXFormatKind_Integer,      true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_RG32SInt,         "RG32_SINT",         8,   1, VGFXFormatKind_Integer,      true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RG32Float,        "RG32_FLOAT",        8,   1, VGFXFormatKind_Float,        true,  true,  false, false, false, false, true,  false },
    { VGFXTextureFormat_RGBA16UInt,       "RGBA16_UINT",       8,   1, VGFXFormatKind_Integer,      true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA16SInt,       "RGBA16_SINT",       8,   1, VGFXFormatKind_Integer,      true,  true,  true,  true,  false, false, true,  false },
    { VGFXTextureFormat_RGBA16UNorm,      "RGBA16_UNORM",      8,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA16SNorm,      "RGBA16_SNORM",      8,   1, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA16Float,      "RGBA16_FLOAT",      8,   1, VGFXFormatKind_Float,        true,  true,  true,  true,  false, false, true,  false },

    { VGFXTextureFormat_RGBA32UInt,       "RGBA32_UINT",       16,  1, VGFXFormatKind_Integer,      true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_RGBA32SInt,       "RGBA32_SINT",       16,  1, VGFXFormatKind_Integer,      true,  true,  true,  true,  false, false, true,  false },
    { VGFXTextureFormat_RGBA32Float,      "RGBA32_FLOAT",      16,  1, VGFXFormatKind_Float,        true,  true,  true,  true,  false, false, true,  false },

    { VGFXTextureFormat_Depth16UNorm,           "Depth16UNorm", 2,   1, VGFXFormatKind_DepthStencil, false, false, false, false, true,  false, false, false },
    { VGFXTextureFormat_Depth24UNormStencil8,   "D24S8",         4,   1, VGFXFormatKind_DepthStencil, false, false, false, false, true,  true,  false, false },
    { VGFXTextureFormat_Depth32Float,           "D32",           4,   1, VGFXFormatKind_DepthStencil, false, false, false, false, true,  false, false, false },
    { VGFXTextureFormat_Depth32FloatStencil8,   "D32S8",         8,   1, VGFXFormatKind_DepthStencil, false, false, false, false, true,  true,  false, false },

    { VGFXTextureFormat_BC1UNorm,       "BC1UNorm",         8,   4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BC1UNormSrgb,   "BC1UNormSrgb",    8,   4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, true  },
    { VGFXTextureFormat_BC2UNorm,       "BC2UNorm",         16,  4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BC2UNormSrgb,   "BC2UNormSrgb",    16,  4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, true  },
    { VGFXTextureFormat_BC3UNorm,       "BC3UNorm",         16,  4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BC3UNormSrgb,   "BC3UNormSrgb",    16,  4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, true  },
    { VGFXTextureFormat_BC4UNorm,       "BC4UNorm",         8,   4, VGFXFormatKind_Normalized,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_BC4SNorm,       "BC4SNorm",         8,   4, VGFXFormatKind_Normalized,   true,  false, false, false, false, false, false, false },
    { VGFXTextureFormat_BC5UNorm,       "BC5UNorm",         16,  4, VGFXFormatKind_Normalized,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_BC5SNorm,       "BC5SNorm",         16,  4, VGFXFormatKind_Normalized,   true,  true,  false, false, false, false, false, false },
    { VGFXTextureFormat_BC6HUFloat,     "BC6HUFloat",       16,  4, VGFXFormatKind_Float,        true,  true,  true,  false, false, false, false, false },
    { VGFXTextureFormat_BC6HSFloat,     "BC6HSFloat",       16,  4, VGFXFormatKind_Float,        true,  true,  true,  false, false, false, true,  false },
    { VGFXTextureFormat_BC7UNorm,       "BC7UNorm",         16,  4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, false },
    { VGFXTextureFormat_BC7UNormSrgb,   "BC7UNormSrgb",    16,  4, VGFXFormatKind_Normalized,   true,  true,  true,  true,  false, false, false, true  },
};

void vgfxGetFormatInfo(VGFXTextureFormat format, const VGFXFormatInfo* pInfo)
{
    VGFX_ASSERT(pInfo);

    //static_assert(sizeof(c_FormatInfo) / sizeof(VGFXFormatInfo) == _VGFXTextureFormat_Count,
    //    "The format info table doesn't have the right number of elements");

    if (format >= _VGFXTextureFormat_Count)
    {
        // UNKNOWN
        pInfo = &c_FormatInfo[0];
        return;
    }

    pInfo = &c_FormatInfo[(uint32_t)format];
    VGFX_ASSERT(pInfo->format == format);
}

bool vgfxIsDepthFormat(VGFXTextureFormat format)
{
    VGFX_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasDepth;
}

bool vgfxIsStencilFormat(VGFXTextureFormat format)
{
    VGFX_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasStencil;
}

bool vgfxIsDepthStencilFormat(VGFXTextureFormat format)
{
    VGFX_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].hasDepth || c_FormatInfo[(uint32_t)format].hasStencil;
}