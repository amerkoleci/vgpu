// Copyright © Amer Koleci.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGPU_DRIVER_H_
#define _VGPU_DRIVER_H_ 1

#include "vgpu.h"
#include <assert.h>
#include <atomic>

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
#define VGPU_UNREACHABLE() __builtin_unreachable()
#define VGPU_DEBUG_BREAK() __builtin_trap()

// CLANG ENABLE/DISABLE WARNING DEFINITION
#define VGPU_DISABLE_WARNINGS() \
    _Pragma("clang diagnostic push")\
	_Pragma("clang diagnostic ignored \"-Wall\"") \
	_Pragma("clang diagnostic ignored \"-Wextra\"") \
	_Pragma("clang diagnostic ignored \"-Wtautological-compare\"")

#define VGPU_ENABLE_WARNINGS() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) || defined(__GNUG__)
#define VGPU_UNREACHABLE() __builtin_unreachable()
#define VGPU_DEBUG_BREAK() __builtin_trap()
// GCC ENABLE/DISABLE WARNING DEFINITION
#	define VGPU_DISABLE_WARNINGS() \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wall\"") \
	_Pragma("GCC diagnostic ignored \"-Wextra\"") \
	_Pragma("GCC diagnostic ignored \"-Wtautological-compare\"")

#define VGPU_ENABLE_WARNINGS() _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define VGPU_UNREACHABLE() __assume(false)
#define VGPU_DEBUG_BREAK() __debugbreak()
#define VGPU_DISABLE_WARNINGS() __pragma(warning(push, 0))
#define VGPU_ENABLE_WARNINGS() __pragma(warning(pop))
#endif

_VGPU_EXTERN void vgpu_log_info(const char* format, ...);
_VGPU_EXTERN void vgpu_log_warn(const char* format, ...);
_VGPU_EXTERN void vgpu_log_error(const char* format, ...);

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

typedef struct VGPURenderer VGPURenderer;
typedef struct VGPUCommandBufferImpl VGPUCommandBufferImpl;

class VGPUObject
{
protected:
    VGPUObject() = default;

public:
    virtual ~VGPUObject() = default;

    // Non-copyable and non-movable
    VGPUObject(const VGPUObject&) = delete;
    VGPUObject(const VGPUObject&&) = delete;
    VGPUObject& operator=(const VGPUObject&) = delete;
    VGPUObject& operator=(const VGPUObject&&) = delete;

    virtual uint32_t AddRef()
    {
        return ++refCount;
    }

    virtual uint32_t Release()
    {
        uint32_t result = --refCount;
        if (result == 0) {
            delete this;
        }
        return result;
    }

    virtual void SetLabel(const char* label) = 0;

private:
    std::atomic<uint32_t> refCount = 1;
};

struct VGPUBufferImpl : public VGPUObject
{
public:
    virtual uint64_t GetSize() const = 0;
    virtual VGPUBufferUsageFlags GetUsage() const = 0;
    virtual VGPUDeviceAddress GetGpuAddress() const = 0;
};

struct VGPUTextureImpl : public VGPUObject
{
public:
    virtual VGPUTextureDimension GetDimension() const = 0;
};

struct VGPUSamplerImpl : public VGPUObject
{
public:
};

struct VGPUPipelineLayoutImpl : public VGPUObject
{
public:
};

struct VGPUPipelineImpl : public VGPUObject
{
public:
    virtual VGPUPipelineType GetType() const = 0;
};

struct VGPUQueryHeapImpl : public VGPUObject
{
public:
};

struct VGPUSwapChainImpl : public VGPUObject
{
public:
    virtual VGPUTextureFormat GetFormat() const = 0;
};

struct VGPUCommandBufferImpl
{
public:
    virtual ~VGPUCommandBufferImpl() = default;

    virtual void pushDebugGroup(const char* groupLabel) = 0;
    virtual void popDebugGroup() = 0;
    virtual void insertDebugMarker(const char* markerLabel) = 0;

    virtual void SetPipeline(VGPUPipeline pipeline) = 0;
    virtual void SetPushConstants(uint32_t pushConstantIndex, const void* data, uint32_t size) = 0;

    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void DispatchIndirect(VGPUBuffer buffer, uint64_t offset) = 0;

    virtual VGPUTexture acquireSwapchainTexture(VGPUSwapChain swapChain, uint32_t* pWidth, uint32_t* pHeight) = 0;
    virtual void beginRenderPass(const VGPURenderPassDesc* desc) = 0;
    virtual void endRenderPass() = 0;

    virtual void setViewport(const VGPUViewport* viewport) = 0;
    virtual void setViewports(uint32_t count, const VGPUViewport* viewports) = 0;
    virtual void SetScissorRect(const VGPURect* rect) = 0;
    virtual void SetScissorRects(uint32_t count, const VGPURect* rects) = 0;

    virtual void setVertexBuffer(uint32_t index, VGPUBuffer buffer, uint64_t offset) = 0;
    virtual void setIndexBuffer(VGPUBuffer buffer, VGPUIndexType type, uint64_t offset) = 0;
    virtual void setStencilReference(uint32_t reference) = 0;

