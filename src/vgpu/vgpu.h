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

typedef struct VGPUDeviceImpl* VGPUDevice;
typedef struct VGPUBufferImpl* VGPUBuffer;
typedef struct VGPUTextureImpl* VGPUTexture;
typedef struct VGPUTextureViewImpl* VGPUTextureView;
typedef struct VGPUSamplerImpl* VGPUSampler;
typedef struct VGPUSwapChain VGPUSwapChain;
typedef struct VGPUShaderModuleImpl* VGPUShaderModule;
typedef struct VGPUPipelineLayoutImpl* VGPUPipelineLayout;
typedef struct VGPUPipelineImpl* VGPUPipeline;
typedef struct VGPUCommandBuffer_T* VGPUCommandBuffer;

typedef enum VGPULogLevel {
    VGPULogLevel_Error = 0,
    VGPULogLevel_Warning = 1,
    VGPULogLevel_Info = 2,

    _VGPULogLevel_Count,
    _VGPULogLevel_Force32 = 0x7FFFFFFF
} VGPULogLevel;

typedef enum VGPUBackend {
    _VGPUBackend_Default = 0,
    VGPUBackend_Vulkan,
    VGPUBackend_D3D12,
    VGPUBackend_D3D11,

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

typedef enum VGPUCpuAccessMode
{
    VGPUCpuAccessMode_None = 0,
    VGPUCpuAccessMode_Write = 1,
    VGPUCpuAccessMode_Read = 2,

    _VGPUCpuAccessMode_Count,
    _VGPUCpuAccessMode_Force32 = 0x7FFFFFFF
} VGPUCpuAccessMode;

typedef enum VGPUBufferUsage {
    VGPUBufferUsage_None = 0,
    VGPUBufferUsage_Vertex = 0x00000001,
    VGPUBufferUsage_Index = 0x00000002,
    VGPUBufferUsage_Constant = 0x00000004,
    VGPUBufferUsage_ShaderRead = 0x00000008,
    VGPUBufferUsage_ShaderWrite = 0x00000010,
    VGPUBufferUsage_Indirect = 0x00000020,
    VGPUBufferUsage_Predication = 0x00000040,
    VGPUBufferUsage_RayTracing = 0x00000080,

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
    VGPUFeature_Predication,
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

typedef enum VGPUStoreAction {
    VGPUStoreAction_Store = 0,
    VGPUStoreAction_DontCare = 1,

    _VGPUStoreAction_Force32 = 0x7FFFFFFF
} VGPUStoreAction;

typedef enum VGPUIndexType {
    VGPUIndexType_UInt16,
    VGPUIndexType_UInt32,

    _VGPUIndexType_Count,
    _VGPUIndexType_Force32 = 0x7FFFFFFF
} VGPUIndexType;

typedef enum VGPUCompareFunction
{
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

typedef enum VGPUStencilOperation
{
    _VGPUStencilOperation_Default = 0,
    VGPUStencilOperation_Keep,
    VGPUStencilOperation_Zero,
    VGPUStencilOperation_Replace,
    VGPUStencilOperation_IncrementClamp,
    VGPUStencilOperation_DecrementClamp,
    VGPUStencilOperation_Invert,
    VGPUStencilOperation_IncrementWrap,
    VGPUStencilOperation_DecrementWrap,
} VGPUStencilOperation;


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
    //VGPUBlendFactor_BlendAlpha = 13,
    //VGPUBlendFactor_OneMinusBlendAlpha = 14,
    //VGPUBlendFactor_Source1Color = 15,
    //VGPUBlendFactor_OneMinusSource1Color = 16,
    //VGPUBlendFactor_Source1Alpha = 17,
    //VGPUBlendFactor_OneMinusSource1Alpha = 18,

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
    VGPULoadAction      depthLoadOp;
    VGPUStoreAction     depthStoreOp;
    float               clearDepth;
    VGPULoadAction      stencilLoadOp;
    VGPUStoreAction     stencilStoreOp;
    uint8_t             clearStencil;
} VGPURenderPassDepthStencilAttachment;

typedef struct VGPURenderPassDesc {
    uint32_t colorAttachmentCount;
    const VGPURenderPassColorAttachment* colorAttachments;
    const VGPURenderPassDepthStencilAttachment* depthStencilAttachment;
} VGPURenderPassDesc;

typedef struct VGPUBufferDescriptor
{
    const char* label;
    uint64_t size;
    VGPUBufferUsageFlags usage;
    VGPUCpuAccessMode cpuAccess;
    uintptr_t handle;
} VGPUBufferDescriptor;

typedef struct VGPUTextureDesc
{
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

typedef struct VGPUPushConstantDesc {
    VGPUShaderStage visibility;
    uint32_t shaderRegister;
    uint32_t size;
} VGPUPushConstantDesc;

typedef struct VGPUPipelineLayoutDescriptor {
    const char* label;
    uint32_t descriptorSetCount;
    const VGPUDescriptorSetDesc* descriptorSets;
    uint32_t pushConstantCount;
    const VGPUPushConstantDesc* pushConstants;
} VGPUPipelineLayoutDescriptor;

typedef struct VGPUStencilFaceState {
    VGPUCompareFunction compareFunction;
    VGPUStencilOperation failOperation;
    VGPUStencilOperation depthFailOperation;
    VGPUStencilOperation passOperation;
} VGPUStencilFaceState;

typedef struct VGPUDepthStencilState {
    VGPUTextureFormat format;
    VGPUBool32 depthWriteEnabled;
    VGPUCompareFunction depthCompareFunction;
    VGPUStencilFaceState stencilFront;
    VGPUStencilFaceState stencilBack;
    uint32_t stencilReadMask;
    uint32_t stencilWriteMask;
} VGPUDepthStencilState;

typedef struct RenderPipelineColorAttachmentDesc
{
    VGPUTextureFormat       format;
    VGPUBool32              blendEnabled;
    VGPUBlendFactor         srcColorBlendFactor;
    VGPUBlendFactor         dstColorBlendFactor;
    VGPUBlendOperation      colorBlendOperation;
    VGPUBlendFactor         srcAlphaBlendFactor;
    VGPUBlendFactor         dstAlphaBlendFactor;
    VGPUBlendOperation      alphaBlendOperation;
    VGPUColorWriteMaskFlags colorWriteMask;
} RenderPipelineColorAttachmentDesc;

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
    VGPUShaderModule module;
    uint32_t layoutCount;
    const VGPUVertexBufferLayout* layouts;
} VGPUVertexState;

typedef struct VGPUPrimitiveState {
    VGPUPrimitiveTopology topology;
    //VGPUIndexFormat stripIndexFormat;
    //VGPUFrontFace frontFace;
    //VGPUCullMode cullMode;
    uint32_t patchControlPoints;
} VGPUPrimitiveState;

typedef struct VGPURenderPipelineDesc {
    const char* label;
    VGPUPipelineLayout layout;
    VGPUVertexState vertex;
    VGPUPrimitiveState primitive;
    VGPUShaderModule fragment;

    VGPUDepthStencilState depthStencilState;

    uint32_t colorAttachmentCount;
    const RenderPipelineColorAttachmentDesc* colorAttachments;
    uint32_t sampleCount;
    VGPUBool32 alphaToCoverageEnabled;
} VGPURenderPipelineDesc;

typedef struct VGPUComputePipelineDescriptor {
    const char* label;
    VGPUPipelineLayout layout;
    VGPUShaderModule shader;
} VGPUComputePipelineDescriptor;

typedef struct VGPURayTracingPipelineDesc {
    const char* label;
} VGPURayTracingPipelineDesc;

typedef struct VGPUSwapChainDesc {
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
} VGPUDeviceDescriptor;

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
} VGPULimits;

typedef void (*VGPULogCallback)(VGPULogLevel level, const char* message, void* user_data);
VGPU_API void vgpuSetLogCallback(VGPULogCallback func, void* userData);

VGPU_API VGPUBool32 vgpuIsBackendSupported(VGPUBackend backend);
VGPU_API VGPUDevice vgpuCreateDevice(const VGPUDeviceDescriptor* descriptor);
VGPU_API void vgpuDestroyDevice(VGPUDevice device);
VGPU_API void vgpuDeviceSetLabel(VGPUDevice device, const char* label);

VGPU_API void vgpuWaitIdle(VGPUDevice device);
VGPU_API VGPUBackend vgpuGetBackend(VGPUDevice device);
VGPU_API VGPUBool32 vgpuQueryFeature(VGPUDevice device, VGPUFeature feature, void* pInfo, uint32_t infoSize);
VGPU_API void vgpuGetAdapterProperties(VGPUDevice device, VGPUAdapterProperties* properties);
VGPU_API void vgpuGetLimits(VGPUDevice device, VGPULimits* limits);
VGPU_API uint64_t vgpuSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count);
VGPU_API uint64_t vgpuGetFrameCount(VGPUDevice device);
VGPU_API uint32_t vgpuGetFrameIndex(VGPUDevice device);

