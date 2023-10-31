// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef VGPU_H_
#define VGPU_H_

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

#define VGPU_MAX_INFLIGHT_FRAMES (2u)
#define VGPU_MAX_COLOR_ATTACHMENTS (8u)
#define VGPU_MAX_VERTEX_ATTRIBUTES (16u)
#define VGPU_WHOLE_SIZE (0xffffffffffffffffULL)

typedef uint32_t VGPUBool32;
typedef uint32_t VGPUFlags;
typedef uint64_t VGPUDeviceAddress;

typedef struct VGPUDeviceImpl*          VGPUDevice;
typedef struct VGPUBufferImpl*          VGPUBuffer;
typedef struct VGPUTextureImpl*         VGPUTexture;
typedef struct VGPUTextureViewImpl*     VGPUTextureView;
typedef struct VGPUSamplerImpl*         VGPUSampler;
typedef struct VGPUBindGroupLayoutImpl* VGPUBindGroupLayout;
typedef struct VGPUPipelineLayoutImpl*  VGPUPipelineLayout;
typedef struct VGPUShaderModuleImpl*    VGPUShaderModule;
typedef struct VGPUPipelineImpl*        VGPUPipeline;
typedef struct VGPUQueryHeapImpl*       VGPUQueryHeap;
typedef struct VGPUSurfaceImpl*         VGPUSurface;
typedef struct VGPUSwapChainImpl*       VGPUSwapChain;
typedef struct VGPUCommandBufferImpl*   VGPUCommandBuffer;

typedef enum VGPULogLevel {
    VGPULogLevel_Off = 0,
    VGPULogLevel_Error = 1,
    VGPULogLevel_Warn = 2,
    VGPULogLevel_Info = 3,
    VGPULogLevel_Debug = 4,
    VGPULogLevel_Trace = 5,

    _VGPULogLevel_Count,
    _VGPULogLevel_Force32 = 0x7FFFFFFF
} VGPULogLevel;

typedef enum VGPUBackend {
    _VGPUBackend_Default = 0,
    VGPUBackend_Vulkan,
    VGPUBackend_D3D12,

    _VGPUBackend_Count,
    _VGPUBackend_Force32 = 0x7FFFFFFF
} VGPUBackend;

typedef enum VGPUValidationMode {
    VGPUValidationMode_Disabled = 0,
    VGPUValidationMode_Enabled,
    VGPUValidationMode_Verbose,
    VGPUValidationMode_GPU,

    _VGPUValidationMode_Count,
    _VGPUValidationMode_Force32 = 0x7FFFFFFF
} VGPUValidationMode;

typedef enum VGPUPowerPreference {
    VGPUPowerPreference_Undefined = 0,
    VGPUPowerPreference_LowPower = 1,
    VGPUPowerPreference_HighPerformance = 2,

    _VGPUPowerPreference_Force32 = 0x7FFFFFFF
} VGPUPowerPreference;

