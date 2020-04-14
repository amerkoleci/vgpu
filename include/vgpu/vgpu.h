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

typedef struct vgpu_texture vgpu_texture;
typedef struct vgpu_sampler vgpu_sampler;
typedef struct vgpu_render_pass vgpu_render_pass;

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

typedef enum vgpu_backend {
    VGPU_BACKEND_DEFAULT = 0,
    VGPU_BACKEND_NULL,
    VGPU_BACKEND_VULKAN,
    VGPU_BACKEND_DIRECT3D12,
    VGPU_BACKEND_DIRECT3D11,
    VGPU_BACKEND_OPENGL,
    VGPU_BACKEND_COUNT,
    _VGPU_BACKEND_FORCE_U32 = 0x7FFFFFFF
} vgpu_backend;

typedef enum vgpu_config_flags_bits {
    VGPU_CONFIG_FLAGS_NONE = 0,
    VGPU_CONFIG_FLAGS_VALIDATION = 0x1,
    _VGPU_CONFIG_FLAGS_FORCE_U32 = 0x7FFFFFFF
} vgpu_config_flags_bits;
typedef uint32_t vgpu_config_flags;

typedef enum vgpu_power_preference {
    /// Prefer high performance GPU.
    VGPU_POWER_PREFERENCE_HIGH_PERFORMANCE,
    /// Prefer lower power GPU.
    VGPU_POWER_PREFERENCE_LOW_POWER,
    /// No GPU preference.
    VGPU_POWER_PREFERENCE_DONT_CARE,
    _VGPU_POWER_PREFERENCE_FORCE_U32 = 0x7FFFFFFF
} vgpu_power_preference;

typedef enum vgpu_sample_count {
    /// 1 sample (no multi-sampling).
    VGPU_SAMPLE_COUNT1 = 1,
    /// 2 Samples.
    VGPU_SAMPLE_COUNT2 = 2,
    /// 4 Samples.
    VGPU_SAMPLE_COUNT4 = 4,
    /// 8 Samples.
    VGPU_SAMPLE_COUNT8 = 8,
    /// 16 Samples.
    VGPU_SAMPLE_COUNT16 = 16,
    /// 32 Samples.
    VGPU_SAMPLE_COUNT32 = 32,
} vgpu_sample_count;

