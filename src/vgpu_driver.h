// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGPU_DRIVER_H_
#define _VGPU_DRIVER_H_

#include "vgpu.h"
#include <stdbool.h>
#include <string.h> 
#include <atomic>
#include <functional>

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
#define VGPU_FORCE_INLINE inline __attribute__((__always_inline__))

// CLANG ENABLE/DISABLE WARNING DEFINITION
#define VGPU_DISABLE_WARNINGS() \
    _Pragma("clang diagnostic push")\
	_Pragma("clang diagnostic ignored \"-Wall\"") \
	_Pragma("clang diagnostic ignored \"-Wextra\"") \
	_Pragma("clang diagnostic ignored \"-Wtautological-compare\"") \
    _Pragma("clang diagnostic ignored \"-Wnullability-completeness\"") \
    _Pragma("clang diagnostic ignored \"-Wnullability-extension\"") \
    _Pragma("clang diagnostic ignored \"-Wunused-parameter\"") \
    _Pragma("clang diagnostic ignored \"-Wunused-function\"")

#define VGPU_ENABLE_WARNINGS() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) || defined(__GNUG__)
#define VGPU_UNREACHABLE() __builtin_unreachable()
#define VGPU_DEBUG_BREAK() __builtin_trap()
#define VGPU_FORCE_INLINE inline __attribute__((__always_inline__))
// GCC ENABLE/DISABLE WARNING DEFINITION
#	define VGPU_DISABLE_WARNINGS() \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wall\"") \
	_Pragma("GCC diagnostic ignored \"-Wextra\"") \
	_Pragma("GCC diagnostic ignored \"-Wtautological-compare\"") \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
    _Pragma("GCC diagnostic ignored \"-Wunused-function\"")

#define VGPU_ENABLE_WARNINGS() _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define VGPU_UNREACHABLE() __assume(false)
#define VGPU_DEBUG_BREAK() __debugbreak()
#define VGPU_FORCE_INLINE __forceinline
#define VGPU_DISABLE_WARNINGS() __pragma(warning(push, 0))
#define VGPU_ENABLE_WARNINGS() __pragma(warning(pop))
#endif

_VGPU_EXTERN bool vgpuShouldLog(VGPULogLevel level);
_VGPU_EXTERN void vgpuLogInfo(const char* format, ...);
_VGPU_EXTERN void vgpuLogWarn(const char* format, ...);
_VGPU_EXTERN void vgpuLogError(const char* format, ...);

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

    template <typename T>
    VGPU_FORCE_INLINE T DivideByMultiple(T value, size_t alignment)
    {
        return (T)((value + alignment - 1) / alignment);
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

    virtual void* GetNativeObject(VGPUNativeObjectType objectType) const { (void)objectType; return nullptr; }

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
    virtual VGPUTextureFormat GetFormat() const = 0;
};

struct VGPUSamplerImpl : public VGPUObject
{
public:
};

struct VGPUBindGroupLayoutImpl : public VGPUObject
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
    virtual VGPUQueryType GetType() const = 0;
    virtual uint32_t GetCount() const = 0;
};

struct VGPUSwapChainImpl : public VGPUObject
{
public:
    virtual VGPUTextureFormat GetFormat() const = 0;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
};

struct VGPUCommandBufferImpl
{
public:
    virtual ~VGPUCommandBufferImpl() = default;

    virtual void PushDebugGroup(const char* groupLabel) = 0;
    virtual void PopDebugGroup() = 0;
    virtual void InsertDebugMarker(const char* markerLabel) = 0;

    virtual void SetPipeline(VGPUPipeline pipeline) = 0;
    virtual void SetPushConstants(uint32_t pushConstantIndex, const void* data, uint32_t size) = 0;

    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void DispatchIndirect(VGPUBuffer buffer, uint64_t offset) = 0;

    virtual VGPUTexture AcquireSwapchainTexture(VGPUSwapChain swapChain) = 0;
    virtual void BeginRenderPass(const VGPURenderPassDesc* desc) = 0;
    virtual void EndRenderPass() = 0;

    virtual void SetViewport(const VGPUViewport* viewport) = 0;
    virtual void SetViewports(uint32_t count, const VGPUViewport* viewports) = 0;
    virtual void SetScissorRect(const VGPURect* rect) = 0;
    virtual void SetScissorRects(uint32_t count, const VGPURect* rects) = 0;

    virtual void SetVertexBuffer(uint32_t index, VGPUBuffer buffer, uint64_t offset) = 0;
    virtual void SetIndexBuffer(VGPUBuffer buffer, VGPUIndexType type, uint64_t offset) = 0;
    virtual void SetStencilReference(uint32_t reference) = 0;

    virtual void BeginQuery(VGPUQueryHeap heap, uint32_t index) = 0;
    virtual void EndQuery(VGPUQueryHeap heap, uint32_t index) = 0;
    virtual void ResolveQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count, VGPUBuffer destinationBuffer, uint64_t destinationOffset) = 0;
    virtual void ResetQuery(VGPUQueryHeap heap, uint32_t index, uint32_t count) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) = 0;
    virtual void DrawIndirect(VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset) = 0;
    virtual void DrawIndexedIndirect(VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset) = 0;
};

struct VGPUDeviceImpl : public VGPUObject
{
    virtual void WaitIdle() = 0;
    virtual VGPUBackend GetBackendType() const = 0;
    virtual VGPUBool32 QueryFeatureSupport(VGPUFeature feature) const = 0;
    virtual void GetAdapterProperties(VGPUAdapterProperties* properties) const = 0;
    virtual void GetLimits(VGPULimits* limits) const = 0;
    virtual uint64_t GetTimestampFrequency() const = 0;

    virtual VGPUBuffer CreateBuffer(const VGPUBufferDesc* desc, const void* pInitialData) = 0;
    virtual VGPUTexture CreateTexture(const VGPUTextureDesc* desc, const void* pInitialData) = 0;

    virtual VGPUSampler CreateSampler(const VGPUSamplerDesc* desc) = 0;

    virtual VGPUBindGroupLayout CreateBindGroupLayout(const VGPUBindGroupLayoutDesc* desc) = 0;
    virtual VGPUPipelineLayout CreatePipelineLayout(const VGPUPipelineLayoutDesc* desc) = 0;

    virtual VGPUPipeline CreateRenderPipeline(const VGPURenderPipelineDesc* desc) = 0;
    virtual VGPUPipeline CreateComputePipeline(const VGPUComputePipelineDesc* desc) = 0;
    virtual VGPUPipeline CreateRayTracingPipeline(const VGPURayTracingPipelineDesc* desc) = 0;

    virtual VGPUQueryHeap CreateQueryHeap(const VGPUQueryHeapDesc* desc) = 0;

    virtual VGPUSwapChain CreateSwapChain(const VGPUSwapChainDesc* desc) = 0;

    virtual VGPUCommandBuffer BeginCommandBuffer(VGPUCommandQueue queueType, const char* label) = 0;
    virtual uint64_t Submit(VGPUCommandBuffer* commandBuffers, uint32_t count) = 0;

    virtual uint64_t GetFrameCount() = 0;
    virtual uint32_t GetFrameIndex() = 0;
};

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
