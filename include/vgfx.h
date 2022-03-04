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

enum {
    VGFX_MAX_INFLIGHT_FRAMES = 2,
    VGFX_MAX_COLOR_ATTACHMENTS = 8,
    VGFX_MAX_VERTEX_ATTRIBUTES = 16,
};

typedef struct VGFXSurface_T* VGFXSurface;
typedef struct VGFXDevice_T* VGFXDevice;
typedef struct VGFXSwapChain_T* VGFXSwapChain;
typedef struct VGFXTexture_T* VGFXTexture;

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

typedef enum VGFXFeature
{
    VGFX_FEATURE_COMPUTE = 0,

    _VGFX_FEATURE_COUNT,
    _VGFX_FEATURE_FORCE_U32 = 0x7FFFFFFF
} VGFXFeature;

typedef struct VGFXDeviceInfo
{
    VGFXAPI preferredApi;
    VGFXValidationMode validationMode;
} VGFXDeviceInfo;

typedef struct VGFXSwapChainInfo
{
    uint32_t width;
    uint32_t height;
} VGFXSwapChainInfo;

typedef void (VGFX_CALL* vgfxLogFunc)(VGFXLogLevel level, const char* message);
VGFX_API void vgfxSetLogFunc(vgfxLogFunc func);

VGFX_API VGFXSurface vgfxCreateSurfaceWin32(void* hinstance, void* hwnd);
VGFX_API VGFXSurface vgfxCreateSurfaceXlib(void* display, uint32_t window);
VGFX_API VGFXSurface vgfxCreateSurfaceWeb(const char* selector);
VGFX_API void vgfxDestroySurface(VGFXSurface surface);
VGFX_API VGFXSurfaceType vgfxGetSurfaceType(VGFXSurface surface);

VGFX_API bool vgfxIsSupported(VGFXAPI api);
VGFX_API VGFXDevice vgfxCreateDevice(VGFXSurface surface, const VGFXDeviceInfo* info);
VGFX_API void vgfxDestroyDevice(VGFXDevice device);
VGFX_API void vgfxFrame(VGFXDevice device);
VGFX_API void vgfxWaitIdle(VGFXDevice device);
VGFX_API bool vgfxQueryFeature(VGFXDevice device, VGFXFeature feature);

/* Texture */
VGFX_API void vgfxDestroyTexture(VGFXTexture texture);

/* SwapChain */
VGFX_API VGFXSwapChain vgfxCreateSwapChain(VGFXDevice device, VGFXSurface surface, const VGFXSwapChainInfo* info);
VGFX_API void vgfxDestroySwapChain(VGFXSwapChain swapChain);
VGFX_API uint32_t vgfxSwapChainGetWidth(VGFXSwapChain swapChain);
VGFX_API uint32_t vgfxSwapChainGetHeight(VGFXSwapChain swapChain);
VGFX_API uint32_t vgfxSwapChainGetBackBufferIndex(VGFXSwapChain swapChain);
VGFX_API VGFXTexture vgfxSwapChainGetNextTexture(VGFXSwapChain swapChain);

#endif /* _VGFX_H */
