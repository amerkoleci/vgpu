// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "vgpu_driver.h"

#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#elif defined(_WIN32)
#   include <Windows.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#define MAX_MESSAGE_SIZE 1024

static void VGPU_DefaultLogCallback(vgpu_log_level level, const char* message, void* user_data)
{
    VGPU_UNUSED(user_data);

#if defined(__EMSCRIPTEN__)
    switch (level)
    {
    case VGFX_LOG_LEVEL_WARN:
        emscripten_log(EM_LOG_CONSOLE | EM_LOG_WARN, "%s", message);
        break;
    case VGFX_LOG_LEVEL_ERROR:
        emscripten_log(EM_LOG_CONSOLE | EM_LOG_ERROR, "%s", message);
        break;
    default:
        emscripten_log(EM_LOG_CONSOLE, "%s", message);
        break;
    }
#elif defined(_WIN32)
    VGPU_UNUSED(level);
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#else
    VGPU_UNUSED(level);
    VGPU_UNUSED(message);
#endif
}

static vgpu_log_callback s_LogFunc = VGPU_DefaultLogCallback;
static void* s_userData = NULL;

void vgpu_log_info(const char* format, ...) {
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_INFO, msg, s_userData);
}

void vgpu_log_warn(const char* format, ...) {
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_WARN, msg, s_userData);
}

void vgpu_log_error(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_ERROR, msg, s_userData);
}

void vgpu_set_log_callback(vgpu_log_callback func, void* userData) {
    s_LogFunc = func;
    s_userData = userData;
}

// Default allocation callbacks.
void* vgpu_default_alloc(size_t size, void* user_data)
{
    VGPU_UNUSED(user_data);
    void* ptr = malloc(size);
    VGPU_ASSERT(ptr);
    return ptr;
}

void vgpu_default_free(void* ptr, void* user_data)
{
    VGPU_UNUSED(user_data);
    free(ptr);
}

const vgpu_allocation_callbacks VGPU_DEFAULT_ALLOC_CB = { vgpu_default_alloc, vgpu_default_free, NULL };

const vgpu_allocation_callbacks* VGPU_ALLOC_CB = &VGPU_DEFAULT_ALLOC_CB;

void vgpuSetAllocationCallbacks(const vgpu_allocation_callbacks* callback)
{
    if (callback == NULL) {
        VGPU_ALLOC_CB = &VGPU_DEFAULT_ALLOC_CB;
    }
    else {
        VGPU_ALLOC_CB = callback;
    }
}

void* _vgpu_alloc(size_t size)
{
    void* ptr = VGPU_ALLOC_CB->allocate(size, VGPU_ALLOC_CB->user_data);
    VGPU_ASSERT(ptr);
    return ptr;
}

void* _vgpu_alloc_clear(size_t size)
{
    void* ptr = _vgpu_alloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void _vgpu_free(void* ptr)
{
    VGPU_ALLOC_CB->free(ptr, VGPU_ALLOC_CB->user_data);
}

static const VGFXDriver* drivers[] = {
//#if defined(VGPU_D3D11_DRIVER)
//    &D3D11_Driver,
//#endif
#if defined(VGPU_D3D12_DRIVER)
    &D3D12_Driver,
#endif
#if defined(VGPU_VULKAN_DRIVER)
    &Vulkan_Driver,
#endif
#if defined(VGPU_OPENGL_DRIVER)
    &OpenGL_Driver,
#endif
#if defined(VGPU_WEBGPU_DRIVER)
    &WebGPU_driver,
#endif
    NULL
};

#define NULL_RETURN(name) if (name == NULL) { return; }
#define NULL_RETURN_NULL(name) if (name == NULL) { return NULL; }

VGPUBool32 vgpu_is_supported(vgpu_backend backend)
{
    for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
    {
        if (!drivers[i])
            break;

        if (drivers[i]->backend == backend)
        {
            return drivers[i]->is_supported();
        }
    }

    return false;
}

static VGPUDevice* s_renderer = NULL;

VGPUBool32 vgpu_init(const vgpu_config* desc)
{
    if (!desc) {
        vgpu_log_warn("vgpu_init: Invalid config");
        return false;
    }

    if (s_renderer)
    {
        vgpu_log_warn("vgpu_init: Already initialized");
        return true;
    }

    vgpu_backend backend = desc->preferred_backend;

retry:
    if (backend == _VGPU_BACKEND_DEFAULT)
    {
        for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->is_supported())
            {
                s_renderer = drivers[i]->createDevice(desc);
                break;
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->backend == backend)
            {
                if (drivers[i]->is_supported())
                {
                    s_renderer = drivers[i]->createDevice(desc);
                    break;
                }
                else
                {
                    vgpu_log_warn("Wanted API not supported, fallback to default");
                    backend = _VGPU_BACKEND_DEFAULT;
                    goto retry;
                }
            }
        }
    }

    return s_renderer != NULL;
}

