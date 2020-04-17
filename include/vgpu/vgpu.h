//
// Copyright (c) 2019 Amer Koleci.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef VGPU_H_
#define VGPU_H_

#if defined(VGPU_SHARED_LIBRARY)
#   if defined(_WIN32)
#       if defined(VGPU_IMPLEMENTATION)
#           define VGPU_EXPORT __declspec(dllexport)
#       else
#           define VGPU_EXPORT __declspec(dllimport)
#       endif
#   else  // defined(_WIN32)
#       if defined(VGPU_IMPLEMENTATION)
#           define VGPU_EXPORT __attribute__((visibility("default")))
#       else
#           define VGPU_EXPORT
#       endif
#   endif  // defined(_WIN32)
#else       // defined(VGPU_SHARED_LIBRARY)
#   define VGPU_EXPORT
#endif  // defined(VGPU_SHARED_LIBRARY)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct VGPUBufferImpl* VGPUBuffer;
typedef struct VGPUTextureImpl* VGPUTexture;
//typedef struct VGPUTextureViewImpl* VGPUTextureView;
typedef struct vgpu_sampler_t* vgpu_sampler;
typedef struct VGPURenderPassImpl* VGPURenderPass;

enum {
    VGPU_MAX_COLOR_ATTACHMENTS = 8u,
    VGPU_MAX_VERTEX_BUFFER_BINDINGS = 8u,
    VGPU_MAX_VERTEX_ATTRIBUTES = 16u,
    VGPU_MAX_VERTEX_ATTRIBUTE_OFFSET = 2047u,
    VGPU_MAX_VERTEX_BUFFER_STRIDE = 2048u,
};

typedef enum vgpu_log_level {
    VGPU_LOG_LEVEL_OFF = 0,
    VGPU_LOG_LEVEL_VERBOSE,
    VGPU_LOG_LEVEL_DEBUG,
    VGPU_LOG_LEVEL_INFO,
    VGPU_LOG_LEVEL_WARN,
    VGPU_LOG_LEVEL_ERROR,
    VGPU_LOG_LEVEL_CRITICAL,
    VGPU_LOG_LEVEL_COUNT,
    _VGPU_LOG_LEVEL_FORCE_U32 = 0x7FFFFFFF
} vgpu_log_level;

typedef enum VGPUBackendType {
    VGPUBackendType_Null = 0,
    VGPUBackendType_D3D11 = 1,
    VGPUBackendType_D3D12 = 2,
    VGPUBackendType_Vulkan = 3,
    VGPUBackendType_OpenGL = 4,
    VGPUBackendType_OpenGLES = 5,
    VGPUBackendType_Force32 = 0x7FFFFFFF
} VGPUBackendType;

typedef enum vgpu_config_flags_bits {
    VGPU_CONFIG_FLAGS_NONE = 0,
    VGPU_CONFIG_FLAGS_VALIDATION = 0x1,
    _VGPU_CONFIG_FLAGS_FORCE_U32 = 0x7FFFFFFF
} vgpu_config_flags_bits;
typedef uint32_t vgpu_config_flags;

typedef enum VGPUAdapterType {
    VGPUAdapterType_DiscreteGPU = 0x00000000,
    VGPUAdapterType_IntegratedGPU = 0x00000001,
    VGPUAdapterType_CPU = 0x00000002,
    VGPUAdapterType_Unknown = 0x00000003,
    VGPUAdapterType_Force32 = 0x7FFFFFFF
} VGPUAdapterType;

typedef enum VGPUPresentMode {
    VGPUPresentMode_Fifo = 0,
    VGPUPresentMode_Mailbox = 1,
    VGPUPresentMode_Immediate = 2,
    VGPUPresentMode_Force32 = 0x7FFFFFFF
} VGPUPresentMode;

