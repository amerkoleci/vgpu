//
// Copyright (c) 2019-2020 Amer Koleci.
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
#           define VGPU_API __declspec(dllexport)
#       else
#           define VGPU_API __declspec(dllimport)
#       endif
#   else  // defined(_WIN32)
#       if defined(VGPU_IMPLEMENTATION)
#           define VGPU_API __attribute__((visibility("default")))
#       else
#           define VGPU_API
#       endif
#   endif  // defined(_WIN32)
#else       // defined(VGPU_SHARED_LIBRARY)
#   define VGPU_API
#endif  // defined(VGPU_SHARED_LIBRARY)

#if defined(_WIN32)
#   define VGPU_CALL __cdecl
#else
#   define VGPU_CALL
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    typedef uint32_t vgpu_bool;

    /* Constants */
    enum {
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

    /* Enums */
    typedef enum vgpu_log_level {
        VGPU_LOG_LEVEL_ERROR = 0,
        VGPU_LOG_LEVEL_WARN = 1,
        VGPU_LOG_LEVEL_INFO = 2,
        VGPU_LOG_LEVEL_DEBUG = 3,
        _VGPU_LOG_LEVEL_COUNT,
        _VGPU_LOG_LEVEL_FORCE_U32 = 0x7FFFFFFF
    } vgpu_log_level;

    typedef enum vgpu_backend_type {
        VGPU_BACKEND_TYPE_DEFAULT,
        VGPU_BACKEND_TYPE_NULL,
        VGPU_BACKEND_TYPE_D3D11,
        VGPU_BACKEND_TYPE_D3D12,
        VGPU_BACKEND_TYPE_METAL,
        VGPU_BACKEND_TYPE_VULKAN,
        VGPU_BACKEND_TYPE_OPENGL,
        VGPU_BACKEND_TYPE_COUNT,
        _VGPU_BACKEND_TYPE_FORCE_U32 = 0x7FFFFFFF
    } vgpu_backend_type;

    typedef enum vgpu_device_preference {
        VGPU_DEVICE_PREFERENCE_HIGH_PERFORMANCE = 0,
        VGPU_DEVICE_PREFERENCE_LOW_POWER = 1,
        VGPU_DEVICE_PREFERENCE_DONT_CARE = 2,
        _VGPU_DEVICE_PREFERENCE_FORCE_U32 = 0x7FFFFFFF
    } vgpu_device_preference;

    typedef enum vgpu_present_mode {
        VGPU_PRESENT_MODE_FIFO,
        VGPU_PRESENT_MODE_IMMEDIATE,
        VGPU_PRESENT_MODE_MAILBOX,
        _VGPU_PRESENT_MODE_FORCE_U32 = 0x7FFFFFFF
    } vgpu_present_mode;

    /// Defines pixel format.
    typedef enum vgpu_texture_format {
        VGPU_TEXTURE_FORMAT_UNDEFINED = 0,
        // 8-bit pixel formats
        VGPUTextureFormat_R8UNorm,
        VGPUTextureFormat_R8SNorm,
        VGPUTextureFormat_R8UInt,
        VGPUTextureFormat_R8SInt,

        // 16-bit pixel formats
        VGPUTextureFormat_R16UNorm,
        VGPUTextureFormat_R16SNorm,
        VGPUTextureFormat_R16UInt,
        VGPUTextureFormat_R16SInt,
        VGPUTextureFormat_R16Float,
        VGPUTextureFormat_RG8UNorm,
        VGPUTextureFormat_RG8SNorm,
        VGPUTextureFormat_RG8UInt,
        VGPUTextureFormat_RG8SInt,

        // 32-bit pixel formats
        VGPUTextureFormat_R32UInt,
        VGPUTextureFormat_R32SInt,
        VGPUTextureFormat_R32Float,
        VGPUTextureFormat_RG16UInt,
        VGPUTextureFormat_RG16SInt,
        VGPUTextureFormat_RG16Float,

        VGPU_TEXTURE_FORMAT_RGBA8,
        VGPU_TEXTURE_FORMAT_RGBA8_SRGB,
        VGPUTextureFormat_RGBA8SNorm,
        VGPUTextureFormat_RGBA8UInt,
        VGPUTextureFormat_RGBA8SInt,
        VGPU_TEXTURE_FORMAT_BGRA8,
        VGPU_TEXTURE_FORMAT_BGRA8_SRGB,

        // Packed 32-Bit Pixel formats
        VGPU_TEXTURE_FORMAT_RGB10A2,
        VGPU_TEXTURE_FORMAT_RG11B10F,

        // 64-Bit Pixel Formats
        VGPUTextureFormat_RG32UInt,
        VGPUTextureFormat_RG32SInt,
        VGPUTextureFormat_RG32Float,
        VGPUTextureFormat_RGBA16UInt,
        VGPUTextureFormat_RGBA16SInt,
        VGPUTextureFormat_RGBA16Float,

        // 128-Bit Pixel Formats
        VGPUTextureFormat_RGBA32UInt,
        VGPUTextureFormat_RGBA32SInt,
        VGPUTextureFormat_RGBA32Float,

        // Depth-stencil
        VGPU_TEXTURE_FORMAT_D16,
        VGPU_TEXTURE_FORMAT_D32F,
        VGPU_TEXTURE_FORMAT_D24_PLUS,
        VGPU_TEXTURE_FORMAT_D24S8,

        // Compressed BC formats
        VGPU_TEXTURE_FORMAT_BC1,
        VGPU_TEXTURE_FORMAT_BC1_SRGB,
        VGPU_TEXTURE_FORMAT_BC2,
        VGPU_TEXTURE_FORMAT_BC2_SRGB,
        VGPU_TEXTURE_FORMAT_BC3,
        VGPU_TEXTURE_FORMAT_BC3_SRGB,
        VGPU_TEXTURE_FORMAT_BC4,
        VGPU_TEXTURE_FORMAT_BC4S,
        VGPU_TEXTURE_FORMAT_BC5,
        VGPU_TEXTURE_FORMAT_BC5S,
        VGPU_TEXTURE_FORMAT_BC6H_UFLOAT,
        VGPU_TEXTURE_FORMAT_BC6H_SFLOAT,
        VGPU_TEXTURE_FORMAT_BC7,
        VGPU_TEXTURE_FORMAT_BC7_SRGB,

        _VGPU_TEXTURE_FORMAT_Count,
        _VGPU_TEXTURE_FORMAT_FORCE_U32 = 0x7FFFFFFF
    } vgpu_texture_format;

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

    typedef enum VGPUTextureType {
        VGPU_TEXTURE_TYPE_2D = 0,
        VGPU_TEXTURE_TYPE_3D,
        VGPU_TEXTURE_TYPE_CUBE,
        _VGPU_TEXTURE_TYPE_FORCE_U32 = 0x7FFFFFFF
    } VGPUTextureType;

    typedef enum VGPUTextureUsage {
        VGPU_TEXTURE_USAGE_SAMPLED = 0x04,
        VGPU_TEXTURE_USAGE_STORAGE = 0x08,
        VGPU_TEXTURE_USAGE_RENDER_TARGET = (1 << 2),
        _VGPU_TEXTURE_USAGE_FORCE_U32 = 0x7FFFFFFF
    } VGPUTextureUsage;
    typedef uint32_t VGPUTextureUsageFlags;

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
        VGPU_LOAD_OP_CLEAR = 0,
        VGPU_LOAD_OP_LOAD = 1,
        _VGPU_LOAD_OP_FORCE_U32 = 0x7FFFFFFF
    } VGPULoadOp;

    /* Callbacks */
    typedef void(VGPU_CALL* vgpu_log_callback)(void* user_data, vgpu_log_level level, const char* message);

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

    typedef struct vgpu_features {
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
    } vgpu_features;

    typedef struct vgpu_limits {
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
    } vgpu_limits;

    typedef struct VGPUDeviceCaps {
        vgpu_backend_type backendType;
        uint32_t vendorId;
        uint32_t deviceId;
        vgpu_features features;
        vgpu_limits limits;
    } VGPUDeviceCaps;

    typedef struct VGPUBufferDescriptor {
        VGPUBufferUsageFlags usage;
        uint64_t size;
        const void* content;
        const char* label;
    } VGPUBufferDescriptor;

    typedef struct VGPUTextureDescription {
        VGPUTextureType type;
        VGPUTextureUsageFlags usage;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        vgpu_texture_format format;
        uint32_t mipLevelCount;
        uint32_t sampleCount;
        /// Pointer to external texture handle
        const void* externalHandle;
    } VGPUTextureDescription;

    typedef struct VGPURenderPassColorAttachment {
        VGPUTexture     texture;
        uint32_t        mipLevel;
        uint32_t        slice;
        VGPULoadOp      loadOp;
        VGPUColor       clearColor;
    } VGPURenderPassColorAttachment;

    typedef struct VGPURenderPassDepthStencilAttachment {
        VGPUTexture     texture;
        VGPULoadOp      depthLoadOp;
        float           clearDepth;
        VGPULoadOp      stencilLoadOp;
        uint32_t        clearStencil;
    } VGPURenderPassDepthStencilAttachment;

    typedef struct VGPURenderPassDescription {
        VGPURenderPassColorAttachment           colorAttachments[VGPU_MAX_COLOR_ATTACHMENTS];
        VGPURenderPassDepthStencilAttachment    depthStencilAttachment;
    } VGPURenderPassDescription;

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
        const uint8_t* pCode;
        const char* source;
        const char* entryPoint;
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
        const char* label;
    } VGPUSamplerDescriptor;

    typedef struct vgpu_swapchain_info {
        uint32_t width;
        uint32_t height;
        vgpu_texture_format color_format;
        vgpu_texture_format depth_stencil_format;
        vgpu_present_mode present_mode;
        vgpu_bool fullscreen;
        /// Native window handle (HWND, IUnknown, ANativeWindow, NSWindow).
        void* window_handle;
    } vgpu_swapchain_info;

    typedef struct vgpu_device_info {
        vgpu_bool debug;
        vgpu_device_preference device_preference;
        vgpu_swapchain_info swapchain;
    } vgpu_device_info;

    /* Functions */

    /* Allocation functions */
    VGPU_API void vgpu_set_allocation_callbacks(const vgpu_allocation_callbacks* callbacks);

    /* Log functions */
    VGPU_API void vgpuSetLogCallback(vgpu_log_callback callback, void* user_data);
    VGPU_API void vgpu_log(vgpu_log_level level, const char* format, ...);
    VGPU_API void vgpu_log_error(const char* format, ...);
    VGPU_API void vgpu_log_info(const char* format, ...);

    /* Device */
    VGPU_API VGPUDevice vgpuCreateDevice(vgpu_backend_type preferred_backend, const vgpu_device_info* info);
    VGPU_API void vgpuDestroyDevice(VGPUDevice device);
    VGPU_API void vgpuGetDeviceCaps(VGPUDevice device, VGPUDeviceCaps* caps);
    VGPU_API vgpu_texture_format vgpuGetDefaultDepthFormat(VGPUDevice device);
    VGPU_API vgpu_texture_format vgpuGetDefaultDepthStencilFormat(VGPUDevice device);

    VGPU_API void vgpuBeginFrame(VGPUDevice device);
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
    VGPU_API void vgpuCmdBeginRenderPass(VGPUDevice device, const VGPURenderPassDescription* desc);
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
    VGPU_API uint32_t vgpu_get_format_bits_per_pixel(vgpu_texture_format format);
    VGPU_API uint32_t vgpu_get_format_block_size(vgpu_texture_format format);

    /// Get the format compression ration along the x-axis.
    VGPU_API uint32_t vgpuGetFormatBlockWidth(vgpu_texture_format format);
    /// Get the format compression ration along the y-axis.
    VGPU_API uint32_t vgpuGetFormatBlockHeight(vgpu_texture_format format);
    /// Get the format Type.
    VGPU_API VGPUTextureFormatType vgpuGetFormatType(vgpu_texture_format format);

    /// Check if the format has a depth component.
    VGPU_API vgpu_bool vgpu_is_depth_format(vgpu_texture_format format);
    /// Check if the format has a stencil component.
    VGPU_API vgpu_bool vgpu_is_stencil_format(vgpu_texture_format format);
    /// Check if the format has depth or stencil components.
    VGPU_API vgpu_bool vgpu_is_depth_stencil_format(vgpu_texture_format format);
    /// Check if the format is a compressed format.
    VGPU_API vgpu_bool vgpuIsCompressedFormat(vgpu_texture_format format);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // VGPU_H_