void vgpu_shutdown(void) {
    NULL_RETURN(s_renderer);
    s_renderer->destroyDevice(s_renderer);
}

uint64_t vgpu_frame(void) {
    VGPU_ASSERT(s_renderer);

    return s_renderer->frame(s_renderer->driverData);
}

void vgpu_wait_idle(void) {
    VGPU_ASSERT(s_renderer);

    s_renderer->waitIdle(s_renderer->driverData);
}

vgpu_backend vgpu_get_backend(void)
{
    if (s_renderer == NULL) {
        return _VGPU_BACKEND_FORCE_U32;
    }

    return s_renderer->getBackendType();
}

VGPUBool32 vgpuQueryFeature(VGPUFeature feature, void* pInfo, uint32_t infoSize)
{
    if (s_renderer == NULL) {
        return false;
    }

    return s_renderer->queryFeature(s_renderer->driverData, feature, pInfo, infoSize);
}

void vgpuGetAdapterProperties(VGPUAdapterProperties* properties)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(properties);

    s_renderer->getAdapterProperties(s_renderer->driverData, properties);
}

void vgpu_get_limits(VGPULimits* limits)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(limits);

    s_renderer->getLimits(s_renderer->driverData, limits);
}

/* Buffer */
static vgpu_buffer_desc _vgpu_buffer_desc_def(const vgpu_buffer_desc* desc)
{
    vgpu_buffer_desc def = *desc;
    def.size = _VGPU_DEF(def.size, 4);
    return def;
}

vgpu_buffer* vgpu_buffer_create(const vgpu_buffer_desc* desc, const void* init_data)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(desc);

    vgpu_buffer_desc desc_def = _vgpu_buffer_desc_def(desc);
    return s_renderer->createBuffer(s_renderer->driverData, &desc_def, init_data);
}

void vgpu_buffer_destroy(vgpu_buffer* buffer)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(buffer);

    s_renderer->destroyBuffer(s_renderer->driverData, buffer);
}

VGPUDeviceAddress vgpu_buffer_get_device_address(vgpu_buffer* buffer)
{
    VGPU_ASSERT(s_renderer);
    VGPU_ASSERT(buffer);

    return s_renderer->buffer_get_device_address(buffer);
}

void vgpu_buffer_set_label(vgpu_buffer* buffer, const char* label)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(buffer);

    s_renderer->buffer_set_label(s_renderer->driverData, buffer, label);
}

