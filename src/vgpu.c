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

static void VGPU_DefaultLogCallback(VGPULogLevel level, const char* message)
{
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
    _VGPU_UNUSED(level);
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#else
    _VGPU_UNUSED(level);
    _VGPU_UNUSED(message);
#endif
}

static vgpu_log_callback s_LogFunc = VGPU_DefaultLogCallback;

void vgpuLogInfo(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_INFO, msg);
}

void vgpu_log_warn(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_WARN, msg);
}

void vgpuLogError(const char* format, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, MAX_MESSAGE_SIZE, format, args);
    va_end(args);
    s_LogFunc(VGPU_LOG_LEVEL_ERROR, msg);
}

void vgpuSetLogCallback(vgpu_log_callback func)
{
    s_LogFunc = func;
}

// Default allocation callbacks.
void* vgpu_default_alloc(size_t size, void* user_data)
{
    _VGPU_UNUSED(user_data);
    void* ptr = malloc(size);
    VGPU_ASSERT(ptr);
    return ptr;
}

void vgpu_default_free(void* ptr, void* user_data)
{
    _VGPU_UNUSED(user_data);
    free(ptr);
}

const vgpu_allocation_callbacks VGPU_DEFAULT_ALLOC_CB = { vgpu_default_alloc, vgpu_default_free };

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
#if defined(VGPU_D3D12_DRIVER)
    &D3D12_Driver,
#endif
#if defined(VGPU_VULKAN_DRIVER)
    &Vulkan_Driver,
#endif
#if defined(VGPU_WEBGPU_DRIVER)
    &webgpu_driver,
#endif
    NULL
};

#define NULL_RETURN(name) if (name == NULL) { return; }
#define NULL_RETURN_NULL(name) if (name == NULL) { return NULL; }

VGPUBool32 vgpuIsBackendSupported(VGPUBackendType backend)
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

VGPUDevice vgpuCreateDevice(const VGPUDeviceDesc* desc)
{
    NULL_RETURN_NULL(desc);

    VGPUBackendType backend = desc->preferredBackend;

retry:
    if (backend == _VGPUBackendType_Default)
    {
        for (uint32_t i = 0; i < _VGPU_COUNT_OF(drivers); ++i)
        {
            if (!drivers[i])
                break;

            if (drivers[i]->is_supported())
            {
                return drivers[i]->createDevice(desc);
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
                    return drivers[i]->createDevice(desc);
                }
                else
                {
                    vgpu_log_warn("Wanted API not supported, fallback to default");
                    backend = _VGPUBackendType_Default;
                    goto retry;
                }
            }
        }
    }

    return NULL;
}

void vgpuDestroyDevice(VGPUDevice device)
{
    NULL_RETURN(device);
    device->destroyDevice(device);
}

uint64_t vgpuFrame(VGPUDevice device)
{
    if (device == NULL)
    {
        return (uint64_t)(-1);
    }

    return device->frame(device->driverData);
}

void vgpuWaitIdle(VGPUDevice device)
{
    NULL_RETURN(device);
    device->waitIdle(device->driverData);
}

VGPUBackendType vgpuGetBackendType(VGPUDevice device)
{
    if (device == NULL) {
        return _VGPUBackendType_Force32;
    }

    return device->getBackendType();
}

VGPUBool32 vgpuQueryFeature(VGPUDevice device, VGPUFeature feature, void* pInfo, uint32_t infoSize)
{
    if (device == NULL) {
        return false;
    }

    return device->queryFeature(device->driverData, feature, pInfo, infoSize);
}

void vgpuGetAdapterProperties(VGPUDevice device, VGPUAdapterProperties* properties)
{
    NULL_RETURN(device);
    NULL_RETURN(properties);

    device->getAdapterProperties(device->driverData, properties);
}

void vgpuGetLimits(VGPUDevice device, VGPULimits* limits)
{
    NULL_RETURN(device);
    NULL_RETURN(limits);

    device->getLimits(device->driverData, limits);
}

/* Buffer */
static VGPUBufferDesc _vgpuBufferDescDef(const VGPUBufferDesc* desc)
{
    VGPUBufferDesc def = *desc;
    def.size = _VGPU_DEF(def.size, 4);
    return def;
}

