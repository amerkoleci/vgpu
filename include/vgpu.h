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

/* Version API */
#define VGPU_VERSION_MAJOR  0
#define VGPU_VERSION_MINOR	1
#define VGPU_VERSION_PATCH	0

enum {
    VGPU_MAX_INFLIGHT_FRAMES = 2,
    VGPU_MAX_COLOR_ATTACHMENTS = 8,
    VGPU_MAX_VERTEX_BUFFERS = 8,
    VGPU_MAX_VERTEX_ATTRIBUTES = 16,
    VGPU_MAX_VIEWPORTS_AND_SCISSORS = 8,
};

typedef uint32_t vgpu_bool;
typedef uint32_t vgpu_flags;

typedef struct VGPUDevice_T* VGPUDevice;
typedef struct VGPUBuffer_T* VGPUBuffer;
typedef struct VGPUTexture_T* VGPUTexture;
typedef struct VGPUSampler_T* VGPUSampler;
typedef struct VGPUSwapChain_T* VGPUSwapChain;
typedef struct VGPUShaderModule_T* VGPUShaderModule;
typedef struct VGPUPipeline_T* VGPUPipeline;
typedef struct VGPUCommandBuffer_T* VGPUCommandBuffer;

typedef enum vgpu_log_level {
    VGPU_LOG_LEVEL_INFO = 0,
    VGPU_LOG_LEVEL_WARN,
    VGPU_LOG_LEVEL_ERROR,

    _VGPU_LOG_LEVEL_COUNT,
    _VGPU_LOG_LEVEL_FORCE_U32 = 0x7FFFFFFF
} vgpu_log_level;

typedef enum VGPUBackendType {
    VGPUBackendType_Default = 0,
    VGPUBackendType_Vulkan,
    VGPUBackendType_D3D12,
    VGPUBackendType_WebGPU,

    _VGPUBackendType_Count,
    _VGPUBackendType_Force32 = 0x7FFFFFFF
} VGPUBackendType;

typedef enum VGPUValidationMode {
    /// No validation is enabled.
    VGPUValidationMode_Disabled = 0,
    /// Print warnings and errors
    VGPUValidationMode_Enabled,
    /// Print all warnings, errors and info messages
    VGPUValidationMode_Verbose,
    /// Enable GPU-based validation
    VGPUValidationMode_GPU,

    _VGPUValidationMode_Force32 = 0x7FFFFFFF
} VGPUValidationMode;

typedef enum VGPUAdapterType {
    VGPUAdapterType_Other = 0,
    VGPUAdapterType_IntegratedGPU,
    VGPUAdapterType_DiscreteGPU,
    VGPUAdapterType_VirtualGpu,
    VGPUAdapterType_Cpu,

    _VGPUAdapterType_Force32 = 0x7FFFFFFF
} VGPUAdapterType;

typedef enum VGPUCpuAccessMode
{
    VGPUCpuAccessMode_None,
    VGPUCpuAccessMode_Write,
    VGPUCpuAccessMode_Read,

    _VGPUCpuAccessMode_Force32 = 0x7FFFFFFF
} VGPUCpuAccessMode;

typedef enum VGPUTextureType {
    _VGPUTextureType_Default = 0,
    VGPUTexture_Type1D,
    VGPUTexture_Type2D,
    VGPUTexture_Type3D,

    _VGPUTextureType_Force32 = 0x7FFFFFFF
} VGPUTextureType;

typedef enum VGPUBufferUsage {
    VGPUBufferUsage_None = 0,
    VGPUBufferUsage_Vertex = (1 << 0),
    VGPUBufferUsage_Index = (1 << 1),
    VGPUBufferUsage_Constant = (1 << 2),
    VGPUBufferUsage_ShaderRead = (1 << 3),
    VGPUBufferUsage_ShaderWrite = (1 << 4),
    VGPUBufferUsage_Indirect = (1 << 5),
    VGPUBufferUsage_RayTracing = (1 << 6),
    VGPUBufferUsage_Predication = (1 << 7),

    _VGPUBufferUsage_Force32 = 0x7FFFFFFF
} VGPUBufferUsage;

typedef enum VGPUTextureUsage {
    VGPUTextureUsage_None = 0,
    VGPUTextureUsage_ShaderRead = (1 << 0),
    VGPUTextureUsage_ShaderWrite = (1 << 1),
    VGPUTextureUsage_RenderTarget = (1 << 2),
    VGPUTextureUsage_ShadingRate = (1 << 3),

    _VGPUTextureUsage_Force32 = 0x7FFFFFFF
} VGPUTextureUsage;