/* Texture */
static VGPUTextureDesc _vgpuTextureDescDef(const VGPUTextureDesc* desc)
{
    VGPUTextureDesc def = *desc;
    def.type = _VGPU_DEF(def.type, VGPUTexture_Type2D);
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_RGBA8Unorm);
    //def.usage = _VGPU_DEF(def.type, VGFXTextureUsage_ShaderRead);
    def.size.width = _VGPU_DEF(def.size.width, 1);
    def.size.height = _VGPU_DEF(def.size.height, 1);
    def.size.depth_or_array_layers = _VGPU_DEF(def.size.depth_or_array_layers, 1);
    def.mipLevelCount = _VGPU_DEF(def.mipLevelCount, 1);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpu_texture_create(const VGPUTextureDesc* desc, const void* init_data)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(desc);

    VGPUTextureDesc desc_def = _vgpuTextureDescDef(desc);

    VGPU_ASSERT(desc_def.size.width > 0 && desc_def.size.height > 0 && desc_def.mipLevelCount >= 0);

    const bool is3D = desc_def.type == VGPUTexture_Type3D;
    const bool isDepthStencil = vgpu_is_depth_stencil_format(desc_def.format);
    bool isCube = false;

    if (desc_def.type == VGPUTexture_Type2D &&
        desc_def.size.width == desc_def.size.height &&
        desc_def.size.depth_or_array_layers >= 6)
    {
        isCube = true;
    }

    uint32_t mipLevels = desc_def.mipLevelCount;
    if (mipLevels == 0)
    {
        if (is3D)
        {
            mipLevels = vgpu_num_mip_levels_3d(desc_def.size.width, desc_def.size.height, desc_def.size.depth_or_array_layers);
        }
        else
        {
            mipLevels = vgpu_num_mip_levels_2d(desc_def.size.width, desc_def.size.height);
        }
    }

    if (desc_def.sampleCount > 1u)
    {
        if (isCube)
        {
            vgpu_log_warn("Cubemap texture cannot be multisample");
            return NULL;
        }

        if (is3D)
        {
            vgpu_log_warn("3D texture cannot be multisample");
            return NULL;
        }

        if (mipLevels > 1)
        {
            vgpu_log_warn("Multisample texture cannot have mipmaps");
            return NULL;
        }
    }

    if (isDepthStencil && mipLevels > 1)
    {
        vgpu_log_warn("Depth texture cannot have mipmaps");
        return NULL;
    }

    //if (isCube)
    //{
    //    ALIMER_ASSERT_MSG(info.width <= limits.maxTextureDimensionCube,
    //        "Requested cube texture size too large: {}, Max allowed: {}", info.width, limits.maxTextureDimensionCube);
    //    ALIMER_ASSERT(info.width == info.height);
    //    ALIMER_ASSERT_MSG(info.sampleCount == TextureSampleCount::Count1, "Cubemap texture cannot be multisampled");
    //}
    //else
    //{
    //    ALIMER_ASSERT_MSG(info.width <= limits.maxTextureDimension2D, "Requested Texture2D width size too large: {}, Max: {}", info.width, limits.maxTextureDimension2D);
    //    ALIMER_ASSERT_MSG(info.height <= limits.maxTextureDimension2D, "Requested Texture2D height size too large: {}, Max: {}", info.height, limits.maxTextureDimension2D);
    //}

    // Check if depth texture and ShaderWrite
    if (isDepthStencil && (desc_def.usage & VGPUTextureUsage_ShaderWrite))
    {
        vgpu_log_warn("Cannot create Depth texture with ShaderWrite usage");
        return NULL;
    }

    return s_renderer->createTexture(s_renderer->driverData, &desc_def, init_data);
}

VGPUTexture vgpu_texture_create_2d(uint32_t width, uint32_t height, vgpu_pixel_format format, uint32_t mipLevelCount, VGPUTextureUsage usage, const void* init_data)
{
    VGPUTextureDesc desc = {
        .size = {width, height, 1u},
        .type = VGPUTexture_Type2D,
        .format = format,
        .usage = usage,
        .mipLevelCount = mipLevelCount,
        .sampleCount = 1u
    };

    return vgpu_texture_create(&desc, init_data);
}

