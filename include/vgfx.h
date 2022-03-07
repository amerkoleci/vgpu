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

/* Version API */
#define VGFX_VERSION_MAJOR  0
#define VGFX_VERSION_MINOR	1
#define VGFX_VERSION_PATCH	0

enum {
    VGFX_MAX_INFLIGHT_FRAMES = 2,
    VGFX_MAX_COLOR_ATTACHMENTS = 8,
    VGFX_MAX_VERTEX_ATTRIBUTES = 16,
};

typedef struct VGFXSurface_T* VGFXSurface;
typedef struct VGFXDevice_T* VGFXDevice;
typedef struct VGFXBuffer_T* VGFXBuffer;
typedef struct VGFXTexture_T* VGFXTexture;
typedef struct VGFXSampler_T* VGFXSampler;
typedef struct VGFXSwapChain_T* VGFXSwapChain;

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
    VGFXSurfaceType_CoreWindow,
    VGFXSurfaceType_SwapChainPanel,
    VGFXSurfaceType_Xlib,
    VGFXSurfaceType_Web,

    VGFXSurfaceType_Count,
    VGFXSurfaceType_Force32 = 0x7FFFFFFF
} VGFXSurfaceType;

typedef enum VGFXTextureFormat
{
    VGFXTextureFormat_Undefined,
    /* 8-bit formats */
    VGFXTextureFormat_R8UNorm,
    VGFXTextureFormat_R8SNorm,
    VGFXTextureFormat_R8UInt,
    VGFXTextureFormat_R8SInt,
    /* 16-bit formats */
    VGFXTextureFormat_R16UNorm,
    VGFXTextureFormat_R16SNorm,
    VGFXTextureFormat_R16UInt,
    VGFXTextureFormat_R16SInt,
    VGFXTextureFormat_R16Float,
    VGFXTextureFormat_RG8UNorm,
    VGFXTextureFormat_RG8SNorm,
    VGFXTextureFormat_RG8UInt,
    VGFXTextureFormat_RG8SInt,
    /* 32-bit formats */
    VGFXTextureFormat_R32Float,
    VGFXTextureFormat_R32UInt,
    VGFXTextureFormat_R32SInt,
    VGFXTextureFormat_RG16UNorm,
    VGFXTextureFormat_RG16SNorm,
    VGFXTextureFormat_RG16UInt,
    VGFXTextureFormat_RG16SInt,
    VGFXTextureFormat_RG16Float,
    VGFXTextureFormat_RGBA8UNorm,
    VGFXTextureFormat_RGBA8UNormSrgb,
    VGFXTextureFormat_RGBA8SNorm,
    VGFXTextureFormat_RGBA8UInt,
    VGFXTextureFormat_RGBA8SInt,
    VGFXTextureFormat_BGRA8UNorm,
    VGFXTextureFormat_BGRA8UNormSrgb,
    /* Packed 32-Bit formats */
    VGFXTextureFormat_RGB10A2UNorm,
    VGFXTextureFormat_RG11B10Float,
    VGFXTextureFormat_RGB9E5Float,
    /* 64-Bit formats */
    VGFXTextureFormat_RG32UInt,
    VGFXTextureFormat_RG32SInt,
    VGFXTextureFormat_RG32Float,
    VGFXTextureFormat_RGBA16UNorm,
    VGFXTextureFormat_RGBA16SNorm,
    VGFXTextureFormat_RGBA16UInt,
    VGFXTextureFormat_RGBA16SInt,
    VGFXTextureFormat_RGBA16Float,
    /* 128-Bit formats */
    VGFXTextureFormat_RGBA32UInt,
    VGFXTextureFormat_RGBA32SInt,
    VGFXTextureFormat_RGBA32Float,
    VGFXTextureFormat_Stencil8,
    VGFXTextureFormat_Depth16UNorm,
    VGFXTextureFormat_Depth24UNormStencil8,
    VGFXTextureFormat_Depth32Float,
    VGFXTextureFormat_Depth32FloatStencil8,
    /* Compressed BC formats */
    VGFXTextureFormat_BC1RGBAUNorm,
    VGFXTextureFormat_BC1RGBAUNormSrgb,
    VGFXTextureFormat_BC2RGBAUNorm,
    VGFXTextureFormat_BC2RGBAUNormSrgb,
    VGFXTextureFormat_BC3RGBAUNorm,
    VGFXTextureFormat_BC3RGBAUNormSrgb,
    VGFXTextureFormat_BC4RUNorm,
    VGFXTextureFormat_BC4RSNorm,
    VGFXTextureFormat_BC5RGUNorm,
    VGFXTextureFormat_BC5RGSNorm,
    VGFXTextureFormat_BC6HRGBUFloat,
    VGFXTextureFormat_BC6HRGBFloat,
    VGFXTextureFormat_BC7RGBAUNorm,
    VGFXTextureFormat_BC7RGBAUNormSrgb,
    /* Compressed EAC/ETC formats */
    VGFXTextureFormat_ETC2RGB8UNorm,
    VGFXTextureFormat_ETC2RGB8UNormSrgb,
    VGFXTextureFormat_ETC2RGB8A1UNorm,
    VGFXTextureFormat_ETC2RGB8A1UNormSrgb,
    VGFXTextureFormat_ETC2RGBA8UNorm,
    VGFXTextureFormat_ETC2RGBA8UNormSrgb,
    VGFXTextureFormat_EACR11UNorm,
    VGFXTextureFormat_EACR11SNorm,
    VGFXTextureFormat_EACRG11UNorm,
    VGFXTextureFormat_EACRG11SNorm,
    /* Compressed ASTC formats */
    VGFXTextureFormat_ASTC4x4UNorm,
    VGFXTextureFormat_ASTC4x4UNormSrgb,
    VGFXTextureFormat_ASTC5x4UNorm,
    VGFXTextureFormat_ASTC5x4UNormSrgb,
    VGFXTextureFormat_ASTC5x5UNorm,
    VGFXTextureFormat_ASTC5x5UNormSrgb,
    VGFXTextureFormat_ASTC6x5UNorm,
    VGFXTextureFormat_ASTC6x5UNormSrgb,
    VGFXTextureFormat_ASTC6x6UNorm,
    VGFXTextureFormat_ASTC6x6UNormSrgb,
    VGFXTextureFormat_ASTC8x5UNorm,
    VGFXTextureFormat_ASTC8x5UNormSrgb,
    VGFXTextureFormat_ASTC8x6UNorm,
    VGFXTextureFormat_ASTC8x6UNormSrgb,
    VGFXTextureFormat_ASTC8x8UNorm,
    VGFXTextureFormat_ASTC8x8UNormSrgb,
    VGFXTextureFormat_ASTC10x5UNorm,
    VGFXTextureFormat_ASTC10x5UNormSrgb,
    VGFXTextureFormat_ASTC10x6UNorm,
    VGFXTextureFormat_ASTC10x6UNormSrgb,
    VGFXTextureFormat_ASTC10x8UNorm,
    VGFXTextureFormat_ASTC10x8UNormSrgb,
    VGFXTextureFormat_ASTC10x10UNorm,
    VGFXTextureFormat_ASTC10x10UNormSrgb,
    VGFXTextureFormat_ASTC12x10UNorm,
    VGFXTextureFormat_ASTC12x10UNormSrgb,
    VGFXTextureFormat_ASTC12x12UNorm,
    VGFXTextureFormat_ASTC12x12UNormSrgb,

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
    VGFXFeature_Compute = 0,
    VGFXFeature_TextureCompressionBC,
    VGFXFeature_TextureCompressionETC2,
    VGFXFeature_TextureCompressionASTC,

    VGFXFeature_Count,
    VGFXFeature_Force32 = 0x7FFFFFFF
} VGFXFeature;