VGPUBuffer vgpuCreateBuffer(VGPUDevice device, const VGPUBufferDesc* desc, const void* pInitialData)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    VGPUBufferDesc desc_def = _vgpuBufferDescDef(desc);
    return device->createBuffer(device->driverData, &desc_def, pInitialData);
}

void vgpuDestroyBuffer(VGPUDevice device, VGPUBuffer buffer)
{
    NULL_RETURN(device);
    NULL_RETURN(buffer);

    device->destroyBuffer(device->driverData, buffer);
}

/* Texture */
static VGPUTextureDesc _vgpuTextureDescDef(const VGPUTextureDesc* desc)
{
    VGPUTextureDesc def = *desc;
    def.type = _VGPU_DEF(def.type, VGPUTexture_Type2D);
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_RGBA8UNorm);
    //def.usage = _VGPU_DEF(def.type, VGFXTextureUsage_ShaderRead);
    def.size.width = _VGPU_DEF(def.size.width, 1);
    def.size.height = _VGPU_DEF(def.size.height, 1);
    def.size.depthOrArrayLayers = _VGPU_DEF(def.size.depthOrArrayLayers, 1);
    def.mipLevelCount = _VGPU_DEF(def.mipLevelCount, 1);
    def.sampleCount = _VGPU_DEF(def.sampleCount, 1);
    return def;
}

VGPUTexture vgpuCreateTexture(VGPUDevice device, const VGPUTextureDesc* desc, const void* pInitialData)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    VGPUTextureDesc desc_def = _vgpuTextureDescDef(desc);

    VGPU_ASSERT(desc_def.size.width > 0 && desc_def.size.height > 0 && desc_def.mipLevelCount >= 0);

    const bool is3D = desc_def.type == VGPUTexture_Type3D;
    const bool isDepthStencil = vgpuIsDepthStencilFormat(desc_def.format);
    bool isCube = false;

    if (desc_def.type == VGPUTexture_Type2D &&
        desc_def.size.width == desc_def.size.height &&
        desc_def.size.depthOrArrayLayers >= 6)
    {
        isCube = true;
    }

    uint32_t mipLevels = desc_def.mipLevelCount;
    if (mipLevels == 0)
    {
        if (is3D)
        {
            mipLevels = vgpuNumMipLevels(desc_def.size.width, desc_def.size.height, desc_def.size.depthOrArrayLayers);
        }
        else
        {
            mipLevels = vgpuNumMipLevels(desc_def.size.width, desc_def.size.height, 1);
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

    return device->createTexture(device->driverData, &desc_def, pInitialData);
}

VGPUTexture vgpuCreateTexture2D(VGPUDevice device, uint32_t width, uint32_t height, VGPUTextureFormat format, uint32_t mipLevelCount, VGPUTextureUsage usage, const void* pInitialData)
{
    VGPUTextureDesc desc = {
        .size = {width, height, 1u},
        .type = VGPUTexture_Type2D,
        .format = format,
        .usage = usage,
        .mipLevelCount = mipLevelCount,
        .sampleCount = 1u
    };

    return vgpuCreateTexture(device, &desc, pInitialData);
}

VGPUTexture vgpuCreateTextureCube(VGPUDevice device, uint32_t size, VGPUTextureFormat format, uint32_t mipLevelCount, const void* pInitialData)
{
    VGPUTextureDesc desc = {
         .size = {size, size, 6u},
        .type = VGPUTexture_Type2D,
        .format = format,
        .usage = VGPUTextureUsage_ShaderRead,
        .mipLevelCount = mipLevelCount,
        .sampleCount = 1u
    };

    return vgpuCreateTexture(device, &desc, pInitialData);
}

void vgpuDestroyTexture(VGPUDevice device, VGPUTexture texture)
{
    NULL_RETURN(device);
    NULL_RETURN(texture);

    device->destroyTexture(device->driverData, texture);
}

/* Sampler*/
static VGPUSamplerDesc _vgpuSamplerDescDef(const VGPUSamplerDesc* desc)
{
    VGPUSamplerDesc def = *desc;
    def.maxAnisotropy = _VGPU_DEF(desc->maxAnisotropy, 1);
    return def;
}

VGPUSampler vgpuCreateSampler(VGPUDevice device, const VGPUSamplerDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    VGPUSamplerDesc desc_def = _vgpuSamplerDescDef(desc);
    return device->createSampler(device->driverData, &desc_def);
}

void vgpuDestroySampler(VGPUDevice device, VGPUSampler sampler)
{
    NULL_RETURN(device);
    NULL_RETURN(sampler);

    device->destroySampler(device->driverData, sampler);
}

/* Shader Module */
VGPUShaderModule vgpuCreateShader(VGPUDevice device, const void* pCode, size_t codeSize)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(pCode);

    return device->createShaderModule(device->driverData, pCode, codeSize);
}