VGPUTexture vgpu_texture_create_cube(uint32_t size, vgpu_pixel_format format, uint32_t mipLevelCount, const void* init_data)
{
    VGPUTextureDesc desc = {
         .size = {size, size, 6u},
        .type = VGPUTexture_Type2D,
        .format = format,
        .usage = VGPUTextureUsage_ShaderRead,
        .mipLevelCount = mipLevelCount,
        .sampleCount = 1u
    };

    return vgpu_texture_create(&desc, init_data);
}

void vgpu_texture_destroy(VGPUTexture texture)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(texture);

    s_renderer->destroyTexture(s_renderer->driverData, texture);
}

/* Sampler*/
static VGPUSamplerDesc _vgpuSamplerDescDef(const VGPUSamplerDesc* desc)
{
    VGPUSamplerDesc def = *desc;
    def.maxAnisotropy = _VGPU_DEF(desc->maxAnisotropy, 1);
    return def;
}

VGPUSampler* vgpuCreateSampler(const VGPUSamplerDesc* desc)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(desc);

    VGPUSamplerDesc desc_def = _vgpuSamplerDescDef(desc);
    return s_renderer->createSampler(s_renderer->driverData, &desc_def);
}

void vgpu_sampler_destroy(VGPUSampler* sampler)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(sampler);

    s_renderer->destroySampler(s_renderer->driverData, sampler);
}

/* Shader Module */
VGPUShaderModule vgpuCreateShader(const void* pCode, size_t codeSize)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(pCode);

    return s_renderer->createShaderModule(s_renderer->driverData, pCode, codeSize);
}

void vgpu_shader_destroy(VGPUShaderModule module)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(module);

    s_renderer->destroyShaderModule(s_renderer->driverData, module);
}

/* Pipeline */
static VGPURenderPipelineDesc _vgpuRenderPipelineDescDef(const VGPURenderPipelineDesc* desc)
{
    VGPURenderPipelineDesc def = *desc;
    def.depthStencilState.stencilReadMask = _VGPU_DEF(def.depthStencilState.stencilReadMask, 0xFF);
    def.depthStencilState.stencilWriteMask = _VGPU_DEF(def.depthStencilState.stencilWriteMask, 0xFF);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUPipeline* vgpu_pipeline_create_render(const VGPURenderPipelineDesc* desc)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->vertex);

    VGPURenderPipelineDesc desc_def = _vgpuRenderPipelineDescDef(desc);
    return s_renderer->createRenderPipeline(s_renderer->driverData, &desc_def);
}

VGPUPipeline* vgpu_pipeline_create_compute(const VGPUComputePipelineDesc* desc)
{
    VGPU_ASSERT(s_renderer);

    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->shader);

    return s_renderer->createComputePipeline(s_renderer->driverData, desc);
}

VGPUPipeline* vgpu_pipeline_create_ray_tracing(const VGPURayTracingPipelineDesc* desc)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(desc);

    return s_renderer->createRayTracingPipeline(s_renderer->driverData, desc);
}

void vgpu_pipeline_destroy(VGPUPipeline* pipeline)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(pipeline);

    s_renderer->destroyPipeline(s_renderer->driverData, pipeline);
}

/* SwapChain */
static VGPUSwapChainDesc _vgpuSwapChainDescDef(const VGPUSwapChainDesc* desc)
{
    VGPUSwapChainDesc def = *desc;
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_BGRA8Unorm);
    def.presentMode = _VGPU_DEF(def.presentMode, VGPUPresentMode_Immediate);
    return def;
}

VGPUSwapChain* vgpuCreateSwapChain(void* window, const VGPUSwapChainDesc* desc)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN_NULL(window);
    NULL_RETURN_NULL(desc);

    VGPUSwapChainDesc def = _vgpuSwapChainDescDef(desc);
    return s_renderer->createSwapChain(s_renderer->driverData, window, &def);
}

