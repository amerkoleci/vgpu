// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGFX_H
#define _VGFX_H

#if defined(VGFX_SHARED_LIBRARY)
#    if defined(_WIN32)
#        if defined(VGFX_IMPLEMENTATION)
#            define _VGFX_EXPORT __declspec(dllexport)
#        else
#            define _VGFX_EXPORT __declspec(dllimport)
#        endif
#    else 
#        if defined(VGFX_IMPLEMENTATION)
#            define _VGFX_EXPORT __attribute__((visibility("default")))
#        else
#            define _VGFX_EXPORT
#        endif
#    endif
#else
#    define _VGFX_EXPORT
#endif

#ifdef __cplusplus
#    define _VGFX_EXTERN extern "C"
#else
#    define _VGFX_EXTERN extern
#endif

#define VGFX_API _VGFX_EXTERN _VGFX_EXPORT

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#   define VGFX_CALL __cdecl
#else
#   define VGFX_CALL
#endif

typedef struct gfxDevice_T* gfxDevice;

typedef enum VGFXLogLevel
{
    VGFX_LOG_LEVEL_INFO = 0,
    VGFX_LOG_LEVEL_WARN,
    VGFX_LOG_LEVEL_ERROR,

    _VGFX_LOG_LEVEL_COUNT,
    _VGFX_LOG_LEVEL_FORCE_U32 = 0x7FFFFFFF
} VGFXLogLevel;

typedef enum VGFXAPI
{
    VGFX_API_VULKAN = 0,
    VGFX_API_D3D12,

    _VGFX_API_COUNT,
    _VGFX_API_FORCE_U32 = 0x7FFFFFFF
} VGFXAPI;

typedef enum VGFXPresentMode
{
    VGFX_PRESENT_MODE_IMMEDIATE = 0,
    VGFX_PRESENT_MODE_MAILBOX,
    VGFX_PRESENT_MODE_FIFO,

    _VGFX_PRESENT_MODE_COUNT,
    _VGFX_PRESENT_MODE_FORCE_U32 = 0x7FFFFFFF
} VGFXPresentMode;

typedef void (VGFX_CALL* vgfxLogFunc)(VGFXLogLevel level, const char* message);
VGFX_API void vgfxSetLogFunc(vgfxLogFunc func);

VGFX_API gfxDevice vgfxCreateDevice(void);
VGFX_API void vgfxDestroyDevice(gfxDevice device);

#endif /* _VGFX_H */
