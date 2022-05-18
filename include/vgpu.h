// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGPU_H
#define _VGPU_H

#if defined(VGPU_SHARED_LIBRARY)
#    if defined(_WIN32)
#        if defined(VGPU_IMPLEMENTATION)
#            define _VGPU_EXPORT __declspec(dllexport)
#        else
#            define _VGPU_EXPORT __declspec(dllimport)
#        endif
#    else 
#        if defined(VGPU_IMPLEMENTATION)
#            define _VGPU_EXPORT __attribute__((visibility("default")))
#        else
#            define _VGPU_EXPORT
#        endif
#    endif
#else
#    define _VGPU_EXPORT
#endif

#ifdef __cplusplus
#    define _VGPU_EXTERN extern "C"
#else
#    define _VGPU_EXTERN extern
#endif

#define VGPU_API _VGPU_EXTERN _VGPU_EXPORT

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#   define VGPU_CALL __cdecl
#else
#   define VGPU_CALL
#endif

/* Version API */
#define VGPU_VERSION_MAJOR  0
#define VGPU_VERSION_MINOR	1
#define VGPU_VERSION_PATCH	0

enum {
    VGPU_MAX_INFLIGHT_FRAMES = 2,
    VGPU_MAX_COLOR_ATTACHMENTS = 8,
    VGPU_MAX_VERTEX_BUFFERS = 8,
    VGPU_MAX_VERTEX_ATTRIBUTES = 16,
};

typedef struct VGPUDevice_T* VGPUDevice;
typedef struct VGPUBuffer_T* VGPUBuffer;
typedef struct VGPUTexture_T* VGPUTexture;
typedef struct VGPUSampler_T* VGPUSampler;
typedef struct VGPUSwapChain_T* VGPUSwapChain;
typedef struct VGPUCommandBuffer_T* VGPUCommandBuffer;

typedef enum VGPULogLevel {
    VGPU_LOG_LEVEL_INFO = 0,
    VGPU_LOG_LEVEL_WARN,
    VGPU_LOG_LEVEL_ERROR,

    _VGPU_LOG_LEVEL_COUNT,
    _VGPU_LOG_LEVEL_FORCE_U32 = 0x7FFFFFFF
} VGPULogLevel;

typedef enum VGPUBackendType {
    VGPU_BACKEND_TYPE_DEFAULT = 0,
    VGPU_BACKEND_TYPE_VULKAN,
    VGPU_BACKEND_TYPE_D3D12,
    VGPU_BACKEND_TYPE_D3D11,
    VGPU_BACKEND_TYPE_WEBGPU,

    _VGPU_BACKEND_TYPE_COUNT,
    _VGPU_BACKEND_TYPE_FORCE_U32 = 0x7FFFFFFF
} VGPUBackendType;

typedef enum VGPUValidationMode {
    /// No validation is enabled.
    VGPU_VALIDATION_MODE_DISABLED = 0,
    /// Print warnings and errors
    VGPU_VALIDATION_MODE_ENABLED,
    /// Print all warnings, errors and info messages
    VGPU_VALIDATION_MODE_VERBOSE,
    /// Enable GPU-based validation
    VGPU_VALIDATION_MODE_GPU,

    _VGPU_VALIDATION_MODE_COUNT,
    _VGPU_VALIDATION_MODE_FORCE_U32 = 0x7FFFFFFF
} VGPUValidationMode;

typedef enum VGPUAdapterType {
    VGPU_ADAPTER_TYPE_OTHER = 0,
    VGPU_ADAPTER_TYPE_INTEGRATED_GPU,
    VGPU_ADAPTER_TYPE_DISCRETE_GPU,
    VGPU_ADAPTER_TYPE_VIRTUAL_GPU,
    VGPU_ADAPTER_TYPE_CPU,

    _VGPU_ADAPTER_TYPE_COUNT,
    _VGPU_ADAPTER_TYPE_FORCE_U32 = 0x7FFFFFFF
} VGPUAdapterType;

typedef enum VGPUTextureType {
    _VGPU_TEXTURE_TYPE_DEFAULT = 0,
    VGPU_TEXTURE_TYPE_1D,
    VGPU_TEXTURE_TYPE_2D,
    VGPU_TEXTURE_TYPE_3D,

    _VGPU_TEXTURE_TYPE_COUNT,
    _VGPU_TEXTURE_TYPE_FORCE_U32 = 0x7FFFFFFF
} VGPUTextureType;