void vgpuDestroyShader(VGPUDevice device, VGPUShaderModule module)
{
    NULL_RETURN(device);
    NULL_RETURN(module);

    device->destroyShaderModule(device->driverData, module);
}

/* Pipeline */
static VGPURenderPipelineDesc _vgpuRenderPipelineDescDef(const VGPURenderPipelineDesc* desc)
{
    VGPURenderPipelineDesc def = *desc;
    return def;
}

VGPUPipeline vgpuCreateRenderPipeline(VGPUDevice device, const VGPURenderPipelineDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->vertex);

    VGPURenderPipelineDesc desc_def = _vgpuRenderPipelineDescDef(desc);
    return device->createRenderPipeline(device->driverData, &desc_def);
}

VGPUPipeline vgpuCreateComputePipeline(VGPUDevice device, const VGPUComputePipelineDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);
    VGPU_ASSERT(desc->shader);

    return device->createComputePipeline(device->driverData, desc);
}

VGPUPipeline vgpuCreateRayTracingPipeline(VGPUDevice device, const VGPURayTracingPipelineDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(desc);

    return device->createRayTracingPipeline(device->driverData, desc);
}

void vgpuDestroyPipeline(VGPUDevice device, VGPUPipeline pipeline)
{
    NULL_RETURN(device);
    NULL_RETURN(pipeline);

    device->destroyPipeline(device->driverData, pipeline);
}

/* SwapChain */
static VGPUSwapChainDesc _vgpuSwapChainDescDef(const VGPUSwapChainDesc* desc)
{
    VGPUSwapChainDesc def = *desc;
    def.format = _VGPU_DEF(def.format, VGPUTextureFormat_BGRA8UNorm);
    def.presentMode = _VGPU_DEF(def.presentMode, VGPUPresentMode_Immediate);
    return def;
}

VGPUSwapChain vgpuCreateSwapChain(VGPUDevice device, void* windowHandle, const VGPUSwapChainDesc* desc)
{
    NULL_RETURN_NULL(device);
    NULL_RETURN_NULL(windowHandle);
    NULL_RETURN_NULL(desc);

    VGPUSwapChainDesc def = _vgpuSwapChainDescDef(desc);
    return device->createSwapChain(device->driverData, windowHandle, &def);
}

void vgpuDestroySwapChain(VGPUDevice device, VGPUSwapChain swapChain)
{
    NULL_RETURN(device);
    NULL_RETURN(swapChain);

    device->destroySwapChain(device->driverData, swapChain);
}

VGPUTextureFormat vgpuSwapChainGetFormat(VGPUDevice device, VGPUSwapChain swapChain)
{
    return device->getSwapChainFormat(device->driverData, swapChain);
}