typedef enum VGPUCommandQueue {
    VGPUCommandQueue_Graphics,
    VGPUCommandQueue_Compute,
    VGPUCommandQueue_Copy,

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

typedef enum VGPUCpuAccessMode {
    VGPUCpuAccessMode_None = 0,
    VGPUCpuAccessMode_Write = 1,
    VGPUCpuAccessMode_Read = 2,

    _VGPUCpuAccessMode_Count,
    _VGPUCpuAccessMode_Force32 = 0x7FFFFFFF
} VGPUCpuAccessMode;

typedef enum VGPUBufferUsage {
    VGPUBufferUsage_None = 0,
    VGPUBufferUsage_Vertex = (1 << 0),
    VGPUBufferUsage_Index = (1 << 1),
    VGPUBufferUsage_Constant = (1 << 2),
    VGPUBufferUsage_ShaderRead = (1 << 3),
    VGPUBufferUsage_ShaderWrite = (1 << 4),
    VGPUBufferUsage_Indirect = (1 << 5),
    VGPUBufferUsage_Predication = (1 << 6),
    VGPUBufferUsage_RayTracing = (1 << 7),

    _VGPUBufferUsage_Force32 = 0x7FFFFFFF
} VGPUBufferUsage;
typedef VGPUFlags VGPUBufferUsageFlags;

typedef enum VGPUTextureDimension {
    _VGPUTextureDimension_Default = 0,
    VGPUTextureDimension_1D,
    VGPUTextureDimension_2D,
    VGPUTextureDimension_3D,

    _VGPUTextureDimension_Count,
    _VGPUTextureDimension_Force32 = 0x7FFFFFFF
} VGPUTextureDimension;

typedef enum VGPUTextureUsage {
    VGPUTextureUsage_None = 0,
    VGPUTextureUsage_ShaderRead = (1 << 0),
    VGPUTextureUsage_ShaderWrite = (1 << 1),
    VGPUTextureUsage_RenderTarget = (1 << 2),
    VGPUTextureUsage_Transient = (1 << 3),
    VGPUTextureUsage_ShadingRate = (1 << 4),
    VGPUTextureUsage_Shared = (1 << 5),

    _VGPUTextureUsage_Force32 = 0x7FFFFFFF
} VGPUTextureUsage;
typedef VGPUFlags VGPUTextureUsageFlags;

typedef enum VGPUTextureFormat {
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
    VGPUTextureFormat_Stencil8,
    VGPUTextureFormat_Depth16Unorm,
    VGPUTextureFormat_Depth32Float,
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

    _VGPUTextureFormat_Count,
    _VGPUTextureFormat_Force32 = 0x7FFFFFFF
} VGPUTextureFormat;

typedef enum VGPUFormatKind {
    VGPUFormatKind_Unorm,
    VGPUFormatKind_UnormSrgb,
    VGPUFormatKind_Snorm,
    VGPUFormatKind_Uint,
    VGPUFormatKind_Sint,
    VGPUFormatKind_Float,

    _VGPUFormatKind_Count,
    _VGPUFormatKind_Force32 = 0x7FFFFFFF
} VGPUFormatKind;

typedef enum VGPUPresentMode {
    VGPUPresentMode_Immediate = 0,
    VGPUPresentMode_Mailbox,
    VGPUPresentMode_Fifo,

    _VGPUPresentMode_Count,
    _VGPUPresentMode_Force32 = 0x7FFFFFFF
} VGPUPresentMode;

typedef enum VGPUShaderStage {
    VGPUShaderStage_All = 0,
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
    VGPUFeature_DepthClipControl,
    VGPUFeature_Depth32FloatStencil8,
    VGPUFeature_TimestampQuery,
    VGPUFeature_PipelineStatisticsQuery,
    VGPUFeature_TextureCompressionBC,
    VGPUFeature_TextureCompressionETC2,
    VGPUFeature_TextureCompressionASTC,
    VGPUFeature_IndirectFirstInstance,
    VGPUFeature_ShaderFloat16,
    VGPUFeature_CacheCoherentUMA,

    VGPUFeature_GeometryShader,
    VGPUFeature_TessellationShader,
    VGPUFeature_DepthBoundsTest,
    VGPUFeature_SamplerMinMax,

    VGPUFeature_ShaderOutputViewportIndex,
    VGPUFeature_DescriptorIndexing,
    VGPUFeature_Predication,
    VGPUFeature_VariableRateShading,
    VGPUFeature_VariableRateShadingTier2,
    VGPUFeature_RayTracing,
    VGPUFeature_RayTracingTier2,
    VGPUFeature_MeshShader,

    _VGPUFeature_Force32 = 0x7FFFFFFF
} VGPUFeature;

typedef enum VGPULoadAction {
    VGPULoadAction_Load = 0,
    VGPULoadAction_Clear = 1,
    VGPULoadAction_DontCare = 2,

    _VGPULoadAction_Force32 = 0x7FFFFFFF
} VGPULoadAction;

typedef enum VGPUStoreAction {
    VGPUStoreAction_Store = 0,
    VGPUStoreAction_DontCare = 1,

    _VGPUStoreAction_Force32 = 0x7FFFFFFF
} VGPUStoreAction;

typedef enum VGPUIndexType {
    VGPUIndexType_Uint16,
    VGPUIndexType_Uint32,

    _VGPUIndexType_Count,
    _VGPUIndexType_Force32 = 0x7FFFFFFF
} VGPUIndexType;

typedef enum VGPUCompareFunction {
    _VGPUCompareFunction_Default = 0,
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

typedef enum VGPUStencilOperation {
    VGPUStencilOperation_Keep = 0,
    VGPUStencilOperation_Zero,
    VGPUStencilOperation_Replace,
    VGPUStencilOperation_IncrementClamp,
    VGPUStencilOperation_DecrementClamp,
    VGPUStencilOperation_Invert,
    VGPUStencilOperation_IncrementWrap,
    VGPUStencilOperation_DecrementWrap,
} VGPUStencilOperation;

typedef enum VGPUSamplerFilter {
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

typedef enum VGPUFillMode {
    VGPUFillMode_Solid = 0,
    VGPUFillMode_Wireframe = 1,

    _VGPUFillMode_Force32 = 0x7FFFFFFF
} VGPUFillMode;

typedef enum VGPUCullMode {
    VGPUCullMode_Back = 0,
    VGPUCullMode_Front = 1,
    VGPUCullMode_None = 2,

    _VGPUCullMode_Force32 = 0x7FFFFFFF
} VGPUCullMode;

typedef enum VGPUDepthClipMode {
    VGPUDepthClipMode_Clip = 0,
    VGPUDepthClipMode_Clamp = 1,

    _VGPUDepthClipMode_Force32 = 0x7FFFFFFF
} VGPUDepthClipMode;

typedef enum VGPUPrimitiveTopology {
    _VGPUPrimitiveTopology_Default = 0,
    VGPUPrimitiveTopology_PointList,
    VGPUPrimitiveTopology_LineList,
    VGPUPrimitiveTopology_LineStrip,
    VGPUPrimitiveTopology_TriangleList,
    VGPUPrimitiveTopology_TriangleStrip,
    VGPUPrimitiveTopology_PatchList,

    _VGPUPrimitiveTopology_Force32 = 0x7FFFFFFF
} VGPUPrimitiveTopology;

typedef enum VGPUBlendFactor {
    _VGPUBlendFactor_Default = 0,
    VGPUBlendFactor_Zero,
    VGPUBlendFactor_One,
    VGPUBlendFactor_SourceColor,
    VGPUBlendFactor_OneMinusSourceColor,
    VGPUBlendFactor_SourceAlpha,
    VGPUBlendFactor_OneMinusSourceAlpha,
    VGPUBlendFactor_DestinationColor,
    VGPUBlendFactor_OneMinusDestinationColor,
    VGPUBlendFactor_DestinationAlpha,
    VGPUBlendFactor_OneMinusDestinationAlpha,
    VGPUBlendFactor_SourceAlphaSaturated,
    VGPUBlendFactor_BlendColor,
    VGPUBlendFactor_OneMinusBlendColor,
    VGPUBlendFactor_BlendAlpha,
    VGPUBlendFactor_OneMinusBlendAlpha,
    VGPUBlendFactor_Source1Color,
    VGPUBlendFactor_OneMinusSource1Color,
    VGPUBlendFactor_Source1Alpha,
    VGPUBlendFactor_OneMinusSource1Alpha,

    _VGPUBlendFactor_Count,
    _VGPUBlendFactor_Force32 = 0x7FFFFFFF
} VGPUBlendFactor;

typedef enum VGPUBlendOperation {
    _VGPUBlendOperation_Default = 0,
    VGPUBlendOperation_Add,
    VGPUBlendOperation_Subtract,
    VGPUBlendOperation_ReverseSubtract,
    VGPUBlendOperation_Min,
    VGPUBlendOperation_Max,

    _VGPUBlendOperation_Force32 = 0x7FFFFFFF
} VGPUBlendOperation;

typedef enum VGPUColorWriteMask {
    _VGPUColorWriteMask_Default = 0,
    VGPUColorWriteMask_None = 0x10, 
    VGPUColorWriteMask_Red = 0x01,
    VGPUColorWriteMask_Green = 0x02,
    VGPUColorWriteMask_Blue = 0x04,
    VGPUColorWriteMask_Alpha = 0x08,
    VGPUColorWriteMask_All = 0x0F,

    _VGPUColorWriteMask_Force32 = 0x7FFFFFFF
} VGPUColorWriteMask;
typedef VGPUFlags VGPUColorWriteMaskFlags;

typedef enum VGPUVertexFormat {
    VGPUVertexFormat_Undefined = 0x00000000,
    VGPUVertexFormat_UByte2,
    VGPUVertexFormat_UByte4,
    VGPUVertexFormat_Byte2,
    VGPUVertexFormat_Byte4,
    VGPUVertexFormat_UByte2Normalized,
    VGPUVertexFormat_UByte4Normalized,
    VGPUVertexFormat_Byte2Normalized,
    VGPUVertexFormat_Byte4Normalized,
    VGPUVertexFormat_UShort2,
    VGPUVertexFormat_UShort4,
    VGPUVertexFormat_Short2,
    VGPUVertexFormat_Short4,
    VGPUVertexFormat_UShort2Normalized,
    VGPUVertexFormat_UShort4Normalized,
    VGPUVertexFormat_Short2Normalized,
    VGPUVertexFormat_Short4Normalized,
    VGPUVertexFormat_Half2,
    VGPUVertexFormat_Half4,
    VGPUVertexFormat_Float,
    VGPUVertexFormat_Float2,
    VGPUVertexFormat_Float3,
    VGPUVertexFormat_Float4,
    VGPUVertexFormat_UInt,
    VGPUVertexFormat_UInt2,
    VGPUVertexFormat_UInt3,
    VGPUVertexFormat_UInt4,
    VGPUVertexFormat_Int,
    VGPUVertexFormat_Int2,
    VGPUVertexFormat_Int3,
    VGPUVertexFormat_Int4,
    VGPUVertexFormat_Int1010102Normalized,
    VGPUVertexFormat_UInt1010102Normalized,
    _VGPUVertexFormat_Force32 = 0x7FFFFFFF
} VGPUVertexFormat;

typedef enum VGPUVertexStepMode {
    VGPUVertexStepMode_Vertex = 0,
    VGPUVertexStepMode_Instance = 1,

    _VGPUVertexStepMode_Force32 = 0x7FFFFFFF
} VGPUVertexStepMode;

typedef enum VGPUPipelineType
{
    VGPUPipelineType_Render = 0,
    VGPUPipelineType_Compute = 1,
    VGPUPipelineType_RayTracing = 2,

    _VGPUPipelineType_Force32 = 0x7FFFFFFF
} VGPUPipelineType;

typedef enum VGPUQueryType {
    /// Used for occlusion query heap or occlusion queries
    VGPUQueryType_Occlusion = 0,
    /// Can be used in the same heap as occlusion
    VGPUQueryType_BinaryOcclusion = 1,
    /// Create a heap to contain timestamp queries
    VGPUQueryType_Timestamp = 2,
    /// Create a heap to contain a structure of `PipelineStatistics`
    //VGPUQueryType_PipelineStatistics = 3,

    _VGPUQueryType_Force32 = 0x7FFFFFFF
} VGPUQueryType;

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

typedef struct VGPUExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} VGPUExtent3D;

typedef struct VGPURect {
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
    uint32_t firstVertex;
    uint32_t firstInstance;
} VGPUDrawIndirectCommand;

typedef struct VGPUDrawIndexedIndirectCommand
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  baseVertex;
    uint32_t firstInstance;
} VGPUDrawIndexedIndirectCommand;

typedef struct VGPURenderPassColorAttachment {
    VGPUTexture         texture;
    uint32_t            level;
    uint32_t            slice;
    VGPULoadAction      loadAction;
    VGPUStoreAction     storeAction;
    VGPUColor           clearColor;
} VGPURenderPassColorAttachment;

typedef struct VGPURenderPassDepthStencilAttachment {
    VGPUTexture         texture;
    uint32_t            level;
    uint32_t            slice;
    VGPULoadAction      depthLoadAction;
    VGPUStoreAction     depthStoreAction;
    float               depthClearValue;
    VGPULoadAction      stencilLoadAction;
    VGPUStoreAction     stencilStoreAction;
    uint32_t            stencilClearValue;
} VGPURenderPassDepthStencilAttachment;

typedef struct VGPURenderPassDesc {
    const char* label;
    uint32_t colorAttachmentCount;
    const VGPURenderPassColorAttachment* colorAttachments;
    const VGPURenderPassDepthStencilAttachment* depthStencilAttachment;
} VGPURenderPassDesc;

typedef struct VGPUBufferDesc {
    const char* label;
    uint64_t size;
    VGPUBufferUsageFlags usage;
    VGPUCpuAccessMode cpuAccess;
    uintptr_t handle;
} VGPUBufferDesc;

typedef struct VGPUTextureDesc {
    const char* label;
    VGPUTextureDimension dimension;
    VGPUTextureFormat format;
    VGPUTextureUsageFlags usage;
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    uint32_t mipLevelCount;
    uint32_t sampleCount;
    VGPUCpuAccessMode cpuAccess;
} VGPUTextureDesc;

typedef struct VGPUTextureData
{
    const void* pData;
    uint32_t rowPitch;
    uint32_t slicePitch;
} VGPUTextureData;

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

typedef enum VGPUDescriptorType {
    VGPUDescriptorType_ShaderResource,
    VGPUDescriptorType_ConstantBuffer,
    VGPUDescriptorType_UnorderedAccess,
    VGPUDescriptorType_Sampler,

    _VGPUDescriptorType_Force32 = 0x7FFFFFFF
} VGPUDescriptorType;

typedef struct VGPUDescriptorRangeDesc {
    uint32_t baseRegisterIndex;
    uint32_t descriptorNum;
    VGPUDescriptorType descriptorType;
    VGPUShaderStage visibility;
} VGPUDescriptorRangeDesc;

typedef struct VGPUDescriptorSetDesc {
    uint32_t registerSpace;
    uint32_t rangeCount;
    const VGPUDescriptorRangeDesc* ranges;
} VGPUDescriptorSetDesc;

typedef struct VGPUBindGroupLayoutDesc {
    const char* label;
} VGPUBindGroupLayoutDesc;

typedef struct VGPUPushConstantRange {
    /// Register index to bind to (supplied in shader).
    uint32_t shaderRegister;
    /// Size in bytes.
    uint32_t size;
    /// The shader stage the constants will be accessible to.
    VGPUShaderStageFlags visibility;
} VGPUPushConstantRange;

typedef struct VGPUPipelineLayoutDesc {
    const char* label;
    uint32_t descriptorSetCount;
    const VGPUDescriptorSetDesc* descriptorSets;
    uint32_t pushConstantRangeCount;
    const VGPUPushConstantRange* pushConstantRanges;
} VGPUPipelineLayoutDesc;

typedef struct VGPUShaderModuleDesc {
    const char*     label;
    const void*     pCode;
    size_t          codeSize;
} VGPUShaderModuleDesc;

typedef struct VGPUShaderStageDesc {
    VGPUShaderModule module;
    VGPUShaderStage stage;
    const char*     entryPoint;
} VGPUShaderStageDesc;

typedef struct VGPURenderTargetBlendState {
    VGPUBool32              blendEnabled;
    VGPUBlendFactor         srcColorBlendFactor;
    VGPUBlendFactor         dstColorBlendFactor;
    VGPUBlendOperation      colorBlendOperation;
    VGPUBlendFactor         srcAlphaBlendFactor;
    VGPUBlendFactor         dstAlphaBlendFactor;
    VGPUBlendOperation      alphaBlendOperation;
    VGPUColorWriteMaskFlags colorWriteMask;
} VGPURenderTargetBlendState;

typedef struct VGPUBlendState {
    VGPUBool32 alphaToCoverageEnable;
    VGPUBool32 independentBlendEnable;

    VGPURenderTargetBlendState renderTargets[VGPU_MAX_COLOR_ATTACHMENTS];
} VGPUBlendState;

typedef struct VGPURasterizerState {
    VGPUFillMode fillMode;
    VGPUCullMode cullMode;
    VGPUBool32 frontFaceCounterClockwise;
    VGPUDepthClipMode depthClipMode;
    float depthBias;
    float depthBiasClamp;
    float slopeScaledDepthBias;
} VGPURasterizerState;

typedef struct VGPUStencilFaceState {
    VGPUCompareFunction compareFunction;
    VGPUStencilOperation failOperation;
    VGPUStencilOperation depthFailOperation;
    VGPUStencilOperation passOperation;
} VGPUStencilFaceState;

typedef struct VGPUDepthStencilState {
    VGPUBool32 depthWriteEnabled;
    VGPUCompareFunction depthCompareFunction;
    VGPUStencilFaceState stencilFront;
    VGPUStencilFaceState stencilBack;
    uint32_t stencilReadMask;
    uint32_t stencilWriteMask;
    VGPUBool32 depthBoundsTestEnable;
} VGPUDepthStencilState;

typedef struct VGPUVertexAttribute {
    VGPUVertexFormat format;
    uint32_t offset;
    uint32_t shaderLocation;
} VGPUVertexAttribute;

typedef struct VGPUVertexBufferLayout {
    uint32_t stride;
    VGPUVertexStepMode stepMode;
    uint32_t attributeCount;
    const VGPUVertexAttribute* attributes;
} VGPUVertexBufferLayout;

typedef struct VGPUVertexState {
    uint32_t layoutCount;
    const VGPUVertexBufferLayout* layouts;
} VGPUVertexState;

typedef struct VGPURenderPipelineDesc {
    const char* label;
    VGPUPipelineLayout          layout;

    uint32_t                    shaderStageCount;
    const VGPUShaderStageDesc*  shaderStages;

    VGPUVertexState             vertex;

    VGPUBlendState              blendState;
    VGPURasterizerState         rasterizerState;
    VGPUDepthStencilState       depthStencilState;

    VGPUPrimitiveTopology       primitiveTopology;
    uint32_t                    patchControlPoints;

    uint32_t                    colorFormatCount;
    const VGPUTextureFormat*    colorFormats;
    VGPUTextureFormat           depthStencilFormat;
    uint32_t                    sampleCount;
} VGPURenderPipelineDesc;

typedef struct VGPUComputePipelineDesc {
    const char*         label;
    VGPUPipelineLayout  layout;
    VGPUShaderStageDesc computeShader;
} VGPUComputePipelineDesc;

typedef struct VGPURayTracingPipelineDesc {
    const char* label;
    VGPUPipelineLayout layout;
} VGPURayTracingPipelineDesc;

typedef struct VGPUQueryHeapDesc {
    const char* label;
    VGPUQueryType type;
    uint32_t count;
    //VGPUQueryPipelineStatisticFlags pipelineStatistics;
} VGPUQueryHeapDesc;

typedef struct VGPUSwapChainDesc {
    const char* label;
    void* displayHandle;
    uintptr_t windowHandle;
    uint32_t width;
    uint32_t height;
    VGPUTextureFormat format;
    VGPUPresentMode presentMode;
    VGPUBool32 isFullscreen;
} VGPUSwapChainDesc;

typedef struct VGPUDeviceDescriptor {
    const char* label;
    VGPUBackend preferredBackend;
    VGPUValidationMode validationMode;
    VGPUPowerPreference powerPreference;
} VGPUDeviceDescriptor;

typedef struct VGPUAdapterProperties {
    uint32_t vendorId;
    uint32_t deviceId;
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
    uint64_t maxConstantBufferBindingSize;
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

    // Ray tracing
    uint64_t rayTracingShaderGroupIdentifierSize;
    uint64_t rayTracingShaderTableAligment;
    uint64_t rayTracingShaderTableMaxStride;
    uint32_t rayTracingShaderRecursionMaxDepth;
    uint32_t rayTracingMaxGeometryCount;
} VGPULimits;

typedef uint32_t VGPUNativeObjectType;

#define VGPU_NATIVE_D3D12Device (0x00020001)

#define VGPU_NATIVE_VkDevice (0x00030001)
#define VGPU_NATIVE_VkPhysicalDevice (0x00030002)
#define VGPU_NATIVE_VkInstance (0x00030003)

typedef void (*VGPULogCallback)(VGPULogLevel level, const char* message, void* userData);
VGPU_API VGPULogLevel vgpuGetLogLevel(void);
VGPU_API void vgpuSetLogLevel(VGPULogLevel level);
VGPU_API void vgpuSetLogCallback(VGPULogCallback func, void* userData);

VGPU_API VGPUBool32 vgpuIsBackendSupported(VGPUBackend backend);
VGPU_API VGPUDevice vgpuCreateDevice(const VGPUDeviceDescriptor* descriptor);
VGPU_API void vgpuDeviceSetLabel(VGPUDevice device, const char* label);
VGPU_API uint32_t vgpuDeviceAddRef(VGPUDevice device);
VGPU_API uint32_t vgpuDeviceRelease(VGPUDevice device);

VGPU_API void vgpuDeviceWaitIdle(VGPUDevice device);
VGPU_API VGPUBackend vgpuDeviceGetBackend(VGPUDevice device);
VGPU_API VGPUBool32 vgpuDeviceQueryFeatureSupport(VGPUDevice device, VGPUFeature feature);
VGPU_API void vgpuDeviceGetAdapterProperties(VGPUDevice device, VGPUAdapterProperties* properties);
VGPU_API void vgpuDeviceGetLimits(VGPUDevice device, VGPULimits* limits);
VGPU_API uint64_t vgpuDeviceSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count);
VGPU_API uint64_t vgpuDeviceGetFrameCount(VGPUDevice device);
VGPU_API uint32_t vgpuDeviceGetFrameIndex(VGPUDevice device);
VGPU_API uint64_t vgpuDeviceGetTimestampFrequency(VGPUDevice device);
VGPU_API void* vgpuDeviceGetNativeObject(VGPUDevice device, VGPUNativeObjectType objectType);

