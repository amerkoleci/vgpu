// Copyright (c) Amer Koleci.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#ifndef VGPU_H_
#define VGPU_H_

#ifdef __cplusplus
#   define _VGPU_EXTERN extern "C"
#else
#   define _VGPU_EXTERN extern
#endif

#if defined(VGPU_SHARED_LIBRARY)
#   if defined(_WIN32)
#       if defined(VGPU_IMPLEMENTATION)
#           define _VGPU_API_DECL __declspec(dllexport)
#       else
#           define _VGPU_API_DECL __declspec(dllimport)
#       endif
#   else  // defined(_WIN32)
#       if defined(VGPU_IMPLEMENTATION)
#           define _VGPU_API_DECL __attribute__((visibility("default")))
#       else
#           define _VGPU_API_DECL
#       endif
#   endif  // defined(_WIN32)
#else       // defined(VGPU_SHARED_LIBRARY)
#   define _VGPU_API_DECL
#endif  // defined(VGPU_SHARED_LIBRARY)

#define VGPU_API _VGPU_EXTERN _VGPU_API_DECL

#if defined(_WIN32)
#   define VGPU_CALL __cdecl
#else
#   define VGPU_CALL
#endif

#include <stdint.h>

typedef uint32_t vgpu_bool;

/* Constants */
enum {
    VGPU_MAX_ADAPTER_NAME_SIZE = 256u,
    VGPU_NUM_INFLIGHT_FRAMES = 2u,
    VGPU_MAX_INFLIGHT_FRAMES = 3u,
    VGPU_MAX_COLOR_ATTACHMENTS = 8u,
    VGPU_MAX_VERTEX_BUFFER_BINDINGS = 8u,
    VGPU_MAX_VERTEX_ATTRIBUTES = 16u,
    VGPU_MAX_VERTEX_ATTRIBUTE_OFFSET = 2047u,
    VGPU_MAX_VERTEX_BUFFER_STRIDE = 2048u,
};

/* Handles */
typedef struct VGPUDeviceImpl* VGPUDevice;
typedef struct VGPUTextureImpl* VGPUTexture;
typedef struct VGPUBufferImpl* VGPUBuffer;
typedef struct VGPUSamplerImpl* VGPUSampler;
typedef struct VGPUShaderModuleImpl* VGPUShaderModule;
typedef struct VGPUPipelineImpl* VGPUPipeline;

/* Enums */
typedef enum VGPULogLevel {
    VGPULogLevel_Error = 0,
    VGPULogLevel_Warn = 1,
    VGPULogLevel_Info = 2,
    _VGPULogLevel_Force32 = 0x7FFFFFFF
} VGPULogLevel;

typedef enum VGPUBackendType {
    VGPU_BACKEND_TYPE_DEFAULT,
    VGPU_BACKEND_TYPE_VULKAN,
    VGPU_BACKEND_TYPE_D3D12,
    VGPU_BACKEND_TYPE_D3D11,
    _VGPU_BACKEND_TYPE_COUNT,
    _VGPU_BACKEND_TYPE_FORCE_U32 = 0x7FFFFFFF
} VGPUBackendType;

typedef enum VGPUDeviceFlags {
    VGPUDeviceFlags_None = 0x00000000,
    VGPUDeviceFlags_Debug = 0x00000001,
    VGPUDeviceFlags_GPUBasedValidation = 0x00000002,
    VGPUDeviceFlags_RenderDoc = 0x00000004,
    _VGPUDeviceFlags_Force32 = 0x7FFFFFFF
} VGPUDeviceFlags;

typedef enum VGPUAdapterType {
    VGPUAdapterType_DiscreteGPU,
    VGPUAdapterType_IntegratedGPU,
    VGPUAdapterType_CPU,
    VGPUAdapterType_Unknown,
    _VGPUAdapterType_Force32 = 0x7FFFFFFF
} VGPUAdapterType;