    virtual void BeginQuery(VGPUQueryHeap heap, uint32_t index) = 0;
    virtual void EndQuery(VGPUQueryHeap heap, uint32_t index) = 0;
    virtual void ResolveQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count, VGPUBuffer destinationBuffer, uint64_t destinationOffset) = 0;
    virtual void ResetQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count) = 0;

    virtual void draw(uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) = 0;
};

typedef struct VGPUDeviceImpl
{
    void (*destroyDevice)(VGPUDevice device);
    void(*setLabel)(VGPURenderer* driverData, const char* label);

    void (*waitIdle)(VGPURenderer* driverData);
    VGPUBackend(*getBackendType)(void);
    VGPUBool32(*queryFeature)(VGPURenderer* driverData, VGPUFeature feature);
    void (*getAdapterProperties)(VGPURenderer* driverData, VGPUAdapterProperties* properties);
    void (*getLimits)(VGPURenderer* driverData, VGPULimits* limits);

    VGPUBuffer(*createBuffer)(VGPURenderer* driverData, const VGPUBufferDescriptor* desc, const void* pInitialData);

    VGPUTexture(*createTexture)(VGPURenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData);

    VGPUSampler(*createSampler)(VGPURenderer* driverData, const VGPUSamplerDesc* desc);

    VGPUShaderModule(*createShaderModule)(VGPURenderer* driverData, const void* pCode, size_t codeSize);
    void(*destroyShaderModule)(VGPURenderer* driverData, VGPUShaderModule resource);

    VGPUPipelineLayout(*createPipelineLayout)(VGPURenderer* driverData, const VGPUPipelineLayoutDescriptor* desc);

    VGPUPipeline(*createRenderPipeline)(VGPURenderer* driverData, const VGPURenderPipelineDescriptor* desc);
    VGPUPipeline(*createComputePipeline)(VGPURenderer* driverData, const VGPUComputePipelineDescriptor* desc);
    VGPUPipeline(*createRayTracingPipeline)(VGPURenderer* driverData, const VGPURayTracingPipelineDescriptor* desc);

    VGPUQueryHeap(*createQueryHeap)(VGPURenderer* driverData, const VGPUQueryHeapDescriptor* descriptor);

    VGPUSwapChain(*createSwapChain)(VGPURenderer* driverData, void* windowHandle, const VGPUSwapChainDescriptor* desc);

    VGPUCommandBuffer(*beginCommandBuffer)(VGPURenderer* driverData, VGPUCommandQueue queueType, const char* label);
    uint64_t(*submit)(VGPURenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count);

    uint64_t(*getFrameCount)(VGPURenderer* driverData);
    uint32_t(*getFrameIndex)(VGPURenderer* driverData);

    /* Opaque pointer for the Driver */
    VGPURenderer* driverData;
} VGPUDeviceImpl;

#define ASSIGN_DRIVER_FUNC(func, name) device->func = name##_##func;

#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(destroyDevice, name) \
ASSIGN_DRIVER_FUNC(setLabel, name) \
ASSIGN_DRIVER_FUNC(waitIdle, name) \
ASSIGN_DRIVER_FUNC(getBackendType, name) \
ASSIGN_DRIVER_FUNC(queryFeature, name) \
ASSIGN_DRIVER_FUNC(getAdapterProperties, name) \
ASSIGN_DRIVER_FUNC(getLimits, name) \
ASSIGN_DRIVER_FUNC(createBuffer, name) \
ASSIGN_DRIVER_FUNC(createTexture, name) \
ASSIGN_DRIVER_FUNC(createSampler, name) \
ASSIGN_DRIVER_FUNC(createShaderModule, name) \
ASSIGN_DRIVER_FUNC(destroyShaderModule, name) \
ASSIGN_DRIVER_FUNC(createPipelineLayout, name) \
ASSIGN_DRIVER_FUNC(createRenderPipeline, name) \
ASSIGN_DRIVER_FUNC(createComputePipeline, name) \
ASSIGN_DRIVER_FUNC(createRayTracingPipeline, name) \
ASSIGN_DRIVER_FUNC(createQueryHeap, name) \
ASSIGN_DRIVER_FUNC(createSwapChain, name) \
ASSIGN_DRIVER_FUNC(beginCommandBuffer, name) \
ASSIGN_DRIVER_FUNC(submit, name) \
ASSIGN_DRIVER_FUNC(getFrameCount, name) \
ASSIGN_DRIVER_FUNC(getFrameIndex, name) \

typedef struct VGPUDriver
{
    VGPUBackend backend;
    VGPUBool32(*isSupported)(void);
    VGPUDeviceImpl* (*createDevice)(const VGPUDeviceDescriptor* descriptor);
} VGPUDriver;

_VGPU_EXTERN VGPUDriver Vulkan_Driver;
_VGPU_EXTERN VGPUDriver D3D11_Driver;
_VGPU_EXTERN VGPUDriver D3D12_Driver;
//_VGPU_EXTERN VGPUDriver WebGPU_driver;

#endif /* _VGPU_DRIVER_H_ */