/* Commands */
VGPUCommandBuffer vgpuBeginCommandBuffer(VGPUDevice device, const char* label)
{
    NULL_RETURN_NULL(device);

    return device->beginCommandBuffer(device->driverData, label);
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

void vgpuSetPipeline(VGPUCommandBuffer commandBuffer, VGPUPipeline pipeline)
{
    VGPU_ASSERT(pipeline);

    commandBuffer->setPipeline(commandBuffer->driverData, pipeline);
}

void vgpuDispatch(VGPUCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    commandBuffer->dispatch(commandBuffer->driverData, groupCountX, groupCountY, groupCountZ);
}

void vgpuDispatchIndirect(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset)
{
    NULL_RETURN(buffer);

    commandBuffer->dispatchIndirect(commandBuffer->driverData, buffer, offset);
}

VGPUTexture vgpuAcquireSwapchainTexture(VGPUCommandBuffer commandBuffer, VGPUSwapChain swapChain, uint32_t* pWidth, uint32_t* pHeight)
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

void vgpuSetViewports(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPUViewport* viewports)
{
    VGPU_ASSERT(viewports);
    VGPU_ASSERT(count < VGPU_MAX_VIEWPORTS_AND_SCISSORS);

    commandBuffer->setViewports(commandBuffer->driverData, count, viewports);
}

void vgpuSetScissorRect(VGPUCommandBuffer commandBuffer, const VGPURect* rect)
{
    VGPU_ASSERT(rect);
    commandBuffer->set_scissor_rect(commandBuffer->driverData, rect);
}

void vgpuSetScissorRects(VGPUCommandBuffer commandBuffer, uint32_t count, const VGPURect* rects)
{
    VGPU_ASSERT(rects);
    VGPU_ASSERT(count < VGPU_MAX_VIEWPORTS_AND_SCISSORS);

    commandBuffer->set_scissor_rects(commandBuffer->driverData, count, rects);
}

void vgpuSetVertexBuffer(VGPUCommandBuffer commandBuffer, uint32_t index, VGPUBuffer buffer, uint64_t offset)
{
    commandBuffer->setVertexBuffer(commandBuffer->driverData, index, buffer, offset);
}

void vgpuSetIndexBuffer(VGPUCommandBuffer commandBuffer, VGPUBuffer buffer, uint64_t offset, VGPUIndexType indexType)
{
    commandBuffer->setIndexBuffer(commandBuffer->driverData, buffer, offset, indexType);
}

void vgpuDraw(VGPUCommandBuffer commandBuffer, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance)
{
    commandBuffer->draw(commandBuffer->driverData, vertexStart, vertexCount, instanceCount, baseInstance);
}

void vgpuSubmit(VGPUDevice device, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    VGPU_ASSERT(commandBuffers);
    VGPU_ASSERT(count);

    device->submit(device->driverData, commandBuffers, count);
}

// Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
static const VGPUFormatInfo c_FormatInfo[] = {
    //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencl signed  srgb
    { VGPUTextureFormat_Undefined,      "Undefined",            0,   0, 0,  VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_R8UNorm,        "R8_UNORM",             1,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_R8SNorm,        "R8_SNORM",             1,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_R8UInt,         "R8_UINT",              1,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_R8SInt,         "R8_SINT",              1,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },

    { VGPUTextureFormat_R16UInt,          "R16_UINT",           2,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_R16SInt,          "R16_SINT",           2,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_R16UNorm,         "R16_UNORM",          2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_R16SNorm,         "R16_SNORM",          2,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_R16Float,         "R16_FLOAT",          2,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RG8UInt,          "RG8_UINT",           2,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RG8SInt,          "RG8_SINT",           2,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RG8UNorm,         "RG8_UNORM",          2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RG8SNorm,         "RG8_SNORM",          2,   1, 1, VGPUTextureFormatKind_Snorm },

    { VGPUTextureFormat_BGRA4UNorm,       "BGRA4_UNORM",        2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_B5G6R5UNorm,      "B5G6R5_UNORM",       2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_B5G5R5A1UNorm,    "B5G5R5A1_UNORM",     2,   1, 1, VGPUTextureFormatKind_Unorm },

    { VGPUTextureFormat_R32UInt,          "R32_UINT",           4,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_R32SInt,          "R32_SINT",           4,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_R32Float,         "R32_FLOAT",          4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RG16UInt,         "RG16_UINT",          4,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RG16SInt,         "RG16_SINT",          4,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RG16UNorm,        "RG16_UNORM",         4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RG16SNorm,        "RG16_SNORM",         4,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_RG16Float,        "RG16_FLOAT",         4,   1, 1, VGPUTextureFormatKind_Float },

    { VGPUTextureFormat_RGBA8UInt,        "RGBA8_UINT",         4,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RGBA8SInt,        "RGBA8_SINT",         4,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RGBA8UNorm,       "RGBA8_UNORM",        4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RGBA8UNormSrgb,      "SRGBA8_UNORM",    4,   1, 1, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_RGBA8SNorm,       "RGBA8_SNORM",        4,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_BGRA8UNorm,       "BGRA8_UNORM",        4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BGRA8UNormSrgb,      "SBGRA8_UNORM",    4,   1, 1, VGPUTextureFormatKind_UnormSrgb },

    { VGPUTextureFormat_RGB10A2UNorm,   "R10G10B10A2_UNORM",    4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RG11B10Float,   "R11G11B10_FLOAT",      4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RGB9E5Float,    "RGB9E5Float",          4,   1, 1, VGPUTextureFormatKind_Float },

    { VGPUTextureFormat_RG32UInt,         "RG32_UINT",          8,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RG32SInt,         "RG32_SINT",          8,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RG32Float,        "RG32_FLOAT",         8,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_RGBA16UInt,       "RGBA16_UINT",        8,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RGBA16SInt,       "RGBA16_SINT",        8,   1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RGBA16UNorm,      "RGBA16_UNORM",       8,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_RGBA16SNorm,      "RGBA16_SNORM",       8,   1, 1, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_RGBA16Float,      "RGBA16_FLOAT",       8,   1, 1, VGPUTextureFormatKind_Float },

    { VGPUTextureFormat_RGBA32UInt,       "RGBA32_UINT",        16,  1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RGBA32SInt,       "RGBA32_SINT",        16,  1, 1, VGPU_TEXTURE_FORMAT_KIND_INTEGER },
    { VGPUTextureFormat_RGBA32Float,      "RGBA32_FLOAT",       16,  1, 1, VGPUTextureFormatKind_Float },

    // Depth-stencil formats
    { VGPUTextureFormat_Depth16Unorm,           "Depth16Unorm",             2,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Depth32Float,           "Depth32Float",             4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_Stencil8,               "Stencil8",                 4,   1, 1, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_Depth24UnormStencil8,   "Depth24UnormStencil8",     4,   1, 1, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_Depth32FloatStencil8,   "Depth32FloatStencil8",     8,   1, 1, VGPUTextureFormatKind_Float },

    // BC compressed formats
    { VGPUTextureFormat_BC1UNorm,       "BC1UNorm",         8,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BC1UNormSrgb,   "BC1UNormSrgb",     8,   4, 4, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_BC2UNorm,       "BC2UNorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BC2UNormSrgb,   "BC2UNormSrgb",     16,  4, 4, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_BC3UNorm,       "BC3UNorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BC3UNormSrgb,   "BC3UNormSrgb",     16,  4, 4, VGPUTextureFormatKind_UnormSrgb  },
    { VGPUTextureFormat_BC4UNorm,       "BC4UNorm",         8,   4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BC4SNorm,       "BC4SNorm",         8,   4, 4, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_BC5UNorm,       "BC5UNorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BC5SNorm,       "BC5SNorm",         16,  4, 4, VGPUTextureFormatKind_Snorm },
    { VGPUTextureFormat_BC6HUFloat,     "BC6HUFloat",       16,  4, 4, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_BC6HSFloat,     "BC6HSFloat",       16,  4, 4, VGPUTextureFormatKind_Float },
    { VGPUTextureFormat_BC7UNorm,       "BC7UNorm",         16,  4, 4, VGPUTextureFormatKind_Unorm },
    { VGPUTextureFormat_BC7UNormSrgb,   "BC7UNormSrgb",     16,  4, 4, VGPUTextureFormatKind_UnormSrgb },

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

VGPUBool32 vgpuIsDepthFormat(VGPUTextureFormat format)
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

VGPUBool32 vgpuIsStencilFormat(VGPUTextureFormat format)
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

VGPUBool32 vgpuIsDepthStencilFormat(VGPUTextureFormat format)
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

VGPUTextureFormatKind vgpuGetPixelFormatKind(VGPUTextureFormat format)
{
    VGPU_ASSERT(c_FormatInfo[(uint32_t)format].format == format);
    return c_FormatInfo[(uint32_t)format].kind;
}

uint32_t vgpuNumMipLevels(uint32_t width, uint32_t height, uint32_t depth)
{
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