typedef enum VGPUBufferUsage {
    VGPU_BUFFER_USAGE_NONE = 0x00,
    VGPU_BUFFER_USAGE_VERTEX = 0x01,
    VGPU_BUFFER_USAGE_INDEX = 0x02,
    VGPU_BUFFER_USAGE_UNIFORM = 0x04,
    VGPU_BUFFER_USAGE_SHADER_READ = 0x08,
    VGPU_BUFFER_USAGE_SHADER_WRITE = 0x10,
    VGPU_BUFFER_USAGE_INDIRECT = 0x20,

    _VGPU_BUFFER_USAGE_FORCE_U32 = 0x7FFFFFFF
} VGPUBufferUsage;

typedef enum VGPUTextureUsage {
    VGPU_TEXTURE_USAGE_NONE = 0x0,
    VGPU_TEXTURE_USAGE_SHADER_READ = 0x1,
    VGPU_TEXTURE_USAGE_SHADER_WRITE = 0x2,
    VGPU_TEXTURE_USAGE_RENDER_TARGET = 0x4,

    _VGPU_TEXTURE_USAGE_FORCE_U32 = 0x7FFFFFFF
} VGPUTextureUsage;

typedef enum VGFXTextureFormat {
    VGFXTextureFormat_Undefined,
    /* 8-bit formats */
    VGFXTextureFormat_R8UInt,
    VGFXTextureFormat_R8SInt,
    VGFXTextureFormat_R8UNorm,
    VGFXTextureFormat_R8SNorm,
    /* 16-bit formats */
    VGFXTextureFormat_R16UInt,
    VGFXTextureFormat_R16SInt,
    VGFXTextureFormat_R16UNorm,
    VGFXTextureFormat_R16SNorm,
    VGFXTextureFormat_R16Float,
    VGFXTextureFormat_RG8UInt,
    VGFXTextureFormat_RG8SInt,
    VGFXTextureFormat_RG8UNorm,
    VGFXTextureFormat_RG8SNorm,
    /* Packed 16-Bit Pixel Formats */
    VGFXTextureFormat_BGRA4UNorm,
    VGFXTextureFormat_B5G6R5UNorm,
    VGFXTextureFormat_B5G5R5A1UNorm,
    /* 32-bit formats */
    VGFXTextureFormat_R32UInt,
    VGFXTextureFormat_R32SInt,
    VGFXTextureFormat_R32Float,
    VGFXTextureFormat_RG16UInt,
    VGFXTextureFormat_RG16SInt,
    VGFXTextureFormat_RG16UNorm,
    VGFXTextureFormat_RG16SNorm,
    VGFXTextureFormat_RG16Float,
    VGFXTextureFormat_RGBA8UInt,
    VGFXTextureFormat_RGBA8SInt,
    VGFXTextureFormat_RGBA8UNorm,
    VGFXTextureFormat_RGBA8UNormSrgb,
    VGFXTextureFormat_RGBA8SNorm,
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
    VGFXTextureFormat_RGBA16UInt,
    VGFXTextureFormat_RGBA16SInt,
    VGFXTextureFormat_RGBA16UNorm,
    VGFXTextureFormat_RGBA16SNorm,
    VGFXTextureFormat_RGBA16Float,
    /* 128-Bit formats */
    VGFXTextureFormat_RGBA32UInt,
    VGFXTextureFormat_RGBA32SInt,
    VGFXTextureFormat_RGBA32Float,
    /* Depth-stencil formats */
    VGFXTextureFormat_Depth16UNorm,
    VGFXTextureFormat_Depth24UNormStencil8,
    VGFXTextureFormat_Depth32Float,
    VGFXTextureFormat_Depth32FloatStencil8,
    /* Compressed BC formats */
    VGFXTextureFormat_BC1UNorm,
    VGFXTextureFormat_BC1UNormSrgb,
    VGFXTextureFormat_BC2UNorm,
    VGFXTextureFormat_BC2UNormSrgb,
    VGFXTextureFormat_BC3UNorm,
    VGFXTextureFormat_BC3UNormSrgb,
    VGFXTextureFormat_BC4UNorm,
    VGFXTextureFormat_BC4SNorm,
    VGFXTextureFormat_BC5UNorm,
    VGFXTextureFormat_BC5SNorm,
    VGFXTextureFormat_BC6HUFloat,
    VGFXTextureFormat_BC6HSFloat,
    VGFXTextureFormat_BC7UNorm,
    VGFXTextureFormat_BC7UNormSrgb,
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

    _VGPU_TEXTURE_FORMAT_COUNT,
    _VGPU_TEXTURE_FORMAT_FORCE_U32 = 0x7FFFFFFF
} VGFXTextureFormat;

