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
    s_LogFunc(VGFX_LOG_LEVEL_INFO, msg);
}

void vgfxLogWarn(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(char) * MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGFX_LOG_LEVEL_WARN, msg);
}

void vgfxLogError(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(char) * MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGFX_LOG_LEVEL_ERROR, msg);
}

void vgfxSetLogFunc(vgfxLogFunc func)
{
    s_LogFunc = func;
}

static const gfxDriver* drivers[] = {
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

VGFXSurface vgfxCreateSurfaceWin32(void* hinstance, void* hwnd) {
    gfxSurface_T* surface = (gfxSurface_T*)VGFX_MALLOC(sizeof(gfxSurface_T));
    surface->type = VGFX_SURFACE_TYPE_WIN32;
#if defined(_WIN32)
    surface->hinstance = (HINSTANCE)hinstance;
    surface->hwnd = (HWND)hwnd;
    if (!IsWindow(surface->hwnd))
    {
        VGFX_FREE(surface);
        return NULL;
    }

    RECT window_rect;
    GetClientRect(surface->hwnd, &window_rect);
    surface->width = window_rect.right - window_rect.left;
    surface->height = window_rect.bottom - window_rect.top;
#else
    _VGFX_UNUSED(hinstance);
    _VGFX_UNUSED(hwnd);
#endif
    return surface;
}

VGFXSurface vgfxCreateSurfaceXlib(void* display, uint32_t window) {
    gfxSurface_T* surface = (gfxSurface_T*)VGFX_MALLOC(sizeof(gfxSurface_T));
    surface->type = VGFX_SURFACE_TYPE_XLIB;
#if defined(__linux__)
    surface->display = display;
    surface->window = window;
#else
    _VGFX_UNUSED(display);
    _VGFX_UNUSED(window);
#endif
    return surface;
}

VGFXSurface vgfxCreateSurfaceWeb(const char* selector) {
    gfxSurface_T* surface = (gfxSurface_T*)VGFX_MALLOC(sizeof(gfxSurface_T));
    surface->type = VGFX_SURFACE_TYPE_WEB;
#if defined(__EMSCRIPTEN__)
    surface->selector = selector;
#else
    _VGFX_UNUSED(selector);
#endif
    return surface;
}

void vgfxDestroySurface(VGFXSurface surface) {
    NULL_RETURN(surface);
    VGFX_FREE(surface);
}

VGFXSurfaceType vgfxGetSurfaceType(VGFXSurface surface) {
    if (surface == NULL) {
        return VGFX_SURFACE_TYPE_UNKNOWN;
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

gfxDevice vgfxCreateDevice(VGFXSurface surface, const VGFXDeviceInfo* info)
{
    VGFXAPI api = info->preferredApi;
retry:
    if (api == VGFX_API_DEFAULT)
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
                    api = VGFX_API_DEFAULT;
                    goto retry;
                }
            }
        }
    }

    return NULL;
}

void vgfxDestroyDevice(gfxDevice device)
{
    NULL_RETURN(device);
    device->destroyDevice(device);
}

void vgfxFrame(gfxDevice device)
{
    NULL_RETURN(device);
    device->frame(device->driverData);
}