typedef enum VGPUTextureFormat {
    VGPUTextureFormat_Undefined,
    /* 8-bit formats */
    VGPUTextureFormat_R8UNorm,
    VGPUTextureFormat_R8SNorm,
    VGPUTextureFormat_R8UInt,
    VGPUTextureFormat_R8SInt,
    /* 16-bit formats */
    VGPUTextureFormat_R16UInt,
    VGPUTextureFormat_R16SInt,
    VGPUTextureFormat_R16UNorm,
    VGPUTextureFormat_R16SNorm,
    VGPUTextureFormat_R16Float,
    VGPUTextureFormat_RG8UInt,
    VGPUTextureFormat_RG8SInt,
    VGPUTextureFormat_RG8UNorm,
    VGPUTextureFormat_RG8SNorm,
    /* Packed 16-Bit Pixel Formats */
    VGPUTextureFormat_BGRA4UNorm,
    VGPUTextureFormat_B5G6R5UNorm,
    VGPUTextureFormat_B5G5R5A1UNorm,
    /* 32-bit formats */
    VGPUTextureFormat_R32UInt,
    VGPUTextureFormat_R32SInt,
    VGPUTextureFormat_R32Float,
    VGPUTextureFormat_RG16UInt,
    VGPUTextureFormat_RG16SInt,
    VGPUTextureFormat_RG16UNorm,
    VGPUTextureFormat_RG16SNorm,
    VGPUTextureFormat_RG16Float,
    VGPUTextureFormat_RGBA8UInt,
    VGPUTextureFormat_RGBA8SInt,
    VGPUTextureFormat_RGBA8UNorm,
    VGPUTextureFormat_RGBA8UNormSrgb,
    VGPUTextureFormat_RGBA8SNorm,
    VGPUTextureFormat_BGRA8UNorm,
    VGPUTextureFormat_BGRA8UNormSrgb,
    /* Packed 32-Bit formats */
    VGPUTextureFormat_RGB10A2UNorm,
    VGPUTextureFormat_RG11B10Float,
    VGPUTextureFormat_RGB9E5Float,
    /* 64-Bit formats */
    VGPUTextureFormat_RG32UInt,
    VGPUTextureFormat_RG32SInt,
    VGPUTextureFormat_RG32Float,
    VGPUTextureFormat_RGBA16UInt,
    VGPUTextureFormat_RGBA16SInt,
    VGPUTextureFormat_RGBA16UNorm,
    VGPUTextureFormat_RGBA16SNorm,
    VGPUTextureFormat_RGBA16Float,
    /* 128-Bit formats */
    VGPUTextureFormat_RGBA32UInt,
    VGPUTextureFormat_RGBA32SInt,
    VGPUTextureFormat_RGBA32Float,
    /* Depth-stencil formats */
    VGPUTextureFormat_Depth16UNorm,
    VGPUTextureFormat_Depth24UNormStencil8,
    VGPUTextureFormat_Depth32Float,
    VGPUTextureFormat_Depth32FloatStencil8,
    /* Compressed BC formats */
    VGPUTextureFormat_BC1UNorm,
    VGPUTextureFormat_BC1UNormSrgb,
    VGPUTextureFormat_BC2UNorm,
    VGPUTextureFormat_BC2UNormSrgb,
    VGPUTextureFormat_BC3UNorm,
    VGPUTextureFormat_BC3UNormSrgb,
    VGPUTextureFormat_BC4UNorm,
    VGPUTextureFormat_BC4SNorm,
    VGPUTextureFormat_BC5UNorm,
    VGPUTextureFormat_BC5SNorm,
    VGPUTextureFormat_BC6HUFloat,
    VGPUTextureFormat_BC6HSFloat,
    VGPUTextureFormat_BC7UNorm,
    VGPUTextureFormat_BC7UNormSrgb,
    /* Compressed EAC/ETC formats */
    VGPUTextureFormat_ETC2RGB8UNorm,
    VGPUTextureFormat_ETC2RGB8UNormSrgb,
    VGPUTextureFormat_ETC2RGB8A1UNorm,
    VGPUTextureFormat_ETC2RGB8A1UNormSrgb,
    VGPUTextureFormat_ETC2RGBA8UNorm,
    VGPUTextureFormat_ETC2RGBA8UNormSrgb,
    VGPUTextureFormat_EACR11UNorm,
    VGPUTextureFormat_EACR11SNorm,
    VGPUTextureFormat_EACRG11UNorm,
    VGPUTextureFormat_EACRG11SNorm,
    /* Compressed ASTC formats */
    VGPUTextureFormat_ASTC4x4UNorm,
    VGPUTextureFormat_ASTC4x4UNormSrgb,
    VGPUTextureFormat_ASTC5x4UNorm,
    VGPUTextureFormat_ASTC5x4UNormSrgb,
    VGPUTextureFormat_ASTC5x5UNorm,
    VGPUTextureFormat_ASTC5x5UNormSrgb,
    VGPUTextureFormat_ASTC6x5UNorm,
    VGPUTextureFormat_ASTC6x5UNormSrgb,
    VGPUTextureFormat_ASTC6x6UNorm,
    VGPUTextureFormat_ASTC6x6UNormSrgb,
    VGPUTextureFormat_ASTC8x5UNorm,
    VGPUTextureFormat_ASTC8x5UNormSrgb,
    VGPUTextureFormat_ASTC8x6UNorm,
    VGPUTextureFormat_ASTC8x6UNormSrgb,
    VGPUTextureFormat_ASTC8x8UNorm,
    VGPUTextureFormat_ASTC8x8UNormSrgb,
    VGPUTextureFormat_ASTC10x5UNorm,
    VGPUTextureFormat_ASTC10x5UNormSrgb,
    VGPUTextureFormat_ASTC10x6UNorm,
    VGPUTextureFormat_ASTC10x6UNormSrgb,
    VGPUTextureFormat_ASTC10x8UNorm,
    VGPUTextureFormat_ASTC10x8UNormSrgb,
    VGPUTextureFormat_ASTC10x10UNorm,
    VGPUTextureFormat_ASTC10x10UNormSrgb,
    VGPUTextureFormat_ASTC12x10UNorm,
    VGPUTextureFormat_ASTC12x10UNormSrgb,
    VGPUTextureFormat_ASTC12x12UNorm,
    VGPUTextureFormat_ASTC12x12UNormSrgb,

    _VGPUTextureFormat_Count,
    _VGPUTextureFormat_Force32 = 0x7FFFFFFF
} VGPUTextureFormat;