typedef enum VGPUPresentMode {
    VGPUPresentMode_Immediate = 0x00000000,
    VGPUPresentMode_Mailbox = 0x00000001,
    VGPUPresentMode_Fifo = 0x00000002,
    _VGPUPresentMode_Force32 = 0x7FFFFFFF
} VGPUPresentMode;

/// Defines pixel format.
typedef enum VGPUTextureFormat {
    VGPU_TEXTURE_FORMAT_UNDEFINED = 0,
    // 8-bit formats.
    VGPUTextureFormat_R8Unorm,
    VGPUTextureFormat_R8Snorm,
    VGPUTextureFormat_R8Uint,
    VGPUTextureFormat_R8Sint,
    // 16-bit formats.
    VGPUTextureFormat_R16Unorm,
    VGPUTextureFormat_R16Snorm,
    VGPUTextureFormat_R16Uint,
    VGPUTextureFormat_R16Sint,
    VGPUTextureFormat_R16Float,
    VGPUTextureFormat_RG8Unorm,
    VGPUTextureFormat_RG8Snorm,
    VGPUTextureFormat_RG8Uint,
    VGPUTextureFormat_RG8Sint,
    // 32-bit formats.
    VGPUTextureFormat_R32UInt,
    VGPUTextureFormat_R32SInt,
    VGPUTextureFormat_R32Float,
    VGPUTextureFormat_RG16UNorm,
    VGPUTextureFormat_RG16SNorm,
    VGPUTextureFormat_RG16UInt,
    VGPUTextureFormat_RG16SInt,
    VGPUTextureFormat_RG16Float,
    VGPUTextureFormat_RGBA8Unorm,
    VGPUTextureFormat_RGBA8UnormSrgb,
    VGPUTextureFormat_RGBA8Snorm,
    VGPUTextureFormat_RGBA8Uint,
    VGPUTextureFormat_RGBA8Sint,
    VGPUTextureFormat_BGRA8Unorm,
    VGPUTextureFormat_BGRA8UnormSrgb,
    // Packed 32-Bit formats.
    VGPUTextureFormat_RGB10A2Unorm,
    VGPUTextureFormat_RG11B10Float,
    VGPUTextureFormat_RGB9E5Float,
    // 64-Bit formats.
    VGPUTextureFormat_RG32Uint,
    VGPUTextureFormat_RG32Sint,
    VGPUTextureFormat_RG32Float,
    VGPUTextureFormat_RGBA16Unorm,
    VGPUTextureFormat_RGBA16Snorm,
    VGPUTextureFormat_RGBA16Uint,
    VGPUTextureFormat_RGBA16Sint,
    VGPUTextureFormat_RGBA16Float,
    // 128-Bit formats.
    VGPUTextureFormat_RGBA32Uint,
    VGPUTextureFormat_RGBA32Sint,
    VGPUTextureFormat_RGBA32Float,
    // Depth-stencil formats.
    VGPUTextureFormat_Depth16Unorm,
    VGPUTextureFormat_Depth32Float,
    VGPUTextureFormat_Depth24UnormStencil8,
    VGPUTextureFormat_Depth32FloatStencil8,
    // Compressed BC formats.
    VGPUTextureFormat_BC1RGBAUnorm,
    VGPUTextureFormat_BC1RGBAUnormSrgb,
    VGPUTextureFormat_BC2RGBAUnorm,
    VGPUTextureFormat_BC2RGBAUnormSrgb,
    VGPUTextureFormat_BC3RGBAUnorm,
    VGPUTextureFormat_BC3RGBAUnormSrgb,
    VGPUTextureFormat_BC4RUnorm,
    VGPUTextureFormat_BC4RSnorm,
    VGPUTextureFormat_BC5RGUnorm,
    VGPUTextureFormat_BC5RGSnorm,
    VGPUTextureFormat_BC6HRGBUFloat,
    VGPUTextureFormat_BC6HRGBFloat,
    VGPUTextureFormat_BC7RGBAUnorm,
    VGPUTextureFormat_BC7RGBAUnormSrgb,

    VGPUTextureFormat_Count,
    _VGPUTextureFormat_Force32 = 0x7FFFFFFF
} VGPUTextureFormat;

