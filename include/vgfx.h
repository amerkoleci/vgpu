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
    VGFXLogLevel_Info = 0,
    VGFXLogLevel_Warn,
    VGFXLogLevel_Error,

    VGFXLogLevel_Count,
    VGFXLogLevel_Force32 = 0x7FFFFFFF
} VGFXLogLevel;

typedef enum VGFXAPI
{
    VGFXAPI_Default = 0,
    VGFXAPI_Vulkan,
    VGFXAPI_D3D12,
    VGFXAPI_D3D11,
    VGFXAPI_WebGPU,

    VGFXAPI_Count,
    VGFXAPI_Force32 = 0x7FFFFFFF
} VGFXAPI;

typedef enum VGFXValidationMode
{
    /// No validation is enabled.
    VGFXValidationMode_Disabled = 0,
    /// Print warnings and errors
    VGFXValidationMode_Enabled,
    /// Print all warnings, errors and info messages
    VGFXValidationMode_Verbose,
    /// Enable GPU-based validation
    VGFXValidationMode_GPU,

    VGFXValidationMode_Count,
    VGFXValidationMode_Force32 = 0x7FFFFFFF
} VGFXValidationMode;

typedef enum VGFXSurfaceType
{
    VGFXSurfaceType_Unknown = 0,
    VGFXSurfaceType_Win32,
    VGFXSurfaceType_Xlib,
    VGFXSurfaceType_Web,

    VGFXSurfaceType_Count,
    VGFXSurfaceType_Force32 = 0x7FFFFFFF
} VGFXSurfaceType;

typedef enum VGFXTextureFormat
{
    VGFXTextureFormat_Undefined,
    VGFXTextureFormat_R8Unorm,
    VGFXTextureFormat_R8Snorm,
    VGFXTextureFormat_R8Uint,
    VGFXTextureFormat_R8Sint,
    VGFXTextureFormat_R16Uint,
    VGFXTextureFormat_R16Sint,
    VGFXTextureFormat_R16Float,
    VGFXTextureFormat_RG8Unorm,
    VGFXTextureFormat_RG8Snorm,
    VGFXTextureFormat_RG8Uint,
    VGFXTextureFormat_RG8Sint,
    VGFXTextureFormat_R32Float,
    VGFXTextureFormat_R32Uint,
    VGFXTextureFormat_R32Sint,
    VGFXTextureFormat_RG16Uint,
    VGFXTextureFormat_RG16Sint,
    VGFXTextureFormat_RG16Float,
    VGFXTextureFormat_RGBA8Unorm,
    VGFXTextureFormat_RGBA8UnormSrgb,
    VGFXTextureFormat_RGBA8Snorm,
    VGFXTextureFormat_RGBA8Uint,
    VGFXTextureFormat_RGBA8Sint,
    VGFXTextureFormat_BGRA8Unorm,
    VGFXTextureFormat_BGRA8UnormSrgb,
    VGFXTextureFormat_RGB10A2Unorm,
    VGFXTextureFormat_RG11B10Ufloa,
    VGFXTextureFormat_RGB9E5Ufloat,
    VGFXTextureFormat_RG32Float,
    VGFXTextureFormat_RG32Uint,
    VGFXTextureFormat_RG32Sint,
    VGFXTextureFormat_RGBA16Uint,
    VGFXTextureFormat_RGBA16Sint,
    VGFXTextureFormat_RGBA16Float,
    VGFXTextureFormat_RGBA32Float,
    VGFXTextureFormat_RGBA32Uint,
    VGFXTextureFormat_RGBA32Sint,
    VGFXTextureFormat_Stencil8,
    VGFXTextureFormat_Depth16Unorm,
    VGFXTextureFormat_Depth24Plus,
    VGFXTextureFormat_Depth24PlusStencil8,
    VGFXTextureFormat_Depth24UnormStencil8,
    VGFXTextureFormat_Depth32Float,
    VGFXTextureFormat_Depth32FloatStencil8,
    VGFXTextureFormat_BC1RGBAUnorm,
    VGFXTextureFormat_BC1RGBAUnormSrgb,
    VGFXTextureFormat_BC2RGBAUnorm,
    VGFXTextureFormat_BC2RGBAUnormSrgb,
    VGFXTextureFormat_BC3RGBAUnorm,
    VGFXTextureFormat_BC3RGBAUnormSrgb,
    VGFXTextureFormat_BC4RUnorm,
    VGFXTextureFormat_BC4RSnorm,
    VGFXTextureFormat_BC5RGUnorm,
    VGFXTextureFormat_BC5RGSnorm,
    VGFXTextureFormat_BC6HRGBUFloat,
    VGFXTextureFormat_BC6HRGBFloat,
    VGFXTextureFormat_BC7RGBAUnorm,
    VGFXTextureFormat_BC7RGBAUnormSrgb,
    VGFXTextureFormat_ETC2RGB8Unorm,
    VGFXTextureFormat_ETC2RGB8UnormSrgb,
    VGFXTextureFormat_ETC2RGB8A1Unorm,
    VGFXTextureFormat_ETC2RGB8A1UnormSrgb,
    VGFXTextureFormat_ETC2RGBA8Unorm,
    VGFXTextureFormat_ETC2RGBA8UnormSrgb,
    VGFXTextureFormat_EACR11Unorm,
    VGFXTextureFormat_EACR11Snorm,
    VGFXTextureFormat_EACRG11Unorm,
    VGFXTextureFormat_EACRG11Snorm,
    VGFXTextureFormat_ASTC4x4Unorm,
    VGFXTextureFormat_ASTC4x4UnormSrgb,
    VGFXTextureFormat_ASTC5x4Unorm,
    VGFXTextureFormat_ASTC5x4UnormSrgb,
    VGFXTextureFormat_ASTC5x5Unorm,
    VGFXTextureFormat_ASTC5x5UnormSrgb,
    VGFXTextureFormat_ASTC6x5Unorm,
    VGFXTextureFormat_ASTC6x5UnormSrgb,
    VGFXTextureFormat_ASTC6x6Unorm,
    VGFXTextureFormat_ASTC6x6UnormSrgb,
    VGFXTextureFormat_ASTC8x5Unorm,
    VGFXTextureFormat_ASTC8x5UnormSrgb,
    VGFXTextureFormat_ASTC8x6Unorm,
    VGFXTextureFormat_ASTC8x6UnormSrgb,
    VGFXTextureFormat_ASTC8x8Unorm,
    VGFXTextureFormat_ASTC8x8UnormSrgb,
    VGFXTextureFormat_ASTC10x5Unorm,
    VGFXTextureFormat_ASTC10x5UnormSrgb,
    VGFXTextureFormat_ASTC10x6Unorm,
    VGFXTextureFormat_ASTC10x6UnormSrgb,
    VGFXTextureFormat_ASTC10x8Unorm,
    VGFXTextureFormat_ASTC10x8UnormSrgb,
    VGFXTextureFormat_ASTC10x10Unorm,
    VGFXTextureFormat_ASTC10x10UnormSrgb,
    VGFXTextureFormat_ASTC12x10Unorm,
    VGFXTextureFormat_ASTC12x10UnormSrgb,
    VGFXTextureFormat_ASTC12x12Unorm,
    VGFXTextureFormat_ASTC12x12UnormSrgb,

    VGFXTextureFormat_Count,
    VGFXTextureFormat_Force32 = 0x7FFFFFFF
} VGFXTextureFormat;