typedef enum VGPUTextureFormatKind {
    VGPU_TEXTURE_FORMAT_KIND_INTEGER,
    VGPU_TEXTURE_FORMAT_KIND_NORMALIZED,
    VGPU_TEXTURE_FORMAT_KIND_FLOAT,
    VGPU_TEXTURE_FORMAT_KIND_DEPTH_STENCIL,

    _VGPU_TEXTURE_FORMAT_KIND_COUNT,
    _VGPU_TEXTURE_FORMAT_KIND_FORCE_U32 = 0x7FFFFFFF
} VGPUTextureFormatKind;

typedef enum VGPUPresentMode {
    VGPUPresentMode_Immediate = 0,
    VGPUPresentMode_Mailbox,
    VGPUPresentMode_Fifo,

    _VGPUPresentMode_Force32 = 0x7FFFFFFF
} VGPUPresentMode;

typedef enum VGPUFeature {
    VGPUFeature_TextureCompressionBC,
    VGPUFeature_TextureCompressionETC2,
    VGPUFeature_TextureCompressionASTC,
    VGPUFeature_ShaderFloat16,
    VGPUFeature_PipelineStatisticsQuery,
    VGPUFeature_TimestampQuery,
    VGPUFeature_DepthClamping,
    VGPUFeature_Depth24UNormStencil8,
    VGPUFeature_Depth32FloatStencil8,

    VGPUFeature_TextureCubeArray,
    VGPUFeature_IndependentBlend,
    VGPUFeature_Tessellation,
    VGPUFeature_DescriptorIndexing,
    VGPUFeature_ConditionalRendering,
    VGPUFeature_DrawIndirectFirstInstance,
    VGPUFeature_ShaderOutputViewportIndex,
    VGPUFeature_SamplerMinMax,
    VGPUFeature_MeshShader,
    VGPUFeature_RayTracing,

    _VGPUFeature_Force32 = 0x7FFFFFFF
} VGPUFeature;

typedef enum VGPULoadOp {
    VGPULoadOp_Load = 0,
    VGPULoadOp_Clear = 1,
    VGPULoadOp_Discard = 2,

    _VGPULoadOp_Force32 = 0x7FFFFFFF
} VGPULoadOp;

typedef enum VGPUStoreOp {
    VGPUStoreOp_Store = 0,
    VGPUStoreOp_Discard = 1,

    _VGPUStoreOp_Force32 = 0x7FFFFFFF
} VGPUStoreOp;

