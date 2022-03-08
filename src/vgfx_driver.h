// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGFX_DRIVER_H_
#define _VGFX_DRIVER_H_

#include "vgfx.h"
#include <stdio.h>
#include <stdarg.h>

#if defined(_WIN32)
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HWND__* HWND;
typedef struct IUnknown IUnknown;
#endif

#ifndef VGFX_MALLOC
#   include <stdlib.h>
#   define VGFX_MALLOC(s) malloc(s)
#   define VGFX_FREE(p) free(p)
#endif

#ifndef VGFX_ASSERT
#   include <assert.h>
#   define VGFX_ASSERT(c) assert(c)
#endif

#ifndef _VGFX_UNUSED
#define _VGFX_UNUSED(x) (void)(x)
#endif

#define _VGFX_COUNT_OF(arr) (sizeof(arr) / sizeof((arr)[0]))
#define _VGFX_DEF(val, def) (((val) == 0) ? (def) : (val))

#if defined(__clang__)
// CLANG ENABLE/DISABLE WARNING DEFINITION
#define VGFX_DISABLE_WARNINGS() \
    _Pragma("clang diagnostic push")\
	_Pragma("clang diagnostic ignored \"-Wall\"") \
	_Pragma("clang diagnostic ignored \"-Wextra\"") \
	_Pragma("clang diagnostic ignored \"-Wtautological-compare\"")

#define VGFX_ENABLE_WARNINGS() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) || defined(__GNUG__)
// GCC ENABLE/DISABLE WARNING DEFINITION
#	define VGFX_DISABLE_WARNINGS() \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wall\"") \
	_Pragma("GCC diagnostic ignored \"-Wextra\"") \
	_Pragma("GCC diagnostic ignored \"-Wtautological-compare\"")

#define VGFX_ENABLE_WARNINGS() _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define VGFX_DISABLE_WARNINGS() __pragma(warning(push, 0))
#define VGFX_ENABLE_WARNINGS() __pragma(warning(pop))
#endif

_VGFX_EXTERN void vgfxLogInfo(const char* format, ...);
_VGFX_EXTERN void vgfxLogWarn(const char* format, ...);
_VGFX_EXTERN void vgfxLogError(const char* format, ...);

#ifdef __cplusplus
#include <functional>

namespace
{

    /// Round up to next power of two.
    constexpr uint64_t vgfxNextPowerOfTwo(uint64_t value)
    {
        // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        --value;
        value |= value >> 1u;
        value |= value >> 2u;
        value |= value >> 4u;
        value |= value >> 8u;
        value |= value >> 16u;
        value |= value >> 32u;
        return ++value;
    }

    template <class T>
    void hash_combine(size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

#endif /* __cplusplus */

typedef struct VGFXRenderer VGFXRenderer;

typedef struct VGFXSurface_T
{
    VGFXSurfaceType type;
#if defined(_WIN32)
    HINSTANCE hinstance;
    HWND window;
    IUnknown* coreWindowOrSwapChainPanel;
#elif defined(__EMSCRIPTEN__)
    const char* selector;
#elif defined(__ANDROID__)
    struct ANativeWindow* window;
#elif defined(__linux__)
    void* display;
    uint32_t window;
#endif

} VGFXSurface_T;

typedef struct VGFXDevice_T
{
    void (*destroyDevice)(VGFXDevice device);
    void (*frame)(VGFXRenderer* driverData);
    void (*waitIdle)(VGFXRenderer* driverData);
    bool (*queryFeature)(VGFXRenderer* driverData, VGFXFeature feature);

    VGFXBuffer(*createBuffer)(VGFXRenderer* driverData, const VGFXBufferDesc* desc, const void* pInitialData);
    void(*destroyBuffer)(VGFXRenderer* driverData, VGFXBuffer resource);

    VGFXTexture(*createTexture)(VGFXRenderer* driverData, const VGFXTextureDesc* desc);
    void(*destroyTexture)(VGFXRenderer* driverData, VGFXTexture texture);

    VGFXSwapChain(*createSwapChain)(VGFXRenderer* driverData, VGFXSurface surface, const VGFXSwapChainDesc* desc);
    void(*destroySwapChain)(VGFXRenderer* driverData, VGFXSwapChain swapChain);
    void (*getSwapChainSize)(VGFXRenderer* driverData, VGFXSwapChain swapChain, VGFXSize2D* pSize);
    VGFXTexture(*acquireNextTexture)(VGFXRenderer* driverData, VGFXSwapChain swapChain);

    void (*beginRenderPass)(VGFXRenderer* driverData, const VGFXRenderPassDesc* desc);
    void (*endRenderPass)(VGFXRenderer* driverData);

    /* Opaque pointer for the Driver */
    VGFXRenderer* driverData;
} VGFXDevice_T;

#define ASSIGN_DRIVER_FUNC(func, name) device->func = name##_##func;

#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(destroyDevice, name) \
ASSIGN_DRIVER_FUNC(frame, name) \
ASSIGN_DRIVER_FUNC(waitIdle, name) \
ASSIGN_DRIVER_FUNC(queryFeature, name) \
ASSIGN_DRIVER_FUNC(createBuffer, name) \
ASSIGN_DRIVER_FUNC(destroyBuffer, name) \
ASSIGN_DRIVER_FUNC(createTexture, name) \
ASSIGN_DRIVER_FUNC(destroyTexture, name) \
ASSIGN_DRIVER_FUNC(createSwapChain, name) \
ASSIGN_DRIVER_FUNC(destroySwapChain, name) \
ASSIGN_DRIVER_FUNC(getSwapChainSize, name) \
ASSIGN_DRIVER_FUNC(acquireNextTexture, name) \
ASSIGN_DRIVER_FUNC(beginRenderPass, name) \
ASSIGN_DRIVER_FUNC(endRenderPass, name) \

typedef struct VGFXDriver
{
    VGFXAPI api;
    bool (*isSupported)(void);
    VGFXDevice(*createDevice)(VGFXSurface surface, const VGFXDeviceInfo* info);
} VGFXDriver;

_VGFX_EXTERN VGFXDriver vulkan_driver;
_VGFX_EXTERN VGFXDriver d3d12_driver;
_VGFX_EXTERN VGFXDriver d3d11_driver;
_VGFX_EXTERN VGFXDriver webgpu_driver;

#endif /* _VGFX_DRIVER_H_ */
