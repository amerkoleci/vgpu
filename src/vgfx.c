// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgfx_driver.h"
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
#if defined(VGFX_D3D12_DRIVER)
    &d3d12_driver,
#endif
#if defined(VGFX_D3D11_DRIVER)
    &d3d11_driver,
#endif
#if defined(VGFX_VULKAN_DRIVER)
    &vulkan_driver,
#endif
#if defined(VGFX_WEBGPU_DRIVER)
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

bool vgfxIsSupported(VGFXAPI api)
{
    for (uint32_t i = 0; i < _VGFX_COUNT_OF(drivers); ++i)
    {
        if (!drivers[i])
            break;

        if (drivers[i]->api == api)
        {
            return drivers[i]->isSupported();
        }
    }

    return false;
}

VGFXDevice vgfxCreateDevice(VGFXSurface surface, const VGFXDeviceInfo* info)
{
    VGFXAPI api = info->preferredApi;
retry:
    if (api == VGFXAPI_Default)
    {
        for (uint32_t i = 0; i < _VGFX_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->isSupported())
            {
                return drivers[i]->createDevice(surface, info);
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < _VGFX_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->api == api)
            {
                if (drivers[i]->isSupported())
                {
                    return drivers[i]->createDevice(surface, info);
                }
                else
                {
                    vgfxLogWarn("Wanted API not supported, fallback to default");
                    api = VGFXAPI_Default;
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

bool vgfxQueryFeature(VGFXDevice device, VGFXFeature feature)
{
    if (device == NULL) {
        return false;
    }

    return device->queryFeature(device->driverData, feature);
}

/* Texture */
void vgfxDestroyTexture(VGFXDevice device, VGFXTexture texture)
{
    NULL_RETURN(device);
    NULL_RETURN(texture);
    device->destroyTexture(device->driverData, texture);
}

/* SwapChain */
VGFXSwapChain vgfxCreateSwapChain(VGFXDevice device, VGFXSurface surface, const VGFXSwapChainInfo* info)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(surface);
    NULL_RETURN_NULL(info);

    return device->createSwapChain(device->driverData, surface, info);
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
void vgfxBeginRenderPass(VGFXDevice device, const VGFXRenderPassInfo* info)
{
    NULL_RETURN(device);
    NULL_RETURN(info);

    device->beginRenderPass(device->driverData, info);
}

void vgfxEndRenderPass(VGFXDevice device)
{
    NULL_RETURN(device);

    device->endRenderPass(device->driverData);
}
