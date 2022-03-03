// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgfx_driver.h"

#define MAX_MESSAGE_SIZE 1024

static void gfxDefaultLogCallback(VGFXLogLevel level, const char* message)
{
    _VGFX_UNUSED(level);
    _VGFX_UNUSED(message);
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

void gfxSetLogFunc(vgfxLogFunc func)
{
    s_LogFunc = func;
}

static const gfxDriver* drivers[] = {
#if defined(VGFX_D3D12_DRIVER)
    &d3d12_driver,
#endif
#if defined(VGFX_VULKAN_DRIVER)
    &vulkan_driver,
#endif
    NULL
};

#define NULL_RETURN(name) if (name == NULL) { return; }

gfxDevice gfxCreateDevice(void)
{
    return drivers[1]->createDevice();
}

 void gfxDestroyDevice(gfxDevice device)
 {
     NULL_RETURN(device);
     device->destroyDevice(device);
 }