/// Defines pixel format.
typedef enum vgpu_pixel_format {
    VGPU_PIXEL_FORMAT_UNDEFINED = 0,
    // 8-bit pixel formats
    VGPU_PIXEL_FORMAT_A8_UNORM,
    VGPU_PIXEL_FORMAT_R8_UNORM,
    VGPU_PIXEL_FORMAT_R8_SNORM,
    VGPU_PIXEL_FORMAT_R8_UINT,
    VGPU_PIXEL_FORMAT_R8_SINT,

    // 16-bit pixel formats
    VGPU_PIXEL_FORMAT_R16_UNORM,
    VGPU_PIXEL_FORMAT_R16_SNORM,
    VGPU_PIXEL_FORMAT_R16_UINT,
    VGPU_PIXEL_FORMAT_R16_SINT,
    VGPU_PIXEL_FORMAT_R16_FLOAT,
    VGPU_PIXEL_FORMAT_RG8_UNORM,
    VGPU_PIXEL_FORMAT_RG8_SNORM,
    VGPU_PIXEL_FORMAT_RG8_UINT,
    VGPU_PIXEL_FORMAT_RG8_SINT,

    // Packed 16-bit pixel formats
    VGPU_PIXEL_FORMAT_R5G6B5_UNORM,
    VGPU_PIXEL_FORMAT_RGBA4_UNORM,

    // 32-bit pixel formats
    VGPU_PIXEL_FORMAT_R32_UINT,
    VGPU_PIXEL_FORMAT_R32_SINT,
    VGPU_PIXEL_FORMAT_R32_FLOAT,
    VGPU_PIXEL_FORMAT_RG16_UNORM,
    VGPU_PIXEL_FORMAT_RG16_SNORM,
    VGPU_PIXEL_FORMAT_RG16_UINT,
    VGPU_PIXEL_FORMAT_RG16_SINT,
    VGPU_PIXEL_FORMAT_RG16_FLOAT,
    VGPU_PIXEL_FORMAT_RGBA8_UNORM,
    VGPU_PIXEL_FORMAT_RGBA8_UNORM_SRGB,
    VGPU_PIXEL_FORMAT_RGBA8_SNORM,
    VGPU_PIXEL_FORMAT_RGBA8_UINT,
    VGPU_PIXEL_FORMAT_RGBA8_SINT,
    VGPU_PIXEL_FORMAT_BGRA8_UNORM,
    VGPU_PIXEL_FORMAT_BGRA8_UNORM_SRGB,

    // Packed 32-Bit Pixel formats
    VGPU_PIXEL_FORMAT_RGB10A2_UNORM,
    VGPU_PIXEL_FORMAT_RGB10A2_UINT,
    VGPU_PIXEL_FORMAT_RG11B10_FLOAT,
    VGPU_PIXEL_FORMAT_RGB9E5_FLOAT,

    // 64-Bit Pixel Formats
    VGPU_PIXEL_FORMAT_RG32_UINT,
    VGPU_PIXEL_FORMAT_RG32_SINT,
    VGPU_PIXEL_FORMAT_RG32_FLOAT,
    VGPU_PIXEL_FORMAT_RGBA16_UNORM,
    VGPU_PIXEL_FORMAT_RGBA16_SNORM,
    VGPU_PIXEL_FORMAT_RGBA16_UINT,
    VGPU_PIXEL_FORMAT_RGBA16_SINT,
    VGPU_PIXEL_FORMAT_RGBA16_FLOAT,

    // 128-Bit Pixel Formats
    VGPU_PIXEL_FORMAT_RGBA32_UINT,
    VGPU_PIXEL_FORMAT_RGBA32_SINT,
    VGPU_PIXEL_FORMAT_RGBA32_FLOAT,

    // Depth-stencil
    VGPU_PIXEL_FORMAT_D16_UNORM,
    VGPU_PIXEL_FORMAT_D32_FLOAT,
    VGPU_PIXEL_FORMAT_D24_UNORM_S8_UINT,
    VGPU_PIXEL_FORMAT_D32_FLOAT_S8_UINT,
    VGPU_PIXEL_FORMAT_S8,

    // Compressed formats
    VGPU_PIXEL_FORMAT_BC1_UNORM,        // DXT1
    VGPU_PIXEL_FORMAT_BC1_UNORM_SRGB,   // DXT1
    VGPU_PIXEL_FORMAT_BC2_UNORM,        // DXT3
    VGPU_PIXEL_FORMAT_BC2_UNORM_SRGB,   // DXT3
    VGPU_PIXEL_FORMAT_BC3_UNORM,        // DXT5
    VGPU_PIXEL_FORMAT_BC3_UNORM_SRGB,   // DXT5
    VGPU_PIXEL_FORMAT_BC4_UNORM,        // RGTC Unsigned Red
    VGPU_PIXEL_FORMAT_BC4_SNORM,        // RGTC Signed Red
    VGPU_PIXEL_FORMAT_BC5_UNORM,        // RGTC Unsigned RG
    VGPU_PIXEL_FORMAT_BC5_SNORM,        // RGTC Signed RG
    VGPU_PIXEL_FORMAT_BC6HS16,
    VGPU_PIXEL_FORMAT_BC6HU16,
    VGPU_PIXEL_FORMAT_BC7_UNORM,
    VGPU_PIXEL_FORMAT_BC7_UNORM_SRGB,

    // Compressed PVRTC Pixel Formats
    VGPU_PIXEL_FORMAT_PVRTC_RGB2,
    VGPU_PIXEL_FORMAT_PVRTC_RGBA2,
    VGPU_PIXEL_FORMAT_PVRTC_RGB4,
    VGPU_PIXEL_FORMAT_PVRTC_RGBA4,

    // Compressed ETC Pixel Formats
    VGPU_PIXEL_FORMAT_ETC2_RGB8,
    VGPU_PIXEL_FORMAT_ETC2_RGB8A1,

    // Compressed ASTC Pixel Formats
    VGPU_PIXEL_FORMAT_ASTC4x4,
    VGPU_PIXEL_FORMAT_ASTC5x5,
    VGPU_PIXEL_FORMAT_ASTC6x6,
    VGPU_PIXEL_FORMAT_ASTC8x5,
    VGPU_PIXEL_FORMAT_ASTC8x6,
    VGPU_PIXEL_FORMAT_ASTC8x8,
    VGPU_PIXEL_FORMAT_ASTC10x10,
    VGPU_PIXEL_FORMAT_ASTC12x12,

    VGPU_PIXEL_FORMAT_COUNT,
    _VGPU_PIXEL_FORMAT_FORCE_U32 = 0x7FFFFFFF
} vgpu_pixel_format;