/// Defines pixel format type.
typedef enum VGPUTextureFormatType {
    /// Unknown format Type
    VGPUTextureFormatType_Unknown = 0,
    /// floating-point formats.
    VGPUTextureFormatType_Float,
    /// Unsigned normalized formats.
    VGPUTextureFormatType_Unorm,
    /// Unsigned normalized SRGB formats
    VGPUTextureFormatType_UnormSrgb,
    /// Signed normalized formats.
    VGPUTextureFormatType_SNorm,
    /// Unsigned integer formats.
    VGPUTextureFormatType_Uint,
    /// Signed integer formats.
    VGPUTextureFormatType_Sint,

    _VGPUTextureFormatType_Force32 = 0x7FFFFFFF
} VGPUTextureFormatType;

typedef enum VGPUTextureType {
    VGPU_TEXTURE_TYPE_2D = 0,
    VGPU_TEXTURE_TYPE_3D,
    VGPU_TEXTURE_TYPE_CUBE,
    _VGPUTextureType_Force32 = 0x7FFFFFFF
} VGPUTextureType;

typedef enum VGPUTextureUsage {
    VGPUTextureUsage_None = 0x00000000,
    VGPUTextureUsage_Sampled = 0x00000001,
    VGPUTextureUsage_Storage = 0x00000002,
    VGPUTextureUsage_RenderTarget = 0x00000004,
    _VGPUTextureUsage_Force32 = 0x7FFFFFFF
} VGPUTextureUsage;
typedef uint32_t VGPUTextureUsageFlags;

typedef enum VGPUBufferUsage {
    VGPUBufferUsage_None = 0x00000000,
    VGPUBufferUsage_Vertex = 0x00000001,
    VGPUBufferUsage_Index = 0x00000002,
    VGPUBufferUsage_Uniform = 0x00000004,
    VGPUBufferUsage_Storage = 0x00000008,
    VGPUBufferUsage_Indirect = 0x00000010,
    VGPUBufferUsage_Dynamic = 0x00000020,
    VGPUBufferUsage_Staging = 0x00000040,
    _VGPUBufferUsage_Force32 = 0x7FFFFFFF
} VGPUBufferUsage;
typedef uint32_t VGPUBufferUsageFlags;

typedef enum VGPUShaderStage {
    VGPUShaderStage_None = 0x00000000,
    VGPUShaderStage_Vertex = 0x00000001,
    VGPUShaderStage_Hull = 0x00000002,
    VGPUShaderStage_Domain = 0x00000004,
    VGPUShaderStage_Geometry = 0x00000008,
    VGPUShaderStage_Fragment = 0x00000010,
    VGPUShaderStage_Compute = 0x00000020,
    _VGPUShaderStage_Force32 = 0x7FFFFFFF
} VGPUShaderStage;
typedef uint32_t VGPUShaderStageFlags;

typedef enum VGPUVertexFormat {
    VGPUVertexFormat_Invalid = 0,
    VGPUVertexFormat_UChar2,
    VGPUVertexFormat_UChar4,
    VGPUVertexFormat_Char2,
    VGPUVertexFormat_Char4,
    VGPUVertexFormat_UChar2Norm,
    VGPUVertexFormat_UChar4Norm,
    VGPUVertexFormat_Char2Norm,
    VGPUVertexFormat_Char4Norm,
    VGPUVertexFormat_UShort2,
    VGPUVertexFormat_UShort4,
    VGPUVertexFormat_Short2,
    VGPUVertexFormat_Short4,
    VGPUVertexFormat_UShort2Norm,
    VGPUVertexFormat_UShort4Norm,
    VGPUVertexFormat_Short2Norm,
    VGPUVertexFormat_Short4Norm,
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
    VGPUVertexFormat_Force32
} VGPUVertexFormat;

typedef enum VGPUInputStepMode {
    VGPUInputStepMode_Vertex = 0,
    VGPUInputStepMode_Instance = 1,
    VGPUInputStepMode_Force32 = 0x7FFFFFFF
} VGPUInputStepMode;