/* Buffer */
VGPU_API VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDesc* desc, const void* pInitialData);
VGPU_API uint64_t vgpuBufferGetSize(VGPUBuffer buffer);
VGPU_API VGPUBufferUsageFlags vgpuBufferGetUsage(VGPUBuffer buffer);
VGPU_API VGPUDeviceAddress vgpuBufferGetAddress(VGPUBuffer buffer);
VGPU_API void vgpuBufferSetLabel(VGPUBuffer buffer, const char* label);
VGPU_API uint32_t vgpuBufferAddRef(VGPUBuffer buffer);
VGPU_API uint32_t vgpuBufferRelease(VGPUBuffer buffer);

/* Texture methods */
VGPU_API VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const VGPUTextureData* pInitialData);
VGPU_API VGPUTextureDimension vgpuTextureGetDimension(VGPUTexture texture);
VGPU_API VGPUTextureFormat vgpuTextureGetFormat(VGPUTexture texture);
VGPU_API void vgpuTextureSetLabel(VGPUTexture texture, const char* label);
VGPU_API uint32_t vgpuTextureAddRef(VGPUTexture texture);
VGPU_API uint32_t vgpuTextureRelease(VGPUTexture texture);

/* Sampler */
VGPU_API VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDesc* desc);
VGPU_API void vgpuSamplerSetLabel(VGPUSampler sampler, const char* label);
VGPU_API uint32_t vgpuSamplerAddRef(VGPUSampler sampler);
VGPU_API uint32_t vgpuSamplerRelease(VGPUSampler sampler);

