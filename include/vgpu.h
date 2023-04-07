// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef VGPU_H
#define VGPU_H 1

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

#include <stddef.h>
#include <stdint.h>

/* Version API */
#define VGPU_VERSION_MAJOR  1
#define VGPU_VERSION_MINOR	0
#define VGPU_VERSION_PATCH	0

enum {
    VGPU_MAX_INFLIGHT_FRAMES = 2,
    VGPU_MAX_COLOR_ATTACHMENTS = 8,
};

typedef uint32_t VGPUBool32;
typedef uint32_t VGPUFlags;
typedef uint64_t VGPUDeviceAddress;

typedef struct vgpu_buffer vgpu_buffer;
typedef struct VGPUTexture_T* VGPUTexture;
typedef struct vgpu_texture_view vgpu_texture_view;
typedef struct VGPUSampler VGPUSampler;
typedef struct VGPUSwapChain VGPUSwapChain;
typedef struct VGPUShaderModule_T* VGPUShaderModule;
typedef struct VGPUPipeline VGPUPipeline;
typedef struct VGPUCommandBuffer_T* VGPUCommandBuffer;

typedef enum vgpu_log_level {
    VGPU_LOG_LEVEL_INFO = 0,
    VGPU_LOG_LEVEL_WARN,
    VGPU_LOG_LEVEL_ERROR,

    _VGPU_LOG_LEVEL_COUNT,
    _VGPU_LOG_LEVEL_FORCE_U32 = 0x7FFFFFFF
} vgpu_log_level;

typedef enum vgpu_backend {
    _VGPU_BACKEND_DEFAULT = 0,
    VGPU_BACKEND_VULKAN,
    VGPU_BACKEND_D3D12,
    VGPU_BACKEND_D3D11,
    VGPU_BACKEND_OPENGL,

    _VGPU_BACKEND_COUNT,
    _VGPU_BACKEND_FORCE_U32 = 0x7FFFFFFF
} vgpu_backend;

typedef enum VGPUValidationMode {
    VGPUValidationMode_Disabled = 0,
    VGPUValidationMode_Enabled,
    VGPUValidationMode_Verbose,
    VGPUValidationMode_GPU,

    _VGPUValidationMode_Count,
    _VGPUValidationMode_Force32 = 0x7FFFFFFF
} VGPUValidationMode;

typedef enum VGPUCommandQueue {
    VGPUCommandQueue_Graphics,
    VGPUCommandQueue_Compute,

    _VGPUCommandQueue_Count,
    _VGPUCommandQueue_Force32 = 0x7FFFFFFF
} VGPUCommandQueue;

typedef enum VGPUAdapterType {
    VGPUAdapterType_Other = 0,
    VGPUAdapterType_IntegratedGPU,
    VGPUAdapterType_DiscreteGPU,
    VGPUAdapterType_VirtualGPU,
    VGPUAdapterType_CPU,

    _VGPUAdapterType_Count,
    _VGPUAdapterType_Force32 = 0x7FFFFFFF
} VGPUAdapterType;

typedef enum vgpu_cpu_access
{
    VGPU_CPU_ACCESS_NONE = 0,
    VGPU_CPU_ACCESS_WRITE,
    VGPU_CPU_ACCESS_READ,

    _VGPU_CPU_ACCESS_FORCE_U32 = 0x7FFFFFFF
} vgpu_cpu_access;

typedef enum vgpu_buffer_usage {
    VGPU_BUFFER_USAGE_NONE = 0,
    VGPU_BUFFER_USAGE_VERTEX = (1 << 0),
    VGPU_BUFFER_USAGE_INDEX = (1 << 1),
    VGPU_BUFFER_USAGE_UNIFORM = (1 << 2),
    VGPU_BUFFER_USAGE_SHADER_READ = (1 << 3),
    VGPU_BUFFER_USAGE_SHADER_WRITE = (1 << 4),
    VGPU_BUFFER_USAGE_INDIRECT = (1 << 5),
    VGPU_BUFFER_USAGE_RAY_TRACING = (1 << 6),
    VGPU_BUFFER_USAGE_PREDICATION = (1 << 7),

    _VGPU_BUFFER_USAGE_FORCE_U32 = 0x7FFFFFFF
} vgpu_buffer_usage;
typedef VGPUFlags vgpu_buffer_usage_flags;