typedef enum VGPUPrimitiveTopology {
    VGPUPrimitiveTopology_PointList = 0x00000000,
    VGPUPrimitiveTopology_LineList = 0x00000001,
    VGPUPrimitiveTopology_LineStrip = 0x00000002,
    VGPUPrimitiveTopology_TriangleList = 0x00000003,
    VGPUPrimitiveTopology_TriangleStrip = 0x00000004,
    _VGPUPrimitiveTopology_Force32 = 0x7FFFFFFF
} VGPUPrimitiveTopology;

typedef enum VGPUIndexType {
    VGPUIndexType_Uint16 = 0,
    VGPUIndexType_Uint32 = 1,
    _VGPUIndexType_Force32 = 0x7FFFFFFF
} VGPUIndexType;

typedef enum VGPUCompareFunction {
    VGPUCompareFunction_Undefined = 0,
    VGPUCompareFunction_Never = 1,
    VGPUCompareFunction_Less = 2,
    VGPUCompareFunction_LessEqual = 3,
    VGPUCompareFunction_Greater = 4,
    VGPUCompareFunction_GreaterEqual = 5,
    VGPUCompareFunction_Equal = 6,
    VGPUCompareFunction_NotEqual = 7,
    VGPUCompareFunction_Always = 8,
    VGPUCompareFunction_Force32 = 0x7FFFFFFF
} VGPUCompareFunction;

typedef enum VGPUFilterMode {
    VGPUFilterMode_Nearest = 0x00000000,
    VGPUFilterMode_Linear = 0x00000001,
    VGPUFilterMode_Force32 = 0x7FFFFFFF
} VGPUFilterMode;

typedef enum VGPUAddressMode {
    VGPUAddressMode_ClampToEdge = 0,
    VGPUAddressMode_Repeat = 1,
    VGPUAddressMode_MirrorRepeat = 2,
    VGPUAddressMode_ClampToBorderColor = 3,
    VGPUAddressMode_Force32 = 0x7FFFFFFF
} VGPUAddressMode;

typedef enum VGPUBorderColor {
    VGPUBorderColor_TransparentBlack = 0,
    VGPUBorderColor_OpaqueBlack = 1,
    VGPUBorderColor_OpaqueWhite = 2,
    VGPUBorderColor_Force32 = 0x7FFFFFFF
} VGPUBorderColor;

typedef enum VGPULoadOp {
    VGPULoadOp_Clear = 0,
    VGPULoadOp_Load = 1,
    VGPULoadOp_Force32 = 0x7FFFFFFF
} VGPULoadOp;

typedef enum VGPUStoreOp {
    VGPUStoreOp_Store = 0,
    VGPUStoreOp_Clear = 1,
    VGPUStoreOp_Force32 = 0x7FFFFFFF
} VGPUStoreOp;

/* Callbacks */
typedef void(VGPU_CALL* vgpu_log_callback)(void* user_data, VGPULogLevel level, const char* message);

/* Structs */
typedef struct VGPUExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} VGPUExtent3D;

typedef struct VGPUColor {
    float r;
    float g;
    float b;
    float a;
} VGPUColor;

typedef struct VGPURect {
    int32_t     x;
    int32_t     y;
    uint32_t    width;
    uint32_t    height;
} VGPURect;

typedef struct VGPUViewport {
    float       x;
    float       y;
    float       width;
    float       height;
    float       minDepth;
    float       maxDepth;
} VGPUViewport;

typedef struct vgpu_allocation_callbacks {
    void* user_data;
    void* (VGPU_CALL* allocate_memory)(void* user_data, size_t size);
    void* (VGPU_CALL* allocate_cleared_memory)(void* user_data, size_t size);
    void (VGPU_CALL* free_memory)(void* user_data, void* ptr);
} vgpu_allocation_callbacks;