/* BindGroupLayout */
VGPU_API VGPUBindGroupLayout vgpuCreateBindGroupLayout(VGPUDevice device, const VGPUBindGroupLayoutDesc* desc);
VGPU_API void vgpuBindGroupLayoutSetLabel(VGPUBindGroupLayout bindGroupLayout, const char* label);
VGPU_API uint32_t vgpuBindGroupLayoutAddRef(VGPUPipelineLayout bindGroupLayout);
VGPU_API uint32_t vgpuBindGroupLayoutRelease(VGPUPipelineLayout bindGroupLayout);

/* PipelineLayout */
VGPU_API VGPUPipelineLayout vgpuCreatePipelineLayout(VGPUDevice device, const VGPUPipelineLayoutDesc* desc);
VGPU_API void vgpuPipelineLayoutSetLabel(VGPUPipelineLayout pipelineLayout, const char* label);
VGPU_API uint32_t vgpuPipelineLayoutAddRef(VGPUPipelineLayout pipelineLayout);
VGPU_API uint32_t vgpuPipelineLayoutRelease(VGPUPipelineLayout pipelineLayout);

/* ShaderModule */
VGPU_API VGPUShaderModule vgpuCreateShaderModule(VGPUDevice device, const VGPUShaderModuleDesc* desc);
VGPU_API void vgpuShaderModuleSetLabel(VGPUShaderModule shaderModule, const char* label);
VGPU_API uint32_t vgpuShaderModuleAddRef(VGPUShaderModule shaderModule);
VGPU_API uint32_t vgpuShaderModuleRelease(VGPUShaderModule shaderModule);