typedef enum VGPUIndexType {
    VGPUIndexType_UInt16 = 0,
    VGPUIndexType_UInt32 = 1,

    _VGPUIndexType_Force32 = 0x7FFFFFFF
} VGPUIndexType;

typedef enum VGPUCompareFunction 
{
    VGPUCompareFunction_Never,
    VGPUCompareFunction_Less,
    VGPUCompareFunction_Equal,
    VGPUCompareFunction_LessEqual,
    VGPUCompareFunction_Greater,
    VGPUCompareFunction_NotEqual,
    VGPUCompareFunction_GreaterEqual,
    VGPUCompareFunction_Always,

    _VGPUCompareFunction_Force32 = 0x7FFFFFFF
} VGPUCompareFunction;

typedef enum VGPUSamplerFilter 
{
    VGPUSamplerFilter_Nearest = 0,
    VGPUSamplerFilter_Linear,

    _VGPUSamplerFilter_Force32 = 0x7FFFFFFF
} VGPUSamplerFilter;

typedef enum VGPUSamplerMipFilter
{
    VGPUSamplerMipFilter_Nearest = 0,
    VGPUSamplerMipFilter_Linear,

    _VGPUSamplerMipFilter_Force32 = 0x7FFFFFFF
} VGPUSamplerMipFilter;

typedef enum VGPUSamplerAddressMode
{
    VGPUSamplerAddressMode_Wrap = 0,
    VGPUSamplerAddressMode_Mirror,
    VGPUSamplerAddressMode_Clamp,
    VGPUSamplerAddressMode_Border,

    _VGPUSamplerAddressMode_Force32 = 0x7FFFFFFF
} VGPUSamplerAddressMode;

typedef enum VGPUSamplerBorderColor 
{
    VGPUSamplerBorderColor_TransparentBlack = 0,
    VGPUSamplerBorderColor_OpaqueBlack,
    VGPUSamplerBorderColor_OpaqueWhite,

    _VGPUSamplerBorderColor_Force32 = 0x7FFFFFFF
} VGPUSamplerBorderColor;

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

typedef struct VGPURect
{
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} VGPURect;

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
    uint32_t    level;
    uint32_t    slice;
    VGPULoadOp  loadOp;
    VGPUStoreOp storeOp;
    VGPUColor   clearColor;
} VGPURenderPassColorAttachment;

typedef struct VGPURenderPassDepthStencilAttachment {
    VGPUTexture texture;
    uint32_t    level;
    uint32_t    slice;
    VGPULoadOp  depthLoadOp;
    VGPUStoreOp depthStoreOp;
    float       clearDepth;
    VGPULoadOp  stencilLoadOp;
    VGPUStoreOp stencilStoreOp;
    uint8_t     clearStencil;
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
    uint32_t size;
    VGPUBufferUsage usage;
    VGPUCpuAccessMode cpuAccess;
} VGPUBufferDesc;

typedef struct VGPUTextureDesc {
    const char*         label;
    VGPUTextureType     type;
    VGPUTextureFormat   format;
    VGPUTextureUsage    usage;
    uint32_t            width;
    uint32_t            height;
    uint32_t            depthOrArraySize;
    uint32_t            mipLevels;
    uint32_t            sampleCount;
} VGPUTextureDesc;

typedef struct VGPUSamplerDesc {
    const char*             label;
    VGPUSamplerFilter       minFilter;
    VGPUSamplerFilter       magFilter;
    VGPUSamplerMipFilter    mipFilter;
    VGPUSamplerAddressMode  addressU;
    VGPUSamplerAddressMode  addressV;
    VGPUSamplerAddressMode  addressW;
    uint32_t                maxAnisotropy;
    float                   mipLodBias;
    VGPUCompareFunction     compareFunction;
    float                   lodMinClamp;
    float                   lodMaxClamp;
    VGPUSamplerBorderColor  borderColor;
} VGPUSamplerDesc;

typedef struct VGPURenderPipelineDesc {
    const char* label;
} VGPURenderPipelineDesc;

typedef struct VGPUComputePipelineDesc {
    const char* label;
} VGPUComputePipelineDesc;

typedef struct VGPURayTracingPipelineDesc {
    const char* label;
} VGPURayTracingPipelineDesc;