typedef enum vgpu_texture_type {
    _VGPUTextureType_Default = 0,
    VGPUTexture_Type1D,
    VGPUTexture_Type2D,
    VGPUTexture_Type3D,

    _VGPU_TEXTURE_TYPE_COUNT,
    _VGPU_TEXTURE_TYPE_FORCE_U32 = 0x7FFFFFFF
} vgpu_texture_type;

typedef enum VGPUTextureUsage {
    VGPUTextureUsage_None = 0,
    VGPUTextureUsage_ShaderRead = (1 << 0),
    VGPUTextureUsage_ShaderWrite = (1 << 1),
    VGPUTextureUsage_RenderTarget = (1 << 2),
    VGPUTextureUsage_ShadingRate = (1 << 3),

    _VGPU_TEXTURE_USAGE_FORCE_U32 = 0x7FFFFFFF
} VGPUTextureUsage;
typedef VGPUFlags VGPUTextureUsageFlags;

typedef enum vgpu_pixel_format {
    VGPUTextureFormat_Undefined,
    /* 8-bit formats */
    VGPUTextureFormat_R8Unorm,
    VGPUTextureFormat_R8Snorm,
    VGPUTextureFormat_R8Uint,
    VGPUTextureFormat_R8Sint,
    /* 16-bit formats */
    VGPUTextureFormat_R16Unorm,
    VGPUTextureFormat_R16Snorm,
    VGPUTextureFormat_R16Uint,
    VGPUTextureFormat_R16Sint,
    VGPUTextureFormat_R16Float,
    VGPUTextureFormat_RG8Unorm,
    VGPUTextureFormat_RG8Snorm,
    VGPUTextureFormat_RG8Uint,
    VGPUTextureFormat_RG8Sint,
    /* Packed 16-Bit Pixel Formats */
    VGPUTextureFormat_BGRA4Unorm,
    VGPUTextureFormat_B5G6R5Unorm,
    VGPUTextureFormat_B5G5R5A1Unorm,
    /* 32-bit formats */
    VGPUTextureFormat_R32Uint,
    VGPUTextureFormat_R32Sint,
    VGPUTextureFormat_R32Float,
    VGPUTextureFormat_RG16Unorm,
    VGPUTextureFormat_RG16Snorm,
    VGPUTextureFormat_RG16Uint,
    VGPUTextureFormat_RG16Sint,
    VGPUTextureFormat_RG16Float,
    VGPUTextureFormat_RGBA8Uint,
    VGPUTextureFormat_RGBA8Sint,
    VGPUTextureFormat_RGBA8Unorm,
    VGPUTextureFormat_RGBA8UnormSrgb,
    VGPUTextureFormat_RGBA8Snorm,
    VGPUTextureFormat_BGRA8Unorm,
    VGPUTextureFormat_BGRA8UnormSrgb,
    /* Packed 32-Bit formats */
    VGPUTextureFormat_RGB9E5Ufloat,
    VGPUTextureFormat_RGB10A2Unorm,
    VGPUTextureFormat_RGB10A2Uint,
    VGPUTextureFormat_RG11B10Float,
    /* 64-Bit formats */
    VGPUTextureFormat_RG32Uint,
    VGPUTextureFormat_RG32Sint,
    VGPUTextureFormat_RG32Float,
    VGPUTextureFormat_RGBA16Unorm,
    VGPUTextureFormat_RGBA16Snorm,
    VGPUTextureFormat_RGBA16Uint,
    VGPUTextureFormat_RGBA16Sint,
    VGPUTextureFormat_RGBA16Float,
    /* 128-Bit formats */
    VGPUTextureFormat_RGBA32Uint,
    VGPUTextureFormat_RGBA32Sint,
    VGPUTextureFormat_RGBA32Float,
    /* Depth-stencil formats */
    VGPUTextureFormat_Depth16Unorm,
    VGPUTextureFormat_Depth32Float,
    VGPUTextureFormat_Stencil8,
    VGPUTextureFormat_Depth24UnormStencil8,
    VGPUTextureFormat_Depth32FloatStencil8,
    /* Compressed BC formats */
    VGPUTextureFormat_Bc1RgbaUnorm,
    VGPUTextureFormat_Bc1RgbaUnormSrgb,
    VGPUTextureFormat_Bc2RgbaUnorm,
    VGPUTextureFormat_Bc2RgbaUnormSrgb,
    VGPUTextureFormat_Bc3RgbaUnorm,
    VGPUTextureFormat_Bc3RgbaUnormSrgb,
    VGPUTextureFormat_Bc4RUnorm,
    VGPUTextureFormat_Bc4RSnorm,
    VGPUTextureFormat_Bc5RgUnorm,
    VGPUTextureFormat_Bc5RgSnorm,
    VGPUTextureFormat_Bc6hRgbUfloat,
    VGPUTextureFormat_Bc6hRgbSfloat,
    VGPUTextureFormat_Bc7RgbaUnorm,
    VGPUTextureFormat_Bc7RgbaUnormSrgb,
    /* ETC2/EAC compressed formats */
    VGPUTextureFormat_Etc2Rgb8Unorm,
    VGPUTextureFormat_Etc2Rgb8UnormSrgb,
    VGPUTextureFormat_Etc2Rgb8A1Unorm,
    VGPUTextureFormat_Etc2Rgb8A1UnormSrgb,
    VGPUTextureFormat_Etc2Rgba8Unorm,
    VGPUTextureFormat_Etc2Rgba8UnormSrgb,
    VGPUTextureFormat_EacR11Unorm,
    VGPUTextureFormat_EacR11Snorm,
    VGPUTextureFormat_EacRg11Unorm,
    VGPUTextureFormat_EacRg11Snorm,
    /* ASTC compressed formats */
    VGPUTextureFormat_Astc4x4Unorm,
    VGPUTextureFormat_Astc4x4UnormSrgb,
    VGPUTextureFormat_Astc5x4Unorm,
    VGPUTextureFormat_Astc5x4UnormSrgb,
    VGPUTextureFormat_Astc5x5Unorm,
    VGPUTextureFormat_Astc5x5UnormSrgb,
    VGPUTextureFormat_Astc6x5Unorm,
    VGPUTextureFormat_Astc6x5UnormSrgb,
    VGPUTextureFormat_Astc6x6Unorm,
    VGPUTextureFormat_Astc6x6UnormSrgb,
    VGPUTextureFormat_Astc8x5Unorm,
    VGPUTextureFormat_Astc8x5UnormSrgb,
    VGPUTextureFormat_Astc8x6Unorm,
    VGPUTextureFormat_Astc8x6UnormSrgb,
    VGPUTextureFormat_Astc8x8Unorm,
    VGPUTextureFormat_Astc8x8UnormSrgb,
    VGPUTextureFormat_Astc10x5Unorm,
    VGPUTextureFormat_Astc10x5UnormSrgb,
    VGPUTextureFormat_Astc10x6Unorm,
    VGPUTextureFormat_Astc10x6UnormSrgb,
    VGPUTextureFormat_Astc10x8Unorm,
    VGPUTextureFormat_Astc10x8UnormSrgb,
    VGPUTextureFormat_Astc10x10Unorm,
    VGPUTextureFormat_Astc10x10UnormSrgb,
    VGPUTextureFormat_Astc12x10Unorm,
    VGPUTextureFormat_Astc12x10UnormSrgb,
    VGPUTextureFormat_Astc12x12Unorm,
    VGPUTextureFormat_Astc12x12UnormSrgb,

    _VGPU_PIXEL_FORMAT_COUNT,
    _VGPU_PIXEL_FORMAT_FORCE_U32 = 0x7FFFFFFF
} vgpu_pixel_format;