/* Pipeline */
VGPU_API VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDesc* desc);
VGPU_API VGPUPipeline vgpuCreateComputePipeline(VGPUDevice device, const VGPUComputePipelineDesc* desc);
VGPU_API VGPUPipeline vgpuCreateRayTracingPipeline(VGPUDevice device, const VGPURayTracingPipelineDesc* desc);
VGPU_API VGPUPipelineType vgpuPipelineGetType(VGPUPipeline pipeline);
VGPU_API void vgpuPipelineSetLabel(VGPUPipeline pipeline, const char* label);
VGPU_API uint32_t vgpuPipelineAddRef(VGPUPipeline pipeline);
VGPU_API uint32_t vgpuPipelineRelease(VGPUPipeline pipeline);

/* QueryHeap */
VGPU_API VGPUQueryHeap vgpuCreateQueryHeap(VGPUDevice device, const VGPUQueryHeapDesc* desc);
VGPU_API VGPUQueryType vgpuQueryHeapGetType(VGPUQueryHeap queryHeap);
VGPU_API uint32_t vgpuQuerySetGetCount(VGPUQueryHeap queryHeap);
VGPU_API void vgpuQueryHeapSetLabel(VGPUQueryHeap queryHeap, const char* label);
VGPU_API uint32_t vgpuQueryHeapAddRef(VGPUQueryHeap queryHeap);
VGPU_API uint32_t vgpuQueryHeapRelease(VGPUQueryHeap queryHeap);