/// Defines pixel format.
typedef enum VGPUTextureFormat {
    VGPUTextureFormat_Undefined = 0,
    // 8-bit pixel formats
    VGPUTextureFormat_R8Unorm,
    VGPUTextureFormat_R8Snorm,
    VGPUTextureFormat_R8Uint,
    VGPUTextureFormat_R8Sint,

    // 16-bit pixel formats
    VGPUTextureFormat_R16Unorm,
    VGPUTextureFormat_R16Snorm,
    VGPUTextureFormat_R16Uint,
    VGPUTextureFormat_R16Sint,
    VGPUTextureFormat_R16Float,
    VGPUTextureFormat_RG8Unorm,
    VGPUTextureFormat_RG8Snorm,
    VGPUTextureFormat_RG8Uint,
    VGPUTextureFormat_RG8Sint,

    // 32-bit pixel formats
    VGPUTextureFormat_R32Uint,
    VGPUTextureFormat_R32Sint,
    VGPUTextureFormat_R32Float,
    VGPUTextureFormat_RG16Uint,
    VGPUTextureFormat_RG16Sint,
    VGPUTextureFormat_RG16Float,

    VGPUTextureFormat_RGBA8Unorm,
    VGPUTextureFormat_RGBA8UnormSrgb,
    VGPUTextureFormat_RGBA8Snorm,
    VGPUTextureFormat_RGBA8Uint,
    VGPUTextureFormat_RGBA8Sint,

    VGPUTextureFormat_BGRA8Unorm,
    VGPUTextureFormat_BGRA8UnormSrgb,

    // Packed 32-Bit Pixel formats
    VGPUTextureFormat_RGB10A2Unorm,
    VGPUTextureFormat_RG11B10Float,

    // 64-Bit Pixel Formats
    VGPUTextureFormat_RG32Uint,
    VGPUTextureFormat_RG32Sint,
    VGPUTextureFormat_RG32Float,
    VGPUTextureFormat_RGBA16Uint,
    VGPUTextureFormat_RGBA16Sint,
    VGPUTextureFormat_RGBA16Float,

    // 128-Bit Pixel Formats
    VGPUTextureFormat_RGBA32Uint,
    VGPUTextureFormat_RGBA32Sint,
    VGPUTextureFormat_RGBA32Float,

    // Depth-stencil
    VGPUTextureFormat_Depth16Unorm,
    VGPUTextureFormat_Depth32Float,
    VGPUTextureFormat_Depth24Plus,
    VGPUTextureFormat_Depth24PlusStencil8,

    // Compressed formats
    VGPUTextureFormat_BC1RGBAUnorm,
    VGPUTextureFormat_BC1RGBAUnormSrgb,
    VGPUTextureFormat_BC2RGBAUnorm ,
    VGPUTextureFormat_BC2RGBAUnormSrgb,
    VGPUTextureFormat_BC3RGBAUnorm,
    VGPUTextureFormat_BC3RGBAUnormSrgb,
    VGPUTextureFormat_BC4RUnorm,
    VGPUTextureFormat_BC4RSnorm,
    VGPUTextureFormat_BC5RGUnorm,
    VGPUTextureFormat_BC5RGSnorm,
    VGPUTextureFormat_BC6HRGBUfloat,
    VGPUTextureFormat_BC6HRGBSfloat,
    VGPUTextureFormat_BC7RGBAUnorm,
    VGPUTextureFormat_BC7RGBAUnormSrgb,

    VGPUTextureFormat_Count,
    VGPUTextureFormat_Force32 = 0x7FFFFFFF
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
    VGPUTextureFormatType_Snorm,
    /// Unsigned integer formats.
    VGPUTextureFormatType_Uint,
    /// Signed integer formats.
    VGPUTextureFormatType_Sint,

    VGPUTextureFormatType_Force32 = 0x7FFFFFFF
} VGPUTextureFormatType;

typedef enum vgpu_texture_type {
    VGPU_TEXTURE_TYPE_2D = 0,
    VGPU_TEXTURE_TYPE_3D,
    VGPU_TEXTURE_TYPE_CUBE,
    _VGPU_TEXTURE_TYPE_COUNT,
    _VGPU_TEXTURE_TYPE_FORCE_U32 = 0x7FFFFFFF
} vgpu_texture_type;

typedef enum vgpu_texture_usage {
    VGPU_TEXTURE_USAGE_NONE = 0,
    VGPU_TEXTURE_USAGE_SAMPLED = 0x04,
    VGPU_TEXTURE_USAGE_STORAGE = 0x08,
    VGPU_TEXTURE_USAGE_RENDERTARGET = 0x10,
    _VGPU_TEXTURE_USAGE_FORCE_U32 = 0x7FFFFFFF
} vgpu_texture_usage;
typedef uint32_t vgpu_texture_usage_flags;