typedef struct VGPUDeviceFeatures {
    vgpu_bool independentBlend;
    vgpu_bool computeShader;
    vgpu_bool tessellationShader;
    vgpu_bool multiViewport;
    vgpu_bool indexUInt32;
    vgpu_bool multiDrawIndirect;
    vgpu_bool fillModeNonSolid;
    vgpu_bool samplerAnisotropy;
    vgpu_bool textureCompressionETC2;
    vgpu_bool textureCompressionASTC_LDR;
    vgpu_bool textureCompressionBC;
    vgpu_bool textureCubeArray;
    vgpu_bool raytracing;
} VGPUDeviceFeatures;

typedef struct VGPUDeviceLimits {
    uint32_t        maxVertexAttributes;
    uint32_t        maxVertexBindings;
    uint32_t        maxVertexAttributeOffset;
    uint32_t        maxVertexBindingStride;
    uint32_t        maxTextureDimension2D;
    uint32_t        maxTextureDimension3D;
    uint32_t        maxTextureDimensionCube;
    uint32_t        maxTextureArrayLayers;
    uint32_t        maxColorAttachments;
    uint32_t        maxUniformBufferRange;
    uint32_t        maxStorageBufferRange;
    uint64_t        minUniformBufferOffsetAlignment;
    uint64_t        minStorageBufferOffsetAlignment;
    uint32_t        maxSamplerAnisotropy;
    uint32_t        maxViewports;
    uint32_t        maxViewportWidth;
    uint32_t        maxViewportHeight;
    uint32_t        maxTessellationPatchSize;
    uint32_t        maxComputeSharedMemorySize;
    uint32_t        maxComputeWorkGroupCountX;
    uint32_t        maxComputeWorkGroupCountY;
    uint32_t        maxComputeWorkGroupCountZ;
    uint32_t        maxComputeWorkGroupInvocations;
    uint32_t        maxComputeWorkGroupSizeX;
    uint32_t        maxComputeWorkGroupSizeY;
    uint32_t        maxComputeWorkGroupSizeZ;
} VGPUDeviceLimits;

typedef struct VGPUDeviceCaps {
    VGPUBackendType     backendType;
    uint32_t            vendorId;
    uint32_t            adapterId;
    VGPUAdapterType     adapterType;
    char                adapterName[VGPU_MAX_ADAPTER_NAME_SIZE];
    VGPUDeviceFeatures  features;
    VGPUDeviceLimits    limits;
} VGPUDeviceCaps;

typedef struct VGPUBufferDescriptor {
    VGPUBufferUsageFlags usage;
    uint64_t size;
    const void* content;
    const char* label;
} VGPUBufferDescriptor;

typedef struct VGPUTextureDescription {
    VGPUTextureType         type;
    VGPUTextureUsageFlags   usage;
    uint32_t                width;
    uint32_t                height;
    uint32_t                depth;
    VGPUTextureFormat       format;
    uint32_t                mipLevelCount;
    uint32_t                sampleCount;
    /// Pointer to external texture handle
    const void*             externalHandle;
} VGPUTextureDescription;

typedef struct VGPURenderPassColorAttachment {
    VGPUTexture     texture;
    uint32_t        mipLevel;
    uint32_t        slice;
    VGPULoadOp      loadOp;
    VGPUStoreOp     storeOp;
    VGPUColor       clearColor;
} VGPURenderPassColorAttachment;

typedef struct VGPURenderPassDepthStencilAttachment {
    VGPUTexture     texture;
    VGPULoadOp      depthLoadOp;
    VGPUStoreOp     depthStoreOp;
    float           clearDepth;
    VGPULoadOp      stencilLoadOp;
    VGPUStoreOp     stencilStoreOp;
    uint32_t        clearStencil;
} VGPURenderPassDepthStencilAttachment;

typedef struct VGPURenderPassDescriptor {
    VGPURenderPassColorAttachment           colorAttachments[VGPU_MAX_COLOR_ATTACHMENTS];
    VGPURenderPassDepthStencilAttachment    depthStencilAttachment;
} VGPURenderPassDescriptor;

typedef struct VgpuVertexBufferLayoutDescriptor {
    uint32_t                stride;
    VGPUInputStepMode       stepMode;
} VgpuVertexBufferLayoutDescriptor;

