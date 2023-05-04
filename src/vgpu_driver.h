// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef VGPU_DRIVER_H_
#define VGPU_DRIVER_H_ 1

#include "vgpu.h"
#include <stdlib.h> // malloc, free
#include <string.h> // memset
#include <float.h> // FLT_MAX
#include <assert.h>

// Custom allocation callbacks.
_VGPU_EXTERN const vgpu_allocation_callbacks* VGPU_ALLOC_CB;

#define VGPU_ALLOC(type)        ((type*)_vgpu_alloc(sizeof(type)))
#define VGPU_ALLOC_CLEAR(type)  ((type*)_vgpu_alloc_clear(sizeof(type)))
#define VGPU_FREE(ptr)          (VGPU_ALLOC_CB->free(ptr, VGPU_ALLOC_CB->user_data))

_VGPU_EXTERN void* _vgpu_alloc(size_t size);
_VGPU_EXTERN void* _vgpu_alloc_clear(size_t size);
_VGPU_EXTERN void _vgpu_free(void* ptr);

#ifndef VGPU_ASSERT
#   include <assert.h>
#   define VGPU_ASSERT(c) assert(c)
#endif

#if (defined(_DEBUG) || defined(PROFILE))
#   define VGPU_VERIFY(cond) VGPU_ASSERT(cond)
#else
#   define VGPU_VERIFY(cond) (void)(cond)
#endif

#define VGPU_UNUSED(x) (void)(x)

#define _VGPU_COUNT_OF(arr) (sizeof(arr) / sizeof((arr)[0]))
#define _VGPU_DEF(val, def) (((val) == 0) ? (def) : (val))
#define _VGPU_MIN(a,b) (((a)<(b))?(a):(b))
#define _VGPU_MAX(a,b) (((a)>(b))?(a):(b))

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

_VGPU_EXTERN void vgpu_log_info(const char* format, ...);
_VGPU_EXTERN void vgpu_log_warn(const char* format, ...);
_VGPU_EXTERN void vgpu_log_error(const char* format, ...);

#ifdef __cplusplus
#include <vector>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <functional>