typedef enum VGPUBufferUsage {
    VGPU_BUFFER_USAGE_NONE = 0,
    VGPU_BUFFER_USAGE_VERTEX = 1,
    VGPU_BUFFER_USAGE_INDEX = 2,
    VGPU_BUFFER_USAGE_UNIFORM = 4,
    VGPU_BUFFER_USAGE_STORAGE = 8,
    VGPU_BUFFER_USAGE_INDIRECT = 16,
    VGPU_BUFFER_USAGE_DYNAMIC = 32,
    VGPU_BUFFER_USAGE_CPU_ACCESSIBLE = 64,
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
    VGPUShaderStage_Force32 = 0x7FFFFFFF
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
    VGPUPrimitiveTopology_Force32 = 0x7FFFFFFF
} VGPUPrimitiveTopology;

typedef enum VGPUIndexType {
    VGPUIndexType_Uint16 = 0,
    VGPUIndexType_Uint32 = 1,
    VGPUIndexType_Force32 = 0x7FFFFFFF
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
    VGPULoadOp_DontCare = 0,
    VGPULoadOp_Load = 1,
    VGPULoadOp_Clear = 2,
    VGPULoadOp_Force32 = 0x7FFFFFFF
} VGPULoadOp;

typedef enum VGPUTextureLayout {
    VGPUTextureLayout_Undefined = 0,
    VGPUTextureLayout_General,
    VGPUTextureLayout_RenderTarget,
    VGPUTextureLayout_ShaderRead,
    VGPUTextureLayout_ShaderWrite,
    VGPUTextureLayout_Present,
    VGPUTextureLayout_Force32 = 0x7FFFFFFF
} VGPUTextureLayout;

/* Callbacks */
typedef void(*vgpu_log_callback)(void* user_data, vgpu_log_level level, const char* message);

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

typedef struct vgpu_rect {
    int32_t     x;
    int32_t     y;
    int32_t    width;
    int32_t    height;
} vgpu_rect;

typedef struct VgpuViewport {
    float       x;
    float       y;
    float       width;
    float       height;
    float       minDepth;
    float       maxDepth;
} VgpuViewport;

typedef struct VGPUFeatures {
    bool independentBlend;
    bool computeShader;
    bool geometryShader;
    bool tessellationShader;
    bool multiViewport;
    bool indexUint32;
    bool multiDrawIndirect;
    bool fillModeNonSolid;
    bool samplerAnisotropy;
    bool textureCompressionETC2;
    bool textureCompressionASTC_LDR;
    bool textureCompressionBC;
    bool textureCubeArray;
    bool raytracing;
} VGPUFeatures;

typedef struct VGPULimits {
    uint32_t        max_vertex_attributes;
    uint32_t        max_vertex_bindings;
    uint32_t        max_vertex_attribute_offset;
    uint32_t        max_vertex_binding_stride;
    uint32_t        max_texture_size_1d;
    uint32_t        max_texture_size_2d;
    uint32_t        max_texture_size_3d;
    uint32_t        max_texture_size_cube;
    uint32_t        max_texture_array_layers;
    uint32_t        max_color_attachments;
    uint32_t        max_uniform_buffer_size;
    uint64_t        min_uniform_buffer_offset_alignment;
    uint32_t        max_storage_buffer_size;
    uint64_t        min_storage_buffer_offset_alignment;
    uint32_t        max_sampler_anisotropy;
    uint32_t        max_viewports;
    uint32_t        max_viewport_width;
    uint32_t        max_viewport_height;
    uint32_t        max_tessellation_patch_size;
    float           point_size_range_min;
    float           point_size_range_max;
    float           line_width_range_min;
    float           line_width_range_max;
    uint32_t        max_compute_shared_memory_size;
    uint32_t        max_compute_work_group_count_x;
    uint32_t        max_compute_work_group_count_y;
    uint32_t        max_compute_work_group_count_z;
    uint32_t        max_compute_work_group_invocations;
    uint32_t        max_compute_work_group_size_x;
    uint32_t        max_compute_work_group_size_y;
    uint32_t        max_compute_work_group_size_z;
} VGPULimits;

typedef struct VGPUBufferDescriptor {
    VGPUBufferUsageFlags usage;
    uint64_t size;
    const void* content;
    const char* label;
} VGPUBufferDescriptor;