typedef struct VGPUSwapChainDesc
{
    uint32_t width;
    uint32_t height;
    VGPUTextureFormat format;
    VGPUPresentMode presentMode;
    vgpu_bool isFullscreen;
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

typedef void (*vgpu_log_callback)(vgpu_log_level level, const char* message);
VGPU_API void vgpu_set_log_callback(vgpu_log_callback func);

VGPU_API vgpu_bool vgpuIsSupported(VGPUBackendType backend);
VGPU_API VGPUDevice vgpuCreateDevice(const VGPUDeviceDesc* desc);
VGPU_API void vgpuDestroyDevice(VGPUDevice device);
VGPU_API uint64_t vgpuFrame(VGPUDevice device);
VGPU_API void vgpuWaitIdle(VGPUDevice device);
VGPU_API VGPUBackendType vgpuGetBackendType(VGPUDevice device);
VGPU_API vgpu_bool vgpuQueryFeature(VGPUDevice device, VGPUFeature feature, void* pInfo, uint32_t infoSize);
VGPU_API void vgpuGetAdapterProperties(VGPUDevice device, VGPUAdapterProperties* properties);
VGPU_API void vgpuGetLimits(VGPUDevice device, VGPULimits* limits);

/* Buffer */
VGPU_API VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDesc* desc, const void* pInitialData);
VGPU_API void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer);

/* Texture */
VGPU_API VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const void* pInitialData);
VGPU_API VGPUTexture vgpuCreateTexture2D(VGPUDevice device, uint32_t width, uint32_t height, VGPUTextureFormat format, uint32_t mipLevels, VGPUTextureUsage usage, const void* pInitialData);
VGPU_API VGPUTexture vgpuCreateTextureCube(VGPUDevice device, uint32_t size, VGPUTextureFormat format, uint32_t mipLevels, const void* pInitialData);
VGPU_API void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture);

/* Sampler */
VGPU_API VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDesc* desc);
VGPU_API void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler);

/* ShaderModule */
VGPU_API VGPUShaderModule vgpuCreateShader(VGPUDevice device, const void* pCode, size_t codeSize);
VGPU_API void vgpuDestroyShader(VGPUDevice device, VGPUShaderModule module);

/* Pipeline */
VGPU_API VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDesc* desc);
VGPU_API void vgpuDestroyPipeline(VGPUDevice device, VGPUPipeline pipeline);

/* SwapChain */
VGPU_API VGPUSwapChain vgpuCreateSwapChain(VGPUDevice device, void* windowHandle, const VGPUSwapChainDesc* desc);
VGPU_API void vgpuDestroySwapChain(VGPUDevice device, VGPUSwapChain swapChain);
VGPU_API VGPUTextureFormat vgpuSwapChainGetFormat(VGPUDevice device, VGPUSwapChain swapChain);

/* Commands */
VGPU_API VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, const char* label);
VGPU_API void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel);
VGPU_API void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* debugLabel);
VGPU_API void vgpuSetPipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline pipeline);

/* Compute commands */
VGPU_API void vgpuDispatch1D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t groupSizeX);
VGPU_API void vgpuDispatch2D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY);
VGPU_API void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
VGPU_API void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer indirectBuffer, uint32_t indirectBufferOffset);

/* Render commands */
VGPU_API VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain swapChain, uint32_t* pWidth, uint32_t* pHeight);
VGPU_API void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc);
VGPU_API void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuSetViewports(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPUViewport* viewports);
VGPU_API void vgpuSetScissorRects(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPURect* scissorRects);
VGPU_API void vgpuSetVertexBuffer(VGPUCommandBuffer commandBuffer, uint32_t index, VGPUBuffer buffer, uint64_t offset);
VGPU_API void vgpuSetIndexBuffer(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset, VGPUIndexType indexType);

VGPU_API void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance);

VGPU_API void vgpuSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count);

/* Helper functions */
typedef struct VGPUFormatInfo {
    VGPUTextureFormat format;
    const char* name;
    uint8_t bytesPerBlock;
    uint8_t blockSize;
    VGPUTextureFormatKind kind;
    vgpu_bool hasRed;
    vgpu_bool hasGreen;
    vgpu_bool hasBlue;
    vgpu_bool hasAlpha;
    vgpu_bool hasDepth;
    vgpu_bool hasStencil;
    vgpu_bool isSigned;
    vgpu_bool isSRGB;
} VGPUFormatInfo;

VGPU_API void vgpuGetFormatInfo(VGPUTextureFormat format, const VGPUFormatInfo* pInfo);
VGPU_API vgpu_bool vgpuIsDepthFormat(VGPUTextureFormat format);
VGPU_API vgpu_bool vgpuIsStencilFormat(VGPUTextureFormat format);
VGPU_API vgpu_bool vgpuIsDepthStencilFormat(VGPUTextureFormat format);
VGPU_API uint32_t vgpuNumMipLevels(uint32_t width, uint32_t height, uint32_t depth);

#endif /* _VGPU_H */