typedef enum VGPUTextureFormatKind {
    VGPUTextureFormatKind_Unorm,
    VGPUTextureFormatKind_UnormSrgb,
    VGPUTextureFormatKind_Snorm,
    VGPUTextureFormatKind_Uint,
    VGPUTextureFormatKind_Sint,
    VGPUTextureFormatKind_Float,

    _VGPUTextureFormatKind_Count,
    _VGPUTextureFormatKind_Force32 = 0x7FFFFFFF
} VGPUTextureFormatKind;

typedef enum VGPUPresentMode {
    VGPUPresentMode_Immediate = 0,
    VGPUPresentMode_Mailbox,
    VGPUPresentMode_Fifo,

    _VGPUPresentMode_Count,
    _VGPUPresentMode_Force32 = 0x7FFFFFFF
} VGPUPresentMode;

typedef enum VGPUShaderStage
{
    VGPUShaderStage_None = 0,
    VGPUShaderStage_Vertex = (1 << 0),
    VGPUShaderStage_Hull = (1 << 1),
    VGPUShaderStage_Domain = (1 << 2),
    VGPUShaderStage_Geometry = (1 << 3),
    VGPUShaderStage_Fragment = (1 << 4),
    VGPUShaderStage_Compute = (1 << 5),
    VGPUShaderStage_Amplification = (1 << 6),
    VGPUShaderStage_Mesh = (1 << 7),
} VGPUShaderStage;
typedef VGPUFlags VGPUShaderStageFlags;

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

