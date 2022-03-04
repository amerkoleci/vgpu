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

typedef struct gfxSurface_T* VGFXSurface;
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
    VGFX_API_DEFAULT = 0,
    VGFX_API_VULKAN,
    VGFX_API_D3D12,
    VGFX_API_D3D11,
    VGFX_API_WEBGPU,

    _VGFX_API_COUNT,
    _VGFX_API_FORCE_U32 = 0x7FFFFFFF
} VGFXAPI;

typedef enum VGFXValidationMode
{
    /// No validation is enabled.
    VGFX_VALIDATION_MODE_DISABLED = 0,
    /// Print warnings and errors
    VGFX_VALIDATION_MODE_ENABLED,
    /// Print all warnings, errors and info messages
    VGFX_VALIDATION_MODE_VERBOSE,
    /// Enable GPU-based validation
    VGFX_VALIDATION_MODE_GPU,

    _VGFX_VALIDATION_MODE_COUNT,
    _VGFX_VALIDATION_MODE_FORCE_U32 = 0x7FFFFFFF
} VGFXValidationMode;

typedef enum VGFXSurfaceType
{
    VGFX_SURFACE_TYPE_UNKNOWN = 0,
    VGFX_SURFACE_TYPE_WIN32,
    VGFX_SURFACE_TYPE_XLIB,
    VGFX_SURFACE_TYPE_WEB,

    _VGFX_SURFACE_TYPE_COUNT,
    _VGFX_SURFACE_TYPE_FORCE_U32 = 0x7FFFFFFF
} VGFXSurfaceType;

typedef enum VGFXPresentMode
{
    VGFX_PRESENT_MODE_IMMEDIATE = 0,
    VGFX_PRESENT_MODE_MAILBOX,
    VGFX_PRESENT_MODE_FIFO,

    _VGFX_PRESENT_MODE_COUNT,
    _VGFX_PRESENT_MODE_FORCE_U32 = 0x7FFFFFFF
} VGFXPresentMode;

typedef struct VGFXDeviceInfo
{
    VGFXAPI preferredApi;
    VGFXValidationMode validationMode;
} VGFXDeviceInfo;

typedef void (VGFX_CALL* vgfxLogFunc)(VGFXLogLevel level, const char* message);
VGFX_API void vgfxSetLogFunc(vgfxLogFunc func);

VGFX_API VGFXSurface vgfxCreateSurfaceWin32(void* hinstance, void* hwnd);
VGFX_API VGFXSurface vgfxCreateSurfaceXlib(void* display, uint32_t window);
VGFX_API VGFXSurface vgfxCreateSurfaceWeb(const char* selector);
VGFX_API void vgfxDestroySurface(VGFXSurface surface);
VGFX_API VGFXSurfaceType vgfxGetSurfaceType(VGFXSurface surface);

VGFX_API bool vgfxIsSupported(VGFXAPI api);
VGFX_API gfxDevice vgfxCreateDevice(VGFXSurface surface, const VGFXDeviceInfo* info);
VGFX_API void vgfxDestroyDevice(gfxDevice device);
VGFX_API void vgfxFrame(gfxDevice device);

#endif /* _VGFX_H */