/// Defines pixel format type.
typedef enum VgpuPixelFormatType {
    /// Unknown format Type
    VGPU_PIXEL_FORMAT_TYPE_UNKNOWN = 0,
    /// _FLOATing-point formats.
    VGPU_PIXEL_FORMAT_TYPE_FLOAT,
    /// Unsigned normalized formats.
    VGPU_PIXEL_FORMAT_TYPE_UNORM,
    /// Unsigned normalized SRGB formats
    VGPU_PIXEL_FORMAT_TYPE_UNORM_SRGB,
    /// Signed normalized formats.
    VGPU_PIXEL_FORMAT_TYPE_SNORM,
    /// Unsigned integer formats.
    VGPU_PIXEL_FORMAT_TYPE_UINT,
    /// Signed integer formats.
    VGPU_PIXEL_FORMAT_TYPE_SINT,

    VGPU_PIXEL_FORMAT_TYPE_COUNT = (VGPU_PIXEL_FORMAT_TYPE_SINT - VGPU_PIXEL_FORMAT_TYPE_UNKNOWN + 1),
    _VGPU_PIXEL_FORMAT_TYPE_MAX_ENUM = 0x7FFFFFFF
} VgpuPixelFormatType;

typedef enum vgpu_texture_type {
    VGPU_TEXTURE_TYPE_2D = 0,
    VGPU_TEXTURE_TYPE_3D,
    VGPU_TEXTURE_TYPE_CUBE,
} vgpu_texture_type;

typedef enum vgpu_texture_usage {
    VGPU_TEXTURE_USAGE_NONE = 0,
    VGPU_TEXTURE_USAGE_TRANSFER_SRC = 1 << 0,
    VGPU_TEXTURE_USAGE_TRANSFER_DEST = 1 << 1,
    VGPU_TEXTURE_USAGE_SAMPLED = 1 << 2,
    VGPU_TEXTURE_USAGE_STORAGE = 1 << 3,
    VGPU_TEXTURE_USAGE_RENDER_TARGET = 1 << 4
} vgpu_texture_usage;

typedef enum VgpuBufferUsage {
    AGPU_BUFFER_USAGE_NONE = 0,
    AGPU_BUFFER_USAGE_VERTEX = 1,
    AGPU_BUFFER_USAGE_INDEX = 2,
    AGPU_BUFFER_USAGE_UNIFORM = 4,
    AGPU_BUFFER_USAGE_STORAGE = 8,
    AGPU_BUFFER_USAGE_INDIRECT = 16,
    AGPU_BUFFER_USAGE_DYNAMIC = 32,
    AGPU_BUFFER_USAGE_CPU_ACCESSIBLE = 64,
} VgpuBufferUsage;

typedef enum VgpuShaderStageFlagBits {
    VGPU_SHADER_STAGE_NONE = 0,
    VGPU_SHADER_STAGE_VERTEX_BIT = 0x00000001,
    VGPU_SHADER_STAGE_TESS_CONTROL_BIT = 0x00000002,
    VGPU_SHADER_STAGE_TESS_EVAL_BIT = 0x00000004,
    VGPU_SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
    VGPU_SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
    VGPU_SHADER_STAGE_COMPUTE_BIT = 0x00000020,
    VGPU_SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
    VGPU_SHADER_STAGE_ALL = 0x7FFFFFFF,
} VgpuShaderStageFlagBits;

typedef uint32_t VgpuShaderStageFlags;

typedef enum VgpuVertexFormat {
    VGPU_VERTEX_FORMAT_UNKNOWN = 0,
    VGPU_VERTEX_FORMAT_FLOAT = 1,
    VGPU_VERTEX_FORMAT_FLOAT2 = 2,
    VGPU_VERTEX_FORMAT_FLOAT3 = 3,
    VGPU_VERTEX_FORMAT_FLOAT4 = 4,
    VGPU_VERTEX_FORMAT_BYTE4 = 5,
    VGPU_VERTEX_FORMAT_BYTE4N = 6,
    VGPU_VERTEX_FORMAT_UBYTE4 = 7,
    VGPU_VERTEX_FORMAT_UBYTE4N = 8,
    VGPU_VERTEX_FORMAT_SHORT2 = 9,
    VGPU_VERTEX_FORMAT_SHORT2N = 10,
    VGPU_VERTEX_FORMAT_SHORT4 = 11,
    VGPU_VERTEX_FORMAT_SHORT4N = 12,
    VGPU_VERTEX_FORMAT_COUNT = (VGPU_VERTEX_FORMAT_SHORT2N - VGPU_VERTEX_FORMAT_UNKNOWN + 1),
    _VGPU_VERTEX_FORMAT_MAX_ENUM = 0x7FFFFFFF
} VgpuVertexFormat;

