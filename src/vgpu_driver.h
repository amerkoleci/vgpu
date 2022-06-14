// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGPU_DRIVER_H_
#define _VGPU_DRIVER_H_

#include "vgpu.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef VGPU_MALLOC
#   include <stdlib.h>
#   define VGPU_MALLOC(s) malloc(s)
#   define VGPU_FREE(p) free(p)
#endif

#ifndef VGPU_ASSERT
#   include <assert.h>
#   define VGPU_ASSERT(c) assert(c)
#endif

#define _VGPU_UNUSED(x) (void)(x)

#define _VGPU_COUNT_OF(arr) (sizeof(arr) / sizeof((arr)[0]))
#define _VGPU_DEF(val, def) (((val) == 0) ? (def) : (val))
#define _VGPU_MIN(a,b) (((a)<(b))?(a):(b))
#define _VGPU_MAX(a,b) (((a)>(b))?(a):(b))

#define _VGPU_DivideByMultiple(value, alignment) (value + alignment - 1) / alignment

#if defined(__clang__)
// CLANG ENABLE/DISABLE WARNING DEFINITION
#define VGPU_DISABLE_WARNINGS() \
    _Pragma("clang diagnostic push")\
	_Pragma("clang diagnostic ignored \"-Wall\"") \
	_Pragma("clang diagnostic ignored \"-Wextra\"") \
	_Pragma("clang diagnostic ignored \"-Wtautological-compare\"")

#define VGPU_ENABLE_WARNINGS() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) || defined(__GNUG__)
// GCC ENABLE/DISABLE WARNING DEFINITION
#	define VGPU_DISABLE_WARNINGS() \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wall\"") \
	_Pragma("GCC diagnostic ignored \"-Wextra\"") \
	_Pragma("GCC diagnostic ignored \"-Wtautological-compare\"")

#define VGPU_ENABLE_WARNINGS() _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define VGPU_DISABLE_WARNINGS() __pragma(warning(push, 0))
#define VGPU_ENABLE_WARNINGS() __pragma(warning(pop))
#endif

_VGPU_EXTERN void vgpuLogInfo(const char* format, ...);
_VGPU_EXTERN void vgpuLogWarn(const char* format, ...);
_VGPU_EXTERN void vgpuLogError(const char* format, ...);

#ifdef __cplusplus
#include <functional>

namespace
{
    template <typename T>
    static bool IsPow2(T x) { return (x & (x - 1)) == 0; }

    template <typename T>
    static T AlignUp(T val, T alignment)
    {
        VGPU_ASSERT(IsPow2(alignment));
        return (val + alignment - 1) & ~(alignment - 1);
    }

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