typedef enum VGPULoadAction {
    VGPULoadAction_Load = 0,
    VGPULoadAction_Clear = 1,
    VGPULoadAction_DontCare = 2,

    _VGPULoadAction_Force32 = 0x7FFFFFFF
} VGPULoadAction;

typedef enum vgpu_store_action {
    VGPU_STORE_ACTION_DONT_CARE = 0,
    VGPU_STORE_ACTION_STORE = 1,
    VGPU_STORE_ACTION_RESOLVE = 2,
    VGPU_STORE_ACTION_RESOLVE_AND_RESOLVE = 3,

    _VGPU_STORE_ACTION_FORCE_U32 = 0x7FFFFFFF
} vgpu_store_action;

typedef enum vgpu_index_type {
    VGPU_INDEX_TYPE_UINT16,
    VGPU_INDEX_TYPE_UINT32,

    _VGPU_INDEX_TYPE_COUNT,
    _VGPU_INDEX_TYPE_FORCE_U32 = 0x7FFFFFFF
} vgpu_index_type;

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

typedef enum VGPUSamplerBorderColor {
    VGPUSamplerBorderColor_TransparentBlack = 0,
    VGPUSamplerBorderColor_OpaqueBlack,
    VGPUSamplerBorderColor_OpaqueWhite,

    _VGPUSamplerBorderColor_Force32 = 0x7FFFFFFF
} VGPUSamplerBorderColor;

typedef enum VGPUPrimitiveTopology {
    VGPUPrimitiveTopology_PointList = 0,
    VGPUPrimitiveTopology_LineList = 1,
    VGPUPrimitiveTopology_LineStrip = 2,
    VGPUPrimitiveTopology_TriangleList = 3,
    VGPUPrimitiveTopology_TriangleStrip = 4,
    VGPUPrimitiveTopology_PatchList = 5,

    _VGPUPrimitiveTopology_Force32 = 0x7FFFFFFF
} VGPUPrimitiveTopology;

typedef enum VGPUBlendFactor {
    VGPUBlendFactor_Zero = 0,
    VGPUBlendFactor_One = 1,
    VGPUBlendFactor_SourceColor = 2,
    VGPUBlendFactor_OneMinusSourceColor = 3,
    VGPUBlendFactor_SourceAlpha = 4,
    VGPUBlendFactor_OneMinusSourceAlpha = 5,
    VGPUBlendFactor_DestinationColor = 6,
    VGPUBlendFactor_OneMinusDestinationColor = 7,
    VGPUBlendFactor_DestinationAlpha = 8,
    VGPUBlendFactor_OneMinusDestinationAlpha = 9,
    VGPUBlendFactor_SourceAlphaSaturated = 10,
    VGPUBlendFactor_BlendColor = 11,
    VGPUBlendFactor_OneMinusBlendColor = 12,
    VGPUBlendFactor_BlendAlpha = 13,
    VGPUBlendFactor_OneMinusBlendAlpha = 14,
    VGPUBlendFactor_Source1Color = 15,
    VGPUBlendFactor_OneMinusSource1Color = 16,
    VGPUBlendFactor_Source1Alpha = 17,
    VGPUBlendFactor_OneMinusSource1Alpha = 18,

    _VGPUBlendFactor_Force32 = 0x7FFFFFFF
} VGPUBlendFactor;

