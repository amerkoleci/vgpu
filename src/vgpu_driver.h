// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGPU_DRIVER_H_
#define _VGPU_DRIVER_H_

#include "vgpu.h"
#include <stdio.h>
#include <stdarg.h>

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

_VGPU_EXTERN void vgfxLogInfo(const char* format, ...);
_VGPU_EXTERN void vgfxLogWarn(const char* format, ...);
_VGPU_EXTERN void vgfxLogError(const char* format, ...);

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
typedef struct VGPUCommandBufferImpl VGPUCommandBufferImpl;

typedef struct VGPUCommandBuffer_T {
    void (*pushDebugGroup)(VGPUCommandBufferImpl* driverData, const char* groupLabel);
    void (*popDebugGroup)(VGPUCommandBufferImpl* driverData);
    void (*insertDebugMarker)(VGPUCommandBufferImpl* driverData, const char* debugLabel);

    void (*beginRenderPass)(VGPUCommandBufferImpl* driverData, const VGFXRenderPassDesc* desc);
    void (*endRenderPass)(VGPUCommandBufferImpl* driverData);

    /* Opaque pointer for the Driver */
    VGPUCommandBufferImpl* driverData;
} VGPUCommandBuffer_T;

typedef struct VGPUDevice_T
{
    void (*destroyDevice)(VGPUDevice device);
    uint64_t(*frame)(VGFXRenderer* driverData);
    void (*waitIdle)(VGFXRenderer* driverData);
    bool (*hasFeature)(VGFXRenderer* driverData, VGPUFeature feature);
    void (*getAdapterProperties)(VGFXRenderer* driverData, VGPUAdapterProperties* properties);
    void (*getLimits)(VGFXRenderer* driverData, VGPULimits* limits);

    VGPUBuffer(*createBuffer)(VGFXRenderer* driverData, const VGFXBufferDesc* desc, const void* pInitialData);
    void(*destroyBuffer)(VGFXRenderer* driverData, VGPUBuffer resource);

    VGPUTexture(*createTexture)(VGFXRenderer* driverData, const VGFXTextureDesc* desc);
    void(*destroyTexture)(VGFXRenderer* driverData, VGPUTexture texture);

    VGPUSwapChain(*createSwapChain)(VGFXRenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc);
    void(*destroySwapChain)(VGFXRenderer* driverData, VGPUSwapChain swapChain);
    void (*getSwapChainSize)(VGFXRenderer* driverData, VGPUSwapChain swapChain, VGPUSize2D* pSize);
    VGPUTexture(*acquireNextTexture)(VGFXRenderer* driverData, VGPUSwapChain swapChain);

    VGPUCommandBuffer(*beginCommandBuffer)(VGFXRenderer* driverData, const char* label);
    void (*submit)(VGFXRenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count);

    /* Opaque pointer for the Driver */
    VGFXRenderer* driverData;
} VGPUDevice_T;

#define ASSIGN_DRIVER_FUNC(func, name) device->func = name##_##func;
#define ASSIGN_COMMAND_BUFFER_FUNC(func, name) commandBuffer->func = name##_##func;

#define ASSIGN_COMMAND_BUFFER(name) \
ASSIGN_COMMAND_BUFFER_FUNC(pushDebugGroup, name) \
ASSIGN_COMMAND_BUFFER_FUNC(popDebugGroup, name) \
ASSIGN_COMMAND_BUFFER_FUNC(insertDebugMarker, name) \
ASSIGN_COMMAND_BUFFER_FUNC(beginRenderPass, name) \
ASSIGN_COMMAND_BUFFER_FUNC(endRenderPass, name) 

#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(destroyDevice, name) \
ASSIGN_DRIVER_FUNC(frame, name) \
ASSIGN_DRIVER_FUNC(waitIdle, name) \
ASSIGN_DRIVER_FUNC(hasFeature, name) \
ASSIGN_DRIVER_FUNC(getAdapterProperties, name) \
ASSIGN_DRIVER_FUNC(getLimits, name) \
ASSIGN_DRIVER_FUNC(createBuffer, name) \
ASSIGN_DRIVER_FUNC(destroyBuffer, name) \
ASSIGN_DRIVER_FUNC(createTexture, name) \
ASSIGN_DRIVER_FUNC(destroyTexture, name) \
ASSIGN_DRIVER_FUNC(createSwapChain, name) \
ASSIGN_DRIVER_FUNC(destroySwapChain, name) \
ASSIGN_DRIVER_FUNC(getSwapChainSize, name) \
ASSIGN_DRIVER_FUNC(acquireNextTexture, name) \
ASSIGN_DRIVER_FUNC(beginCommandBuffer, name) \
ASSIGN_DRIVER_FUNC(submit, name) \

typedef struct VGFXDriver
{
    VGPUBackendType backend;
    bool (*isSupported)(void);
    VGPUDevice(*createDevice)(const VGPUDeviceDesc* desc);
} VGFXDriver;

_VGPU_EXTERN VGFXDriver Vulkan_Driver;
_VGPU_EXTERN VGFXDriver D3D12_Driver;
_VGPU_EXTERN VGFXDriver D3D11_Driver;
_VGPU_EXTERN VGFXDriver webgpu_driver;

#endif /* _VGPU_DRIVER_H_ */