    void (*setPipeline)(VGPUCommandBufferImpl* driverData, VGPUPipeline pipeline);
    void (*dispatch)(VGPUCommandBufferImpl* driverData, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void (*dispatchIndirect)(VGPUCommandBufferImpl* driverData, VGPUBuffer indirectBuffer, uint32_t indirectBufferOffset);

    VGPUTexture(*acquireSwapchainTexture)(VGPUCommandBufferImpl* driverData, VGPUSwapChain swapChain, uint32_t* pWidth, uint32_t* pHeight);
    void (*beginRenderPass)(VGPUCommandBufferImpl* driverData, const VGPURenderPassDesc* desc);
    void (*endRenderPass)(VGPUCommandBufferImpl* driverData);

    void (*setViewports)(VGPUCommandBufferImpl* driverData, const VGPUViewport* viewports, uint32_t count);
    void (*setScissorRects)(VGPUCommandBufferImpl* driverData, const VGPURect* scissorRects, uint32_t count);
    void (*setVertexBuffer)(VGPUCommandBufferImpl* driverData, uint32_t index, VGPUBuffer buffer, uint64_t offset);
    void (*setIndexBuffer)(VGPUCommandBufferImpl* driverData, VGPUBuffer buffer, uint64_t offset, VGPUIndexType indexType);

    void (*draw)(VGPUCommandBufferImpl* driverData, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance);

    /* Opaque pointer for the Driver */
    VGPUCommandBufferImpl* driverData;
} VGPUCommandBuffer_T;

typedef struct VGPUDevice_T
{
    void (*destroyDevice)(VGPUDevice device);
    uint64_t(*frame)(VGFXRenderer* driverData);
    void (*waitIdle)(VGFXRenderer* driverData);
    VGPUBackendType(*getBackendType)(void);
    bool (*queryFeature)(VGFXRenderer* driverData, VGPUFeature feature, void* pInfo, uint32_t infoSize);
    void (*getAdapterProperties)(VGFXRenderer* driverData, VGPUAdapterProperties* properties);
    void (*getLimits)(VGFXRenderer* driverData, VGPULimits* limits);

    VGPUBuffer(*createBuffer)(VGFXRenderer* driverData, const VGPUBufferDesc* desc, const void* pInitialData);
    void(*destroyBuffer)(VGFXRenderer* driverData, VGPUBuffer resource);

    VGPUTexture(*createTexture)(VGFXRenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData);
    void(*destroyTexture)(VGFXRenderer* driverData, VGPUTexture resource);

    VGPUSampler(*createSampler)(VGFXRenderer* driverData, const VGPUSamplerDesc* desc);
    void(*destroySampler)(VGFXRenderer* driverData, VGPUSampler resource);

    VGPUShaderModule(*createShaderModule)(VGFXRenderer* driverData, const void* pCode, size_t codeSize);
    void(*destroyShaderModule)(VGFXRenderer* driverData, VGPUShaderModule resource);

    VGPUPipeline(*createRenderPipeline)(VGFXRenderer* driverData, const VGPURenderPipelineDesc* desc);
    VGPUPipeline(*createComputePipeline)(VGFXRenderer* driverData, const VGPUComputePipelineDesc* desc);
    VGPUPipeline(*createRayTracingPipeline)(VGFXRenderer* driverData, const VGPURayTracingPipelineDesc* desc);
    void(*destroyPipeline)(VGFXRenderer* driverData, VGPUPipeline resource);

    VGPUSwapChain(*createSwapChain)(VGFXRenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc);
    void(*destroySwapChain)(VGFXRenderer* driverData, VGPUSwapChain swapChain);
    VGPUTextureFormat(*getSwapChainFormat)(VGFXRenderer* driverData, VGPUSwapChain swapChain);

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
ASSIGN_COMMAND_BUFFER_FUNC(setPipeline, name) \
ASSIGN_COMMAND_BUFFER_FUNC(dispatch, name) \
ASSIGN_COMMAND_BUFFER_FUNC(dispatchIndirect, name) \
ASSIGN_COMMAND_BUFFER_FUNC(acquireSwapchainTexture, name) \
ASSIGN_COMMAND_BUFFER_FUNC(beginRenderPass, name) \
ASSIGN_COMMAND_BUFFER_FUNC(endRenderPass, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setViewports, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setScissorRects, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setVertexBuffer, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setIndexBuffer, name) \
ASSIGN_COMMAND_BUFFER_FUNC(draw, name) 

#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(destroyDevice, name) \
ASSIGN_DRIVER_FUNC(frame, name) \
ASSIGN_DRIVER_FUNC(waitIdle, name) \
ASSIGN_DRIVER_FUNC(getBackendType, name) \
ASSIGN_DRIVER_FUNC(queryFeature, name) \
ASSIGN_DRIVER_FUNC(getAdapterProperties, name) \
ASSIGN_DRIVER_FUNC(getLimits, name) \
ASSIGN_DRIVER_FUNC(createBuffer, name) \
ASSIGN_DRIVER_FUNC(destroyBuffer, name) \
ASSIGN_DRIVER_FUNC(createTexture, name) \
ASSIGN_DRIVER_FUNC(destroyTexture, name) \
ASSIGN_DRIVER_FUNC(createSampler, name) \
ASSIGN_DRIVER_FUNC(destroySampler, name) \
ASSIGN_DRIVER_FUNC(createShaderModule, name) \
ASSIGN_DRIVER_FUNC(destroyShaderModule, name) \
ASSIGN_DRIVER_FUNC(createRenderPipeline, name) \
ASSIGN_DRIVER_FUNC(createComputePipeline, name) \
ASSIGN_DRIVER_FUNC(createRayTracingPipeline, name) \
ASSIGN_DRIVER_FUNC(destroyPipeline, name) \
ASSIGN_DRIVER_FUNC(createSwapChain, name) \
ASSIGN_DRIVER_FUNC(destroySwapChain, name) \
ASSIGN_DRIVER_FUNC(getSwapChainFormat, name) \
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
//_VGPU_EXTERN VGFXDriver webgpu_driver;

#endif /* _VGPU_DRIVER_H_ */