typedef enum VGPUBlendOperation {
    VGPUBlendOperation_Add = 0,
    VGPUBlendOperation_Subtract = 1,
    VGPUBlendOperation_ReverseSubtract = 2,
    VGPUBlendOperation_Min = 3,
    VGPUBlendOperation_Max = 4,

    _VGPUBlendOperation_Force32 = 0x7FFFFFFF
} VGPUBlendOperation;

typedef enum VGPUColorWriteMask {
    VGPUColorWriteMask_None = 0,
    VGPUColorWriteMask_Red = 1,
    VGPUColorWriteMask_Green = 2,
    VGPUColorWriteMask_Blue = 4,
    VGPUColorWriteMask_Alpha = 8,
    VGPUColorWriteMask_All = 15,

    _VGPUColorWriteMask_Force32 = 0x7FFFFFFF
} VGPUColorWriteMask;
typedef VGPUFlags VGPUColorWriteMaskFlags;

typedef struct VGPUColor {
    float r;
    float g;
    float b;
    float a;
} VGPUColor;

typedef struct VGPUExtent2D {
    uint32_t width;
    uint32_t height;
} VGPUExtent2D;

typedef struct vgpu_extent3d {
    uint32_t width;
    uint32_t height;
    uint32_t depth_or_array_layers;
} vgpu_extent3d;

typedef struct VGPURect
{
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} VGPURect;

typedef struct VGPUViewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
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
    VGPUTexture         texture;
    uint32_t            level;
    uint32_t            slice;
    VGPULoadAction      loadOp;
    vgpu_store_action   store_action;
    VGPUColor           clear_color;
} VGPURenderPassColorAttachment;

typedef struct VGPURenderPassDepthStencilAttachment {
    VGPUTexture         texture;
    uint32_t            level;
    uint32_t            slice;
    VGPULoadAction      depthLoadOp;
    vgpu_store_action   depthStoreOp;
    float               clearDepth;
    VGPULoadAction      stencilLoadOp;
    vgpu_store_action   stencilStoreOp;
    uint8_t             clearStencil;
} VGPURenderPassDepthStencilAttachment;

typedef struct VGPURenderPassDesc {
    uint32_t colorAttachmentCount;
    const VGPURenderPassColorAttachment* colorAttachments;
    const VGPURenderPassDepthStencilAttachment* depthStencilAttachment;
} VGPURenderPassDesc;

typedef struct vgpu_buffer_desc
{
    uint64_t size;
    vgpu_buffer_usage_flags usage;
    vgpu_cpu_access access;
    uintptr_t handle;
    const char* label;
} vgpu_buffer_desc;

typedef struct VGPUTextureDesc
{
    VGPUTextureUsageFlags usage;
    vgpu_texture_type type;
    vgpu_extent3d size;
    vgpu_pixel_format format;
    uint32_t mipLevelCount;
    uint32_t sampleCount;
    const char* label;
} VGPUTextureDesc;