typedef struct VgpuVertexAttributeDescriptor {
    VGPUVertexFormat            format;
    uint32_t                    offset;
    uint32_t                    bufferIndex;
} VgpuVertexAttributeDescriptor;

typedef struct VgpuVertexDescriptor {
    VgpuVertexBufferLayoutDescriptor    layouts[VGPU_MAX_VERTEX_BUFFER_BINDINGS];
    VgpuVertexAttributeDescriptor       attributes[VGPU_MAX_VERTEX_ATTRIBUTES];
} VgpuVertexDescriptor;

typedef struct VGPUShaderModuleDescriptor {
    VGPUShaderStage stage;
    const void*     code;
    size_t          size;
    const char*     entry;
    const char*     label;
} VGPUShaderModuleDescriptor;

typedef struct VGPURenderPipelineDescriptor {
    //VgpuShader                  shader;
    VgpuVertexDescriptor        vertexDescriptor;
    VGPUPrimitiveTopology       primitiveTopology;
} VGPURenderPipelineDescriptor;

typedef struct VgpuComputePipelineDescriptor {
    uint32_t dummy;
    //VgpuShader                  shader;
} VgpuComputePipelineDescriptor;

typedef struct VGPUSamplerDescriptor {
    VGPUAddressMode         addressModeU;
    VGPUAddressMode         addressModeV;
    VGPUAddressMode         addressModeW;
    VGPUFilterMode          magFilter;
    VGPUFilterMode          minFilter;
    VGPUFilterMode          mipmapFilter;
    float                   lodMinClamp;
    float                   lodMaxClamp;
    VGPUCompareFunction     compare;
    uint32_t                maxAnisotropy;
    VGPUBorderColor         border_color;
    const char* label;
} VGPUSamplerDescriptor;

typedef struct vgpu_swapchain_info {
    /// Native window handle (HWND, IUnknown, ANativeWindow, NSWindow).
    void* window_handle;
    VGPUTextureFormat colorFormat;
    uint32_t width;
    uint32_t height;
    VGPUPresentMode presentMode;
    vgpu_bool fullscreen;
    const char* label;
} vgpu_swapchain_info;

typedef struct VGPUDeviceDescriptor {
    VGPUBackendType     preferredBackend;
    VGPUDeviceFlags     flags;
    VGPUAdapterType     adapterPreference;
    vgpu_swapchain_info swapchain;
} VGPUDeviceDescriptor;

/* Functions */

/* Allocation functions */
VGPU_API void vgpuSetAllocationCallbacks(const vgpu_allocation_callbacks* callbacks);

/* Log functions */
VGPU_API void vgpuSetLogCallback(vgpu_log_callback callback, void* user_data);
VGPU_API void vgpuLog(VGPULogLevel level, const char* format, ...);
VGPU_API void vgpuLogError(const char* format, ...);
VGPU_API void vgpuLogInfo(const char* format, ...);

/* Device */
VGPU_API VGPUDevice vgpuCreateDevice(const VGPUDeviceDescriptor* descriptor);
VGPU_API void vgpuDestroyDevice(VGPUDevice device);
VGPU_API void vgpuGetDeviceCaps(VGPUDevice device, VGPUDeviceCaps* caps);
VGPU_API VGPUTextureFormat vgpuGetDefaultDepthFormat(VGPUDevice device);
VGPU_API VGPUTextureFormat vgpuGetDefaultDepthStencilFormat(VGPUDevice device);

VGPU_API vgpu_bool vgpuBeginFrame(VGPUDevice device);
VGPU_API void vgpuEndFrame(VGPUDevice device);
VGPU_API VGPUTexture vgpuGetBackbufferTexture(VGPUDevice device);

/* Texture */
VGPU_API VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDescription* desc);
VGPU_API void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture);

/* Buffer */
VGPU_API VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDescriptor* descriptor);
VGPU_API void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer);

/* Sampler */
VGPU_API VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDescriptor* descriptor);
VGPU_API void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler);

