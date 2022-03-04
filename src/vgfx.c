// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgfx_driver.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
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

gfxDevice vgfxCreateDevice(const VGFXDeviceInfo* info)
{
    return drivers[0]->createDevice(info);
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