/* Buffer */
VGPU_API VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDescriptor* descriptor, const void* init_data);
VGPU_API void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer);
VGPU_API VGPUDeviceAddress vgpuGetBufferDeviceAddress(VGPUDevice device, VGPUBuffer buffer);
VGPU_API void vgpuSetBufferLabel(VGPUDevice device, VGPUBuffer buffer, const char* label);

/* Texture methods */
VGPU_API VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const void* init_data);
VGPU_API void vgpuTextureDestroy(VGPUTexture texture);
VGPU_API VGPUTextureDimension vgpuTextureGetDimension(VGPUTexture texture);
VGPU_API void vgpuTextureSetLabel(VGPUTexture texture, const char* label);
VGPU_API uint32_t vgpuTextureAddRef(VGPUTexture texture);
VGPU_API uint32_t vgpuTextureRelease(VGPUTexture texture);

/* Sampler */
VGPU_API VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDesc* desc);
VGPU_API void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler);

/* ShaderModule */
VGPU_API VGPUShaderModule vgpuCreateShaderModule(VGPUDevice device, const void* pCode, size_t codeSize);
VGPU_API void vgpuDestroyShaderModule(VGPUDevice device, VGPUShaderModule module);

/* PipelineLayout */
VGPU_API VGPUPipelineLayout vgpuCreatePipelineLayout(VGPUDevice device, const VGPUPipelineLayoutDescriptor* descriptor);
VGPU_API void vgpuDestroyPipelineLayout(VGPUDevice device, VGPUPipelineLayout pipelineLayout);