typedef enum VGPUTextureFormatKind {
    VGPU_TEXTURE_FORMAT_KIND_INTEGER,
    VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,
    VGPU_TEXTURE_FORMAT_KIND_FLOAT,
    VGPU_TEXTURE_FORMAT_KIND_DEPTH_STENCIL,

    _VGPU_TEXTURE_FORMAT_KIND_COUNT,
    _VGPU_TEXTURE_FORMAT_KIND_FORCE_U32 = 0x7FFFFFFF
} VGPUTextureFormatKind;

typedef enum VGPUPresentMode {
    VGPU_PRESENT_MODE_IMMEDIATE = 0,
    VGPU_PRESENT_MODE_MAILBOX,
    VGPU_PRESENT_MODE_FIFO,

    _VGPU_PRESENT_MODE_COUNT,
    _VGPU_PRESENT_MODE_FORCE_U32 = 0x7FFFFFFF
} VGPUPresentMode;

typedef enum VGPUFeature {
    VGPU_FEATURE_COMPUTE = 0,
    VGPU_FEATURE_INDEPENDENT_BLEND,
    VGPU_FEATURE_TEXTURE_CUBE_ARRAY,
    VGPU_FEATURE_TEXTURE_COMPRESSION_BC,
    VGPU_FEATURE_TEXTURE_COMPRESSION_ETC2,
    VGPU_FEATURE_TEXTURE_COMPRESSION_ASTC,

    _VGPU_FEATURE_COUNT,
    _VGPU_FEATURE_FORCE_U32 = 0x7FFFFFFF
} VGPUFeature;

typedef enum VGPULoadOp {
    VGPU_LOAD_OP_LOAD = 0,
    VGPU_LOAD_OP_CLEAR = 1,
    VGPU_LOAD_OP_DISCARD = 2,

    _VGPU_LOAD_OP_COUNT,
    _VGPU_LOAD_OP_FORCE_U32 = 0x7FFFFFFF
} VGPULoadOp;

typedef enum VGFXStoreOp {
    VGPU_STORE_OP_STORE = 0,
    VGPU_STORE_OP_DISCARD = 1,

    _VGPU_STORE_OP_COUNT,
    _VGPU_STORE_OP_FORCE_U32 = 0x7FFFFFFF
} VGFXStoreOp;

typedef struct VGPUColor {
    float r;
    float g;
    float b;
    float a;
} VGPUColor;

typedef struct VGPUSize2D {
    uint32_t width;
    uint32_t height;
} VGPUSize2D;

typedef struct VGPUSize3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} VGPUSize3D;

typedef struct VGPUViewport {
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
} VGPUViewport;

typedef struct VGPUDispatchIndirectCommand
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
} VGPUDispatchIndirectCommand;

typedef struct VGPUDrawIndirectCommand
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t vertexStart;
    uint32_t baseInstance;
} VGPUDrawIndirectCommand;

typedef struct VGPUDrawIndexedIndirectCommand
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t startIndex;
    int32_t  baseVertex;
    uint32_t baseInstance;
} VGPUDrawIndexedIndirectCommand;

typedef struct VGPURenderPassColorAttachment {
    VGPUTexture texture;
    uint32_t level;
    uint32_t slice;
    VGPULoadOp loadOp;
    VGFXStoreOp storeOp;
    VGPUColor clearColor;
} VGPURenderPassColorAttachment;

typedef struct VGPURenderPassDepthStencilAttachment {
    VGPUTexture texture;
    uint32_t level;
    uint32_t slice;
    VGPULoadOp depthLoadOp;
    VGFXStoreOp depthStoreOp;
    float clearDepth;
    VGPULoadOp stencilLoadOp;
    VGFXStoreOp stencilStoreOp;
    uint8_t clearStencil;
} VGPURenderPassDepthStencilAttachment;

typedef struct VGPURenderPassDesc {
    uint32_t width;
    uint32_t height;
    uint32_t colorAttachmentCount;
    const VGPURenderPassColorAttachment* colorAttachments;
    const VGPURenderPassDepthStencilAttachment* depthStencilAttachment;
} VGPURenderPassDesc;

typedef struct VGPUBufferDesc {
    const char* label;
    uint64_t size;
    VGPUBufferUsage usage;
} VGPUBufferDesc;

typedef struct VGPUTextureDesc {
    const char* label;
    VGPUTextureType type;
    VGFXTextureFormat format;
    VGPUTextureUsage usage;
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArraySize;
    uint32_t mipLevelCount;
    uint32_t sampleCount;
} VGPUTextureDesc;

typedef struct VGPUSamplerDesc
{
    const char* label;
} VGPUSamplerDesc;

typedef struct VGPUSwapChainDesc
{
    VGFXTextureFormat format;
    uint32_t width;
    uint32_t height;
    VGPUPresentMode presentMode;
} VGPUSwapChainDesc;