typedef enum VGFXPresentMode
{
    VGFXPresentMode_Immediate = 0x00000000,
    VGFXPresentMode_Mailbox = 0x00000001,
    VGFXPresentMode_Fifo = 0x00000002,

    VGFXPresentMode_Count,
    VGFXPresentMode_Force32 = 0x7FFFFFFF
} VGFXPresentMode;

typedef enum VGFXFeature
{
    VGFX_FEATURE_COMPUTE = 0,

    _VGFX_FEATURE_COUNT,
    _VGFX_FEATURE_FORCE_U32 = 0x7FFFFFFF
} VGFXFeature;

typedef enum VGFXLoadOp {
    VGFXLoadOp_Undefined = 0x00000000,
    VGFXLoadOp_Clear = 0x00000001,
    VGFXLoadOp_Load = 0x00000002,
    VGFXLoadOp_Force32 = 0x7FFFFFFF
} VGFXLoadOp;

typedef enum VGFXStoreOp {
    VGFXStoreOp_Undefined = 0x00000000,
    VGFXStoreOp_Store = 0x00000001,
    VGFXStoreOp_Discard = 0x00000002,
    VGFXStoreOp_Force32 = 0x7FFFFFFF
} VGFXStoreOp;

typedef struct VGFXColor
{
    float r;
    float g;
    float b;
    float a;
} VGFXColor;

typedef struct VGFXRenderPassColorAttachment
{
    VGFXTexture texture;
    VGFXLoadOp loadOp;
    VGFXStoreOp storeOp;
    VGFXColor clearColor;
} VGFXRenderPassColorAttachment;

typedef struct VGFXRenderPassInfo
{
    uint32_t colorAttachmentCount;
    const VGFXRenderPassColorAttachment* colorAttachments;
    //const VGFXRenderPassDepthStencilAttachment* depthStencilAttachment;
} VGFXRenderPassInfo;

typedef struct VGFXDeviceInfo
{
    VGFXAPI preferredApi;
    VGFXValidationMode validationMode;
} VGFXDeviceInfo;

typedef struct VGFXSwapChainInfo
{
    VGFXTextureFormat format;
    uint32_t width;
    uint32_t height;
    VGFXPresentMode presentMode;
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
VGFX_API VGFXTexture vgfxSwapChainGetNextTexture(VGFXSwapChain swapChain);

/* Commands */
VGFX_API void vgfxBeginRenderPass(VGFXDevice device, const VGFXRenderPassInfo* info);
VGFX_API void vgfxEndRenderPass(VGFXDevice device);

#endif /* _VGFX_H */