typedef enum VgpuVertexInputRate {
    VGPU_VERTEX_INPUT_RATE_VERTEX = 0,
    VGPU_VERTEX_INPUT_RATE_INSTANCE = 1,
} VgpuVertexInputRate;

typedef enum VgpuPrimitiveTopology {
    VGPU_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
    VGPU_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
    VGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP = 2,
    VGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3,
    VGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4,
    VGPU_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 5,
    VGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 6,
    VGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 7,
    VGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 8,
    VGPU_PRIMITIVE_TOPOLOGY_PATCH_LIST = 9,
    VGPU_PRIMITIVE_TOPOLOGY_COUNT = (VGPU_PRIMITIVE_TOPOLOGY_PATCH_LIST - VGPU_PRIMITIVE_TOPOLOGY_POINT_LIST + 1),
    _VGPU_PRIMITIVE_TOPOLOGY_MAX_ENUM = 0x7FFFFFFF
} VgpuPrimitiveTopology;

typedef enum VgpuIndexType {
    VGPU_INDEX_TYPE_UINT16 = 0,
    VGPU_INDEX_TYPE_UINT32 = 1,
    _VGPU_INDEX_TYPE_MAX_ENUM = 0x7FFFFFFF
} VgpuIndexType;

typedef enum VgpuCompareOp {
    VGPU_COMPARE_OP_NEVER = 0,
    VGPU_COMPARE_OP_LESS,
    VGPU_COMPARE_OP_EQUAL,
    VGPU_COMPARE_OP_LESS_EQUAL,
    VGPU_COMPARE_OP_GREATER,
    VGPU_COMPARE_OP_NOT_EQUAL,
    VGPU_COMPARE_OP_GREATER_EQUAL,
    VGPU_COMPARE_OP_ALWAYS,
    VGPU_COMPARE_OP_COUNT,
    _VGPU_COMPARE_OP_MAX_ENUM = 0x7FFFFFFF
} VgpuCompareOp;

typedef enum VgpuSamplerMinMagFilter {
    VGPU_SAMPLER_MIN_MAG_FILTER_NEAREST = 0,
    VGPU_SAMPLER_MIN_MAG_FILTER_LINEAR,
    _VGPU_SAMPLER_MIN_MAG_FILTER_MAX_ENUM = 0x7FFFFFFF
} VgpuSamplerMinMagFilter;

typedef enum VgpuSamplerMipFilter {
    VGPU_SAMPLER_MIP_FILTER_NEAREST = 0,
    VGPU_SAMPLER_MIP_FILTER_LINEAR,
    _VGPU_SAMPLER_MIP_FILTER_MAX_ENUM = 0x7FFFFFFF
} VgpuSamplerMipFilter;

typedef enum VgpuSamplerAddressMode {
    VGPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    VGPU_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
    VGPU_SAMPLER_ADDRESS_MODE_REPEAT,
    VGPU_SAMPLER_ADDRESS_MODE_MIRROR_REPEAT,
    VGPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER_COLOR,
    VGPU_SAMPLER_ADDRESS_MODE_COUNT,
    _VGPU_SAMPLER_ADDRESS_MODE_MAX_ENUM = 0x7FFFFFFF
} VgpuSamplerAddressMode;

typedef enum VgpuSamplerBorderColor {
    VK_SAMPLER_BORDER_COLOR_TRANSPARENT_BLACK = 0,
    VK_SAMPLER_BORDER_COLOR_OPAQUE_BLACK,
    VK_SAMPLER_BORDER_COLOR_OPAQUE_WHITE,
    VK_SAMPLER_BORDER_COLOR_COUNT,
    _VK_SAMPLER_BORDER_COLOR_MAX_ENUM = 0x7FFFFFFF
} VgpuSamplerBorderColor;

/* Callbacks */
typedef void(*vgpu_log_callback)(void* user_data, vgpu_log_level level, const char* message);

/* Structs */
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