/* SwapChain */
VGPU_API VGPUSwapChain vgpuCreateSwapChain(VGPUDevice device, const VGPUSwapChainDesc* desc);
VGPU_API VGPUTextureFormat vgpuSwapChainGetFormat(VGPUSwapChain swapChain);
VGPU_API void vgpuSwapChainGetSize(VGPUSwapChain swapChain, uint32_t* width, uint32_t* height);
VGPU_API uint32_t vgpuSwapChainAddRef(VGPUSwapChain swapChain);
VGPU_API uint32_t vgpuSwapChainRelease(VGPUSwapChain swapChain);

/* Commands */
VGPU_API VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, VGPUCommandQueue queueType, const char* label);
VGPU_API void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel);
VGPU_API void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* markerLabel);
VGPU_API void vgpuSetPipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline pipeline);
VGPU_API void vgpuSetPushConstants(VGPUCommandBuffer commandBuffer, uint32_t pushConstantIndex, const void* data, uint32_t size);

/* Compute commands */
VGPU_API void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
VGPU_API void vgpuDispatch1D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t groupSizeX);
VGPU_API void vgpuDispatch2D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t threadCountY, uint32_t groupSizeX, uint32_t groupSizeY);
VGPU_API void vgpuDispatch3D(VGPUCommandBuffer commandBuffer, uint32_t threadCountX, uint32_t threadCountY, uint32_t threadCountZ, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);
VGPU_API void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset);