/* Pipeline */
VGPU_API VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDesc* descriptor);
VGPU_API VGPUPipeline vgpuCreateComputePipeline(VGPUDevice device, const VGPUComputePipelineDescriptor* descriptor);
VGPU_API VGPUPipeline vgpuCreateRayTracingPipeline(VGPUDevice device, const VGPURayTracingPipelineDesc* descriptor);
VGPU_API void vgpuDestroyPipeline(VGPUDevice device, VGPUPipeline pipeline);

/* SwapChain */
VGPU_API VGPUSwapChain* vgpuCreateSwapChain(VGPUDevice device, void* window, const VGPUSwapChainDesc* desc);
VGPU_API void vgpuDestroySwapChain(VGPUDevice device, VGPUSwapChain* swapChain);
VGPU_API VGPUTextureFormat vgpuGetSwapChainFormat(VGPUDevice device, VGPUSwapChain* swapChain);

/* Commands */
VGPU_API VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, VGPUCommandQueue queueType, const char* label);
VGPU_API void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel);
VGPU_API void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* debugLabel);
VGPU_API void vgpuSetPipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline pipeline);

/* Compute commands */
VGPU_API void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
VGPU_API void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset);

/* Render commands */
VGPU_API VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight);
VGPU_API void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc);
VGPU_API void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer);
VGPU_API void vgpuSetViewport(VGPUCommandBuffer commandBuffer, const VGPUViewport* viewport);
VGPU_API void vgpuSetViewports(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPUViewport* viewports);
VGPU_API void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* rect);
VGPU_API void vgpuSetVertexBuffer(VGPUCommandBuffer commandBuffer, uint32_t index, VGPUBuffer buffer, uint64_t offset);
VGPU_API void vgpuSetIndexBuffer(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset, VGPUIndexType type);
VGPU_API void vgpuSetStencilReference(VGPUCommandBuffer commandBuffer, uint32_t reference);

VGPU_API void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance);
VGPU_API void vgpuDrawIndexed(VGPUCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);

/* Helper functions */
typedef struct VGPUPixelFormatInfo
{
    VGPUTextureFormat format;
    const char* name;
    uint8_t bytesPerBlock;
    uint8_t blockWidth;
    uint8_t blockHeight;
    VGPUFormatKind kind;
} VGPUPixelFormatInfo;

typedef struct VGPUVertexFormatInfo
{
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

#endif /* VGPU_H */