typedef struct vgpu_texture_desc {
    vgpu_texture_type type;
    vgpu_texture_usage_flags usage;
    uint32_t width;
    uint32_t height;
    union {
        uint32_t depth;
        uint32_t layers;
    };
    VGPUTextureFormat format;
    uint32_t mip_levels;
    uint32_t sample_count;
    /// Initial content to initialize with.
    const void* content;
    /// Pointer to external texture handle
    const void* external_handle;
    const char* label;
} vgpu_texture_desc;

typedef struct VGPUColorAttachment {
    VGPUTexture     texture;
    uint32_t        mipLevel;
    uint32_t        slice;
    VGPULoadOp      loadOp;
    VGPUColor       clearColor;
} VGPUColorAttachment;

typedef struct VGPUDepthStencilAttachment {
    VGPUTexture               texture;
    float clearDepth;
    uint32_t clearStencil;
} VGPUDepthStencilAttachment;

typedef struct VGPURenderPassDescriptor {
    VGPUColorAttachment         colorAttachments[VGPU_MAX_COLOR_ATTACHMENTS];
    VGPUDepthStencilAttachment  depthStencilAttachment;
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
    uint64_t        codeSize;
    const uint8_t*  pCode;
    const char*     source;
    const char*     entryPoint;
} VGPUShaderModuleDescriptor;

typedef struct VgpuShaderStageDescriptor {
    //VgpuShaderModule            shaderModule;
    const char* entryPoint;
    /*const VgpuSpecializationInfo* specializationInfo;*/
} VgpuShaderStageDescriptor;

typedef struct VgpuShaderDescriptor {
    uint32_t                            stageCount;
    const VgpuShaderStageDescriptor* stages;
} VgpuShaderDescriptor;

typedef struct VgpuRenderPipelineDescriptor {
    //VgpuShader                  shader;
    VgpuVertexDescriptor        vertexDescriptor;
    VGPUPrimitiveTopology       primitiveTopology;
} VgpuRenderPipelineDescriptor;

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
    const char*             label;
} VGPUSamplerDescriptor;

typedef struct VGPUSwapChainDescriptor {
    /// Native window handle (HWND, IUnknown, ANativeWindow, NSWindow).
    void*               nativeHandle;

    uint32_t            width;
    uint32_t            height;
    bool                fullscreen;
    VGPUTextureFormat   colorFormat;
    VGPUColor           colorClearValue;
    VGPUTextureFormat   depthStencilFormat;
    VGPUPresentMode     presentMode;
} VGPUSwapChainDescriptor;

typedef struct VGpuDeviceDescriptor {
    VGPUBackendType                 preferredBackend;
    vgpu_config_flags               flags;
    const VGPUSwapChainDescriptor*  swapchain;
} VGpuDeviceDescriptor;