typedef struct vgpu_features {
    bool  independent_blend;
    bool  compute_shader;
    bool  geometry_shader;
    bool  tessellation_shader;
    bool  multi_viewport;
    bool  index_uint32;
    bool  multi_draw_indirect;
    bool  fill_mode_non_solid;
    bool  sampler_anisotropy;
    bool  texture_compression_BC;
    bool  texture_compression_PVRTC;
    bool  texture_compression_ETC2;
    bool  texture_compression_ASTC;
    bool  texture_1D;
    bool  texture_3D;
    bool  texture_2D_array;
    bool  texture_cube_array;
    bool  raytracing;
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

typedef struct vgpu_clear_value {
    union {
        struct {
            float               r;
            float               g;
            float               b;
            float               a;
        };
        struct {
            float               depth;
            uint8_t             stencil;
        };
    };
} vgpu_clear_value;

typedef struct vgpu_color_attachment {
    vgpu_texture*               texture;
    uint32_t                    mip_level;
    union {
        uint32_t face;
        uint32_t layer;
        uint32_t slice;
    };
} vgpu_color_attachment;

typedef struct VgpuBufferDescriptor {
    VgpuBufferUsage             usage;
    uint64_t                    size;
    uint32_t                    stride;
    const char* name;
} VgpuBufferDescriptor;

typedef struct vgpu_texture_desc {
    vgpu_texture_type           type;
    uint32_t                    width;
    uint32_t                    height;
    union {
        uint32_t depth;
        uint32_t layers;
    };
    uint32_t                    mip_levels;
    vgpu_pixel_format           format;
    vgpu_texture_usage          usage;
    vgpu_sample_count           sample_count;
} vgpu_texture_desc;

typedef struct vgpu_render_pass_desc {
    vgpu_color_attachment   color_attachments[VGPU_MAX_COLOR_ATTACHMENTS];
    vgpu_color_attachment   depth_stencil_attachment;
} vgpu_render_pass_desc;

typedef struct vgpu_begin_render_pass_desc {
    vgpu_render_pass*       render_pass;
    vgpu_clear_value        color_attachments[VGPU_MAX_COLOR_ATTACHMENTS];
    vgpu_clear_value        depth_stencil_attachment;
} vgpu_begin_render_pass_desc;

typedef struct VgpuVertexBufferLayoutDescriptor {
    uint32_t                    stride;
    VgpuVertexInputRate         inputRate;
} VgpuVertexBufferLayoutDescriptor;

typedef struct VgpuVertexAttributeDescriptor {
    VgpuVertexFormat            format;
    uint32_t                    offset;
    uint32_t                    bufferIndex;
} VgpuVertexAttributeDescriptor;

typedef struct VgpuVertexDescriptor {
    VgpuVertexBufferLayoutDescriptor    layouts[VGPU_MAX_VERTEX_BUFFER_BINDINGS];
    VgpuVertexAttributeDescriptor       attributes[VGPU_MAX_VERTEX_ATTRIBUTES];
} VgpuVertexDescriptor;

typedef struct VgpuShaderModuleDescriptor {
    VgpuShaderStageFlagBits     stage;
    uint64_t                    codeSize;
    const uint8_t* pCode;
    const char* source;
    const char* entryPoint;
} VgpuShaderModuleDescriptor;

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
    VgpuPrimitiveTopology       primitiveTopology;
} VgpuRenderPipelineDescriptor;

typedef struct VgpuComputePipelineDescriptor {
    uint32_t dummy;
    //VgpuShader                  shader;
} VgpuComputePipelineDescriptor;

typedef struct vgpu_sampler_desc {
    VgpuSamplerAddressMode  addressModeU;
    VgpuSamplerAddressMode  addressModeV;
    VgpuSamplerAddressMode  addressModeW;
    VgpuSamplerMinMagFilter magFilter;
    VgpuSamplerMinMagFilter minFilter;
    VgpuSamplerMipFilter    mipmapFilter;
    uint32_t                maxAnisotropy;
    VgpuCompareOp           compareOp;
    VgpuSamplerBorderColor  borderColor;
    float                   lodMinClamp;
    float                   lodMaxClamp;
} vgpu_sampler_desc;

typedef struct vgpu_swap_chain_desc {
    /// Native window handle (HWND, IUnknown, ANativeWindow, NSWindow).
    uintptr_t           native_handle;
    /// Native window display (HMODULE, X11Display, WaylandDisplay).
    uintptr_t           native_display;

    uint32_t            width;
    uint32_t            height;
    bool                fullscreen;
    vgpu_pixel_format   color_format;
    vgpu_clear_value    color_clear_value;
    vgpu_pixel_format   depth_stencil_format;
    vgpu_clear_value    depth_stencil_clear_value;
    bool                vsync;
    vgpu_sample_count   samples;
} vgpu_swap_chain_desc;