typedef struct VGPUSamplerDesc {
    const char* label;
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

typedef struct VGPUDepthStencilState
{
    VGPUBool32 depthWriteEnabled;
    VGPUCompareFunction depthCompare;
    uint32_t stencilReadMask;
    uint32_t stencilWriteMask;
    //VGPUStencilState frontFace{};
    //VGPUStencilState backFace{};
} VGPUDepthStencilState;

typedef struct RenderPipelineColorAttachmentDesc
{
    vgpu_pixel_format       format;
    VGPUBool32              blendEnabled;
    VGPUBlendFactor         srcColorBlendFactor;
    VGPUBlendFactor         dstColorBlendFactor;
    VGPUBlendOperation      colorBlendOperation;
    VGPUBlendFactor         srcAlphaBlendFactor;
    VGPUBlendFactor         dstAlphaBlendFactor;
    VGPUBlendOperation      alphaBlendOperation;
    VGPUColorWriteMaskFlags colorWriteMask;
} RenderPipelineColorAttachmentDesc;

typedef struct VGPURenderPipelineDesc {
    const char* label;
    VGPUShaderModule vertex;
    VGPUShaderModule fragment;

    VGPUDepthStencilState depthStencilState;

    VGPUPrimitiveTopology primitiveTopology;
    uint32_t patchControlPoints;

    uint32_t colorAttachmentCount;
    const RenderPipelineColorAttachmentDesc* colorAttachments;
    vgpu_pixel_format depthAttachmentFormat;
    vgpu_pixel_format stencilAttachmentFormat;
    uint32_t sampleCount;
    VGPUBool32 alphaToCoverageEnabled;
} VGPURenderPipelineDesc;

typedef struct VGPUComputePipelineDesc {
    const char* label;
    VGPUShaderModule shader;

} VGPUComputePipelineDesc;

typedef struct VGPURayTracingPipelineDesc {
    const char* label;
} VGPURayTracingPipelineDesc;

typedef struct VGPUSwapChainDesc
{
    uint32_t width;
    uint32_t height;
    vgpu_pixel_format format;
    VGPUPresentMode presentMode;
    VGPUBool32 isFullscreen;
} VGPUSwapChainDesc;

typedef struct vgpu_config {
    const char* label;
    vgpu_backend preferred_backend;
    VGPUValidationMode validationMode;

    struct {
        void*(*gl_get_proc_address)(const char* name);
    } gl;
} vgpu_config;

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
    uint32_t maxTextureDimensionCube;
    uint32_t maxTextureArrayLayers;
    uint64_t maxUniformBufferBindingSize;
    uint64_t maxStorageBufferBindingSize;
    uint32_t minUniformBufferOffsetAlignment;
    uint32_t minStorageBufferOffsetAlignment;
    uint32_t maxVertexBuffers;
    uint32_t maxVertexAttributes;
    uint32_t maxVertexBufferArrayStride;
    uint32_t maxComputeWorkgroupStorageSize;
    uint32_t maxComputeInvocationsPerWorkGroup;
    uint32_t maxComputeWorkGroupSizeX;
    uint32_t maxComputeWorkGroupSizeY;
    uint32_t maxComputeWorkGroupSizeZ;
    uint32_t maxComputeWorkGroupsPerDimension;
    uint32_t maxViewports;
    /// Maximum viewport dimensions.
    uint32_t maxViewportDimensions[2];
    uint32_t maxColorAttachments;
} VGPULimits;

typedef void (*vgpu_log_callback)(vgpu_log_level level, const char* message, void* user_data);
VGPU_API void vgpu_set_log_callback(vgpu_log_callback func, void* userData);

typedef struct vgpu_allocation_callbacks {
    void* (*allocate)(size_t size, void* user_data);
    void (*free)(void* ptr, void* user_data);
    void* user_data;
} vgpu_allocation_callbacks;
VGPU_API void vgpuSetAllocationCallbacks(const vgpu_allocation_callbacks* callback);

VGPU_API VGPUBool32 vgpu_is_supported(vgpu_backend backend);
VGPU_API VGPUBool32 vgpu_init(const vgpu_config* desc);
VGPU_API void vgpu_shutdown(void);
VGPU_API uint64_t vgpu_frame(void);
VGPU_API void vgpu_wait_idle(void);
VGPU_API vgpu_backend vgpu_get_backend(void);
VGPU_API VGPUBool32 vgpuQueryFeature(VGPUFeature feature, void* pInfo, uint32_t infoSize);
VGPU_API void vgpuGetAdapterProperties(VGPUAdapterProperties* properties);
VGPU_API void vgpu_get_limits(VGPULimits* limits);

/* Buffer */
VGPU_API vgpu_buffer* vgpu_buffer_create(const vgpu_buffer_desc* desc, const void* pInitialData);
VGPU_API void vgpu_buffer_destroy(vgpu_buffer* buffer);
VGPU_API VGPUDeviceAddress vgpu_buffer_get_device_address(vgpu_buffer* buffer);
VGPU_API void vgpu_buffer_set_label(vgpu_buffer* buffer, const char* label);