/* Render commands */
VGPU_API VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain swapChain);
VGPU_API void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc);
VGPU_API void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuSetViewport(VGPUCommandBuffer commandBuffer, const VGPUViewport* viewport);
VGPU_API void vgpuSetViewports(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPUViewport* viewports);
VGPU_API void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* scissorRect);
VGPU_API void vgpuSetScissorRects(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPURect* scissorRects);

VGPU_API void vgpuSetVertexBuffer(VGPUCommandBuffer commandBuffer, uint32_t index, VGPUBuffer buffer, uint64_t offset);
VGPU_API void vgpuSetIndexBuffer(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, VGPUIndexType type, uint64_t offset);
VGPU_API void vgpuSetStencilReference(VGPUCommandBuffer commandBuffer, uint32_t reference);

VGPU_API void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
VGPU_API void vgpuDrawIndexed(VGPUCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);
VGPU_API void vgpuDrawIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset);
VGPU_API void vgpuDrawIndexedIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer indirectBuffer, uint64_t indirectBufferOffset);

VGPU_API void vgpuBeginQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index);
VGPU_API void vgpuEndQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index);
VGPU_API void vgpuResolveQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index, uint32_t count, VGPUBuffer destinationBuffer, uint64_t destinationOffset);
VGPU_API void vgpuResetQuery(VGPUCommandBuffer commandBuffer, VGPUQueryHeap queryHeap, uint32_t index, uint32_t count);