typedef struct VGPUDeviceDesc {
    const char* label;
    VGPUBackendType preferredBackend;
    VGPUValidationMode validationMode;
} VGPUDeviceDesc;

typedef struct VGPUAdapterProperties {
    uint32_t vendorID;
    uint32_t deviceID;
    const char* name;
    const char* driverDescription;
    VGPUAdapterType adapterType;
    VGPUBackendType backendType;
} VGPUAdapterProperties;

typedef struct VGPULimits {
    uint32_t maxTextureDimension1D;
    uint32_t maxTextureDimension2D;
    uint32_t maxTextureDimension3D;
    uint32_t maxTextureArrayLayers;
    uint32_t maxBindGroups;
    uint32_t maxDynamicUniformBuffersPerPipelineLayout;
    uint32_t maxDynamicStorageBuffersPerPipelineLayout;
    uint32_t maxSampledTexturesPerShaderStage;
    uint32_t maxSamplersPerShaderStage;
    uint32_t maxStorageBuffersPerShaderStage;
    uint32_t maxStorageTexturesPerShaderStage;
    uint32_t maxUniformBuffersPerShaderStage;
    uint64_t maxUniformBufferBindingSize;
    uint64_t maxStorageBufferBindingSize;
    uint32_t minUniformBufferOffsetAlignment;
    uint32_t minStorageBufferOffsetAlignment;
    uint32_t maxVertexBuffers;
    uint32_t maxVertexAttributes;
    uint32_t maxVertexBufferArrayStride;
    uint32_t maxInterStageShaderComponents;
    uint32_t maxComputeWorkgroupStorageSize;
    uint32_t maxComputeInvocationsPerWorkGroup;
    uint32_t maxComputeWorkGroupSizeX;
    uint32_t maxComputeWorkGroupSizeY;
    uint32_t maxComputeWorkGroupSizeZ;
    uint32_t maxComputeWorkGroupsPerDimension;
} VGPULimits;

typedef void (VGPU_CALL* VGPULogCallback)(VGPULogLevel level, const char* message);
VGPU_API void VGPU_SetLogCallback(VGPULogCallback func);

VGPU_API bool vgpuIsSupported(VGPUBackendType backend);
VGPU_API VGPUDevice vgpuCreateDevice(const VGPUDeviceDesc* desc);
VGPU_API void vgpuDestroyDevice(VGPUDevice device);
VGPU_API uint64_t vgpuFrame(VGPUDevice device);
VGPU_API void vgpuWaitIdle(VGPUDevice device);
VGPU_API bool vgpuQueryFeature(VGPUDevice device, VGPUFeature feature);
VGPU_API void vgpuGetAdapterProperties(VGPUDevice device, VGPUAdapterProperties* properties);
VGPU_API void vgpuGetLimits(VGPUDevice device, VGPULimits* limits);

/* Buffer */
VGPU_API VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDesc* desc, const void* pInitialData);
VGPU_API void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer);

/* Texture */
VGPU_API VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc);
VGPU_API void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture);

/* SwapChain */
VGPU_API VGPUSwapChain vgpuCreateSwapChain(VGPUDevice device, void* windowHandle, const VGPUSwapChainDesc* desc);
VGPU_API void vgpuDestroySwapChain(VGPUDevice device, VGPUSwapChain swapChain);
VGPU_API VGFXTextureFormat vgpuSwapChainGetFormat(VGPUDevice device, VGPUSwapChain swapChain);

/* Commands */
VGPU_API VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, const char* label);
VGPU_API void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel);
VGPU_API void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* debugLabel);

VGPU_API VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain swapChain, uint32_t* pWidth, uint32_t* pHeight);
VGPU_API void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc);
VGPU_API void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance);

VGPU_API void vgpuSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count);

/* Helper functions */
typedef struct VGPUFormatInfo {
    VGFXTextureFormat format;
    const char* name;
    uint8_t bytesPerBlock;
    uint8_t blockSize;
    VGPUTextureFormatKind kind;
    bool hasRed : 1;
    bool hasGreen : 1;
    bool hasBlue : 1;
    bool hasAlpha : 1;
    bool hasDepth : 1;
    bool hasStencil : 1;
    bool isSigned : 1;
    bool isSRGB : 1;
} VGPUFormatInfo;

VGPU_API void vgpuGetFormatInfo(VGFXTextureFormat format, const VGPUFormatInfo* pInfo);
VGPU_API bool vgpuIsDepthFormat(VGFXTextureFormat format);
VGPU_API bool vgpuIsStencilFormat(VGFXTextureFormat format);
VGPU_API bool vgpuIsDepthStencilFormat(VGFXTextureFormat format);

#endif /* _VGPU_H */