void vgpuDestroySwapChain(VGPUSwapChain* swapChain)
{
    VGPU_ASSERT(s_renderer);
    NULL_RETURN(swapChain);

    s_renderer->destroySwapChain(s_renderer->driverData, swapChain);
}

vgpu_pixel_format vgpuSwapChainGetFormat(VGPUSwapChain* swapChain)
{
    VGPU_ASSERT(s_renderer);

    return s_renderer->getSwapChainFormat(s_renderer->driverData, swapChain);
}

/* Commands */
VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUCommandQueue queueType, const char* label)
{
    VGPU_ASSERT(s_renderer);

    return s_renderer->beginCommandBuffer(s_renderer->driverData, queueType, label);
}

void vgpuPushDebugGroup(VGPUCommandBuffer commandBuffer, const char* groupLabel)
{
    NULL_RETURN(groupLabel);

    commandBuffer->pushDebugGroup(commandBuffer->driverData, groupLabel);
}

void vgpuPopDebugGroup(VGPUCommandBuffer commandBuffer)
{
    commandBuffer->popDebugGroup(commandBuffer->driverData);
}

void vgpuInsertDebugMarker(VGPUCommandBuffer commandBuffer, const char* debugLabel)
{
    NULL_RETURN(debugLabel);

    commandBuffer->insertDebugMarker(commandBuffer->driverData, debugLabel);
}

void vgpuSetPipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline* pipeline)
{
    VGPU_ASSERT(pipeline);

    commandBuffer->setPipeline(commandBuffer->driverData, pipeline);
}

void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    commandBuffer->dispatch(commandBuffer->driverData, groupCountX, groupCountY, groupCountZ);
}

void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, vgpu_buffer* buffer, uint64_t offset)
{
    NULL_RETURN(buffer);

    commandBuffer->dispatchIndirect(commandBuffer->driverData, buffer, offset);
}

VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight)
{
    return commandBuffer->acquireSwapchainTexture(commandBuffer->driverData, swapChain, pWidth, pHeight);
}

void vgpuBeginRenderPass(VGPUCommandBuffer commandBuffer, const VGPURenderPassDesc* desc)
{
    NULL_RETURN(desc);

    commandBuffer->beginRenderPass(commandBuffer->driverData, desc);
}

void vgpuEndRenderPass(VGPUCommandBuffer commandBuffer)
{
    commandBuffer->endRenderPass(commandBuffer->driverData);
}

void vgpuSetViewport(VGPUCommandBuffer commandBuffer, const VGPUViewport* viewport)
{
    VGPU_ASSERT(viewport);

    commandBuffer->setViewport(commandBuffer->driverData, viewport);
}

void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* rect)
{
    VGPU_ASSERT(rect);
    commandBuffer->setScissorRect(commandBuffer->driverData, rect);
}

void vgpuSetVertexBuffer(VGPUCommandBuffer commandBuffer, uint32_t index, vgpu_buffer* buffer, uint64_t offset)
{
    commandBuffer->setVertexBuffer(commandBuffer->driverData, index, buffer, offset);
}

void vgpuSetIndexBuffer(VGPUCommandBuffer commandBuffer, vgpu_buffer* buffer, uint64_t offset, vgpu_index_type type)
{
    commandBuffer->setIndexBuffer(commandBuffer->driverData, buffer, offset, type);
}

void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance)
{
    commandBuffer->draw(commandBuffer->driverData, vertexStart, vertexCount, instanceCount, firstInstance);
}

void vgpuDrawIndexed(VGPUCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    commandBuffer->drawIndexed(commandBuffer->driverData, indexCount, instanceCount, firstIndex, firstIndex, firstInstance);
}

void vgpu_submit(VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    VGPU_ASSERT(s_renderer);
    VGPU_ASSERT(commandBuffers);
    VGPU_ASSERT(count);

    s_renderer->submit(s_renderer->driverData, commandBuffers, count);
}

// Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
static const vgpu_pixel_format_info c_FormatInfo[] = {
    //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencl signed  srgb
    { VGPUTextureFormat_Undefined,      "Undefined",            0,   0, 0,  _VGPUTextureFormatKind_Force32 },
    { VGPUTextureFormat_R8Unorm,        "R8Unorm",              1,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_R8Snorm,        "R8Snorm",              1,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_R8Uint,         "R8Uint",               1,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_R8Sint,         "R8USnt",               1,   1, 1, VGPUTextureFormatKind_Sint },

    { VGPUTextureFormat_R16Unorm,         "R16Unorm",           2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_R16Snorm,         "R16Snorm",           2,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_R16Uint,          "R16Uint",            2,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_R16Sint,          "R16Sint",            2,   1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_R16Float,         "R16Float",           2,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RG8Unorm,         "RG8Unorm",           2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RG8Snorm,         "RG8Snorm",           2,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_RG8Uint,          "RG8Uint",            2,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RG8Sint,          "RG8Sint",            2,   1, 1, VGPUTextureFormatKind_Sint },

    { VGPUTextureFormat_BGRA4Unorm,       "BGRA4Unorm",         2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_B5G6R5Unorm,      "B5G6R5Unorm",        2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_B5G5R5A1Unorm,    "B5G5R5A1Unorm",      2,   1, 1, VGPUTextureFormatKind_Unorm },

    { VGPUTextureFormat_R32Uint,          "R32Uint",            4,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_R32Sint,          "R32Sint",            4,   1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_R32Float,         "R32Float",           4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RG16Unorm,        "RG16Unorm",          4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RG16Snorm,        "RG16Snorm",          4,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_RG16Uint,         "RG16Uint",           4,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RG16Sint,         "RG16Sint",           4,   1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_RG16Float,        "RG16Float",          4,   1, 1, VGPUTextureFormatKind_Float },

    { VGPUTextureFormat_RGBA8Uint,        "RGBA8Uint",          4,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RGBA8Sint,        "RGBA8Sint",          4,   1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_RGBA8Unorm,       "RGBA8Unorm",         4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RGBA8UnormSrgb,   "RGBA8UnormSrgb",     4,   1, 1, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_RGBA8Snorm,       "RGBA8Snorm",         4,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_BGRA8Unorm,       "BGRA8Unorm",         4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BGRA8UnormSrgb,   "BGRA8UnormSrgb",     4,   1, 1, VGPUTextureFormatKind_UnormSrgb },

    { VGPUTextureFormat_RGB9E5Ufloat,    "RGB9E5Ufloat",        4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RGB10A2Unorm,    "RGB10A2Unorm",        4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RGB10A2Uint,     "RGB10A2Uint",         4,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RG11B10Float,    "RG11B10Float",        4,   1, 1, VGPUTextureFormatKind_Float },

    { VGPUTextureFormat_RG32Uint,         "RG32Uint",           8,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RG32Sint,         "RG32Sint",           8,   1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_RG32Float,        "RG32Float",          8,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RGBA16Unorm,      "RGBA16Unorm",        8,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RGBA16Snorm,      "RGBA16Snorm",        8,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_RGBA16Uint,       "RGBA16Uint",         8,   1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RGBA16Sint,       "RGBA16Sint",         8,   1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_RGBA16Float,      "RGBA16Float",        8,   1, 1, VGPUTextureFormatKind_Float },

    { VGPUTextureFormat_RGBA32Uint,       "RGBA32Uint",        16,  1, 1, VGPUTextureFormatKind_Uint },
    { VGPUTextureFormat_RGBA32Sint,       "RGBA32Sint",        16,  1, 1, VGPUTextureFormatKind_Sint },
    { VGPUTextureFormat_RGBA32Float,      "RGBA32Float",       16,  1, 1, VGPUTextureFormatKind_Float },

    // Depth-stencil formats
    { VGPUTextureFormat_Depth16Unorm,           "Depth16Unorm",             2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Depth32Float,           "Depth32Float",             4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_Stencil8,               "Stencil8",                 4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_Depth24UnormStencil8,   "Depth24UnormStencil8",     4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Depth32FloatStencil8,   "Depth32FloatStencil8",     8,   1, 1, VGPUTextureFormatKind_Float },

    // BC compressed formats
    { VGPUTextureFormat_Bc1RgbaUnorm,       "BC1RgbaUnorm",         8,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Bc1RgbaUnormSrgb,   "BC1RgbaUnormSrgb",     8,   4, 4, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc2RgbaUnorm,       "BC2RgbaUnorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Bc2RgbaUnormSrgb,   "BC2RgbaUnormSrgb",     16,  4, 4, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc3RgbaUnorm,       "BC3RgbaUnorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Bc3RgbaUnormSrgb,   "BC3RgbaUnormSrgb",     16,  4, 4, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_Bc4RUnorm,          "BC4RUnorm",            8,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Bc4RSnorm,          "BC4RSnorm",            8,   4, 4, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_Bc5RgUnorm,         "BC5Unorm",             16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Bc5RgSnorm,         "BC5Snorm",             16,  4, 4, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_Bc6hRgbUfloat,      "Bc6hRgbUfloat",        16,  4, 4, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_Bc6hRgbSfloat,      "Bc6hRgbSfloat",         16,  4, 4, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_Bc7RgbaUnorm,       "Bc7RgbaUnorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Bc7RgbaUnormSrgb,   "Bc7RgbaUnormSrgb",     16,  4, 4, VGPUTextureFormatKind_UnormSrgb },

    // ETC2/EAC compressed formats
    { VGPUTextureFormat_Etc2Rgb8Unorm,       "Etc2Rgb8Unorm",         8,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgb8UnormSrgb,   "Etc2Rgb8UnormSrgb",     8,   4, 4, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Etc2Rgb8A1Unorm,     "Etc2Rgb8A1Unorm,",     16,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgb8A1UnormSrgb, "Etc2Rgb8A1UnormSrgb",  16,   4, 4, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Etc2Rgba8Unorm,      "Etc2Rgba8Unorm",       16,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Etc2Rgba8UnormSrgb,  "Etc2Rgba8UnormSrgb",   16,   4, 4, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_EacR11Unorm,         "EacR11Unorm",          8,    4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_EacR11Snorm,         "EacR11Snorm",          8,    4, 4, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_EacRg11Unorm,        "EacRg11Unorm",         16,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_EacRg11Snorm,        "EacRg11Snorm",         16,   4, 4, VGPUTextureFormatKind_Snorm },

    // ASTC compressed formats
    { VGPUTextureFormat_Astc4x4Unorm,        "ASTC4x4Unorm",         16,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc4x4UnormSrgb,    "ASTC4x4UnormSrgb",     16,   4, 4, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc5x4Unorm,        "ASTC5x4Unorm",         16,   5, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc5x4UnormSrgb,    "ASTC5x4UnormSrgb",     16,   5, 4, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc5x5Unorm,        "ASTC5x5UnormSrgb",     16,   5, 5, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc5x5UnormSrgb,    "ASTC5x5UnormSrgb",     16,   5, 5, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc6x5Unorm,        "ASTC6x5Unorm",         16,   6, 5, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc6x5UnormSrgb,    "ASTC6x5UnormSrgb",     16,   6, 5, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc6x6Unorm,        "ASTC6x6Unorm",         16,   6, 6, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc6x6UnormSrgb,    "ASTC6x6UnormSrgb",     16,   6, 6, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x5Unorm,        "ASTC8x5Unorm",         16,   8, 5, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x5UnormSrgb,    "ASTC8x5UnormSrgb",     16,   8, 5, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x6Unorm,        "ASTC8x6Unorm",         16,   8, 6, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x6UnormSrgb,    "ASTC8x6UnormSrgb",     16,   8, 6, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc8x8Unorm,        "ASTC8x8Unorm",         16,   8, 8, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc8x8UnormSrgb,    "ASTC8x8UnormSrgb",     16,   8, 8, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x5Unorm,       "ASTC10x5Unorm",        16,   10, 5, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x5UnormSrgb,   "ASTC10x5UnormSrgb",    16,   10, 5, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x6Unorm,       "ASTC10x6Unorm",        16,   10, 6, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x6UnormSrgb,   "ASTC10x6UnormSrgb",    16,   10, 6, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x8Unorm,       "ASTC10x8Unorm",        16,   10, 8, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Astc10x8UnormSrgb,   "ASTC10x8UnormSrgb",    16,   10, 8, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc10x10Unorm,      "ASTC10x10Unorm",       16,   10, 10, VGPUTextureFormatKind_Unorm  },
    { VGPUTextureFormat_Astc10x10UnormSrgb,  "ASTC10x10UnormSrgb",   16,   10, 10, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc12x10Unorm,      "ASTC12x10Unorm",       16,   12, 10, VGPUTextureFormatKind_Unorm  },
    { VGPUTextureFormat_Astc12x10UnormSrgb,  "ASTC12x10UnormSrgb",   16,   12, 10, VGPUTextureFormatKind_UnormSrgb },
    { VGPUTextureFormat_Astc12x12Unorm,      "ASTC12x12Unorm",       16,   12, 12, VGPUTextureFormatKind_Unorm  },
    { VGPUTextureFormat_Astc12x12UnormSrgb,  "ASTC12x12UnormSrgb",   16,   12, 12, VGPUTextureFormatKind_UnormSrgb },
};

VGPUBool32 vgpu_is_depth_format(vgpu_pixel_format format)
{
    switch (format)
    {
    case VGPUTextureFormat_Depth16Unorm:
    case VGPUTextureFormat_Depth32Float:
    case VGPUTextureFormat_Depth24UnormStencil8:
    case VGPUTextureFormat_Depth32FloatStencil8:
        return true;
    default:
        return false;
    }
}

VGPUBool32 vgpu_is_stencil_format(vgpu_pixel_format format)
{
    switch (format)
    {
    case VGPUTextureFormat_Stencil8:
    case VGPUTextureFormat_Depth24UnormStencil8:
    case VGPUTextureFormat_Depth32FloatStencil8:
        return true;
    default:
        return false;
    }
}

VGPUBool32 vgpu_is_depth_stencil_format(vgpu_pixel_format format)
{
    switch (format)
    {
    case VGPUTextureFormat_Depth16Unorm:
    case VGPUTextureFormat_Depth32Float:
    case VGPUTextureFormat_Stencil8:
    case VGPUTextureFormat_Depth24UnormStencil8:
    case VGPUTextureFormat_Depth32FloatStencil8:
        return true;
    default:
        return false;
    }
}

VGPUTextureFormatKind vgpuGetPixelFormatKind(vgpu_pixel_format format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].kind;
}

uint32_t vgpu_num_mip_levels_2d(uint32_t width, uint32_t height) {
    uint32_t mipLevels = 0;
    uint32_t size = _VGPU_MAX(width, height);
    while (1u << mipLevels <= size)
    {
        ++mipLevels;
    }

    if (1u << mipLevels < size)
        ++mipLevels;

    return mipLevels;
}

uint32_t vgpu_num_mip_levels_3d(uint32_t width, uint32_t height, uint32_t depth) {
    uint32_t mipLevels = 0;
    uint32_t size = _VGPU_MAX(_VGPU_MAX(width, height), depth);
    while (1u << mipLevels <= size)
    {
        ++mipLevels;
    }

    if (1u << mipLevels < size)
        ++mipLevels;

    return mipLevels;
}