/* ShaderModule */
VGPU_API VGPUShaderModule vgpuCreateShaderModule(VGPUDevice device, const VGPUShaderModuleDescriptor* descriptor);
VGPU_API void vgpuDestroyShaderModule(VGPUDevice device, VGPUShaderModule shader);

/* Pipeline */
VGPU_API VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDescriptor* descriptor);
//VGPU_API VgpuPipeline vgpuCreateComputePipeline(const VgpuComputePipelineDescriptor* descriptor);
VGPU_API void vgpuDestroyPipeline(VGPUDevice device, VGPUPipeline pipeline);

/* Commands */
VGPU_API void vgpuCmdBeginRenderPass(VGPUDevice device, const VGPURenderPassDescriptor* descriptor);
VGPU_API void vgpuCmdEndRenderPass(VGPUDevice device);
/*VGPU_EXPORT void vgpuCmdSetShader(VgpuCommandBuffer commandBuffer, VgpuShader shader);
VGPU_EXPORT void vgpuCmdSetVertexBuffer(VgpuCommandBuffer commandBuffer, uint32_t binding, VgpuBuffer buffer, uint64_t offset, VgpuVertexInputRate inputRate);
VGPU_EXPORT void vgpuCmdSetIndexBuffer(VgpuCommandBuffer commandBuffer, VgpuBuffer buffer, uint64_t offset, VgpuIndexType indexType);

VGPU_EXPORT void vgpuCmdSetViewport(VgpuCommandBuffer commandBuffer, VgpuViewport viewport);
VGPU_EXPORT void vgpuCmdSetViewports(VgpuCommandBuffer commandBuffer, uint32_t viewportCount, const VgpuViewport* pViewports);
VGPU_EXPORT void vgpuCmdSetScissor(VgpuCommandBuffer commandBuffer, VgpuRect2D scissor);
VGPU_EXPORT void vgpuCmdSetScissors(VgpuCommandBuffer commandBuffer, uint32_t scissorCount, const VgpuRect2D* pScissors);

VGPU_EXPORT void vgpuCmdSetPrimitiveTopology(VgpuCommandBuffer commandBuffer, VgpuPrimitiveTopology topology);
VGPU_EXPORT void vgpuCmdDraw(VgpuCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t firstVertex);
VGPU_EXPORT void vgpuCmdDrawIndexed(VgpuCommandBuffer commandBuffer, uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset);
*/

/* Helper methods */
/// Get the number of bits per format
VGPU_API uint32_t vgpuGetFormatBitsPerPixel(VGPUTextureFormat format);
VGPU_API uint32_t vgpuGetFormatBlockSize(VGPUTextureFormat format);

/// Get the format compression ration along the x-axis.
VGPU_API uint32_t vgpuGetFormatBlockWidth(VGPUTextureFormat format);
/// Get the format compression ration along the y-axis.
VGPU_API uint32_t vgpuGetFormatBlockHeight(VGPUTextureFormat format);
/// Get the format Type.
VGPU_API VGPUTextureFormatType vgpuGetFormatType(VGPUTextureFormat format);

/// Check if the format has a depth component.
VGPU_API vgpu_bool vgpuIsDepthFormat(VGPUTextureFormat format);
/// Check if the format has a stencil component.
VGPU_API vgpu_bool vgpuIsStencilFormat(VGPUTextureFormat format);
/// Check if the format has depth or stencil components.
VGPU_API vgpu_bool vgpuIsDepthStencilFormat(VGPUTextureFormat format);
/// Check if the format is a compressed format.
VGPU_API vgpu_bool vgpuIsCompressedFormat(VGPUTextureFormat format);
/// Check if a format represents sRGB color space.
VGPU_API vgpu_bool vgpuIsSrgbFormat(VGPUTextureFormat format);
/// Convert a SRGB format to linear. If the format is already linear no conversion will be made.
VGPU_API VGPUTextureFormat vgpuSRGBToLinearFormat(VGPUTextureFormat format);

#endif // VGPU_H_