/* Texture */
VGPU_API VGPUTexture vgpuCreateTexture(const VGPUTextureDesc* desc, const void* pInitialData);
VGPU_API VGPUTexture vgpuCreateTexture2D(uint32_t width, uint32_t height, vgpu_pixel_format format, uint32_t mipLevelCount, VGPUTextureUsage usage, const void* pInitialData);
VGPU_API VGPUTexture vgpuCreateTextureCube(uint32_t size, vgpu_pixel_format format, uint32_t mipLevelCount, const void* pInitialData);
VGPU_API void vgpu_texture_destroy(VGPUTexture texture);

/* Sampler */
VGPU_API VGPUSampler* vgpuCreateSampler(const VGPUSamplerDesc* desc);
VGPU_API void vgpu_sampler_destroy(VGPUSampler* sampler);

/* ShaderModule */
VGPU_API VGPUShaderModule vgpuCreateShader(const void* pCode, size_t codeSize);
VGPU_API void vgpu_shader_destroy(VGPUShaderModule module);

/* Pipeline */
VGPU_API VGPUPipeline* vgpu_pipeline_create_render(const VGPURenderPipelineDesc* desc);
VGPU_API VGPUPipeline* vgpu_pipeline_create_compute(const VGPUComputePipelineDesc* desc);
VGPU_API VGPUPipeline* vgpu_pipeline_create_ray_tracing(const VGPURayTracingPipelineDesc* desc);
VGPU_API void vgpu_pipeline_destroy(VGPUPipeline* pipeline);

/* SwapChain */
VGPU_API VGPUSwapChain* vgpuCreateSwapChain(void* window, const VGPUSwapChainDesc* desc);
VGPU_API void vgpuDestroySwapChain(VGPUSwapChain* swapChain);
VGPU_API vgpu_pixel_format vgpuSwapChainGetFormat(VGPUSwapChain* swapChain);

/* Commands */
VGPU_API VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUCommandQueue queueType, const char* label);
VGPU_API void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel);
VGPU_API void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* debugLabel);
VGPU_API void vgpu_set_pipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline* pipeline);

/* Compute commands */
VGPU_API void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
VGPU_API void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, vgpu_buffer* buffer, uint64_t offset);

/* Render commands */
VGPU_API VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight);
VGPU_API void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc);
VGPU_API void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuSetViewport(VGPUCommandBuffer commandBuffer, const VGPUViewport* viewport);
VGPU_API void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* rect);
VGPU_API void vgpu_set_vertex_buffer(VGPUCommandBuffer commandBuffer, uint32_t index, vgpu_buffer* buffer, uint64_t offset);
VGPU_API void vgpu_set_index_buffer(VGPUCommandBuffer commandBuffer, vgpu_buffer* buffer, uint64_t offset, vgpu_index_type type);

VGPU_API void vgpu_draw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance);

VGPU_API void vgpu_submit(VGPUCommandBuffer* commandBuffers, uint32_t count);

/* Helper functions */
typedef struct vgpu_pixel_format_info
{
    vgpu_pixel_format format;
    const char* name;
    uint8_t bytesPerBlock;
    uint8_t blockWidth;
    uint8_t blockHeight;
    VGPUTextureFormatKind kind;
} vgpu_pixel_format_info;

VGPU_API VGPUBool32 vgpu_is_depth_format(vgpu_pixel_format format);
VGPU_API VGPUBool32 vgpu_is_stencil_format(vgpu_pixel_format format);
VGPU_API VGPUBool32 vgpu_is_depth_stencil_format(vgpu_pixel_format format);
VGPU_API VGPUTextureFormatKind vgpuGetPixelFormatKind(vgpu_pixel_format format);
VGPU_API uint32_t vgpu_num_mip_levels_2d(uint32_t width, uint32_t height);
VGPU_API uint32_t vgpu_num_mip_levels_3d(uint32_t width, uint32_t height, uint32_t depth);

VGPU_API uint32_t vgpuToDxgiFormat(vgpu_pixel_format format);
//VGPU_API VGPUTextureFormat vgpuFromDxgiFormat(uint32_t dxgiFormat);

VGPU_API uint32_t vgpuToVkFormat(vgpu_pixel_format format);

#endif /* VGPU_H */