typedef enum VGFXLoadAction {
    VGFXLoadAction_Discard = 0,
    VGFXLoadAction_Load,
    VGFXLoadAction_Clear,

    VGFXLoadAction_Force32 = 0x7FFFFFFF
} VGFXLoadAction;

typedef enum VGFXStoreAction {
    VGFXStoreAction_Discard = 0,
    VGFXStoreAction_Store,

    VGFXStoreAction_Force32 = 0x7FFFFFFF
} VGFXStoreAction;

typedef struct VGFXColor
{
    float r;
    float g;
    float b;
    float a;
} VGFXColor;

typedef struct VGFXSize2D {
    uint32_t    width;
    uint32_t    height;
} VGFXSize2D;

typedef struct VGFXViewport {
    /// Top left x coordinate.
    float x;
    /// Top left y coordinate.
    float y;
    /// Width of the viewport rectangle.
    float width;
    /// Height of the viewport rectangle (Y is down).
    float height;
    /// Minimum depth of the viewport. Ranges between 0 and 1.
    float minDepth;
    /// Maximum depth of the viewport. Ranges between 0 and 1.
    float maxDepth;
} VGFXViewport;

typedef struct VGFXRenderPassColorAttachment
{
    VGFXTexture texture;
    VGFXLoadAction loadAction;
    VGFXStoreAction storeAction;
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
VGFX_API void vgfxDestroyTexture(VGFXDevice device, VGFXTexture texture);

/* SwapChain */
VGFX_API VGFXSwapChain vgfxCreateSwapChain(VGFXDevice device, VGFXSurface surface, const VGFXSwapChainInfo* info);
VGFX_API void vgfxDestroySwapChain(VGFXDevice device, VGFXSwapChain swapChain);
VGFX_API void vgfxSwapChainGetSize(VGFXDevice device, VGFXSwapChain swapChain, VGFXSize2D* pSize);
VGFX_API VGFXTexture vgfxSwapChainAcquireNextTexture(VGFXDevice device, VGFXSwapChain swapChain);

/* Commands */
VGFX_API void vgfxBeginRenderPass(VGFXDevice device, const VGFXRenderPassInfo* info);
VGFX_API void vgfxEndRenderPass(VGFXDevice device);

#endif /* _VGFX_H */