/* Helper functions */
typedef struct VGPUPixelFormatInfo {
    VGPUTextureFormat format;
    const char* name;
    uint8_t bytesPerBlock;
    uint8_t blockWidth;
    uint8_t blockHeight;
    VGPUFormatKind kind;
} VGPUPixelFormatInfo;

typedef struct VGPUVertexFormatInfo {
    VGPUVertexFormat format;
    uint32_t byteSize;
    uint32_t componentCount;
    uint32_t componentByteSize;
    VGPUFormatKind baseType;
} VGPUVertexFormatInfo;

VGPU_API VGPUBool32 vgpuIsDepthFormat(VGPUTextureFormat format);
VGPU_API VGPUBool32 vgpuIsDepthOnlyFormat(VGPUTextureFormat format);
VGPU_API VGPUBool32 vgpuIsStencilOnlyFormat(VGPUTextureFormat format);
VGPU_API VGPUBool32 vgpuIsStencilFormat(VGPUTextureFormat format);
VGPU_API VGPUBool32 vgpuIsDepthStencilFormat(VGPUTextureFormat format);
VGPU_API VGPUBool32 vgpuIsCompressedFormat(VGPUTextureFormat format);

VGPU_API VGPUFormatKind vgpuGetPixelFormatKind(VGPUTextureFormat format);

VGPU_API uint32_t vgpuToDxgiFormat(VGPUTextureFormat format);
VGPU_API VGPUTextureFormat vgpuFromDxgiFormat(uint32_t dxgiFormat);

VGPU_API uint32_t vgpuToVkFormat(VGPUTextureFormat format);

VGPU_API VGPUBool32 vgpuStencilTestEnabled(const VGPUDepthStencilState* depthStencil);

VGPU_API void vgpuGetPixelFormatInfo(VGPUTextureFormat format, VGPUPixelFormatInfo* pInfo);
VGPU_API void vgpuGetVertexFormatInfo(VGPUVertexFormat format, VGPUVertexFormatInfo* pInfo);

#endif /* VGPU_H_ */