namespace
{
    /// Round up to next power of two.
    constexpr uint64_t vgpuNextPowerOfTwo(uint64_t value)
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

typedef struct VGPURenderer VGPURenderer;
typedef struct VGPUCommandBufferImpl VGPUCommandBufferImpl;

typedef struct VGPUCommandBuffer_T {
    void (*pushDebugGroup)(VGPUCommandBufferImpl* driverData, const char* groupLabel);
    void (*popDebugGroup)(VGPUCommandBufferImpl* driverData);
    void (*insertDebugMarker)(VGPUCommandBufferImpl* driverData, const char* debugLabel);

    void (*setPipeline)(VGPUCommandBufferImpl* driverData, VGPUPipeline* pipeline);
    void (*dispatch)(VGPUCommandBufferImpl* driverData, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void (*dispatchIndirect)(VGPUCommandBufferImpl* driverData, vgpu_buffer* buffer, uint64_t offset);

    VGPUTexture(*acquireSwapchainTexture)(VGPUCommandBufferImpl* driverData, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight);
    void (*beginRenderPass)(VGPUCommandBufferImpl* driverData, const VGPURenderPassDesc* desc);
    void (*endRenderPass)(VGPUCommandBufferImpl* driverData);

    void (*setViewport)(VGPUCommandBufferImpl* driverData, const VGPUViewport* viewport);
    void (*setScissorRect)(VGPUCommandBufferImpl* driverData, const VGPURect* rects);
    void (*setVertexBuffer)(VGPUCommandBufferImpl* driverData, uint32_t index, vgpu_buffer* buffer, uint64_t offset);
    void (*setIndexBuffer)(VGPUCommandBufferImpl* driverData, vgpu_buffer* buffer, uint64_t offset, vgpu_index_type type);

    void (*draw)(VGPUCommandBufferImpl* driverData, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance);
    void (*drawIndexed)(VGPUCommandBufferImpl* driverData, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);

    /* Opaque pointer for the Driver */
    VGPUCommandBufferImpl* driverData;
} VGPUCommandBuffer_T;

typedef struct VGPUDeviceImpl
{
    void (*destroyDevice)(VGPUDevice device);
    uint64_t(*frame)(VGPURenderer* driverData);
    void (*waitIdle)(VGPURenderer* driverData);
    vgpu_backend(*getBackendType)(void);
    VGPUBool32(*queryFeature)(VGPURenderer* driverData, VGPUFeature feature, void* pInfo, uint32_t infoSize);
    void (*getAdapterProperties)(VGPURenderer* driverData, VGPUAdapterProperties* properties);
    void (*getLimits)(VGPURenderer* driverData, VGPULimits* limits);

    vgpu_buffer*(*createBuffer)(VGPURenderer* driverData, const vgpu_buffer_desc* desc, const void* pInitialData);
    void(*destroyBuffer)(VGPURenderer* driverData, vgpu_buffer* resource);
    VGPUDeviceAddress(*buffer_get_device_address)(vgpu_buffer* resource);
    void(*buffer_set_label)(VGPURenderer* driverData, vgpu_buffer* resource, const char* label);

    VGPUTexture(*createTexture)(VGPURenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData);
    void(*destroyTexture)(VGPURenderer* driverData, VGPUTexture resource);

    VGPUSampler*(*createSampler)(VGPURenderer* driverData, const VGPUSamplerDesc* desc);
    void(*destroySampler)(VGPURenderer* driverData, VGPUSampler* resource);

    VGPUShaderModule(*createShaderModule)(VGPURenderer* driverData, const void* pCode, size_t codeSize);
    void(*destroyShaderModule)(VGPURenderer* driverData, VGPUShaderModule resource);

    VGPUPipeline*(*createRenderPipeline)(VGPURenderer* driverData, const VGPURenderPipelineDesc* desc);
    VGPUPipeline*(*createComputePipeline)(VGPURenderer* driverData, const VGPUComputePipelineDesc* desc);
    VGPUPipeline*(*createRayTracingPipeline)(VGPURenderer* driverData, const VGPURayTracingPipelineDesc* desc);
    void(*destroyPipeline)(VGPURenderer* driverData, VGPUPipeline* resource);

    VGPUSwapChain*(*createSwapChain)(VGPURenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc);
    void(*destroySwapChain)(VGPURenderer* driverData, VGPUSwapChain* swapChain);
    vgpu_pixel_format(*getSwapChainFormat)(VGPURenderer* driverData, VGPUSwapChain* swapChain);

    VGPUCommandBuffer(*beginCommandBuffer)(VGPURenderer* driverData, VGPUCommandQueue queueType, const char* label);
    void (*submit)(VGPURenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count);

    /* Opaque pointer for the Driver */
    VGPURenderer* driverData;
} VGPUDeviceImpl;

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
ASSIGN_COMMAND_BUFFER_FUNC(setViewport, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setScissorRect, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setVertexBuffer, name) \
ASSIGN_COMMAND_BUFFER_FUNC(setIndexBuffer, name) \
ASSIGN_COMMAND_BUFFER_FUNC(draw, name) \
ASSIGN_COMMAND_BUFFER_FUNC(drawIndexed, name)

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
ASSIGN_DRIVER_FUNC(buffer_get_device_address, name) \
ASSIGN_DRIVER_FUNC(buffer_set_label, name) \
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
    vgpu_backend backend;
    VGPUBool32(*is_supported)(void);
    VGPUDeviceImpl*(*createDevice)(const VGPUDeviceDescriptor* descriptor);
} VGFXDriver;

_VGPU_EXTERN VGFXDriver Vulkan_Driver;
_VGPU_EXTERN VGFXDriver D3D11_Driver;
_VGPU_EXTERN VGFXDriver D3D12_Driver;
//_VGPU_EXTERN VGFXDriver WebGPU_driver;

#endif /* VGPU_DRIVER_H_ */