typedef struct vgpu_desc {
    vgpu_backend preferred_backend;
    vgpu_config_flags flags;
    vgpu_power_preference power_preference;
    const vgpu_swap_chain_desc* swapchain;
} vgpu_desc;

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

    VGPU_EXPORT vgpu_backend vgpu_get_default_platform_backend(void);
    VGPU_EXPORT bool vgpu_is_backend_supported(vgpu_backend backend);
    VGPU_EXPORT bool vgpu_init(const char* app_name, const vgpu_desc* desc);
    VGPU_EXPORT void vgpu_shutdown(void);
    VGPU_EXPORT vgpu_backend vgpu_get_backend(void);
    VGPU_EXPORT vgpu_features vgpu_get_features(voide);
    VGPU_EXPORT vgpu_limits vgpu_get_limits(void);
    VGPU_EXPORT vgpu_render_pass* vgpu_get_default_render_pass(void);

    VGPU_EXPORT void vgpu_begin_frame(void);
    VGPU_EXPORT void vgpu_end_frame(void);
    //VGPU_EXPORT VgpuPixelFormat vgpuGetDefaultDepthFormat();
    //VGPU_EXPORT VgpuPixelFormat vgpuGetDefaultDepthStencilFormat();
    //VGPU_EXPORT VgpuFramebuffer vgpuGetCurrentFramebuffer();
   
    /* Buffer */
    //VGPU_EXPORT VgpuResult vgpuCreateBuffer(const VgpuBufferDescriptor* descriptor, const void* pInitData, VgpuBuffer* pBuffer);
    //VGPU_EXPORT VgpuResult vgpuCreateExternalBuffer(const VgpuBufferDescriptor* descriptor, void* handle, VgpuBuffer* pBuffer);
    //VGPU_EXPORT void vgpuDestroyBuffer(VgpuBuffer buffer);

    /* Texture */
    VGPU_EXPORT vgpu_texture* vgpu_texture_create(const vgpu_texture_desc* desc, const void* initial_data);
    VGPU_EXPORT vgpu_texture* vgpu_texture_create_external(const vgpu_texture_desc* desc, void* handle);
    VGPU_EXPORT void vgpu_texture_destroy(vgpu_texture* texture);

    /* Sampler */
    VGPU_EXPORT vgpu_sampler* vgpu_sampler_create(const vgpu_sampler_desc* desc);
    VGPU_EXPORT void vgpu_sampler_destroy(vgpu_sampler* sampler);

    /* RenderPass */
    VGPU_EXPORT vgpu_render_pass* vgpu_render_pass_create(const vgpu_render_pass_desc* desc);
    VGPU_EXPORT void vgpu_render_pass_destroy(vgpu_render_pass* render_pass);
    VGPU_EXPORT void vgpu_render_pass_get_extent(vgpu_render_pass* render_pass, uint32_t* width, uint32_t* height);

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
    VGPU_EXPORT void vgpu_cmd_begin_render_pass(const vgpu_begin_render_pass_desc* begin_pass_desc);
    VGPU_EXPORT void vgpu_cmd_end_render_pass(void);
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
    VGPU_EXPORT uint32_t vgpu_get_format_bits_per_pixel(vgpu_pixel_format format);
    VGPU_EXPORT uint32_t vgpu_get_format_block_size(vgpu_pixel_format format);

    /// Get the format compression ration along the x-axis.
    VGPU_EXPORT uint32_t vgpuGetFormatBlockWidth(vgpu_pixel_format format);
    /// Get the format compression ration along the y-axis.
    VGPU_EXPORT uint32_t vgpuGetFormatBlockHeight(vgpu_pixel_format format);
    /// Get the format Type.
    VGPU_EXPORT VgpuPixelFormatType vgpuGetFormatType(vgpu_pixel_format format);

    /// Check if the format has a depth component.
    VGPU_EXPORT bool vgpu_is_depth_format(vgpu_pixel_format format);
    /// Check if the format has a stencil component.
    VGPU_EXPORT bool vgpu_is_stencil_format(vgpu_pixel_format format);
    /// Check if the format has depth or stencil components.
    VGPU_EXPORT bool vgpu_is_depth_stencil_format(vgpu_pixel_format format);
    /// Check if the format is a compressed format.
    VGPU_EXPORT bool vgpu_is_compressed_format(vgpu_pixel_format format);
    /// Get format string name.
    VGPU_EXPORT const char* vgpu_get_format_name(vgpu_pixel_format format);

#ifdef __cplusplus
}
#endif

#endif // VGPU_H_