#ifdef __cplusplus
extern "C"
{
#endif

    /* Log functions */
    /// Get the current log output function.
    VGPU_EXPORT void vgpu_get_log_callback_function(vgpu_log_callback* callback, void** user_data);
    /// Set the current log output function.
    VGPU_EXPORT void vgpu_set_log_callback_function(vgpu_log_callback callback, void* user_data);

    VGPU_EXPORT void vgpu_log(vgpu_log_level level, const char* message);
    VGPU_EXPORT void vgpu_log_format(vgpu_log_level level, const char* format, ...);
    VGPU_EXPORT void vgpu_log_error(const char* message);
    VGPU_EXPORT void vgpu_log_error_format(const char* format, ...);

    VGPU_EXPORT VGPUBackendType vgpuGetDefaultPlatformBackend(void);
    VGPU_EXPORT bool vgpuIsBackendSupported(VGPUBackendType backend);
    VGPU_EXPORT bool vgpuInit(const VGpuDeviceDescriptor* desc);
    VGPU_EXPORT void vgpuShutdown(void);
    VGPU_EXPORT VGPUBackendType vgpuGetBackend(void);
    VGPU_EXPORT VGPUFeatures vgpuGetFeatures(void);
    VGPU_EXPORT VGPULimits vgpuGetLimits(void);
    VGPU_EXPORT VGPURenderPass vgpuGetDefaultRenderPass(void);

    VGPU_EXPORT void vgpuBeginFrame(void);
    VGPU_EXPORT void vgpuEndFrame(void);
    //VGPU_EXPORT VgpuPixelFormat vgpuGetDefaultDepthFormat(void);
    //VGPU_EXPORT VgpuPixelFormat vgpuGetDefaultDepthStencilFormat(void);
   
    /* Buffer */
    VGPU_EXPORT VGPUBuffer vgpuCreateBuffer(const VGPUBufferDescriptor* desc);
    VGPU_EXPORT void vgpuDestroyBuffer(VGPUBuffer buffer);

    /* Texture */
    VGPU_EXPORT VGPUTexture vgpu_create_texture(const vgpu_texture_desc* desc);
    VGPU_EXPORT void vgpu_destroy_texture(VGPUTexture texture);
    VGPU_EXPORT vgpu_texture_desc vgpu_query_texture_desc(VGPUTexture texture);
    VGPU_EXPORT uint32_t vgpu_get_texture_width(VGPUTexture texture, uint32_t mip_level);
    VGPU_EXPORT uint32_t vgpu_get_texture_height(VGPUTexture texture, uint32_t mip_level);

    /* Sampler */
    VGPU_EXPORT vgpu_sampler vgpu_create_sampler(const VGPUSamplerDescriptor* desc);
    VGPU_EXPORT void vgpu_destroy_sampler(vgpu_sampler sampler);

    /* RenderPass */
    VGPU_EXPORT VGPURenderPass vgpuCreateRenderPass(const VGPURenderPassDescriptor* descriptor);
    //VGPU_EXPORT VGPURenderPass vgpuRenderPassCreate(const VGPUSwapChainDescriptor* descriptor);
    VGPU_EXPORT void vgpuDestroyRenderPass(VGPURenderPass renderPass);
    VGPU_EXPORT void vgpuRenderPassGetExtent(VGPURenderPass renderPass, uint32_t* width, uint32_t* height);
    VGPU_EXPORT void vgpuRenderPassSetClearColors(VGPURenderPass renderPass, uint32_t firstAttachment, uint32_t count, const VGPUColor* pColor);

    /* ShaderModule */
    //VGPU_EXPORT VgpuShaderModule vgpuCreateShaderModule(const VgpuShaderModuleDescriptor* descriptor);
    //VGPU_EXPORT void vgpuDestroyShaderModule(VgpuShaderModule shaderModule);

    /* Shader */
    //VGPU_EXPORT VgpuShader vgpuCreateShader(const VgpuShaderDescriptor* descriptor);
    //VGPU_EXPORT void vgpuDestroyShader(VgpuShader shader);

    /* Pipeline */
    //VGPU_EXPORT VgpuPipeline vgpuCreateRenderPipeline(const VgpuRenderPipelineDescriptor* descriptor);
    //VGPU_EXPORT VgpuPipeline vgpuCreateComputePipeline(const VgpuComputePipelineDescriptor* descriptor);
    //VGPU_EXPORT void vgpuDestroyPipeline(VgpuPipeline pipeline);

    /* Commands */
    VGPU_EXPORT void vgpuCmdBeginRenderPass(VGPURenderPass renderPass);
    VGPU_EXPORT void vgpuCmdEndRenderPass(void);
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
    VGPU_EXPORT uint32_t vgpu_get_format_bits_per_pixel(VGPUTextureFormat format);
    VGPU_EXPORT uint32_t vgpu_get_format_block_size(VGPUTextureFormat format);

    /// Get the format compression ration along the x-axis.
    VGPU_EXPORT uint32_t vgpuGetFormatBlockWidth(VGPUTextureFormat format);
    /// Get the format compression ration along the y-axis.
    VGPU_EXPORT uint32_t vgpuGetFormatBlockHeight(VGPUTextureFormat format);
    /// Get the format Type.
    VGPU_EXPORT VGPUTextureFormatType vgpuGetFormatType(VGPUTextureFormat format);

    /// Check if the format has a depth component.
    VGPU_EXPORT bool vgpu_is_depth_format(VGPUTextureFormat format);
    /// Check if the format has a stencil component.
    VGPU_EXPORT bool vgpu_is_stencil_format(VGPUTextureFormat format);
    /// Check if the format has depth or stencil components.
    VGPU_EXPORT bool vgpu_is_depth_stencil_format(VGPUTextureFormat format);
    /// Check if the format is a compressed format.
    VGPU_EXPORT bool vgpu_is_compressed_format(VGPUTextureFormat format);
    /// Get format string name.
    VGPU_EXPORT const char* vgpu_get_format_name(VGPUTextureFormat format);

#ifdef __cplusplus
}
#endif

#endif // VGPU_H_
